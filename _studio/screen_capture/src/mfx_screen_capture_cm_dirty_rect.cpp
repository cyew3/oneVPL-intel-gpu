/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_cm_dirty_rect.cpp

\* ****************************************************************************** */

#include "mfx_screen_capture_cm_dirty_rect.h"
#include "mfx_utils.h"
#include "mfxstructures.h"
#include <memory>
#include <list>
#include <algorithm> //min
#include <assert.h>

#include "dirty_rect_genx_hsw_isa.h"
#include "dirty_rect_genx_bdw_isa.h"
#include "dirty_rect_genx_skl_isa.h"
#include "dirty_rect_genx_jit_isa.h"

#define H_BLOCKS 16
#define W_BLOCKS 16

#if !defined(MSDK_ALIGN32)
#define MSDK_ALIGN32(value)                      (((value + 31) >> 5) << 5) // round up to a multiple of 32
#endif
#ifndef ALIGN16
#define ALIGN16(SZ) (((SZ + 15) >> 4) << 4) // round up to a multiple of 16
#define ALIGN16_DOWN(SZ) (((SZ ) >> 4) << 4) // round up to a multiple of 16
#endif

#define MFXSC_CHECK_CM_RESULT_AND_PTR(result, ptr)  \
do {                                                \
    if(CM_SUCCESS != result || 0 == ptr)            \
    {                                               \
        assert(CM_SUCCESS == result);               \
        Close();                                    \
        return MFX_ERR_DEVICE_FAILED;               \
    }                                               \
} while (0,0)

#define MFXSC_CHECK_IPP_RESULT_AND_PTR(result, ptr) \
do {                                                \
    if(ippStsNoErr != result || 0 == ptr)           \
    {                                               \
        assert(ippStsNoErr == result);              \
        Close();                                    \
        return MFX_ERR_DEVICE_FAILED;               \
    }                                               \
} while (0,0)

namespace MfxCapture
{

template<typename T>
T* GetExtendedBuffer(const mfxU32& BufferId, const mfxFrameSurface1* surf)
{
    if(!surf || !BufferId)
        return 0;
    if(!surf->Data.NumExtParam || !surf->Data.ExtParam)
        return 0;
    for(mfxU32 i = 0; i < surf->Data.NumExtParam; ++i)
    {
        if(!surf->Data.ExtParam[i])
            continue;
        if(BufferId == surf->Data.ExtParam[i]->BufferId && sizeof(T) == surf->Data.ExtParam[i]->BufferSz)
            return (T*) surf->Data.ExtParam[i];
    }

    return 0;
}

CmDirtyRectFilter::CmDirtyRectFilter(const mfxCoreInterface* _core)
    :m_pmfxCore(_core)
{
    Mode = CM_DR;

    m_pCMDevice   = 0;
    m_pQueue      = 0;

    m_width = 0;
    m_height = 0;
}

CmDirtyRectFilter::~CmDirtyRectFilter()
{
    Close();
    FreeSurfs();
}

mfxStatus CmDirtyRectFilter::Init(const mfxVideoParam* par, bool isSysMem, bool isOpaque)
{
    if(!par) return MFX_ERR_NULL_PTR;

    m_bSysMem  = isSysMem;
    m_bOpaqMem = isOpaque;

    mfxStatus mfxRes = MFX_ERR_NONE;
    if(!m_pmfxCore)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    mfxCoreParam param = {0};
    mfxRes = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &param);
    MFX_CHECK_STS(mfxRes);

    if(MFX_IMPL_HARDWARE != MFX_IMPL_BASETYPE(param.Impl))
        return MFX_ERR_UNSUPPORTED;

    mfxHandleType type = (MFX_IMPL_VIA_D3D11 == (param.Impl & 0x0F00)) ? MFX_HANDLE_D3D11_DEVICE : MFX_HANDLE_D3D9_DEVICE_MANAGER;
    mfxHDL handle = 0;
    mfxRes = m_pmfxCore->CreateAccelerationDevice(m_pmfxCore->pthis, type, &handle);
    MFX_CHECK_STS(mfxRes);
    if(0 == handle)
        return MFX_ERR_NOT_FOUND;

    m_mfxDeviceType = type;
    m_mfxDeviceHdl  = handle;

