//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2012 Intel Corporation. All Rights Reserved.
//

#ifndef __MEMBUFFOUT_H__
#define __MEMBUFFOUT_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER) || defined (UMC_ENABLE_MJPEG_VIDEO_ENCODER)
#include <stdio.h>
#ifndef __BASESTREAM_H__
#include "basestream.h"
#endif
#ifndef __BASESTREAMOUT_H__
#include "basestreamout.h"
#endif


class CMemBuffOutput : public CBaseStreamOutput
{
public:
  CMemBuffOutput(void);
  ~CMemBuffOutput(void);

  JERRCODE Open(Ipp8u* pBuf, int buflen);
  JERRCODE Close(void);

  JERRCODE Write(void* buf,uic_size_t len,uic_size_t* cnt);

  size_t GetPosition() { return (size_t)m_currpos; }

private:
  JERRCODE Open(vm_char* name) { name; return JPEG_NOT_IMPLEMENTED; }

protected:
  Ipp8u*  m_buf;
  int     m_buflen;
  int     m_currpos;

};

#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER || UMC_ENABLE_MJPEG_VIDEO_ENCODER
#endif // __MEMBUFFOUT_H__

