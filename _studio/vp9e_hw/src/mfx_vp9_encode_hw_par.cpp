//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#include "mfxvp9.h"
#include "mfx_enc_common.h"
#include "mfx_vp9_encode_hw_par.h"
#include "mfx_vp9_encode_hw_utils.h"
#include <math.h>
#include <memory.h>
#include "mfx_common_int.h"
#include "mfx_ext_buffers.h"

#if defined (PRE_SI_TARGET_PLATFORM_GEN10)

namespace MfxHwVP9Encode
{

bool IsExtBufferSupportedInInit(mfxU32 id)
{
    return id == MFX_EXTBUFF_CODING_OPTION_VP9;
}

bool IsExtBufferSupportedInRuntime(mfxU32 id)
{
    id;
    return false;
}

mfxStatus CheckExtBufferHeaders(mfxVideoParam const &par, bool isRuntime)
{
    for (mfxU16 i = 0; i < par.NumExtParam; i++)
    {
        mfxExtBuffer *pBuf = par.ExtParam[i];

        // check that NumExtParam complies with ExtParam
        MFX_CHECK_NULL_PTR1(par.ExtParam[i]);

        // check that there is no ext buffer duplication in ExtParam
        for (mfxU16 j = i + 1; j < par.NumExtParam; j++)
        {
            if (par.ExtParam[j]->BufferId == pBuf->BufferId)
            {
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }

        bool isSupported = isRuntime ?
            IsExtBufferSupportedInRuntime(pBuf->BufferId) : IsExtBufferSupportedInInit(pBuf->BufferId);

        if (!isSupported)
        {
            return MFX_ERR_UNSUPPORTED;
        }
    }

    return MFX_ERR_NONE;
}

#define _CopyPar(pDst, pSrc, PAR) pDst->PAR = pSrc->PAR;
#define _SetOrCopyPar(PAR)\
if (pSrc)\
{\
    _CopyPar(pDst, pSrc, PAR); \
}\
else\
{\
    pDst->PAR = 1; \
}

inline mfxStatus SetOrCopySupportedParams(mfxInfoMFX *pDst, mfxInfoMFX *pSrc = 0)
{
    Zero(*pDst);

    MFX_CHECK_NULL_PTR1(pDst);

    _SetOrCopyPar(FrameInfo.Width);
    _SetOrCopyPar(FrameInfo.Height);
    _SetOrCopyPar(FrameInfo.CropW);
    _SetOrCopyPar(FrameInfo.CropH);
    _SetOrCopyPar(FrameInfo.CropX);
    _SetOrCopyPar(FrameInfo.CropY);
    _SetOrCopyPar(FrameInfo.FrameRateExtN);
    _SetOrCopyPar(FrameInfo.FrameRateExtD);
    _SetOrCopyPar(FrameInfo.AspectRatioW);
    _SetOrCopyPar(FrameInfo.AspectRatioH);
    _SetOrCopyPar(FrameInfo.ChromaFormat);
    _SetOrCopyPar(FrameInfo.PicStruct);
    _SetOrCopyPar(FrameInfo.FourCC);

    _SetOrCopyPar(LowPower);
    _SetOrCopyPar(CodecId);
    _SetOrCopyPar(CodecProfile);
    _SetOrCopyPar(TargetUsage);
    _SetOrCopyPar(GopPicSize);
    _SetOrCopyPar(GopRefDist);
    _SetOrCopyPar(RateControlMethod);
    _SetOrCopyPar(BufferSizeInKB);
    _SetOrCopyPar(InitialDelayInKB);
    _SetOrCopyPar(QPI);
    _SetOrCopyPar(TargetKbps);
    _SetOrCopyPar(MaxKbps);
    _SetOrCopyPar(BRCParamMultiplier);
    _SetOrCopyPar(NumRefFrame);

    return MFX_ERR_NONE;
}

inline mfxStatus SetOrCopySupportedParams(mfxExtCodingOptionVP9 *pDst, mfxExtCodingOptionVP9 *pSrc = 0)
{
    MFX_CHECK_NULL_PTR1(pDst);

    ZeroExtBuffer(*pDst);

    _SetOrCopyPar(SharpnessLevel);
    _SetOrCopyPar(WriteIVFHeaders);
    _SetOrCopyPar(QIndexDeltaLumaDC);
    _SetOrCopyPar(QIndexDeltaChromaAC);
    _SetOrCopyPar(QIndexDeltaChromaDC);
    _SetOrCopyPar(WriteIVFHeaders);
    _SetOrCopyPar(WriteIVFHeaders);
    _SetOrCopyPar(WriteIVFHeaders);

    for (mfxU8 i = 0; i < MAX_REF_LF_DELTAS; i++)
    {
        _SetOrCopyPar(LoopFilterRefDelta[i]);
    }
    for (mfxU8 i = 0; i < MAX_MODE_LF_DELTAS; i++)
    {
        _SetOrCopyPar(LoopFilterModeDelta[i]);
    }

    return MFX_ERR_NONE;
}

mfxStatus SetSupportedParameters(mfxVideoParam & par)
{
    par.AsyncDepth = 1;
    par.IOPattern = 1;
    par.Protected = 0;
    memset(par.reserved, 0, sizeof(par.reserved));
    memset(&par.reserved2, 0, sizeof(par.reserved2));
    memset(&par.reserved3, 0, sizeof(par.reserved3));

    // mfxInfoMfx
    SetOrCopySupportedParams(&par.mfx);

    // extended buffers
    MFX_CHECK_STS(CheckExtBufferHeaders(par));

    mfxExtCodingOptionVP9 *pOpt = GetExtBuffer(par);
    if (pOpt != 0)
    {
        SetOrCopySupportedParams(pOpt);
    }

    return MFX_ERR_NONE;
}

struct Bool
{
    Bool(bool initValue) : value(initValue) {};
    Bool & operator=(bool newValue)
    {
        value = newValue;
        return *this;
    };

    bool operator==(bool valueToCheck)
    {
        return (value == valueToCheck);
    };

    bool value;
};

bool CheckTriStateOption(mfxU16 & opt)
{
    if (opt != MFX_CODINGOPTION_UNKNOWN &&
        opt != MFX_CODINGOPTION_ON &&
        opt != MFX_CODINGOPTION_OFF)
    {
        opt = MFX_CODINGOPTION_UNKNOWN;
        return false;
    }

    return true;
}

template <class T, class U>
bool CheckRangeDflt(T & opt, U min, U max, U deflt)
{
    if (opt < static_cast<T>(min) || opt > static_cast<T>(max))
    {
        opt = static_cast<T>(deflt);
        return false;
    }

    return true;
}

mfxStatus CleanOutUnsupportedParameters(VP9MfxVideoParam &par)
{
    mfxStatus sts = MFX_ERR_NONE;

    VP9MfxVideoParam tmp = par;
    MFX_CHECK_STS(SetOrCopySupportedParams(&par.mfx, &tmp.mfx));
    if (memcmp(&par.mfx, &tmp.mfx, sizeof(mfxInfoMFX)))
    {
        sts = MFX_ERR_UNSUPPORTED;
    }

    mfxExtCodingOptionVP9 &optTmp = GetExtBufferRef(tmp);
    mfxExtCodingOptionVP9 &optPar = GetExtBufferRef(par);
    SetOrCopySupportedParams(&optPar, &optTmp);
    if (memcmp(&optPar, &optTmp, sizeof(mfxExtCodingOptionVP9)))
    {
        sts = MFX_ERR_UNSUPPORTED;
    }

    return sts;
}

mfxStatus CheckParameters(VP9MfxVideoParam &par, ENCODE_CAPS_VP9 const &caps)
{
    caps;

    Bool changed = false;
    Bool unsupported = false;

    if (MFX_ERR_UNSUPPORTED == CleanOutUnsupportedParameters(par))
    {
        unsupported = true;
    }

    if (par.IOPattern &&
        par.IOPattern != MFX_IOPATTERN_IN_VIDEO_MEMORY &&
        par.IOPattern != MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        par.IOPattern = 0;
        unsupported = true;
    }

    // check mfxInfoMfx
    double      frameRate = 0.0;

    if ((par.mfx.FrameInfo.Width & 0x0f) != 0 || (par.mfx.FrameInfo.Height & 0x0f) != 0)
    {
        par.mfx.FrameInfo.Width = 0;
        par.mfx.FrameInfo.Height = 0;
        unsupported = true;
    }

    if (par.mfx.FrameInfo.CropX + par.mfx.FrameInfo.CropW > par.mfx.FrameInfo.Width ||
        par.mfx.FrameInfo.CropY + par.mfx.FrameInfo.CropH > par.mfx.FrameInfo.Height)
    {
        par.mfx.FrameInfo.CropX = 0;
        par.mfx.FrameInfo.CropW = 0;
        par.mfx.FrameInfo.CropY = 0;
        par.mfx.FrameInfo.CropH = 0;
        unsupported = true;
    }

    if (par.mfx.FrameInfo.FrameRateExtD != 0)
    {
        frameRate = (double)par.mfx.FrameInfo.FrameRateExtN / (double)par.mfx.FrameInfo.FrameRateExtD;
    }

    if (par.mfx.FrameInfo.FrameRateExtD != 0 &&
        (frameRate < 1.0 || frameRate > 180.0))
    {
        par.mfx.FrameInfo.FrameRateExtN = 0;
        par.mfx.FrameInfo.FrameRateExtD = 0;
        unsupported = true;
    }

    if (par.mfx.FrameInfo.AspectRatioH != 0 || par.mfx.FrameInfo.AspectRatioW != 0)
    {
        if (!(par.mfx.FrameInfo.AspectRatioH == 1 && par.mfx.FrameInfo.AspectRatioW == 1))
        {
            par.mfx.FrameInfo.AspectRatioH = 1;
            par.mfx.FrameInfo.AspectRatioW = 1;
            changed = true;
        }
    }

    if (par.mfx.FrameInfo.PicStruct > MFX_PICSTRUCT_PROGRESSIVE)
    {
        par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        unsupported = true;
    }

    if (par.mfx.FrameInfo.ChromaFormat > MFX_CHROMAFORMAT_YUV420)
    {
        par.mfx.FrameInfo.ChromaFormat = 0;
        unsupported = true;
    }

    if (par.mfx.CodecProfile > MFX_PROFILE_VP9_0)
    {
        par.mfx.CodecProfile = MFX_PROFILE_UNKNOWN;
        unsupported = true;
    }

    if (par.mfx.NumThread > 1)
    {
        par.mfx.NumThread = 0;
        unsupported = true;
    }

    if (par.mfx.TargetUsage > MFX_TARGETUSAGE_BEST_SPEED)
    {
        par.mfx.TargetUsage = MFX_TARGETUSAGE_UNKNOWN;
        unsupported = true;
    }

    if (par.mfx.GopRefDist > 1)
    {
        par.mfx.GopRefDist = 1;
        changed = true;
    }

    if (par.mfx.RateControlMethod != 0)
    {
        if (par.mfx.RateControlMethod > MFX_RATECONTROL_CQP)
        {
            par.mfx.RateControlMethod = 0;
            par.mfx.InitialDelayInKB = 0;
            par.mfx.BufferSizeInKB = 0;
            par.mfx.TargetKbps = 0;
            par.mfx.MaxKbps = 0;
            par.mfx.BRCParamMultiplier = 0;
            unsupported = true;
        }
        else if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR
                || par.mfx.RateControlMethod == MFX_RATECONTROL_VBR)
        {
            if ((par.mfx.RateControlMethod == MFX_RATECONTROL_CBR  && par.mfx.MaxKbps != par.mfx.TargetKbps) ||
                (par.mfx.RateControlMethod == MFX_RATECONTROL_VBR  && par.mfx.MaxKbps < par.mfx.TargetKbps))
            {
                par.mfx.MaxKbps = 0;
                changed = true;
            }

            if (par.mfx.InitialDelayInKB > par.mfx.BufferSizeInKB)
            {
                par.mfx.InitialDelayInKB = 0;
                unsupported = true;
            }
        }
        else if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        {
            if (par.mfx.QPI > MAX_Q_INDEX)
            {
                par.mfx.QPI = MAX_Q_INDEX;
                changed = true;
            }

            if (par.mfx.QPP > MAX_Q_INDEX)
            {
                par.mfx.QPP = MAX_Q_INDEX;
                changed = true;
            }

            if (par.mfx.QPB > 0)
            {
                par.mfx.QPB = 0;
                changed = true;
            }
        }
    }

    if (par.mfx.NumRefFrame > 3)
    {
        par.mfx.NumRefFrame = 3;
        changed = true;
    }

    // check extended buffers
    mfxExtCodingOptionVP9 &opt = GetExtBufferRef(par);
    if (false == CheckTriStateOption(opt.EnableMultipleSegments))
    {
        changed = true;
    }
    if (false == CheckTriStateOption(opt.WriteIVFHeaders))
    {
        changed = true;
    }

    for (mfxU8 i = 0; i < MAX_REF_LF_DELTAS; i++)
    {
        if (false == CheckRangeDflt(opt.LoopFilterRefDelta[i], -MAX_LF_LEVEL, MAX_LF_LEVEL, 0))
        {
            changed = true;
        }
    }

    for (mfxU8 i = 0; i < MAX_MODE_LF_DELTAS; i++)
    {
        if (false == CheckRangeDflt(opt.LoopFilterModeDelta[i], -MAX_LF_LEVEL, MAX_LF_LEVEL, 0))
        {
            changed = true;
        }
    }

    if (unsupported == true)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return (changed == true) ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
}

template <typename T>
inline bool SetDefault(T &par, int defaultValue)
{
    bool defaultSet = false;

    if (par == 0)
    {
        par = (T)defaultValue;
        defaultSet = true;
    }

    return defaultSet;
}

template <typename T>
inline bool SetTwoDefaults(T &par1, T &par2, int defaultValue1, int defaultValue2)
{
    bool defaultsSet = false;

    if (par1 == 0 && par2 == 0)
    {
        par1 = (T)defaultValue1;
        par2 = (T)defaultValue2;
        defaultsSet = true;
    }

    return defaultsSet;
}

inline bool IsBufferBasedBRC(mfxU16 brcMethod)
{
    return brcMethod == MFX_RATECONTROL_CBR
        || brcMethod == MFX_RATECONTROL_VBR;
}

inline mfxU16 GetDefaultBufferSize(VP9MfxVideoParam const &par)
{
    if (IsBufferBasedBRC(par.mfx.RateControlMethod))
    {
        const mfxU8 defaultBufferInSec = 2;
        return defaultBufferInSec * ((par.mfx.TargetKbps + 7) / 8);
    }
    else
    {
        mfxFrameInfo const &fi = par.mfx.FrameInfo;
        return (fi.Width * fi.Height * 3) / 2; // uncompressed frame size for 420 8 bit
    }
}

inline mfxU16 GetDefaultAsyncDepth(VP9MfxVideoParam const &par)
{
    par;
    return 1; // TODO: fix to appropriate value when async mode operation will be enabled.
}

#define DEFAULT_GOP_SIZE 0xffff
#define DEFAULT_FRAME_RATE 30

mfxStatus SetDefaults(VP9MfxVideoParam &par, ENCODE_CAPS_VP9 const &caps)
{
    SetDefault(par.AsyncDepth, GetDefaultAsyncDepth(par));

    // mfxInfoMfx
    SetDefault(par.mfx.CodecProfile, MFX_PROFILE_VP9_0);
    SetDefault(par.mfx.GopPicSize, DEFAULT_GOP_SIZE);
    SetDefault(par.mfx.GopRefDist, 1);
    SetDefault(par.mfx.NumRefFrame, 1);
    SetDefault(par.mfx.BRCParamMultiplier, 1);
    SetDefault(par.mfx.TargetUsage, MFX_TARGETUSAGE_BALANCED);
    SetDefault(par.mfx.RateControlMethod, MFX_RATECONTROL_CBR);
    SetDefault(par.mfx.BufferSizeInKB, GetDefaultBufferSize(par));
    if (IsBufferBasedBRC(par.mfx.RateControlMethod))
    {
        SetDefault(par.mfx.InitialDelayInKB, par.mfx.BufferSizeInKB / 2);
    }
    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
    {
        SetDefault(par.mfx.MaxKbps, par.mfx.TargetKbps);
    }

    // mfxInfomfx.FrameInfo
    mfxFrameInfo &fi = par.mfx.FrameInfo;

    if (false == SetTwoDefaults(fi.FrameRateExtN, fi.FrameRateExtD, 30, 1))
    {
        SetDefault(fi.FrameRateExtN, fi.FrameRateExtD * DEFAULT_FRAME_RATE);
        SetDefault(fi.FrameRateExtD,
            fi.FrameRateExtN % DEFAULT_FRAME_RATE == 0 ? fi.FrameRateExtN / DEFAULT_FRAME_RATE : 1);
    }
    SetTwoDefaults(fi.AspectRatioW, fi.AspectRatioH, 1, 1);
    SetDefault(fi.PicStruct, MFX_PICSTRUCT_PROGRESSIVE);
    SetDefault(fi.ChromaFormat, MFX_CHROMAFORMAT_YUV420);

    // ext buffers
    mfxExtCodingOptionVP9 &opt = GetExtBufferRef(par);
    SetDefault(opt.WriteIVFHeaders, MFX_CODINGOPTION_ON);
    SetDefault(opt.EnableMultipleSegments, MFX_CODINGOPTION_OFF);

    // check final set of parameters
    MFX_CHECK_STS(CheckParameters(par, caps));

    return MFX_ERR_NONE;
}

mfxStatus CheckParametersAndSetDefaults(VP9MfxVideoParam &par, ENCODE_CAPS_VP9 const &caps)
{
    // check mandatory parameters
    // (1) resolution
    mfxFrameInfo const &fi = par.mfx.FrameInfo;
    if (fi.Width == 0 || fi.Height == 0)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    // (2) target and max bitrates
    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR && par.mfx.TargetKbps == 0)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    if (par.mfx.RateControlMethod == MFX_RATECONTROL_VBR &&
        (par.mfx.TargetKbps == 0 || par.mfx.MaxKbps == 0))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    // (3) crops
    if (fi.CropW || fi.CropH || fi.CropX || fi.CropY)
    {
        if (fi.CropW == 0 || fi.CropH == 0)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }

