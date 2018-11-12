// Copyright (c) 2014-2018 Intel Corporation
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
