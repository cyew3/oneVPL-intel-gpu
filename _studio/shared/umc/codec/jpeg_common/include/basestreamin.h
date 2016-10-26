//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2005-2012 Intel Corporation. All Rights Reserved.
//

#ifndef __BASESTREAMIN_H__
#define __BASESTREAMIN_H__

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER)
#include "jpegbase.h"
#include "basestream.h"


class CBaseStreamInput : public CBaseStream
{
public:
  CBaseStreamInput(void) {}
  ~CBaseStreamInput(void) {}

  virtual JERRCODE Seek(long offset, int origin) = 0;
  virtual JERRCODE Read(void* buf,uic_size_t len,uic_size_t* cnt) = 0;
  virtual JERRCODE TellPos(size_t* pos) = 0;
};

#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER
#endif // __BASESTREAMIN_H__

