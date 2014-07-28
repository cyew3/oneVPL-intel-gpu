//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

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