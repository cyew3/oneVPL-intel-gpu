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
/*********************************************************************************

File: mf_vpp_plg.cpp

Purpose: define common code for MSDK based VPP MFTs.

*********************************************************************************/
#if !MFX_D3D11_SUPPORT

#include "mf_vpp_plg.h"

/*------------------------------------------------------------------------------*/

#undef  MFX_TRACE_CATEGORY
#define MFX_TRACE_CATEGORY _T("mfx_mft_unk")

#define OMIT_VPP_ASYNC_SYNC_POINT

/*------------------------------------------------------------------------------*/

HRESULT CreateVppPlugin(REFIID iid,
                        void** ppMFT,
                        ClassRegData* pRegistrationData)
{
    return MFPluginVpp::CreateInstance(iid, ppMFT, pRegistrationData);
}

/*------------------------------------------------------------------------------*/
// MFPluginVpp class

#undef  MFX_TRACE_CATEGORY
#ifdef MF_SHORT_CAT_NAMES
    #define MFX_TRACE_CATEGORY _T("mfx_mft_vpp")
#else
    #define MFX_TRACE_CATEGORY m_Reg.pPluginName
#endif

/*------------------------------------------------------------------------------*/

MFPluginVpp::MFPluginVpp(HRESULT &hr,
                         ClassRegData *pRegistrationData) :
    MFPlugin(hr, pRegistrationData),
    m_nRefCount(0),
    m_MSDK_impl(MF_MFX_IMPL),
    m_pMfxVideoSession(NULL),
    m_pInternalMfxVideoSession(NULL),
    m_pmfxVPP(NULL),
    m_pOutputTypeCandidate(NULL),
    m_Framerate(0),
    m_pInputInfo(NULL),
    m_pOutputInfo(NULL),
    m_LastSts(MFX_ERR_NONE),
    m_SyncOpSts(MFX_ERR_NONE),
    m_bInitialized(false),
    m_bDirectConnectionMFX(false),
    m_bChangeOutputType(false),
    m_bStartDrain(false),
    m_bSendDrainComplete(false),
    m_bNeedInSurface(false),
    m_bNeedOutSurface(false),
    m_bEndOfInput(false),
    m_bReinit(false),
    m_bReinitStarted(false),
    m_bSendFakeSrf(false),
    m_bSetDiscontinuityAttribute(false),
    m_pFakeSample(NULL),
    m_pPostponedInput(NULL),
    m_uiInSurfacesMemType(MFX_IOPATTERN_IN_SYSTEM_MEMORY), // VPP in surfaces
    m_nInSurfacesNum(MF_ADDITIONAL_IN_SURF_NUM),
    m_pInSurfaces(NULL),
    m_pInSurface(NULL),
    m_uiOutSurfacesMemType(MFX_IOPATTERN_OUT_SYSTEM_MEMORY), // VPP out surfaces
    m_nOutSurfacesNum(MF_ADDITIONAL_OUT_SURF_NUM),
    m_pWorkSurfaces(NULL),
    m_pWorkSurface(NULL),
    m_pOutSurfaces(NULL),
    m_pOutSurface(NULL),
    m_pDispSurface(NULL),
    m_pDeviceDXVA(NULL),
    m_pFreeSamplesPool(NULL),
    m_uiHasOutputEventExists(0),
    m_uiDelayedOutputsNum(0),
    m_dbg_vppin(NULL),
    m_dbg_vppout(NULL)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);

    if (FAILED(hr)) return;
    SAFE_NEW(m_pWorkTicker, MFTicker(&m_ticks_WorkTime));
    SAFE_NEW(m_pCpuUsager, MFCpuUsager(&m_TimeTotal, &m_TimeKernel, &m_TimeUser, &m_CpuUsage));

    if (!m_Reg.pFillParams) hr = E_POINTER;
    if (SUCCEEDED(hr))
    { // setting default vpp parameters
        mfxStatus sts = MFX_ERR_NONE;

        sts = m_Reg.pFillParams(&m_MfxParamsVideo);
        // zeroing of original parameters (which were set by MF) structure
        memset(&m_MfxParamsVideo_original, 0, sizeof(mfxVideoParam));
        memset((void*)&m_VppInAllocResponse, 0, sizeof(mfxFrameAllocResponse));
        memset((void*)&m_VppOutAllocResponse, 0, sizeof(mfxFrameAllocResponse));

        if (MFX_ERR_NONE != sts) hr = E_FAIL;
    }
#if defined(_DEBUG) || (DBG_YES == USE_DBG_TOOLS)
    if (SUCCEEDED(hr))
    { // reading registry for some parameters
        TCHAR file_name[MAX_PATH] = {0};
        DWORD var = 1;

        if (S_OK == mf_get_reg_string(_T(REG_PARAMS_PATH),
                                      _T(REG_VPP_IN_FILE), file_name, MAX_PATH))
        {
            m_dbg_vppin = mywfopen(file_name, L"wb");
        }
        if (S_OK == mf_get_reg_string(_T(REG_PARAMS_PATH),
                                      _T(REG_VPP_OUT_FILE), file_name, MAX_PATH))
        {
            m_dbg_vppout = mywfopen(file_name, L"wb");
        }
        if (S_OK == mf_get_reg_dword(_T(REG_PARAMS_PATH),
                                     _T(REG_VPP_MSDK), &var))
        {
            if (1 == var) m_dbg_MSDK = dbgSwMsdk;
            else if (2 == var) m_dbg_MSDK = dbgHwMsdk;
        }
        if (S_OK == mf_get_reg_dword(_T(REG_PARAMS_PATH),
                                     _T(REG_VPP_MEMORY), &var))
        {
            if (1 == var) m_dbg_Memory = dbgSwMemory;
            else if (2 == var) m_dbg_Memory = dbgHwMemory;
        }
        if (S_OK == mf_get_reg_dword(_T(REG_PARAMS_PATH),
                                      _T(REG_VPP_NO_SW_FB), &var))
        {
            m_dbg_no_SW_fallback = (var)? true: false;
        }
        MFX_LTRACE_I(MF_TL_PERF, m_dbg_MSDK);
        MFX_LTRACE_I(MF_TL_PERF, m_dbg_Memory);
        MFX_LTRACE_I(MF_TL_PERF, m_dbg_no_SW_fallback);
    }
#endif // #if defined(_DEBUG) || (DBG_YES == USE_DBG_TOOLS)
    if (SUCCEEDED(hr))
    {
        hr = MFCreateMfxVideoSession(&m_pMfxVideoSession);
    }
    if (SUCCEEDED(hr))
    { // internal media session initialization
        mfxStatus sts = MFX_ERR_NONE;

        // adjustment of MSDK implementation
        if (dbgHwMsdk == m_dbg_MSDK) m_MSDK_impl = MFX_IMPL_HARDWARE;
        else if (dbgSwMsdk == m_dbg_MSDK) m_MSDK_impl = MFX_IMPL_SOFTWARE;

        if (m_pD3DMutex) m_pD3DMutex->Lock();
        sts = m_pMfxVideoSession->Init(m_MSDK_impl, &g_MfxVersion);
        if (m_pD3DMutex) m_pD3DMutex->Unlock();

        if (MFX_ERR_NONE == sts)
        {
            sts = m_pMfxVideoSession->QueryIMPL(&m_MSDK_impl);
            m_MfxCodecInfo.m_MSDKImpl = m_MSDK_impl;
        }
        if (MFX_ERR_NONE == sts)
        {
            MFXVideoSession* session = (MFXVideoSession*)m_pMfxVideoSession;

            SAFE_NEW(m_pmfxVPP, MFXVideoVPPEx(*session));
            if (!m_pmfxVPP) hr = MFX_ERR_MEMORY_ALLOC;
        }
        if (MFX_ERR_NONE != sts) hr = E_FAIL;
        MFX_LTRACE_I(MF_TL_PERF, m_MSDK_impl);
    }
    if (SUCCEEDED(hr))
    {
        // initializing of the DXVA support
#if MFX_D3D11_SUPPORT
        hr = DXVASupportInit();
#else
        hr = DXVASupportInit(GetAdapterNum(&m_MSDK_impl));
#endif //MFX_D3D11_SUPPORT

    }
    GetPerformance();
    MFX_LTRACE_WS(MF_TL_PERF, (m_Reg.pPluginName)? m_Reg.pPluginName: L"null_plg_name");
    MFX_LTRACE_D(MF_TL_PERF, hr);
}

/*------------------------------------------------------------------------------*/

MFPluginVpp::~MFPluginVpp(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);

    MFX_LTRACE_WS(MF_TL_PERF, (m_Reg.pPluginName)? m_Reg.pPluginName: L"null_plg_name");

    lock.Unlock();
    AsyncThreadWait();
    lock.Lock();

    SAFE_DELETE(m_pWorkTicker);
    SAFE_DELETE(m_pCpuUsager);
    UpdateTimes();
    GetPerformance();
    // print work info
    PrintInfo();
    // destructing objects
    SAFE_RELEASE(m_pInputType);
    ReleaseMediaType(m_pOutputTypeCandidate);
    ReleaseMediaType(m_pOutputType);
    // closing codec
    CloseCodec(lock);
    SAFE_DELETE(m_pmfxVPP);
    // internal session deinitialization
    SAFE_RELEASE(m_pMfxVideoSession);
    SAFE_RELEASE(m_pInternalMfxVideoSession);
    // deinitalization of the DXVA support
    DXVASupportClose();

    if (m_Reg.pFreeParams) m_Reg.pFreeParams(&m_MfxParamsVideo);

    if (m_dbg_vppin) fclose(m_dbg_vppin);
    if (m_dbg_vppout) fclose(m_dbg_vppout);
}

/*------------------------------------------------------------------------------*/
// IUnknown methods

ULONG MFPluginVpp::AddRef(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MFX_LTRACE_I(MF_TL_GENERAL, m_nRefCount+1);
    return InterlockedIncrement(&m_nRefCount);
}

ULONG MFPluginVpp::Release(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    ULONG uCount = InterlockedDecrement(&m_nRefCount);
    MFX_LTRACE_I(MF_TL_GENERAL, uCount);
    if (uCount == 0) delete this;
    // For thread safety, return a temporary variable.
    return uCount;
}

HRESULT MFPluginVpp::QueryInterface(REFIID iid, void** ppv)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    bool bAggregation = false;

    if (!ppv) return E_POINTER;
    if ((iid == IID_IUnknown) || (iid == IID_IMFTransform))
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "IUnknown or IMFTransform");
        *ppv = static_cast<IMFTransform*>(this);
    }
    else if (iid == IID_IMFMediaEventGenerator)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "IMFMediaEventGenerator");
        *ppv = static_cast<IMFMediaEventGenerator*>(this);
    }
    else if (iid == IID_IMFShutdown)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "IMFShutdown");
        *ppv = static_cast<IMFShutdown*>(this);
    }
    else if (iid == IID_IConfigureMfxVpp)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "IConfigureMfxVpp");
        *ppv = static_cast<IConfigureMfxCodec*>(this);
    }
    else if (iid == IID_IMFDeviceDXVA)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "IMFDeviceDXVA");
        *ppv = static_cast<IMFDeviceDXVA*>(this);
    }
    else if ((iid == IID_IPropertyStore) && m_pPropertyStore)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "IPropertyStore");
        m_pPropertyStore->AddRef();
        *ppv = m_pPropertyStore;
        bAggregation = true;
    }
    else
    {
        MFX_LTRACE_GUID(MF_TL_GENERAL, iid);
        return E_NOINTERFACE;
    }
    if (!bAggregation) AddRef();

    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/
