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

File: mf_venc_plg.cpp

Purpose: define common code for MSDK based encoder MFTs.

*********************************************************************************/
#include "mf_venc_plg.h"
#include "mf_hw_platform.h"
#include "mf_private.h"

#include <comdef.h>
#include <comutil.h>
/*------------------------------------------------------------------------------*/

#undef  MFX_TRACE_CATEGORY
#define MFX_TRACE_CATEGORY _T("mfx_mft_unk")

#define DISABLE_VPP_ON_CONNECTION_STAGE
#define OMIT_VPP_SYNC_POINT

#undef ATLASSERT
#define ATLASSERT(a)

/*------------------------------------------------------------------------------*/

HRESULT CreateVEncPlugin(REFIID iid,
                         void** ppMFT,
                         ClassRegData* pRegistrationData)
{
    return MFPluginVEnc::CreateInstance(iid, ppMFT, pRegistrationData);
}

/*------------------------------------------------------------------------------*/

bool UseVPP(mfxInfoVPP* pVppInfo)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    bool bUseVPP = false;

    if (pVppInfo)
    {
        if (!bUseVPP && (pVppInfo->In.Width != pVppInfo->Out.Width)) bUseVPP = true;
        if (!bUseVPP && (pVppInfo->In.Height != pVppInfo->Out.Height)) bUseVPP = true;
        if (!bUseVPP && (pVppInfo->In.CropX != pVppInfo->Out.CropX)) bUseVPP = true;
        if (!bUseVPP && (pVppInfo->In.CropY != pVppInfo->Out.CropY)) bUseVPP = true;
        if (!bUseVPP && (pVppInfo->In.CropW != pVppInfo->Out.CropW)) bUseVPP = true;
        if (!bUseVPP && (pVppInfo->In.CropH != pVppInfo->Out.CropH)) bUseVPP = true;
        if (!bUseVPP && (pVppInfo->In.AspectRatioH != pVppInfo->Out.AspectRatioH)) bUseVPP = true;
        if (!bUseVPP && (pVppInfo->In.AspectRatioW != pVppInfo->Out.AspectRatioW)) bUseVPP = true;
        if (!bUseVPP && !mf_are_framerates_equal(pVppInfo->In.FrameRateExtN,
                                                 pVppInfo->In.FrameRateExtD,
                                                 pVppInfo->Out.FrameRateExtN,
                                                 pVppInfo->Out.FrameRateExtD))
        {
            bUseVPP = true;
        }
        if (!bUseVPP && (pVppInfo->In.PicStruct != pVppInfo->Out.PicStruct)) bUseVPP = true;
        if (!bUseVPP && (pVppInfo->In.FourCC != pVppInfo->Out.FourCC)) bUseVPP = true;
        if (!bUseVPP && (pVppInfo->In.ChromaFormat != pVppInfo->Out.ChromaFormat)) bUseVPP = true;
    }
    MFX_LTRACE_I(MF_TL_GENERAL, bUseVPP);
    return bUseVPP;
}

/*------------------------------------------------------------------------------*/

mfxU16 GetMinNumRefFrame(mfxU16 nTemporal, mfxU16 nLTR)
{
    // Specify number of reference frames depending on temporal layers count & LTR buffer size
    if (0 == nTemporal && nLTR > 0)
        nTemporal = 1; //TODO: 2 for B-frames?
    return nTemporal + nLTR;
}

/*------------------------------------------------------------------------------*/
// MFPluginVEnc class

#undef  MFX_TRACE_CATEGORY
#ifdef MF_SHORT_CAT_NAMES
    #define MFX_TRACE_CATEGORY _T("mfx_mft_enc")
#else
    #define MFX_TRACE_CATEGORY m_Reg.pPluginName
#endif

/*------------------------------------------------------------------------------*/

MFPluginVEnc::MFPluginVEnc(HRESULT &hr,
                           ClassRegData *pRegistrationData) :
    MFPlugin(hr, pRegistrationData),
    m_nRefCount(0),
    m_MSDK_impl(MF_MFX_IMPL),
    m_pMfxVideoSession(NULL),
    m_pInternalMfxVideoSession(NULL),
    m_pmfxENC(NULL),
    m_pmfxVPP(NULL),
    m_pInputInfo(NULL),
    m_pOutputInfo(NULL),
    m_iNumberInputSamplesBeforeVPP(0),
    m_iNumberInputSamples(0),
    m_iNumberOutputSamples(0),
    m_State(stHeaderNotSet),
    m_TypeDynamicallyChanged(false),
    m_LastSts(MFX_ERR_NONE),
    m_SyncOpSts(MFX_ERR_NONE),
    m_bInitialized(false),
    m_bDirectConnectionMFX(false),
    m_bConnectedWithVpp(false),
    m_bEndOfInput(false),
    m_bNeedInSurface(false),
    m_bNeedOutSurface(false),
    m_bStartDrain(false),
    m_bStartDrainEnc(false),
    m_bSendDrainComplete(false),
    m_bReinit(false),
    m_pFakeSample(NULL),
#if MFX_D3D11_SUPPORT
    m_uiInSurfacesMemType(MFX_IOPATTERN_IN_VIDEO_MEMORY), // VPP in surfaces
#else
    m_uiInSurfacesMemType(MFX_IOPATTERN_IN_SYSTEM_MEMORY), // VPP in surfaces
#endif
    m_nInSurfacesNum(MF_ADDITIONAL_IN_SURF_NUM),
    m_pInSurfaces(NULL),
    m_pInSurface(NULL),
    m_uiOutSurfacesMemType(MFX_IOPATTERN_OUT_VIDEO_MEMORY), // VPP out surfaces
    m_nOutSurfacesNum(MF_ADDITIONAL_OUT_SURF_NUM),
    m_pOutSurfaces(NULL),
    m_pOutSurface(NULL),
    m_DccState(dccNotRequired),
    m_bRejectProcessInputWhileDCC(false),
    m_bTypeChangeAfterEndOfStream(false),
    m_pSyncPoint(NULL),
    m_RateControlMode(&m_MfxParamsVideo),
    m_dbg_vppin(NULL),
    m_dbg_vppout(NULL),
    m_eLastMessage(),
    m_TemporalScalablity(this),
    m_Ltr(MFLongTermReferenceFactory::CreateLongTermReference(this))
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    memset(&m_VPPInitParams, 0, sizeof(mfxVideoParam));

    if (FAILED(hr)) return;
    SAFE_NEW(m_pWorkTicker, MFTicker(&m_ticks_WorkTime));
    SAFE_NEW(m_pCpuUsager, MFCpuUsager(&m_TimeTotal, &m_TimeKernel, &m_TimeUser, &m_CpuUsage));

    if (!m_Reg.pFillParams) hr = E_POINTER;
#if defined(_DEBUG) || (DBG_YES == USE_DBG_TOOLS)
    if (SUCCEEDED(hr))
    { // reading registry for some parameters
        TCHAR file_name[MAX_PATH] = {0};
        DWORD var = 1;

        if (S_OK == mf_get_reg_string(_T(REG_PARAMS_PATH),
                                      _T(REG_ENC_VPPIN_FILE), file_name, MAX_PATH))
        {
            m_dbg_vppin = mywfopen(file_name, L"wb");
        }
        if (S_OK == mf_get_reg_string(_T(REG_PARAMS_PATH),
                                      _T(REG_ENC_VPPOUT_FILE), file_name, MAX_PATH))
        {
            m_dbg_vppout = mywfopen(file_name, L"wb");
        }
        if (S_OK == mf_get_reg_string(_T(REG_PARAMS_PATH),
                                      _T(REG_ENC_OUT_FILE), file_name, MAX_PATH))
        {
            m_OutputBitstream.m_dbg_encout = mywfopen(file_name, L"wb");
        }
        if (S_OK == mf_get_reg_dword(_T(REG_PARAMS_PATH),
                                     _T(REG_ENC_MSDK), &var))
        {
            if (1 == var) m_dbg_MSDK = dbgSwMsdk;
            else if (2 == var) m_dbg_MSDK = dbgHwMsdk;
        }
        if (S_OK == mf_get_reg_dword(_T(REG_PARAMS_PATH),
                                     _T(REG_ENC_MEMORY), &var))
        {
            if (1 == var) m_dbg_Memory = dbgSwMemory;
            else if (2 == var) m_dbg_Memory = dbgHwMemory;
        }
        MFX_LTRACE_I(MF_TL_PERF, m_dbg_MSDK);
        MFX_LTRACE_I(MF_TL_PERF, m_dbg_Memory);
    }
#endif // #if defined(_DEBUG) || (DBG_YES == USE_DBG_TOOLS)

    if (SUCCEEDED(hr))
    {
        // internal media session initialization
#if MFX_D3D11_SUPPORT
        mfxStatus sts = InitMfxVideoSession(MFX_IMPL_VIA_D3D11);
#else
        mfxStatus sts = InitMfxVideoSession(MFX_IMPL_VIA_ANY);
#endif
        switch (sts)
        {
        case MFX_ERR_NONE:          hr = S_OK; break;
        case MFX_ERR_MEMORY_ALLOC:  hr = E_OUTOFMEMORY; break;
        default:                    hr = E_FAIL; break;
        }
    }

    if (SUCCEEDED(hr))
    {
        // initializing of the DXVA support
#if MFX_D3D11_SUPPORT
        hr = DXVASupportInit();
#else
        hr = DXVASupportInit(GetAdapterNum(&m_MSDK_impl));
#endif
        ATLASSERT(SUCCEEDED(hr));
    }
#if MFX_D3D11_SUPPORT //leave old SetHandle behavior for Win7
    if (SUCCEEDED(hr))
    {
        CComQIPtr<ID3D11Device> d3d11Device = GetD3D11Device();
        if (!d3d11Device)
        {
            MFX_LTRACE_S(MF_TL_GENERAL, "Failed to query ID3D11Device");
            hr = E_FAIL;
        }
        mfxStatus sts = m_pMfxVideoSession->SetHandle(MFX_HANDLE_D3D11_DEVICE, d3d11Device);
        if ((MFX_ERR_NONE != sts) && (m_dbg_no_SW_fallback || (MFX_WRN_PARTIAL_ACCELERATION != sts)))
        {
            MFX_LTRACE_1(MF_TL_GENERAL, "SetHandle failed, sts=", "%d", sts);
            ATLASSERT(MFX_ERR_NONE == sts);
            hr = E_FAIL;
        }
    }
#endif
    if (SUCCEEDED(hr))
    { // setting default encoder parameters
        mfxStatus sts = MFX_ERR_NONE;
        mfxVideoParam tmp_params;

        sts = m_Reg.pFillParams(&m_MfxParamsVideo);
        hr = SetAllDefaults();
        if (FAILED(hr)) return;

        memset(&m_VPPInitParams_original, 0, sizeof(mfxVideoParam));
        memset(&m_ENCInitInfo_original, 0, sizeof(mfxInfoMFX));
        m_AsyncDepth_original = 0;
        memset((void*)&m_VppAllocResponse, 0, sizeof(mfxFrameAllocResponse));
        memset((void*)&m_EncAllocResponse, 0, sizeof(mfxFrameAllocResponse));

        tmp_params.NumExtParam = sizeof(g_pVppExtBuf) / sizeof(g_pVppExtBuf[0]);
        tmp_params.ExtParam    = (mfxExtBuffer**)g_pVppExtBuf;
        sts = mf_copy_extparam(&tmp_params, &m_VPPInitParams);

        if (MFX_ERR_NONE != sts) hr = E_FAIL;
    }
    if (SUCCEEDED(hr))
    {
        // check HW caps

        //prepare in param for Query
        mfxVideoParam tempVideoParamIn = {0};
        MFExtBufCollection arrTempEncExtBufIn;
        mfxExtEncoderCapability *pExtEncoderCapabilityIn = NULL;
        if (SUCCEEDED(hr))
        {
            memset(&tempVideoParamIn, 0, sizeof(tempVideoParamIn));
            tempVideoParamIn.mfx.CodecId = m_MfxParamsVideo.mfx.CodecId;
            pExtEncoderCapabilityIn = arrTempEncExtBufIn.Create<mfxExtEncoderCapability>();
            if (NULL == pExtEncoderCapabilityIn)
            {
                hr = E_OUTOFMEMORY;
            }else
            {
                pExtEncoderCapabilityIn->reserved[0] = 0x667; //special mode do avoid extra CreateVideoDecoder call
            }
            arrTempEncExtBufIn.UpdateExtParam(tempVideoParamIn);
        }

        //prepare out param for Query
        mfxVideoParam tempVideoParamOut = {0};
        MFExtBufCollection arrTempEncExtBufOut;
        mfxExtEncoderCapability *pExtEncoderCapabilityOut = NULL;
        if (SUCCEEDED(hr))
        {
            memset(&tempVideoParamOut, 0, sizeof(tempVideoParamOut));
            tempVideoParamOut.mfx.CodecId = m_MfxParamsVideo.mfx.CodecId;
            pExtEncoderCapabilityOut = arrTempEncExtBufOut.Create<mfxExtEncoderCapability>();
            if (NULL == pExtEncoderCapabilityOut)
            {
                hr = E_OUTOFMEMORY;
            }
            arrTempEncExtBufOut.UpdateExtParam(tempVideoParamOut);
        }

        if (SUCCEEDED(hr))
        {
            mfxStatus sts = m_pmfxENC->Query(&tempVideoParamIn, &tempVideoParamOut);
            if ((MFX_ERR_NONE != sts) && (m_dbg_no_SW_fallback || (MFX_WRN_PARTIAL_ACCELERATION != sts)))
            {
                MFX_LTRACE_I(MF_TL_PERF, sts);
                hr = E_FAIL;
                ATLASSERT(MFX_ERR_NONE == sts);
            }
        }
    }
    if (SUCCEEDED(hr))
    {
        SAFE_NEW(m_pSyncPoint, mfxSyncPoint);
        if (!m_pSyncPoint) hr = E_OUTOFMEMORY;
    }
    //MFScalableVideoCoding *pSVC = NULL;
    //SAFE_NEW(pSVC, MFScalableVideoCoding(this));
    //m_pSVC = pSVC;

    //TODO: this is ugly. Need error reporting from MFEncoderParams::MFEncoderParam
    if (NULL == m_pNominalRange.get() || NULL == m_pSliceControl.get() || NULL == m_pCabac.get() || NULL == m_pMaxMBperSec.get())
    {
        ATLASSERT(NULL != m_pNominalRange.get());
        ATLASSERT(NULL != m_pSliceControl.get());
        ATLASSERT(NULL != m_pCabac.get());
        ATLASSERT(NULL != m_pMaxMBperSec.get());
        hr = E_OUTOFMEMORY;
    }

    GetPerformance();
    MFX_LTRACE_WS(MF_TL_PERF, (m_Reg.pPluginName)? m_Reg.pPluginName: L"null_plg_name");
    MFX_LTRACE_D(MF_TL_PERF, hr);
}

/*------------------------------------------------------------------------------*/

MFPluginVEnc::~MFPluginVEnc(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);

    MFX_LTRACE_I(MF_TL_PERF, m_iNumberInputSamplesBeforeVPP);
    MFX_LTRACE_I(MF_TL_PERF, m_iNumberInputSamples);
    MFX_LTRACE_I(MF_TL_PERF, m_iNumberOutputSamples);
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
    SAFE_RELEASE(m_pOutputType);
    // closing codec(s)
    CloseCodec();
    SAFE_DELETE(m_pmfxVPP);
    SAFE_DELETE(m_pmfxENC);
    SAFE_DELETE(m_pSyncPoint);
#if MFX_D3D11_SUPPORT
    // releasing frame allocator
    SAFE_RELEASE(m_pFrameAllocator);
#endif
    // internal session deinitialization
    SAFE_RELEASE(m_pMfxVideoSession);
    SAFE_RELEASE(m_pInternalMfxVideoSession);
#if !MFX_D3D11_SUPPORT
    // deinitalization of the DXVA support
    DXVASupportClose();
#endif
    mf_free_extparam(&m_VPPInitParams);
    if (m_Reg.pFreeParams) m_Reg.pFreeParams(&m_MfxParamsVideo);

    if (m_dbg_vppin)  fclose(m_dbg_vppin);
    if (m_dbg_vppout) fclose(m_dbg_vppout);
    if (m_OutputBitstream.m_dbg_encout) fclose(m_OutputBitstream.m_dbg_encout);
}

/*------------------------------------------------------------------------------*/
// IUnknown methods

ULONG MFPluginVEnc::AddRef(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MFX_LTRACE_I(MF_TL_GENERAL, m_nRefCount+1);
    return InterlockedIncrement(&m_nRefCount);
}

ULONG MFPluginVEnc::Release(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    ULONG uCount = InterlockedDecrement(&m_nRefCount);
    MFX_LTRACE_I(MF_TL_GENERAL, uCount);
    if (uCount == 0) delete this;
    // For thread safety, return a temporary variable.
    return uCount;
}

HRESULT MFPluginVEnc::QueryInterface(REFIID iid, void** ppv)
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
    else if (iid == IID_IConfigureMfxEncoder)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "IConfigureMfxEncoder");
        *ppv = static_cast<IConfigureMfxCodec*>(this);
    }
    else if (iid == IID_ICodecAPI)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "ICodecAPI");
        *ppv = static_cast<ICodecAPI*>(this);
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

HRESULT MFPluginVEnc::GetInputAvailableType(DWORD dwInputStreamID,
                                            DWORD dwTypeIndex,
                                            IMFMediaType** ppType)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);
    HRESULT       hr    = S_OK;
    CComPtr<IMFMediaType> pType;

    MFX_LTRACE_I(MF_TL_GENERAL, dwTypeIndex);

    GetPerformance();
    hr = MFPlugin::GetInputAvailableType(dwInputStreamID, dwTypeIndex, &pType);
#if MFX_D3D11_SUPPORT //TODO: remove #if
    if (SUCCEEDED(hr))
    {
        if (!m_pOutputType)
        {
            MFX_LTRACE_P(MF_TL_GENERAL, m_pOutputType);
            MFX_LTRACE_D(MF_TL_GENERAL, MF_E_TRANSFORM_TYPE_NOT_SET);
            return MF_E_TRANSFORM_TYPE_NOT_SET;
        }
    }
#endif
    if (SUCCEEDED(hr))
    {
        if (m_pInputType)
        { // if input type was set we may offer only this type
            hr = m_pInputType->CopyAllItems(pType.p);
        }
        else
        {
            // setting hint for acccepted type
            if (SUCCEEDED(hr)) hr = pType->SetGUID(MF_MT_MAJOR_TYPE, m_Reg.pInputTypes[dwTypeIndex].major_type);
            if (SUCCEEDED(hr)) hr = pType->SetGUID(MF_MT_SUBTYPE,    m_Reg.pInputTypes[dwTypeIndex].subtype);
            // if output type was set we may be more precise about acceptable types
#ifdef DISABLE_VPP_ON_CONNECTION_STAGE
            if (m_pOutputType)
            {
                UINT32 par1 = 0, par2 = 0;

                // Currently MSFT prohibits in some ways VPP behavior from encoder.
                // Potentially changeable parameters (VPP enabling):
                // - interlace mode (MF_MT_INTERLACE_MODE)
                // - frame rate (MF_MT_FRAME_RATE)
                // - width, height, aspect ratio (MF_MT_FRAME_SIZE, MF_MT_PIXEL_ASPECT_RATIO)
                if (SUCCEEDED(hr) && SUCCEEDED(MFGetAttributeSize(m_pOutputType, MF_MT_FRAME_SIZE, &par1, &par2)))
                    hr = MFSetAttributeSize(pType, MF_MT_FRAME_SIZE, par1, par2);
                if (SUCCEEDED(hr) && SUCCEEDED(MFGetAttributeRatio(m_pOutputType, MF_MT_PIXEL_ASPECT_RATIO, &par1, &par2)))
                    hr = MFSetAttributeRatio(pType, MF_MT_PIXEL_ASPECT_RATIO, par1, par2);
                if (SUCCEEDED(hr) && SUCCEEDED(m_pOutputType->GetUINT32(MF_MT_INTERLACE_MODE, &par1)))
                    hr = pType->SetUINT32(MF_MT_INTERLACE_MODE, par1);
                if (SUCCEEDED(hr) && SUCCEEDED(MFGetAttributeRatio(m_pOutputType, MF_MT_FRAME_RATE, &par1, &par2)))
                    hr = MFSetAttributeRatio(pType, MF_MT_FRAME_RATE, par1, par2);
                if (SUCCEEDED(hr) && SUCCEEDED(m_pOutputType->GetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, &par1)))
                    hr = pType->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, par1);
            }
#endif
        }
    }
    if (SUCCEEDED(hr))
    {
        pType.CopyTo(ppType);
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::GetOutputAvailableType(DWORD dwOutputStreamID,
                                             DWORD dwTypeIndex,
                                             IMFMediaType** ppType)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);
    HRESULT       hr    = S_OK;
    IMFMediaType* pType = NULL;

    MFX_LTRACE_I(MF_TL_GENERAL, dwTypeIndex);

    GetPerformance();
    hr = MFPlugin::GetOutputAvailableType(dwOutputStreamID, dwTypeIndex, &pType);
    if (SUCCEEDED(hr))
    {
        if (m_pOutputType)
        { // if output type was set we may offer only this type
            hr = m_pOutputType->CopyAllItems(pType);
        }
        else
        {
            // setting hint for acccepted type
            if (SUCCEEDED(hr)) hr = pType->SetGUID(MF_MT_MAJOR_TYPE, m_Reg.pOutputTypes[dwTypeIndex].major_type);
            if (SUCCEEDED(hr)) hr = pType->SetGUID(MF_MT_SUBTYPE,    m_Reg.pOutputTypes[dwTypeIndex].subtype);
#ifdef DISABLE_VPP_ON_CONNECTION_STAGE
            // if input type was set we may be more precise about acceptable types
            if (m_pInputType)
            {
                UINT32 par1 = 0, par2 = 0;

                // Currently MSFT prohibits in some ways VPP behavior from encoder.
                // Potentially changeable parameters (VPP enabling):
                // - interlace mode (MF_MT_INTERLACE_MODE)
                // - frame rate (MF_MT_FRAME_RATE)
                // - width, height, aspect ratio (MF_MT_FRAME_SIZE, MF_MT_PIXEL_ASPECT_RATIO)
                if (SUCCEEDED(hr) && SUCCEEDED(MFGetAttributeSize(m_pInputType, MF_MT_FRAME_SIZE, &par1, &par2)))
                    hr = MFSetAttributeSize(pType, MF_MT_FRAME_SIZE, par1, par2);
                if (SUCCEEDED(hr) && SUCCEEDED(MFGetAttributeRatio(m_pInputType, MF_MT_PIXEL_ASPECT_RATIO, &par1, &par2)))
                    hr = MFSetAttributeRatio(pType, MF_MT_PIXEL_ASPECT_RATIO, par1, par2);
                if (SUCCEEDED(hr) && SUCCEEDED(m_pInputType->GetUINT32(MF_MT_INTERLACE_MODE, &par1)))
                    hr = pType->SetUINT32(MF_MT_INTERLACE_MODE, par1);
                if (SUCCEEDED(hr) && SUCCEEDED(MFGetAttributeRatio(m_pInputType, MF_MT_FRAME_RATE, &par1, &par2)))
                    hr = MFSetAttributeRatio(pType, MF_MT_FRAME_RATE, par1, par2);
                if (SUCCEEDED(hr) && SUCCEEDED(m_pInputType->GetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, &par1)))
                    hr = pType->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, par1);
            }
#endif
        }
    }
    if (SUCCEEDED(hr))
    {
        TraceMediaType(MF_TL_GENERAL,pType);
        pType->AddRef();
        *ppType = pType;
    }
    SAFE_RELEASE(pType);
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::CheckInputMediaType(IMFMediaType* pType)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
#ifdef MF_ENABLE_PICTURE_COMPLEXITY_LIMITS
    GUID dec_subtype = GUID_NULL;

    if (SUCCEEDED(pType->GetGUID(MF_MT_DEC_SUBTYPE, &dec_subtype)))
    {
        // some code to restrict usage of the encoder plug-in depending on
        // type of input stream
    }
#else
    pType;
#endif
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

// Checks if data provided in pType could be decoded
bool MFPluginVEnc::CheckHwSupport(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    bool ret_val = MFPlugin::CheckHwSupport();
    mfxStatus sts = MFX_ERR_NONE;
    mfxVideoParam inVideoParams = {0}, outVideoParams = {0};

    if (ret_val && m_pInputType && m_pOutputType)
    { // checking VPP
        memcpy_s(&inVideoParams, sizeof(inVideoParams), &m_VPPInitParams, sizeof(m_VPPInitParams));
        memset(&outVideoParams, 0, sizeof(mfxVideoParam));
        sts = mf_copy_extparam(&inVideoParams, &outVideoParams);
        if (MFX_ERR_NONE != sts) ret_val = false;
        else
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
            inVideoParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;

            if (UseVPP(&(inVideoParams.vpp)))
            {
                //VPP inside of Encoder plugin is not allowed for Win8
#if MFX_D3D11_SUPPORT
                MFX_LTRACE_S(MF_TL_NOTES, "VPP is required");
                ret_val = false;
#else
                if (!m_pmfxVPP)
                {
                    MFXVideoSession* session = (MFXVideoSession*)m_pMfxVideoSession;

                    SAFE_NEW(m_pmfxVPP, MFXVideoVPPEx(*session));
                    if (!m_pmfxVPP) ret_val = false;
                }
                if (ret_val)
                {
                    sts = m_pmfxVPP->Query(&inVideoParams, &outVideoParams);
                    if ((MFX_ERR_NONE != sts) && (m_dbg_no_SW_fallback || (MFX_WRN_PARTIAL_ACCELERATION != sts)))
                        ret_val = false;
                }
#endif
            }
        }
        sts = mf_free_extparam(&outVideoParams);
        if (ret_val && (MFX_ERR_NONE != sts)) ret_val = false;
    }
    if (ret_val && m_pOutputType)
    { // checking Encoder
        memcpy_s(&inVideoParams, sizeof(inVideoParams), &m_MfxParamsVideo, sizeof(m_MfxParamsVideo));
        memset(&outVideoParams, 0, sizeof(mfxVideoParam));
        sts = mf_copy_extparam(&inVideoParams, &outVideoParams);
        if (MFX_ERR_NONE != sts) ret_val = false;
        else
        {
            if (!(inVideoParams.mfx.FrameInfo.FourCC) || !(inVideoParams.mfx.FrameInfo.ChromaFormat))
            {
                inVideoParams.mfx.FrameInfo.FourCC       = MAKEFOURCC('N','V','1','2');
                inVideoParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            }
            if (MFX_ERR_NONE == sts)
            {
                sts = QueryENC(&inVideoParams, &outVideoParams);
                MFX_LTRACE_I(MF_TL_GENERAL, sts);
                if ((MFX_ERR_NONE != sts) && (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM != sts) && (m_dbg_no_SW_fallback || (MFX_WRN_PARTIAL_ACCELERATION != sts)))
                {
                    ATLASSERT(MFX_ERR_NONE == sts);
                    ret_val = false;
                }
            }
        }
        sts = mf_free_extparam(&outVideoParams);
        if (ret_val && (MFX_ERR_NONE != sts)) ret_val = false;
    }
    MFX_LTRACE_I(MF_TL_GENERAL, ret_val);
    return ret_val;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::SetInputType(DWORD         dwInputStreamID,
                                   IMFMediaType* pType,
                                   DWORD         dwFlags)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);
    HRESULT hr  = S_OK;

    MFX_LTRACE_P(MF_TL_PERF, pType);

    //checking for locked MFT firstly
    hr = MFPlugin::SetInputType(dwInputStreamID, pType, dwFlags);
    if (FAILED(hr))
    {
        MFX_LTRACE_S(MF_TL_PERF, "MFPlugin::SetInputType failed");
        MFX_LTRACE_D(MF_TL_PERF, hr);
    }
    CHECK_EXPRESSION(SUCCEEDED(hr), hr);

    if (0 != (dwFlags & MFT_SET_TYPE_TEST_ONLY))
    {
        MFX_LTRACE_I(MF_TL_GENERAL, (dwFlags & MFT_SET_TYPE_TEST_ONLY));
    }
    TraceMediaType(MF_TL_GENERAL, pType);

    GetPerformance();
    if (!pType)
    { // resetting media type
        ReleaseMediaType(m_pInputType);
        MFX_LTRACE_P(MF_TL_PERF, pType);
        MFX_LTRACE_D(MF_TL_PERF, S_OK);
    }
    CHECK_POINTER(pType, S_OK);

