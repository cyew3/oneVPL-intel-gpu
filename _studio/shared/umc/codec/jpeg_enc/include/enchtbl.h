//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2001-2012 Intel Corporation. All Rights Reserved.
//

#ifndef __ENCHTBL_H__
#define __ENCHTBL_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_ENCODER)

#include "ippj.h"
#include "jpegbase.h"

class CJPEGEncoderHuffmanTable
{
private:
  IppiEncodeHuffmanSpec* m_table;
  bool                   m_bValid;

public:
  int                    m_id;
  int                    m_hclass;
  Ipp8u                  m_bits[16];
  Ipp8u                  m_vals[256];

  CJPEGEncoderHuffmanTable(void);
  virtual ~CJPEGEncoderHuffmanTable(void);

  JERRCODE Create(void);
  JERRCODE Destroy(void);
  JERRCODE Init(int id,int hclass,Ipp8u* bits,Ipp8u* vals);
  
  bool     IsValid(void)                { return m_bValid; }

  operator IppiEncodeHuffmanSpec*(void) { return m_table; }
};


class CJPEGEncoderHuffmanState
{
private:
  IppiEncodeHuffmanState* m_state;

public:
  CJPEGEncoderHuffmanState(void);
  virtual ~CJPEGEncoderHuffmanState(void);

  JERRCODE Create(void);
  JERRCODE Destroy(void);
  JERRCODE Init(void);

  operator IppiEncodeHuffmanState*(void) { return m_state; }
};

#endif
#endif // __ENCHTBL_H__
