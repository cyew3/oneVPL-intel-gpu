//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include"mfxaudio++.h"
#include"gmock/gmock.h"


//class MockMFXAudioSession : public MFXAudioSession {
//public:
//    MOCK_METHOD2(Init,
//        mfxStatus(mfxIMPL impl, mfxVersion *ver));
//    MOCK_METHOD0(Close, 
//        mfxStatus(void));
//    MOCK_METHOD1(QueryIMPL,
//        mfxStatus(mfxIMPL *impl));
//    MOCK_METHOD1(QueryVersion,
//        mfxStatus(mfxVersion *version));
//    MOCK_METHOD1(JoinSession,
//        mfxStatus(mfxSession child_session));
//    MOCK_METHOD0(DisjoinSession,
//        mfxStatus(void));
//    MOCK_METHOD1(CloneSession,
//        mfxStatus(mfxSession *clone));
//    MOCK_METHOD1(SetPriority,
//        mfxStatus(mfxPriority priority));
//    MOCK_METHOD1(GetPriority,
//        mfxStatus(mfxPriority *priority));
//   /* MOCK_METHOD0(operator mfxSession,
//        mfxSession());*/
//};

class MockMFXAudioDECODE : public MFXAudioDECODE {
public:
    MockMFXAudioDECODE():
        MFXAudioDECODE((mfxSession)0){}
    MOCK_METHOD2(Query, mfxStatus(mfxAudioParam *, mfxAudioParam *));
    MOCK_METHOD2(DecodeHeader, mfxStatus(mfxBitstream *, mfxAudioParam *));
    MOCK_METHOD2(QueryIOSize, mfxStatus(mfxAudioParam *, mfxAudioAllocRequest *));
    MOCK_METHOD1(Init, mfxStatus(mfxAudioParam *));
    MOCK_METHOD1(Reset, mfxStatus(mfxAudioParam *));
    MOCK_METHOD0(Close, mfxStatus());
    MOCK_METHOD1(GetAudioParam, mfxStatus(mfxAudioParam *));
    MOCK_METHOD3(DecodeFrameAsync, mfxStatus(mfxBitstream *, mfxAudioFrame *, mfxSyncPoint *));
};

class MockMFXAudioENCODE : public MFXAudioENCODE {
public:
    MockMFXAudioENCODE():
        MFXAudioENCODE((mfxSession)0){}
    MOCK_METHOD2(Query, mfxStatus(mfxAudioParam *, mfxAudioParam *));
    MOCK_METHOD2(QueryIOSize, mfxStatus(mfxAudioParam *, mfxAudioAllocRequest *));
    MOCK_METHOD1(Init, mfxStatus(mfxAudioParam *));
    MOCK_METHOD1(Reset, mfxStatus(mfxAudioParam *));
    MOCK_METHOD0(Close, mfxStatus());
    MOCK_METHOD1(GetAudioParam, mfxStatus(mfxAudioParam *));
    MOCK_METHOD3(EncodeFrameAsync, mfxStatus(mfxAudioFrame *, mfxBitstream *, mfxSyncPoint *));
};
