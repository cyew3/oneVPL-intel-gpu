//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2012 Intel Corporation. All Rights Reserved.
//

#include <ippdc.h>
#include <memory>

#include "video_data_checker.h"
#include "outline_factory.h"
#include "umc_video_processing.h"
#include "umc_h264_dec.h"

VideoDataChecker::VideoDataChecker()
{
    m_Mode = TEST_OUTLINE_NONE;
    m_FrameNumber = -1;
    m_SequenceNumber = -1;
    m_CheckFramesNumber = true;
}

VideoDataChecker::~VideoDataChecker()
{
    Close();
}

Status VideoDataChecker::Reset()
{
    m_FrameNumber = -1;
    m_SequenceNumber = -1;
    m_CheckFramesNumber = false;
    return UMC::UMC_OK;
}

Status VideoDataChecker::Init(const struct TestComponentParams *pTestParams)
{
    if (!pTestParams)
        return UMC::UMC_ERR_NULL_PTR;

    m_Mode = pTestParams->m_Mode;
    m_CheckFramesNumber = true;

    try
    {
        if (m_Mode & TEST_OUTLINE_WRITE)
        {
            m_pOutlineWriter.reset(GetOutlineFactory()->CreateVideoOutlineWriter());
            m_pOutlineWriter->Init(pTestParams->m_OutlineFilename);
        }

        if (m_Mode & TEST_OUTLINE_READ)
        {
            m_pReader.reset(GetOutlineFactory()->CreateReader());
            m_pReader->Init(pTestParams->m_RefOutlineFilename);
            m_checker.reset(GetOutlineFactory()->CreateChecker());
        }
    } catch(...)
    {
        return UMC::UMC_ERR_FAILED;
    }

    m_Preprocessing.reset(new VideoProcessing());

    return UMC::UMC_OK;
};

UMC::Status VideoDataChecker::Close()
{
    UMC::Status sts = UMC::UMC_OK;

    if (m_CheckFramesNumber && (IsFrameExist() || IsSequenceExist()))
        sts = UMC::UMC_ERR_FAILED;

    m_pOutlineWriter.reset();
    m_pReader.reset();

    m_Preprocessing->Close();

    m_Mode = TEST_OUTLINE_NONE;
    Reset();

    return sts;
};

void VideoDataChecker::SetParams(const OutlineParams & params)
{
    m_params = params;
}

bool VideoDataChecker::IsFrameExist(Ipp32s frameNumber) const
{
    Ipp32s frameNum  = frameNumber == -1 ? m_FrameNumber + 1 : frameNumber;

    if (m_Mode & TEST_OUTLINE_READ)
    {
        std::auto_ptr<Entry_Read> entry(m_pReader->GetEntry(m_SequenceNumber, frameNum));

        if (!entry.get())
            return false;

        return true;
    }

    return false;
}

bool VideoDataChecker::IsSequenceExist(Ipp32s id) const
{
    Ipp32s seqNum = id == -1 ? m_SequenceNumber + 1 : id;

    if (m_Mode & TEST_OUTLINE_READ)
    {
        std::auto_ptr<Entry_Read> entry(m_pReader->GetSequenceEntry(seqNum));

        if (!entry.get())
            return false;

        return true;
    }

    return false;
}

ListOfEntries VideoDataChecker::FindClosestFrame(const TestVideoData &testData)
{
    ListOfEntries entries = m_pReader->FindEntry(testData, m_params.m_searchMethod);

    if (entries.empty())
    {
        throw OutlineException(UMC_ERR_FAILED, VM_STRING("can't find frame"));
    }

    return entries;
}

void VideoDataChecker::UpdateIgnoreMembers(TestVideoData *in1, TestVideoData *in2)
{
    if (!in1 || !in2)
        return;

    if (m_params.m_ignoreList & IGNORE_TIMESTAMP)
    {
        in1->SetIntTime(0);
        in2->SetIntTime(0);
    }
}

