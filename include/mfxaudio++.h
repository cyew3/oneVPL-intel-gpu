/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.


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

    virtual mfxStatus Init(mfxIMPL impl, mfxVersion *ver) { return MFXInit(impl, ver, &m_session); }
    virtual mfxStatus Close(void)
    {
        mfxStatus mfxRes;
        mfxRes = MFXClose(m_session); m_session = (mfxSession) 0;
        return mfxRes;
    }

    virtual mfxStatus QueryIMPL(mfxIMPL *impl) { return MFXQueryIMPL(m_session, impl); }
    virtual mfxStatus QueryVersion(mfxVersion *version) { return MFXQueryVersion(m_session, version); }

    virtual mfxStatus JoinSession(mfxSession child_session) { return MFXJoinSession(m_session, child_session);}
    virtual mfxStatus DisjoinSession( ) { return MFXDisjoinSession(m_session);}
    virtual mfxStatus CloneSession( mfxSession *clone) { return MFXCloneSession(m_session, clone);}
    virtual mfxStatus SetPriority( mfxPriority priority) { return MFXSetPriority(m_session, priority);}
    virtual mfxStatus GetPriority( mfxPriority *priority) { return MFXGetPriority(m_session, priority);}

//    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait) { return MFXVideoCORE_SyncOperation(m_session, syncp, wait); }

    virtual operator mfxSession (void) { return m_session; }

protected:

    mfxSession m_session;                                       // (mfxSession) handle to the owning session
};


class MFXAudioDECODE
{
public:

    MFXAudioDECODE(mfxSession session) { m_session = session; }
    virtual ~MFXAudioDECODE(void) { Close(); }

    virtual mfxStatus Query(mfxAudioParam *in, mfxAudioParam *out) { return MFXAudioDECODE_Query(m_session, in, out); }
    virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxAudioParam *par) { return MFXAudioDECODE_DecodeHeader(m_session, bs, par); }
    virtual mfxStatus QueryIOsize(mfxAudioParam *par, mfxAudioAllocRequest *request) { return MFXAudioDECODE_QueryIOSize(m_session, par, request); }
    virtual mfxStatus Init(mfxAudioParam *par) { return MFXAudioDECODE_Init(m_session, par); }
    virtual mfxStatus Reset(mfxAudioParam *par) { return MFXAudioDECODE_Reset(m_session, par); }
    virtual mfxStatus Close(void) { return MFXAudioDECODE_Close(m_session); }
    virtual mfxStatus GetAudioParam(mfxAudioParam *par) { return MFXAudioDECODE_GetAudioParam(m_session, par); }
    virtual mfxStatus DecodeFrameAsync(mfxBitstream *bs, mfxBitstream *buffer_out, mfxSyncPoint *syncp) { return MFXAudioDECODE_DecodeFrameAsync(m_session, bs, buffer_out, syncp); }


protected:

    mfxSession m_session;                                       // (mfxSession) handle to the owning session
};


class MFXAudioENCODE
{
public:

    MFXAudioENCODE(mfxSession session) { m_session = session; }
    virtual ~MFXAudioENCODE(void) { Close(); }

    virtual mfxStatus Query(mfxAudioParam *in, mfxAudioParam *out) { return MFXAudioENCODE_Query(m_session, in, out); }
    virtual mfxStatus QueryIOSize(mfxAudioParam *par, mfxAudioAllocRequest *request) { return MFXAudioENCODE_QueryIOSize(m_session, par, request); }
    virtual mfxStatus Init(mfxAudioParam *par) { return MFXAudioENCODE_Init(m_session, par); }
    virtual mfxStatus Reset(mfxAudioParam *par) { return MFXAudioENCODE_Reset(m_session, par); }
    virtual mfxStatus Close(void) { return MFXAudioENCODE_Close(m_session); }
    virtual mfxStatus GetAudioParam(mfxAudioParam *par) { return MFXAudioENCODE_GetAudioParam(m_session, par); }
    virtual mfxStatus EncodeFrameAsync(mfxBitstream *bs, mfxBitstream *buffer_out, mfxSyncPoint *syncp) { return MFXAudioENCODE_EncodeFrameAsync(m_session, bs, buffer_out, syncp); }

protected:

    mfxSession m_session;                                       // (mfxSession) handle to the owning session
};

#endif