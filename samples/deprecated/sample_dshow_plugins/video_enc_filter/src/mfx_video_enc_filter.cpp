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

#include "mfx_video_enc_filter.h"
#include "memory_allocator.h"

#include <list>
#include <algorithm>

CEncVideoFilter::CEncVideoFilter(TCHAR *tszName,LPUNKNOWN punk, const GUID VideoCodecGUID, HRESULT *phr,
                                 mfxU16 APIVerMinor, mfxU16 APIVerMajor) :
CTransformFilter(tszName, punk, VideoCodecGUID),
m_bStop(FALSE),
m_pAdapter(NULL),
m_Thread(0),
m_bUseVPP(FALSE),
m_FramesReceived(0)
{
    m_pTimeManager = new CTimeManager;

    mfxStatus sts = MFX_ERR_NONE;

    // save version to be able to re-create BaseEncoder when needed
    m_nAPIVerMinor = APIVerMinor;
    m_nAPIVerMajor = APIVerMajor;

    m_pEncoder = new CBaseEncoder(APIVerMinor, APIVerMajor, &sts);
    if (phr)
        *phr = (MFX_ERR_NONE == sts) ? S_OK : E_FAIL;


    memset(&m_mfxParamsVideo, 0, sizeof(mfxVideoParam));
    memset(&m_mfxParamsVPP,   0, sizeof(mfxVideoParam));

    DWORD nTemp = 0;
    SetParamToReg(_T("RealBitrate"),   nTemp);
    SetParamToReg(_T("FramesEncoded"), nTemp);
}

CEncVideoFilter::~CEncVideoFilter(void)
{
    MSDK_SAFE_DELETE(m_pTimeManager);
    MSDK_SAFE_DELETE(m_pEncoder);
}

HRESULT CEncVideoFilter::DeliverBitstream(mfxBitstream* pBS)
{
    mfxStatus               sts = MFX_ERR_NONE;
    HRESULT                 hr  = S_OK;
    CComPtr<IMediaSample>   pOutSample;
    REFERENCE_TIME          rtStart(0), rtEnd(0);
    mfxU8*                  pBuffer = NULL;

    MSDK_CHECK_POINTER(pBS,       E_POINTER);
    MSDK_CHECK_POINTER(pBS->Data, E_POINTER);

    if (0 == pBS->DataLength)
    {
        //empty bitstream
        return S_OK;
    }

    // get delivery buffer
    hr = m_pOutput->GetDeliveryBuffer(&pOutSample, NULL, NULL, 0);
    CHECK_RESULT_P_RET(hr, S_OK);
    MSDK_CHECK_POINTER(pOutSample, E_POINTER);

    if (m_pAdapter)
    {
        hr = m_pAdapter->Adapt(*pBS);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, (int)E_UNEXPECTED);

        pOutSample->SetSyncPoint(m_pAdapter->IsKeyFrame(*pBS));
        CHECK_RESULT_P_RET(hr, S_OK);
    }

    pOutSample->GetPointer(&pBuffer);
    CHECK_RESULT_P_RET(hr, S_OK);

    //check that buffer size is enough
    if ((mfxU32)pOutSample->GetSize() < pBS->DataLength)
    {
        return E_UNEXPECTED;
    }

    MSDK_MEMCPY_BUF(pBuffer,0,pOutSample->GetSize(),pBS->Data,pBS->DataLength);

    pOutSample->SetActualDataLength(pBS->DataLength);
    CHECK_RESULT_P_RET(hr, S_OK);

    rtStart = ConvertMFXTime2ReferenceTime(pBS->TimeStamp);

    if (rtStart != -1e7)
    {
        rtEnd = (REFERENCE_TIME)(rtStart + (1.0 / m_pTimeManager->GetFrameRate()) * 1e7);
    }
    else
    {
        // if time stamp is invalid we calculate it basing on frame rate
        hr = m_pTimeManager->GetTime(&rtStart, &rtEnd);
        CHECK_RESULT_P_RET(hr, S_OK);
    }

    hr = pOutSample->SetTime(&rtStart, &rtEnd);
    CHECK_RESULT_P_RET(hr, S_OK);

    hr = m_pOutput->Deliver(pOutSample);
    CHECK_RESULT_P_RET(hr, S_OK);

    pBS->DataLength = 0;

    return S_OK;
};

HRESULT CEncVideoFilter::CompleteConnect(PIN_DIRECTION direction, IPin *pReceivePin)
{
    HRESULT   hr  = S_OK;

    if (PINDIR_OUTPUT == direction && m_pInput->IsConnected())
    {
        CComQIPtr<IMemAllocator> alloc;
        MFXFrameAllocator *pMfxAlloc = NULL;

        hr = m_pInput->GetAllocator(&alloc.p);
        MSDK_CHECK_RESULT(hr, S_OK, (int)E_FAIL);

        CComQIPtr<IMFXAllocator> mfxalloc(alloc);
        if (NULL != mfxalloc)
        {
            hr = mfxalloc->GetMfxFrameAllocator(&pMfxAlloc);
            MSDK_CHECK_RESULT(hr, S_OK, (int)E_FAIL);

            MSDK_CHECK_RESULT(m_pEncoder->SetExtFrameAllocator(pMfxAlloc), MFX_ERR_NONE, (int)E_FAIL);
        }
    }

    hr = CTransformFilter::CompleteConnect(direction, pReceivePin);
    MSDK_CHECK_RESULT(hr, S_OK, (int)E_FAIL);

    // reconnect the output pin if it is connected during input pin connection final stage
    if (PINDIR_INPUT == direction && m_pOutput && m_pOutput->IsConnected())
    {
        CComPtr<IFilterGraph>   pGraph;

        hr = m_pGraph->QueryInterface(IID_IFilterGraph, (void**)&pGraph);
        CHECK_RESULT_FAIL(hr);

        hr = pGraph->Reconnect(m_pOutput);
    }

    return hr;
}

