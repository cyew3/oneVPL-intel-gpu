/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2011 Intel Corporation. All Rights Reserved.

File Name: test_thread_safety_vc1_encode.cpp

\* ****************************************************************************** */

#include "vm_types.h"
#include "test_thread_safety_encode.h"

#ifdef ENABLE_TEST_THREAD_SAFETY_FOR_VC1_ENCPAK

#ifdef _tmain
#undef _tmain
#endif

#ifdef _tcprintf
#undef _tcprintf
#endif

#define _tcprintf
#define printf
#define CBitstreamWriter CBitstreamWriterChild
#define _tmain VC1EncPakStarter

//#include "enc_pak/vc1/src/mfx_vc1_encoder_params.cpp"
//#include "enc_pak/vc1/src/vc1_video_encoder_utils.cpp"
#include "test_vc1_enc_pak.cpp"

#undef _tmain
#undef CBitstreamWriter
#undef printf
#undef _tcprintf

#else //ENABLE_TEST_THREAD_SAFETY_FOR_VC1_ENCPAK

int VC1EncPakStarter(int, vm_char** const) { return 1; }

#endif //ENABLE_TEST_THREAD_SAFETY_FOR_VC1_ENCPAK
