/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2016 Intel Corporation. All Rights Reserved.

File Name: libmf_core_hw.cpp

\* ****************************************************************************** */

#include "mfx_common.h"
#if defined (MFX_VA_WIN)

#include "libmfx_core_hw.h"
#include "umc_va_dxva2.h"
#include "mfx_common_decode_int.h"
#include "mfx_enc_common.h"
#include "mfxpcp.h"

using namespace UMC;

VideoAccelerationHW ConvertMFXToUMCType(eMFXHWType type)
{
    VideoAccelerationHW umcType = VA_HW_UNKNOWN;

    switch(type)
    {
    case MFX_HW_LAKE:
        umcType = VA_HW_LAKE;
        break;
    case MFX_HW_LRB:
        umcType = VA_HW_LRB;
        break;
    case MFX_HW_SNB:
        umcType = VA_HW_SNB;
        break;
    case MFX_HW_IVB:
        umcType = VA_HW_IVB;
        break;
    case MFX_HW_HSW:
        umcType = VA_HW_HSW;
        break;
    case MFX_HW_HSW_ULT:
        umcType = VA_HW_HSW_ULT;
        break;
    case MFX_HW_VLV:
        umcType = VA_HW_VLV;
        break;
    case MFX_HW_BDW:
        umcType = VA_HW_BDW;
        break;
    case MFX_HW_CHV:
        umcType = VA_HW_CHV;
        break;
    case MFX_HW_SCL:
        umcType = VA_HW_SCL;
        break;
    case MFX_HW_BXT:
        umcType = VA_HW_BXT;
        break;
    case MFX_HW_KBL:
        umcType = VA_HW_KBL;
        break;
    case MFX_HW_CNL:
        umcType = VA_HW_CNL;
        break;
    case MFX_HW_SOFIA:
        umcType = VA_HW_SOFIA;
    case MFX_HW_ICL:
        umcType = VA_HW_ICL;
        break;
    case MFX_HW_ICL_LP:
        umcType = VA_HW_ICL_LP;
        break;
    }

    return umcType;
}

mfxU32 ChooseProfile(mfxVideoParam * param, eMFXHWType )
{
    mfxU32 profile = UMC::VA_VLD;

    // video accelerator is needed for decoders only
    switch (param->mfx.CodecId)
    {
    case MFX_CODEC_VC1:
        profile |= VA_VC1;
        break;
    case MFX_CODEC_MPEG2:
        profile |= VA_MPEG2;
        break;
    case MFX_CODEC_AVC:
        {
        profile |= VA_H264;

        mfxU32 profile_idc = ExtractProfile(param->mfx.CodecProfile);

        if (IsMVCProfile(profile_idc))
        {
            mfxExtMVCSeqDesc * points = (mfxExtMVCSeqDesc *)GetExtendedBuffer(param->ExtParam, param->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);

            if (profile_idc == MFX_PROFILE_AVC_MULTIVIEW_HIGH && points && points->NumView > 2)
                profile |= VA_PROFILE_MVC_MV;
            else
                profile |= param->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? VA_PROFILE_MVC_STEREO_PROG : VA_PROFILE_MVC_STEREO;
        }

        if (IsSVCProfile(profile_idc))
        {
            profile |= (profile_idc == MFX_PROFILE_AVC_SCALABLE_HIGH) ? VA_PROFILE_SVC_HIGH : VA_PROFILE_SVC_BASELINE;
        }

        if (IS_PROTECTION_WIDEVINE(param->Protected))
        {
            profile |= VA_PROFILE_WIDEVINE;
        }

        }
        break;
    case MFX_CODEC_JPEG:
        profile |= VA_JPEG;
        break;
    case MFX_CODEC_VP8:
        profile |= VA_VP8;
        break;
    case MFX_CODEC_VP9:
        profile |= VA_VP9;
        if (param->mfx.FrameInfo.FourCC == MFX_FOURCC_P010)
        {
            profile |= VA_PROFILE_10;
        }
        break;
    case MFX_CODEC_HEVC:
        profile |= VA_H265;

        if (param->mfx.FrameInfo.FourCC == MFX_FOURCC_P010)
        {
            profile |= VA_PROFILE_10;
        }
        if (IS_PROTECTION_WIDEVINE(param->Protected))
        {
            profile |= VA_PROFILE_WIDEVINE;
        }
        break;

    default:
        return 0;
    }

    return profile;
}

bool IsHwMvcEncSupported()
{
    return true;
}

#endif
/* EOF */
