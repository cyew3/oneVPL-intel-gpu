//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2012 Intel Corporation. All Rights Reserved.
//

#ifndef __VIDEO_DATA_CHECKER_H__
#define __VIDEO_DATA_CHECKER_H__

#include <memory>
#include "test_component.h"

using namespace UMC;

namespace UMC
{
    class VideoProcessing;
};

class OutlineReader;

class VideoDataCheckerBase
{
public:
    virtual ~VideoDataCheckerBase() { }

    virtual Status Init(const struct TestComponentParams *pTestParams) = 0;
    virtual Status Close() = 0;
    virtual Status Reset() = 0;

    virtual bool IsFrameExist(Ipp32s frameNumber = -1) const = 0;
    virtual bool IsSequenceExist(Ipp32s seqNumber = -1) const = 0;

    virtual Status ProcessSequence(BaseCodecParams *info, Ipp32s id = -1) = 0;
    virtual Status ProcessFrame(MediaData *in, Ipp32s id = -1) = 0;

    virtual void SetParams(const OutlineParams & params) = 0;
};

//Base test component class
class VideoDataChecker : public VideoDataCheckerBase
{
public:
    VideoDataChecker();
    virtual ~VideoDataChecker();

    virtual Status Init(const struct TestComponentParams *pTestParams);
    virtual Status Close();
    virtual Status Reset(void);

    virtual bool IsFrameExist(Ipp32s frameNumber = -1) const;
    virtual bool IsSequenceExist(Ipp32s seqNumber = -1) const;

    virtual Status ProcessSequence(BaseCodecParams *info, Ipp32s id = -1);
    virtual Status ProcessFrame(MediaData *in, Ipp32s id = -1);

    virtual void SetParams(const OutlineParams & params);

protected:
    Ipp32s m_FrameNumber;
    Ipp32s m_SequenceNumber;
    Ipp32s m_Mode;
    bool   m_CheckFramesNumber;

    OutlineParams m_params;

    std::auto_ptr<OutlineReader> m_pReader;
    std::auto_ptr<VideoOutlineWriter> m_pOutlineWriter;
    std::auto_ptr<CheckerBase> m_checker;
    UMC::VideoDecoderParams m_CurrentSequence;

    UMC::Status PerformPreprocessing(VideoData *pDataIn, VideoData *pDataOut);
    std::auto_ptr<UMC::VideoProcessing> m_Preprocessing; //for conversion of input media data
                                     //to certain color format

    Ipp32u ComputeCRC32(UMC::VideoData *data);
    ListOfEntries FindClosestFrame(const TestVideoData &testData);
    void UpdateIgnoreMembers(TestVideoData *in1, TestVideoData *in2);
};

#endif //__VIDEO_DATA_CHECKER_H__
