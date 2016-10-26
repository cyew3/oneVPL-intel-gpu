//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2013 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_AAC_AUDIO_ENCODER)


#ifdef SBR_NEED_LOG

#include "sbr_enc_dbg.h"
/* HEAAC profile */
#include "sbr_enc_api_fp.h"
#include "sbr_enc_own_fp.h"

FILE* logFile;

#else
# pragma warning( disable: 4206 )
#endif

#else
# pragma warning( disable: 4206 )
#endif //UMC_ENABLE_AAC_AUDIO_ENCODER
