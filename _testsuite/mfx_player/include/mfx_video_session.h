/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.


File Name: mfx_video_session.h

\* ****************************************************************************** */


#pragma once
#include "mfx_ivideo_session.h"

class MFXVideoSessionImpl : public  IVideoSession
{
public:
    MFXVideoSessionImpl() 
        : m_frameAllocator()
        , m_externalImpl()
        , m_session()
    { 
    }
    virtual ~MFXVideoSessionImpl(void) { Close(); }
    virtual  mfxStatus Init(mfxIMPL impl, mfxVersion *ver, const tstring &libraryPath)
    {
        m_externalImpl = impl;
        if (!libraryPath.empty())
        {
            return myMFXInit(libraryPath.c_str(), impl, ver, &m_session);
        } else
        {
            return MFXInit(impl, ver, &m_session);
        }
    }

    virtual  mfxStatus InitEx(mfxInitParam par, const tstring &libraryPath)
    {
        m_externalImpl = par.Implementation;
        if (!libraryPath.empty())
        {
            return myMFXInitEx(libraryPath.c_str(), par, &m_session);
        } else
        {
            return MFXInitEx(par, &m_session);
        }
    }

    virtual mfxStatus QueryIMPLExternal(mfxIMPL *impl) 
    { 
        MFX_CHECK_POINTER(impl);
        *impl = m_externalImpl;
        return MFX_ERR_NONE; 
    }

    virtual mfxStatus SetFrameAllocator(MFXFrameAllocatorRW *allocator)
    {
        mfxStatus sts = MFXVideoCORE_SetFrameAllocator(m_session, allocator);
        if ((MFX_ERR_NONE == sts)||
            (MFX_ERR_UNDEFINED_BEHAVIOR == sts)) // for second call to set allocator
        {
            m_frameAllocator = allocator;
            sts = MFX_ERR_NONE;
        }
        return sts;
    }

    virtual MFXFrameAllocatorRW* GetFrameAllocator()
    {
        return m_frameAllocator;
    }
    virtual mfxStatus Close(void)
    { 
        mfxStatus sts = MFXClose(m_session); 
        m_session = (mfxSession)0;
        return sts;
    }
    virtual mfxStatus QueryIMPL(mfxIMPL *impl) { return MFXQueryIMPL(m_session, impl); }
    virtual mfxStatus QueryVersion(mfxVersion *version) { return MFXQueryVersion(m_session, version); }
    virtual mfxStatus JoinSession(mfxSession child_session) { return MFXJoinSession(m_session, child_session);}
    virtual mfxStatus DisjoinSession( ) { return MFXDisjoinSession(m_session);}
    virtual mfxStatus CloneSession( mfxSession *clone) { return MFXCloneSession(m_session, clone);}
    virtual mfxStatus SetPriority( mfxPriority priority) { return MFXSetPriority(m_session, priority);}
    virtual mfxStatus GetPriority( mfxPriority *priority) { return MFXGetPriority(m_session, priority);}
    virtual mfxStatus SetBufferAllocator(mfxBufferAllocator *allocator) { return MFXVideoCORE_SetBufferAllocator(m_session, allocator); }
    virtual mfxStatus SetHandle(mfxHandleType type, mfxHDL hdl) 
    { 
        mfxHDL _hdl = NULL;
        GetHandle(type, &_hdl);
        if (hdl == _hdl)
        {
            return MFX_ERR_NONE;
        }

        return MFXVideoCORE_SetHandle(m_session, type, hdl); 
    }
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *hdl) { return MFXVideoCORE_GetHandle(m_session, type, hdl); }
    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait) { return MFXVideoCORE_SyncOperation(m_session, syncp, wait); }
    virtual mfxSession GetMFXSession()
    {
        return m_session;
    }
protected:
    MFXFrameAllocatorRW *m_frameAllocator;
    mfxIMPL m_externalImpl;
    mfxSession m_session;                                       // (mfxSession) handle to the owning session
};
