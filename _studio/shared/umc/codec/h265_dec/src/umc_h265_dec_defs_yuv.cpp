/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER
#include "umc_default_memory_allocator.h"
#include "umc_h265_dec_defs_yuv.h"
#include "ippvc.h"
#include "umc_h265_frame.h"
#include "h265_global_rom.h"

namespace UMC_HEVC_DECODER
{
H265DecYUVBufferPadded::H265DecYUVBufferPadded()
    : m_pitch_luma(0)
    , m_pitch_chroma(0)
    , m_color_format(UMC::NV12)
    , m_pYPlane(NULL)
    , m_pUPlane(NULL)
    , m_pVPlane(NULL)
    , m_pUVPlane(NULL)
    , m_LumaMarginX(0)
    , m_LumaMarginY(0)
    , m_ChromaMarginX(0)
    , m_ChromaMarginY(0)
    , m_ElementSizeY(0)
    , m_ElementSizeUV(0)
    , m_pAllocatedBuffer(0)
{
    m_lumaSize.width = 0;
    m_lumaSize.height = 0;
}

H265DecYUVBufferPadded::H265DecYUVBufferPadded(UMC::MemoryAllocator *pMemoryAllocator)
    : m_pitch_luma(0)
    , m_pitch_chroma(0)
    , m_color_format(UMC::NV12)
    , m_pYPlane(NULL)
    , m_pUPlane(NULL)
    , m_pVPlane(NULL)
    , m_pUVPlane(NULL)
    , m_LumaMarginX(0)
    , m_LumaMarginY(0)
    , m_ChromaMarginX(0)
    , m_ChromaMarginY(0)
    , m_ElementSizeY(0)
    , m_ElementSizeUV(0)
    , m_pAllocatedBuffer(0)
{
    m_lumaSize.width = 0;
    m_lumaSize.height = 0;

    m_pMemoryAllocator = pMemoryAllocator;
    m_midAllocatedBuffer = 0;
    //m_midAllocatedRefBaseBuffer = 0;
}

H265DecYUVBufferPadded::~H265DecYUVBufferPadded()
{
    deallocate();
}

const UMC::FrameData * H265DecYUVBufferPadded::GetFrameData() const
{
    return &m_frameData;
}

void H265DecYUVBufferPadded::deallocate()
{
    if (m_frameData.GetFrameMID() != UMC::FRAME_MID_INVALID)
    {
        m_frameData.Close();
        return;
    }

    m_pYPlane = m_pUPlane = m_pVPlane = m_pUVPlane = NULL;

    m_lumaSize.width = 0;
    m_lumaSize.height = 0;
    m_pitch_luma = 0;
    m_pitch_chroma = 0;

    m_LumaMarginX = 0;
    m_LumaMarginY = 0;
    m_ChromaMarginX = 0;
    m_ChromaMarginY = 0;
    m_ElementSizeY = 0;
    m_ElementSizeUV = 0;
}

void H265DecYUVBufferPadded::Init(const UMC::VideoDataInfo *info)
{
    VM_ASSERT(info);

    m_bpp = IPP_MAX(info->GetPlaneBitDepth(0), info->GetPlaneBitDepth(1));

    m_color_format = info->GetColorFormat();
    m_chroma_format = GetH265ColorFormat(info->GetColorFormat());
    m_lumaSize = info->GetPlaneInfo(0)->m_ippSize;
    m_pYPlane = 0;
    m_pUPlane = 0;
    m_pVPlane = 0;
    m_pUVPlane = 0;

    m_LumaMarginX = g_MaxCUWidth  + 16;
    m_LumaMarginY = g_MaxCUHeight + 16;

    m_ChromaMarginX = m_LumaMarginX >> 1;
    m_ChromaMarginY = m_LumaMarginY >> 1;

    m_ElementSizeY = sizeof(H265PlaneYCommon);
    m_ElementSizeUV = sizeof(H265PlaneUVCommon);

    if (m_chroma_format > 0)
    {
        m_chromaSize = info->GetPlaneInfo(1)->m_ippSize;
    }
    else
    {
        m_chromaSize.width = 0;
        m_chromaSize.height = 0;
    }
}

void H265DecYUVBufferPadded::allocate(const UMC::FrameData * frameData, const UMC::VideoDataInfo *info)
{
    VM_ASSERT(info);
    VM_ASSERT(frameData);

    m_frameData = *frameData;

    if (frameData->GetPlaneMemoryInfo(0)->m_planePtr)
        m_frameData.m_locked = true;

    m_color_format = info->GetColorFormat();
    m_bpp = IPP_MAX(info->GetPlaneBitDepth(0), info->GetPlaneBitDepth(1));

    m_chroma_format = GetH265ColorFormat(info->GetColorFormat());
    m_lumaSize = info->GetPlaneInfo(0)->m_ippSize;
    m_pitch_luma = (Ipp32s)m_frameData.GetPlaneMemoryInfo(0)->m_pitch / info->GetPlaneInfo(0)->m_iSampleSize; //this from h264

    m_pYPlane = (H265PlanePtrYCommon)m_frameData.GetPlaneMemoryInfo(0)->m_planePtr; //wtf H265 uses internal 14 bit depth

    if (m_chroma_format > 0 || GetH265ColorFormat(frameData->GetInfo()->GetColorFormat()) > 0)
    {
        if (m_chroma_format == 0)
            info = frameData->GetInfo();
        m_chromaSize = info->GetPlaneInfo(1)->m_ippSize;
        m_pitch_chroma = (Ipp32s)m_frameData.GetPlaneMemoryInfo(1)->m_pitch / info->GetPlaneInfo(1)->m_iSampleSize; //this from h264

        if (m_frameData.GetInfo()->GetColorFormat() == UMC::NV12)
        {
            m_pUVPlane = (H265PlanePtrUVCommon)m_frameData.GetPlaneMemoryInfo(1)->m_planePtr; //wtf H265 uses internal 14 bit depth
            m_pUPlane = 0;
            m_pVPlane = 0;
        }
        else
        {
            m_pUPlane = (H265PlanePtrUVCommon)m_frameData.GetPlaneMemoryInfo(1)->m_planePtr;    //wtf H265 uses internal 14 bit depth
            m_pVPlane = (H265PlanePtrUVCommon)m_frameData.GetPlaneMemoryInfo(2)->m_planePtr; //wtf H265 uses internal 14 bit depth
            m_pUVPlane = 0;
        }
    }
    else
    {
        m_chromaSize.width = 0;
        m_chromaSize.height = 0;
        m_pitch_chroma = 0;
        m_pUPlane = 0;
        m_pVPlane = 0;
    }
}

UMC::ColorFormat H265DecYUVBufferPadded::GetColorFormat() const
{
    return m_color_format;
}

//h265
void H265DecYUVBufferPadded::create(Ipp32u PicWidth, Ipp32u PicHeight, Ipp32u ElementSizeY, Ipp32u ElementSizeUV, Ipp32u MaxCUWidth, Ipp32u MaxCUHeight, Ipp32u MaxCUDepth)
{
    m_MaxCUWidth = MaxCUWidth;
    m_MaxCUHeight = MaxCUHeight;
    m_MaxCUDepth = MaxCUDepth;

    m_lumaSize.width = PicWidth;
    m_lumaSize.height = PicHeight;

    m_chromaSize.width = PicWidth >> 1;
    m_chromaSize.height = PicHeight >> 1;
    m_ElementSizeY = ElementSizeY;
    m_ElementSizeUV = ElementSizeUV;

    m_pitch_luma = PicWidth;
    m_pitch_chroma = PicWidth;

    size_t allocationSize = (m_lumaSize.height) * m_pitch_luma * ElementSizeY + 
        (m_chromaSize.height) * m_pitch_chroma * ElementSizeUV*2 + 512;

    m_pAllocatedBuffer = new Ipp8u[allocationSize];
    m_pYPlane = UMC::align_pointer<H265PlanePtrYCommon>(m_pAllocatedBuffer, 64);

    m_pUVPlane = m_pUPlane = UMC::align_pointer<H265PlanePtrYCommon>(m_pYPlane + (m_lumaSize.height) * m_pitch_luma * ElementSizeY + 128, 64);
    m_pVPlane = m_pUPlane + m_chromaSize.height * m_chromaSize.width * ElementSizeUV;
}

void H265DecYUVBufferPadded::destroy()
{
    delete [] m_pAllocatedBuffer;
    m_pAllocatedBuffer = NULL;
}

void H265DecYUVBufferPadded::clear()
{
    ::memset(m_pYPlane, 0, m_lumaSize.width * m_lumaSize.height * m_ElementSizeY * sizeof(H265PlaneYCommon));
    // NV12
    ::memset(m_pUPlane, 0, m_chromaSize.width * m_chromaSize.height * m_ElementSizeUV * sizeof(H265PlaneUVCommon) * 2);
}

Ipp32s GetAddrOffset(Ipp32u PartUnitIdx, Ipp32u width)
{
    Ipp32s blkX = g_RasterToPelX[g_ZscanToRaster[PartUnitIdx]];
    Ipp32s blkY = g_RasterToPelY[g_ZscanToRaster[PartUnitIdx]];

    return blkX + blkY * width;
}

Ipp32s GetAddrOffset(Ipp32u TransUnitIdx, Ipp32u BlkSize, Ipp32u width)
{
    Ipp32s blkX = (TransUnitIdx * BlkSize) & (width - 1);
    Ipp32s blkY = (TransUnitIdx * BlkSize) &~ (width - 1);

    return blkX + blkY * BlkSize;
}

H265PlanePtrYCommon H265DecYUVBufferPadded::GetLumaAddr(void)
{
    return m_pYPlane;
}
H265PlanePtrUVCommon H265DecYUVBufferPadded::GetCbAddr(void)
{
    return m_pUPlane;
}
H265PlanePtrUVCommon H265DecYUVBufferPadded::GetCrAddr(void)
{
    return m_pVPlane;
}
H265PlanePtrUVCommon H265DecYUVBufferPadded::GetCbCrAddr(void)
{
    return m_pUVPlane;
}

//  Access function for YUV buffer
//  Access starting position of YUV partition unit buffer
H265PlanePtrYCommon H265DecYUVBufferPadded::GetPartLumaAddr(Ipp32u PartUnitIdx)
{
    return (m_pYPlane + GetAddrOffset(PartUnitIdx, m_lumaSize.width));
}
H265PlanePtrUVCommon H265DecYUVBufferPadded::GetPartCbAddr(Ipp32u PartUnitIdx)
{
    return (m_pUPlane + (GetAddrOffset(PartUnitIdx, m_chromaSize.width) >> 1));
}
H265PlanePtrUVCommon H265DecYUVBufferPadded::GetPartCrAddr(Ipp32u PartUnitIdx)
{
    return (m_pVPlane + (GetAddrOffset(PartUnitIdx, m_chromaSize.width) >> 1));
}
H265PlanePtrUVCommon H265DecYUVBufferPadded::GetPartCbCrAddr(Ipp32u PartUnitIdx)
{
    return (m_pUVPlane + GetAddrOffset(PartUnitIdx, m_chromaSize.width));
}

//  Access starting position of YUV transform unit buffer
H265PlanePtrYCommon H265DecYUVBufferPadded::GetPartLumaAddr(Ipp32u TransUnitIdx, Ipp32u BlkSize)
{
    return (m_pYPlane + GetAddrOffset(TransUnitIdx, BlkSize, m_lumaSize.width));
}

H265PlanePtrUVCommon H265DecYUVBufferPadded::GetPartCbAddr(Ipp32u TransUnitIdx, Ipp32u BlkSize)
{
    return (m_pUPlane + GetAddrOffset(TransUnitIdx, BlkSize, m_chromaSize.width));
}

H265PlanePtrUVCommon H265DecYUVBufferPadded::GetPartCrAddr(Ipp32u TransUnitIdx, Ipp32u BlkSize)
{
    return (m_pVPlane + GetAddrOffset(TransUnitIdx, BlkSize, m_chromaSize.width));
}

H265PlanePtrUVCommon H265DecYUVBufferPadded::GetPartCbCrAddr(Ipp32u TransUnitIdx, Ipp32u BlkSize)
{
    return (m_pUVPlane + GetAddrOffset(TransUnitIdx, BlkSize, m_chromaSize.width) * 2);
}

void H265DecYUVBufferPadded::CopyWeighted_S16U8(H265DecYUVBufferPadded* pPicYuvSrc, Ipp32u PartIdx, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round)
{
    H265CoeffsPtrCommon pSrc = (H265CoeffsPtrCommon)pPicYuvSrc->GetLumaAddr() + GetAddrOffset(PartIdx, pPicYuvSrc->m_lumaSize.width);
    H265CoeffsPtrCommon pSrcUV = (H265CoeffsPtrCommon)pPicYuvSrc->GetCbCrAddr() + GetAddrOffset(PartIdx, pPicYuvSrc->m_chromaSize.width);

    H265PlanePtrYCommon pDst = GetPartLumaAddr(PartIdx);
    H265PlanePtrUVCommon pDstUV = GetPartCbCrAddr(PartIdx);

    Ipp32u SrcStride = pPicYuvSrc->pitch_luma();
    Ipp32u DstStride = pitch_luma();

    for (Ipp32u y = 0; y < Height; y++)
    {
        for (Ipp32u x = 0; x < Width; x++)
        {
            pDst[x] = (H265PlaneYCommon)ClipY(((w[0] * pSrc[x] + round[0]) >> logWD[0]) + o[0]);
        }
        pSrc += SrcStride;
        pDst += DstStride;
    }

    SrcStride = pPicYuvSrc->pitch_chroma();
    DstStride = pitch_chroma();
    Width >>= 1;
    Height >>= 1;

    for (Ipp32u y = 0; y < Height; y++)
    {
        for (Ipp32u x = 0; x < Width * 2; x += 2)
        {
            pDstUV[x] = (H265PlaneUVCommon)ClipC(((w[1] * pSrcUV[x] + round[1]) >> logWD[1]) + o[1]);
            pDstUV[x + 1] = (H265PlaneUVCommon)ClipC(((w[2] * pSrcUV[x + 1] + round[2]) >> logWD[2]) + o[2]);
        }
        pSrcUV += SrcStride;
        pDstUV += DstStride;
    }
}

void H265DecYUVBufferPadded::CopyWeightedBidi_S16U8(H265DecYUVBufferPadded* pPicYuvSrc0, H265DecYUVBufferPadded* pPicYuvSrc1, Ipp32u PartIdx, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round)
{
    H265CoeffsPtrCommon pSrc0 = (H265CoeffsPtrCommon)pPicYuvSrc0->GetLumaAddr() + GetAddrOffset(PartIdx, pPicYuvSrc0->m_lumaSize.width);
    H265CoeffsPtrCommon pSrcUV0 = (H265CoeffsPtrCommon)pPicYuvSrc0->GetCbCrAddr() + GetAddrOffset(PartIdx, pPicYuvSrc0->m_chromaSize.width);

    H265CoeffsPtrCommon pSrc1 = (H265CoeffsPtrCommon)pPicYuvSrc1->GetLumaAddr() + GetAddrOffset(PartIdx, pPicYuvSrc1->m_lumaSize.width);
    H265CoeffsPtrCommon pSrcUV1 = (H265CoeffsPtrCommon)pPicYuvSrc1->GetCbCrAddr() + GetAddrOffset(PartIdx, pPicYuvSrc1->m_chromaSize.width);

    H265PlanePtrYCommon pDst = GetPartLumaAddr(PartIdx);
    H265PlanePtrUVCommon pDstUV = GetPartCbCrAddr(PartIdx);

    Ipp32u SrcStride0 = pPicYuvSrc0->pitch_luma();
    Ipp32u SrcStride1 = pPicYuvSrc1->pitch_luma();
    Ipp32u DstStride = pitch_luma();

    for (Ipp32u y = 0; y < Height; y++)
    {
        for (Ipp32u x = 0; x < Width; x++)
        {
            pDst[x] = (H265PlaneYCommon)ClipY((w0[0] * pSrc0[x] + w1[0] * pSrc1[x] + round[0]) >> logWD[0]);
        }
        pSrc0 += SrcStride0;
        pSrc1 += SrcStride1;
        pDst += DstStride;
    }

    SrcStride0 = pPicYuvSrc0->pitch_chroma();
    SrcStride1 = pPicYuvSrc1->pitch_chroma();
    DstStride = pitch_chroma();
    Width >>= 1;
    Height >>= 1;

    for (Ipp32u y = 0; y < Height; y++)
    {
        for (Ipp32u x = 0; x < Width * 2; x += 2)
        {
            pDstUV[x] = (H265PlaneUVCommon)ClipC((w0[1] * pSrcUV0[x] + w1[1] * pSrcUV1[x] + round[1]) >> logWD[1]);
            pDstUV[x + 1] = (H265PlaneUVCommon)ClipC((w0[2] * pSrcUV0[x + 1] + w1[2] * pSrcUV1[x + 1] + round[2]) >> logWD[2]);
        }
        pSrcUV0 += SrcStride0;
        pSrcUV1 += SrcStride1;
        pDstUV += DstStride;
    }
}

void H265DecYUVBufferPadded::CopyPartToPic(H265DecoderFrame* pPicYuvDst, Ipp32u CUAddr, Ipp32u AbsZorderIdx, Ipp32u PartIdx, Ipp32u Width, Ipp32u Height)
{
    Ipp32u SrcStride = pitch_luma();
    Ipp32u DstStride = pPicYuvDst->pitch_luma();

    H265PlanePtrYCommon pSrc = GetPartLumaAddr(PartIdx);
    H265PlanePtrYCommon pDst = pPicYuvDst->GetLumaAddr(CUAddr, AbsZorderIdx) + GetAddrOffset(PartIdx, DstStride);

    for (Ipp32s y = Height; y != 0; y--)
    {
        ::memcpy(pDst, pSrc, Width * sizeof(H265PlaneYCommon));
        pSrc += SrcStride;
        pDst += DstStride;
    }

    Width >>= 1;
    Height >>= 1;

    SrcStride = pitch_chroma();
    DstStride = pPicYuvDst->pitch_chroma();

    H265PlanePtrUVCommon pSrcUV = GetPartCbCrAddr(PartIdx);
    H265PlanePtrUVCommon pDstUV = pPicYuvDst->GetCbCrAddr(CUAddr, AbsZorderIdx) + GetAddrOffset(PartIdx, DstStride >> 1);

    for (Ipp32s y = Height; y != 0; y--)
    {
        ::memcpy(pDstUV, pSrcUV, Width * 2 * sizeof(H265PlaneUVCommon));
        pSrcUV += SrcStride;
        pDstUV += DstStride;
    }
}

void H265DecYUVBufferPadded::AddAverageToPic(H265DecoderFrame* pPicYuvDst, H265DecYUVBufferPadded* pPicYuvSrc1, Ipp32u CUAddr, Ipp32u AbsZorderIdx, Ipp32u PartIdx, Ipp32u Width, Ipp32u Height)
{
    // Should add partition offset explicitly to pointer because it should be multiplied by 2 in case of 2-byte elements
    H265CoeffsPtrCommon pSrc0 = (H265CoeffsPtrCommon)GetLumaAddr() + GetAddrOffset(PartIdx, m_lumaSize.width);
    H265CoeffsPtrCommon pSrcUV0 = (H265CoeffsPtrCommon)GetCbCrAddr() + GetAddrOffset(PartIdx, m_chromaSize.width);
    H265CoeffsPtrCommon pSrc1 = (H265CoeffsPtrCommon)pPicYuvSrc1->GetLumaAddr() + GetAddrOffset(PartIdx, pPicYuvSrc1->m_lumaSize.width);
    H265CoeffsPtrCommon pSrcUV1 = (H265CoeffsPtrCommon)pPicYuvSrc1->GetCbCrAddr() + GetAddrOffset(PartIdx, pPicYuvSrc1->m_chromaSize.width);

    Ipp32u SrcStride0 = pitch_luma();
    Ipp32u SrcStride1 = pPicYuvSrc1->pitch_luma();
    Ipp32u DstStride = pPicYuvDst->pitch_luma();

    H265PlanePtrYCommon pDst = pPicYuvDst->GetLumaAddr(CUAddr, AbsZorderIdx) + GetAddrOffset(PartIdx, DstStride);

    Ipp32s shift = 15 - g_bitDepthY;
    Ipp32s offset = 1 << (shift - 1);

#if (HEVC_OPT_CHANGES & 0x4)
    // ML: OPT: TODO: Move below into a separate template function with shift as templ param
    if ( shift == 7 )
        #pragma ivdep
        for (Ipp32u y = 0; y < Height; y++)
        {
            #pragma ivdep
            #pragma vector always
            for (Ipp32u x = 0; x < Width; x++)
                pDst[x] = (H265PlaneYCommon)ClipY((pSrc0[x] + pSrc1[x] + (1<<6)) >> 7);

            pSrc0 += SrcStride0;
            pSrc1 += SrcStride1;
            pDst += DstStride;
        }
    else
#endif
    for (Ipp32u y = 0; y < Height; y++)
    {
        for (Ipp32u x = 0; x < Width; x++)
        {
            pDst[x] = (H265PlaneYCommon)ClipY((pSrc0[x] + pSrc1[x] + offset) >> shift);
        }
        pSrc0 += SrcStride0;
        pSrc1 += SrcStride1;
        pDst += DstStride;
    }

    SrcStride0 = pitch_chroma();
    SrcStride1 = pPicYuvSrc1->pitch_chroma();

    Width >>= 1;
    Height >>= 1;
    shift = 15 - g_bitDepthC;
    offset = 1 << (shift - 1);

    DstStride = pPicYuvDst->pitch_luma();
    H265PlanePtrUVCommon pDstUV = pPicYuvDst->GetCbCrAddr(CUAddr, AbsZorderIdx) + GetAddrOffset(PartIdx, DstStride >> 1);

#if (HEVC_OPT_CHANGES & 0x4)
    // ML: OPT: TODO: Move below into a separate template function with shift as templ param
    if ( shift == 7 )
        #pragma ivdep
        for (Ipp32u y = 0; y < Height; y++)
        {
            #pragma ivdep
            for (Ipp32u x = 0; x < Width * 2; x++)
                pDstUV[x] = (H265PlaneUVCommon)ClipC((pSrcUV0[x] + pSrcUV1[x] + (1<<6)) >> 7);

            pSrcUV0 += SrcStride0;
            pSrcUV1 += SrcStride1;
            pDstUV += DstStride;
        }
    else
#endif
    for (Ipp32u y = 0; y < Height; y++)
    {
        for (Ipp32u x = 0; x < Width * 2; x++)
        {
            pDstUV[x] = (H265PlaneUVCommon)ClipC((pSrcUV0[x] + pSrcUV1[x] + offset) >> shift);
        }
        pSrcUV0 += SrcStride0;
        pSrcUV1 += SrcStride1;
        pDstUV += DstStride;
    }
}

void H265DecYUVBufferPadded::CopyPartToPicAndSaturate(H265DecoderFrame* pPicYuvDst, Ipp32u CUAddr, Ipp32u AbsZorderIdx, Ipp32u PartIdx, Ipp32u Width, Ipp32u Height, Ipp32s copyPart)
{
    Ipp32u SrcStride = pitch_luma();
    Ipp32u DstStride = pPicYuvDst->pitch_luma();

    H265CoeffsPtrCommon pSrc = (H265CoeffsPtrCommon)GetLumaAddr() + GetAddrOffset(PartIdx, m_lumaSize.width);
    H265PlanePtrYCommon pDst = pPicYuvDst->GetLumaAddr(CUAddr, AbsZorderIdx) + GetAddrOffset(PartIdx, DstStride);

    if (copyPart == 0 || copyPart == 1)
    {
        for (Ipp32s y = Height; y != 0; y--)
        {
            for (Ipp32u x = 0; x < Width; x++)
            {
                pDst[x] = (H265PlaneYCommon)ClipY(pSrc[x]);
            }
            pSrc += SrcStride;
            pDst += DstStride;
        }
    }

    if (copyPart == 0 || copyPart == 2)
    {
        Width >>= 1;
        Height >>= 1;

        SrcStride = pitch_chroma();
        DstStride = pPicYuvDst->pitch_chroma();

        H265CoeffsPtrCommon pSrcUV = (H265CoeffsPtrCommon)GetCbCrAddr() + GetAddrOffset(PartIdx, m_chromaSize.width);
        H265PlanePtrUVCommon pDstUV = pPicYuvDst->GetCbCrAddr(CUAddr, AbsZorderIdx) + GetAddrOffset(PartIdx, DstStride >> 1);

        for (Ipp32s y = Height; y != 0; y--)
        {
            for (Ipp32u x = 0; x < Width * 2; x++)
            {
                pDstUV[x] = (H265PlaneUVCommon)ClipC(pSrcUV[x]);
            }
            pSrcUV += SrcStride;
            pDstUV += DstStride;
        }
    }
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
