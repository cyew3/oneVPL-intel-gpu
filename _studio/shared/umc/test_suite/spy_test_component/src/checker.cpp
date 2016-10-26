//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2012 Intel Corporation. All Rights Reserved.
//

#include "checker.h"
#include "outline_writer_defs.h"
#include "outline_factory.h"
#include "umc_h264_dec.h"

void Checker::CheckSequenceInfo(const UMC::BaseCodecParams *input1, const UMC::BaseCodecParams *input2)
{
    const UMC::VideoDecoderParams *info1 = DynamicCast<const UMC::VideoDecoderParams> (input1);
    const UMC::VideoDecoderParams *info2 = DynamicCast<const UMC::VideoDecoderParams> (input2);

    try
    {
        if (!info1 || !info2)
            throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("sequence: invalid input parameters: object isn't derived from VideoDecoderParams"));

        if (info1->info.color_format != info2->info.color_format)
            throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("sequence: color format"));

        const UMC::H264VideoDecoderParams *in1 = DynamicCast<const UMC::H264VideoDecoderParams>(info1);
        const UMC::H264VideoDecoderParams *in2 = DynamicCast<const UMC::H264VideoDecoderParams>(info2);

        if (in1 && in2)
        {
            if (in1->m_fullSize.width != in2->m_fullSize.width)
                throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("sequence: width"));

            if (in1->m_fullSize.height != in2->m_fullSize.height)
                throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("sequence: height"));

            if (in1->info.clip_info.width != in2->info.clip_info.width)
                throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("sequence: crop width"));

            if (in1->info.clip_info.height != in2->info.clip_info.height)
                throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("sequence: crop height"));

            if (in1->m_cropArea.left != in2->m_cropArea.left)
                throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("sequence: crop left"));

            if (in1->m_cropArea.top != in2->m_cropArea.top)
                throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("sequence: crop top"));
        }
        else
        {
            if (info1->info.clip_info.width != info2->info.clip_info.width)
                throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("sequence: width"));

            if (info1->info.clip_info.height != info2->info.clip_info.height)
                throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("sequence: height"));
        }

        if (info1->info.aspect_ratio_width != info2->info.aspect_ratio_width)
            throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("sequence: aspect_ratio_width"));

        if (info1->info.aspect_ratio_height != info2->info.aspect_ratio_height)
            throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("sequence: aspect_ratio_height"));

        if (info1->info.profile != info2->info.profile)
            throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("sequence: profile"));

        if (info1->info.level != info2->info.level)
            throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("sequence: level"));

        if ((Ipp64u)(info1->info.framerate * 100) != (Ipp64u)(info2->info.framerate * 100))
            throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("sequence: framerate"));

        /* bitdepth
        if (info1->info.profile != info2->info.profile)
            return false;

        if (info1->info.level != info2->info.level)
            return false;
        */

    }
    catch (OutlineException & ex)
    {
        OutlineError  error(OutlineError::OUTLINE_ERROR_TYPE_SEQUENCE, *info1, *info2);
        ex.SetError(error);
        throw;
    }
}

void Checker::CheckFrameInfo(const UMC::MediaData *in1, const UMC::MediaData *in2)
{
    TestVideoData *data1 = const_cast<TestVideoData*>(DynamicCast<const TestVideoData> (in1));
    TestVideoData *data2 = const_cast<TestVideoData*>(DynamicCast<const TestVideoData> (in2));

    try
    {
        if (!data1 || !data2)
            throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("frame: invalid input parameters: object isn't derived from TestVideoData"));

        if (data1->GetCRC32() != data2->GetCRC32())
            throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("frame: crc32"));

        if (data1->GetFrameType() != data2->GetFrameType())
            throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("frame: type"));

        if (data1->GetPictureStructure() != data2->GetPictureStructure())
            throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("frame: picture structure"));

        /*Ipp64f start1, start2, end;
        data1->GetTime(start1, end);
        data2->GetTime(start2, end);

        Ipp64u ts1 = GetIntTimeStamp(start1);
        Ipp64u ts2 = GetIntTimeStamp(start2);
        if (ts1 != ts2)
            throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("timestamp"));*/

        if (data1->GetIntTime() != data2->GetIntTime())
            throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("frame: timestamp"));

        if (data1->GetInvalid() != data2->GetInvalid())
            throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("frame: invalid flag"));

        if (data1->m_is_mfx && data2->m_is_mfx)
        {
            if (data1->m_mfx_picStruct != data2->m_mfx_picStruct)
                throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("frame: m_mfx_picStruct"));

            if (data1->m_mfx_dataFlag != data2->m_mfx_dataFlag)
                throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("frame: m_mfx_dataFlag"));

            if (data1->m_mfx_viewId != data2->m_mfx_viewId)
                throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("frame: m_mfx_viewId"));

            if (data1->m_mfx_temporalId != data2->m_mfx_temporalId)
                throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("frame: m_mfx_temporalId"));

            if (data1->m_mfx_priorityId != data2->m_mfx_priorityId)
                throw OutlineException(UMC::UMC_ERR_FAILED, VM_STRING("frame: m_mfx_priorityId"));
        }
    }
    catch(OutlineException & ex)
    {
        OutlineError  error(OutlineError::OUTLINE_ERROR_TYPE_FRAME, *data1, *data2);
        ex.SetError(error);
        throw;
    }
}
