// Copyright (c) 2008-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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

    std::unique_ptr<OutlineReader> m_pReader;
    std::unique_ptr<VideoOutlineWriter> m_pOutlineWriter;
    std::unique_ptr<CheckerBase> m_checker;
    UMC::VideoDecoderParams m_CurrentSequence;

    UMC::Status PerformPreprocessing(VideoData *pDataIn, VideoData *pDataOut);
    std::unique_ptr<UMC::VideoProcessing> m_Preprocessing; //for conversion of input media data
                                     //to certain color format

    Ipp32u ComputeCRC32(UMC::VideoData *data);
    ListOfEntries FindClosestFrame(const TestVideoData &testData);
    void UpdateIgnoreMembers(TestVideoData *in1, TestVideoData *in2);
};

#endif //__VIDEO_DATA_CHECKER_H__
