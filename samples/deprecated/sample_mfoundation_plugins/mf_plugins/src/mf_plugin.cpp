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

File: mf_plugin.cpp

Purpose: define base class for Intel MSDK based MediaFoundation MFTs.

*********************************************************************************/
#include "mf_plugin.h"

/*------------------------------------------------------------------------------*/

#undef  MFX_TRACE_CATEGORY
#ifdef MF_SHORT_CAT_NAMES
    #define MFX_TRACE_CATEGORY _T("mfx_mft_unk")
#else
    #define MFX_TRACE_CATEGORY m_Reg.pPluginName
#endif

/*------------------------------------------------------------------------------*/

MFPlugin::MFPlugin(HRESULT &hr, ClassRegData *pRegData):
    m_bUnlocked(false),
    m_bIsShutdown(false),
    m_bStreamingStarted(false),
    m_bDoNotRequestInput(false),
    m_pD3DMutex(NULL),
    m_pPropertyStore(NULL),
    m_pAttributes(NULL),
    m_pEventQueue(NULL),
    m_hrError(S_OK),
    m_uiErrorResetCount(0),
    m_bErrorHandlingStarted(false),
    m_bErrorHandlingFinished(false),
    m_pInputType(NULL),
    m_pOutputType(NULL),
    m_MajorType(GUID_NULL),
    m_pAsyncThread(NULL),
    m_pAsyncThreadSemaphore(NULL),
    m_bAsyncThreadStop(false),
    m_pAsyncThreadEvent(NULL),
    m_pDevBusyEvent(NULL),
    m_pErrorFoundEvent(NULL),
    m_pErrorHandledEvent(NULL),
    m_dbg_MSDK(dbgDefMsdk),
    m_dbg_Memory(dbgDefMemory),
    m_dbg_return_errors(false),
#ifdef MF_NO_SW_FALLBACK
    m_dbg_no_SW_fallback(true)
#else
    m_dbg_no_SW_fallback(false)
#endif
{
    DllAddRef();

    //fail could be in MFCodecMerit constructor, uncomment if want to handle merit errors
    //if (FAILED(hr)) return;

    hr = E_FAIL;
    memset(&m_NeedInputEventInfo, 0, sizeof(MFEventsInfo));
    memset(&m_HasOutputEventInfo, 0, sizeof(MFEventsInfo));
    memset(&m_Reg, 0, sizeof(ClassRegData));
    if (pRegData && pRegData->pPluginName) m_Reg = *pRegData;
    else return;

    // placing auto trace here because we need valid m_Reg.pPluginName pointer
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);

    SAFE_NEW(m_pD3DMutex, MyNamedMutex(hr, _T(MF_D3D_MUTEX)));
    if (!m_pD3DMutex) hr = E_OUTOFMEMORY;
    if (FAILED(hr)) return;

    //TODO: what is this?
#if !MFX_D3D11_SUPPORT
    m_pD3DMutex->Lock();
    hr = mf_test_createdevice();
    m_pD3DMutex->Unlock();
    if (FAILED(hr)) return;
#endif

    hr = MFCreateEventQueue(&m_pEventQueue);
    if (FAILED(hr)) return;

    hr = PSCreateMemoryPropertyStore(IID_PPV_ARGS(&m_pPropertyStore));
    if (SUCCEEDED(hr))
    {
        PROPVARIANT var;

        // preparing Property Store which states that MFT will handle out sample attributes
        memset(&var, 0, sizeof(PROPVARIANT));
        var.vt      = VT_BOOL;
        var.boolVal = VARIANT_TRUE;
        hr = m_pPropertyStore->SetValue(MFPKEY_EXATTRIBUTE_SUPPORTED, var);
    }
    if (FAILED(hr)) return;

    SAFE_NEW(m_pAsyncThreadSemaphore, MySemaphore(hr));
    if (!m_pAsyncThreadSemaphore) hr = E_OUTOFMEMORY;
    if (FAILED(hr)) return;

    SAFE_NEW(m_pAsyncThreadEvent, MyEvent(hr));
    if (!m_pAsyncThreadEvent) hr = E_OUTOFMEMORY;
    if (FAILED(hr)) return;

    SAFE_NEW(m_pErrorFoundEvent, MyEvent(hr));
    if (!m_pErrorFoundEvent) hr = E_OUTOFMEMORY;
    if (FAILED(hr)) return;

    SAFE_NEW(m_pErrorHandledEvent, MyEvent(hr));
    if (!m_pErrorHandledEvent) hr = E_OUTOFMEMORY;
    if (FAILED(hr)) return;

    SAFE_NEW(m_pDevBusyEvent, MyEvent(hr));
    if (!m_pDevBusyEvent) hr = E_OUTOFMEMORY;
    if (FAILED(hr)) return;