HRESULT CEncVideoFilter::Receive(IMediaSample* pSample)
{
    HRESULT hr = S_OK;
    mfxStatus sts = MFX_ERR_NONE;
    mfxU8* pBuffer = NULL;
    REFERENCE_TIME rtStart(0), rtEnd(0);

    CComPtr<IMediaEventSink> pSink;
    hr = m_pGraph->QueryInterface(IID_IMediaEventSink, (void**)&pSink);
    if (FAILED(hr))
        pSink.p = 0;

    CAutoLock cObjectLock(&m_csLock);

    // check pointer
    MSDK_CHECK_POINTER(pSample, S_OK);

    // check if the process has stopped
    MSDK_CHECK_ERROR(m_bStop, TRUE, (int)E_FAIL);

    mfxIMPL impl;
    m_pEncoder->QueryIMPL(&impl);

    // prepare surface pointer to pass to RunEncode
    mfxFrameSurface1* pmfxSurface = NULL;

    CComPtr<IMFXSample> pMFXSample(NULL);
    hr = pSample->QueryInterface(IID_IMFXSample, reinterpret_cast<void**>(&pMFXSample));

    if (SUCCEEDED(hr))
    {
        hr = pMFXSample->GetMfxSurfacePointer(&pmfxSurface);
        CHECK_RESULT_FAIL(hr);
        m_pEncoder->m_bmfxSample = true;
    }

    if (!m_pEncoder->m_bmfxSample)
    {
        // MediaSample contains YUV data in system memory buffer,
        // so we need to allocate mfx surface structure, initialize fields and set plane pointers
        pmfxSurface = new mfxFrameSurface1;
        MSDK_CHECK_POINTER(pmfxSurface, E_OUTOFMEMORY);
        m_pEncoder->m_bSurfaceStored = false; // mark that Receive function is responsible for freeing memory

        memset(pmfxSurface, 0, sizeof(mfxFrameSurface1));

        // get timestamps set by upstream filter
        if (SUCCEEDED(pSample->GetTime(&rtStart, &rtEnd)))
        {
            if (-1e7 != rtStart)
            {
                rtStart = (rtStart < 0) ? 0 : rtStart;
            }
        }
        else
        {
            rtStart = (REFERENCE_TIME) -1e7;
        }

        pmfxSurface->Data.TimeStamp = ConvertReferenceTime2MFXTime(rtStart);

        m_bUseVPP = memcmp(&m_mfxParamsVPP.vpp.In, &m_mfxParamsVPP.vpp.Out, sizeof(mfxFrameInfo)) ? true : false;

        // init surface according to selected params
        if (m_bUseVPP)
        {
            sts = InitMfxFrameSurface(pmfxSurface, &m_mfxParamsVPP.vpp.In, pSample);
        }
        else
        {
            sts = InitMfxFrameSurface(pmfxSurface, &m_mfxParamsVideo.mfx.FrameInfo, pSample);
        }

        if (MFX_ERR_NONE != sts && pSink)
            pSink->Notify(EC_ERRORABORT, 0, 0);

        MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, (int)E_FAIL, MSDK_SAFE_DELETE(pmfxSurface));

        hr = pSample->GetPointer(&pBuffer);
        MSDK_CHECK_RESULT_SAFE(hr, S_OK, (int)E_FAIL, MSDK_SAFE_DELETE(pmfxSurface));

        // find out the frame height at connection
        // to correctly handle the case when we are connected with H and cropH != H
        GUID guidFormat = *m_pInput->CurrentMediaType().FormatType();
        BITMAPINFOHEADER bmiHeader;
        VIDEOINFOHEADER2 *pVIH2(NULL);
        VIDEOINFOHEADER *pVIH(NULL);

        bmiHeader.biHeight = 0;

        if (FORMAT_VideoInfo2 == guidFormat)
        {
            pVIH2 = reinterpret_cast<VIDEOINFOHEADER2* >(m_pInput->CurrentMediaType().Format());
            MSDK_CHECK_POINTER_SAFE(pVIH2, E_UNEXPECTED, MSDK_SAFE_DELETE(pmfxSurface));
            bmiHeader = pVIH2->bmiHeader;
        }
        else if (FORMAT_VideoInfo == guidFormat)
        {
            pVIH = reinterpret_cast<VIDEOINFOHEADER* >(m_pInput->CurrentMediaType().Format());
            MSDK_CHECK_POINTER_SAFE(pVIH, E_UNEXPECTED, MSDK_SAFE_DELETE(pmfxSurface));
            bmiHeader = pVIH->bmiHeader;
        }

        LONG lConnectionHeight = abs(bmiHeader.biHeight);

        // set mfx surface plane pointers
        switch (pmfxSurface->Info.FourCC)
        {
        case MFX_FOURCC_NV12:
            pmfxSurface->Data.Y   = pBuffer;
            pmfxSurface->Data.UV  = pBuffer + pmfxSurface->Data.Pitch * lConnectionHeight;
            break;
        case MFX_FOURCC_YUY2:
            pmfxSurface->Data.Y = pBuffer;
            pmfxSurface->Data.U = pmfxSurface->Data.Y + 1;
            pmfxSurface->Data.V = pmfxSurface->Data.Y + 3;
            break;
        case MFX_FOURCC_RGB4:
            pmfxSurface->Data.R = pBuffer;
            pmfxSurface->Data.G = pBuffer + 4;
            pmfxSurface->Data.B = pBuffer + 8;
            break;
        default:
            MSDK_SAFE_DELETE(pmfxSurface);
            return E_UNEXPECTED;
        }
    }

    // initialize mfx encoder
    if (!m_pEncoder->m_bInitialized || EncResetRequired(&pmfxSurface->Info))
    {
        sts = m_pEncoder->Init(&m_mfxParamsVideo, &m_mfxParamsVPP, this);
        WriteMfxImplToRegistry();
    }

    if (MFX_ERR_NONE == sts)
    {
        ++m_FramesReceived;
        sts = m_pEncoder->RunEncode(pSample, pmfxSurface);
    }

    if (MFX_ERR_NONE != sts && MFX_ERR_MORE_DATA != sts)
    {
        if (pSink)
            pSink->Notify(EC_ERRORABORT, 0, 0);

        hr = E_FAIL;
    }

    // check if need to free memory allocated for mfxSurface structure
    if (!m_pEncoder->m_bmfxSample && !m_pEncoder->m_bSurfaceStored)
    {
        MSDK_SAFE_DELETE(pmfxSurface);
    }

    return hr;
}

mfxStatus CEncVideoFilter::OnFrameReady(mfxBitstream* pBS)
{
    //need to add error handling
    DeliverBitstream(pBS);

    return MFX_ERR_NONE;
}

bool CEncVideoFilter::EncResetRequired(const mfxFrameInfo *pNewFrameInfo)
{
    mfxFrameInfo *pOldFrameInfo = &m_mfxParamsVideo.mfx.FrameInfo;

    if (m_bUseVPP)
    {
        pNewFrameInfo = &m_mfxParamsVPP.vpp.Out;
    }

    if ( (pOldFrameInfo->CropX != pNewFrameInfo->CropX) ||
         (pOldFrameInfo->CropY != pNewFrameInfo->CropY) ||
         (pOldFrameInfo->CropW != pNewFrameInfo->CropW) ||
         (pOldFrameInfo->CropH != pNewFrameInfo->CropH) ||
         (pOldFrameInfo->Width != pNewFrameInfo->Width) ||
         (pOldFrameInfo->Height != pNewFrameInfo->Height) )
    {
        MSDK_MEMCPY_VAR(m_mfxParamsVideo.mfx.FrameInfo, pNewFrameInfo, sizeof(mfxFrameInfo));
        return true;
    }

    return false;
}

