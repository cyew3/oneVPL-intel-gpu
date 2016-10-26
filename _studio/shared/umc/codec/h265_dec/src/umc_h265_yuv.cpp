//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER
#include "umc_h265_yuv.h"
#include "umc_h265_frame.h"

namespace UMC_HEVC_DECODER
{
H265DecYUVBufferPadded::H265DecYUVBufferPadded()
    : m_chroma_format(CHROMA_FORMAT_420)
    , m_pYPlane(NULL)
    , m_pUVPlane(NULL)
    , m_pUPlane(NULL)
    , m_pVPlane(NULL)
    , m_pMemoryAllocator(0)
    , m_midAllocatedBuffer(0)
    , m_pAllocatedBuffer(0)
    , m_pitch_luma(0)
    , m_pitch_chroma(0)
    , m_color_format(UMC::NV12)
{
    m_lumaSize.width = 0;
    m_lumaSize.height = 0;

    m_chromaSize.width = 0;
    m_chromaSize.height = 0;
}

H265DecYUVBufferPadded::H265DecYUVBufferPadded(UMC::MemoryAllocator *pMemoryAllocator)
    : m_chroma_format(CHROMA_FORMAT_420)
    , m_pYPlane(NULL)
    , m_pUVPlane(NULL)
    , m_pUPlane(NULL)
    , m_pVPlane(NULL)
    , m_pMemoryAllocator(pMemoryAllocator)
    , m_midAllocatedBuffer(0)
    , m_pAllocatedBuffer(0)
    , m_pitch_luma(0)
    , m_pitch_chroma(0)
    , m_color_format(UMC::NV12)
{
    m_lumaSize.width = 0;
    m_lumaSize.height = 0;

    m_chromaSize.width = 0;
    m_chromaSize.height = 0;
}

H265DecYUVBufferPadded::~H265DecYUVBufferPadded()
{
    deallocate();
}

// Returns pointer to FrameData instance
UMC::FrameData * H265DecYUVBufferPadded::GetFrameData()
{
    return &m_frameData;
}

const UMC::FrameData * H265DecYUVBufferPadded::GetFrameData() const
{
    return &m_frameData;
}

// Deallocate all memory
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
}

// Initialize variables to default values
void H265DecYUVBufferPadded::Init(const UMC::VideoDataInfo *info)
{
    VM_ASSERT(info);

    m_color_format = info->GetColorFormat();
    m_chroma_format = GetH265ColorFormat(info->GetColorFormat());
    m_lumaSize = info->GetPlaneInfo(0)->m_ippSize;
    m_pYPlane = 0;
    m_pUPlane = 0;
    m_pVPlane = 0;
    m_pUVPlane = 0;

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

// Allocate YUV frame buffer planes and initialize pointers to it.
// Used to contain decoded frames.
void H265DecYUVBufferPadded::allocate(const UMC::FrameData * frameData, const UMC::VideoDataInfo *info)
{
    VM_ASSERT(info);
    VM_ASSERT(frameData);

    m_frameData = *frameData;

    if (frameData->GetPlaneMemoryInfo(0)->m_planePtr)
        m_frameData.m_locked = true;

    m_color_format = info->GetColorFormat();

    m_chroma_format = GetH265ColorFormat(info->GetColorFormat());
    m_lumaSize = info->GetPlaneInfo(0)->m_ippSize;
    m_pitch_luma = (Ipp32s)m_frameData.GetPlaneMemoryInfo(0)->m_pitch / info->GetPlaneSampleSize(0);

    m_pYPlane = (PlanePtrY)m_frameData.GetPlaneMemoryInfo(0)->m_planePtr;

    if (m_chroma_format > 0 || GetH265ColorFormat(frameData->GetInfo()->GetColorFormat()) > 0)
    {
        if (m_chroma_format == 0)
            info = frameData->GetInfo();
        m_chromaSize = info->GetPlaneInfo(1)->m_ippSize;
        m_pitch_chroma = (Ipp32s)m_frameData.GetPlaneMemoryInfo(1)->m_pitch / info->GetPlaneSampleSize(1);

        if (m_frameData.GetInfo()->GetNumPlanes() == 2)
        {
            m_pUVPlane = (PlanePtrUV)m_frameData.GetPlaneMemoryInfo(1)->m_planePtr;
            m_pUPlane = 0;
            m_pVPlane = 0;
        }
        else
        {
            m_pUPlane = (PlanePtrUV)m_frameData.GetPlaneMemoryInfo(1)->m_planePtr;
            m_pVPlane = (PlanePtrUV)m_frameData.GetPlaneMemoryInfo(2)->m_planePtr;
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

// Returns color formap of allocated frame
UMC::ColorFormat H265DecYUVBufferPadded::GetColorFormat() const
{
    return m_color_format;
}

// Allocate memory and initialize frame plane pointers and pitches.
// Used for temporary picture buffers, e.g. residuals.
void H265DecYUVBufferPadded::createPredictionBuffer(const H265SeqParamSet * sps)
{
    Ipp32u ElementSizeY = sizeof(Ipp16s);
    Ipp32u ElementSizeUV = sizeof(Ipp16s);
    m_lumaSize.width = sps->MaxCUSize;
    m_lumaSize.height = sps->MaxCUSize;

    m_chromaSize.width = sps->MaxCUSize / sps->SubWidthC();
    m_chromaSize.height = sps->MaxCUSize / sps->SubHeightC();

    m_pitch_luma = sps->MaxCUSize;
    m_pitch_chroma = sps->MaxCUSize;

    size_t allocationSize = (m_lumaSize.height) * m_pitch_luma * ElementSizeY +
        (m_chromaSize.height) * m_pitch_chroma * ElementSizeUV*2 + 512;

    m_pAllocatedBuffer = h265_new_array_throw<Ipp8u>((Ipp32s)allocationSize);
    m_pYPlane = UMC::align_pointer<PlanePtrY>(m_pAllocatedBuffer, 64);

    m_pUVPlane = m_pUPlane = UMC::align_pointer<PlanePtrY>(m_pYPlane + (m_lumaSize.height) * m_pitch_luma * ElementSizeY + 128, 64);
    m_pVPlane = m_pUPlane + m_chromaSize.height * m_chromaSize.width * ElementSizeUV;
}

// Deallocate planes memory
void H265DecYUVBufferPadded::destroy()
{
    delete [] m_pAllocatedBuffer;
    m_pAllocatedBuffer = NULL;
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
