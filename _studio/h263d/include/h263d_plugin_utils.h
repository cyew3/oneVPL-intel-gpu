//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#pragma once

#include <stdio.h>

namespace MFX_H263DEC
{

struct h263_bitstream {
  h263_bitstream():
    buffer(NULL),
    bufptr(NULL),
    buflen(0)
  {}

  mfxU8* buffer;
  mfxU8* bufptr;
  mfxU32 buflen;

  inline mfxI64 remain() {
    return (mfxI64)(buflen - (bufptr - buffer));
  }
  mfxU8* findStartCodePtr()
  {
    mfxI64 i, len = remain();
    mfxU8* ptr = bufptr;
    mfxU8 mask = 0xFC;

    for (i = 0; i < len - 3; ++i) {
      if (ptr[i] == 0 && ptr[i + 1] == 0 && (((ptr[i + 2] & 0xFC) == 0x80) ||
                                             ((ptr[i + 2] & 0xF8) == 0x80))) {
        return ptr + i;
      }
    }
    return NULL;
  }
  inline mfxU32 getSkip()
  {
    if (!buflen) return 0;
    if (buffer[buflen-1]) return buflen;
    if (!(buflen-1)) return 0;
    if (buffer[buflen-2]) return buflen-1;
    return buflen-2;
  }
};

} // namespace MFX_H263DEC
