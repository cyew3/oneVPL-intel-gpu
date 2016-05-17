//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2016 Intel Corporation. All Rights Reserved.
//

#pragma once

#if defined(_WIN32) || defined(_WIN64)
  #include <windows.h>
  #include <psapi.h>
  #include <tchar.h>
#else
#define _T(x) x
#endif
#include <map>

#include "sample_defs.h"
#include "sample_utils.h"
#include "mfxvideo++.h"

#define TUM_WAIT_INTERVAL (6000)
#define TUM_ERR_STS        (897)
#define TUM_OK_STS         (0)

#define PRINT_USER_MSG( MSG )     { msdk_printf(MSDK_STRING("\n\nReturn on error: %s\n"), MSG); }
#define RETURN_ON_ERROR(MSG, ERR) { PRINT_USER_MSG(MSG); return ERR; }

//typedef mfxU32 (__stdcall *tum_thread_callback)(void *);

// "_" means unique session for components.
// example: 
// DEC_VPP_ENC_SESSION - unique session for DEC/VPP/ENC
// DECENC_VPP_SESSION  - the same session for DEC and ENC and other session for VPP
typedef enum {
    DEC_VPP_ENC_SESSION = 0, 
    DECVPP_ENC_SESSION  = 1,
    DECENC_VPP_SESSION  = 2,
    DEC_VPPENC_SESSION  = 3,
    DECVPPENC_SESSION   = 4,

    // Join Session modes
    DECVPP_ENC_JOIN_SESSION,
    DEC_VPPENC_JOIN_SESSION,
    DEC_VPP_ENC_JOIN_SESSION

    // MultiSessionMode
    // TBD

} SessionMode;

#define MFX_IMPL_NOT_INIT  (-1)

struct AppParam
{
    mfxU32 srcVideoFormat;    
    
    mfxU32 dstVideoFormat;    
    mfxU16 width; // desired output width.  optional   
    mfxU16 height;// desired output height. optional
    mfxU32 bitRate;
    mfxF64 frameRate;
    mfxU16 targetUsage;    

    mfxU16 asyncDepth; // asyncronous queue
    mfxU16 usageModel;

    mfxU32 framesCount;// frames to be encoded

    mfxU16 NumSlice;

    const msdk_char* pSrcFileName;
    const msdk_char* pDstFileName;

    std::map<const msdk_char*, mfxIMPL>  impLib; // type of MediaSDK library (HW/SW)

    mfxU16   IOPattern;

    SessionMode sessionMode; 
};

enum {
    USAGE_MODEL_REFERENCE = 0, // the same thread for DEC/VPP/ENC/SYNC && async_depth=1   && common mfx_session
    USAGE_MODEL_1 = 1,         // unique thread per component and SYNC && async_depth=ANY && common mfx_session
    USAGE_MODEL_2 = 2,         // unique thread per component and SYNC && async_depth=ANY && different mfx_session
    USAGE_MODEL_3,             // unique thread per component and SYNC && async_depth=ANY && ses1(DEC+VPP) + ses2(ENC)
    USAGE_MODEL_4,             // unique thread per component and SYNC && async_depth=ANY && ses1(DEC+ENC) + ses2(VPP)
    USAGE_MODEL_5,             // unique thread per component and SYNC && async_depth=ANY && ses1(DEC) + ses2(VPP+ENC)
    // Join Session Mode
    USAGE_MODEL_6,             // means join( ses1(DEC+VPP) + ses2(ENC) ) + unique thread per component and SYNC && async_depth=ANY
    USAGE_MODEL_7,             // means join( ses1(DEC) + ses2(VPP+ENC) ) + unique thread per component and SYNC && async_depth=ANY
    USAGE_MODEL_8,              // means join( ses1(DEC)+ses2(VPP)+ses3(ENC) ) + unique thread per component and SYNC && async_depth=ANY 
    // Reference for Join Session models
    USAGE_MODEL_JOIN_REFERENCE
};

// rules:
// (1) function replaces IN<->OUT. 
// (2) Type of memory (sys/d3d) isn't modified.
// (3) mixed Pattern (ex IN_SYSTEM_MEMORY|OUT_VIDEO_MEMORY) not supported
mfxU16 InvertIOPattern( mfxU16 inPattern );

/* EOF */