// IMFTransform methods

HRESULT MFPluginVpp::GetInputAvailableType(DWORD dwInputStreamID,
                                            DWORD dwTypeIndex,
                                            IMFMediaType** ppType)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);
    HRESULT       hr    = S_OK;
    IMFMediaType* pType = NULL;

    MFX_LTRACE_I(MF_TL_GENERAL, dwTypeIndex);

    GetPerformance();
    hr = MFPlugin::GetInputAvailableType(dwInputStreamID, dwTypeIndex, &pType);
    if (SUCCEEDED(hr))
    {
        if (m_pInputType)
        { // if input type was set we may offer only this type
            hr = m_pInputType->CopyAllItems(pType);
        }
        else
        {
            // setting hint for acccepted type
            if (SUCCEEDED(hr)) hr = pType->SetGUID(MF_MT_MAJOR_TYPE, m_Reg.pInputTypes[dwTypeIndex].major_type);
            if (SUCCEEDED(hr)) hr = pType->SetGUID(MF_MT_SUBTYPE,    m_Reg.pInputTypes[dwTypeIndex].subtype);
        }
    }
    if (SUCCEEDED(hr))
    {
        pType->AddRef();
        *ppType = pType;
    }
    SAFE_RELEASE(pType);

    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVpp::GetOutputAvailableType(DWORD dwOutputStreamID,
                                             DWORD dwTypeIndex,
                                             IMFMediaType** ppType)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);
    HRESULT       hr    = S_OK;
    IMFMediaType* pType = NULL;

    MFX_LTRACE_I(MF_TL_GENERAL, dwTypeIndex);

    GetPerformance();
    hr = MFPlugin::GetOutputAvailableType(dwOutputStreamID, dwTypeIndex ? (dwTypeIndex - 1) : dwTypeIndex, &pType);
    if (SUCCEEDED(hr))
    {
        if (m_bChangeOutputType && m_pOutputType)
        {
            if (!m_pOutputTypeCandidate)
            { // creating candidate type
                UINT32 w = 0, h = 0;
                MFVideoArea area;

                hr = MFCreateMediaType(&m_pOutputTypeCandidate);
                if (SUCCEEDED(hr))
                {
                    w = m_MfxParamsVideo.vpp.Out.Width;
                    h = m_MfxParamsVideo.vpp.Out.Height;
                    memset(&area, 0, sizeof(MFVideoArea));
                    area.OffsetX.value = m_MfxParamsVideo.vpp.Out.CropX;
                    area.OffsetY.value = m_MfxParamsVideo.vpp.Out.CropY;
                    area.Area.cx = m_MfxParamsVideo.vpp.Out.CropW;
                    area.Area.cy = m_MfxParamsVideo.vpp.Out.CropH;

                    MFX_LTRACE_I(MF_TL_GENERAL, m_MfxParamsVideo.vpp.Out.CropX);
                    MFX_LTRACE_I(MF_TL_GENERAL, m_MfxParamsVideo.vpp.Out.CropY);
                    MFX_LTRACE_I(MF_TL_GENERAL, m_MfxParamsVideo.vpp.Out.CropW);
                    MFX_LTRACE_I(MF_TL_GENERAL, m_MfxParamsVideo.vpp.Out.CropH);
                }
                if (SUCCEEDED(hr)) hr = m_pOutputType->CopyAllItems(m_pOutputTypeCandidate);
                // correcting some parameters
                if (SUCCEEDED(hr)) hr = MFSetAttributeSize(m_pOutputTypeCandidate, MF_MT_FRAME_SIZE, w, h);
                if (SUCCEEDED(hr)) hr = m_pOutputTypeCandidate->SetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)&area, sizeof(MFVideoArea));
                if (SUCCEEDED(hr)) hr = m_pOutputTypeCandidate->SetBlob(MF_MT_GEOMETRIC_APERTURE, (UINT8*)&area, sizeof(MFVideoArea));
            }
            if (SUCCEEDED(hr))
            {
                m_pOutputTypeCandidate->AddRef();
                pType = m_pOutputTypeCandidate;
            }
        }
        else if (m_pOutputType)
        { // if output type was set we may offer only this type
            hr = m_pOutputType->CopyAllItems(pType);
        }
        else if (m_pOutputTypeCandidate)
        { // offering type candidate constructed from regected output type
            hr = m_pOutputTypeCandidate->CopyAllItems(pType);
        }
        else
        {
            // setting hint for acccepted type
            if (SUCCEEDED(hr)) hr = pType->SetGUID(MF_MT_MAJOR_TYPE, m_Reg.pOutputTypes[dwTypeIndex].major_type);
            if (SUCCEEDED(hr)) hr = pType->SetGUID(MF_MT_SUBTYPE,    m_Reg.pOutputTypes[dwTypeIndex].subtype);
            if (SUCCEEDED(hr)) hr = pType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

            if (SUCCEEDED(hr) && m_pInputType && (0 == dwTypeIndex))
            {
                UINT32 par1 = 0, par2 = 0;

                if (SUCCEEDED(MFGetAttributeSize(m_pInputType, MF_MT_FRAME_SIZE, &par1, &par2)))
                    hr = MFSetAttributeSize(pType, MF_MT_FRAME_SIZE, par1, par2);
            }
        }
    }
    if (SUCCEEDED(hr))
    {
        pType->AddRef();
        *ppType = pType;
    }
    SAFE_RELEASE(pType);
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVpp::CheckInputMediaType(IMFMediaType* pType)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
#ifdef MF_ENABLE_PICTURE_COMPLEXITY_LIMITS
    GUID dec_subtype = GUID_NULL;

    if (SUCCEEDED(pType->GetGUID(MF_MT_DEC_SUBTYPE, &dec_subtype)))
    {
        if ((MEDIASUBTYPE_H264 == dec_subtype) ||
            (MEDIASUBTYPE_H264_MFX == dec_subtype) ||
            (MEDIASUBTYPE_AVC1 == dec_subtype))
        {
            UINT32 width = 0, height = 0;

            if (SUCCEEDED(MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height)) &&
                ((width * height) >= MF_VPP_H264_PICTURE_COMPLEXITY_LIMIT))
            {
                hr = MF_E_INVALIDTYPE;
            }
        }
    }
#else
    pType;
#endif
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