void CEncVideoFilter::WriteMfxImplToRegistry()
{
    mfxIMPL impl;
    m_pEncoder->QueryIMPL(&impl);
    bool bIsHWLib = (MFX_IMPL_HARDWARE & impl) ? true : false;
    SetParamToReg(_T("IsHWMfxLib"), bIsHWLib);
    SetParamToReg(_T("IsEncodePartiallyAccelerated"), m_pEncoder->m_bEncPartialAcceleration);
}

HRESULT CEncVideoFilter::StartStreaming(void)
{
    mfxStatus sts = MFX_ERR_NONE;
    CAutoLock cObjectLock(&m_csLock);

    //let's start
    m_bStop = FALSE;

    if (m_pTimeManager)
    {
        m_pTimeManager->Reset();
    }

    m_FramesReceived = 0;

    sts = m_pEncoder->Init(&m_mfxParamsVideo, &m_mfxParamsVPP, this);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, (int)E_FAIL);

    WriteMfxImplToRegistry();

    // call method from parent
    return CTransformFilter::StartStreaming();
}

HRESULT CEncVideoFilter::StopStreaming(void)
{
    CAutoLock cObjectLock(&m_csLock);

    //stop working
    m_bStop = TRUE;

    // update registry with encoding statistics before closing
    UpdateRegistry();

    m_pEncoder->Close();

    return CTransformFilter::StopStreaming();
}

HRESULT CEncVideoFilter::EndOfStream(void)
{
    mfxStatus       sts = MFX_ERR_NONE;

    CAutoLock cObjectLock(&m_csLock);

    //encode until all data is taken by encoder
    while (MFX_ERR_NONE == sts && !m_bStop)
    {
       sts = m_pEncoder->RunEncode(NULL, NULL);
    }

    // Collect final statistics
    Statistics stat;
    GetRunTimeStatistics(&stat);

    return CTransformFilter::EndOfStream();
}

HRESULT CEncVideoFilter::DecideBufferSize(IMemAllocator* pAlloc,ALLOCATOR_PROPERTIES* pProperties)
{
    AM_MEDIA_TYPE           mt;
    HRESULT                 hr =S_OK;
    BITMAPINFOHEADER*       pbmi = NULL;
    ALLOCATOR_PROPERTIES    Actual;

    // Is the input pin connected
    hr = m_pOutput->ConnectionMediaType(&mt);
    CHECK_RESULT_P_RET(hr, S_OK);

    if(FORMAT_VideoInfo2 == mt.formattype)
    {
        pbmi = HEADER2(mt.pbFormat);
    }

    // on failure to get BITMAPINFOHEADER
    if (!pbmi)
    {
        FreeMediaType(mt);
        return E_FAIL;
    }

    pProperties->cbBuffer = DIBSIZE(*pbmi);

    if (0 == pProperties->cbAlign)
    {
        pProperties->cbAlign = 1;
    }

    if (0 == pProperties->cBuffers)
    {
        pProperties->cBuffers = 1;
    }

    // get suggested output size
    // will be a good approximation for HW impl case
    // actual Init with frames allocation is deferred
    if (m_pEncoder)
    {
        mfxStatus sts = MFX_ERR_NONE;
        mfxVideoParam par;
        MFXVideoENCODE *pEnc = m_pEncoder->m_pmfxENC;

        memset(&par, 0, sizeof(mfxVideoParam));
        par.mfx.CodecId = m_mfxParamsVideo.mfx.CodecId;
        m_mfxParamsVideo.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

        pEnc->Query(&m_mfxParamsVideo, &par);
        pEnc->Init(&par);
        sts = pEnc->GetVideoParam(&par);
        pEnc->Close();

        m_mfxParamsVideo.mfx.BufferSizeInKB = par.mfx.BufferSizeInKB; // will use this value for further encoder initialization

        //sts = m_pEncoder->InternalReset(&m_mfxParamsVideo, &m_mfxParamsVPP, false);

        if (MFX_ERR_NONE == sts)
        {
            pProperties->cbBuffer = std::max(pProperties->cbBuffer, (long)par.mfx.BufferSizeInKB * 1000);
        }
    }

    hr = pAlloc->SetProperties(pProperties, &Actual);
    if (SUCCEEDED(hr) && ((pProperties->cBuffers > Actual.cBuffers) || (pProperties->cbBuffer > Actual.cbBuffer)))
    {
        hr = E_FAIL;
    }

    // Release the format block.
    FreeMediaType(mt);

    return hr;
}

