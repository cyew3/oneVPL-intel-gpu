/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_dec_defs_dec.h"
#include "umc_structures.h"

namespace UMC
{

H264DecoderLocalMacroblockDescriptor::H264DecoderLocalMacroblockDescriptor(void)
{
    MVDeltas[0] =
    MVDeltas[1] = NULL;
    MacroblockCoeffsInfo = NULL;
    mbs = NULL;
    active_next_mb_table = NULL;
    layerMbs = NULL;
    baseFrameInfo = NULL;

    SavedMacroblockCoeffs = NULL;
    SavedMacroblockTCoeffs = NULL;

    m_MBLayerSizePadded = NULL;
    m_numberOfLayers = 1;
    m_isScalable = 0;

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
    layerMbs = NULL;
    baseFrameInfo = NULL;

    SavedMacroblockCoeffs = NULL;
    SavedMacroblockTCoeffs = NULL;

    m_pAllocated = NULL;
    m_nAllocatedSize = 0;

    m_midAllocated = 0;
} // void H264DecoderLocalMacroblockDescriptor::Release(void)

bool H264DecoderLocalMacroblockDescriptor::Allocate(Ipp32s iMBCount, MemoryAllocator *pMemoryAllocator)
{
    Ipp8u *ptr;
    size_t nSize;
    Ipp32s i;
    static int multTable[] = {64, 64, 128, 256};
    Ipp32s pixel_sz = (m_bpp > 8) + 1;
    Ipp32s numberOfLayersToAllocate = m_numberOfLayers;
    Ipp32s numberOfLayersToAllocate_mem = numberOfLayersToAllocate;
    Ipp32s iMBCountPadded;

    if (numberOfLayersToAllocate_mem > 2)
        numberOfLayersToAllocate_mem = 2;

    // check error(s)
    if (NULL == pMemoryAllocator)
        return false;

    // allocate buffer
    nSize = ((sizeof(H264DecoderMacroblockMVs) +
              sizeof(H264DecoderMacroblockMVs) +
              sizeof(H264DecoderMacroblockCoeffsInfo) +
              sizeof(H264DecoderMacroblockLocalInfo)) * m_MBLayerSize[m_numberOfLayers-1] + 16 * 4);

    if (m_isScalable)
    {
        nSize += 2*((sizeof(Ipp16s) * COEFFICIENTS_BUFFER_SIZE) * m_MBLayerSize[m_numberOfLayers-1] + 16);

        nSize += sizeof(H264DecoderBaseFrameDescriptor) +
           ((2 * sizeof(H264DecoderMacroblockMVs) +
                 sizeof(H264DecoderMacroblockGlobalInfo)) * m_MBLayerSize[m_numberOfLayers-1] + 16 * 7);

        if (m_numberOfLayers > 0)
        {
            nSize += sizeof(H264DecoderLayerDescriptor) * (numberOfLayersToAllocate) + 16;
            nSize += sizeof(H264DecoderLayerDescriptor) * (numberOfLayersToAllocate_mem) + 16;

            Ipp32s k = 0;
            for (i = 0; i < numberOfLayersToAllocate_mem; i++)
            {
                size_t iMBCount = m_MBLayerSize[numberOfLayersToAllocate - 1 - i];
                for (; !iMBCount && k < numberOfLayersToAllocate; )
                {
                    iMBCount = m_MBLayerSize[numberOfLayersToAllocate - 1 - k];
                    k++;
                }

                nSize += ((2 * sizeof(H264DecoderMacroblockMVs) +
                               sizeof(H264DecoderMacroblockLayerInfo) +
                           2 * pixel_sz * (256 + 2 * multTable[m_chroma_format])) * iMBCount + 16 * 11);
            }
            nSize += (256 + 2 * multTable[m_chroma_format]) * m_MBLayerSizePadded + 16 * 11;
            nSize += sizeof(Ipp32s) * (m_LayerLumaSize[m_numberOfLayers-1].width) *
              (m_LayerLumaSize[m_numberOfLayers-1].height + 64 + 4) + 16;
            nSize += sizeof(Ipp32s) * (m_LayerLumaSize[m_numberOfLayers-1].width) *
              (m_LayerLumaSize[m_numberOfLayers-1].height + 64 + 4) + 16;
            nSize += sizeof(Ipp32s) * (m_frame_height + 16) + 16;
            nSize += sizeof(Ipp32s) * m_MBLayerSizePadded * 256 + 16;
        }
    }

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
        m_pAllocated = (Ipp8u *) m_pMemoryAllocator->Lock(m_midAllocated);

        ippsZero_8u(m_pAllocated, (Ipp32s)nSize);
        m_nAllocatedSize = nSize;
    }

    iMBCount = m_MBLayerSize[m_numberOfLayers-1];

