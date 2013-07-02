//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008-2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfxdefs.h"
#include "mfx_h265_encode.h"
#include "mfx_common_int.h"
#include "mfx_task.h"
#include "mfx_brc_common.h"
#include "mfx_session.h"
#include "mfx_tools.h"
#include "vm_thread.h"
#include "vm_interlocked.h"
#include "mfx_ext_buffers.h"
#include <new>

#include "mfx_h265_defs.h"
#include "umc_structures.h"
#include "mfx_enc_common.h"

//////////////////////////////////////////////////////////////////////////

#define H265ENC_UNREFERENCED_PARAMETER(X) X=X

#define CHECK_VERSION(ver)
#define CHECK_CODEC_ID(id, myid)
#define CHECK_FUNCTION_ID(id, myid)

//////////////////////////////////////////////////////////////////////////

void mfxVideoH265InternalParam::SetCalcParams( mfxVideoParam *parMFX) {

    mfxU16 mult = IPP_MAX( parMFX->mfx.BRCParamMultiplier, 1);

    calcParam.TargetKbps = parMFX->mfx.TargetKbps * mult;
    calcParam.MaxKbps = parMFX->mfx.MaxKbps * mult;
    calcParam.BufferSizeInKB = parMFX->mfx.BufferSizeInKB * mult;
    calcParam.InitialDelayInKB = parMFX->mfx.InitialDelayInKB * mult;
}
void mfxVideoH265InternalParam::GetCalcParams( mfxVideoParam *parMFX) {

    mfxU32 maxVal = IPP_MAX( IPP_MAX( calcParam.TargetKbps, calcParam.MaxKbps), IPP_MAX( calcParam.BufferSizeInKB, calcParam.InitialDelayInKB));
    mfxU16 mult = (mfxU16)((maxVal + 0xffff) / 0x10000);

    if (mult) {
        parMFX->mfx.BRCParamMultiplier = mult;
        parMFX->mfx.TargetKbps = (mfxU16)(calcParam.TargetKbps / mult);
        parMFX->mfx.MaxKbps = (mfxU16)(calcParam.MaxKbps / mult);
        parMFX->mfx.BufferSizeInKB = (mfxU16)(calcParam.BufferSizeInKB / mult);
        parMFX->mfx.InitialDelayInKB = (mfxU16)(calcParam.InitialDelayInKB / mult);
    }
}

mfxVideoH265InternalParam::mfxVideoH265InternalParam()
{
    memset(this, 0, sizeof(*this));
}

mfxVideoH265InternalParam::mfxVideoH265InternalParam(mfxVideoParam const & par)
{
    mfxVideoParam & base = *this;
    base = par;
    SetCalcParams( &base);
}

mfxVideoH265InternalParam& mfxVideoH265InternalParam::operator=(mfxVideoParam const & par)
{
    mfxVideoParam & base = *this;
    base = par;
    SetCalcParams( &base);
    return *this;
}

inline Ipp32u h265enc_ConvertBitrate(mfxU16 TargetKbps)
{
    return (TargetKbps * 1000);
}

//////////////////////////////////////////////////////////////////////////

MFXVideoENCODEH265::MFXVideoENCODEH265(VideoCORE *core, mfxStatus *stat)
: VideoENCODE(),
  m_isInitialized(false),
  m_core(core)
{
    ippStaticInit();
    *stat = MFX_ERR_NONE;
}

MFXVideoENCODEH265::~MFXVideoENCODEH265()
{
    Close();
}