HRESULT CEncVideoFilter::CheckInputType(const CMediaType* mtIn)
{
    mfxStatus           sts = MFX_ERR_NONE;
    GUID                guidFormat;
    VIDEOINFOHEADER2    pvi;
    mfxU32              nBitrateRecommended(0);

    MSDK_CHECK_POINTER(mtIn, E_POINTER);

    MSDK_CHECK_NOT_EQUAL(*mtIn->Type(), MEDIATYPE_Video, (int)VFW_E_INVALIDMEDIATYPE);

    guidFormat = *mtIn->FormatType();

    memset(&pvi, 0, sizeof(VIDEOINFOHEADER2));

    if (FORMAT_VideoInfo2 == guidFormat)
    {
        MSDK_MEMCPY_VAR(pvi, mtIn->pbFormat, sizeof(VIDEOINFOHEADER2));
    }
    else if (FORMAT_VideoInfo == guidFormat)
    {
        VIDEOINFOHEADER*    pvi2 = reinterpret_cast<VIDEOINFOHEADER*>(mtIn->pbFormat);
        pvi.bmiHeader       = pvi2->bmiHeader;
        pvi.AvgTimePerFrame = pvi2->AvgTimePerFrame;

        pvi.dwPictAspectRatioX = pvi.bmiHeader.biWidth;
        pvi.dwPictAspectRatioY = pvi.bmiHeader.biHeight;
    }

    GUID msub = *mtIn->Subtype();
    if ((MEDIASUBTYPE_NV12 == msub) ||
        (MEDIASUBTYPE_YUY2 == msub) ||
        (MEDIASUBTYPE_RGB32 == msub))
    {
        m_mfxParamsVideo.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    }
    else
    {
        return VFW_E_INVALIDMEDIATYPE;
    }

    // actual frame sizes, check if rcSource is specified
    mfxU16 frameWidth = ((pvi.rcSource.right - pvi.rcSource.left) > 0 ) ?
        (mfxU16)(pvi.rcSource.right - pvi.rcSource.left) : (mfxU16)(pvi.bmiHeader.biWidth);
    mfxU16 frameHeight = ((pvi.rcSource.bottom - pvi.rcSource.top) > 0) ?
        (mfxU16)(pvi.rcSource.bottom - pvi.rcSource.top) : (mfxU16)(pvi.bmiHeader.biHeight);

    m_mfxParamsVideo.mfx.FrameInfo.Width = (frameWidth + 15) &~ 15;
    m_mfxParamsVideo.mfx.FrameInfo.Height = (frameHeight + 31) &~ 31;
    m_mfxParamsVideo.mfx.FrameInfo.CropW = frameWidth;
    m_mfxParamsVideo.mfx.FrameInfo.CropH = frameHeight;

    if (pvi.dwInterlaceFlags & AMINTERLACE_IsInterlaced)
    {
        if (pvi.dwInterlaceFlags & AMINTERLACE_DisplayModeBobOrWeave)
        {
            // if stream has mixed content or picture structure is unknown at connection, encode as progressive
            m_mfxParamsVideo.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        }
        else if (pvi.dwInterlaceFlags & AMINTERLACE_Field1First)
        {
            m_mfxParamsVideo.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
        }
        else
        {
            m_mfxParamsVideo.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_FIELD_BFF;
        }
    }
    else
    {
        m_mfxParamsVideo.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }

    //convert display aspect ratio (is in VIDEOINFOHEADER2) to pixel aspect ratio (is accepted by MediaSDK components)
    sts = DARtoPAR(pvi.dwPictAspectRatioX, pvi.dwPictAspectRatioY,
        m_mfxParamsVideo.mfx.FrameInfo.CropW, m_mfxParamsVideo.mfx.FrameInfo.CropH,
        &m_mfxParamsVideo.mfx.FrameInfo.AspectRatioW, &m_mfxParamsVideo.mfx.FrameInfo.AspectRatioH);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, (int)E_FAIL);

    MSDK_CHECK_POINTER(m_pTimeManager, E_OUTOFMEMORY);

    mfxU32  nTargetFrameRateX100 = 0;
    ReadFrameRateFromRegistry(nTargetFrameRateX100);
    if (nTargetFrameRateX100 != 0)
    {
        m_pTimeManager->Init(nTargetFrameRateX100 / 1e2);
    }
    else
    {
        m_pTimeManager->Init(1e7 / pvi.AvgTimePerFrame);
    }

    mfxU32 nFrameRateExtN(0), nFrameRateExtD(0);

    sts = ConvertFrameRate(m_pTimeManager->GetFrameRate(), &nFrameRateExtN, &nFrameRateExtD);
    MSDK_CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, sts);

    m_mfxParamsVideo.mfx.FrameInfo.FrameRateExtN = nFrameRateExtN;
    m_mfxParamsVideo.mfx.FrameInfo.FrameRateExtD = nFrameRateExtD;

    m_mfxParamsVideo.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;

    m_mfxParamsVideo.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

    MSDK_MEMCPY_VAR(m_mfxParamsVPP.vpp.In,  &m_mfxParamsVideo.mfx.FrameInfo, sizeof(mfxFrameInfo));

    // set proper input format for VPP
    if(MEDIASUBTYPE_YUY2 == msub)
    {
        m_mfxParamsVPP.vpp.In.FourCC = MFX_FOURCC_YUY2;
    }
    else if(MEDIASUBTYPE_RGB32 == msub)
    {
        m_mfxParamsVPP.vpp.In.FourCC = MFX_FOURCC_RGB4;
    }

    // if vpp parameters were set via frame control update frame sizes and crops in m_mfxParamsVideo
    AlignFrameSizes(&m_mfxParamsVideo.mfx.FrameInfo,
                    m_mfxParamsVPP.vpp.Out.CropW,
                    m_mfxParamsVPP.vpp.Out.CropH);

    // specify parameters for aspect ratio conversion if needed along with resize
    m_mfxParamsVPP.vpp.In.AspectRatioH = m_mfxParamsVideo.mfx.FrameInfo.AspectRatioH;
    m_mfxParamsVPP.vpp.In.AspectRatioW = m_mfxParamsVideo.mfx.FrameInfo.AspectRatioW;
    sts = DARtoPAR(pvi.dwPictAspectRatioX, pvi.dwPictAspectRatioY,
        m_mfxParamsVideo.mfx.FrameInfo.CropW, m_mfxParamsVideo.mfx.FrameInfo.CropH,
        &m_mfxParamsVideo.mfx.FrameInfo.AspectRatioW, &m_mfxParamsVideo.mfx.FrameInfo.AspectRatioH);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, (int)E_FAIL);

    // this enables deinterlacing if necessary
    m_mfxParamsVideo.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    // copy all frame parameters back to vpp output frame info
    MSDK_MEMCPY_VAR(m_mfxParamsVPP.vpp.Out,  &m_mfxParamsVideo.mfx.FrameInfo, sizeof(mfxFrameInfo));

    if (m_mfxParamsVideo.mfx.TargetKbps == 0)
    {
        nBitrateRecommended = CalculateDefaultBitrate(m_mfxParamsVideo.mfx.CodecId, m_mfxParamsVideo.mfx.TargetUsage,
                                                      m_mfxParamsVideo.mfx.FrameInfo.Width, m_mfxParamsVideo.mfx.FrameInfo.Height, m_pTimeManager->GetFrameRate());
        m_mfxParamsVideo.mfx.MaxKbps    = (mfxU16)(nBitrateRecommended);
        m_mfxParamsVideo.mfx.TargetKbps = m_mfxParamsVideo.mfx.MaxKbps;
    }

    return S_OK;
};

