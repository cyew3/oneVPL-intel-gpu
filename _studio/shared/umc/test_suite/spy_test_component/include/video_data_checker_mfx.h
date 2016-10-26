//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2012 Intel Corporation. All Rights Reserved.
//

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