    UINT version = 0;
    int result = CM_FAILURE;//CreateCmDevice(pDevice, version);
#if defined(_WIN32) || defined(_WIN64)
    if(MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9 == m_mfxDeviceType)
        result = CreateCmDevice(m_pCMDevice,version,(IDirect3DDeviceManager9*) m_mfxDeviceHdl);
    else if(MFX_HANDLE_D3D11_DEVICE == m_mfxDeviceType)
        result = CreateCmDevice(m_pCMDevice,version,(ID3D11Device*) m_mfxDeviceHdl);
    else
        return MFX_ERR_UNKNOWN;
#else if defined(LINUX32) || defined (LINUX64)
    unsupported;
#endif

    if(CM_SUCCESS != result || 0 == m_pCMDevice)
        return MFX_ERR_DEVICE_FAILED;

    bool jit = true;
    const unsigned char * buffer = dirty_rect_genx_jit;
    unsigned int len = sizeof(dirty_rect_genx_jit);

    GPU_PLATFORM platform = PLATFORM_INTEL_UNKNOWN;
    size_t cap_size = sizeof(platform);
    m_pCMDevice->GetCaps(CAP_GPU_PLATFORM, cap_size, &platform);

    switch ( platform )
    {
        case PLATFORM_INTEL_HSW:
            jit = false;
            buffer = dirty_rect_genx_hsw;
            len = sizeof(dirty_rect_genx_hsw);
            break;

        case PLATFORM_INTEL_BDW:
        case PLATFORM_INTEL_CHV:
            jit = false;
            buffer = dirty_rect_genx_bdw;
            len = sizeof(dirty_rect_genx_bdw);
            break;

        case PLATFORM_INTEL_SKL:
        case PLATFORM_INTEL_BXT:
            jit = false;
            buffer = dirty_rect_genx_skl;
            len = sizeof(dirty_rect_genx_skl);
            break;

        case PLATFORM_INTEL_VLV:
        case PLATFORM_INTEL_SNB:
        case PLATFORM_INTEL_IVB:
        case PLATFORM_INTEL_CNL:
        default:
            break;
    }

    CmProgram * pProgram = NULL;
    if(jit)
        result = ::ReadProgramJit(m_pCMDevice, pProgram, buffer, len);
    else
        result = ::ReadProgram(m_pCMDevice, pProgram, buffer, len);

    if(CM_SUCCESS != result || 0 == pProgram)
    {
        jit = true;
        buffer = dirty_rect_genx_jit;
        len = sizeof(dirty_rect_genx_jit);

        result = ::ReadProgramJit(m_pCMDevice, pProgram, buffer, len);
        if(CM_SUCCESS != result || 0 == pProgram)
            return MFX_ERR_DEVICE_FAILED;
    }

    m_vPrograms.push_back(pProgram);

    result = m_pCMDevice->CreateQueue(m_pQueue);
    if(CM_SUCCESS != result || 0 == m_pQueue)
        return MFX_ERR_DEVICE_FAILED;

    const mfxU16 width  = m_width  = (par->mfx.FrameInfo.CropW ? par->mfx.FrameInfo.CropW : par->mfx.FrameInfo.Width );
    const mfxU16 height = m_height = (par->mfx.FrameInfo.CropH ? par->mfx.FrameInfo.CropH : par->mfx.FrameInfo.Height);

    //Init resources for each task
    const mfxU16 AsyncDepth = IPP_MAX(1, par->AsyncDepth);
    TaskContext* pTaskCtx = 0;
    for(mfxU32 i = 0; i < AsyncDepth; ++i)
    {
        try{
            pTaskCtx = new TaskContext;
        } catch(...) {
            Close();
            return MFX_ERR_MEMORY_ALLOC;
        }
        memset(pTaskCtx,0,sizeof(TaskContext));

        m_vpTasks.push_back(pTaskCtx);

        result = m_pCMDevice->CreateQueue(pTaskCtx->pQueue);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->pQueue);

