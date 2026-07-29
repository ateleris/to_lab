/************************************************************************
 * NASA Docket No. GSC-19,200-1, and identified as "cFS Draco"
 *
 * Copyright (c) 2023 United States Government as represented by the
 * Administrator of the National Aeronautics and Space Administration.
 * All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ************************************************************************/

/**
 * @file
 *   Define TO Lab Application header file
 */

#ifndef TO_LAB_APP_H
#define TO_LAB_APP_H

#include "common_types.h"
#include "osapi.h"
#include "cfe.h"

#include "to_lab_mission_cfg.h"
#include "to_lab_platform_cfg.h"
#include "to_lab_cmds.h"
#include "to_lab_dispatch.h"
#include "to_lab_msg.h"
#include "to_lab_tbl.h"

/************************************************************************
** Type Definitions
*************************************************************************/

/* TM Transfer Frame layout constants (CCSDS 132.0-B-3). Per-channel security-header and
 * MAC sizes are SA-derived per send by TO_LAB_DeriveVcGeometry (to_lab_app.c); there is
 * no fixed fallback: an SDLS VC without an operational SA drops its packets. */
#define TM_FRAME_MAX_SIZE    1786
#define TM_FRAME_HEADER_SIZE 6
#define TM_OCF_SIZE          4

#define TO_LAB_VC_COUNT 8 /* TM VCID is 3 bits (CCSDS 132.0-B-3) */

/**
 * Per-virtual-channel TM frame build state. Geometry fields are (re)derived per send by
 * TO_LAB_DeriveVcGeometry so that SAs created at runtime via SDLS EP are picked up.
 */
typedef struct
{
    bool   is_sdls;        /* GVCID is SDLS-protected (route through CryptoLib) */
    bool   has_fecf;       /* GVCID frame carries a FECF (frame-level CRC) */
    uint8  ocf_flag;       /* Operational Control Field flag */
    uint16 data_offset;    /* Byte offset where SP data begins (primary hdr + sec hdr) */
    uint16 data_capacity;  /* Usable SP data bytes per frame */
    uint8  vc_frame_count; /* Virtual Channel Frame Count, per-VC per 132.0-B-3 */
    bool   sa_warned;      /* Throttle for the no-operational-SA drop event */
} TO_LAB_TMVirtualChannel_t;

/**
 * CI global data structure
 */
typedef struct
{
    CFE_SB_PipeId_t Tlm_pipe;
    CFE_SB_PipeId_t Cmd_pipe;
    osal_id_t       TLMsockid;
    bool            downlink_on;
    char            tlm_dest_IP[17];
    bool            suppress_sendto;
    bool            AllowPassthru;

    TO_LAB_HkTlm_t        HkTlm;
    TO_LAB_DataTypesTlm_t DataTypesTlm;

    TO_LAB_Subs_t *  SubsTblPtr;
    CFE_TBL_Handle_t SubsTblHandle;

    /* TM Transfer Frame Mode fields */
    bool    tm_frame_mode_enabled;
    uint8   tm_tfvn;              /* Transfer Frame Version Number */
    uint16  tm_scid;              /* Spacecraft ID */
    uint8   tm_default_vcid;      /* VC for MIDs without a table assignment (enable cmd arg) */
    uint8   tm_mc_frame_count;    /* Master Channel Frame Count, global across VCs (1 octet) */
    bool    tm_vcid_range_warned; /* One-time warning for out-of-range table VCIDs */

    /* Per-VC frame build state (geometry + VC frame counter) */
    TO_LAB_TMVirtualChannel_t TmVc[TO_LAB_VC_COUNT];

    /* TM frame mode auto-start (from TO_LAB_TM_FRAME_VCID env var). Deferred to the first
     * main-loop pass so CI_LAB has configured the CryptoLib managed params before the
     * geometry is first derived. */
    bool    tm_frame_autostart_pending;
    uint8   tm_frame_autostart_vcid;

    /* TM frame spanning state for oversized Space Packets. Only one span is ever active:
     * a spanned SP is fully drained into continuation frames before the next SB message
     * is processed. tm_span_geom snapshots the owning VC's geometry at span start so an
     * SA change mid-span cannot corrupt the tail (vc_frame_count in the snapshot is
     * unused: counters always come live from TmVc[tm_span_vcid]). */
    uint8   tm_span_buf[CFE_MISSION_SB_MAX_SB_MSG_SIZE]; /* Pending SP data */
    uint32  tm_span_len;          /* Total bytes of pending SP (0 = no span active) */
    uint32  tm_span_offset;       /* Bytes already sent from tm_span_buf */
    uint8   tm_span_vcid;         /* VC the active span belongs to */
    TO_LAB_TMVirtualChannel_t tm_span_geom; /* Geometry snapshot at span start */

} TO_LAB_GlobalData_t;

/************************************************************************
 * Function Prototypes
 ************************************************************************/

void  TO_LAB_AppMain(void);
void  TO_LAB_openTLM(void);
int32 TO_LAB_init(void);
void  TO_LAB_process_commands(void);
void  TO_LAB_forward_telemetry(void);
CFE_Status_t TO_LAB_CreateTMFrame(const CFE_SB_Buffer_t *BufPtr, uint8 *tm_frame, uint16 *tm_frame_len,
                                  const TO_LAB_TMVirtualChannel_t *geom, uint8 vcid);

/******************************************************************************/

/* Global State Object */
extern TO_LAB_GlobalData_t TO_LAB_Global;

#endif