HRESULT CEncVideoFilter::GetMediaType(int iPosition, CMediaType* pMediaType)
{
    GUID               guidFormat;
    VIDEOINFOHEADER2   SrcVIH, *pDstVIH(NULL);

    MSDK_CHECK_POINTER(pMediaType, E_POINTER);
    MSDK_CHECK_POINTER(m_pInput,   E_POINTER);
    MSDK_CHECK_ERROR(m_pInput->IsConnected(), FALSE, (int)E_UNEXPECTED);

    if (iPosition < 0)
    {
        return E_FAIL;
    }

    if (iPosition > 1)
    {
        return VFW_S_NO_MORE_ITEMS;
    }

    pMediaType->cbFormat = sizeof(VIDEOINFOHEADER2);
    pMediaType->pbFormat = (unsigned char*)CoTaskMemAlloc(pMediaType->cbFormat);
    MSDK_CHECK_POINTER(pMediaType->pbFormat, E_UNEXPECTED);
    memset(pMediaType->pbFormat, 0, pMediaType->cbFormat);

    pDstVIH = reinterpret_cast<VIDEOINFOHEADER2*> (pMediaType->pbFormat);
    MSDK_CHECK_POINTER(pDstVIH, E_UNEXPECTED);

    guidFormat = *m_pInput->CurrentMediaType().FormatType();

    if (FORMAT_VideoInfo2 == guidFormat)
    {
        MSDK_MEMCPY_VAR(SrcVIH, m_pInput->CurrentMediaType().Format(), sizeof(VIDEOINFOHEADER2));
    }
    else if (FORMAT_VideoInfo == guidFormat)
    {
        VIDEOINFOHEADER* pvih = reinterpret_cast<VIDEOINFOHEADER*>(m_pInput->CurrentMediaType().Format());
        MSDK_CHECK_POINTER(pvih, E_UNEXPECTED);

        memset(&SrcVIH, 0, sizeof(VIDEOINFOHEADER2));

        SrcVIH.rcSource         = pvih->rcSource;
        SrcVIH.rcTarget         = pvih->rcTarget;
        SrcVIH.dwBitRate        = pvih->dwBitRate;
        SrcVIH.dwBitErrorRate   = pvih->dwBitErrorRate;
        SrcVIH.AvgTimePerFrame  = pvih->AvgTimePerFrame;
        SrcVIH.bmiHeader        = pvih->bmiHeader;
    }
    else
    {
        return E_UNEXPECTED;
    }

    MSDK_MEMCPY_VAR(*pDstVIH, &SrcVIH, sizeof(VIDEOINFOHEADER2));

    // set correct frame sizes
    pDstVIH->bmiHeader.biWidth =
        pDstVIH->rcSource.right =
        pDstVIH->rcTarget.right = m_mfxParamsVideo.mfx.FrameInfo.CropW;

    pDstVIH->bmiHeader.biHeight =
        pDstVIH->rcSource.bottom =
        pDstVIH->rcTarget.bottom = m_mfxParamsVideo.mfx.FrameInfo.CropH;

    pDstVIH->bmiHeader.biSize   = sizeof(BITMAPINFOHEADER);
    pDstVIH->dwBitRate          = m_mfxParamsVideo.mfx.TargetKbps * 1000;
    pDstVIH->dwPictAspectRatioX = m_mfxParamsVideo.mfx.FrameInfo.AspectRatioW * m_mfxParamsVideo.mfx.FrameInfo.CropW;
    pDstVIH->dwPictAspectRatioY = m_mfxParamsVideo.mfx.FrameInfo.AspectRatioH * m_mfxParamsVideo.mfx.FrameInfo.CropH;

    pDstVIH->AvgTimePerFrame = (REFERENCE_TIME) (1e7 / m_pTimeManager->GetFrameRate());

    pDstVIH->dwControlFlags  = 0;
    pDstVIH->dwInterlaceFlags  = 0;

    pMediaType->SetType(&MEDIATYPE_Video);
    pMediaType->SetFormatType(&FORMAT_VideoInfo2);
    pMediaType->SetTemporalCompression(TRUE);
    pMediaType->SetSampleSize(0);

    const GUID* gSubtype = CodecId2GUID(m_mfxParamsVideo.mfx.CodecId, m_mfxParamsVideo.mfx.CodecProfile, iPosition);

    if (gSubtype)
    {
        pMediaType->SetSubtype(gSubtype);
    }

    if (MFX_CODEC_AVC == m_mfxParamsVideo.mfx.CodecId)
    {
        pDstVIH->bmiHeader.biCompression   = MAKEFOURCC('H','2','6','4');
    }
    if (MFX_CODEC_MPEG2 == m_mfxParamsVideo.mfx.CodecId)
    {
        pDstVIH->bmiHeader.biCompression   = MAKEFOURCC('M','P','E','G');
    }

    return S_OK;
}

HRESULT CEncVideoFilter::BreakConnect(PIN_DIRECTION dir)
{
    if (PINDIR_INPUT == dir)
    {
        mfxIMPL impl = MFX_IMPL_UNSUPPORTED;
        m_pEncoder->QueryIMPL(&impl);
        if (MFX_IMPL_HARDWARE & impl)
        {
            // it is not possible to re-use allocator
            // re-create encoder, new allocator will be provided on connect
            MSDK_SAFE_DELETE(m_pEncoder);
            m_pEncoder = new CBaseEncoder(m_nAPIVerMinor, m_nAPIVerMajor);
            MSDK_CHECK_POINTER(m_pEncoder, E_FAIL);
        }
    }

    return CTransformFilter::BreakConnect(dir);
}

STDMETHODIMP CEncVideoFilter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    if (riid == IID_IVideoPropertyPage)
    {
        return GetInterface(static_cast<IVideoEncoderProperty*>(this), ppv);
    }

    if (riid == IID_IAboutPropertyPage)
    {
        return GetInterface(static_cast<IAboutProperty*>(this), ppv);
    }

    if (riid == IID_IConfigureVideoEncoder)
    {
        return GetInterface(static_cast<IConfigureVideoEncoder*>(this), ppv);
    }

    if (riid == IID_ISpecifyPropertyPages)
    {
        return GetInterface(static_cast<ISpecifyPropertyPages*>(this), ppv);
    }

    if (riid == IID_IExposeMfxMemTypeFlags)
    {
        return GetInterface(static_cast<IExposeMfxMemTypeFlags*>(this), ppv);
    }

    if (riid == IID_IShareMfxSession)
    {
        return GetInterface(static_cast<IShareMfxSession*>(this), ppv);
    }

    return CBaseFilter::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT CEncVideoFilter::GetRunTimeStatistics(Statistics *statistics)
{
    mfxStatus       sts = MFX_ERR_NONE;
    mfxEncodeStat   mfxStat;

    MSDK_CHECK_POINTER(statistics, E_POINTER);

    //set all values to 0
    memset(statistics, 0, sizeof(Statistics));

    //check if encoder was already created and get statistics and params
    if (m_pEncoder && State_Running == m_State)
    {
        m_pEncoder->GetVideoParams(&m_mfxParamsVideo);

        sts = m_pEncoder->GetEncodeStat(&mfxStat);

        if (MFX_ERR_NONE == sts)
        {
            m_pTimeManager->SetEncodedFramesCount(mfxStat.NumFrame);

            if (mfxStat.NumFrame)
            {
                m_pTimeManager->SetBitrate((INT)(mfxStat.NumBit / mfxStat.NumFrame * m_pTimeManager->GetFrameRate()));
            }
        }
    }

    //fill statistics
    statistics->requested_bitrate           = m_mfxParamsVideo.mfx.TargetKbps * 1000;
    statistics->width                       = m_mfxParamsVideo.mfx.FrameInfo.CropW;
    statistics->height                      = m_mfxParamsVideo.mfx.FrameInfo.CropH;
    statistics->aspect_ratio.vertical       = m_mfxParamsVideo.mfx.FrameInfo.AspectRatioH;
    statistics->aspect_ratio.horizontal     = m_mfxParamsVideo.mfx.FrameInfo.AspectRatioW;

    statistics->frame_rate                  = (DWORD)(m_pTimeManager->GetFrameRate() * 100);
    statistics->frames_encoded              = m_pTimeManager->GetEncodedFramesCount();
    statistics->real_bitrate                = m_pTimeManager->GetBitrate();
    statistics->frames_received             = m_FramesReceived;

    return S_OK;
}

