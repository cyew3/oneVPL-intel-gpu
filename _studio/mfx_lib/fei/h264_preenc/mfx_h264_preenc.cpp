/*//////////////////////////////////////////////////////////////////////////////
////                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"
#include "mfx_h264_preenc.h"

#ifdef MFX_VA
#if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)

#include <algorithm>
#include <numeric>

#include "mfx_session.h"
#include "mfx_task.h"
#include "libmfx_core.h"
#include "libmfx_core_hw.h"
#include "libmfx_core_interface.h"
#include "mfx_ext_buffers.h"

#include "mfx_h264_encode_hw.h"
#include "mfx_enc_common.h"
#include "mfx_h264_enc_common_hw.h"
#include "mfx_h264_encode_hw_utils.h"
#include "umc_va_dxva2_protected.h"

#include "mfx_h264_preenc.h"
#include "mfx_ext_buffers.h"

//#if defined (MFX_VA_LINUX)
    #include "mfx_h264_encode_vaapi.h"
//#endif

#if defined(_DEBUG)
#define mdprintf fprintf
#else
#define mdprintf(...)
#endif

using namespace MfxEncPREENC;

namespace MfxEncPREENC{
static mfxU16 GetGopSize(mfxVideoParam *par)
{
    return  par->mfx.GopPicSize == 0 ? 500 : par->mfx.GopPicSize;
}
static mfxU16 GetRefDist(mfxVideoParam *par)
{
    mfxU16 GopSize = GetGopSize(par);
    mfxU16 refDist = par->mfx.GopRefDist == 0 ?  2 : par->mfx.GopRefDist;
    return refDist <= GopSize ? refDist: GopSize;
}
static mfxU16 GetAsyncDeph(mfxVideoParam *par)
{
    return par->AsyncDepth == 0 ? 3 : par->AsyncDepth;
}

static bool IsVideoParamExtBufferIdSupported(mfxU32 id)
{
    return
        id == MFX_EXTBUFF_CODING_OPTION             ||
        id == MFX_EXTBUFF_CODING_OPTION_SPSPPS      ||
        id == MFX_EXTBUFF_DDI                       ||
        id == MFX_EXTBUFF_DUMP                      ||
        id == MFX_EXTBUFF_PAVP_OPTION               ||
        id == MFX_EXTBUFF_MVC_SEQ_DESC              ||
        id == MFX_EXTBUFF_VIDEO_SIGNAL_INFO         ||
        id == MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION ||
        id == MFX_EXTBUFF_PICTURE_TIMING_SEI        ||
        id == MFX_EXTBUFF_AVC_TEMPORAL_LAYERS       ||
        id == MFX_EXTBUFF_CODING_OPTION2            ||
        id == MFX_EXTBUFF_SVC_SEQ_DESC              ||
        id == MFX_EXTBUFF_SVC_RATE_CONTROL          ||
        id == MFX_EXTBUFF_ENCODER_RESET_OPTION      ||
        id == MFX_EXTBUFF_ENCODER_CAPABILITY        ||
        id == MFX_EXTBUFF_ENCODER_WIDI_USAGE        ||
        id == MFX_EXTBUFF_ENCODER_ROI               ||
        id == MFX_EXTBUFF_FEI_PARAM;
}

mfxExtBuffer* GetExtBuffer(mfxExtBuffer** ebuffers, mfxU32 nbuffers, mfxU32 BufferId)
{
    if (!ebuffers) return 0;
    for(mfxU32 i=0; i<nbuffers; i++) {
        if (!ebuffers[i]) continue;
        if (ebuffers[i]->BufferId == BufferId) {
            return ebuffers[i];
        }
    }
    return 0;
}

static mfxStatus CheckExtBufferId(mfxVideoParam const & par)
{
    for (mfxU32 i = 0; i < par.NumExtParam; i++)
    {
        if (par.ExtParam[i] == 0)
            return MFX_ERR_INVALID_VIDEO_PARAM;

        if (!MfxEncPREENC::IsVideoParamExtBufferIdSupported(par.ExtParam[i]->BufferId))
            return MFX_ERR_INVALID_VIDEO_PARAM;

        // check if buffer presents twice in video param
        if (MfxEncPREENC::GetExtBuffer(
            par.ExtParam + i + 1,
            par.NumExtParam - i - 1,
            par.ExtParam[i]->BufferId) != 0)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }

    return MFX_ERR_NONE;
}

static mfxU16  GetFrameType(mfxU32 frameOrder, int gopSize, int gopRefDist)
{

    if (frameOrder % gopSize == 0)
    {
        return MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|((frameOrder == 0) ? MFX_FRAMETYPE_IDR : 0);
    }
    else if ((frameOrder % gopSize) % gopRefDist == 0)
    {
        return MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF;
    }
    return MFX_FRAMETYPE_B;
}

static bool CheckExtenedBuffer(mfxVideoParam *par)
{
    mfxU32  BufferId[2] = {MFX_EXTBUFF_LOOKAHEAD_CTRL, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION}; 
    mfxU32  num = sizeof(BufferId)/sizeof(BufferId[0]);
    if (!par->ExtParam) return true;
   
    for (mfxU32 i = 0; i < par->NumExtParam; i++)
    {
        mfxU32 j = 0;
        for (; j < num; j++)
        {
            if (par->ExtParam[i] != 0 && par->ExtParam[i]->BufferId == BufferId[j]) break;
        } 
        if (j == num) return false;
    }
    return true;
}
static
mfxStatus GetNativeSurface(
    VideoCORE & core,
    MfxHwH264Encode::MfxVideoParam const & video,
    mfxFrameSurface1 *  opaq_surface,
    mfxFrameSurface1 ** native_surface)
{
    mfxStatus sts = MFX_ERR_NONE;
   *native_surface = opaq_surface;

    if (video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        *native_surface = core.GetNativeSurface(opaq_surface);
        MFX_CHECK_NULL_PTR1(*native_surface);

        (*native_surface)->Info            = opaq_surface->Info;
        (*native_surface)->Data.TimeStamp  = opaq_surface->Data.TimeStamp;
        (*native_surface)->Data.FrameOrder = opaq_surface->Data.FrameOrder;
        (*native_surface)->Data.Corrupted  = opaq_surface->Data.Corrupted;
        (*native_surface)->Data.DataFlag   = opaq_surface->Data.DataFlag;
    }
    return sts;
}

static
mfxStatus GetOpaqSurface(
    VideoCORE & core,
    MfxHwH264Encode::MfxVideoParam const & video,
    mfxFrameSurface1 *native_surface,
    mfxFrameSurface1 ** opaq_surface)
{
    mfxStatus sts = MFX_ERR_NONE;

   *opaq_surface = native_surface;

    if (video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        *opaq_surface = core.GetOpaqSurface(native_surface->Data.MemId);
        MFX_CHECK_NULL_PTR1(*opaq_surface);

        (*opaq_surface)->Info            = native_surface->Info;
        (*opaq_surface)->Data.TimeStamp  = native_surface->Data.TimeStamp;
        (*opaq_surface)->Data.FrameOrder = native_surface->Data.FrameOrder;
        (*opaq_surface)->Data.Corrupted  = native_surface->Data.Corrupted;
        (*opaq_surface)->Data.DataFlag   = native_surface->Data.DataFlag;
    }
    return sts;
}

bool CheckTriStateOptionPreEnc(mfxU16 & opt)
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

mfxStatus CheckVideoParamQueryLikePreEnc(
    MfxVideoParam &     par,
    ENCODE_CAPS const & hwCaps,
    eMFXHWType          platform,
    eMFXVAType          vaType)
{
    bool unsupported(false);
    bool changed(false);
    bool warning(false);

    mfxExtFeiParam *           feiParam     = GetExtBuffer(par);
    bool isPREENC = feiParam && (MFX_FEI_FUNCTION_PREENC == feiParam->Func);

    // check hw capabilities
    if (par.mfx.FrameInfo.Width  > hwCaps.MaxPicWidth ||
        par.mfx.FrameInfo.Height > hwCaps.MaxPicHeight)
        return Error(MFX_ERR_UNSUPPORTED);

    if ((par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PART1 )!= MFX_PICSTRUCT_PROGRESSIVE && hwCaps.NoInterlacedField){
        if(par.mfx.LowPower == MFX_CODINGOPTION_ON)
        {
            par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            changed = true;
        }
        else
            return Error(MFX_WRN_PARTIAL_ACCELERATION);
    }

    if (hwCaps.MaxNum_TemporalLayer != 0 &&
        hwCaps.MaxNum_TemporalLayer < par.calcParam.numTemporalLayer)
        return Error(MFX_WRN_PARTIAL_ACCELERATION);

    if (!CheckTriStateOptionPreEnc(par.mfx.LowPower))
        changed = true;

    if (IsOn(par.mfx.LowPower))
    {
        unsupported = true;
        par.mfx.LowPower = 0;
    }

    if (par.IOPattern != 0)
    {
        if ((par.IOPattern & MFX_IOPATTERN_IN_MASK) != par.IOPattern)
        {
            changed = true;
            par.IOPattern &= MFX_IOPATTERN_IN_MASK;
        }

        if (par.IOPattern != MFX_IOPATTERN_IN_VIDEO_MEMORY)
        {
            changed = true;
            par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
        }
    }

    if (par.mfx.GopPicSize > 0 &&
        par.mfx.GopRefDist > 0 &&
        par.mfx.GopRefDist > par.mfx.GopPicSize)
    {
        changed = true;
        par.mfx.GopRefDist = par.mfx.GopPicSize - 1;
    }

    if (par.mfx.GopRefDist > 1)
    {
        if (IsAvcBaseProfile(par.mfx.CodecProfile) ||
            par.mfx.CodecProfile == MFX_PROFILE_AVC_CONSTRAINED_HIGH)
        {
            changed = true;
            par.mfx.GopRefDist = 1;
        }
    }

    /* max allowed combination */
    if (par.mfx.FrameInfo.PicStruct > (MFX_PICSTRUCT_PART1|MFX_PICSTRUCT_PART2))
    { /* */
        unsupported = true;
        par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_UNKNOWN;
    }

    if (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PART2)
    { // repeat/double/triple flags are for EncodeFrameAsync
        changed = true;
        par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_UNKNOWN;
    }

    mfxU16 picStructPart1 = par.mfx.FrameInfo.PicStruct;
    if (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PART1)
    {
        if (picStructPart1 & (picStructPart1 - 1))
        { // more then one flag set
            changed = true;
            par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_UNKNOWN;
        }
    }

    if (par.mfx.FrameInfo.Width & 15)
    {
        unsupported = true;
        par.mfx.FrameInfo.Width = 0;
    }

    if (par.mfx.FrameInfo.Height & 15)
    {
        unsupported = true;
        par.mfx.FrameInfo.Height = 0;
    }

    if ((par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0 &&
        (par.mfx.FrameInfo.Height & 31) != 0)
    {
        unsupported = true;
        par.mfx.FrameInfo.PicStruct = 0;
        par.mfx.FrameInfo.Height = 0;
    }

    // driver doesn't support resolution 16xH
    if (par.mfx.FrameInfo.Width == 16)
    {
        unsupported = true;
        par.mfx.FrameInfo.Width = 0;
    }

    // driver doesn't support resolution Wx16
    if (par.mfx.FrameInfo.Height == 16)
    {
        unsupported = true;
        par.mfx.FrameInfo.Height = 0;
    }

    if (par.mfx.FrameInfo.Width > 0)
    {
        if (par.mfx.FrameInfo.CropX > par.mfx.FrameInfo.Width)
        {
            unsupported = true;
            par.mfx.FrameInfo.CropX = 0;
        }

        if (par.mfx.FrameInfo.CropX + par.mfx.FrameInfo.CropW > par.mfx.FrameInfo.Width)
        {
            unsupported = true;
            par.mfx.FrameInfo.CropW = par.mfx.FrameInfo.Width - par.mfx.FrameInfo.CropX;
        }
    }

    if (par.mfx.FrameInfo.Height > 0)
    {
        if (par.mfx.FrameInfo.CropY > par.mfx.FrameInfo.Height)
        {
            unsupported = true;
            par.mfx.FrameInfo.CropY = 0;
        }

        if (par.mfx.FrameInfo.CropY + par.mfx.FrameInfo.CropH > par.mfx.FrameInfo.Height)
        {
            unsupported = true;
            par.mfx.FrameInfo.CropH = par.mfx.FrameInfo.Height - par.mfx.FrameInfo.CropY;
        }
    }

    if (CorrectCropping(par) != MFX_ERR_NONE)
    { // cropping read from sps header always has correct alignment
        changed = true;
    }

    if (par.mfx.FrameInfo.ChromaFormat != 0 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV422 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV444)
    {
        changed = true;
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    }

    if (par.mfx.FrameInfo.FourCC != 0 &&
        par.mfx.FrameInfo.FourCC != MFX_FOURCC_NV12)
    {
        unsupported = true;
        par.mfx.FrameInfo.FourCC = 0;
    }

    if (par.mfx.FrameInfo.FourCC == MFX_FOURCC_NV12 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_MONOCHROME)
    {
        changed = true;
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    }

    if (hwCaps.Color420Only &&
        (par.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV422 ||
         par.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV444))
    {
        unsupported = true;
        par.mfx.FrameInfo.ChromaFormat = 0;
    }

    if ((isPREENC) && !(
            (MFX_CODINGOPTION_UNKNOWN == feiParam->SingleFieldProcessing) ||
            (MFX_CODINGOPTION_ON == feiParam->SingleFieldProcessing) ||
            (MFX_CODINGOPTION_OFF == feiParam->SingleFieldProcessing)) )
        unsupported = true;

    return unsupported
        ? MFX_ERR_UNSUPPORTED
        : (changed || warning)
            ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
            : MFX_ERR_NONE;
}