#if MFX_D3D11_SUPPORT
    // Accept input type only in case of output is set. MSFT vision:
    // With encoders, the typical pipeline behavior is to negotiate the output type first (this is driven by the encoder returning MF_E_TRANSFORM_TYPE_NOT_SET from SetInputType as you noticed).
    // Once the output type is set the encoder, its expected to expose complete types from GetInputAvailableType() as that is what is used to configure the video processor upstream.
    // In other words, in capture scenario (actually in SinkWriter scenarios since that is where this logic lives), its not possible for encoder to expose partial types from GetInputAvailableType().
    // Moreover, this would likely be meaningless since negotiation would simply fail later on. Specifically, even if the encoder accepted an arbitrary input type from upstream
    // (lets say the colorspace format matched something that encoder likes), it would then see a call to SetOutputType() where the resolution may not match whats set on input.
    // Since the encoder is not supposed to do scaling, it would reject the output type and the negotiation would fail.
    if (!m_pOutputType)
    {
        MFX_LTRACE_S(MF_TL_PERF, "!m_pOutputType. Accept input type only in case of output is set");
        MFX_LTRACE_D(MF_TL_PERF, MF_E_TRANSFORM_TYPE_NOT_SET);
        return MF_E_TRANSFORM_TYPE_NOT_SET;
    }
#endif

    // Validate the type: general checking, input type checking
    if (SUCCEEDED(hr)) hr = CheckMediaType(pType, m_Reg.pInputTypes, m_Reg.cInputTypes, &m_pInputInfo);
    bool bFormatChange = false;
    if (SUCCEEDED(hr))
    {
        if (IsVppNeeded(pType, m_pOutputType))
        {
            MFX_LTRACE_S(MF_TL_GENERAL, "Proposed InputType Mismatches with current OutputType:");
            TraceMediaType(MF_TL_GENERAL, m_pOutputType);

            if (!m_TemporalScalablity.IsEnabled()) //allow dynamic format change for Lync only
            {
                // dynamic change of input type is supported for Lync mode only
                CHECK_EXPRESSION(!m_bInitialized, MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING);
            }

            if (!m_bStreamingStarted) //  Need to reject it
            {
                MFX_LTRACE_S(MF_TL_GENERAL, "Cannot change type during format negotiation stage");
                hr = MF_E_INVALIDMEDIATYPE;
            }
            else
            {
                if (IsInvalidResolutionChange(pType, m_pOutputType))
                {
                    MFX_LTRACE_S(MF_TL_GENERAL, "Only resolution and framerate can be changed dynamically");
                    hr = MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING;
                }
                else
                {
                    if (0 == (dwFlags & MFT_SET_TYPE_TEST_ONLY))
                    {
                        UINT32 par1 = 0, par2 = 0;
                        if (SUCCEEDED(hr) && SUCCEEDED(MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &par1, &par2)))
                        {
                            hr = MFSetAttributeSize(m_pOutputType, MF_MT_FRAME_SIZE, par1, par2);
                            MFX_LTRACE_S(MF_TL_GENERAL, "current Output FRAME_SIZE updated");
                        }
                        if (SUCCEEDED(hr) && SUCCEEDED(MFGetAttributeSize(pType, MF_MT_FRAME_RATE, &par1, &par2)))
                        {
                            hr = MFSetAttributeSize(m_pOutputType, MF_MT_FRAME_RATE, par1, par2);
                            MFX_LTRACE_S(MF_TL_GENERAL, "current Output FRAME_RATE updated");
                        }
                        MFVideoArea area={};
                        if (SUCCEEDED(pType->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)&area, sizeof(area), NULL)))
                        {
                            hr = m_pOutputType->SetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)&area, sizeof(area));
                            MFX_LTRACE_S(MF_TL_GENERAL, "current Output MINIMUM_DISPLAY_APERTURE updated");
                        }
                        bFormatChange = true;
                    }
                }
            }
        }
    }
    if (0 == (dwFlags & MFT_SET_TYPE_TEST_ONLY))
    {
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

            // Really set the type, unless the caller was just testing.
            ReleaseMediaType(m_pInputType);
            pType->AddRef();
            m_pInputType = pType;

            hr = m_pInputType->SetUnknown(MF_MT_D3D_DEVICE, pUnk);
        }
        if (SUCCEEDED(hr)) hr = mf_mftype2mfx_frame_info(m_pInputType, &(m_VPPInitParams_original.vpp.In));
        if (SUCCEEDED(hr)) hr = SetType(m_pInputType, true); //analyze MF_MT_VIDEO_NOMINAL_RANGE, etc
        if (SUCCEEDED(hr))
        {
            memcpy_s(&(m_VPPInitParams.vpp.In), sizeof(m_VPPInitParams.vpp.In),
                     &(m_VPPInitParams_original.vpp.In), sizeof(m_VPPInitParams_original.vpp.In));
            m_OutputBitstream.SetFramerate(m_VPPInitParams.vpp.In.FrameRateExtN, m_VPPInitParams.vpp.In.FrameRateExtD);

            hr = mf_align_geometry(&(m_VPPInitParams.vpp.In));

            if (m_pOutputType)
            {
                // setting cropping parameters (letter & pillar boxing)
                mf_set_cropping(&(m_VPPInitParams.vpp));
                // re-synchronizing VPP and ENC parameters
                memcpy_s(&(m_MfxParamsVideo.mfx.FrameInfo), sizeof(m_MfxParamsVideo.mfx.FrameInfo),
                         &(m_VPPInitParams.vpp.Out), sizeof(mfxFrameInfo));
                memcpy_s(&(m_ENCInitInfo_original), sizeof(m_ENCInitInfo_original),
                         &(m_MfxParamsVideo.mfx), sizeof(m_MfxParamsVideo.mfx));
                m_AsyncDepth_original = m_MfxParamsVideo.AsyncDepth;
                //TODO: Protected, IOPattern, ExtParam, NumExtParam

                if (bFormatChange)
                {
                    MFX_LTRACE_S(MF_TL_PERF, "calling RequireDynamicConfigurationChange");
                    hr = RequireDynamicConfigurationChange(dccTypeChangeRequired);
                    if (FAILED(hr))
                    {
                        ATLASSERT(SUCCEEDED(hr));
                        hr = MF_E_INVALIDMEDIATYPE;
                    }
                }
            }
        }
        if (!bFormatChange && !(m_bInitialized && stHeaderNotSet == m_State) && !m_TypeDynamicallyChanged)
        {
            if (SUCCEEDED(hr) && !CheckHwSupport())
            {
                MFX_LTRACE_S(MF_TL_GENERAL, "CheckHwSupport() failed");
                hr = MF_E_INVALIDMEDIATYPE;
            }
        }
        if (FAILED(hr))
        {
            ReleaseMediaType(m_pInputType);
        }
    }
    MFX_LTRACE_P(MF_TL_PERF, m_pInputType);
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::SetOutputType(DWORD         dwOutputStreamID,
                                    IMFMediaType* pType,
                                    DWORD         dwFlags)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = S_OK;

    MFX_LTRACE_P(MF_TL_PERF, pType);


    if (0 != (dwFlags & MFT_SET_TYPE_TEST_ONLY))
    {
        MFX_LTRACE_I(MF_TL_GENERAL, (dwFlags & MFT_SET_TYPE_TEST_ONLY));
    }
    TraceMediaType(MF_TL_GENERAL, pType);


    GetPerformance();
    hr = MFPlugin::SetOutputType(dwOutputStreamID, pType, dwFlags);
    if (FAILED(hr))
    {
        MFX_LTRACE_S(MF_TL_PERF, "MFPlugin::SetOutputType failed");
        MFX_LTRACE_D(MF_TL_PERF, hr);
    }
    CHECK_EXPRESSION(SUCCEEDED(hr), hr);

    if (!pType)
    { // resetting media type
        SAFE_RELEASE(m_pOutputType);
        MFX_LTRACE_P(MF_TL_PERF, pType);
        MFX_LTRACE_D(MF_TL_PERF, S_OK);
    }
    CHECK_POINTER(pType, S_OK);
    // Validate the type: general checking, output type checking
    if (SUCCEEDED(hr)) hr = CheckMediaType(pType, m_Reg.pOutputTypes, m_Reg.cOutputTypes, &m_pOutputInfo);

    //TODO: It is probably worth to do the same on SetInputType
    if (m_bTypeChangeAfterEndOfStream)
    {
        m_bRejectProcessInputWhileDCC = false;

        // TODO: remove copy-paste (also in PerformDynamicConfigurationChange)
        // CloseCodec overwrites current encoder settings with m_ENCInitInfo_original
        // let's update them before CloseCodec call
        memcpy_s(&(m_ENCInitInfo_original), sizeof(m_ENCInitInfo_original),
                    &(m_MfxParamsVideo.mfx), sizeof(m_MfxParamsVideo.mfx));
        m_AsyncDepth_original = m_MfxParamsVideo.AsyncDepth;
        CloseCodec();
        SAFE_RELEASE(m_pInputType);
        SAFE_RELEASE(m_pOutputType);
        m_bTypeChangeAfterEndOfStream = false;
    }

    bool bRequireResolutionChange = false;
    ATLASSERT(SUCCEEDED(hr));
#ifdef DISABLE_VPP_ON_CONNECTION_STAGE
    if (SUCCEEDED(hr)) hr = (IsVppNeeded(pType, m_pOutputType))? E_FAIL: S_OK;
    if (FAILED(hr))
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "New output type mismatches current output type:");
        TraceMediaType(MF_TL_GENERAL, m_pOutputType);
    }
#endif
    if (SUCCEEDED(hr))
    {
        if (m_TypeDynamicallyChanged)
        {
            m_TypeDynamicallyChanged = false;
            MFX_LTRACE_I(MF_TL_NOTES, m_TypeDynamicallyChanged );
            bRequireResolutionChange = true;
        }
    }
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
            if (stHeaderSetAwaiting == m_State)
            {
                MFX_LTRACE_S(MF_TL_GENERAL, "Second set of type (header)");
                ATLASSERT(NULL != m_pOutputType); // should not get there if HMFT is used properly
                if (SUCCEEDED(hr) && m_pOutputType != pType && NULL != m_pOutputType)
                {
                    hr = m_pOutputType->CopyAllItems(pType);
                }
                if (SUCCEEDED(hr))
                {
                    hr = pType->SetBlob(MF_MT_MPEG_SEQUENCE_HEADER,
                                        m_OutputBitstream.m_pHeader, m_OutputBitstream.m_uiHeaderSize);
                }
                if (SUCCEEDED(hr))
                {
                    SAFE_RELEASE(m_pOutputType);
                    pType->AddRef();
                    m_pOutputType = pType;
                }
                if (SUCCEEDED(hr))
                {
                    m_State = stHeaderSampleNotSent;
                    MFX_LTRACE_I(MF_TL_NOTES, m_State);
                }

                MFX_LTRACE_S(MF_TL_NOTES, "calling SendOutput");
                HRESULT hr_sts = SendOutput();
                if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
            }
            else
            {
                MFX_LTRACE_S(MF_TL_GENERAL, "First set of type");
                if (SUCCEEDED(hr))
                {
                    SAFE_RELEASE(m_pOutputType);
                    pType->AddRef();
                    m_pOutputType = pType;
                }
                if (SUCCEEDED(hr)) hr = mf_mftype2mfx_info(m_pOutputType, &(m_MfxParamsVideo.mfx));
                m_pCabac->SetCodecProfile(m_MfxParamsVideo.mfx.CodecProfile);
                HandleNewMessage(mtRequireUpdateMaxMBperSec, this); // ignore return value

                if (SUCCEEDED(hr)) hr = SetType(m_pOutputType, false); //analyze MF_MT_VIDEO_NOMINAL_RANGE, etc
                if (SUCCEEDED(hr))
                {
                    memcpy_s(&(m_VPPInitParams_original.vpp.Out), sizeof(m_VPPInitParams_original.vpp.Out),
                             &(m_MfxParamsVideo.mfx.FrameInfo),sizeof(m_MfxParamsVideo.mfx.FrameInfo));
                    hr = mf_align_geometry(&(m_MfxParamsVideo.mfx.FrameInfo));
                    // synchronizing VPP and ENC parameters
                    memcpy_s(&(m_VPPInitParams.vpp.Out), sizeof(m_VPPInitParams.vpp.Out),
                             &(m_MfxParamsVideo.mfx.FrameInfo), sizeof(m_MfxParamsVideo.mfx.FrameInfo));

                    if (m_pInputType)
                    {
                        // setting cropping parameters (letter & pillar boxing)
                        mf_set_cropping(&(m_VPPInitParams.vpp));
                        // re-synchronizing VPP and ENC parameters
                        memcpy_s(&(m_MfxParamsVideo.mfx.FrameInfo), sizeof(m_MfxParamsVideo.mfx.FrameInfo),
                                 &(m_VPPInitParams.vpp.Out), sizeof(m_VPPInitParams.vpp.Out));
                        memcpy_s(&(m_ENCInitInfo_original), sizeof(m_ENCInitInfo_original),
                                 &(m_MfxParamsVideo.mfx), sizeof(m_MfxParamsVideo.mfx));
                        m_AsyncDepth_original = m_MfxParamsVideo.AsyncDepth;
                        //TODO: Protected, IOPattern, ExtParam, NumExtParam
                        if (bRequireResolutionChange)
                        {
                            hr = PerformDynamicConfigurationChange();
                        }
                    }
                }
                if (!bRequireResolutionChange)
                {
                    if (SUCCEEDED(hr) && !CheckHwSupport())
                    {
                        MFX_LTRACE_S(MF_TL_GENERAL, "CheckHwSupport() failed");
                        ATLASSERT(false);
                        hr = MF_E_INVALIDMEDIATYPE;
                    }
                }
                if (FAILED(hr))
                {
                    SAFE_RELEASE(m_pOutputType);
                }
            }
        }
    }
    MFX_LTRACE_P(MF_TL_PERF, m_pOutputType);
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::ProcessMessage(MFT_MESSAGE_TYPE eMessage,
                                     ULONG_PTR        ulParam)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = S_OK;

    GetPerformance();
    hr = MFPlugin::ProcessMessage(eMessage, ulParam);
    CHECK_EXPRESSION(SUCCEEDED(hr), hr);
    m_eLastMessage = eMessage;
    switch (eMessage)
    {
    case MFT_MESSAGE_COMMAND_FLUSH:
        MFX_LTRACE_S(MF_TL_PERF, "MFT_MESSAGE_COMMAND_FLUSH");
        if (MFX_ERR_NONE != ResetCodec()) hr = hr;//E_FAIL;
        break;

    case MFT_MESSAGE_COMMAND_DRAIN:
        MFX_LTRACE_S(MF_TL_PERF, "MFT_MESSAGE_COMMAND_DRAIN");
        if (m_pmfxENC)
        { // processing eos only if we were initialized
            m_bEndOfInput = true;
            m_bSendDrainComplete = true;
            // we can't accept any input if draining has begun
            m_bDoNotRequestInput = true; // prevent calling RequestInput
            m_NeedInputEventInfo.m_requested = 0; // prevent processing of sent RequestInput
            if (!m_bReinit) AsyncThreadPush();
#if MFX_D3D11_SUPPORT //TODO: review
            if (FAILED(m_hrError))
            {
                m_pAsyncThreadEvent->Signal();
            }
#endif
        }
        break;

    case MFT_MESSAGE_COMMAND_MARKER:
        MFX_LTRACE_S(MF_TL_PERF, "MFT_MESSAGE_COMMAND_MARKER");
        // we always produce as much data as possible
        if (m_pmfxENC) hr = SendMarker(ulParam);
        break;

    case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
        MFX_LTRACE_S(MF_TL_PERF, "MFT_MESSAGE_NOTIFY_BEGIN_STREAMING");
        m_bStreamingStarted = true;
#ifdef INITENC_ON_BEGINSTREAMING
        if (SUCCEEDED(hr) && !m_bInitialized)
        {
            mfxStatus sts = InitCodec();
            if (MFX_ERR_NONE != sts)
            {
                hr = E_FAIL;
                ATLASSERT(SUCCEEDED(hr));
            }
        }
#endif //#if INITENC_ON_BEGINSTREAMING
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
        else if (m_bInitialized)
        {
            if (MFX_ERR_NONE != ResetCodec())
            {
                hr = E_FAIL;
                ATLASSERT(SUCCEEDED(hr));
            }
        }
        m_bStreamingStarted  = true;
        m_bDoNotRequestInput = false;
        if (SUCCEEDED(hr)) hr = RequestInput();
        break;

    case MFT_MESSAGE_NOTIFY_END_OF_STREAM:
        MFX_LTRACE_S(MF_TL_PERF, "MFT_MESSAGE_NOTIFY_END_OF_STREAM");
        // stream ends, we must not accept input data
        m_bDoNotRequestInput = true; // prevent calling RequestInput
        m_NeedInputEventInfo.m_requested = 0; // prevent processing of sent RequestInput
        m_bTypeChangeAfterEndOfStream = true;
        break;

    case MFT_MESSAGE_SET_D3D_MANAGER:
        MFX_LTRACE_1(MF_TL_PERF, "MFT_MESSAGE_SET_D3D_MANAGER, ulParam=", MFX_TRACE_FORMAT_P, ulParam);
#if MFX_D3D11_SUPPORT
        IUnknown *pUnknown = reinterpret_cast<IUnknown *>(ulParam);
        CComQIPtr<IMFDXGIDeviceManager> pDXGIDeviceManager (pUnknown);
        MFX_LTRACE_P(MF_TL_GENERAL, pDXGIDeviceManager);
        if (!pDXGIDeviceManager)
        {
        //TODO: does not satsify requirements http://msdn.microsoft.com/en-us/library/windows/desktop/ee663585(v=vs.85).aspx
        //"Alternatively, if the client does not find an acceptable DXVA format, the client may send another
        //MFT_MESSAGE_SET_D3D_MANAGER message, this time setting ulParam to NULL. The MFT must release the
        //IDirect3DDeviceManager9 pointer (the IMFDXGIDeviceManager pointer, if IMFDXGIDeviceManager was used) and any
        //other DXVA interfaces, and revert to software processing. At this point, the MFT must not use DXVA processing."
            break;//not a d3d11
        }

        HANDLE d3d11deviceHandle = 0;
        pDXGIDeviceManager->OpenDeviceHandle(&d3d11deviceHandle);
        MFX_LTRACE_D(MF_TL_GENERAL, d3d11deviceHandle);
        CComPtr<ID3D11Device> pOldD3D11Device = m_pD3D11Device;
        SAFE_RELEASE(m_pD3D11Device);
        pDXGIDeviceManager->GetVideoService(d3d11deviceHandle,  IID_ID3D11Device,  (void**)&m_pD3D11Device);
        MFX_LTRACE_P(MF_TL_GENERAL, m_pD3D11Device);

        if (NULL != m_pD3D11Device)
        {
            if (m_pD3D11Device != pOldD3D11Device)
            {
                if (m_bInitialized)
                {
                    //dynamic change of device is not supported
                    ATLASSERT(!m_bInitialized);
                    hr = E_FAIL;
                    break;
                }
                else
                {
                    CComQIPtr<ID3D10Multithread> pD3D10Multithread(m_pD3D11Device);
                    if (pD3D10Multithread)
                    {
                        if (!pD3D10Multithread->GetMultithreadProtected())
                        {
                            //otherwise SetHandle will fail
                            pD3D10Multithread->SetMultithreadProtected(true);
                        }
                    }

                    mfxStatus sts = m_pMfxVideoSession->SetHandle(MFX_HANDLE_D3D11_DEVICE, m_pD3D11Device);
                    if (MFX_ERR_UNDEFINED_BEHAVIOR == sts)
                    {
                        // MediaSDK starting from API verions 1.6 requires SetHandle to be called
                        // right after MFXInit before any Query / QueryIOSurf calls, otherwise it
                        // creates own and returns UNDEFINED_BEHAVIOR if application tries to set handle
                        // afterwards.  Need to reinit MediaSDK session to set proper handle.
                        sts = CloseInitMfxVideoSession(MFX_IMPL_VIA_D3D11);
                        if (MFX_ERR_NONE == sts)
                        {
                            sts = m_pMfxVideoSession->SetHandle(MFX_HANDLE_D3D11_DEVICE, m_pD3D11Device);
                            if (MFX_ERR_NONE != sts)
                            {
                                MFX_LTRACE_1(MF_TL_GENERAL, "SetHandle failed, sts=", "%d", sts);
                                ATLASSERT(MFX_ERR_NONE == sts);
                            }
                        }
                        if (MFX_ERR_NONE == sts && !CheckHwSupport())
                        {
                            //Query fails after changing the device.
                            ATLASSERT(false);
                            sts = MFX_ERR_UNKNOWN;
                        }
                    }
                    if (MFX_ERR_NONE != sts)
                    {
                        hr = E_FAIL;
                        ATLASSERT(MFX_ERR_NONE == sts);
                    }
                }
            }
        }
#else
        if (ulParam) hr = E_NOTIMPL; // this return status is an exception (see MS docs)
#endif

        if (SUCCEEDED(hr))
        {
            HandleNewMessage(mtRequireUpdateMaxMBperSec, this); // ignore return value
        }

        break;
    }
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/
// ICodecAPI methods

