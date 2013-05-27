/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2012 Intel Corporation. All Rights Reserved.

File Name: mock_mfx_video_session.h

\* ****************************************************************************** */

#pragma once

#include "mfx_ivideo_session.h"
#include "test_method.h"


class MockVideoSession : public IVideoSession
{
public:
    DECLARE_TEST_METHOD3( mfxStatus, Init, mfxIMPL , MAKE_STATIC_TRAIT(mfxVersion, mfxVersion*), const tstring &)
    { 
        TEST_METHOD_TYPE(Init) params_holder;
        _Init.RegisterEvent(TEST_METHOD_TYPE(Init)(MFX_ERR_NONE, _0, _1 ? *_1 : mfxVersion(), _2));

        return MFX_ERR_NONE;
    }
    virtual mfxStatus Close(){ return MFX_ERR_NONE;}
    virtual mfxStatus QueryIMPL(mfxIMPL * /*impl*/)  { return MFX_ERR_NONE;}
    virtual mfxStatus QueryIMPLExternal(mfxIMPL * /*impl*/) { return MFX_ERR_NONE;}
    virtual mfxStatus QueryVersion(mfxVersion * /*version*/)  { return MFX_ERR_NONE;}
    virtual mfxStatus JoinSession(mfxSession /*child_session*/)  { return MFX_ERR_NONE;}
    virtual mfxStatus DisjoinSession( )  { return MFX_ERR_NONE;}
    virtual mfxStatus CloneSession( mfxSession * /*clone*/)  { return MFX_ERR_NONE;}
    virtual mfxStatus SetPriority( mfxPriority /*priority*/)  { return MFX_ERR_NONE;}
    virtual mfxStatus GetPriority( mfxPriority * /*priority*/)  { return MFX_ERR_NONE;}
    virtual mfxStatus SetBufferAllocator(mfxBufferAllocator * /*allocator*/)  { return MFX_ERR_NONE;}
    virtual mfxStatus SetFrameAllocator(MFXFrameAllocatorRW * /*allocator*/)  { return MFX_ERR_NONE;}
    virtual MFXFrameAllocatorRW* GetFrameAllocator(){ return NULL;}
    virtual mfxStatus SetHandle(mfxHandleType /*type*/, mfxHDL /*hdl*/)  { return MFX_ERR_NONE;}
    virtual mfxStatus GetHandle(mfxHandleType /*type*/, mfxHDL * /*hdl*/)  { return MFX_ERR_NONE;}
    virtual mfxStatus SyncOperation(mfxSyncPoint /*syncp*/, mfxU32 /*wait*/){ return MFX_ERR_NONE;}
    virtual mfxSession GetMFXSession(){ return (mfxSession)0;}
};