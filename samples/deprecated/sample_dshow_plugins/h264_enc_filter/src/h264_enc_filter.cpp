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

#include "h264_enc_filter.h"
#include "filter_defs.h"
#include "mfx_filter_register.h"
#include "mfx_video_enc_filter_utils.h"

CH264EncVideoFilter::CH264EncVideoFilter(TCHAR* tszName, LPUNKNOWN punk, HRESULT* phr) :
    CEncVideoFilter(tszName, punk, FILTER_GUID, phr)
{
    m_bstrFilterName                  = FILTER_NAME;
    m_mfxParamsVideo.mfx.CodecId      = MFX_CODEC_AVC;

    //fill m_EncoderParams with default values
    SetDefaultParams();

    // Fill m_EncoderParams with values from registry if available,
    // otherwise write current m_EncoderParams values to registry
    ReadParamsFromRegistry();

    CopyEncoderToMFXParams(&m_EncoderParams, &m_mfxParamsVideo);
    AlignFrameSizes(&m_mfxParamsVPP.vpp.Out, (mfxU16)m_EncoderParams.frame_control.width, (mfxU16)m_EncoderParams.frame_control.height);
}

void CH264EncVideoFilter::SetDefaultParams(void)
{
    m_EncoderParams.level_idc                      = Params::LL_AUTOSELECT;
    m_EncoderParams.profile_idc                    = Params::PF_AUTOSELECT;

    m_EncoderParams.pc_control                     = Params::PC_FRAME;

    // autoselect
    m_EncoderParams.ps_control.GopPicSize          = 0;
    m_EncoderParams.ps_control.GopRefDist          = 0;
    m_EncoderParams.ps_control.NumSlice            = 0;

    m_EncoderParams.rc_control.rc_method           = IConfigureVideoEncoder::Params::RCControl::RC_CBR;
    m_EncoderParams.rc_control.bitrate             = 0;

    m_EncoderParams.target_usage                   = MFX_TARGETUSAGE_BALANCED;
    m_EncoderParams.preset                         = CodecPreset::PRESET_USER_DEFINED;
    m_EncoderParams.frame_control.width            = 0;
    m_EncoderParams.frame_control.height           = 0;
}

void CH264EncVideoFilter::ReadParamsFromRegistry(void)
{
    GetParamFromReg(_T("Profile"),                    m_EncoderParams.profile_idc);
    GetParamFromReg(_T("Level"),                      m_EncoderParams.level_idc);
    GetParamFromReg(_T("PSControl.GopPicSize"),       m_EncoderParams.ps_control.GopPicSize);
    GetParamFromReg(_T("PSControl.GopRefDist"),       m_EncoderParams.ps_control.GopRefDist);
    GetParamFromReg(_T("PSControl.NumSlice"),         m_EncoderParams.ps_control.NumSlice);
    GetParamFromReg(_T("RCControl.rc_method"),        m_EncoderParams.rc_control.rc_method);
    GetParamFromReg(_T("RCControl.bitrate"),          m_EncoderParams.rc_control.bitrate);
    GetParamFromReg(_T("PCControl"),                  m_EncoderParams.pc_control);
    GetParamFromReg(_T("TargetUsage"),                m_EncoderParams.target_usage);
    GetParamFromReg(_T("Preset"),                     m_EncoderParams.preset);
    GetParamFromReg(_T("FrameControl.width"),         m_EncoderParams.frame_control.width);
    GetParamFromReg(_T("FrameControl.height"),        m_EncoderParams.frame_control.height);
}

void CH264EncVideoFilter::WriteParamsToRegistry(void)
{
    SetParamToReg(_T("Profile"),                        m_EncoderParams.profile_idc);
    SetParamToReg(_T("Level"),                          m_EncoderParams.level_idc);
    SetParamToReg(_T("PSControl.GopPicSize"),           m_EncoderParams.ps_control.GopPicSize);
    SetParamToReg(_T("PSControl.GopRefDist"),           m_EncoderParams.ps_control.GopRefDist);
    SetParamToReg(_T("PSControl.NumSlice"),             m_EncoderParams.ps_control.NumSlice);
    SetParamToReg(_T("RCControl.rc_method"),            m_EncoderParams.rc_control.rc_method);
    SetParamToReg(_T("RCControl.bitrate"),              m_EncoderParams.rc_control.bitrate);
    SetParamToReg(_T("PCControl"),                      m_EncoderParams.pc_control);
    SetParamToReg(_T("TargetUsage"),                    m_EncoderParams.target_usage);
    SetParamToReg(_T("Preset"),                         m_EncoderParams.preset);
}

