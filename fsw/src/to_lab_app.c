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
#include "cfe_config.h"
#include <stdlib.h>

#include "to_lab_app.h"
#include "to_lab_encode.h"
#include "to_lab_eventids.h"
#include "to_lab_msgids.h"
#include "to_lab_perfids.h"
#include "to_lab_version.h"
#include "to_lab_msg.h"
#include "to_lab_tbl.h"

#include "crypto.h"
#include "crypto_error.h"

/* TM Transfer Frame constants (CCSDS 132.0-B-3) */
#define TM_FRAME_MAX_SIZE         1786
#define SDLS_SECURITY_HEADER_SIZE 14
#define TM_FRAME_HEADER_SIZE      6
#define TM_DATA_FIELD_CAPACITY    (TM_FRAME_MAX_SIZE - TM_FRAME_HEADER_SIZE - SDLS_SECURITY_HEADER_SIZE)

/* Data Field Status word components */
#define TM_DFS_SEGMENT_LENGTH_ID  (0x3 << 11)   /* 11 = Space Packets (CCSDS 132.0-B-3 Table 4-3) */
#define TM_FHP_CONTINUATION       0x7FE          /* No new packet starts in this frame */

/*
** TO Global Data Section
*/
TO_LAB_GlobalData_t TO_LAB_Global;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                   */
/* TO_LAB_AppMain() -- Application entry point and main process loop */
/*                                                                   */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void TO_LAB_AppMain(void)
{
    uint32       RunStatus = CFE_ES_RunStatus_APP_RUN;
    CFE_Status_t status;

    CFE_ES_PerfLogEntry(TO_LAB_MAIN_TASK_PERF_ID);

    status = TO_LAB_init();

    if (status != CFE_SUCCESS)
    {
        /*
        ** Set request to terminate main loop...
        */
        RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    /*
    ** TO RunLoop
    */
    while (CFE_ES_RunLoop(&RunStatus) == true)
    {
        CFE_ES_PerfLogExit(TO_LAB_MAIN_TASK_PERF_ID);

        OS_TaskDelay(TO_LAB_PLATFORM_TASK_MSEC);

        CFE_ES_PerfLogEntry(TO_LAB_MAIN_TASK_PERF_ID);

        TO_LAB_forward_telemetry();

        TO_LAB_process_commands();
    }

    CFE_ES_ExitApp(RunStatus);
}

/*
** TO delete callback function.
** This function will be called in the event that the TO app is killed.
** It will close the network socket for TO
*/
void TO_LAB_delete_callback(void)
{
    OS_printf("TO delete callback -- Closing TO Network socket.\n");
    if (TO_LAB_Global.downlink_on)
    {
        OS_close(TO_LAB_Global.TLMsockid);
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_init() -- TO initialization                              */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
CFE_Status_t TO_LAB_init(void)
{
    CFE_Status_t  status;
    char          PipeName[16];
    uint16        PipeDepth;
    uint16        i;
    char          ToTlmPipeName[16];
    uint16        ToTlmPipeDepth;
    void *        TblPtr;
    TO_LAB_Sub_t *SubEntry;
    char          VersionString[TO_LAB_CFG_MAX_VERSION_STR_LEN];

    /* Zero out the global data structure */
    memset(&TO_LAB_Global, 0, sizeof(TO_LAB_Global));

    TO_LAB_Global.AllowPassthru = true;
    TO_LAB_Global.downlink_on   = false;
    PipeDepth                   = TO_LAB_PLATFORM_CMD_PIPE_DEPTH;
    strcpy(PipeName, "TO_LAB_CMD_PIPE");
    ToTlmPipeDepth = TO_LAB_PLATFORM_TLM_PIPE_DEPTH;
    strcpy(ToTlmPipeName, "TO_LAB_TLM_PIPE");

    /*
    ** Register with EVS
    */
    status = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("TO_LAB: Error registering for Event Services, RC = 0x%08X\n", (unsigned int)status);
    }

    if (status == CFE_SUCCESS)
    {
        /*
        ** Initialize housekeeping packet (clear user data area)...
        */
        CFE_MSG_Init(CFE_MSG_PTR(TO_LAB_Global.HkTlm.TelemetryHeader), CFE_SB_ValueToMsgId(TO_LAB_HK_TLM_MID),
                     sizeof(TO_LAB_Global.HkTlm));

        status = CFE_TBL_Register(&TO_LAB_Global.SubsTblHandle, "Subscriptions", sizeof(TO_LAB_Subs_t),
                                  CFE_TBL_OPT_DEFAULT, NULL);

        if (status != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(TO_LAB_TBL_ERR_EID, CFE_EVS_EventType_ERROR, "L%d TO Can't register table status %i",
                              __LINE__, (int)status);
        }
    }

    if (status == CFE_SUCCESS)
    {
        status = CFE_TBL_Load(TO_LAB_Global.SubsTblHandle, CFE_TBL_SRC_FILE, "/cf/to_lab_sub.tbl");

        if (status != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(TO_LAB_TBL_ERR_EID, CFE_EVS_EventType_ERROR, "L%d TO Can't load table status %i",
                              __LINE__, (int)status);
        }
    }

    if (status == CFE_SUCCESS)
    {
        status = CFE_TBL_GetAddress((void **)&TblPtr, TO_LAB_Global.SubsTblHandle);

        if (status != CFE_SUCCESS && status != CFE_TBL_INFO_UPDATED)
        {
            CFE_EVS_SendEvent(TO_LAB_TBL_ERR_EID, CFE_EVS_EventType_ERROR, "L%d TO Can't get table addr status %i",
                              __LINE__, (int)status);
        }
    }

    if (status == CFE_SUCCESS || status == CFE_TBL_INFO_UPDATED)
    {
        TO_LAB_Global.SubsTblPtr = TblPtr; /* Save returned address */

        /* Subscribe to my commands */
        status = CFE_SB_CreatePipe(&TO_LAB_Global.Cmd_pipe, PipeDepth, PipeName);
        if (status != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(TO_LAB_CR_PIPE_ERR_EID, CFE_EVS_EventType_ERROR, "L%d TO Can't create cmd pipe status %i",
                              __LINE__, (int)status);
        }
    }

    if (status == CFE_SUCCESS)
    {

        CFE_SB_Subscribe(CFE_SB_ValueToMsgId(TO_LAB_CMD_MID), TO_LAB_Global.Cmd_pipe);
        CFE_SB_Subscribe(CFE_SB_ValueToMsgId(TO_LAB_SEND_HK_MID), TO_LAB_Global.Cmd_pipe);

        /* Create TO TLM pipe */
        status = CFE_SB_CreatePipe(&TO_LAB_Global.Tlm_pipe, ToTlmPipeDepth, ToTlmPipeName);
        if (status != CFE_SUCCESS)
        {
            CFE_EVS_SendEvent(TO_LAB_TLMPIPE_ERR_EID, CFE_EVS_EventType_ERROR, "L%d TO Can't create Tlm pipe status %i",
                              __LINE__, (int)status);
        }
    }

    if (status == CFE_SUCCESS)
    {
        /* Subscriptions for TLM pipe*/
        SubEntry = TO_LAB_Global.SubsTblPtr->Subs;
        for (i = 0; i < TO_LAB_MISSION_MAX_SUBSCRIPTIONS; i++)
        {
            if (!CFE_SB_IsValidMsgId(SubEntry->Stream))
            {
                /* Only process until invalid MsgId is found*/
                break;
            }

            status = CFE_SB_SubscribeEx(SubEntry->Stream, TO_LAB_Global.Tlm_pipe, SubEntry->Flags, SubEntry->BufLimit);
            if (status != CFE_SUCCESS)
            {
                CFE_EVS_SendEvent(TO_LAB_SUBSCRIBE_ERR_EID, CFE_EVS_EventType_ERROR,
                                  "L%d TO Can't subscribe to stream 0x%x status %i", __LINE__,
                                  (unsigned int)CFE_SB_MsgIdToValue(SubEntry->Stream), (int)status);
            }

            ++SubEntry;
        }

        CFE_Config_GetVersionString(VersionString, TO_LAB_CFG_MAX_VERSION_STR_LEN, "TO Lab", TO_LAB_VERSION,
                                    TO_LAB_BUILD_CODENAME, TO_LAB_LAST_OFFICIAL);

        CFE_EVS_SendEvent(TO_LAB_INIT_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "TO Lab Initialized.%s, Awaiting enable command.", VersionString);

        /* Auto-enable output if TO_LAB_TLM_DEST_IP is set */
        const char *auto_dest_ip = getenv("TO_LAB_TLM_DEST_IP");
        if (auto_dest_ip != NULL && auto_dest_ip[0] != '\0')
        {
            strncpy(TO_LAB_Global.tlm_dest_IP, auto_dest_ip, sizeof(TO_LAB_Global.tlm_dest_IP) - 1);
            TO_LAB_Global.tlm_dest_IP[sizeof(TO_LAB_Global.tlm_dest_IP) - 1] = '\0';
            TO_LAB_Global.suppress_sendto = false;
            TO_LAB_openTLM();
            TO_LAB_Global.downlink_on = true;

            CFE_EVS_SendEvent(TO_LAB_TLMOUTENA_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "TO: output auto-enabled for IP %s (from env)", TO_LAB_Global.tlm_dest_IP);
        }
    }

    /*
     ** Install the delete handler
     */
    OS_TaskInstallDeleteHandler(&TO_LAB_delete_callback);

    return status;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_process_commands() -- Process command pipe message       */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void TO_LAB_process_commands(void)
{
    CFE_SB_Buffer_t *SBBufPtr;
    CFE_Status_t     Status;

    /* Exit command processing loop if no message received. */
    while (1)
    {
        Status = CFE_SB_ReceiveBuffer(&SBBufPtr, TO_LAB_Global.Cmd_pipe, CFE_SB_POLL);
        if (Status != CFE_SUCCESS)
        {
            break;
        }

        TO_LAB_TaskPipe(SBBufPtr);
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_openTLM() -- Open TLM                                    */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void TO_LAB_openTLM(void)
{
    int32 status;

    status = OS_SocketOpen(&TO_LAB_Global.TLMsockid, OS_SocketDomain_INET, OS_SocketType_DATAGRAM);
    if (status != OS_SUCCESS)
    {
        CFE_EVS_SendEvent(TO_LAB_TLMOUTSOCKET_ERR_EID, CFE_EVS_EventType_ERROR, "L%d, TO TLM socket error: %d",
                          __LINE__, (int)status);
    }

    /*---------------- Add static arp entries ----------------*/
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_WriteTMHeader() -- Build TM primary header into frame    */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static void TO_LAB_WriteTMHeader(uint8 *tm_frame, uint16 first_header_pointer)
{
    uint16 tfvn_scid_vcid;
    uint16 status_word;

    /* Bytes 0-1: TFVN(2) + SCID(10) + VCID(3) + OCF_Flag(1) */
    tfvn_scid_vcid = (TO_LAB_Global.tm_tfvn << 14) | (TO_LAB_Global.tm_scid << 4) |
                     (TO_LAB_Global.tm_vcid << 1) | TO_LAB_Global.tm_ocf_flag;
    tm_frame[0] = (tfvn_scid_vcid >> 8) & 0xFF;
    tm_frame[1] = tfvn_scid_vcid & 0xFF;

    /* Byte 2: Master Channel Frame Count */
    tm_frame[2] = TO_LAB_Global.tm_mc_frame_count & 0xFF;

    /* Byte 3: Virtual Channel Frame Count */
    tm_frame[3] = TO_LAB_Global.tm_vc_frame_count & 0xFF;

    /* Bytes 4-5: Transfer Frame Data Field Status
     * Bit 15:    TF Secondary Header Flag = 0
     * Bit 14:    Synch Flag = 0 (octet-synchronised, forward-ordered packets)
     * Bit 13:    Packet Order Flag = 0
     * Bits 12-11: Segment Length ID = 11 (Space Packets, CCSDS 132.0-B-3 Table 4-3)
     * Bits 10-0:  First Header Pointer */
    status_word = TM_DFS_SEGMENT_LENGTH_ID | (first_header_pointer & 0x7FF);
    tm_frame[4] = (status_word >> 8) & 0xFF;
    tm_frame[5] = status_word & 0xFF;

    /* Bytes 6-19: Zero security header space (filled by Crypto_TM_ApplySecurity) */
    memset(&tm_frame[TM_FRAME_HEADER_SIZE], 0, SDLS_SECURITY_HEADER_SIZE);

    /* Increment frame counters */
    TO_LAB_Global.tm_mc_frame_count++;
    TO_LAB_Global.tm_vc_frame_count++;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_SendTMFrame() -- Encrypt and send one TM frame           */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static int32 TO_LAB_SendTMFrame(uint8 *tm_frame, uint16 tm_frame_len, OS_SockAddr_t *d_addr)
{
    int32_t crypto_status = Crypto_TM_ApplySecurity(tm_frame, tm_frame_len);

    if (crypto_status != CRYPTO_LIB_SUCCESS)
    {
        CFE_EVS_SendEvent(TO_LAB_ENCODE_ERR_EID, CFE_EVS_EventType_ERROR,
                          "TM frame encryption failed: %d", (int)crypto_status);
        return CFE_STATUS_EXTERNAL_RESOURCE_FAIL;
    }

    return OS_SocketSendTo(TO_LAB_Global.TLMsockid, tm_frame, tm_frame_len, d_addr);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_CreateContinuationFrame() -- Build next span frame       */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static CFE_Status_t TO_LAB_CreateContinuationFrame(uint8 *tm_frame, uint16 *tm_frame_len)
{
    uint32 remaining = TO_LAB_Global.tm_span_len - TO_LAB_Global.tm_span_offset;
    uint32 copy_len  = (remaining < TM_DATA_FIELD_CAPACITY) ? remaining : TM_DATA_FIELD_CAPACITY;

    TO_LAB_WriteTMHeader(tm_frame, TM_FHP_CONTINUATION);

    memcpy(&tm_frame[TM_FRAME_HEADER_SIZE + SDLS_SECURITY_HEADER_SIZE],
           &TO_LAB_Global.tm_span_buf[TO_LAB_Global.tm_span_offset],
           copy_len);

    if (copy_len < TM_DATA_FIELD_CAPACITY)
    {
        memset(&tm_frame[TM_FRAME_HEADER_SIZE + SDLS_SECURITY_HEADER_SIZE + copy_len],
               0x55, TM_DATA_FIELD_CAPACITY - copy_len);
    }

    TO_LAB_Global.tm_span_offset += copy_len;

    if (TO_LAB_Global.tm_span_offset >= TO_LAB_Global.tm_span_len)
    {
        TO_LAB_Global.tm_span_len    = 0;
        TO_LAB_Global.tm_span_offset = 0;
    }

    *tm_frame_len = TM_FRAME_MAX_SIZE;
    return CFE_SUCCESS;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_forward_telemetry() -- Forward telemetry                 */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void TO_LAB_forward_telemetry(void)
{
    OS_SockAddr_t    d_addr;
    int32            OsStatus;
    CFE_Status_t     CfeStatus;
    CFE_SB_Buffer_t *SBBufPtr;
    const void *     NetBufPtr;
    size_t           NetBufSize;
    uint32           PktCount = 0;
    uint16           PortNum = TO_LAB_MISSION_TLM_PORT + CFE_PSP_GetProcessorId() - 1;
    uint8            tm_frame_buf[2048];
    uint16           tm_frame_len;

    OS_SocketAddrInit(&d_addr, OS_SocketDomain_INET);
    OS_SocketAddrSetPort(&d_addr, PortNum);
    OS_SocketAddrFromString(&d_addr, TO_LAB_Global.tlm_dest_IP);
    OsStatus = 0;

    do
    {
        CfeStatus = CFE_SB_ReceiveBuffer(&SBBufPtr, TO_LAB_Global.Tlm_pipe, TO_LAB_PLATFORM_TLM_PIPE_TIMEOUT);

        if ((CfeStatus == CFE_SUCCESS) && (TO_LAB_Global.suppress_sendto == false))
        {
            OsStatus = OS_SUCCESS;

            if (TO_LAB_Global.downlink_on == true)
            {
                CFE_ES_PerfLogEntry(TO_LAB_SOCKET_SEND_PERF_ID);

                /* Check if TM frame mode is enabled */
                if (TO_LAB_Global.tm_frame_mode_enabled)
                {
                    /* Build and send first (or only) frame for this SP */
                    CfeStatus = TO_LAB_CreateTMFrame(SBBufPtr, tm_frame_buf, &tm_frame_len);

                    if (CfeStatus == CFE_SUCCESS)
                    {
                        OsStatus = TO_LAB_SendTMFrame(tm_frame_buf, tm_frame_len, &d_addr);

                        /* Drain continuation frames if SP was too large for one frame */
                        while (OsStatus >= 0 && TO_LAB_Global.tm_span_len > 0)
                        {
                            TO_LAB_CreateContinuationFrame(tm_frame_buf, &tm_frame_len);
                            OsStatus = TO_LAB_SendTMFrame(tm_frame_buf, tm_frame_len, &d_addr);
                        }
                    }
                }
                else
                {
                    /* Standard mode: encode and send without TM frame wrapper */
                    CfeStatus = TO_LAB_EncodeOutputMessage(SBBufPtr, &NetBufPtr, &NetBufSize);

                    if (CfeStatus != CFE_SUCCESS)
                    {
                        CFE_EVS_SendEvent(TO_LAB_ENCODE_ERR_EID, CFE_EVS_EventType_ERROR, "Error packing output: %d\n",
                                          (int)CfeStatus);
                    }
                    else
                    {
                        OsStatus = OS_SocketSendTo(TO_LAB_Global.TLMsockid, NetBufPtr, NetBufSize, &d_addr);
                    }
                }

                CFE_ES_PerfLogExit(TO_LAB_SOCKET_SEND_PERF_ID);
            }

            if (OsStatus < 0)
            {
                CFE_EVS_SendEvent(TO_LAB_TLMOUTSTOP_ERR_EID, CFE_EVS_EventType_ERROR,
                                  "L%d TO sendto error %d. Tlm output suppressed\n", __LINE__, (int)OsStatus);
                TO_LAB_Global.suppress_sendto = true;
            }
        }
        /* If CFE_SB_status != CFE_SUCCESS, then no packet was received from CFE_SB_ReceiveBuffer() */

        PktCount++;
    } while (CfeStatus == CFE_SUCCESS && PktCount < TO_LAB_PLATFORM_MAX_TLM_PKTS);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* TO_LAB_CreateTMFrame() -- Create TM Transfer Frame              */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
CFE_Status_t TO_LAB_CreateTMFrame(const CFE_SB_Buffer_t *BufPtr, uint8 *tm_frame, uint16 *tm_frame_len)
{
    size_t       pkt_len;
    CFE_Status_t status;
    uint32       copy_len;

    status = CFE_MSG_GetSize(&BufPtr->Msg, &pkt_len);
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(TO_LAB_ENCODE_ERR_EID, CFE_EVS_EventType_ERROR,
                          "TM Frame: Failed to get message size");
        return status;
    }

    if (pkt_len > CFE_MISSION_SB_MAX_SB_MSG_SIZE)
    {
        CFE_EVS_SendEvent(TO_LAB_ENCODE_ERR_EID, CFE_EVS_EventType_ERROR,
                          "TM Frame: Packet size %d exceeds SB maximum", (int)pkt_len);
        return CFE_STATUS_EXTERNAL_RESOURCE_FAIL;
    }

    copy_len = (pkt_len < TM_DATA_FIELD_CAPACITY) ? pkt_len : TM_DATA_FIELD_CAPACITY;

    /* FHP = 0: SP header starts at byte 0 of the data field */
    TO_LAB_WriteTMHeader(tm_frame, 0);

    memcpy(&tm_frame[TM_FRAME_HEADER_SIZE + SDLS_SECURITY_HEADER_SIZE], BufPtr, copy_len);

    if (copy_len < TM_DATA_FIELD_CAPACITY)
    {
        memset(&tm_frame[TM_FRAME_HEADER_SIZE + SDLS_SECURITY_HEADER_SIZE + copy_len],
               0x55, TM_DATA_FIELD_CAPACITY - copy_len);
    }

    /* If SP exceeds one frame, store remainder for continuation frames */
    if (pkt_len > TM_DATA_FIELD_CAPACITY)
    {
        memcpy(TO_LAB_Global.tm_span_buf, (const uint8 *)BufPtr, pkt_len);
        TO_LAB_Global.tm_span_len    = pkt_len;
        TO_LAB_Global.tm_span_offset = copy_len;
    }

    *tm_frame_len = TM_FRAME_MAX_SIZE;
    return CFE_SUCCESS;
}

/************************/
/*  End of File Comment */
/************************/
