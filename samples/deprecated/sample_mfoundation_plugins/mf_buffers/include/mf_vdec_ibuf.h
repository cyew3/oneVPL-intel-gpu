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

File: mf_vdec_ibuf.h

Purpose: define code for support of input buffering in Intel MediaFoundation
decoder plug-ins.

Defined Classes, Structures & Enumerations:
  * MFDecBitstream - enables synchronization between IMFSample and mfxBitstream

*********************************************************************************/
#ifndef __MF_VDEC_IBUF_H__
#define __MF_VDEC_IBUF_H__

#include "mf_utils.h"

/*------------------------------------------------------------------------------*/

class MFDecBitstream
{
public:
    MFDecBitstream (HRESULT &hr);
    ~MFDecBitstream(void);

    // resets all buffers
    HRESULT Reset(void);

    // init frame constructor (if needed)
    HRESULT InitFC(mfxVideoParam* pVideoParams, mfxU8* data, mfxU32 size);
    // load/unload/sync bitstream data
    HRESULT Load(IMFSample* pSample);
    HRESULT Sync(void);
    HRESULT Unload(void);
    // get bitstream with data
    mfxBitstream* GetMfxBitstream(void){ return m_pBst; }
    mfxBitstream* GetInternalMfxBitstream(void);
    void SetNoDataStatus(bool bSts) { m_bNoData = bSts; }

    void SetFiles(FILE* file, FILE* fc_file) { m_DbgFile.SetFile(file); m_DbgFileFc.SetFile(fc_file); }

protected: // functions
    HRESULT LoadNextBuffer(void);

    mfxU32  BstBufNeedHdr  (mfxU8* data, mfxU32 size);
    HRESULT BstBufAppendHdr(mfxU8* data, mfxU32 size);
    // increase buffer capacity with saving of buffer content (realloc)
    HRESULT BstBufRealloc(mfxU32 add_size);
    // increase buffer capacity without saving of buffer content (freee/malloc)
    HRESULT BstBufMalloc (mfxU32 new_size);
    HRESULT BstBufSync(void);

protected: // variables
    // mfx bistreams:
    // pointer to current bitstream
    mfxBitstream*       m_pBst;
    // buffered data: seq header or remained from previos sample
    mfxBitstream*       m_pBstBuf;
    // data from sample (buffering and copying is not needed)
    mfxBitstream*       m_pBstIn;
    // video params
    mfxVideoParam       m_VideoParams;
    bool                m_bNeedFC;
    // special flag to state that data in internal buffer is over and
    // GetIntrenalMfxBitstream should return NULL
    bool                m_bNoData;
    // MF buffer
    IMFSample*          m_pSample;
    IMFMediaBuffer*     m_pMediaBuffer;
    DWORD               m_nBuffersCount;
    DWORD               m_nBufferIndex;
    mfxU64              m_SampleTime;
    // some statistics:
    mfxU32              m_nBstBufReallocs;
    mfxU32              m_nBstBufCopyBytes;

    MFDebugDump         m_DbgFile;
    MFDebugDump         m_DbgFileFc;

    bool                m_bFirstFrame;
    LONGLONG            m_nFirstFrameTimeMinus1Sec;

private:
    // avoiding possible problems by defining operator= and copy constructor
    MFDecBitstream(const MFDecBitstream&);
    MFDecBitstream& operator=(const MFDecBitstream&);
};

#endif // #ifndef __MF_VDEC_IBUF_H__
