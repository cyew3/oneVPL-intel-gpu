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

#ifndef MOCK_ALLOCATOR_H_
#define MOCK_ALLOCATOR_H_

#include <gmock/gmock.h>
#include "sysmem_allocator.h"

class MockSysMemFrameAllocator: public SysMemFrameAllocator
{
public:
    MOCK_METHOD1(Init, mfxStatus(mfxAllocatorParams *pParams));
    MOCK_METHOD0(Close, mfxStatus());
    MOCK_METHOD2(LockFrame, mfxStatus(mfxMemId mid, mfxFrameData *ptr));
    MOCK_METHOD2(UnlockFrame, mfxStatus(mfxMemId mid, mfxFrameData *ptr));
    MOCK_METHOD2(GetFrameHDL, mfxStatus(mfxMemId mid, mfxHDL *handle));

    MOCK_METHOD2(AllocFrames, mfxStatus(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response));
    MOCK_METHOD1(FreeFrames, mfxStatus(mfxFrameAllocResponse *response));
};

#if defined(_WIN32) || defined(_WIN64)

#include "d3d_allocator.h"
#include "d3d11_allocator.h"

class MockD3DFrameAllocator: public D3DFrameAllocator
{
public:
    MOCK_METHOD1(Init, mfxStatus(mfxAllocatorParams *pParams));
    MOCK_METHOD0(Close, mfxStatus());
    MOCK_METHOD2(LockFrame, mfxStatus(mfxMemId mid, mfxFrameData *ptr));
    MOCK_METHOD2(UnlockFrame, mfxStatus(mfxMemId mid, mfxFrameData *ptr));
    MOCK_METHOD2(GetFrameHDL, mfxStatus(mfxMemId mid, mfxHDL *handle));
    MOCK_METHOD2(AllocFrames, mfxStatus(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response));
    MOCK_METHOD1(FreeFrames, mfxStatus(mfxFrameAllocResponse *response));
};

class MockD3D11FrameAllocator: public D3D11FrameAllocator
{
public:
    MOCK_METHOD1(Init, mfxStatus(mfxAllocatorParams *pParams));
    MOCK_METHOD0(Close, mfxStatus());
    MOCK_METHOD2(LockFrame, mfxStatus(mfxMemId mid, mfxFrameData *ptr));
    MOCK_METHOD2(UnlockFrame, mfxStatus(mfxMemId mid, mfxFrameData *ptr));
    MOCK_METHOD2(GetFrameHDL, mfxStatus(mfxMemId mid, mfxHDL *handle));
    MOCK_METHOD0(GetD3D11Device, ID3D11Device* ());
    MOCK_METHOD2(AllocFrames, mfxStatus(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response));
    MOCK_METHOD1(FreeFrames, mfxStatus(mfxFrameAllocResponse *response));
};

class MockMFXFrameAllocator: public MFXFrameAllocator
{
public:
    MOCK_METHOD1(Init, mfxStatus(mfxAllocatorParams *pParams));
    MOCK_METHOD0(Close, mfxStatus());
    MOCK_METHOD2(LockFrame, mfxStatus(mfxMemId mid, mfxFrameData *ptr));
    MOCK_METHOD2(UnlockFrame, mfxStatus(mfxMemId mid, mfxFrameData *ptr));
    MOCK_METHOD2(GetFrameHDL, mfxStatus(mfxMemId mid, mfxHDL *handle));
    MOCK_METHOD0(GetD3D11Device, ID3D11Device* ());
    MOCK_METHOD2(AllocFrames, mfxStatus(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response));
    MOCK_METHOD1(FreeFrames, mfxStatus(mfxFrameAllocResponse *response));

    /*MOCK_METHOD3(Alloc, mfxStatus(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response));
    MOCK_METHOD3(Lock, mfxStatus(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr));
    MOCK_METHOD3(Unlock, mfxStatus(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr));
    MOCK_METHOD3(GetHDL, mfxStatus(mfxHDL pthis, mfxMemId mid, mfxHDL *ptr));
    MOCK_METHOD2(Free, mfxStatus(mfxHDL pthis, mfxFrameAllocResponse *response));*/
};
#else

#include <gmock/gmock.h>
#if defined(LIBVA_SUPPORT)
#include "vaapi_allocator.h"

class MockVaapiFrameAllocator: public vaapiFrameAllocator
{
public:
    MOCK_METHOD1(Init, mfxStatus(mfxAllocatorParams *pParams));
    MOCK_METHOD0(Close, mfxStatus());
    MOCK_METHOD2(AllocFrames, mfxStatus(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response));
    MOCK_METHOD1(FreeFrames, mfxStatus(mfxFrameAllocResponse *response));
protected:
    MOCK_METHOD2(LockFrame, mfxStatus(mfxMemId mid, mfxFrameData *ptr));
    MOCK_METHOD2(UnlockFrame, mfxStatus(mfxMemId mid, mfxFrameData *ptr));
    MOCK_METHOD2(GetFrameHDL, mfxStatus(mfxMemId mid, mfxHDL *handle));

    MOCK_METHOD1(CheckRequestType, mfxStatus(mfxFrameAllocRequest *request));
    MOCK_METHOD1(ReleaseResponse, mfxStatus(mfxFrameAllocResponse *response));
    MOCK_METHOD2(AllocImpl, mfxStatus(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response));
};
#endif //defined(LIBVA_SUPPORT)
#endif //#if defined(_WIN32) || defined(_WIN64)

#endif //MOCK_ALLOCATOR_H_