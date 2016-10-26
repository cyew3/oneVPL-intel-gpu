//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2001-2012 Intel Corporation. All Rights Reserved.
//

#ifndef __ENCQTBL_H__
#define __ENCQTBL_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_ENCODER)

#include "ippj.h"
#include "jpegbase.h"

class CJPEGEncoderQuantTable
{
private:
  Ipp8u   m_rbf[DCTSIZE2*sizeof(Ipp16u)+(CPU_CACHE_LINE-1)];
  Ipp8u   m_qbf[DCTSIZE2*sizeof(Ipp32f)+(CPU_CACHE_LINE-1)];
  Ipp16u* m_qnt16u;
  Ipp32f* m_qnt32f;

public:
  int     m_id;
  bool    m_initialized;
  int     m_precision;
  Ipp8u*  m_raw8u;
  Ipp16u* m_raw16u;

  CJPEGEncoderQuantTable(void);
  virtual ~CJPEGEncoderQuantTable(void);

  JERRCODE Init(int id,Ipp8u  raw[DCTSIZE2],int quality);
  JERRCODE Init(int id,Ipp16u raw[DCTSIZE2],int quality);

  operator Ipp16u*() { return m_precision == 0 ? m_qnt16u : 0; }
  operator Ipp32f*() { return m_precision == 1 ? m_qnt32f : 0; }
};

#endif
#endif // __ENCQTBL_H__
