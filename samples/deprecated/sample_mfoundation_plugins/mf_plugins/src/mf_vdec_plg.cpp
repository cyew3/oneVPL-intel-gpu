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

File: mf_vdec_plg.cpp

Purpose: define common code for MSDK based decoder MFTs.

*********************************************************************************/
#include "mf_vdec_plg.h"
#include "sample_utils.h"

/*------------------------------------------------------------------------------*/

#undef  MFX_TRACE_CATEGORY
#define MFX_TRACE_CATEGORY _T("mfx_mft_unk")

#define OMIT_DEC_ASYNC_SYNC_POINT
#define MFX_MFT_WORKAROUND 0x32

/*------------------------------------------------------------------------------*/

HRESULT CreateVDecPlugin(REFIID iid,
                         void** ppMFT,
                         ClassRegData* pRegistrationData)
{
    return MFPluginVDec::CreateInstance(iid, ppMFT, pRegistrationData);
}

/*------------------------------------------------------------------------------*/
// MFPluginVDec class

#undef  MFX_TRACE_CATEGORY
#ifdef MF_SHORT_CAT_NAMES
    #define MFX_TRACE_CATEGORY _T("mfx_mft_dec")
#else
    #define MFX_TRACE_CATEGORY m_Reg.pPluginName
#endif

MFPluginVDec::MFPluginVDec(HRESULT &hr,
                           ClassRegData *pRegistrationData) :
    MFPlugin(hr, pRegistrationData),
    m_nRefCount(0),
    m_MSDK_impl(MF_MFX_IMPL),
    m_pMfxVideoSession(NULL),
    m_pmfxDEC(NULL),
    m_pInputInfo(NULL),
    m_pOutputInfo(NULL),
    m_pOutputTypeCandidate(NULL),
    m_iNumberLockedSurfaces(0),
    m_iNumberInputSurfaces(0),
    m_State(stDecoderNotCreated),
    m_bEndOfInput(false),
    m_bSendDrainComplete(false),
    m_bNeedWorkSurface(false),
    m_bStartDrain(false),
    m_bReinit(false),
    m_bReinitStarted(false),
    m_bSendFakeSrf(false),
    m_bSetDiscontinuityAttribute(false),
    m_uiSurfacesNum(MF_RENDERING_SURF_NUM),
    m_pWorkSurfaces(NULL),
    m_pWorkSurface(NULL),
    m_pOutSurfaces(NULL),
    m_pOutSurface(NULL),
    m_pDispSurface(NULL),
    m_pMFBitstream(NULL),
    m_pReinitMFBitstream(NULL),
    m_pBitstream(NULL),
    m_pFreeSamplesPool(NULL),
    m_pDeviceDXVA(NULL),
    m_MemType(MFT_MEM_SW),
    m_deviceHandle(NULL),
    m_pFrameAllocator(NULL),
    m_bChangeOutputType(false),
    m_bOutputTypeChangeRequired(false),
    m_uiHasOutputEventExists(0),
    m_pPostponedInput(NULL),
    m_LastPts(0),
    m_LastFramerate(0.0),
    m_SyncOpSts(MFX_ERR_NONE),
    m_dbg_decin(NULL),
    m_dbg_decin_fc(NULL),
    m_dbg_decout(NULL),
    m_eLastMessage()
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);

    if (FAILED(hr)) return;
    SAFE_NEW(m_pWorkTicker, MFTicker(&m_ticks_WorkTime));
    SAFE_NEW(m_pCpuUsager, MFCpuUsager(&m_TimeTotal, &m_TimeKernel, &m_TimeUser, &m_CpuUsage));

    if (!m_Reg.pFillParams) hr = E_POINTER;
    if (SUCCEEDED(hr))
    {
        // NOTE: decoders (currently VC1) may pretend to support extended
        // number of output types in registration; the following code
        // corrects this for the running plug-ins
        m_Reg.cOutputTypes = ARRAY_SIZE(g_UncompressedVideoTypes);
        m_Reg.pOutputTypes = (GUID_info*)g_UncompressedVideoTypes;
    }

    memset(&m_FrameInfo_original, 0, sizeof(m_FrameInfo_original));
    memset(&m_VideoParams_input, 0, sizeof(m_VideoParams_input));
    memset((void*)&m_DecoderRequest, 0, sizeof(mfxFrameAllocRequest));
    memset(&m_DecAllocResponse, 0, sizeof(m_DecAllocResponse));

    if (SUCCEEDED(hr))
    { // setting default decoder parameters
        mfxStatus sts = MFX_ERR_NONE;
        sts = m_Reg.pFillParams(&m_MfxParamsVideo);
        memcpy_s(&m_VideoParams_input, sizeof(m_VideoParams_input), &m_MfxParamsVideo, sizeof(m_MfxParamsVideo));
        if (MFX_ERR_NONE != sts) hr = E_FAIL;
    }

#if defined(_DEBUG) || (DBG_YES == USE_DBG_TOOLS)
    if (SUCCEEDED(hr))
    { // reading registry for some parameters
        TCHAR file_name[MAX_PATH] = {0};
        DWORD var = 1;

        if (S_OK == mf_get_reg_string(_T(REG_PARAMS_PATH),
                                      _T(REG_DEC_IN_FILE), file_name, MAX_PATH))
        {
            m_dbg_decin = mywfopen(file_name, L"wb");
        }
        if (S_OK == mf_get_reg_string(_T(REG_PARAMS_PATH),
                                      _T(REG_DEC_IN_FILE_FC), file_name, MAX_PATH))
        {
            m_dbg_decin_fc = mywfopen(file_name, L"wb");
        }
        if (S_OK == mf_get_reg_string(_T(REG_PARAMS_PATH),
                                      _T(REG_DEC_OUT_FILE), file_name, MAX_PATH))
        {
            m_dbg_decout = mywfopen(file_name, L"wb");
        }
        if (S_OK == mf_get_reg_dword(_T(REG_PARAMS_PATH),
                                     _T(REG_DEC_MSDK), &var))
        {
            if (1 == var) m_dbg_MSDK = dbgSwMsdk;
            else if (2 == var) m_dbg_MSDK = dbgHwMsdk;
        }
        if (S_OK == mf_get_reg_dword(_T(REG_PARAMS_PATH),
                                     _T(REG_DEC_MEMORY), &var))
        {
            if (1 == var) m_dbg_Memory = dbgSwMemory;
            else if (2 == var) m_dbg_Memory = dbgHwMemory;
        }
        if ((S_OK == mf_get_reg_dword(_T(REG_PARAMS_PATH),
                                      _T(REG_VPP_NO_SW_FB), &var)) && var)
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
        SAFE_NEW(m_pMFBitstream, MFDecBitstream(hr));
        if (m_pMFBitstream) m_pMFBitstream->SetFiles(m_dbg_decin, m_dbg_decin_fc);
        else hr = E_OUTOFMEMORY;
    }
    if (SUCCEEDED(hr))
    {
        // prepare MediaSDK
#if MFX_D3D11_SUPPORT
        mfxStatus sts = InitMfxVideoSession(MFX_IMPL_VIA_D3D11);
#else
        mfxStatus sts = InitMfxVideoSession(MFX_IMPL_VIA_ANY);
#endif
        switch (sts)
        {
        case MFX_ERR_NONE:
            m_State = stHeaderNotDecoded;
            hr = S_OK;
            break;
        case MFX_ERR_MEMORY_ALLOC:  hr = E_OUTOFMEMORY; break;
        default:                    hr = E_FAIL; break;
        }
    }
    GetPerformance();
    MFX_LTRACE_D(MF_TL_PERF, hr);
    MFX_LTRACE_WS(MF_TL_PERF, (m_Reg.pPluginName)? m_Reg.pPluginName: L"null_plg_name");
}

/*------------------------------------------------------------------------------*/

MFPluginVDec::~MFPluginVDec(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);

    MFX_LTRACE_I(MF_TL_PERF, m_uiSurfacesNum);
    MFX_LTRACE_I(MF_TL_PERF, m_iNumberLockedSurfaces);
    MFX_LTRACE_I(MF_TL_PERF, m_iNumberInputSurfaces);
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
    SAFE_RELEASE(m_pAttributes);
    SAFE_RELEASE(m_pEventQueue);
    SAFE_RELEASE(m_pInputType);
    // Delitioning of unneded properties:
    // MF_MT_D3D_DEVICE - it contains pointer to enc plug-in, need to delete
    // it or enc destructor will not be invoked
    ReleaseMediaType(m_pOutputType);
    ReleaseMediaType(m_pOutputTypeCandidate);
    // closing codec
    CloseCodec(lock);
    SAFE_DELETE(m_pmfxDEC);
    SAFE_DELETE(m_pMFBitstream);
    SAFE_DELETE(m_pReinitMFBitstream);
    // session deinitialization
    SAFE_RELEASE(m_pMfxVideoSession);

    if (m_Reg.pFreeParams) m_Reg.pFreeParams(&m_MfxParamsVideo);

    if (m_dbg_decin) fclose(m_dbg_decin);
    if (m_dbg_decin_fc) fclose(m_dbg_decin_fc);
    if (m_dbg_decout) fclose(m_dbg_decout);
}

/*------------------------------------------------------------------------------*/
// IUnknown methods

ULONG MFPluginVDec::AddRef(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MFX_LTRACE_I(MF_TL_GENERAL, m_nRefCount+1);
    return InterlockedIncrement(&m_nRefCount);
}

ULONG MFPluginVDec::Release(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    ULONG uCount = InterlockedDecrement(&m_nRefCount);
    MFX_LTRACE_I(MF_TL_GENERAL, uCount);
    if (uCount == 0) delete this;
    // For thread safety, return a temporary variable.
    return uCount;
}

HRESULT MFPluginVDec::QueryInterface(REFIID iid, void** ppv)
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
#if MFX_D3D11_SUPPORT
    else if (iid == IID_IMFQualityAdvise)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "IMFQualityAdvise");
        *ppv = static_cast<IMFQualityAdvise*>(this);
    }
    else if (iid == IID_IMFRealTimeClientEx)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "IMFRealTimeClientEx");
        *ppv = static_cast<IMFRealTimeClientEx*>(this);
    }
#endif
    else if (iid == IID_IConfigureMfxDecoder)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "IConfigureMfxDecoder");
        *ppv = static_cast<IConfigureMfxCodec*>(this);
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

HRESULT MFPluginVDec::GetAttributes(IMFAttributes** pAttributes)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = MFPlugin::GetAttributes(pAttributes);

    GetPerformance();
    // setting D3D aware attribute to notify renderer that plug-in supports HWA
    if ((dbgSwMsdk != m_dbg_MSDK) || (dbgHwMsdk == m_dbg_Memory))
    {
        if (SUCCEEDED(hr)) hr = (*pAttributes)->SetUINT32(MF_SA_D3D_AWARE, TRUE);
#if MFX_D3D11_SUPPORT
        if (SUCCEEDED(hr)) hr = (*pAttributes)->SetUINT32(MF_SA_D3D11_AWARE, TRUE);
#endif
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVDec::GetInputAvailableType(DWORD dwInputStreamID,
                                            DWORD dwTypeIndex, // 0-based
                                            IMFMediaType** ppType)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);
    HRESULT       hr    = S_OK;
    IMFMediaType* pType = NULL;

    MFX_LTRACE_I(MF_TL_GENERAL, dwTypeIndex);

    GetPerformance();
    hr = MFPlugin::GetInputAvailableType(dwInputStreamID, dwTypeIndex, &pType);
    if (SUCCEEDED(hr)) hr = pType->SetGUID(MF_MT_MAJOR_TYPE, m_Reg.pInputTypes[dwTypeIndex].major_type);
    if (SUCCEEDED(hr)) hr = pType->SetGUID(MF_MT_SUBTYPE, m_Reg.pInputTypes[dwTypeIndex].subtype);
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

