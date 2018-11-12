// Copyright (c) 2003-2018 Intel Corporation
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
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_frame_ex.h"
#include "umc_h264_task_supplier.h"
#include "umc_h264_dec_debug.h"

namespace UMC
{

//////////////////////////////////////////////////////////////////////////////
//  H264DecoderFrameExtension class implementation
//////////////////////////////////////////////////////////////////////////////
H264DecoderFrameEx::H264DecoderFrameEx(MemoryAllocator *pMemoryAllocator, H264_Heap_Objects * pObjHeap)
    : H264DecoderFrame(pMemoryAllocator, pObjHeap)
    , m_mbinfo()
{
    m_pParsedFrameDataNew = 0;
    m_paddedParsedFrameDataSize.width = 0;
    m_paddedParsedFrameDataSize.height = 0;
}

H264DecoderFrameEx::~H264DecoderFrameEx()
{
    deallocateParsedFrameData();
}

void H264DecoderFrameEx::deallocateParsedFrameData()
{
    // new structure(s) hold pointer
    if (m_pParsedFrameDataNew)
    {
        // Free the old buffer.
        m_pObjHeap->Free(m_pParsedFrameDataNew);

        m_pParsedFrameDataNew = 0;

        m_mbinfo.MV[0] = 0;
        m_mbinfo.MV[1] = 0;
        m_mbinfo.mbs = 0;
    }

    m_paddedParsedFrameDataSize.width = 0;
    m_paddedParsedFrameDataSize.height = 0;
}    // deallocateParsedFrameData

size_t H264DecoderFrameEx::GetFrameDataSize(const mfxSize &lumaSize)
{
    size_t nMBCount = (lumaSize.width >> 4) * (lumaSize.height >> 4) + 2;

    // allocate buffer
    size_t nMemSize = (sizeof(H264DecoderMacroblockMVs) +
                       sizeof(H264DecoderMacroblockMVs) +
                       sizeof(H264DecoderMacroblockGlobalInfo)) * nMBCount + 16 * 5;

    return nMemSize;
}

Status H264DecoderFrameEx::allocateParsedFrameData()
{
    Status      umcRes = UMC_OK;

    // If our buffer and internal pointers are already set up for this
    // image size, then there's nothing more to do.

    if (!m_pParsedFrameDataNew || m_paddedParsedFrameDataSize.width != m_lumaSize.width ||
        m_paddedParsedFrameDataSize.height != m_lumaSize.height)
    {
        if (m_pParsedFrameDataNew)
        {
            deallocateParsedFrameData();
        }

        // allocate new MB structure(s)
        size_t nMBCount = (m_lumaSize.width >> 4) * (m_lumaSize.height >> 4) + 2;

        // allocate buffer
        size_t nMemSize = GetFrameDataSize(m_lumaSize);

        m_pParsedFrameDataNew = m_pObjHeap->Allocate<uint8_t>(nMemSize);

        const int ALIGN_VALUE = 16;

        // set pointer(s)
        m_mbinfo.MV[0] = align_pointer<H264DecoderMacroblockMVs *> (m_pParsedFrameDataNew, ALIGN_VALUE);
        m_mbinfo.MV[1] = align_pointer<H264DecoderMacroblockMVs *> (m_mbinfo.MV[0]+ nMBCount, ALIGN_VALUE);
        m_mbinfo.mbs = align_pointer<H264DecoderMacroblockGlobalInfo *> (m_mbinfo.MV[1] + nMBCount, ALIGN_VALUE);

        m_paddedParsedFrameDataSize.width = m_lumaSize.width;
        m_paddedParsedFrameDataSize.height = m_lumaSize.height;
    }

    return umcRes;
}

void H264DecoderFrameEx::FreeResources()
{
    H264DecoderFrame::FreeResources();
    if (GetRefCounter() == 0 && !isShortTermRef() && !isLongTermRef())
    {
        deallocateParsedFrameData();
    }
}

} // end namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
