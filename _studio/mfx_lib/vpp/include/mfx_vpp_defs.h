//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2010-2016 Intel Corporation. All Rights Reserved.
//

/* ****************************************************************************** */

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#ifndef __MFX_VPP_DEFS_H
#define __MFX_VPP_DEFS_H

#include "mfxvideo++int.h"
#include "mfx_ext_buffers.h"

#define MAX_NUM_VPP_FILTERS (15)
#define MAX_NUM_CONNECTIONS (MAX_NUM_VPP_FILTERS-1)

#define VPP_MAX_REQUIRED_FRAMES_COUNT (7) // == MAX( inputFramesCount, outputFramesCount )

// (DN, DTL, ProcAmp, Gamut, TCC, STE)
#define MAX_NUM_OF_VPP_CONFIG_FILTERS (6)

// Max samples number for composition filter
#if defined(_WIN32) || defined(_WIN64)
    #define MAX_NUM_OF_VPP_COMPOSITE_STREAMS (8)
#else
    #define MAX_NUM_OF_VPP_COMPOSITE_STREAMS (64)
#endif

// (DO_NOT_USE, DO_USE) + (MAX_NUM_OF_VPP_CONFIG_FILTERS)
#define MAX_NUM_OF_VPP_EXT_PARAM      (2 + MAX_NUM_OF_VPP_CONFIG_FILTERS)

// common utility for vpp filters  see spec
#define VPP_GET_WIDTH(info, width)          \
{                                           \
    width = info->CropW;                    \
    if (width == 0 ||                       \
    (width > info->Width && info->Width > 0)) \
    width = info->Width;                    \
}

#define VPP_GET_HEIGHT(info, height)           \
{                                              \
    height = info->CropH;                      \
    if (height == 0 ||                         \
    (height > info->Height && info->Height > 0)) \
    height = info->Height;                     \
}

#define VPP_GET_REAL_WIDTH(info, width)  {width = info->Width;}

#define VPP_GET_REAL_HEIGHT(info, height){height = info->Height;}

#define VPP_GET_CROPX(info, cropX){cropX = info->CropX;}

#define VPP_GET_CROPY(info, cropY){cropY = info->CropY;}

// macros for processing
#define VPP_INIT_SUCCESSFUL  { m_errPrtctState.isInited = true; }

#define VPP_CHECK_MULTIPLE_INIT   {if( m_errPrtctState.isInited ) return MFX_ERR_UNDEFINED_BEHAVIOR;}

#define VPP_CHECK_NOT_INITIALIZED {if( !m_errPrtctState.isInited ) return MFX_ERR_NOT_INITIALIZED;}

#define VPP_CLEAN       { m_errPrtctState.isInited = false; m_errPrtctState.isFirstFrameProcessed = false; }

#define VPP_RESET       { m_errPrtctState.isFirstFrameProcessed = false; }

#define VPP_RANGE_CLIP(val, min_val, max_val)   IPP_MAX( IPP_MIN(max_val, val), min_val )

// error processing
#define VPP_CHECK_STS_CONTINUE(sts1, sts2) { if (sts1 != MFX_ERR_NONE) sts2 = sts1; }

#define VPP_CHECK_IPP_STS(err) if (err != ippStsNoErr) {return MFX_ERR_UNDEFINED_BEHAVIOR;}

#define IPP_CHECK_STS(err) if (err != ippStsNoErr) {return err;}

#define IPP_CHECK_NULL_PTR1(pointer) { if (!(pointer)) return ippStsNullPtrErr; }
#define VPP_IGNORE_MFX_STS(P, X)   {if ((X) == (P)) {P = MFX_ERR_NONE;}}

// memory allign
#define VPP_ALIGN32(X) (((mfxU32)((X)+31)) & (~ (mfxU32)31))

