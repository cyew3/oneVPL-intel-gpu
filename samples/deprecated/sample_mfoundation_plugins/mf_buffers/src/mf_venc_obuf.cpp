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

File: mf_yuv_ibuf.cpp

Purpose: define code for support of input buffering in Intel MediaFoundation
encoder plug-ins.

*********************************************************************************/
#include "mf_venc_obuf.h"
#include "mf_param_brc_multiplier.h"
#include <bitset>

/*------------------------------------------------------------------------------*/

#undef  MFX_TRACE_CATEGORY
#define MFX_TRACE_CATEGORY _T("mfx_mft_unk")

/*------------------------------------------------------------------------------*/
// MFEncBitstream class

MFEncBitstream::MFEncBitstream(void):
    m_bInitialized(false),
    m_bLocked(false),
    m_bGap(false),
    m_BufSize(0),
    m_pSample(NULL),
    m_pMediaBuffer(NULL),
    m_Bst(),
    m_pSamplesPool(NULL)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    SAFE_NEW(m_pEncodedFrameInfo, mfxExtAVCEncodedFrameInfo);
}

/*------------------------------------------------------------------------------*/

MFEncBitstream::~MFEncBitstream(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    if (m_bLocked && m_pMediaBuffer)
    {
        m_pMediaBuffer->Unlock();
        m_bLocked = false;
    }
    SAFE_RELEASE(m_pMediaBuffer);
    SAFE_RELEASE(m_pSample);
    SAFE_RELEASE(m_pSamplesPool);
    SAFE_DELETE(m_pEncodedFrameInfo);
    Close();
}

/*------------------------------------------------------------------------------*/

mfxStatus MFEncBitstream::Init(mfxU32 bufSize,
                               MFSamplesPool* pSamplesPool)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    CHECK_EXPRESSION(!m_bInitialized, MFX_ERR_ABORTED);
    CHECK_POINTER(pSamplesPool, MFX_ERR_NULL_PTR);

    m_BufSize = bufSize;
    m_Bst.TimeStamp  = MF_TIME_STAMP_INVALID;

    pSamplesPool->AddRef();
    m_pSamplesPool = pSamplesPool;

    m_bInitialized = true;
    MFX_LTRACE_S(MF_TL_GENERAL, "MFX_ERR_NONE");
    return MFX_ERR_NONE;
}

/*------------------------------------------------------------------------------*/

void MFEncBitstream::Close(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    m_bInitialized = false;
}

/*------------------------------------------------------------------------------*/

mfxStatus MFEncBitstream::Alloc(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;
    HRESULT hr = S_OK;

    CHECK_EXPRESSION(m_bInitialized, MFX_ERR_NOT_INITIALIZED);
    m_pSample = m_pSamplesPool->GetSample();
    if (m_pSample)
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "Sample taken from pool");
        if (SUCCEEDED(hr)) hr = m_pSample->GetBufferByIndex(0, &m_pMediaBuffer);
        if (SUCCEEDED(hr)) hr = m_pSamplesPool->AddSample(m_pSample);
        else
        {
            SAFE_RELEASE(m_pMediaBuffer);
            SAFE_RELEASE(m_pSample);
        }
    }
    else
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "Sample created");
        if (SUCCEEDED(hr)) hr = MFCreateMemoryBuffer(m_BufSize, &m_pMediaBuffer);
        if (SUCCEEDED(hr)) hr = MFCreateVideoSampleFromSurface(NULL, &m_pSample);
        if (SUCCEEDED(hr)) hr = m_pSample->AddBuffer(m_pMediaBuffer);
        if (SUCCEEDED(hr)) hr = m_pSamplesPool->AddSample(m_pSample);
        else
        {
            SAFE_RELEASE(m_pMediaBuffer);
            SAFE_RELEASE(m_pSample);
        }
    }
    MFX_LTRACE_1(MF_TL_GENERAL, "m_pMediaBuffer->Lock: pBitstream: ", "%p", &m_Bst);
    if (SUCCEEDED(hr)) hr = m_pMediaBuffer->Lock((BYTE**)&(m_Bst.Data),
                                                 (DWORD*)&(m_Bst.MaxLength),
                                                 NULL);

    init_ext_buffer(*m_pEncodedFrameInfo);
    m_pEncodedFrameInfo->FrameOrder = _UI32_MAX;
    m_Bst.ExtParam = (mfxExtBuffer**)(&m_pEncodedFrameInfo);
    m_Bst.NumExtParam = 1;

    m_bLocked = SUCCEEDED(hr);
    CHECK_EXPRESSION(SUCCEEDED(hr), MFX_ERR_UNKNOWN);
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