HRESULT MFPluginVDec::GetOutputAvailableType(DWORD dwOutputStreamID,
                                             DWORD dwTypeIndex, // 0-based
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
        if (m_bChangeOutputType && m_pOutputType)
        {
            if (!m_pOutputTypeCandidate)
            { // creating candidate type
                UINT32 w = 0, h = 0;
                MFVideoArea area;
                UINT32 arw = 0, arh = 0;

                hr = MFCreateMediaType(&m_pOutputTypeCandidate);
                if (SUCCEEDED(hr))
                {
                    w = m_MfxParamsVideo.mfx.FrameInfo.Width;
                    h = m_MfxParamsVideo.mfx.FrameInfo.Height;
                    arw = m_MfxParamsVideo.mfx.FrameInfo.AspectRatioW;
                    arh = m_MfxParamsVideo.mfx.FrameInfo.AspectRatioH;
                    memset(&area, 0, sizeof(MFVideoArea));
                    area.OffsetX.value = m_MfxParamsVideo.mfx.FrameInfo.CropX;
                    area.OffsetY.value = m_MfxParamsVideo.mfx.FrameInfo.CropY;
                    area.Area.cx = m_MfxParamsVideo.mfx.FrameInfo.CropW;
                    area.Area.cy = m_MfxParamsVideo.mfx.FrameInfo.CropH;

                    MFX_LTRACE_I(MF_TL_GENERAL, m_MfxParamsVideo.mfx.FrameInfo.CropX);
                    MFX_LTRACE_I(MF_TL_GENERAL, m_MfxParamsVideo.mfx.FrameInfo.CropY);
                    MFX_LTRACE_I(MF_TL_GENERAL, m_MfxParamsVideo.mfx.FrameInfo.CropW);
                    MFX_LTRACE_I(MF_TL_GENERAL, m_MfxParamsVideo.mfx.FrameInfo.CropH);
                    MFX_LTRACE_I(MF_TL_GENERAL, m_MfxParamsVideo.mfx.FrameInfo.PicStruct);
                }
                if (SUCCEEDED(hr)) hr = m_pOutputType->CopyAllItems(m_pOutputTypeCandidate);
                // correcting some parameters
                if (SUCCEEDED(hr)) hr = MFSetAttributeSize(m_pOutputTypeCandidate, MF_MT_FRAME_SIZE, w, h);
                if (SUCCEEDED(hr)) hr = MFSetAttributeRatio(m_pOutputTypeCandidate, MF_MT_PIXEL_ASPECT_RATIO, arw, arh);
                if (SUCCEEDED(hr)) hr = m_pOutputTypeCandidate->SetUINT32(MF_MT_INTERLACE_MODE, mf_mfx2ms_imode(m_MfxParamsVideo.mfx.FrameInfo.PicStruct));
                if (SUCCEEDED(hr)) hr = m_pOutputTypeCandidate->SetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)&area, sizeof(MFVideoArea));
                if (SUCCEEDED(hr)) hr = m_pOutputTypeCandidate->SetBlob(MF_MT_GEOMETRIC_APERTURE, (UINT8*)&area, sizeof(MFVideoArea));
            }
            if (SUCCEEDED(hr))
            {
                hr = m_pOutputTypeCandidate->CopyAllItems(pType);
            }
        }
        else if (m_pOutputType)
        { // if output type was set we may offer only this type
            // should not occur
            hr = m_pOutputType->CopyAllItems(pType);
        }
        else
        {
            if (SUCCEEDED(hr)) hr = pType->SetGUID(MF_MT_MAJOR_TYPE, m_Reg.pOutputTypes[dwTypeIndex].major_type);
            if (SUCCEEDED(hr)) hr = pType->SetGUID(MF_MT_SUBTYPE,    m_Reg.pOutputTypes[dwTypeIndex].subtype);
            // if next attribute will not be set topology will insert MS color converter between decoder and EVR,
            // this may lead to unworking HW decoding
            if (SUCCEEDED(hr)) hr = pType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
            // if input type is not set we can only produce the hint to the caller about output types which MFT generates
            // otherwise we can be more precise and specify parameters of accepted output type
            if (m_pInputType)
            {
                if (!m_pOutputTypeCandidate)
                {
                    hr = MFCreateMediaType(&m_pOutputTypeCandidate);
                    if (SUCCEEDED(hr))
                    {
                        GUID subtype = GUID_NULL;
                        UINT32 par1 = 0, par2 = 0;

                        mfxVideoParam mfxVideoParamTemp;
                        memcpy_s(&mfxVideoParamTemp, sizeof(mfxVideoParamTemp), &m_VideoParams_input, sizeof(m_VideoParams_input));
                        if (MFX_PICSTRUCT_UNKNOWN == mfxVideoParamTemp.mfx.FrameInfo.PicStruct)
                            mfxVideoParamTemp.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE; //temporary treating as progressive

                        if (MFX_ERR_NONE == m_pmfxDEC->Query(&mfxVideoParamTemp, &mfxVideoParamTemp))
                            hr = MFSetAttributeSize(m_pOutputTypeCandidate, MF_MT_FRAME_SIZE, mfxVideoParamTemp.mfx.FrameInfo.Width, mfxVideoParamTemp.mfx.FrameInfo.Height);

                        if (SUCCEEDED(hr) && SUCCEEDED(MFGetAttributeRatio(m_pInputType, MF_MT_PIXEL_ASPECT_RATIO, &par1, &par2)))
                            hr = MFSetAttributeRatio(m_pOutputTypeCandidate, MF_MT_PIXEL_ASPECT_RATIO, par1, par2);
                        if (SUCCEEDED(hr) && SUCCEEDED(m_pInputType->GetUINT32(MF_MT_INTERLACE_MODE, &par1)))
                            hr = m_pOutputTypeCandidate->SetUINT32(MF_MT_INTERLACE_MODE, par1);
                        if (SUCCEEDED(hr) && SUCCEEDED(MFGetAttributeRatio(m_pInputType, MF_MT_FRAME_RATE, &par1, &par2)))
                            hr = MFSetAttributeRatio(m_pOutputTypeCandidate, MF_MT_FRAME_RATE, par1, par2);
                        if (SUCCEEDED(hr) && SUCCEEDED(m_pInputType->GetGUID(MF_MT_SUBTYPE, &subtype)))
                            hr = m_pOutputTypeCandidate->SetGUID(MF_MT_DEC_SUBTYPE, subtype);
                        if (SUCCEEDED(hr) && SUCCEEDED(m_pInputType->GetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, &par1)))
                            hr = m_pOutputTypeCandidate->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, par1);
                    }
                }
                if (SUCCEEDED(hr))
                {
                    UINT32 count = 0;
                    GUID guidKey;
                    PROPVARIANT Value;
                    hr = pType->GetCount(&count);
                    for (UINT32 i = 0; i < count; i++)
                    {
                        if (SUCCEEDED(hr)) hr = pType->GetItemByIndex(i, &guidKey, &Value);
                        if (SUCCEEDED(hr)) hr = m_pOutputTypeCandidate->SetItem(guidKey, Value);
                    }

                    hr = m_pOutputTypeCandidate->CopyAllItems(pType);
                }
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

HRESULT MFPluginVDec::CheckInputMediaType(IMFMediaType* pType)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;

