/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008 Intel Corporation. All Rights Reserved.

File Name: mfx_mpeg2_bsd_utils.h

\* ****************************************************************************** */
#ifndef __MFX_MPEG2_BSD_UTILS_H__
#define __MFX_MPEG2_BSD_UTILS_H__

#include "mfx_common.h"
#if defined (MFX_ENABLE_MPEG2_VIDEO_BSD)

#include "mfxvideo.h"
#include "umc_mpeg2_dec_defs.h"

//#define MFX_BSD_UNSUPPORTED(MSG) { vm_string_printf(VM_STRING("%s\n"), VM_STRING(#MSG)); return MFX_ERR_UNSUPPORTED; }
#define MFX_BSD_UNSUPPORTED(MSG) { return MFX_ERR_UNSUPPORTED; }

namespace MfxMpeg2Bsd
{
    enum { MFX_TIME_STAMP_FACTOR = 30000 };

    void ClearMfxStruct(mfxVideoParam *par);
    void ClearMfxStruct(mfxFrameParam *par);
    void ClearMfxStruct(mfxSliceParam *par);
    mfxU8 GetMfxSliceTypeType(mfxU8 frameType);
    mfxU8 GetMfxMvQuantity(mfxU8 picStruct, mfxU8 mbType, mfxU8 motionType);
    mfxU8 GetMfxFieldMbFlag(mfxU8 picStruct, mfxU8 motionType);
    mfxU8 GetMfxMbDataSize128b(mfxU16 numCoef);
    mfxU16 GetMfxMbDataOffset32b(mfxHDL ptr, mfxHDL base);
    mfxU16 GetMfxCodedPattern4x4Y(mfxU32 umcCodedPattern, mfxU8 ChromaFormatIdc);
    mfxU16 GetMfxCodedPattern4x4U(mfxU32 umcCodedPattern, mfxU8 ChromaFormatIdc);
    mfxU16 GetMfxCodedPattern4x4V(mfxU32 umcCodedPattern, mfxU8 ChromaFormatIdc);
    mfxU16 GetMfxMbType5bits(mfxU8 picStruct, Ipp8u mbType, Ipp8u motionType);

} //namespace

#endif
#endif
