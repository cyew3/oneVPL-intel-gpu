//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

#include "video_data_checker_mfx.h"
#include "outline_factory.h"

#include "umc_h264_dec.h"

static const mfxU32 MFX_TIME_STAMP_FREQUENCY = 90000; // will go to mfxdefs.h
static const mfxU64 MFX_TIME_STAMP_INVALID = (mfxU64)-1; // will go to mfxdefs.h

Ipp64f CalculateUMCFramerate( mfxU32 FrameRateExtN, mfxU32 FrameRateExtD)
{
    if (FrameRateExtN && FrameRateExtD)
        return (Ipp64f)FrameRateExtN / FrameRateExtD;
    else
        return 0;
}

inline mfxF64 GetUmcTimeStamp(mfxU64 ts)
{
    return ts == MFX_TIME_STAMP_INVALID ? -1.0 : ts / (mfxF64)MFX_TIME_STAMP_FREQUENCY;
}


UMC::Status VideoDataChecker_MFX::Init(const struct TestComponentParams *pTestParams)
{
    return VideoDataChecker::Init(pTestParams);
}

UMC::Status VideoDataChecker_MFX::Close()
{
    return VideoDataChecker::Close();
}

UMC::Status VideoDataChecker_MFX::Reset()
{
    return VideoDataChecker::Reset();
}

void VideoDataChecker_MFX::SetParams(const OutlineParams & params)
{
    VideoDataChecker::SetParams(params);
}

bool VideoDataChecker_MFX::IsFrameExist(Ipp32s frameNumber) const
{
    return VideoDataChecker::IsFrameExist(frameNumber);
}

bool VideoDataChecker_MFX::IsSequenceExist(Ipp32s seqNumber) const
{
    return VideoDataChecker::IsSequenceExist(seqNumber);
}

UMC::Status VideoDataChecker_MFX::ProcessSequence(BaseCodecParams *info, Ipp32s id)
{
    return VideoDataChecker::ProcessSequence(info, id);
}

UMC::Status VideoDataChecker_MFX::ProcessFrame(MediaData *in, Ipp32s id)
{
    return VideoDataChecker::ProcessFrame(in, id);
}

UMC::Status VideoDataChecker_MFX::ProcessSequence(const mfxVideoParam * pars, Ipp32s id)
{
    UMC::H264VideoDecoderParams umc_pars;
    ConvertSequenceToUmc(pars, &umc_pars);
    return VideoDataChecker::ProcessSequence(&umc_pars, id);
}

UMC::Status VideoDataChecker_MFX::ProcessFrame(const mfxFrameSurface1 * surf, Ipp32s id)
{
    TestVideoData data;
    ConvertFrameToUmc(surf, &data);

    m_FrameNumber++;
    m_FrameNumber = id == -1 ? m_FrameNumber : id;

    data.SetCRC32(ComputeCRC32(&data));
    data.SetSequenceNumber(m_SequenceNumber);
    data.SetFrameNumber(m_FrameNumber);

    data.m_mfx_dataFlag = surf->Data.DataFlag;
    data.m_mfx_picStruct = surf->Info.PicStruct;
    data.m_mfx_viewId = surf->Info.FrameId.ViewId;
    data.m_mfx_temporalId = surf->Info.FrameId.TemporalId;
    data.m_mfx_priorityId = surf->Info.FrameId.PriorityId;

    data.m_is_mfx = true;

    return VideoDataChecker::ProcessFrame(&data, id);
}