#ifdef MF_ENABLE_PICTURE_COMPLEXITY_LIMITS
    GUID subtype = GUID_NULL;

    // checking for supported resolution on connection stage only
    if (!m_bStreamingStarted && SUCCEEDED(pType->GetGUID(MF_MT_SUBTYPE, &subtype)))
    {
        if (MEDIASUBTYPE_MPEG2_VIDEO == subtype)
        {
            UINT32 width = 0, height = 0;

            if (SUCCEEDED(MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height)) &&
                ((width * height) < MF_DEC_MPEG2_PICTURE_COMPLEXITY_LIMIT))
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
bool MFPluginVDec::CheckHwSupport(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    bool ret_val = MFPlugin::CheckHwSupport();

    if (ret_val)
    {
        mfxStatus     sts = MFX_ERR_NONE;
        mfxVideoParam mfxVideoParamTemp;
        mfxFrameInfo* pmfxFrameInfo = &mfxVideoParamTemp.mfx.FrameInfo;

        memcpy_s(&mfxVideoParamTemp, sizeof(mfxVideoParamTemp), &m_VideoParams_input, sizeof(m_VideoParams_input));
        //TODO: AsyncDepth, Protected, IOPattern, ExtParam, NumExtParam

        if (!(pmfxFrameInfo->FourCC) || !(pmfxFrameInfo->ChromaFormat))
        {
            // set any color format to make suitable values for Query, suppose it is NV12;
            // actual color format is obtained on output type setting
            pmfxFrameInfo->FourCC       = MAKEFOURCC('N','V','1','2');
            pmfxFrameInfo->ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        }
        // temporal alignments: width to 16 and height to 32 to make suitable values for Query
        pmfxFrameInfo->Width        = (pmfxFrameInfo->CropW + 15) &~ 15;
        pmfxFrameInfo->Height       = (pmfxFrameInfo->CropH + 31) &~ 31;

        mfxVideoParamTemp.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE; // FIXME

        if (MFX_ERR_NONE == sts) sts = m_pmfxDEC->Query(&mfxVideoParamTemp, &mfxVideoParamTemp);
        if ((MFX_ERR_NONE != sts) && (m_dbg_no_SW_fallback || (MFX_WRN_PARTIAL_ACCELERATION != sts)))
            ret_val = false;
    }
    MFX_LTRACE_I(MF_TL_GENERAL, ret_val);
    return ret_val;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVDec::SetInputType(DWORD         dwInputStreamID,
                                   IMFMediaType* pType,
                                   DWORD         dwFlags)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = S_OK;
    bool bReinitNeeded = false;

    MFX_LTRACE_P(MF_TL_PERF, pType);

    GetPerformance();
    hr = MFPlugin::SetInputType(dwInputStreamID, pType, dwFlags);
    CHECK_EXPRESSION(SUCCEEDED(hr), hr);

    if (!pType)
    { // resetting media type
        SAFE_RELEASE(m_pInputType);
    }
    CHECK_POINTER(pType, S_OK);
    // Validate the type.
    if (SUCCEEDED(hr)) hr = CheckMediaType(pType, m_Reg.pInputTypes, m_Reg.cInputTypes, &m_pInputInfo);
    if (SUCCEEDED(hr)) hr = CheckInputMediaType(pType);
    if (0 == (dwFlags & MFT_SET_TYPE_TEST_ONLY))
    {
        if (SUCCEEDED(hr))
        {
            // set major type; check major type is the same as was on output
            if (GUID_NULL == m_MajorType)
                m_MajorType = m_pInputInfo->major_type;
            else if (m_pInputInfo->major_type != m_MajorType)
                hr = MF_E_INVALIDTYPE;
        }
        if (SUCCEEDED(hr))
        {
            if(m_pInputType)
            {
                MFX_LTRACE_S(MF_TL_GENERAL, "Input Type has been changed - Output Type is needed to be changed");
                m_bChangeOutputType = true;
                SAFE_RELEASE(m_pOutputTypeCandidate);
            }
            SAFE_RELEASE(m_pInputType);
            pType->AddRef();
            m_pInputType = pType;

            bReinitNeeded = (stReady == m_State) && IsVppNeeded(pType, m_pOutputType);
        }
        if (SUCCEEDED(hr)) hr = mf_mftype2mfx_frame_info(m_pInputType, &(m_VideoParams_input.mfx.FrameInfo));
        if (SUCCEEDED(hr) && bReinitNeeded)
        {
            SAFE_DELETE(m_pReinitMFBitstream);

            SAFE_NEW(m_pReinitMFBitstream, MFDecBitstream(hr));
            if (m_pReinitMFBitstream) m_pReinitMFBitstream->SetFiles(m_dbg_decin, m_dbg_decin_fc);
            else hr = E_OUTOFMEMORY;
        }
        if (SUCCEEDED(hr))
        {
            HRESULT hr_sts = S_OK;
            mfxU32  size = 0;
            MFDecBitstream* pMFBitstream = (bReinitNeeded)? m_pReinitMFBitstream: m_pMFBitstream;

            if ((WMMEDIASUBTYPE_WVC1 == m_pInputInfo->subtype) ||
                (WMMEDIASUBTYPE_WMV3 == m_pInputInfo->subtype))
            {
                hr_sts = m_pInputType->GetBlobSize(MF_MT_USER_DATA, &size);
                MFX_LTRACE_1(MF_TL_PERF, "m_pInputType->GetBlobSize(MF_MT_USER_DATA): ", "size = %d", size);
                MFX_LTRACE_D(MF_TL_PERF, hr_sts);
                if (SUCCEEDED(hr_sts) && size)
                {
                    mfxU8 *buf = (mfxU8*)malloc(size), *bdata = buf;
                    if (buf)
                    {
                        hr = m_pInputType->GetBlob(MF_MT_USER_DATA, bdata, size, &size);
                        if (SUCCEEDED(hr))
                        {
                            if (WMMEDIASUBTYPE_WVC1 == m_pInputInfo->subtype)
                            {
                                MFX_LTRACE_S(MF_TL_GENERAL, "WMMEDIASUBTYPE_WVC1");
                                m_VideoParams_input.mfx.CodecProfile = MFX_PROFILE_VC1_ADVANCED;
                                ++bdata; // skipping 1-st byte
                            }
                            else
                            {
                                MFX_LTRACE_S(MF_TL_GENERAL, "WMMEDIASUBTYPE_WMV3");
                                m_VideoParams_input.mfx.CodecProfile = MFX_PROFILE_VC1_SIMPLE; // or main, that's the same for FC initialization
                            }
                            hr = pMFBitstream->InitFC(&m_VideoParams_input, bdata, size);
                        }
                        SAFE_FREE(buf);
                    }
                    else hr = E_OUTOFMEMORY;
                }
            }
            if (MEDIASUBTYPE_H264 == m_pInputInfo->subtype)
            {
                // TODO: this is not currently used. Written for future purposes.
                hr_sts = m_pInputType->GetBlobSize(MF_MT_MPEG4_SAMPLE_DESCRIPTION, &size);

                MFX_LTRACE_1(MF_TL_PERF, "m_pInputType->GetBlobSize(MF_MT_MPEG4_SAMPLE_DESCRIPTION): ", "size = %d", size);
                MFX_LTRACE_D(MF_TL_PERF, hr_sts);

                if (SUCCEEDED(hr_sts) && size)
                {
                    mfxU8 *buf = (mfxU8*)malloc(size), *bdata = buf;
                    if (buf)
                    {
                        hr = m_pInputType->GetBlob(MF_MT_MPEG4_SAMPLE_DESCRIPTION, bdata, size, &size);
                        if (SUCCEEDED(hr))
                        {
                            MFX_LTRACE_S(MF_TL_GENERAL, "MEDIASUBTYPE_H264");
#if 0
                            hr = pMFBitstream->InitFC(&m_VideoParams_input, bdata, size);
#endif
                        }
                        SAFE_FREE(buf);
                    }
                    else hr = E_OUTOFMEMORY;
                }
            }
        }
        /* We will not check whether HW supports DEC operations in the following cases:
         *  - if "resolution change" occurs (we can encounter error and better to found it on
         * actual reinitialization; otherwise we can hang)
         */
        if (SUCCEEDED(hr) && (stReady != m_State) && !CheckHwSupport()) hr = E_FAIL;
        if (FAILED(hr))
        {
            SAFE_RELEASE(m_pInputType);
        }
        else if (bReinitNeeded)
        {
            if (SUCCEEDED(m_hrError))
            {
                // that's "resolution change"; need to reinit decoder
                m_bReinit     = true;
                m_bEndOfInput = true; // we need to obtain buffered frames
                m_bOutputTypeChangeRequired = true; // forcing change of output type
                if (m_pDeviceDXVA) m_bSendFakeSrf = true; // we need to send fake frame
                else m_bSendFakeSrf = false;
                // pushing async thread (starting call DecFrameAsync with NULL bitstream)
                AsyncThreadPush();
            }
            else if (m_bErrorHandlingFinished && !m_pDeviceDXVA)
            { // TODO: spread this opportunity on case when decoder works with INTC encoder and vpp
                // trying to recover from error if error handling is already finished
                HRESULT hr_sts = S_OK;
                mfxStatus sts = MFX_ERR_NONE;

                ResetPlgError();
                SAFE_RELEASE(m_pPostponedInput);
                m_bReinit = true;
                hr_sts = TryReinit(lock, sts, false);
                if (FAILED(hr_sts)) SetPlgError(hr_sts, sts);
                if (SUCCEEDED(m_hrError))
                {
                    /* If no errors occured in TryReinit, then we successfully
                     * recovered from previous plug-in error and need to reset
                     * error handling flags
                     */
                    m_bErrorHandlingStarted  = false;
                    m_bErrorHandlingFinished = false;
                }
            }
        }
        else
        {
            // that's plug-ins connnection stage
            memcpy_s(&m_MfxParamsVideo, sizeof(m_MfxParamsVideo), &m_VideoParams_input, sizeof(m_VideoParams_input));
        }
    }
    MFX_LTRACE_P(MF_TL_PERF, m_pInputType);
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVDec::SetOutputType(DWORD         dwOutputStreamID,
                                    IMFMediaType* pType,
                                    DWORD         dwFlags)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);
    HRESULT     hr = S_OK;

    MFX_LTRACE_P(MF_TL_PERF, pType);

    GetPerformance();
    hr = MFPlugin::SetOutputType(dwOutputStreamID, pType, dwFlags);
    CHECK_EXPRESSION(SUCCEEDED(hr), hr);
    // input type should be set previously
//    CHECK_POINTER(m_pInputType, MF_E_TRANSFORM_TYPE_NOT_SET);

    if (!pType)
    { // resetting media type
        ReleaseMediaType(m_pOutputType);
        ReleaseMediaType(m_pOutputTypeCandidate);
    }
    CHECK_POINTER(pType, S_OK);
    // Validate the type.
    if (SUCCEEDED(hr))
    {
        hr = CheckMediaType(pType, m_Reg.pOutputTypes, m_Reg.cOutputTypes, &m_pOutputInfo);

        if (SUCCEEDED(hr))
        {
            if (m_bChangeOutputType)
            {
                hr = (IsVppNeeded(pType, m_pOutputTypeCandidate))? E_FAIL: S_OK;
                if (FAILED(hr)) hr = S_OK; // will try "Pretend" algo
                else
                { // switching off pretending
                    memcpy_s(&m_FrameInfo_original, sizeof(m_FrameInfo_original),
                             &(m_MfxParamsVideo.mfx.FrameInfo), sizeof(m_MfxParamsVideo.mfx.FrameInfo));
                    //TODO: AsyncDepth, Protected, IOPattern, ExtParam, NumExtParam
                }
            }
            else
            {
                if (m_pOutputType)
                    hr = (IsVppNeeded(pType, m_pOutputType))? E_FAIL: S_OK;
                else
                    hr = (IsVppNeeded(pType, m_pOutputTypeCandidate))? E_FAIL: S_OK;
            }
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
            // Really set the type, unless the caller was just testing.
            ReleaseMediaType(m_pOutputType);

            pType->AddRef();
            m_pOutputType = pType;

            ReleaseMediaType(m_pOutputTypeCandidate);
        }
        if (SUCCEEDED(hr) && m_bChangeOutputType)
        {
            MFX_LTRACE_S(MF_TL_GENERAL, "Finishing change of output type");
            m_bChangeOutputType = false;
            hr = SendOutput();
        }
    }
    MFX_LTRACE_P(MF_TL_PERF, m_pOutputType);
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVDec::ProcessMessage(MFT_MESSAGE_TYPE eMessage,
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
        m_bStreamingStarted = false;
        if (MFX_ERR_NONE != ResetCodec(lock)) hr = hr;//E_FAIL;
        break;

    case MFT_MESSAGE_COMMAND_DRAIN:
        MFX_LTRACE_S(MF_TL_PERF, "MFT_MESSAGE_COMMAND_DRAIN");
        if (m_pmfxDEC)
        { // processing eos only if we were initialized
            m_bEndOfInput = true;
            m_bSendDrainComplete = true;
            // we can't accept any input if draining has begun
            m_bDoNotRequestInput = true; // prevent calling RequestInput
            m_NeedInputEventInfo.m_requested = 0; // prevent processing of sent RequestInput
            // pushing async thread (starting call DecFrameAsync with NULL bitstream)
            if (!m_bReinit) AsyncThreadPush();
        }
        break;

    case MFT_MESSAGE_COMMAND_MARKER:
        MFX_LTRACE_S(MF_TL_PERF, "MFT_MESSAGE_COMMAND_MARKER");
        // we always produce as much data as possible
        if (m_pmfxDEC) hr = SendMarker(ulParam);
        break;

    case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
        m_bStreamingStarted = true;
        MFX_LTRACE_S(MF_TL_PERF, "MFT_MESSAGE_NOTIFY_BEGIN_STREAMING");
        break;

    case MFT_MESSAGE_NOTIFY_END_STREAMING:
        m_bStreamingStarted = false;
        MFX_LTRACE_S(MF_TL_PERF, "MFT_MESSAGE_NOTIFY_END_STREAMING");
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
        // stream ends, we must not accept input data
        m_bDoNotRequestInput = true; // prevent calling RequestInput
        m_NeedInputEventInfo.m_requested = 0; // prevent processing of sent RequestInput
        MFX_LTRACE_S(MF_TL_PERF, "MFT_MESSAGE_NOTIFY_END_OF_STREAM");
        break;

    case MFT_MESSAGE_SET_D3D_MANAGER:
        MFX_LTRACE_S(MF_TL_PERF, "MFT_MESSAGE_SET_D3D_MANAGER");
        if (ulParam)
        {
            if (!m_pHWDevice)
            {
                IUnknown *pUnknown = reinterpret_cast<IUnknown *>(ulParam);
                mfxHandleType handleType = MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9;
#if MFX_D3D11_SUPPORT
                CComQIPtr<IMFDXGIDeviceManager> pDXGIDeviceManager (pUnknown);
                MFX_LTRACE_P(MF_TL_GENERAL, pDXGIDeviceManager);
                if(pDXGIDeviceManager)
                {
                    if(m_deviceHandle) pDXGIDeviceManager->CloseDeviceHandle(m_deviceHandle);
                    pDXGIDeviceManager->OpenDeviceHandle(&m_deviceHandle);
                    MFX_LTRACE_D(MF_TL_GENERAL, m_deviceHandle);
                    m_pHWDevice.Release();
                    pDXGIDeviceManager->GetVideoService(m_deviceHandle,  IID_ID3D11Device,  (void**)&m_pHWDevice);

                    if (m_pHWDevice)
                    {
                        MFX_LTRACE_S(MF_TL_PERF, "Obtained ID3D11Device");
                        MFX_LTRACE_P(MF_TL_GENERAL, m_pHWDevice);
                        m_MemType = MFT_MEM_DX11;
                        handleType = MFX_HANDLE_D3D11_DEVICE;
                    }
                    else
                    {
                        MFX_LTRACE_S(MF_TL_GENERAL, "Can't QI ");
                        hr = E_FAIL;
                    }
                }
                else
#endif
                {
                    m_pHWDevice.Release();
                    pUnknown->QueryInterface(IID_IDirect3DDeviceManager9, (void**)&m_pHWDevice);
                    if (m_pHWDevice)
                    {
                        MFX_LTRACE_S(MF_TL_PERF, "Obtained IDirect3DDeviceManager9");
                        MFX_LTRACE_P(MF_TL_GENERAL, m_pHWDevice);
                        m_MemType = MFT_MEM_DX9;

                        CComQIPtr<IDirect3DDeviceManager9> d3d9Mgr(m_pHWDevice);
                        if (NULL != d3d9Mgr) {
                            if(m_deviceHandle) d3d9Mgr->CloseDeviceHandle(m_deviceHandle);
                            d3d9Mgr->OpenDeviceHandle(&m_deviceHandle);
                        }
                    }
                    else
                    {
                        MFX_LTRACE_S(MF_TL_GENERAL, "Can't QI ");
                        hr = E_FAIL;
                    }
                }
                if (SUCCEEDED(hr))
                {
#if MFX_D3D11_SUPPORT
                    if (MFX_ERR_NONE != SetHandle(handleType, true))
#else               //recreating session may cause issues when the plugins are directly connected
                    if (MFX_ERR_NONE != SetHandle(handleType, false))
#endif
                    {
                        hr = E_FAIL;
                        ATLASSERT(false); // SetHandle failed for new device
                    }
                }
            }
            else
            {
                MFX_LTRACE_S(MF_TL_GENERAL, "Can set device manager only once");
                hr = E_NOTIMPL;
                ATLASSERT(!m_pHWDevice);
            }
        }
        else
        {
            MFX_LTRACE_S(MF_TL_GENERAL, "Fallback to the SW mode, releasing HW interfaces...");
            m_pHWDevice.Release();
        }
        break;
    default:
        MFX_LTRACE_X(MF_TL_PERF, eMessage);
        break;
    }
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/
// Initialize Frame Allocator (if needed)

mfxStatus MFPluginVDec::InitFRA(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;

    if (!m_pDeviceDXVA && m_pHWDevice)
    { // decoder plug-in creates allocator
        if (!m_pFrameAllocator)
        {
            std::auto_ptr<mfxAllocatorParams> allocParams;
#if MFX_D3D11_SUPPORT
            if(MFT_MEM_DX11 == m_MemType)
            {
                CComQIPtr<ID3D11Device> d3d11Device(m_pHWDevice);
                if (!d3d11Device)
                {
                    MFX_LTRACE_S(MF_TL_GENERAL, "Failed to query ID3D11Device");
                    sts = MFX_ERR_NULL_PTR;
                }
                if (MFX_ERR_NONE == sts)
                {
                    SAFE_NEW(m_pFrameAllocator, MFFrameAllocator(new MFAllocatorHelper<D3D11FrameAllocator>));
                    if (!m_pFrameAllocator) sts = MFX_ERR_MEMORY_ALLOC;
                    else m_pFrameAllocator->AddRef();
                }
                if (MFX_ERR_NONE == sts)
                {
                    D3D11AllocatorParams *pD3D11AllocParams;
                    allocParams.reset(pD3D11AllocParams = new D3D11AllocatorParams);

                    if (NULL != pD3D11AllocParams)
                    {
                        HRESULT hr = S_OK;
                        UINT32 bindFlags = 0;
                        hr = m_pAttributes->GetUINT32(MF_SA_D3D11_BINDFLAGS, &bindFlags);
                        MFX_LTRACE_I(MF_TL_GENERAL, bindFlags);
                        if(SUCCEEDED(hr) && bindFlags)
                        {
//                            if(bindFlags & D3D11_BIND_SHADER_RESOURCE)
                            {
//                                pD3D11AllocParams->nBindFlags = D3D11_BIND_SHADER_RESOURCE;
                            }
                        }

                        pD3D11AllocParams->pDevice = d3d11Device;
                    }
                }
            }
            else
#endif
            if (MFT_MEM_DX9 == m_MemType)
            {
                CComQIPtr<IDirect3DDeviceManager9> d3d9Mgr(m_pHWDevice);
                if (!d3d9Mgr)
                {
                    MFX_LTRACE_S(MF_TL_GENERAL, "Failed to query IDirect3DDeviceManager9");
                    sts = MFX_ERR_NULL_PTR;
                }

                if (MFX_ERR_NONE == sts)
                {
                    SAFE_NEW(m_pFrameAllocator, MFFrameAllocator(new MFAllocatorHelper<D3DFrameAllocator>));
                    if (!m_pFrameAllocator) sts = MFX_ERR_MEMORY_ALLOC;
                    else m_pFrameAllocator->AddRef();
                }
                if (MFX_ERR_NONE == sts)
                {
                    D3DAllocatorParams *pD3DAllocParams;
                    allocParams.reset(pD3DAllocParams = new D3DAllocatorParams);

                    if (NULL != pD3DAllocParams)
                    {
                        pD3DAllocParams->pManager = d3d9Mgr;
                    }
                }
            }
            else
            {
                MFX_LTRACE_1(MF_TL_GENERAL, "Only DX9 or DX11 are supported, m_MemType=", "%d", m_MemType);
                sts = MFX_ERR_UNKNOWN;
            }
            if (MFX_ERR_NONE == sts)
            {
                if (!m_pFrameAllocator || !allocParams.get())
                {
                    sts = MFX_ERR_MEMORY_ALLOC;
                }
                else
                {
                    m_pFrameAllocator->AddRef();
                }

                sts = m_pFrameAllocator->GetMFXAllocator()->Init(allocParams.get());
            }
            if (MFX_ERR_NONE == sts) sts = m_pMfxVideoSession->SetFrameAllocator(m_pFrameAllocator->GetMFXAllocator());
        }
        if (MFX_ERR_NONE == sts)
        {
            mfxFrameAllocRequest request;

            memset((void*)&request, 0, sizeof(mfxFrameAllocRequest));
            memset((void*)&m_DecAllocResponse, 0, sizeof(mfxFrameAllocResponse));
            memcpy_s(&(request.Info), sizeof(request.Info),
                     &(m_MfxParamsVideo.mfx.FrameInfo), sizeof(m_MfxParamsVideo.mfx.FrameInfo));
            request.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
            if(m_VideoParams_input.mfx.CodecId == MFX_CODEC_JPEG)
            {
                request.Type = m_DecoderRequest.Type; //from QueryIOSurf
            }
            request.NumFrameMin = request.NumFrameSuggested = (mfxU16)m_uiSurfacesNum;
            sts = m_pFrameAllocator->GetMFXAllocator()->Alloc(m_pFrameAllocator->GetMFXAllocator()->pthis,
                                                              &request, &m_DecAllocResponse);
        }
        if ((MFX_ERR_NONE == sts) && (m_uiSurfacesNum != m_DecAllocResponse.NumFrameActual))
        {
            sts = MFX_ERR_MEMORY_ALLOC;
        }
    }
    else if (m_pDeviceDXVA && m_pFrameAllocator)
    { // downstream plug-in creates allocator
        if (MFX_ERR_NONE == sts)
        {
            mfxFrameAllocRequest request;

            memset((void*)&request, 0, sizeof(mfxFrameAllocRequest));
            memset((void*)&m_DecAllocResponse, 0, sizeof(mfxFrameAllocResponse));
            memcpy_s(&(request.Info), sizeof(request.Info),
                     &(m_MfxParamsVideo.mfx.FrameInfo), sizeof(m_MfxParamsVideo.mfx.FrameInfo));
            request.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
            if(m_VideoParams_input.mfx.CodecId == MFX_CODEC_JPEG)
            {
                request.Type = m_DecoderRequest.Type; //from QueryIOSurf
            }
            request.NumFrameMin = request.NumFrameSuggested = (mfxU16)m_uiSurfacesNum;
            sts = m_pFrameAllocator->GetMFXAllocator()->Alloc(m_pFrameAllocator->GetMFXAllocator()->pthis,
                                                              &request, &m_DecAllocResponse);
        }
        if ((MFX_ERR_NONE == sts) && (m_uiSurfacesNum != m_DecAllocResponse.NumFrameActual))
        {
            sts = MFX_ERR_MEMORY_ALLOC;
        }
    }
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/
// Initialize Surfaces

mfxStatus MFPluginVDec::InitSRF(MyAutoLock& lock)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 i = 0;

    if (MFX_ERR_NONE == sts)
    {
        memset((void*)&m_DecoderRequest, 0, sizeof(mfxFrameAllocRequest));
        sts = m_pmfxDEC->QueryIOSurf(&m_MfxParamsVideo, &m_DecoderRequest);
        if (!m_dbg_no_SW_fallback && (MFX_WRN_PARTIAL_ACCELERATION == sts))
        {
            sts = MFX_ERR_NONE;
            if ((dbgHwMemory != m_dbg_Memory) && m_pDeviceDXVA)
            {
                // switching to SW mode
                MFX_LTRACE_S(MF_TL_GENERAL, "Switching to System Memory");
                m_MfxParamsVideo.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

                sts = m_pmfxDEC->QueryIOSurf(&m_MfxParamsVideo, &m_DecoderRequest);
                if (MFX_WRN_PARTIAL_ACCELERATION == sts) sts = MFX_ERR_NONE;
            }
        }
        if (MFX_ERR_NONE == sts)
        {
            m_uiSurfacesNum += MAX(m_DecoderRequest.NumFrameSuggested,
                                   MAX(m_DecoderRequest.NumFrameMin, 1));
        }
    }
    if (m_pDeviceDXVA)
    {
        // if our decoder and encoder are directly connected, we will not change
        // output type
        m_bChangeOutputType = false;
        if (MFX_ERR_NONE == sts)
        {
            sts = m_pDeviceDXVA->InitPlg(m_pMfxVideoSession,
                                         &m_MfxParamsVideo,
                                         &m_uiSurfacesNum);
        }
        if ((MFX_ERR_NONE == sts) && (MFX_IOPATTERN_OUT_VIDEO_MEMORY == m_MfxParamsVideo.IOPattern))
        {
            // requesting additional interfaces if we are going to work on HW memory
            if (!m_pHWDevice)
            {
                m_pFrameAllocator = m_pDeviceDXVA->GetFrameAllocator();
                CComPtr<IUnknown> pUnknown;
                //no ext addref
                pUnknown.p = m_pDeviceDXVA->GetDeviceManager();

                if (pUnknown)
                {
#if MFX_D3D11_SUPPORT
                    pUnknown->QueryInterface(IID_ID3D11Device, (void**)&m_pHWDevice);
                    if(m_pHWDevice)
                    {
                        m_MemType = MFT_MEM_DX11;
                    }
                    else
#endif
                    {
                        pUnknown->QueryInterface(IID_IDirect3DDeviceManager9, (void**)&m_pHWDevice);
                        m_MemType = MFT_MEM_DX9;
                    }

                    if (NULL == m_pHWDevice)
                    {
                        sts = MFX_ERR_NULL_PTR;
                        m_MemType = MFT_MEM_SW;
                    }

                }
                else sts = MFX_ERR_NULL_PTR;
                MFX_LTRACE_P(MF_TL_GENERAL, m_pFrameAllocator);
                MFX_LTRACE_P(MF_TL_GENERAL, m_pHWDevice);
            }
        }
    }
    // Free samples pool allocation (required for HW mode, nice to have for SW)
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
            if (FAILED(m_pFreeSamplesPool->Init(m_uiSurfacesNum, this))) sts = MFX_ERR_UNKNOWN;
        }
    }
    if (MFX_ERR_NONE == sts) sts = InitFRA(); // initialize FRA if needed
    if (MFX_ERR_NONE == sts)
    {
        m_pWorkSurfaces = (MFYuvOutSurface**)calloc(m_uiSurfacesNum, sizeof(MFYuvOutSurface*));
        m_pOutSurfaces  = (MFYuvOutData*)calloc(m_uiSurfacesNum, sizeof(MFYuvOutData));
        if (!m_pWorkSurfaces || !m_pOutSurfaces) sts = MFX_ERR_MEMORY_ALLOC;
        else
        {
            for (i = 0; (MFX_ERR_NONE == sts) && (i < m_uiSurfacesNum); ++i)
            {
                SAFE_NEW(m_pWorkSurfaces[i], MFYuvOutSurface);
                if (!m_pWorkSurfaces[i]) sts = MFX_ERR_MEMORY_ALLOC;
                if (MFX_ERR_NONE == sts) m_pWorkSurfaces[i]->SetFile(m_dbg_decout);
                if (MFX_ERR_NONE == sts) sts = m_pWorkSurfaces[i]->Init(&(m_MfxParamsVideo.mfx.FrameInfo), m_pFreeSamplesPool, m_pFrameAllocator ? m_pFrameAllocator->GetMFXAllocator() : NULL, (MFT_MEM_DX9 == m_MemType) ? true : false);
                if (MFX_ERR_NONE == sts) sts = m_pWorkSurfaces[i]->Alloc((m_pFrameAllocator)? m_DecAllocResponse.mids[i]: NULL);
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
    }
    MFX_LTRACE_I(MF_TL_GENERAL, m_uiSurfacesNum);
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

mfxStatus MFPluginVDec::InitMfxVideoSession(mfxIMPL implD3D)
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
        m_MSDK_impl = MF_MFX_IMPL;
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

            SAFE_NEW(m_pmfxDEC, MFXVideoDECODE(*session));
            if (!m_pmfxDEC) sts = MFX_ERR_MEMORY_ALLOC;
            MFX_LTRACE_1(MF_TL_PERF, "SAFE_NEW(m_pmfxDEC, MFXVideoDECODE(*session)); = ", "%d", sts);
        }
        if (MFX_ERR_NONE != sts) hr = E_FAIL;
        MFX_LTRACE_I(MF_TL_PERF, m_MSDK_impl);
    }
    return sts;
}

