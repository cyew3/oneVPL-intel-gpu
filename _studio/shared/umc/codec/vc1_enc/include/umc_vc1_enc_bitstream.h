// Copyright (c) 2008-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef _ENCODER_VC1_BITSTREAM_H_
#define _ENCODER_VC1_BITSTREAM_H_

#include "vm_types.h"
#include "umc_media_data.h"
#include "ippvc.h"
#include "vm_debug.h"
#include "assert.h"
#include "umc_vc1_enc_statistic.h"

namespace UMC_VC1_ENCODER
{

#ifdef _BIG_ENDIAN_
#define BSWAP(x) (x)
#define BNOSWAP(x) (unsigned int)(((x) << 24) + (((x)&0xff00) << 8) + (((x) >> 8)&0xff00) + ((x) >> 24))
#else
#define BNOSWAP(x) (x)
#define BSWAP(x) (unsigned int)(((x) << 24) + (((x)&0xff00) << 8) + (((x) >> 8)&0xff00) + ((x) >> 24))
#endif


class VC1EncoderBitStreamSM
{
private:
    uint32_t*      m_pBitStream; /*  pointer to bitstream        */
    int32_t       m_iOffset;    /*  bit offset [0,32]           */

    uint32_t*      m_pBufferStart;
    size_t       m_iBufferLen;

    double       m_dPTS;

    uint32_t*      m_pBlankSegment;
    size_t       m_iBlankSegmentLen;

public:

    VC1EncoderBitStreamSM():
        m_pBlankSegment(0),
        m_iBlankSegmentLen(0),
        m_pBitStream(0),
        m_iOffset(32),
        m_pBufferStart (0),
        m_iBufferLen(0),
        m_dPTS(0)
    {}

    ~VC1EncoderBitStreamSM(){}

    void            Init(UMC::MediaData* data);

    UMC::Status     DataComplete(UMC::MediaData* data);

    UMC::Status     AddLastBits();
    UMC::Status     PutBits         (uint32_t val,int32_t len);
    UMC::Status     PutBitsHeader   (uint32_t val,int32_t len);


    UMC::Status     MakeBlankSegment(int32_t len);
    UMC::Status     FillBlankSegment(uint32_t value);
    void            DeleteBlankSegment();
    uint32_t          GetDataLen()
    {
        return ((uint32_t)((uint8_t*)m_pBitStream - (uint8_t*)m_pBufferStart)) + (32-m_iOffset+7)/8;
    }
#ifdef VC1_ME_MB_STATICTICS
public:
#endif
    uint32_t          GetCurrBit ()
    {
        return (((uint32_t)((uint8_t*)m_pBitStream - (uint8_t*)m_pBufferStart))<<3)+ (32 - m_iOffset);
    }
protected:
    void            Reset();
};

class VC1EncoderBitStreamAdv
{
private:
    uint32_t*      m_pBitStream; /*  pointer to bitstream        */
    int32_t       m_iOffset;    /*  bit offset [0,32]           */

    uint32_t*      m_pBufferStart;
    size_t       m_iBufferLen;

    double       m_dPTS;
    uint8_t        m_uiCodeStatus;
    bool         m_bLast;

public:

    VC1EncoderBitStreamAdv():
        m_pBitStream(0),
        m_iOffset(32),
        m_pBufferStart (0),
        m_iBufferLen(0),
        m_dPTS(0),
        m_uiCodeStatus(0),
        m_bLast(false)
    {}

    ~VC1EncoderBitStreamAdv(){}

    void            Init(UMC::MediaData* data);
    UMC::Status     DataComplete(UMC::MediaData* data);

    UMC::Status     AddLastBits();
    UMC::Status     PutBits      (uint32_t val,int32_t len);
    UMC::Status     PutStartCode (uint32_t val,int32_t len=32);

    uint32_t          GetDataLen()
    {
        assert (m_iOffset==32);
        return ((uint32_t)((uint8_t*)m_pBitStream - (uint8_t*)m_pBufferStart));
    }
    UMC::Status     AddUserData(uint8_t* pUD, uint32_t size, uint32_t startCode);

protected:
    void           Reset();
    int32_t         CheckCode(uint32_t code);
    void           ResetCodeStatus() {m_uiCodeStatus = 0;}
    UMC::Status    PutLastBits();

#ifdef VC1_ME_MB_STATICTICS
public:
#endif
    uint32_t         GetCurrBit ()
    {
        return (((uint32_t)((uint8_t*)m_pBitStream - (uint8_t*)m_pBufferStart))<<3)+ (32 - m_iOffset);
    }
};
}
#endif
#endif //defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
