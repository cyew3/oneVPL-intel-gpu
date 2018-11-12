// Copyright (c) 2003-2018 Intel Corporation
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