/*    SAFE_NEW(m_pAsyncThread, MyThread(hr, thAsyncThreadFunc, this));
    if (!m_pAsyncThread) hr = E_OUTOFMEMORY;
    if (FAILED(hr)) return;*/

    // Creating attributs store initially for 3 items:
    // * MF_TRANSFORM_ASYNC
    // * MFT_SUPPORT_DYNAMIC_FORMAT_CHANGE
    // * MF_TRANSFORM_ASYNC_UNLOCK
    hr = MFCreateAttributes(&m_pAttributes, 3);
    if (FAILED(hr)) return;

    {
        DWORD align = 1;
        if ((S_OK == mf_get_reg_dword(_T(REG_PARAMS_PATH),
                                      _T(REG_ALIGN_DEC), &align)) && align)
        {
            m_gMemAlignDec = (mfxU16)align;
        }
        if ((S_OK == mf_get_reg_dword(_T(REG_PARAMS_PATH),
                                      _T(REG_ALIGN_VPP), &align)) && align)
        {
            m_gMemAlignVpp = (mfxU16)align;
        }
        if ((S_OK == mf_get_reg_dword(_T(REG_PARAMS_PATH),
                                      _T(REG_ALIGN_ENC), &align)) && align)
        {
            m_gMemAlignEnc = (mfxU16)align;
        }
        MFX_LTRACE_I(MF_TL_PERF, m_gMemAlignDec);
        MFX_LTRACE_I(MF_TL_PERF, m_gMemAlignVpp);
        MFX_LTRACE_I(MF_TL_PERF, m_gMemAlignEnc);
    }

    MFX_LTRACE_WS(MF_TL_PERF, (m_Reg.pPluginName)? m_Reg.pPluginName: L"null_plg_name");
}

/*------------------------------------------------------------------------------*/

MFPlugin::~MFPlugin(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MFX_LTRACE_WS(MF_TL_PERF, (m_Reg.pPluginName)? m_Reg.pPluginName: L"null_plg_name");

    SAFE_DELETE(m_pAsyncThread);
    SAFE_DELETE(m_pAsyncThreadSemaphore);
    SAFE_DELETE(m_pAsyncThreadEvent);
    SAFE_DELETE(m_pDevBusyEvent);
    SAFE_DELETE(m_pErrorFoundEvent);
    SAFE_DELETE(m_pErrorHandledEvent);
    SAFE_DELETE(m_pD3DMutex);

    SAFE_RELEASE(m_pPropertyStore);
    SAFE_RELEASE(m_pAttributes);
    SAFE_RELEASE(m_pEventQueue);
    SAFE_RELEASE(m_pInputType);
    SAFE_RELEASE(m_pOutputType);

    DllRelease();
}

/*------------------------------------------------------------------------------*/

