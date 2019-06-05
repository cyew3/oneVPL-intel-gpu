/******************************************************************************* *\

Copyright (C) 2016-2019 Intel Corporation.  All rights reserved.

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

File Name: mfxbrc.h

*******************************************************************************/
#ifndef __MFXBRC_H__
#define __MFXBRC_H__

#include "mfxvstructures.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* Extended Buffer Ids */
enum {
    MFX_EXTBUFF_BRC = MFX_MAKEFOURCC('E','B','R','C')
};

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {
#if (MFX_VERSION >= 1026)
    mfxU32 reserved[23];
    mfxU16 SceneChange;     // Frame is Scene Chg frame
    mfxU16 LongTerm;        // Frame is long term refrence
    mfxU32 FrameCmplx;      // Frame Complexity
#else
    mfxU32 reserved[25];
#endif
    mfxU32 EncodedOrder;    // Frame number in a sequence of reordered frames starting from encoder Init()
    mfxU32 DisplayOrder;    // Frame number in a sequence of frames in display order starting from last IDR
    mfxU32 CodedFrameSize;  // Size of frame in bytes after encoding
    mfxU16 FrameType;       // See FrameType enumerator
    mfxU16 PyramidLayer;    // B-pyramid or P-pyramid layer, frame belongs to
    mfxU16 NumRecode;       // Number of recodings performed for this frame
    mfxU16 NumExtParam;
    mfxExtBuffer** ExtParam;
} mfxBRCFrameParam;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {
    mfxI32 QpY;             // Frame-level Luma QP
#if (MFX_VERSION >= 1029)
    mfxU32 InitialCpbRemovalDelay;
    mfxU32 InitialCpbRemovalOffset;
    mfxU32 reserved1[7];
    mfxU32 MaxFrameSize;    // Max frame size in bytes (used for rePak)
    mfxU8  DeltaQP[8];      // deltaQP[i] is adding to QP value while i-rePak
    mfxU16 MaxNumRepak;     // Max number of rePak to provide MaxFrameSize (from 0 to 8)
    mfxU16 NumExtParam;
    mfxExtBuffer** ExtParam;   // extension buffer list
#else
    mfxU32 reserved1[13];
    mfxHDL reserved2;
#endif
} mfxBRCFrameCtrl;
MFX_PACK_END()

/* BRCStatus */
enum {
    MFX_BRC_OK                = 0, // CodedFrameSize is acceptable, no further recoding/padding/skip required
    MFX_BRC_BIG_FRAME         = 1, // Coded frame is too big, recoding required
    MFX_BRC_SMALL_FRAME       = 2, // Coded frame is too small, recoding required
    MFX_BRC_PANIC_BIG_FRAME   = 3, // Coded frame is too big, no further recoding possible - skip frame
    MFX_BRC_PANIC_SMALL_FRAME = 4  // Coded frame is too small, no further recoding possible - required padding to MinFrameSize
};

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
    mfxU32 MinFrameSize;    // Size in bytes, coded frame must be padded to when Status = MFX_BRC_PANIC_SMALL_FRAME
    mfxU16 BRCStatus;       // See BRCStatus enumerator
    mfxU16 reserved[25];
    mfxHDL reserved1;
} mfxBRCFrameStatus;
MFX_PACK_END()

/* Structure contains set of callbacks to perform external bit-rate control.
Can be attached to mfxVideoParam structure during encoder initialization.
Turn mfxExtCodingOption2::ExtBRC option ON to make encoder use external BRC instead of native one. */
MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {
    mfxExtBuffer Header;

    mfxU32 reserved[14];
    mfxHDL pthis; // Pointer to user-defined BRC instance. Will be passed to each mfxExtBRC callback.

    // Initialize BRC. Will be invoked during encoder Init(). In - pthis, par.
    mfxStatus (MFX_CDECL *Init)         (mfxHDL pthis, mfxVideoParam* par);

    // Reset BRC. Will be invoked during encoder Reset(). In - pthis, par.
    mfxStatus (MFX_CDECL *Reset)        (mfxHDL pthis, mfxVideoParam* par);

    // Close BRC. Will be invoked during encoder Close(). In - pthis.
    mfxStatus (MFX_CDECL *Close)        (mfxHDL pthis);

    // Obtain from BRC controls required for frame encoding.
    // Will be invoked BEFORE encoding of each frame. In - pthis, par; Out - ctrl.
    mfxStatus (MFX_CDECL *GetFrameCtrl) (mfxHDL pthis, mfxBRCFrameParam* par, mfxBRCFrameCtrl* ctrl);

    // Update BRC state and return command to continue/recode frame/do padding/skip frame.
    // Will be invoked AFTER encoding of each frame. In - pthis, par, ctrl; Out - status.
    mfxStatus (MFX_CDECL *Update)       (mfxHDL pthis, mfxBRCFrameParam* par, mfxBRCFrameCtrl* ctrl, mfxBRCFrameStatus* status);

    mfxHDL reserved1[10];
} mfxExtBRC;
MFX_PACK_END()

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */

#endif

