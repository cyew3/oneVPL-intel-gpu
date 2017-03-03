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
    //MOCK_METHOD1(GetFrameAllocator, mfxStatus (mfxFrameAllocator *&refAllocator));
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