        if(MFX_FOURCC_NV12 == par->mfx.FrameInfo.FourCC)
            result = m_pCMDevice->CreateKernel(pProgram, CM_KERNEL_FUNCTION( cmkDirtyRectNV12<16U, 16U> ), pTaskCtx->pKernel, 0);
        else
            result = m_pCMDevice->CreateKernel(pProgram, CM_KERNEL_FUNCTION( cmkDirtyRectRGB4<16U, 16U> ), pTaskCtx->pKernel, 0);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->pKernel);

        //unsigned int bl_w = width / 16;
        //unsigned int bl_h = height / 16;
        //result = pTaskCtx->pKernel->SetKernelArg(0, sizeof(unsigned int), &bl_w);
        //if(CM_SUCCESS != result)
        //    return MFX_ERR_DEVICE_FAILED;
        //result = pTaskCtx->pKernel->SetKernelArg(1, sizeof(unsigned int), &bl_h);
        //if(CM_SUCCESS != result)
        //    return MFX_ERR_DEVICE_FAILED;

        result = pTaskCtx->pKernel->SetKernelArg(0, sizeof(width), &width);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, width);

        result = pTaskCtx->pKernel->SetKernelArg(1, sizeof(height), &height);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, height);

        result = m_pCMDevice->CreateTask(pTaskCtx->pTask);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->pTask);

        result = pTaskCtx->pTask->AddKernel(pTaskCtx->pKernel);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->pTask);

        const UINT threadsWidth  = (m_width +  16 - 1) / 16; //from cmut::DivUp
        const UINT threadsHeight = (m_height + 16 - 1) / 16;
        result = m_pCMDevice->CreateThreadSpace(threadsWidth, threadsHeight, pTaskCtx->pThreadSpace);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->pThreadSpace);

        result = pTaskCtx->pKernel->SetThreadCount(threadsWidth * threadsHeight);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, threadsWidth * threadsHeight);

        UINT pitch = 0;
        UINT physSize = 0;
        UINT roi_W = threadsWidth;
        UINT roi_H = 4 * threadsHeight;
        pTaskCtx->roiMap.width  = threadsWidth;
        pTaskCtx->roiMap.height = threadsHeight;
        result = m_pCMDevice->GetSurface2DInfo(roi_W, roi_H, CM_SURFACE_FORMAT_A8R8G8B8, pitch, physSize);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pitch + physSize);

        pTaskCtx->roiMap.physBuffer = (mfxU8*) CM_ALIGNED_MALLOC(physSize, 0x1000);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->roiMap.physBuffer);
        memset(pTaskCtx->roiMap.physBuffer,0,physSize);
        pTaskCtx->roiMap.physBufferSz = physSize;
        pTaskCtx->roiMap.physBufferPitch = pitch;

        CM_SURFACE_FORMAT roi_format = CM_SURFACE_FORMAT_A8R8G8B8;
        result = m_pCMDevice->CreateSurface2DUP(roi_W, roi_H, roi_format, pTaskCtx->roiMap.physBuffer, pTaskCtx->roiMap.cmSurf);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->roiMap.cmSurf);

        SurfaceIndex* surfIndex;
        result = pTaskCtx->roiMap.cmSurf->GetIndex(surfIndex);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->roiMap.cmSurf);

        result = pTaskCtx->pKernel->SetKernelArg(2, sizeof(SurfaceIndex), surfIndex);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->roiMap.cmSurf);

        pTaskCtx->roiMap.roiMAP = ippsMalloc_8u(ALIGN16(pTaskCtx->roiMap.width) * ALIGN16(pTaskCtx->roiMap.height));
        MFXSC_CHECK_IPP_RESULT_AND_PTR(ippStsNoErr, pTaskCtx->roiMap.roiMAP);
    }

    return MFX_ERR_NONE;
}