Status VideoDataChecker::ProcessFrame(MediaData *in, Ipp32s frameNumber)
{
    TestVideoData *testData = DynamicCast<TestVideoData>(in);
    
    VideoData *data = DynamicCast<VideoData>(in);
    if (!data)
        return UMC_ERR_NULL_PTR;

    try
    {
        TestVideoData tmpTestData;

        if (!testData)
        {
            m_FrameNumber++;
            m_FrameNumber = frameNumber == -1 ? m_FrameNumber : frameNumber;

            UMC::Status sts = tmpTestData.Init(data, m_SequenceNumber, m_FrameNumber);

            Ipp64f start, end;
            data->GetTime(start, end);

            Ipp64f framerate = m_CurrentSequence.info.framerate;
            Ipp64u align_value = CalculateTimestampAlignValue(framerate);
            Ipp64u ts = AlignTimestamp(GetIntTimeStamp(start), align_value);

            tmpTestData.SetIntTime(ts);

            if (sts != UMC::UMC_OK)
                return sts;

            tmpTestData.SetCRC32(ComputeCRC32(data));
            testData = &tmpTestData;
        }

        if (m_Mode & TEST_OUTLINE_WRITE)
        {
            m_pOutlineWriter->WriteFrame(testData);
        }

        if (m_Mode & TEST_OUTLINE_READ)
        {
            TestVideoData refTstData;
            ListOfEntries entries = FindClosestFrame(*testData);

            bool isOk = false;
            ListOfEntries::iterator iter = entries.begin();
            for (Ipp32s i = 0; iter != entries.end(); ++iter, i++)
            {
                m_pReader->ReadEntryStruct((*iter), &refTstData);
                try
                {
                    UpdateIgnoreMembers(&refTstData, testData);
                    m_checker->CheckFrameInfo(&refTstData, testData);
                    isOk = true;
                    break;
                } catch(...)
                {
                    if (i == (Ipp32s)entries.size() - 1)
                        throw;
                }
            }
        }
    }
    catch(OutlineException & ex)
    {
        GetOutlineFactory()->GetOutlineErrors()->SetLastError(&ex);
        return UMC_ERR_FAILED;
    }
    catch(...)
    {
        OutlineException ex(UMC_ERR_FAILED, VM_STRING("uknown error"));
        GetOutlineFactory()->GetOutlineErrors()->SetLastError(&ex);
        return UMC_ERR_FAILED;
    }

    return UMC_OK;
};

Status VideoDataChecker::ProcessSequence(BaseCodecParams *info, Ipp32s id)
{
    const UMC::VideoDecoderParams *info1 = DynamicCast<const UMC::VideoDecoderParams> (info);
    if (!info1)
        return UMC_ERR_FAILED;

    m_SequenceNumber++;
    m_SequenceNumber = id == -1 ? m_SequenceNumber : id;

    m_FrameNumber = -1; // reset frame counter

    try
    {
        m_CurrentSequence = *info1;

        if (m_Mode & TEST_OUTLINE_WRITE)
        {
            m_pOutlineWriter->WriteSequence(m_SequenceNumber, info);
        }
     
        if (m_Mode & TEST_OUTLINE_READ)
        {
            std::auto_ptr<Entry_Read> entry(m_pReader->GetSequenceEntry(m_SequenceNumber));

            if (!entry.get())
                return UMC_ERR_FAILED;

            UMC::H264VideoDecoderParams params;
            m_pReader->ReadSequenceStruct(entry.get(), &params);

            m_checker->CheckSequenceInfo(info, &params);
        }
    }
    catch(OutlineException & ex)
    {
        GetOutlineFactory()->GetOutlineErrors()->SetLastError(&ex);
        return UMC_ERR_FAILED;
    }
    catch(...)
    {
        OutlineException ex(UMC_ERR_FAILED, VM_STRING("uknown error"));
        GetOutlineFactory()->GetOutlineErrors()->SetLastError(&ex);
        return UMC_ERR_FAILED;
    }

    return UMC::UMC_OK;
}

Status VideoDataChecker::PerformPreprocessing(VideoData *pDataIn, VideoData *pDataOut)
{
    Status umcRes = UMC_OK;
    Ipp32s w, h;
    w = pDataIn->GetWidth();
    h = pDataIn->GetHeight();

    // if parameters changed we need to re- init and allocate dst video data
    // and va image (this way it'll be done at first pass)
    if ((pDataOut->GetWidth() != w) || (pDataOut->GetHeight() != h))
    {
        pDataOut->Init(w, h, NONE);
        pDataOut->Alloc();
    }

    umcRes = m_Preprocessing->GetFrame(pDataIn, pDataOut);
    return umcRes;
};

Ipp32u VideoDataChecker::ComputeCRC32(UMC::VideoData *data)
{
    Ipp32u crc32 = 0;

    if (data->GetNumPlanes() == 0)
        throw OutlineException(UMC::UMC_ERR_INVALID_PARAMS, VM_STRING("can't calculate crc32"));

    for(Ipp32s i = 0; i < data->GetNumPlanes(); i++)
    {
        UMC::VideoData::PlaneInfo info;
        data->GetPlaneInfo(&info, i);

        Ipp8u* ptr = info.m_pPlane;
        Ipp32u width  = info.m_ippSize.width * info.m_iSamples * info.m_iSampleSize;
        Ipp32u height = info.m_ippSize.height;
        size_t pitch  = info.m_nPitch;

        for (Ipp32u j = 0; j < height; j++)
        {
            ippsAdler32_8u(ptr + j*pitch, width, &crc32);
        }
    }

    return crc32;
};