// Checks if data provided in pType could be decoded
bool MFPluginVpp::CheckHwSupport(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    bool ret_val = MFPlugin::CheckHwSupport();

    if (ret_val)
    {
        mfxStatus sts = MFX_ERR_NONE, res = MFX_ERR_NONE;
        mfxVideoParam inVideoParams, outVideoParams;

        memcpy_s(&inVideoParams, sizeof(inVideoParams), &m_MfxParamsVideo, sizeof(m_MfxParamsVideo));
        memset(&outVideoParams, 0, sizeof(mfxVideoParam));
        sts = mf_copy_extparam(&inVideoParams, &outVideoParams);
        if (MFX_ERR_NONE == sts)
        {
            // set any color format to make suitable values for Query, suppose it is NV12;
            // actual color format is obtained on output type setting
            if (!(inVideoParams.vpp.In.FourCC) || !(inVideoParams.vpp.In.ChromaFormat))
            {
                inVideoParams.vpp.In.FourCC        = MAKEFOURCC('N','V','1','2');
                inVideoParams.vpp.In.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
            }
            if (!(inVideoParams.vpp.Out.FourCC) || !(inVideoParams.vpp.Out.ChromaFormat))
            {
                inVideoParams.vpp.Out.FourCC       = MAKEFOURCC('N','V','1','2');
                inVideoParams.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            }
            inVideoParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

            sts = m_pmfxVPP->Query(&inVideoParams, &outVideoParams);
            if ((MFX_ERR_NONE != sts) && (m_dbg_no_SW_fallback || (MFX_WRN_PARTIAL_ACCELERATION != sts)))
                ret_val = false;
        }
        res = mf_free_extparam(&outVideoParams);
        if ((MFX_ERR_NONE == sts) && (MFX_ERR_NONE != res)) sts = res;
    }
    MFX_LTRACE_I(MF_TL_GENERAL, ret_val);
    return ret_val;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVpp::SetInputType(DWORD         dwInputStreamID,
                                  IMFMediaType* pType,
                                  DWORD         dwFlags)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);
    HRESULT hr  = S_OK;

    MFX_LTRACE_P(MF_TL_PERF, pType);

    GetPerformance();
    hr = MFPlugin::SetInputType(dwInputStreamID, pType, dwFlags);
    CHECK_EXPRESSION(SUCCEEDED(hr), hr);
    // checking errors
    CHECK_POINTER(m_pmfxVPP, E_POINTER);

    if (!pType)
    { // resetting media type
        ReleaseMediaType(m_pInputType);
    }
    CHECK_POINTER(pType, S_OK);
    // Validate the type: general checking, input type checking
    hr = CheckMediaType(pType, m_Reg.pInputTypes, m_Reg.cInputTypes, &m_pInputInfo);
    if (0 == (dwFlags & MFT_SET_TYPE_TEST_ONLY))
    {
        if (SUCCEEDED(hr)) hr = CheckInputMediaType(pType);
        if (SUCCEEDED(hr))
        {
            // set major type; check major type is the same as was on input
            if (GUID_NULL == m_MajorType)
                m_MajorType = m_pInputInfo->major_type;
            else if (m_pInputInfo->major_type != m_MajorType)
                hr = MF_E_INVALIDTYPE;
        }
        if (SUCCEEDED(hr))
        {
            IUnknown *pUnk = static_cast<IMFTransform*>(this);

            ReleaseMediaType(m_pInputType);
            pType->AddRef();
            m_pInputType = pType;

            if (!m_bInitialized) hr = m_pInputType->SetUnknown(MF_MT_D3D_DEVICE, pUnk);
        }
        if (SUCCEEDED(hr)) hr = mf_mftype2mfx_frame_info(m_pInputType, &(m_MfxParamsVideo_original.vpp.In));
        if (SUCCEEDED(hr))
        {
            memcpy_s(&(m_MfxParamsVideo.vpp.In), sizeof(m_MfxParamsVideo.vpp.In),
                     &(m_MfxParamsVideo_original.vpp.In), sizeof(m_MfxParamsVideo_original.vpp.In));
            hr = mf_align_geometry(&(m_MfxParamsVideo.vpp.In));
        }
        /* We will not check whether HW supports VPP operations in the following cases:
         *  - if output type still is not set (just can't check)
         *  - if "resolution change" occurs (we can encounter error and better to found it on
         * actual reinitialization; otherwise we can hang)
         */
        if (SUCCEEDED(hr) && !m_bInitialized && m_pOutputType && !CheckHwSupport()) hr = E_FAIL;
        if (FAILED(hr))
        {
            ReleaseMediaType(m_pInputType);
        }
        else if (m_bInitialized)
        {
            if (SUCCEEDED(m_hrError))
            {
                // that's "reolution change"; need to reinit VPP
                m_bReinit     = true;
                m_bEndOfInput = true; // we need to obtain buffered frames
                if (m_pDeviceDXVA) m_bSendFakeSrf = true; // we need to send fake frame
                else m_bSendFakeSrf = false;
                // forcing output sample request if needed
                hr = FlushDelayedOutputs();
                // pushing async thread (starting call RunFrameVPPAsync with NULL bitstream)
                AsyncThreadPush();
            }
        }
    }
    MFX_LTRACE_P(MF_TL_PERF, m_pInputType);
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVpp::SetOutputType(DWORD         dwOutputStreamID,
                                   IMFMediaType* pType,
                                   DWORD         dwFlags)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = S_OK;

    MFX_LTRACE_P(MF_TL_PERF, pType);

    GetPerformance();
    hr = MFPlugin::SetOutputType(dwOutputStreamID, pType, dwFlags);
    CHECK_EXPRESSION(SUCCEEDED(hr), hr);
    // checking errors
    CHECK_POINTER(m_pmfxVPP, E_POINTER);

    if (!pType)
    { // resetting media type
        ReleaseMediaType(m_pOutputType);
        ReleaseMediaType(m_pOutputTypeCandidate);
    }
    CHECK_POINTER(pType, S_OK);
    // Validate the type: general checking, output type checking
    hr = CheckMediaType(pType, m_Reg.pOutputTypes, m_Reg.cOutputTypes, &m_pOutputInfo);
    if (0 == (dwFlags & MFT_SET_TYPE_TEST_ONLY))
    {
        if (SUCCEEDED(hr))
        {
            // set major type; check major type is the same as was on input
            if (GUID_NULL == m_MajorType)
                m_MajorType = m_pOutputInfo->major_type;
            else if (m_pOutputInfo->major_type != m_MajorType)
                hr = MF_E_INVALIDTYPE;
        }
        if (SUCCEEDED(hr))
        {
            ReleaseMediaType(m_pOutputType);
            pType->AddRef();
            m_pOutputType = pType;
        }
        if (SUCCEEDED(hr)) hr = mf_mftype2mfx_frame_info(m_pOutputType, &(m_MfxParamsVideo_original.vpp.Out));
        if (SUCCEEDED(hr))
        {
            memcpy_s(&(m_MfxParamsVideo.vpp.Out), sizeof(m_MfxParamsVideo.vpp.Out),
                     &(m_MfxParamsVideo_original.vpp.Out), sizeof(m_MfxParamsVideo_original.vpp.Out));
            m_Framerate = mf_get_framerate(m_MfxParamsVideo.vpp.Out.FrameRateExtN, m_MfxParamsVideo.vpp.Out.FrameRateExtD);

            hr = mf_align_geometry(&(m_MfxParamsVideo.vpp.Out));
        }
        if (SUCCEEDED(hr))
        {
            if (m_bChangeOutputType)
            {
                MFX_LTRACE_S(MF_TL_GENERAL, "Finishing change of output type");
                m_bChangeOutputType = false;
                ReleaseMediaType(m_pOutputTypeCandidate);
                hr = SendOutput();
            }
            else
            { // checking parameters
                if (SUCCEEDED(hr) && m_pInputType && !CheckHwSupport()) hr = E_FAIL;
            }
        }
        if (FAILED(hr))
        {
            ReleaseMediaType(m_pOutputType);
        }
    }
    MFX_LTRACE_P(MF_TL_PERF, m_pOutputType);
    MFX_LTRACE_P(MF_TL_PERF, m_pOutputTypeCandidate);
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVpp::ProcessMessage(MFT_MESSAGE_TYPE eMessage,
                                    ULONG_PTR        ulParam)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = S_OK;

    GetPerformance();
    hr = MFPlugin::ProcessMessage(eMessage, ulParam);
    CHECK_EXPRESSION(SUCCEEDED(hr), hr);
    switch (eMessage)
    {
    case MFT_MESSAGE_COMMAND_FLUSH:
        MFX_LTRACE_S(MF_TL_PERF, "MFT_MESSAGE_COMMAND_FLUSH");
        if (MFX_ERR_NONE != ResetCodec(lock)) hr = hr;//E_FAIL;
        break;

    case MFT_MESSAGE_COMMAND_DRAIN:
        MFX_LTRACE_S(MF_TL_PERF, "MFT_MESSAGE_COMMAND_DRAIN");
        if (m_pmfxVPP)
        { // processing eos only if we were initialized
            m_bEndOfInput = true;
            m_bSendDrainComplete = true;
            // we can't accept any input if draining has begun
            m_bDoNotRequestInput = true; // prevent calling RequestInput
            m_NeedInputEventInfo.m_requested = 0; // prevent processing of sent RequestInput
            // forcing output sample request if needed
            if (SUCCEEDED(hr)) hr = FlushDelayedOutputs();
            if (!m_bReinit) AsyncThreadPush();
        }
        break;

    case MFT_MESSAGE_COMMAND_MARKER:
        MFX_LTRACE_S(MF_TL_PERF, "MFT_MESSAGE_COMMAND_MARKER");
        // we always produce as much data as possible
        if (m_pmfxVPP) hr = SendMarker(ulParam);
        break;

    case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
        MFX_LTRACE_S(MF_TL_PERF, "MFT_MESSAGE_NOTIFY_BEGIN_STREAMING");
        m_bStreamingStarted = true;
        break;

    case MFT_MESSAGE_NOTIFY_END_STREAMING:
        MFX_LTRACE_S(MF_TL_PERF, "MFT_MESSAGE_NOTIFY_END_STREAMING");
        m_bStreamingStarted = false;
        break;

    case MFT_MESSAGE_NOTIFY_START_OF_STREAM:
        MFX_LTRACE_S(MF_TL_PERF, "MFT_MESSAGE_NOTIFY_START_OF_STREAM");
        if (!m_pAsyncThread)
        {
            SAFE_NEW(m_pAsyncThread, MyThread(hr, thAsyncThreadFunc, this));
            if (SUCCEEDED(hr) && !m_pAsyncThread) hr = E_FAIL;
        }
        else if (MFX_ERR_NONE != ResetCodec(lock)) hr = E_FAIL;
        m_bStreamingStarted  = true;
        m_bDoNotRequestInput = false;
        if (SUCCEEDED(hr)) hr = RequestInput();
        break;

    case MFT_MESSAGE_NOTIFY_END_OF_STREAM:
        MFX_LTRACE_S(MF_TL_PERF, "MFT_MESSAGE_NOTIFY_END_OF_STREAM");
        // stream ends, we must not accept input data
        m_bDoNotRequestInput = true; // prevent calling RequestInput
        m_NeedInputEventInfo.m_requested = 0; // prevent processing of sent RequestInput
        break;

    case MFT_MESSAGE_SET_D3D_MANAGER:
        MFX_LTRACE_S(MF_TL_PERF, "MFT_MESSAGE_SET_D3D_MANAGER");
        if (ulParam) hr = E_NOTIMPL; // this return status is an exception (see MS docs)
        break;
    }
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/
// Initialize Frame Allocator

