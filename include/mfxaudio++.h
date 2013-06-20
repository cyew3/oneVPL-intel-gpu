/* ****************************************************************************** *\

Copyright (C) 2007-2013 Intel Corporation.  All rights reserved.

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


File Name: mfxaudio++.h

\* ****************************************************************************** */
#ifndef __MFXAUDIOPLUSPLUS_H
#define __MFXAUDIOPLUSPLUS_H

#include "mfxaudio.h"

class MFXAudioSession
{
public:
    MFXAudioSession(void) { m_session = (mfxSession) 0; }
    virtual ~MFXAudioSession(void) { Close(); }

    mfxStatus Init(mfxIMPL impl, mfxVersion *ver) { return MFXInit(impl, ver, &m_session); }
    mfxStatus Close(void)
    {
        mfxStatus mfxRes;
        mfxRes = MFXClose(m_session); m_session = (mfxSession) 0;
        return mfxRes;
    }

    mfxStatus QueryIMPL(mfxIMPL *impl) { return MFXQueryIMPL(m_session, impl); }
    mfxStatus QueryVersion(mfxVersion *version) { return MFXQueryVersion(m_session, version); }

    mfxStatus JoinSession(mfxSession child_session) { return MFXJoinSession(m_session, child_session);}
    mfxStatus DisjoinSession( ) { return MFXDisjoinSession(m_session);}
    mfxStatus CloneSession( mfxSession *clone) { return MFXCloneSession(m_session, clone);}
    mfxStatus SetPriority( mfxPriority priority) { return MFXSetPriority(m_session, priority);}
    mfxStatus GetPriority( mfxPriority *priority) { return MFXGetPriority(m_session, priority);}

    mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait) { return MFXVideoCORE_SyncOperation(m_session, syncp, wait); }

    operator mfxSession (void) { return m_session; }

protected:

    mfxSession m_session;                                       // (mfxSession) handle to the owning session
};


class MFXAudioDECODE
{
public:

    MFXAudioDECODE(mfxSession session) { m_session = session; }
    virtual ~MFXAudioDECODE(void) { Close(); }

    mfxStatus Query(mfxAudioParam *in, mfxAudioParam *out) { return MFXAudioDECODE_Query(m_session, in, out); }
    mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par) { return MFXAudioDECODE_DecodeHeader(m_session, bs, par); }
    mfxStatus QueryIOSurf(mfxAudioParam *par, mfxFrameAllocRequest *request) { return MFXAudioDECODE_QueryIOSurf(m_session, par, request); }
    mfxStatus Init(mfxAudioParam *par) { return MFXAudioDECODE_Init(m_session, par); }
    mfxStatus Reset(mfxAudioParam *par) { return MFXAudioDECODE_Reset(m_session, par); }
    mfxStatus Close(void) { return MFXAudioDECODE_Close(m_session); }
    mfxStatus GetAudioParam(mfxAudioParam *par) { return MFXAudioDECODE_GetAudioParam(m_session, par); }
    mfxStatus DecodeFrameAsync(mfxBitstream *bs, mfxBitstream *bitstream_work, mfxBitstream **bitstream_out, mfxSyncPoint *syncp) { return MFXAudioDECODE_DecodeFrameAsync(m_session, bs, bitstream_work, bitstream_out, syncp); }

protected:

    mfxSession m_session;                                       // (mfxSession) handle to the owning session
};