/*------------------------------------------------------------------------------*/

mfxStatus MFPluginVDec::CloseInitMfxVideoSession(mfxIMPL implD3D)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;

    if (m_pD3DMutex) m_pD3DMutex->Lock();   //this mutex will be recursively locked by InitMfxVideoSession
    SAFE_DELETE(m_pmfxDEC);
    m_pMfxVideoSession->Close();
    SAFE_RELEASE(m_pMfxVideoSession);

    sts = InitMfxVideoSession(implD3D);
    if (m_pD3DMutex) m_pD3DMutex->Unlock(); //already unlocked in InitMfxVideoSession
    return sts;
}

/*------------------------------------------------------------------------------*/

mfxStatus MFPluginVDec::SetHandle(mfxHandleType handleType, bool bAllowCloseInit)
{
    mfxStatus sts = MFX_ERR_NONE;

    mfxIMPL initInterface = m_MSDK_impl & -MFX_IMPL_VIA_ANY;
    if(MFX_IMPL_VIA_D3D11 == initInterface && MFX_HANDLE_D3D9_DEVICE_MANAGER == handleType) // mismatch
        sts = CloseInitMfxVideoSession(MFX_IMPL_VIA_D3D9);

    if (MFX_ERR_NONE == sts)
    {
        sts = m_pMfxVideoSession->SetHandle(handleType, m_pHWDevice);
    }
    if (MFX_ERR_UNDEFINED_BEHAVIOR == sts && bAllowCloseInit)
    {
        // MediaSDK starting from API verions 1.6 requires SetHandle to be called
        // right after MFXInit before any Query / QueryIOSurf calls, otherwise it
        // creates own and returns UNDEFINED_BEHAVIOR if application tries to set handle
        // afterwards.  Need to reinit MediaSDK session to set proper handle.
        mfxIMPL implD3D = MFX_IMPL_UNSUPPORTED;
        switch (handleType)
        {
            case MFX_HANDLE_D3D11_DEVICE:               implD3D = MFX_IMPL_VIA_D3D11; break;
            case MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9:   implD3D = MFX_IMPL_VIA_D3D9; break;
            default: ATLASSERT(false); //support others if needed.
        }
        sts = CloseInitMfxVideoSession(implD3D);
        if (MFX_ERR_NONE == sts)
        {
            sts = m_pMfxVideoSession->SetHandle(handleType, m_pHWDevice);
            MFX_LTRACE_1(MF_TL_GENERAL, "SetHandle failed, sts=", "%d", sts);
            ATLASSERT(MFX_ERR_NONE == sts);
        }
        if (MFX_ERR_NONE == sts)
        {
            if (m_pInputType)
            {
                if (!CheckHwSupport())
                {
                    //Query fails after changing the device.
                    ATLASSERT(false);
                    sts = MFX_ERR_UNKNOWN;
                }
            }
            else
            {
                // check HW caps
                mfxVideoParam videoParam;
                memset(&videoParam, 0, sizeof(videoParam));
                videoParam.mfx.CodecId = m_MfxParamsVideo.mfx.CodecId;
                mfxStatus sts = m_pmfxDEC->Query(0, &videoParam);
                if ((MFX_ERR_NONE != sts) && (m_dbg_no_SW_fallback || (MFX_WRN_PARTIAL_ACCELERATION != sts)))
                {
                    MFX_LTRACE_I(MF_TL_PERF, sts);
                    ATLASSERT(MFX_ERR_NONE == sts);
                }
            }
        }
    }
    return sts;
}

