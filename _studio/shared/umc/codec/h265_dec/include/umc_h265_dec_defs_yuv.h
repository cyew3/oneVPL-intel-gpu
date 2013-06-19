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

#ifndef __UMC_H265_DEC_DEFS_YUV_H__
#define __UMC_H265_DEC_DEFS_YUV_H__

#include "umc_h265_dec_defs_dec.h"
#include "umc_video_decoder.h"
//#include "umc_default_memory_allocator.h"
#include "umc_frame_data.h"

namespace UMC_HEVC_DECODER
{

class H265DecYUVBufferPadded
{
    DYNAMIC_CAST_DECL_BASE(H265DecYUVBufferPadded)

public:
    //h265
    Ipp32u m_MaxCUWidth;
    Ipp32u m_MaxCUHeight;
    Ipp32u m_MaxCUDepth;

    Ipp32u m_LumaMarginX;
    Ipp32u m_LumaMarginY;
    Ipp32u m_ChromaMarginX;
    Ipp32u m_ChromaMarginY;

    Ipp32u m_ElementSizeY, m_ElementSizeUV;
    //end of h265

    Ipp32s  m_bpp;           // should be >= 8
    Ipp32s  m_chroma_format; // AVC standard value chroma_format_idc

    H265PlanePtrYCommon               m_pYPlane;

    H265PlanePtrUVCommon              m_pUVPlane;  // for NV12 support

    H265PlanePtrUVCommon              m_pUPlane;
    H265PlanePtrUVCommon              m_pVPlane;

    H265DecYUVBufferPadded();
    H265DecYUVBufferPadded(UMC::MemoryAllocator *pMemoryAllocator);
    virtual ~H265DecYUVBufferPadded();

    void Init(const UMC::VideoDataInfo *info);

    void allocate(const UMC::FrameData * frameData, const UMC::VideoDataInfo *info);

    //h265
    void create(Ipp32u PicWidth, Ipp32u PicHeight, Ipp32u ElementSizeY, Ipp32u ElementSizeUV, Ipp32u MaxCUWidth, Ipp32u MaxCUHeight, Ipp32u MaxCUDepth);
    void destroy();
    void clear();

    H265PlanePtrYCommon GetLumaAddr(void);
    H265PlanePtrUVCommon GetCbAddr(void);
    H265PlanePtrUVCommon GetCrAddr(void);
    H265PlanePtrUVCommon GetCbCrAddr(void);

    //  Access starting position of YUV partition unit buffer
    H265PlanePtrYCommon GetPartLumaAddr(Ipp32u PartUnitIdx);
    H265PlanePtrUVCommon GetPartCbAddr(Ipp32u PartUnitIdx);
    H265PlanePtrUVCommon GetPartCrAddr(Ipp32u PartUnitIdx);
    H265PlanePtrUVCommon GetPartCbCrAddr(Ipp32u PartUnitIdx);

    //  Access starting position of YUV transform unit buffer
    H265PlanePtrYCommon GetPartLumaAddr(Ipp32u TransUnitIdx, Ipp32u BlkSize);
    H265PlanePtrUVCommon GetPartCbAddr(Ipp32u TransUnitIdx, Ipp32u BlkSize);
    H265PlanePtrUVCommon GetPartCrAddr(Ipp32u TransUnitIdx, Ipp32u BlkSize);
    H265PlanePtrUVCommon GetPartCbCrAddr(Ipp32u TransUnitIdx, Ipp32u BlkSize);

    void CopyPartToPic(H265DecoderFrame* pPicYuvDst, Ipp32u CUAddr, Ipp32u PartIdx, Ipp32u Width, Ipp32u Height);
#if 0
// ML: OPT: Interpolation now averages, saturates and writes into pic directly
    void CopyPartToPicAndSaturate(H265DecoderFrame* pPicYuvDst, Ipp32u CUAddr, Ipp32u AbsZorderIdx, Ipp32u PartIdx, Ipp32u Width, Ipp32u Height, Ipp32s copyPart = 0);
    void AddAverageToPic(H265DecoderFrame* pPicYuvDst, H265DecYUVBufferPadded* pPicYuvSrc1, Ipp32u CUAddr, Ipp32u AbsZorderIdx, Ipp32u PartIdx, Ipp32u Width, Ipp32u Height);
#endif

    void CopyWeighted_S16U8(H265DecYUVBufferPadded* pPicYuvSrc, Ipp32u PartIdx, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round);
    void CopyWeightedBidi_S16U8(H265DecYUVBufferPadded* pPicYuvSrc0, H265DecYUVBufferPadded* pPicYuvSrc1, Ipp32u PartIdx, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round);

    //end of h265

    void deallocate();

    const IppiSize& lumaSize() const { return m_lumaSize; }
    const IppiSize& chromaSize() const { return m_chromaSize; }

    Ipp32u pitch_luma() const { return m_pitch_luma; }
    Ipp32u pitch_chroma() const { return m_pitch_chroma; }

    const UMC::FrameData * GetFrameData() const;

    UMC::ColorFormat GetColorFormat() const;

protected:
    UMC::MemoryAllocator *m_pMemoryAllocator;                        // (MemoryAllocator *) pointer to memory allocator
    UMC::MemID m_midAllocatedBuffer;                                 // (MemID) mem id for allocated buffer

    Ipp8u                 *m_pAllocatedBuffer;
    // m_pAllocatedBuffer contains the pointer returned when
    // we allocated space for the data.

    Ipp32u                 m_allocatedSize;
    // This is the size with which m_pAllocatedBuffer was allocated.

    bool    m_is_external_memory;

    IppiSize            m_lumaSize;
    IppiSize            m_chromaSize;

    Ipp32s  m_pitch_luma;
    Ipp32s  m_pitch_chroma;

    UMC::FrameData m_frameData;

    UMC::ColorFormat m_color_format;
};

Ipp32s GetAddrOffset(Ipp32u PartUnitIdx, Ipp32u width);
Ipp32s GetAddrOffset(Ipp32u TransUnitIdx, Ipp32u BlkSize, Ipp32u width);


} // namespace UMC_HEVC_DECODER

#endif // __UMC_H264_DEC_DEFS_YUV_H__
#endif // UMC_ENABLE_H264_VIDEO_DECODER
