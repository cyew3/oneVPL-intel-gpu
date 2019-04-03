// Copyright (c) 2003-2019 Intel Corporation
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

#ifndef __OUTLINE_MFX_WRAPPER_H__
#define __OUTLINE_MFX_WRAPPER_H__

#include <memory>
#include "mfxvideo++.h"
#include "video_data_checker_mfx.h"

class MFXVideoDECODE_Checker : public MFXVideoDECODE
{
public:

    MFXVideoDECODE_Checker(mfxSession session)
        : MFXVideoDECODE(session)
    {
        // m_checker.reset(new VideoDataChecker_MFX());
    }

    mfxStatus Init(mfxVideoParam *par)
    {
        m_checker->ProcessSequence(par);
        return MFXVideoDECODE_Init(m_session, par);
    }

    mfxStatus DecodeFrameAsync(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
    {
        return MFXVideoDECODE_DecodeFrameAsync(m_session, bs, surface_work, surface_out, syncp);
    }

private:

    std::unique_ptr<VideoDataChecker_MFX> m_checker;

};

#endif // __OUTLINE_MFX_WRAPPER_H__