    // reset pointer(s)
    MVDeltas[0] = align_pointer<H264DecoderMacroblockMVs *> (m_pAllocated, ALIGN_VALUE);
    MVDeltas[1] = align_pointer<H264DecoderMacroblockMVs *> (MVDeltas[0] + iMBCount, ALIGN_VALUE);
    MacroblockCoeffsInfo = align_pointer<H264DecoderMacroblockCoeffsInfo *> (MVDeltas[1] + iMBCount, ALIGN_VALUE);
    mbs = align_pointer<H264DecoderMacroblockLocalInfo *> (MacroblockCoeffsInfo + iMBCount, ALIGN_VALUE);

    if (m_isScalable)
    {
        SavedMacroblockCoeffs = align_pointer<Ipp16s *> (mbs + iMBCount, ALIGN_VALUE); // PK
        SavedMacroblockTCoeffs = align_pointer<Ipp16s *> (SavedMacroblockCoeffs + COEFFICIENTS_BUFFER_SIZE*iMBCount, ALIGN_VALUE);
        baseFrameInfo = align_pointer<H264DecoderBaseFrameDescriptor *> (SavedMacroblockTCoeffs + COEFFICIENTS_BUFFER_SIZE*iMBCount, ALIGN_VALUE);

        ptr = (Ipp8u*)(baseFrameInfo + 1);

        baseFrameInfo->m_mbinfo.MV[0] = align_pointer<H264DecoderMacroblockMVs *> (ptr, ALIGN_VALUE);
        ptr = (Ipp8u*)(baseFrameInfo->m_mbinfo.MV[0] + iMBCount);

        baseFrameInfo->m_mbinfo.MV[1] = align_pointer<H264DecoderMacroblockMVs *> (ptr, ALIGN_VALUE);
        ptr = (Ipp8u*)(baseFrameInfo->m_mbinfo.MV[1] + iMBCount);

        baseFrameInfo->m_mbinfo.mbs = align_pointer<H264DecoderMacroblockGlobalInfo *> (ptr, ALIGN_VALUE);
        ptr = (Ipp8u*)(baseFrameInfo->m_mbinfo.mbs + iMBCount);

        layerMbs = NULL;
        layerMbs_mem = NULL;
        if (m_numberOfLayers > 0)
        {
            layerMbs = align_pointer<H264DecoderLayerDescriptor *> (ptr, ALIGN_VALUE);
            ptr = (Ipp8u*)(layerMbs + numberOfLayersToAllocate);
            layerMbs_mem = align_pointer<H264DecoderLayerDescriptor *> (ptr, ALIGN_VALUE);
            ptr = (Ipp8u*)(layerMbs_mem + numberOfLayersToAllocate_mem);

            Ipp32s k = 0;
            for (i = 0; i < numberOfLayersToAllocate_mem; i++)
            {
                iMBCount = m_MBLayerSize[numberOfLayersToAllocate - 1 - i];

                for (; !iMBCount && k < numberOfLayersToAllocate; )
                {
                    iMBCount = m_MBLayerSize[numberOfLayersToAllocate - 1 - k];
                    k++;
                }

                layerMbs_mem[i].MV[0] = align_pointer<H264DecoderMacroblockMVs *> (ptr, ALIGN_VALUE);
                ptr = (Ipp8u*)(layerMbs_mem[i].MV[0] + iMBCount);

                layerMbs_mem[i].MV[1] = align_pointer<H264DecoderMacroblockMVs *> (ptr, ALIGN_VALUE);
                ptr = (Ipp8u*)(layerMbs_mem[i].MV[1] + iMBCount);

                layerMbs_mem[i].mbs = align_pointer<H264DecoderMacroblockLayerInfo *> (ptr, ALIGN_VALUE);
                ptr = (Ipp8u*)(layerMbs_mem[i].mbs + iMBCount);

                layerMbs_mem[i].m_pYResidual = align_pointer<Ipp16s*> (ptr, ALIGN_VALUE);
                ptr = (Ipp8u*)((Ipp16u*)layerMbs_mem[i].m_pYResidual + pixel_sz * iMBCount * 256);

                layerMbs_mem[i].m_pUResidual = align_pointer<Ipp16s*> (ptr, ALIGN_VALUE);
                ptr = (Ipp8u*)((Ipp16u*)layerMbs_mem[i].m_pUResidual + pixel_sz * iMBCount * multTable[m_chroma_format]);

                layerMbs_mem[i].m_pVResidual = align_pointer<Ipp16s*> (ptr, ALIGN_VALUE);
                ptr = (Ipp8u*)((Ipp16u*)layerMbs_mem[i].m_pVResidual + pixel_sz * iMBCount * multTable[m_chroma_format]);
            }
            iMBCount = m_MBLayerSize[numberOfLayersToAllocate - 1];
            iMBCountPadded = m_MBLayerSizePadded;

            layerMbs_mem[0].m_pYPlane = align_pointer<Ipp8u*> (ptr, ALIGN_VALUE);
            ptr = (Ipp8u*)((Ipp8u*)layerMbs_mem[0].m_pYPlane + pixel_sz * iMBCountPadded * 256);

            layerMbs_mem[0].m_pUPlane = align_pointer<Ipp8u*> (ptr, ALIGN_VALUE);
            ptr = (Ipp8u*)((Ipp8u*)layerMbs_mem[0].m_pUPlane + pixel_sz * iMBCountPadded * multTable[m_chroma_format]);

            layerMbs_mem[0].m_pVPlane = align_pointer<Ipp8u*> (ptr, ALIGN_VALUE);
            ptr = (Ipp8u*)((Ipp8u*)layerMbs_mem[0].m_pVPlane + pixel_sz * iMBCountPadded * multTable[m_chroma_format]);


            Ipp32s idx = 0;
            for (i = numberOfLayersToAllocate - 1; i >= 0; i--, idx ^= 1)
            {
                layerMbs[i].MV[0] = layerMbs_mem[idx].MV[0];

                layerMbs[i].MV[1] = layerMbs_mem[idx].MV[1];

                layerMbs[i].mbs = layerMbs_mem[idx].mbs;

                layerMbs[i].m_pYPlane = layerMbs_mem[0].m_pYPlane + 32 * (m_LayerLumaSize[i].width + 32) + 16; // padding

                layerMbs[i].m_pUPlane = layerMbs_mem[0].m_pUPlane + 16 * (m_LayerLumaSize[i].width + 32) / 2 + 8; // padding

                layerMbs[i].m_pVPlane = layerMbs_mem[0].m_pVPlane + 16 * (m_LayerLumaSize[i].width + 32) / 2 + 8; // padding

                layerMbs[i].m_pYResidual = layerMbs_mem[idx].m_pYResidual;

                layerMbs[i].m_pUResidual = layerMbs_mem[idx].m_pUResidual;

                layerMbs[i].m_pVResidual = layerMbs_mem[idx].m_pVResidual;
            }
            m_tmpBuf0 = align_pointer<Ipp32s*> (ptr, ALIGN_VALUE);
            ptr = (Ipp8u*)(m_tmpBuf0 +  (m_LayerLumaSize[m_numberOfLayers-1].width) *
                (m_LayerLumaSize[m_numberOfLayers-1].height + 64 + 4));

            m_tmpBuf01 = align_pointer<Ipp32s*> (ptr, ALIGN_VALUE);
            ptr = (Ipp8u*)(m_tmpBuf01 +  (m_LayerLumaSize[m_numberOfLayers-1].width) *
                (m_LayerLumaSize[m_numberOfLayers-1].height + 64 + 4));

            m_tmpBuf1 = align_pointer<Ipp32s*> (ptr, ALIGN_VALUE);
            ptr = (Ipp8u*)(m_tmpBuf1 + m_frame_height+16);

            m_tmpBuf2 = align_pointer<Ipp32s*> (ptr, ALIGN_VALUE);
        }
    }

