/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2016 Intel Corporation. All Rights Reserved.
//
//
*/
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

size_t H264DecoderFrameEx::GetFrameDataSize(const IppiSize &lumaSize)
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

        m_pParsedFrameDataNew = m_pObjHeap->Allocate<Ipp8u>(nMemSize);

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
