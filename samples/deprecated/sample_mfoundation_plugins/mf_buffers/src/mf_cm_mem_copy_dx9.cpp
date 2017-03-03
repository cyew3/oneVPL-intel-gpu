/******************************************************************************\
Copyright (c) 2005-2016, Intel Corporation
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

#include "mf_utils.h"

#if 0
#include "cm_mem_copy.h"
#include "cm_gpu_copy_code.h"
#include <fstream>

#include "cm_def.h"
#include "cm_vm.h"

typedef mfxI32 cmStatus;
typedef const mfxU8 mfxUC8;
#define ALIGN128(X) (((mfxU32)((X)+127)) & (~ (mfxU32)127))

#define CHECK_CM_STATUS(_sts, _ret)              \
        if (CM_SUCCESS != _sts)                  \
        {                                           \
            return _ret;                        \
        }

#define CHECK_CM_STATUS_RET_NULL(_sts, _ret)              \
        if (CM_SUCCESS != _sts)                  \
        {                                           \
            return NULL;                        \
        }

#define CHECK_CM_NULL_PTR(_ptr, _ret)              \
        if (NULL == _ptr)                  \
        {                                           \
            return _ret;                        \
        }

#define THREAD_SPACE

CmCopyWrapper::CmCopyWrapper(IDirect3DDeviceManager9* pD3DDeviceMgr)
{
    m_pCmProgram = NULL;
    m_pCmDevice  = NULL;
    m_pCmKernel1 = NULL;
    m_pCmKernel2 = NULL;

    m_pCmSurface2D = NULL;
    m_pCmUserBuffer = NULL;

    m_pCmSrcIndex = NULL;
    m_pCmDstIndex = NULL;

    m_pCmQueue = NULL;
    m_pCmTask1 = NULL;
    m_pCmTask2 = NULL;

    m_pThreadSpace = NULL;

    m_pD3DDeviceMgr = pD3DDeviceMgr;

    m_cachedObjects.clear();

    m_tableCmRelations.clear();
    m_tableSysRelations.clear();

    m_tableCmIndex.clear();
    m_tableSysIndex.clear();

    m_tableCmRelations2.clear();
    m_tableSysRelations2.clear();

    m_tableCmIndex2.clear();
    m_tableSysIndex2.clear();


} // CmCopyWrapper::CmCopyWrapper(void)

CmCopyWrapper::~CmCopyWrapper(void)
{
    Release();

} // CmCopyWrapper::~CmCopyWrapper(void)

mfxStatus CmCopyWrapper::Initialize(mfxU32 width, mfxU32 height)
{
    cmStatus cmSts = CM_SUCCESS;
    // release object before allocation
    Release();

    mfxU32 cmVersion = 0;

    int tempv = width;
    tempv = height;

    cmSts = ::CreateCmDevice(m_pCmDevice, cmVersion, m_pD3DDeviceMgr);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    if (CM_1_0 > cmVersion)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    void *pCommonISACode = new mfxU32[CM_CODE_SIZE];

    if (false == pCommonISACode)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    MSDK_MEMCPY((char*)pCommonISACode, cm_code, CM_CODE_SIZE);

    cmSts = m_pCmDevice->LoadProgram(pCommonISACode, CM_CODE_SIZE, m_pCmProgram);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    delete [] pCommonISACode;

#ifndef THREAD_SPACE

    cmSts = m_pCmDevice->CreateKernel(m_pCmProgram, _NAME(surfaceCopy_2DToBufferNV12_2) , m_pCmKernel);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

#else

    cmSts = m_pCmDevice->CreateKernel(m_pCmProgram, _NAME(surfaceCopy_2DToBuffer_Single_nv12_128_full) , m_pCmKernel1);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    cmSts = m_pCmDevice->CreateKernel(m_pCmProgram, _NAME(surfaceCopy_BufferTo2D_Single_nv12_128_full) , m_pCmKernel2);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

#endif

    //mfxU32 width_ = ALIGN128(width);

//    mfxU32 threadWidth = (unsigned int)ceil((double)width / 128);
//    mfxU32 threadHeight = (unsigned int)ceil((double)height / 8);

//    m_pCmKernel1->SetThreadCount(threadWidth * threadHeight);
//    m_pCmKernel2->SetThreadCount(threadWidth * threadHeight);

#ifndef THREAD_SPACE

    mfxU32 threadId = 0, block_y = 0, x, y;

    mfxU32 xxx = (int)ceil((float)height / 32) * 32;

    for (block_y = 0; block_y < xxx; block_y += 32)
    {
        for (x = 0; x < width; x += BLOCK_PIXEL_WIDTH)
        {
            for (y = block_y; y < block_y + 32; y += BLOCK_HEIGHT)
            {
                if (y < height)
                {
                    m_pCmKernel->SetThreadArg(threadId, 2, 4, &x);
                    m_pCmKernel->SetThreadArg(threadId, 3, 4, &y);

                    threadId++;
                }
            }
        }
    }
#else
 //   m_pCmDevice->CreateThreadSpace(threadWidth, threadHeight, m_pThreadSpace);
#endif

    cmSts = m_pCmDevice->CreateQueue(m_pCmQueue);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    cmSts = m_pCmDevice->CreateTask(m_pCmTask1);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    cmSts = m_pCmDevice->CreateTask(m_pCmTask2);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED)

    cmSts = m_pCmTask1->AddKernel(m_pCmKernel1);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    cmSts = m_pCmTask2->AddKernel(m_pCmKernel2);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;

} // mfxStatus CmCopyWrapper::Initialize(void)

mfxStatus CmCopyWrapper::Release(void)
{
    std::map<void *, CmSurface2D *>::iterator itSrc;
    std::map<mfxU8 *, CmBufferUP *>::iterator itDst;

    for (itSrc = m_tableCmRelations.begin() ; itSrc != m_tableCmRelations.end(); itSrc++)
    {
        CmSurface2D *temp = itSrc->second;
        m_pCmDevice->DestroySurface(temp);
    }

    m_tableCmRelations.clear();

    for (itDst = m_tableSysRelations.begin() ; itDst != m_tableSysRelations.end(); itDst++)
    {
        CmBufferUP *temp = itDst->second;
        m_pCmDevice->DestroyBufferUP(temp);
    }

    m_tableSysRelations.clear();

    if (m_pCmKernel1)
    {
        m_pCmDevice->DestroyKernel(m_pCmKernel1);
    }

    m_pCmKernel1 = NULL;

    if (m_pCmKernel2)
    {
        m_pCmDevice->DestroyKernel(m_pCmKernel2);
    }

    m_pCmKernel2 = NULL;

    if (m_pCmProgram)
    {
        m_pCmDevice->DestroyProgram(m_pCmProgram);
    }

    m_pCmProgram = NULL;

    if (m_pThreadSpace)
    {
        m_pCmDevice->DestroyThreadSpace(m_pThreadSpace);
    }

    m_pThreadSpace = NULL;

    if (m_pCmTask1)
    {
        m_pCmDevice->DestroyTask(m_pCmTask1);
    }

    m_pCmTask1 = NULL;

    if (m_pCmTask2)
    {
        m_pCmDevice->DestroyTask(m_pCmTask2);
    }

    m_pCmTask2 = NULL;

    if (m_pCmDevice)
    {
        DestroyCmDevice(m_pCmDevice);
    }

    m_pCmDevice = NULL;

    return MFX_ERR_NONE;

} // mfxStatus CmCopyWrapper::Release(void)

CmSurface2D * CmCopyWrapper::CreateCmSurface2D(void *pSrc, mfxU32 width, mfxU32 height, bool isSecondMode,
                                               std::map<void *, CmSurface2D *> & tableCmRelations,
                                               std::map<CmSurface2D *, SurfaceIndex *> & tableCmIndex)
{
    cmStatus cmSts = 0;

    CmSurface2D *pCmSurface2D;
    SurfaceIndex *pCmSrcIndex;

    std::map<void *, CmSurface2D *>::iterator it;

    it = tableCmRelations.find(pSrc);

    if (tableCmRelations.end() == it)
    {
        /*if (true == isSecondMode)
        {
            HRESULT hRes = S_OK;
            D3DLOCKED_RECT sLockRect;

            hRes |= ((IDirect3DSurface9 *)pSrc)->LockRect(&sLockRect, NULL, D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY);

            m_pCmDevice->CreateSurface2D(width, height, (D3DFORMAT)(MAKEFOURCC('N', 'V', '1', '2')), pCmSurface2D);
            pCmSurface2D->WriteSurface((mfxU8 *) sLockRect.pBits, NULL);

            ((IDirect3DSurface9 *)pSrc)->UnlockRect();
        }
        else*/
        {
            m_pCmDevice->CreateSurface2D((IDirect3DSurface9 *) pSrc, pCmSurface2D);
            tableCmRelations.insert(std::pair<void *, CmSurface2D *>(pSrc, pCmSurface2D));
        }

        cmSts = pCmSurface2D->GetIndex(pCmSrcIndex);
        CHECK_CM_STATUS_RET_NULL(cmSts, MFX_ERR_DEVICE_FAILED);

        tableCmIndex.insert(std::pair<CmSurface2D *, SurfaceIndex *>(pCmSurface2D, pCmSrcIndex));
    }
    else
    {
        pCmSurface2D = it->second;
    }

    return pCmSurface2D;

} // CmSurface2D * CmCopyWrapper::CreateCmSurface2D(void *pSrc, mfxU32 width, mfxU32 height, bool isSecondMode)

