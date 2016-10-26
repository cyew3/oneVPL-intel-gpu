//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2001-2010 Intel Corporation. All Rights Reserved.
//

#ifndef __DECHTBL_H__
#define __DECHTBL_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER)
#ifndef __IPPJ_H__
#include "ippj.h"
#endif
#ifndef __JPEGBASE_H__
#include "jpegbase.h"
#endif




class CJPEGDecoderHuffmanTable
{
private:
  IppiDecodeHuffmanSpec* m_table;
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
  operator IppiDecodeHuffmanSpec*(void) { return m_table; }

  const Ipp8u*   GetBits() const        { return m_bits; }
  const Ipp8u*   GetValues() const      { return m_vals; }
};


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

#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER
#endif // __DECHTBL_H__