mfxStatus CmDirtyRectFilter::Init(const mfxFrameInfo& in, const mfxFrameInfo& out)
{
    in;
    out;

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus CmDirtyRectFilter::RunFrameVPP(mfxFrameSurface1& in, mfxFrameSurface1& out)
{
    TaskContext* task = GetFreeTask();
    if(!task) return MFX_ERR_DEVICE_FAILED;

    mfxExtDirtyRect* mfxRect = GetExtendedBuffer<mfxExtDirtyRect>(MFX_EXTBUFF_DIRTY_RECTANGLES, &out);
    if(!mfxRect)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    Ipp8u* roiMap = task->roiMap.roiMAP;
    if(!roiMap)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    const IppiSize roiMapSize = {task->roiMap.width, task->roiMap.height};
    const IppiSize roiMapRealSize = {ALIGN16(task->roiMap.width), ALIGN16(task->roiMap.height)};

    IppStatus resultIPP = ippiSet_8u_C1R(0, roiMap, roiMapRealSize.width, roiMapRealSize);
    MFXSC_CHECK_IPP_RESULT_AND_PTR(resultIPP, 1);

    memset(task->roiMap.physBuffer, 0, task->roiMap.physBufferSz);

    int resultCM = 0;

    task->pTask->Reset();

    resultCM = task->pTask->AddKernel(task->pKernel);
    MFXSC_CHECK_CM_RESULT_AND_PTR(resultCM, 1);

    SurfaceIndex * pSurfIn  = GetCMSurfaceIndex(&in);
    MFXSC_CHECK_CM_RESULT_AND_PTR(CM_SUCCESS, pSurfIn);

    SurfaceIndex * pSurfOut = GetCMSurfaceIndex(&out);
    MFXSC_CHECK_CM_RESULT_AND_PTR(CM_SUCCESS, pSurfOut);

    resultCM = task->pKernel->SetKernelArg(3, sizeof(SurfaceIndex), pSurfIn);
    MFXSC_CHECK_CM_RESULT_AND_PTR(resultCM, 1);

    resultCM = task->pKernel->SetKernelArg(4, sizeof(SurfaceIndex), pSurfOut);
    MFXSC_CHECK_CM_RESULT_AND_PTR(resultCM, 1);

    CmEvent* pEvent = 0;
    resultCM = task->pQueue->Enqueue(task->pTask, pEvent, task->pThreadSpace);
    MFXSC_CHECK_CM_RESULT_AND_PTR(resultCM, 1);

    CM_STATUS status;
    pEvent->GetStatus(status);
    while (status != CM_STATUS_FINISHED) {
        pEvent->GetStatus(status);
    }

    task->pQueue->DestroyEvent(pEvent);
    pEvent = 0;

    mfxExtDirtyRect tmpRect;
    memset(&tmpRect,0,sizeof(tmpRect));
    tmpRect.Header.BufferId = MFX_EXTBUFF_DIRTY_RECTANGLES;
    tmpRect.Header.BufferSz = sizeof(tmpRect);

    Ipp64f res = 0.0f;
    mfxU64 totalRectsN = 0;
    const mfxU8* roiBuffer = task->roiMap.physBuffer;
    const mfxU32 rowSize = task->roiMap.physBufferPitch;

    for(Ipp32s i = 0; i < roiMapSize.height; ++i)
    {
        for(Ipp32s j = 0; j < roiMapSize.width; ++j)
        {
            roiMap[i * roiMapRealSize.width + j] = *(roiBuffer + 4 * (i * rowSize) + 4 * j);
        }
    }

    resultIPP = ippiSum_8u_C1R(roiMap, roiMapRealSize.width, roiMapRealSize, &res);
    MFXSC_CHECK_IPP_RESULT_AND_PTR(resultIPP, 1);

    totalRectsN = (int) (res + 0.5);

    Ipp8u  macroMap[W_BLOCKS * W_BLOCKS];
    Ipp16u densityMap[W_BLOCKS * H_BLOCKS];
    const IppiSize roiBlock16x16 = {W_BLOCKS, H_BLOCKS};

    resultIPP = ippiSet_8u_C1R(0, macroMap, roiBlock16x16.width, roiBlock16x16);
    MFXSC_CHECK_IPP_RESULT_AND_PTR(resultIPP, 1);
    resultIPP = ippiSet_16u_C1R(0, densityMap, roiBlock16x16.width << 1, roiBlock16x16);
    MFXSC_CHECK_IPP_RESULT_AND_PTR(resultIPP, 1);

    const mfxU32 maxRectsN = 256;
    const IppiSize roiBigBlock = {roiMapRealSize.width / W_BLOCKS, roiMapRealSize.height / H_BLOCKS};

    mfxU32 macroMapN = 0;

    if(totalRectsN > maxRectsN)
    {
        int    idxX, idxY;
        Ipp16u maxDensity;

        //calc density
        for(mfxU32 i = 0; i < H_BLOCKS; ++i)
        {
            for(mfxU32 j = 0; j < W_BLOCKS; ++j)
            {
                resultIPP = ippiSum_8u_C1R(roiMap + i * roiBigBlock.height * roiMapRealSize.width + j * roiBigBlock.width, roiMapRealSize.width, roiBigBlock, &res);
                MFXSC_CHECK_IPP_RESULT_AND_PTR(resultIPP, 1);
                densityMap[i * roiBlock16x16.width + j] = (Ipp16u) (res + 0.5);
            }
        }

        //group cm blocks into big blocks by density
        do
        {
            resultIPP = ippiMax_16u_C1R(densityMap, roiBlock16x16.width << 1, roiBlock16x16, &maxDensity);
            MFXSC_CHECK_IPP_RESULT_AND_PTR(resultIPP, 1);

            for (int i = 0; i < maxRectsN; ++i)
            {
                if (densityMap[i] + 3 < maxDensity) continue;

                idxY = i >> 4;
                idxX = i - (idxY << 4);

                resultIPP = ippiSet_8u_C1R(0, roiMap + idxY * roiBigBlock.height * roiMapRealSize.width + idxX * roiBigBlock.width, roiMapRealSize.width, roiBigBlock);
                MFXSC_CHECK_IPP_RESULT_AND_PTR(resultIPP, 1);

                totalRectsN -= (densityMap[idxY * roiBlock16x16.width + idxX] - 1); // instead of all macroblocks we save only one big rectangle
                macroMap  [idxY * roiBlock16x16.width + idxX] = 1;
                densityMap[idxY * roiBlock16x16.width + idxX] = 1;

                macroMapN++;
            }
        } while (totalRectsN > maxRectsN);
    }

    if (macroMapN > 0)
    {
        for(mfxU32 i = 0; i < H_BLOCKS; ++i)
        {
            for(mfxU32 j = 0; j < W_BLOCKS; ++j)
            {
                if( macroMap[i * roiBlock16x16.width + j] )
                {
                    assert(tmpRect.NumRect <= maxRectsN);
                    if(maxRectsN <= tmpRect.NumRect) continue;

                    //roi
                    tmpRect.Rect[tmpRect.NumRect].Top = i * roiBigBlock.height * H_BLOCKS;
                    tmpRect.Rect[tmpRect.NumRect].Bottom = IPP_MIN(tmpRect.Rect[tmpRect.NumRect].Top + roiBigBlock.height * H_BLOCKS, m_height);
                    tmpRect.Rect[tmpRect.NumRect].Left = j * roiBigBlock.width * W_BLOCKS;
                    tmpRect.Rect[tmpRect.NumRect].Right = IPP_MIN(tmpRect.Rect[tmpRect.NumRect].Left + roiBigBlock.width * W_BLOCKS, m_width);

                    ++tmpRect.NumRect;
                }
            }
        }
    }

    if(totalRectsN - macroMapN > 0)
    {
        for(mfxU32 i = 0; i < task->roiMap.height; ++i)
        {
            for(mfxU32 j = 0; j < task->roiMap.width; ++j)
            {
                if( roiMap[i * roiMapRealSize.width + j] )
                {
                    assert(tmpRect.NumRect <= maxRectsN);
                    if(maxRectsN <= tmpRect.NumRect) continue;

                    //roi
                    tmpRect.Rect[tmpRect.NumRect].Top = i * 16;
                    tmpRect.Rect[tmpRect.NumRect].Bottom = IPP_MIN(tmpRect.Rect[tmpRect.NumRect].Top + 16, m_height);
                    tmpRect.Rect[tmpRect.NumRect].Left = j * 16;
                    tmpRect.Rect[tmpRect.NumRect].Right = IPP_MIN(tmpRect.Rect[tmpRect.NumRect].Left + 16, m_width);

                    ++tmpRect.NumRect;
                }
            }
        }
    }

    if(mfxRect)
        *mfxRect = tmpRect;

    ReleaseTask(task);

    return MFX_ERR_NONE;
}

mfxStatus CmDirtyRectFilter::GetParam(mfxFrameInfo& in, mfxFrameInfo& out)
{
    in;
    out;

    return MFX_ERR_UNSUPPORTED;
}

TaskContext* CmDirtyRectFilter::GetFreeTask()
{
    TaskContext* task = 0;
    if(0 == m_vpTasks.size())
        return 0;
    else
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        for(std::vector<TaskContext*>::iterator it = m_vpTasks.begin(); it != m_vpTasks.end(); ++it)
        {
            if((*it) && 0 == (*it)->busy)
            {
                task = (*it);
                ++(task->busy);
                return task;
            }
        }
    }
    return 0;
}

