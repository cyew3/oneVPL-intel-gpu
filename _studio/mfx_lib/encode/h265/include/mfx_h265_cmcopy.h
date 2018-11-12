// Copyright (c) 2015-2018 Intel Corporation
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

#pragma once

#include "mfx_common.h"
#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_defs.h"

class CmDevice;
class CmProgram;
class CmKernel;
class CmQueue;
class CmTask;
class CmThreadSpace;

namespace H265Enc {

    class CmCopy : public NonCopyable
    {
    public:
        CmCopy();
        mfxStatus Init(mfxHDL handle, mfxHandleType handleType);
        void Close();
        mfxStatus SetParam(Ipp32s width, Ipp32s height, Ipp32s fourcc, Ipp32s pitchLuma, Ipp32s pitchChroma, Ipp32s paddingLuW, Ipp32s paddingChW);
        mfxStatus Copy(mfxHDL video, Ipp8u *luma, Ipp8u *chroma);
    private:
        CmDevice *m_device;
        CmProgram *m_program;
        CmKernel *m_kernel;
        CmQueue *m_queue;
        CmTask *m_task;
        CmThreadSpace *m_threadSpace;
        Ipp32s m_width;
        Ipp32s m_height;
        Ipp32s m_paddingLuW;
        Ipp32s m_paddingChW;
    };
};

#endif