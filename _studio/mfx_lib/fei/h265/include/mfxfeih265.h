/******************************************************************************* *\

Copyright (C) 2014 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfxfeih265.h

*******************************************************************************/
#ifndef __MFXFEIH265_H__
#define __MFXFEIH265_H__

#include "mfxdefs.h"
#include "mfxstructures.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

typedef void *mfxFEIH265;
typedef void *mfxFEISyncPoint;

#define FEI_INTERP_BORDER       4
#define FEI_MAX_NUM_REF_FRAMES  8
#define FEI_MAX_INTRA_MODES     33  /* intra angular prediction modes (excluding DC and planar) */

enum FEIBlockSize {
    FEI_256x256 = 0,
    FEI_128x128 = 1,
    FEI_64x64 = 2,
    FEI_32x32 = 3,
    FEI_32x16 = 4,
    FEI_16x32 = 5,
    FEI_16x16 = 6,
    FEI_8x8   = 9,

    FEI_BLK_MAX = 12,
};

enum FEIOperation
{
    FEI_NOP         = 0x00,
    FEI_INTRA_MODE  = 0x01,
    FEI_INTRA_DIST  = 0x02,
    FEI_INTER_ME    = 0x04,
    FEI_INTERPOLATE = 0x08,
};

enum FEIFrameType
{
    FEI_I_FRAME = 1,
    FEI_P_FRAME = 2,
    FEI_B_FRAME = 4,
};

typedef struct
{
    mfxU16 Dist;
    mfxU16 reserved;
} mfxFEIH265IntraDist;

/* input parameters - set once at init */
typedef struct
{
    mfxI32 Width;
    mfxI32 Height;
    mfxI32 MaxCUSize;
    mfxI32 MPMode;
    mfxI32 NumRefFrames;
    mfxI32 NumIntraModes;
} mfxFEIH265Param;

/* basic info for current and reference frames */
typedef struct
{
    mfxU8* YPlane;
    mfxI32 YPitch;
    mfxI32 PicOrder;
    mfxI32 EncOrder;
} mfxFEIH265Frame;

/* FEI input - update before each call to ProcessFrameAsync */
typedef struct
{
    FEIOperation FEIOp;
    FEIFrameType FrameType;
    mfxI32 RefIdx;

    mfxFEIH265Frame FEIFrameIn;
    mfxFEIH265Frame FEIFrameRef;

} mfxFEIH265Input;

/* FEI output for current frame - can be used directly in SW encoder */
typedef struct
{
    /* intra processing pads to multiple of 16 if needed */
    mfxI32                PaddedWidth;
    mfxI32                PaddedHeight;

    /* ranked list of IntraMaxModes modes for each block size */
    mfxI32                IntraMaxModes;
    mfxI32              * IntraModes4x4;
    mfxI32              * IntraModes8x8;
    mfxI32              * IntraModes16x16;
    mfxI32              * IntraModes32x32;
    mfxU32                IntraPitch4x4;
    mfxU32                IntraPitch8x8;
    mfxU32                IntraPitch16x16;
    mfxU32                IntraPitch32x32;

    /* intra distortion estiamates (16x16 grid) */
    mfxI32                IntraPitch;
    mfxFEIH265IntraDist * IntraDist;

    /* motion vector estimates for multiple block sizes */
    mfxI32                PitchDist[FEI_BLK_MAX];
    mfxI32                PitchMV[FEI_BLK_MAX];
    mfxU32              * Dist[FEI_MAX_NUM_REF_FRAMES][FEI_BLK_MAX];
    mfxI16Pair          * MV[FEI_MAX_NUM_REF_FRAMES][FEI_BLK_MAX];

    /* half-pel interpolated planes - 4-pixel border added on all sides */
    mfxI32                InterpolateWidth;
    mfxI32                InterpolateHeight;
    mfxI32                InterpolatePitch;
    mfxU8               * Interp[FEI_MAX_NUM_REF_FRAMES][3];

} mfxFEIH265Output;

enum 
{
    MFX_EXTBUFF_FEIH265_PARAM  =   MFX_MAKEFOURCC('F','E','5','P'),
    MFX_EXTBUFF_FEIH265_INPUT  =   MFX_MAKEFOURCC('F','E','5','I'),
    MFX_EXTBUFF_FEIH265_OUTPUT =   MFX_MAKEFOURCC('F','E','5','O'),
};

typedef struct
{
    mfxExtBuffer       Header;
    mfxFEIH265Param    feiParam;
} mfxExtFEIH265Param;

typedef struct
{
    mfxExtBuffer       Header;
    mfxFEIH265Input    feiIn;
} mfxExtFEIH265Input;

typedef struct
{
    mfxExtBuffer       Header;
    mfxFEIH265Output  *feiOut;
} mfxExtFEIH265Output;


#ifdef __cplusplus
}
#endif

#endif  /* __MFXFEIH265_H__ */