void CmDirtyRectFilter::ReleaseTask(TaskContext*& task)
{
    if(!task) return;
    --task->busy;
}

CmSurface2D* CmDirtyRectFilter::GetCMSurface(const mfxFrameSurface1* mfxSurf)
{
    int cmSts = 0;

    mfxFrameSurface1* opaqSurf = 0;
    if(m_bOpaqMem)
    {
        mfxStatus mfxSts = m_pmfxCore->GetRealSurface(m_pmfxCore->pthis, (mfxFrameSurface1*) mfxSurf, &opaqSurf);
        if(MFX_ERR_NONE == mfxSts || 0 == opaqSurf)
            return 0;
    }
    const mfxFrameSurface1* realSurf = opaqSurf ? opaqSurf : mfxSurf;

    CmSurface2D*& cmSurf = m_CMSurfaces[realSurf];

    bool sysSurf = false;
    mfxHDLPair native_surf = {};
    mfxStatus mfxSts = m_pmfxCore->FrameAllocator.GetHDL(m_pmfxCore->FrameAllocator.pthis, realSurf->Data.MemId, (mfxHDL*)&native_surf);
    if(MFX_ERR_NONE > mfxSts)
        sysSurf = true;

    if( sysSurf )
    {
        if(!cmSurf)
        {
            if(realSurf->Info.FourCC == MFX_FOURCC_NV12)
                cmSts = m_pCMDevice->CreateSurface2D(realSurf->Info.Width, realSurf->Info.Height, CM_SURFACE_FORMAT_NV12, cmSurf);
            else
                cmSts = m_pCMDevice->CreateSurface2D(realSurf->Info.Width, realSurf->Info.Height, CM_SURFACE_FORMAT_A8R8G8B8, cmSurf);
            assert(cmSts == 0);
            if(CM_SUCCESS != cmSts || 0 == cmSurf)
                return 0;
        }
        mfxSts = CMCopySysToGpu(cmSurf, (mfxFrameSurface1*) realSurf);
        assert(MFX_ERR_NONE == mfxSts);
        if(mfxSts)
            return 0;
    }
    else
    {
        if(!cmSurf)
        {
            cmSts = m_pCMDevice->CreateSurface2D(native_surf.first, cmSurf);
            if(CM_SUCCESS != cmSts || 0 == cmSurf)
                return 0;
        }
    }

    return cmSurf;
}