HRESULT MFPluginVEnc::IsSupported(const GUID *Api)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = MFEncoderParams::IsSupported(Api);
    if (FAILED(hr) )
    {
        CHECK_POINTER(Api, E_POINTER);
        if ((CODECAPI_AVEncVideoTemporalLayerCount == *Api) ||
            (CODECAPI_AVEncVideoSelectLayer == *Api) ||
            (CODECAPI_AVEncVideoEncodeQP == *Api) ||
            (CODECAPI_AVEncVideoForceKeyFrame == *Api) ||
            (CODECAPI_AVEncCommonLowLatency == *Api || CODECAPI_AVLowLatencyMode == *Api) ||
            (CODECAPI_AVEncCommonRateControlMode == *Api) ||
            (CODECAPI_AVEncCommonMeanBitRate == *Api) ||
            (CODECAPI_AVEncCommonMaxBitRate == *Api) ||
            (CODECAPI_AVEncCommonBufferSize == *Api) ||
            (CODECAPI_AVEncMPVGOPSize == *Api) ||
            (CODECAPI_AVEncCommonQuality == *Api) ||
            (CODECAPI_AVEncCommonQualityVsSpeed == *Api) ||
            //(CODECAPI_AVEncVideoEncodeFrameTypeQP == *Api) ||
            (CODECAPI_AVEncSliceControlMode == *Api) ||
            (CODECAPI_AVEncSliceControlSize == *Api) ||
            (CODECAPI_AVEncVideoMaxNumRefFrame == *Api) ||
            //(CODECAPI_AVEncVideoMeanAbsoluteDifference == *Api) ||
            //(CODECAPI_AVEncVideoMaxQP == *Api) ||
            (CODECAPI_AVEncMPVDefaultBPictureCount == *Api && HWPlatform::Type() != HWPlatform::VLV) ||
            (CODECAPI_AVEncVideoLTRBufferControl == *Api) ||
            (CODECAPI_AVEncVideoMarkLTRFrame == *Api) ||
            (CODECAPI_AVEncVideoUseLTRFrame == *Api) ||
            (CODECAPI_AVEncH264CABACEnable == *Api))
        {
            hr = S_OK;
        }
        else
        {
            hr = E_NOTIMPL;
        }
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::IsModifiable(const GUID* Api)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = MFEncoderParams::IsModifiable(Api);
    if (FAILED(hr))
    {
        hr = E_INVALIDARG;
        CHECK_POINTER(Api, E_POINTER);
        if (CODECAPI_AVEncMPVProfile == *Api)
        {
            hr = S_OK;
        }
        else if (CODECAPI_AVEncMPVLevel == *Api)
        {
            if (m_bInitialized) // Dynamic change is not allowed
                hr = S_FALSE;
            else
                hr = S_OK;
        }
        else if (CODECAPI_AVEncVideoTemporalLayerCount == *Api)
        {
            hr = S_FALSE;
            if (MFX_CODEC_AVC == m_MfxParamsVideo.mfx.CodecId)
            {
                if (m_bInitialized) // Dynamic change is allowed only for values > 0 (Lync mode)
                {
                    //TODO: change for WinBlue
                    if (m_TemporalScalablity.IsEnabled())
                    {
                        hr = S_OK; // When started with 1..MAX_TEMPORAL_LAYERS_COUNT, can be dynamically changed within that interval
                    }
                    else
                    {
                        hr = S_FALSE; // Temporal scalability cannot be turned on dynamically
                    }
                }
                else
                    hr = S_OK;
            }
        }
        else if (CODECAPI_AVEncVideoSelectLayer == *Api)
        {
            hr = S_OK;
        }
        else if (CODECAPI_AVEncVideoEncodeQP == *Api)
        {
            hr = S_FALSE;
            if ( m_RateControlMode.IsApplicable(Api) )
            {
                hr = S_OK;
            }
        }
        else if (CODECAPI_AVEncVideoForceKeyFrame == *Api)
        {
            hr = S_OK;
        }
        else if (CODECAPI_AVEncCommonLowLatency == *Api || CODECAPI_AVLowLatencyMode == *Api)
        {
            if (m_bInitialized) // Dynamic change is not allowed
                hr = S_FALSE;
            else
            {
                //TODO: examine other properties to find out which of them can interact with this one
                //and add them to IsModifiable
                if ( m_TemporalScalablity.IsEnabled())
                {
                    //MSFT doesn't plan to use CODECAPI_AVEncCommonLowLatency VARIANT_FALSE for Lync
                    ATLASSERT(false);
                    hr = S_FALSE;
                }
                else
                {
                    hr = S_OK;
                }
            }
        }
        else if (CODECAPI_AVEncCommonRateControlMode == *Api)
        {
            if (m_bInitialized) // Dynamic change is not allowed
                hr = S_FALSE;
            else
                hr = S_OK;
        }
        else if (CODECAPI_AVEncCommonMeanBitRate == *Api)
        {
            hr = S_FALSE;
            if ( m_RateControlMode.IsApplicable(Api) )
            {
                hr = S_OK;
                if ( eAVEncCommonRateControlMode_CBR == m_RateControlMode.GetMode() &&
                    m_bInitialized && !m_TemporalScalablity.IsEnabled() )
                {
                    hr = S_FALSE; // dynamic change in CBR mode is allowed for Lync only
                }
            }
        }
        else if (CODECAPI_AVEncCommonMaxBitRate == *Api)
        {
            hr = S_FALSE;
            if ( m_RateControlMode.IsApplicable(Api) )
            {
                hr = S_OK;
                //no need to check for CBR & Dyn. change & Lync mode.
                //IsApplicable returns true for PeakConstrainedVBR only
            }
        }
        else if(CODECAPI_AVEncCommonBufferSize == *Api)
        {
            hr = S_FALSE;
            if ( m_RateControlMode.IsApplicable(Api) )
            {
                hr = S_OK;
                if ( eAVEncCommonRateControlMode_CBR == m_RateControlMode.GetMode() &&
                    m_bInitialized && !m_TemporalScalablity.IsEnabled() )
                {
                    hr = S_FALSE; // dynamic change in CBR mode is allowed for Lync only
                }
            }
        }
        else if (CODECAPI_AVEncMPVGOPSize == *Api)
        {
            if (m_bInitialized) // Dynamic change is not allowed
                hr = S_FALSE;
            else
                hr = S_OK;
        }
        else if (CODECAPI_AVEncCommonQuality == *Api)
        {
            hr = S_FALSE;
            if (!m_bInitialized) // Dynamic change is not allowed
            {
                if ( m_RateControlMode.IsApplicable(Api) )
                {
                    hr = S_OK;
                }
            }
        }
        else if (CODECAPI_AVEncCommonQualityVsSpeed == *Api)
        {
            if (m_bInitialized) // Dynamic change is not allowed
                hr = S_FALSE;
            else
                hr = S_OK;
        }

        else if (CODECAPI_AVEncMPVDefaultBPictureCount == *Api)
        {
            if (m_bInitialized) // Dynamic change is not allowed
                hr = S_FALSE;
            else
                hr = S_OK;
        }
        //else if (CODECAPI_AVEncVideoEncodeFrameTypeQP == *Api)
        else if (CODECAPI_AVEncSliceControlMode == *Api)
        {
            if (m_bInitialized) // Dynamic change is not allowed
                hr = S_FALSE;
            else
                hr = S_OK;
        }
        else if (CODECAPI_AVEncSliceControlSize == *Api)
        {
            if (m_pSliceControl->IsEnabled())
                hr = S_OK;
            else
                hr = S_FALSE;
        }
        else if (CODECAPI_AVEncVideoMaxNumRefFrame == *Api)
        {
            if (m_bInitialized) // Dynamic change is not allowed
                hr = S_FALSE;
            else
                hr = S_OK;
        }
        //else if (CODECAPI_AVEncVideoMeanAbsoluteDifference == *Api)
        //else if (CODECAPI_AVEncVideoMaxQP == *Api) - static
        else if (CODECAPI_AVEncVideoLTRBufferControl == *Api)
        {
            if (m_bInitialized) // Dynamic change is not allowed
                hr = S_FALSE;
            else
                hr = S_OK;
        }
        else if (CODECAPI_AVEncVideoMarkLTRFrame == *Api)
        {
            if (m_Ltr->IsEnabled())
                hr = S_OK;
            else
                hr = S_FALSE;
        }
        else if (CODECAPI_AVEncVideoUseLTRFrame == *Api)
        {
            if (m_Ltr->IsEnabled())
                hr = S_OK;
            else
                hr = S_FALSE;
        }
        else if (CODECAPI_AVEncH264CABACEnable == *Api)
        {
            m_pCabac->SetCodecProfile(m_MfxParamsVideo.mfx.CodecProfile); //TODO: remove this paranoid code
            hr = m_pCabac->IsModifiable();
        }
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::GetParameterRange(const GUID* Api,
                                           VARIANT* ValueMin,
                                           VARIANT* ValueMax,
                                           VARIANT* SteppingDelta)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = MFEncoderParams::GetParameterRange(Api, ValueMin, ValueMax, SteppingDelta);
    if (E_NOTIMPL == hr)
    {
        CHECK_POINTER(Api, E_POINTER);
        //if (CODECAPI_AVEncMPVProfile == *Api)
        //else if (CODECAPI_AVEncMPVLevel == *Api)
        //else if (CODECAPI_AVEncVideoTemporalLayerCount == *Api)
        //else if (CODECAPI_AVEncVideoSelectLayer == *Api)
        //else if (CODECAPI_AVEncVideoEncodeQP == *Api)
        //else if (CODECAPI_AVEncVideoForceKeyFrame == *Api)
        //else if (CODECAPI_AVEncCommonLowLatency == *Api)
        //else if (CODECAPI_AVLowLatencyMode == *Api)
        //else if (CODECAPI_AVEncCommonRateControlMode == *Api)
        //else if (CODECAPI_AVEncCommonMeanBitRate == *Api)
        //else if (CODECAPI_AVEncCommonMaxBitRate == *Api)
        //else if(CODECAPI_AVEncCommonBufferSize == *Api)
        //else if (CODECAPI_AVEncMPVGOPSize == *Api)
        //else if (CODECAPI_AVEncCommonQuality == *Api)
        //else if (CODECAPI_AVEncCommonQualityVsSpeed == *Api)
        //else if (CODECAPI_AVEncMPVDefaultBPictureCount == *Api)
        //else if (CODECAPI_AVEncVideoEncodeFrameTypeQP == *Api)
        if (CODECAPI_AVEncSliceControlMode == *Api)
        {
            hr = m_pSliceControl->GetModeParameterRange(ValueMin, ValueMax, SteppingDelta);
        }
        //else if (CODECAPI_AVEncSliceControlSize == *Api)
        //else if (CODECAPI_AVEncVideoMaxNumRefFrame == *Api)
        //else if (CODECAPI_AVEncVideoMeanAbsoluteDifference == *Api)
        //else if (CODECAPI_AVEncVideoMaxQP == *Api) - static
        //else if (CODECAPI_AVEncVideoLTRBufferControl == *Api)
        //else if (CODECAPI_AVEncVideoMarkLTRFrame == *Api)
        //else if (CODECAPI_AVEncVideoUseLTRFrame == *Api)
        //else if (CODECAPI_AVEncH264CABACEnable == *Api)
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::GetParameterValues(const GUID* Api,
                                            VARIANT** Values,
                                            ULONG* ValuesCount)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = E_NOTIMPL;

    CHECK_POINTER(Api, E_POINTER);
    CHECK_POINTER(Values, E_POINTER);
    CHECK_POINTER(ValuesCount, E_POINTER);
    *Values = NULL;

    //TODO: add support of CODECAPI_AVEncVideoEncodeFrameTypeQP
    if (CODECAPI_AVEncVideoLTRBufferControl == *Api)
    {
        MFX_LTRACE_GUID(MF_TL_NOTES, CODECAPI_AVEncVideoLTRBufferControl);
        hr = E_OUTOFMEMORY;
        *ValuesCount = m_Ltr->GetMaxBufferControlSize()+1; //enable values 0...MaxSize
        MFX_LTRACE_I(MF_TL_GENERAL, *ValuesCount);
        *Values = (VARIANT*)CoTaskMemAlloc(*ValuesCount * sizeof(VARIANT));
        ATLASSERT(NULL != *Values);
        if (NULL != *Values)
        {
            hr = S_OK;
            memset(*Values, 0, *ValuesCount * sizeof(VARIANT));
            for (size_t i = 0; i < *ValuesCount; i++)
            {
                VARIANT &value = (*Values)[i];
                hr = VariantClear(&value);
                if (FAILED(hr))
                    break;
                value.vt = VT_UI4;
                value.ulVal = i + (MF_LTR_BC_MODE_TRUST_UNTIL << 16);
            }
        }
    }
    if (FAILED(hr))
    {
        CoTaskMemFree(*Values);
        *Values = NULL;
        *ValuesCount = 0;
    }
    MFX_LTRACE_D(MF_TL_NOTES, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::GetDefaultValue(const GUID* Api, VARIANT* Value)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = MFEncoderParams::GetDefaultValue(Api, Value);
    if ( FAILED(hr) )
    {
        CHECK_POINTER(Api, E_POINTER);
        CHECK_POINTER(Value, E_POINTER);
        hr = S_OK;

        if (CODECAPI_AVEncMPVProfile == *Api)
        {
            hr = VFW_E_CODECAPI_NO_DEFAULT;
        }
        else if (CODECAPI_AVEncMPVLevel == *Api)
        {
            hr = VFW_E_CODECAPI_NO_DEFAULT;
        }
        else if (CODECAPI_AVEncVideoTemporalLayerCount == *Api)
        {
            Value->vt   = VT_UI4;
            Value->ulVal = 1;
        }
        else if (CODECAPI_AVEncVideoSelectLayer == *Api)
        {
            Value->vt   = VT_UI4;
            Value->ulVal = 0;
        }
        else if (CODECAPI_AVEncVideoEncodeQP == *Api)
        {
            hr = VFW_E_CODECAPI_NO_DEFAULT;
        }
        else if (CODECAPI_AVEncVideoForceKeyFrame == *Api)
        {
            Value->vt   = VT_UI4;
            Value->ulVal = 0;
        }
        else if (CODECAPI_AVEncCommonLowLatency == *Api || CODECAPI_AVLowLatencyMode == *Api)
        {
            Value->vt   = VT_BOOL;
            Value->boolVal = VARIANT_FALSE;
        }
        else if (CODECAPI_AVEncCommonRateControlMode == *Api)
        {
            Value->vt   = VT_UI4;
            Value->ulVal = eAVEncCommonRateControlMode_CBR;
        }
        else if (CODECAPI_AVEncCommonMeanBitRate == *Api)
        {
            Value->vt   = VT_UI4;
            Value->ulVal = 2222000;
        }
        else if (CODECAPI_AVEncCommonMaxBitRate == *Api)
        {
            Value->vt   = VT_UI4;
            Value->ulVal = 0;
        }
        else if (CODECAPI_AVEncCommonBufferSize == *Api)
        {
            Value->vt   = VT_UI4;
            Value->ulVal = 0;
        }
        else if (CODECAPI_AVEncMPVGOPSize == *Api)
        {
            Value->vt   = VT_UI4;
            Value->ulVal = 128;
        }
        else if (CODECAPI_AVEncCommonQuality == *Api)
        {
            Value->vt   = VT_UI4;
            Value->ulVal = 50; //in %
        }
        else if (CODECAPI_AVEncCommonQualityVsSpeed == *Api)
        {
            Value->vt   = VT_UI4;
            if (HWPlatform::Type() == HWPlatform::VLV) {
                Value->ullVal = 0; //MFX_TARGETUSAGE_BEST_SPEED for VLV
            } else {
                Value->ulVal = 50; // 0 - Lower quality, faster encoding; 100 - Higher quality, slower encoding.
            }
        }
        else if (CODECAPI_AVEncMPVDefaultBPictureCount == *Api)
        {
            Value->vt   = VT_UI4;
            Value->ulVal = 0; //no B frames
        }
        //else if (CODECAPI_AVEncVideoEncodeFrameTypeQP == *Api)
        else if (CODECAPI_AVEncSliceControlMode == *Api)
        {
            Value->vt   = VT_UI4;
            Value->ulVal = MF_PARAM_SLICECONTROL_MODE_MBROW_PER_SLICE;
        }
        else if (CODECAPI_AVEncSliceControlSize == *Api)
        {
            hr = VFW_E_CODECAPI_NO_DEFAULT;
            //TODO: Default Value: Recommended default is to have a single slice for whole frame.
            //TODO: on the other hand we need to init with max possibly needed NumSlice.
        }
        //else if (CODECAPI_AVEncVideoMaxNumRefFrame == *Api) - static
        //else if (CODECAPI_AVEncVideoMeanAbsoluteDifference == *Api)
        //else if (CODECAPI_AVEncVideoMaxQP == *Api) - static
        //else if (CODECAPI_AVEncVideoLTRBufferControl == *Api)
        //else if (CODECAPI_AVEncVideoMarkLTRFrame == *Api)
        //else if (CODECAPI_AVEncVideoUseLTRFrame == *Api)
        //else if (CODECAPI_AVEncH264CABACEnable == *Api)
        else
        {
            hr = VFW_E_CODECAPI_NO_DEFAULT;
        }
    }
    ATLASSERT(S_OK == hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::GetValue(const GUID* Api, VARIANT* Value)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = MFEncoderParams::GetValue(Api, Value);
    if ( FAILED(hr) )
    {
        CHECK_POINTER(Api, E_POINTER);
        CHECK_POINTER(Value, E_POINTER);
        if (CODECAPI_AVEncVideoTemporalLayerCount == *Api)
        {
            Value->vt   = VT_UI4;
            hr = m_TemporalScalablity.GetTemporalLayersCount( Value->ulVal );
            if (SUCCEEDED(hr))
            {
                MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncVideoTemporalLayerCount: ",
                            "%lu",
                            Value->ulVal);
            }
        }
        else if (CODECAPI_AVEncVideoSelectLayer == *Api)
        {
            Value->vt   = VT_UI4;
            hr = m_TemporalScalablity.GetSelectedLayerNumber( Value->ulVal );
            if (SUCCEEDED(hr))
            {
                MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncVideoSelectLayer: ",
                            "%lu",
                            Value->ulVal);
            }
        }
        else if (CODECAPI_AVEncVideoEncodeQP == *Api)
        {
            if (m_RateControlMode.IsApplicable(Api))
            {
                Value->vt   = VT_UI8;
                hr = m_TemporalScalablity.GetLayerEncodeQP( Value->ullVal );
                if (SUCCEEDED(hr))
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncVideoEncodeQP: ",
                                "%I64u",
                                Value->ullVal);
                }
            }
        }
        else if (CODECAPI_AVEncVideoForceKeyFrame == *Api)
        {
            Value->vt   = VT_UI4;
            hr = S_OK;
            Value->ulVal = ( m_TemporalScalablity.IsForceKeyFrameRequested() ? 1 : 0 );
            MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncVideoForceKeyFrame: ",
                        "%lu",
                        Value->ulVal);
        }
        else if (CODECAPI_AVEncCommonLowLatency == *Api || CODECAPI_AVLowLatencyMode == *Api)
        {
            UINT32 nLowLatency;
            HRESULT hrTmp = m_pAttributes->GetUINT32(MF_LOW_LATENCY, &nLowLatency);
            if (SUCCEEDED(hrTmp))
            {
                if ( (!!nLowLatency) != (!!Value->boolVal) ) // "!!" is to cast VARIANT_BOOL and BOOL to bool
                {
                    //it seems that LowLatency setting was updated through IMFAttributes

                    if (!nLowLatency && m_TemporalScalablity.IsEnabled())
                    {
                        //MSFT doesn't plan to use CODECAPI_AVEncCommonLowLatency VARIANT_FALSE for Lync
                        m_pAttributes->SetUINT32(MF_LOW_LATENCY, TRUE);
                        ATLASSERT(nLowLatency || !m_TemporalScalablity.IsEnabled());
                    }
                    else
                    {
                        //update current value
                        m_MfxParamsVideo.AsyncDepth = (nLowLatency ? 1 : 0);
                    }
                }
            }
            Value->vt   = VT_BOOL;
            hr = S_OK;
            Value->boolVal = ( ( 1 == m_MfxParamsVideo.AsyncDepth ) ? VARIANT_TRUE : VARIANT_FALSE );
            MFX_LTRACE_1(MF_TL_GENERAL,
                        (CODECAPI_AVLowLatencyMode == *Api ? "CODECAPI_AVLowLatencyMode: "
                                                        : "CODECAPI_AVEncCommonLowLatency: "),
                        "%lu",
                        (Value->boolVal ? 1 : 0));
        }
        else if (CODECAPI_AVEncCommonRateControlMode == *Api)
        {
            Value->vt   = VT_UI4;
            hr = m_RateControlMode.GetMode(Value->ulVal);
            if (SUCCEEDED(hr))
            {
                MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncCommonRateControlMode: ",
                            "%lu",
                            Value->ulVal);
            }
        }
        else if (CODECAPI_AVEncCommonMeanBitRate == *Api)
        {
            if (m_RateControlMode.IsApplicable(Api))
            {
                Value->vt   = VT_UI4;
                Value->ulVal = m_RateControlMode.GetMeanBitRate();
                hr = S_OK;
                MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncCommonMeanBitRate: ",
                            "%lu",
                            Value->ulVal);
            }
        }
        else if (CODECAPI_AVEncCommonMaxBitRate == *Api)
        {
            if (m_RateControlMode.IsApplicable(Api))
            {
                Value->vt   = VT_UI4;
                Value->ulVal = m_RateControlMode.GetMaxBitRate();
                hr = S_OK;
                MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncCommonMaxBitRate: ",
                            "%lu",
                            Value->ulVal);
            }
        }
        else if (CODECAPI_AVEncCommonBufferSize == *Api)
        {
            if (m_RateControlMode.IsApplicable(Api))
            {
                Value->vt   = VT_UI4;
                Value->ulVal = m_RateControlMode.GetBufferSize();
                hr = S_OK;
                MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncCommonBufferSize: ",
                            "%lu",
                            Value->ulVal);
            }
        }
        else if (CODECAPI_AVEncMPVGOPSize == *Api)
        {
            Value->vt   = VT_UI4;
            hr = S_OK;
            //TODO: check CODECAPI_AVEncMPVDefaultBPictureCount value and resolve conflict by rejecting or updating
            Value->ulVal = m_MfxParamsVideo.mfx.GopPicSize;
            if (MFX_GOPSIZE_INFINITE == Value->ulVal)
                Value->ulVal = MF_GOPSIZE_INFINITE;
            MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncMPVGOPSize: ",
                        "%lu", Value->ulVal);
        }
        else if (CODECAPI_AVEncCommonQuality == *Api)
        {
            if (m_RateControlMode.IsApplicable(Api))
            {
                Value->vt   = VT_UI4;
                hr = m_RateControlMode.GetQualityPercent(Value->ulVal);
                if (SUCCEEDED(hr))
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncCommonQuality: ",
                                "%lu",
                                Value->ulVal);
                }
            }
        }
        else if (CODECAPI_AVEncCommonQualityVsSpeed == *Api)
        {
            Value->vt   = VT_UI4;
            hr = m_RateControlMode.GetQualityVsSpeed(Value->ulVal);
            if (SUCCEEDED(hr))
            {
                MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncCommonQualityVsSpeed: ",
                            "%lu",
                            Value->ulVal);
            }
        }
        else if (CODECAPI_AVEncMPVDefaultBPictureCount == *Api)
        {
            if (0 == m_MfxParamsVideo.mfx.GopRefDist)
            {
                //Should not occur.
                //Number of B frames is not specified.
                hr = VFW_E_CODECAPI_NO_CURRENT_VALUE;
                ATLASSERT(0 > m_MfxParamsVideo.mfx.GopRefDist);
            }
            else
            {
                Value->vt   = VT_UI4;
                hr = S_OK;
                Value->ulVal = m_MfxParamsVideo.mfx.GopRefDist - 1;
                MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncMPVDefaultBPictureCount: ",
                            "%lu", Value->ulVal);
            }
        }
        //else if (CODECAPI_AVEncVideoEncodeFrameTypeQP == *Api)
        else if (CODECAPI_AVEncSliceControlMode == *Api)
        {
            Value->vt   = VT_UI4;
            Value->ulVal = m_pSliceControl->GetMode(&hr);
            MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncSliceControlMode: ", "%lu", Value->ulVal);
        }
        else if (CODECAPI_AVEncSliceControlSize == *Api)
        {
            Value->vt   = VT_UI4;
            Value->ulVal = m_pSliceControl->GetSize(&hr);
            MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncSliceControlSize: ", "%lu", Value->ulVal);
        }
        else if (CODECAPI_AVEncVideoMaxNumRefFrame == *Api)
        {
            if (0 == m_MfxParamsVideo.mfx.NumRefFrame)
            {
                hr = VFW_E_CODECAPI_NO_CURRENT_VALUE;
            }
            else
            {
                Value->vt   = VT_UI4;
                hr = S_OK;
                Value->ulVal = m_MfxParamsVideo.mfx.NumRefFrame;
                MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncVideoMaxNumRefFrame: ",
                            "%lu", Value->ulVal);
            }
        }
        //else if (CODECAPI_AVEncVideoMeanAbsoluteDifference == *Api)
        //else if (CODECAPI_AVEncVideoMaxQP == *Api) - static
        else if (CODECAPI_AVEncVideoLTRBufferControl == *Api)
        {
            Value->vt   = VT_UI4;
            hr = m_Ltr->GetBufferControl( Value->ulVal );
            if (SUCCEEDED(hr))
            {
                MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncVideoLTRBufferControl: ",
                            "0x%08lx", Value->ulVal);
            }
        }
        //else if (CODECAPI_AVEncVideoMarkLTRFrame == *Api)
        //else if (CODECAPI_AVEncVideoUseLTRFrame == *Api)
        else if (CODECAPI_AVEncH264CABACEnable == *Api)
        {
            m_pCabac->SetCodecProfile(m_MfxParamsVideo.mfx.CodecProfile);//TODO: remove this paranoid code
            Value->vt   = VT_BOOL;
            Value->boolVal = m_pCabac->Get(&hr);
        }
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