CmBufferUP * CmCopyWrapper::CreateUpBuffer(mfxU8 *pDst, mfxU32 memSize,
                                           std::map<mfxU8 *, CmBufferUP *> & tableSysRelations,
                                           std::map<CmBufferUP *,  SurfaceIndex *> & tableSysIndex)
{
    cmStatus cmSts = 0;

    CmBufferUP *pCmUserBuffer;
    SurfaceIndex *pCmDstIndex;

    std::map<mfxU8 *, CmBufferUP *>::iterator it;
    it = tableSysRelations.find(pDst);

    if (tableSysRelations.end() == it)
    {
        cmSts = m_pCmDevice->CreateBufferUP(memSize, pDst, pCmUserBuffer);
        CHECK_CM_STATUS_RET_NULL(cmSts, MFX_ERR_DEVICE_FAILED);

        tableSysRelations.insert(std::pair<mfxU8 *, CmBufferUP *>(pDst, pCmUserBuffer));

        cmSts = pCmUserBuffer->GetIndex(pCmDstIndex);
        CHECK_CM_STATUS_RET_NULL(cmSts, MFX_ERR_DEVICE_FAILED);

        tableSysIndex.insert(std::pair<CmBufferUP *, SurfaceIndex *>(pCmUserBuffer, pCmDstIndex));
    }
    else
    {
        pCmUserBuffer = it->second;
    }

    return pCmUserBuffer;

} // CmBufferUP * CmCopyWrapper::CreateUpBuffer(mfxU8 *pDst, mfxU32 memSize)

