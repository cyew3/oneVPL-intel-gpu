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

File: mf_venc_obuf.h

Purpose: define code for support of output buffering in Intel MediaFoundation
encoder plug-ins.

Defined Classes, Structures & Enumerations:
  * MFEncBitstream - enables synchronization between IMFSample and mfxBitstream

*********************************************************************************/
#ifndef __MF_VENC_OBUF_H__
#define __MF_VENC_OBUF_H__

#include "mf_utils.h"
#include "mf_samples_extradata.h"
#include <limits>

#define LTR_DEFAULT_FRAMEINFO 0xFFFF // [31..16] = 0, [15..0] = 0xFFFF
/*------------------------------------------------------------------------------*/

class MFEncBitstream : public MFDebugDump
{
public:
    MFEncBitstream (void);
    ~MFEncBitstream(void);

    mfxStatus Init   (mfxU32 bufSize,
                      MFSamplesPool* pSamplesPool);
    void      Close  (void);
    mfxStatus Alloc  (void);
    HRESULT   Sync   (void);
    HRESULT   Release(void);

    static mfxU32 CalcBufSize(const mfxVideoParam &params);

    mfxBitstream* GetMfxBitstream(void){ return &m_Bst; }
    void IsGapBitstream(bool bGap){ m_bGap = bGap; }

    IMFSample* GetSample(void)
    { if (m_pSample) m_pSample->AddRef(); return m_pSample; }

    mfxU32 GetFrameOrder() const
    {
        mfxU32 nResult = _UI32_MAX;
        if (NULL != m_pEncodedFrameInfo)
        {
            nResult = m_pEncodedFrameInfo->FrameOrder;
        }
        return nResult;
    }

protected:
    bool            m_bInitialized; // flag to indicate initialization
    bool            m_bLocked;      // flag to indicate lock of MF buffer
    bool            m_bGap;         // flag to indicate discontinuity frame
    mfxU32          m_BufSize;
    IMFSample*      m_pSample;
    IMFMediaBuffer* m_pMediaBuffer;
    mfxBitstream    m_Bst;
    MFSamplesPool*  m_pSamplesPool;
    mfxExtAVCEncodedFrameInfo*  m_pEncodedFrameInfo;

private:
    // avoiding possible problems by defining operator= and copy constructor
    MFEncBitstream(const MFEncBitstream&);
    MFEncBitstream& operator=(const MFEncBitstream&);
};

/*------------------------------------------------------------------------------*/

struct MFEncOutData
{
    bool             bFreeData;         // Empty entry
    bool             bSynchronized;     // SyncOperation is completed
    mfxSyncPoint     syncPoint;

    bool             bSyncPointUsed;    // SyncOperation is started
    mfxStatus        iSyncPointSts;
    MFEncBitstream*  pMFBitstream;

    void Use()
    {
        bFreeData = false;
    }

    mfxStatus SyncOperation(MFXVideoSession* pMfxVideoSession)
    {
        ATLASSERT(!(bFreeData));
        bSyncPointUsed = true;
        ATLASSERT(!bSynchronized);
        iSyncPointSts = pMfxVideoSession->SyncOperation(syncPoint, INFINITE);
        bSynchronized = true;
        return iSyncPointSts;
    }

    void Release()
    {
        pMFBitstream->Release();
        syncPoint      = NULL;
        bSyncPointUsed = false;
        bFreeData      = true;
        bSynchronized  = false;
    }
};

/*------------------------------------------------------------------------------*/

// initial number of output bitstreams (samples)
#define MF_BITSTREAMS_NUM 1
// number of buffered output events to start receiving input data again after maximum is reached
#define MF_OUTPUT_EVENTS_COUNT_LOW_LIMIT 5
// maximum number of buffered output events (bitstreams)
#define MF_OUTPUT_EVENTS_COUNT_HIGH_LIMIT 15

/*------------------------------------------------------------------------------*/

class MFOutputBitstream
{
public:
    MFOutputBitstream::MFOutputBitstream();
    virtual void            Free(); //free m_pFreeSamplesPool, OutBitstreams, Header
    void                    DiscardCachedSamples(MFXVideoSession* pMfxVideoSession);
    void                    ReleaseSample();
    bool                    GetSample(IMFSample** ppResult, HRESULT &hr);
    HRESULT                 FindHeader();
    mfxStatus               UseBitstream(mfxU32 bufSize);
    mfxStatus               InitFreeSamplesPool(); // Free samples pool allocation
    bool                    HandleDevBusy(mfxStatus& sts, MFXVideoSession* pMfxVideoSession);
    mfxStatus               SetFreeBitstream(mfxU32 bufSize);
    void                    CheckBitstreamsNumLimit(void);
    bool                    HaveBitstreamToDisplay() const; //m_pDispBitstream != m_pOutBitstream
    void                    SetFramerate(mfxU32 fr_n, mfxU32 fr_d) { m_Framerate = mf_get_framerate(fr_n, fr_d); }
    //used only in ProcessFrameEnc as arguments for EncodeFrameAsync
    MFEncOutData*           GetOutBitstream() const;
    //used in 1) AsyncThread to SyncOperation or get saved sts. 2) ProcessOutput (check against NULL)
    MFEncOutData*           GetDispBitstream() const;

    mfxU32                  m_uiHeaderSize;
    mfxU8*                  m_pHeader;
    mfxU32                  m_uiHasOutputEventExists; //TODO: try to hide

    //used widely (4 places)
    bool                    m_bBitstreamsLimit;
    //fields below are used about 1-2 times
    bool                    m_bSetDiscontinuityAttribute;
    //TODO: move to MFSampleExtradataTransport
    std::queue<LONGLONG>    m_arrDTS;        // Input timestamps used for output DTS
    bool                    m_bSetDTS;       // if not set previous DTSes are ignored.
    MFSampleExtradataTransport m_ExtradataTransport;

    FILE*                   m_dbg_encout;
    //debug tracing functions:
    void                    TraceBitstream(size_t i) const;
    void                    TraceBitstreams() const;
protected:
    HRESULT                 FindHeaderInternal(mfxBitstream* pBst);
    virtual void            FreeHeader();
    virtual void            FreeOutBitstreams();
    //moves to next bitstream on circular buffer (may return to m_pOutBitstreams)
    void                    MoveToNextBitstream(size_t &nBitstreamIndex) const;
    mfxStatus               AllocBitstream(size_t nIndex, mfxU32 bufSize);

    mfxF64                  m_Framerate;
    MFSamplesPool*          m_pFreeSamplesPool;

    // pointers to encoded bitstreams
    // Stored in "circular buffer" with "one slot always open".
    // m_nOutBitstreamsNum      - number of allocated elements. m_pOutBitstreams - allocated buffer.
    // m_nDispBitstreamIndex    - index of "start"
    // m_nOutBitstreamIndex     - index of next element after the "end", This is "open slot"
    mfxU32                  m_nOutBitstreamsNum;
    MFEncOutData**          m_pOutBitstreams;
    size_t                  m_nOutBitstreamIndex;
    size_t                  m_nDispBitstreamIndex;
};

#endif // #ifndef __MF_VENC_OBUF_H__