HRESULT MFEncBitstream::Sync(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;

    CHECK_EXPRESSION(m_bInitialized, E_FAIL);
    CHECK_POINTER(m_pSample, E_POINTER);
    CHECK_POINTER(m_pMediaBuffer, E_POINTER);
    if (SUCCEEDED(hr))
    {
        WriteSample(&(m_Bst.Data[m_Bst.DataOffset]), m_Bst.DataLength);
        m_pMediaBuffer->SetCurrentLength(m_Bst.DataLength);
        if (MF_TIME_STAMP_INVALID != m_Bst.TimeStamp)
        {
            m_pSample->SetSampleTime(MFX2REF_TIME(m_Bst.TimeStamp));
            MFX_LTRACE_F(MF_TL_GENERAL, (mfxF64)REF2SEC_TIME(MFX2REF_TIME(m_Bst.TimeStamp)));
        }
        if (MFX_FRAMETYPE_I & m_Bst.FrameType)
        {
            MFX_LTRACE_S(MF_TL_GENERAL, "I frame");
            m_pSample->SetUINT32(MFSampleExtension_CleanPoint, TRUE);
        }
        else
        {
            MFX_LTRACE_S(MF_TL_GENERAL, "not I frame");
            m_pSample->SetUINT32(MFSampleExtension_CleanPoint, FALSE);
        }
        m_pSample->SetUINT32(MFSampleExtension_Discontinuity, (m_bGap)? TRUE: FALSE);

#if MFX_D3D11_SUPPORT
        //set interlacing type to output sample
        if ((MFX_PICSTRUCT_FIELD_TFF & m_Bst.PicStruct) || (MFX_PICSTRUCT_FIELD_BFF & m_Bst.PicStruct))
        {
            m_pSample->SetUINT32(MFSampleExtension_Interlaced, TRUE);
            m_pSample->SetUINT32(MFSampleExtension_BottomFieldFirst, (MFX_PICSTRUCT_FIELD_BFF & m_Bst.PicStruct) ? TRUE : FALSE);
        }
        else
        {
            m_pSample->SetUINT32(MFSampleExtension_Interlaced, FALSE);
        }
#endif //MFX_D3D11_SUPPORT

        UINT32 LtrFrameInfo = LTR_DEFAULT_FRAMEINFO;
        std::bitset<16> bitmap;
        for (int i = 0; i < 32 ; i++)
        {
            if (m_pEncodedFrameInfo->UsedRefListL0[i].LongTermIdx != MF_INVALID_IDX)
            {
                bitmap.set(m_pEncodedFrameInfo->UsedRefListL0[i].LongTermIdx);
            }
        }
        mfxU16 longTermIdx = m_pEncodedFrameInfo->LongTermIdx;
        LtrFrameInfo = MAKE_ULONG(bitmap.to_ulong(), longTermIdx);
        m_pSample->SetUINT32(MFSampleExtension_LongTermReferenceFrameInfo, LtrFrameInfo);
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFEncBitstream::Release(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    mfxStatus sts = MFX_ERR_NONE;

    CHECK_EXPRESSION(m_bInitialized, E_FAIL);
    memset(&m_Bst, 0, sizeof(mfxBitstream));
    m_Bst.TimeStamp = MF_TIME_STAMP_INVALID;
    if (m_bLocked && m_pMediaBuffer)
    {
        m_pMediaBuffer->Unlock();
        m_bLocked = false;
    }
    SAFE_RELEASE(m_pMediaBuffer);
    SAFE_RELEASE(m_pSample);
    // allocating new buffer
    sts = Alloc();
    if (MFX_ERR_NONE != sts) hr = E_FAIL;
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

mfxU32 MFEncBitstream::CalcBufSize(const mfxVideoParam &params)
{
    mfxU32 bufSize = 0;
    if (MfxGetBufferSizeInKB(params.mfx))
        bufSize  = 1000 * MfxGetBufferSizeInKB(params.mfx);
    else
        bufSize  = 2 * params.mfx.FrameInfo.Height *
                            params.mfx.FrameInfo.Width;
    return bufSize;
}

/*------------------------------------------------------------------------------*/

HRESULT FindNalUnit(mfxU32 length, mfxU8* data, mfxU32* start, mfxU32* end)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxU32 i = 0;

    CHECK_POINTER(data,  E_POINTER);
    CHECK_POINTER(start, E_POINTER);
    CHECK_POINTER(end,   E_POINTER);

    *start = 0; *end = 0;
    for (i = 2; i < length; ++i)
    {
        if ((0x00 == data[i-2]) && (0x00 == data[i-1]) && (0x01 == data[i]))
        {
            *start = i-2;
            *end   = ((i+1) < length)? i+1: i;
            break;
        }
    }
    for (i = (*end) + 1; i < length; ++i)
    {
        if (((i+2) < length) &&
            (0x00 == data[i]) && (0x00 == data[i+1]) && (0x01 == data[i+2]))
        {
            *end = i-1;
            break;
        }
        else
            ++(*end);
    }
    MFX_LTRACE_BUFFER(MF_TL_GENERAL, "nalu", &(data[*start]), (*end)-(*start)+1);
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/
// MFOutputBitstream class

MFOutputBitstream::MFOutputBitstream():
    m_uiHeaderSize(0),
    m_pHeader(NULL),
    m_uiHasOutputEventExists(0),
    m_nOutBitstreamIndex(0),
    m_nDispBitstreamIndex(0),
    m_bBitstreamsLimit(false),
    m_bSetDiscontinuityAttribute(false),
    m_bSetDTS(false),
    m_dbg_encout(NULL),
    m_Framerate(0),
    m_pFreeSamplesPool(NULL),
    m_nOutBitstreamsNum(0), // Encoded bitstreams
    m_pOutBitstreams(NULL)
{
}

/*------------------------------------------------------------------------------*/

void MFOutputBitstream::FreeHeader()
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    SAFE_FREE(m_pHeader);
    m_uiHeaderSize = 0;
}

/*------------------------------------------------------------------------------*/

void MFOutputBitstream::FreeOutBitstreams()
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    if (m_pOutBitstreams)
    {
        for (mfxU32 i = 0; i < m_nOutBitstreamsNum; ++i)
        {
            SAFE_DELETE(m_pOutBitstreams[i]->pMFBitstream);
        }
    }
    SAFE_FREE(m_pOutBitstreams);
    m_nOutBitstreamsNum    = 0;
    m_nOutBitstreamIndex   = 0;
    m_nDispBitstreamIndex  = 0;
}

/*------------------------------------------------------------------------------*/

void MFOutputBitstream::Free()
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    SAFE_RELEASE(m_pFreeSamplesPool);
    FreeOutBitstreams();
    FreeHeader();
}