bool MFPluginVEnc::InternalValueEquals(const GUID *Api, VARIANT *Value)
{
    bool bResult = false;
    _variant_t variantCurrentValue;
    HRESULT hr = GetValue(Api, &variantCurrentValue);
    if (SUCCEEDED(hr))
    {
        //_variant_t::operator== performs check against NULL and dereferences the Value
        bResult = (variantCurrentValue == Value);
    }
    return bResult;
}
/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::SetValue(const GUID *Api, VARIANT *Value)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = S_OK;
    if ( !(InternalValueEquals(Api, Value)) ) // if is not changed - nothing to do
    {
        HRESULT hrIsModifiable = IsModifiable(Api);
        if ( S_FALSE == hrIsModifiable )
            hr = S_FALSE; //Cannot change the property now.
        else
        {
            //if (m_pSVC)
            //{
            //    hr = m_pSVC->SetValue(Api, Value);
            //    if (E_NOTIMPL != hr)
            //        return hr;
            //}
            CHECK_POINTER(Api, E_POINTER);
            CHECK_POINTER(Value, E_POINTER);

            hr = E_INVALIDARG;

            if (CODECAPI_AVEncMPVProfile == *Api)
            {
                if (VT_UI4 == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncMPVProfile: ",
                                "%lu",
                                Value->ulVal);
                    hr = S_OK;
                    const mfxU16 nOldMfxValue = m_MfxParamsVideo.mfx.CodecProfile;
                    const mfxU16 nOldGopRefDist = m_MfxParamsVideo.mfx.GopRefDist;
                    m_MfxParamsVideo.mfx.CodecProfile = (mfxU16)(MS_2_MFX_VALUE(ProfilesTbl, Value->ulVal));
                    if (MFX_PROFILE_UNKNOWN == m_MfxParamsVideo.mfx.CodecProfile &&
                        eAVEncMPVProfile_unknown != Value->ulVal &&
                        eAVEncH264VProfile_unknown != Value->ulVal)
                    {
                        m_MfxParamsVideo.mfx.CodecProfile = nOldMfxValue;
                        hr = E_INVALIDARG; //do not accept unexpected values
                    }
                    else
                    {
                        //Baseline and Constrained (Base/High) profiles do not support B-frames. Need to disable them.
                        //TODO: remove check for TemporalLayersCount because profile check is enough.
                        if (m_MfxParamsVideo.mfx.GopRefDist > 1 &&
                                (m_TemporalScalablity.IsEnabled() ||
                                MFX_PROFILE_AVC_BASELINE == m_MfxParamsVideo.mfx.CodecProfile ||
                                MFX_PROFILE_AVC_CONSTRAINED_BASELINE == m_MfxParamsVideo.mfx.CodecProfile ||
                                MFX_PROFILE_AVC_CONSTRAINED_HIGH == m_MfxParamsVideo.mfx.CodecProfile))
                        {
                            m_MfxParamsVideo.mfx.GopRefDist = 1;
                        }

                        if (m_bInitialized)
                        {
                            MFX_LTRACE_S(MF_TL_PERF, "calling RequireDynamicConfigurationChange for CODECAPI_AVEncMPVProfile");
                            if (FAILED(RequireDynamicConfigurationChange(dccResetWithIdrRequired)))
                            {
                                //new settings are invalid. rollback changes:
                                m_MfxParamsVideo.mfx.CodecProfile = nOldMfxValue;
                                m_MfxParamsVideo.mfx.GopRefDist = nOldGopRefDist;
                                hr = E_INVALIDARG;
                            }
                        }
                        if (SUCCEEDED(hr))
                        {
                            m_pCabac->SetCodecProfile(m_MfxParamsVideo.mfx.CodecProfile);
                            HandleNewMessage(mtRequireUpdateMaxMBperSec, this); // ignore return value
                        }
                    }
                }
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
            else if (CODECAPI_AVEncVideoTemporalLayerCount == *Api)
            {
                if (VT_UI4 == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncVideoTemporalLayerCount : ",
                                "%lu",
                                Value->ulVal);

                    //TODO: consider removing check, because it is checked below inside of SetNewTemporalLayersCount
                    if (m_TemporalScalablity.IsTemporalLayersCountValid(Value->ulVal))
                    {
                        hr = S_OK;
                    }
                    if (SUCCEEDED(hr))
                    {
                        hr = m_TemporalScalablity.SetNewTemporalLayersCount(Value->ulVal); //should not fail, checked above
                    }
                    if (SUCCEEDED(hr))
                    {
                        hr = m_TemporalScalablity.UpdateVideoParam(m_MfxParamsVideo, m_arrEncExtBuf);
                        if (E_NOTIMPL == hr) hr = S_OK;
                    }

                    if (SUCCEEDED(hr))
                    {
                        const ULONG nOldValue = m_TemporalScalablity.GetTemporalLayersCount();
                        const mfxU16 nOldGopRefDist = m_MfxParamsVideo.mfx.GopRefDist;
                        const mfxU16 nOldGopOptFlag = m_MfxParamsVideo.mfx.GopOptFlag;
                        const mfxU16 nOldGopPicSize = m_MfxParamsVideo.mfx.GopPicSize;
                        const mfxU16 nOldAsyncDepth = m_MfxParamsVideo.AsyncDepth;

                        //TODO: consider moving into UpdateVideoParam (two calls: new settings + rollback)
                        // Update related properties to keep them consistent with new number of layers:

                        if (Value->ulVal > 0)
                        {
                            // B frames are unsupported for Lync mode
                            m_MfxParamsVideo.mfx.GopRefDist = 1;

                            //MSFT doesn't plan to use CODECAPI_AVEncCommonLowLatency VARIANT_FALSE for Lync
                            m_MfxParamsVideo.AsyncDepth = 1;

                            // GOPSize must to multiplies of 2^(TemporalLayersCount-1)
                            mfxU16 nDivisor = 1 << (MAX_TEMPORAL_LAYERS_COUNT - 1);
                            if (MFX_GOPSIZE_INFINITE != m_MfxParamsVideo.mfx.GopPicSize &&
                                m_MfxParamsVideo.mfx.GopPicSize % nDivisor)
                            {
                                // need to ceil GopSize up to multiply of 2^(TemporalLayersCount-1):
                                m_MfxParamsVideo.mfx.GopPicSize += (nDivisor-1);
                                m_MfxParamsVideo.mfx.GopPicSize = m_MfxParamsVideo.mfx.GopPicSize >> (MAX_TEMPORAL_LAYERS_COUNT - 1);
                                m_MfxParamsVideo.mfx.GopPicSize = m_MfxParamsVideo.mfx.GopPicSize << (MAX_TEMPORAL_LAYERS_COUNT - 1);
                            }
                        }

                        if (Value->ulVal > 1)
                        {
                            // to avoid unexpected IDR-s which could ruin temporal sequence
                            m_MfxParamsVideo.mfx.GopOptFlag |= MFX_GOP_STRICT;
                        }

                        hr = S_OK;
                        mfxStatus sts = CheckMfxParams();
                        ATLASSERT(MFX_ERR_NONE == sts || MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts);
                        if (MFX_ERR_NONE != sts && MFX_WRN_INCOMPATIBLE_VIDEO_PARAM != sts)
                        {
                            hr = E_INVALIDARG;
                        }
                        if (SUCCEEDED(hr) && m_bInitialized)
                        {
                            hr = m_TemporalScalablity.PostponeDCCBeforeBaseLayer();
                            HandleNewMessage(mtRequireUpdateMaxMBperSec, this); // ignore return value
                        }
                        if (FAILED(hr))
                        {
                            ATLASSERT(SUCCEEDED(hr));
                            //new settings are invalid. rollback changes:
                            m_TemporalScalablity.SetNewTemporalLayersCount(nOldValue);
                            m_TemporalScalablity.UpdateVideoParam(m_MfxParamsVideo, m_arrEncExtBuf);

                            m_MfxParamsVideo.mfx.GopRefDist = nOldGopRefDist;
                            m_MfxParamsVideo.mfx.GopOptFlag = nOldGopOptFlag;
                            m_MfxParamsVideo.mfx.GopPicSize = nOldGopPicSize;
                            m_MfxParamsVideo.AsyncDepth = nOldAsyncDepth;
                        }
                    }

                    if (SUCCEEDED(hr) && !m_bInitialized) //otherwise it will be done on base layer
                    {
                        // There is a trick:
                        // Some values (temporal layers sequence, qp settings, currentl layer selection)
                        // need to be reset on temporal layers count change.
                        // It is impossible to to that on SetTemporalLayersCount
                        // because that makes rollback (see above) impossible.
                        // So we may reset them after we are sure that change is correct.
                        m_TemporalScalablity.TemporalLayersCountChanged();
                    }
                }
            }
            else if (CODECAPI_AVEncVideoSelectLayer == *Api)
            {
                if (VT_UI4 == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncVideoSelectLayer : ",
                                "%lu",
                                Value->ulVal);
                    hr = m_TemporalScalablity.SelectLayer(Value->ulVal);
                }
            }
            else if (CODECAPI_AVEncVideoEncodeQP == *Api)
            {
                if (VT_UI8 == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncVideoEncodeQP : ",
                                "%I64u",
                                Value->ullVal);
                    hr = m_TemporalScalablity.SetLayerEncodeQP(Value->ullVal);
                }
            }
            else if (CODECAPI_AVEncVideoForceKeyFrame == *Api)
            {
                if (VT_UI4 == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncVideoForceKeyFrame : ",
                                "%lu",
                                Value->ulVal);
                    if ( Value->ulVal )
                    {
                        hr = m_TemporalScalablity.ForceKeyFrameForBaseLayer();
                    }
                }
            }
            else if (CODECAPI_AVEncCommonLowLatency == *Api || CODECAPI_AVLowLatencyMode == *Api)
            {
                if ( VT_BOOL == Value->vt )
                {
                    MFX_LTRACE_1(MF_TL_GENERAL,
                                 (CODECAPI_AVLowLatencyMode == *Api ? "CODECAPI_AVLowLatencyMode: "
                                                                    : "CODECAPI_AVEncCommonLowLatency: "),
                                "%lu",
                                (Value->boolVal ? 1 : 0) );
                    if (Value->boolVal != VARIANT_TRUE && Value->boolVal != VARIANT_FALSE)
                    {
                        ATLASSERT(Value->boolVal == VARIANT_TRUE || Value->boolVal == VARIANT_FALSE);
                        hr = E_INVALIDARG;
                    }
                    else
                    {
                        m_MfxParamsVideo.AsyncDepth = (Value->boolVal ? 1 : 0);
                        m_pAttributes->SetUINT32(MF_LOW_LATENCY, (Value->boolVal ? TRUE : FALSE));
                        //TODO: other settings, suggested for MSFT
                        hr = S_OK;
                    }
                }
            }
            else if (CODECAPI_AVEncCommonRateControlMode == *Api)
            {
                if (VT_UI4 == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncCommonRateControlMode : ",
                                "%lu",
                                Value->ulVal);
                    hr = m_RateControlMode.SetMode(Value->ulVal);
                    //RateControlMode really changed (otherwise wouldn't get here)
                    //Let's reset all applicable properties to their default values
                    if (SUCCEEDED(hr))
                    {
                        //TODO: not neccessary?
                        MfxSetInitialDelayInKB(m_MfxParamsVideo.mfx, 0);   // aka QPI aka Accuracy
                        MfxSetBufferSizeInKB  (m_MfxParamsVideo.mfx, 0);
                        MfxSetTargetKbps      (m_MfxParamsVideo.mfx, 0);   // aka QPP
                        MfxSetMaxKbps         (m_MfxParamsVideo.mfx, 0);   // aka QPB aka Convergence

                        if (SUCCEEDED(hr)) // for Quality RateControlMode
                            hr = InternalSetDefaultRCParamIfApplicable(&CODECAPI_AVEncCommonQuality);
                        if (SUCCEEDED(hr)) // for CBR, PeakConstrainedVBR and UnconstrainedVBR
                            hr = InternalSetDefaultRCParamIfApplicable(&CODECAPI_AVEncCommonMeanBitRate);
                        if (SUCCEEDED(hr)) // for PeakConstrainedVBR
                            hr = InternalSetDefaultRCParamIfApplicable(&CODECAPI_AVEncCommonMaxBitRate);
                        if (SUCCEEDED(hr)) // for CBR, PeakConstrainedVBR
                            hr = InternalSetDefaultRCParamIfApplicable(&CODECAPI_AVEncCommonBufferSize);
                    }
                }
            }
            else if (CODECAPI_AVEncCommonMeanBitRate == *Api)
            {
                if (VT_UI4 == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncCommonMeanBitRate: ",
                                "%lu",
                                Value->ulVal);
                    hr = S_OK;
                    mfxInfoMFX oldBRC = {0};
                    MfxCopyBRCparams(oldBRC, m_MfxParamsVideo.mfx);
                    const ULONG nOldValue = m_RateControlMode.GetMeanBitRate();
                    m_RateControlMode.SetMeanBitRate(Value->ulVal);
                    if (MfxGetTargetKbps(oldBRC) != MfxGetTargetKbps(m_MfxParamsVideo.mfx)) // need additional check because of truncation
                    {
                        if (m_bInitialized)
                        {
                            MFX_LTRACE_S(MF_TL_PERF, "calling RequireDynamicConfigurationChange for CODECAPI_AVEncCommonMeanBitRate");
                            if (FAILED(RequireDynamicConfigurationChange(dccResetWoIdrRequired)))
                            {
                                //new settings are invalid. rollback changes:
                                MfxCopyBRCparams(m_MfxParamsVideo.mfx, oldBRC);
                                m_RateControlMode.SetMeanBitRate(nOldValue);
                                hr = E_INVALIDARG;
                            }
                        }
                    }
                }
            }
            else if (CODECAPI_AVEncCommonMaxBitRate == *Api)
            {
                if (VT_UI4 == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncCommonMaxBitRate: ",
                                "%lu",
                                Value->ulVal);
                    hr = S_OK;
                    mfxInfoMFX oldBRC = {0};
                    MfxCopyBRCparams(oldBRC, m_MfxParamsVideo.mfx);
                    const ULONG nOldValue = m_RateControlMode.GetMaxBitRate();
                    m_RateControlMode.SetMaxBitRate(Value->ulVal);
                    if (MfxGetMaxKbps(oldBRC) != MfxGetMaxKbps(m_MfxParamsVideo.mfx)) // need additional check because of truncation
                    {
                        if (m_bInitialized)
                        {
                            MFX_LTRACE_S(MF_TL_PERF, "calling RequireDynamicConfigurationChange for CODECAPI_AVEncCommonMaxBitRate");
                            if (FAILED(RequireDynamicConfigurationChange(dccResetWoIdrRequired)))
                            {
                                //new settings are invalid. rollback changes:
                                MfxCopyBRCparams(m_MfxParamsVideo.mfx, oldBRC);
                                m_RateControlMode.SetMaxBitRate(nOldValue);
                                hr = E_INVALIDARG;
                            }
                        }
                    }
                }
            }
            else if (CODECAPI_AVEncCommonBufferSize == *Api)
            {
                if (VT_UI4 == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncCommonBufferSize: ",
                                "%lu",
                                Value->ulVal);
                    hr = S_OK;
                    mfxInfoMFX oldBRC = {0};
                    MfxCopyBRCparams(oldBRC, m_MfxParamsVideo.mfx);
                    const ULONG nOldValue = m_RateControlMode.GetBufferSize();
                    m_RateControlMode.SetBufferSize(Value->ulVal);
                    if (MfxGetBufferSizeInKB(oldBRC) != MfxGetBufferSizeInKB(m_MfxParamsVideo.mfx)) // need additional check because of truncation
                    {
                        if (m_bInitialized)
                        {
                            MFX_LTRACE_S(MF_TL_PERF, "calling RequireDynamicConfigurationChange for CODECAPI_AVEncCommonBufferSize");
                            if (FAILED(RequireDynamicConfigurationChange(dccResetWoIdrRequired)))
                            {
                                //new settings are invalid. rollback changes:
                                MfxCopyBRCparams(m_MfxParamsVideo.mfx, oldBRC);
                                m_RateControlMode.SetBufferSize(nOldValue);
                                hr = E_INVALIDARG;
                            }
                        }
                    }
                }
            }
            else if (CODECAPI_AVEncMPVGOPSize == *Api)
            {
                if (VT_UI4 == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncMPVGOPSize : ",
                                "%lu",
                                Value->ulVal);
                    mfxU16 newGopPicSize = MFX_GOPSIZE_INFINITE;
                    bool bValidValue = true;
                    if (MF_GOPSIZE_INFINITE != Value->ulVal)
                    {
                        if (Value->ulVal > USHRT_MAX)
                        {
                            MFX_LTRACE_2(MF_TL_GENERAL, "CODECAPI_AVEncMPVGOPSize is too big: ", "%lu (%0x08lx)", Value->ulVal, Value->ulVal);
                            // value is too big for MediaSDK, but we still need to support it.
                            // let's use biggest supported by MediaSDK,
                            // but keep information about remainder of division by 2^8, to check if it aligned with TLC
                            newGopPicSize = ((mfxU16)Value->ulVal ) | 0xFF00;
                        }
                        else
                        {
                            newGopPicSize = (mfxU16)Value->ulVal;
                        }
                        if ( m_TemporalScalablity.GetTemporalLayersCount() > 1 )
                        {
                            // GOPSize must to multiplies of 2^(MaxTemporalLayersCount-1)
                            ULONG nDivisor = 1 << (MAX_TEMPORAL_LAYERS_COUNT - 1);
                            if ( newGopPicSize % nDivisor )
                            {
                                MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncMPVGOPSize must be multiplies to 2^(MaxTLC-1): ", "%lu", Value->ulVal);
                                bValidValue = false; //TODO: adjust value?
                            }
                        }
                    }
                    if ( bValidValue )
                    {
                        m_MfxParamsVideo.mfx.GopPicSize = newGopPicSize;
                        hr = S_OK;
                    }
                    else
                    {
                        hr = E_INVALIDARG;
                    }
                }
            }
            else if (CODECAPI_AVEncCommonQuality == *Api)
            {
                if (VT_UI4 == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncCommonQuality : ",
                                "%lu",
                                Value->ulVal);
                    hr = m_RateControlMode.SetQualityPercent(Value->ulVal);
                }
            }
            else if (CODECAPI_AVEncCommonQualityVsSpeed == *Api)
            {
                if (VT_UI4 == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncCommonQualityVsSpeed : ",
                                "%lu",
                                Value->ulVal);
                    hr = m_RateControlMode.SetQualityVsSpeed(Value->ulVal);
                    //TODO: move inside of MFRateControlMode
                    HandleNewMessage(mtRequireUpdateMaxMBperSec, this); // ignore return value
                }
            }
            else if (CODECAPI_AVEncMPVDefaultBPictureCount == *Api)
            {
                if (VT_UI4 == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncMPVDefaultBPictureCount: ",
                                "%lu",
                                Value->ulVal);
                    //TODO: examine other properties to find out which of them can interact with this one
                    //and add them to IsModifiable
                    //B-frames disabled for 1==TemporalLayersCount, as agreed with MSFT not requred for Lync
                    //TODO: replace by check for constrained profile because Lync use only them?
                    if (m_TemporalScalablity.IsEnabled() ||
                        MFX_PROFILE_AVC_BASELINE == m_MfxParamsVideo.mfx.CodecProfile ||
                        MFX_PROFILE_AVC_CONSTRAINED_BASELINE == m_MfxParamsVideo.mfx.CodecProfile ||
                        MFX_PROFILE_AVC_CONSTRAINED_HIGH == m_MfxParamsVideo.mfx.CodecProfile)
                    {
                        ATLASSERT( 1 == m_MfxParamsVideo.mfx.GopRefDist );
                        m_MfxParamsVideo.mfx.GopRefDist = 1;
                        hr = E_INVALIDARG;
                    }
                    else
                    {
                        m_MfxParamsVideo.mfx.GopRefDist = (mfxU16)Value->ulVal + 1;
                        hr = S_OK;
                    }
                    HandleNewMessage(mtRequireUpdateMaxMBperSec, this); // ignore return value
                }
            }
            //else if (CODECAPI_AVEncVideoEncodeFrameTypeQP == *Api)
            else if (CODECAPI_AVEncSliceControlMode == *Api)
            {
                if (VT_UI4 == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncSliceControlMode: ",
                                "%lu",
                                Value->ulVal);
                    hr = m_pSliceControl->SetMode(Value->ulVal);
                    ATLASSERT(SUCCEEDED(hr));
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncSliceControlMode: ", "%lu", Value->ulVal);
                }
            }
            else if (CODECAPI_AVEncSliceControlSize == *Api)
            {
                if (VT_UI4 == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncSliceControlSize: ",
                                "%lu",
                                Value->ulVal);
                    hr = S_OK;

                    const mfxU16 nOldMfxValue = m_MfxParamsVideo.mfx.NumSlice;
                    const ULONG nOldValue = m_pSliceControl->GetSize(NULL);
                    m_pSliceControl->SetFrameInfo(m_MfxParamsVideo.mfx.FrameInfo);

                    hr = m_pSliceControl->SetSize(Value->ulVal);
                    ATLASSERT(SUCCEEDED(hr));
                    if (SUCCEEDED(hr))
                    {
                        m_MfxParamsVideo.mfx.NumSlice = m_pSliceControl->GetNumSlice();
                        // need additional check because values can map to same NumSlice
                        if (nOldMfxValue != m_MfxParamsVideo.mfx.NumSlice)
                        {
                            if (m_bInitialized)
                            {
                                MFX_LTRACE_S(MF_TL_PERF, "calling RequireDynamicConfigurationChange for CODECAPI_AVEncSliceControlSize");
                                if (FAILED(RequireDynamicConfigurationChange(dccResetWoIdrRequired)))
                                {
                                    //new settings are invalid. rollback changes:
                                    m_pSliceControl->SetSize(nOldValue);
                                    hr = E_INVALIDARG;
                                }
                            }
                        }
                    }
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncSliceControlSize: ", "%lu", Value->ulVal);
                }
            }
            else if (CODECAPI_AVEncVideoMaxNumRefFrame == *Api)
            {
                if (VT_UI4 == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncVideoMaxNumRefFrame: ",
                                "%lu",
                                Value->ulVal);
                    //TODO: examine other properties to find out which of them can interact with this one
                    //and add them to IsModifiable
                    mfxU16 nMin = GetMinNumRefFrame(m_TemporalScalablity.GetMinNumRefFrame(), m_Ltr->GetBufferControlSize());
                    if ((mfxU16)Value->ulVal < nMin)
                    {
                        ATLASSERT( (mfxU16)Value->ulVal < nMin );
                        hr = E_INVALIDARG;
                    }
                    else
                    {
                        m_MfxParamsVideo.mfx.NumRefFrame = (mfxU16)Value->ulVal;
                        hr = S_OK;
                    }
                }
            }
            //else if (CODECAPI_AVEncVideoMeanAbsoluteDifference == *Api)
            //else if (CODECAPI_AVEncVideoMaxQP == *Api) - static
            else if (CODECAPI_AVEncVideoLTRBufferControl == *Api)
            {
                if (VT_UI4 == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncVideoLTRBufferControl: ",
                                "0x%08lx",
                                Value->ulVal);
                    hr = m_Ltr->SetBufferControl(Value->ulVal);
                }
            }
            else if (CODECAPI_AVEncVideoMarkLTRFrame == *Api)
            {
                if (VT_UI4 == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncVideoMarkLTRFrame: ",
                                "0x%08lx",
                                Value->ulVal);
                    hr = m_Ltr->MarkFrame(Value->ulVal);
                }
            }
            else if (CODECAPI_AVEncVideoUseLTRFrame == *Api)
            {
                if (VT_UI4 == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncVideoUseLTRFrame: ",
                                "0x%08lx",
                                Value->ulVal);
                    hr = m_Ltr->UseFrame(Value->ulVal);
                }
            }
            else if (CODECAPI_AVEncH264CABACEnable == *Api)
            {
                if (VT_BOOL == Value->vt)
                {
                    MFX_LTRACE_1(MF_TL_GENERAL, "CODECAPI_AVEncH264CABACEnable: ",
                                "%lu",
                                (Value->boolVal ? 1 : 0) );
                    m_pCabac->SetCodecProfile(m_MfxParamsVideo.mfx.CodecProfile); //TODO: remove this paranoid code
                    hr = m_pCabac->Set(Value->boolVal);
                    if (SUCCEEDED(hr))
                    {
                        hr = m_pCabac->UpdateVideoParam(m_MfxParamsVideo, m_arrEncExtBuf);
                    }
                }
            }
            else
            {
                MFX_LTRACE_S(MF_TL_GENERAL, "Unknown property");
            }
        }
    }
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::InternalSetDefaultValue(const GUID *Api)
{
    VARIANT variantDefaultValue;
    HRESULT hr = S_OK;
    hr = GetDefaultValue(Api, &variantDefaultValue);
    if (SUCCEEDED(hr))
    {
        hr = SetValue(Api, &variantDefaultValue);
        ATLASSERT(S_OK == hr);
    }
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::InternalSetDefaultRCParamIfApplicable(const GUID *Api)
{
    VARIANT variantDefaultValue;
    HRESULT hr = S_OK;
    if (m_RateControlMode.IsApplicable(Api))
    {
        hr = GetDefaultValue(Api, &variantDefaultValue);
        if (SUCCEEDED(hr))
            hr = SetValue(Api, &variantDefaultValue);
    }
    ATLASSERT(S_OK == hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::SetAllDefaults(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = MFEncoderParams::SetAllDefaults();
    if ( E_NOTIMPL == hr )
        hr = S_OK;
    //no need in IsModifiable check here, default path must always be valid
    if (SUCCEEDED(hr)) hr = InternalSetDefaultValue(&CODECAPI_AVLowLatencyMode);
    if (SUCCEEDED(hr)) hr = InternalSetDefaultValue(&CODECAPI_AVEncMPVDefaultBPictureCount);
    if (SUCCEEDED(hr)) hr = InternalSetDefaultValue(&CODECAPI_AVEncCommonRateControlMode);
    // Also sets default value for next properties (if applicable):
    // CODECAPI_AVEncCommonQuality
    // CODECAPI_AVEncCommonMeanBitRate
    // CODECAPI_AVEncCommonMaxBitRate
    // CODECAPI_AVEncCommonBufferSize
    if (SUCCEEDED(hr)) hr = InternalSetDefaultValue(&CODECAPI_AVEncMPVGOPSize);
    if (SUCCEEDED(hr)) hr = InternalSetDefaultValue(&CODECAPI_AVEncCommonQualityVsSpeed);
    if (FAILED(hr))
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "MFPluginVEnc::SetAllDefaults failed");
        MFX_LTRACE_D(MF_TL_PERF, hr);
        ATLASSERT(S_OK == hr);
    }
    return hr;
}

/*------------------------------------------------------------------------------*/

mfxStatus MFPluginVEnc::QueryENC(mfxVideoParam* pParams,
                                 mfxVideoParam* pCorrectedParams)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;

    if (MFX_ERR_NONE == sts)
    {
        bool bLevelChanged = false;

        pCorrectedParams->mfx.CodecId = pParams->mfx.CodecId;
        if ((MFX_CODEC_AVC == pParams->mfx.CodecId) &&
            (MFX_PROFILE_UNKNOWN != pParams->mfx.CodecProfile) &&
            (MFX_LEVEL_UNKNOWN == pParams->mfx.CodecLevel))
        { // setting mimimal level to enable it's correction
            pParams->mfx.CodecLevel = MFX_LEVEL_AVC_1;
            bLevelChanged = true;
        }
        if ((MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == (sts = m_pmfxENC->Query(pParams, pCorrectedParams))) &&
            bLevelChanged)
        {
            pParams->mfx.CodecLevel = pCorrectedParams->mfx.CodecLevel;
            HandleNewMessage(mtRequireUpdateMaxMBperSec, this); // ignore return value

            //bitrate, profile, level parameters alignment
            //in MediaSDK internal logic bitrate has higher priority then profile,
            //for devices profile has more proirity then bitrate
            if (pParams->mfx.CodecLevel <= MFX_LEVEL_AVC_42 &&
                pParams->mfx.CodecProfile == MFX_PROFILE_AVC_BASELINE &&
                MfxGetTargetKbps(pParams->mfx) > 50 * 1200)
            {
                MfxSetTargetKbps(pParams->mfx, 50 * 1200);
            }

            sts = m_pmfxENC->Query(pParams, pCorrectedParams);
            MFX_LTRACE_I(MF_TL_GENERAL, sts);
            ATLASSERT(sts >= MFX_ERR_NONE);
        }
    }
    return sts;
}

/*------------------------------------------------------------------------------*/

