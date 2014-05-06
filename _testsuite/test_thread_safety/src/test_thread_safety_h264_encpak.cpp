/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2011 Intel Corporation. All Rights Reserved.

File Name: test_thread_safety_h264_encode.cpp

\* ****************************************************************************** */

#include "vm_types.h"
#include "test_thread_safety_encode.h"

#ifdef ENABLE_TEST_THREAD_SAFETY_FOR_H264_ENCPAK

#ifdef _tmain
#undef _tmain
#endif

#ifdef _tcprintf
#undef _tcprintf
#endif
#define _tcprintf

#ifdef _tprintf
#undef _tprintf
#endif
#define _tprintf

#ifndef printf
#define printf printf_GlobalFunc
#endif

#define CBitstreamWriter CBitstreamWriterChild
#define _tmain H264EncPakStarter

#include "enc_pak_utils.cpp"
#include "enc_pak_test.cpp"

#undef _tmain
#undef CBitstreamWriter
#undef printf
#undef _tprintf
#undef _tcprintf

#else //ENABLE_TEST_THREAD_SAFETY_FOR_H264_ENCPAK

int H264EncPakStarter(int, vm_char** const) { return 1; }

#endif //ENABLE_TEST_THREAD_SAFETY_FOR_H264_ENCPAK
