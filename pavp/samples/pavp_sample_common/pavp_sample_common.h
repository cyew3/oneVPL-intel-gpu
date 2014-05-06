/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef __PAVP_SAMPLE_COMMON__
#define __PAVP_SAMPLE_COMMON__

#include "oscl_platform_def.h"
#include "pavpsdk_defs.h"

/**
@defgroup sampleCommon Core helper functionality.
Implements logic common for PAVP SDK samples.
*/

/**
@ingroup sampleCommon
@{
*/

extern "C" const uint8 Test_v10_Cert3p[]; ///< Test certificate for SIGMA session of size Test_v10_Cert3pSize, version Test_v10_ApiVersion with private key Test_v10_DsaPrivateKey.
extern "C" const uint32 Test_v10_Cert3pSize; ///< Test_v10_Cert3p size in bytes.
extern "C" const uint8 Test_v10_DsaPrivateKey[]; ///< Test_v10_Cert3p certificate private key
extern "C" const uint32 Test_v10_ApiVersion; ///< Test_v10_Cert3p certificate version.

#include "windows.h"
void PAVPSDK_printGUID(GUID val);
void PAVPSDK_printNumber(uint64 val);


/** @} */ //@ingroup sampleCommon

#endif//__PAVP_SAMPLE_COMMON__