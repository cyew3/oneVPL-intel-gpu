/******************************************************************************\
Copyright (c) 2017, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "hevc_fei_preenc.h"


FEI_Preenc::FEI_Preenc(MFXVideoSession * session, const mfxFrameInfo& frameInfo)
    : m_pmfxSession(session)
    , m_mfxPREENC(*m_pmfxSession)
{
    MSDK_MEMCPY_VAR(m_videoParams.mfx.FrameInfo, &frameInfo, sizeof(mfxFrameInfo));
}

FEI_Preenc::~FEI_Preenc()
{
    m_mfxPREENC.Close();
    m_pmfxSession = NULL;
}

mfxStatus FEI_Preenc::Init()
{
    mfxStatus sts = m_mfxPREENC.Init(&m_videoParams);
    MSDK_CHECK_STATUS(sts, "FEI PreENC Init failed");
    MSDK_CHECK_WRN(sts, "FEI PreENC Init");

    // just in case, call GetVideoParam to get current Encoder state
    sts = m_mfxPREENC.GetVideoParam(&m_videoParams);
    MSDK_CHECK_STATUS(sts, "FEI PreENC GetVideoParam failed");

    return sts;
}


mfxStatus FEI_Preenc::Reset(mfxU16 width, mfxU16 height)
{
    if (width && height)
    {
        m_videoParams.mfx.FrameInfo.Width = width;
        m_videoParams.mfx.FrameInfo.Height = height;
    }

    mfxStatus sts = m_mfxPREENC.Reset(&m_videoParams);
    MSDK_CHECK_STATUS(sts, "FEI PreENC Reset failed");
    MSDK_CHECK_WRN(sts, "FEI PreENC Reset");

    // just in case, call GetVideoParam to get current Encoder state
    sts = m_mfxPREENC.GetVideoParam(&m_videoParams);
    MSDK_CHECK_STATUS(sts, "FEI PreENC GetVideoParam failed");

    return sts;
}

mfxStatus FEI_Preenc::QueryIOSurf(mfxFrameAllocRequest* request)
{
    mfxStatus sts;

    // temporary session is created to avoid potential conflicts with components in current session
    MFXVideoSession tmpSession;
    sts = tmpSession.Init(MFX_IMPL_HARDWARE_ANY | MFX_IMPL_VIA_VAAPI, NULL);
    MSDK_CHECK_STATUS(sts, "tmpSession.Init failed");
    // temporary ENCODE component is created to init surfaces, as PreENC and ENCODE share the same surfaces pool,
    // and as PreENC doesn't support QueryIOSurf without DS.
    MFXVideoENCODE tmpEncode(tmpSession);
    MfxVideoParamsWrapper tmpVideoParamsWrapper(m_videoParams);

    mfxExtFeiParam* pExtBufInit = tmpVideoParamsWrapper.get<mfxExtFeiParam>();
    MSDK_CHECK_POINTER(pExtBufInit, MFX_ERR_NULL_PTR);
    pExtBufInit->Func = MFX_FEI_FUNCTION_ENCODE;

    sts = tmpEncode.QueryIOSurf(static_cast<mfxVideoParam*>(&tmpVideoParamsWrapper), request);
    MSDK_CHECK_STATUS(sts, "FEI PreENC QueryIOSurf failed");

    return sts;
}

mfxStatus FEI_Preenc::SetParameters(const sInputParams& params)
{
    mfxStatus sts = MFX_ERR_NONE;

    // default settings
    m_videoParams.mfx.CodecId           = MFX_CODEC_AVC; // not MFX_CODEC_HEVC until PreENC is changed for HEVC
    m_videoParams.mfx.RateControlMethod = MFX_RATECONTROL_CQP; // For now FEI work with RATECONTROL_CQP only
    m_videoParams.mfx.TargetUsage       = 0; // FEI doesn't have support of
    m_videoParams.mfx.TargetKbps        = 0; // these features
    m_videoParams.AsyncDepth            = 1; // inherited limitation from AVC FEI
    m_videoParams.IOPattern             = MFX_IOPATTERN_IN_VIDEO_MEMORY;

    // user defined settings
    m_videoParams.mfx.QPI = m_videoParams.mfx.QPP = m_videoParams.mfx.QPB = params.QP;
    m_videoParams.mfx.GopRefDist   = params.nRefDist;
    m_videoParams.mfx.GopPicSize   = params.nGopSize;
    m_videoParams.mfx.GopOptFlag   = params.nGopOptFlag;
    m_videoParams.mfx.IdrInterval  = params.nIdrInterval;
    m_videoParams.mfx.NumRefFrame  = params.nNumRef;
    m_videoParams.mfx.NumSlice     = params.nNumSlices;
    m_videoParams.mfx.EncodedOrder = params.bEncodedOrder;


    /* Create extension buffer to Init FEI PREENC */
    m_videoParams.enableExtParam(MFX_EXTBUFF_FEI_PARAM);
    mfxExtFeiParam* pExtBufInit = m_videoParams.get<mfxExtFeiParam>();
    MSDK_CHECK_POINTER(pExtBufInit, MFX_ERR_NULL_PTR);

    pExtBufInit->Func = MFX_FEI_FUNCTION_PREENC;

    // configure B-pyramid settings
    m_videoParams.enableExtParam(MFX_EXTBUFF_CODING_OPTION2);
    mfxExtCodingOption2* pCodingOption2 = m_videoParams.get<mfxExtCodingOption2>();
    MSDK_CHECK_POINTER(pCodingOption2, MFX_ERR_NULL_PTR);

    pCodingOption2->BRefType = params.BRefType;

    m_videoParams.enableExtParam(MFX_EXTBUFF_CODING_OPTION3);
    mfxExtCodingOption3* pCO3 = m_videoParams.get<mfxExtCodingOption3>();
    MSDK_CHECK_POINTER(pCO3, MFX_ERR_NULL_PTR);

    pCO3->GPB = params.GPB;
    pCO3->NumRefActiveP[0]   = params.NumRefActiveP;
    pCO3->NumRefActiveBL0[0] = params.NumRefActiveBL0;
    pCO3->NumRefActiveBL1[0] = params.NumRefActiveBL1;

    return sts;
}

const MfxVideoParamsWrapper& FEI_Preenc::GetVideoParam()
{
    return m_videoParams;
}


mfxFrameInfo * FEI_Preenc::GetFrameInfo()
{
    return &m_videoParams.mfx.FrameInfo;
}
