//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_DEC_DEFS_YUV_H__
#define __UMC_H264_DEC_DEFS_YUV_H__

#include "umc_h264_dec_defs_dec.h"
#include "umc_video_decoder.h"
#include "umc_frame_data.h"

namespace UMC
{

class H264DecYUVBufferPadded
{
    DYNAMIC_CAST_DECL_BASE(H264DecYUVBufferPadded)

public:

    int32_t  m_bpp;           // should be >= 8
    int32_t  m_chroma_format; // AVC standard value chroma_format_idc

    PlanePtrYCommon               m_pYPlane;

    PlanePtrUVCommon              m_pUVPlane;  // for NV12 support

    PlanePtrUVCommon              m_pUPlane;
    PlanePtrUVCommon              m_pVPlane;

    H264DecYUVBufferPadded();
    virtual ~H264DecYUVBufferPadded();

    void Init(const VideoDataInfo *info);

    void allocate(const FrameData * frameData, const VideoDataInfo *info);
    void deallocate();

    const mfxSize& lumaSize() const { return m_lumaSize; }
    const mfxSize& chromaSize() const { return m_chromaSize; }

    uint32_t pitch_luma() const { return m_pitch_luma; }
    uint32_t pitch_chroma() const { return m_pitch_chroma; }

    void setImageSize(mfxSize dimensions, int32_t chroma_format)
    {
        m_lumaSize.width = dimensions.width;
        m_lumaSize.height = dimensions.height;

        dimensions.width = dimensions.width >> ((int32_t) (3 > chroma_format));
        dimensions.height = dimensions.height >> ((int32_t) (2 > chroma_format));

        m_chromaSize.width = dimensions.width;
        m_chromaSize.height = dimensions.height;

        m_chroma_format = chroma_format;
    }

    FrameData * GetFrameData();
    const FrameData * GetFrameData() const;

    ColorFormat GetColorFormat() const;

protected:

    mfxSize            m_lumaSize;
    mfxSize            m_chromaSize;

    int32_t  m_pitch_luma;
    int32_t  m_pitch_chroma;

    FrameData m_frameData;

    ColorFormat m_color_format;
};

} // namespace UMC

#endif // __UMC_H264_DEC_DEFS_YUV_H__
#endif // UMC_ENABLE_H264_VIDEO_DECODER
