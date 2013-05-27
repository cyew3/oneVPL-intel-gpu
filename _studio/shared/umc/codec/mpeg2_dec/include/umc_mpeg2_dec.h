/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2011 Intel Corporation. All Rights Reserved.
//
*/
//  MPEG-2 is a international standard promoted by ISO/IEC and
//  other organizations. Implementations of this standard, or the standard
//  enabled platforms may require licenses from various entities, including
//  Intel Corporation.

#pragma once

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#include "umc_video_decoder.h"
#include "umc_va_base.h"

//#define OTLAD

#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX) // common for linux and windows
#   define VA_NO       UNKNOWN
#   define VA_IT_W     MPEG2_IT
#   define VA_VLD_W    MPEG2_VLD
#   define VA_VLD_L    MPEG2_VLD
#endif

#define MPEG2_VIRTUAL

namespace UMC
{

// profile and level definitions for mpeg2 video
// values for profile and level fields in BaseCodecParams
#define MPEG2_PROFILE_SIMPLE  5
#define MPEG2_PROFILE_MAIN    4
#define MPEG2_PROFILE_SNR     3
#define MPEG2_PROFILE_SPATIAL 2
#define MPEG2_PROFILE_HIGH    1

#define MPEG2_LEVEL_LOW   10
#define MPEG2_LEVEL_MAIN   8
#define MPEG2_LEVEL_H14    6
#define MPEG2_LEVEL_HIGH   4

    class MPEG2VideoDecoderBase;

    class MPEG2VideoDecoder : public VideoDecoder
    {
        DYNAMIC_CAST_DECL(MPEG2VideoDecoder, VideoDecoder)
    public:
        ///////////////////////////////////////////////////////
        /////////////High Level public interface///////////////
        ///////////////////////////////////////////////////////
        // Default constructor
        MPEG2VideoDecoder(void);

        // Default destructor
        MPEG2_VIRTUAL ~MPEG2VideoDecoder(void);

        // Initialize for subsequent frame decoding.
        MPEG2_VIRTUAL Status Init(BaseCodecParams *init);

        // Get next frame
        MPEG2_VIRTUAL Status GetFrame(MediaData* in, MediaData* out);

        // Close  decoding & free all allocated resources
        MPEG2_VIRTUAL Status Close(void);

        // Reset decoder to initial state
        MPEG2_VIRTUAL Status Reset(void);

        // Get video stream information, valid after initialization
        MPEG2_VIRTUAL Status GetInfo(BaseCodecParams* info);

        Status    GetPerformance(Ipp64f *perf);

        //reset skip frame counter
        Status    ResetSkipCount();

        // increment skip frame counter
        Status    SkipVideoFrame(Ipp32s);

        // get skip frame counter statistic
        Ipp32u    GetNumOfSkippedFrames();

        //access to the latest decoded frame
        //Status PreviewLastFrame(VideoData *out, BaseCodec *pPostProcessing = NULL);

        // returns closed capture data from gop user data
        MPEG2_VIRTUAL Status GetUserData(MediaData* pCC);

        MPEG2_VIRTUAL Status SetParams(BaseCodecParams* params);

    protected:
        MPEG2VideoDecoderBase* m_pDec;
    };
}

#endif // ifdef UMC_ENABLE_MPEG2_VIDEO_DECODER