mfxStatus CmCopyWrapper::IsCmCopySupported(mfxFrameSurface1 *pSurface, IppiSize roi)
{
    if ((roi.width & 15) || (roi.height & 7))
    {
        return MFX_ERR_UNSUPPORTED;
    }

    if(pSurface->Info.FourCC != MFX_FOURCC_NV12)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    if(pSurface->Data.UV - pSurface->Data.Y != pSurface->Data.Pitch * pSurface->Info.Height)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;

} // mfxStatus CmCopyWrapper::IsCmCopySupported(mfxFrameSurface1 *pSurface, IppiSize roi)

#if 0
//#define DEBUG_LEVEL_1
//#define DEBUG_LEVEL_2

mfxStatus CmCopyWrapper::CopyVideoToSystemMemory(mfxU8 *pDst, mfxU32 dstPitch, void *pSrc, mfxU32 srcPitch, IppiSize roi, bool isSecondMode)
{
    srcPitch;
    cmStatus cmSts = 0;

    CmEvent* e = NULL;
    CM_STATUS sts;

    SurfaceIndex *pCmSrcIndex;
    SurfaceIndex *pCmDstIndex;

#ifdef DEBUG_LEVEL_1
     __int64 ctr0 = 0, ctr1 = 0, ctr2 = 0, ctr3 = 0, ctr4 = 0, ctr5 = 0, freq = 0;
#endif

    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;
    mfxU32 memSize = dstPitch * height * 3 / 2;
    int ii = 0;

#ifdef DEBUG_LEVEL_1
    QueryPerformanceCounter((LARGE_INTEGER *)&ctr0);
#endif

    mfxU32 threadWidth = (unsigned int)((double)width / 128);
    mfxU32 threadHeight = (unsigned int)((double)height / 8);

    cmSts = m_pCmTask1->Reset();
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    cmSts = m_pCmKernel1->SetThreadCount(threadWidth * threadHeight);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    cmSts = m_pCmTask1->AddKernel(m_pCmKernel1);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    if (m_pThreadSpace)
    {
        cmSts = m_pCmDevice->DestroyThreadSpace(m_pThreadSpace);
        CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);
    }

    cmSts = m_pCmDevice->CreateThreadSpace(threadWidth, threadHeight, m_pThreadSpace);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