    mfxStatus sts = MFX_ERR_NONE;

    // check parameters defined by application
    mfxStatus checkSts = MFX_ERR_NONE;
    checkSts = CheckParameters(par, caps);
    if (checkSts == MFX_ERR_UNSUPPORTED)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    MFX_CHECK(sts >= 0, sts);

    // set defaults for parameters not defined by application
    sts = SetDefaults(par, caps);
    MFX_CHECK(sts >= 0, sts);

    return checkSts;
}

mfxStatus CheckEncodeFrameParam(
    mfxVideoParam const & video,
    mfxEncodeCtrl       * ctrl,
    mfxFrameSurface1    * surface,
    mfxBitstream        * bs,
    bool                  isExternalFrameAllocator)
{
    mfxStatus checkSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(bs);

    if (surface != 0)
    {
        MFX_CHECK((surface->Data.Y == 0) == (surface->Data.UV == 0), MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(surface->Data.Y != 0 || isExternalFrameAllocator, MFX_ERR_UNDEFINED_BEHAVIOR);

        if (surface->Info.Width != video.mfx.FrameInfo.Width || surface->Info.Height != video.mfx.FrameInfo.Height)
            checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }
    else
    {
        checkSts = MFX_ERR_MORE_DATA;
    }

    MFX_CHECK((mfxU64)bs->MaxLength > ((mfxU64)bs->DataOffset + (mfxU64)bs->DataLength), MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(((mfxI32)bs->MaxLength - ((mfxI32)bs->DataOffset + (mfxI32)bs->DataLength) >= (mfxI32)video.mfx.BufferSizeInKB*1000), MFX_ERR_NOT_ENOUGH_BUFFER);

    if (ctrl)
    {
        MFX_CHECK (ctrl->QP <= 63, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK (ctrl->FrameType <= MFX_FRAMETYPE_P, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    }

    return checkSts;
}
} //namespace MfxHwVP9Encode

#endif // PRE_SI_TARGET_PLATFORM_GEN10