void MFPlugin::SetPlgError(HRESULT hr, mfxStatus /*sts*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    MFX_LTRACE_D(MF_TL_GENERAL, m_hrError);

    if (SUCCEEDED(m_hrError)) m_hrError = hr;
}

/*------------------------------------------------------------------------------*/

void MFPlugin::ResetPlgError(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    ++m_uiErrorResetCount;
    m_hrError = S_OK;
}

/*------------------------------------------------------------------------------*/

inline bool MFPlugin::IsMftUnlocked(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    if (!m_bUnlocked)
    { // checking unlock status if MFT is still locked
        UINT32 unlocked = false;
        unlocked = MFGetAttributeUINT32(m_pAttributes,
                                        MF_TRANSFORM_ASYNC_UNLOCK, FALSE);
        m_bUnlocked = (unlocked)? TRUE: FALSE;
    }
    MFX_LTRACE_I(MF_TL_NOTES, m_bUnlocked);
    return m_bUnlocked;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::CheckMediaType(IMFMediaType*     pType,
                                 const GUID_info*  pArray,
                                 DWORD             arraySize,
                                 const GUID_info** pEntry)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    GUID major_type = GUID_NULL;
    GUID subtype = GUID_NULL;
    DWORD i;

    // get major type
    hr = pType->GetGUID(MF_MT_MAJOR_TYPE, &major_type);
    // get subtype
    if (SUCCEEDED(hr))
    {
        MFX_LTRACE_GUID(MF_TL_GENERAL, major_type);
        hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);
    }
    if (SUCCEEDED(hr))
    {
        MFX_LTRACE_GUID(MF_TL_GENERAL, subtype);
        // check major type and subtype to be in our list of accepted types
#if MFX_D3D11_SUPPORT
        hr = MF_E_INVALIDMEDIATYPE;
#else
        hr = MF_E_INVALIDTYPE;
#endif
        for (i = 0; i < arraySize; ++i)
        {
            MFX_LTRACE_GUID(MF_TL_GENERAL, pArray[i].subtype);
            if ((pArray[i].major_type == major_type) && (pArray[i].subtype == subtype))
            {
                hr = S_OK;
                if (pEntry) *pEntry = &pArray[i];
                break;
            }
        }
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

void MFPlugin::ReleaseMediaType(IMFMediaType*& pType)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    if (pType)
    {
        HRESULT hr = pType->DeleteItem(MF_MT_D3D_DEVICE);
        hr = hr;
    }
    SAFE_RELEASE(pType);
}

/*------------------------------------------------------------------------------*/

bool MFPlugin::IsVppNeeded(IMFMediaType* pType1, IMFMediaType* pType2)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    UINT32 parA1 = 0, parB1 = 0;
    UINT32 parA2 = 0, parB2 = 0;
    MFVideoArea area1 = {}, area2 = {};
    HRESULT hr1 = S_OK, hr2 = S_OK;

    CHECK_POINTER(pType1, false);
    CHECK_POINTER(pType2, false);
    CHECK_EXPRESSION(pType1 != pType2, false);

    //TODO: probably this is a bug: e.g. pType2 does not have attribute and pType1 has one.
    //This case will be wrongly treated as no difference.
    if (SUCCEEDED(MFGetAttributeSize(pType2, MF_MT_FRAME_SIZE, &parA2, &parB2)) &&
        SUCCEEDED(MFGetAttributeSize(pType1, MF_MT_FRAME_SIZE, &parA1, &parB1)))
    {
        MFX_LTRACE_2(MF_TL_GENERAL, NULL, "width1 = %d, height1 = %d", parA1, parB1);
        MFX_LTRACE_2(MF_TL_GENERAL, NULL, "width2 = %d, height2 = %d", parA2, parB2);
        CHECK_EXPRESSION((parA1 == parA2) && (parB1 == parB2), true);
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
    //TODO: probably this is a bug: e.g. pType2 does not have attribute and pType1 has one.
    //This case will be wrongly treated as no difference.
    if (SUCCEEDED(MFGetAttributeRatio(pType2, MF_MT_PIXEL_ASPECT_RATIO, &parA2, &parB2)) &&
        SUCCEEDED(MFGetAttributeRatio(pType1, MF_MT_PIXEL_ASPECT_RATIO, &parA1, &parB1)))
    {
        MFX_LTRACE_2(MF_TL_GENERAL, NULL, "aratio1 = %d:%d", parA1, parB1);
        MFX_LTRACE_2(MF_TL_GENERAL, NULL, "aratio2 = %d:%d", parA2, parB2);
        CHECK_EXPRESSION((parA1 == parA2) && (parB1 == parB2), true);
    }
    //TODO: probably this is a bug: e.g. pType2 does not have attribute and pType1 has one.
    //This case will be wrongly treated as no difference.
    if (SUCCEEDED(MFGetAttributeRatio(pType2, MF_MT_FRAME_RATE, &parA2, &parB2)) &&
        SUCCEEDED(MFGetAttributeRatio(pType1, MF_MT_FRAME_RATE, &parA1, &parB1)))
    {
        MFX_LTRACE_2(MF_TL_GENERAL, NULL, "framerate1 = %d/%d", parA1, parB1);
        MFX_LTRACE_2(MF_TL_GENERAL, NULL, "framerate2 = %d/%d", parA2, parB2);
        CHECK_EXPRESSION((parA1 == parA2) && (parB1 == parB2), true);
    }
    //TODO: probably this is a bug: e.g. pType2 does not have attribute and pType1 has one.
    //This case will be wrongly treated as no difference.
    if (SUCCEEDED(pType2->GetUINT32(MF_MT_INTERLACE_MODE, &parA2)) &&
        SUCCEEDED(pType1->GetUINT32(MF_MT_INTERLACE_MODE, &parA1)))
    {
        MFX_LTRACE_1(MF_TL_GENERAL, NULL, "imode1 = %d", parA1);
        MFX_LTRACE_1(MF_TL_GENERAL, NULL, "imode2 = %d", parA2);
        CHECK_EXPRESSION((parA1 == parA2), true);
    }

    hr1 = pType1->GetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, &parA1);
    hr2 = pType2->GetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, &parA2);
    if (SUCCEEDED(hr1) || SUCCEEDED(hr2))
    { // this parameter may absent on the given type
        MFX_LTRACE_1(MF_TL_GENERAL, NULL, "NominalRange: %d", parA1);
        MFX_LTRACE_1(MF_TL_GENERAL, NULL, "NominalRange: %d", parA2);
        CHECK_EXPRESSION((parA1 == parA2), true);
    }

    //TODO: currently profile changes are ignored if no other changes.
    //This is a bug and needs to be fixed. See also IsInvalidResolutionChange
    hr1 = pType1->GetUINT32(MF_MT_MPEG2_PROFILE, &parA1);
    hr2 = pType2->GetUINT32(MF_MT_MPEG2_PROFILE, &parA2);
    if (SUCCEEDED(hr1) && SUCCEEDED(hr2))
    { // this parameter may absent on the given type
        MFX_LTRACE_1(MF_TL_GENERAL, NULL, "Profile: %d", parA1);
        MFX_LTRACE_1(MF_TL_GENERAL, NULL, "Profile: %d", parA2);
        ATLASSERT(parA1 == parA2);
        //CHECK_EXPRESSION((parA1 == parA2), true);
    }

    //TODO: check for bitrate?

    MFX_LTRACE_S(MF_TL_GENERAL, "false");
    return false;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::RequestInput(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    IMFMediaEvent* pEvent = NULL;

    hr = MFCreateMediaEvent(METransformNeedInput, GUID_NULL,
                            S_OK, NULL, &pEvent);
    if (SUCCEEDED(hr)) hr = pEvent->SetUINT32(MF_EVENT_MFT_INPUT_STREAM_ID, 0);
    if (SUCCEEDED(hr)) hr = m_pEventQueue->QueueEvent(pEvent);
    if (SUCCEEDED(hr)) ++(m_NeedInputEventInfo.m_requested);
    SAFE_RELEASE(pEvent);
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::SendOutput(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    IMFMediaEvent* pEvent = NULL;

    hr = MFCreateMediaEvent(METransformHaveOutput, GUID_NULL,
                            S_OK, NULL, &pEvent);
    if (SUCCEEDED(hr)) hr = m_pEventQueue->QueueEvent(pEvent);
    if (SUCCEEDED(hr)) ++(m_HasOutputEventInfo.m_requested);
    SAFE_RELEASE(pEvent);
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::DrainComplete(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    IMFMediaEvent* pEvent = NULL;

    hr = MFCreateMediaEvent(METransformDrainComplete, GUID_NULL,
                            S_OK, NULL, &pEvent);
    if (SUCCEEDED(hr)) hr = pEvent->SetUINT32(MF_EVENT_MFT_INPUT_STREAM_ID, 0);
    if (SUCCEEDED(hr)) hr = m_pEventQueue->QueueEvent(pEvent);
    SAFE_RELEASE(pEvent);
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::ReportError(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    IMFMediaEvent* pEvent = NULL;

    hr = MFCreateMediaEvent(MEError, GUID_NULL,
                            S_OK, NULL, &pEvent);
    if (SUCCEEDED(hr)) hr = pEvent->SetUINT32(MF_EVENT_MFT_INPUT_STREAM_ID, 0);
    if (SUCCEEDED(hr)) hr = m_pEventQueue->QueueEvent(pEvent);
    SAFE_RELEASE(pEvent);
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::SendMarker(UINT64 ulParam)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    IMFMediaEvent* pEvent = NULL;

    hr = MFCreateMediaEvent(METransformMarker, GUID_NULL,
                            S_OK, NULL, &pEvent);
    if (SUCCEEDED(hr)) hr = pEvent->SetUINT64(MF_EVENT_MFT_CONTEXT, ulParam);
    if (SUCCEEDED(hr)) hr = m_pEventQueue->QueueEvent(pEvent);
    SAFE_RELEASE(pEvent);
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

unsigned int MY_THREAD_CALLCONVENTION thAsyncThreadFunc(void* arg)
{
    MFPlugin* plg = (MFPlugin*)arg;

    if (plg && SUCCEEDED(plg->AsyncThreadFunc())) return 0;
    return 1;
}

/*------------------------------------------------------------------------------*/

void MFPlugin::AsyncThreadPush(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    m_pAsyncThreadSemaphore->Post();
}

/*------------------------------------------------------------------------------*/

void MFPlugin::AsyncThreadWait(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    if (m_pAsyncThread && m_pAsyncThreadSemaphore)
    {
        m_bAsyncThreadStop = true;
        m_pAsyncThreadSemaphore->Post();
        m_pAsyncThreadEvent->Signal();
        m_pAsyncThread->Wait();
        m_bAsyncThreadStop = false;
    }
}

/*------------------------------------------------------------------------------*/
// IMFTransform

HRESULT MFPlugin::GetAttributes(IMFAttributes** pAttributes)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    HRESULT hr = S_OK;

    // checking errors
    CHECK_POINTER(pAttributes, E_POINTER);
    // setting attributes required for ASync plug-ins
    if (SUCCEEDED(hr))
        hr = m_pAttributes->SetUINT32(MF_TRANSFORM_ASYNC, TRUE);
    if (SUCCEEDED(hr))
        hr = m_pAttributes->SetUINT32(MFT_SUPPORT_DYNAMIC_FORMAT_CHANGE, TRUE);
    if (SUCCEEDED(hr) && ((dbgSwMsdk != m_dbg_MSDK) || (dbgHwMsdk == m_dbg_Memory)))
        hr = m_pAttributes->SetString(MFT_ENUM_HARDWARE_URL_Attribute, MFT_ENUM_HARDWARE_URL_STRING);
#if MFX_D3D11_SUPPORT
    if (SUCCEEDED(hr))
        hr = m_pAttributes->SetString(MFT_ENUM_HARDWARE_VENDOR_ID_Attribute, _T("VEN_8086"));
    if (SUCCEEDED(hr))
        hr = m_pAttributes->SetUINT32(MF_SA_D3D11_AWARE, TRUE);
#endif
    if (SUCCEEDED(hr))
    {
        m_pAttributes->AddRef();
        *pAttributes = m_pAttributes;
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::GetStreamLimits(DWORD *pdwInputMinimum,
                                  DWORD *pdwInputMaximum,
                                  DWORD *pdwOutputMinimum,
                                  DWORD *pdwOutputMaximum)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);
    // Fixed stream limits.
    if (pdwInputMinimum) *pdwInputMinimum = 1;
    if (pdwInputMaximum) *pdwInputMaximum = 1;
    if (pdwOutputMinimum) *pdwOutputMinimum = 1;
    if (pdwOutputMaximum) *pdwOutputMaximum = 1;
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::GetStreamCount(DWORD *pcInputStreams,
                                 DWORD *pcOutputStreams)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    //no locking check is required in this function - exception pointed in MSDN

    // Fixed stream count.
    if (pcInputStreams) *pcInputStreams = 1;
    if (pcOutputStreams) *pcOutputStreams = 1;
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::GetStreamIDs(DWORD  /*dwInputIDArraySize*/,
                               DWORD* /*pdwInputIDs*/,
                               DWORD  /*dwOutputIDArraySize*/,
                               DWORD* /*pdwOutputIDs*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);
    // Do not need to implement, because MFT has a fixed number of
    // streams and the stream IDs match the stream indexes.
    MFX_LTRACE_S(MF_TL_GENERAL, "E_NOTIMPL");
    return E_NOTIMPL;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::GetInputStreamInfo(DWORD dwInputStreamID,
                                     MFT_INPUT_STREAM_INFO* pStreamInfo)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);
    CHECK_EXPRESSION(0 == dwInputStreamID, MF_E_INVALIDSTREAMNUMBER);

    memset(pStreamInfo, 0, sizeof(MFT_INPUT_STREAM_INFO));

    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::GetOutputStreamInfo(DWORD dwOutputStreamID,
                                      MFT_OUTPUT_STREAM_INFO* pStreamInfo)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);
    CHECK_EXPRESSION(0 == dwOutputStreamID, MF_E_INVALIDSTREAMNUMBER);

    memset(pStreamInfo, 0, sizeof(MFT_OUTPUT_STREAM_INFO));
    pStreamInfo->dwFlags = MFT_OUTPUT_STREAM_PROVIDES_SAMPLES;

    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::GetInputStreamAttributes(DWORD           /*dwInputStreamID*/,
                                           IMFAttributes** ppAttributes)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    HRESULT hr = S_OK;

    CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);

    // Fixed stream count = 1
    hr = GetAttributes(ppAttributes);

    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::GetOutputStreamAttributes(DWORD           /*dwOutputStreamID*/,
                                            IMFAttributes** ppAttributes)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    HRESULT hr = S_OK;

    CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);

    // Fixed stream count = 1
    hr = GetAttributes(ppAttributes);

    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::DeleteInputStream(DWORD /*dwStreamID*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);
    MFX_LTRACE_S(MF_TL_GENERAL, "E_NOTIMPL");
    return E_NOTIMPL; // Fixed streams.
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::AddInputStreams(DWORD  /*cStreams*/,
                                  DWORD* /*adwStreamIDs*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);
    MFX_LTRACE_S(MF_TL_GENERAL, "E_NOTIMPL");
    return E_NOTIMPL; // Fixed streams.
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::GetInputAvailableType(DWORD dwInputStreamID,
                                        DWORD dwTypeIndex, // 0-based
                                        IMFMediaType** ppType)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    HRESULT hr = S_OK;

    CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);
    CHECK_EXPRESSION(0 == dwInputStreamID, MF_E_INVALIDSTREAMNUMBER);
    CHECK_EXPRESSION(dwTypeIndex < m_Reg.cInputTypes, MF_E_NO_MORE_TYPES);
    CHECK_POINTER(ppType, E_POINTER);

    hr = MFCreateMediaType(ppType);

    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::GetOutputAvailableType(DWORD dwOutputStreamID,
                                         DWORD dwTypeIndex, // 0-based
                                         IMFMediaType** ppType)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    HRESULT hr = S_OK;

    CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);
    CHECK_EXPRESSION(0 == dwOutputStreamID, MF_E_INVALIDSTREAMNUMBER);
    CHECK_EXPRESSION(dwTypeIndex < m_Reg.cOutputTypes, MF_E_NO_MORE_TYPES);
    CHECK_POINTER(ppType, E_POINTER);

    hr = MFCreateMediaType(ppType);

    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::SetInputType(DWORD         dwInputStreamID,
                               IMFMediaType* /*pType*/,
                               DWORD         /*dwFlags*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);
    CHECK_EXPRESSION(0 == dwInputStreamID, MF_E_INVALIDSTREAMNUMBER);
    ATLASSERT(0 == dwInputStreamID);
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::SetOutputType(DWORD         dwOutputStreamID,
                                IMFMediaType* /*pType*/,
                                DWORD         /*dwFlags*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    if ((REG_AS_VIDEO_ENCODER != m_Reg.iPluginCategory) &&
        (REG_AS_AUDIO_ENCODER != m_Reg.iPluginCategory))
    {
        CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);
    }
    CHECK_EXPRESSION(0 == dwOutputStreamID, MF_E_INVALIDSTREAMNUMBER);
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::GetInputCurrentType(DWORD dwInputStreamID,
                                      IMFMediaType** ppType)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);
    CHECK_EXPRESSION(dwInputStreamID == 0, MF_E_INVALIDSTREAMNUMBER);
    CHECK_POINTER(ppType, E_POINTER);
    CHECK_POINTER(m_pInputType, MF_E_TRANSFORM_TYPE_NOT_SET);

    m_pInputType->AddRef();
    *ppType = m_pInputType;

    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::GetOutputCurrentType(DWORD dwOutputStreamID,
                                       IMFMediaType** ppType)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    if ((REG_AS_VIDEO_ENCODER != m_Reg.iPluginCategory) &&
        (REG_AS_AUDIO_ENCODER != m_Reg.iPluginCategory))
    {
        CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);
    }
    CHECK_EXPRESSION(dwOutputStreamID == 0, MF_E_INVALIDSTREAMNUMBER);
    CHECK_POINTER(ppType, E_POINTER);
    CHECK_POINTER(m_pOutputType, MF_E_TRANSFORM_TYPE_NOT_SET);

    m_pOutputType->AddRef();
    *ppType = m_pOutputType;

    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::GetInputStatus(DWORD  dwInputStreamID,
                                 DWORD* pdwFlags)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);
    CHECK_EXPRESSION(0 == dwInputStreamID, MF_E_INVALIDSTREAMNUMBER);
    CHECK_POINTER(pdwFlags, E_POINTER);

    *pdwFlags = MFT_INPUT_STATUS_ACCEPT_DATA;
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::GetOutputStatus(DWORD *pdwFlags)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);
    CHECK_POINTER(pdwFlags, E_POINTER);
    *pdwFlags = MFT_OUTPUT_STATUS_SAMPLE_READY;
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::SetOutputBounds(LONGLONG /*hnsLowerBound*/,
                                  LONGLONG /*hnsUpperBound*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);
    MFX_LTRACE_S(MF_TL_GENERAL, "E_NOTIMPL");
    return E_NOTIMPL;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::ProcessEvent(DWORD /*dwInputStreamID*/,
                               IMFMediaEvent* /*pEvent*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);
    MFX_LTRACE_S(MF_TL_GENERAL, "E_NOTIMPL");
    return E_NOTIMPL;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::ProcessMessage(MFT_MESSAGE_TYPE eMessage,
                                 ULONG_PTR        ulParam)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);
    switch (eMessage)
    {
    case MFT_MESSAGE_COMMAND_DRAIN:
        MFX_LTRACE_S(MF_TL_GENERAL, "MFT_MESSAGE_COMMAND_DRAIN");
        CHECK_EXPRESSION(0 == (DWORD)ulParam, MF_E_INVALIDSTREAMNUMBER);
        break;
    case MFT_MESSAGE_NOTIFY_END_OF_STREAM:
        MFX_LTRACE_S(MF_TL_GENERAL, "MFT_MESSAGE_NOTIFY_END_OF_STREAM");
        CHECK_EXPRESSION(0 == (DWORD)ulParam, MF_E_INVALIDSTREAMNUMBER);
        break;
    };
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::ProcessInput(DWORD      dwInputStreamID,
                               IMFSample* pSample,
                               DWORD      /*dwFlags*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    UNREFERENCED_PARAMETER(pSample);
    MyAutoLock lock(m_CritSec);
    HRESULT hr_sts = S_OK;

    // Checking request acceptance (MFT locking)
    CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);

    { // Checking request acceptance (event counts)
        // We can not accept samples which do not match pending events
        CHECK_EXPRESSION(m_NeedInputEventInfo.m_sent, MF_E_NOTACCEPTING);

        if (m_NeedInputEventInfo.m_sent) --(m_NeedInputEventInfo.m_sent);
        if (m_NeedInputEventInfo.m_requested) --(m_NeedInputEventInfo.m_requested);

        MFX_LTRACE_I(MF_TL_PERF, m_NeedInputEventInfo.m_sent);
        MFX_LTRACE_I(MF_TL_PERF, m_NeedInputEventInfo.m_requested);
    }

    // Checking errors
    if (FAILED(m_hrError)) hr_sts = RequestInput();
    ATLASSERT(SUCCEEDED(hr_sts));

    // Client must set input and output types.
    SET_HR_ERROR(m_pInputType, hr_sts, MF_E_TRANSFORM_TYPE_NOT_SET);
    ATLASSERT(SUCCEEDED(hr_sts));
    SET_HR_ERROR(m_pOutputType, hr_sts, MF_E_TRANSFORM_TYPE_NOT_SET);
    ATLASSERT(SUCCEEDED(hr_sts));
    // Only 1 stream
    SET_HR_ERROR((0 == dwInputStreamID), hr_sts, MF_E_INVALIDSTREAMNUMBER);
    ATLASSERT(SUCCEEDED(hr_sts));
    // Input sample should exist
#if !MFX_D3D11_SUPPORT
    SET_HR_ERROR(pSample, hr_sts, E_POINTER);
#endif// MFX_D3D11_SUPPORT
    ATLASSERT(SUCCEEDED(hr_sts));

    SetPlgError(hr_sts);

    MFX_LTRACE_D(MF_TL_PERF, hr_sts);
    MFX_LTRACE_S(MF_TL_PERF, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::ProcessOutput(DWORD  /*dwFlags*/,
                                DWORD  cOutputBufferCount,
                                MFT_OUTPUT_DATA_BUFFER* pOutputSamples,
                                DWORD* pdwStatus)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);
    HRESULT hr_sts = S_OK;

    // Checking request acceptance (MFT locking)
    CHECK_EXPRESSION(IsMftUnlocked(), MF_E_TRANSFORM_ASYNC_LOCKED);

    { // Checking request acceptance (event counts)
        if (!m_HasOutputEventInfo.m_sent)
        {
            if (pOutputSamples && (cOutputBufferCount > 0))
            {
                pOutputSamples[0].dwStatus = MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE;
            }
#if MFX_D3D11_SUPPORT //See Revision: 41632 Microsoft compliance test WA, support for interlaced/progressive mixed mode
            return E_UNEXPECTED;
#else
            return MF_E_UNEXPECTED;
#endif
        }
        // We can not accept requests for samples which do not match pending events
        if (m_HasOutputEventInfo.m_sent) --(m_HasOutputEventInfo.m_sent);
        if (m_HasOutputEventInfo.m_requested) --(m_HasOutputEventInfo.m_requested);

        MFX_LTRACE_I(MF_TL_PERF, m_HasOutputEventInfo.m_sent);
        MFX_LTRACE_I(MF_TL_PERF, m_HasOutputEventInfo.m_requested);
    }

    // Checking errors
    // Client must set input and output types.
    SET_HR_ERROR(m_pInputType, hr_sts, MF_E_TRANSFORM_TYPE_NOT_SET);
    SET_HR_ERROR(m_pOutputType, hr_sts, MF_E_TRANSFORM_TYPE_NOT_SET);
    // Must be exactly one output buffer.
    SET_HR_ERROR(1 == cOutputBufferCount, hr_sts, E_INVALIDARG);
    SET_HR_ERROR(pOutputSamples, hr_sts, E_POINTER);
     // MFT provides samples
    SET_HR_ERROR(!(pOutputSamples[0].pSample), hr_sts, E_FAIL);
    SET_HR_ERROR(pdwStatus, hr_sts, E_POINTER);

    SetPlgError(hr_sts);

    MFX_LTRACE_D(MF_TL_PERF, hr_sts);
    MFX_LTRACE_S(MF_TL_PERF, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/
// IMFMediaEventGenerator

HRESULT MFPlugin::IncrementEventsCount(IMFMediaEvent* pEvent)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    HRESULT        hr = S_OK;
    MediaEventType met;

    hr = pEvent->GetType(&met);

    if (SUCCEEDED(hr))
    {
        if (METransformNeedInput == met)
        {
            if (m_NeedInputEventInfo.m_requested) ++(m_NeedInputEventInfo.m_sent);
            MFX_LTRACE_I(MF_TL_GENERAL, m_NeedInputEventInfo.m_requested);
            MFX_LTRACE_I(MF_TL_GENERAL, m_NeedInputEventInfo.m_sent);
        }
        else if (METransformHaveOutput == met)
        {
            if (m_HasOutputEventInfo.m_requested) ++(m_HasOutputEventInfo.m_sent);
            MFX_LTRACE_I(MF_TL_GENERAL, m_HasOutputEventInfo.m_requested);
            MFX_LTRACE_I(MF_TL_GENERAL, m_HasOutputEventInfo.m_sent);
        }
    }

    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::BeginGetEvent(IMFAsyncCallback* pCallback,
                                IUnknown* pUnkState)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = S_OK;

    CHECK_EXPRESSION(!m_bIsShutdown, MF_E_SHUTDOWN);
    hr = m_pEventQueue->BeginGetEvent(pCallback, pUnkState);

    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::EndGetEvent(IMFAsyncResult* pResult,
                              IMFMediaEvent** ppEvent)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = S_OK;

    CHECK_EXPRESSION(!m_bIsShutdown, MF_E_SHUTDOWN);

    hr = m_pEventQueue->EndGetEvent(pResult, ppEvent);
    if (SUCCEEDED(hr)) hr = IncrementEventsCount(*ppEvent);

    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::GetEvent(DWORD dwFlags,
                           IMFMediaEvent** ppEvent)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = S_OK;

    CHECK_EXPRESSION(!m_bIsShutdown, MF_E_SHUTDOWN);

    hr = m_pEventQueue->GetEvent(dwFlags, ppEvent);
    if (SUCCEEDED(hr)) hr = IncrementEventsCount(*ppEvent);

    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::QueueEvent(MediaEventType met,
                             REFGUID guidExtendedType,
                             HRESULT hrStatus,
                             const PROPVARIANT* pvValue)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = S_OK;

    CHECK_EXPRESSION(!m_bIsShutdown, MF_E_SHUTDOWN);
    hr = m_pEventQueue->QueueEventParamVar(met, guidExtendedType,
                                           hrStatus, pvValue);
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/
// IMFShutdown

HRESULT MFPlugin::Shutdown(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = S_OK;

    if (!m_bIsShutdown)
    {
        hr = m_pEventQueue->Shutdown();
        m_bIsShutdown = true;
    }
    ReleaseMediaType(m_pInputType);
    ReleaseMediaType(m_pOutputType);

    MFX_LTRACE_D(MF_TL_NOTES, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFPlugin::GetShutdownStatus(MFSHUTDOWN_STATUS* pStatus)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    MyAutoLock lock(m_CritSec);
    HRESULT hr = S_OK;

    CHECK_POINTER(pStatus, E_POINTER);
    if (m_bIsShutdown) *pStatus = MFSHUTDOWN_COMPLETED;
    else hr = MF_E_INVALIDREQUEST; // shutdown should be called firstly
    MFX_LTRACE_D(MF_TL_NOTES, hr);
    return hr;
}

UINT MFPlugin::GetAdapterNum(mfxIMPL* pImpl)
{
    UINT nAdapter = 0;

    switch (*pImpl & 0xF)
    {
        case MFX_IMPL_HARDWARE:  nAdapter = 0; break;
        case MFX_IMPL_HARDWARE2: nAdapter = 1; break;
        case MFX_IMPL_HARDWARE3: nAdapter = 2; break;
        case MFX_IMPL_HARDWARE4: nAdapter = 3; break;
        default: nAdapter = 0;
    }

    return nAdapter;
};