    memset(MacroblockCoeffsInfo, 0, iMBCount * sizeof(H264DecoderMacroblockCoeffsInfo));

    return true;

} // bool H264DecoderLocalMacroblockDescriptor::Allocate(Ipp32s iMBCount)

H264DecoderLocalMacroblockDescriptor &H264DecoderLocalMacroblockDescriptor::operator = (H264DecoderLocalMacroblockDescriptor &Desc)
{
    MVDeltas[0] = Desc.MVDeltas[0];
    MVDeltas[1] = Desc.MVDeltas[1];
    MacroblockCoeffsInfo = Desc.MacroblockCoeffsInfo;
    mbs = Desc.mbs;
    active_next_mb_table = Desc.active_next_mb_table;
    layerMbs = Desc.layerMbs;
    baseFrameInfo = Desc.baseFrameInfo;
    SavedMacroblockCoeffs = Desc.SavedMacroblockCoeffs;
    SavedMacroblockTCoeffs = Desc.SavedMacroblockTCoeffs;

    m_tmpBuf0 = Desc.m_tmpBuf0;
    m_tmpBuf01 = Desc.m_tmpBuf01;
    m_tmpBuf1 = Desc.m_tmpBuf1;
    m_tmpBuf2 = Desc.m_tmpBuf2;

    return *this;

} // H264DecoderLocalMacroblockDescriptor &H264DecoderLocalMacroblockDescriptor::operator = (H264DecoderLocalMacroblockDescriptor &Dest)

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
