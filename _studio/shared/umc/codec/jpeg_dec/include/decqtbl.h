// Copyright (c) 2001-2018 Intel Corporation
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

#ifndef __DECQTBL_H__
#define __DECQTBL_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER)
#include "ippj.h"
#include "jpegbase.h"

#ifndef OPEN_SOURCE
#define ALLOW_JPEG_SW_FALLBACK
#endif


class CJPEGDecoderQuantTable
{
private:
  Ipp8u   m_rbf[DCTSIZE2*sizeof(Ipp16u)+(CPU_CACHE_LINE-1)];
#ifdef ALLOW_JPEG_SW_FALLBACK
  Ipp8u   m_qbf[DCTSIZE2*sizeof(Ipp32f)+(CPU_CACHE_LINE-1)];
  Ipp16u* m_qnt16u;
  Ipp32f* m_qnt32f;
#endif

public:
  int     m_id;
  int     m_precision;
  int     m_initialized;
  Ipp8u*  m_raw8u;
  Ipp16u* m_raw16u;

  CJPEGDecoderQuantTable(void);
  virtual ~CJPEGDecoderQuantTable(void);

  JERRCODE Init(int id,Ipp8u  raw[DCTSIZE2]);

  JERRCODE Init(int id,Ipp16u raw[DCTSIZE2]);
#ifdef ALLOW_JPEG_SW_FALLBACK
  JERRCODE ConvertToLowPrecision(void);
  JERRCODE ConvertToHighPrecision(void);

  operator Ipp16u*() { return m_precision == 0 ? m_qnt16u : 0; }
  operator Ipp32f*() { return m_precision == 1 ? m_qnt32f : 0; }
#endif
};


#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER
#endif // __DECQTBL_H__
