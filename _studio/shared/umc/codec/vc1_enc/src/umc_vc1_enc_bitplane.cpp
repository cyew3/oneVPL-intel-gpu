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

#include "umc_vc1_enc_bitplane.h"

namespace UMC_VC1_ENCODER
{
  UMC::Status VC1EncoderBitplane::Init(Ipp16u width, Ipp16u height)
  {
      Close();
      m_uiHeight    = height;
      m_uiWidth     = width;
      pBitplane = new bool[height*width];
      if (!pBitplane)
          return UMC::UMC_ERR_ALLOC;
      return UMC::UMC_OK;
  }
  void VC1EncoderBitplane::Close()
  {
      if (pBitplane)
      {
          delete [] pBitplane;
          pBitplane = 0;
      }
      m_uiHeight    = 0;
      m_uiWidth     = 0;
  }

  UMC::Status VC1EncoderBitplane::SetValue(bool value, Ipp16u x, Ipp16u y)
  {
    if (x >= m_uiWidth || y >= m_uiHeight)
        return UMC::UMC_ERR_FAILED;
       pBitplane[ x + y*m_uiWidth] = value;
    return UMC::UMC_OK;
  }

  Ipp8u VC1EncoderBitplane::Get2x3Normal(Ipp16u x,Ipp16u y)
  {
      return (pBitplane[x + (y+0)*m_uiWidth]<<0)|(pBitplane[x+1+ (y+0)*m_uiWidth]<<1)|
             (pBitplane[x + (y+1)*m_uiWidth]<<2)|(pBitplane[x+1+ (y+1)*m_uiWidth]<<3)|
             (pBitplane[x + (y+2)*m_uiWidth]<<4)|(pBitplane[x+1+ (y+2)*m_uiWidth]<<5);
  }
  Ipp8u VC1EncoderBitplane::Get3x2Normal(Ipp16u x,Ipp16u y)
  {
      return (pBitplane[x + (y+0)*m_uiWidth]<<0) |(pBitplane[x+1+ (y+0)*m_uiWidth]<<1) |(pBitplane[x+2+ (y+0)*m_uiWidth]<<2)|
             (pBitplane[x + (y+1)*m_uiWidth]<<3) |(pBitplane[x+1+ (y+1)*m_uiWidth]<<4) |(pBitplane[x+2+ (y+1)*m_uiWidth]<<5);
  }
}
#endif // defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