void CH264EncVideoFilter::ReadFrameRateFromRegistry(mfxU32 &iFrameRate)
{
    GetAuxParamFromReg(_T("TargetFrameRate"), iFrameRate);
}

HRESULT CH264EncVideoFilter::GetPages(CAUUID* pPages)
{
    MSDK_CHECK_POINTER(pPages, E_POINTER);

    pPages->cElems = 3;
    pPages->pElems = (GUID*)CoTaskMemAlloc(sizeof(GUID) * pPages->cElems);

    MSDK_CHECK_POINTER(pPages->pElems, E_OUTOFMEMORY);

    pPages->pElems[0] = CLSID_VideoPropertyPage;
    pPages->pElems[1] = CLSID_H246EncPropertyPage;
    pPages->pElems[2] = CLSID_AboutPropertyPage;

    return S_OK;
}

CUnknown * WINAPI CH264EncVideoFilter::CreateInstance(LPUNKNOWN punk, HRESULT* phr)
{
    CH264EncVideoFilter *pNewObject = new CH264EncVideoFilter(FILTER_NAME, punk, phr);

    if (NULL == pNewObject)
    {
        *phr = E_OUTOFMEMORY;
    }

    return pNewObject;
}

mfxU8 GetPictStructFromCodingType(mfxI32 nCodingType)
{
    switch (nCodingType)
    {
    case 0x01:
        return MFX_PICSTRUCT_PROGRESSIVE;
    case 0x02:
        return MFX_PICSTRUCT_FIELD_TFF;
    case 0x04:
        return MFX_PICSTRUCT_FIELD_BFF;
    }

    return 0;
}

HRESULT CH264EncVideoFilter::CheckInputType(const CMediaType* mtIn)
{
    HRESULT hr = CEncVideoFilter::CheckInputType(mtIn);
    CHECK_RESULT_P_RET(hr, S_OK);

    // update registry params according to internal computations
    CopyMFXToEncoderParams(&m_EncoderParams, &m_mfxParamsVideo);

    WriteParamsToRegistry();
    return S_OK;
}

BOOL CH264EncVideoFilter::CheckOnlyBitrateChanged(mfxVideoParam* paramsVideo1, mfxVideoParam* paramsVideo2, DWORD preset)
{
    BOOL        bOnlyBitrateChanged = FALSE;

    if (CodecPreset::PRESET_LOW_LATENCY == preset)
    {
        IConfigureVideoEncoder::Params  H264Params1, H264Params2;

        memset(&H264Params1, 0, sizeof(H264Params1));
        memset(&H264Params2, 0, sizeof(H264Params2));

        H264Params1.preset =
            H264Params2.preset = preset;

        CopyMFXToEncoderParams(&H264Params1, paramsVideo1);
        CopyMFXToEncoderParams(&H264Params2, paramsVideo2);

        H264Params1.rc_control.bitrate =
            H264Params2.rc_control.bitrate = 100;

        if (!memcmp(&H264Params1, &H264Params2, sizeof(H264Params2)))
        {
            bOnlyBitrateChanged = TRUE;
        }
    }

    return bOnlyBitrateChanged;
}

STDMETHODIMP CH264EncVideoFilter::SetParams(Params* params)
{
    MSDK_CHECK_POINTER(params, E_POINTER)

    HRESULT       hr = S_OK;
    mfxVideoParam old_mfxparams = m_mfxParamsVideo;

    BOOL          bOnlyBitrateChanged = FALSE; //only bitrate changed

    if (m_EncoderParams.preset == params->preset)
    {
        CopyEncoderToMFXParams(params, &m_mfxParamsVideo);
        bOnlyBitrateChanged = CheckOnlyBitrateChanged(&old_mfxparams, &m_mfxParamsVideo, m_EncoderParams.preset);
    }

    if (bOnlyBitrateChanged && State_Running != m_State)
    {
        mfxFrameInfo info = m_mfxParamsVideo.mfx.FrameInfo;

        AlignFrameSizes(&info, (mfxU16)m_EncoderParams.frame_control.width, (mfxU16)m_EncoderParams.frame_control.height);

        if (memcmp(&m_mfxParamsVideo.mfx.FrameInfo, &info, sizeof(mfxFrameInfo)))
        {
            bOnlyBitrateChanged = FALSE;
        }
    }

    if (bOnlyBitrateChanged)
    {
        if (MFX_ERR_NONE != m_pEncoder->DynamicBitrateChange(&m_mfxParamsVideo))
        {
            hr = E_FAIL;
        }
    }
    else
    {
        m_mfxParamsVideo = old_mfxparams;
        hr = CEncVideoFilter::SetParams(params);
    }

    return hr;
}