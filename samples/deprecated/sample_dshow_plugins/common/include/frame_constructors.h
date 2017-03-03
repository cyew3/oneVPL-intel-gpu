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

#include "windows.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"

#include <time.h>
#include <iostream>
#include <dshow.h>
#include <dvdmedia.h>
#include <limits>
#include <algorithm>

class CFrameConstructor
{
public:
    CFrameConstructor();
    virtual ~CFrameConstructor();

    virtual mfxStatus ConstructFrame(IMediaSample* pSample, mfxBitstream* pBS);
    virtual void      Reset() {};
    void              SaveResidialData(mfxBitstream* pBS);

protected:

    mfxBitstream      m_mfxResidualBS;
};

class CVC1FrameConstructor : public CFrameConstructor
{
public:
    CVC1FrameConstructor(mfxVideoParam* pVideoParam);

    mfxStatus       ConstructFrame              (IMediaSample* pSample, mfxBitstream* pBS);

    mfxStatus       ConstructHeaderSM           (mfxU8* pHeaderSM, mfxU32 nHeaderSize, mfxU8* pDataBuffer, mfxU32 nDataSize);

protected:

    mfxStatus       ConstructSequenceHeaderSM   (IMediaSample* pSample);
    bool            StartCodeExist              (mfxU8* pStart);

protected:

    mfxVideoParam*  m_pVideoParam;
};

//For AVC1 support

typedef enum
{
    NALU_TYPE_SLICE    = 1,
    NALU_TYPE_DPA      = 2,
    NALU_TYPE_DPB      = 3,
    NALU_TYPE_DPC      = 4,
    NALU_TYPE_IDR      = 5,
    NALU_TYPE_SEI      = 6,
    NALU_TYPE_SPS      = 7,
    NALU_TYPE_PPS      = 8,
    NALU_TYPE_AUD      = 9,
    NALU_TYPE_EOSEQ    = 10,
    NALU_TYPE_EOSTREAM = 11,
    NALU_TYPE_FILL     = 12
} NALU_TYPE;


class CAVCFrameConstructor : public CFrameConstructor
{
public:
    CAVCFrameConstructor();
    ~CAVCFrameConstructor();

    virtual mfxStatus ConstructFrame(IMediaSample* pSample, mfxBitstream *pBS);

    mfxStatus ReadAVCHeader( MPEG2VIDEOINFO *pMPEG2VidInfo,  mfxBitstream  *pBS );

    virtual void Reset() { m_bInsertHeaders = true; };

private:
    bool         m_bInsertHeaders;
    mfxU32       m_NalSize;
    mfxU32       m_HeaderNalSize;

    mfxBitstream m_Headers;
    mfxBitstream m_StartCodeBS;
};

class StartCodeIteratorMP4
{
public:

    StartCodeIteratorMP4() :m_nCurPos(0), m_nNALStartPos(0), m_nNALDataPos(0), m_nNextRTP(0){};


    void Init(BYTE * pBuffer, int nSize, int nNALSize)
    {
         m_pBuffer = pBuffer;
         m_nSize = nSize;
         m_nNALSize = nNALSize;

    }

    NALU_TYPE    GetType()        const { return m_nal_type; };
    int          GetDataLength()    const { return m_nCurPos - m_nNALDataPos; };
    BYTE*        GetDataBuffer() { return m_pBuffer + m_nNALDataPos; };

    bool ReadNext()
    {
        mfxU32 nTemp;
        if (m_nCurPos >= m_nSize) return false;
        if ((m_nNALSize != 0) && (m_nCurPos == m_nNextRTP))
        {

            m_nNALStartPos   = m_nCurPos;
            m_nNALDataPos    = m_nCurPos + m_nNALSize;
            nTemp            = 0;
            for (mfxU32 i=0; i<m_nNALSize; i++)
            {
                nTemp = (nTemp << 8) + m_pBuffer[m_nCurPos++];
            }
            // check possible overflow
            if (nTemp      >= std::numeric_limits<mfxU32>::max() - m_nNextRTP) return false;
            if (m_nNALSize >= std::numeric_limits<mfxU32>::max() - m_nNextRTP - nTemp) return false;
            m_nNextRTP += nTemp + m_nNALSize;
            MoveToNextStartcode();
        }
        else
        {

            while (m_pBuffer[m_nCurPos]==0x00 && ((*((DWORD*)(m_pBuffer+m_nCurPos)) & 0x00FFFFFF) != 0x00010000))
                m_nCurPos++;

            m_nNALStartPos    = m_nCurPos;
            m_nCurPos       += 3;
            m_nNALDataPos    = m_nCurPos;
            MoveToNextStartcode();
        }
        m_nal_type    = (NALU_TYPE) (m_pBuffer[m_nNALDataPos] & 0x1f);

        return true;
    }

private:
    mfxU32        m_nNALStartPos;
    mfxU32        m_nNALDataPos;
    mfxU32        m_nDatalen;
    mfxU32        m_nCurPos;
    mfxU32        m_nNextRTP;
    mfxU32        m_nSize;
    mfxU32        m_nNALSize;
    NALU_TYPE    m_nal_type;
    BYTE *        m_pBuffer;

    bool MoveToNextStartcode()
    {
        mfxU32        nBuffEnd = (m_nNextRTP > 0) ? std::min (m_nNextRTP, m_nSize-4) : m_nSize-4;

        for (mfxU32 i=m_nCurPos; i<nBuffEnd; i++)
        {
            if ((*((DWORD*)(m_pBuffer+i)) & 0x00FFFFFF) == 0x00010000)
            {
            // Find next AnnexB Nal
            m_nCurPos = i;
            return true;
            }
        }

        if ((m_nNALSize != 0) && (m_nNextRTP < m_nSize))
        {
            m_nCurPos = m_nNextRTP;
            return true;
        }

        m_nCurPos = m_nSize;
        return false;
    }
};
