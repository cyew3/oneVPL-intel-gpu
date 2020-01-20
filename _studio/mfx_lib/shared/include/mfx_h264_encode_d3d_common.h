// Copyright (c) 2011-2019 Intel Corporation
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

#ifndef __MFX_H264_ENCODE_D3D_COMMON_H
#define __MFX_H264_ENCODE_D3D_COMMON_H

#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined (MFX_VA_WIN)

#include "mfx_interface_scheduler.h"

#include "encoding_ddi.h"
#include "mfx_h264_encode_interface.h"

namespace MfxHwH264Encode
{
    class D3DXCommonEncoder : public DriverEncoder
    {
    public:
        D3DXCommonEncoder();

        virtual
        ~D3DXCommonEncoder();

        virtual
            mfxStatus QueryStatus(DdiTask & task, mfxU32 fieldId);

        virtual
            mfxStatus Execute(mfxHDLPair pair, DdiTask const & task, mfxU32 fieldId, PreAllocatedVector const & sei);

        // Init
        virtual
            mfxStatus Init(VideoCORE *core);

        // Destroy
        virtual
            mfxStatus Destroy();

    protected:
        // async call
        virtual mfxStatus QueryStatusAsync(DdiTask & task, mfxU32 fieldId) = 0;

        // sync call
        virtual mfxStatus WaitTaskSync(DdiTask & task, mfxU32 fieldId, mfxU32 timeOutM);

        virtual mfxStatus ExecuteImpl(mfxHDLPair pair, DdiTask const & task, mfxU32 fieldId, PreAllocatedVector const & sei) = 0;

        MFXIScheduler2 *pSheduler;
        bool m_bSingleThreadMode;
        mfxU32 m_timeoutSync;
        mfxU32 m_timeoutForTDR;

    private:
        D3DXCommonEncoder(D3DXCommonEncoder const &); // no implementation
        D3DXCommonEncoder & operator =(D3DXCommonEncoder const &); // no implementation

    };
}; // namespace

#endif // #if defined (MFX_ENABLE_H264_VIDEO_ENCODE_HW) && (MFX_VA_WIN)
#endif // __MFX_H264_ENCODE_D3D_COMMON_H
/* EOF */
