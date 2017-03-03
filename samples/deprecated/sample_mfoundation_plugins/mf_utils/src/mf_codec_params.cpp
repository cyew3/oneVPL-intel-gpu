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

#include "mf_utils.h"
#include "mf_codec_params.h"

#undef  MFX_TRACE_CATEGORY
#define MFX_TRACE_CATEGORY _T("mfx_mft_unk")

/*------------------------------------------------------------------------------*/
// MFConfigureMfxCodec class

MFConfigureMfxCodec::MFConfigureMfxCodec(void):
    m_bVpp(false),
    m_pWorkTicker(NULL),
    m_ticks_LiveTimeStartTick(0),
    m_ticks_WorkTime(0),
    m_ticks_ProcessInput(0),
    m_ticks_ProcessOutput(0),
#ifdef GET_MEM_USAGE
    m_AvgMemUsage(0),
    m_MaxMemUsage(0),
    m_MemCount(0),
#endif //GET_MEM_USAGE
    m_pCpuUsager(NULL),
    m_CpuUsage(0),
    m_TimeTotal(0),
    m_TimeKernel(0),
    m_TimeUser(0),
    m_pInfoFile(NULL)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    TCHAR file_name[MAX_PATH] = {0};

    memset(&m_MfxParamsVideo, 0, sizeof(mfxVideoParam));
    memset(&m_MfxCodecInfo, 0, sizeof(mfxCodecInfo));

    if (S_OK == mf_get_reg_string(_T(REG_PARAMS_PATH),
                                  _T(REG_INFO_FILE), file_name, MAX_PATH))
    {
        m_pInfoFile = mywfopen(file_name, L"a");
    }
    MFX_LTRACE_WS(MF_TL_GENERAL, file_name);
    MFX_LTRACE_P(MF_TL_GENERAL, m_pInfoFile);
}

/*------------------------------------------------------------------------------*/

MFConfigureMfxCodec::~MFConfigureMfxCodec(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    if (m_pInfoFile) fclose(m_pInfoFile);
}

/*------------------------------------------------------------------------------*/

HRESULT MFConfigureMfxCodec::GetParams(mfxVideoParam *params)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);

    CHECK_POINTER(params, E_POINTER);
    memset(params, 0, sizeof(mfxVideoParam));
    params->AsyncDepth = m_MfxParamsVideo.AsyncDepth;
    if (m_bVpp)
        memcpy_s((void*)&(params->vpp), sizeof(params->vpp), (void*)&(m_MfxParamsVideo.vpp), sizeof(m_MfxParamsVideo.vpp));
    else
        memcpy_s((void*)&(params->mfx), sizeof(params->mfx), (void*)&(m_MfxParamsVideo.mfx), sizeof(m_MfxParamsVideo.mfx));
    params->Protected = m_MfxParamsVideo.Protected;
    params->IOPattern = m_MfxParamsVideo.IOPattern;
    //TODO: params->ExtParam
    //TODO: params->NumExtParam
    MFX_LTRACE_S(MF_TL_NOTES, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

HRESULT MFConfigureMfxCodec::GetInfo(mfxCodecInfo *info)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);

    CHECK_POINTER(info, E_POINTER);
    memcpy_s((void*)info, sizeof(mfxCodecInfo), (void*)&m_MfxCodecInfo, sizeof(mfxCodecInfo));
    MFX_LTRACE_S(MF_TL_NOTES, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

void MFConfigureMfxCodec::UpdateTimes(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);

    m_MfxCodecInfo.m_LiveTime = GET_TIME(mf_get_tick(), m_ticks_LiveTimeStartTick, m_gFrequency);
    m_MfxCodecInfo.m_WorkTime = GET_TIME(m_ticks_WorkTime, 0, m_gFrequency);
    m_MfxCodecInfo.m_ProcessInputTime  = GET_TIME(m_ticks_ProcessInput, 0, m_gFrequency);
    m_MfxCodecInfo.m_ProcessOutputTime = GET_TIME(m_ticks_ProcessOutput, 0, m_gFrequency);

    MFX_LTRACE_F(MF_TL_NOTES, m_MfxCodecInfo.m_LiveTime);
    MFX_LTRACE_F(MF_TL_NOTES, m_MfxCodecInfo.m_WorkTime);
    MFX_LTRACE_F(MF_TL_NOTES, m_MfxCodecInfo.m_ProcessInputTime);
    MFX_LTRACE_F(MF_TL_NOTES, m_MfxCodecInfo.m_ProcessOutputTime);
}

