/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2014 Intel Corporation. All Rights Reserved.
*/

#ifndef __UMC_VA_VIDEO_PROCESSING_H
#define __UMC_VA_VIDEO_PROCESSING_H

#include "umc_va_base.h"
#include "mfxstructures.h"

namespace UMC
{

class VideoProcessingVA
{
public:

    VideoProcessingVA();

    virtual ~VideoProcessingVA();

    virtual Status Init(mfxVideoParam * vpParams, mfxExtDecVideoProcessing * videoProcessing);

    virtual void SetOutputSurface(mfxHDL surfHDL);

    mfxHDL GetCurrentOutputSurface() const;

#ifdef UMC_VA_LINUX
    VAProcPipelineParameterBuffer m_pipelineParams;
#endif

protected:

#ifdef UMC_VA_DXVA

#elif defined(UMC_VA_LINUX)

    VARectangle m_surf_region;
    VARectangle m_output_surf_region;
    VASurfaceID output_surface_array[1];
#endif

    mfxHDL m_currentOutputSurface;
};

}

#endif // __UMC_VA_VIDEO_PROCESSING_H