mfxStatus MFXVideoENCODEH265::EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams)
{
    mfxStatus st = MFX_ERR_NONE;

    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(bs);
    MFX_CHECK_NULL_PTR1(bs->Data);

    H265ENC_UNREFERENCED_PARAMETER(pInternalParams);
    MFX_CHECK(bs->MaxLength > (bs->DataOffset + bs->DataLength),MFX_ERR_UNDEFINED_BEHAVIOR);

    if (surface)
    { // check frame parameters
        MFX_CHECK(surface->Info.ChromaFormat == m_mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(surface->Info.Width  == m_mfxVideoParam.mfx.FrameInfo.Width, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(surface->Info.Height == m_mfxVideoParam.mfx.FrameInfo.Height, MFX_ERR_INVALID_VIDEO_PARAM);

        if (surface->Data.Y)
        {
            MFX_CHECK (surface->Data.Pitch < 0x8000 && surface->Data.Pitch!=0, MFX_ERR_UNDEFINED_BEHAVIOR);
        }
    }

    *reordered_surface = surface;

    if (m_mfxVideoParam.mfx.EncodedOrder) {
        return MFX_ERR_UNSUPPORTED;
    } else {
        if (ctrl && (ctrl->FrameType != MFX_FRAMETYPE_UNKNOWN) && ((ctrl->FrameType & 0xff) != (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR)))
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        m_frameCountSync++;
        if (surface)
            m_core->IncreaseReference(&(surface->Data));

        mfxU32 noahead = 1;

        if (!surface) {
            if (m_frameCountBufferedSync == 0) { // buffered frames to be encoded
                return MFX_ERR_MORE_DATA;
            }
            m_frameCountBufferedSync --;
        }
        else if (m_frameCountSync > noahead && m_frameCountBufferedSync <
            (mfxU32)m_mfxVideoParam.mfx.GopRefDist - noahead ) {
            m_frameCountBufferedSync++;
            return (mfxStatus)MFX_ERR_MORE_DATA_RUN_TASK;
        }
    }

    return st;
}

mfxStatus MFXVideoENCODEH265::Init(mfxVideoParam* par_in)
{
    mfxStatus sts;
    MFX_CHECK(!m_isInitialized, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK_NULL_PTR1(par_in);
    MFX_CHECK(par_in->Protected == 0,MFX_ERR_INVALID_VIDEO_PARAM);

    sts = CheckVideoParamEncoders(par_in, m_core->IsExternalFrameAllocator(), MFX_HW_UNKNOWN);
    MFX_CHECK_STS(sts);

    mfxExtCodingOption* opts_in = GetExtCodingOptions( par_in->ExtParam, par_in->NumExtParam );
    mfxExtCodingOptionHEVC* opts_hevc = (mfxExtCodingOptionHEVC*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_HEVCENC );

    mfxVideoH265InternalParam inInt = *par_in;
    mfxVideoH265InternalParam *par = &inInt;

    if (!par->mfx.BufferSizeInKB)
        par->mfx.BufferSizeInKB = 100; //temp

    memset(&m_mfxVideoParam,0,sizeof(mfxVideoParam));
    if (opts_in) m_mfxExtOpts = *opts_in;
    if (opts_hevc) m_mfxHEVCOpts = *opts_hevc;

    m_mfxVideoParam.mfx = par->mfx;
    m_mfxVideoParam.calcParam = par->calcParam;
    m_mfxVideoParam.IOPattern = par->IOPattern;
    m_mfxVideoParam.Protected = 0;
    m_mfxVideoParam.AsyncDepth = par->AsyncDepth;

    m_frameCountSync = 0;
    m_frameCount = 0;
    m_frameCountBufferedSync = 0;
    m_frameCountBuffered = 0;

    m_enc = new H265Encoder();
    MFX_CHECK_STS_ALLOC(m_enc);

    sts = InitEnc();
    MFX_CHECK_STS(sts);

    m_isInitialized = true;

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::Close()
{
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);
    if (m_enc) {
        CloseEnc();
        delete m_enc;
        m_enc = NULL;
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::Reset(mfxVideoParam *par_in)
{
    mfxStatus sts;

    MFX_CHECK_NULL_PTR1(par_in);
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);

    m_frameCountSync = 0;
    m_frameCount = 0;
    m_frameCountBufferedSync = 0;
    m_frameCountBuffered = 0;

    sts = ResetEnc();
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::Query(mfxVideoParam *par_in, mfxVideoParam *par_out)
{
    mfxU32 isCorrected = 0;
    mfxU32 isInvalid = 0;
    MFX_CHECK_NULL_PTR1(par_out)
    CHECK_VERSION(par_in->Version);
    CHECK_VERSION(par_out->Version);

    //First check for zero input
    if( par_in == NULL ){ //Set ones for filed that can be configured
        mfxVideoParam *out = par_out;
        memset( &out->mfx, 0, sizeof( mfxInfoMFX ));
        out->mfx.FrameInfo.FourCC = 1;
        out->mfx.FrameInfo.Width = 1;
        out->mfx.FrameInfo.Height = 1;
        out->mfx.FrameInfo.CropX = 1;
        out->mfx.FrameInfo.CropY = 1;
        out->mfx.FrameInfo.CropW = 1;
        out->mfx.FrameInfo.CropH = 1;
        out->mfx.FrameInfo.FrameRateExtN = 1;
        out->mfx.FrameInfo.FrameRateExtD = 1;
        out->mfx.FrameInfo.AspectRatioW = 1;
        out->mfx.FrameInfo.AspectRatioH = 1;
        out->mfx.FrameInfo.PicStruct = 1;
        out->mfx.FrameInfo.ChromaFormat = 1;
        out->mfx.CodecId = MFX_CODEC_HEVC; // restore cleared mandatory
        out->mfx.CodecLevel = 1;
        out->mfx.CodecProfile = 1;
        out->mfx.NumThread = 1;
        out->mfx.TargetUsage = 1;
        out->mfx.GopPicSize = 1;
        out->mfx.GopRefDist = 1;
        out->mfx.GopOptFlag = 1;
        out->mfx.IdrInterval = 1;
        out->mfx.RateControlMethod = 1;
        out->mfx.InitialDelayInKB = 1;
        out->mfx.BufferSizeInKB = 1;
        out->mfx.TargetKbps = 1;
        out->mfx.MaxKbps = 1;
        out->mfx.NumSlice = 1;
        out->mfx.NumRefFrame = 1;
        out->mfx.EncodedOrder = 1;
        out->AsyncDepth = 1;
        out->IOPattern = 1;
        out->Protected = 0;
        // ignore all reserved
    } else { //Check options for correctness
        mfxU16 cropSampleMask, correctCropTop, correctCropBottom;
        mfxVideoH265InternalParam inInt = *par_in;
        mfxVideoH265InternalParam outInt = *par_out;
        mfxVideoH265InternalParam *in = &inInt;
        mfxVideoH265InternalParam *out = &outInt;
        mfxVideoH265InternalParam par_SPSPPS = *par_in;

        if ( in->mfx.CodecId != MFX_CODEC_HEVC)
            return MFX_ERR_UNSUPPORTED;

        out->mfx.NumThread = in->mfx.NumThread;

        if( in->mfx.TargetUsage < MFX_TARGETUSAGE_UNKNOWN || in->mfx.TargetUsage > MFX_TARGETUSAGE_BEST_SPEED) {
            isCorrected ++;
            out->mfx.TargetUsage = MFX_TARGETUSAGE_UNKNOWN;
        }
        else out->mfx.TargetUsage = in->mfx.TargetUsage;

        if (in->mfx.RateControlMethod != 0 &&
            in->mfx.RateControlMethod != MFX_RATECONTROL_CBR && in->mfx.RateControlMethod != MFX_RATECONTROL_VBR && in->mfx.RateControlMethod != MFX_RATECONTROL_CQP && in->mfx.RateControlMethod != MFX_RATECONTROL_AVBR) {
            out->mfx.RateControlMethod = MFX_RATECONTROL_VBR;
            isCorrected ++;
        } else out->mfx.RateControlMethod = in->mfx.RateControlMethod;

        out->calcParam.BufferSizeInKB = in->calcParam.BufferSizeInKB;
        if (out->mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
            out->mfx.RateControlMethod != MFX_RATECONTROL_AVBR) {
            out->calcParam.TargetKbps = in->calcParam.TargetKbps;
            out->calcParam.InitialDelayInKB = in->calcParam.InitialDelayInKB;
            if (out->mfx.FrameInfo.Width && out->mfx.FrameInfo.Height && out->mfx.FrameInfo.FrameRateExtD && out->calcParam.TargetKbps) {
                // last denominator 1000 gives about 0.75 Mbps for 1080p x 30
                mfxU32 minBitRate = (mfxU32)((mfxF64)out->mfx.FrameInfo.Width * out->mfx.FrameInfo.Height * 12 // size of raw image (luma + chroma 420) in bits
                                             * out->mfx.FrameInfo.FrameRateExtN / out->mfx.FrameInfo.FrameRateExtD / 1000 / 1000);
                if (minBitRate > out->calcParam.TargetKbps) {
                    out->calcParam.TargetKbps = (mfxU32)minBitRate;
                    isCorrected ++;
                }
                mfxU32 AveFrameKB = out->calcParam.TargetKbps * out->mfx.FrameInfo.FrameRateExtD / out->mfx.FrameInfo.FrameRateExtN / 8;
                if (out->calcParam.BufferSizeInKB != 0 && out->calcParam.BufferSizeInKB < 2 * AveFrameKB) {
                    out->calcParam.BufferSizeInKB = (mfxU32)(2 * AveFrameKB);
                    isCorrected ++;
                }
                if (out->calcParam.InitialDelayInKB != 0 && out->calcParam.InitialDelayInKB < AveFrameKB) {
                    out->calcParam.InitialDelayInKB = (mfxU32)AveFrameKB;
                    isCorrected ++;
                }
            }

            if (in->calcParam.MaxKbps != 0 && in->calcParam.MaxKbps < out->calcParam.TargetKbps) {
                out->calcParam.MaxKbps = out->calcParam.TargetKbps;
            } else
                out->calcParam.MaxKbps = in->calcParam.MaxKbps;
        }
        // check for correct QPs for const QP mode
        else if (out->mfx.RateControlMethod == MFX_RATECONTROL_CQP) {
            if (in->mfx.QPI > 51) {
                in->mfx.QPI = 0;
                isInvalid ++;
            }
            if (in->mfx.QPP > 51) {
                in->mfx.QPP = 0;
                isInvalid ++;
            }
            if (in->mfx.QPB > 51) {
                in->mfx.QPB = 0;
                isInvalid ++;
            }
        }
        else {
            out->calcParam.TargetKbps = in->calcParam.TargetKbps;
            out->mfx.Accuracy = in->mfx.Accuracy;
            out->mfx.Convergence = in->mfx.Convergence;
        }

        out->mfx.EncodedOrder = in->mfx.EncodedOrder;

        // Check crops
        if (in->mfx.FrameInfo.CropH > in->mfx.FrameInfo.Height && in->mfx.FrameInfo.Height) {
            out->mfx.FrameInfo.CropH = 0;
            isInvalid ++;
        } else
            out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH;
        if (in->mfx.FrameInfo.CropW > in->mfx.FrameInfo.Width && in->mfx.FrameInfo.Width) {
            out->mfx.FrameInfo.CropW = 0;
            isInvalid ++;
        } else
            out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW;
        if (in->mfx.FrameInfo.CropX + in->mfx.FrameInfo.CropW > in->mfx.FrameInfo.Width) {
            out->mfx.FrameInfo.CropX = 0;
            isInvalid ++;
        } else
            out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX;
        if (in->mfx.FrameInfo.CropY + in->mfx.FrameInfo.CropH > in->mfx.FrameInfo.Height) {
            out->mfx.FrameInfo.CropY = 0;
            isInvalid ++;
        } else
            out->mfx.FrameInfo.CropY = in->mfx.FrameInfo.CropY;

        // Assume 420 checking horizontal crop to be even
        if ((in->mfx.FrameInfo.CropX & 1) && (in->mfx.FrameInfo.CropW & 1)) {
            if (out->mfx.FrameInfo.CropX == in->mfx.FrameInfo.CropX) // not to correct CropX forced to zero
                out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX + 1;
            if (out->mfx.FrameInfo.CropW) // can't decrement zero CropW
                out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW - 1;
            isCorrected ++;
        } else if (in->mfx.FrameInfo.CropX & 1)
        {
            if (out->mfx.FrameInfo.CropX == in->mfx.FrameInfo.CropX) // not to correct CropX forced to zero
                out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX + 1;
            if (out->mfx.FrameInfo.CropW) // can't decrement zero CropW
                out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW - 2;
            isCorrected ++;
        }
        else if (in->mfx.FrameInfo.CropW & 1) {
            if (out->mfx.FrameInfo.CropW) // can't decrement zero CropW
                out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW - 1;
            isCorrected ++;
        }

        // Assume 420 checking vertical crop to be even for progressive and multiple of 4 for other PicStruct
        if ((in->mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF))  == MFX_PICSTRUCT_PROGRESSIVE)
            cropSampleMask = 1;
        else
            cropSampleMask = 3;

        correctCropTop = (in->mfx.FrameInfo.CropY + cropSampleMask) & ~cropSampleMask;
        correctCropBottom = (in->mfx.FrameInfo.CropY + out->mfx.FrameInfo.CropH) & ~cropSampleMask;

        if ((in->mfx.FrameInfo.CropY & cropSampleMask) || (out->mfx.FrameInfo.CropH & cropSampleMask)) {
            if (out->mfx.FrameInfo.CropY == in->mfx.FrameInfo.CropY) // not to correct CropY forced to zero
                out->mfx.FrameInfo.CropY = correctCropTop;
            if (correctCropBottom >= out->mfx.FrameInfo.CropY)
                out->mfx.FrameInfo.CropH = correctCropBottom - out->mfx.FrameInfo.CropY;
            else // CropY < cropSample
                out->mfx.FrameInfo.CropH = 0;
            isCorrected ++;
        }

        out->mfx.FrameInfo.AspectRatioW = in->mfx.FrameInfo.AspectRatioW;
        out->mfx.FrameInfo.AspectRatioH = in->mfx.FrameInfo.AspectRatioH;

        if (in->mfx.FrameInfo.ChromaFormat != 0 && in->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420) {
            isCorrected ++;
            out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        } else
            out->mfx.FrameInfo.ChromaFormat = in->mfx.FrameInfo.ChromaFormat;

        if( in->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
//            in->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_TFF &&
//            in->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_BFF &&
            in->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_UNKNOWN ) {
            out->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_UNKNOWN;
            isCorrected ++;
        } else out->mfx.FrameInfo.PicStruct = in->mfx.FrameInfo.PicStruct;

        if (out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE && (out->mfx.FrameInfo.Height & 16)) {
            out->mfx.FrameInfo.PicStruct = 0;
            out->mfx.FrameInfo.Height = 0;
            isInvalid ++;
        }

        out->mfx.NumRefFrame = in->mfx.NumRefFrame;


        mfxVideoH265InternalParam checked = *out;

        if (out->mfx.CodecProfile != MFX_PROFILE_UNKNOWN && out->mfx.CodecProfile != checked.mfx.CodecProfile) {
            out->mfx.CodecProfile = checked.mfx.CodecProfile;
            isCorrected ++;
        }
        if (out->mfx.CodecLevel != MFX_LEVEL_UNKNOWN && out->mfx.CodecLevel != checked.mfx.CodecLevel) {
            out->mfx.CodecLevel = checked.mfx.CodecLevel;
            isCorrected ++;
        }
        if (out->calcParam.TargetKbps != 0 && out->calcParam.TargetKbps != checked.calcParam.TargetKbps) {
            out->calcParam.TargetKbps = checked.calcParam.TargetKbps;
            isCorrected ++;
        }
        if (out->calcParam.MaxKbps != 0 && out->calcParam.MaxKbps != checked.calcParam.MaxKbps) {
            out->calcParam.MaxKbps = checked.calcParam.MaxKbps;
            isCorrected ++;
        }
        if (out->calcParam.BufferSizeInKB != 0 && out->calcParam.BufferSizeInKB != checked.calcParam.BufferSizeInKB) {
            out->calcParam.BufferSizeInKB = checked.calcParam.BufferSizeInKB;
            isCorrected ++;
        }
        if (out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_UNKNOWN && out->mfx.FrameInfo.PicStruct != checked.mfx.FrameInfo.PicStruct) {
            out->mfx.FrameInfo.PicStruct = checked.mfx.FrameInfo.PicStruct;
            isCorrected ++;
        }

        *par_out = *out;
        if (out->mfx.RateControlMethod != MFX_RATECONTROL_CQP)
            out->GetCalcParams(par_out);
    }

    if (isInvalid)
        return MFX_ERR_UNSUPPORTED;
    if (isCorrected)
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

    return (IsHWLib())? MFX_WRN_PARTIAL_ACCELERATION : MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR1(par)
    MFX_CHECK_NULL_PTR1(request)

    if (par->Protected != 0)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    mfxU16 nFrames = 1; // no reordering at all

    request->NumFrameMin = nFrames;
    request->NumFrameSuggested = IPP_MAX(nFrames,par->AsyncDepth);

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::GetEncodeStat(mfxEncodeStat *stat)
{
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(stat)
    memset(stat, 0, sizeof(mfxEncodeStat));
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::EncodeFrame(mfxEncodeCtrl *ctrl, mfxEncodeInternalParams *pInternalParams, mfxFrameSurface1 *inputSurface, mfxBitstream *bs)
{
    mfxStatus st;
    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxBitstream *bitstream = 0;
    mfxU8* dataPtr;
    mfxU32 initialDataLength = 0;
    mfxFrameSurface1 *surface = inputSurface;

    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);

    // inputSurface can be opaque. Get real surface from core
//    surface = GetOriginalSurface(inputSurface);

    if (inputSurface != surface) { // input surface is opaque surface
        surface->Data.FrameOrder = inputSurface->Data.FrameOrder;
        surface->Data.TimeStamp = inputSurface->Data.TimeStamp;
        surface->Data.Corrupted = inputSurface->Data.Corrupted;
        surface->Data.DataFlag = inputSurface->Data.DataFlag;
        surface->Info = inputSurface->Info;
    }

    // bs could be NULL if input frame is buffered
    if (bs) {
        bitstream = bs;
        dataPtr = bitstream->Data + bitstream->DataOffset + bitstream->DataLength;
        initialDataLength = bitstream->DataLength;
    }

    if (surface)
    {
        UMC::ColorFormat cf = UMC::NONE;
        switch( surface->Info.FourCC ){
            case MFX_FOURCC_YV12:
                cf = UMC::YUV420;
                break;
            case MFX_FOURCC_RGB3:
                cf = UMC::RGB24;
                break;
            case MFX_FOURCC_NV12:
                cf = UMC::NV12;
                break;
            default:
                return MFX_ERR_NULL_PTR;
        }

        bool locked = false;
        if (surface->Data.Y == 0 && surface->Data.U == 0 &&
            surface->Data.V == 0 && surface->Data.A == 0)
        {
            st = m_core->LockExternalFrame(surface->Data.MemId, &(surface->Data));
            if (st == MFX_ERR_NONE)
                locked = true;
            else
                return st;
        }

        // need to call always now
//        res = Encode(cur_enc, &m_data_in, &m_data_out, p_data_out_ext, picStruct, ctrl, initialSurface);

        mfxRes = Encode(surface, bitstream);
        m_core->DecreaseReference(&(surface->Data));

        if (locked)
            m_core->UnlockExternalFrame(surface->Data.MemId, &(surface->Data));

        m_frameCount++;
    } else {
        mfxRes = Encode(0, bitstream);
        //        res = Encode(cur_enc, 0, &m_data_out, p_data_out_ext, 0, 0, 0);
    }

    if (mfxRes == MFX_ERR_MORE_DATA)
        mfxRes = MFX_ERR_NONE;

    return mfxRes;
}

mfxStatus MFXVideoENCODEH265::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK(m_isInitialized, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(par)

    par->mfx = m_mfxVideoParam.mfx;
    par->Protected = m_mfxVideoParam.Protected;
    par->AsyncDepth = m_mfxVideoParam.AsyncDepth;
    par->IOPattern = m_mfxVideoParam.IOPattern;

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::GetFrameParam(mfxFrameParam * /*par*/)
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MFXVideoENCODEH265::InitEnc()
{
    // temp buf size - todo reduce
    Ipp32u bufSize = m_mfxVideoParam.mfx.FrameInfo.Width * m_mfxVideoParam.mfx.FrameInfo.Height * 3 / 2;

    m_enc->memBuf = (Ipp8u *)H265_Malloc(sizeof(H265BsReal)*2 +sizeof(H265BsFake) +
        bufSize + MAX_SLICEHEADER_LEN*2 + DATA_ALIGN * 4);
    MFX_CHECK_STS_ALLOC(m_enc->memBuf);

    m_enc->bs = UMC::align_pointer<H265BsReal*>(m_enc->memBuf, DATA_ALIGN);
    m_enc->bs_sh = UMC::align_pointer<H265BsReal*>((Ipp8u*)m_enc->bs + sizeof(H265BsReal), DATA_ALIGN);
    m_enc->bsf = UMC::align_pointer<H265BsFake*>((Ipp8u*)m_enc->bs_sh + sizeof(H265BsReal), DATA_ALIGN);
    m_enc->bs_sh->m_base.m_pbsBase = UMC::align_pointer<Ipp8u*>((Ipp8u*)m_enc->bsf + sizeof(H265BsFake), DATA_ALIGN);
    m_enc->bs_sh->m_base.m_maxBsSize = MAX_SLICEHEADER_LEN;
    m_enc->bs->m_base.m_pbsBase = m_enc->bs_sh->m_base.m_pbsBase + MAX_SLICEHEADER_LEN*2;
    m_enc->bs->m_base.m_maxBsSize = bufSize;
    m_enc->bs_sh->Reset();
    m_enc->bs->Reset();
    m_enc->bsf->Reset();

    return m_enc->Init(&m_mfxVideoParam, &m_mfxExtOpts, &m_mfxHEVCOpts);
}

mfxStatus MFXVideoENCODEH265::ResetEnc()
{
    // todo fixme - implement reset frame management reset
    m_enc->bs->Reset();
    m_enc->bsf->Reset();
    m_enc->bs_sh->Reset();

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::CloseEnc()
{
    m_enc->Close();
    if (m_enc->memBuf) {
        H265_Free(m_enc->memBuf);
        m_enc->memBuf = NULL;
    }

    m_enc->bs = NULL;
    m_enc->bs_sh = NULL;
    m_enc->bsf = NULL;

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH265::Encode(mfxFrameSurface1 *surface, mfxBitstream *mfxBS)
{
    return m_enc->EncodeFrame(surface, mfxBS);
}

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
