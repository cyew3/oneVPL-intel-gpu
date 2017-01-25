//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_DEFS_H__
#define __UMC_DEFS_H__

#define ALLOW_SW_VC1_FALLBACK
// This file contains defines which switch on/off support of
// codecs and renderers on application level
/*
// Windows
*/
#if defined(WIN64) || defined (WIN32)

    // video decoders
    #define UMC_ENABLE_H264_VIDEO_DECODER
    #define UMC_ENABLE_H265_VIDEO_DECODER
    #define UMC_ENABLE_MPEG2_VIDEO_DECODER
    #define UMC_ENABLE_MJPEG_VIDEO_DECODER
    #define UMC_ENABLE_VC1_VIDEO_DECODER
    //#define UMC_ENABLE_VP8_VIDEO_DECODER
    #define UMC_ENABLE_VP9_VIDEO_DECODER

    // video encoders
    #define UMC_ENABLE_H264_VIDEO_ENCODER
    #define UMC_ENABLE_MVC_VIDEO_ENCODER
    #define UMC_ENABLE_MPEG2_VIDEO_ENCODER
    #define UMC_ENABLE_MJPEG_VIDEO_ENCODER

    #define UMC_ENABLE_UMC_SCENE_ANALYZER

// audio decoders
    #define UMC_ENABLE_AAC_AUDIO_DECODER
    #define UMC_ENABLE_MP3_AUDIO_DECODER

    // audio encoders
    #define UMC_ENABLE_AAC_AUDIO_ENCODER
    #define UMC_ENABLE_MP3_AUDIO_ENCODER

#endif // Winx64 on EM64T

/*
// Linux on IA32
*/

#if defined(LINUX32) || defined(__APPLE__)

    // video decoders
    #define UMC_ENABLE_MJPEG_VIDEO_DECODER
    //#define UMC_ENABLE_VP8_VIDEO_DECODER
    #define UMC_ENABLE_VC1_VIDEO_DECODER
    #define UMC_ENABLE_H264_VIDEO_DECODER
    #define UMC_ENABLE_H265_VIDEO_DECODER
    #define UMC_ENABLE_MPEG2_VIDEO_DECODER
    //#define UMC_ENABLE_VP9_VIDEO_DECODER

    // video encoders
    #define UMC_ENABLE_H264_VIDEO_ENCODER
    #define UMC_ENABLE_MVC_VIDEO_ENCODER
    #define UMC_ENABLE_MPEG2_VIDEO_ENCODER
    #define UMC_ENABLE_MPEG4_VIDEO_ENCODER
    #define UMC_ENABLE_MJPEG_VIDEO_ENCODER

    #define UMC_ENABLE_UMC_SCENE_ANALYZER

    // audio decoders
    #define UMC_ENABLE_AAC_AUDIO_DECODER
    #define UMC_ENABLE_MP3_AUDIO_DECODER

    // audio encoders
    #define UMC_ENABLE_AAC_AUDIO_ENCODER
    //#define UMC_ENABLE_AC3_AUDIO_ENCODER
    //#define UMC_ENABLE_MP3_AUDIO_ENCODER

#endif // Linux on IA32


#ifdef __cplusplus

namespace UMC
{

};

#endif //__cplusplus

#ifndef OPEN_SOURCE

    // readers/writers
    #define UMC_ENABLE_FILE_READER
    #define UMC_ENABLE_FIO_READER
    #define UMC_ENABLE_FILE_WRITER

    // splitters
    #define UMC_ENABLE_AVI_SPLITTER
    #define UMC_ENABLE_MPEG2_SPLITTER
    #define UMC_ENABLE_MP4_SPLITTER
    #define UMC_ENABLE_VC1_SPLITTER
    #define UMC_ENABLE_H264_SPLITTER

#include "ipps.h"
#define MFX_INTERNAL_CPY(dst, src, size) ippsCopy_8u((const Ipp8u *)(src), (Ipp8u *)(dst), (int)size)
#define MFX_INTERNAL_CPY_S(dst, dstsize, src, src_size) ippsCopy_8u((const Ipp8u *)(src), (Ipp8u *)(dst), (int)dstsize)
#else
#include <stdint.h>

#ifdef __cplusplus
#include <algorithm>
#endif //__cplusplus

#define MFX_INTERNAL_CPY_S(dst, dstsize, src, src_size) memcpy_s((Ipp8u *)(dst), (uint8_t)(dstsize), (const Ipp8u *)(src), (int)src_size)
#define MFX_INTERNAL_CPY(dst, src, size) std::copy((const uint8_t *)(src), (const uint8_t *)(src) + (int)(size), (uint8_t *)(dst))

#define MFX_MAX( a, b ) ( ((a) > (b)) ? (a) : (b) )
#define MFX_MIN( a, b ) ( ((a) < (b)) ? (a) : (b) )

#define MFX_MAX_32S    ( 2147483647 )
#define MFX_MAXABS_64F ( 1.7976931348623158e+308 )
#define MFX_MAX_32U    ( 0xFFFFFFFF )

#if defined( _WIN32 ) || defined ( _WIN64 )
  #define MFX_MAX_64S  ( 9223372036854775807i64 )
#else
  #define MFX_MAX_64S  ( 9223372036854775807LL )
#endif

typedef struct {
    int width;
    int height;
} mfxSize;

#if defined( _WIN32 ) || defined ( _WIN64 )
  #define __STDCALL  __stdcall
  #define __CDECL    __cdecl
#else
  #define __STDCALL
  #define __CDECL
#endif

#endif

#if defined(_WIN32) || defined(_WIN64)
  #define ALIGN_DECL(X) __declspec(align(X))
#else
  #define ALIGN_DECL(X) __attribute__ ((aligned(X)))
#endif

#if (defined(_MSC_VER) && _MSC_VER >= 1900)
/*
this is a hot fix for C++11 compiler
By default, the compiler generates implicit noexcept(true) specifiers for user-defined destructors and deallocator functions
https://msdn.microsoft.com/en-us/library/84e2zwhh.aspx
Exception Specifications (throw) is deprecated in C++11.
https://msdn.microsoft.com/en-us/library/wfa0edys.aspx
So must use noexcept(true)
http://en.cppreference.com/w/cpp/language/noexcept_spec
http://en.cppreference.com/w/cpp/language/destructor
Non-throwing function are permitted to call potentially-throwing functions.
Whenever an exception is thrown and the search for a handler encounters the outermost block of a non-throwing function, the function std::terminate is called
So we MUST fix this warning
*/
#define THROWSEXCEPTION noexcept(false)
#else
#define THROWSEXCEPTION
#endif
/******************************************************************************/

#endif // __UMC_DEFS_H__