HRESULT CEncVideoFilter::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    CAutoLock cObjectLock(&m_csLock);

    return CTransformFilter::NewSegment(tStart,tStop,dRate);
}

CBasePin* CEncVideoFilter::GetPin(int n)
{
    HRESULT hr = S_OK;

    if (0 == n)
    {
        if (m_pInput == NULL)
        {
            mfxIMPL impl;
            m_pEncoder->QueryIMPL(&impl);
            m_pInput = (impl & MFX_IMPL_HARDWARE) ? new CVideoMemEncoderInputPin(this, &hr) :
                new CEncoderInputPin(this, &hr);
        }

        return m_pInput;
    }

    if (1 == n)
    {
        if (NULL == m_pOutput)
        {
            m_pOutput = new CEncoderOutputPin(this, &hr);
        }

        return m_pOutput;
    }

    return NULL;
};

HRESULT CEncVideoFilter::AlignFrameSizes(mfxFrameInfo* pInfo, mfxU16 nWidth, mfxU16 nHeight)
{
    MSDK_CHECK_POINTER(pInfo, E_POINTER);

    // set new sizes if they are specified
    if (nHeight)
    {
        pInfo->Height = nHeight;
        pInfo->CropH  = nHeight;
    }

    if (nWidth)
    {
        pInfo->Width  = nWidth;
        pInfo->CropW  = nWidth;
    }

    pInfo->Width  = (pInfo->Width + 15) &~ 15;
    pInfo->Height = (pInfo->Height + 31) &~ 31;

    return S_OK;
};

HRESULT CEncVideoFilter::DynamicFrameSizeChange(mfxFrameInfo* pInfo)
{
    MSDK_CHECK_POINTER(pInfo, E_POINTER);
    MSDK_CHECK_POINTER(m_pOutput, E_FAIL);

    CMediaType mt;

    IPin* pPin = m_pOutput->GetConnected();
    MSDK_CHECK_POINTER(pPin, E_FAIL);

    HRESULT hr = pPin->ConnectionMediaType(&mt);
    CHECK_RESULT_FAIL(hr);

    if (FORMAT_VideoInfo2 == *mt.FormatType())
    {
        VIDEOINFOHEADER2* pvi = reinterpret_cast<VIDEOINFOHEADER2*>(mt.pbFormat);
        pvi->bmiHeader.biWidth =
            pvi->rcSource.right =
            pvi->rcTarget.right = pInfo->CropW;

        pvi->bmiHeader.biHeight =
            pvi->rcSource.bottom =
            pvi->rcTarget.bottom = pInfo->CropH;
    }
    else if (FORMAT_VideoInfo == *mt.FormatType())
    {
        VIDEOINFOHEADER* pvi = reinterpret_cast<VIDEOINFOHEADER*>(mt.pbFormat);
        pvi->bmiHeader.biWidth =
            pvi->rcSource.right =
            pvi->rcTarget.right = pInfo->CropW;

        pvi->bmiHeader.biHeight =
            pvi->rcSource.bottom =
            pvi->rcTarget.bottom = pInfo->CropH;
    }

    CComPtr<IFilterGraph2> pGraph;
    m_pGraph->QueryInterface(IID_IFilterGraph2, (void**)&pGraph);

    if (pGraph)
    {
        hr = pGraph->ReconnectEx(m_pOutput, &mt);
    }

    // Release the format block.
    FreeMediaType(mt);

    return hr;
};

