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

File: mf_vdec_ibuf.cpp

Purpose: define code for support of input buffering in Intel MediaFoundation
decoder plug-ins.

*********************************************************************************/
#include "mf_vdec_ibuf.h"

/*------------------------------------------------------------------------------*/

#undef  MFX_TRACE_CATEGORY
#define MFX_TRACE_CATEGORY _T("mfx_mft_unk")

/*------------------------------------------------------------------------------*/

MFDecBitstream::MFDecBitstream(HRESULT &hr):
    m_pBst(NULL),
    m_pBstBuf(NULL),
    m_pBstIn(NULL),
    m_bNeedFC(false),
    m_bNoData(false),
    m_pSample(NULL),
    m_pMediaBuffer(NULL),
    m_nBuffersCount(0),
    m_nBufferIndex(0),
    m_SampleTime(MF_TIME_STAMP_INVALID),
    m_nBstBufReallocs(0),
    m_nBstBufCopyBytes(0),
    m_bFirstFrame(true),
    m_nFirstFrameTimeMinus1Sec(0)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    hr = E_FAIL;
    m_pBstBuf = (mfxBitstream*)calloc(1, sizeof(mfxBitstream));
    if (!m_pBstBuf) return;
    m_pBstIn = (mfxBitstream*)calloc(1, sizeof(mfxBitstream));
    if (!m_pBstIn) return;
    memset(&m_VideoParams, 0, sizeof(mfxVideoParam));
    hr = S_OK;
}

/*------------------------------------------------------------------------------*/

MFDecBitstream::~MFDecBitstream(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    if (m_pBstBuf)
    {
        MFX_LTRACE_I(MF_TL_GENERAL, m_pBstBuf->MaxLength);
        MFX_LTRACE_I(MF_TL_GENERAL, m_nBstBufReallocs);
        MFX_LTRACE_I(MF_TL_GENERAL, m_nBstBufCopyBytes);

        SAFE_FREE(m_pBstBuf->Data);
        SAFE_FREE(m_pBstBuf);
    }
    SAFE_FREE(m_pBstIn);
}

/*------------------------------------------------------------------------------*/

