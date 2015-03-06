/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2013 Intel Corporation. All Rights Reserved.

*********************************************************************************

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
