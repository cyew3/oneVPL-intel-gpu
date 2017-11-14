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
#if defined (MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE)
#include "mfx_h265_fei_encode_hw.h"

using namespace MfxHwH265Encode;

namespace MfxHwH265FeiEncode
{

H265FeiEncodePlugin::H265FeiEncodePlugin(bool CreateByDispatcher)
    : Plugin(CreateByDispatcher)
{
}

H265FeiEncodePlugin::~H265FeiEncodePlugin()
{
    Close();
}

DriverEncoder* H265FeiEncodePlugin::CreateHWh265Encoder(MFXCoreInterface* core, ENCODER_TYPE type)
{
    core;
    type;

    return new VAAPIh265FeiEncoder;
}

mfxStatus H265FeiEncodePlugin::GetPluginParam(mfxPluginParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    par->PluginUID        = MFX_PLUGINID_HEVC_FEI_ENCODE;
    par->PluginVersion    = 1;
    par->ThreadPolicy     = MFX_THREADPOLICY_SERIAL;
    par->MaxThreadNum     = 1;
    par->APIVersion.Major = MFX_VERSION_MAJOR;
    par->APIVersion.Minor = MFX_VERSION_MINOR;
    par->Type             = MFX_PLUGINTYPE_VIDEO_ENCODE;
    par->CodecId          = MFX_CODEC_HEVC;

    return MFX_ERR_NONE;
}


mfxStatus H265FeiEncodePlugin::Close()
{
    Plugin::Close();
    return MFX_ERR_NONE;
}

mfxStatus H265FeiEncodePlugin::ExtraParametersCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs)
{
    if (!surface) return MFX_ERR_NONE; // In case of frames draining in display order

    MFX_CHECK(ctrl, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxPlatform p = {};

    mfxStatus sts = m_core.QueryPlatform(&p);
    MFX_CHECK_STS(sts);

#if (MFX_VERSION >= MFX_VERSION_NEXT)
    bool isSKL = p.CodeName == MFX_PLATFORM_SKYLAKE, isICLplus = p.CodeName >= MFX_PLATFORM_ICELAKE;
#else
    bool isSKL = p.CodeName == MFX_PLATFORM_SKYLAKE, isICLplus = false;
#endif


    // mfxExtFeiHevcEncFrameCtrl is a mandatory buffer
    mfxExtFeiHevcEncFrameCtrl* EncFrameCtrl = reinterpret_cast<mfxExtFeiHevcEncFrameCtrl*>(GetBufById(ctrl, MFX_EXTBUFF_HEVCFEI_ENC_CTRL));
    MFX_CHECK(EncFrameCtrl, MFX_ERR_UNDEFINED_BEHAVIOR);

    // Check HW limitations for mfxExtFeiHevcEncFrameCtrl parameters
    MFX_CHECK(EncFrameCtrl->NumMvPredictors[0] <= 4, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(EncFrameCtrl->NumMvPredictors[1] <= 4, MFX_ERR_UNDEFINED_BEHAVIOR);

    MFX_CHECK(EncFrameCtrl->MultiPred[0]       <= (isICLplus ? 2 : 1),
                                                     MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(EncFrameCtrl->MultiPred[1]       <= (isICLplus ? 2 : 1),
                                                     MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(EncFrameCtrl->SubPelMode         <= 3 &&
              EncFrameCtrl->SubPelMode         != 2, MFX_ERR_UNDEFINED_BEHAVIOR);

    MFX_CHECK(EncFrameCtrl->MVPredictor == 0
          ||  EncFrameCtrl->MVPredictor == 1
          ||  EncFrameCtrl->MVPredictor == 2
          || (EncFrameCtrl->MVPredictor == 3 && isICLplus)
          ||  EncFrameCtrl->MVPredictor == 7,        MFX_ERR_UNDEFINED_BEHAVIOR);

    MFX_CHECK(EncFrameCtrl->ForceCtuSplit <= (isSKL ? 1: 0), MFX_ERR_UNDEFINED_BEHAVIOR);

    switch (EncFrameCtrl->NumFramePartitions)
    {
    case 1:
    case 2:
    case 4:
    case 8:
    case 16:
        break;
    default:
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    if (EncFrameCtrl->SearchWindow)
    {
        MFX_CHECK(EncFrameCtrl->AdaptiveSearch == 0
               && EncFrameCtrl->LenSP          == 0
               && EncFrameCtrl->SearchPath     == 0
               && EncFrameCtrl->RefWidth       == 0
               && EncFrameCtrl->RefHeight      == 0, MFX_ERR_UNDEFINED_BEHAVIOR);

        MFX_CHECK(EncFrameCtrl->SearchWindow <= 6,   MFX_ERR_UNDEFINED_BEHAVIOR);
    }
    else
    {
        MFX_CHECK(EncFrameCtrl->AdaptiveSearch <= 1,  MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(EncFrameCtrl->SearchPath     <= 2,  MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(EncFrameCtrl->LenSP          >= 1 &&
                  EncFrameCtrl->LenSP          <= 63, MFX_ERR_UNDEFINED_BEHAVIOR);

        if (EncFrameCtrl->AdaptiveSearch)
        {
            MFX_CHECK(EncFrameCtrl->LenSP      >= 2,  MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        if (isSKL)
        {
            mfxU16 frameType = GetFrameType(m_vpar, m_frameOrder - m_lastIDR);

            MFX_CHECK(EncFrameCtrl->RefWidth  % 4 == 0
                   && EncFrameCtrl->RefHeight % 4 == 0
                   && EncFrameCtrl->RefWidth  <= ((frameType & MFX_FRAMETYPE_B) ? 32 : 64)
                   && EncFrameCtrl->RefHeight <= 32,
                   MFX_ERR_UNDEFINED_BEHAVIOR);
        }
        else
        {
            // ICL+
            MFX_CHECK(  (EncFrameCtrl->RefWidth == 64 && EncFrameCtrl->RefHeight == 64)
                     || (EncFrameCtrl->RefWidth == 48 && EncFrameCtrl->RefHeight == 40),
                     MFX_ERR_UNDEFINED_BEHAVIOR);
        }
    }

    // Check if requested buffers are provided

    if (EncFrameCtrl->MVPredictor)
    {
        MFX_CHECK(GetBufById(ctrl, MFX_EXTBUFF_HEVCFEI_ENC_MV_PRED), MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    if (EncFrameCtrl->PerCuQp)
    {
        MFX_CHECK(GetBufById(ctrl, MFX_EXTBUFF_HEVCFEI_ENC_QP), MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    if (EncFrameCtrl->PerCtuInput)
    {
        MFX_CHECK(GetBufById(ctrl, MFX_EXTBUFF_HEVCFEI_ENC_CTU_CTRL), MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    return MFX_ERR_NONE;
}

};
#endif
