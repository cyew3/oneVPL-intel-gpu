/******************************************************************************* *\

Copyright (C) 2007-2016 Intel Corporation.  All rights reserved.

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

File Name: mfxvp9.h

*******************************************************************************/
#ifndef __MFXVP9_H__
#define __MFXVP9_H__

#include "mfxdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Extended Buffer Ids */
enum {
    MFX_EXTBUFF_CODING_OPTION_VP9 =   MFX_MAKEFOURCC('I','V','P','9'),
    MFX_EXTBUFF_VP9_DECODED_FRAME_INFO = MFX_MAKEFOURCC('9','D','F','I')
};

enum {
    MFX_VP9_RASCTRL_REFINT  = (0x01 | (0x0<<1)),
    MFX_VP9_RASCTRL_REFLST  = (0x01 | (0x1<<1)),
    MFX_VP9_RASCTRL_REFGLD  = (0x01 | (0x2<<1)),
    MFX_VP9_RASCTRL_REFALT  = (0x01 | (0x3<<1)),
    MFX_VP9_RASCTRL_SKIPZMV = (0x1<<4)
};

typedef struct {
    mfxU16  ReferenceAndSkipCtrl;
    mfxI16  LoopFilterLevelDelta;
    mfxI16  QIndexDelta;
    mfxU16  reserved[13];
} mfxSegmentParamVP9;

typedef struct { 
    mfxExtBuffer Header;

    mfxU16  Version;
    mfxU16  SharpnessLevel;
    mfxU16  WriteIVFHeaders;        /* tri-state option */
    mfxU32  NumFramesForIVF;

    mfxI16  LoopFilterRefDelta[4];
    mfxI16  LoopFilterModeDelta[2];

    mfxI16  QIndexDeltaLumaDC;
    mfxI16  QIndexDeltaChromaAC;
    mfxI16  QIndexDeltaChromaDC;

    mfxU16  Log2TileRows;
    mfxU16  Log2TileColumns;
    
    mfxU16  reserved1;
    mfxU16  EnableMultipleSegments; /* tri-state option */

    mfxSegmentParamVP9 Segment[8];

    mfxU16   reserved[106];
} mfxExtCodingOptionVP9;

typedef struct {
    mfxExtBuffer Header;

    mfxU16       DisplayWidth;
    mfxU16       DisplayHeight;
    mfxU16       reserved[58];
} mfxExtVP9DecodedFrameInfo;


#ifdef __cplusplus
} // extern "C"
#endif

#endif

