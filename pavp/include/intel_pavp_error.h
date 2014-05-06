/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef _INTEL_PAVP_ERROR_H
#define _INTEL_PAVP_ERROR_H

#include "oscl_platform_def.h"

typedef uint32 PavpEpidStatus;

// Status spaces base values
#define PAVP_INTERNAL_STATUS_BASE           0x1000        // Internal status base value
#define PAVP_INT_SIGMA_STATUS_BASE          0x3000        // Internal SIGMA status base value
#define PAVP_INT_PAVP15_STATUS_BASE         0x4000        // Internal PAVP 1.5 specyfic status base value
#define PAVP_DEV_STATUS_BASE                0x5000        // Base for application errors

#define PAVP_STATUS_SUCCESS                    0x0000 
#define PAVP_STATUS_INTERNAL_ERROR            0x1000 
#define PAVP_STATUS_UNKNOWN_ERROR            0x1001 
#define PAVP_STATUS_INCORRECT_API_VERSION    0x1002 
#define PAVP_STATUS_INVALID_FUNCTION        0x1003 
#define PAVP_STATUS_INVALID_BUFFER_LENGTH    0x1004 
#define PAVP_STATUS_INVALID_PARAMS            0x1005 
#define PAVP_STATUS_EPID_INVALID_PUBKEY                0x3000 
#define PAVP_STATUS_SIGMA_SESSION_LIMIT_REACHED        0x3002 
#define PAVP_STATUS_SIGMA_SESSION_INVALID_HANDLE    0x3003 
#define PAVP_STATUS_SIGMA_PUBKEY_GENERATION_FAILED    0x3004 
#define PAVP_STATUS_SIGMA_INVALID_3PCERT_HMAC        0x3005 
#define PAVP_STATUS_SIGMA_INVALID_SIG_INTEL            0x3006 
#define PAVP_STATUS_SIGMA_INVALID_SIG_CERT            0x3007 
#define PAVP_STATUS_SIGMA_EXPIRED_3PCERT            0x3008 
#define PAVP_STATUS_SIGMA_INVALID_SIG_GAGB            0x3009 
#define PAVP_STATUS_PAVP_EPID_INVALID_PATH_ID        0x4000 
#define PAVP_STATUS_PAVP_EPID_INVALID_STREAM_ID        0x4001 
#define PAVP_STATUS_PAVP_EPID_STREAM_SLOT_OWNED        0x4002 
#define PAVP_STATUS_INVALID_STREAM_KEY_SIG            0x4003 
#define PAVP_STATUS_INVALID_TITLE_KEY_SIG            0x4004 
#define PAVP_STATUS_INVALID_TITLE_KEY_TIME            0x4005 
#define PAVP_STATUS_COMMAND_NOT_AUTHORIZED            0x4006 
#define PAVP_STATUS_INVALID_DRM_TIME                0x4007 
#define PAVP_STATUS_INVALID_TIME_SIG                0x4009 
#define PAVP_STATUS_TIME_OVERFLOW                    0x400A 

#define PAVP_EPID_SUCCESS(Status) (((PavpEpidStatus)(Status)) == PAVP_STATUS_SUCCESS)
#define PAVP_EPID_FAILURE(Status) (((PavpEpidStatus)(Status)) != PAVP_STATUS_SUCCESS)

#endif // _INTEL_PAVP_ERROR_H