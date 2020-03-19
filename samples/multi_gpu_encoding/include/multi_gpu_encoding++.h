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

File Name: multi_gpu_encoding++.h

\* ****************************************************************************** */

#ifndef __MULTIGPUENCODINGPLUSPLUS_H
#define __MULTIGPUENCODINGPLUSPLUS_H

#include "multi_gpu_encoding.h"

class MFXVideoMultiGpuENCODE: public MFXVideoENCODE
{
public:

    MFXVideoMultiGpuENCODE(mfxSession session)
        :MFXVideoENCODE(session)
    {
        m_session = session;
    }

    virtual ~MFXVideoMultiGpuENCODE(void) { Close(); }

    mfxStatus Query(mfxVideoParam* in, mfxVideoParam* out) override { return MFXVideoMultiGpuENCODE_Query(m_session, in, out); }
    mfxStatus QueryIOSurf(mfxVideoParam* par, mfxFrameAllocRequest* request) override { return MFXVideoMultiGpuENCODE_QueryIOSurf(m_session, par, request); }
    mfxStatus Init(mfxVideoParam* par) override { return MFXVideoMultiGpuENCODE_Init(m_session, par); }
    mfxStatus Reset(mfxVideoParam* par) override { return MFXVideoMultiGpuENCODE_Reset(m_session, par); }
    mfxStatus Close(void) override { return MFXVideoMultiGpuENCODE_Close(m_session); }

    mfxStatus GetVideoParam(mfxVideoParam* par) override { return MFXVideoMultiGpuENCODE_GetVideoParam(m_session, par); }
    mfxStatus GetEncodeStat(mfxEncodeStat* stat) override { return MFXVideoMultiGpuENCODE_GetEncodeStat(m_session, stat); }

    mfxStatus EncodeFrameAsync(mfxEncodeCtrl* ctrl, mfxFrameSurface1* surface, mfxBitstream* bs, mfxSyncPoint* syncp) override { return MFXVideoMultiGpuENCODE_EncodeFrameAsync(m_session, ctrl, surface, bs, syncp); }

protected:

    mfxSession m_session;                                       // (mfxSession) handle to the owning session
};

#endif