/*------------------------------------------------------------------------------*/

void MFConfigureMfxCodec::GetPerformance(void)
{
#ifdef GET_MEM_USAGE
    size_t mem_usage = mf_get_mem_usage();

    if (mem_usage > m_MaxMemUsage) m_MaxMemUsage = mem_usage;
    m_AvgMemUsage = (mfxF64)m_AvgMemUsage + (mfxF64)(mem_usage - m_AvgMemUsage)/(mfxF64)(m_MemCount+1);
    ++m_MemCount;
#endif //GET_MEM_USAGE
}

/*------------------------------------------------------------------------------*/

#define PRINT_PARAM(NAME) \
{ \
    fwprintf(m_pInfoFile, L"plugin = %s: id = %p: %s.%s = %d\n", \
        plugin, id, (m_bVpp)? L"vpp": L"mfx", _T(#NAME), params->NAME); \
    fflush(m_pInfoFile); \
}

/*------------------------------------------------------------------------------*/

void MFConfigureMfxCodec::PrintInfo(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);

    if (m_pInfoFile)
    {
        void* id = GetID();
        LPWSTR plugin = GetCodecName();

        if (!plugin) plugin = L"Unknown plug-in";

        fwprintf(m_pInfoFile, L"\nplugin = %s: id = %p: PLUG-IN INFO\n",
            plugin, id); fflush(m_pInfoFile);

        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_MSDKImpl = %d\n",
            plugin, id, m_MfxCodecInfo.m_MSDKImpl); fflush(m_pInfoFile);
        // statistics: times, mem and cpu usages
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_LiveTime = %f\n",
            plugin, id, m_MfxCodecInfo.m_LiveTime); fflush(m_pInfoFile);
//        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_WorkTime = %f\n",
//            plugin, id, m_MfxCodecInfo.m_WorkTime); fflush(m_pInfoFile);
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_ProcessInputTime = %f\n",
            plugin, id, m_MfxCodecInfo.m_ProcessInputTime); fflush(m_pInfoFile);
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_ProcessOutputTime = %f\n",
            plugin, id, m_MfxCodecInfo.m_ProcessOutputTime); fflush(m_pInfoFile);
#ifdef GET_MEM_USAGE
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_AvgMemUsage = %d (%f mb)\n",
            plugin, id, (mfxU32)m_AvgMemUsage, m_AvgMemUsage/(1024.0*1024.0)); fflush(m_pInfoFile);
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_MaxMemUsage = %d (%f mb)\n",
            plugin, id, m_MaxMemUsage, m_MaxMemUsage/(1024.0*1024.0)); fflush(m_pInfoFile);
