//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "samples_pool.h"
#include "gmock/gmock.h"

#include "mfxaudio++.h"
#include "video_session.h"

class MockMFXVideoSession : public MFXVideoSessionExt
{
public:
    MockMFXVideoSession(PipelineFactory& factory) :
        MFXVideoSessionExt(factory){}
    MOCK_METHOD1(GetFrameAllocator, mfxStatus (mfxFrameAllocator *&refAllocator));
    MOCK_METHOD2(Init, mfxStatus (mfxIMPL, mfxVersion*));
    MOCK_METHOD0(Close, mfxStatus ());
    MOCK_METHOD1(QueryIMPL, mfxStatus (mfxIMPL*));
    MOCK_METHOD1(QueryVersion, mfxStatus (mfxVersion*));
    MOCK_METHOD1(JoinSession, mfxStatus (mfxSession));
    MOCK_METHOD0(DisjoinSession, mfxStatus ());
    MOCK_METHOD1(CloneSession, mfxStatus (mfxSession*));
    MOCK_METHOD1(SetPriority, mfxStatus (mfxPriority));
    MOCK_METHOD1(GetPriority, mfxStatus (mfxPriority*));
    MOCK_METHOD1(SetBufferAllocator, mfxStatus (mfxBufferAllocator*));
    MOCK_METHOD1(SetFrameAllocator, mfxStatus (mfxFrameAllocator*));
    MOCK_METHOD2(SetHandle, mfxStatus (mfxHandleType, mfxHDL));
    MOCK_METHOD2(GetHandle, mfxStatus (mfxHandleType, mfxHDL*));
    MOCK_METHOD2(SyncOperation, mfxStatus (mfxSyncPoint, mfxU32));

    MOCK_METHOD0(GetDevice, std::auto_ptr<CHWDevice>& ());
    MOCK_METHOD0(GetAllocator, std::auto_ptr<BaseFrameAllocator>& ());
};

class MockMFXAudioSession : public MFXAudioSession
{
public:
    MOCK_METHOD2(Init, mfxStatus (mfxIMPL, mfxVersion*));
    MOCK_METHOD0(Close, mfxStatus ());
    MOCK_METHOD1(QueryIMPL, mfxStatus (mfxIMPL*));
    MOCK_METHOD1(QueryVersion, mfxStatus (mfxVersion*));
    MOCK_METHOD1(JoinSession, mfxStatus (mfxSession));
    MOCK_METHOD0(DisjoinSession, mfxStatus ());
    MOCK_METHOD1(CloneSession, mfxStatus (mfxSession*));
    MOCK_METHOD1(SetPriority, mfxStatus (mfxPriority));
    MOCK_METHOD1(GetPriority, mfxStatus (mfxPriority*));
    MOCK_METHOD1(SetBufferAllocator, mfxStatus (mfxBufferAllocator*));
    MOCK_METHOD1(SetFrameAllocator, mfxStatus (mfxFrameAllocator*));
    MOCK_METHOD2(SetHandle, mfxStatus (mfxHandleType, mfxHDL));
    MOCK_METHOD2(GetHandle, mfxStatus (mfxHandleType, mfxHDL*));
    MOCK_METHOD2(SyncOperation, mfxStatus (mfxSyncPoint, mfxU32));
};

class MockMFXVideoDECODE : public MFXVideoDECODE
{
public:
    MockMFXVideoDECODE() :
        MFXVideoDECODE((mfxSession)0){}
    MOCK_METHOD2(Query, mfxStatus(mfxVideoParam *in, mfxVideoParam *out));
    MOCK_METHOD2(DecodeHeader, mfxStatus(mfxBitstream *bs, mfxVideoParam *par));
    MOCK_METHOD2(QueryIOSurf, mfxStatus(mfxVideoParam *par, mfxFrameAllocRequest *request));
    MOCK_METHOD1(Init, mfxStatus(mfxVideoParam *par));
    MOCK_METHOD1(Reset, mfxStatus(mfxVideoParam *par));
    MOCK_METHOD0(Close, mfxStatus());
    MOCK_METHOD1(GetVideoParam, mfxStatus(mfxVideoParam *par));
    MOCK_METHOD1(GetDecodeStat, mfxStatus(mfxDecodeStat *stat));
    MOCK_METHOD2(GetPayload, mfxStatus(mfxU64 *ts, mfxPayload *payload));
    MOCK_METHOD1(SetSkipMode, mfxStatus(mfxSkipMode mode));
    MOCK_METHOD4(DecodeFrameAsync, mfxStatus(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp));
};

class MockMFXVideoENCODE : public MFXVideoENCODE
{
public:
    MockMFXVideoENCODE() :
        MFXVideoENCODE((mfxSession)0){}
    MOCK_METHOD2(Query, mfxStatus(mfxVideoParam *in, mfxVideoParam *out));
    MOCK_METHOD2(QueryIOSurf, mfxStatus(mfxVideoParam *par, mfxFrameAllocRequest *request));
    MOCK_METHOD1(Init, mfxStatus(mfxVideoParam *par));
    MOCK_METHOD1(Reset, mfxStatus(mfxVideoParam *par));
    MOCK_METHOD0(Close, mfxStatus());
    MOCK_METHOD1(GetVideoParam, mfxStatus(mfxVideoParam *par));
    MOCK_METHOD1(GetEncodeStat, mfxStatus(mfxEncodeStat *stat));
    MOCK_METHOD4(EncodeFrameAsync, mfxStatus(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp));
};

class MockMFXVideoVPP : public MFXVideoVPP
{
public:
    MockMFXVideoVPP() :
        MFXVideoVPP((mfxSession)0){}
    MOCK_METHOD2(Query, mfxStatus(mfxVideoParam *in, mfxVideoParam *out));
    MOCK_METHOD2(QueryIOSurf, mfxStatus(mfxVideoParam *par, mfxFrameAllocRequest *request));
    MOCK_METHOD1(Init, mfxStatus(mfxVideoParam *par));
    MOCK_METHOD1(Reset, mfxStatus(mfxVideoParam *par));
    MOCK_METHOD0(Close, mfxStatus());
    MOCK_METHOD1(GetVideoParam, mfxStatus(mfxVideoParam *par));
    MOCK_METHOD1(GetVPPStat, mfxStatus(mfxVPPStat *stat));
    MOCK_METHOD4(RunFrameVPPAsync, mfxStatus(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp));
};