mfxStatus CheckVideoParamPreEncInit(
    MfxVideoParam &     par,
    ENCODE_CAPS const & hwCaps,
    eMFXHWType          platform,
    eMFXVAType          vaType)
{
    bool unsupported(false);
    bool changed(false);
    bool warning(false);

    mfxExtFeiParam *           feiParam     = GetExtBuffer(par);
    bool isPREENC = feiParam && (MFX_FEI_FUNCTION_PREENC == feiParam->Func);

    if (feiParam && (MFX_FEI_FUNCTION_PREENC != feiParam->Func))
        unsupported = true;

    // check hw capabilities
    if (par.mfx.FrameInfo.Width  > hwCaps.MaxPicWidth ||
        par.mfx.FrameInfo.Height > hwCaps.MaxPicHeight)
        return Error(MFX_ERR_UNSUPPORTED);

    if ((par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PART1 )!= MFX_PICSTRUCT_PROGRESSIVE && hwCaps.NoInterlacedField){
        if(par.mfx.LowPower == MFX_CODINGOPTION_ON)
        {
            par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            changed = true;
        }
        else
            return Error(MFX_WRN_PARTIAL_ACCELERATION);
    }

    if (hwCaps.MaxNum_TemporalLayer != 0 &&
        hwCaps.MaxNum_TemporalLayer < par.calcParam.numTemporalLayer)
        return Error(MFX_WRN_PARTIAL_ACCELERATION);

    if (!CheckTriStateOptionPreEnc(par.mfx.LowPower))
        changed = true;

    if (IsOn(par.mfx.LowPower))
    {
        unsupported = true;
        par.mfx.LowPower = 0;
    }

    if (par.IOPattern != 0)
    {
        if (par.IOPattern != MFX_IOPATTERN_IN_VIDEO_MEMORY)
        {
            unsupported = true;
        }
    }

    if (par.mfx.GopPicSize > 0 &&
        par.mfx.GopRefDist > 0 &&
        par.mfx.GopRefDist > par.mfx.GopPicSize)
    {
        changed = true;
        par.mfx.GopRefDist = par.mfx.GopPicSize - 1;
    }

    if (par.mfx.GopRefDist > 1)
    {
        if (IsAvcBaseProfile(par.mfx.CodecProfile) ||
            par.mfx.CodecProfile == MFX_PROFILE_AVC_CONSTRAINED_HIGH)
        {
            changed = true;
            par.mfx.GopRefDist = 1;
        }
    }

    /* max allowed combination */
    if (par.mfx.FrameInfo.PicStruct > (MFX_PICSTRUCT_PART1|MFX_PICSTRUCT_PART2))
    { /* */
        unsupported = true;
        par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_UNKNOWN;
    }

    if (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PART2)
    { // repeat/double/triple flags are for EncodeFrameAsync
        changed = true;
        par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_UNKNOWN;
    }

    mfxU16 picStructPart1 = par.mfx.FrameInfo.PicStruct;
    if (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PART1)
    {
        if (picStructPart1 & (picStructPart1 - 1))
        { // more then one flag set
            changed = true;
            par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_UNKNOWN;
        }
    }

    if (par.mfx.FrameInfo.Width & 15)
    {
        unsupported = true;
        par.mfx.FrameInfo.Width = 0;
    }

    if (par.mfx.FrameInfo.Height & 15)
    {
        unsupported = true;
        par.mfx.FrameInfo.Height = 0;
    }

    if ((par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0 &&
        (par.mfx.FrameInfo.Height & 31) != 0)
    {
        unsupported = true;
        par.mfx.FrameInfo.PicStruct = 0;
        par.mfx.FrameInfo.Height = 0;
    }

    // driver doesn't support resolution 16xH
    if ((0 == par.mfx.FrameInfo.Width) || (par.mfx.FrameInfo.Width == 16))
    {
        unsupported = true;
        par.mfx.FrameInfo.Width = 0;
    }

    // driver doesn't support resolution Wx16
    if ((0 == par.mfx.FrameInfo.Height) || (par.mfx.FrameInfo.Height == 16))
    {
        unsupported = true;
        par.mfx.FrameInfo.Height = 0;
    }

    if (par.mfx.FrameInfo.Width > 0)
    {
        if (par.mfx.FrameInfo.CropX > par.mfx.FrameInfo.Width)
        {
            unsupported = true;
            par.mfx.FrameInfo.CropX = 0;
        }

        if (par.mfx.FrameInfo.CropX + par.mfx.FrameInfo.CropW > par.mfx.FrameInfo.Width)
        {
            unsupported = true;
            par.mfx.FrameInfo.CropW = par.mfx.FrameInfo.Width - par.mfx.FrameInfo.CropX;
        }
    }

    if (par.mfx.FrameInfo.Height > 0)
    {
        if (par.mfx.FrameInfo.CropY > par.mfx.FrameInfo.Height)
        {
            unsupported = true;
            par.mfx.FrameInfo.CropY = 0;
        }

        if (par.mfx.FrameInfo.CropY + par.mfx.FrameInfo.CropH > par.mfx.FrameInfo.Height)
        {
            unsupported = true;
            par.mfx.FrameInfo.CropH = par.mfx.FrameInfo.Height - par.mfx.FrameInfo.CropY;
        }
    }

    if (CorrectCropping(par) != MFX_ERR_NONE)
    { // cropping read from sps header always has correct alignment
        changed = true;
    }

    if (par.mfx.FrameInfo.ChromaFormat != 0 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV422 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV444)
    {
        changed = true;
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    }

    if (par.mfx.FrameInfo.FourCC != 0 &&
        par.mfx.FrameInfo.FourCC != MFX_FOURCC_NV12)
    {
        unsupported = true;
        par.mfx.FrameInfo.FourCC = 0;
    }

    if (par.mfx.FrameInfo.FourCC == MFX_FOURCC_NV12 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_MONOCHROME)
    {
        changed = true;
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    }

    if (hwCaps.Color420Only &&
        (par.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV422 ||
         par.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV444))
    {
        unsupported = true;
        par.mfx.FrameInfo.ChromaFormat = 0;
    }

//    if (par.mfx.NumRefFrame == 0)
//    {
//        changed = true;
//        par.mfx.NumRefFrame = 1;
//    }

//    if ((par.AsyncDepth == 0) || (par.AsyncDepth > 1))
//    {
//        /* Supported only Async 1 */
//        changed = true;
//        par.AsyncDepth = 1;
//    }

    if ((isPREENC) && !(
            (MFX_CODINGOPTION_UNKNOWN == feiParam->SingleFieldProcessing) ||
            (MFX_CODINGOPTION_ON == feiParam->SingleFieldProcessing) ||
            (MFX_CODINGOPTION_OFF == feiParam->SingleFieldProcessing)) )
        unsupported = true;

    return unsupported
        ? MFX_ERR_UNSUPPORTED
        : (changed || warning)
            ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
            : MFX_ERR_NONE;
}

} ////