#ifdef DEBUG_LEVEL_1
    QueryPerformanceCounter((LARGE_INTEGER *)&ctr1);
#endif

    // create or find already associated cm surface 2d
    CmSurface2D *pCmSurface2D;
    pCmSurface2D = CreateCmSurface2D(pSrc, width, height, isSecondMode, m_tableCmRelations, m_tableCmIndex);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);

    // find surface cached index
    std::map<CmSurface2D *, SurfaceIndex *>::iterator itSrc;
    itSrc = m_tableCmIndex.find(pCmSurface2D);
    pCmSrcIndex = itSrc->second;

    // create or find already associated cm user provided buffer
    CmBufferUP *pCmUserBuffer = NULL;
    pCmUserBuffer = CreateUpBuffer(pDst, memSize, m_tableSysRelations, m_tableSysIndex);
    CHECK_CM_NULL_PTR(pCmUserBuffer, MFX_ERR_DEVICE_FAILED);

    // find buffer cached index
    std::map<CmBufferUP *, SurfaceIndex *>::iterator itDst;
    itDst = m_tableSysIndex.find(pCmUserBuffer);

    pCmDstIndex = itDst->second;

#ifdef DEBUG_LEVEL_1
    QueryPerformanceCounter((LARGE_INTEGER *)&ctr2);
#endif


    m_pCmKernel1->SetKernelArg(0, sizeof(SurfaceIndex), pCmSrcIndex);
    m_pCmKernel1->SetKernelArg(1, sizeof(SurfaceIndex), pCmDstIndex);

#ifdef THREAD_SPACE
    m_pCmKernel1->SetKernelArg(2, sizeof(mfxU32), &dstPitch);
    m_pCmKernel1->SetKernelArg(3, sizeof(mfxU32), &height);
#endif

#ifdef DEBUG_LEVEL_1
    QueryPerformanceCounter((LARGE_INTEGER *)&ctr3);
#endif

#ifndef THREAD_SPACE
    cmSts = m_pCmQueue->Enqueue(m_pCmTask, e);
#else
    cmSts = m_pCmQueue->Enqueue(m_pCmTask1, e, m_pThreadSpace);
#endif

    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

#ifdef DEBUG_LEVEL_1
    QueryPerformanceCounter((LARGE_INTEGER *)&ctr4);