SurfaceIndex* CmDirtyRectFilter::GetCMSurfaceIndex(const mfxFrameSurface1* mfxSurf)
{
    SurfaceIndex * index = 0;
    CmSurface2D* cmSurf = GetCMSurface(mfxSurf);
    assert(0 != cmSurf);
    if(cmSurf)
        cmSurf->GetIndex(index);
    return index;
}

mfxStatus CmDirtyRectFilter::CMCopySysToGpu(CmSurface2D* cmDst, mfxFrameSurface1* mfxSrc)
{
    if(!mfxSrc)
        return MFX_ERR_NULL_PTR;

    mfxI32 cmSts = CM_SUCCESS;
    CM_STATUS sts;
    mfxStatus mfxSts = MFX_ERR_NONE;

    bool unlock_req = false;
    if(mfxSrc->Data.MemId)
    {
        mfxSts = m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, mfxSrc->Data.MemId, &mfxSrc->Data);
        if(MFX_ERR_NONE != mfxSts)
            return mfxSts;
        unlock_req = true;
        if(!mfxSrc->Data.Y || !mfxSrc->Data.UV)
            return MFX_ERR_LOCK_MEMORY;
    }

    mfxU32 srcPitch = mfxSrc->Data.Pitch;
    CmEvent* e = NULL;
    if(MFX_FOURCC_NV12 == mfxSrc->Info.FourCC)
    {
        mfxI64 verticalPitch = (mfxI64)(mfxSrc->Data.UV - mfxSrc->Data.Y);
        verticalPitch = (verticalPitch % mfxSrc->Data.Pitch)? 0 : verticalPitch / mfxSrc->Data.Pitch;
        mfxI64& srcUVOffset = verticalPitch;

        if(srcUVOffset != 0){
            cmSts = m_pQueue->EnqueueCopyCPUToGPUFullStride(cmDst, mfxSrc->Data.Y, srcPitch, (mfxU32) srcUVOffset, 0, e);
        }
        else{
            cmSts = m_pQueue->EnqueueCopyCPUToGPUStride(cmDst, mfxSrc->Data.Y, srcPitch, e);
        }
    }
    else
    {
        mfxU8* src = IPP_MIN(IPP_MIN(mfxSrc->Data.R,mfxSrc->Data.G),mfxSrc->Data.B);
        mfxU32 srcUVOffset = mfxSrc->Info.Height;

        cmSts = m_pQueue->EnqueueCopyCPUToGPUFullStride(cmDst, src, srcPitch, srcUVOffset, 0, e);
    }



    if (CM_SUCCESS == cmSts && e)
    {
        e->GetStatus(sts);

        while (sts != CM_STATUS_FINISHED)
        {
            e->GetStatus(sts);
        }
    }else{
        mfxSts = MFX_ERR_DEVICE_FAILED;
    }
    if(e) m_pQueue->DestroyEvent(e);

    if(mfxSts)
        return mfxSts;

    if(unlock_req)
    {
        mfxSts = m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, mfxSrc->Data.MemId, &mfxSrc->Data);
    }

    return mfxSts;
}