bool bEnc_PREENC(mfxVideoParam *par)
{
    mfxExtFeiParam *pControl = NULL;
//    mfxExtFeiParam *pControl = (mfxExtFeiParam *) GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_FEI_PARAM);
    for (mfxU16 i = 0; i < par->NumExtParam; i++)
        if (par->ExtParam[i] != 0 && par->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PARAM){ // assuming aligned buffers
            pControl = (mfxExtFeiParam *)par->ExtParam[i];
            break;
        }
    
    bool res = pControl ? (pControl->Func == MFX_FEI_FUNCTION_PREENC) : false;
    return res;
}

static mfxStatus AsyncRoutine(void * state, void * param, mfxU32, mfxU32)
{
    VideoENC_PREENC & impl = *(VideoENC_PREENC *)state;
    return  impl.RunFrameVmeENC(0,0);
}

mfxStatus VideoENC_PREENC::RunFrameVmeENC(mfxENCInput *in, mfxENCOutput *out)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoENC_PREENC::RunFrameVmeENC");

    //mfxExtCodingOption    const * extOpt = GetExtBuffer(m_video);
    //mfxExtCodingOptionDDI const * extDdi = GetExtBuffer(m_video);

    mfxStatus sts = MFX_ERR_NONE;
    DdiTask & task = m_incoming.front();
    mfxU32 f = 0, f_start = 0;
    mfxU32 fieldCount = task.m_fieldPicFlag;
    mfxENCInput* inEncBuf = (mfxENCInput*)task.m_userData[0];
    mfxExtFeiPreEncCtrl* feiCtrl = GetExtBufferFEI(inEncBuf, 0);

    task.m_idx    = FindFreeResourceIndex(m_raw);
    task.m_midRaw = AcquireResource(m_raw, task.m_idx);

    sts = GetNativeHandleToRawSurface(*m_core, m_video, task, task.m_handleRaw);
    if (sts != MFX_ERR_NONE)
         return Error(sts);

    sts = CopyRawSurfaceToVideoMemory(*m_core, m_video, task);
    if (sts != MFX_ERR_NONE)
          return Error(sts);

    if (MFX_CODINGOPTION_ON == m_singleFieldProcessingMode)
    {
        f_start = fieldCount = m_firstFieldDone;
    }

    for (f = f_start; f <= fieldCount; f++)
    {
        //mprintf(stderr,"handle=0x%x mid=0x%x\n", task.m_handleRaw, task.m_midRaw );
        sts = m_ddi->Execute(task.m_handleRaw.first, task, task.m_fid[f], m_sei);
        if (sts != MFX_ERR_NONE)
                 return Error(sts);
    }

    if (MFX_CODINGOPTION_ON == m_singleFieldProcessingMode)
        m_firstFieldDone = 1 - m_firstFieldDone;

    return sts;
}

