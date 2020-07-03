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

#include "mfx_common.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE) && !defined (MFX_VA_LINUX)

#include "av1ehw_base_win.h"
#include "av1ehw_base_data.h"
#include "av1ehw_base_general.h"
#include "av1ehw_base_packer.h"
#include "av1ehw_base_alloc_win.h"
#include "av1ehw_base_task.h"
#include "av1ehw_base_d3d11_win.h"
#include "av1ehw_base_ddi_packer_win.h"
#include "av1ehw_base_tile.h"
#include "av1ehw_base_dirty_rect_win.h"
#include "av1ehw_base_blocking_sync_win.h"


using namespace AV1EHW;

Windows::Base::MFXVideoENCODEAV1_HW::MFXVideoENCODEAV1_HW(
    VideoCORE& core
    , mfxStatus& status
    , eFeatureMode mode)
    : AV1EHW::Base::MFXVideoENCODEAV1_HW(core)
{
    status = MFX_ERR_UNKNOWN;
    auto vaType = core.GetVAType();

    m_features.emplace_back(new Allocator(FEATURE_ALLOCATOR));

    if (vaType == MFX_HW_D3D11)
    {
        m_features.emplace_back(new DDI_D3D11(FEATURE_DDI));
    }
    else
    {
        status = MFX_ERR_UNSUPPORTED;
        return;
    }

    m_features.emplace_back(new DDIPacker(FEATURE_DDI_PACKER));
    m_features.emplace_back(new General(FEATURE_GENERAL));
    m_features.emplace_back(new TaskManager(FEATURE_TASK_MANAGER));
    m_features.emplace_back(new Packer(FEATURE_PACKER));
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)
    m_features.emplace_back(new BlockingSync(FEATURE_BLOCKING_SYNC));
#endif
    m_features.emplace_back(new Tile(FEATURE_TILE));
    m_features.emplace_back(new DirtyRect(FEATURE_DIRTY_RECT));

    InternalInitFeatures(status, mode);
}

mfxStatus Windows::Base::MFXVideoENCODEAV1_HW::Init(mfxVideoParam *par)
{
    mfxStatus sts = AV1EHW::Base::MFXVideoENCODEAV1_HW::Init(par);
    MFX_CHECK(sts >= MFX_ERR_NONE, sts);

    {
        auto& queue = BQ<BQ_SubmitTask>::Get(*this);
        Reorder(queue
            , { FEATURE_DDI, IDDI::BLK_SubmitTask }
            , { FEATURE_DIRTY_RECT, DirtyRect::BLK_PatchDDITask });
    }

    {
        auto& queue = BQ<BQ_QueryTask>::Get(*this);
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)
        Reorder(queue
            , { FEATURE_DDI, IDDI::BLK_QueryTask}
            , {FEATURE_BLOCKING_SYNC, BlockingSync::BLK_WaitTask});
        Reorder(queue
            , {FEATURE_DDI_PACKER, IDDIPacker::BLK_QueryTask}
            , {FEATURE_BLOCKING_SYNC, BlockingSync::BLK_ReportHang});
#endif //defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)
    }

    return sts;
}

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
