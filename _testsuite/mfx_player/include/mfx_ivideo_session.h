/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.


File Name: mfx_ivideo_session.h

\* ****************************************************************************** */

#pragma once

#include "mfxstructures.h"
#include "mfx_iproxy.h"


class IVideoSession : public EnableProxyForThis<IVideoSession>
{
public:
    //different from mediasdk's init
    virtual mfxStatus Init(mfxIMPL impl, mfxVersion *ver, const tstring &libraryPath) = 0;
    virtual mfxStatus InitEx(mfxInitParam par, const tstring &libraryPath) = 0;
    virtual mfxStatus Close() = 0;
    virtual mfxStatus QueryIMPL(mfxIMPL *impl)  = 0;
    virtual mfxStatus QueryIMPLExternal(mfxIMPL *impl) = 0;
    virtual mfxStatus QueryVersion(mfxVersion *version)  = 0;
    virtual mfxStatus JoinSession(mfxSession child_session)  = 0;
    virtual mfxStatus DisjoinSession( )  = 0;
    virtual mfxStatus CloneSession( mfxSession *clone)  = 0;
    virtual mfxStatus SetPriority( mfxPriority priority)  = 0;
    virtual mfxStatus GetPriority( mfxPriority *priority)  = 0;
    virtual mfxStatus SetBufferAllocator(mfxBufferAllocator *allocator)  = 0;
    virtual mfxStatus SetFrameAllocator(MFXFrameAllocatorRW *allocator)  = 0;
    virtual MFXFrameAllocatorRW* GetFrameAllocator() = 0;
    virtual mfxStatus SetHandle(mfxHandleType type, mfxHDL hdl)  = 0;
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *hdl)  = 0;
    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait) = 0;
    virtual mfxSession GetMFXSession() = 0;
};


template <>
class InterfaceProxy<IVideoSession> : public InterfaceProxyBase<IVideoSession>
{
public:
    typedef InterfaceProxyBase<IVideoSession> base;

    InterfaceProxy(std::auto_ptr<IVideoSession> &pSession)
        : base(pSession)
    {
    }
    virtual mfxStatus Init(mfxIMPL impl, mfxVersion *ver, const tstring &libraryPath) { return Target().Init(impl, ver, libraryPath); }
    virtual mfxStatus InitEx(mfxInitParam par, const tstring &libraryPath) { return Target().InitEx(par, libraryPath); }
    virtual mfxStatus Close(){ return Target().Close(); }
    virtual mfxStatus QueryIMPL(mfxIMPL *impl) { return Target().QueryIMPL(impl); }
    virtual mfxStatus QueryIMPLExternal(mfxIMPL *impl) { return Target().QueryIMPLExternal(impl);}
    virtual mfxStatus QueryVersion(mfxVersion *version) { return Target().QueryVersion( version); }
    virtual mfxStatus JoinSession(mfxSession child_session) { return Target().JoinSession( child_session);}
    virtual mfxStatus DisjoinSession( ) { return Target().DisjoinSession();}
    virtual mfxStatus CloneSession( mfxSession *clone) { return Target().CloneSession( clone);}
    virtual mfxStatus SetPriority( mfxPriority priority) { return Target().SetPriority( priority);}
    virtual mfxStatus GetPriority( mfxPriority *priority) { return Target().GetPriority( priority);}
    virtual mfxStatus SetBufferAllocator(mfxBufferAllocator *allocator) { return Target().SetBufferAllocator( allocator); }
    virtual mfxStatus SetFrameAllocator(MFXFrameAllocatorRW *allocator) { return Target().SetFrameAllocator( allocator); }
    virtual MFXFrameAllocatorRW* GetFrameAllocator() { return Target().GetFrameAllocator(); }
    virtual mfxStatus SetHandle(mfxHandleType type, mfxHDL hdl) { return Target().SetHandle( type, hdl); }
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *hdl) { return Target().GetHandle( type, hdl); }
    virtual mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait) { return Target().SyncOperation( syncp, wait); }
    virtual mfxSession GetMFXSession() {return Target().GetMFXSession();}
};