#endif

    e->GetStatus(sts);

    while (CM_STATUS_FINISHED != sts)
    {
        ii++;
        e->GetStatus(sts);
    }

    //unsigned char* Src_CM = (unsigned char*) _aligned_malloc(width * height * 3 / 2, 0x1000);
    //pCmSurface2D->ReadSurface(Src_CM, NULL);
    //int res = memcmp(pDst, Src_CM, width * height * 3/2);
    //_aligned_free(Src_CM);
    //if(res != 0)
    //    return MFX_ERR_UNKNOWN;

/*
    mfxU64 executionTime = 0;
    cmSts = e->GetExecutionTime(executionTime);

    while (CM_SUCCESS != cmSts)
    {
        cmSts = e->GetExecutionTime(executionTime);
    }
*/

#ifdef DEBUG_LEVEL_1
    QueryPerformanceCounter((LARGE_INTEGER *)&ctr5);
    QueryPerformanceFrequency((LARGE_INTEGER *)&freq);

    int ms0 = (int)(ctr1 - ctr0);

    int ms1 = (int)(ctr2 - ctr1);

    int ms2 = (int)(ctr3 - ctr2);

    int ms3 = (int)(ctr4 - ctr3);

    int ms4 = (int)(ctr5 - ctr4);

    //int sum = ms3 + ms + ms2;

    char cStr[1024];

    sprintf_s(cStr, sizeof(cStr), "CM v2s ThreadSpace %4d; DataStructures %4d; SetArgs %4d; Enqueue %4d; GetStatus %5d; Frequency %10d \n", ms0, ms1, ms2, ms3, ms4, freq);
    printf(cStr);//OutputDebugStringA(cStr);
#endif

    return MFX_ERR_NONE;

} // mfxStatus CmCopyWrapper::CopyVideoToSystemMemory(mfxU8 *pDst, mfxU32 dstPitch, void *pSrc, mfxU32 srcPitch, IppiSize roi, bool isSecondMode)

mfxStatus CmCopyWrapper::CopySystemToVideoMemory(void *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, IppiSize roi, bool isSecondMode)
{
    dstPitch;
    cmStatus cmSts = 0;

    CmEvent* e = NULL;
    CM_STATUS sts;

    SurfaceIndex *pCmSrcIndex;
    SurfaceIndex *pCmDstIndex;

#ifdef DEBUG_LEVEL_2
     __int64 ctr0 = 0, ctr1 = 0, ctr2 = 0, ctr3 = 0, ctr4 = 0, ctr5 = 0, freq = 0;
#endif

    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;
    mfxU32 memSize = srcPitch * height * 3/2;

#ifdef DEBUG_LEVEL_2
    QueryPerformanceCounter((LARGE_INTEGER *)&ctr0);
#endif

    mfxU32 threadWidth = (unsigned int)((double)width / 128);
    mfxU32 threadHeight = (unsigned int)((double)height / 8);

    cmSts = m_pCmTask2->Reset();
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    cmSts = m_pCmKernel2->SetThreadCount(threadWidth * threadHeight);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    cmSts = m_pCmTask2->AddKernel(m_pCmKernel2);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    if (m_pThreadSpace)
    {
        cmSts = m_pCmDevice->DestroyThreadSpace(m_pThreadSpace);
        CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);
    }

    cmSts = m_pCmDevice->CreateThreadSpace(threadWidth, threadHeight, m_pThreadSpace);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

#ifdef DEBUG_LEVEL_2
    QueryPerformanceCounter((LARGE_INTEGER *)&ctr1);
#endif

    // create or find already associated cm surface 2d
    CmSurface2D *pCmSurface2D;
    pCmSurface2D = CreateCmSurface2D(pDst, width, height, isSecondMode, m_tableCmRelations2, m_tableCmIndex2);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);

    // find surface cached index
    std::map<CmSurface2D *, SurfaceIndex *>::iterator itDst;
    itDst = m_tableCmIndex2.find(pCmSurface2D);
    pCmDstIndex = itDst->second;

    // create or find already associated cm user provided buffer
    CmBufferUP *pCmUserBuffer = NULL;
    pCmUserBuffer = CreateUpBuffer(pSrc, memSize, m_tableSysRelations2, m_tableSysIndex2);
    CHECK_CM_NULL_PTR(pCmUserBuffer, MFX_ERR_DEVICE_FAILED);

    // find buffer cached index
    std::map<CmBufferUP *, SurfaceIndex *>::iterator itSrc;
    itSrc = m_tableSysIndex2.find(pCmUserBuffer);

    pCmSrcIndex = itSrc->second;