mfxStatus MFPluginVpp::InitFRA(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;

    if (!m_pDeviceDXVA && m_pDeviceManager)
    {
        if (IS_MEM_VIDEO(m_uiInSurfacesMemType))
        {
            MFX_LTRACE_S(MF_TL_GENERAL, "Creating frame allocator");
            if (!m_pFrameAllocator)
            {
                if (MFX_ERR_NONE == sts)
                {
                    sts = m_pMfxVideoSession->SetHandle(MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9, m_pDeviceManager);
                }
                if (MFX_ERR_NONE == sts)
                {
                    SAFE_NEW(m_pFrameAllocator, MFFrameAllocator(new MFAllocatorHelper<D3DFrameAllocator>));
                    if (!m_pFrameAllocator) sts = MFX_ERR_MEMORY_ALLOC;
                    else m_pFrameAllocator->AddRef();
                }
                if (MFX_ERR_NONE == sts)
                {
                    D3DAllocatorParams AllocParams;

                    m_pDeviceManager->AddRef();
                    AllocParams.pManager = m_pDeviceManager;
                    sts = m_pFrameAllocator->GetMFXAllocator()->Init(&AllocParams);
                    SAFE_RELEASE(AllocParams.pManager);
                }
                if (MFX_ERR_NONE == sts)
                    sts = m_pMfxVideoSession->SetFrameAllocator(m_pFrameAllocator->GetMFXAllocator());
            }
        }
        else
        {
            DXVASupportClose();
        }
        MFX_LTRACE_P(MF_TL_GENERAL, m_pFrameAllocator);
    }
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/
// Initialize input surfaces

mfxStatus MFPluginVpp::InitInSRF(mfxU32* pDecSurfacesNum)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 i = 0;

    if (MFX_ERR_NONE == sts) sts = InitFRA();
    if ((MFX_ERR_NONE == sts) &&
        m_pFrameAllocator && IS_MEM_VIDEO(m_uiInSurfacesMemType))
    {
        mfxFrameAllocRequest request;

        memset((void*)&request, 0, sizeof(mfxFrameAllocRequest));
        memset((void*)&m_VppInAllocResponse, 0, sizeof(mfxFrameAllocResponse));
        memcpy_s(&(request.Info), sizeof(request.Info), &(m_MfxParamsVideo.vpp.In), sizeof(m_MfxParamsVideo.vpp.In));
        request.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET |
                       MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_FROM_VPPIN;
        request.NumFrameMin = request.NumFrameSuggested = (mfxU16)m_nInSurfacesNum;

        sts = m_pFrameAllocator->GetMFXAllocator()->Alloc(m_pFrameAllocator->GetMFXAllocator()->pthis,
                                                          &request, &m_VppInAllocResponse);
        if (MFX_ERR_NONE == sts)
        {
            if (m_nInSurfacesNum > m_VppInAllocResponse.NumFrameActual) sts = MFX_ERR_MEMORY_ALLOC;
            else
            {
                m_nInSurfacesNum = m_VppInAllocResponse.NumFrameActual;
                if (pDecSurfacesNum) *pDecSurfacesNum = m_nInSurfacesNum;
                MFX_LTRACE_I(MF_TL_GENERAL, m_nInSurfacesNum);
            }
        }
    }
    if (MFX_ERR_NONE == sts)
    {
        SAFE_NEW_ARRAY(m_pInSurfaces, MFYuvInSurface, m_nInSurfacesNum);
        if (!m_pInSurfaces) sts = MFX_ERR_MEMORY_ALLOC;
        else
        {
            for (i = 0; (MFX_ERR_NONE == sts) && (i < m_nInSurfacesNum); ++i)
            {
                m_pInSurfaces[i].SetFile(m_dbg_vppin);
                sts = m_pInSurfaces[i].Init(&(m_MfxParamsVideo.vpp.In),
                                            (m_bDirectConnectionMFX)? NULL: &(m_MfxParamsVideo_original.vpp.In),
                                            NULL, false);
            }
        }
        if (MFX_ERR_NONE == sts) m_pInSurface = &(m_pInSurfaces[0]);
    }
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

mfxStatus MFPluginVpp::InitOutSRF(MyAutoLock& lock)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 i = 0;

    // Free samples pool allocation
    if (MFX_ERR_NONE == sts)
    {
        if (m_pFreeSamplesPool)
        {
            lock.Unlock();
            m_pFreeSamplesPool->RemoveCallback();
            lock.Lock();
            SAFE_RELEASE(m_pFreeSamplesPool);
        }
        SAFE_NEW(m_pFreeSamplesPool, MFSamplesPool);
        if (!m_pFreeSamplesPool) sts = MFX_ERR_MEMORY_ALLOC;
        else
        {
            m_pFreeSamplesPool->AddRef();
            if (FAILED(m_pFreeSamplesPool->Init(m_nOutSurfacesNum, this))) sts = MFX_ERR_UNKNOWN;
        }
    }
    if ((MFX_ERR_NONE == sts) &&
        m_pFrameAllocator && IS_MEM_VIDEO(m_uiOutSurfacesMemType))
    {
        mfxFrameAllocRequest request;

        memset((void*)&request, 0, sizeof(mfxFrameAllocRequest));
        memset((void*)&m_VppOutAllocResponse, 0, sizeof(mfxFrameAllocResponse));
        memcpy_s(&(request.Info), sizeof(request.Info), &(m_MfxParamsVideo.vpp.Out), sizeof(m_MfxParamsVideo.vpp.Out));
        request.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET |
                       MFX_MEMTYPE_FROM_VPPOUT/* | MFX_MEMTYPE_FROM_ENCODE*/;
        request.NumFrameMin = request.NumFrameSuggested = (mfxU16)m_nOutSurfacesNum;

        sts = m_pFrameAllocator->GetMFXAllocator()->Alloc(m_pFrameAllocator->GetMFXAllocator()->pthis,
                                                          &request, &m_VppOutAllocResponse);
        if ((MFX_ERR_NONE == sts) && (m_nOutSurfacesNum != m_VppOutAllocResponse.NumFrameActual))
        {
            sts = MFX_ERR_MEMORY_ALLOC;
        }
    }
    // surfaces allocation
    if (MFX_ERR_NONE == sts)
    {
        m_pWorkSurfaces = (MFYuvOutSurface**)calloc(m_nOutSurfacesNum, sizeof(MFYuvOutSurface*));
        m_pOutSurfaces  = (MFYuvOutData*)calloc(m_nOutSurfacesNum, sizeof(MFYuvOutData));
        if (!m_pWorkSurfaces || !m_pOutSurfaces) sts = MFX_ERR_MEMORY_ALLOC;
        else
        {
            for (i = 0; (MFX_ERR_NONE == sts) && (i < m_nOutSurfacesNum); ++i)
            {
                SAFE_NEW(m_pWorkSurfaces[i], MFYuvOutSurface);
                if (!m_pWorkSurfaces[i]) sts = MFX_ERR_MEMORY_ALLOC;
                if (MFX_ERR_NONE == sts) m_pWorkSurfaces[i]->SetFile(m_dbg_vppout);
                if (MFX_ERR_NONE == sts) sts = m_pWorkSurfaces[i]->Init(&(m_MfxParamsVideo.vpp.Out), m_pFreeSamplesPool);
                if (MFX_ERR_NONE == sts)
                {
                    if (m_pFrameAllocator && IS_MEM_VIDEO(m_uiOutSurfacesMemType))
                    {
                        sts = m_pWorkSurfaces[i]->Alloc(m_VppOutAllocResponse.mids[i]);
                    }
                    else sts = m_pWorkSurfaces[i]->Alloc(NULL);
                }
                if (MFX_ERR_NONE == sts)
                {
                    SAFE_NEW(m_pOutSurfaces[i].pSyncPoint, mfxSyncPoint);
                    if (!m_pOutSurfaces[i].pSyncPoint) sts = MFX_ERR_MEMORY_ALLOC;
                }
            }
        }
    }
    if (MFX_ERR_NONE == sts)
    {
        m_pWorkSurface = m_pWorkSurfaces[0];
        m_pOutSurface  = m_pOutSurfaces;
        m_pDispSurface = m_pOutSurfaces;

        m_pOutSurface->pMFSurface = m_pWorkSurface;
        m_pOutSurface->pSurface   = m_pWorkSurface->GetSurface();
    }
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/
// Initialize VPP (if needed)

mfxStatus MFPluginVpp::InitVPP(MyAutoLock& lock, mfxU32* pDecSurfacesNum)
{
    MFX_LTRACE_I(MF_TL_GENERAL, (pDecSurfacesNum)? *pDecSurfacesNum: 0);
    mfxStatus sts = MFX_ERR_NONE;

    // VPP surfaces allocation
    if (MFX_ERR_NONE == sts)
    {
        mfxFrameAllocRequest VPPRequest[2]; // [0] - input, [1] - output

        memset((void*)&VPPRequest, 0, 2*sizeof(mfxFrameAllocRequest));

        sts = m_pmfxVPP->QueryIOSurf(&m_MfxParamsVideo, VPPRequest);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts)
        {
            sts = MFX_ERR_NONE;
            if ((dbgHwMemory != m_dbg_Memory) && m_pDeviceDXVA)
            {
                // switching to SW mode
                MFX_LTRACE_S(MF_TL_GENERAL, "Switching to System Memory");
                m_uiOutSurfacesMemType = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
                m_MfxParamsVideo.IOPattern = m_uiInSurfacesMemType | m_uiOutSurfacesMemType;

                sts = m_pmfxVPP->QueryIOSurf(&m_MfxParamsVideo, VPPRequest);
                if (!m_dbg_no_SW_fallback && (MFX_WRN_PARTIAL_ACCELERATION == sts)) sts = MFX_ERR_NONE;
            }
        }
        if (MFX_ERR_NONE == sts)
        {
            mfxU32 in_frames_num  = MAX((VPPRequest[0]).NumFrameSuggested, MAX((VPPRequest[0]).NumFrameMin, 1));
            mfxU32 out_frames_num = MAX((VPPRequest[1]).NumFrameSuggested, MAX((VPPRequest[1]).NumFrameMin, 1));

            m_nInSurfacesNum  += MF_ADDITIONAL_SURF_MULTIPLIER * in_frames_num;
            m_nOutSurfacesNum += MF_ADDITIONAL_SURF_MULTIPLIER * out_frames_num;

            if (pDecSurfacesNum)
            {
                m_nInSurfacesNum += (*pDecSurfacesNum) - 1;
                *pDecSurfacesNum = m_nInSurfacesNum;
            }

            MFX_LTRACE_I(MF_TL_GENERAL, m_nInSurfacesNum);
            MFX_LTRACE_I(MF_TL_GENERAL, m_nOutSurfacesNum);
        }
    }
    if (m_pDeviceDXVA)
    {
        // if our vpp and encoder are directly connected, we will not change
        // output type
        m_bChangeOutputType = false;
        // switching off pretending
        memcpy_s(&(m_MfxParamsVideo_original.vpp.Out), sizeof(m_MfxParamsVideo_original.vpp.Out),
                 &(m_MfxParamsVideo.vpp.Out), sizeof(m_MfxParamsVideo.vpp.Out));
        // initializing encoder
        if (MFX_ERR_NONE == sts)
        {
            sts = m_pDeviceDXVA->InitPlg(m_pMfxVideoSession,
                                         &m_MfxParamsVideo,
                                         &m_nOutSurfacesNum);
        }
        if ((MFX_ERR_NONE == sts) && (MFX_IOPATTERN_OUT_VIDEO_MEMORY == m_uiOutSurfacesMemType))
        {
            if (m_pDeviceDXVA)
            {
                if (FAILED(DXVASupportInitWrapper(m_pDeviceDXVA))) sts = MFX_ERR_UNKNOWN;
            }
        }
    }
    if (MFX_ERR_NONE == sts) sts = InitInSRF(pDecSurfacesNum);
    if (MFX_ERR_NONE == sts) sts = InitOutSRF(lock);
    // VPP initialization
    if (MFX_ERR_NONE == sts)
    {
        sts = m_pmfxVPP->Init(&m_MfxParamsVideo);
        if (!m_dbg_no_SW_fallback && (MFX_WRN_PARTIAL_ACCELERATION == sts)) sts = MFX_ERR_NONE;
    }
    if (MFX_ERR_NONE == sts) sts = m_pmfxVPP->GetVideoParam(&m_MfxParamsVideo);
    if (MFX_ERR_NONE == sts)
    {
        // catching changes in output media type
        if ((m_MfxParamsVideo_original.vpp.Out.Width  != m_MfxParamsVideo.vpp.Out.Width) ||
            (m_MfxParamsVideo_original.vpp.Out.Height != m_MfxParamsVideo.vpp.Out.Height) ||
            (m_MfxParamsVideo_original.vpp.Out.CropX  != m_MfxParamsVideo.vpp.Out.CropX) ||
            (m_MfxParamsVideo_original.vpp.Out.CropY  != m_MfxParamsVideo.vpp.Out.CropY) ||
            (m_MfxParamsVideo_original.vpp.Out.CropW  != m_MfxParamsVideo.vpp.Out.CropW) ||
            (m_MfxParamsVideo_original.vpp.Out.CropH  != m_MfxParamsVideo.vpp.Out.CropH))
        {
            MFX_LTRACE_S(MF_TL_GENERAL, "Output Type is needed to be changed");
            // NOTE: MSFT encoder does not support change of input media type
#if 0
            m_bChangeOutputType = true;
#endif
        }
    }
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/
// Initialize codec

mfxStatus MFPluginVpp::InitCodec(MyAutoLock& lock,
                                 mfxU16  uiDecMemPattern,
                                 mfxU32* pDecSurfacesNum)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;

    MFX_LTRACE_D(MF_TL_GENERAL, uiDecMemPattern);
    MFX_LTRACE_D(MF_TL_GENERAL, (pDecSurfacesNum)? *pDecSurfacesNum: 0);

    // Deletioning of unneded properties:
    // MF_MT_D3D_DEVICE - it contains pointer to enc plug-in, need to delete
    // it or enc destructor will not be invoked
    m_pInputType->DeleteItem(MF_MT_D3D_DEVICE);
    // parameters correction
    if (MFX_ERR_NONE == sts)
    {
        if (!m_pDeviceDXVA)
        {
           m_pOutputType->GetUnknown(MF_MT_D3D_DEVICE, IID_IMFDeviceDXVA, (void**)&m_pDeviceDXVA);
        }
    }
    if (MFX_ERR_NONE == sts)
    {
        // by default setting SYS memory to be used on the input
        m_uiInSurfacesMemType = (mfxU16)MFX_IOPATTERN_IN_SYSTEM_MEMORY;
        // now corrrecting input memory type
        if (pDecSurfacesNum)
        { // if we are connected with decoder, use decoder's memory type
            m_uiInSurfacesMemType = (mfxU16)MEM_OUT_TO_IN(uiDecMemPattern);
            if (IS_MEM_VIDEO(m_uiInSurfacesMemType))
                m_bShareAllocator = true;
        }
        // no connection with decoder - deinitializing built in DXVA support
        if (!m_bShareAllocator) DXVASupportClose();

        // by default setting preffered memory type to be used on the output
        m_uiOutSurfacesMemType = (mfxU16)((m_pDeviceDXVA)? MFX_IOPATTERN_OUT_VIDEO_MEMORY: MFX_IOPATTERN_OUT_SYSTEM_MEMORY);
        // now corrrecting output memory type
        if (dbgHwMemory != m_dbg_Memory)
        {
            if (MFX_IMPL_SOFTWARE == m_MSDK_impl)
            { // forcing SW memory
                m_uiOutSurfacesMemType = (mfxU16)MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
            }
        }
        else
        { // we were forced to use HW memory, checking if this is possible
            if (!m_pDeviceDXVA) sts = MFX_ERR_ABORTED;
        }
        m_MfxParamsVideo.IOPattern  = m_uiInSurfacesMemType | m_uiOutSurfacesMemType;
        MFX_LTRACE_I(MF_TL_GENERAL, m_uiInSurfacesMemType);
        MFX_LTRACE_I(MF_TL_GENERAL, m_uiOutSurfacesMemType);
    }
    // Setting some VPP parameters
    if (MFX_ERR_NONE == sts)
    {
        mfxU32 colorFormat  = mf_get_color_format (m_pInputInfo->subtype.Data1);
        mfxU16 chromaFormat = mf_get_chroma_format(m_pInputInfo->subtype.Data1);

        // setting cropping parameters (letter&pillar boxing)
        mf_set_cropping(&(m_MfxParamsVideo.vpp));
        // setting color formats
        if (colorFormat)
        {
            m_MfxParamsVideo.vpp.In.FourCC  = colorFormat;
            m_MfxParamsVideo.vpp.Out.FourCC = MFX_FOURCC_NV12;

            m_MfxParamsVideo.vpp.In.ChromaFormat  = chromaFormat;
            m_MfxParamsVideo.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        }
        else sts = MFX_ERR_UNSUPPORTED;
    }
    if (MFX_ERR_NONE == sts) sts = InitVPP(lock, pDecSurfacesNum);
    m_MfxCodecInfo.m_InitStatus = sts;
    if (MFX_ERR_NONE != sts) CloseCodec(lock);
    else
    {
        m_bInitialized = true;
        m_MfxCodecInfo.m_uiInFramesType  = m_uiInSurfacesMemType;
        m_MfxCodecInfo.m_uiOutFramesType = m_uiOutSurfacesMemType;
        MFX_LTRACE_I(MF_TL_GENERAL, m_MSDK_impl);
    }
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

void MFPluginVpp::CloseCodec(MyAutoLock& lock)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxU32 i = 0;

    if (m_pFreeSamplesPool)
    {
        lock.Unlock();
        m_pFreeSamplesPool->RemoveCallback();
        lock.Lock();
        SAFE_RELEASE(m_pFreeSamplesPool);
    }
    if (m_pmfxVPP) m_pmfxVPP->Close();
    SAFE_DELETE_ARRAY(m_pInSurfaces);
    if (m_pWorkSurfaces)
    {
        for (i = 0; i < m_nOutSurfacesNum; ++i)
        {
            if (m_pWorkSurfaces) SAFE_DELETE(m_pWorkSurfaces[i]);
            if (m_pOutSurfaces)  SAFE_DELETE(m_pOutSurfaces[i].pSyncPoint);
        }
    }
    SAFE_FREE(m_pWorkSurfaces);
    SAFE_FREE(m_pOutSurfaces);
    if (m_pFrameAllocator)
    {
        if (IS_MEM_VIDEO(m_uiInSurfacesMemType))
            m_pFrameAllocator->GetMFXAllocator()->Free(m_pFrameAllocator->GetMFXAllocator()->pthis, &m_VppInAllocResponse);
        if (IS_MEM_VIDEO(m_uiOutSurfacesMemType))
            m_pFrameAllocator->GetMFXAllocator()->Free(m_pFrameAllocator->GetMFXAllocator()->pthis, &m_VppOutAllocResponse);
        memset((void*)&m_VppInAllocResponse, 0, sizeof(mfxFrameAllocResponse));
        memset((void*)&m_VppOutAllocResponse, 0, sizeof(mfxFrameAllocResponse));
    }
    if (!m_bReinit)
    {
        SAFE_RELEASE(m_pFrameAllocator);
        if (m_pDeviceDXVA)
        {
            DXVASupportCloseWrapper();
            m_pDeviceDXVA->ReleaseFrameAllocator();
        }
        SAFE_RELEASE(m_pDeviceDXVA);
        SAFE_RELEASE(m_pFakeSample);
        SAFE_RELEASE(m_pPostponedInput);
    }
    // return parameters to initial state
    m_uiInSurfacesMemType  = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    m_uiOutSurfacesMemType = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    m_nInSurfacesNum       = MF_ADDITIONAL_IN_SURF_NUM;
    m_nOutSurfacesNum      = MF_ADDITIONAL_OUT_SURF_NUM;
    m_pInSurface           = NULL;
    m_pOutSurface          = NULL;
    m_pDispSurface         = NULL;

    m_bInitialized = false;
}