/*------------------------------------------------------------------------------*/

void MFOutputBitstream::DiscardCachedSamples(MFXVideoSession* pMfxVideoSession)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    while (m_uiHasOutputEventExists && HaveBitstreamToDisplay())
    {
        --m_uiHasOutputEventExists;
        // dropping encoded frames
        MFEncOutData* pDispBitstream = GetDispBitstream();
        ATLASSERT(NULL != pDispBitstream);
        if (NULL != pDispBitstream)
        {
            if (!pDispBitstream->bSynchronized)
            {
                //This covers two options:
                //m_pDispBitstream->bSyncPointUsed = false - no SyncOperation is started for this SyncPoint
                //m_pDispBitstream->bSyncPointUsed = true  - SyncOperation is working in async thread. Need to wait.
                pMfxVideoSession->SyncOperation(pDispBitstream->syncPoint, INFINITE);
            }
            pDispBitstream->iSyncPointSts  = MFX_ERR_NONE;
            pDispBitstream->Release();
            // moving to next undisplayed surface
            MoveToNextBitstream(m_nDispBitstreamIndex);
        }
    }
    ATLASSERT(!HaveBitstreamToDisplay());
    m_bBitstreamsLimit = false;
    m_uiHasOutputEventExists = 0;

    //clear output DTS
    std::queue<LONGLONG> empty;
    std::swap( m_arrDTS, empty );

    //clear samples extradata
    m_ExtradataTransport.Clear();

    FreeHeader();
}

/*------------------------------------------------------------------------------*/