mfxStatus MFPluginVEnc::CheckMfxParams(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;
    mfxVideoParam corrected_params;

    memset(&corrected_params, 0, sizeof(mfxVideoParam));
    sts = mf_copy_extparam(&m_MfxParamsVideo, &corrected_params);
    if (MFX_ERR_NONE == sts)
    {
        sts = QueryENC(&m_MfxParamsVideo, &corrected_params);
        if (!m_dbg_no_SW_fallback && (MFX_WRN_PARTIAL_ACCELERATION == sts)) sts = MFX_ERR_NONE;
    }
    if (MFX_ERR_NONE == sts)
    {
        MFX_LTRACE_I(MF_TL_GENERAL, m_MfxParamsVideo.mfx.CodecProfile);
        MFX_LTRACE_I(MF_TL_GENERAL, m_MfxParamsVideo.mfx.CodecLevel);
        MFX_LTRACE_I(MF_TL_GENERAL, corrected_params.mfx.CodecProfile);
        MFX_LTRACE_I(MF_TL_GENERAL, corrected_params.mfx.CodecLevel);

        if (MFX_PROFILE_UNKNOWN != m_MfxParamsVideo.mfx.CodecProfile)
        { // check profile only if it was requested
            if (corrected_params.mfx.CodecProfile != m_MfxParamsVideo.mfx.CodecProfile)
            {
                MFX_LTRACE_S(MF_TL_GENERAL, "EP: status: invalid profile obtained");
                sts = MFX_ERR_INVALID_VIDEO_PARAM;
            }
        }
        else
        {
            m_MfxParamsVideo.mfx.CodecProfile = corrected_params.mfx.CodecProfile;
            HandleNewMessage(mtRequireUpdateMaxMBperSec, this); // ignore return value
        }
        if (MFX_LEVEL_UNKNOWN != m_MfxParamsVideo.mfx.CodecLevel)
        { // check level only if it was requested
            if (corrected_params.mfx.CodecLevel != m_MfxParamsVideo.mfx.CodecLevel)
            {
                MFX_LTRACE_S(MF_TL_GENERAL, "EP: status: invalid level obtained");
                sts = MFX_ERR_INVALID_VIDEO_PARAM;
            }
        }
        else m_MfxParamsVideo.mfx.CodecLevel = corrected_params.mfx.CodecLevel;
    }
    mfxStatus sts_tmp = mf_free_extparam(&corrected_params);
    if (MFX_ERR_NONE == sts)
        sts = sts_tmp;

    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/
// Initialize Frame Allocator

mfxStatus MFPluginVEnc::InitFRA(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;

#if MFX_D3D11_SUPPORT //TODO: refactor
    for(;;)
    {
        if (IS_MEM_VIDEO(m_uiInSurfacesMemType) ||
            IS_MEM_VIDEO(m_uiOutSurfacesMemType))
        {
            MFX_LTRACE_S(MF_TL_GENERAL, "Creating frame allocator");
            if (m_pFrameAllocator)
                break;

            CComQIPtr<ID3D11Device> d3d11Device = GetD3D11Device();
            if (!d3d11Device)
            {
                MFX_LTRACE_S(MF_TL_GENERAL, "Failed to query ID3D11Device");
                sts = MFX_ERR_NULL_PTR;
                break;
            }
#ifndef MF_D3D11_COPYSURFACES
            m_pFrameAllocator =  new MFFrameAllocator(new MFAllocatorHelper<MFD3D11FrameAllocator>);
#else //#ifndef MF_D3D11_COPYSURFACES
            m_pFrameAllocator =  new MFFrameAllocator(new MFAllocatorHelper<D3D11FrameAllocator>);
#endif //#ifndef MF_D3D11_COPYSURFACES
            D3D11AllocatorParams *pD3D11AllocParams;
            std::auto_ptr<mfxAllocatorParams> allocParams;
            allocParams.reset(pD3D11AllocParams = new D3D11AllocatorParams);

            if (NULL != pD3D11AllocParams)
            {
                pD3D11AllocParams->pDevice = d3d11Device.p;
            }

            if (!m_pFrameAllocator || !allocParams.get())
            {
                sts = MFX_ERR_MEMORY_ALLOC;
                break;
            }
            else
            {
                m_pFrameAllocator->AddRef();
            }

            sts = m_pFrameAllocator->GetMFXAllocator()->Init(allocParams.get());
            if (MFX_ERR_NONE == sts) sts = m_pMfxVideoSession->SetFrameAllocator(m_pFrameAllocator->GetMFXAllocator());
        }
        else
        {
            DXVASupportClose();
        }
        MFX_LTRACE_P(MF_TL_GENERAL, m_pFrameAllocator);
        break;
    }
#else //#if MFX_D3D11_SUPPORT
    if (m_pDeviceManager)
    {
        if (IS_MEM_VIDEO(m_uiInSurfacesMemType) ||
            IS_MEM_VIDEO(m_uiOutSurfacesMemType))
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
#endif //#if MFX_D3D11_SUPPORT
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/
// Initialize input surfaces

mfxStatus MFPluginVEnc::InitInSRF(mfxU32* pSurfacesNum)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 i = 0;

    if (m_pmfxVPP)
    {
        if (MFX_ERR_NONE == sts) sts = InitFRA();
        if ((MFX_ERR_NONE == sts) &&
            m_pFrameAllocator && IS_MEM_VIDEO(m_uiInSurfacesMemType))
        {
            mfxFrameAllocRequest request;

            memset((void*)&request, 0, sizeof(mfxFrameAllocRequest));
            memset((void*)&m_VppAllocResponse, 0, sizeof(mfxFrameAllocResponse));
            memcpy_s(&(request.Info), sizeof(request.Info),
                     &(m_VPPInitParams.vpp.In), sizeof(m_VPPInitParams.vpp.In));
            request.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET |
                           MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_FROM_VPPIN;
            request.NumFrameMin = request.NumFrameSuggested = (mfxU16)m_nInSurfacesNum;

#if MFX_D3D11_SUPPORT
            sts = m_pFrameAllocator->GetMFXAllocator()->AllocFrames(&request, &m_VppAllocResponse);
#else
            sts = m_pFrameAllocator->GetMFXAllocator()->Alloc(m_pFrameAllocator->GetMFXAllocator()->pthis,
                                                              &request, &m_VppAllocResponse);
#endif
            if (MFX_ERR_NONE == sts)
            {
                if (m_nInSurfacesNum > m_VppAllocResponse.NumFrameActual) sts = MFX_ERR_MEMORY_ALLOC;
                else
                {
                    m_nInSurfacesNum = m_VppAllocResponse.NumFrameActual;
                    if (pSurfacesNum) *pSurfacesNum = m_nInSurfacesNum;
                }
            }
        }
        if (MFX_ERR_NONE == sts && m_pFrameAllocator)
        {
            SAFE_NEW_ARRAY(m_pInSurfaces, MFYuvInSurface, m_nInSurfacesNum);
            if (!m_pInSurfaces) sts = MFX_ERR_MEMORY_ALLOC;
            else
            {
                for (i = 0; (MFX_ERR_NONE == sts) && (i < m_nInSurfacesNum); ++i)
                {
                    m_pInSurfaces[i].SetFile(m_dbg_vppin);
                    sts = m_pInSurfaces[i].Init(&(m_VPPInitParams.vpp.In),
                                                (m_bDirectConnectionMFX)? NULL: &(m_VPPInitParams_original.vpp.In),
                                                NULL,
                                                false,
#if MFX_D3D11_SUPPORT
                                                m_pD3D11Device,
#else
                                                NULL,
#endif
                                                m_pFrameAllocator);
                }
            }
            if (MFX_ERR_NONE == sts) m_pInSurface = &(m_pInSurfaces[0]);
        }
    }
    MFX_LTRACE_I(MF_TL_GENERAL, m_nInSurfacesNum);
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/
// Initialize VPP (if needed)

mfxStatus MFPluginVEnc::InitVPP(mfxU32* pSurfacesNum)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;
    bool bUseVpp = UseVPP(&(m_VPPInitParams.vpp));
    if (bUseVpp)
    {
        MFX_LTRACE_I(MF_TL_GENERAL, bUseVpp);
    }
    ATLASSERT(false == bUseVpp);
#if MFX_D3D11_SUPPORT
    CHECK_EXPRESSION(false == bUseVpp, MFX_ERR_UNKNOWN); //VPP inside of Encoder plugin is not allowed for Win8
#endif

    MFX_LTRACE_I(MF_TL_GENERAL, (pSurfacesNum)? *pSurfacesNum: 0);

    // VPP creation (if needed)
    if (MFX_ERR_NONE == sts)
    {
        m_MfxCodecInfo.m_bVppUsed = bUseVpp;
        if (bUseVpp && !m_pmfxVPP)
        {
            MFXVideoSession* session = (MFXVideoSession*)m_pMfxVideoSession;

            SAFE_NEW(m_pmfxVPP, MFXVideoVPPEx(*session));
            if (!m_pmfxVPP) sts = MFX_ERR_MEMORY_ALLOC;
        }
        if (!bUseVpp && m_pmfxVPP)
        {
            SAFE_DELETE(m_pmfxVPP);
        }
    }
    MFX_LTRACE_P(MF_TL_GENERAL, m_pmfxVPP);
    if (m_pmfxVPP)
    {
        // VPP surfaces allocation
        if (MFX_ERR_NONE == sts)
        {
            mfxFrameAllocRequest VPPRequest[2]; // [0] - input, [1] - output

            memcpy_s(&(m_VPPInitParams.vpp.Out),        sizeof(m_VPPInitParams.vpp.Out),
                     &(m_MfxParamsVideo.mfx.FrameInfo), sizeof(m_MfxParamsVideo.mfx.FrameInfo));
            memset((void*)&VPPRequest, 0, 2*sizeof(mfxFrameAllocRequest));

            sts = m_pmfxVPP->QueryIOSurf(&m_VPPInitParams, VPPRequest);
            if (!m_dbg_no_SW_fallback && (MFX_WRN_PARTIAL_ACCELERATION == sts))
            {
                sts = MFX_ERR_NONE;
                if ((dbgHwMemory != m_dbg_Memory) && IS_MEM_VIDEO(m_uiOutSurfacesMemType))
                {
                    MFX_LTRACE_S(MF_TL_GENERAL, "Switching to System Memory (VPP out = ENC in)");
                    m_uiOutSurfacesMemType    = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
                    m_VPPInitParams.IOPattern = m_uiInSurfacesMemType | m_uiOutSurfacesMemType;

                    sts = m_pmfxVPP->QueryIOSurf(&m_VPPInitParams, VPPRequest);
                    if (MFX_WRN_PARTIAL_ACCELERATION == sts) sts = MFX_ERR_NONE;
                }
            }
            if (MFX_ERR_NONE == sts)
            {
                mfxU32 in_frames_num  = MAX((VPPRequest[0]).NumFrameSuggested, MAX((VPPRequest[0]).NumFrameMin, 1));
                mfxU32 out_frames_num = MAX((VPPRequest[1]).NumFrameSuggested, MAX((VPPRequest[1]).NumFrameMin, 1));

                m_nInSurfacesNum  += MF_ADDITIONAL_SURF_MULTIPLIER * in_frames_num;
                m_nOutSurfacesNum += MF_ADDITIONAL_SURF_MULTIPLIER * out_frames_num;

                if (pSurfacesNum)
                {
                    m_nInSurfacesNum += (*pSurfacesNum) - 1;
                    *pSurfacesNum = m_nInSurfacesNum;
                }

                MFX_LTRACE_I(MF_TL_GENERAL, m_nInSurfacesNum);
                MFX_LTRACE_I(MF_TL_GENERAL, m_nOutSurfacesNum);
            }
        }
        if (MFX_ERR_NONE == sts) sts = InitInSRF(pSurfacesNum);
        // VPP initialization
        if (MFX_ERR_NONE == sts)
        {
            sts = m_pmfxVPP->Init(&m_VPPInitParams);
            if (!m_dbg_no_SW_fallback && (MFX_WRN_PARTIAL_ACCELERATION == sts)) sts = MFX_ERR_NONE;
        }
    }
    MFX_LTRACE_I(MF_TL_GENERAL, m_nInSurfacesNum);
    MFX_LTRACE_I(MF_TL_GENERAL, m_nOutSurfacesNum);
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

bool MFPluginVEnc::SendDrainCompleteIfNeeded()
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    if (m_bSendDrainComplete)
    {
        m_bSendDrainComplete = false;
        DrainComplete();
        return true;
    }
    else
    {
        return false;
    }
}

/*------------------------------------------------------------------------------*/

void MFPluginVEnc::StopProcessingDuringDCC()
{
    m_bEndOfInput = true;
    //not needed because no drain was requested outside of plugin: m_bSendDrainComplete = true;
    m_bDoNotRequestInput = true; // prevent calling RequestInput
    m_bRejectProcessInputWhileDCC = true;
    MFX_LTRACE_I(MF_TL_PERF, m_bRejectProcessInputWhileDCC);
    if (!m_bReinit)
    {
        MFX_LTRACE_S(MF_TL_NOTES, "calling AsyncThreadPush");
        AsyncThreadPush();
    }
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::HandleMessage(MFParamsManagerMessage *pMessage, MFParamsMessagePeer *pSender)
{
    UNREFERENCED_PARAMETER(pSender);
    HRESULT hr = MFParamsManager::HandleMessage(pMessage, pSender);
    switch (pMessage->GetType())
    {
    case mtRequireDCC:
        //TODO: merge dccResetWithIdrRequired and dccResetWoIdrRequired because they have same meaning now
        MFX_LTRACE_S(MF_TL_PERF, "calling RequireDynamicConfigurationChange");
        hr = RequireDynamicConfigurationChange(dccResetWithIdrRequired);
        break;
    case mtRequireUpdateMaxMBperSec:
        UINT32 nValue = 0;
        hr = m_pMaxMBperSec->UpdateMaxMBperSec(m_pmfxENC, m_MfxParamsVideo.mfx.CodecLevel, m_MfxParamsVideo.mfx.TargetUsage, m_MfxParamsVideo.mfx.GopRefDist, nValue);
        ATLASSERT(SUCCEEDED(hr));
        if (SUCCEEDED(hr))
        {
            hr = m_pAttributes->SetUINT32(MF_VIDEO_MAX_MB_PER_SEC, nValue);
        }
        break;
    };
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::RequireDynamicConfigurationChange(MFEncDynamicConfigurationChange newDccState)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MFX_LTRACE_I(MF_TL_PERF, m_DccState);
    MFX_LTRACE_I(MF_TL_PERF, newDccState);
    HRESULT hr = E_FAIL;
    //TODO: check whether any actions are required
    if ((dccResetWoIdrRequired == newDccState || dccResetWithIdrRequired == newDccState) &&
        (dccResetWithIdrRequired == m_DccState || dccResetWoIdrRequired == m_DccState || dccNotRequired == m_DccState) )
    {
        if (!m_pmfxENC)
        {
            MFX_LTRACE_P(MF_TL_PERF, m_pmfxENC);
            ATLASSERT(m_pmfxENC);
        }
        else
        {
            if (dccResetWoIdrRequired == newDccState)
            {
                //Following workaround is required because current implementation
                //uses values from GetVideoParam for MaxKbps & BufferSize,
                //which may be invalid after dynamic bitrate change.
                if (eAVEncCommonRateControlMode_CBR == m_RateControlMode.GetMode())
                {
                    //this parameter is not applicable to CBR mode
                    m_MfxParamsVideo.mfx.MaxKbps = 0;
                }
            }

            if (m_bSendDrainComplete) // reset already planned
            {
                hr = S_OK;
            }
            else
            {
                mfxStatus sts = CheckMfxParams();
                if (MFX_ERR_NONE == sts || MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts)
                {
                    if (m_DccState < newDccState)
                    {
                        if (dccNotRequired == m_DccState)
                        {
                            StopProcessingDuringDCC();
                            std::auto_ptr<MFParamsManagerMessage> pMessage( MFParamsManagerMessage::Create(mtRequireDCC) );
                            CHECK_POINTER(pMessage.get(), E_OUTOFMEMORY);
                            hr = BroadcastMessage(pMessage.get(), this);
                        }
                        m_DccState = newDccState;
                        MFX_LTRACE_I(MF_TL_PERF, m_DccState);
                    }
                    hr = S_OK;
                }
                else
                {
                    ATLASSERT(MFX_ERR_NONE == sts || MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts);
                }
            }
        }
    }
    else if (dccTypeChangeRequired == newDccState)
    {
        if (dccNotRequired == m_DccState)
        {
            MFX_LTRACE_S(MF_TL_NOTES, "resolution change. Starting drain");
            ATLASSERT(m_pmfxENC);
            if (m_pmfxENC)
            {
                if (m_bSendDrainComplete) // no need in resetting encoder during draining
                    hr = S_OK;
                else
                {
                    m_DccState = newDccState;
                    MFX_LTRACE_I(MF_TL_PERF, m_DccState);
                    StopProcessingDuringDCC();
                    hr = S_OK;
                }
            }
        }
        else if (dccTypeChangeRequired == m_DccState)
        {
            MFX_LTRACE_S(MF_TL_NOTES, "resolution change (repeated) during drain");
            hr = S_OK;
        }
        else if (dccResetWithIdrRequired == m_DccState)
        {
            MFX_LTRACE_S(MF_TL_NOTES, "resolution change (repeated) between drain complete and format change notification using ProcessOutput");
            hr = S_OK;
        }
        else
        {
            MFX_LTRACE_S(MF_TL_PERF, "unsupported state");
            ATLASSERT(false);
        }
    }
    else
    {
        MFX_LTRACE_S(MF_TL_PERF, "new state is not supported\n");
        ATLASSERT(false);
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    ATLASSERT(SUCCEEDED(hr));
    return hr;
}

/*------------------------------------------------------------------------------*/

enum {
    MFX_DCC_CHANGED_RESOLUTION     = 1 << 0, // FrameInfo: Width, Height, CropX, CropY, CropW, CropH
    MFX_DCC_CHANGED_TMP_LAYERS_CNT = 1 << 1, // ExtParam: MFX_EXTBUFF_AVC_TEMPORAL_LAYERS (mfxExtAvcTemporalLayers) Layer
    MFX_DCC_CHANGED_FRAMERATE      = 1 << 2, // FrameInfo: FrameRateExtN, FrameRateExtD
    MFX_DCC_CHANGED_BITRATE        = 1 << 3, // TargetKbps, MaxKbps
    MFX_DCC_CHANGED_OTHER_SPS      = 1 << 4, //
    MFX_DCC_CHANGED_OTHER          = 1 << 5, // BufferSizeInKB

    MFX_DCC_IDR                    = 1 << 6,

    MFX_DCC_METHOD_RESET           = 1 << 8,
    MFX_DCC_METHOD_CLOSEINIT       = 1 << 9
};

mfxU8 GetTemporalLayersCount(mfxVideoParam& param)
{
    mfxU8 nTemporalLayersCount = 0;
    for (mfxU16 nExtParam = 0; nExtParam < param.NumExtParam; ++nExtParam)
    {
        mfxExtBuffer* pExtBuffer = param.ExtParam[nExtParam];
        if (NULL != pExtBuffer && MFX_EXTBUFF_AVC_TEMPORAL_LAYERS == pExtBuffer->BufferId)
        {
            mfxExtAvcTemporalLayers* pExtAvcTemporalLayers = (mfxExtAvcTemporalLayers*)pExtBuffer;
            for (mfxU8 nLayer = 0; nLayer < 8; ++nLayer)
            {
                if (pExtAvcTemporalLayers->Layer[nLayer].Scale > 0)
                {
                    nTemporalLayersCount++;
                }
                else
                {
                    break;
                }
            }
            break;
        }
    }
    return nTemporalLayersCount;
}

mfxU32 MFPluginVEnc::CheckDccType(mfxVideoParam& oldParam, mfxVideoParam& newParam)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    mfxU32 nResult = 0;
    if (oldParam.mfx.FrameInfo.Width  != newParam.mfx.FrameInfo.Width  ||
        oldParam.mfx.FrameInfo.Height != newParam.mfx.FrameInfo.Height ||
        oldParam.mfx.FrameInfo.CropX  != newParam.mfx.FrameInfo.CropX  ||
        oldParam.mfx.FrameInfo.CropY  != newParam.mfx.FrameInfo.CropY  ||
        oldParam.mfx.FrameInfo.CropW  != newParam.mfx.FrameInfo.CropW  ||
        oldParam.mfx.FrameInfo.CropH  != newParam.mfx.FrameInfo.CropH)
    {
        MFX_LTRACE_2(MF_TL_GENERAL, "Resolution change: Width", "%d -> %d",
                    oldParam.mfx.FrameInfo.Width, newParam.mfx.FrameInfo.Width);
        MFX_LTRACE_2(MF_TL_GENERAL, "Resolution change: Height", "%d -> %d",
                    oldParam.mfx.FrameInfo.Height, newParam.mfx.FrameInfo.Height);
        MFX_LTRACE_2(MF_TL_GENERAL, "Resolution change: CropW", "%d -> %d",
                    oldParam.mfx.FrameInfo.CropW, newParam.mfx.FrameInfo.CropW);
        MFX_LTRACE_2(MF_TL_GENERAL, "Resolution change: Height", "%d -> %d",
                    oldParam.mfx.FrameInfo.CropH, newParam.mfx.FrameInfo.CropH);
        nResult |= (MFX_DCC_CHANGED_RESOLUTION | MFX_DCC_IDR | MFX_DCC_METHOD_RESET | MFX_DCC_METHOD_CLOSEINIT);
    }

    if (oldParam.mfx.CodecProfile != newParam.mfx.CodecProfile)
    {
        MFX_LTRACE_2(MF_TL_GENERAL, "Profile change: ", "%d -> %d",
                    oldParam.mfx.CodecProfile, newParam.mfx.CodecProfile);
        nResult |= (MFX_DCC_CHANGED_OTHER_SPS | MFX_DCC_IDR | MFX_DCC_METHOD_RESET | MFX_DCC_METHOD_CLOSEINIT);
    }

    mfxU8 oldTemporalLayersCount = GetTemporalLayersCount(oldParam);
    mfxU8 newTemporalLayersCount = GetTemporalLayersCount(newParam);

    if (oldTemporalLayersCount != newTemporalLayersCount)
    {
        MFX_LTRACE_2(MF_TL_GENERAL, "TemporalLayersCount change: ", "%hu -> %hu",
                    oldTemporalLayersCount, newTemporalLayersCount);
        nResult |= MFX_DCC_CHANGED_TMP_LAYERS_CNT;
        mfxU16 nMin = GetMinNumRefFrame(m_TemporalScalablity.GetMinNumRefFrame(), m_Ltr->GetBufferControlSize());
        if (m_MfxParamsVideo.mfx.NumRefFrame < nMin)
            nResult |= (MFX_DCC_METHOD_CLOSEINIT | MFX_DCC_IDR);
        else
            nResult |= MFX_DCC_METHOD_RESET; //TODO: check if next layer is base
    }

    if (oldParam.mfx.FrameInfo.FrameRateExtN != newParam.mfx.FrameInfo.FrameRateExtN ||
        oldParam.mfx.FrameInfo.FrameRateExtD != newParam.mfx.FrameInfo.FrameRateExtD)
    {
        MFX_LTRACE_2(MF_TL_GENERAL, "FrameRate change: FrameRateExtN", "%d -> %d",
                    oldParam.mfx.FrameInfo.FrameRateExtN, newParam.mfx.FrameInfo.FrameRateExtN);
        MFX_LTRACE_2(MF_TL_GENERAL, "FrameRate change: FrameRateExtD", "%d -> %d",
                    oldParam.mfx.FrameInfo.FrameRateExtD, newParam.mfx.FrameInfo.FrameRateExtD);
        nResult |= (MFX_DCC_CHANGED_FRAMERATE | MFX_DCC_IDR | MFX_DCC_METHOD_RESET /*TODO: ?| MFX_DCC_METHOD_CLOSEINIT?*/);
    }

    //TODO: needs to be aligned to MSDK compare implementation for case of BRCParamMultiplier > 1
    if (oldParam.mfx.TargetKbps != newParam.mfx.TargetKbps ||
        oldParam.mfx.MaxKbps != newParam.mfx.MaxKbps)
    {
        MFX_LTRACE_2(MF_TL_GENERAL, "Bitrate change: TargetKbps", "%d -> %d",
                    oldParam.mfx.TargetKbps, newParam.mfx.TargetKbps);
        MFX_LTRACE_2(MF_TL_GENERAL, "Bitrate change: MaxKbps", "%d -> %d",
                    oldParam.mfx.MaxKbps, newParam.mfx.MaxKbps);
        nResult |= MFX_DCC_CHANGED_BITRATE | MFX_DCC_METHOD_RESET; //TODO: requires buffer resize?
    }

    //TODO: needs to be aligned to MSDK compare implementation for case of BRCParamMultiplier > 1
    if (oldParam.mfx.BufferSizeInKB != newParam.mfx.BufferSizeInKB)
    {
        //TODO: check for VuiNalHrdParameters
        MFX_LTRACE_2(MF_TL_GENERAL, "BufferSizeInKB change: ", "%d -> %d",
                    oldParam.mfx.BufferSizeInKB, newParam.mfx.BufferSizeInKB);
        nResult |= MFX_DCC_CHANGED_OTHER | MFX_DCC_IDR | MFX_DCC_METHOD_CLOSEINIT; //TODO: requires buffer resize?
    }

    if (oldParam.mfx.NumSlice != newParam.mfx.NumSlice)
    {
        MFX_LTRACE_2(MF_TL_GENERAL, "NumSlice: ", "%d -> %d",
                    oldParam.mfx.NumSlice, newParam.mfx.NumSlice);
        nResult |= MFX_DCC_CHANGED_OTHER;
        if (m_pSliceControl->IsIdrNeeded())
            nResult |= MFX_DCC_IDR | MFX_DCC_METHOD_RESET | MFX_DCC_METHOD_CLOSEINIT; //TODO: which one?
        else
            nResult |= MFX_DCC_METHOD_RESET;
    }

    mfxU32 nDccChangedAny = MFX_DCC_CHANGED_RESOLUTION | MFX_DCC_CHANGED_FRAMERATE | MFX_DCC_CHANGED_TMP_LAYERS_CNT |
                            MFX_DCC_CHANGED_FRAMERATE | MFX_DCC_CHANGED_BITRATE |
                            MFX_DCC_CHANGED_OTHER_SPS | MFX_DCC_CHANGED_OTHER;
    mfxU32 nDccChangedSPS = MFX_DCC_CHANGED_RESOLUTION | MFX_DCC_CHANGED_FRAMERATE |
                            MFX_DCC_CHANGED_OTHER_SPS;

    if (nResult & nDccChangedAny)
    {
        if (nResult & nDccChangedSPS)
        {
            nResult |= MFX_DCC_IDR;
        }
    }
    else
    {
        bool bEqual = false;
        mfxStatus sts = mf_compare_videoparam(&oldParam, &newParam, bEqual);
        if (MFX_ERR_NONE == sts)
        {
            if (bEqual)
            {
                //TODO: why are we trying to perform DCC?
                ATLASSERT(false);
                nResult |= MFX_DCC_IDR; //useless, no Reset or Close-Init is planned
            }
            else
            {
                //TODO: we don't know what have been changed.
                ATLASSERT(false);
            }
        }
        else
        {
            //should never happen
            ATLASSERT(MFX_ERR_NONE == sts);
        }
    }

    MFX_LTRACE_I(MF_TL_PERF, nResult);
    return nResult;
}

HRESULT MFPluginVEnc::PerformDynamicConfigurationChange()
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MFX_LTRACE_I(MF_TL_PERF, m_DccState);
    HRESULT hr = S_OK;
    mfxStatus sts = MFX_ERR_NONE;

    switch (m_DccState)
    {
    case dccNotRequired:
        MFX_LTRACE_S(MF_TL_NOTES, "no actions required");
        // No actions required
        break;
    case dccTypeChangeRequired:
        MFX_LTRACE_S(MF_TL_GENERAL, "resolution change (1/2)");
        m_pAsyncThreadSemaphore->Reset();
        m_pAsyncThreadEvent->Reset();
        MFX_LTRACE_S(MF_TL_GENERAL, "Need to change format (MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE / MF_E_TRANSFORM_STREAM_CHANGE)");
        m_TypeDynamicallyChanged = true;
        MFX_LTRACE_I(MF_TL_GENERAL, m_TypeDynamicallyChanged);
        MFX_LTRACE_S(MF_TL_NOTES, "calling SendOutput");
        hr = SendOutput();
        if (SUCCEEDED(hr))
        {
            hr = ResizeSurfaces();
        }
        if (SUCCEEDED(hr))
        {
            //TODO: check new resolution against values used for initialization
            //Use dccCloseInitRequired if it is bigger
            m_DccState = dccResetWithIdrRequired;
        }
        break;
    case dccResetWoIdrRequired:
    case dccResetWithIdrRequired:
        {
            if (SUCCEEDED(hr) && NULL != m_pmfxVPP)
            {
                ATLASSERT(NULL == m_pmfxVPP);
                hr = E_FAIL;
            }

            mfxU32 dccType = 0;

            ATLASSERT(NULL != m_pmfxENC);
            if (SUCCEEDED(hr) && NULL != m_pmfxENC)
            {
                //Call m_pmfxENC->GetVideoParam to get current settings
                //then compare them and new ones using CheckDccType
                //to understand how DCC would happen.
                mfxVideoParam currentParam;
                memset(&currentParam, 0, sizeof(mfxVideoParam));

                mfxStatus sts = MFX_ERR_NONE;
                sts = mf_alloc_same_extparam(&m_MfxParamsVideo, &currentParam);
                ATLASSERT(MFX_ERR_NONE == sts);

                if (MFX_ERR_NONE == sts)
                {
                    sts = m_pmfxENC->GetVideoParam(&currentParam);
                    MFX_LTRACE_I(MF_TL_GENERAL, sts);
                    if (!m_dbg_no_SW_fallback && (MFX_WRN_PARTIAL_ACCELERATION == sts) ||
                        (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts))
                    {
                        sts = MFX_ERR_NONE;
                    }
                    ATLASSERT(MFX_ERR_NONE == sts || MFX_ERR_NOT_INITIALIZED == sts);
                }

                if (MFX_ERR_NONE == sts)
                {
                    dccType = CheckDccType(currentParam, m_MfxParamsVideo);
                }

                if (MFX_ERR_NOT_INITIALIZED == sts && !m_bInitialized)
                {
                    //Encoder already in Close-Init state, reason is unknown
                    //TODO: find solution
                    dccType = (MFX_DCC_CHANGED_RESOLUTION | MFX_DCC_CHANGED_BITRATE | MFX_DCC_IDR);
                    sts = MFX_ERR_NONE;
                }

                if (MFX_ERR_NONE != sts)
                {
                    hr = E_FAIL;
                }

                if (MFX_ERR_NONE !=  mf_free_extparam(&currentParam))
                {
                    hr = E_FAIL;
                }
            }

            if (SUCCEEDED(hr) && (dccType & MFX_DCC_CHANGED_RESOLUTION))
            {
                MFX_LTRACE_S(MF_TL_GENERAL, "resolution change (2/2)");
                m_pAsyncThreadSemaphore->Reset();
                m_pAsyncThreadEvent->Reset();
            }

            if (SUCCEEDED(hr) && NULL != m_pmfxENC &&
                (dccType & (MFX_DCC_METHOD_RESET | MFX_DCC_METHOD_CLOSEINIT)) )
            {
                hr = E_FAIL;

                if (FAILED(hr) && (dccType & MFX_DCC_METHOD_RESET))
                {
                    MFX_LTRACE_S(MF_TL_PERF, "Trying Reset:");
                    MFX_LTRACE_I(MF_TL_PERF, m_iNumberInputSamples);
                    MFX_LTRACE_I(MF_TL_PERF, m_iNumberOutputSamples);
                    sts = m_pmfxENC->Reset(&m_MfxParamsVideo);
                    MFX_LTRACE_I(MF_TL_GENERAL, sts);

                    if (!m_dbg_no_SW_fallback && (MFX_WRN_PARTIAL_ACCELERATION == sts) ||
                        (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts))
                    {
                        sts = MFX_ERR_NONE;
                    }
                    if (MFX_ERR_NONE == sts)
                    {
                        hr = S_OK;
                    }
                    ATLASSERT(MFX_ERR_NONE == sts || (dccType & MFX_DCC_METHOD_CLOSEINIT));
                }

                if (FAILED(hr) && (dccType & MFX_DCC_METHOD_CLOSEINIT))
                {
                    MFX_LTRACE_S(MF_TL_PERF, "Trying Close-Init:");

                    // TODO: remove copy-paste (also in PerformDynamicConfigurationChange)
                    // CloseCodec overwrites current encoder settings with m_ENCInitInfo_original
                    // let's update them before CloseCodec call
                    memcpy_s(&(m_ENCInitInfo_original), sizeof(m_ENCInitInfo_original),
                             &(m_MfxParamsVideo.mfx), sizeof(m_MfxParamsVideo.mfx));
                    m_AsyncDepth_original = m_MfxParamsVideo.AsyncDepth;

                    hr = TryReinit(sts);
                    MFX_LTRACE_D(MF_TL_PERF, hr);
                    ATLASSERT(SUCCEEDED(hr));
                }

                if (SUCCEEDED(hr) && (dccType & MFX_DCC_CHANGED_BITRATE)) //TODO: fix this non reliable check
                {
                    hr = ResizeBitstreamBuffer();
                }

                if (SUCCEEDED(hr) && (dccType & MFX_DCC_IDR))
                {
                    m_TemporalScalablity.ResetLayersSequence();
                    m_Ltr->ForceFrameType(MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF);
                    //TODO: notify m_Ltr about Reset or CloseInit?
                }
                ATLASSERT(SUCCEEDED(hr));
            }

            if (SUCCEEDED(hr))
            {
                m_bRejectProcessInputWhileDCC = false;
                MFX_LTRACE_I(MF_TL_PERF, m_bRejectProcessInputWhileDCC);
                m_bDoNotRequestInput = false;
                MFX_LTRACE_I(MF_TL_PERF, m_bDoNotRequestInput);
                MFX_LTRACE_I(MF_TL_PERF, m_bEndOfInput);
                ATLASSERT(!m_bEndOfInput); //should be done in AsyncThreadFunc

                MFX_LTRACE_I(MF_TL_GENERAL, m_NeedInputEventInfo.m_requested);
                MFX_LTRACE_I(MF_TL_GENERAL, m_NeedInputEventInfo.m_sent);
                if (m_NeedInputEventInfo.m_requested)
                {
                    MFX_LTRACE_S(MF_TL_GENERAL, "Not requesting input, already requested earlier");
                }
                else
                {
                    if (!m_OutputBitstream.m_bBitstreamsLimit && !m_bNeedInSurface && !m_bNeedOutSurface)
                    {
                        hr = RequestInput();
                    }
                    else
                    {
                        MFX_LTRACE_S(MF_TL_PERF, "Some flag blocks from requesting input:");
                        MFX_LTRACE_I(MF_TL_PERF, m_OutputBitstream.m_bBitstreamsLimit);
                        ATLASSERT(!m_OutputBitstream.m_bBitstreamsLimit);
                        MFX_LTRACE_I(MF_TL_PERF, m_bNeedInSurface);
                        ATLASSERT(!m_bNeedInSurface);
                        MFX_LTRACE_I(MF_TL_PERF, m_bNeedOutSurface);
                        ATLASSERT(!m_bNeedOutSurface);
                        hr = E_FAIL;
                    }
                }
            }
            if (SUCCEEDED(hr))
            {
                m_DccState = dccNotRequired;
            }
        }
        break;
    default:
        MFX_LTRACE_S(MF_TL_PERF, "unsupported state\n");
        hr = E_FAIL;
        ATLASSERT(false);
    }
    MFX_LTRACE_D(MF_TL_PERF, hr);
    ATLASSERT(SUCCEEDED(hr));
    return hr;
}
/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::ResizeSurfaces()
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;

    if (m_pInSurfaces)
    {
        MFX_LTRACE_I(MF_TL_NOTES, m_nInSurfacesNum);
        for (mfxU32 i = 0; i < m_nInSurfacesNum; ++i)
        {
            if (0 == i)
            {
                MFX_LTRACE_2(MF_TL_NOTES, "OLD: ", "%dx%d",
                                m_pInSurfaces[i].GetSurface()->Info.Width,
                                m_pInSurfaces[i].GetSurface()->Info.Height);
                MFX_LTRACE_2(MF_TL_NOTES, "NEW: ", "%dx%d",
                                m_MfxParamsVideo.mfx.FrameInfo.Width,
                                m_MfxParamsVideo.mfx.FrameInfo.Height);
            }
            ATLASSERT(
                m_pInSurfaces[i].GetSurface()->Info.Width
                != m_MfxParamsVideo.mfx.FrameInfo.Width
                ||
                m_pInSurfaces[i].GetSurface()->Info.Height
                != m_MfxParamsVideo.mfx.FrameInfo.Height);
            memcpy_s(&(m_pInSurfaces[i].GetSurface()->Info), sizeof(m_pInSurfaces[i].GetSurface()->Info),
                   &(m_MfxParamsVideo.mfx.FrameInfo), // or vpp in?
                   sizeof(m_MfxParamsVideo.mfx.FrameInfo));
        }
    }
    else
    {
        MFX_LTRACE_P(MF_TL_GENERAL, m_pInSurfaces);
    }

    if (m_pOutSurfaces)
    {
#define ALWAYS_REALLOC_SURFACES
#ifdef ALWAYS_REALLOC_SURFACES
        // releasing resources
        for (mfxU32 i = 0; i < m_nOutSurfacesNum; ++i)
        {
            if (FAILED(m_pOutSurfaces[i].Release()) && (SUCCEEDED(hr)))
            {
                hr = E_FAIL;
                ATLASSERT(SUCCEEDED(hr));
            }
            if (SUCCEEDED(hr))
            {
                m_pOutSurfaces[i].Close();
                mfxStatus sts = m_pOutSurfaces[i].Init(&(m_MfxParamsVideo.mfx.FrameInfo),
                    (m_pmfxVPP)? NULL: ((m_bDirectConnectionMFX)? NULL: &(m_VPPInitParams_original.vpp.In)),
#if MFX_D3D11_SUPPORT
                    (m_pFrameAllocator && m_EncAllocResponse.mids)? m_EncAllocResponse.mids[i]: NULL,
#else
                    (m_pmfxVPP && m_pFrameAllocator && m_EncAllocResponse.mids)? m_EncAllocResponse.mids[i]: NULL,
#endif
                    (m_pmfxVPP && (!m_pFrameAllocator || !m_EncAllocResponse.mids)),
#if MFX_D3D11_SUPPORT
                    m_pD3D11Device,
#else
                    NULL,
#endif
                    m_pFrameAllocator );
                if (MFX_ERR_NONE != sts)
                {
                    hr = E_FAIL;
                    ATLASSERT(SUCCEEDED(hr));
                }
            }
        }
#else //ALWAYS_REALLOC_SURFACES
        MFX_LTRACE_I(MF_TL_NOTES, m_nOutSurfacesNum);
        for (int i = 0; i < m_nOutSurfacesNum; ++i)
        {
            if (0 == i)
            {
                MFX_LTRACE_2(MF_TL_NOTES, "OLD: ", "%dx%d",
                                m_pOutSurfaces[i].GetSurface()->Info.Width,
                                m_pOutSurfaces[i].GetSurface()->Info.Height
                MFX_LTRACE_2(MF_TL_NOTES, "NEW: ", "%dx%d",
                                m_MfxParamsVideo.mfx.FrameInfo.Width,
                                m_MfxParamsVideo.mfx.FrameInfo.Height);
            }
            ATLASSERT(
                m_pOutSurfaces[i].GetSurface()->Info.Width
                != m_MfxParamsVideo.mfx.FrameInfo.Width
                ||
                m_pOutSurfaces[i].GetSurface()->Info.Height
                != m_MfxParamsVideo.mfx.FrameInfo.Height);
            MSDK_MEMCPY(&(m_pOutSurfaces[i].GetSurface()->Info),
                   &(m_MfxParamsVideo.mfx.FrameInfo),
                   sizeof(m_MfxParamsVideo.mfx.FrameInfo));
        }
#endif //ALWAYS_REALLOC_SURFACES
    }
    else
    {
        MFX_LTRACE_P(MF_TL_GENERAL, m_pOutSurfaces);
    }

    //TODO: ENC->Query = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM => CheckHwSupport FAIL => SetOutputType FAIL
    //m_State = stHeaderNotSet;
    //MFX_LTRACE_I(MF_TL_NOTES, m_State);
    return hr;
}
/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::ResizeBitstreamBuffer()
{
    HRESULT hr = S_OK;
    //mfxStatus sts = MFX_ERR_NONE;
    //if (m_pOutBitstreams)
    //{
    //    for (int i = 0; i < m_nOutBitstreamsNum; ++i)
    //    {
    //        SAFE_DELETE(m_pOutBitstreams[i].pMFBitstream);
    //        SAFE_DELETE(m_pOutBitstreams[i].syncPoint);
    //    }
    //}
    //SAFE_FREE(m_pOutBitstreams);
    //sts = SetFreeBitstream();
    //if (MFX_ERR_NONE != sts)
    //{
    //    hr = E_FAIL;
    //    ATLASSERT(SUCCEEDED(hr));
    //}

    return hr; //TODO: implement
}

/*------------------------------------------------------------------------------*/

bool MFPluginVEnc::IsInvalidResolutionChange(IMFMediaType* pType1, IMFMediaType* pType2)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    UINT32 parA1 = 0, parB1 = 0;
    UINT32 parA2 = 0, parB2 = 0;
    MFVideoArea area1 = {}, area2 = {};
    HRESULT hr1 = S_OK, hr2 = S_OK;

    CHECK_POINTER(pType1, false);
    CHECK_POINTER(pType2, false);
    CHECK_EXPRESSION(pType1 != pType2, false);

    if (SUCCEEDED(MFGetAttributeSize(pType2, MF_MT_FRAME_SIZE, &parA2, &parB2)) &&
        SUCCEEDED(MFGetAttributeSize(pType1, MF_MT_FRAME_SIZE, &parA1, &parB1)))
    {
        MFX_LTRACE_2(MF_TL_GENERAL, NULL, "width1 = %d, height1 = %d", parA1, parB1);
        MFX_LTRACE_2(MF_TL_GENERAL, NULL, "width2 = %d, height2 = %d", parA2, parB2);
        //CHECK_EXPRESSION((parA1 == parA2) && (parB1 == parB2), true);
    }
    //TODO: according to MSFT PM Shijun Sun:
    //MF_MT_MINIMUM_DISPLAY_APERTURE is not the same as crop and is not the same as PanScan
    //it only affects resolution and doesn't make encoder to write any cropping information in stream (VUI?)
    hr1 = pType1->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)&area1, sizeof(area1), NULL);
    hr2 = pType2->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)&area2, sizeof(area2), NULL);
    if (FAILED(hr1))
    {
        area1.OffsetX.value = 0; area1.OffsetY.value = 0;
        area1.Area.cx = parA1; area1.Area.cy = parB1;
    }
    if (FAILED(hr2))
    {
        area2.OffsetX.value = 0; area2.OffsetY.value = 0;
        area2.Area.cx = parA2; area2.Area.cy = parB2;
    }
    if (SUCCEEDED(hr1) || SUCCEEDED(hr2))
    { // this parameter may absent on the given type
        MFX_LTRACE_2(MF_TL_GENERAL, NULL, "area1.Offset: %d, %d", area1.OffsetX.value, area1.OffsetY.value);
        MFX_LTRACE_2(MF_TL_GENERAL, NULL, "area1.Area: %d x %d", area1.Area.cx, area1.Area.cy);
        MFX_LTRACE_2(MF_TL_GENERAL, NULL, "area2.Offset: %d, %d", area2.OffsetX.value, area2.OffsetY.value);
        MFX_LTRACE_2(MF_TL_GENERAL, NULL, "area2.Area: %d x %d", area2.Area.cx, area2.Area.cy);
        CHECK_EXPRESSION((area1.OffsetX.value == area2.OffsetX.value) && (area1.OffsetY.value == area2.OffsetY.value), true);
        CHECK_EXPRESSION((area1.Area.cx == area2.Area.cx) && (area1.Area.cy == area2.Area.cy), true);
    }
    if (SUCCEEDED(MFGetAttributeRatio(pType2, MF_MT_PIXEL_ASPECT_RATIO, &parA2, &parB2)) &&
        SUCCEEDED(MFGetAttributeRatio(pType1, MF_MT_PIXEL_ASPECT_RATIO, &parA1, &parB1)))
    {
        MFX_LTRACE_2(MF_TL_GENERAL, NULL, "aratio1 = %d:%d", parA1, parB1);
        MFX_LTRACE_2(MF_TL_GENERAL, NULL, "aratio2 = %d:%d", parA2, parB2);
        CHECK_EXPRESSION((parA1 == parA2) && (parB1 == parB2), true);
    }
    if (SUCCEEDED(MFGetAttributeRatio(pType2, MF_MT_FRAME_RATE, &parA2, &parB2)) &&
        SUCCEEDED(MFGetAttributeRatio(pType1, MF_MT_FRAME_RATE, &parA1, &parB1)))
    {
        MFX_LTRACE_2(MF_TL_GENERAL, NULL, "framerate1 = %d/%d", parA1, parB1);
        MFX_LTRACE_2(MF_TL_GENERAL, NULL, "framerate2 = %d/%d", parA2, parB2);
        //CHECK_EXPRESSION((parA1 == parA2) && (parB1 == parB2), true);
    }
    if (SUCCEEDED(pType2->GetUINT32(MF_MT_INTERLACE_MODE, &parA2)) &&
        SUCCEEDED(pType1->GetUINT32(MF_MT_INTERLACE_MODE, &parA1)))
    {
        MFX_LTRACE_1(MF_TL_GENERAL, NULL, "imode1 = %d", parA1);
        MFX_LTRACE_1(MF_TL_GENERAL, NULL, "imode2 = %d", parA2);
        CHECK_EXPRESSION((parA1 == parA2), true);
    }
    hr1 = pType1->GetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, &parA1);
    hr2 = pType2->GetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, &parA2);
    if (SUCCEEDED(hr1) && SUCCEEDED(hr2)) // || is wrong for this case
    { // this parameter may absent on the given type
        MFX_LTRACE_1(MF_TL_GENERAL, NULL, "NominalRange1: %d", parA1);
        MFX_LTRACE_1(MF_TL_GENERAL, NULL, "NominalRange2: %d", parA2);
        CHECK_EXPRESSION((parA1 == parA2), true);
    }
    //TODO: check for bitrate?

    MFX_LTRACE_S(MF_TL_GENERAL, "false");
    return false;
}
/*------------------------------------------------------------------------------*/

