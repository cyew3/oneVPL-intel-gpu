// Copyright (c) 2016-2020 Intel Corporation
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

#ifndef __MFX_VP9_ENCODE_HW_D3D_COMMON_H
#define __MFX_VP9_ENCODE_HW_D3D_COMMON_H

#if defined (MFX_VA_WIN)

#include "mfx_common.h"

#include "mfx_interface_scheduler.h"
#include "mfx_vp9_encode_hw_utils.h"
#include "mfx_vp9_encode_hw_ddi.h"
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
#include "mfx_win_event_cache.h"
#endif

namespace MfxHwVP9Encode
{
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
                Task & task) override;

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
        // Init
        virtual
            mfxStatus InitBlockingSync(
                VideoCORE *pCore);

        // Destroy
        virtual
            mfxStatus DestroyBlockingSync();
#endif
    protected:
        // async call
        virtual
            mfxStatus QueryStatusAsync(
                Task & task) = 0;

        bool m_bIsBlockingTaskSyncEnabled;

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
        mfxStatus SetGPUSyncEventEnable(VideoCORE *pCore);

        // sync call
        virtual
            mfxStatus WaitTaskSync(
                Task & task,
                mfxU32 timeOutMs);

        MFXIScheduler2 *pScheduler;
        bool m_bSingleThreadMode;
        mfxU32 m_TaskSyncTimeOutMs;

        std::unique_ptr<EventCache> m_EventCache;
#endif

        D3DXCommonEncoder(D3DXCommonEncoder const &) = delete;
        D3DXCommonEncoder & operator =(D3DXCommonEncoder const &) = delete;

    };
}; // namespace

#endif // #if defined (MFX_VA_WIN)
#endif // __MFX_VP9_ENCODE_HW_D3D_COMMON_H
/* EOF */