void MFOutputBitstream::ReleaseSample()
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    // releasing bitstream sample and alocating new one
    MFEncOutData* pDispBitstream = GetDispBitstream();
    ATLASSERT(NULL != pDispBitstream);
    if (NULL != pDispBitstream)
    {
        ATLASSERT(pDispBitstream->bSynchronized);
        pDispBitstream->Release();
        // moving to next undisplayed surface
        MoveToNextBitstream(m_nDispBitstreamIndex);
    }
    if (m_bBitstreamsLimit)
    {
        CheckBitstreamsNumLimit();
    }
}

/*------------------------------------------------------------------------------*/

bool MFOutputBitstream::GetSample(IMFSample** ppResult, HRESULT &hr)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    hr = S_OK;
    bool bDisplayFrame = false;

    if (m_uiHasOutputEventExists)
    {
        --m_uiHasOutputEventExists;
    }
    if (HaveBitstreamToDisplay())
    {
        bDisplayFrame = true;

        //copy input sample attributes to output
        IMFSample *pSample = NULL;
        MFEncBitstream* pDispMFBitstream = NULL;
        MFEncOutData* dispBitStream = GetDispBitstream();
        if (NULL != dispBitStream)
            pDispMFBitstream = dispBitStream->pMFBitstream;
        if (NULL != pDispMFBitstream)
        {
            pSample = pDispMFBitstream->GetSample();
            if (NULL != pSample)
            {
                mfxU32 nFrameOrder = pDispMFBitstream->GetFrameOrder();
                //TODO: Does MSDK guarantee to fill valid FrameOrder in all cases?
                CComPtr<IMFSampleExtradata> pExtraData = m_ExtradataTransport.Receive(nFrameOrder);
                if (NULL != pExtraData)
                {
                    CComPtr<IMFAttributes> pAttributes = pExtraData->GetAttributes();
                    if (NULL != pAttributes)
                        pAttributes->CopyAllItems(pSample);
                }
            }
            SAFE_RELEASE(pSample);

            hr = pDispMFBitstream->Sync();
        }
        else
        {
            ATLASSERT(NULL != pDispMFBitstream);
            hr = E_POINTER;
        }

        ATLASSERT(SUCCEEDED(hr));
    }
    ATLASSERT(bDisplayFrame);
    if (SUCCEEDED(hr))
    {
        //TODO: if ( ! HaveBitstreamToDisplay()) - what sample will be returned?
        //MFX_LTRACE_P(MF_TL_DEBUG, GetDispBitstream()->pMFBitstream);
        MFEncOutData* dispBitStream = GetDispBitstream();
        *ppResult = NULL;
        if (dispBitStream)
        {
            MFEncBitstream* tmp = dispBitStream->pMFBitstream;
            if (tmp)
                *ppResult = tmp->GetSample();
        }
        if (NULL != *ppResult)
        {
            REFERENCE_TIME duration = SEC2REF_TIME((m_Framerate)? 1.0/m_Framerate: 0.04);
            (*ppResult)->SetSampleDuration(duration);

#if MFX_D3D11_SUPPORT
            // In case of B-frames we set DTS for output
            if (m_bSetDTS)
            {
                LONGLONG nDts = 0;
                if (!m_arrDTS.empty())
                {
                    nDts = m_arrDTS.front();
                    if (nDts < 0)
                        nDts = 0;
                    m_arrDTS.pop();
                }
                else
                {
                    MFX_LTRACE_S(MF_TL_GENERAL, "no DTS in queue");
                    ATLASSERT(!m_arrDTS.empty());
                }
                MFX_LTRACE_I(MF_TL_NOTES, nDts);
                (*ppResult)->SetUINT64(MFSampleExtension_DecodeTimestamp, nDts);
            }
#endif
        }
    }
    return bDisplayFrame;
}

/*------------------------------------------------------------------------------*/

HRESULT MFOutputBitstream::FindHeader()
{
    MFEncOutData* pDispBitstream = GetDispBitstream();
    CHECK_POINTER(pDispBitstream, E_POINTER);
    return FindHeaderInternal(pDispBitstream->pMFBitstream->GetMfxBitstream());
}

/*------------------------------------------------------------------------------*/

