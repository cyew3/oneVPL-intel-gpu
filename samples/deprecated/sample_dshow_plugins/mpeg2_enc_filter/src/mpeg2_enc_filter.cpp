/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
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

#include "mfx_samples_config.h"

#include "mpeg2_enc_filter.h"
#include "filter_defs.h"
#include "mfx_filter_register.h"

CMPEG2EncVideoFilter::CMPEG2EncVideoFilter(TCHAR *tszName,LPUNKNOWN punk,HRESULT *phr) :
    CEncVideoFilter(tszName,punk, FILTER_GUID,phr)
{
    m_bstrFilterName = FILTER_NAME;
    m_mfxParamsVideo.mfx.CodecId = MFX_CODEC_MPEG2;

    //fill m_EncoderParams with default values
    SetDefaultParams();

    // Fill m_EncoderParams with values from registry if available,
    // otherwise write current m_EncoderParams values to registry
    ReadParamsFromRegistry();

    CopyEncoderToMFXParams(&m_EncoderParams, &m_mfxParamsVideo);
    AlignFrameSizes(&m_mfxParamsVPP.vpp.Out, (mfxU16)m_EncoderParams.frame_control.width, (mfxU16)m_EncoderParams.frame_control.height);
}

CMPEG2EncVideoFilter::~CMPEG2EncVideoFilter()
{
}

CUnknown * WINAPI CMPEG2EncVideoFilter::CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
    CMPEG2EncVideoFilter *pNewObject = new CMPEG2EncVideoFilter(FILTER_NAME, punk, phr);
    if (pNewObject == NULL) {
        *phr = E_OUTOFMEMORY;
    }
    return pNewObject;

}


HRESULT CMPEG2EncVideoFilter::GetPages(CAUUID* pPages)
{
    MSDK_CHECK_POINTER(pPages, E_POINTER);

    pPages->cElems = 3;
    pPages->pElems = (GUID*)CoTaskMemAlloc(sizeof(GUID) * pPages->cElems);

    MSDK_CHECK_POINTER(pPages->pElems, E_OUTOFMEMORY);

    pPages->pElems[0] = CLSID_VideoPropertyPage;
    pPages->pElems[1] = CLSID_MPEG2EncPropertyPage;
    pPages->pElems[2] = CLSID_AboutPropertyPage;

    return S_OK;
}

void CMPEG2EncVideoFilter::SetDefaultParams(void)
{
    m_EncoderParams.level_idc                      = Params::LL_AUTOSELECT;
    m_EncoderParams.profile_idc                    = Params::PF_AUTOSELECT;

    m_EncoderParams.ps_control.BufferSizeInKB      = 768;

    m_EncoderParams.rc_control.rc_method           = IConfigureVideoEncoder::Params::RCControl::RC_CBR;
    m_EncoderParams.rc_control.bitrate             = 0;

    m_EncoderParams.target_usage                   = MFX_TARGETUSAGE_BALANCED;
    m_EncoderParams.preset                         = CodecPreset::PRESET_USER_DEFINED;
    m_EncoderParams.frame_control.width            = 0;
    m_EncoderParams.frame_control.height           = 0;

//  adds the Sequence Header before every I-frame
    m_mfxParamsVideo.mfx.IdrInterval               = 1;
}

void CMPEG2EncVideoFilter::ReadParamsFromRegistry(void)
{
    GetParamFromReg(_T("Profile"),                   m_EncoderParams.profile_idc);
    GetParamFromReg(_T("Level"),                     m_EncoderParams.level_idc);
    GetParamFromReg(_T("PSControl.GopPicSize"),      m_EncoderParams.ps_control.GopPicSize);
    GetParamFromReg(_T("PSControl.GopRefDist"),      m_EncoderParams.ps_control.GopRefDist);
    GetParamFromReg(_T("PSControl.BufferSizeInKB"),  m_EncoderParams.ps_control.BufferSizeInKB);
    GetParamFromReg(_T("RCControl.rc_method"),       m_EncoderParams.rc_control.rc_method);
    GetParamFromReg(_T("RCControl.bitrate"),         m_EncoderParams.rc_control.bitrate);
    GetParamFromReg(_T("TargetUsage"),               m_EncoderParams.target_usage);
    GetParamFromReg(_T("Preset"),                    m_EncoderParams.preset);
    GetParamFromReg(_T("FrameControl.width"),        m_EncoderParams.frame_control.width);
    GetParamFromReg(_T("FrameControl.height"),       m_EncoderParams.frame_control.height);
}