HRESULT CEncVideoFilter::GetRequiredFramesNum( mfxU16* nMinFrames, mfxU16* nRecommendedFrames )
{
    HRESULT                 hr = S_OK;
    mfxStatus               sts = MFX_ERR_NONE;
    mfxIMPL                 impl = MFX_IMPL_SOFTWARE;
    mfxFrameAllocRequest    mfxEncRequest;
    mfxFrameAllocRequest    mfxVppRequest[2];

    MSDK_CHECK_POINTER(nMinFrames, E_POINTER);
    MSDK_CHECK_POINTER(nRecommendedFrames, E_POINTER);
    MSDK_CHECK_POINTER(m_pEncoder, E_POINTER);
    MSDK_CHECK_POINTER(m_pEncoder->m_pmfxENC, E_POINTER);
    MSDK_CHECK_POINTER(m_pEncoder->m_pmfxVPP, E_POINTER);

    memset(&mfxEncRequest, 0, sizeof(mfxFrameAllocRequest));
    memset(mfxVppRequest, 0, 2 * sizeof(mfxFrameAllocRequest));

    sts = m_pEncoder->QueryIMPL(&impl);

    // Issue: application may change m_mfxParamsVideo later!
    if (MFX_ERR_NONE == sts)
    {
        if (MFX_IMPL_HARDWARE & impl)
        {
            m_mfxParamsVideo.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
            m_mfxParamsVPP.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        }
        else
        {
            m_mfxParamsVideo.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
            m_mfxParamsVPP.IOPattern =  MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        }

        sts = m_pEncoder->m_pmfxENC->QueryIOSurf(&m_mfxParamsVideo, &mfxEncRequest);
        MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        MSDK_IGNORE_MFX_STS(sts, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

        m_bUseVPP = memcmp(&m_mfxParamsVPP.vpp.In, &m_mfxParamsVPP.vpp.Out, sizeof(mfxFrameInfo)) ? true : false;
        if (m_bUseVPP)
        {
            sts = m_pEncoder->m_pmfxVPP->QueryIOSurf(&m_mfxParamsVPP, mfxVppRequest);
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        }

        if (MFX_ERR_NONE != sts)
        {
            hr = E_UNEXPECTED;
        }
        else
        {
            *nMinFrames = std::max((mfxU16)(mfxEncRequest.NumFrameMin + mfxVppRequest[1].NumFrameMin),
                mfxVppRequest[0].NumFrameMin);
            *nRecommendedFrames = std::max((mfxU16)(mfxEncRequest.NumFrameSuggested + mfxVppRequest[1].NumFrameSuggested),
                mfxVppRequest[0].NumFrameSuggested);

            m_pEncoder->m_nEncoderFramesNum = *nRecommendedFrames;
        }
    }

    return hr;
};

STDMETHODIMP CEncVideoFilter::SetParams(Params* pParams)
{
    HRESULT                 hr = S_OK;
    CComPtr<IFilterGraph>   pGraph;

    mfxStatus               sts = MFX_ERR_NONE;
    mfxIMPL                 impl;

    MSDK_CHECK_POINTER(m_pEncoder, E_FAIL);
    MSDK_CHECK_POINTER(m_pEncoder->m_pmfxENC, E_FAIL);

    mfxVideoParam paramsVideo = m_mfxParamsVideo;
    mfxVideoParam paramsVPP = m_mfxParamsVPP;

    StoreEncoderParams(pParams);

    m_pEncoder->QueryIMPL(&impl);
    m_mfxParamsVideo.IOPattern = (mfxU16)((MFX_IMPL_HARDWARE & impl) ? MFX_IOPATTERN_IN_VIDEO_MEMORY :
        MFX_IOPATTERN_IN_SYSTEM_MEMORY);

    // check if parameters are supported and let MFX encoder correct them if possible
    mfxVideoParam SupportedParams;
    memset(&SupportedParams, 0, sizeof(mfxVideoParam));
    SupportedParams.mfx.CodecId = m_mfxParamsVideo.mfx.CodecId;

    sts = m_pEncoder->m_pmfxENC->Query(&m_mfxParamsVideo, &SupportedParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    if (MFX_ERR_NONE != sts)
    {
        m_mfxParamsVideo = paramsVideo;
        m_mfxParamsVPP = paramsVPP;
        return E_FAIL;
    }

    // update current parameters
    m_mfxParamsVideo = SupportedParams;

    hr = m_pGraph->QueryInterface(IID_IFilterGraph, (void**)&pGraph);

    if (pGraph && SUCCEEDED(hr) && m_pInput)
    {
        hr = pGraph->Reconnect(m_pInput);
    }

    if (FAILED(hr))
    {
        m_mfxParamsVideo = paramsVideo;
        m_mfxParamsVPP = paramsVPP;
    }

    return hr;
}

HRESULT CEncVideoFilter::GetMfxMemTypeFlags(DWORD *pFlags)
{
    MSDK_CHECK_POINTER(pFlags, E_POINTER);

    m_bUseVPP = memcmp(&m_mfxParamsVPP.vpp.In, &m_mfxParamsVPP.vpp.Out, sizeof(mfxFrameInfo)) ? true : false;

    if (m_bUseVPP)
    {
        *pFlags = MFX_MEMTYPE_FROM_VPPIN;
    }
    else
    {
        *pFlags = MFX_MEMTYPE_FROM_ENCODE;
    }

    return S_OK;
}

HRESULT CEncVideoFilter::GetMfxSession(void **ppSession)
{
    MSDK_CHECK_POINTER(ppSession, E_POINTER);

    *ppSession = reinterpret_cast<void*>(&m_pEncoder->m_mfxVideoSession);

    return S_OK;
}

STDMETHODIMP CEncVideoFilter::GetParams(Params *params)
{
    CheckPointer(params, E_POINTER)

    if (NULL != m_pEncoder)
    {
        m_pEncoder->GetVideoParams(&m_mfxParamsVideo);
        CopyMFXToEncoderParams(params,&m_mfxParamsVideo);
        params->preset = m_EncoderParams.preset;

        return S_OK;
    }

    return E_FAIL;
}

mfxStatus CEncVideoFilter::StoreEncoderParams(Params *params)
{
    MSDK_CHECK_POINTER(params, MFX_ERR_NULL_PTR);

    HRESULT hr = S_OK;
    mfxVideoParam paramsVideo;
    mfxVideoParam paramsVPP;

    m_EncoderParams.frame_control.height = m_mfxParamsVideo.mfx.FrameInfo.CropH;
    m_EncoderParams.frame_control.width  = m_mfxParamsVideo.mfx.FrameInfo.CropW;

    if (0 == memcmp(&m_EncoderParams, params, sizeof(Params)))
    {
        return MFX_ERR_NONE;
    }

    MSDK_MEMCPY_VAR(m_EncoderParams, params, sizeof(m_EncoderParams));

    WriteParamsToRegistry();

    MSDK_MEMCPY_VAR(paramsVideo, &m_mfxParamsVideo, sizeof(paramsVideo));
    MSDK_MEMCPY_VAR(paramsVPP,   &m_mfxParamsVPP,   sizeof(paramsVPP));

    CopyEncoderToMFXParams(&m_EncoderParams, &m_mfxParamsVideo);

    if (State_Running != m_State)
    {
        MSDK_MEMCPY_VAR(m_mfxParamsVPP.vpp.Out, &m_mfxParamsVideo.mfx.FrameInfo, sizeof(mfxFrameInfo));

        AlignFrameSizes(&m_mfxParamsVPP.vpp.Out, (mfxU16)m_EncoderParams.frame_control.width, (mfxU16)m_EncoderParams.frame_control.height);

        if (memcmp(&m_mfxParamsVideo.mfx.FrameInfo, &m_mfxParamsVPP.vpp.Out, sizeof(mfxFrameInfo)))
        {
            MSDK_MEMCPY_VAR(m_mfxParamsVideo.mfx.FrameInfo, &m_mfxParamsVPP.vpp.Out, sizeof(mfxFrameInfo));
            hr = DynamicFrameSizeChange(&m_mfxParamsVPP.vpp.Out);
        }
    }

    return MFX_ERR_NONE;
}

/////////////////////////////////////////////////////////////////////////
/* CEncoderInputPin */
CEncoderInputPin::CEncoderInputPin(CTransformFilter* pTransformFilter, HRESULT* phr) :
CTransformInputPin(NAME("InputPin"), pTransformFilter, phr, L"In")
{
};

CEncoderInputPin::~CEncoderInputPin()
{
}

HRESULT CEncoderInputPin::GetAllocatorRequirements(ALLOCATOR_PROPERTIES *pProps)
{
    HRESULT                 hr =S_OK;
    AM_MEDIA_TYPE           mt;
    BITMAPINFOHEADER*       pbmi = NULL;
    long                    lPitch(0), lHeight(0);
    mfxU16                  nMin(0), nRecommended(0);

    hr = ConnectionMediaType(&mt);
    CHECK_RESULT_P_RET(hr, S_OK);

    pbmi = (FORMAT_VideoInfo2 == mt.formattype) ? HEADER2(mt.pbFormat) : HEADER(mt.pbFormat);

    // width must have 16-bytes alignment
    lPitch  = (((pbmi->biWidth + 15) &~ 15) * pbmi->biBitCount + 7) / 8;
    lHeight = (abs(pbmi->biHeight) + 31) &~ 31;

    pProps->cbBuffer = lPitch * lHeight;
    pProps->cBuffers = 16;
    pProps->cbAlign  = 16;
    pProps->cbPrefix = 0;

    //update buffers count basing on request from encoder
    hr = ((CEncVideoFilter*)m_pFilter)->GetRequiredFramesNum(&nMin, &nRecommended);
    if (SUCCEEDED(hr))
    {
        pProps->cBuffers = nRecommended;
    }

    // Release the format block.
    FreeMediaType(mt);

    return S_OK;
}

HRESULT CEncoderInputPin::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    if (riid == IID_IExposeMfxMemTypeFlags || riid == IID_IShareMfxSession)
    {
        return m_pFilter->NonDelegatingQueryInterface(riid, ppv);
    }
    else
    {
        return CTransformInputPin::NonDelegatingQueryInterface(riid, ppv);
    }
}

HRESULT CEncoderInputPin::NotifyAllocator(IMemAllocator * pAllocator, BOOL bReadOnly)
{
    HRESULT                 hr = S_OK;
    ALLOCATOR_PROPERTIES    props;
    mfxU16                  nMin(0), nRecommended(0);

    MSDK_CHECK_POINTER(pAllocator, E_POINTER);
    MSDK_CHECK_POINTER(m_pFilter, E_POINTER);

    hr = pAllocator->GetProperties(&props);
    CHECK_RESULT_P_RET(hr, S_OK);

    hr = ((CEncVideoFilter*)m_pFilter)->GetRequiredFramesNum(&nMin, &nRecommended);
    CHECK_RESULT_P_RET(hr, S_OK);

    if (nMin > (mfxU32)props.cBuffers)
    {
        hr = E_FAIL;
    }

    if (SUCCEEDED(hr))
    {
        hr = CTransformInputPin::NotifyAllocator(pAllocator, bReadOnly);
    }

    return hr;
};

/* CVideoMemEncoderInputPin */
CVideoMemEncoderInputPin::CVideoMemEncoderInputPin(CTransformFilter* pTransformFilter, HRESULT* phr) :
CEncoderInputPin(pTransformFilter, phr)
{
    m_pd3d = NULL;
    m_pd3dDevice = NULL;
    m_pd3dDeviceManager = NULL;
};

CVideoMemEncoderInputPin::~CVideoMemEncoderInputPin()
{
    MSDK_SAFE_RELEASE(m_pd3dDevice);
    MSDK_SAFE_RELEASE(m_pd3dDeviceManager);
    MSDK_SAFE_RELEASE(m_pd3d);
}

HRESULT CVideoMemEncoderInputPin::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    if (riid == __uuidof(IMFGetService))
    {
        return GetInterface((IMFGetService *) this, ppv);
    }
    else if (riid == __uuidof(IDirectXVideoMemoryConfiguration))
    {
        return GetInterface((IDirectXVideoMemoryConfiguration *) this, ppv);
    }
    else
    {
        return CEncoderInputPin::NonDelegatingQueryInterface(riid, ppv);
    }
}