#ifdef DEBUG_LEVEL_2
    QueryPerformanceCounter((LARGE_INTEGER *)&ctr2);
#endif

    m_pCmKernel2->SetKernelArg(0, sizeof(SurfaceIndex), pCmSrcIndex);
    m_pCmKernel2->SetKernelArg(1, sizeof(SurfaceIndex), pCmDstIndex);

#ifdef THREAD_SPACE
    m_pCmKernel2->SetKernelArg(2, sizeof(mfxU32), &width);
    m_pCmKernel2->SetKernelArg(3, sizeof(mfxU32), &height);
#endif

#ifdef DEBUG_LEVEL_2
    QueryPerformanceCounter((LARGE_INTEGER *)&ctr3);
#endif

#ifndef THREAD_SPACE
    cmSts = m_pCmQueue->Enqueue(m_pCmTask, e);
#else
    cmSts = m_pCmQueue->Enqueue(m_pCmTask2, e, m_pThreadSpace);
#endif

    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

#ifdef DEBUG_LEVEL_2
    QueryPerformanceCounter((LARGE_INTEGER *)&ctr4);
#endif

    e->GetStatus(sts);

    while (CM_STATUS_FINISHED != sts)
    {
        e->GetStatus(sts);
    }

    //unsigned char* Dst_CM = (unsigned char*) _aligned_malloc(width * height * 3 / 2, 0x1000);
    //pCmSurface2D->ReadSurface(Dst_CM, NULL);
    //int res = memcmp(pSrc, Dst_CM, width * height * 3/2);
    //_aligned_free(Dst_CM);
    //if(res != 0)
    //    return MFX_ERR_UNKNOWN;


#ifdef DEBUG_LEVEL_2
    QueryPerformanceCounter((LARGE_INTEGER *)&ctr5);
    QueryPerformanceFrequency((LARGE_INTEGER *)&freq);

    int ms0 = (int)(ctr1 - ctr0);

    int ms1 = (int)(ctr2 - ctr1);

    int ms2 = (int)(ctr3 - ctr2);

    int ms3 = (int)(ctr4 - ctr3);

    int ms4 = (int)(ctr5 - ctr4);

    //int sum = ms3 + ms + ms2;

    char cStr[1024];

    sprintf_s(cStr, sizeof(cStr), "CM  s2v ThreadSpace %4d; DataStructures %4d; SetArgs %4d; Enqueue %4d; GetStatus %5d; Frequency %10d \n", ms0, ms1, ms2, ms3, ms4, freq);
    printf(cStr);//OutputDebugStringA(cStr);