void MFPluginVEnc::TraceMediaType(mfxTraceLevel level, IMFMediaType* pType1) const
{
    UNREFERENCED_PARAMETER(level);
    UINT32 par1 = 0, par2 = 0;
    MFVideoArea area={};

    if (NULL == pType1)
    {
        MFX_LTRACE_P(level, pType1);
    }
    else
    {
        if (SUCCEEDED(MFGetAttributeSize(pType1, MF_MT_FRAME_SIZE, &par1, &par2)))
        {
            MFX_LTRACE_2((level <= MF_TL_PERF ? level : MF_TL_PERF), "FRAME_SIZE = ", MFX_TRACE_FORMAT_I "x" MFX_TRACE_FORMAT_I, par1, par2);
        }
        else
        {
            MFX_LTRACE_S((level <= MF_TL_PERF ? level : MF_TL_PERF), "FRAME_SIZE absent ");
        }

        if (SUCCEEDED(pType1->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)&area, sizeof(area), NULL)))
        { // this parameter may absent on the given type
            MFX_LTRACE_2(MF_TL_GENERAL, NULL, "MINIMUM_DISPLAY_APERTURE.Offset = %d,%d", area.OffsetX.value, area.OffsetY.value);
            MFX_LTRACE_2(MF_TL_GENERAL, NULL, "MINIMUM_DISPLAY_APERTURE.Area = %dx%d", area.Area.cx, area.Area.cy);
        }
        else
        {
            MFX_LTRACE_S((level <= MF_TL_PERF ? level : MF_TL_PERF), "MINIMUM_DISPLAY_APERTURE absent ");
        }

        if (SUCCEEDED(MFGetAttributeRatio(pType1, MF_MT_PIXEL_ASPECT_RATIO, &par1, &par2)))
        {
            MFX_LTRACE_2(level, "PIXEL_ASPECT_RATIO = ", MFX_TRACE_FORMAT_I ":" MFX_TRACE_FORMAT_I, par1, par2);
        }
        else
        {
            MFX_LTRACE_S(level, "PIXEL_ASPECT_RATIO absent ");
        }

        if (SUCCEEDED(MFGetAttributeRatio(pType1, MF_MT_FRAME_RATE, &par1, &par2)))
        {
            MFX_LTRACE_2(level, "FRAME_RATE = ", MFX_TRACE_FORMAT_I "/" MFX_TRACE_FORMAT_I, par1, par2);
        }
        else
        {
            MFX_LTRACE_S(level, "FRAME_RATE absent ");
        }

        if (SUCCEEDED(pType1->GetUINT32(MF_MT_INTERLACE_MODE, &par1)))
        {
            MFX_LTRACE_1(level, "INTERLACE_MODE = ", MFX_TRACE_FORMAT_I, par1);
        }
        else
        {
            MFX_LTRACE_S(level, "INTERLACE_MODE absent ");
        }

        if (SUCCEEDED(pType1->GetUINT32(MF_MT_AVG_BITRATE, &par1)))
        {
            MFX_LTRACE_1(level, "MF_MT_AVG_BITRATE = ", MFX_TRACE_FORMAT_I, par1);
        }
        else
        {
            MFX_LTRACE_S(level, "MF_MT_AVG_BITRATE absent ");
        }

        if (SUCCEEDED(pType1->GetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, &par1)))
        {
            MFX_LTRACE_1(level, "MF_MT_VIDEO_NOMINAL_RANGE = ", MFX_TRACE_FORMAT_I, par1);
        }
        else
        {
            MFX_LTRACE_S(level, "MF_MT_VIDEO_NOMINAL_RANGE absent ");
        }

        if (SUCCEEDED(pType1->GetUINT32(MF_MT_MPEG2_PROFILE, &par1)))
        {
            MFX_LTRACE_1(level, "MF_MT_MPEG2_PROFILE = ", MFX_TRACE_FORMAT_I, par1);
        }
        else
        {
            MFX_LTRACE_S(level, "MF_MT_MPEG2_PROFILE absent ");
        }
    }
}


// Initialize Encoder

mfxStatus MFPluginVEnc::InitENC(mfxU32* pSurfacesNum)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 i = 0;

    MFX_LTRACE_I(MF_TL_GENERAL, (pSurfacesNum)? *pSurfacesNum: 0);
    // Encoder creation
    // encoder surfaces allocation
    if (MFX_ERR_NONE == sts)
    {
        mfxFrameAllocRequest EncoderRequest;

        memset((void*)&EncoderRequest, 0, sizeof(mfxFrameAllocRequest));
        if (!m_pmfxVPP) m_uiOutSurfacesMemType = (mfxU16)MEM_IN_TO_OUT(m_uiInSurfacesMemType);
        m_MfxParamsVideo.IOPattern = (mfxU16)MEM_OUT_TO_IN(m_uiOutSurfacesMemType);

        sts = m_pmfxENC->QueryIOSurf(&m_MfxParamsVideo, &EncoderRequest);
        MFX_LTRACE_I(MF_TL_GENERAL, sts);
        ATLASSERT(MFX_ERR_NONE == sts);
        if (!m_dbg_no_SW_fallback && (MFX_WRN_PARTIAL_ACCELERATION == sts)) sts = MFX_ERR_NONE;
        if (MFX_ERR_NONE == sts)
        {
            if (1 == m_MfxParamsVideo.AsyncDepth)
            {
                //Lync mode and Low Latency mode
                m_nOutSurfacesNum = 0;
            }
            m_nOutSurfacesNum += MAX(EncoderRequest.NumFrameSuggested, MAX(EncoderRequest.NumFrameMin, 1));
            if (m_pmfxVPP)
            {
                m_nOutSurfacesNum -= 1;
            }
            else if (pSurfacesNum)
            {
                m_nOutSurfacesNum += (*pSurfacesNum) - 1;
                *pSurfacesNum = m_nOutSurfacesNum;
            }
        }
    }
    if (MFX_ERR_NONE == sts) sts = InitFRA();
    ATLASSERT(MFX_ERR_NONE == sts);
    if ((MFX_ERR_NONE == sts) && m_pFrameAllocator && IS_MEM_VIDEO(m_uiOutSurfacesMemType))
    {
        mfxFrameAllocRequest request;

        memset((void*)&request, 0, sizeof(mfxFrameAllocRequest));
        memcpy_s(&(request.Info), sizeof(request.Info),
                 &(m_MfxParamsVideo.mfx.FrameInfo),sizeof(m_MfxParamsVideo.mfx.FrameInfo));
        if (m_pmfxVPP)
        {
            request.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET |
                           MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_FROM_ENCODE;
        }
        else
        {
            if (m_bConnectedWithVpp)
            {
                request.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET |
                               MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_FROM_ENCODE;
            }
            else
            {
                request.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET |
                               MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_FROM_ENCODE;
            }
        }
        request.NumFrameMin = request.NumFrameSuggested = (mfxU16)m_nOutSurfacesNum;
        memset(&m_EncAllocResponse, 0, sizeof(mfxFrameAllocResponse));

#if MFX_D3D11_SUPPORT
        sts = m_pFrameAllocator->GetMFXAllocator()->AllocFrames(&request, &m_EncAllocResponse);
#else
        sts = m_pFrameAllocator->GetMFXAllocator()->Alloc(m_pFrameAllocator->GetMFXAllocator()->pthis,
                                                          &request, &m_EncAllocResponse);
#endif
        if (MFX_ERR_NONE == sts)
        {
            if (m_nOutSurfacesNum > m_EncAllocResponse.NumFrameActual) sts = MFX_ERR_MEMORY_ALLOC;
            else
            {
                m_nOutSurfacesNum = m_EncAllocResponse.NumFrameActual;
                if (!m_pmfxVPP && pSurfacesNum) *pSurfacesNum = m_nOutSurfacesNum;
            }
        }
    }
    ATLASSERT(MFX_ERR_NONE == sts);
    if (MFX_ERR_NONE == sts)
    {
        SAFE_NEW_ARRAY(m_pOutSurfaces, MFYuvInSurface, m_nOutSurfacesNum);
        if (!m_pOutSurfaces) sts = MFX_ERR_MEMORY_ALLOC;
        else
        {
            for (i = 0; (MFX_ERR_NONE == sts) && (i < m_nOutSurfacesNum); ++i)
            {
                m_pOutSurfaces[i].SetFile((m_pmfxVPP)? m_dbg_vppout: m_dbg_vppin);
                sts = m_pOutSurfaces[i].Init(&(m_MfxParamsVideo.mfx.FrameInfo),
                    (m_pmfxVPP)? NULL: ((m_bDirectConnectionMFX)? NULL: &(m_VPPInitParams_original.vpp.In)),
#if MFX_D3D11_SUPPORT
                    (m_pFrameAllocator && m_EncAllocResponse.mids)? m_EncAllocResponse.mids[i]: NULL,
#else
                    (m_pmfxVPP && m_pFrameAllocator && m_EncAllocResponse.mids)? m_EncAllocResponse.mids[i]: NULL,
#endif
                    (m_pmfxVPP && (!m_pFrameAllocator || !m_EncAllocResponse.mids)),
#if MFX_D3D11_SUPPORT
                    m_pD3D11Device,
#else
                    NULL,
#endif
                    m_pFrameAllocator );
            }
        }
        if (MFX_ERR_NONE == sts) m_pOutSurface = &(m_pOutSurfaces[0]);
    }
    ATLASSERT(MFX_ERR_NONE == sts);
    // Encoder initialization
    if (MFX_ERR_NONE == sts)
    {
        m_arrEncExtBuf.Free();
        m_arrEncExtBuf.UpdateExtParam(m_MfxParamsVideo);

        m_pCabac->SetCodecProfile(m_MfxParamsVideo.mfx.CodecProfile); //TODO: remove this paranoid code

        // apply m_TemporalScalablity, m_pNominalRange, etc
        HRESULT hr = UpdateVideoParam(m_MfxParamsVideo, m_arrEncExtBuf);
        if (E_NOTIMPL == hr)
            hr = S_OK;
        if (FAILED(hr))
            sts = MFX_ERR_UNKNOWN;
    }

    if (MFX_ERR_NONE == sts)
    {
        //TODO: consider enabling most of code below for case of no temporal scalability because of new WinBlue requirements

        mfxExtCodingOption *pCodingOption = m_arrEncExtBuf.FindOrCreate<mfxExtCodingOption>();
#ifdef PRIVATE_API
        mfxExtCodingOptionDDI *pCodingOptionDDI = m_arrEncExtBuf.FindOrCreate<mfxExtCodingOptionDDI>();

        if (NULL == pCodingOption || NULL == pCodingOptionDDI)
        {
#else
        if (NULL == pCodingOption)
        {
#endif
            ATLASSERT(NULL == pCodingOption);
            sts = MFX_ERR_MEMORY_ALLOC;
        }
        else
        {
            m_arrEncExtBuf.UpdateExtParam(m_MfxParamsVideo);
            if (m_TemporalScalablity.IsEnabled())
            {
                if (1 == m_MfxParamsVideo.AsyncDepth) // Lync mode
                {
                    //workaround for HCK VisVal 640, 641, 642.
                    //TODO: Consider enabling AUD in MediaSDK
                    pCodingOption->AUDelimiter = MFX_CODINGOPTION_ON;

                    // It is recommended to set mfxExtCodingOption::MaxDecFrameBuffering
                    // equal to number of reference frames.
                    pCodingOption->VuiVclHrdParameters   = MFX_CODINGOPTION_OFF; //disabling SEI
                    pCodingOption->MaxDecFrameBuffering  = m_MfxParamsVideo.mfx.NumRefFrame;
                    pCodingOption->PicTimingSEI          = MFX_CODINGOPTION_OFF; //disabling SEI
                    pCodingOption->VuiNalHrdParameters   = MFX_CODINGOPTION_OFF; //disabling SEI
                }
                // for dynamic bitrate change
                // at Win7 also for 1 reference frame. we already have no conformance because of Temporal Layers
                pCodingOption->NalHrdConformance = MFX_CODINGOPTION_OFF;

                if (MFX_RATECONTROL_CBR == m_MfxParamsVideo.mfx.RateControlMethod ||
                    MFX_RATECONTROL_VBR == m_MfxParamsVideo.mfx.RateControlMethod)
                {
                    if (0 == MfxGetBufferSizeInKB(m_MfxParamsVideo.mfx))
                    {
                        //Temporal workaround for dynamic BR issues.
                        //It is not good solution to use maximum buffer size, supported by profile/level.
                        //But real ranges for bitrate are unknown.
                        MfxSetBufferSizeInKB(m_MfxParamsVideo.mfx, 5000); //1 second of 40 Mbps
                    }
                }
            }
            else if (MFX_RATECONTROL_CQP == m_MfxParamsVideo.mfx.RateControlMethod)
            {
                // CQP mode doesn't require HRD conformance. SEI and repeatPPS are turned off to pass WBHCK VisVal quality tests
                pCodingOption->VuiVclHrdParameters   = MFX_CODINGOPTION_OFF;
                pCodingOption->PicTimingSEI          = MFX_CODINGOPTION_OFF;
                pCodingOption->VuiNalHrdParameters   = MFX_CODINGOPTION_OFF;
#ifdef PRIVATE_API
                pCodingOptionDDI->RepeatPPS          = MFX_CODINGOPTION_OFF;
#endif
            }

            // for dynamic configuration change
            pCodingOption->EndOfStream = MFX_CODINGOPTION_OFF;
        }
    }

    if (MFX_ERR_NONE == sts)
    {
        // Specify number of reference frames depending on temporal layers count & LTR buffer size
        mfxU16 nMin = GetMinNumRefFrame(m_TemporalScalablity.GetMinNumRefFrame(), m_Ltr->GetBufferControlSize());
        if (m_MfxParamsVideo.mfx.NumRefFrame < nMin)
        {
            m_MfxParamsVideo.mfx.NumRefFrame = nMin;
        }
    }
    if (MFX_ERR_NONE == sts)
    {
        sts = CheckMfxParams();
        if (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts)
        {
            sts = MFX_ERR_NONE;
        }
        ATLASSERT(MFX_ERR_NONE == sts);
    }
    if (MFX_ERR_NONE == sts)
    {
        MFX_LTRACE_2(MF_TL_PERF, "calling m_pmfxENC->Init with resolution: ", "%dx%d", m_MfxParamsVideo.mfx.FrameInfo.Width, m_MfxParamsVideo.mfx.FrameInfo.Height);
        sts = m_pmfxENC->Init(&m_MfxParamsVideo);
        MFX_LTRACE_I(MF_TL_PERF, sts);

        if (!m_dbg_no_SW_fallback && (MFX_WRN_PARTIAL_ACCELERATION == sts) ||
            (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts)) sts = MFX_ERR_NONE;
        // currently, we need to send SPS/PPS data only for H.264 encoder
        if (MFX_CODEC_AVC != m_MfxParamsVideo.mfx.CodecId)
        {
            m_State = stReady;
            MFX_LTRACE_I(MF_TL_NOTES, m_State);
        }
        ATLASSERT(MFX_ERR_NONE == sts);
    }
    if (MFX_ERR_NONE == sts)
    {
        sts = m_pmfxENC->GetVideoParam(&m_MfxParamsVideo);
        MFX_LTRACE_I(MF_TL_GENERAL, sts);
    }
    if (MFX_ERR_NONE == sts)
    {
        m_Ltr->SetNumRefFrame(m_MfxParamsVideo.mfx.NumRefFrame);
        m_Ltr->SetGopParams(m_MfxParamsVideo.mfx.GopOptFlag,
                           m_MfxParamsVideo.mfx.GopPicSize,
                           m_MfxParamsVideo.mfx.GopRefDist,
                           m_MfxParamsVideo.mfx.IdrInterval);

        m_pSliceControl->SetInitNumSlice(m_MfxParamsVideo.mfx.NumSlice);
    }

    // In case of B-frames we set DTS for output
    if (m_MfxParamsVideo.mfx.GopRefDist > 1)
        m_OutputBitstream.m_bSetDTS = true;

    // Free samples pool allocation
    if (MFX_ERR_NONE == sts)
    {
        sts = m_OutputBitstream.InitFreeSamplesPool();
    }
    if (MFX_ERR_NONE == sts) sts = m_OutputBitstream.SetFreeBitstream(MFEncBitstream::CalcBufSize(m_MfxParamsVideo));
    MFX_LTRACE_I(MF_TL_GENERAL, m_nOutSurfacesNum);
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

mfxStatus MFPluginVEnc::InitMfxVideoSession(mfxIMPL implD3D)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;

    HRESULT hr = MFCreateMfxVideoSession(&m_pMfxVideoSession);
    if (FAILED(hr))
    {
        sts = MFX_ERR_MEMORY_ALLOC;
    }
    else
    {
        // adjustment of MSDK implementation
        m_MSDK_impl =  MF_MFX_IMPL;
        if (dbgHwMsdk == m_dbg_MSDK)
            m_MSDK_impl = MFX_IMPL_HARDWARE;
        else if (dbgSwMsdk == m_dbg_MSDK)
            m_MSDK_impl = MFX_IMPL_SOFTWARE;

        m_MSDK_impl = m_MSDK_impl | implD3D;

        if (m_pD3DMutex) m_pD3DMutex->Lock();
        sts = m_pMfxVideoSession->Init(m_MSDK_impl, &g_MfxVersion);
        if (m_pD3DMutex) m_pD3DMutex->Unlock();
        MFX_LTRACE_1(MF_TL_PERF, "m_pMfxVideoSession->Init = ", "%d", sts);
        ATLASSERT(MFX_ERR_NONE == sts);

        if (MFX_ERR_NONE == sts)
        {
            sts = m_pMfxVideoSession->QueryIMPL(&m_MSDK_impl);
            MFX_LTRACE_1(MF_TL_PERF, "m_pMfxVideoSession->QueryIMPL = ", "%d", sts);
            m_MfxCodecInfo.m_MSDKImpl = m_MSDK_impl;
        }
        if (MFX_ERR_NONE == sts)
        {
            MFXVideoSession* session = (MFXVideoSession*)m_pMfxVideoSession;

            SAFE_NEW(m_pmfxENC, MFXVideoENCODE(*session));
            if (!m_pmfxENC) sts = MFX_ERR_MEMORY_ALLOC;
            MFX_LTRACE_1(MF_TL_PERF, "SAFE_NEW(m_pmfxENC, MFXVideoENCODE(*session)); = ", "%d", sts);
        }
        if (MFX_ERR_NONE != sts) hr = E_FAIL;
        MFX_LTRACE_I(MF_TL_PERF, m_MSDK_impl);
    }
    return sts;
}