#endif //GET_MEM_USAGE
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_CpuUsage = %f\n",
            plugin, id, m_CpuUsage); fflush(m_pInfoFile);
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_TimeTotal = %f\n",
            plugin, id, GET_TIME(m_TimeTotal, 0, m_gFrequency)); fflush(m_pInfoFile);
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_TimeKernel = %f\n",
            plugin, id, GET_CPU_TIME(m_TimeKernel, 0, MF_CPU_TIME_FREQUENCY)); fflush(m_pInfoFile);
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_TimeUser = %f\n",
            plugin, id, GET_CPU_TIME(m_TimeUser, 0, MF_CPU_TIME_FREQUENCY)); fflush(m_pInfoFile);
        // work statistics
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_bVppUsed = %d\n",
            plugin, id, m_MfxCodecInfo.m_bVppUsed); fflush(m_pInfoFile);
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_nInFrames = %d\n",
            plugin, id, m_MfxCodecInfo.m_nInFrames); fflush(m_pInfoFile);
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_uiInFramesType = %d\n",
            plugin, id, m_MfxCodecInfo.m_uiInFramesType); fflush(m_pInfoFile);
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_nVppOutFrames = %d\n",
            plugin, id, m_MfxCodecInfo.m_nVppOutFrames); fflush(m_pInfoFile);
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_uiVppOutFramesType = %d\n",
            plugin, id, m_MfxCodecInfo.m_uiVppOutFramesType); fflush(m_pInfoFile);
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_nOutFrames = %d\n",
            plugin, id, m_MfxCodecInfo.m_nOutFrames); fflush(m_pInfoFile);
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_uiOutFramesType = %d\n",
            plugin, id, m_MfxCodecInfo.m_uiOutFramesType); fflush(m_pInfoFile);
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_InitStatus = %d\n",
            plugin, id, m_MfxCodecInfo.m_InitStatus); fflush(m_pInfoFile);
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_ErrorStatus = %d\n",
            plugin, id, m_MfxCodecInfo.m_ErrorStatus); fflush(m_pInfoFile);
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_hrErrorStatus = %x\n",
            plugin, id, m_MfxCodecInfo.m_hrErrorStatus); fflush(m_pInfoFile);
        fwprintf(m_pInfoFile, L"plugin = %s: id = %p: m_uiErrorResetCount = %d\n",
            plugin, id, m_MfxCodecInfo.m_uiErrorResetCount); fflush(m_pInfoFile);

        if (!m_bVpp)
        { // decoders and encoders common info
            mfxInfoMFX *params = &(m_MfxParamsVideo.mfx);
            //TODO: AsyncDepth, Protected, IOPattern, ExtParam, NumExtParam

            fwprintf(m_pInfoFile, L"\nplugin = %s: id = %p: MFX COMMON (DECODER/ENCODER) INFO\n",
                plugin, id); fflush(m_pInfoFile);

            PRINT_PARAM(FrameInfo.FourCC);
            PRINT_PARAM(FrameInfo.Width);
            PRINT_PARAM(FrameInfo.Height);
            PRINT_PARAM(FrameInfo.CropX);
            PRINT_PARAM(FrameInfo.CropY);
            PRINT_PARAM(FrameInfo.CropW);
            PRINT_PARAM(FrameInfo.CropH);
            PRINT_PARAM(FrameInfo.FrameRateExtN);
            PRINT_PARAM(FrameInfo.FrameRateExtD);
            PRINT_PARAM(FrameInfo.AspectRatioW);
            PRINT_PARAM(FrameInfo.AspectRatioH);
            PRINT_PARAM(FrameInfo.PicStruct);
            PRINT_PARAM(FrameInfo.ChromaFormat);
            PRINT_PARAM(CodecId);
            PRINT_PARAM(CodecProfile);
            PRINT_PARAM(CodecLevel);
            PRINT_PARAM(NumThread);
        }
    }
}

/*------------------------------------------------------------------------------*/
// MFEncoderParams class

MFEncoderParams::MFEncoderParams(void)
{
    MFParamNominalRange *pNominalRange = NULL;
    SAFE_NEW(pNominalRange, MFParamNominalRange(this));
    m_pNominalRange.reset(pNominalRange);

    MFParamSliceControl *pSliceControl = NULL;
    SAFE_NEW(pSliceControl, MFParamSliceControl(this));
    m_pSliceControl.reset(pSliceControl);

    MFParamCabac *pCabac = NULL;
    SAFE_NEW(pCabac, MFParamCabac(this));
    m_pCabac.reset(pCabac);

    MFParamMaxMBperSec *pMaxMBperSec = NULL;
    SAFE_NEW(pMaxMBperSec, MFParamMaxMBperSec(this));
    m_pMaxMBperSec.reset(pMaxMBperSec);
}

/*------------------------------------------------------------------------------*/
// ICodecAPI methods

HRESULT MFEncoderParams::IsSupported(const GUID *Api)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    CHECK_POINTER(Api, E_POINTER);
    if ((CODECAPI_AVEncMPVProfile == *Api) ||
        (CODECAPI_AVEncMPVLevel == *Api))
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
        return S_OK;
    }
    MFX_LTRACE_S(MF_TL_GENERAL, "E_NOTIMPL");
    return E_NOTIMPL;
}

/*------------------------------------------------------------------------------*/

