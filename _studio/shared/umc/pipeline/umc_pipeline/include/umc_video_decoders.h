// Copyright (c) 2003-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef __UMC_VIDEO_DECODERS_H__
#define __UMC_VIDEO_DECODERS_H__

#include "umc_video_decoder.h"
#include "umc_defs.h"

#ifdef UMC_ENABLE_MPEG4_VIDEO_DECODER
    #include "umc_mpeg4_video_decoder.h"
#endif

#ifdef MFX_ENABLE_H264_VIDEO_DECODE
    #include "umc_h264_dec.h"
#endif

#if defined (MFX_ENABLE_MPEG2_VIDEO_DECODE)
    #include "umc_mpeg2_dec.h"
#endif

#ifdef UMC_ENABLE_DV_VIDEO_DECODER
    #include "umc_dv_decoder.h"

    #ifdef UMC_ENABLE_DV50_VIDEO_DECODER
        #include "umc_dv50_decoder.h"
    #endif

#endif

#ifdef MFX_ENABLE_MJPEG_VIDEO_DECODE
    #include "umc_mjpeg_video_decoder.h"
#endif

#ifdef UMC_ENABLE_H263_VIDEO_DECODER
    #include "umc_h263_video_decoder.h"
#endif

#ifdef UMC_ENABLE_H261_VIDEO_DECODER
    #include "umc_h261_video_decoder.h"
#endif

#if defined (MFX_ENABLE_VC1_VIDEO_DECODE)
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
#if defined(MFX_ENABLE_MPEG2_VIDEO_DECODE)
     case MPEG1_VIDEO:
     case MPEG2_VIDEO:
       return (VideoDecoder*)(new MPEG2VideoDecoder());
#endif // defined(MFX_ENABLE_MPEG2_VIDEO_DECODE)

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

#if defined(MFX_ENABLE_H264_VIDEO_DECODE)
      case H264_VIDEO:
        return (VideoDecoder*)(new H264VideoDecoder());
#endif // defined(MFX_ENABLE_H264_VIDEO_DECODE)

#if defined(MFX_ENABLE_MJPEG_VIDEO_DECODE)
      case MJPEG_VIDEO:
        return (VideoDecoder*)(new MJPEGVideoDecoder());
#endif // defined(MFX_ENABLE_MJPEG_VIDEO_DECODE)

#if defined (UMC_ENABLE_UNCOMPRESSED_VIDEO_DECODER)
      case UNCOMPRESSED_VIDEO:
        return (VideoDecoder*)(new UncompressedVideoDecoder());
#endif // defined (UMC_ENABLE_UNCOMPRESSED_VIDEO_DECODER)

#if defined(MFX_ENABLE_VC1_VIDEO_DECODE)
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