/*------------------------------------------------------------------------------*/

mfxStatus MFPluginVEnc::CloseInitMfxVideoSession(mfxIMPL implD3D)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;

    if (m_pD3DMutex) m_pD3DMutex->Lock();   //this mutex will be recursively locked by InitMfxVideoSession
    SAFE_DELETE(m_pmfxVPP);
    SAFE_DELETE(m_pmfxENC);
    m_pMfxVideoSession->Close();
    SAFE_RELEASE(m_pMfxVideoSession);

    sts = InitMfxVideoSession(implD3D);
    if (m_pD3DMutex) m_pD3DMutex->Unlock(); //already unlocked in InitMfxVideoSession
    return sts;
}

/*------------------------------------------------------------------------------*/

#ifdef MFX_D3D11_SUPPORT
CComPtr<ID3D11Device> MFPluginVEnc::GetD3D11Device()
{
    CComPtr<IUnknown> pUnknown;
    //no ext addref
    pUnknown.p = GetDeviceManager();

    CComQIPtr<ID3D11Device> d3d11Device(pUnknown);
    return d3d11Device;
}
#endif

/*------------------------------------------------------------------------------*/
// Initialize codec

mfxStatus MFPluginVEnc::InitCodec(mfxU16  uiMemPattern,
                                  mfxU32* pSurfacesNum)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;

    MFX_LTRACE_D(MF_TL_GENERAL, uiMemPattern);
    MFX_LTRACE_D(MF_TL_GENERAL, (pSurfacesNum)? *pSurfacesNum: 0);

    // Deleting unneeded properties:
    // MF_MT_D3D_DEVICE - it contains pointer to enc plug-in, need to delete
    // it or enc destructor will not be invoked
    m_pInputType->DeleteItem(MF_MT_D3D_DEVICE);
    // parameters correction
    if (MFX_ERR_NONE == sts)
    {
        // adjustment of out memory type
        if (dbgSwMemory == m_dbg_Memory) m_uiOutSurfacesMemType = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

        if (pSurfacesNum)
        {
            m_uiInSurfacesMemType = (mfxU16)MEM_OUT_TO_IN(uiMemPattern);
            if (IS_MEM_VIDEO(m_uiInSurfacesMemType))
                m_bShareAllocator = true;
        }
        if ((dbgHwMemory != m_dbg_Memory) && (MFX_IMPL_SOFTWARE == m_MSDK_impl))
        {
            if (!m_bShareAllocator)
            {
                DXVASupportClose();
                m_uiInSurfacesMemType  = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
            }
            m_uiOutSurfacesMemType = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        }
        m_VPPInitParams.IOPattern  = m_uiInSurfacesMemType | m_uiOutSurfacesMemType;

        MFX_LTRACE_D(MF_TL_GENERAL, m_uiInSurfacesMemType);
        MFX_LTRACE_D(MF_TL_GENERAL, m_uiOutSurfacesMemType);
    }
    // Setting some encoder & VPP parameters
    if (MFX_ERR_NONE == sts)
    {
        mfxU32 colorFormat  = mf_get_color_format (m_pInputInfo->subtype.Data1);
        mfxU16 chromaFormat = mf_get_chroma_format(m_pInputInfo->subtype.Data1);

        if (colorFormat)
        {
            m_MfxParamsVideo.mfx.FrameInfo.FourCC = colorFormat;
            m_VPPInitParams.vpp.In.FourCC         = colorFormat;
            m_VPPInitParams.vpp.Out.FourCC        = colorFormat;

            m_MfxParamsVideo.mfx.FrameInfo.ChromaFormat = chromaFormat;
            m_VPPInitParams.vpp.In.ChromaFormat         = chromaFormat;
            m_VPPInitParams.vpp.Out.ChromaFormat        = chromaFormat;
        }
        else sts = MFX_ERR_UNSUPPORTED;
    }
    if (MFX_ERR_NONE == sts) sts = InitVPP(pSurfacesNum);
    if (MFX_ERR_NONE == sts) sts = InitENC(pSurfacesNum);
    m_MfxCodecInfo.m_InitStatus = sts;
    if (MFX_ERR_NONE != sts) CloseCodec();
    else
    {
        m_bInitialized = true;
        m_MfxCodecInfo.m_uiInFramesType     = m_uiInSurfacesMemType;
        m_MfxCodecInfo.m_uiVppOutFramesType = m_uiOutSurfacesMemType;
        MFX_LTRACE_I(MF_TL_GENERAL, m_MSDK_impl);
    }
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

void MFPluginVEnc::CloseCodec(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);

    if (m_pmfxVPP) m_pmfxVPP->Close();
    MFX_LTRACE_I(MF_TL_PERF, m_iNumberInputSamples);
    MFX_LTRACE_I(MF_TL_PERF, m_iNumberOutputSamples);
    if (m_pmfxENC) m_pmfxENC->Close();
    SAFE_DELETE_ARRAY(m_pInSurfaces);
    SAFE_DELETE_ARRAY(m_pOutSurfaces);

    m_OutputBitstream.Free(); //free m_pFreeSamplesPool, OutBitstreams, Header

    if (m_pFrameAllocator)
    {
#if MFX_D3D11_SUPPORT
        if (m_pmfxVPP && IS_MEM_VIDEO(m_uiInSurfacesMemType))
            m_pFrameAllocator->GetMFXAllocator()->FreeFrames(&m_VppAllocResponse);
        if (IS_MEM_VIDEO(m_uiOutSurfacesMemType))
            m_pFrameAllocator->GetMFXAllocator()->FreeFrames(&m_EncAllocResponse);
#else
        if (m_pmfxVPP && IS_MEM_VIDEO(m_uiInSurfacesMemType))
            m_pFrameAllocator->GetMFXAllocator()->Free(m_pFrameAllocator->GetMFXAllocator()->pthis, &m_VppAllocResponse);
        if (IS_MEM_VIDEO(m_uiOutSurfacesMemType))
            m_pFrameAllocator->GetMFXAllocator()->Free(m_pFrameAllocator->GetMFXAllocator()->pthis, &m_EncAllocResponse);
#endif
        memset((void*)&m_VppAllocResponse, 0, sizeof(mfxFrameAllocResponse));
        memset((void*)&m_EncAllocResponse, 0, sizeof(mfxFrameAllocResponse));
    }
    if (!m_bReinit)
    {
#if !MFX_D3D11_SUPPORT
        SAFE_RELEASE(m_pFrameAllocator);
#endif
        SAFE_RELEASE(m_pFakeSample);
    }
    // return parameters to initial state
    {
        if (m_Reg.pFreeParams) m_Reg.pFreeParams(&m_MfxParamsVideo);
        memset(&m_MfxParamsVideo, 0, sizeof(mfxVideoParam));
        m_Reg.pFillParams(&m_MfxParamsVideo);
        memcpy_s(&(m_MfxParamsVideo.mfx), sizeof(m_MfxParamsVideo.mfx),
                 &m_ENCInitInfo_original, sizeof(m_ENCInitInfo_original));
        m_MfxParamsVideo.AsyncDepth = m_AsyncDepth_original;
        m_arrEncExtBuf.Free();
        //TODO: Protected, IOPattern, ExtParam, NumExtParam
    }
    m_State                = (m_bReinit)? stReady: stHeaderNotSet; // no need to send header again on reinit
    MFX_LTRACE_I(MF_TL_NOTES, m_State);
#if MFX_D3D11_SUPPORT
    m_uiInSurfacesMemType  = MFX_IOPATTERN_IN_VIDEO_MEMORY;
#else
    m_uiInSurfacesMemType  = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
#endif
    m_uiOutSurfacesMemType = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    m_nInSurfacesNum       = MF_ADDITIONAL_IN_SURF_NUM;
    m_nOutSurfacesNum      = MF_ADDITIONAL_OUT_SURF_NUM;
    m_pInSurface           = NULL;
    m_pOutSurface          = NULL;

    m_iNumberInputSamples  = 0;
    m_Ltr->ResetFrameOrder();

    m_TemporalScalablity.ResetLayersSequence();

    m_DccState = dccNotRequired;

    m_bInitialized = false;
}

/*------------------------------------------------------------------------------*/

mfxStatus MFPluginVEnc::ResetCodec(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE, res = MFX_ERR_NONE;
    mfxU32 i = 0;

    m_pAsyncThreadSemaphore->Reset();
    m_pAsyncThreadEvent->Reset();

    m_OutputBitstream.DiscardCachedSamples(m_pMfxVideoSession);

    if (m_pmfxVPP)
    { // resetting encoder
        res = m_pmfxVPP->Reset(&m_VPPInitParams);
        MFX_LTRACE_I(MF_TL_GENERAL, res);
        if (MFX_ERR_NONE == sts) sts = res;
    }
    if (m_pmfxENC)
    { // resetting encoder
        MFX_LTRACE_I(MF_TL_PERF, m_iNumberInputSamples);
        MFX_LTRACE_I(MF_TL_PERF, m_iNumberOutputSamples);
        res = m_pmfxENC->Reset(&m_MfxParamsVideo);
        MFX_LTRACE_I(MF_TL_GENERAL, res);

        if (!m_dbg_no_SW_fallback && (MFX_WRN_PARTIAL_ACCELERATION == res) ||
            (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == res))
        {
            res = MFX_ERR_NONE;
        }

        if (MFX_ERR_NONE == sts) sts = res;
    }
    if (m_pInSurfaces)
    { // releasing resources
        for (i = 0; i < m_nInSurfacesNum; ++i)
        {
            if (FAILED(m_pInSurfaces[i].Release()) && (MFX_ERR_NONE == sts)) sts = MFX_ERR_UNKNOWN;
        }
        m_pInSurface = SetFreeSurface(m_pInSurfaces, m_nInSurfacesNum);
    }
    if (m_pOutSurfaces)
    { // releasing resources
        for (i = 0; i < m_nOutSurfacesNum; ++i)
        {
            if (FAILED(m_pOutSurfaces[i].Release(true)) && (MFX_ERR_NONE == sts)) sts = MFX_ERR_UNKNOWN;
        }
        m_pOutSurface = SetFreeSurface(m_pOutSurfaces, m_nOutSurfacesNum);
    }

    m_State = stReady;
    MFX_LTRACE_I(MF_TL_NOTES, m_State);

    m_bEndOfInput      = false;
    m_bStartDrain      = false;
    m_bStartDrainEnc   = false;
    m_bNeedInSurface   = false;
    m_bNeedOutSurface  = false;

    if (dccResetWoIdrRequired != m_DccState)
    {
        //TODO: force IDR?
        m_TemporalScalablity.ResetLayersSequence();
        m_Ltr->ForceFrameType(MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF);
        //TODO: notify m_Ltr about Reset or CloseInit?
    }
    //TODO: reset dcc flag?

#if !MFX_D3D11_SUPPORT
    if (!m_bStreamingStarted)
#endif
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
    m_iNumberInputSamples = 0;
    m_Ltr->ResetFrameOrder();

    m_LastSts   = MFX_ERR_NONE;
    m_SyncOpSts = MFX_ERR_NONE;

#if MFX_D3D11_SUPPORT
    //to no run async thread twice in case of message command flush
    if (MFT_MESSAGE_COMMAND_FLUSH == m_eLastMessage)
    {
        m_pAsyncThreadEvent->Signal();
    }
#endif
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::TryReinit(mfxStatus& sts)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    HRESULT hr = S_OK;

    sts = MFX_ERR_NONE;
    CloseCodec();
    m_bReinit = false;
    m_OutputBitstream.m_bSetDiscontinuityAttribute = true;
    SAFE_RELEASE(m_pFakeSample);
    /* Releasing here fake sample we will invoke SampleAppeared function
     * in upstream vpp (or decoder) plug-in and it will continue processing.
     */
    if (!m_bDoNotRequestInput && !m_NeedInputEventInfo.m_requested) hr = RequestInput();
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

mfxStatus MFPluginVEnc::InitPlg(IMfxVideoSession* pSession,
                                mfxVideoParam*    pVideoParams,
                                mfxU32*           pSurfacesNum)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);
    mfxStatus sts = MFX_ERR_NONE;

    if (FAILED(m_hrError)) sts = MFX_ERR_UNKNOWN;
    if ((MFX_ERR_NONE == sts) && !pSession) sts = MFX_ERR_NULL_PTR;
    if ((MFX_ERR_NONE == sts) && !pVideoParams) sts = MFX_ERR_NULL_PTR;
    if ((MFX_ERR_NONE == sts) && !pSurfacesNum) sts = MFX_ERR_NULL_PTR;
    if (MFX_ERR_NONE == sts)
    {
        m_bDirectConnectionMFX = true;
        m_bConnectedWithVpp = (pVideoParams->IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_IN_SYSTEM_MEMORY)) &&
                              (pVideoParams->IOPattern & (MFX_IOPATTERN_OUT_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY));
        if (!m_bConnectedWithVpp && !(IS_MEM_OUT(pVideoParams->IOPattern)))
        {
            sts = MFX_ERR_UNSUPPORTED;
        }
    }
    if (MFX_ERR_NONE == sts)
    { // correcting of parameters obtained on plug-ins connection
        if (m_bConnectedWithVpp)
        {
            if ((m_VPPInitParams.vpp.In.Width != pVideoParams->vpp.Out.Width) ||
                (m_VPPInitParams.vpp.In.Height != pVideoParams->vpp.Out.Height))
            {
                sts = MFX_ERR_UNKNOWN;
            }
            /* We should not copy vpp info here because it may contain letter boxing settings which
             * will be interpreted by encoder as crop area, instead encoder should process the whole
             * frame with letter boxing:
             *   memcpy(&(m_VPPInitParams.vpp.In), &(pVideoParams->vpp.Out), sizeof(mfxFrameInfo));
             */
        }
        else
        {
            memcpy_s(&(m_VPPInitParams.vpp.In), sizeof(m_VPPInitParams.vpp.In),
                     &(pVideoParams->mfx.FrameInfo), sizeof(pVideoParams->mfx.FrameInfo));
        }
    }
    if ((MFX_ERR_NONE == sts) && pSession)
    {
        MFXVideoVPPEx*  vpp = NULL;
        MFXVideoENCODE* enc = NULL;
        MFXVideoSession* session = (MFXVideoSession*)pSession;

        if (m_pmfxVPP)
        { // vpp may be not needed
            SAFE_NEW(vpp, MFXVideoVPPEx(*session));
        }
        SAFE_NEW(enc, MFXVideoENCODE(*session));
        if ((m_pmfxVPP && !vpp) || !enc)
        {
            SAFE_DELETE(vpp);
            SAFE_DELETE(enc);
            sts = MFX_ERR_MEMORY_ALLOC;
        }
        if (MFX_ERR_NONE == sts)
        {
            if (m_pmfxVPP)
            {
                m_pmfxVPP->Close();
                SAFE_DELETE(m_pmfxVPP);
            }
            if (m_pmfxENC)
            {
                m_pmfxENC->Close();
                SAFE_DELETE(m_pmfxENC);
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
            m_pmfxENC = enc;
        }
    }
    if (MFX_ERR_NONE == sts)
    {
        // changing original geometry for direct connection of decoder & encoder
        memcpy_s(&(m_VPPInitParams_original.vpp.In), sizeof(m_VPPInitParams_original.vpp.In),
                 &(m_VPPInitParams.vpp.In), sizeof(m_VPPInitParams.vpp.In));
        // setting cropping parameters (letter & pillar boxing)
        mf_set_cropping(&(m_VPPInitParams.vpp)); // resynchronisation with enc parameters is not needed
    }
    // encoder initialization
    if (MFX_ERR_NONE == sts)
    {
        mfxU16 mem_pattern = pVideoParams->IOPattern & (MFX_IOPATTERN_OUT_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY);
        sts = InitCodec(mem_pattern, pSurfacesNum);
    }
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

inline MFYuvInSurface* MFPluginVEnc::SetFreeSurface(MFYuvInSurface* pSurfaces,
                                                    mfxU32 nSurfacesNum)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MFYuvInSurface* pSurface = NULL;
    HRESULT hr = S_OK;
    mfxU32 i = 0;
    bool bFound = false;

    ATLASSERT(NULL != pSurfaces);
    CHECK_POINTER(pSurfaces,  NULL);

    for (i = 0; i < nSurfacesNum; ++i)
    {
        CHECK_POINTER(pSurfaces+i,  NULL);
        hr = pSurfaces[i].Release();
        // if release failed then surface could not be unlocked yet,
        // that's not an error
        if (SUCCEEDED(hr) && !bFound)
        {
            bFound = true;
            pSurface = &(pSurfaces[i]);
        }
    }
    MFX_LTRACE_P(MF_TL_GENERAL, pSurface);
    return pSurface;
}

/*------------------------------------------------------------------------------*/

void MFPluginVEnc::SetPlgError(HRESULT hr, mfxStatus sts)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    MFX_LTRACE_I(MF_TL_GENERAL, sts);

    MFPlugin::SetPlgError(hr, sts);
    ATLASSERT(SUCCEEDED(hr));
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
        ReportError();
    }
    return;
}

/*------------------------------------------------------------------------------*/

