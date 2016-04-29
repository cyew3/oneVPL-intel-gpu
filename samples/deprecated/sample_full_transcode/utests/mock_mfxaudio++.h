/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

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
