// Copyright (c) 2008-2018 Intel Corporation
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

#ifndef __VIDEO_DATA_CHECKER_MFX_H__
#define __VIDEO_DATA_CHECKER_MFX_H__

#include "video_data_checker.h"

using namespace UMC;

#include "mfxstructures.h"

/*class MFXCheckerBase
{
public:
    virtual ~MFXCheckerBase() {}
    virtual Status ProcessSequence(const mfxVideoParam * pars, Ipp32s id = -1) = 0;
    virtual Status ProcessFrame(const mfxFrameSurface1 * surf, Ipp32s id = -1) = 0;
};*/

class VideoDataChecker_MFX_Base : public VideoDataCheckerBase
{
public:
    virtual Status ProcessSequence(const mfxVideoParam * pars, Ipp32s id = -1) = 0;
    virtual Status ProcessFrame(const mfxFrameSurface1 * surf, Ipp32s id = -1) = 0;
};

class VideoDataChecker_MFX : public VideoDataChecker_MFX_Base, private VideoDataChecker
{
public:
    virtual Status Init(const struct TestComponentParams *pTestParams);
    virtual Status Close();
    virtual Status Reset();

    virtual bool IsFrameExist(Ipp32s frameNumber = -1) const;
    virtual bool IsSequenceExist(Ipp32s seqNumber = -1) const;

    virtual Status ProcessSequence(BaseCodecParams *info, Ipp32s id = -1);
    virtual Status ProcessFrame(MediaData *in, Ipp32s id = -1);

    virtual Status ProcessSequence(const mfxVideoParam * pars, Ipp32s id = -1);
    virtual Status ProcessFrame(const mfxFrameSurface1 * surf, Ipp32s id = -1);

    virtual void SetParams(const OutlineParams & params);

private:
    void ConvertSequenceToUmc(const mfxVideoParam * pars, VideoDecoderParams * umc_pars);
    void ConvertFrameToUmc(const mfxFrameSurface1 * surf, TestVideoData *pData);
};

#endif //__VIDEO_DATA_CHECKER_MFX_H__