/*------------------------------------------------------------------------------*/

mfxStatus MFPluginVpp::ResetCodec(MyAutoLock& /*lock*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE, res = MFX_ERR_NONE;
    mfxU32 i = 0;
    MFYuvOutData* pDispSurface = NULL;

    // resetting async thread
    m_pAsyncThreadSemaphore->Reset();
    m_pAsyncThreadEvent->Reset();
    // releasing all input and output samples
    while (m_uiHasOutputEventExists && (m_pDispSurface != m_pOutSurface))
    {
        --m_uiHasOutputEventExists;
        // dropping decoded frames
        if (!(m_pDispSurface->bSyncPointUsed))
        {
            m_pMfxVideoSession->SyncOperation(*(m_pDispSurface->pSyncPoint),
                                              INFINITE);
        }
        // trying to free this surface
        if (m_pDispSurface->pMFSurface) // should not be NULL, checking just in case
        {
            m_pDispSurface->pMFSurface->IsDisplayed(true);
            m_pDispSurface->pMFSurface->Release();
        }
        m_pDispSurface->pSurface   = NULL;
        m_pDispSurface->pMFSurface = NULL;
        m_pDispSurface->bSyncPointUsed = false;
        m_pDispSurface->iSyncPointSts  = MFX_ERR_NONE;
        // moving to next undisplayed surface
        pDispSurface = m_pDispSurface;
        ++pDispSurface;
        if ((mfxU32)(pDispSurface-m_pOutSurfaces) >= m_nOutSurfacesNum)
            pDispSurface = m_pOutSurfaces;
        m_pDispSurface = pDispSurface;
    }
    if (m_pmfxVPP)
    { // resetting vpp
        res = m_pmfxVPP->Reset(&m_MfxParamsVideo);
        if (MFX_ERR_NONE == sts) sts = res;
    }
    if (m_pWorkSurfaces)
    { // releasing resources
        for (i = 0; i < m_nOutSurfacesNum; ++i)
        {
            if (FAILED((m_pWorkSurfaces[i])->Release()) && (MFX_ERR_NONE == sts)) sts = MFX_ERR_UNKNOWN;
        }
        m_pOutSurface->pMFSurface = m_pWorkSurface;
        m_pOutSurface->pSurface = m_pWorkSurface->GetSurface();
    }
    if (m_pInSurfaces)
    { // releasing resources
        for (i = 0; i < m_nInSurfacesNum; ++i)
        {
            if (FAILED(m_pInSurfaces[i].Release(true)) && (MFX_ERR_NONE == sts)) sts = MFX_ERR_UNKNOWN;
        }
        m_pInSurface = &(m_pInSurfaces[0]);
    }
    m_bEndOfInput     = false;
    m_bStartDrain     = false;
    m_bNeedInSurface  = false;
    m_bNeedOutSurface = false;

    if (!m_bStreamingStarted)
    {
        /* We should zero event counts only if we are resetting before
         * processing new stream. We should not zero them on errors during
         * current processing - this may cause hangs, because ProcessOutput
         * may be called out of order.
         */
        m_NeedInputEventInfo.m_requested = 0;
        m_HasOutputEventInfo.m_requested = 0;
        m_NeedInputEventInfo.m_sent = 0;
        m_HasOutputEventInfo.m_sent = 0;
    }
    m_uiHasOutputEventExists = 0;
    m_uiDelayedOutputsNum = 0;

    m_LastSts   = MFX_ERR_NONE;
    m_SyncOpSts = MFX_ERR_NONE;

    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVpp::TryReinit(MyAutoLock& lock, mfxStatus& sts, bool bReady)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    HRESULT hr = S_OK;

    sts = MFX_ERR_NONE;
    if (SUCCEEDED(m_hrError))
    {
        if (!m_pDeviceDXVA || m_pDeviceDXVA && bReady)
        { // we are connected with CIP decoder and 3-d party encoder
            m_bReinitStarted = true;

            CloseCodec(lock);
            m_bReinit = false;
            m_bSetDiscontinuityAttribute = true;
            SAFE_RELEASE(m_pFakeSample);
            /* Releasing here fake sample we will invoke SampleAppeared function
             * in upstream decoder plug-in and it will continue processing.
             */
            if (m_pPostponedInput)
            {
                hr = ProcessSample(lock, m_pPostponedInput, sts);
                SAFE_RELEASE(m_pPostponedInput);
            }
            else if (!m_bDoNotRequestInput && !m_NeedInputEventInfo.m_requested) hr = RequestInput();
            m_bReinitStarted = false;
        }
    }
    else
    {
        m_bReinitStarted = true;
        CloseCodec(lock);
        m_bReinit = false;
        SAFE_RELEASE(m_pFakeSample);
        SAFE_RELEASE(m_pPostponedInput);
        if (!m_bDoNotRequestInput && !m_NeedInputEventInfo.m_requested) hr = RequestInput();
        m_bReinitStarted = false;
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

mfxStatus MFPluginVpp::InitPlg(IMfxVideoSession* pSession,
                               mfxVideoParam*    pDecVideoParams,
                               mfxU32*           pDecSurfacesNum)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);
    mfxStatus sts = MFX_ERR_NONE;

    m_bDirectConnectionMFX = true;

    if (FAILED(m_hrError)) sts = MFX_ERR_UNKNOWN;
    if ((MFX_ERR_NONE == sts) && !pSession) sts = MFX_ERR_NULL_PTR;
    if ((MFX_ERR_NONE == sts) && !pDecVideoParams) sts = MFX_ERR_NULL_PTR;
    if ((MFX_ERR_NONE == sts) && !pDecSurfacesNum) sts = MFX_ERR_NULL_PTR;
    if ((MFX_ERR_NONE == sts) && !IS_MEM_OUT(pDecVideoParams->IOPattern)) sts = MFX_ERR_UNSUPPORTED;
    if ((MFX_ERR_NONE == sts) && pSession)
    {
        MFXVideoVPPEx* vpp = NULL;
        MFXVideoSession* session = (MFXVideoSession*)pSession;

        SAFE_NEW(vpp, MFXVideoVPPEx(*session));
        if (!vpp) sts = MFX_ERR_MEMORY_ALLOC;

        if (MFX_ERR_NONE == sts)
        {
            if (m_pmfxVPP)
            {
                m_pmfxVPP->Close();
                SAFE_DELETE(m_pmfxVPP);
            }
            if (!m_pInternalMfxVideoSession)
            {
                m_pMfxVideoSession->AddRef();
                m_pInternalMfxVideoSession = m_pMfxVideoSession;
            }
            SAFE_RELEASE(m_pMfxVideoSession);

            pSession->AddRef();
            m_pMfxVideoSession = pSession;
            m_pmfxVPP = vpp;
        }
    }
    if (MFX_ERR_NONE == sts)
    {
        // correcting of parameters obtained on plug-ins connection
        memcpy_s(&(m_MfxParamsVideo.vpp.In), sizeof(m_MfxParamsVideo.vpp.In),
                 &(pDecVideoParams->mfx.FrameInfo), sizeof(pDecVideoParams->mfx.FrameInfo));
        // changing original geometry for direct connection of decoder & vpp
        memcpy_s(&(m_MfxParamsVideo_original.vpp.In), sizeof(m_MfxParamsVideo_original.vpp.In),
                 &(m_MfxParamsVideo.vpp.In), sizeof(m_MfxParamsVideo.vpp.In));
        //TODO: AsyncDepth, Protected, IOPattern, ExtParam, NumExtParam
    }
    // encoder initialization
    if (MFX_ERR_NONE == sts) sts = InitCodec(lock,
                                             pDecVideoParams->IOPattern,
                                             pDecSurfacesNum);
    /* NOTE:
     *  If plug-in error will not be set here, then crash can occur on
     * ProcessFrame call from async thread.
     */
    if ((MFX_ERR_NONE != sts) && SUCCEEDED(m_hrError))
    {
        // setting plug-in error
        SetPlgError(E_FAIL, sts);
        // handling error
        HandlePlgError(lock);
    }
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

