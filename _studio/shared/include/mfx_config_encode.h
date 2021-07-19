// Copyright (c) 2021 Intel Corporation
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

#ifndef _MFX_CONFIG_ENCODE_H_
#define _MFX_CONFIG_ENCODE_H_

#define MFX_ENABLE_PARTIAL_BITSTREAM_OUTPUT

#if !defined(OPEN_SOURCE)
    #define MFX_ENABLE_DEBLOCKING_FILTER
    #define MFX_ENABLE_AV1_VIDEO_ENCODE
#endif // #ifndef OPEN_SOURCE

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE)

    #undef MFX_ENABLE_H264_PRIVATE_CTRL
    #define MFX_ENABLE_APQ_LQ
    #if defined(MFX_VA_WIN)
        #define MFX_ENABLE_H264_REPARTITION_CHECK
        #define ENABLE_H264_MBFORCE_INTRA
        #define MFX_ENABLE_AVC_CUSTOM_QMATRIX
        #define MFX_ENABLE_GPU_BASED_SYNC
    #endif
    #define MFX_ENABLE_H264_ROUNDING_OFFSET
    #ifndef OPEN_SOURCE
        #define MFX_ENABLE_AVCE_DIRTY_RECTANGLE
        #define MFX_ENABLE_AVCE_MOVE_RECTANGLE
        #if defined(PRE_SI_TARGET_PLATFORM_GEN12P5)
            #define MFX_ENABLE_AVCE_VDENC_B_FRAMES
        #endif
    #endif
    #if defined(MFX_ENABLE_MCTF) && defined(MFX_ENABLE_KERNELS)
        #define MFX_ENABLE_MCTF_IN_AVC
    #endif
    #if !defined(MFX_ENABLE_VIDEO_BRC_COMMON)
        #define MFX_ENABLE_VIDEO_BRC_COMMON
    #endif
    #if !defined(UMC_ENABLE_VIDEO_BRC)
        #define UMC_ENABLE_VIDEO_BRC
    #endif
    #define USE_AGOP 0
    #if USE_AGOP
    #define USE_DOWN_SAMPLE_KERNELS
    #endif
#endif

#if defined(MFX_ENABLE_MVC_VIDEO_ENCODE)
    #define MFX_ENABLE_MVC_I_TO_P
    #define MFX_ENABLE_MVC_ADD_REF
    #undef MFX_ENABLE_AVC_BS
#endif

#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)
    #define MFX_ENABLE_HEVCE_INTERLACE
    #define MFX_ENABLE_HEVCE_ROI
    #if !defined(OPEN_SOURCE)
        #define MFX_ENABLE_HEVC_EXT_DPB
        #define MFX_ENABLE_HEVCE_DIRTY_RECT
        #define MFX_ENABLE_HEVCE_WEIGHTED_PREDICTION
        #if defined (MFX_ENABLE_HEVCE_WEIGHTED_PREDICTION)
            #define MFX_ENABLE_HEVCE_FADE_DETECTION
        #endif
        #define MFX_ENABLE_HEVCE_HDR_SEI
        #if defined(MFX_VA_WIN)
            #define MFX_ENABLE_HEVCE_UNITS_INFO
            #define MFX_ENABLE_HEVC_CUSTOM_QMATRIX
        #endif
        #define MFX_ENABLE_HEVCE_SCC
    #endif
#endif

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE)
    #if !defined(MFX_ENABLE_VIDEO_BRC_COMMON)
        #define MFX_ENABLE_VIDEO_BRC_COMMON
    #endif
    #if !defined(UMC_ENABLE_VIDEO_BRC)
        #define UMC_ENABLE_VIDEO_BRC
    #endif
    #if defined(MFX_VA_WIN)
    #if !defined(MFX_PROTECTED_FEATURE_DISABLE)
        #define PAVP_SUPPORT
    #endif
    #endif
#endif

#define MFX_ENABLE_QVBR

#if defined(MFX_VA_WIN) 
#if !defined(STRIP_EMBARGO)
    #define MFX_ENABLE_VIDEO_HYPER_ENCODE_HW
#endif
    #define MFX_ENABLE_HW_BLOCKING_TASK_SYNC_ENCODE
#endif

#ifdef MFX_ENABLE_USER_ENCTOOLS
    #define MFX_ENABLE_ENCTOOLS
    #if defined(MFX_VA_WIN)
        #define MFX_ENABLE_ENCTOOLS_LPLA
        #define MFX_ENABLE_LP_LOOKAHEAD
    #endif
#endif

#endif // _MFX_CONFIG_ENCODE_H_