void VideoDataChecker_MFX::ConvertSequenceToUmc(const mfxVideoParam * pars, VideoDecoderParams * umc_pars)
{
    umc_pars->info.color_format = UMC::YUV420;

    UMC::H264VideoDecoderParams *in = DynamicCast<UMC::H264VideoDecoderParams>(umc_pars);

    if (in)
    {
        in->m_fullSize.width = pars->mfx.FrameInfo.Width;
        in->m_fullSize.height = pars->mfx.FrameInfo.Height;
        in->info.clip_info.width = pars->mfx.FrameInfo.CropW;
        in->info.clip_info.height = pars->mfx.FrameInfo.CropH;
        in->m_cropArea.left = pars->mfx.FrameInfo.CropX;
        in->m_cropArea.top = pars->mfx.FrameInfo.CropY;

        in->m_cropArea.bottom = (Ipp16s)(in->m_cropArea.top + in->info.clip_info.height);
        in->m_cropArea.right  = (Ipp16s)(in->m_cropArea.left + in->info.clip_info.width);
    }
    else
    {
        umc_pars->info.clip_info.width = pars->mfx.FrameInfo.Width;
        umc_pars->info.clip_info.height = pars->mfx.FrameInfo.Height;
    }

    umc_pars->info.aspect_ratio_width = pars->mfx.FrameInfo.AspectRatioW;
    umc_pars->info.aspect_ratio_height = pars->mfx.FrameInfo.AspectRatioH;
    umc_pars->profile = pars->mfx.CodecProfile;
    umc_pars->level = pars->mfx.CodecLevel;

    umc_pars->info.profile = pars->mfx.CodecProfile;
    umc_pars->info.level = pars->mfx.CodecLevel;

    umc_pars->info.framerate = CalculateUMCFramerate(pars->mfx.FrameInfo.FrameRateExtN, pars->mfx.FrameInfo.FrameRateExtD);
}

UMC::ColorFormat GetUMCColorFormat(Ipp32s color_format_idc)
{
    UMC::ColorFormat format;
    switch(color_format_idc)
    {
    case 0:
        format = UMC::GRAY;
        break;
    case 2:
        format = UMC::YUV422;
        break;
    case 3:
        format = UMC::YUV444;
        break;
    case 1:
    default:
        format = UMC::YUV420;
        break;
    }

    return format;
}

void VideoDataChecker_MFX::ConvertFrameToUmc(const mfxFrameSurface1 * surf, TestVideoData *pData)
{
    pData->UMC::VideoData::Init(surf->Info.CropW, surf->Info.CropH, GetUMCColorFormat(surf->Info.ChromaFormat), 8);

    mfxU32 offset = surf->Info.CropX + surf->Info.CropY * surf->Data.Pitch;
    switch(surf->Info.FourCC)
    {
        case MFX_FOURCC_NV12:
            pData->SetPlanePointer((void*)(surf->Data.Y + offset), 0);
            pData->SetPlanePointer((void*)(surf->Data.UV + offset), 1);
            pData->SetPlanePitch(surf->Data.Pitch, 0);
            pData->SetPlanePitch(surf->Data.Pitch, 1);
            break;

        case MFX_FOURCC_YV12:
        default:
            pData->SetPlanePointer((void*)(surf->Data.Y + offset), 0);
            pData->SetPlanePointer((void*)(surf->Data.U + offset/2), 1);
            pData->SetPlanePointer((void*)(surf->Data.V + offset/2), 2);
            pData->SetPlanePitch(surf->Data.Pitch, 0);
            pData->SetPlanePitch(surf->Data.Pitch/2, 1);
            pData->SetPlanePitch(surf->Data.Pitch/2, 2);
            break;
    }

    pData->SetFrameNumber(surf->Data.FrameOrder);

    pData->SetFrameType(UMC::VIDEO_FRAME);

    if ((surf->Info.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) || surf->Info.PicStruct == MFX_PICSTRUCT_UNKNOWN)
    {
        pData->SetPictureStructure(UMC::PS_FRAME);
    }
    else
    {
        pData->SetPictureStructure((surf->Info.PicStruct & MFX_PICSTRUCT_FIELD_TFF) ? UMC::PS_TOP_FIELD_FIRST : UMC::PS_BOTTOM_FIELD_FIRST);
    }

    pData->SetTime(GetUmcTimeStamp(surf->Data.TimeStamp));
    pData->SetInvalid(surf->Data.Corrupted);

    Ipp64f framerate = 0;
    if (surf->Info.FrameRateExtD && surf->Info.FrameRateExtN)
        framerate = surf->Info.FrameRateExtN / surf->Info.FrameRateExtD;
    Ipp64u align_value = CalculateTimestampAlignValue(framerate);
    Ipp64u ts = 0;
    if (align_value)
        ts = AlignTimestamp(surf->Data.TimeStamp, align_value); 

    pData->SetIntTime(ts);
}