HRESULT MFEncoderParams::IsModifiable(const GUID* /*Api*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}

/*------------------------------------------------------------------------------*/

HRESULT MFEncoderParams::GetParameterRange(const GUID* /*Api*/,
                                           VARIANT* /*ValueMin*/,
                                           VARIANT* /*ValueMax*/,
                                           VARIANT* /*SteppingDelta*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}

/*------------------------------------------------------------------------------*/

HRESULT MFEncoderParams::GetParameterValues(const GUID* /*Api*/,
                                            VARIANT** /*Values*/,
                                            ULONG* /*ValuesCount*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}

/*------------------------------------------------------------------------------*/

HRESULT MFEncoderParams::GetDefaultValue(const GUID* /*Api*/, VARIANT* /*Value*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}

/*------------------------------------------------------------------------------*/

HRESULT MFEncoderParams::GetValue(const GUID* Api, VARIANT* Value)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = VFW_E_CODECAPI_NO_CURRENT_VALUE;

    CHECK_POINTER(Api, E_POINTER);
    CHECK_POINTER(Value, E_POINTER);
    if (CODECAPI_AVEncMPVProfile == *Api)
    {
        mfxU32 ms_profile = 0, mfx_profile = 0;

        mfx_profile = (mfxU32)m_MfxParamsVideo.mfx.CodecProfile;
        ms_profile  = MFX_2_MS_VALUE(ProfilesTbl, mfx_profile);
        if ((eAVEncMPVProfile_unknown != ms_profile) ||
            (eAVEncH264VProfile_unknown != ms_profile))
        {
            Value->vt   = VT_UI4;
            Value->lVal = ms_profile;
            hr = S_OK;
        }
        MFX_LTRACE_2(MF_TL_GENERAL, "CODECAPI_AVEncMPVProfile: ",
                    "ms_profile = %d, mfx_profile = %d",
                    ms_profile, mfx_profile);
    }
    else if (CODECAPI_AVEncMPVLevel == *Api)
    {
        mfxU32 ms_level = 0, mfx_level = 0;

        mfx_level = (mfxU32)m_MfxParamsVideo.mfx.CodecLevel;
        ms_level  = MFX_2_MS_VALUE(LevelsTbl, mfx_level);
        if (ms_level)
        {
            Value->vt   = VT_UI4;
            Value->lVal = ms_level;
            hr = S_OK;
        }
        MFX_LTRACE_2(MF_TL_GENERAL, "CODECAPI_AVEncMPVLevel: ",
                    "ms_level = %d, mfx_level = %d",
                    ms_level, mfx_level);
    }
    else
    {
        MFX_LTRACE_GUID(MF_TL_GENERAL, *Api);
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFEncoderParams::SetValue(const GUID *Api, VARIANT *Value)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = E_INVALIDARG;

    CHECK_POINTER(Api, E_POINTER);
    CHECK_POINTER(Value, E_POINTER);
    if (CODECAPI_AVEncMPVProfile == *Api)
    {
        mfxU32 ms_profile = 0, mfx_profile = 0;
        if (VT_UI4 == Value->vt)
        {
            ms_profile  = Value->lVal;
            mfx_profile = MS_2_MFX_VALUE(ProfilesTbl, ms_profile);
            if (MFX_PROFILE_UNKNOWN != mfx_profile)
            {
                m_MfxParamsVideo.mfx.CodecProfile = (mfxU16)mfx_profile;
                hr = S_OK;
            }
        }
        MFX_LTRACE_2(MF_TL_GENERAL, "CODECAPI_AVEncMPVProfile: ",
                    "ms_profile = %d, mfx_profile = %d",
                    ms_profile, mfx_profile);
    }
    else if (CODECAPI_AVEncMPVLevel == *Api)
    {
        mfxU32 ms_level = 0, mfx_level = 0;
        if (VT_UI4 == Value->vt)
        {
            ms_level  = Value->lVal;
            mfx_level = MS_2_MFX_VALUE(LevelsTbl, ms_level);
            if (MFX_PROFILE_UNKNOWN != mfx_level)
            {
                m_MfxParamsVideo.mfx.CodecLevel = (mfxU16)mfx_level;
                HandleNewMessage(mtRequireUpdateMaxMBperSec, this); // ignore return value
                hr = S_OK;
            }
        }
        MFX_LTRACE_2(MF_TL_GENERAL, "CODECAPI_AVEncMPVLevel: ",
            "ms_level = %d, mfx_level = %d",
            ms_level, mfx_level);
    }
    else
    {
        MFX_LTRACE_GUID(MF_TL_GENERAL, *Api);
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFEncoderParams::RegisterForEvent(const GUID* /*Api*/, LONG_PTR /*userData*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}

/*------------------------------------------------------------------------------*/

HRESULT MFEncoderParams::UnregisterForEvent(const GUID* /*Api*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}

/*------------------------------------------------------------------------------*/

HRESULT MFEncoderParams::SetAllDefaults(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}

/*------------------------------------------------------------------------------*/

HRESULT MFEncoderParams::SetValueWithNotify(const GUID* /*Api*/,
                                            VARIANT* /*Value*/,
                                            GUID** /*ChangedParam*/,
                                            ULONG* /*ChangedParamCount*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}

/*------------------------------------------------------------------------------*/

HRESULT MFEncoderParams::SetAllDefaultsWithNotify(GUID** /*ChangedParam*/,
                                                  ULONG* /*ChangedParamCount*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}

/*------------------------------------------------------------------------------*/

HRESULT MFEncoderParams::GetAllSettings(IStream* /*pStream*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}

/*------------------------------------------------------------------------------*/

HRESULT MFEncoderParams::SetAllSettings(IStream* /*pStream*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}

/*------------------------------------------------------------------------------*/

HRESULT MFEncoderParams::SetAllSettingsWithNotify(IStream* /*pStream*/,
                                                  GUID** /*ChangedParam*/,
                                                  ULONG* /*ChangedParamCount*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}

/*------------------------------------------------------------------------------*/
// IConfigureMfxCodec methods

#define SET_PARAM_MFX(NAME) \
    if (pattern->mfx.NAME) info_mfx->NAME = params->mfx.NAME;

HRESULT MFEncoderParams::SetParams(mfxVideoParam *pattern, mfxVideoParam *params)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);

    CHECK_POINTER(params, E_POINTER);
    CHECK_POINTER(pattern, E_POINTER);

    mfxInfoMFX *info_mfx = &(m_MfxParamsVideo.mfx);
    //TODO: AsyncDepth, Protected, IOPattern, ExtParam, NumExtParam
    SET_PARAM_MFX(FrameInfo.FourCC);
    SET_PARAM_MFX(FrameInfo.Width);
    SET_PARAM_MFX(FrameInfo.Height);
    SET_PARAM_MFX(FrameInfo.CropX);
    SET_PARAM_MFX(FrameInfo.CropY);
    SET_PARAM_MFX(FrameInfo.CropW);
    SET_PARAM_MFX(FrameInfo.CropH);
    SET_PARAM_MFX(FrameInfo.FrameRateExtN);
    SET_PARAM_MFX(FrameInfo.FrameRateExtD);
    SET_PARAM_MFX(FrameInfo.AspectRatioW);
    SET_PARAM_MFX(FrameInfo.AspectRatioH);
    SET_PARAM_MFX(FrameInfo.PicStruct);
    SET_PARAM_MFX(FrameInfo.ChromaFormat);
    SET_PARAM_MFX(CodecId);
    SET_PARAM_MFX(CodecProfile);
    SET_PARAM_MFX(CodecLevel);
    SET_PARAM_MFX(NumThread);
    SET_PARAM_MFX(TargetUsage);
    SET_PARAM_MFX(GopPicSize);
    SET_PARAM_MFX(GopRefDist);
    SET_PARAM_MFX(GopOptFlag);
    SET_PARAM_MFX(IdrInterval);
    SET_PARAM_MFX(RateControlMethod);
    SET_PARAM_MFX(InitialDelayInKB);
    SET_PARAM_MFX(BufferSizeInKB);
    SET_PARAM_MFX(TargetKbps);
    SET_PARAM_MFX(MaxKbps);
    SET_PARAM_MFX(NumSlice);
    SET_PARAM_MFX(NumRefFrame);
    SET_PARAM_MFX(EncodedOrder);
    return S_OK;
}

/*------------------------------------------------------------------------------*/

void MFEncoderParams::PrintInfo(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);

    MFConfigureMfxCodec::PrintInfo();
    if (m_pInfoFile)
    {
        void* id = GetID();
        LPWSTR plugin = GetCodecName();
        mfxInfoMFX *params = &(m_MfxParamsVideo.mfx);
        //TODO: AsyncDepth, Protected, IOPattern, ExtParam, NumExtParam

        if (!plugin) plugin = L"Unknown encoder plug-in";

        fwprintf(m_pInfoFile, L"\nplugin = %s: id = %p: MFX ENCODER INFO\n", plugin, id);
        fflush(m_pInfoFile);
        PRINT_PARAM(TargetUsage);
        PRINT_PARAM(GopPicSize);
        PRINT_PARAM(GopRefDist);
        PRINT_PARAM(GopOptFlag);
        PRINT_PARAM(IdrInterval);
        PRINT_PARAM(RateControlMethod);
        PRINT_PARAM(InitialDelayInKB);
        PRINT_PARAM(BufferSizeInKB);
        PRINT_PARAM(TargetKbps);
        PRINT_PARAM(MaxKbps);
        PRINT_PARAM(NumSlice);
        PRINT_PARAM(NumRefFrame);
        PRINT_PARAM(EncodedOrder);
        fwprintf(m_pInfoFile, L"\n", plugin, id); fflush(m_pInfoFile);
    }
}

/*------------------------------------------------------------------------------*/

HRESULT MFEncoderParams::SetType(IMFMediaType* pType, bool bIn)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    UNREFERENCED_PARAMETER(bIn);
    CHECK_POINTER(pType, E_POINTER);
    HRESULT hr = S_OK;
    UINT32 nominalRange = 0;
    if (SUCCEEDED(pType->GetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, &nominalRange)))
    {
        hr = m_pNominalRange->Set(nominalRange);
    }
    return hr;
}

/*------------------------------------------------------------------------------*/
// MFDecoderParams class

MFDecoderParams::MFDecoderParams(void)
{
}

/*------------------------------------------------------------------------------*/
#if MFX_D3D11_SUPPORT
// IMFQualityAdvise methods

HRESULT MFDecoderParams::DropTime(LONGLONG /* hnsAmountToDrop */)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}

HRESULT MFDecoderParams::GetDropMode(MF_QUALITY_DROP_MODE* /*peDropMode*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}

HRESULT MFDecoderParams::GetQualityLevel(MF_QUALITY_LEVEL* /*peQualityLevel*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}

HRESULT MFDecoderParams::SetDropMode(MF_QUALITY_DROP_MODE /*eDropMode*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}

HRESULT MFDecoderParams::SetQualityLevel(MF_QUALITY_LEVEL /*eQualityLevel*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}

/*------------------------------------------------------------------------------*/
// IMFRealTimeClientEx methods

HRESULT MFDecoderParams::RegisterThreadsEx(DWORD* /*pdwTaskIndex*/, LPCWSTR /*wszClassName*/, LONG /*lBasePriority*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}

HRESULT MFDecoderParams::SetWorkQueueEx(DWORD /*dwMultithreadedWorkQueueId*/, LONG /*lWorkItemBasePriority*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}

HRESULT MFDecoderParams::UnregisterThreads()
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    return E_NOTIMPL;
}
#endif
/*------------------------------------------------------------------------------*/
// IConfigureCodec methods

HRESULT MFDecoderParams::SetParams(mfxVideoParam *pattern, mfxVideoParam *params)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);

    CHECK_POINTER(params, E_POINTER);
    CHECK_POINTER(pattern, E_POINTER);

    mfxInfoMFX *info_mfx = &(m_MfxParamsVideo.mfx);
    //TODO: AsyncDepth, Protected, IOPattern, ExtParam, NumExtParam
    SET_PARAM_MFX(FrameInfo.FourCC);
    SET_PARAM_MFX(FrameInfo.Width);
    SET_PARAM_MFX(FrameInfo.Height);
    SET_PARAM_MFX(FrameInfo.CropX);
    SET_PARAM_MFX(FrameInfo.CropY);
    SET_PARAM_MFX(FrameInfo.CropW);
    SET_PARAM_MFX(FrameInfo.CropH);
    SET_PARAM_MFX(FrameInfo.FrameRateExtN);
    SET_PARAM_MFX(FrameInfo.FrameRateExtD);
    SET_PARAM_MFX(FrameInfo.AspectRatioW);
    SET_PARAM_MFX(FrameInfo.AspectRatioH);
    SET_PARAM_MFX(FrameInfo.PicStruct);
    SET_PARAM_MFX(FrameInfo.ChromaFormat);
    SET_PARAM_MFX(CodecId);
    SET_PARAM_MFX(CodecProfile);
    SET_PARAM_MFX(CodecLevel);
    SET_PARAM_MFX(NumThread);
    SET_PARAM_MFX(DecodedOrder);
    return S_OK;
}

/*------------------------------------------------------------------------------*/

void MFDecoderParams::PrintInfo(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);

    MFConfigureMfxCodec::PrintInfo();
    if (m_pInfoFile)
    {
        void* id = GetID();
        LPWSTR plugin = GetCodecName();
        mfxInfoMFX *params = &(m_MfxParamsVideo.mfx);

        if (!plugin) plugin = L"Unknown decoder plug-in";

        fwprintf(m_pInfoFile, L"\nplugin = %s: id = %p: MFX DECODER INFO\n",
            plugin, id); fflush(m_pInfoFile);
        PRINT_PARAM(DecodedOrder);
        fwprintf(m_pInfoFile, L"\n", plugin, id); fflush(m_pInfoFile);
    }
}

/*------------------------------------------------------------------------------*/
// MFVppParams class

MFVppParams::MFVppParams(void)
{
    m_bVpp = true;
}

/*------------------------------------------------------------------------------*/
// IConfigureCodec methods

#define SET_PARAM_VPP(NAME) \
    if (pattern->vpp.NAME) info_vpp->NAME = params->vpp.NAME;

HRESULT MFVppParams::SetParams(mfxVideoParam *pattern, mfxVideoParam *params)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);

    CHECK_POINTER(params, E_POINTER);
    CHECK_POINTER(pattern, E_POINTER);

    mfxInfoVPP *info_vpp = &(m_MfxParamsVideo.vpp);
    // setting input parameters
    SET_PARAM_VPP(In.FourCC);
    SET_PARAM_VPP(In.Width);
    SET_PARAM_VPP(In.Height);
    SET_PARAM_VPP(In.CropX);
    SET_PARAM_VPP(In.CropY);
    SET_PARAM_VPP(In.CropW);
    SET_PARAM_VPP(In.CropH);
    SET_PARAM_VPP(In.FrameRateExtN);
    SET_PARAM_VPP(In.FrameRateExtD);
    SET_PARAM_VPP(In.AspectRatioW);
    SET_PARAM_VPP(In.AspectRatioH);
    SET_PARAM_VPP(In.PicStruct);
    SET_PARAM_VPP(In.ChromaFormat);
    // setting output parameters
    SET_PARAM_VPP(Out.FourCC);
    SET_PARAM_VPP(Out.Width);
    SET_PARAM_VPP(Out.Height);
    SET_PARAM_VPP(Out.CropX);
    SET_PARAM_VPP(Out.CropY);
    SET_PARAM_VPP(Out.CropW);
    SET_PARAM_VPP(Out.CropH);
    SET_PARAM_VPP(Out.FrameRateExtN);
    SET_PARAM_VPP(Out.FrameRateExtD);
    SET_PARAM_VPP(Out.AspectRatioW);
    SET_PARAM_VPP(Out.AspectRatioH);
    SET_PARAM_VPP(Out.PicStruct);
    SET_PARAM_VPP(Out.ChromaFormat);
    return S_OK;
}

/*------------------------------------------------------------------------------*/

void MFVppParams::PrintInfo(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);

    MFConfigureMfxCodec::PrintInfo();
    if (m_pInfoFile)
    {
        void* id = GetID();
        LPWSTR plugin = GetCodecName();
        mfxInfoVPP *params = &(m_MfxParamsVideo.vpp);

        if (!plugin) plugin = L"Unknown VPP plug-in";

        fwprintf(m_pInfoFile, L"\nplugin = %s: id = %p: MFX VPP INFO\n",
            plugin, id); fflush(m_pInfoFile);
        // setting input parameters
        PRINT_PARAM(In.FourCC);
        PRINT_PARAM(In.Width);
        PRINT_PARAM(In.Height);
        PRINT_PARAM(In.CropX);
        PRINT_PARAM(In.CropY);
        PRINT_PARAM(In.CropW);
        PRINT_PARAM(In.CropH);
        PRINT_PARAM(In.FrameRateExtN);
        PRINT_PARAM(In.FrameRateExtD);
        PRINT_PARAM(In.AspectRatioW);
        PRINT_PARAM(In.AspectRatioH);
        PRINT_PARAM(In.PicStruct);
        PRINT_PARAM(In.ChromaFormat);
        // setting output parameters
        PRINT_PARAM(Out.FourCC);
        PRINT_PARAM(Out.Width);
        PRINT_PARAM(Out.Height);
        PRINT_PARAM(Out.CropX);
        PRINT_PARAM(Out.CropY);
        PRINT_PARAM(Out.CropW);
        PRINT_PARAM(Out.CropH);
        PRINT_PARAM(Out.FrameRateExtN);
        PRINT_PARAM(Out.FrameRateExtD);
        PRINT_PARAM(Out.AspectRatioW);
        PRINT_PARAM(Out.AspectRatioH);
        PRINT_PARAM(Out.PicStruct);
        PRINT_PARAM(Out.ChromaFormat);
        fwprintf(m_pInfoFile, L"\n", plugin, id); fflush(m_pInfoFile);
    }
}
