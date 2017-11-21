/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2017 Intel Corporation. All Rights Reserved.

File Name: test_thread_safety.h

\* ****************************************************************************** */

#ifndef __TEST_THREAD_SAFETY_H__
#define __TEST_THREAD_SAFETY_H__

#define ENABLE_TEST_THREAD_SAFETY_FOR_H264_DECODE
#define ENABLE_TEST_THREAD_SAFETY_FOR_H264_ENCODE
//#define ENABLE_TEST_THREAD_SAFETY_FOR_H264_ENCPAK
#define ENABLE_TEST_THREAD_SAFETY_FOR_MPEG2_DECODE
#define ENABLE_TEST_THREAD_SAFETY_FOR_MPEG2_ENCODE
//#define ENABLE_TEST_THREAD_SAFETY_FOR_MPEG2_ENCPAK
#define ENABLE_TEST_THREAD_SAFETY_FOR_VC1_DECODE
//#define ENABLE_TEST_THREAD_SAFETY_FOR_VC1_ENCODE
//#define ENABLE_TEST_THREAD_SAFETY_FOR_VC1_ENCPAK

#include "mfxdefs.h"
#ifdef assert
#undef assert
#endif //assert
#include "test_thread_safety_output.h"

const vm_char DefaultOutputFileName[] = VM_STRING("default_output_file_name_for_output.1234567890");

enum { SYNC_OPT_NO_SYNC = 0, SYNC_OPT_PER_WRITE = 1 };

enum { TEST_JPEGDECODE
     , TEST_H264DECODE
     , TEST_MPEG2DECODE
     , TEST_MVCDECODE
     , TEST_SVCDECODE
     , TEST_HEVCDECODE
     , TEST_VC1DECODE
     , TEST_VP8DECODE
     , TEST_VP9DECODE
     , TEST_H263DECODE
     
     , TEST_JPEGENCODE  
     , TEST_H264ENCODE
     , TEST_MPEG2ENCODE
     , TEST_MVCENCODE
     , TEST_HEVCENCODE
     , TEST_VC1ENCODE
     , TEST_VP8ENCODE
     , TEST_H263ENCODE
     , TEST_VP9ENCODE

     , TEST_UNDEF 
};

inline int printf_GlobalFunc(const char*, ...) { return 0; }

#endif //__TEST_THREAD_SAFETY_H__