HRESULT MFDecBitstream::Reset(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;

    CHECK_POINTER(m_pBstBuf, E_POINTER);
    CHECK_POINTER(m_pBstIn, E_POINTER);

    m_pBst = NULL;
    memset(m_pBstIn, 0, sizeof(mfxBitstream));
    m_pBstBuf->DataOffset = 0;
    m_pBstBuf->DataLength = 0;
    m_pBstBuf->TimeStamp  = 0;
    // the following code is just in case (it should occur)
    if (m_pMediaBuffer) m_pMediaBuffer->Unlock();
    SAFE_RELEASE(m_pMediaBuffer);
    m_nBuffersCount = 0;
    m_nBufferIndex  = 0;
    m_SampleTime    = MF_TIME_STAMP_INVALID;

    m_bFirstFrame   = true;
    m_nFirstFrameTimeMinus1Sec = 0;

    SAFE_RELEASE(m_pSample);

    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

static inline mfxU32 BstGetValue32(mfxU8* pBuf)
{
    if (!pBuf) return 0;
    return ((*pBuf) << 24) | (*(pBuf + 1) << 16) | (*(pBuf + 2) << 8) | (*(pBuf + 3));
}

/*------------------------------------------------------------------------------*/

static inline void BstSetValue32(mfxU32 nValue, mfxU8* pBuf)
{
    if (pBuf)
    {
        mfxU32 i = 0;
        for (i = 0; i < 4; ++i)
        {
            *pBuf = (mfxU8)(nValue >> (8 * i));
            ++pBuf;
        }
    }
    return;
}

/*------------------------------------------------------------------------------*/

HRESULT MFDecBitstream::BstBufRealloc(mfxU32 add_size)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    mfxU32  needed_MaxLength = 0;

    CHECK_POINTER(m_pBstBuf, E_POINTER);
    if (add_size)
    {
        needed_MaxLength = m_pBstBuf->DataLength + add_size;
        if (m_pBstBuf->MaxLength < needed_MaxLength)
        { // increasing buffer capacity if needed
            // statistics (for successfull operation)
            m_nBstBufCopyBytes += m_pBstBuf->MaxLength;
            ++m_nBstBufReallocs;

            m_pBstBuf->Data = (mfxU8*)realloc(m_pBstBuf->Data, needed_MaxLength);
            m_pBstBuf->MaxLength = needed_MaxLength;
        }
        if (!(m_pBstBuf->Data))
        {
            m_pBstBuf->MaxLength = 0;
            hr = E_OUTOFMEMORY;
        }
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFDecBitstream::BstBufMalloc(mfxU32 new_size)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    mfxU32  needed_MaxLength = 0;

    CHECK_POINTER(m_pBstBuf, E_POINTER);
    if (new_size)
    {
        needed_MaxLength = new_size;
        if (m_pBstBuf->MaxLength < needed_MaxLength)
        { // increasing buffer capacity if needed
            SAFE_FREE(m_pBstBuf->Data);
            m_pBstBuf->Data = (mfxU8*)malloc(needed_MaxLength);
            m_pBstBuf->MaxLength = needed_MaxLength;
            ++m_nBstBufReallocs;
        }
        if (!(m_pBstBuf->Data))
        {
            m_pBstBuf->MaxLength = 0;
            hr = E_OUTOFMEMORY;
        }
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

// NeedHeader: returns buffer size needed for the bitstream header
mfxU32 MFDecBitstream::BstBufNeedHdr(mfxU8* data, mfxU32 size)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    CHECK_EXPRESSION(m_bNeedFC, 0);
    CHECK_EXPRESSION(data && size, 0);
    if (MFX_CODEC_VC1 == m_VideoParams.mfx.CodecId)
    {
        if ((MFX_PROFILE_VC1_SIMPLE == m_VideoParams.mfx.CodecProfile) ||
            (MFX_PROFILE_VC1_MAIN   == m_VideoParams.mfx.CodecProfile))
        {
            size = 8; // header will be: "<4 bytes: frame_size> 0x00000000"
        }
        else if (MFX_PROFILE_VC1_ADVANCED == m_VideoParams.mfx.CodecProfile)
        {
            CHECK_EXPRESSION(data, 0);
            if (size >= 4)
            {
                switch (BstGetValue32(data))
                {
                    case 0x010A: case 0x010C: case 0x010D: case 0x010E: case 0x010F:
                    case 0x011B: case 0x011C: case 0x011D: case 0x011E: case 0x011F:
                        size = 0;
                        break;
                    default:
                        size = 4; // header will be 0x0D010000
                }
            }
            else // buffer is less than 4 bytes, it can't contain header
            {    // we need to append 0x0D010000
                size = 4;
            }
        }
    }
    MFX_LTRACE_I(MF_TL_GENERAL, size);
    return size;
}

/*------------------------------------------------------------------------------*/

// AppendHeader: appends header to the buffered bistream
HRESULT MFDecBitstream::BstBufAppendHdr(mfxU8* /*data*/, mfxU32 /*size*/)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;

    CHECK_POINTER(m_pBstBuf, E_POINTER);
    if (MFX_CODEC_VC1 == m_VideoParams.mfx.CodecId)
    {
        if ((MFX_PROFILE_VC1_SIMPLE == m_VideoParams.mfx.CodecProfile) ||
            (MFX_PROFILE_VC1_MAIN   == m_VideoParams.mfx.CodecProfile))
        {
            hr = BstBufRealloc(8);
            if (SUCCEEDED(hr))
            {
                DWORD total_size = 0;
                mfxU8* buf = m_pBstBuf->Data + m_pBstBuf->DataOffset + m_pBstBuf->DataLength;

                if (m_pSample) m_pSample->GetTotalLength(&total_size);

                BstSetValue32(total_size, buf);
                BstSetValue32(0x00000000, buf + 4);
                m_pBstBuf->DataLength += 8;
                m_nBstBufCopyBytes += 8;

                m_DbgFileFc.WriteSample(buf, 8);
            }
        }
        else if (MFX_PROFILE_VC1_ADVANCED == m_VideoParams.mfx.CodecProfile)
        {
            hr = BstBufRealloc(4);
            if (SUCCEEDED(hr))
            {
                mfxU8* buf = m_pBstBuf->Data + m_pBstBuf->DataOffset + m_pBstBuf->DataLength;

                BstSetValue32(0x0D010000, buf);
                m_pBstBuf->DataLength += 4;
                m_nBstBufCopyBytes += 4;

                m_DbgFileFc.WriteSample(buf, 4);
            }
        }
    }
    else hr = E_FAIL;
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFDecBitstream::InitFC(mfxVideoParam* pVideoParams,
                               mfxU8* data, mfxU32 size)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;

    MFX_LTRACE_I(MF_TL_GENERAL, size);

    CHECK_POINTER(pVideoParams, MFX_ERR_NULL_PTR);
    CHECK_POINTER(m_pBstBuf, E_POINTER);
    if ((MFX_CODEC_VC1 == pVideoParams->mfx.CodecId) &&
        (MFX_PROFILE_UNKNOWN != pVideoParams->mfx.CodecProfile))
    {
        m_bNeedFC = true;
        memcpy_s(&m_VideoParams, sizeof(m_VideoParams), pVideoParams, sizeof(mfxVideoParam));
        if ((MFX_PROFILE_VC1_SIMPLE == m_VideoParams.mfx.CodecProfile) ||
            (MFX_PROFILE_VC1_MAIN   == m_VideoParams.mfx.CodecProfile))
        {
            hr = BstBufRealloc(8 + size + 12);
            if (SUCCEEDED(hr))
            {
                mfxU8* buf = m_pBstBuf->Data + m_pBstBuf->DataOffset + m_pBstBuf->DataLength;

                // set start code
                BstSetValue32(0xC5000000, buf);
                // set size of sequence header
                BstSetValue32(size,  buf + 4);
                // copy frame data
                memcpy_s(buf + 8,
                        (m_pBstBuf->MaxLength - m_pBstBuf->DataOffset - m_pBstBuf->DataLength - 8)*sizeof(mfxU8),
                        data, size);
                // set sizes to the end of data
                BstSetValue32(m_VideoParams.mfx.FrameInfo.CropH, buf + 8 + size);
                BstSetValue32(m_VideoParams.mfx.FrameInfo.CropW,  buf + 8 + size + 4);
                // set 0 to the last 4 bytes
                BstSetValue32(0, buf + 8 + size + 8);
                m_pBstBuf->DataLength += 8 + size + 12;
                m_nBstBufCopyBytes += 8 + size + 12;

                m_DbgFileFc.WriteSample(buf, 8 + size + 12);
            }
        }
        else if (MFX_PROFILE_VC1_ADVANCED == m_VideoParams.mfx.CodecProfile)
        {
            hr = BstBufRealloc(size);
            if (SUCCEEDED(hr))
            {
                mfxU8* buf = m_pBstBuf->Data + m_pBstBuf->DataOffset + m_pBstBuf->DataLength;

                memcpy_s(buf,
                        (m_pBstBuf->MaxLength - m_pBstBuf->DataOffset - m_pBstBuf->DataLength)*sizeof(mfxU8),
                        data, size);
                m_pBstBuf->DataLength += size;
                m_nBstBufCopyBytes += size;

                m_DbgFileFc.WriteSample(buf, size);
            }
        }
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFDecBitstream::LoadNextBuffer(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    bool   bLocked = false, bUnlock = false;
    mfxU8* pData = NULL;
    mfxU32 uiDataLen = 0, uiDataSize = 0, hsize = 0;

    if (m_pSample && (m_nBufferIndex < m_nBuffersCount))
    {
        hr = m_pSample->GetBufferByIndex(m_nBufferIndex, &m_pMediaBuffer);
        if (SUCCEEDED(hr)) hr = m_pMediaBuffer->Lock((BYTE**)&pData, (DWORD*)&uiDataLen, (DWORD*)&uiDataSize);
        bLocked = SUCCEEDED(hr);
        if (SUCCEEDED(hr))
        {
            m_DbgFile.WriteSample(pData, uiDataSize);

            if (0 == m_nBufferIndex) hsize = BstBufNeedHdr(pData, uiDataSize);
            else hsize = 0;
            if (hsize || (m_pBstBuf->DataLength))
            {
                hr = BstBufRealloc(hsize + uiDataSize);
                if (hsize) hr = BstBufAppendHdr(pData, uiDataSize);
                if (SUCCEEDED(hr))
                {
                    mfxU8* buf = m_pBstBuf->Data + m_pBstBuf->DataOffset + m_pBstBuf->DataLength;

                    m_DbgFileFc.WriteSample(pData, uiDataSize);

                    memcpy_s(buf,
                            (m_pBstBuf->MaxLength - m_pBstBuf->DataOffset - m_pBstBuf->DataLength)*sizeof(mfxU8),
                            pData, uiDataSize);
                    m_pBstBuf->DataLength += uiDataSize;
                    m_nBstBufCopyBytes += uiDataSize;
                }
            }
            else
            {
                m_DbgFileFc.WriteSample(pData, uiDataSize);
            }
            if (m_pBstBuf->DataLength) bUnlock = true;
            else bUnlock = false;
        }
        if (bUnlock || FAILED(hr))
        {
            if (bLocked) m_pMediaBuffer->Unlock();
            SAFE_RELEASE(m_pMediaBuffer);
        }
        ++m_nBufferIndex;
    }
    else hr = E_FAIL;
    if (SUCCEEDED(hr))
    {
        if (m_pBstBuf->DataLength) m_pBst = m_pBstBuf;
        else
        {
            m_pBstIn->DataLength = uiDataSize;
            m_pBstIn->MaxLength  = uiDataLen;
            m_pBstIn->Data       = pData;
            m_pBst = m_pBstIn;
        }
        m_pBst->TimeStamp = m_SampleTime;
    }
    else m_pBst = NULL;
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

mfxBitstream* MFDecBitstream::GetInternalMfxBitstream(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    if (!m_bNoData && m_pBstBuf->DataLength) m_pBst = m_pBstBuf;
    MFX_LTRACE_P(MF_TL_GENERAL, m_pBst);
    return m_pBst;
}

/*------------------------------------------------------------------------------*/

HRESULT MFDecBitstream::Load(IMFSample* pSample)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    REFERENCE_TIME time = 0;

    CHECK_POINTER(pSample, E_POINTER);
    CHECK_POINTER(m_pBstBuf, E_POINTER);
    CHECK_POINTER(m_pBstIn, E_POINTER);

    SAFE_RELEASE(m_pSample);
    pSample->AddRef();
    m_pSample = pSample;

    hr = m_pSample->GetBufferCount(&m_nBuffersCount);

    if (SUCCEEDED(m_pSample->GetSampleTime(&time)))
    {
#if !MFX_D3D11_SUPPORT
        if (m_bFirstFrame)
        {
            //if first frame timestamp > 10, then save it to correct all timestamps
            if (time > 10*1e7)
                m_nFirstFrameTimeMinus1Sec = time - 1e7;
            m_bFirstFrame = false;
        }
        //correct timestamps, in case of start from very big value (>10secs).
        //generally m_nFirstFrameTimeMinus1Sec == 0
        time -= m_nFirstFrameTimeMinus1Sec;
#endif //!MFX_D3D11_SUPPORT
        m_SampleTime = REF2MFX_TIME(time);
    }
    else
        m_SampleTime = MF_TIME_STAMP_INVALID;

    if (SUCCEEDED(hr)) hr = LoadNextBuffer();
    MFX_LTRACE_F(MF_TL_GENERAL, REF2SEC_TIME(MFX2REF_TIME(m_SampleTime)));
    MFX_LTRACE_I(MF_TL_GENERAL, m_nBuffersCount);
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFDecBitstream::Unload(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    SAFE_RELEASE(m_pSample);
    m_nBuffersCount = 0;
    m_nBufferIndex  = 0;
    m_SampleTime = MF_TIME_STAMP_INVALID;
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

HRESULT MFDecBitstream::BstBufSync(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;

    if (SUCCEEDED(hr) && m_pBst)
    {
        if (m_pBst == m_pBstBuf)
        {
            if (m_pBstBuf->DataLength && m_pBstBuf->DataOffset)
            { // shifting data to the beginning of the buffer
                memmove(m_pBstBuf->Data, m_pBstBuf->Data + m_pBstBuf->DataOffset,
                    m_pBstBuf->DataLength);
                m_nBstBufCopyBytes += m_pBstBuf->DataLength;
            }
            m_pBstBuf->DataOffset = 0;
        }
        if ((m_pBst == m_pBstIn) && m_pBstIn->DataLength)
        { // copying data from m_pBstOut to m_pBstBuf
            // Note: here m_pBstBuf is empty
            hr = BstBufMalloc(m_pBstIn->DataLength);
            if (SUCCEEDED(hr))
            {
                memcpy_s(m_pBstBuf->Data, m_pBstBuf->MaxLength*sizeof(mfxU8), m_pBstIn->Data + m_pBstIn->DataOffset, m_pBstIn->DataLength);
                m_pBstBuf->DataOffset = 0;
                m_pBstBuf->DataLength = m_pBstIn->DataLength;
                m_pBstBuf->TimeStamp  = m_pBstIn->TimeStamp;
                m_nBstBufCopyBytes += m_pBstIn->DataLength;
            }
        }
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFDecBitstream::Sync(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;

    hr = BstBufSync();
    m_pBst = NULL;
    if (m_pBstIn) memset(m_pBstIn, 0, sizeof(mfxBitstream));
    if (m_pMediaBuffer) m_pMediaBuffer->Unlock();
    SAFE_RELEASE(m_pMediaBuffer);

    // NOTE: LoadNextBuffer may fail if there is no more data in sample - that's
    // not an error
    if (SUCCEEDED(hr)) /*hr =*/ LoadNextBuffer();
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}