void MFPluginVEnc::HandlePlgError(MyAutoLock& lock, bool bCalledFromAsyncThread)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    ATLASSERT(SUCCEEDED(m_hrError));
    if (FAILED(m_hrError))
    {
        if (m_bErrorHandlingFinished)
        {
            if (m_bEndOfInput)
            {
                if (SendDrainCompleteIfNeeded())
                {
                    m_bEndOfInput = false;
                }
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
                MFX_LTRACE_S(MF_TL_NOTES, "calling m_pAsyncThreadEvent->Signal");
                m_pAsyncThreadEvent->Signal();
                m_pAsyncThreadSemaphore->Post();
                // awaiting while error will be detected by async thread
                lock.Unlock();
                m_pErrorFoundEvent->Wait();
                m_pErrorFoundEvent->Reset();
                lock.Lock();
            }
            // resetting
            ResetCodec(); // status is not needed here

            if ( ! SendDrainCompleteIfNeeded() )
            {
                if (m_bReinit)
                {
                    mfxStatus sts = MFX_ERR_NONE;

                    TryReinit(sts); //TODO: no check from return result
                }
                else
                { // forcing calling of ProcessInput on errors during processing
                    if (!m_bDoNotRequestInput && !m_NeedInputEventInfo.m_requested) RequestInput();
                }
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

bool MFPluginVEnc::ReturnPlgError(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    if (m_bDirectConnectionMFX) return false;
    return (m_dbg_return_errors)? true: false;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::AsyncThreadFunc(void)
{
    { MFX_AUTO_LTRACE(MF_TL_PERF, "ThreadName=AT ENC"); }
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);
    mfxStatus sts = MFX_ERR_NONE;
    MFEncOutData* pDispBitstream = NULL;

    while (1)
    {
        HRESULT hr = S_OK;

        // awaiting for the signal that next surface is ready
        lock.Unlock();
        {
            MFX_AUTO_LTRACE(MF_TL_PERF, "wait::frame (m_pAsyncThreadSemaphore, until next surface is ready");
            m_pAsyncThreadSemaphore->Wait();
        }
        lock.Lock();
        if (m_bAsyncThreadStop) break;
        if (FAILED(m_hrError) && m_bErrorHandlingStarted && !m_bErrorHandlingFinished)
        {
            // sending event that error was detected and thread was stopped
            m_pErrorFoundEvent->Signal();
            // awaiting while error will be handled
            lock.Unlock();
            {
                MFX_AUTO_LTRACE(MF_TL_PERF, "m_pErrorHandledEvent->Wait()... (until error is handled)");
                m_pErrorHandledEvent->Wait();
            }
            m_pErrorHandledEvent->Reset();
            lock.Lock();
            continue;
        }
        ATLASSERT(SUCCEEDED(hr) && SUCCEEDED(m_hrError));
        //TODO: this is the only external use of m_OutputBitstream.m_uiHasOutputEventExists
        //investigate if possible to replace with m_OutputBitstream.HaveBitstreamToDisplay()
        //in case of FAILED(hr) in begin of ProcessOutput m_OutputBitstream.m_uiHasOutputEventExists
        //may be decreased to 0 while there output bitstreams still exist.
        //On the other hand this will SetPlgError
        if (SUCCEEDED(m_hrError) && m_bEndOfInput && !m_OutputBitstream.m_uiHasOutputEventExists)
        {
            //MediaSDK doesn't support encoding after drain (EncodeFrameAsync(NULL)) without Reset or Close-Init.
            //if m_iNumberInputSamples == 0, we don't need to start drain.
            if (m_bInitialized && m_iNumberInputSamples > 0)
            {
                MFX_LTRACE_S(MF_TL_NOTES, "start drain");
                m_bStartDrain = true;
                if (!m_pmfxVPP) m_bStartDrainEnc = true;
                hr = ProcessFrame(sts);
            }
            else hr = MF_E_TRANSFORM_NEED_MORE_INPUT;

            if (MF_E_TRANSFORM_NEED_MORE_INPUT == hr)
            {
                hr = S_OK;
                MFX_LTRACE_S(MF_TL_NOTES, "drain complete");

                m_bEndOfInput = false;
                m_bStartDrain = false;
                m_bStartDrainEnc = false;

                if (m_bReinit) hr = TryReinit(sts);

                if (SUCCEEDED(hr))
                {
                    hr = PerformDynamicConfigurationChange();
                }

                if (SUCCEEDED(hr))
                {
                    SendDrainCompleteIfNeeded(); //ignore result
                    continue;
                }
            }
            else if (SUCCEEDED(hr))
            {
                MFX_LTRACE_S(MF_TL_NOTES, "calling AsyncThreadPush()");
                AsyncThreadPush();
                continue;
            }
        }
        ATLASSERT(SUCCEEDED(hr) && SUCCEEDED(m_hrError));
        if (SUCCEEDED(hr) && SUCCEEDED(m_hrError))
        {
            pDispBitstream = m_OutputBitstream.GetDispBitstream();
            if (NULL == pDispBitstream) //should never happen
            {
                hr = E_POINTER;
            }
        }
        ATLASSERT(SUCCEEDED(hr) && SUCCEEDED(m_hrError));
        if (SUCCEEDED(hr) && SUCCEEDED(m_hrError))
        {

            // awaiting for the async operation completion if needed
            //TODO: replace with bSynchronized to avoid using result of unfinished SyncOperation
            if (pDispBitstream->bSyncPointUsed)
            {
                sts = pDispBitstream->iSyncPointSts;
                MFX_LTRACE_S(MF_TL_NOTES, "using saved sts");
                MFX_LTRACE_I(MF_TL_NOTES, sts);
            }
            else
            {
                lock.Unlock();
                {
                    MFX_AUTO_LTRACE(MF_TL_PERF, "wait::SyncOperation");
                    sts = pDispBitstream->SyncOperation(m_pMfxVideoSession);
                    //sts = MFX_ERR_NONE;
                }
                MFX_LTRACE_I(MF_TL_PERF, sts);
                m_SyncOpSts = sts;
                // sending event that some resources may be free
                m_pDevBusyEvent->Signal();
                lock.Lock();
                ATLASSERT(pDispBitstream->bSynchronized || pDispBitstream->bFreeData);
            }
            // sending request for ProcessOutput call
            ATLASSERT(MFX_ERR_NONE == sts);

            if (MFT_MESSAGE_COMMAND_FLUSH != m_eLastMessage)
            {
                mfxBitstream* bs = pDispBitstream->pMFBitstream->GetMfxBitstream();
                if (NULL == bs || 0 == bs->DataLength)
                {
                    MFX_LTRACE_S(MF_TL_NOTES, "WARNING: zero data length!");
                    ATLASSERT(!"WARNING: zero data length!");
                }
                if (MFX_ERR_NONE == sts)
                {
                    MFX_LTRACE_S(MF_TL_NOTES, "calling SendOutput");
                    hr = SendOutput();
                }
                else hr = E_FAIL;
            }
        }
        ATLASSERT(SUCCEEDED(hr) && SUCCEEDED(m_hrError));
        if (SUCCEEDED(hr) && SUCCEEDED(m_hrError))
        {
            // awaiting for ProcessOuput completion
            lock.Unlock();
            {
                MFX_AUTO_LTRACE(MF_TL_PERF, "wait::ProcessOutput (m_pAsyncThreadEvent->Waiting, ProcessOutput complete)");
                m_pAsyncThreadEvent->Wait();
            }
            m_pAsyncThreadEvent->Reset();
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

HRESULT MFPluginVEnc::UpdateAVCRefListCtrl(mfxEncodeCtrl* &pEncodeCtrl)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    if (m_Ltr->IsEnabled())
    {
        if (NULL == pEncodeCtrl) //may be NULL if there is no Temporal Scalability
        {
            // allocate own new one
            SAFE_NEW(pEncodeCtrl, mfxEncodeCtrl);
            if ( NULL != pEncodeCtrl )
            {
                memset( pEncodeCtrl, 0, sizeof(mfxEncodeCtrl) );
            }
            else
            {
                ATLASSERT(NULL != pEncodeCtrl);
                hr = E_OUTOFMEMORY;
            }
        }
        else
        {
            if (0 != pEncodeCtrl->FrameType)
            {
                m_Ltr->ForceFrameType(pEncodeCtrl->FrameType);
            }
        }

        if ( SUCCEEDED(hr) )
        {
            hr = m_Ltr->GenerateFrameEncodeCtrlExtParam(*pEncodeCtrl);
        }
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::ProcessFrameEnc(mfxStatus& sts)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    HRESULT hr = S_OK;
    mfxFrameSurface1* pOutFrameSurface = NULL;

    sts = MFX_ERR_NONE;
    pOutFrameSurface = (m_bStartDrainEnc)? NULL : m_pOutSurface->GetSurface();

    if (SUCCEEDED(hr))
    {
        if (m_pOutSurface && m_pmfxVPP) m_pOutSurface->DropDataToFile();
        if (pOutFrameSurface) pOutFrameSurface->Data.FrameOrder = m_iNumberInputSamples;

        do
        {
            mfxEncodeCtrl * pEncodeCtrl = NULL;
            if ( NULL != m_pOutSurface )
            {
                pEncodeCtrl = m_pOutSurface->GetEncodeCtrl();
                // The pEncodeCtrl will be deleted with releasing the surface.
            }

            MFEncOutData* pOutBitstream = m_OutputBitstream.GetOutBitstream();
            ATLASSERT(NULL != pOutBitstream);
            CHECK_POINTER(pOutBitstream, MFX_ERR_NULL_PTR);               //should not be NULL unless thread unsafe usage
            ATLASSERT(NULL != pOutBitstream->pMFBitstream);
            CHECK_POINTER(pOutBitstream->pMFBitstream, MFX_ERR_NULL_PTR); //is checked inside of MFOutputBitstream
            if (!m_bStartDrainEnc)
                UpdateAVCRefListCtrl(pEncodeCtrl);
            MFX_LTRACE_S(MF_TL_NOTES, "Calling m_pmfxENC->EncodeFrameAsync");
            //TODO: extract method sts = m_OutputBitstream.EncodeFrameAsync(pEncodeCtrl, pOutFrameSurface, m_pmfxENC);
            ATLASSERT(!pOutBitstream->bSynchronized);
            sts = m_pmfxENC->EncodeFrameAsync(pEncodeCtrl, pOutFrameSurface,
                pOutBitstream->pMFBitstream->GetMfxBitstream(), &pOutBitstream->syncPoint);

            MFX_LTRACE_P(MF_TL_NOTES, pEncodeCtrl);
            MFX_LTRACE_P(MF_TL_NOTES, pOutFrameSurface);
            MFX_LTRACE_I(MF_TL_NOTES, sts);
            if (NULL != pOutFrameSurface)
            {
                MFX_LTRACE_2(MF_TL_NOTES, "pOutFrameSurface resolution: ", "%dx%d",
                                pOutFrameSurface->Info.Width, pOutFrameSurface->Info.Height);
            }

            if ((MFX_WRN_DEVICE_BUSY == sts) && (MFX_ERR_NONE == m_SyncOpSts))
            {
                MFX_AUTO_LTRACE(MF_TL_PERF, "mfx(EncodeFrameAsync)::MFX_WRN_DEVICE_BUSY");
                mfxStatus res = MFX_ERR_NONE;

                if (!m_OutputBitstream.HandleDevBusy(res, m_pMfxVideoSession))
                {
                    m_pDevBusyEvent->TimedWait(MF_DEV_BUSY_SLEEP_TIME);
                    m_pDevBusyEvent->Reset();
                }
                if (MFX_ERR_NONE != res) sts = res;
            }
        } while ((MFX_WRN_DEVICE_BUSY == sts) && (MFX_ERR_NONE == m_SyncOpSts));
        // valid cases are:
        // MFX_ERR_NONE - data processed, output will be generated
        // MFX_ERR_MORE_DATA - data buffered, output will not be generated
        // MFX_WRN_INCOMPATIBLE_VIDEO_PARAM - frame info is not synchronized with initialization one

        if (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts) sts = MFX_ERR_NONE; // ignoring this warning

        if ((MFX_ERR_MORE_DATA == sts) && m_bStartDrainEnc) hr = MF_E_TRANSFORM_NEED_MORE_INPUT;
        else if ((MFX_ERR_NONE == sts) || (MFX_ERR_MORE_DATA == sts))
        {
            bool bOutExists = (MFX_ERR_NONE == sts);

            ++m_iNumberInputSamples;
            m_Ltr->IncrementFrameOrder();

            if (bOutExists)
            {
                sts = m_OutputBitstream.UseBitstream(MFEncBitstream::CalcBufSize(m_MfxParamsVideo));
                MFX_LTRACE_S(MF_TL_NOTES, "calling AsyncThreadPush");
                AsyncThreadPush();
                if (SUCCEEDED(hr) && (MFX_ERR_NONE != sts))
                {
                    hr = E_FAIL;
                    ATLASSERT(SUCCEEDED(hr));
                }
            }
            if (!m_pmfxVPP || m_bStartDrainEnc)
            {
                // trying to set free surface
                m_pOutSurface = SetFreeSurface(m_pOutSurfaces, m_nOutSurfacesNum);
                if (!m_pOutSurface) m_bNeedOutSurface = true;
                MFX_LTRACE_I(MF_TL_PERF, m_bNeedOutSurface);
            }
        }
        else
        {
            hr = E_FAIL;
            ATLASSERT(SUCCEEDED(hr));
        }
    }
    MFX_LTRACE_I(MF_TL_PERF, sts);
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::ProcessFrameVpp(mfxStatus& sts)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);

    ATLASSERT(false);
    HRESULT hr = S_OK;
    mfxFrameSurface1* pInFrameSurface  = NULL;
    mfxFrameSurface1* pOutFrameSurface = NULL;

    sts = MFX_ERR_MORE_SURFACE;
    pInFrameSurface  = (m_bStartDrain)? NULL : m_pInSurface->GetSurface();

    while (SUCCEEDED(hr) && (MFX_ERR_MORE_SURFACE == sts) && !m_bNeedOutSurface)
    {
        m_pOutSurface->PseudoLoad();
        pOutFrameSurface = m_pOutSurface->GetSurface();
        do
        {
            m_LastSts = sts = m_pmfxVPP->RunFrameVPPAsync(pInFrameSurface,
                                                          pOutFrameSurface,
                                                          NULL,
                                                          m_pSyncPoint);
            if ((MFX_WRN_DEVICE_BUSY == sts) && (MFX_ERR_NONE == m_SyncOpSts))
            {
                MFX_AUTO_LTRACE(MF_TL_PERF, "mfx(RunFrameVPPAsync)::MFX_WRN_DEVICE_BUSY");
                mfxStatus res = MFX_ERR_NONE;

                if (!m_OutputBitstream.HandleDevBusy(res, m_pMfxVideoSession))
                {
                    m_pDevBusyEvent->TimedWait(MF_DEV_BUSY_SLEEP_TIME);
                    m_pDevBusyEvent->Reset();
                }
                if (MFX_ERR_NONE != res) sts = res;
            }
        } while ((MFX_WRN_DEVICE_BUSY == sts) && (MFX_ERR_NONE == m_SyncOpSts));
        // valid cases for the status are:
        // MFX_WRN_DEVICE_BUSY - no buffers in HW device, need to wait; treating as MFX_ERR_MORE_DATA
        // MFX_ERR_NONE - data processed, output will be generated
        // MFX_ERR_MORE_DATA - data buffered, output will not be generated

        if ((MFX_ERR_MORE_DATA == sts) && m_bStartDrain)
        {
            m_bStartDrainEnc = true;
            hr = ProcessFrameEnc(sts);
            break;
        }
        else if ((MFX_ERR_NONE == sts) || (MFX_ERR_MORE_DATA == sts) || (MFX_ERR_MORE_SURFACE == sts))
        {
            HRESULT hr_sts = S_OK;
            bool bOutExists = ((MFX_ERR_NONE == sts) || (MFX_ERR_MORE_SURFACE == sts));

            if (bOutExists)
            {
#ifdef OMIT_VPP_SYNC_POINT
                ++(m_MfxCodecInfo.m_nVppOutFrames);
#else
                sts = m_pMfxSession->SyncOperation(*m_pSyncPoint, INFINITE);
                if (MFX_ERR_NONE == sts) ++(m_MfxCodecInfo.m_nVppOutFrames);
                else hr_sts = E_FAIL;
#endif
                if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
                if (SUCCEEDED(hr))
                {
                    mfxStatus res = MFX_ERR_NONE;

                    hr_sts = ProcessFrameEnc(res);
                    if (FAILED(hr_sts)) sts = res;
                }
                if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
            }
            if (pOutFrameSurface->Data.Locked || bOutExists)
            {
                // setting free out surface
                m_pOutSurface = SetFreeSurface(m_pOutSurfaces, m_nOutSurfacesNum);
                if (m_bStartDrain)
                {
                    if (SUCCEEDED(hr) && !m_pOutSurface) hr = E_FAIL;
                }
                else
                { // will wait for free surface
                    if (!m_pOutSurface) m_bNeedOutSurface = true;
                    MFX_LTRACE_I(MF_TL_PERF, m_bNeedOutSurface);
                }
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
            // trying to set free input surface
            m_pInSurface = SetFreeSurface(m_pInSurfaces, m_nInSurfacesNum);
            if (SUCCEEDED(hr) && !m_pInSurface) m_bNeedInSurface = true;
        }
        MFX_LTRACE_I(MF_TL_PERF, m_bNeedInSurface);
    }
    MFX_LTRACE_I(MF_TL_PERF, sts);
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::ProcessFrame(mfxStatus& sts)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    HRESULT hr = S_OK;

    if (m_bInitialized)
    {
        if (m_pmfxVPP && !m_bStartDrainEnc)
        {
            hr = ProcessFrameVpp(sts);
        }
        else
        {
            hr = ProcessFrameEnc(sts);
        }
    }
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::ProcessInput(DWORD      dwInputStreamID,
                                   IMFSample* pSample,
                                   DWORD      dwFlags)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MFTicker ticker(&m_ticks_ProcessInput);
    //TODO: fix this workaround by buffering input in queue.
    long i = 5000;
    while(m_bRejectProcessInputWhileDCC && i--)
    {
        Sleep(1);
    }
    if (m_bRejectProcessInputWhileDCC)
    {
        MFX_LTRACE_S(MF_TL_PERF, "timeout while waiting reset of flag m_bRejectProcessInputWhileDCC");
        ATLASSERT(!m_bRejectProcessInputWhileDCC);
    }

    MyAutoLock lock(m_CritSec);
    HRESULT hr = S_OK;
    mfxStatus sts = MFX_ERR_NONE;

    GetPerformance();
    hr = MFPlugin::ProcessInput(dwInputStreamID, pSample, dwFlags);
    CHECK_EXPRESSION(SUCCEEDED(hr), hr);

    CHECK_EXPRESSION(!m_bRejectProcessInputWhileDCC, MF_E_NOTACCEPTING);

#if MFX_D3D11_SUPPORT
    if (NULL == pSample)
    {
        return RequestInput();
    }
#endif
    SET_HR_ERROR(0 == dwFlags, hr, E_INVALIDARG);
    ATLASSERT(!m_bEndOfInput);
    SET_HR_ERROR(!m_bEndOfInput, hr, E_FAIL);
    SET_HR_ERROR(stHeaderSetAwaiting != m_State, hr, MF_E_TRANSFORM_TYPE_NOT_SET);
    ATLASSERT(SUCCEEDED(hr));

    if (SUCCEEDED(hr)) hr = m_hrError;
    ATLASSERT(SUCCEEDED(hr));
    // Encoder initialization
    if (SUCCEEDED(hr) && !m_bInitialized)
    {
        sts = InitCodec();
        if (MFX_ERR_NONE != sts)
        {
            hr = E_FAIL;
            ATLASSERT(SUCCEEDED(hr));
        }
    }
    if (SUCCEEDED(hr) && !m_pmfxVPP) m_pInSurface = m_pOutSurface;
    if (SUCCEEDED(hr) && NULL == m_pInSurface)
    {
        MFX_LTRACE_P(MF_TL_PERF, m_pInSurface);
        ATLASSERT(NULL != m_pInSurface);
        hr = E_FAIL;
        MFX_LTRACE_D(MF_TL_PERF, hr);
    }
    if (SUCCEEDED(hr))
    {
        hr = m_pInSurface->Load(pSample, m_bDirectConnectionMFX);
//#define WORKAROUND_FOR_INPUT_SAMPLE_0_BUFFERS E_INVALIDARG
#define WORKAROUND_FOR_INPUT_SAMPLE_0_BUFFERS S_OK
#ifdef WORKAROUND_FOR_INPUT_SAMPLE_0_BUFFERS
        if (FAILED(hr))
        {
            m_OutputBitstream.CheckBitstreamsNumLimit();
            // requesting input while free surfaces (in & out) are available
            if (!m_bDoNotRequestInput && !m_NeedInputEventInfo.m_requested &&
                !m_bNeedInSurface && !m_bNeedOutSurface && !m_OutputBitstream.m_bBitstreamsLimit)
            {
                hr = RequestInput();
                ATLASSERT(SUCCEEDED(hr));
                if (SUCCEEDED(hr))
                {
                    MFX_LTRACE_S(MF_TL_PERF, "return WORKAROUND_FOR_INPUT_SAMPLE_0_BUFFERS");
                    MFX_LTRACE_D(MF_TL_PERF, WORKAROUND_FOR_INPUT_SAMPLE_0_BUFFERS);
                    return WORKAROUND_FOR_INPUT_SAMPLE_0_BUFFERS;
                }
            }
        }
#endif //WORKAROUND_FOR_INPUT_SAMPLE_0_BUFFERS
    }
    ATLASSERT(SUCCEEDED(hr));

#if MFX_D3D11_SUPPORT
    // In case of B-frames we set DTS for output
    if (m_OutputBitstream.m_bSetDTS)
    {
        if (SUCCEEDED(hr) && NULL != pSample)
        {
            LONGLONG hnsTime = MS_TIME_STAMP_INVALID;
            if (FAILED(pSample->GetSampleTime(&hnsTime)))
            {
                hnsTime = MS_TIME_STAMP_INVALID;
            }
            MFX_LTRACE_I(MF_TL_GENERAL, hnsTime);
            m_OutputBitstream.m_arrDTS.push(hnsTime);
        }
    }
#endif

    if (SUCCEEDED(hr) && !m_pInSurface->IsFakeSrf()) ++(m_MfxCodecInfo.m_nInFrames);
    ATLASSERT(SUCCEEDED(hr));
    if (SUCCEEDED(hr))
    {
        if (m_pInSurface->IsFakeSrf())
        {
            // saving this sample
            pSample->AddRef();
            m_pFakeSample = pSample;

            // setting flags to process this situation
            m_bReinit     = true;
            m_bEndOfInput = true; // we need to obtain buffered frames
            // pushing async thread (starting call EncodeFrameAsync with NULL bitstream)
            MFX_LTRACE_S(MF_TL_NOTES, "calling AsyncThreadPush");
            AsyncThreadPush();
        }
        else
        {
            // Prepare mfxEncodeCtrl to set frame qp & force key frames.
            // It's needs to be keeped untouched while the surface is processed,
            // so it will be associated with the surface.
            mfxEncodeCtrl * pEncodeCtrl = m_TemporalScalablity.GenerateFrameEncodeCtrl();
            m_pOutSurface->SetEncodeCtrl(pEncodeCtrl); //now m_pOutSurface manages pEncodeCtrl's memory
            //TODO: need another approach in case of NULL != m_pmfxVPP

            //copy input sample attributes for applying them to output samples later
            PrintAttributes(pSample);
            CComPtr<IMFSampleExtradata> pData = NULL;
            SAFE_NEW(pData, IMFSampleExtradata);
            if (NULL == pData)
            {
                hr = E_OUTOFMEMORY;
            }
            if (SUCCEEDED(hr))
            {
                hr = pData->SetAttributes(pSample);
                ATLASSERT(SUCCEEDED(hr));
            }
            //TODO: move DTS and other stuff here.
            if (SUCCEEDED(hr))
            {
                hr = m_OutputBitstream.m_ExtradataTransport.Send(m_iNumberInputSamples, pData);
                ATLASSERT(SUCCEEDED(hr));
            }
            if (SUCCEEDED(hr))
            {
                hr = ProcessFrame(sts);
                ATLASSERT(SUCCEEDED(hr));
            }
        }
    }
    ATLASSERT(SUCCEEDED(hr) && SUCCEEDED(m_hrError));
    if (SUCCEEDED(hr) && !m_bReinit)
    {
        m_OutputBitstream.CheckBitstreamsNumLimit();
        // requesting input while free surfaces (in & out) are available
        if (!m_bDoNotRequestInput && !m_NeedInputEventInfo.m_requested &&
            !m_bNeedInSurface && !m_bNeedOutSurface && !m_OutputBitstream.m_bBitstreamsLimit)
        {
            hr = RequestInput();
            ATLASSERT(SUCCEEDED(hr));
        }
    }
    ATLASSERT(SUCCEEDED(hr) && SUCCEEDED(m_hrError));
    if (FAILED(hr) && SUCCEEDED(m_hrError))
    {
        // setting plug-in error
        SetPlgError(hr, sts);
        // handling error
        HandlePlgError(lock);
    }
    // errors will be handled either in this function or in downstream plug-in
    if (!ReturnPlgError()) hr = S_OK;
    MFX_LTRACE_I(MF_TL_PERF, m_iNumberInputSamplesBeforeVPP);
    MFX_LTRACE_I(MF_TL_PERF, m_iNumberInputSamples);
    MFX_LTRACE_I(MF_TL_PERF, sts);
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVEnc::ProcessOutput(DWORD  dwFlags,
                                    DWORD  cOutputBufferCount,
                                    MFT_OUTPUT_DATA_BUFFER* pOutputSamples,
                                    DWORD* pdwStatus)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MFTicker ticker(&m_ticks_ProcessOutput);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = S_OK;
    mfxStatus sts = MFX_ERR_NONE;

    GetPerformance();
    hr = MFPlugin::ProcessOutput(dwFlags, cOutputBufferCount, pOutputSamples, pdwStatus);
    CHECK_EXPRESSION(SUCCEEDED(hr), hr);

    SET_HR_ERROR(0 == dwFlags, hr, E_INVALIDARG);
    SET_HR_ERROR(stHeaderSetAwaiting != m_State, hr, MF_E_TRANSFORM_TYPE_NOT_SET);

    if (SUCCEEDED(hr))
    {
        if (m_TypeDynamicallyChanged)
        {
            MFX_LTRACE_I(MF_TL_GENERAL, m_TypeDynamicallyChanged);
            pOutputSamples[0].dwStatus = MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE;
            MFX_LTRACE_S(MF_TL_GENERAL, "pOutputSamples[0].dwStatus = MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE");
            *pdwStatus = 0;
            hr = MF_E_TRANSFORM_STREAM_CHANGE;
            MFX_LTRACE_S(MF_TL_GENERAL, "hr = MF_E_TRANSFORM_STREAM_CHANGE");
        }
    }

    SET_HR_ERROR(m_OutputBitstream.GetDispBitstream() && (m_OutputBitstream.HaveBitstreamToDisplay() || m_bEndOfInput),
                 hr, E_FAIL);

    if (SUCCEEDED(hr)) hr = m_hrError;
    if (stHeaderNotSet == m_State)
    {
        MFX_LTRACE_I(MF_TL_GENERAL, m_State);
        MFX_LTRACE_S(MF_TL_PERF, "Starting header set process");
        if (SUCCEEDED(hr)) hr = m_OutputBitstream.FindHeader();
        if (SUCCEEDED(hr))
        {
            pOutputSamples[0].dwStatus = MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE;
            MFX_LTRACE_S(MF_TL_GENERAL, "pOutputSamples[0].dwStatus = MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE");
            *pdwStatus = 0;
            hr = MF_E_TRANSFORM_STREAM_CHANGE;
            MFX_LTRACE_S(MF_TL_GENERAL, "hr = MF_E_TRANSFORM_STREAM_CHANGE");
            m_State = stHeaderSetAwaiting;
            MFX_LTRACE_I(MF_TL_GENERAL, m_State);
        }
    }
    else
    {
        MFX_LTRACE_S(MF_TL_PERF, "Normal encoding");


        m_State = stReady;
        MFX_LTRACE_I(MF_TL_NOTES, m_State);

        //returns HaveBitstreamToDisplay(); hr = GetDispBitstream()->pMFBitstream->Sync()
        bool bDisplayFrame = false;

        if (SUCCEEDED(hr))
        {
            bDisplayFrame = m_OutputBitstream.GetSample(&(pOutputSamples[0].pSample), hr);
            PrintAttributes(pOutputSamples[0].pSample);
        }

        if (SUCCEEDED(hr))
        {
            // Set status flags
            if (m_HasOutputEventInfo.m_requested)
                pOutputSamples[0].dwStatus = MFT_OUTPUT_DATA_BUFFER_INCOMPLETE;
            *pdwStatus = 0;

            ++(m_MfxCodecInfo.m_nOutFrames);
        }
        if (SUCCEEDED(hr) && bDisplayFrame) ++m_iNumberOutputSamples;
        if (bDisplayFrame)
        {
            m_OutputBitstream.ReleaseSample();

            // trying to release resources
            if (m_pInSurfaces)  SetFreeSurface(m_pInSurfaces, m_nInSurfacesNum);
            if (m_pOutSurfaces) SetFreeSurface(m_pOutSurfaces, m_nOutSurfacesNum);

            MFX_LTRACE_S(MF_TL_NOTES, "m_pAsyncThreadEvent->Signal");
            m_pAsyncThreadEvent->Signal();
        }
        if (m_bNeedOutSurface)
        { // processing remained output and setting free out surface
            HRESULT hr_sts = S_OK;
            // setting free out surface
            m_pOutSurface = SetFreeSurface(m_pOutSurfaces, m_nOutSurfacesNum);
            if (m_pOutSurface)
            {
                m_bNeedOutSurface = false;
                if (MFX_ERR_MORE_SURFACE == m_LastSts)
                {
                    hr_sts = ProcessFrame(sts);
                    if (SUCCEEDED(hr) && FAILED(hr_sts))
                    {
                        MFX_LTRACE_D(MF_TL_GENERAL, hr_sts);
                        hr = hr_sts;
                    }
                }
            }
        }
        if (m_bNeedInSurface && (MFX_ERR_MORE_SURFACE != m_LastSts))
        { // setting free in surface
            m_pInSurface = SetFreeSurface(m_pInSurfaces, m_nInSurfacesNum);
            if (m_pInSurface) m_bNeedInSurface = false;
        }
        MFX_LTRACE_I(MF_TL_PERF, m_bNeedInSurface);
        MFX_LTRACE_I(MF_TL_PERF, m_bNeedOutSurface);
        MFX_LTRACE_D(MF_TL_PERF, m_OutputBitstream.HaveBitstreamToDisplay());
        if (!m_OutputBitstream.m_bBitstreamsLimit && !m_NeedInputEventInfo.m_requested && !m_bEndOfInput && !m_bDoNotRequestInput)
        { // trying to request more input only if there are no pending requests
            if (!m_bNeedInSurface && !m_bNeedOutSurface)
            { // we have free input & output surfaces
                RequestInput();
            }
            else if (SUCCEEDED(hr) && !m_OutputBitstream.HaveBitstreamToDisplay())
            { // we have: 1) no free surfaces, 2) no buffered output
                MFX_LTRACE_S(MF_TL_NOTES, "!m_OutputBitstream.HaveBitstreamToDisplay");
                hr = E_FAIL;
                MFX_LTRACE_D(MF_TL_NOTES, hr);
            }
        }
    }
    if (MF_E_TRANSFORM_STREAM_CHANGE != hr)
    {
        if (pOutputSamples && (FAILED(hr) || !(pOutputSamples[0].pSample) && !(pOutputSamples[0].dwStatus)))
        { // we should set MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE status if we do not generate output
            SAFE_RELEASE(pOutputSamples[0].pSample);
            pOutputSamples[0].dwStatus = MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE;
            MFX_LTRACE_S(MF_TL_PERF, "pOutputSamples[0].dwStatus = MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE");
            ATLASSERT(false);
        }
        if (FAILED(hr) && SUCCEEDED(m_hrError))
        {
            MFX_LTRACE_D(MF_TL_PERF, hr);
            // setting plug-in error
            SetPlgError(hr, sts);
            // handling error
            HandlePlgError(lock);
        }
        // errors will be handled either in ProcessInput or in upstream plug-in
        hr = S_OK;
    }
    MFX_LTRACE_I(MF_TL_PERF, m_iNumberOutputSamples);
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
    #define MFX_TRACE_CATEGORY _T("mfx_mft_enc")
#else
    #define MFX_TRACE_CATEGORY (pRegData && pRegData->pPluginName)? pRegData->pPluginName: _T("mfx_mft_enc")
#endif

// CreateInstance
// Static method to create an instance of the source.
//
// iid:         IID of the requested interface on the source.
// ppSource:    Receives a ref-counted pointer to the source.
HRESULT MFPluginVEnc::CreateInstance(REFIID iid,
                                     void** ppMFT,
                                     ClassRegData* pRegData)

{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    MFPluginVEnc *pMFT = NULL;
    mf_tick tick = mf_get_tick();

    CHECK_POINTER(ppMFT, E_POINTER);
#if 0 // checking of supported platform
    if (!CheckPlatform())
    {
        *ppMFT = NULL;
        return E_FAIL;
    }
#endif

    SAFE_NEW(pMFT, MFPluginVEnc(hr, pRegData));
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

/*------------------------------------------------------------------------------*/
