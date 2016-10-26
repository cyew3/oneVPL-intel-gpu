//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_BRC_COMMON_H__
#define __MFX_BRC_COMMON_H__

#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_ENCODE) || defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE)
#define UMC_ENABLE_VIDEO_BRC
#define MFX_ENABLE_VIDEO_BRC_COMMON

#include "umc_video_brc.h"

mfxStatus ConvertVideoParam_Brc(const mfxVideoParam *parMFX, UMC::VideoBrcParams *parUMC);

#endif
#endif