void CMPEG2EncVideoFilter::WriteParamsToRegistry(void)
{
    SetParamToReg(_T("Profile"),                     m_EncoderParams.profile_idc);
    SetParamToReg(_T("Level"),                       m_EncoderParams.level_idc);
    SetParamToReg(_T("PSControl.GopPicSize"),        m_EncoderParams.ps_control.GopPicSize);
    SetParamToReg(_T("PSControl.GopRefDist"),        m_EncoderParams.ps_control.GopRefDist);
    SetParamToReg(_T("PSControl.BufferSizeInKB"),    m_EncoderParams.ps_control.BufferSizeInKB);
    SetParamToReg(_T("RCControl.rc_method"),         m_EncoderParams.rc_control.rc_method);
    SetParamToReg(_T("RCControl.bitrate"),           m_EncoderParams.rc_control.bitrate);
    SetParamToReg(_T("TargetUsage"),                 m_EncoderParams.target_usage);
    SetParamToReg(_T("Preset"),                      m_EncoderParams.preset);
}

void CMPEG2EncVideoFilter::ReadFrameRateFromRegistry(mfxU32 &iFrameRate)
{
    GetAuxParamFromReg(_T("TargetFrameRate"), iFrameRate);
}

bool CMPEG2EncVideoFilter::EncResetRequired(const mfxFrameInfo *pNewFrameInfo)
{
    bool ret = (m_mfxParamsVPP.vpp.In.CropX != pNewFrameInfo->CropX) ||
               (m_mfxParamsVPP.vpp.In.CropY != pNewFrameInfo->CropY) ||
               (m_mfxParamsVPP.vpp.In.CropW != pNewFrameInfo->CropW) ||
               (m_mfxParamsVPP.vpp.In.CropH != pNewFrameInfo->CropH) ||
               (m_mfxParamsVPP.vpp.In.Width != pNewFrameInfo->Width) ||
               (m_mfxParamsVPP.vpp.In.Height != pNewFrameInfo->Height);

    // Crops are not supported by MPEG2 standard, so we need to use VPP
    m_mfxParamsVPP.vpp.In.CropX = pNewFrameInfo->CropX;
    m_mfxParamsVPP.vpp.In.CropY = pNewFrameInfo->CropY;
    m_mfxParamsVPP.vpp.In.CropW = pNewFrameInfo->CropW;
    m_mfxParamsVPP.vpp.In.CropH = pNewFrameInfo->CropH;
    m_mfxParamsVPP.vpp.In.Width = pNewFrameInfo->Width;
    m_mfxParamsVPP.vpp.In.Height = pNewFrameInfo->Height;

    m_mfxParamsVPP.vpp.Out.CropX = 0;
    m_mfxParamsVPP.vpp.Out.CropY = 0;
    m_mfxParamsVPP.vpp.Out.CropW = pNewFrameInfo->CropW;
    m_mfxParamsVPP.vpp.Out.CropH = pNewFrameInfo->CropH;
    m_mfxParamsVPP.vpp.Out.Width = MSDK_ALIGN16(pNewFrameInfo->Width);
    m_mfxParamsVPP.vpp.Out.Height = MSDK_ALIGN32(pNewFrameInfo->Height);

    //return CEncVideoFilter::EncResetRequired(pNewFrameInfo);
    return ret;
}

HRESULT CMPEG2EncVideoFilter::CheckInputType(const CMediaType* mtIn)
{
    HRESULT hr = CEncVideoFilter::CheckInputType(mtIn);
    CHECK_RESULT_P_RET(hr, S_OK);

    // update registry params according to internal computations
    CopyMFXToEncoderParams(&m_EncoderParams,&m_mfxParamsVideo);

    WriteParamsToRegistry();
    return S_OK;
};

//////////////////////////////////////////////////////////////////////////