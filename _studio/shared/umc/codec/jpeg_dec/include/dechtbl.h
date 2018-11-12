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

#ifndef __DECHTBL_H__
#define __DECHTBL_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER)
#include "ippj.h"
#include "jpegbase.h"

#ifndef OPEN_SOURCE
#define ALLOW_JPEG_SW_FALLBACK
#endif

class CJPEGDecoderHuffmanTable
{
private:
#ifdef ALLOW_JPEG_SW_FALLBACK
  IppiDecodeHuffmanSpec* m_table;
#endif

  Ipp8u                  m_bits[16];
  Ipp8u                  m_vals[256];
  bool                   m_bEmpty;
  bool                   m_bValid;

public:
  int                    m_id;
  int                    m_hclass;

  CJPEGDecoderHuffmanTable(void);
  virtual ~CJPEGDecoderHuffmanTable(void);

  JERRCODE Create(void);
  JERRCODE Destroy(void);

  JERRCODE Init(int id,int hclass,Ipp8u* bits,Ipp8u* vals);

  bool     IsEmpty(void)                { return m_bEmpty; }
  bool     IsValid(void)                { return m_bValid; }
  void     SetInvalid(void)             { m_bValid = 0; return; }

#ifdef ALLOW_JPEG_SW_FALLBACK
  operator IppiDecodeHuffmanSpec*(void) { return m_table; }
#endif

  const Ipp8u*   GetBits() const        { return m_bits; }
  const Ipp8u*   GetValues() const      { return m_vals; }
};


#ifdef ALLOW_JPEG_SW_FALLBACK
class CJPEGDecoderHuffmanState
{
private:
  IppiDecodeHuffmanState* m_state;

public:
  CJPEGDecoderHuffmanState(void);
  virtual ~CJPEGDecoderHuffmanState(void);

  JERRCODE Create(void);
  JERRCODE Destroy(void);

  JERRCODE Init(void);

  operator IppiDecodeHuffmanState*(void) { return m_state; }
};
#endif

#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER
#endif // __DECHTBL_H__
