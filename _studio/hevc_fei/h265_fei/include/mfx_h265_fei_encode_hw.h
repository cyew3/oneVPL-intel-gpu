//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 - 2018 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE)

#include "mfx_h265_fei_encode_vaapi.h"
#include "mfx_h265_encode_hw.h"

namespace MfxHwH265FeiEncode
{

class H265FeiEncode_HW : public MfxHwH265Encode::MFXVideoENCODEH265_HW
{
public:
    H265FeiEncode_HW(mfxCoreInterface *core, mfxStatus *status)
        :MFXVideoENCODEH265_HW(core, status)
    {}

    virtual ~H265FeiEncode_HW()
    {
        Close();
    }

    virtual MfxHwH265Encode::DriverEncoder* CreateHWh265Encoder(MFXCoreInterface* /*core*/, MfxHwH265Encode::ENCODER_TYPE type = MfxHwH265Encode::ENCODER_DEFAULT) override
    {
        type;

        return new VAAPIh265FeiEncoder;
    }

    virtual mfxStatus Reset(mfxVideoParam *par) override
    {
        // waiting for submitted in driver tasks
        // This Sync is required to guarantee correct encoding of Async tasks in case of dynamic CTU QP change
        mfxStatus sts = WaitingForAsyncTasks(true);
        MFX_CHECK_STS(sts);

        // Call base Reset()
        return MfxHwH265Encode::MFXVideoENCODEH265_HW::Reset(par);
    }

    virtual mfxStatus ExtraParametersCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs) override;

    virtual mfxStatus ExtraCheckVideoParam(MfxHwH265Encode::MfxVideoParam & par, ENCODE_CAPS_HEVC const & caps, bool bInit = false) override
    {
        // HEVC FEI Encoder uses own controls to switch on LCU QP buffer
        if (MfxHwH265Encode::IsOn(m_vpar.m_ext.CO3.EnableMBQP))
        {
            m_vpar.m_ext.CO3.EnableMBQP = MFX_CODINGOPTION_OFF;

            return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

        return MFX_ERR_NONE;
    }

    virtual void ExtraTaskPreparation(MfxHwH265Encode::Task& task) override
    {
        mfxExtFeiHevcEncFrameCtrl* EncFrameCtrl = reinterpret_cast<mfxExtFeiHevcEncFrameCtrl*>(GetBufById(task.m_ctrl, MFX_EXTBUFF_HEVCFEI_ENC_CTRL));

        mfxExtFeiHevcRepackCtrl* repackctrl = reinterpret_cast<mfxExtFeiHevcRepackCtrl*>(GetBufById(task.m_ctrl, MFX_EXTBUFF_HEVCFEI_REPACK_CTRL));

        if (m_vpar.m_pps.cu_qp_delta_enabled_flag)
        {
            if (!repackctrl && !EncFrameCtrl->PerCuQp)
            {
                // repackctrl or PerCuQp is disabled, so insert PPS and turn the flag off.
                SoftReset(task);
            }
        }
        else
        {
            if (repackctrl || EncFrameCtrl->PerCuQp)
            {
                // repackctrl or PerCuQp is enabled, so insert PPS and turn the flag on.
                SoftReset(task);
            }
        }
    }

    void SoftReset(MfxHwH265Encode::Task& task)
    {
        m_vpar.m_pps.cu_qp_delta_enabled_flag = 1 - m_vpar.m_pps.cu_qp_delta_enabled_flag;

        // If state of CTU QP buffer changes, PPS header update required
        task.m_insertHeaders |= MfxHwH265Encode::INSERT_PPS;

        (dynamic_cast<VAAPIh265FeiEncoder*> (m_ddi.get()))->SoftReset(m_vpar);
    }
};

} //MfxHwH265FeiEncode

#endif
