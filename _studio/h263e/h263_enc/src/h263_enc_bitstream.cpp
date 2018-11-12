// Copyright (c) 2005-2018 Intel Corporation
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

#include "umc_defs.h"

#include "h263_enc_bitstream.hpp"
#include "ippdefs.h"

#if /*(_WIN32_WCE == 500) && */(_MSC_VER == 1201)
// INTERNAL COMPILER ERROR
#pragma optimize ("g", off)
#endif

void H263BitStream::PutBits(Ipp32u val, Ipp32s n)
{
    val <<= 32 - n;
    if (mBitOff == 0) {
        mPtr[0] = (Ipp8u)(val >> 24);
        if (n > 8) {
            mPtr[1] = (Ipp8u)(val >> 16);
            if (n > 16) {
                mPtr[2] = (Ipp8u)(val >> 8);
                if (n > 24) {
                    mPtr[3] = (Ipp8u)(val);
                }
            }
        }
    } else {
        mPtr[0] = (Ipp8u)((mPtr[0] & (0xFF << (8 - mBitOff))) | (Ipp8u)(val >> (24 + mBitOff)));
        if (n > 8 - mBitOff) {
            val <<= 8 - mBitOff;
            mPtr[1] = (Ipp8u)(val >> 24);
            if (n > 16 - mBitOff) {
                mPtr[2] = (Ipp8u)(val >> 16);
                if (n > 24 - mBitOff) {
                    mPtr[3] = (Ipp8u)(val >> 8);
                    if (n > 32 - mBitOff) {
                        mPtr[4] = (Ipp8u)val;
                    }
                }
            }
        }
    }
    mPtr += (mBitOff + n) >> 3;
    mBitOff = (mBitOff + n) & 7;
}
