//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common_decode_int.h"
//#include "mfx_enc_common.h"
#include "umc_va_base.h"

MFXMediaDataAdapter::MFXMediaDataAdapter(mfxBitstream *pBitstream)
{
    Load(pBitstream);
}

void MFXMediaDataAdapter::Load(mfxBitstream *pBitstream)
{
    if (!pBitstream)
        return;

    SetBufferPointer(pBitstream->Data, pBitstream->DataOffset + pBitstream->DataLength);
    SetDataSize(pBitstream->DataOffset + pBitstream->DataLength);
    MoveDataPointer(pBitstream->DataOffset);
    SetTime(GetUmcTimeStamp(pBitstream->TimeStamp));

    SetFlags(0);

    if (pBitstream->DataFlag & MFX_BITSTREAM_EOS)
    {
        SetFlags(UMC::MediaData::FLAG_VIDEO_DATA_END_OF_STREAM);
    }
    else
    {
        if (!(pBitstream->DataFlag & MFX_BITSTREAM_COMPLETE_FRAME))
        {
            SetFlags(UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_UNIT | UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME);
        }
    }
}

void MFXMediaDataAdapter::Save(mfxBitstream *pBitstream)
{
    if (!pBitstream)
        return;

    pBitstream->DataOffset = (mfxU32)((mfxU8*)GetDataPointer() - (mfxU8*)GetBufferPointer());
    pBitstream->DataLength = (mfxU32)GetDataSize();
}


mfxStatus ConvertUMCStatusToMfx(UMC::Status status)
{
    switch((UMC::eUMC_VA_Status)status)
    {
    case UMC::UMC_ERR_FRAME_LOCKED:
    case UMC::UMC_ERR_DEVICE_FAILED:
        return MFX_ERR_DEVICE_FAILED;
    case UMC::UMC_ERR_DEVICE_LOST:
        return MFX_ERR_DEVICE_LOST;
    }

    mfxStatus sts = MFX_ERR_NONE;

    switch (status)
    {
        case UMC::UMC_OK:
        case UMC::UMC_ERR_SYNC:
            sts = MFX_ERR_NONE;
            break;
        case UMC::UMC_ERR_NULL_PTR:
            sts = MFX_ERR_NULL_PTR;
            break;
        case UMC::UMC_ERR_NOT_ENOUGH_BUFFER:
            sts = MFX_ERR_NOT_ENOUGH_BUFFER;
            break;
        case UMC::UMC_ERR_NOT_IMPLEMENTED:
        case UMC::UMC_ERR_UNSUPPORTED:
            sts = MFX_ERR_UNSUPPORTED;
            break;
        case UMC::UMC_ERR_ALLOC:
            sts = MFX_ERR_MEMORY_ALLOC;
            break;
        case UMC::UMC_ERR_LOCK:
            sts = MFX_ERR_LOCK_MEMORY;
            break;
        case UMC::UMC_ERR_INIT:
        case UMC::UMC_ERR_INVALID_PARAMS:
        case UMC::UMC_ERR_INVALID_STREAM:
        case UMC::UMC_ERR_FAILED:
        case UMC::UMC_ERR_TIMEOUT:
            sts = MFX_ERR_UNDEFINED_BEHAVIOR;
            break;
        default :
            sts = MFX_ERR_UNKNOWN;
            break;
    }

    return sts;
}

void ConvertMFXParamsToUMC(mfxVideoParam *par, UMC::VideoStreamInfo *umcVideoParams)
{
    umcVideoParams->clip_info.height = par->mfx.FrameInfo.Height;
    umcVideoParams->clip_info.width = par->mfx.FrameInfo.Width;

    umcVideoParams->disp_clip_info.height = umcVideoParams->clip_info.height;
    umcVideoParams->disp_clip_info.width = umcVideoParams->clip_info.width;

    if(par->mfx.CodecId == MFX_CODEC_JPEG && (MFX_ROTATION_90 == par->mfx.Rotation || MFX_ROTATION_270 == par->mfx.Rotation))
    {
        std::swap(umcVideoParams->clip_info.height, umcVideoParams->clip_info.width);
    }

    switch (par->mfx.FrameInfo.FourCC)
    {
    case MFX_FOURCC_RGB4:
        umcVideoParams->color_format = UMC::RGB32;
        break;
    case MFX_FOURCC_RGB3:
        umcVideoParams->color_format = UMC::RGB24;
        break;
    case MFX_FOURCC_YUY2:
        umcVideoParams->color_format = UMC::YUY2;
        break;
    case MFX_FOURCC_YV12:
        umcVideoParams->color_format = UMC::YV12;
        break;
    case MFX_FOURCC_P010:
        umcVideoParams->color_format = UMC::P010;
        break;
    case MFX_FOURCC_P210:
        umcVideoParams->color_format = UMC::P210;
        break;
#if defined (PRE_SI_TARGET_PLATFORM_GEN11)
    case MFX_FOURCC_Y210:
        umcVideoParams->color_format = UMC::Y210;
        break;
    case MFX_FOURCC_Y216:
        umcVideoParams->color_format = UMC::Y216;
        break;
    case MFX_FOURCC_Y410:
        umcVideoParams->color_format = UMC::Y410;
        break;
#endif //PRE_SI_TARGET_PLATFORM_GEN11
    case MFX_FOURCC_AYUV:
        umcVideoParams->color_format = UMC::YUV444A;
        break;
    case MFX_FOURCC_IMC3:
    case MFX_FOURCC_YUV400:
    case MFX_FOURCC_YUV411:
    case MFX_FOURCC_YUV422H:
    case MFX_FOURCC_YUV422V:
    case MFX_FOURCC_YUV444:
    case MFX_FOURCC_RGBP:
        umcVideoParams->color_format = UMC::YUY2;
        break;

    case MFX_FOURCC_NV12:
    default:
        umcVideoParams->color_format = UMC::NV12;
        break;
    }

    umcVideoParams->interlace_type = UMC::PROGRESSIVE;

    if (par->mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BFF)
        umcVideoParams->interlace_type = UMC::INTERLEAVED_BOTTOM_FIELD_FIRST;

    if (par->mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_TFF)
        umcVideoParams->interlace_type = UMC::INTERLEAVED_TOP_FIELD_FIRST;

    umcVideoParams->stream_subtype = UMC::UNDEF_VIDEO_SUBTYPE;
    umcVideoParams->stream_type = UMC::H264_VIDEO;

    umcVideoParams->framerate = (par->mfx.FrameInfo.FrameRateExtN && par->mfx.FrameInfo.FrameRateExtD)? (Ipp64f)par->mfx.FrameInfo.FrameRateExtN / par->mfx.FrameInfo.FrameRateExtD: 0;

    umcVideoParams->profile = ExtractProfile(par->mfx.CodecProfile);
    umcVideoParams->level = par->mfx.CodecLevel;
}

void ConvertMFXParamsToUMC(mfxVideoParam *par, UMC::VideoDecoderParams *umcVideoParams)
{
    ConvertMFXParamsToUMC(par, &umcVideoParams->info);

    umcVideoParams->numThreads = par->mfx.NumThread;

    switch(par->mfx.TimeStampCalc)
    {
    case MFX_TIMESTAMPCALC_TELECINE:
        umcVideoParams->lFlags |= UMC::FLAG_VDEC_TELECINE_PTS;
        break;

    case MFX_TIMESTAMPCALC_UNKNOWN:
        break;

    default:
        VM_ASSERT(false);
        break;
    }
}

bool IsNeedChangeVideoParam(mfxVideoParam *)
{
    return false;
}