inline MFYuvInSurface* MFPluginVpp::SetFreeInSurface(MFYuvInSurface* pSurfaces,
                                                     mfxU32 nSurfacesNum)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MFYuvInSurface* pSurface = NULL;
    HRESULT hr = S_OK;
    mfxU32 i = 0;
    bool bFound = false;

    for (i = 0; i < nSurfacesNum; ++i)
    {
        hr = pSurfaces[i].Release();
        // if release failed then surface could not be unlocked yet,
        // that's not an error
        if (SUCCEEDED(hr) && !bFound)
        {
            bFound = true;
            pSurface = &(pSurfaces[i]);
        }
    }
    MFX_LTRACE_P(MF_TL_PERF, pSurface);
    return pSurface;
}

/*------------------------------------------------------------------------------*/

inline HRESULT MFPluginVpp::SetFreeWorkSurface(MyAutoLock& /*lock*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    HRESULT hr = E_FAIL, hr_sts = S_OK;
    mfxU32 i = 0, j = 0;
    bool bFound = false, bWait = false;

    do
    {
        // trying to find free out surface
        for (i = 0; i < m_nOutSurfacesNum; ++i)
        {
            hr_sts = m_pWorkSurfaces[i]->Release();
            // if release failed then surface could not be unlocked yet,
            // that's not an error
            if (SUCCEEDED(hr_sts) && !bFound)
            {
                hr = S_OK;
                bFound = true;
                m_pWorkSurface = m_pWorkSurfaces[i];
            }
        }
#if 0 // will not wait
        // wait for free surface
        bWait = !bFound;
        if (bWait) Sleep(MF_DEV_BUSY_SLEEP_TIME);
#endif
        ++j;
    } while (bWait && (j < MF_FREE_SURF_WAIT_NUM));

    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

inline HRESULT MFPluginVpp::SetFreeOutSurface(MyAutoLock& lock)
{
    HRESULT hr = S_OK;
    MFYuvOutData* pOutSurface = m_pOutSurface;

    hr = SetFreeWorkSurface(lock);
    if (SUCCEEDED(hr))
    {
        ++pOutSurface;
        if ((mfxU32)(pOutSurface-m_pOutSurfaces) >= m_nOutSurfacesNum)
        {
            pOutSurface = m_pOutSurfaces;
        }
        pOutSurface->pMFSurface = m_pWorkSurface;
        pOutSurface->pSurface   = m_pWorkSurface->GetSurface();
        // setting free out surface using atomic operation
        m_pOutSurface = pOutSurface;
    }
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVpp::FlushDelayedOutputs(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    HRESULT hr = S_OK, hr_sts = S_OK;

    for (; m_uiDelayedOutputsNum; --m_uiDelayedOutputsNum)
    {
        hr_sts = SendOutput();
        if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
    }
    return hr;
}

/*------------------------------------------------------------------------------*/

/* SampleAppeared function notes:
 *  1. Whether arrived surface can be locked?
 *    - on normal processing with CIP downstrea plug-in arrived sample will be
 * associated with the surface already unlocked by MediaSDK.
 *    - if error occured in CIP downstream plug-in then surface can be still
 * locked (by current plug-in).
 *    - on processing with 3-d party downstream plug-in surface can also be
 * still locked (by current plug-in), but this should be rare case.
 *  2. If call of the function is fake, it is needed to be careful with used
 * objects: they can be not initialized yet.
 */
void MFPluginVpp::SampleAppeared(bool bFakeSample, bool bReinitSample)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = S_OK;
    mfxStatus sts = MFX_ERR_NONE;

    MFX_LTRACE_I(MF_TL_PERF, bFakeSample);
    MFX_LTRACE_I(MF_TL_PERF, m_bNeedInSurface);
    MFX_LTRACE_I(MF_TL_PERF, m_bNeedOutSurface);
    MFX_LTRACE_I(MF_TL_PERF, m_bEndOfInput);

    // errors checking
    if (m_bReinit && m_bReinitStarted) return;
    if (bFakeSample)
    { // additional checks on fake call
        if (!m_bInitialized) return;
    }
    if (bReinitSample || m_bReinit && !m_bEndOfInput)
    {
        hr = TryReinit(lock, sts, bReinitSample);
    }
    else
    {
        if (SUCCEEDED(m_hrError) && m_bNeedOutSurface)
        {
            MFX_LTRACE_S(MF_TL_PERF, "Setting free out surface");
            // setting free work surface
            if (SUCCEEDED(SetFreeOutSurface(lock)) && SUCCEEDED(m_hrError))
            {
                m_bNeedOutSurface = false;

                if (!m_bStartDrain)
                {
                    if (MFX_ERR_MORE_SURFACE == m_LastSts)
                    {
                        MFX_AUTO_LTRACE(MF_TL_PERF, "ProcessFrame");
                        hr = ProcessFrame(lock, sts);
                    }
                }
                else if (!bFakeSample)
                {
                    AsyncThreadPush();
                }
            }
        }
        if (SUCCEEDED(m_hrError) && m_bNeedInSurface && (MFX_ERR_MORE_SURFACE != m_LastSts))
        { // setting free in surface
            MFX_LTRACE_S(MF_TL_PERF, "Setting free in surface");
            m_pInSurface = SetFreeInSurface(m_pInSurfaces, m_nInSurfacesNum);
            if (m_pInSurface) m_bNeedInSurface = false;
        }
        if (SUCCEEDED(m_hrError) && !m_bReinit && !m_bDoNotRequestInput)
        {
            if (!m_NeedInputEventInfo.m_requested && !m_bEndOfInput)
            { // trying to request more input only if there are no pending requests
                if (!m_bNeedInSurface && !m_bNeedOutSurface)
                { // we have free input & output surfaces
                    RequestInput();
                }
                else if (m_pDispSurface == m_pOutSurface)
                { // we have: 1) no free surfaces, 2) no buffered output
                    MFX_LTRACE_S(MF_TL_PERF, "SampleAppeared awaiting...");
                }
            }
        }
    }
    MFX_LTRACE_D(MF_TL_PERF, hr);
    if (FAILED(hr) && SUCCEEDED(m_hrError))
    {
        // setting plug-in error
        SetPlgError(hr, sts);
        // handling error
        HandlePlgError(lock, bFakeSample);
    }
}

/*------------------------------------------------------------------------------*/

void MFPluginVpp::SetPlgError(HRESULT hr, mfxStatus sts)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    MFX_LTRACE_I(MF_TL_GENERAL, sts);

    MFPlugin::SetPlgError(hr, sts);
    if (FAILED(hr))
    { // dumping error information
        if (SUCCEEDED(m_MfxCodecInfo.m_hrErrorStatus))
        {
            m_MfxCodecInfo.m_hrErrorStatus = hr;
        }
        if (MFX_ERR_NONE == m_MfxCodecInfo.m_ErrorStatus)
        {
            m_MfxCodecInfo.m_ErrorStatus = sts;
        }
    }
    return;
}


/*------------------------------------------------------------------------------*/

void MFPluginVpp::ResetPlgError(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    MFPlugin::ResetPlgError();
    m_MfxCodecInfo.m_uiErrorResetCount = m_uiErrorResetCount;
}
/*------------------------------------------------------------------------------*/

void MFPluginVpp::HandlePlgError(MyAutoLock& lock, bool bCalledFromAsyncThread)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    if (FAILED(m_hrError))
    {
        if (m_bErrorHandlingFinished)
        {
            if (m_bEndOfInput && m_bSendDrainComplete)
            {
                m_bEndOfInput = false;
                m_bSendDrainComplete = false;
                DrainComplete();
            }
        }
        else if (!m_bErrorHandlingStarted)
        {
            m_bErrorHandlingStarted = true;

            // notifying upstream plug-in about error occurred only if no output was generated
            if (m_pInputType && m_dbg_return_errors)
            {
                m_pInputType->SetUINT32(MF_MT_DOWNSTREAM_ERROR, m_hrError);
            }
            // notifying async thread
            if (m_pAsyncThread && !bCalledFromAsyncThread)
            {
                m_pAsyncThreadEvent->Signal();
                m_pAsyncThreadSemaphore->Post();
                // awaiting while error will be detected by async thread
                lock.Unlock();
                m_pErrorFoundEvent->Wait();
                m_pErrorFoundEvent->Reset();
                lock.Lock();
            }
            // resetting
            ResetCodec(lock); // status is not needed here

            if (m_bSendDrainComplete)
            {
                m_bSendDrainComplete = false;
                DrainComplete();
            }
            else if (m_bReinit)
            {
                if (!m_bReinitStarted)
                {
                    mfxStatus sts = MFX_ERR_NONE;

                    TryReinit(lock, sts, false);
                }
            }
            else
            { // forcing calling of ProcessInput on errors during processing
                if (!m_bDoNotRequestInput && !m_NeedInputEventInfo.m_requested) RequestInput();
            }

            // sending event that error was handled
            if (m_pAsyncThread && !bCalledFromAsyncThread)
            {
                m_pErrorHandledEvent->Signal();
            }
            m_bErrorHandlingFinished = true;
        }
    }
}