/*------------------------------------------------------------------------------*/
// Initialize codec

mfxStatus MFPluginVDec::InitCodec(MyAutoLock& lock)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;

    if ((MFX_ERR_NONE == sts) && (stHeaderNotDecoded == m_State))
    {
        mfxVideoParam VideoParams;

        // saving parameters
        memcpy_s(&VideoParams, sizeof(VideoParams), &m_MfxParamsVideo, sizeof(m_MfxParamsVideo));
        // decoding header
        sts = m_pmfxDEC->DecodeHeader(m_pBitstream, &m_MfxParamsVideo);

        if (MFX_ERR_NONE == sts && m_MfxParamsVideo.mfx.CodecId == MFX_CODEC_JPEG)
        {
            sts = MJPEG_AVI_ParsePicStruct(m_pBitstream);

            //align height appropriately
            if (MFX_ERR_NONE == sts &&
                m_pBitstream->PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
                m_pBitstream->PicStruct != MFX_PICSTRUCT_UNKNOWN)
            {
                m_MfxParamsVideo.mfx.FrameInfo.Height = (m_MfxParamsVideo.mfx.FrameInfo.Height << 1);
                m_MfxParamsVideo.mfx.FrameInfo.CropH  = (m_MfxParamsVideo.mfx.FrameInfo.CropH << 1);

                m_MfxParamsVideo.mfx.FrameInfo.PicStruct = m_pBitstream->PicStruct;
            }
        }

        // valid cases are:
        // MFX_ERR_NONE - header decoded, initialization can be performed
        // MFX_ERR_MORE_DATA - header not decoded, need provide more data
        if (MFX_ERR_MORE_DATA != sts) m_State = stReady;
        if (!m_dbg_no_SW_fallback && (MFX_WRN_PARTIAL_ACCELERATION == sts)) sts = MFX_ERR_NONE;
        if (MFX_ERR_NONE == sts)
        {
            // returning some parameter values
            m_MfxParamsVideo.mfx.NumThread = VideoParams.mfx.NumThread;
            if (!m_MfxParamsVideo.mfx.FrameInfo.FrameRateExtN || !m_MfxParamsVideo.mfx.FrameInfo.FrameRateExtD)
            {
                m_MfxParamsVideo.mfx.FrameInfo.FrameRateExtN = VideoParams.mfx.FrameInfo.FrameRateExtN;
                m_MfxParamsVideo.mfx.FrameInfo.FrameRateExtD = VideoParams.mfx.FrameInfo.FrameRateExtD;
            }
            if (!m_MfxParamsVideo.mfx.FrameInfo.AspectRatioW || !m_MfxParamsVideo.mfx.FrameInfo.AspectRatioH)
            {
                m_MfxParamsVideo.mfx.FrameInfo.AspectRatioW = VideoParams.mfx.FrameInfo.AspectRatioW;
                m_MfxParamsVideo.mfx.FrameInfo.AspectRatioH = VideoParams.mfx.FrameInfo.AspectRatioH;
            }
            // catching changes in output media type
            if (m_bOutputTypeChangeRequired ||
                (VideoParams.mfx.FrameInfo.Width != m_MfxParamsVideo.mfx.FrameInfo.Width) ||
                (VideoParams.mfx.FrameInfo.Height != m_MfxParamsVideo.mfx.FrameInfo.Height) ||
                (VideoParams.mfx.FrameInfo.AspectRatioW != m_MfxParamsVideo.mfx.FrameInfo.AspectRatioW) ||
                (VideoParams.mfx.FrameInfo.AspectRatioH != m_MfxParamsVideo.mfx.FrameInfo.AspectRatioH) ||
                (VideoParams.mfx.FrameInfo.CropX != m_MfxParamsVideo.mfx.FrameInfo.CropX) ||
                (VideoParams.mfx.FrameInfo.CropY != m_MfxParamsVideo.mfx.FrameInfo.CropY) ||
                (VideoParams.mfx.FrameInfo.CropW != m_MfxParamsVideo.mfx.FrameInfo.CropW) ||
                (VideoParams.mfx.FrameInfo.CropH != m_MfxParamsVideo.mfx.FrameInfo.CropH) ||
                (VideoParams.mfx.FrameInfo.PicStruct != m_MfxParamsVideo.mfx.FrameInfo.PicStruct))
            {
                MFX_LTRACE_S(MF_TL_GENERAL, "Output Type is needed to be changed");
                m_bOutputTypeChangeRequired = false;
                m_bChangeOutputType = true;
            }
            if (m_bChangeOutputType)
            {
                memcpy_s(&m_FrameInfo_original, sizeof(m_FrameInfo_original),
                         &(VideoParams.mfx.FrameInfo), sizeof(VideoParams.mfx.FrameInfo));
                //TODO: AsyncDepth, Protected, IOPattern, ExtParam, NumExtParam
            }
            else
            {
                memcpy_s(&m_FrameInfo_original, sizeof(m_FrameInfo_original),
                         &(m_MfxParamsVideo.mfx.FrameInfo), sizeof(m_MfxParamsVideo.mfx.FrameInfo));
                //TODO: AsyncDepth, Protected, IOPattern, ExtParam, NumExtParam
            }
        }
        else
        {
            // copying parameters back on MFX_ERR_MORE_DATA and errors
            memcpy_s(&m_MfxParamsVideo, sizeof(m_MfxParamsVideo), &VideoParams, sizeof(VideoParams));
        }
    }
    // parameters correction
    if (MFX_ERR_NONE == sts)
    {
        if (!m_pDeviceDXVA)
        {
            m_pOutputType->GetUnknown(MF_MT_D3D_DEVICE, IID_IMFDeviceDXVA, (void**)&m_pDeviceDXVA);
        }
        // by default setting preffered memory type
        m_MfxParamsVideo.IOPattern = (mfxU16)((m_pHWDevice || m_pDeviceDXVA)?
                                        MFX_IOPATTERN_OUT_VIDEO_MEMORY:
                                        MFX_IOPATTERN_OUT_SYSTEM_MEMORY);
        // now corrrecting memory type
        if (dbgHwMemory != m_dbg_Memory)
        {
            if (m_pDeviceDXVA && (MFX_IMPL_SOFTWARE == m_MSDK_impl))
            { // forcing SW memory (playback case is excluded, if m_pHWDevice was set)
                m_MfxParamsVideo.IOPattern = (mfxU16)MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
            }
        }
        else
        { // we were forced to use HW memory, checking if this is possible
            if (!m_pHWDevice && !m_pDeviceDXVA) sts = MFX_ERR_ABORTED;
        }
    }
    // setting some parameters from output type
    if (MFX_ERR_NONE == sts)
    {
        mfxU32 colorFormat  = mf_get_color_format (m_pOutputInfo->subtype.Data1);
        mfxU16 chromaFormat = mf_get_chroma_format(m_pOutputInfo->subtype.Data1);

        if (colorFormat)
        {
            m_MfxParamsVideo.mfx.FrameInfo.FourCC       = colorFormat;
            m_MfxParamsVideo.mfx.FrameInfo.ChromaFormat = chromaFormat;
        }
        else sts = MFX_ERR_UNSUPPORTED;
    }
    if (MFX_ERR_NONE == sts) sts = InitSRF(lock); // init surfaces
    // Decoder initialization
    if (MFX_ERR_NONE == sts)
    {
        sts = m_pmfxDEC->Init(&m_MfxParamsVideo);
        if (!m_dbg_no_SW_fallback && (MFX_WRN_PARTIAL_ACCELERATION == sts)) sts = MFX_ERR_NONE;
    }
    if (MFX_ERR_NONE == sts)
    {
#if !MFX_D3D11_SUPPORT
        m_pmfxDEC->SetSkipMode((mfxSkipMode)MFX_MFT_WORKAROUND);
#endif
        sts = m_pmfxDEC->GetVideoParam(&m_MfxParamsVideo);
        m_MfxCodecInfo.m_uiOutFramesType = m_MfxParamsVideo.IOPattern;
    }
    if (MFX_ERR_MORE_DATA != sts)
    {
        m_MfxCodecInfo.m_InitStatus = sts;
        if (MFX_ERR_NONE != sts) CloseCodec(lock);
        else
        {
            MFX_LTRACE_I(MF_TL_GENERAL, m_MSDK_impl);
            MFX_LTRACE_P(MF_TL_GENERAL, m_pHWDevice);
        }
    }
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

