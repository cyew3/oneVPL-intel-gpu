//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#include "mfx_config.h"
#if defined(MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE)

#include "mfx_h265_fei_encode_vaapi.h"

using namespace MfxHwH265Encode;

namespace MfxHwH265FeiEncode
{
    VAAPIh265FeiEncoder::VAAPIh265FeiEncoder()
        : VAAPIEncoder()
    {}

    VAAPIh265FeiEncoder::~VAAPIh265FeiEncoder()
    {}

    mfxStatus VAAPIh265FeiEncoder::PreSubmitExtraStage(Task const & task)
    {
#if 0
        {
            VABufferID &vaFeiFrameControlId = VABufferNew(VABID_FEI_FRM_CTRL, 0);

            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "FrameCtrl");

            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaCreateBuffer");
                vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncMiscParameterBufferType,
                    sizeof(VAEncMiscParameterBuffer) + sizeof (VAEncMiscParameterFEIFrameControlHevc),
                    1,
                    NULL,
                    &vaFeiFrameControlId);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }

            VAEncMiscParameterBuffer *miscParam;
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
                vaMapBuffer(m_vaDisplay,
                    vaFeiFrameControlId,
                    (void **)&miscParam);
            }

            miscParam->type = (VAEncMiscParameterType)VAEncMiscParameterTypeFEIFrameControlIntel; // ?????
            VAEncMiscParameterFEIFrameControlHevc* vaFeiFrameControl = (VAEncMiscParameterFEIFrameControlHevc*)miscParam->data;
            memset(vaFeiFrameControl, 0, sizeof(VAEncMiscParameterFEIFrameControlHevc));

            mfxExtFeiHevcEncFrameCtrl* EncFrameCtrl = reinterpret_cast<mfxExtFeiHevcEncFrameCtrl*>GetBufById(task.m_ctrl, MFX_EXTBUFF_HEVCFEI_ENC_CTRL);
            MFX_CHECK(EncFrameCtrl, MFX_ERR_UNDEFINED_BEHAVIOR);

            vaFeiFrameControl->function                 = ENC_FUNCTION_ENC_PAK;
            vaFeiFrameControl->search_path              = EncFrameCtrl->SearchPath;
            vaFeiFrameControl->len_sp                   = EncFrameCtrl->LenSP;
            vaFeiFrameControl->ref_width                = EncFrameCtrl->RefWidth;
            vaFeiFrameControl->ref_height               = EncFrameCtrl->RefHeight;
            vaFeiFrameControl->search_window            = EncFrameCtrl->SearchWindow;
            vaFeiFrameControl->num_mv_predictors_l0     = EncFrameCtrl->NumMvPredictorsL0;
            vaFeiFrameControl->num_mv_predictors_l1     = EncFrameCtrl->NumMvPredictorsL1;
            vaFeiFrameControl->multi_pred_l0            = EncFrameCtrl->MultiPredL0;
            vaFeiFrameControl->multi_pred_l1            = EncFrameCtrl->MultiPredL1;
            vaFeiFrameControl->sub_pel_mode             = EncFrameCtrl->SubPelMode;
            vaFeiFrameControl->adaptive_search          = EncFrameCtrl->AdaptiveSearch;
            vaFeiFrameControl->mv_predictor_input       = EncFrameCtrl->MVPredictor;
            vaFeiFrameControl->per_block_qp             = EncFrameCtrl->PerCtbQp;
            vaFeiFrameControl->per_ctb_input            = EncFrameCtrl->PerCtbInput;
            vaFeiFrameControl->colocated_ctb_distortion = EncFrameCtrl->CoLocatedCtbDistortion;
            vaFeiFrameControl->force_lcu_split          = EncFrameCtrl->ForceLcuSplit;

            mfxExtFeiHevcEncMVPredictors* mvp = reinterpret_cast<mfxExtFeiHevcEncMVPredictors*>GetBufById(task.m_ctrl, MFX_EXTBUFF_HEVCFEI_ENC_MV_PRED);
            vaFeiFrameControl->mv_predictor = mvp ? mvp->VaBufferID : VA_INVALID_ID;


            mfxExtFeiHevcEncQP* qp = reinterpret_cast<mfxExtFeiHevcEncQP*>GetBufById(task.m_ctrl, MFX_EXTBUFF_HEVCFEI_ENC_QP);
            vaFeiFrameControl->qp = qp ? qp->VaBufferID : VA_INVALID_ID;

            mfxExtFeiHevcEncCtbCtrl* ctbctrl = reinterpret_cast<mfxExtFeiHevcEncCtbCtrl*>GetBufById(task.m_ctrl, MFX_EXTBUFF_HEVCFEI_ENC_CTB_CTRL);
            vaFeiFrameControl->ctb_ctrl = ctbctrl ? ctbctrl->VaBufferID : VA_INVALID_ID;


            mfxExtFeiHevcPakCtbRecordV0* ctbcmd = reinterpret_cast<mfxExtFeiHevcPakCtbRecordV0*>GetBufById(task.m_ctrl, MFX_EXTBUFF_HEVCFEI_PAK_CTB_REC);
            vaFeiFrameControl->ctb_cmd = ctbcmd ? ctbcmd->VaBufferID : VA_INVALID_ID;

            mfxExtFeiHevcPakCuRecordV0* curec = reinterpret_cast<mfxExtFeiHevcPakCuRecordV0*>GetBufById(task.m_ctrl, MFX_EXTBUFF_HEVCFEI_PAK_CU_REC);
            vaFeiFrameControl->cu_record = curec ? curec->VaBufferID : VA_INVALID_ID;

            mfxExtFeiHevcDistortion* distortion = reinterpret_cast<mfxExtFeiHevcDistortion*>GetBufById(task.m_ctrl, MFX_EXTBUFF_HEVCFEI_ENC_DIST);
            vaFeiFrameControl->distortion = distortion ? distortion->VaBufferID : VA_INVALID_ID;
        }
#endif
        return MFX_ERR_NONE;
    }

    mfxStatus VAAPIh265FeiEncoder::PostQueryExtraStage()
    {
        return MFX_ERR_NONE;
    }
}

#endif