static mfxStatus AsyncQuery(void * state, void * param, mfxU32 /*threadNumber*/, mfxU32 /*callNumber*/)
{
    VideoENC_PREENC & impl = *(VideoENC_PREENC *)state;
    DdiTask& task = *(DdiTask *)param;
    return impl.QueryStatus(task);
}

mfxStatus VideoENC_PREENC::QueryStatus(DdiTask& task)
{
    mdprintf(stderr,"query\n");
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 f = 0, f_start = 0;
    mfxU32 fieldCount = task.m_fieldPicFlag;
    mfxENCInput* in = (mfxENCInput*)task.m_userData[0];
    mfxExtFeiPreEncCtrl* feiCtrl = GetExtBufferFEI(in, 0);

    if (MFX_CODINGOPTION_ON == m_singleFieldProcessingMode)
    {
        f_start = fieldCount = 1 - m_firstFieldDone;
    }

    for (f = f_start; f <= fieldCount; f++)
    {
        sts = m_ddi->QueryStatus(task, task.m_fid[f]);
        if (sts == MFX_WRN_DEVICE_BUSY)
            return MFX_TASK_BUSY;
        if (sts != MFX_ERR_NONE)
            return Error(sts);
    }

    m_core->DecreaseReference(&task.m_yuv->Data);

    UMC::AutomaticUMCMutex guard(m_listMutex);
    //move that task to free tasks from m_incoming
    //m_incoming
    std::list<DdiTask>::iterator it = std::find(m_incoming.begin(), m_incoming.end(), task);
    if(it != m_incoming.end())
        m_free.splice(m_free.end(), m_incoming, it);
    else
        return MFX_ERR_NOT_FOUND;

//    if (MFX_CODINGOPTION_ON == m_singleFieldProcessingMode)
//        m_firstFieldDone = MFX_CODINGOPTION_UNKNOWN;

    return MFX_ERR_NONE;

}