void MFPluginVDec::CloseCodec(MyAutoLock& lock)
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
    if (m_pmfxDEC) m_pmfxDEC->Close();
    if (m_pWorkSurfaces)
    {
        for (i = 0; i < m_uiSurfacesNum; ++i)
        {
            if (m_pWorkSurfaces) SAFE_DELETE(m_pWorkSurfaces[i]);
            if (m_pOutSurfaces)  SAFE_DELETE(m_pOutSurfaces[i].pSyncPoint);
        }
    }
    SAFE_FREE(m_pWorkSurfaces);
    SAFE_FREE(m_pOutSurfaces);
    if (m_pFrameAllocator)
    { // free memory if it was allocated inside decoder
        m_pFrameAllocator->GetMFXAllocator()->Free(m_pFrameAllocator->GetMFXAllocator()->pthis, &m_DecAllocResponse);
        memset((void*)&m_DecAllocResponse, 0, sizeof(mfxFrameAllocResponse));
    }
    if (!m_bReinit)
    {
        SAFE_RELEASE(m_pFrameAllocator);
        m_pHWDevice.Release();
        if (m_pDeviceDXVA) m_pDeviceDXVA->ReleaseFrameAllocator();
        SAFE_RELEASE(m_pDeviceDXVA);

        if (m_pMFBitstream) m_pMFBitstream->Reset();
        SAFE_RELEASE(m_pPostponedInput);
    }
    // return parameters to initial state
    m_State         = stHeaderNotDecoded;
    m_uiSurfacesNum = MF_RENDERING_SURF_NUM;
    m_pWorkSurface  = NULL;
    m_pOutSurface   = NULL;
    m_pDispSurface  = NULL;
    m_iNumberLockedSurfaces = 0;
}

/*------------------------------------------------------------------------------*/

mfxStatus MFPluginVDec::ResetCodec(MyAutoLock& lock)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE, res = MFX_ERR_NONE;
    mfxU32 i = 0;
    MFYuvOutData* pDispSurface = NULL;

    m_pAsyncThreadSemaphore->Reset();
    m_pAsyncThreadEvent->Reset();
    while (m_uiHasOutputEventExists && (m_pDispSurface != m_pOutSurface) || (m_uiHasOutputEventExists == m_uiSurfacesNum && m_pDispSurface == m_pOutSurface))
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
        if ((mfxU32)(pDispSurface-m_pOutSurfaces) >= m_uiSurfacesNum)
            pDispSurface = m_pOutSurfaces;
        m_pDispSurface = pDispSurface;
    }
    if (m_pmfxDEC && (stReady == m_State))
    { // resetting decoder
        sts = m_pmfxDEC->Reset(&m_MfxParamsVideo);
        if (!m_dbg_no_SW_fallback && (MFX_WRN_PARTIAL_ACCELERATION == sts)) sts = MFX_ERR_NONE;
        if (MFX_ERR_NONE == sts) sts = res;
    }
    if (m_pMFBitstream)
    {
        if (FAILED(m_pMFBitstream->Reset()) && (MFX_ERR_NONE == sts)) sts = MFX_ERR_UNKNOWN;
    }
    if (m_pWorkSurfaces)
    { // releasing resources
        for (i = 0; i < m_uiSurfacesNum; ++i)
        {
            //if (FAILED((m_pWorkSurfaces[i])->Release()) && (MFX_ERR_NONE == sts)) sts = MFX_ERR_UNKNOWN;
            m_pWorkSurfaces[i]->Release(); // if release failed that's not an error
        }

        SetFreeWorkSurface(lock);
    }
    m_bEndOfInput      = false;
    m_bStartDrain      = false;
    m_bNeedWorkSurface = false;
    m_bOutputTypeChangeRequired = false;

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
    m_iNumberLockedSurfaces  = 0;

    m_hrError = S_OK;
    m_MfxCodecInfo.m_hrErrorStatus = S_OK;
    m_MfxCodecInfo.m_ErrorStatus = MFX_ERR_NONE;

    m_SyncOpSts = MFX_ERR_NONE;
    //to no run async thread twice in case of message command flush
    if (MFT_MESSAGE_COMMAND_FLUSH == m_eLastMessage)
    {
        m_pAsyncThreadEvent->Signal();
    }

    m_ExtradataTransport.Clear();

    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

/* TryReset function notes:
 *  1. This function reinits decoder plug-in only if downstream plug-in is
 * 3-d party one.
 *  2. If dowsntream plug-in is INTC then decoder will be reinited thru
 * IMFResetCallback interface.
 *  3. Reset handled by TryReinit function can't start if not all frames arrived
 * from downstream plug-in.
 */
