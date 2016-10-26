//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_VIDEO_DECODERS_H__
#define __UMC_VIDEO_DECODERS_H__

#include "umc_video_decoder.h"
#include "umc_defs.h"

#ifdef UMC_ENABLE_MPEG4_VIDEO_DECODER
    #include "umc_mpeg4_video_decoder.h"
#endif

#ifdef UMC_ENABLE_H264_VIDEO_DECODER
    #include "umc_h264_dec.h"
#endif

#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)
    #include "umc_mpeg2_dec.h"
#endif

#ifdef UMC_ENABLE_DV_VIDEO_DECODER
    #include "umc_dv_decoder.h"

    #ifdef UMC_ENABLE_DV50_VIDEO_DECODER
        #include "umc_dv50_decoder.h"
    #endif

#endif

#ifdef UMC_ENABLE_MJPEG_VIDEO_DECODER
    #include "umc_mjpeg_video_decoder.h"
#endif

#ifdef UMC_ENABLE_H263_VIDEO_DECODER
    #include "umc_h263_video_decoder.h"
#endif

#ifdef UMC_ENABLE_H261_VIDEO_DECODER
    #include "umc_h261_video_decoder.h"
#endif

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)
    #include "umc_vc1_video_decoder.h"
#endif

#ifdef UMC_ENABLE_DVHD_VIDEO_DECODER
    #include "umc_dv100_decoder.h"
#endif

#ifdef UMC_ENABLE_UNCOMPRESSED_VIDEO_DECODER
    #include "umc_uncompressed_video_dec.h"
#endif

#if defined (UMC_ENABLE_AVS_VIDEO_DECODER)
    #include "umc_avs_dec.h"
#endif


/////////////////////////////////////////////////////////////////////////////

#ifdef UMC_DECODER_CREATE_FUNCTION
namespace UMC
{
static VideoDecoder* CreateVideoDecoder(VideoStreamType stream_type)
{
  switch(stream_type) {
#if defined(UMC_ENABLE_MPEG2_VIDEO_DECODER)
     case MPEG1_VIDEO:
     case MPEG2_VIDEO:
       return (VideoDecoder*)(new MPEG2VideoDecoder());
#endif // defined(UMC_ENABLE_MPEG2_VIDEO_DECODER)

#if defined(UMC_ENABLE_DV_VIDEO_DECODER)
#if defined(UMC_ENABLE_DV50_VIDEO_DECODER)
      case DIGITAL_VIDEO_50:
        return (VideoDecoder*)(new DV50VideoDecoder());
#endif // defined(UMC_ENABLE_DV50_VIDEO_DECODER)

#if defined(UMC_ENABLE_DVHD_VIDEO_DECODER)
      case DIGITAL_VIDEO_HD:
        return (VideoDecoder*)(new DV100VideoDecoder());
#endif // defined(UMC_ENABLE_DV_VIDEO_DECODER)

      case DIGITAL_VIDEO_SD:
        return (VideoDecoder*)(new DVVideoDecoder());
#endif // defined(UMC_ENABLE_DV_VIDEO_DECODER)

#if defined(UMC_ENABLE_MPEG4_VIDEO_DECODER)
      case MPEG4_VIDEO:
        return (VideoDecoder*)(new MPEG4VideoDecoder());
#endif // defined(UMC_ENABLE_MPEG4_VIDEO_DECODER)

#if defined(UMC_ENABLE_H263_VIDEO_DECODER)
      case H263_VIDEO:
        return (VideoDecoder*)(new H263VideoDecoder());
#endif // defined(UMC_ENABLE_H263_VIDEO_DECODER)

#if defined(UMC_ENABLE_H261_VIDEO_DECODER)
      case H261_VIDEO:
        return (VideoDecoder*)(new H261VideoDecoder());
#endif // defined(UMC_ENABLE_H261_VIDEO_DECODER)

#if defined(UMC_ENABLE_H264_VIDEO_DECODER)
      case H264_VIDEO:
        return (VideoDecoder*)(new H264VideoDecoder());
#endif // defined(UMC_ENABLE_H264_VIDEO_DECODER)

#if defined(UMC_ENABLE_MJPEG_VIDEO_DECODER)
      case MJPEG_VIDEO:
        return (VideoDecoder*)(new MJPEGVideoDecoder());
#endif // defined(UMC_ENABLE_MJPEG_VIDEO_DECODER)

#if defined (UMC_ENABLE_UNCOMPRESSED_VIDEO_DECODER)
      case UNCOMPRESSED_VIDEO:
        return (VideoDecoder*)(new UncompressedVideoDecoder());
#endif // defined (UMC_ENABLE_UNCOMPRESSED_VIDEO_DECODER)

#if defined(UMC_ENABLE_VC1_VIDEO_DECODER)
      case VC1_VIDEO:
      case WMV_VIDEO:
        return (VideoDecoder*)(new VC1VideoDecoder());
#endif

#if defined(UMC_ENABLE_AVS_VIDEO_DECODER)
      case AVS_VIDEO:
        return (VideoDecoder*)(new AVSVideoDecoder());
#endif
      default:
        //vm_debug_message(__VM_STRING("Unknown video stream type\n"));
        return NULL;
  }
  //return NULL;
}
} // namespace UMC
#endif // UMC_CREATE_FUNCTION

#endif // __UMC_VIDEO_DECODERS_H__
