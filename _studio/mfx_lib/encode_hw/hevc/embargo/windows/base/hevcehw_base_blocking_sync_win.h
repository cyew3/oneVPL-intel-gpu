// Copyright (c) 2019-2020 Intel Corporation
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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && !defined(MFX_VA_LINUX) && defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)

#include "hevcehw_base.h"
#include "hevcehw_base_data.h"
#include "mfx_win_event_cache.h"
#include "mfx_interface_scheduler.h"

namespace HEVCEHW
{
namespace Windows
{
namespace Base
{
    class BlockingSync
        : public FeatureBase
    {
    public:
#define DECL_BLOCK_LIST\
        DECL_BLOCK(InitInternal)\
        DECL_BLOCK(InitAlloc)\
        DECL_BLOCK(AllocTask)\
        DECL_BLOCK(SubmitTask)\
        DECL_BLOCK(WaitTask)\
        DECL_BLOCK(ReportHang)
#define DECL_FEATURE_NAME "Base_BlockingSync"
#include "hevcehw_decl_blocks.h"

        BlockingSync(mfxU32 FeatureId)
            : FeatureBase(FeatureId)
        {}
        ~BlockingSync();

        void SetTimeout(mfxU32 timeOutMs) { m_timeOutMs = timeOutMs; };

        struct EventDescr
        {
            GPU_SYNC_EVENT_HANDLE Handle    = { GPU_COMPONENT_ENCODE, INVALID_HANDLE_VALUE };
            mfxU32                ReportID  = mfxU32(-1);
        };

        using TaskEvent = StorageVar<HEVCEHW::Base::Task::ReservedKey0, EventDescr>;

    protected:
        mfxU32                      m_timeOutMs         = 60000;
        MFXIScheduler2*             m_pSheduler         = nullptr;
        bool                        m_bSingleThreadMode = false;
        bool                        m_bEnabled          = false;
        std::unique_ptr<EventCache> m_EventCache;

        virtual void InitInternal(const FeatureBlocks& blocks, TPushII Push) override;
        virtual void InitAlloc(const FeatureBlocks& blocks, TPushIA Push) override;
        virtual void AllocTask(const FeatureBlocks& blocks, TPushAT Push) override;
        virtual void SubmitTask(const FeatureBlocks& blocks, TPushST Push) override;
        virtual void QueryTask(const FeatureBlocks& blocks, TPushQT Push) override;

        static mfxStatus WaitTaskSync(
            HANDLE gpuSyncEvent
            , mfxU32 reportID
            , mfxU32 timeOutMs);
    };

} //Base
} //Windows
} //namespace HEVCEHW

#endif