HRESULT CVideoMemEncoderInputPin::GetService(REFGUID guidService,
                                     REFIID riid,
                                     LPVOID *ppvObject)
{
    if (guidService != MR_VIDEO_ACCELERATION_SERVICE ||
        riid != __uuidof(IDirect3DDeviceManager9))
    {
        return MF_E_UNSUPPORTED_SERVICE;
    }

    HRESULT hr = S_OK;

    if (!m_pd3dDeviceManager)
    {
        // Create IDirect3DDeviceManager9
        UINT ResetToken = 0;
        int SourceWidth = 320;
        int SourceHeight = 240;
        D3DPRESENT_PARAMETERS d3dParams;

        // use window at point (0,0)
        POINT Point = {0, 0};
        HWND hWindow = WindowFromPoint(Point);

        ZeroMemory(&d3dParams, sizeof (d3dParams));
        d3dParams.Windowed               = TRUE;
        d3dParams.SwapEffect             = D3DSWAPEFFECT_DISCARD;
        d3dParams.BackBufferFormat       = D3DFMT_UNKNOWN;
        d3dParams.PresentationInterval   = D3DPRESENT_INTERVAL_IMMEDIATE; // D3DPRESENT_INTERVAL_ONE
        d3dParams.BackBufferCount        = 1;
        d3dParams.Flags                  = D3DPRESENTFLAG_VIDEO;
        d3dParams.BackBufferFormat       = D3DFMT_X8R8G8B8;
        d3dParams.BackBufferWidth        = SourceWidth;
        d3dParams.BackBufferHeight       = SourceHeight;

        // create D3D
        Direct3DCreate9Ex(D3D_SDK_VERSION, &m_pd3d);

        if (!m_pd3d)
            return E_FAIL;

        // create D3D device
        hr = m_pd3d->CreateDeviceEx(
            MSDKAdapter::GetNumber(),
            D3DDEVTYPE_HAL,
            hWindow,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
            &d3dParams,
            NULL,
            &m_pd3dDevice);

        if (SUCCEEDED(hr))
        {
            hr = DXVA2CreateDirect3DDeviceManager9(&ResetToken, &m_pd3dDeviceManager);
        }

        if (SUCCEEDED(hr))
        {
            hr = m_pd3dDeviceManager->ResetDevice(m_pd3dDevice, ResetToken);
        }
    }

    MSDK_CHECK_POINTER(m_pd3dDeviceManager, E_UNEXPECTED);
    MSDK_CHECK_POINTER(m_pd3dDevice, E_UNEXPECTED);

    *(IDirect3DDeviceManager9**)ppvObject = m_pd3dDeviceManager;
    (*(IDirect3DDeviceManager9**)ppvObject)->AddRef();

    return hr;
}

HRESULT CVideoMemEncoderInputPin::GetAvailableSurfaceTypeByIndex(
    DWORD dwTypeIndex,
    DXVA2_SurfaceType *pdwType)
{
    if (dwTypeIndex) return E_FAIL;
    if (!pdwType) return E_POINTER;
    // we can accept DecoderRenderTarget D3D9 surfaces only
    *pdwType = DXVA2_SurfaceType_DecoderRenderTarget;
    return S_OK;
}

HRESULT CVideoMemEncoderInputPin::SetSurfaceType(DXVA2_SurfaceType dwType)
{
    // we can accept DecoderRenderTarget D3D9 surfaces only
    if (dwType != DXVA2_SurfaceType_DecoderRenderTarget) return E_FAIL;

    return S_OK;
}
