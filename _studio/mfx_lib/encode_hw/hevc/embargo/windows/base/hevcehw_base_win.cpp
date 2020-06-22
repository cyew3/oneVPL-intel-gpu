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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && !defined (MFX_VA_LINUX)

#include "hevcehw_base_win.h"
#include "hevcehw_base_data.h"
#include "hevcehw_base_legacy.h"
#include "hevcehw_base_parser.h"
#include "hevcehw_base_recon_info_win.h"
#include "hevcehw_base_packer.h"
#include "hevcehw_base_hrd.h"
#include "hevcehw_base_alloc_win.h"
#include "hevcehw_base_task.h"
#include "hevcehw_base_ext_brc.h"
#include "hevcehw_base_dirty_rect_win.h"
#if !defined(MFX_EXT_DPB_HEVC_DISABLE) && (MFX_VERSION >= MFX_VERSION_NEXT)
#include "hevcehw_base_dpb_report.h"
#endif
#include "hevcehw_base_dump_files.h"
#include "hevcehw_base_extddi_win.h"
#include "hevcehw_base_hdr_sei.h"
#include "hevcehw_base_max_frame_size_win.h"
#include "hevcehw_base_d3d9_win.h"
#include "hevcehw_base_d3d11_win.h"
#include "hevcehw_base_ddi_packer_win.h"
#include "hevcehw_base_protected_win.h"
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)
#include "hevcehw_base_blocking_sync_win.h"
#endif
#include "hevcehw_base_roi_win.h"
#include "hevcehw_base_interlace_win.h"
#include "hevcehw_base_brc_sliding_window_win.h"
#include "hevcehw_base_mb_per_sec_win.h"
#include "hevcehw_base_encoded_frame_info_win.h"
#if defined(MFX_ENABLE_HEVCE_WEIGHTED_PREDICTION)
#include "hevcehw_base_weighted_prediction_win.h"
#endif
#if defined (MFX_ENABLE_HEVC_CUSTOM_QMATRIX)
#include "hevcehw_base_qmatrix_win.h"
#endif
#if defined (MFX_ENABLE_HEVCE_UNITS_INFO)
#include "hevcehw_base_units_info_win.h"
#endif
#if defined (MFX_ENABLE_LP_LOOKAHEAD) || defined (MFX_ENABLE_ENCTOOLS_LPLA)
#include "hevcehw_base_lpla_analysis.h"
#include "hevcehw_base_lpla_enc.h"
#endif

using namespace HEVCEHW;
using namespace HEVCEHW::Base;

Windows::Base::MFXVideoENCODEH265_HW::MFXVideoENCODEH265_HW(
    VideoCORE& core
    , mfxStatus& status
    , eFeatureMode mode)
    : HEVCEHW::Base::MFXVideoENCODEH265_HW(core)
{
    status = MFX_ERR_UNKNOWN;
    auto vaType = core.GetVAType();

    m_features.emplace_back(new Parser(FEATURE_PARSER));
    m_features.emplace_back(new Allocator(FEATURE_ALLOCATOR));

    if (vaType == MFX_HW_D3D11)
    {
        m_features.emplace_back(new DDI_D3D11(FEATURE_DDI));
    }
    else if (vaType == MFX_HW_D3D9)
    {
        m_features.emplace_back(new DDI_D3D9(FEATURE_DDI));
    }
    else
    {
        status = MFX_ERR_UNSUPPORTED;
        return;
    }

    m_features.emplace_back(new DDIPacker(FEATURE_DDI_PACKER));
    m_features.emplace_back(new Legacy(FEATURE_LEGACY));
    m_features.emplace_back(new ReconInfo(FEATURE_RECON_INFO));
    m_features.emplace_back(new HRD(FEATURE_HRD));
    m_features.emplace_back(new TaskManager(FEATURE_TASK_MANAGER));
    m_features.emplace_back(new Packer(FEATURE_PACKER));
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)
    m_features.emplace_back(new BlockingSync(FEATURE_BLOCKING_SYNC));
#endif
    m_features.emplace_back(new ExtBRC(FEATURE_EXT_BRC));
    m_features.emplace_back(new DirtyRect(FEATURE_DIRTY_RECT));
#if !defined(MFX_EXT_DPB_HEVC_DISABLE) && (MFX_VERSION >= MFX_VERSION_NEXT)
    m_features.emplace_back(new DPBReport(FEATURE_DPB_REPORT));
#endif
#if (DEBUG_REC_FRAMES_INFO)
    m_features.emplace_back(new DumpFiles(FEATURE_DUMP_FILES));
#endif //(DEBUG_REC_FRAMES_INFO)
    m_features.emplace_back(new ExtDDI(FEATURE_EXTDDI));
    m_features.emplace_back(new HdrSei(FEATURE_HDR_SEI));
    m_features.emplace_back(new MaxFrameSize(FEATURE_MAX_FRAME_SIZE));
    m_features.emplace_back(new Protected(FEATURE_PROTECTED));
#if defined(MFX_ENABLE_HEVCE_ROI)
    m_features.emplace_back(new ROI(FEATURE_ROI));
