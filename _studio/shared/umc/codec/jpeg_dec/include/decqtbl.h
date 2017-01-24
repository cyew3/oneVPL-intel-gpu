//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2001-2017 Intel Corporation. All Rights Reserved.
//

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