mfxStatus MFOutputBitstream::UseBitstream(mfxU32 bufSize)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    MFEncOutData* pOutBitstream = GetOutBitstream();
    CHECK_POINTER(pOutBitstream, MFX_ERR_NULL_PTR);
    pOutBitstream->Use();
    pOutBitstream->pMFBitstream->IsGapBitstream(m_bSetDiscontinuityAttribute);
    m_bSetDiscontinuityAttribute = false;
    mfxStatus sts = SetFreeBitstream(bufSize);
    // sending output in sync or async way
    ++m_uiHasOutputEventExists;
    return sts;
}

/*------------------------------------------------------------------------------*/

void MFOutputBitstream::TraceBitstream(size_t i) const
{
    static const TCHAR *typeD=_T("Disp    ");
    static const TCHAR *typeO=_T("Out     ");
    static const TCHAR *typeB=_T("Disp&Out");
    static const TCHAR *typeN=_T("        ");
    const TCHAR *type = typeN;
    if (i == m_nDispBitstreamIndex && i == m_nOutBitstreamIndex)
        type = typeB;
    else if (i == m_nDispBitstreamIndex)
        type = typeD;
    else if (i == m_nOutBitstreamIndex)
        type = typeO;
    ATLASSERT(NULL != m_pOutBitstreams);
    ATLASSERT(i < m_nOutBitstreamsNum);
    if (NULL != m_pOutBitstreams && i < m_nOutBitstreamsNum)
    {
        MFEncOutData* p = m_pOutBitstreams[i];
        UNREFERENCED_PARAMETER(p);
        MFX_LTRACE_6(MF_TL_NOTES, "#", "%i: %S, %p, bFreeData=%d, bSynchronized=%d, syncPoint=%p",
                                        i, type, p, p->bFreeData, p->bSynchronized, p->syncPoint);
    }
}

/*------------------------------------------------------------------------------*/

void MFOutputBitstream::TraceBitstreams() const
{
    MFX_LTRACE_4(MF_TL_NOTES, "m_pOutBitstreams: ", "%p, Num=%d, Disp=%d, Out=%d",
        m_pOutBitstreams, m_nOutBitstreamsNum, m_nDispBitstreamIndex, m_nOutBitstreamIndex);
    for (size_t i = 0; i < m_nOutBitstreamsNum; i++)
    {
        TraceBitstream(i);
    }
}

/*------------------------------------------------------------------------------*/

HRESULT MFOutputBitstream::FindHeaderInternal(mfxBitstream* pBst)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    mfxU8* data = NULL;
    mfxU32 len = 0, start = 0, end = 0, mask = 0;
    mfxU32 uiOldHeaderSize = 0;
    bool bSPSFound = false, bPPSFound = false;

    CHECK_POINTER(pBst && (pBst->Data), E_POINTER);

    data = &(pBst->Data[pBst->DataOffset]);
    len = pBst->DataLength;
    do
    {
        hr = FindNalUnit(len, data, &start, &end);
        if (SUCCEEDED(hr) && (end-start+1 > 4))
        {
            mask = 0x1f & data[start+3];
            MFX_LTRACE_1(MF_TL_GENERAL, "mask = ", "%02x", mask);
            if ((0x07 == mask) || (0x08 == mask))
            {
                MFX_LTRACE_S(MF_TL_GENERAL, "SPS or PPS found");
                if (0x07 == mask) bSPSFound = true;
                if (0x08 == mask) bPPSFound = true;
                uiOldHeaderSize = m_uiHeaderSize;
                m_uiHeaderSize += end-start+1;

                if (!m_pHeader) m_pHeader = (mfxU8*)malloc(m_uiHeaderSize);
                else m_pHeader = (mfxU8*)realloc(m_pHeader, m_uiHeaderSize);

                if (m_pHeader)
                {
                    memcpy_s(&(m_pHeader[uiOldHeaderSize]), (m_uiHeaderSize-uiOldHeaderSize)*sizeof(mfxU8),
                             &(data[start]), end-start+1);
                }
                else hr = E_FAIL;
                if (bSPSFound && bPPSFound) break;
            }
            else if ((0x01 == mask) || (0x05 == mask)) break;
            data = &(data[end+1]);
            len -= end;
        }
    } while (SUCCEEDED(hr) && len);

    MFX_LTRACE_BUFFER(MF_TL_GENERAL, "m_pHeader", m_pHeader, m_uiHeaderSize);
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

