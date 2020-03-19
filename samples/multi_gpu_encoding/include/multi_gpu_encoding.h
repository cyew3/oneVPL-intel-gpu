/* ****************************************************************************** *\

Copyright (C) 2020 Intel Corporation.  All rights reserved.

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

File Name: multi_gpu_encoding.h

\* ****************************************************************************** */

#ifndef __MULTIGPUENCODING_H
#define __MULTIGPUENCODING_H

#include "mfxvideo.h"
#include "mfxvideo++.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* VideoMultiGpuENCODE */
mfxStatus MFX_CDECL MFXVideoMultiGpuENCODE_Query(mfxSession session, mfxVideoParam* in, mfxVideoParam* out);
mfxStatus MFX_CDECL MFXVideoMultiGpuENCODE_QueryIOSurf(mfxSession session, mfxVideoParam* par, mfxFrameAllocRequest* request);
mfxStatus MFX_CDECL MFXVideoMultiGpuENCODE_Init(mfxSession session, mfxVideoParam* par);
mfxStatus MFX_CDECL MFXVideoMultiGpuENCODE_Reset(mfxSession session, mfxVideoParam* par);
mfxStatus MFX_CDECL MFXVideoMultiGpuENCODE_Close(mfxSession session);

mfxStatus MFX_CDECL MFXVideoMultiGpuENCODE_GetVideoParam(mfxSession session, mfxVideoParam* par);
mfxStatus MFX_CDECL MFXVideoMultiGpuENCODE_GetEncodeStat(mfxSession session, mfxEncodeStat* stat);
mfxStatus MFX_CDECL MFXVideoMultiGpuENCODE_EncodeFrameAsync(mfxSession session, mfxEncodeCtrl* ctrl, mfxFrameSurface1* surface, mfxBitstream* bs, mfxSyncPoint* syncp);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
