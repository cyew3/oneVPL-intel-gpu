/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement.
This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
Copyright(c) 2005-2015 Intel Corporation. All Rights Reserved.

**********************************************************************************/
#include "modified_transcoding_pipeline.h"
#pragma warning( disable : 4702)

#define MOD_SMT_CREATE_PIPELINE return new CModifiedTranscodingPipeline;
#define MOD_SMT_PRINT_HELP      msdk_printf(MSDK_STRING("  -noHME        Disable HME (H264 encode only)\n"));
#define MOD_SMT_PARSE_INPUT     else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-noHME"))){InputParams.reserved[0] = 1;}

