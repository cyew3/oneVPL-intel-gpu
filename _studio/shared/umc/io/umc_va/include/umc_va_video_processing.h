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

#ifdef UMC_VA_DXVA

#elif defined(UMC_VA_LINUX)

#define MFX_VAConfigAttribDecProcessing 8
#define VA_DEC_PROCESSING_NONE     0x00000000

typedef struct _MFX_VAProcPipelineParameterBuffer {
    /**
     * \brief Source surface ID.
     *
     * ID of the source surface to process. If subpictures are associated
     * with the video surfaces then they shall be rendered to the target
     * surface, if the #VA_PROC_PIPELINE_SUBPICTURES pipeline flag is set.
     */
    VASurfaceID         surface;
    /**
     * \brief Region within the source surface to be processed.
     *
     * Pointer to a #VARectangle defining the region within the source
     * surface to be processed. If NULL, \c surface_region implies the
     * whole surface.
     */
    const VARectangle  *surface_region;
    /**
     * \brief Requested input color primaries.
     *
     * Color primaries are implicitly converted throughout the processing
     * pipeline. The video processor chooses the best moment to apply
     * this conversion. The set of supported color primaries primaries
     * for input shall be queried with vaQueryVideoProcPipelineCaps().
     */
    VAProcColorStandardType surface_color_standard;
    /**
     * \brief Region within the output surface.
     *
     * Pointer to a #VARectangle defining the region within the output
     * surface that receives the processed pixels. If NULL, \c output_region
     * implies the whole surface.
     *
     * Note that any pixels residing outside the specified region will
     * be filled in with the \ref output_background_color.
     */
    const VARectangle  *output_region;
    /**
     * \brief Background color.
     *
     * Background color used to fill in pixels that reside outside of the
     * specified \ref output_region. The color is specified in ARGB format:
     * [31:24] alpha, [23:16] red, [15:8] green, [7:0] blue.
     *
     * Unless the alpha value is zero or the \ref output_region represents
     * the whole target surface size, implementations shall not render the
     * source surface to the target surface directly. Rather, in order to
     * maintain the exact semantics of \ref output_background_color, the
     * driver shall use a temporary surface and fill it in with the
     * appropriate background color. Next, the driver will blend this
     * temporary surface into the target surface.
     */
    unsigned int        output_background_color;
    /**
     * \brief Requested output color primaries.
     */
    VAProcColorStandardType output_color_standard;
    /**
     * \brief Pipeline filters. See video pipeline flags.
     *
     * Flags to control the pipeline, like whether to apply subpictures
     * or not, notify the driver that it can opt for power optimizations,
     * should this be needed.
     */
    unsigned int        pipeline_flags;
    /**
     * \brief Extra filter flags. See vaPutSurface() flags.
     *
     * Filter flags are used as a fast path, wherever possible, to use
     * vaPutSurface() flags instead of explicit filter parameter buffers.
     *
     * Allowed filter flags API-wise. Use vaQueryVideoProcPipelineCaps()
     * to check for implementation details:
     * - Bob-deinterlacing: \c VA_FRAME_PICTURE, \c VA_TOP_FIELD,
     *   \c VA_BOTTOM_FIELD. Note that any deinterlacing filter
     *   (#VAProcFilterDeinterlacing) will override those flags.
     * - Color space conversion: \c VA_SRC_BT601, \c VA_SRC_BT709,
     *   \c VA_SRC_SMPTE_240.
     * - Scaling: \c VA_FILTER_SCALING_DEFAULT, \c VA_FILTER_SCALING_FAST,
     *   \c VA_FILTER_SCALING_HQ, \c VA_FILTER_SCALING_NL_ANAMORPHIC.
     * - Enable auto noise reduction: \c VA_FILTER_NOISEREDUCTION_AUTO.
     */
    unsigned int        filter_flags;
    /**
     * \brief Array of filters to apply to the surface.
     *
     * The list of filters shall be ordered in the same way the driver expects
     * them. i.e. as was returned from vaQueryVideoProcFilters().
     * Otherwise, a #VA_STATUS_ERROR_INVALID_FILTER_CHAIN is returned
     * from vaRenderPicture() with this buffer.
     *
     * #VA_STATUS_ERROR_UNSUPPORTED_FILTER is returned if the list
     * contains an unsupported filter.
     *
     * Note: no filter buffer is destroyed after a call to vaRenderPicture(),
     * only this pipeline buffer will be destroyed as per the core API
     * specification. This allows for flexibility in re-using the filter for
     * other surfaces to be processed.
     */
    VABufferID         *filters;
    /** \brief Actual number of filters. */
    unsigned int        num_filters;
    /** \brief Array of forward reference frames. */
    VASurfaceID        *forward_references;
    /** \brief Number of forward reference frames that were supplied. */
    unsigned int        num_forward_references;
    /** \brief Array of backward reference frames. */
    VASurfaceID        *backward_references;
    /** \brief Number of backward reference frames that were supplied. */
    unsigned int        num_backward_references;
    /**
     * \brief Rotation state. See rotation angles.
     *
     * The rotation angle is clockwise. There is no specific rotation
     * center for this operation. Rather, The source \ref surface is
     * first rotated by the specified angle and then scaled to fit the
     * \ref output_region.
     *
     * This means that the top-left hand corner (0,0) of the output
     * (rotated) surface is expressed as follows:
     * - \ref VA_ROTATION_NONE: (0,0) is the top left corner of the
     *   source surface -- no rotation is performed ;
     * - \ref VA_ROTATION_90: (0,0) is the bottom-left corner of the
     *   source surface ;
     * - \ref VA_ROTATION_180: (0,0) is the bottom-right corner of the
     *   source surface -- the surface is flipped around the X axis ;
     * - \ref VA_ROTATION_270: (0,0) is the top-right corner of the
     *   source surface.
     *
     * Check VAProcPipelineCaps::rotation_flags first prior to
     * defining a specific rotation angle. Otherwise, the hardware can
     * perfectly ignore this variable if it does not support any
     * rotation.
     */
    unsigned int        rotation_state;
    /**
     * \brief blending state. See "Video blending state definition".
     *
     * If \ref blend_state is NULL, then default operation mode depends
     * on the source \ref surface format:
     * - RGB: per-pixel alpha blending ;
     * - YUV: no blending, i.e override the underlying pixels.
     *
     * Otherwise, \ref blend_state is a pointer to a #VABlendState
     * structure that shall be live until vaEndPicture().
     *
     * Implementation note: the driver is responsible for checking the
     * blend state flags against the actual source \ref surface format.
     * e.g. premultiplied alpha blending is only applicable to RGB
     * surfaces, and luma keying is only applicable to YUV surfaces.
     * If a mismatch occurs, then #VA_STATUS_ERROR_INVALID_BLEND_STATE
     * is returned.
     */
    const VABlendState *blend_state;
    /**
     * \bried mirroring state. See "Mirroring directions".
     *
     * Mirroring of an image can be performed either along the
     * horizontal or vertical axis. It is assumed that the rotation
     * operation is always performed before the mirroring operation.
     */
    unsigned int      mirror_state;
    /** \brief Array of additional output surfaces. */
    VASurfaceID        *additional_outputs;
    /** \brief Number of additional output surfaces. */
    unsigned int        num_additional_outputs;
    /**
     * \brief Flag to indicate the input surface flag such as chroma-siting,
     * range flag and so on.
     *
     * The lower 4 bits are still used as chroma-siting flag
     * The range_flag bit is used to indicate that the range flag of color-space conversion.
     * -\ref VA_SOURCE_RANGE_FULL(Full range): Y/Cb/Cr is in [0, 255].It is
     *   mainly used for JPEG/JFIF formats. The combination with the BT601 flag
     *   means that JPEG/JFIF color-space conversion matrix is used.
     * -\ref VA_SOURCE_RANGE_FULL(Reduced range): Y is in [16, 235] and Cb/Cr
     *   is in [16, 240]. It is mainly used for the YUV<->RGB color-space
     *   conversion in SDTV/HDTV/UHDTV.
     */
    unsigned int        input_surface_flag;
} MFX_VAProcPipelineParameterBuffer;

#endif


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

    MFX_VAProcPipelineParameterBuffer m_pipelineParams;

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