#endif //defined(MFX_ENABLE_HEVCE_ROI)
    m_features.emplace_back(new Interlace(FEATURE_INTERLACE));
    m_features.emplace_back(new BRCSlidingWindow(FEATURE_BRC_SLIDING_WINDOW));
    m_features.emplace_back(new MbPerSec(FEATURE_MB_PER_SEC));
    m_features.emplace_back(new EncodedFrameInfo(FEATURE_ENCODED_FRAME_INFO));
#if defined(MFX_ENABLE_HEVCE_WEIGHTED_PREDICTION)
    m_features.emplace_back(new WeightPred(FEATURE_WEIGHTPRED));
#endif
#if defined (MFX_ENABLE_HEVC_CUSTOM_QMATRIX)
    m_features.emplace_back(new QMatrix(FEATURE_QMATRIX));
#endif
#if defined (MFX_ENABLE_HEVCE_UNITS_INFO)
    m_features.emplace_back(new UnitsInfo(FEATURE_UNITS_INFO));
#endif
#if defined (MFX_ENABLE_LP_LOOKAHEAD) || defined (MFX_ENABLE_ENCTOOLS_LPLA)
    m_features.emplace_back(new LpLookAheadAnalysis(FEATURE_LPLA_ANALYSIS));
    m_features.emplace_back(new LpLookAheadEnc(FEATURE_LPLA_ENCODE));
#endif

    InternalInitFeatures(status, mode);

    if (mode & QUERY1)
    {
        auto& queue = BQ<BQ_Query1WithCaps>::Get(*this);

        queue.splice(queue.begin(), queue, Get(queue, { FEATURE_MB_PER_SEC, MbPerSec::BLK_QueryCORE }));

        Reorder(
            queue
            , { FEATURE_DDI, IDDI::BLK_QueryCaps }
            , { FEATURE_MB_PER_SEC, MbPerSec::BLK_QueryDDI }
            , PLACE_AFTER);
    }

    if (mode & INIT)
    {
        auto& qIA = BQ<BQ_InitAlloc>::Get(*this);
        Reorder(qIA
            , { FEATURE_LEGACY, Legacy::BLK_AllocRaw }
            , { FEATURE_RECON_INFO, ReconInfo::BLK_AllocRec }
        , PLACE_AFTER);
        Reorder(qIA
            , { FEATURE_RECON_INFO, ReconInfo::BLK_AllocRec }
            , { FEATURE_DDI, IDDI::BLK_CreateService }
        , PLACE_AFTER);
    }
}

mfxStatus Windows::Base::MFXVideoENCODEH265_HW::Init(mfxVideoParam *par)
{
    mfxStatus sts = HEVCEHW::Base::MFXVideoENCODEH265_HW::Init(par);
    MFX_CHECK(sts >= MFX_ERR_NONE, sts);

    {
        auto& queue = BQ<BQ_FrameSubmit>::Get(*this);
        Reorder(queue
            , { FEATURE_LEGACY, Legacy::BLK_CheckBS }
            , { FEATURE_PROTECTED, Protected::BLK_SetBsInfo });
    }

    {
        auto& queue = BQ<BQ_SubmitTask>::Get(*this);
        Reorder(queue
            , { FEATURE_DDI, IDDI::BLK_SubmitTask }
            , { FEATURE_BRC_SLIDING_WINDOW, BRCSlidingWindow::BLK_PatchDDITask });
#if defined (MFX_ENABLE_HEVC_CUSTOM_QMATRIX)
        Reorder(queue
            , { FEATURE_DDI, IDDI::BLK_SubmitTask }
            , { FEATURE_QMATRIX, QMatrix::BLK_PatchDDITask });
#endif //defined(MFX_ENABLE_HEVC_CUSTOM_QMATRIX)
        Reorder(queue
            , { FEATURE_DDI, IDDI::BLK_SubmitTask }
            , { FEATURE_DIRTY_RECT, DirtyRect::BLK_PatchDDITask });
        Reorder(queue
            , { FEATURE_DDI, IDDI::BLK_SubmitTask }
            , { FEATURE_EXTDDI, ExtDDI::BLK_PatchDDITask });
#if defined(MFX_ENABLE_HEVCE_ROI)
        Reorder(queue, { FEATURE_DDI, IDDI::BLK_SubmitTask }, { FEATURE_ROI, ROI::BLK_PatchDDITask });
#endif //defined(MFX_ENABLE_HEVCE_ROI)
        Reorder(queue, { FEATURE_DDI, IDDI::BLK_SubmitTask }
            , { FEATURE_MAX_FRAME_SIZE, MaxFrameSize::BLK_PatchDDITask });
    }

    {
        auto& queue = BQ<BQ_QueryTask>::Get(*this);
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)
        Reorder(queue
            , { FEATURE_DDI, IDDI::BLK_QueryTask }
            , { FEATURE_BLOCKING_SYNC, BlockingSync::BLK_WaitTask });
        Reorder(queue
            , { FEATURE_DDI_PACKER, IDDIPacker::BLK_QueryTask }
            , { FEATURE_BLOCKING_SYNC, BlockingSync::BLK_ReportHang });
#endif //defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC)
        Reorder(queue
            , { FEATURE_LEGACY, Legacy::BLK_CopyBS }
            , { FEATURE_PROTECTED, Protected::BLK_SetBsInfo });
        Reorder(queue
            , { FEATURE_DDI_PACKER, IDDIPacker::BLK_QueryTask }
            , { FEATURE_PROTECTED, Protected::BLK_GetFeedback });
    }

    return sts;
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
