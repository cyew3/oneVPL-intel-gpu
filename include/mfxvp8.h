/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010 - 2012 Intel Corporation. All Rights Reserved.

File Name: mfxvp8.h

*******************************************************************************/
#ifndef __MFXVP8_H__
#define __MFXVP8_H__

#include "mfxdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MFX_CODEC_VP8 = MFX_MAKEFOURCC('V','P','8',' '),
};

/* CodecProfile*/
enum {
    MFX_PROFILE_VP8_0                       = 0+1, 
    MFX_PROFILE_VP8_1                       = 1+1,
    MFX_PROFILE_VP8_2                       = 2+1,
    MFX_PROFILE_VP8_3                       = 3+1,
};
/*Token partitions*/
enum {
    MFX_TOKENPART_VP8_UNKNOWN               = 0,
    MFX_TOKENPART_VP8_1                     = 0+1, 
    MFX_TOKENPART_VP8_2                     = 1+1,
    MFX_TOKENPART_VP8_4                     = 2+1,
    MFX_TOKENPART_VP8_8                     = 3+1,
};

/* Extended Buffer Ids */
enum {
    MFX_EXTBUFF_VP8_EX_CODING_OPT =   MFX_MAKEFOURCC('V','P','8','E'),
};

typedef struct { 
    mfxExtBuffer    Header;
    mfxU16          EnableAutoAltRef;        /* tri-state option */
    mfxU16          TokenPartitions;         /* see enum above   */
    mfxU16          EnableMultipleSegments;  /* tri-state option */
    mfxU16          reserved[9];
} mfxExtCodingOptionVP8;


#ifdef __cplusplus
} // extern "C"
#endif

#endif

