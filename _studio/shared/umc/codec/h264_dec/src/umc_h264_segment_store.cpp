// Copyright (c) 2003-2019 Intel Corporation
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
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#include "umc_h264_dec_defs_dec.h"
#include "umc_structures.h"
#include "umc_h264_dec_structures.h"

namespace UMC
{

const UMC::H264DecoderMotionVector zeroVector = {0, 0};

H264DecoderLocalMacroblockDescriptor::H264DecoderLocalMacroblockDescriptor(void)
{
    MVDeltas[0] =
    MVDeltas[1] = NULL;
    MacroblockCoeffsInfo = NULL;
    mbs = NULL;
    active_next_mb_table = NULL;

    m_pAllocated = NULL;
    m_nAllocatedSize = 0;

    m_midAllocated = 0;
    m_pMemoryAllocator = NULL;

    m_isBusy = false;
    m_pFrame = 0;

} // H264DecoderLocalMacroblockDescriptor::H264DecoderLocalMacroblockDescriptor(void)

H264DecoderLocalMacroblockDescriptor::~H264DecoderLocalMacroblockDescriptor(void)
{
    Release();

} // H264DecoderLocalMacroblockDescriptor::~H264DecoderLocalMacroblockDescriptor(void)

void H264DecoderLocalMacroblockDescriptor::Release(void)
{
    if (m_midAllocated)
    {
        m_pMemoryAllocator->Unlock(m_midAllocated);
        m_pMemoryAllocator->Free(m_midAllocated);
    }

    MVDeltas[0] =
    MVDeltas[1] = NULL;
    MacroblockCoeffsInfo = NULL;
    mbs = NULL;
    active_next_mb_table = NULL;

    m_pAllocated = NULL;
    m_nAllocatedSize = 0;

    m_midAllocated = 0;
} // void H264DecoderLocalMacroblockDescriptor::Release(void)

bool H264DecoderLocalMacroblockDescriptor::Allocate(int32_t iMBCount, MemoryAllocator *pMemoryAllocator)
{
    // check error(s)
    if (NULL == pMemoryAllocator)
        return false;

    // allocate buffer
    size_t nSize = (sizeof(H264DecoderMacroblockMVs) +
                    sizeof(H264DecoderMacroblockMVs) +
                    sizeof(H264DecoderMacroblockCoeffsInfo) +
                    sizeof(H264DecoderMacroblockLocalInfo)) * iMBCount + 16 * 6;

    if ((NULL == m_pAllocated) ||
        (m_nAllocatedSize < nSize))
    {
        // release object before initialization
        Release();

        m_pMemoryAllocator = pMemoryAllocator;
        if (UMC_OK != m_pMemoryAllocator->Alloc(&m_midAllocated,
                                                nSize,
                                                UMC_ALLOC_PERSISTENT))
            return false;
        m_pAllocated = (uint8_t *) m_pMemoryAllocator->Lock(m_midAllocated);

        ippsZero_8u(m_pAllocated, (int32_t)nSize);
        m_nAllocatedSize = nSize;
    }

    // reset pointer(s)
    const int ALIGN_VALUE = 16;
    MVDeltas[0] = align_pointer<H264DecoderMacroblockMVs *> (m_pAllocated, ALIGN_VALUE);
    MVDeltas[1] = align_pointer<H264DecoderMacroblockMVs *> (MVDeltas[0] + iMBCount, ALIGN_VALUE);
    MacroblockCoeffsInfo = align_pointer<H264DecoderMacroblockCoeffsInfo *> (MVDeltas[1] + iMBCount, ALIGN_VALUE);
    mbs = align_pointer<H264DecoderMacroblockLocalInfo *> (MacroblockCoeffsInfo + iMBCount, ALIGN_VALUE);

    return true;

} // bool H264DecoderLocalMacroblockDescriptor::Allocate(int32_t iMBCount)

H264DecoderLocalMacroblockDescriptor &H264DecoderLocalMacroblockDescriptor::operator = (H264DecoderLocalMacroblockDescriptor &Desc)
{
    MVDeltas[0] = Desc.MVDeltas[0];
    MVDeltas[1] = Desc.MVDeltas[1];
    MacroblockCoeffsInfo = Desc.MacroblockCoeffsInfo;
    mbs = Desc.mbs;
    active_next_mb_table = Desc.active_next_mb_table;

    return *this;

} // H264DecoderLocalMacroblockDescriptor &H264DecoderLocalMacroblockDescriptor::operator = (H264DecoderLocalMacroblockDescriptor &Dest)

} // namespace UMC
#endif // MFX_ENABLE_H264_VIDEO_DECODE