// Free samples pool allocation
mfxStatus MFOutputBitstream::InitFreeSamplesPool()
{
    mfxStatus sts = MFX_ERR_NONE;
    SAFE_RELEASE(m_pFreeSamplesPool);
    if (!m_pFreeSamplesPool)
    {
        SAFE_NEW(m_pFreeSamplesPool, MFSamplesPool);
    }
    if (!m_pFreeSamplesPool) sts = MFX_ERR_MEMORY_ALLOC;
    else
    {
        m_pFreeSamplesPool->AddRef();
        if (FAILED(m_pFreeSamplesPool->Init(MF_BITSTREAMS_NUM))) sts = MFX_ERR_UNKNOWN;
    }
    return sts;
}

/*------------------------------------------------------------------------------*/

// Function returns 'true' if SyncOperation was called and 'false' overwise
bool MFOutputBitstream::HandleDevBusy(mfxStatus& sts, MFXVideoSession* pMfxVideoSession)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    bool ret_val = false;
    size_t outBitstreamIndex = m_nDispBitstreamIndex;

    sts = MFX_ERR_NONE;
    do
    {
        MFEncOutData* pOutBitstream = m_pOutBitstreams[outBitstreamIndex];
        if (NULL == pOutBitstream)
        {
            ATLASSERT(NULL != pOutBitstream);
            break;
        }
        if (!(pOutBitstream->bSyncPointUsed) && !(pOutBitstream->bFreeData))
        {
            pOutBitstream->bSyncPointUsed = true;
            sts = pOutBitstream->iSyncPointSts  = pMfxVideoSession->SyncOperation(pOutBitstream->syncPoint, INFINITE);
            ATLASSERT(!pOutBitstream->bSynchronized);
            pOutBitstream->bSynchronized = true;
            TraceBitstream(outBitstreamIndex);
            ret_val = true;
            break;
        }
        MoveToNextBitstream(outBitstreamIndex);
    } while(outBitstreamIndex != m_nDispBitstreamIndex);
    MFX_LTRACE_I(MF_TL_GENERAL, ret_val);
    return ret_val;
}

/*------------------------------------------------------------------------------*/

mfxStatus MFOutputBitstream::SetFreeBitstream(mfxU32 bufSize)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 i = 0;

    if (!m_pOutBitstreams)
    {
        //TODO: allocate MF_OUTPUT_EVENTS_COUNT_HIGH_LIMIT+2
        m_pOutBitstreams = (MFEncOutData**)calloc(MF_BITSTREAMS_NUM, sizeof(MFEncOutData*));
        if (!m_pOutBitstreams) sts = MFX_ERR_MEMORY_ALLOC;
        else
        {
            m_nOutBitstreamsNum = MF_BITSTREAMS_NUM;
            for (i = 0; (MFX_ERR_NONE == sts) && (i < m_nOutBitstreamsNum); ++i)
            {
                sts = AllocBitstream(i, bufSize);
            }
            if (MFX_ERR_NONE == sts)
            {
                m_nOutBitstreamIndex  = 0;
                m_nDispBitstreamIndex = 0;
            }
        }
    }
    else {
        MFEncOutData* outBitStream = GetOutBitstream();
        if (NULL == outBitStream)
        {
            ATLASSERT(NULL != outBitStream);
            sts = MFX_ERR_NULL_PTR;
        }
        else if (!(outBitStream->bFreeData))
        {
            MoveToNextBitstream(m_nOutBitstreamIndex);

            if (!(outBitStream->bFreeData)) // i. e. m_pOutBitstream == m_pDispBitstream
            { // increasing number of output bitstreams
                TraceBitstreams();

                MFEncOutData **pOutBitstreams = NULL;
                mfxU32 disp_index = (mfxU32)(m_nDispBitstreamIndex);

                pOutBitstreams = (MFEncOutData**)realloc(m_pOutBitstreams, (m_nOutBitstreamsNum+1)*sizeof(MFEncOutData*));
                if (!pOutBitstreams) sts = MFX_ERR_MEMORY_ALLOC;
                else
                {
                    m_pOutBitstreams = pOutBitstreams;
                    ++m_nOutBitstreamsNum;

                    // shifting tail of the array
                    memmove(m_pOutBitstreams + disp_index+1,
                            m_pOutBitstreams + disp_index,
                            (m_nOutBitstreamsNum - disp_index - 1)*sizeof(MFEncOutData*));

                    m_nOutBitstreamIndex  = disp_index;
                    m_nDispBitstreamIndex = disp_index+1;

                    // initializing new bitstream instance
                    sts = AllocBitstream(m_nOutBitstreamIndex, bufSize);
                }
                TraceBitstreams();
            }
        }
    }
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

void MFOutputBitstream::CheckBitstreamsNumLimit(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    MFX_LTRACE_I(MF_TL_GENERAL, m_nOutBitstreamsNum);
    MFX_LTRACE_I(MF_TL_GENERAL, m_uiHasOutputEventExists);
    bool bBitstreamsLimitOld = m_bBitstreamsLimit;

    if (m_uiHasOutputEventExists > MF_OUTPUT_EVENTS_COUNT_HIGH_LIMIT)
    {
        m_bBitstreamsLimit = true;
    }
    else if (m_uiHasOutputEventExists > MF_OUTPUT_EVENTS_COUNT_LOW_LIMIT)
    {
        //do nothing
        //if (!m_bBitstreamsLimit) m_bBitstreamsLimit = false;
    }
    else m_bBitstreamsLimit = false;
    if (bBitstreamsLimitOld != m_bBitstreamsLimit)
    {
        MFX_LTRACE_D(MF_TL_NOTES, m_uiHasOutputEventExists);
        MFX_LTRACE_D(MF_TL_NOTES, m_bBitstreamsLimit);
    }
}

/*------------------------------------------------------------------------------*/

bool MFOutputBitstream::HaveBitstreamToDisplay() const
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    MFX_LTRACE_I(MF_TL_NOTES, (m_nDispBitstreamIndex != m_nOutBitstreamIndex));
    return m_nDispBitstreamIndex != m_nOutBitstreamIndex;
}

/*------------------------------------------------------------------------------*/

MFEncOutData* MFOutputBitstream::GetOutBitstream() const
{
    MFEncOutData* pResult = NULL;
    if (NULL != m_pOutBitstreams && m_nOutBitstreamIndex < m_nOutBitstreamsNum)
        pResult = m_pOutBitstreams[m_nOutBitstreamIndex];
    ATLASSERT(NULL != pResult);
    return pResult;
}

/*------------------------------------------------------------------------------*/

MFEncOutData* MFOutputBitstream::GetDispBitstream() const
{
    MFEncOutData* pResult = NULL;
    if (NULL != m_pOutBitstreams && m_nDispBitstreamIndex < m_nOutBitstreamsNum)
        pResult = m_pOutBitstreams[m_nDispBitstreamIndex];
    ATLASSERT(NULL != pResult);
    return pResult;
}

/*------------------------------------------------------------------------------*/

void MFOutputBitstream::MoveToNextBitstream(size_t &nBitstreamIndex) const
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    TraceBitstream(nBitstreamIndex);
    ++nBitstreamIndex;
    ATLASSERT(NULL != m_pOutBitstreams);
    if (NULL != m_pOutBitstreams)
    {
        if ((mfxU32)(nBitstreamIndex) >= m_nOutBitstreamsNum)
        {
            nBitstreamIndex = 0;
        }
    }
    TraceBitstream(nBitstreamIndex);
}

/*------------------------------------------------------------------------------*/

mfxStatus MFOutputBitstream::AllocBitstream(size_t nIndex, mfxU32 bufSize)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFEncOutData* &pOutBitstream = m_pOutBitstreams[nIndex];
    SAFE_NEW(pOutBitstream, MFEncOutData);
    if (NULL == pOutBitstream)
    {
        sts = MFX_ERR_MEMORY_ALLOC;
    }
    if (MFX_ERR_NONE == sts)
    {
        memset(pOutBitstream, 0, sizeof(MFEncOutData));
        pOutBitstream->bFreeData = true;
        SAFE_NEW(pOutBitstream->pMFBitstream, MFEncBitstream);
        if (NULL == pOutBitstream->pMFBitstream)
            sts = MFX_ERR_MEMORY_ALLOC;
    }
    if (MFX_ERR_NONE == sts)
    {
        MFEncBitstream* pMFBitstream = pOutBitstream->pMFBitstream;
        if (MFX_ERR_NONE == sts) sts = pMFBitstream->Init(bufSize, m_pFreeSamplesPool);
        if (MFX_ERR_NONE == sts) sts = pMFBitstream->Alloc();
        if (MFX_ERR_NONE == sts) pMFBitstream->SetFile(m_dbg_encout);
    }
    return sts;
}
