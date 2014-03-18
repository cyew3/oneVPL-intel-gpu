/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
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
public:

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
    void create(Ipp32u PicWidth, Ipp32u PicHeight, Ipp32u ElementSizeY, Ipp32u ElementSizeUV);
    void destroy();

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

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H264_DEC_DEFS_YUV_H__
#endif // UMC_ENABLE_H264_VIDEO_DECODER
