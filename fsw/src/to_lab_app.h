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
    uint8   tm_vcid;              /* Virtual Channel ID */
    uint8   tm_ocf_flag;          /* Operational Control Field flag */
    uint8   tm_mc_frame_count;    /* Master Channel Frame Count (1 octet per 132.0-B-3) */
    uint8   tm_vc_frame_count;    /* Virtual Channel Frame Count (1 octet per 132.0-B-3) */

} TO_LAB_GlobalData_t;

/************************************************************************
 * Function Prototypes
 ************************************************************************/

void  TO_LAB_AppMain(void);
void  TO_LAB_openTLM(void);
int32 TO_LAB_init(void);
void  TO_LAB_process_commands(void);
void  TO_LAB_forward_telemetry(void);
CFE_Status_t TO_LAB_CreateTMFrame(const CFE_SB_Buffer_t *BufPtr, uint8 *tm_frame, uint16 *tm_frame_len);

/******************************************************************************/

/* Global State Object */
extern TO_LAB_GlobalData_t TO_LAB_Global;

#endif
