/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2005-2008 Intel Corporation. All Rights Reserved.
//
//
*/

#ifndef __VIDEO_CC_UTILS_H
#define __VIDEO_CC_UTILS_H

#include "mfxdefs.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"

mfxStatus   GetPitch            (mfxU32 FourCC, mfxU32  Width ,mfxU16* pPitch);
mfxStatus   CF_GetChromaFormat  (mfxU32 FourCC, mfxU8*  pChromaFormat);

mfxStatus   GetFramePointers    (mfxU32 FourCC, mfxU16 w, mfxU16 h, mfxU8 *pBuffer,
                                 mfxU8** pPointers);
mfxStatus   GetPlanes           (mfxU32 FourCC, mfxU8* pNPlanes, mfxU8* pFrameNum);
mfxI32      GetSubPicOfset      (mfxI16 CropX, mfxI16 CropY, mfxU16 Pitch, mfxU16 Step);

#endif