void CmDirtyRectFilter::FreeSurfs()
{
    if(!m_pCMDevice || 0 == m_CMSurfaces.size()) return;
    UMC::AutomaticUMCMutex guard(m_guard);

    int result = 0;
    for(std::map<const mfxFrameSurface1*,CmSurface2D*>::iterator it = m_CMSurfaces.begin(); it != m_CMSurfaces.end(); ++it)
    {
        CmSurface2D*& cm_surf = it->second;
        if(cm_surf)
        {
            try{
                result = m_pCMDevice->DestroySurface(cm_surf);
                assert(result == 0);
                cm_surf = 0;
            }
            catch(...)
            {
            }
        }
    }

    m_CMSurfaces.clear();
    return;
}

mfxStatus CmDirtyRectFilter::Close()
{
    UMC::AutomaticUMCMutex guard(m_guard);
    FreeSurfs();

    for(std::vector<TaskContext*>::iterator it = m_vpTasks.begin(); it != m_vpTasks.end(); ++it)
    {
        TaskContext*& task = (*it);
        if(task)
        {
            if(m_pCMDevice)
            {
                if(task->pKernel)
                {
                    m_pCMDevice->DestroyKernel(task->pKernel);
                    task->pKernel = 0;
                }
                if(task->pTask)
                {
                    m_pCMDevice->DestroyTask(task->pTask);
                    task->pTask = 0;
                }
                if(task->pThreadSpace)
                {
                    m_pCMDevice->DestroyThreadSpace(task->pThreadSpace);
                    task->pThreadSpace = 0;
                }
                if(task->pQueue)
                {
                    //no func to destroy Queue
                    task->pQueue = 0;
                }

                if(task->roiMap.cmSurf)
                {
                    m_pCMDevice->DestroySurface2DUP(task->roiMap.cmSurf);
                    task->roiMap.cmSurf = 0;
                }
            }
            if(task->roiMap.roiMAP)
            {
                ippiFree(task->roiMap.roiMAP);
            }

            if(task->roiMap.physBuffer)
            {
                CM_ALIGNED_FREE(task->roiMap.physBuffer);
                task->roiMap.physBuffer = 0;
            }
            task->roiMap.physBufferPitch = 0;
            task->roiMap.physBufferSz = 0;

            delete task;
            task = 0;
        }
    }
    m_vpTasks.clear();

    for(std::vector<CmProgram*>::iterator it = m_vPrograms.begin(); it != m_vPrograms.end(); ++it)
    {
        CmProgram*& program = (*it);
        if(m_pCMDevice)
            m_pCMDevice->DestroyProgram(program);
        program = 0;
    }
    m_vPrograms.clear();

    if(m_pCMDevice)
    {
        DestroyCmDevice(m_pCMDevice);
        m_pCMDevice = 0;
    }

    return MFX_ERR_NONE;

}


} //namespace MfxCapture
