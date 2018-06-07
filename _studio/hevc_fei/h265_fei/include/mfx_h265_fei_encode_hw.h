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

static const mfxPluginUID  MFX_PLUGINID_HEVC_FEI_ENCODE = {{0x54, 0x18, 0xa7, 0x06, 0x66, 0xf9, 0x4d, 0x5c, 0xb4, 0xf7, 0xb1, 0xca, 0xee, 0x86, 0x33, 0x9b}};

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

        if (EncFrameCtrl && (!!EncFrameCtrl->PerCuQp != !!m_vpar.m_pps.cu_qp_delta_enabled_flag))
        {
            SoftReset(task);
        }

        return;
    }

    void SoftReset(MfxHwH265Encode::Task& task)
    {
        m_vpar.m_pps.cu_qp_delta_enabled_flag = 1 - m_vpar.m_pps.cu_qp_delta_enabled_flag;

        // If state of CTU QP buffer changes, PPS header update required
        task.m_insertHeaders |= MfxHwH265Encode::INSERT_PPS;

        (dynamic_cast<VAAPIh265FeiEncoder*> (m_ddi.get()))->SoftReset(m_vpar);
    }
};

class H265FeiEncodePlugin : public MFXEncoderPlugin
{
public:
    static MFXEncoderPlugin* Create()
    {
        return new H265FeiEncodePlugin(false);
    }

    static mfxStatus CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg)
    {
        if (memcmp(&guid, &MFX_PLUGINID_HEVC_FEI_ENCODE, sizeof(mfxPluginUID)))
        {
            return MFX_ERR_NOT_FOUND;
        }

        H265FeiEncodePlugin* tmp_pplg = nullptr;

        try
        {
            tmp_pplg = new H265FeiEncodePlugin(false);
        }
        catch(std::bad_alloc&)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }
        catch(...)
        {
            return MFX_ERR_UNKNOWN;
        }

        *mfxPlg = tmp_pplg->m_adapter;
        tmp_pplg->m_createdByDispatcher = true;

        return MFX_ERR_NONE;
    }

    virtual mfxStatus PluginInit(mfxCoreInterface *core) override;
    virtual mfxStatus PluginClose() override;

    virtual mfxStatus GetPluginParam(mfxPluginParam *par) override;

    virtual mfxU32 GetPluginType()
    {
        return MFX_PLUGINTYPE_VIDEO_ENCODE;
    }

    virtual void Release() override
    {
        delete this;
    }

    virtual mfxStatus Close() override
    {
        if (m_pImpl.get())
            return m_pImpl->Close();
        else
            return MFX_ERR_NOT_INITIALIZED;
    }

    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a) override
    {
        if (m_pImpl.get())
            return H265FeiEncode_HW::Execute((reinterpret_cast<void*>(m_pImpl.get())), task, uid_p, uid_a);
        else
            return MFX_ERR_NOT_INITIALIZED;
    }

    virtual mfxStatus FreeResources(mfxThreadTask /*task*/, mfxStatus /*sts*/) override
    {
        return MFX_ERR_NONE;
    }

    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest * /*out*/) override
    {
        return H265FeiEncode_HW::QueryIOSurf(&m_core, par, in);
    }

    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) override
    {
        return H265FeiEncode_HW::Query(&m_core, in, out);
    }

    virtual mfxStatus Init(mfxVideoParam *par) override
    {
        mfxStatus sts;
        if (m_pImpl.get() && (m_pImpl->IsInitialized()))
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        m_pImpl.reset(new H265FeiEncode_HW(&m_core, &sts));
        MFX_CHECK_STS(sts);
        return m_pImpl->Init(par);
    }

    virtual mfxStatus Reset(mfxVideoParam *par) override
    {
        if (m_pImpl.get())
            return m_pImpl->Reset(par);
        else
            return MFX_ERR_NOT_INITIALIZED;
    }

    virtual mfxStatus GetVideoParam(mfxVideoParam *par) override
    {
        if (m_pImpl.get())
            return m_pImpl->GetVideoParam(par);
        else
            return MFX_ERR_NOT_INITIALIZED;
    }

    virtual mfxStatus EncodeFrameSubmit(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxThreadTask *task) override
    {
        if (m_pImpl.get())
            return m_pImpl->EncodeFrameSubmit(ctrl, surface, bs, task);
        else
            return MFX_ERR_NOT_INITIALIZED;
    }

    virtual mfxStatus SetAuxParams(void*, int) override
    {
        return MFX_ERR_UNSUPPORTED;
    }
protected:
    explicit H265FeiEncodePlugin(bool CreateByDispatcher);
    virtual ~H265FeiEncodePlugin();

    bool m_createdByDispatcher;
    MFXPluginAdapter<MFXEncoderPlugin> m_adapter;
    mfxCoreInterface m_core;
    std::unique_ptr<H265FeiEncode_HW> m_pImpl;
};

} //MfxHwH265FeiEncode

#endif
