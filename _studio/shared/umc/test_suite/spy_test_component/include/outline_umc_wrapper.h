//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

#ifndef __OUTLINE_UMC_WRAPPER_H__
#define __OUTLINE_UMC_WRAPPER_H__

#include "umc_video_decoder.h"
#include "video_data_checker.h"

class OutlineVideoDecoder : public UMC::VideoDecoder
{
public:

    OutlineVideoDecoder(UMC::VideoDecoder * dec, const struct TestComponentParams * params)
        : m_dec(dec)
    {
        m_checker.reset(new VideoDataChecker());
        if (m_checker->Init(params) != UMC_OK)
            throw "";
    }

    virtual Status Init(BaseCodecParams *init)
    {
        UMC::Status  sts = m_dec->Init(init);
        if (sts != UMC::UMC_OK)
            return sts;

        VideoDecoderParams params;
        sts = m_dec->GetInfo(&params);
        if (sts != UMC::UMC_OK)
            return sts;

        sts = m_checker->ProcessSequence(&params);
        if (sts != UMC::UMC_OK)
            return sts;

        return UMC::UMC_OK;
    }

    virtual Status GetFrame(MediaData *in, MediaData *out)
    {
        UMC::Status sts = m_dec->GetFrame(in, out);
        if (sts != UMC::UMC_OK)
            return sts;

        sts = m_checker->ProcessFrame(out);
        if (sts != UMC::UMC_OK)
            return sts;

        return UMC::UMC_OK;
    }

    virtual UMC::Status Reset()
    {
        UMC::Status sts = m_dec->Reset();
        if (sts != UMC::UMC_OK)
            return sts;

        sts = m_checker->Reset();
        if (sts != UMC::UMC_OK)
            return sts;

        return UMC::UMC_ERR_UNSUPPORTED;
    }

    virtual UMC::Status ResetSkipCount()
    {
        return m_dec->ResetSkipCount();
    }

    virtual UMC::Status SkipVideoFrame(Ipp32s num)
    {
        return m_dec->SkipVideoFrame(num);
    }

    virtual Ipp32u GetNumOfSkippedFrames()
    {
        return m_dec->GetNumOfSkippedFrames();
    }

private:
    UMC::VideoDecoder     *m_dec;
    std::auto_ptr<VideoDataChecker> m_checker;
};

#endif //__OUTLINE_UMC_WRAPPER_H__
