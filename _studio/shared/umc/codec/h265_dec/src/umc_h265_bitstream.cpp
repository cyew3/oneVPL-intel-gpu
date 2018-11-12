// Copyright (c) 2012-2018 Intel Corporation
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
#ifdef UMC_ENABLE_H265_VIDEO_DECODER
#ifndef MFX_VA

#include "vm_debug.h"
#include "umc_h265_bitstream.h"
#include "umc_h265_bitstream_inlines.h"
#include "umc_h265_slice_decoding.h"
#include "umc_h265_headers.h"

namespace UMC_HEVC_DECODER
{

#if INSTRUMENTED_CABAC
    uint32_t H265Bitstream::m_c = 0;
    FILE* H265Bitstream::cabac_bits = 0;
#endif

H265Bitstream::H265Bitstream(uint8_t * const pb, const uint32_t maxsize)
     : H265HeadersBitstream(pb, maxsize)
{
    m_bitsNeeded = 0;
    m_lcodIOffset = 0;
    m_lcodIRange = 0;
    m_LastByte = 0;
} // H265Bitstream::H265Bitstream(uint8_t * const pb,

H265Bitstream::H265Bitstream()
    : H265HeadersBitstream()
    , m_lcodIRange(0)
    , m_lcodIOffset(0)
    , m_bitsNeeded(0)
    , m_LastByte(0)
{
#if INSTRUMENTED_CABAC
    if (!cabac_bits)
    {
        cabac_bits=fopen(__CABAC_FILE__,"w+t");
        m_c = 0;
    }
#endif
} // H265Bitstream::H265Bitstream(void)

H265Bitstream::~H265Bitstream()
{
#if INSTRUMENTED_CABAC
    fflush(cabac_bits);
#endif
} // H265Bitstream::~H265Bitstream()

} // namespace UMC_HEVC_DECODER
#endif // #ifndef MFX_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER
