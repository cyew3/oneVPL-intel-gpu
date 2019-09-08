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

#ifndef __MFX_H265_ENCODE_HW_D3D_COMMON_H
#define __MFX_H265_ENCODE_HW_D3D_COMMON_H

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) && defined (MFX_VA_WIN)

#include "mfx_interface_scheduler.h"

#include "encoding_ddi.h"
#include "mfx_h265_encode_hw_utils.h"
#include "mfx_h265_encode_hw.h"

namespace MfxHwH265Encode
{

#define DEFAULT_H265_TIMEOUT_MS         60000      // 1 minute
#define DEFAULT_H265_TIMEOUT_MS_SIM     3600000    // 1 hour

    class D3DXCommonEncoder : public DriverEncoder
    {
    public:
        D3DXCommonEncoder();

        virtual
        ~D3DXCommonEncoder();

        // It may be blocking or not blocking call
        // depend on define MFX_ENABLE_HW_BLOCKING_TASK_SYNC
        virtual
        mfxStatus QueryStatus(
            Task & task);

        virtual
        mfxStatus Execute(
            Task const & task, mfxHDLPair surface);

        // Init
        virtual
        mfxStatus Init(VideoCORE *pCore, GUID guid);

        // Destroy
        virtual
            mfxStatus Destroy();

    protected:
        // async call
        virtual
        mfxStatus QueryStatusAsync(
            Task & task)=0;

        // sync call
        virtual
        mfxStatus WaitTaskSync(
            HANDLE gpuSyncEvent,
            mfxU32 statusReportNumber,
            mfxU32 timeOutMs);

        virtual
        mfxStatus ExecuteImpl(
            Task const & task, mfxHDLPair surface) = 0;

        MFXIScheduler2 *pSheduler;
        bool m_bSingleThreadMode;
        bool m_bIsBlockingTaskSyncEnabled;
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
        mfxU32 m_TaskSyncTimeOutMs;
#endif

    private:
        D3DXCommonEncoder(D3DXCommonEncoder const &); // no implementation
        D3DXCommonEncoder & operator =(D3DXCommonEncoder const &); // no implementation

    };
}; // namespace

#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) && (MFX_VA_WIN)
#endif // __MFX_H265_ENCODE_HW_D3D_COMMON_H
/* EOF */
