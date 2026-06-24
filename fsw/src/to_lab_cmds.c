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
 * \file
 *  This file contains the source code for the TO lab application
 */

#include "cfe.h"
#include "cfe_config.h" // For CFE_Config_GetVersionString

#include "to_lab_app.h"
#include "to_lab_cmds.h"
#include "to_lab_msg.h"
#include "to_lab_eventids.h"
#include "to_lab_msgids.h"
#include "to_lab_version.h"

#include "ci_lab_msgids.h"
#include "apqs_app_msgids.h"
#include "cfe_es_msgids.h"
#include "cfe_evs_msgids.h"
#include "cfe_sb_msgids.h"
#include "cfe_tbl_msgids.h"
#include "cfe_time_msgids.h"
#include "cf_msgids.h"

#include "crypto.h"
#include "crypto_error.h"
#include "e2eqss_sdls_cfg.h"


/* HK MIDs managed by TO_LAB_EnableHkCmd / TO_LAB_DisableHkCmd */
static const CFE_SB_MsgId_Atom_t TO_LAB_HkMids[] = {
    TO_LAB_HK_TLM_MID,
    CFE_ES_HK_TLM_MID,
    CFE_EVS_HK_TLM_MID,
    CFE_SB_HK_TLM_MID,
    CFE_TBL_HK_TLM_MID,
    CFE_TIME_HK_TLM_MID,
    CI_LAB_HK_TLM_MID,
    CFE_EVS_SHORT_EVENT_MSG_MID,
    CFE_EVS_LONG_EVENT_MSG_MID,
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_EnableOutput() -- TLM output enabled                     */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
CFE_Status_t TO_LAB_EnableOutputCmd(const TO_LAB_EnableOutputCmd_t *data)
{
    const TO_LAB_EnableOutput_Payload_t *pCmd = &data->Payload;

    (void)CFE_SB_MessageStringGet(TO_LAB_Global.tlm_dest_IP, pCmd->dest_IP, "", sizeof(TO_LAB_Global.tlm_dest_IP),
                                  sizeof(pCmd->dest_IP));
    TO_LAB_Global.suppress_sendto = false;
    CFE_EVS_SendEvent(TO_LAB_TLMOUTENA_INF_EID, CFE_EVS_EventType_INFORMATION, "TO telemetry output enabled for IP %s",
                      TO_LAB_Global.tlm_dest_IP);

    if (!TO_LAB_Global.downlink_on) /* Then turn it on, otherwise we will just switch destination addresses*/
    {
        TO_LAB_openTLM();
        TO_LAB_Global.downlink_on = true;
    }

    ++TO_LAB_Global.HkTlm.Payload.CommandCounter;
    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_Noop() -- Noop Handler                                   */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
CFE_Status_t TO_LAB_NoopCmd(const TO_LAB_NoopCmd_t *data)
{
    char VersionString[TO_LAB_CFG_MAX_VERSION_STR_LEN];

    CFE_Config_GetVersionString(VersionString, TO_LAB_CFG_MAX_VERSION_STR_LEN, "TO Lab", TO_LAB_VERSION,
                                TO_LAB_BUILD_CODENAME, TO_LAB_LAST_OFFICIAL);

    CFE_EVS_SendEvent(TO_LAB_NOOP_INF_EID, CFE_EVS_EventType_INFORMATION, "TO: NOOP command. %s", VersionString);

    ++TO_LAB_Global.HkTlm.Payload.CommandCounter;
    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_ResetCounters() -- Reset counters                        */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
CFE_Status_t TO_LAB_ResetCountersCmd(const TO_LAB_ResetCountersCmd_t *data)
{
    TO_LAB_Global.HkTlm.Payload.CommandErrorCounter = 0;
    TO_LAB_Global.HkTlm.Payload.CommandCounter      = 0;

    CFE_EVS_SendEvent(TO_LAB_RESET_INF_EID, CFE_EVS_EventType_INFORMATION, "Reset counters command");

    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_SendDataTypes()  -- Output data types                    */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
CFE_Status_t TO_LAB_SendDataTypesCmd(const TO_LAB_SendDataTypesCmd_t *data)
{
    int16 i;
    char  string_variable[10] = "ABCDEFGHIJ";

    /* initialize data types packet */
    CFE_MSG_Init(CFE_MSG_PTR(TO_LAB_Global.DataTypesTlm.TelemetryHeader), CFE_SB_ValueToMsgId(TO_LAB_DATA_TYPES_MID),
                 sizeof(TO_LAB_Global.DataTypesTlm));

    CFE_SB_TimeStampMsg(CFE_MSG_PTR(TO_LAB_Global.DataTypesTlm.TelemetryHeader));

    /* initialize the packet data */
    TO_LAB_Global.DataTypesTlm.Payload.synch = 0x6969;
#if 0
    TO_LAB_Global.DataTypesTlm.Payload.bit1 = 1;
    TO_LAB_Global.DataTypesTlm.Payload.bit2 = 0;
    TO_LAB_Global.DataTypesTlm.Payload.bit34 = 2;
    TO_LAB_Global.DataTypesTlm.Payload.bit56 = 3;
    TO_LAB_Global.DataTypesTlm.Payload.bit78 = 1;
    TO_LAB_Global.DataTypesTlm.Payload.nibble1 = 0xA;
    TO_LAB_Global.DataTypesTlm.Payload.nibble2 = 0x4;
#endif
    TO_LAB_Global.DataTypesTlm.Payload.bl1 = false;
    TO_LAB_Global.DataTypesTlm.Payload.bl2 = true;
    TO_LAB_Global.DataTypesTlm.Payload.b1  = 16;
    TO_LAB_Global.DataTypesTlm.Payload.b2  = 127;
    TO_LAB_Global.DataTypesTlm.Payload.b3  = 0x7F;
    TO_LAB_Global.DataTypesTlm.Payload.b4  = 0x45;
    TO_LAB_Global.DataTypesTlm.Payload.w1  = 0x2468;
    TO_LAB_Global.DataTypesTlm.Payload.w2  = 0x7FFF;
    TO_LAB_Global.DataTypesTlm.Payload.dw1 = 0x12345678;
    TO_LAB_Global.DataTypesTlm.Payload.dw2 = 0x87654321;
    TO_LAB_Global.DataTypesTlm.Payload.f1  = 90.01;
    TO_LAB_Global.DataTypesTlm.Payload.f2  = .0000045;
    TO_LAB_Global.DataTypesTlm.Payload.df1 = 99.9;
    TO_LAB_Global.DataTypesTlm.Payload.df2 = .4444;

    for (i = 0; i < 10; i++)
        TO_LAB_Global.DataTypesTlm.Payload.str[i] = string_variable[i];

    CFE_SB_TransmitMsg(CFE_MSG_PTR(TO_LAB_Global.DataTypesTlm.TelemetryHeader), true);

    ++TO_LAB_Global.HkTlm.Payload.CommandCounter;
    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_SendHousekeeping() -- HK status                          */
/* Does not increment CommandCounter                               */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
CFE_Status_t TO_LAB_SendHkCmd(const TO_LAB_SendHkCmd_t *data)
{
    CFE_SB_TimeStampMsg(CFE_MSG_PTR(TO_LAB_Global.HkTlm.TelemetryHeader));
    CFE_SB_TransmitMsg(CFE_MSG_PTR(TO_LAB_Global.HkTlm.TelemetryHeader), true);
    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_AddPacket() -- Add packets                               */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
CFE_Status_t TO_LAB_AddPacketCmd(const TO_LAB_AddPacketCmd_t *data)
{
    const TO_LAB_AddPacket_Payload_t *pCmd = &data->Payload;
    int32                             status;

    status = CFE_SB_SubscribeEx(pCmd->Stream, TO_LAB_Global.Tlm_pipe, pCmd->Flags, pCmd->BufLimit);

    if (status != CFE_SUCCESS)
        CFE_EVS_SendEvent(TO_LAB_ADDPKT_ERR_EID, CFE_EVS_EventType_ERROR, "L%d TO Can't subscribe 0x%x status %i",
                          __LINE__, (unsigned int)CFE_SB_MsgIdToValue(pCmd->Stream), (int)status);
    else
        CFE_EVS_SendEvent(TO_LAB_ADDPKT_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "L%d TO AddPkt 0x%x, QoS %d.%d, limit %d", __LINE__,
                          (unsigned int)CFE_SB_MsgIdToValue(pCmd->Stream), pCmd->Flags.Priority,
                          pCmd->Flags.Reliability, pCmd->BufLimit);

    ++TO_LAB_Global.HkTlm.Payload.CommandCounter;
    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_RemovePacket() -- Remove Packet                          */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
CFE_Status_t TO_LAB_RemovePacketCmd(const TO_LAB_RemovePacketCmd_t *data)
{
    const TO_LAB_RemovePacket_Payload_t *pCmd = &data->Payload;
    int32                                status;

    status = CFE_SB_Unsubscribe(pCmd->Stream, TO_LAB_Global.Tlm_pipe);
    if (status != CFE_SUCCESS)
        CFE_EVS_SendEvent(TO_LAB_REMOVEPKT_ERR_EID, CFE_EVS_EventType_ERROR,
                          "L%d TO Can't Unsubscribe to Stream 0x%x, status %i", __LINE__,
                          (unsigned int)CFE_SB_MsgIdToValue(pCmd->Stream), (int)status);
    else
        CFE_EVS_SendEvent(TO_LAB_REMOVEPKT_INF_EID, CFE_EVS_EventType_INFORMATION, "L%d TO RemovePkt 0x%x", __LINE__,
                          (unsigned int)CFE_SB_MsgIdToValue(pCmd->Stream));
    ++TO_LAB_Global.HkTlm.Payload.CommandCounter;
    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_RemoveAll() --  Remove All Packets                       */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
CFE_Status_t TO_LAB_RemoveAllCmd(const TO_LAB_RemoveAllCmd_t *data)
{
    int32         status;
    int           i;
    TO_LAB_Sub_t *SubEntry;

    SubEntry = TO_LAB_Global.SubsTblPtr->Subs;
    for (i = 0; i < TO_LAB_MISSION_MAX_SUBSCRIPTIONS; i++)
    {
        if (CFE_SB_IsValidMsgId(SubEntry->Stream))
        {
            status = CFE_SB_Unsubscribe(SubEntry->Stream, TO_LAB_Global.Tlm_pipe);

            if (status != CFE_SUCCESS)
                CFE_EVS_SendEvent(TO_LAB_REMOVEALLPTKS_ERR_EID, CFE_EVS_EventType_ERROR,
                                  "L%d TO Can't Unsubscribe to stream 0x%x status %i", __LINE__,
                                  (unsigned int)CFE_SB_MsgIdToValue(SubEntry->Stream), (int)status);
        }
    }

    CFE_EVS_SendEvent(TO_LAB_REMOVEALLPKTS_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "L%d TO Unsubscribed to all Commands and Telemetry", __LINE__);

    ++TO_LAB_Global.HkTlm.Payload.CommandCounter;
    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_EnableTMFrameModeCmd() -- Enable TM Frame Mode           */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Find the TM managed parameters for a GVCID. Crypto_Get_Managed_Parameters_For_Gvcid()
 * matches by (tfvn,scid,vcid) only and returns the FIRST entry, which is the TC entry when
 * TC and TM share a VCID (e.g. the bootstrap channel scid=3/vcid=0). TM entries are
 * distinguished by has_ocf carrying a TM value (TM_NO_OCF / TM_HAS_OCF) rather than
 * TC_OCF_NA, so scan for that. */
static bool TO_LAB_FindTmManagedParams(uint8 tfvn, uint16 scid, uint8 vcid, GvcidManagedParameters_t *out)
{
    int i;
    for (i = 0; i < gvcid_counter; i++)
    {
        const GvcidManagedParameters_t *mp = &gvcid_managed_parameters_array[i];
        if (mp->tfvn == tfvn && mp->scid == scid && mp->vcid == vcid &&
            (mp->has_ocf == TM_HAS_OCF || mp->has_ocf == TM_NO_OCF))
        {
            *out = *mp;
            return true;
        }
    }
    return false;
}

void TO_LAB_EnableTMFrameMode(uint8 vcid)
{
    TO_LAB_Global.tm_frame_mode_enabled = true;

    /* Initialize TM frame parameters */
    TO_LAB_Global.tm_tfvn = 0;      /* Transfer Frame Version Number */
    TO_LAB_Global.tm_scid = 0x0003; /* Spacecraft ID */
    TO_LAB_Global.tm_vcid = vcid;   /* Virtual Channel ID */

    /* Frame shape (OCF / FECF presence) comes from this GVCID's TM managed parameters;
     * default to none if the GVCID has no TM managed parameters configured. */
    GvcidManagedParameters_t gvcid_params;
    uint16                   ocf_size  = 0;
    uint16                   fecf_size = 0;
    if (TO_LAB_FindTmManagedParams(TO_LAB_Global.tm_tfvn, TO_LAB_Global.tm_scid, TO_LAB_Global.tm_vcid,
                                   &gvcid_params))
    {
        ocf_size  = (gvcid_params.has_ocf == TM_HAS_OCF) ? TM_OCF_SIZE : 0;
        fecf_size = (gvcid_params.has_fecf == TM_HAS_FECF) ? 2 : 0;
    }
    TO_LAB_Global.tm_ocf_flag = (ocf_size > 0) ? 1 : 0;
    TO_LAB_Global.tm_has_fecf = (fecf_size > 0);

    /* SDLS gate: only route this GVCID through CryptoLib if it is SDLS-protected. The
     * security-header and MAC sizes are SA-derived (SPI + IV + SN + PAD, and stmacf_len),
     * so read them from the operational TM SA; clear channels carry neither. */
    TO_LAB_Global.tm_is_sdls =
        E2EQSS_Gvcid_Has_Sdls(TO_LAB_Global.tm_tfvn, TO_LAB_Global.tm_scid, TO_LAB_Global.tm_vcid);

    uint16 sec_hdr_size = 0;
    uint16 mac_size     = 0;
    if (TO_LAB_Global.tm_is_sdls)
    {
        SecurityAssociation_t *sa = NULL;
        if (sa_if->sa_get_operational_sa_from_gvcid(TO_LAB_Global.tm_tfvn, TO_LAB_Global.tm_scid,
                                                    TO_LAB_Global.tm_vcid, 0, &sa) == CRYPTO_LIB_SUCCESS)
        {
            sec_hdr_size = SPI_LEN + sa->shivf_len + sa->shsnf_len + sa->shplf_len;
            mac_size     = sa->stmacf_len;
        }
        else
        {
            /* SA not available yet: fall back to the legacy fixed sizes. */
            sec_hdr_size = SDLS_SECURITY_HEADER_SIZE;
            mac_size     = TM_MAC_SIZE;
        }
    }
    TO_LAB_Global.tm_data_offset = TM_FRAME_HEADER_SIZE + sec_hdr_size;
    TO_LAB_Global.tm_data_capacity =
        TM_FRAME_MAX_SIZE - TO_LAB_Global.tm_data_offset - mac_size - ocf_size - fecf_size;

    TO_LAB_Global.tm_mc_frame_count = 0;  /* Reset Master Channel counter */
    TO_LAB_Global.tm_vc_frame_count = 0;  /* Reset Virtual Channel counter */
    TO_LAB_Global.tm_span_len       = 0;  /* Clear any in-progress span */
    TO_LAB_Global.tm_span_offset    = 0;

    CFE_EVS_SendEvent(TO_LAB_ENABLE_TM_FRAME_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "TO: TM Frame Mode ENABLED - SCID=0x%04X, VCID=%d",
                      TO_LAB_Global.tm_scid, TO_LAB_Global.tm_vcid);
}

CFE_Status_t TO_LAB_EnableTMFrameModeCmd(const TO_LAB_EnableTMFrameModeCmd_t *data)
{
    uint8 vcid = data->Payload.VCID;

    if (vcid > 7)
    {
        CFE_EVS_SendEvent(TO_LAB_ENABLE_TM_FRAME_INF_EID, CFE_EVS_EventType_ERROR,
                          "TO: Enable TM Frame Mode rejected - VCID %d out of range (0-7)", vcid);
        ++TO_LAB_Global.HkTlm.Payload.CommandErrorCounter;
        return CFE_STATUS_WRONG_MSG_LENGTH;
    }

    TO_LAB_EnableTMFrameMode(vcid);

    ++TO_LAB_Global.HkTlm.Payload.CommandCounter;
    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_DisableTMFrameModeCmd() -- Disable TM Frame Mode         */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
CFE_Status_t TO_LAB_DisableTMFrameModeCmd(const TO_LAB_DisableTMFrameModeCmd_t *data)
{
    TO_LAB_Global.tm_frame_mode_enabled = false;
    TO_LAB_Global.tm_span_len           = 0;  /* Discard any in-progress span */
    TO_LAB_Global.tm_span_offset        = 0;

    CFE_EVS_SendEvent(TO_LAB_DISABLE_TM_FRAME_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "TO: TM Frame Mode DISABLED - returning to raw space packet mode");

    ++TO_LAB_Global.HkTlm.Payload.CommandCounter;
    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_EnableHkCmd() -- Subscribe to all HK MIDs               */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
CFE_Status_t TO_LAB_EnableHkCmd(const TO_LAB_EnableHkCmd_t *data)
{
    int32  status;
    size_t i;
    int    count = 0;

    for (i = 0; i < sizeof(TO_LAB_HkMids) / sizeof(TO_LAB_HkMids[0]); i++)
    {
        CFE_SB_MsgId_t mid = CFE_SB_ValueToMsgId(TO_LAB_HkMids[i]);
        status = CFE_SB_SubscribeEx(mid, TO_LAB_Global.Tlm_pipe, CFE_SB_DEFAULT_QOS, 4);
        if (status != CFE_SUCCESS && status != CFE_SB_PIPE_CR_ERR)
        {
            CFE_EVS_SendEvent(TO_LAB_HK_SUB_ERR_EID, CFE_EVS_EventType_ERROR,
                              "TO EnableHK: Subscribe failed for MID 0x%x status %i",
                              (unsigned int)TO_LAB_HkMids[i], (int)status);
        }
        else
        {
            count++;
        }
    }

    CFE_EVS_SendEvent(TO_LAB_ENABLE_HK_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "TO: HK telemetry ENABLED (%d MIDs subscribed)", count);

    ++TO_LAB_Global.HkTlm.Payload.CommandCounter;
    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_DisableHkCmd() -- Unsubscribe from all HK MIDs          */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
CFE_Status_t TO_LAB_DisableHkCmd(const TO_LAB_DisableHkCmd_t *data)
{
    int32  status;
    size_t i;
    int    count = 0;

    for (i = 0; i < sizeof(TO_LAB_HkMids) / sizeof(TO_LAB_HkMids[0]); i++)
    {
        CFE_SB_MsgId_t mid = CFE_SB_ValueToMsgId(TO_LAB_HkMids[i]);
        status = CFE_SB_Unsubscribe(mid, TO_LAB_Global.Tlm_pipe);
        if (status != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(TO_LAB_HK_SUB_ERR_EID, CFE_EVS_EventType_ERROR,
                              "TO DisableHK: Unsubscribe failed for MID 0x%x status %i",
                              (unsigned int)TO_LAB_HkMids[i], (int)status);
        }
        else
        {
            count++;
        }
    }

    CFE_EVS_SendEvent(TO_LAB_DISABLE_HK_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "TO: HK telemetry DISABLED (%d MIDs unsubscribed)", count);

    ++TO_LAB_Global.HkTlm.Payload.CommandCounter;
    return CFE_SUCCESS;
}