/*------------------------------------------------------------------------------*/

bool MFPluginVpp::ReturnPlgError(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    if (m_bDirectConnectionMFX) return false;
    return (m_dbg_return_errors)? true: false;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVpp::AsyncThreadFunc(void)
{
    { MFX_AUTO_LTRACE(MF_TL_PERF, "ThreadName=AT VPP"); }
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);
    mfxStatus sts = MFX_ERR_NONE;
    mfxSyncPoint* pSyncPoint = NULL;

    while (1)
    {
        HRESULT hr = S_OK;

        // avaiting for the signal that next surface is ready
        lock.Unlock();
        {
            MFX_AUTO_LTRACE(MF_TL_PERF, "wait::frame");
            while (WAIT_TIMEOUT == m_pAsyncThreadSemaphore->TimedWait(MF_VPP_FAKE_SAMPLE_APPEARED_SLEEP_TIME))
            {
                SampleAppeared(true, false);
            }
        }
        lock.Lock();
        if (m_bAsyncThreadStop) break;
        if (FAILED(m_hrError) && m_bErrorHandlingStarted && !m_bErrorHandlingFinished)
        {
            // sending event that error was detected and thread was stopped
            m_pErrorFoundEvent->Signal();
            // awaiting while error will be handled
            lock.Unlock();
            m_pErrorHandledEvent->Wait();
            m_pErrorHandledEvent->Reset();
            lock.Lock();
            continue;
        }
        if (SUCCEEDED(m_hrError) && m_bEndOfInput && !m_uiHasOutputEventExists)
        {
            if (m_bInitialized)
            {
                m_bStartDrain = true;
                if (m_bNeedOutSurface)
                {
                    lock.Unlock();
                    SampleAppeared(true, false);
                    lock.Lock();
                }
                hr = ProcessFrame(lock, sts);
            }
            else hr = MF_E_TRANSFORM_NEED_MORE_INPUT;

            if (MF_E_TRANSFORM_NEED_MORE_INPUT == hr)
            {
                hr = S_OK;

                m_bEndOfInput = false;
                m_bStartDrain = false;

                if (m_bReinit && !m_bReinitStarted) hr = TryReinit(lock, sts, false);

                if (SUCCEEDED(hr))
                {
                    if (m_bSendDrainComplete)
                    {
                        m_bSendDrainComplete = false;
                        DrainComplete();
                    }
                    continue;
                }
            }
            else if (SUCCEEDED(hr))
            {
                if (!m_bNeedOutSurface) AsyncThreadPush();
                continue;
            }
        }
        if (SUCCEEDED(hr) && SUCCEEDED(m_hrError))
        {
            // awaiting for the async operation completion if needed
            if (m_pDispSurface->bSyncPointUsed) sts = m_pDispSurface->iSyncPointSts;
            else
            {
#ifdef OMIT_VPP_ASYNC_SYNC_POINT
                if (!m_pDeviceDXVA)
#else
                if (1)
#endif
                {
                    m_pDispSurface->bSyncPointUsed = true;
                    pSyncPoint = m_pDispSurface->pSyncPoint;
                    lock.Unlock();
                    {
                        MFX_AUTO_LTRACE(MF_TL_PERF, "wait::SyncOperation");
                        m_SyncOpSts = sts = m_pMfxVideoSession->SyncOperation(*pSyncPoint, INFINITE);
                        MFX_LTRACE_I(MF_TL_PERF, sts);
                        // sending event that some resources may be free
                        m_pDevBusyEvent->Signal();
                    }
                    lock.Lock();
                }
                else sts = MFX_ERR_NONE;
                m_pDispSurface->iSyncPointSts = sts;
            }
            // sending request for ProcessOutput call
            if (MFX_ERR_NONE == sts)
            {
                if (m_NeedInputEventInfo.m_requested && !m_bEndOfInput && !m_bReinit) ++m_uiDelayedOutputsNum;
                else hr = SendOutput();
            }
            else hr = E_FAIL;
        }
        if (SUCCEEDED(hr) && SUCCEEDED(m_hrError))
        {
            // awaiting for ProcessOuput completion
            lock.Unlock();
            {
                MFX_AUTO_LTRACE(MF_TL_PERF, "wait::ProcessOutput");
                m_pAsyncThreadEvent->Wait();
                m_pAsyncThreadEvent->Reset();
            }
            lock.Lock();
        }
        // handling errors
        if (FAILED(hr) || FAILED(m_hrError))
        { // only here we will call HandlePlgError on errors - we need to send DrainComplete event
            SetPlgError(hr, sts);
            HandlePlgError(lock, true);
        }
    };
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

// Function returns 'true' if SyncOperation was called and 'false' overwise
bool MFPluginVpp::HandleDevBusy(mfxStatus& sts)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    bool ret_val = false;
    MFYuvOutData* pOutSurface = m_pDispSurface;

    sts = MFX_ERR_NONE;
    do
    {
        if (!(pOutSurface->bSyncPointUsed) && pOutSurface->pMFSurface)
        {
            pOutSurface->bSyncPointUsed = true;
            sts = pOutSurface->iSyncPointSts  = m_pMfxVideoSession->SyncOperation(*(pOutSurface->pSyncPoint), INFINITE);
            ret_val = true;
            break;
        }
        ++pOutSurface;
        if ((mfxU32)(pOutSurface-m_pOutSurfaces) >= m_nOutSurfacesNum) pOutSurface = m_pOutSurfaces;
    } while(pOutSurface != m_pDispSurface);
    MFX_LTRACE_I(MF_TL_GENERAL, ret_val);
    return ret_val;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVpp::ProcessFrame(MyAutoLock& lock, mfxStatus& sts)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    HRESULT hr = S_OK;
    mfxFrameSurface1* pInFrameSurface  = NULL;
    mfxFrameSurface1* pOutFrameSurface = NULL;
    bool bSendFakeSrf = false;

    if (m_bInitialized)
    {
        sts = MFX_ERR_MORE_SURFACE;
        pInFrameSurface = (m_bStartDrain)? NULL : m_pInSurface->GetSurface();
        while (SUCCEEDED(hr) && (MFX_ERR_MORE_SURFACE == sts) && !m_bNeedOutSurface)
        {
            pOutFrameSurface = m_pWorkSurface->GetSurface(); // can not be NULL
            if (pOutFrameSurface) // should alwais be valid pointer
            {
                do
                {
                    m_LastSts = sts = m_pmfxVPP->RunFrameVPPAsync(pInFrameSurface,
                                                                  pOutFrameSurface,
                                                                  NULL,
                                                                  m_pOutSurface->pSyncPoint);
                    if ((MFX_WRN_DEVICE_BUSY == sts) && (MFX_ERR_NONE == m_SyncOpSts))
                    {
                        MFX_AUTO_LTRACE(MF_TL_PERF, "mfx(RunFrameVPPAsync)::MFX_WRN_DEVICE_BUSY");
                        mfxStatus res = MFX_ERR_NONE;

                        if (!HandleDevBusy(res))
                        {
                            m_pDevBusyEvent->TimedWait(MF_DEV_BUSY_SLEEP_TIME);
                            m_pDevBusyEvent->Reset();
                        }
                        if (MFX_ERR_NONE != res) sts = res;
                    }
                } while ((MFX_WRN_DEVICE_BUSY == sts) && (MFX_ERR_NONE == m_SyncOpSts));
            }
            else sts = MFX_ERR_NULL_PTR;
            // valid cases for the status are:
            // MFX_ERR_NONE - data processed, output will be generated
            // MFX_ERR_MORE_DATA - data buffered, output will not be generated

            // Status correction
            if ((MFX_ERR_MORE_DATA == sts) && m_bStartDrain && m_bSendFakeSrf)
            {
                sts = MFX_ERR_NONE;
                m_pOutSurface->iSyncPointSts  = MFX_ERR_NONE;
                m_pOutSurface->bSyncPointUsed = true;

                // we will send fake surface only once on each reinit
                bSendFakeSrf   = true;
                m_bSendFakeSrf = false;
            }
            // Status processing
            if ((MFX_ERR_MORE_DATA == sts) && m_bStartDrain && !bSendFakeSrf)
            {
                hr = MF_E_TRANSFORM_NEED_MORE_INPUT;
            }
            else if ((MFX_ERR_NONE == sts) || (MFX_ERR_MORE_DATA == sts) || (MFX_ERR_MORE_SURFACE == sts))
            {
                bool bOutExists = ((MFX_ERR_NONE == sts) || (MFX_ERR_MORE_SURFACE == sts));

                if (bOutExists)
                {
                    // locking surface for future displaying
                    m_pOutSurface->pMFSurface->IsDisplayed(false);
                    m_pOutSurface->pMFSurface->IsFakeSrf(bSendFakeSrf);
                    m_pOutSurface->pMFSurface->IsGapSrf(m_bSetDiscontinuityAttribute);
                    if (m_bSetDiscontinuityAttribute) m_bSetDiscontinuityAttribute = false;
                    {
                        // sending output
                        ++m_uiHasOutputEventExists;
                        if (SUCCEEDED(hr)) hr = FlushDelayedOutputs();
                        AsyncThreadPush();
                    }
                }
                if (pOutFrameSurface->Data.Locked || bOutExists)
                {
                    // setting free out surface
                    if (FAILED(SetFreeOutSurface(lock))) m_bNeedOutSurface = true;
                    MFX_LTRACE_I(MF_TL_PERF, m_bNeedOutSurface);
                }
            }
            else hr = E_FAIL;
            // no iterations on EOS
            if (m_bEndOfInput) break;
        }
        if (!m_bStartDrain)
        {
            if (MFX_ERR_MORE_SURFACE == m_LastSts) m_bNeedInSurface = true;
            else
            {
                // trying to set free surface
                m_pInSurface = SetFreeInSurface(m_pInSurfaces, m_nInSurfacesNum);
                if (SUCCEEDED(hr) && !m_pInSurface) m_bNeedInSurface = true;
            }
            MFX_LTRACE_I(MF_TL_PERF, m_bNeedInSurface);
        }
    }
    MFX_LTRACE_I(MF_TL_PERF, sts);
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVpp::ProcessSample(MyAutoLock& lock,
                                   IMFSample* pSample,
                                   mfxStatus& sts)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    HRESULT hr = S_OK;

    sts = MFX_ERR_NONE;
    // vpp initialization
    if (SUCCEEDED(hr) && !m_bInitialized)
    {
        sts = InitCodec(lock);
        if (MFX_ERR_NONE != sts) hr = E_FAIL;
    }
    if (m_bReinit)
    {
        /* Reinitalization detected in SetInputType function. We can't process
         * input sample right now - buffering it for future processing.
         */
        SAFE_RELEASE(m_pPostponedInput); // should be NULL here, releasing just in case
        pSample->AddRef();
        m_pPostponedInput = pSample;
    }
    else
    {
        if (SUCCEEDED(hr)) hr = m_pInSurface->Load(pSample, m_bDirectConnectionMFX);
        if (SUCCEEDED(hr) && !m_pInSurface->IsFakeSrf()) ++(m_MfxCodecInfo.m_nInFrames);
        // Processing input data
        if (SUCCEEDED(hr))
        {
            if (m_pInSurface->IsFakeSrf())
            {
                // saving this sample
                pSample->AddRef();
                m_pFakeSample = pSample;

                m_pInSurface = SetFreeInSurface(m_pInSurfaces, m_nInSurfacesNum);
                // setting flags to process this situation
                m_bReinit     = true;
                m_bEndOfInput = true; // we need to obtain buffered frames
                if (m_pDeviceDXVA) m_bSendFakeSrf = true; // we need to send fake frame
                else m_bSendFakeSrf = false;
                // forcing output sample request if needed
                if (SUCCEEDED(hr)) hr = FlushDelayedOutputs();
                // pushing async thread (starting call RunFrameVPPAsync with NULL bitstream)
                AsyncThreadPush();
            }
            else hr = ProcessFrame(lock, sts);
        }
        if (SUCCEEDED(hr) && !m_bReinit && !m_bDoNotRequestInput)
        {
            // requesting input while free surfaces (in & out) are available
            if (!m_NeedInputEventInfo.m_requested && !m_bNeedInSurface && !m_bNeedOutSurface)
            {
                hr = RequestInput();
            }
            else hr = FlushDelayedOutputs();
        }
    }
    MFX_LTRACE_I(MF_TL_PERF, sts);
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVpp::ProcessInput(DWORD      dwInputStreamID,
                                  IMFSample* pSample,
                                  DWORD      dwFlags)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MFTicker ticker(&m_ticks_ProcessInput);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = S_OK;
    mfxStatus sts = MFX_ERR_NONE;

    GetPerformance();
    hr = MFPlugin::ProcessInput(dwInputStreamID, pSample, dwFlags);
    CHECK_EXPRESSION(SUCCEEDED(hr), hr);

    SET_HR_ERROR(0 == dwFlags, hr, E_INVALIDARG);
    SET_HR_ERROR(!m_bEndOfInput || m_bReinit, hr, E_FAIL);
    if (SUCCEEDED(hr) && m_pOutputType)
    {
        UINT32 hr_sts = S_OK;

        if (SUCCEEDED(m_pOutputType->GetUINT32(MF_MT_DOWNSTREAM_ERROR, &hr_sts)))
        {
            hr = hr_sts;
        }
    }

    if (SUCCEEDED(hr)) hr = m_hrError;
    if (SUCCEEDED(hr)) hr = ProcessSample(lock, pSample, sts);
    if (FAILED(hr) && SUCCEEDED(m_hrError))
    {
        // setting plug-in error
        SetPlgError(hr, sts);
        // handling error
        HandlePlgError(lock);
    }
    // errors will be handled either in this function or in downstream plug-in
    if (!ReturnPlgError()) hr = S_OK;
    MFX_LTRACE_I(MF_TL_PERF, sts);
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVpp::ProcessOutput(DWORD  dwFlags,
                                   DWORD  cOutputBufferCount,
                                   MFT_OUTPUT_DATA_BUFFER* pOutputSamples,
                                   DWORD* pdwStatus)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MFTicker ticker(&m_ticks_ProcessOutput);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = S_OK;
    mfxStatus sts = MFX_ERR_NONE;
    bool bDisplayFrame = false;

    GetPerformance();
    hr = MFPlugin::ProcessOutput(dwFlags, cOutputBufferCount, pOutputSamples, pdwStatus);
    CHECK_EXPRESSION(SUCCEEDED(hr), hr);

    SET_HR_ERROR(0 == dwFlags, hr, E_INVALIDARG);
    SET_HR_ERROR(m_pmfxVPP, hr, E_FAIL);
    SET_HR_ERROR(m_pDispSurface && ((m_pDispSurface != m_pOutSurface) || m_bNeedOutSurface || m_bEndOfInput),
                 hr, E_FAIL);

    if (SUCCEEDED(hr)) hr = m_hrError;
    if (m_bChangeOutputType)
    {
        MFX_LTRACE_S(MF_TL_PERF, "Starting change of output type");
        if (SUCCEEDED(hr))
        {
            pOutputSamples[0].dwStatus = MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE;
            *pdwStatus = 0;
            hr = MF_E_TRANSFORM_STREAM_CHANGE;
        }
    }
    else
    {
        MFX_LTRACE_S(MF_TL_PERF, "Normal processing");
        if (m_uiHasOutputEventExists) --m_uiHasOutputEventExists;
        if (SUCCEEDED(hr) && ((m_pDispSurface != m_pOutSurface) || m_bNeedOutSurface)) bDisplayFrame = true;
        if (SUCCEEDED(hr) && bDisplayFrame)
        {
            hr = m_pDispSurface->pMFSurface->Sync();
            if (((m_MfxParamsVideo.vpp.Out.Width != m_MfxParamsVideo_original.vpp.Out.Width) ||
                 (m_MfxParamsVideo.vpp.Out.Height != m_MfxParamsVideo_original.vpp.Out.Height)))
            {
                HRESULT hr_sts = S_OK;
                hr_sts = m_pDispSurface->pMFSurface->Pretend(m_MfxParamsVideo_original.vpp.Out.Width,
                                                             m_MfxParamsVideo_original.vpp.Out.Height);
                if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
            }
        }
        // Set status flags
        if (SUCCEEDED(hr))
        {
            pOutputSamples[0].pSample  = (bDisplayFrame)? m_pDispSurface->pMFSurface->GetSample(): NULL;
            if (m_HasOutputEventInfo.m_requested)
            {
                pOutputSamples[0].dwStatus = MFT_OUTPUT_DATA_BUFFER_INCOMPLETE;
            }
            *pdwStatus = 0;

            if (pOutputSamples[0].pSample)
            {
                UINT32 bFakeSrf = FALSE;

                if (FAILED(pOutputSamples[0].pSample->GetUINT32(MF_MT_FAKE_SRF, &bFakeSrf)) || !bFakeSrf)
                {
                    ++(m_MfxCodecInfo.m_nOutFrames);
                }
            }
        }
        if (bDisplayFrame)
        {
            MFYuvOutData* pDispSurface = NULL;

            // trying to free this surface
            m_pDispSurface->pMFSurface->IsDisplayed(true);
            m_pDispSurface->pMFSurface->Release();
            m_pDispSurface->pSurface   = NULL;
            m_pDispSurface->pMFSurface = NULL;
            m_pDispSurface->bSyncPointUsed = false;
            // moving to next undisplayed surface
            pDispSurface = m_pDispSurface;
            ++pDispSurface;
            if ((mfxU32)(pDispSurface-m_pOutSurfaces) >= m_nOutSurfacesNum)
                pDispSurface = m_pOutSurfaces;
            m_pDispSurface = pDispSurface;

            m_pAsyncThreadEvent->Signal();
        }
        if (m_bNeedOutSurface)
        { // processing remained output and setting free out surface
            HRESULT hr_sts = S_OK;
            // setting free out surface
            hr_sts = SetFreeOutSurface(lock);
            if (SUCCEEDED(hr_sts))
            {
                m_bNeedOutSurface = false;
                if (MFX_ERR_MORE_SURFACE == m_LastSts)
                {
                    hr_sts = ProcessFrame(lock, sts);
                    if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
                }
            }
        }
        MFX_LTRACE_I(MF_TL_PERF, m_bNeedOutSurface);
        if (m_bNeedInSurface && (MFX_ERR_MORE_SURFACE != m_LastSts))
        { // setting free in surface
            m_pInSurface = SetFreeInSurface(m_pInSurfaces, m_nInSurfacesNum);
            if (m_pInSurface) m_bNeedInSurface = false;
        }
        MFX_LTRACE_I(MF_TL_PERF, m_bNeedInSurface);
        if (!m_bEndOfInput && !m_bDoNotRequestInput)
        { // requesting input if needed
            if (!m_NeedInputEventInfo.m_requested)
            { // trying to request more input only if there are no pending requests
                if (!m_bNeedInSurface && !m_bNeedOutSurface)
                { // we have free input & output surfaces
                    RequestInput();
                }
                else if (m_pDispSurface == m_pOutSurface)
                { // we have: 1) no free surfaces, 2) no buffered output
                    MFX_LTRACE_S(MF_TL_PERF, "SampleAppeared awaiting...");
                }
            }
        }
    }
    if (MF_E_TRANSFORM_STREAM_CHANGE != hr)
    {
        if (pOutputSamples && (FAILED(hr) || !(pOutputSamples[0].pSample) && !(pOutputSamples[0].dwStatus)))
        { // we should set MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE status if we do not generate output
            SAFE_RELEASE(pOutputSamples[0].pSample);
            pOutputSamples[0].dwStatus = MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE;
        }
        if (FAILED(hr))
        {
            if (SUCCEEDED(m_hrError))
            {
                // setting plug-in error
                SetPlgError(hr, sts);
                // handling error
                HandlePlgError(lock);
            }
        }
        // errors will be handled either in ProcessInput or in upstream plug-in
        hr = S_OK;
    }
    MFX_LTRACE_I(MF_TL_PERF, m_MfxCodecInfo.m_nOutFrames);
    MFX_LTRACE_I(MF_TL_PERF, sts);
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/
// Global functions

#undef  PTR_THIS // no 'this' pointer in global functions
#define PTR_THIS NULL

#undef  MFX_TRACE_CATEGORY
#ifdef MF_SHORT_CAT_NAMES
    #define MFX_TRACE_CATEGORY _T("mfx_mft_vpp")
#else
    #define MFX_TRACE_CATEGORY (pRegData && pRegData->pPluginName)? pRegData->pPluginName: _T("mfx_mft_vpp")
#endif

// CreateInstance
// Static method to create an instance of the source.
//
// iid:         IID of the requested interface on the source.
// ppSource:    Receives a ref-counted pointer to the source.
HRESULT MFPluginVpp::CreateInstance(REFIID iid,
                                     void** ppMFT,
                                     ClassRegData* pRegData)

{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    MFPluginVpp *pMFT = NULL;
    mf_tick tick = mf_get_tick();

    CHECK_POINTER(ppMFT, E_POINTER);
#if 0 // checking of supported platform
    if (!CheckPlatform())
    {
        *ppMFT = NULL;
        return E_FAIL;
    }
#endif

    SAFE_NEW(pMFT, MFPluginVpp(hr, pRegData));
    CHECK_POINTER(pMFT, E_OUTOFMEMORY);

    pMFT->SetStartTick(tick);
    if (SUCCEEDED(hr))
    {
        hr = pMFT->QueryInterface(iid, ppMFT);
    }
    if (FAILED(hr))
    {
        SAFE_DELETE(pMFT);
    }
    MFX_LTRACE_WS(MF_TL_PERF, (pRegData->pPluginName)? pRegData->pPluginName: L"null_plg_name");
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

#endif// MFX_D3D11_SUPPORT