mfxStatus VideoENC_PREENC::Query(VideoCORE* core, mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoENC_PREENC::Query");
    MFX_CHECK_NULL_PTR1(core);
    MFX_CHECK_NULL_PTR1(out);
    mfxStatus sts = MFX_ERR_NONE;
    ENCODE_CAPS hwCaps = { };

    if (NULL == in)
    {
        Zero(out->mfx);

        out->IOPattern             = MFX_IOPATTERN_IN_VIDEO_MEMORY;
        out->AsyncDepth            = 1;
        out->mfx.NumRefFrame       = 1;
        out->mfx.NumThread         = 1;
        out->mfx.EncodedOrder      = 1;
        out->mfx.FrameInfo.FourCC        = 1;
        out->mfx.FrameInfo.Width         = 1;
        out->mfx.FrameInfo.Height        = 1;
        out->mfx.FrameInfo.CropX         = 1;
        out->mfx.FrameInfo.CropY         = 1;
        out->mfx.FrameInfo.CropW         = 1;
        out->mfx.FrameInfo.CropH         = 1;
        out->mfx.FrameInfo.FrameRateExtN = 1;
        out->mfx.FrameInfo.FrameRateExtD = 1;
        out->mfx.FrameInfo.AspectRatioW  = 1;
        out->mfx.FrameInfo.AspectRatioH  = 1;
        out->mfx.FrameInfo.ChromaFormat  = 1;
        out->mfx.FrameInfo.PicStruct     = 1;

        return MFX_ERR_NONE;
    }

    MfxVideoParam tmp = *in; // deep copy, to cteare all ext buffers

    std::auto_ptr<DriverEncoder> ddi;
    ddi.reset( new MfxHwH264Encode::VAAPIFEIPREENCEncoder );
    if (ddi.get() == 0)
        return MFX_WRN_PARTIAL_ACCELERATION;

    sts = ddi->CreateAuxilliaryDevice(
        core,
        DXVA2_Intel_Encode_AVC,
        GetFrameWidth(tmp),
        GetFrameHeight(tmp));
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    sts = ddi->QueryEncodeCaps(hwCaps);
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    mfxStatus checkSts = CheckVideoParamQueryLikePreEnc(tmp, hwCaps, core->GetHWType(), core->GetVAType());

    if (checkSts == MFX_WRN_PARTIAL_ACCELERATION)
        return MFX_WRN_PARTIAL_ACCELERATION;
    else if (checkSts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
        checkSts = MFX_ERR_UNSUPPORTED;

    out->IOPattern  = tmp.IOPattern;
    out->AsyncDepth = tmp.AsyncDepth;
    out->mfx = tmp.mfx;

    /**/
    return checkSts;
} // mfxStatus VideoENC_PREENC::Query(VideoCORE*, mfxVideoParam *in, mfxVideoParam *out)

mfxStatus VideoENC_PREENC::QueryIOSurf(VideoCORE* , mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR2(par,request);
#if 0
    mfxExtLAControl *pControl = (mfxExtLAControl *) GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_LOOKAHEAD_CTRL);

    MFX_CHECK(pControl,MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(pControl->LookAheadDepth,MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxU32 inPattern = par->IOPattern & MfxHwH264Encode::MFX_IOPATTERN_IN_MASK;
    MFX_CHECK(
        inPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ||
        inPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY ||
        inPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY,
        MFX_ERR_INVALID_VIDEO_PARAM);

    if (inPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        request->Type =
            MFX_MEMTYPE_EXTERNAL_FRAME |
            MFX_MEMTYPE_FROM_ENCODE |
            MFX_MEMTYPE_SYSTEM_MEMORY;
    }
    else // MFX_IOPATTERN_IN_VIDEO_MEMORY || MFX_IOPATTERN_IN_OPAQUE_MEMORY
    {
        request->Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET;
        request->Type |= (inPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
            ? MFX_MEMTYPE_OPAQUE_FRAME
            : MFX_MEMTYPE_EXTERNAL_FRAME;
    }
    request->NumFrameMin         = GetRefDist(par) + GetAsyncDeph(par) + pControl->LookAheadDepth;
    request->NumFrameSuggested   = request->NumFrameMin;
    request->Info                = par->mfx.FrameInfo;
#endif
    return MFX_ERR_NONE;
} // mfxStatus VideoENC_PREENC::QueryIOSurf(VideoCORE* , mfxVideoParam *par, mfxFrameAllocRequest *request)


VideoENC_PREENC::VideoENC_PREENC(VideoCORE *core,  mfxStatus * sts)
    : m_bInit( false )
    , m_core( core )
    , m_singleFieldProcessingMode(0)
    , m_firstFieldDone(0)
{
    *sts = MFX_ERR_NONE;
}


VideoENC_PREENC::~VideoENC_PREENC()
{
    Close();
}

mfxStatus VideoENC_PREENC::Init(mfxVideoParam *par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoENC_PREENC::Init");
    mfxStatus sts = MfxEncPREENC::CheckExtBufferId(*par);
    MFX_CHECK_STS(sts);

    m_video = *par;
    //add ext buffers from par to m_video

    /* Check W & H*/
    MFX_CHECK((par->mfx.FrameInfo.Width > 0 && par->mfx.FrameInfo.Height > 0), MFX_ERR_INVALID_VIDEO_PARAM);

    m_ddi.reset( new MfxHwH264Encode::VAAPIFEIPREENCEncoder );
    if (m_ddi.get() == 0)
        return MFX_WRN_PARTIAL_ACCELERATION;

    sts = m_ddi->CreateAuxilliaryDevice(
        m_core,
        DXVA2_Intel_Encode_AVC,
        GetFrameWidth(m_video),
        GetFrameHeight(m_video));
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    sts = m_ddi->QueryEncodeCaps(m_caps);
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    mfxStatus checkStatus = CheckVideoParamPreEncInit(m_video, m_caps, m_currentPlatform, m_currentVaType);
    switch (checkStatus)
    {
    case MFX_ERR_UNSUPPORTED:
        return MFX_ERR_INVALID_VIDEO_PARAM;
    case MFX_ERR_INVALID_VIDEO_PARAM:
    case MFX_WRN_PARTIAL_ACCELERATION:
    case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
        return sts;
    case MFX_WRN_INCOMPATIBLE_VIDEO_PARAM:
        sts = checkStatus;
        break;
    default:
        break;
    }

    const mfxExtFeiParam* params = GetExtBuffer(m_video);
    if (NULL == params)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    else
    {
        if ((MFX_CODINGOPTION_ON == params->SingleFieldProcessing) &&
             ((MFX_PICSTRUCT_FIELD_TFF == m_video.mfx.FrameInfo.PicStruct) ||
              (MFX_PICSTRUCT_FIELD_BFF == m_video.mfx.FrameInfo.PicStruct)) )
            m_singleFieldProcessingMode = MFX_CODINGOPTION_ON;
    }

    m_currentPlatform = m_core->GetHWType();
    m_currentVaType   = m_core->GetVAType();

    // PRE-ENC works with surfaces which was passed in vaCreateContext() stage
    mfxFrameAllocRequest request = { };
    request.Info = m_video.mfx.FrameInfo;
    request.Type = MFX_MEMTYPE_FROM_ENC | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME;
    /* WA for Beh test
     * But code will be re-worked in future */
    request.NumFrameMin = 1;
    if ((m_video.mfx.GopRefDist > 0) && (m_video.mfx.GopRefDist < 16))
        request.NumFrameMin = m_video.mfx.GopRefDist*2;

    request.NumFrameSuggested = request.NumFrameMin + m_video.AsyncDepth;
    request.AllocId = par->AllocId;
    /* TODO:
     * Actually MSDK since drv build 54073 does not need to Alloc surface here*/
    sts = m_core->AllocFrames(&request, &m_raw);
    //sts = m_raw.Alloc(m_core, request);
    MFX_CHECK_STS(sts);

    sts = m_ddi->Register(m_raw, D3DDDIFMT_NV12);
    MFX_CHECK_STS(sts);

    sts = m_ddi->CreateAccelerationService(m_video);
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    m_inputFrameType =
        m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY || m_video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY
            ? MFX_IOPATTERN_IN_SYSTEM_MEMORY
            : MFX_IOPATTERN_IN_VIDEO_MEMORY;

    m_free.resize(m_video.AsyncDepth);
    m_incoming.clear();

    m_bInit = true;
    return checkStatus;
}

mfxStatus VideoENC_PREENC::Reset(mfxVideoParam *par)
{
    Close();
    return Init(par);
}

mfxStatus VideoENC_PREENC::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK(CheckExtenedBuffer(par), MFX_ERR_UNDEFINED_BEHAVIOR);
    return MFX_ERR_NONE;
}

mfxStatus VideoENC_PREENC::RunFrameVmeENCCheck(
                    mfxENCInput *input,
                    mfxENCOutput *output,
                    MFX_ENTRY_POINT pEntryPoints[],
                    mfxU32 &numEntryPoints)
{
    MFX_CHECK( m_bInit, MFX_ERR_UNDEFINED_BEHAVIOR);

    if ((NULL == input) || (NULL == output))
        return MFX_ERR_NULL_PTR;

    //set frame type
    mfxU8 mtype_first_field = 0;
    mfxU8 mtype_second_field = 0;
    mfxExtFeiPreEncCtrl* feiCtrl = GetExtBufferFEI(input, 0);
    if (NULL == feiCtrl)
        return MFX_ERR_NULL_PTR; /* this is fatal error */
    if ((NULL == feiCtrl->RefFrame[0])  && (NULL == feiCtrl->RefFrame[1]))
        mtype_first_field = MFX_FRAMETYPE_I;
    else if ((NULL != feiCtrl->RefFrame[0])  && (NULL == feiCtrl->RefFrame[1]))
        mtype_first_field = MFX_FRAMETYPE_P;
    else if ((NULL != feiCtrl->RefFrame[0])  || (NULL != feiCtrl->RefFrame[1]))
        mtype_first_field = MFX_FRAMETYPE_B;

    if (MFX_PICSTRUCT_PROGRESSIVE != m_video.mfx.FrameInfo.PicStruct)
    {
        /* same for second field */
        feiCtrl = GetExtBufferFEI(input, 1);
        if (NULL == feiCtrl)
            return MFX_ERR_NULL_PTR; /* this is fatal error */
        if ((NULL == feiCtrl->RefFrame[0])  && (NULL == feiCtrl->RefFrame[1]))
            mtype_second_field = MFX_FRAMETYPE_I;
        else if ((NULL != feiCtrl->RefFrame[0])  && (NULL == feiCtrl->RefFrame[1]))
            mtype_second_field = MFX_FRAMETYPE_P;
        else if ((NULL != feiCtrl->RefFrame[0])  || (NULL != feiCtrl->RefFrame[1]))
            mtype_second_field = MFX_FRAMETYPE_B;
    }

    UMC::AutomaticUMCMutex guard(m_listMutex);

    m_free.front().m_yuv = input->InSurface;
    //m_free.front().m_ctrl = 0;
    m_free.front().m_type = Pair<mfxU8>(mtype_first_field, mtype_second_field);

    m_free.front().m_extFrameTag = input->InSurface->Data.FrameOrder;
    m_free.front().m_frameOrder  = input->InSurface->Data.FrameOrder;
    m_free.front().m_timeStamp   = input->InSurface->Data.TimeStamp;
    m_free.front().m_userData.resize(2);
    m_free.front().m_userData[0] = input;
    m_free.front().m_userData[1] = output;

    m_incoming.splice(m_incoming.end(), m_free, m_free.begin());
    DdiTask& task = m_incoming.front();

    /* This value have to be initialized here
     * as we use MfxHwH264Encode::GetPicStruct
     * (Legacy encoder do initialization  by itself)
     * */
    mfxExtCodingOption * extOpt = GetExtBuffer(m_video);
    extOpt->FieldOutput = 32;
    task.m_picStruct    = GetPicStruct(m_video, task);
    task.m_fieldPicFlag = task.m_picStruct[ENC] != MFX_PICSTRUCT_PROGRESSIVE;
    task.m_fid[0]       = task.m_picStruct[ENC] == MFX_PICSTRUCT_FIELD_BFF;
    task.m_fid[1]       = task.m_fieldPicFlag - task.m_fid[0];

    /* We need to match picture types...
     * (1): If Init() was for "MFX_PICSTRUCT_UNKNOWN", all Picture types is allowed
     * (2): Else allowed only picture type which was on Init() stage
     *  */
    mfxU16 picStructTask = task.GetPicStructForEncode();
    mfxU16 picStructInit = m_video.mfx.FrameInfo.PicStruct;
    if (! ((MFX_PICSTRUCT_UNKNOWN == picStructInit) || (picStructInit == picStructTask)) )
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (task.m_picStruct[ENC] == MFX_PICSTRUCT_FIELD_BFF)
        std::swap(task.m_type.top, task.m_type.bot);

    m_core->IncreaseReference(&input->InSurface->Data);

    pEntryPoints[0].pState               = this;
    pEntryPoints[0].pParam               = &m_incoming.front();
    pEntryPoints[0].pCompleteProc        = 0;
    pEntryPoints[0].pGetSubTaskProc      = 0;
    pEntryPoints[0].pCompleteSubTaskProc = 0;
    pEntryPoints[0].requiredNumThreads   = 1;
    pEntryPoints[0].pRoutineName         = "AsyncRoutine";
    pEntryPoints[0].pRoutine             = AsyncRoutine;
    pEntryPoints[1] = pEntryPoints[0];
    pEntryPoints[1].pRoutineName = "Async Query";
    pEntryPoints[1].pRoutine     = AsyncQuery;
    pEntryPoints[1].pParam       = &m_incoming.front();

    numEntryPoints = 2;

    return MFX_ERR_NONE;
} 

static mfxStatus CopyRawSurfaceToVideoMemory(  VideoCORE &  core,
                                        MfxHwH264Encode::MfxVideoParam const & video,
                                        mfxFrameSurface1 *  src_sys, 
                                        mfxMemId            dst_d3d,
                                        mfxHDL&             handle)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "FEI:PeENC::CopyRawSurfaceToVideoMemory");
    mfxExtOpaqueSurfaceAlloc const * extOpaq = GetExtBuffer(video);

    mfxFrameData d3dSurf = {0};
    if (video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ||
        (video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && (extOpaq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY)))
    {
        mfxFrameData sysSurf = src_sys->Data;
        d3dSurf.MemId = dst_d3d;

        MfxHwH264Encode::FrameLocker lock2(&core, sysSurf, true);

        MFX_CHECK_NULL_PTR1(sysSurf.Y)
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "Copy input (sys->d3d)");
            MFX_CHECK_STS(MfxHwH264Encode::CopyFrameDataBothFields(&core, d3dSurf, sysSurf, video.mfx.FrameInfo));
        }

        MFX_CHECK_STS(lock2.Unlock());
    }
    else
    {
        d3dSurf.MemId =  src_sys->Data.MemId;
    }

    if (video.IOPattern != MFX_IOPATTERN_IN_OPAQUE_MEMORY) 
       MFX_CHECK_STS(core.GetExternalFrameHDL(d3dSurf.MemId, &handle))
    else
       MFX_CHECK_STS(core.GetFrameHDL(d3dSurf.MemId, &handle));
    
    return MFX_ERR_NONE;
}


mfxStatus VideoENC_PREENC::Close(void)
{
    if (!m_bInit) return MFX_ERR_NONE;
    m_bInit = false;
    m_ddi->Destroy();

    m_core->FreeFrames(&m_raw);
    //m_core->FreeFrames(&m_opaqHren);

    return MFX_ERR_NONE;
} 

#endif  
#endif  // MFX_VA

/* EOF */