// names of VPP smart filters. internal using only
enum {
    MFX_EXTBUFF_VPP_CSC          =   MFX_MAKEFOURCC('C','S','C','F'),//COLOR SPACE CONVERSION FILTER
    MFX_EXTBUFF_VPP_RESIZE       =   MFX_MAKEFOURCC('R','S','Z','F'),
    MFX_EXTBUFF_VPP_DI           =   MFX_MAKEFOURCC('S','D','I','F'),//STANDARD DEINTERLACE FILTER (60i->30p)
    MFX_EXTBUFF_VPP_DI_30i60p    =   MFX_MAKEFOURCC('F','D','I','F'),//FULL PTS DEINTERLACE FILTER (60i->60p)
    MFX_EXTBUFF_VPP_DI_WEAVE     =   MFX_MAKEFOURCC('F','D','I','W'),//WEAVE DEINTERLACE FILTER (60i->30p)
    MFX_EXTBUFF_VPP_ITC          =   MFX_MAKEFOURCC('I','T','C','F'), //INV TELECINE FILTER

    // MSDK 2013 requires [NV12, RGB4] as output.SW algorithm designed for NV12 only.
    // the best solution is (XXX->NV12) + VPP + (NV12->RGB4)
    MFX_EXTBUFF_VPP_CSC_OUT_RGB4    =   MFX_MAKEFOURCC('C','S','R','4'), //COLOR SPACE CONVERSION FILTER
    MFX_EXTBUFF_VPP_CSC_OUT_A2RGB10 =   MFX_MAKEFOURCC('C','S','1','0'), //COLOR SPACE CONVERSION FILTER
    MFX_EXTBUFF_VPP_RSHIFT_IN       =   MFX_MAKEFOURCC('R','S','F','I'),
    MFX_EXTBUFF_VPP_LSHIFT_IN       =   MFX_MAKEFOURCC('L','S','F','I'),
    MFX_EXTBUFF_VPP_RSHIFT_OUT      =   MFX_MAKEFOURCC('R','S','F','O'),
    MFX_EXTBUFF_VPP_LSHIFT_OUT      =   MFX_MAKEFOURCC('L','S','F','O'),
    MFX_EXTBUFF_VPP_FIELD_WEAVING   =   MFX_MAKEFOURCC('F','W','E','F'),
    MFX_EXTBUFF_VPP_FIELD_DEWEAVING =   MFX_MAKEFOURCC('F','D','W','F'),
};

typedef enum {
    MFX_REQUEST_FROM_VPP_CHECK    = 0x0001,
    MFX_REQUEST_FROM_VPP_PROCESS = 0x0002

} mfxRequestType;

enum {
    VPP_IN  = 0x00,
    VPP_OUT = 0x01
};

typedef enum {
    PASS_THROUGH_PICSTRUCT_MODE   = 0x0001,
    DYNAMIC_DI_PICSTRUCT_MODE     = 0x0004,
    ERR_PICSTRUCT_MODE            = 0x0000

} PicStructMode;

typedef enum
{
    // gamut compression
    GAMUT_COMPRESS_BASE_MODE     = 1,
    GAMUT_COMPRESS_ADVANCED_MODE = 2,

    // RGB2YUV tuning
    GAMUT_PASSIVE_MODE  = 3,

    // gamut expansion
    // none

    // invalid mode
    GAMUT_INVALID_MODE  = 100

} mfxGamutMode;


// frames for internal float-point calculation of SW algorithms
typedef struct
{
    union {
        Ipp32f* pY;
        Ipp32f* pL;
    };
    union {
        Ipp32f* pUV;
        Ipp32f* pCH;
    };

    mfxU32  FourCC;

    mfxU16  Pitch;
    mfxU16  Width;
    mfxU16  Height;

} Surface1_32f;

typedef enum
{
    MIRROR_INPUT   = 0x0001,
    MIRROR_OUTPUT  = 0x0002,
    MIRROR_WO_EXEC = 0x0004
} MirroringPositions;

#endif // __MFX_VPP_DEFS_H
#endif // MFX_ENABLE_VPP
/*EOF*/
