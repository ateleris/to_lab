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

#ifndef TO_LAB_CMDS_H
#define TO_LAB_CMDS_H

#include "common_types.h"
#include "cfe_error.h"
#include "to_lab_msg.h"

/******************************************************************************/

/*
** Prototypes Section
*/
CFE_Status_t TO_LAB_AddPacketCmd(const TO_LAB_AddPacketCmd_t *data);
CFE_Status_t TO_LAB_NoopCmd(const TO_LAB_NoopCmd_t *data);
CFE_Status_t TO_LAB_EnableOutputCmd(const TO_LAB_EnableOutputCmd_t *data);
CFE_Status_t TO_LAB_RemoveAllCmd(const TO_LAB_RemoveAllCmd_t *data);
CFE_Status_t TO_LAB_RemovePacketCmd(const TO_LAB_RemovePacketCmd_t *data);
CFE_Status_t TO_LAB_ResetCountersCmd(const TO_LAB_ResetCountersCmd_t *data);
CFE_Status_t TO_LAB_SendDataTypesCmd(const TO_LAB_SendDataTypesCmd_t *data);
CFE_Status_t TO_LAB_SendHkCmd(const TO_LAB_SendHkCmd_t *data);
CFE_Status_t TO_LAB_EnableTMFrameModeCmd(const TO_LAB_EnableTMFrameModeCmd_t *data);
CFE_Status_t TO_LAB_DisableTMFrameModeCmd(const TO_LAB_DisableTMFrameModeCmd_t *data);

/* Enable TM frame mode on the given VCID (0-7). Shared by the command handler and the
 * TO_LAB_TM_FRAME_VCID env-var auto-start; sets the TM frame params and derives the OCF
 * flag from the CryptoLib managed parameters for the GVCID. */
void TO_LAB_EnableTMFrameMode(uint8 vcid);
CFE_Status_t TO_LAB_EnableHkCmd(const TO_LAB_EnableHkCmd_t *data);
CFE_Status_t TO_LAB_DisableHkCmd(const TO_LAB_DisableHkCmd_t *data);

/******************************************************************************/

#endif
