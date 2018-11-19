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

#include "umc_vc1_enc_bitstream.h"
namespace UMC_VC1_ENCODER
{
void VC1EncoderBitStreamSM::Init(UMC::MediaData* data)
{
    m_pBitStream    = (uint32_t*) ((uint8_t*)data->GetBufferPointer() + data->GetDataSize());
    m_iOffset       = 32;
    m_pBitStream[0] = 0;

    m_pBufferStart  = m_pBitStream;
    m_iBufferLen    = data->GetBufferSize() - data->GetDataSize();

    m_pBlankSegment     = 0;
    m_iBlankSegmentLen  = 0;

    m_dPTS          = data->GetTime();
}
void VC1EncoderBitStreamAdv::Init(UMC::MediaData* data)
{
    m_pBitStream    = (uint32_t*) ((uint8_t*)data->GetBufferPointer() + data->GetDataSize());
    m_iOffset       = 32;
    m_pBitStream[0] = 0;

    m_pBufferStart  = m_pBitStream;
    m_iBufferLen    = data->GetBufferSize() - data->GetDataSize();

    m_dPTS          = data->GetTime();

    ResetCodeStatus();

    m_bLast         = false;
}
void  VC1EncoderBitStreamSM::Reset()
{
    m_pBitStream        = 0;
    m_pBufferStart      = 0;
    m_iBufferLen        = 0;
    m_iOffset           = 32;
    m_dPTS              = -1;
    m_pBlankSegment     = 0;
    m_iBlankSegmentLen  = 0;
}
void  VC1EncoderBitStreamAdv::Reset()
{
    m_pBitStream        = 0;
    m_pBufferStart      = 0;
    m_iBufferLen        = 0;
    m_iOffset           = 32;
    m_dPTS              = -1;
    m_bLast             = false;
}

UMC::Status VC1EncoderBitStreamSM::DataComplete(UMC::MediaData* data)
{
    size_t  dataLen = 0;

    dataLen = GetDataLen();
    VM_ASSERT (dataLen < m_iBufferLen);

    UMC::Status ret = AddLastBits();
    if (ret != UMC::UMC_OK) return ret;

    ret = data->SetDataSize(data->GetDataSize() + dataLen);
    if (ret != UMC::UMC_OK) return ret;

    ret = data->SetTime(m_dPTS);
    if (ret != UMC::UMC_OK) return ret;

    Reset();

    return ret;
}
UMC::Status VC1EncoderBitStreamAdv::DataComplete(UMC::MediaData* data)
{
    size_t  dataLen = 0;

    UMC::Status ret = AddLastBits();
    if (ret != UMC::UMC_OK) return ret;

    dataLen = (m_pBitStream - m_pBufferStart)*sizeof(uint32_t);
    VM_ASSERT (dataLen < m_iBufferLen);

    ret = data->SetDataSize(data->GetDataSize() + dataLen);
    if (ret != UMC::UMC_OK) return ret;

    ret = data->SetTime(m_dPTS);
    if (ret != UMC::UMC_OK) return ret;

    Reset();

    return ret;
}

//UMC::Status VC1EncoderBitStreamSM::AddLastBits8u()
//{
//    UMC::Status ret = UMC::UMC_OK;
//    uint8_t bits = m_iOffset%8;
//    if (bits!=0 )
//    {
//        ret =PutBits((1<<(bits-1)),bits);
//    }
//    return ret;
//}
UMC::Status VC1EncoderBitStreamSM::AddLastBits()
{
    UMC::Status ret = UMC::UMC_OK;
    if (m_iOffset!=32 )
    {
        ret =PutBits((1<<(m_iOffset-1)),m_iOffset);
    }
    return ret;
}
UMC::Status VC1EncoderBitStreamAdv::AddLastBits()
{
    UMC::Status ret = UMC::UMC_OK;
    assert (m_iOffset!=0);
    if (!m_bLast)
    {
        uint8_t z = (uint8_t)(m_iOffset%8);
        z = (z)? z : 8;
        ret = PutBits((1<<(z-1)),z);
        if (ret != UMC::UMC_OK) return ret;
        ret = PutLastBits();
    }
    m_bLast = true;
    return ret;
}
static uint32_t mask[] = {
                   0x00000000,
                   0x00000001,0x00000003,0x00000007,0x0000000F,
                   0x0000001F,0x0000003F,0x0000007F,0x000000FF,
                   0x000001FF,0x000003FF,0x000007FF,0x00000FFF,
                   0x00001FFF,0x00003FFF,0x00007FFF,0x0000FFFF,
                   0x0001FFFF,0x0003FFFF,0x0007FFFF,0x000FFFFF,
                   0x001FFFFF,0x003FFFFF,0x007FFFFF,0x00FFFFFF,
                   0x01FFFFFF,0x03FFFFFF,0x07FFFFFF,0x0FFFFFFF,
                   0x1FFFFFFF,0x3FFFFFFF,0x7FFFFFFF,0xFFFFFFFF
};

UMC::Status VC1EncoderBitStreamSM::PutBits(uint32_t val,int32_t len)
 {
    int32_t tmpcnt;
    uint32_t r_tmp;

    assert(m_pBitStream!=NULL);
    assert(len<=32);

    if ((uint8_t*)m_pBitStream + (len + m_iOffset)/8 >= (uint8_t*)m_pBufferStart + m_iBufferLen - 1)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

    val = val & mask[len];

    tmpcnt = (m_iOffset) - (len);

    if(tmpcnt <= 0)
    {
      uint32_t z=0;
      r_tmp = (m_pBitStream)[0] | ((val) >> (-tmpcnt));
      (m_pBitStream)[0] = BSWAP(r_tmp);
      (m_pBitStream)++;
      z = (tmpcnt<0)? (val << (32 + tmpcnt)):z;
      (m_pBitStream)[0] = z ;
      (m_iOffset) = 32 + tmpcnt;
    }
    else
    {
      (m_pBitStream)[0] |= (val) << tmpcnt;
      m_iOffset = tmpcnt;
    }
    return UMC::UMC_OK;
}
 UMC::Status VC1EncoderBitStreamSM::PutBitsHeader(uint32_t val,int32_t len)
 {
    int32_t tmpcnt;
    uint32_t r_tmp;

    assert(m_pBitStream!=NULL);
    assert(len<=32);

    if ((uint8_t*)m_pBitStream + (len + m_iOffset)/8 >= (uint8_t*)m_pBufferStart + m_iBufferLen - 1)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

    val = val & mask[len];

    tmpcnt = (m_iOffset) - (len);

    if(tmpcnt <= 0)
    {
      uint32_t z=0;
      r_tmp = (m_pBitStream)[0] | ((val) >> (-tmpcnt));
      (m_pBitStream)[0] = BNOSWAP(r_tmp);
      (m_pBitStream)++;
      z = (tmpcnt<0)? (val << (32 + tmpcnt)):z;
      (m_pBitStream)[0] = z ;
      (m_iOffset) = 32 + tmpcnt;
    }
    else
    {
      (m_pBitStream)[0] |= (val) << tmpcnt;
      m_iOffset = tmpcnt;
    }
    return UMC::UMC_OK;
}
static uint32_t maskUpper[] = {0x00000000, 0xFF000000, 0xFFFF0000,0xFFFFFF00};
static uint32_t maskLower[] = {0xFFFFFFFF, 0x00FFFFFF, 0x0000FFFF,0x000000FF};
static uint32_t maskL[] = {0x00000000, 0x000000FF, 0x0000FFFF,0x00000000};
static uint32_t iArr[] = {0x03000000,0x030000,0x0300,0x03};

UMC::Status VC1EncoderBitStreamAdv::PutBits(uint32_t val,int32_t len)
 {
    int32_t tmpcnt;

    assert(m_pBitStream!=NULL);
    assert(len<=32);

    if ((uint8_t*)m_pBitStream + (len + m_iOffset)/8 >= (uint8_t*)m_pBufferStart + m_iBufferLen - 1)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

    val = val & mask[len];
    tmpcnt = (m_iOffset) - (len);

    while (tmpcnt <= 0)
    {
      int32_t i=-1;
      uint8_t  n_bytes  = 0;
      uint32_t r_tmp = 0;
      uint32_t tmp = 0;

      r_tmp = (m_pBitStream)[0] | ((val) >> (-tmpcnt));
      tmp = r_tmp;

      len = len -  m_iOffset;
      i = 0;
      while ((i = CheckCode(tmp | maskUpper[i]))>=0)
      {
         n_bytes ++;
         tmp             = (tmp & maskUpper[i]) | (iArr[i])| ((tmp & maskLower[i])>>8);
      }
      m_pBitStream[0]   =  BSWAP(tmp);
      m_pBitStream ++;

      m_iOffset         =  32 - n_bytes*8;
      m_pBitStream[0]   =  (r_tmp & maskL[n_bytes])<< m_iOffset;

      val = val & mask[len];
      tmpcnt = (m_iOffset) - (len);
    }
    if (len>0)
    {
       assert (tmpcnt>=0);
      (m_pBitStream)[0] |= (val) << tmpcnt;
       m_iOffset = tmpcnt;
    }
    return UMC::UMC_OK;
}
 UMC::Status VC1EncoderBitStreamAdv::PutLastBits()
 {
    int32_t pos = (32 - m_iOffset)>>3;
    int32_t i=-1;
    uint32_t tmp = (m_pBitStream)[0];

    assert(m_pBitStream!=NULL);
    assert(m_iOffset%8 == 0);

    if (m_iOffset == 32)
        return UMC::UMC_OK;

    i = CheckCode(tmp);
    if (i>=0 && i<= pos)
    {
        tmp  = (tmp & maskUpper[i]) | (0x03 <<((3-i)*8))| ((tmp & maskLower[i])>>8);
    }
    m_pBitStream[0]   =  BSWAP(tmp);
    m_pBitStream ++;
    m_pBitStream[0]   = 0;
    m_iOffset = 32;

    return UMC::UMC_OK;
}
 UMC::Status VC1EncoderBitStreamAdv::PutStartCode(uint32_t val, int32_t len)
 {
   int32_t tmpcnt;
   uint32_t r_tmp;

   assert(m_pBitStream!=NULL);
   assert(len<=32);
   assert(m_iOffset % 8 == 0);

    if ((uint8_t*)m_pBitStream + (len + m_iOffset)/8 >= (uint8_t*)m_pBufferStart + m_iBufferLen - 1)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

    val = val & mask[len];
    tmpcnt = (m_iOffset) - (len);

    if(tmpcnt <= 0)
    {
      uint32_t z=0;
      r_tmp = (m_pBitStream)[0] | ((val) >> (-tmpcnt));
      (m_pBitStream)[0] = BSWAP(r_tmp);
      (m_pBitStream)++;
      z = (tmpcnt<0)? (val << (32 + tmpcnt)):z;
      (m_pBitStream)[0] = z ;
      (m_iOffset) = 32 + tmpcnt;
    }
    else
    {
      (m_pBitStream)[0] |= (val) << tmpcnt;
      m_iOffset = tmpcnt;
    }
    m_bLast = false;
    ResetCodeStatus();
    return UMC::UMC_OK;
}
 UMC::Status     VC1EncoderBitStreamAdv::AddUserData(uint8_t* pUD, uint32_t len, uint32_t startCode)
 {
     assert(m_pBitStream!=NULL);
     uint32_t bytepos = (32 - m_iOffset)>>3;

     if ((uint8_t*)m_pBitStream + len + bytepos >= (uint8_t*)m_pBufferStart + m_iBufferLen - 1)
         return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
     
     PutStartCode(startCode);     
     memcpy ((uint8_t*)m_pBitStream + bytepos, pUD,len);

     m_pBitStream += (bytepos + len)/4;
     bytepos = (bytepos + len)%4;
     
     m_iOffset = 32 - bytepos*8;
     PutStartCode(0x80,8);
     return UMC::UMC_OK;

 }
static uint32_t iShift[] = {24,16,8,0};
int32_t       VC1EncoderBitStreamAdv::CheckCode(uint32_t code)
{
    int32_t i;

    for (i=0;i<4;i++)
    {
        uint8_t nextByte = (uint8_t)((code >>(iShift[i]))&0xFF);
        if (nextByte>3)
        {
            m_uiCodeStatus = 0;
        }
        else
        {
            if (m_uiCodeStatus <2)
            {
                m_uiCodeStatus = (nextByte == 0)?  m_uiCodeStatus + 1 : 0;
            }
            else
            {
                m_uiCodeStatus = 0;
                return i;
            }
        }
    }
    return -1;
}

 UMC::Status  VC1EncoderBitStreamSM::MakeBlankSegment(int32_t len)
 {
    if (m_iOffset != 32 || m_pBlankSegment)
         return UMC::UMC_ERR_NOT_IMPLEMENTED;

    if ((uint8_t*)(m_pBitStream + len) >= (uint8_t*)m_pBufferStart + m_iBufferLen - 1)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

    m_pBlankSegment    = m_pBitStream;
    m_iBlankSegmentLen = len;
    m_pBitStream      += len;

    m_pBitStream[0]=0;
    return UMC::UMC_OK;

 }
 UMC::Status  VC1EncoderBitStreamSM::FillBlankSegment(uint32_t value)
 {
     if (!m_pBlankSegment || !m_iBlankSegmentLen)
         return UMC::UMC_ERR_NOT_IMPLEMENTED;

     *m_pBlankSegment = BNOSWAP(value);
     m_pBlankSegment++;
     m_iBlankSegmentLen--;

     return UMC::UMC_OK;
 }
void VC1EncoderBitStreamSM::DeleteBlankSegment()
{
    m_pBlankSegment     = 0;
    m_iBlankSegmentLen  = 0;
}
}

#endif // defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