#endif

    return MFX_ERR_NONE;

} // mfxStatus CmCopyWrapper::CopySystemToVideoMemory(void *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, IppiSize roi, bool isSecondMode)
#endif
mfxStatus CmCopyWrapper::CopySystemToVideoMemoryAPI(void *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, IppiSize roi)
{
    cmStatus cmSts = 0;

    CmEvent* e = NULL;
    CM_STATUS sts;

    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

     // create or find already associated cm surface 2d
    CmSurface2D *pCmSurface2D;
    pCmSurface2D = CreateCmSurface2D(pDst, width, height, false, m_tableCmRelations2, m_tableCmIndex2);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);

    cmSts = m_pCmQueue->EnqueueCopyCPUToGPU(pCmSurface2D, pSrc, e);

    if (CM_SUCCESS == cmSts && e)
    {
        e->GetStatus(sts);

        while (sts != CM_STATUS_FINISHED)
        {
            e->GetStatus(sts);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::CopyVideoToSystemMemoryAPI(mfxU8 *pDst, mfxU32 dstPitch, void *pSrc, mfxU32 srcPitch, IppiSize roi)
{
    cmStatus cmSts = 0;

    CmEvent* e = NULL;
    CM_STATUS sts;

    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;
    mfxU32 memSize = srcPitch * height * 3/2;

     // create or find already associated cm surface 2d
    CmSurface2D *pCmSurface2D;
    pCmSurface2D = CreateCmSurface2D(pSrc, width, height, false, m_tableCmRelations2, m_tableCmIndex2);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);

    cmSts = m_pCmQueue->EnqueueCopyGPUToCPU(pCmSurface2D, pDst, e);

    if (CM_SUCCESS == cmSts && e)
    {
        e->GetStatus(sts);

        while (sts != CM_STATUS_FINISHED)
        {
            e->GetStatus(sts);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::CopyVideoToVideoMemoryAPI(void *pDst, void *pSrc, IppiSize roi)
{
    cmStatus cmSts = 0;

    CmEvent* e = NULL;
    CM_STATUS sts;

    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

     // create or find already associated cm surface 2d
    CmSurface2D *pDstCmSurface2D;
    pDstCmSurface2D = CreateCmSurface2D(pDst, width, height, false, m_tableCmRelations2, m_tableCmIndex2);
    CHECK_CM_NULL_PTR(pDstCmSurface2D, MFX_ERR_DEVICE_FAILED);

    CmSurface2D *pSrcCmSurface2D;
    pSrcCmSurface2D = CreateCmSurface2D(pSrc, width, height, false, m_tableCmRelations2, m_tableCmIndex2);
    CHECK_CM_NULL_PTR(pSrcCmSurface2D, MFX_ERR_DEVICE_FAILED);

    cmSts = m_pCmQueue->EnqueueCopyGPUToGPU(pDstCmSurface2D, pSrcCmSurface2D, e);

    if (CM_SUCCESS == cmSts && e)
    {
        e->GetStatus(sts);

        while (sts != CM_STATUS_FINISHED)
        {
            e->GetStatus(sts);
        }
    }

    return MFX_ERR_NONE;
}

#endif

#ifdef CM_COPY_RESOURCE

#define CM_DX9
#include <cm_rt.h>

#include <mf_cm_mem_copy.h>
#ifdef ENABLE_CM_COPY_D3D9

#pragma comment (lib, "igfxcmrt32.lib")

CmDeviceHelper<IDirect3DDeviceManager9> :: CmDeviceHelper(const CComPtr<IDirect3DDeviceManager9> & rdevice)
    : base(rdevice)
{
    mfxU32 cmVersion = 0;
    INT cmSts = ::CreateCmDevice(m_pCmDevice, cmVersion, rdevice);

    if (CM_SUCCESS != cmSts)
        return ;

    if (CM_1_0 > cmVersion)
    {
        DestroyCmDevice(m_pCmDevice);
        m_pCmDevice = NULL;
        return ;
    }

    cmSts = m_pCmDevice->CreateQueue(m_pQueue);
    if (CM_SUCCESS != cmSts)
    {
        DestroyCmDevice(m_pCmDevice);
        m_pCmDevice = NULL;
    }
}

CmDeviceHelper<IDirect3DDeviceManager9> :: ~CmDeviceHelper()
{
    if (NULL == m_pCmDevice)
        return;
    DestroyCmDevice(m_pCmDevice);
}

mfxStatus CmDeviceHelper<IDirect3DDeviceManager9> :: CreateCmSurface(CmSurface2D *&pCmSurface2D, CComPtr<Surface_t> & dxSurface)
{
    if (!m_pCmDevice)
        return MFX_ERR_DEVICE_FAILED;

    INT cm_sts = m_pCmDevice->CreateSurface2D(dxSurface, pCmSurface2D);
    //todo: handle cm ret_code
    if (CM_SUCCESS == cm_sts)
        return MFX_ERR_NONE;

    return MFX_ERR_DEVICE_FAILED;
}

#endif // ifdef ENABLE_CM_COPY_D3D9

#endif //ifdef CM_COPY_RESOURCE