HRESULT MFPluginVDec::TryReinit(MyAutoLock& lock, mfxStatus& sts, bool bReady)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    HRESULT hr = S_OK;

    sts = MFX_ERR_NONE;
    if (SUCCEEDED(m_hrError))
    {
        if (!m_pDeviceDXVA || m_pDeviceDXVA && bReady)
        { // if we are connected with 3-d party plug-in we can reinitialize here
            HRESULT hr_sts = S_OK;

            m_bReinitStarted = true;

            m_LastFramerate = GetCurrentFramerate();
            if (m_LastFramerate) m_LastPts += (mfxU64)((mfxF64)MF_TIME_STAMP_FREQUENCY/m_LastFramerate);

            CloseCodec(lock);
            m_bReinit = false;
            m_bSetDiscontinuityAttribute = true;

            if (m_pReinitMFBitstream) // "resolution change" from SetInputType
            {
                // loading new frame info arrived from SetInputType
                memcpy_s(&m_MfxParamsVideo, sizeof(m_MfxParamsVideo), &m_VideoParams_input, sizeof(m_VideoParams_input));

                // deleting old bitstream
                m_pMFBitstream->Reset();
                SAFE_DELETE(m_pMFBitstream);
                // setting new bitstream
                m_pMFBitstream = m_pReinitMFBitstream;
                m_pReinitMFBitstream = NULL;

                if (m_pPostponedInput)
                {
                    hr = DecodeSample(lock, m_pPostponedInput, sts);
                    SAFE_RELEASE(m_pPostponedInput);
                }
                else if (!m_bDoNotRequestInput && !m_NeedInputEventInfo.m_requested) hr = RequestInput();
            }
            else
            {
                m_pBitstream = m_pMFBitstream->GetInternalMfxBitstream();
                if (SUCCEEDED(hr) && m_pBitstream && m_bSetDiscontinuityAttribute)
                {
                    // setting first time stamp after reinitilization if there is no time stamp on bitstream
                    if (MF_TIME_STAMP_INVALID == m_pBitstream->TimeStamp)
                    {
                        MFX_LTRACE_F(MF_TL_PERF, REF2SEC_TIME(MFX2REF_TIME(m_LastPts)));
                        m_pBitstream->TimeStamp = m_LastPts;
                    }
                }
                if (SUCCEEDED(hr))
                {
                    sts = InitCodec(lock);
                    if ((MFX_ERR_NONE != sts) && (MFX_ERR_MORE_DATA != sts)) hr = E_FAIL;
                }
                if (SUCCEEDED(hr)) hr_sts = DecodeFrame(lock, sts);
                if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
                hr_sts = m_pMFBitstream->Sync();
                if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
                m_pBitstream = NULL;
                if (SUCCEEDED(hr) && !m_bReinit && !m_bDoNotRequestInput)
                {
                    // requesting input while free work surfaces are available
                    if (!m_NeedInputEventInfo.m_requested && !m_bNeedWorkSurface)
                    {
                        HRESULT hr_sts = S_OK;

                        hr_sts = RequestInput();
                        if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
                    }
                }
            }
            m_bReinitStarted = false;
        }
    }
    else
    {
        m_bReinitStarted = true;
        CloseCodec(lock);
        m_bReinit = false;
        SAFE_RELEASE(m_pPostponedInput);
        if (!m_bDoNotRequestInput && !m_NeedInputEventInfo.m_requested) hr = RequestInput();
        m_bReinitStarted = false;
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

mfxF64 MFPluginVDec::GetCurrentFramerate(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    mfxF64 framerate = 0;
    mfxVideoParam params;

    memset(&params, 0, sizeof(mfxVideoParam));
    if (!m_pmfxDEC || (MFX_ERR_NONE != m_pmfxDEC->GetVideoParam(&params)))
    {
        memcpy_s(&params, sizeof(params), &m_MfxParamsVideo, sizeof(m_MfxParamsVideo));
    }
    if (!params.mfx.FrameInfo.FrameRateExtN ||
        !params.mfx.FrameInfo.FrameRateExtD)
    {
        params.mfx.FrameInfo.FrameRateExtN = MF_DEFAULT_FRAMERATE_NOM;
        params.mfx.FrameInfo.FrameRateExtD = MF_DEFAULT_FRAMERATE_DEN;
    }
    framerate = mf_get_framerate(params.mfx.FrameInfo.FrameRateExtN,
                                 params.mfx.FrameInfo.FrameRateExtD);
    MFX_LTRACE_F(MF_TL_PERF, framerate);
    return framerate;
}

/*------------------------------------------------------------------------------*/

inline HRESULT MFPluginVDec::SetFreeWorkSurface(MyAutoLock& /*lock*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    HRESULT hr = E_FAIL, hr_sts = S_OK;
    mfxU32 i = 0, j = 0;
    bool bFound = false, bWait = false;

    do
    {
        for (i = 0; i < m_uiSurfacesNum; ++i)
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
#if 0
        // wait for free surface in HW case only
        bWait = (m_pHWDevice && !bFound);
#else
        // wait for free surface in SW and HW case
        bWait = !bFound;
#endif
        if (bWait) Sleep(MF_DEV_BUSY_SLEEP_TIME);
#endif
        ++j;
    } while (bWait && (j < MF_FREE_SURF_WAIT_NUM));

    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

inline HRESULT MFPluginVDec::SetFreeOutSurface(MyAutoLock& /*lock*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    HRESULT hr = S_OK;
    MFYuvOutData* pOutSurface = m_pOutSurface;

    ++pOutSurface;
    if ((mfxU32)(pOutSurface-m_pOutSurfaces) >= m_uiSurfacesNum)
        pOutSurface = m_pOutSurfaces;
    // setting free out surface using atomic operation
    m_pOutSurface = pOutSurface;
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVDec::CopyAllAttributes(CComPtr<IMFAttributes> pSrc, CComPtr<IMFSample> pOutSample)
{
    HRESULT hr = S_OK;
    GUID guidKey;
    PROPVARIANT Value;
    const GUID guidsToSave[] = {MF_MT_MFX_FRAME_SRF};
    UINT32 count = 0;

    if (NULL == pSrc || NULL == pOutSample)
        hr = E_FAIL;

    CComPtr<IMFAttributes> pTmpAttr;
    if (SUCCEEDED(hr))
    {
        hr = MFCreateAttributes(&pTmpAttr, 0);
    }

    if (SUCCEEDED(hr) && FAILED(pOutSample->GetCount(&count)))
    {
        count = 0;
    }
    if (SUCCEEDED(hr) && count > 0)
    {
        hr = pOutSample->CopyAllItems(pTmpAttr);
    }

    if (SUCCEEDED(hr))
    {
        hr = pSrc->CopyAllItems(pOutSample);
    }
    for (UINT32 i = 0; i < count && SUCCEEDED(hr); i++)
    {
        hr = pTmpAttr->GetItemByIndex(i, &guidKey, &Value);
        for (UINT32 k = 0; k < _countof(guidsToSave) && SUCCEEDED(hr); k++)
        {
            if (guidsToSave[k] == guidKey)
                hr = pOutSample->SetItem(guidKey, Value);
        }
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
 *  2. If call of th function is fake, it is needed to be careful with used
 * objects: they can be not initialized yet.
 */

void MFPluginVDec::SampleAppeared(bool bFakeSample, bool bReinitSample)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = S_OK;
    mfxStatus sts = MFX_ERR_NONE;

    MFX_LTRACE_I(MF_TL_PERF, bFakeSample);

    // errors checking
    if (m_bReinit && m_bReinitStarted) return;
    if (bFakeSample)
    { // additional checks on fake call
        if (stReady != m_State) return;
    }
    if (bReinitSample || m_bReinit && !m_bEndOfInput)
    {
        hr = TryReinit(lock, sts, bReinitSample);
    }
    else if (SUCCEEDED(m_hrError) && m_bNeedWorkSurface)
    {
        MFX_LTRACE_S(MF_TL_PERF, "Setting free work surface");
        MFX_LTRACE_I(MF_TL_PERF, m_bEndOfInput);

        // setting free work surface
        if (SUCCEEDED(SetFreeWorkSurface(lock)) && SUCCEEDED(m_hrError))
        {
            m_bNeedWorkSurface = false;

            if (!m_bStartDrain)
            {
                HRESULT hr_sts = S_OK;

                if (NULL != (m_pBitstream = m_pMFBitstream->GetInternalMfxBitstream()))
                { // pushing buffered data to the decoder
                    MFX_AUTO_LTRACE(MF_TL_PERF, "DecodeFrame");
                    hr_sts = DecodeFrame(lock, sts);
                    // in DecodeFrame m_hrError could be set and handled - checking errors
                    if (SUCCEEDED(m_hrError) && SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
                    hr_sts = m_pMFBitstream->Sync();
                    if (SUCCEEDED(m_hrError) && SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
                    m_pBitstream = NULL;
                }
                if (SUCCEEDED(m_hrError) && !m_bReinit && !m_bDoNotRequestInput)
                {
                    if (!m_NeedInputEventInfo.m_requested && !m_bNeedWorkSurface)
                    {
                        RequestInput();
                    }
                }
            }
            else if (!bFakeSample) AsyncThreadPush(); // async thread already started, status is unneeded
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

void MFPluginVDec::SetPlgError(HRESULT hr, mfxStatus sts)
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

void MFPluginVDec::ResetPlgError(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    MFPlugin::ResetPlgError();
    m_MfxCodecInfo.m_uiErrorResetCount = m_uiErrorResetCount;
}

/*------------------------------------------------------------------------------*/

void MFPluginVDec::HandlePlgError(MyAutoLock& lock, bool bCalledFromAsyncThread)
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

bool MFPluginVDec::ReturnPlgError(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    return (m_dbg_return_errors)? true: false;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVDec::AsyncThreadFunc(void)
{
    { MFX_AUTO_LTRACE(MF_TL_PERF, "ThreadName=AT DEC"); }
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);
    mfxStatus sts = MFX_ERR_NONE;
    mfxSyncPoint* pSyncPoint = NULL;
    mfxU16 nTerminateIndex = 0;

    while (1)
    {
        HRESULT hr = S_OK;

        // awaiting for the signal that next surface is ready
        lock.Unlock();
        {
            MFX_AUTO_LTRACE(MF_TL_PERF, "wait::sample");
            while (WAIT_TIMEOUT == m_pAsyncThreadSemaphore->TimedWait(MF_DEC_FAKE_SAMPLE_APPEARED_SLEEP_TIME))
            {
                SampleAppeared(true, false);

                if (++nTerminateIndex > 1200)
                {
                    hr = E_FAIL;
                    nTerminateIndex = 0;

                    MFX_LTRACE_S(MF_TL_PERF, "TerminateIndex > 1200");
                    MFX_LTRACE_I(MF_TL_GENERAL, hr);

                    break;
                }
            }
            nTerminateIndex = 0;
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
            if (stHeaderNotDecoded != m_State)
            {
                m_bStartDrain = true;
                if (m_bNeedWorkSurface)
                {
                    lock.Unlock();
                    SampleAppeared(true, false);
                    lock.Lock();
                }
                // getting remained data in the buffer
                m_pBitstream = (!m_bReinit && m_pMFBitstream)? m_pMFBitstream->GetInternalMfxBitstream(): NULL;
                // decoding data or getting remained frames from the decoder
                hr = DecodeFrame(lock, sts);
                if (m_pBitstream)
                {
                    if (MF_E_TRANSFORM_NEED_MORE_INPUT == hr) m_pMFBitstream->SetNoDataStatus(true);
                    // we are not interested in the status from Sync here
                    m_pMFBitstream->Sync();
                    m_pBitstream = NULL;
                    // getting remained frames from the decoder
                    if (MF_E_TRANSFORM_NEED_MORE_INPUT == hr) hr = DecodeFrame(lock, sts);
                }
            }
            else hr = MF_E_TRANSFORM_NEED_MORE_INPUT;

            if (MF_E_TRANSFORM_NEED_MORE_INPUT == hr)
            {
                hr = S_OK;

                m_bEndOfInput = false;
                m_bStartDrain = false;

                if (m_pMFBitstream) m_pMFBitstream->SetNoDataStatus(false);
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
                if (!m_bNeedWorkSurface) AsyncThreadPush();
                continue;
            }
        }
        if (SUCCEEDED(hr) && SUCCEEDED(m_hrError))
        {
            //this check is not thread-safe but covers particular paranoid
            //Klocwork issue with HandlePlgError && m_bReinit == true.
            if (NULL == m_pDispSurface)
            {
                MFX_LTRACE_P(MF_TL_PERF, m_pDispSurface);
                ATLASSERT(NULL != m_pDispSurface);
                hr = E_FAIL;
            }
        }
        if (SUCCEEDED(hr) && SUCCEEDED(m_hrError))
        {
            // awaiting for the async operation completion if needed
            if (m_pDispSurface->bSyncPointUsed) sts = m_pDispSurface->iSyncPointSts;
            else
            {
#ifdef OMIT_DEC_ASYNC_SYNC_POINT
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
            // sending request for ProcessOutput callB
            if (MFX_ERR_NONE == sts) hr = SendOutput();
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
bool MFPluginVDec::HandleDevBusy(mfxStatus& sts)
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
        if ((mfxU32)(pOutSurface-m_pOutSurfaces) >= m_uiSurfacesNum) pOutSurface = m_pOutSurfaces;
    } while(pOutSurface != m_pDispSurface);
    MFX_LTRACE_I(MF_TL_GENERAL, ret_val);
    return ret_val;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVDec::DecodeFrame(MyAutoLock& lock, mfxStatus& sts)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    HRESULT hr = S_OK;
    mfxU32 i = 0;
    bool bSendFakeSrf = false;

    sts = MFX_ERR_NONE;
    while (SUCCEEDED(hr) && (stReady == m_State) && !m_bNeedWorkSurface &&
           (!m_pBitstream || m_pBitstream->DataLength))
    {
        mfxFrameSurface1* pWorkSurface = m_pWorkSurface->GetSurface(); // can not be NULL

        if (pWorkSurface) // should always be valid pointer
        {
            mfxU32 nTerminateIndex = 0;

            pWorkSurface->Data.FrameOrder = m_iNumberLockedSurfaces;
            // Decoding bitstream. If function called on EOS then draining begins and we must
            // wait while decoder can accept input data
            do
            {
                sts = m_pmfxDEC->DecodeFrameAsync(m_pBitstream, pWorkSurface,
                                                  &(m_pOutSurface->pSurface),
                                                  m_pOutSurface->pSyncPoint);
                if ((MFX_WRN_DEVICE_BUSY == sts) && (MFX_ERR_NONE == m_SyncOpSts))
                {
                    MFX_AUTO_LTRACE(MF_TL_PERF, "mfx(DecodeFrameAsync)::MFX_WRN_DEVICE_BUSY");
                    mfxStatus res = MFX_ERR_NONE;

                    if (!HandleDevBusy(res))
                    {
                        m_pDevBusyEvent->TimedWait(MF_DEV_BUSY_SLEEP_TIME);
                        m_pDevBusyEvent->Reset();
                    }
                    if (MFX_ERR_NONE != res) sts = res;

                    if ((MFX_WRN_DEVICE_BUSY == sts) && (++nTerminateIndex > 1000))
                    {
                        sts = MFX_ERR_ABORTED;
                    }
                }
            } while ((MFX_WRN_DEVICE_BUSY == sts) && (MFX_ERR_NONE == m_SyncOpSts));
        }
        else
        {
            ATLASSERT(!"Invalid pWorkSurface");
            sts = MFX_ERR_NULL_PTR;
        }
        // valid cases for the status are:
        // MFX_ERR_NONE - data processed, output will be generated
        // MFX_ERR_MORE_DATA - data buffered, output will not be generated
        // MFX_WRN_VIDEO_PARAM_CHANGED - some params changed, but decoding can be continued
        // MFX_ERR_INCOMPATIBLE_VIDEO_PARAM - some params changed, decoding should be reinitialized

        // in case of corrupted we assume that internal logic of MPEG2 decoder is broken
        // need to stop decoding
        if (MFX_CODEC_MPEG2 == m_MfxParamsVideo.mfx.CodecId && NULL != m_pOutSurface->pSurface && m_pOutSurface->pSurface->Data.Corrupted)
        {
            sts = MFX_ERR_ABORTED;
        }

        // Status correction
        if (MFX_WRN_VIDEO_PARAM_CHANGED == sts) sts = MFX_ERR_MORE_SURFACE;
        else if ((MFX_ERR_MORE_DATA == sts) && m_bStartDrain && m_bSendFakeSrf)
        {
            sts = MFX_ERR_NONE;
            m_pOutSurface->pSurface = pWorkSurface;
            m_pOutSurface->iSyncPointSts  = MFX_ERR_NONE;
            m_pOutSurface->bSyncPointUsed = true;

            // we will send fake surface only once on each re-init
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
            HRESULT hr_sts = S_OK;
            bool bOutExists = (NULL != m_pOutSurface->pSurface);

            if ((MFX_ERR_NONE == sts) && bOutExists)
            {
                // searching for MFSurface
                for (i = 0; i < m_uiSurfacesNum; ++i)
                {
                    if (m_pOutSurface->pSurface == m_pWorkSurfaces[i]->GetSurface())
                    {
                        if (!bSendFakeSrf) ++m_iNumberInputSurfaces;
                        m_pOutSurface->pMFSurface = m_pWorkSurfaces[i];
                        // locking surface for future displaying
                        m_pOutSurface->pMFSurface->IsDisplayed(false);
                        m_pOutSurface->pMFSurface->IsFakeSrf(bSendFakeSrf);
                        m_pOutSurface->pMFSurface->IsGapSrf(m_bSetDiscontinuityAttribute);
                        if (m_bSetDiscontinuityAttribute) m_bSetDiscontinuityAttribute = false;
                        // saving time stamp for future use
                        m_LastPts = m_pOutSurface->pSurface->Data.TimeStamp;
                        // sending output
                        ++m_uiHasOutputEventExists;
                        AsyncThreadPush();
                        break;
                    }
                }
                // normally, the following will not occur
                if (SUCCEEDED(hr_sts) && (i >= m_uiSurfacesNum)) hr_sts = E_FAIL;
                // checking errors
                if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
                // setting free out surface
                hr_sts = SetFreeOutSurface(lock);
                if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
            }
            if (pWorkSurface->Data.Locked || bOutExists)
            {
                if (pWorkSurface->Data.Locked) ++m_iNumberLockedSurfaces;
                // setting free work surface
                if (FAILED(SetFreeWorkSurface(lock)))
                { // will wait for free surface
                    sts = MFX_ERR_MORE_DATA;
                    m_bNeedWorkSurface = true;
                }
                MFX_LTRACE_I(MF_TL_PERF, m_bNeedWorkSurface);
            }
            if (MFX_ERR_MORE_DATA == sts) break;
        }
        else if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == sts)
        {
            // setting flags to process this situation
            m_bReinit      = true;
            m_bEndOfInput  = true; // we need to obtain buffered frames
            if (m_pDeviceDXVA) m_bSendFakeSrf = true; // we need to send fake frame
            else m_bSendFakeSrf = false;
            // pushing async thread (starting call DecFrameAsync with NULL bitstream)
            AsyncThreadPush();
            break;
        }
        else hr = E_FAIL;
        // no iterations on EOS
        if (m_bEndOfInput) break;
    }
    MFX_LTRACE_I(MF_TL_PERF, sts);
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVDec::DecodeSample(MyAutoLock& lock, IMFSample* pSample, mfxStatus& sts)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    HRESULT hr = S_OK;

    sts = MFX_ERR_NONE;
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
        if (SUCCEEDED(hr)) hr = m_pMFBitstream->Load(pSample);

        while (SUCCEEDED(hr) && (NULL != (m_pBitstream = m_pMFBitstream->GetMfxBitstream())))
        {
            HRESULT hr_sts = S_OK;

            if (SUCCEEDED(hr) && m_bSetDiscontinuityAttribute)
            {
                // setting first time stamp after reinitilization if there is no time stamp on bitstream
                if (MF_TIME_STAMP_INVALID == m_pBitstream->TimeStamp)
                {
                    MFX_LTRACE_F(MF_TL_PERF, REF2SEC_TIME(MFX2REF_TIME(m_LastPts)));
                    m_pBitstream->TimeStamp = m_LastPts;
                }
            }
            // Decoder initialization
            if (SUCCEEDED(hr) && (stReady != m_State))
            {
                sts = InitCodec(lock);
                if ((MFX_ERR_NONE != sts) && (MFX_ERR_MORE_DATA != sts)) hr = E_FAIL;
            }
            // Input data processing
            if (SUCCEEDED(hr)) hr = DecodeFrame(lock, sts);
            // Synchronizing input buffer
            hr_sts = m_pMFBitstream->Sync();
            if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;

            if (m_bReinit) break;
        }
        m_pMFBitstream->Unload();
        if (SUCCEEDED(hr) && !m_bReinit && !m_bDoNotRequestInput)
        {
            // requesting input while free work surfaces are available
            if (!m_NeedInputEventInfo.m_requested && !m_bNeedWorkSurface)
            {
                HRESULT hr_sts = S_OK;

                hr_sts = RequestInput();
                if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
            }
        }
    }
    MFX_LTRACE_I(MF_TL_PERF, sts);
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVDec::ProcessInput(DWORD      dwInputStreamID,
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

    if (stReady == m_State)
        sts = TestDeviceAndReinit(lock);
    CHECK_EXPRESSION(MFX_ERR_NONE == sts, E_FAIL);

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
    if (SUCCEEDED(hr))
    {
        hr = pData->SetSampleDuration(pSample);
        ATLASSERT(SUCCEEDED(hr));
    }
    if (SUCCEEDED(hr))
    {
        hr = m_ExtradataTransport.Send(m_iNumberLockedSurfaces, pData);
    }

    if (SUCCEEDED(hr)) hr = m_hrError;
    if (SUCCEEDED(hr)) ++(m_MfxCodecInfo.m_nInFrames);
    if (SUCCEEDED(hr)) hr = DecodeSample(lock, pSample, sts);
    if (FAILED(hr) && SUCCEEDED(m_hrError))
    {
        // setting plug-in error
        SetPlgError(hr, sts);
        // handling error
        HandlePlgError(lock);
    }
    if (!ReturnPlgError()) hr = S_OK;
    MFX_LTRACE_I(MF_TL_PERF, m_MfxCodecInfo.m_nInFrames);
    MFX_LTRACE_I(MF_TL_PERF, m_iNumberLockedSurfaces);
    MFX_LTRACE_I(MF_TL_PERF, m_iNumberInputSurfaces);
    MFX_LTRACE_I(MF_TL_PERF, sts);
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPluginVDec::ProcessOutput(DWORD  dwFlags,
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

    SET_HR_ERROR(SUCCEEDED(hr), hr, E_FAIL);
    SET_HR_ERROR(0 == dwFlags, hr, E_INVALIDARG);
    SET_HR_ERROR(m_pmfxDEC, hr, E_FAIL);
    SET_HR_ERROR(m_pDispSurface && ((m_pDispSurface != m_pOutSurface) || m_bNeedWorkSurface || m_bEndOfInput),
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
        MFX_LTRACE_S(MF_TL_PERF, "Normal decoding");
        if (m_uiHasOutputEventExists) --m_uiHasOutputEventExists;
        if (SUCCEEDED(hr) && ((m_pDispSurface != m_pOutSurface) || m_bNeedWorkSurface)) bDisplayFrame = true;
        if (SUCCEEDED(hr) && bDisplayFrame)
        {
            pOutputSamples[0].pSample  = m_pDispSurface->pMFSurface->GetSample();

            if (NULL != pOutputSamples[0].pSample)
            {
                mfxFrameSurface1* pOutSurface = m_pDispSurface->pMFSurface->GetSurface();
                ATLASSERT(NULL != pOutSurface);
                mfxU32 FrameOrder = (NULL == pOutSurface ? _UI32_MAX : pOutSurface->Data.FrameOrder);
                if (_UI32_MAX != FrameOrder)
                {
                    CComPtr<IMFSampleExtradata> pExtraData = m_ExtradataTransport.Receive(FrameOrder);
                    if (NULL != pExtraData)
                    {
                        CComPtr<IMFAttributes> pInAttributes = pExtraData->GetAttributes();
                        if (pInAttributes)
                        {
                            hr = CopyAllAttributes(pInAttributes, pOutputSamples[0].pSample);
                        }
                        if (SUCCEEDED(hr))
                        {
                            REFERENCE_TIME nDuration = pExtraData->GetSampleDuration();
                            if (nDuration)
                            {
                                hr = pOutputSamples[0].pSample->SetSampleDuration(pExtraData->GetSampleDuration());
                            }
                        }
                    }
                }
            }
            if (SUCCEEDED(hr))
                hr = m_pDispSurface->pMFSurface->Sync();

            if (!m_pDeviceDXVA &&
               ((m_MfxParamsVideo.mfx.FrameInfo.Width != m_FrameInfo_original.Width) ||
               (m_MfxParamsVideo.mfx.FrameInfo.Height != m_FrameInfo_original.Height)))
            {
                HRESULT hr_sts = S_OK;
                hr_sts = m_pDispSurface->pMFSurface->Pretend(m_FrameInfo_original.Width, m_FrameInfo_original.Height);
                if (SUCCEEDED(hr) && FAILED(hr_sts)) hr = hr_sts;
            }
        }
        else
            pOutputSamples[0].pSample = NULL;

        // Set status flags
        if (SUCCEEDED(hr))
        {
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

            // in case of corrupted we assume that internal logic of MPEG2 decoder is broken
            // need to stop decoding
            if (MFX_CODEC_MPEG2 == m_MfxParamsVideo.mfx.CodecId && m_pDispSurface->pSurface->Data.Corrupted)
            {
                hr = E_FAIL;
                sts = MFX_ERR_ABORTED;
            }

            // trying to free this surface
            m_pDispSurface->pMFSurface->IsDisplayed(true);
            m_pDispSurface->pMFSurface->Release();
            m_pDispSurface->pSurface   = NULL;
            m_pDispSurface->pMFSurface = NULL;
            m_pDispSurface->bSyncPointUsed = false;
            // moving to next undisplayed surface
            pDispSurface = m_pDispSurface;
            ++pDispSurface;
            if ((mfxU32)(pDispSurface-m_pOutSurfaces) >= m_uiSurfacesNum)
                pDispSurface = m_pOutSurfaces;
            m_pDispSurface = pDispSurface;

            m_pAsyncThreadEvent->Signal();
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
        // errors will be handled either in ProcessInput
        hr = S_OK;
    }
    MFX_LTRACE_I(MF_TL_PERF, m_MfxCodecInfo.m_nOutFrames);
    MFX_LTRACE_I(MF_TL_PERF, sts);
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

mfxStatus MFPluginVDec::TestDeviceAndReinit(MyAutoLock& lock)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    mfxStatus sts = MFX_ERR_NONE;
    HRESULT hr = S_OK;
    mfxIMPL imp = MFX_IMPL_VIA_ANY;
    bool bFullReinit = false;
    mfxHandleType handleType = MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9;

    bool isNoHwDeviceYet = (NULL == m_pHWDevice);
    if (isNoHwDeviceYet)
        return MFX_ERR_NONE;

    if (MFT_MEM_DX9 == m_MemType)
    {
        CComQIPtr<IDirect3DDeviceManager9> d3d9Mgr(m_pHWDevice);
        if (!d3d9Mgr)
        {
            MFX_LTRACE_S(MF_TL_GENERAL, "Failed to query IDirect3DDeviceManager9");
            return MFX_ERR_NULL_PTR;
        }

        hr = d3d9Mgr->TestDevice(m_deviceHandle);
        if (hr != S_OK)
        {
            hr = d3d9Mgr->CloseDeviceHandle(m_deviceHandle);
            if (SUCCEEDED(hr))
            {
                hr = d3d9Mgr->OpenDeviceHandle(&m_deviceHandle);
                imp = MFX_IMPL_VIA_D3D9;
                bFullReinit = true;
            }
        }
    }

    if (bFullReinit)
    {
        m_pDevBusyEvent->Wait();

        m_pAsyncThreadSemaphore->Reset();
        m_pAsyncThreadEvent->Signal();

        CComPtr<IUnknown> pTmpHwDevice = m_pHWDevice;
        CloseCodec(lock);
        m_pHWDevice = pTmpHwDevice;

        sts = CloseInitMfxVideoSession(imp);
        if (MFX_ERR_NONE == sts)
        {
            sts = m_pMfxVideoSession->SetHandle(handleType, m_pHWDevice);
        }

        m_NeedInputEventInfo.m_sent = 0;
        m_NeedInputEventInfo.m_requested = 0;
        m_HasOutputEventInfo.m_sent = 0;
        m_HasOutputEventInfo.m_requested= 0;
    }

    MFX_LTRACE_I(MF_TL_PERF, sts);
    return sts;
}
/*------------------------------------------------------------------------------*/
// Global functions

#undef  PTR_THIS // no 'this' pointer in global functions
#define PTR_THIS NULL

#undef  MFX_TRACE_CATEGORY
#ifdef MF_SHORT_CAT_NAMES
    #define MFX_TRACE_CATEGORY _T("mfx_mft_dec")
#else
    #define MFX_TRACE_CATEGORY (pRegData && pRegData->pPluginName)? pRegData->pPluginName: _T("mfx_mft_enc")
#endif

// CreateInstance
// Static method to create an instance of the source.
//
// iid:         IID of the requested interface on the source.
// ppSource:    Receives a ref-counted pointer to the source.
HRESULT MFPluginVDec::CreateInstance(REFIID iid,
                                     void** ppMFT,
                                     ClassRegData* pRegData)

{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    MFPluginVDec *pMFT = NULL;
    mf_tick tick = mf_get_tick();

    CHECK_POINTER(ppMFT, E_POINTER);
#if 0 // checking of supported platform
    if (!CheckPlatform())
    {
        *ppMFT = NULL;
        return E_FAIL;
    }
#endif
    SAFE_NEW(pMFT, MFPluginVDec(hr, pRegData));
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
