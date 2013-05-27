/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: mfxlinux.h

*******************************************************************************/
#ifndef __MFXLINUX_H__
#define __MFXLINUX_H__

#include "mfxdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*  mfxHandleType */
typedef enum {
    MFX_HANDLE_VA_DISPLAY                       = 4
};

enum  {
    MFX_IMPL_VIA_VAAPI    = 0x0400
};

#ifdef __cplusplus
} // extern "C"
#endif

#endif

