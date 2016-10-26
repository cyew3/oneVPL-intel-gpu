//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2014 Intel Corporation. All Rights Reserved.
//

#include "umc_va_video_processing.h"

namespace UMC
{

VideoProcessingVA::VideoProcessingVA()
{
}

VideoProcessingVA::~VideoProcessingVA()
{
}

void VideoProcessingVA::SetOutputSurface(mfxHDL surfHDL)
{
    m_currentOutputSurface = surfHDL;
}

mfxHDL VideoProcessingVA::GetCurrentOutputSurface() const
{
    return m_currentOutputSurface;
}

#ifdef UMC_VA_DXVA
Status VideoProcessingVA::Init(mfxVideoParam * , mfxExtDecVideoProcessing * )
{
    return UMC_ERR_UNSUPPORTED;
}

#elif defined(UMC_VA_LINUX)

Status VideoProcessingVA::Init(mfxVideoParam * vpParams, mfxExtDecVideoProcessing * videoProcessing)
{
    VAProcPipelineParameterBuffer *pipelineBuf = &m_pipelineParams;

    pipelineBuf->surface = 0;  // should filled in packer

    m_surf_region.x = videoProcessing->In.CropX;
    m_surf_region.y = videoProcessing->In.CropY;
    m_surf_region.width = videoProcessing->In.CropW;
    m_surf_region.height = videoProcessing->In.CropH;

    pipelineBuf->surface_region = &m_surf_region;

    pipelineBuf->surface_color_standard = VAProcColorStandardBT601;

    m_output_surf_region.x = videoProcessing->Out.CropX;
    m_output_surf_region.y = videoProcessing->Out.CropY;
    m_output_surf_region.width = videoProcessing->Out.CropW;
    m_output_surf_region.height = videoProcessing->Out.CropH;

    pipelineBuf->output_region = &m_output_surf_region;

    if (videoProcessing->FillBackground)
        pipelineBuf->output_background_color = videoProcessing->V | (videoProcessing->U << 8) | (videoProcessing->Y << 16);
    else
        pipelineBuf->output_background_color = 0;

    pipelineBuf->output_color_standard = VAProcColorStandardBT601;

    pipelineBuf->pipeline_flags = 0;
    pipelineBuf->filter_flags = 0;
    pipelineBuf->filters = 0;
    pipelineBuf->num_filters = 0;

    pipelineBuf->forward_references = 0;
    pipelineBuf->num_forward_references = 0;

    pipelineBuf->backward_references = 0;
    pipelineBuf->num_backward_references = 0;

    pipelineBuf->rotation_state = VA_ROTATION_NONE;
    pipelineBuf->blend_state = 0;
    pipelineBuf->mirror_state = 0;

    output_surface_array[0] = 0;

    pipelineBuf->additional_outputs = output_surface_array;
    pipelineBuf->num_additional_outputs = 1;

    pipelineBuf->input_surface_flag = 0;

    return UMC_OK;
}

#endif

} // namespace UMC