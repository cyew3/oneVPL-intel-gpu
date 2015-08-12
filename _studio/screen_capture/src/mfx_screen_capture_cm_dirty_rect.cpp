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

#define H_BLOCKS 16
#define W_BLOCKS 16

#if !defined(MSDK_ALIGN32)
#define MSDK_ALIGN32(value)                      (((value + 31) >> 5) << 5) // round up to a multiple of 32
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

    CmProgram * pProgram = NULL;
    if(jit)
        result = ::ReadProgramJit(m_pCMDevice, pProgram, dirty_rect_genx_hsw, sizeof(dirty_rect_genx_hsw));
    else
        result = ::ReadProgram(m_pCMDevice, pProgram, dirty_rect_genx_hsw, sizeof(dirty_rect_genx_hsw));

    if(CM_SUCCESS != result || 0 == pProgram)
        return MFX_ERR_DEVICE_FAILED;

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
        pTaskCtx = new TaskContext;
        memset(pTaskCtx,0,sizeof(TaskContext));

        m_vpTasks.push_back(pTaskCtx);

        result = m_pCMDevice->CreateQueue(pTaskCtx->pQueue);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->pQueue);

        if(MFX_FOURCC_NV12 == par->mfx.FrameInfo.FourCC)
            result = m_pCMDevice->CreateKernel(pProgram, CM_KERNEL_FUNCTION(DirtyRectNV12), pTaskCtx->pKernel, 0);
        else
            result = m_pCMDevice->CreateKernel(pProgram, CM_KERNEL_FUNCTION(DirtyRectRGB4), pTaskCtx->pKernel, 0);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->pKernel);

        unsigned int bl_w = width / 16;
        unsigned int bl_h = height / 16;
        result = pTaskCtx->pKernel->SetKernelArg(0, sizeof(unsigned int), &bl_w);
        if(CM_SUCCESS != result)
            return MFX_ERR_DEVICE_FAILED;
        result = pTaskCtx->pKernel->SetKernelArg(1, sizeof(unsigned int), &bl_h);
        if(CM_SUCCESS != result)
            return MFX_ERR_DEVICE_FAILED;

        result = pTaskCtx->pKernel->SetKernelArg(3, sizeof(width), &width);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, width);

        result = pTaskCtx->pKernel->SetKernelArg(4, sizeof(height), &height);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, height);

        result = m_pCMDevice->CreateTask(pTaskCtx->pTask);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->pTask);

        result = pTaskCtx->pTask->AddKernel(pTaskCtx->pKernel);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->pTask);

        const UINT threadsWidth  = (m_width + 4 - 1) / 4; //from cmut::DivUp
        const UINT threadsHeight = (m_height + 4 - 1) / 4;
        result = m_pCMDevice->CreateThreadSpace(threadsWidth, threadsHeight, pTaskCtx->pThreadSpace);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, pTaskCtx->pThreadSpace);

        result = pTaskCtx->pKernel->SetThreadCount(threadsWidth * threadsHeight);
        MFXSC_CHECK_CM_RESULT_AND_PTR(result, threadsWidth * threadsHeight);

        UINT pitch = 0;
        UINT physSize = 0;
        UINT roi_W = W_BLOCKS;
        UINT roi_H = 4 * H_BLOCKS;
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
    }

    return MFX_ERR_NONE;
}

mfxStatus CmDirtyRectFilter::Init(const mfxFrameInfo& in, const mfxFrameInfo& out)
{
    in;
    out;

    return MFX_ERR_UNSUPPORTED;
}

#if 0
mfxStatus CmDirtyRectFilter::RunFrameVPP_1(mfxFrameSurface1& in, mfxFrameSurface1& out)
{
    const UINT threadsWidth = (m_width + 4 - 1) / 4; //from cmut::DivUp
    const UINT threadsHeight = (m_height + 4 - 1) / 4;

    CmThreadSpace * pThreadSpace = 0;

    int result = m_pCMDevice->CreateThreadSpace(threadsWidth, threadsHeight, pThreadSpace);
    if(CM_SUCCESS != result || 0 == pThreadSpace)
        return MFX_ERR_DEVICE_FAILED;

    result = m_pKernel->SetThreadCount(threadsWidth * threadsHeight);
    if(CM_SUCCESS != result)
        return MFX_ERR_DEVICE_FAILED;

    //m_pCMDevice->InitPrintBuffer();

    UINT pitch = 0;
    UINT physSize = 0;
    UINT roi_W = 16;
    UINT roi_H = 4 * 16;
    result = m_pCMDevice->GetSurface2DInfo(roi_W, roi_H, CM_SURFACE_FORMAT_A8R8G8B8, pitch, physSize);
    if(CM_SUCCESS != result || 0 == pitch || 0 == physSize)
        return MFX_ERR_DEVICE_FAILED;

    char* pData = (char*) CM_ALIGNED_MALLOC(physSize, 0x1000);
    memset(pData,0,physSize);

    CmSurface2DUP * CmROIMap = 0;
    CM_SURFACE_FORMAT roi_format = CM_SURFACE_FORMAT_A8R8G8B8;
    result = m_pCMDevice->CreateSurface2DUP(roi_W, roi_H, roi_format, pData, CmROIMap);
    if(CM_SUCCESS != result || 0 == CmROIMap)
        return MFX_ERR_DEVICE_FAILED;

    CmSurface2D * cmsurf_in = 0;
    CmSurface2D * cmsurf_out = 0;

    mfxHDLPair native_surf = {};
    mfxStatus mfxSts = m_pmfxCore->FrameAllocator.GetHDL(m_pmfxCore->FrameAllocator.pthis, in.Data.MemId, (mfxHDL*)&native_surf);
    if(MFX_ERR_NONE > mfxSts)
        return mfxSts;

    result = m_pCMDevice->CreateSurface2D(native_surf.first, cmsurf_in);
    if(CM_SUCCESS != result || 0 == cmsurf_in)
        return MFX_ERR_DEVICE_FAILED;

    native_surf.first = native_surf.second = 0;
    mfxSts = m_pmfxCore->FrameAllocator.GetHDL(m_pmfxCore->FrameAllocator.pthis, out.Data.MemId, (mfxHDL*)&native_surf);
    if(MFX_ERR_NONE > mfxSts)
        return mfxSts;

    result = m_pCMDevice->CreateSurface2D(native_surf.first, cmsurf_out);
    if(CM_SUCCESS != result || 0 == cmsurf_in)
        return MFX_ERR_DEVICE_FAILED;

    SurfaceIndex * pSurfaceIndex = 0;
    result = cmsurf_in->GetIndex(pSurfaceIndex);
    if(CM_SUCCESS != result)
        return MFX_ERR_DEVICE_FAILED;

    result = m_pKernel->SetKernelArg(0, sizeof(SurfaceIndex), pSurfaceIndex);
    if(CM_SUCCESS != result)
        return MFX_ERR_DEVICE_FAILED;

    result = cmsurf_out->GetIndex(pSurfaceIndex);
    if(CM_SUCCESS != result)
        return MFX_ERR_DEVICE_FAILED;

    result = m_pKernel->SetKernelArg(1, sizeof(SurfaceIndex), pSurfaceIndex);
    if(CM_SUCCESS != result)
        return MFX_ERR_DEVICE_FAILED;

    unsigned int bl_w = in.Info.Width / 16;
    unsigned int bl_h = in.Info.Height / 16;
    result = m_pKernel->SetKernelArg(2, sizeof(unsigned int), &bl_w);
    if(CM_SUCCESS != result)
        return MFX_ERR_DEVICE_FAILED;
    result = m_pKernel->SetKernelArg(3, sizeof(unsigned int), &bl_h);
    if(CM_SUCCESS != result)
        return MFX_ERR_DEVICE_FAILED;

    result = CmROIMap->GetIndex(pSurfaceIndex);
    if(CM_SUCCESS != result)
        return MFX_ERR_DEVICE_FAILED;

    result = m_pKernel->SetKernelArg(4, sizeof(SurfaceIndex), pSurfaceIndex);
    if(CM_SUCCESS != result)
        return MFX_ERR_DEVICE_FAILED;

    CmTask * pTask = 0;
    result = m_pCMDevice->CreateTask(pTask);
    if(CM_SUCCESS != result || 0 == pTask)
        return MFX_ERR_DEVICE_FAILED;

    result = pTask->AddKernel(m_pKernel);
    if(CM_SUCCESS != result)
        return MFX_ERR_DEVICE_FAILED;

    CmEvent* pEvent = 0;
    result = m_pQueue->Enqueue(pTask, pEvent, pThreadSpace);
    if(CM_SUCCESS != result)
        return MFX_ERR_DEVICE_FAILED;

    CM_STATUS status;
    pEvent->GetStatus(status);
    while (status != CM_STATUS_FINISHED) {
        pEvent->GetStatus(status);
    }

    m_pCMDevice->DestroyTask(pTask);
    pTask = 0;

    m_pQueue->DestroyEvent(pEvent);
    pEvent = 0;

    m_pCMDevice->DestroyThreadSpace(pThreadSpace);
    pThreadSpace = 0;

    m_pCMDevice->DestroySurface(cmsurf_in);
    cmsurf_in = 0;
    m_pCMDevice->DestroySurface(cmsurf_out);
    cmsurf_out = 0;

    //m_pCMDevice->FlushPrintBuffer();

    mfxExtDirtyRect* mfxRect = GetExtendedBuffer<mfxExtDirtyRect>(MFX_EXTBUFF_DIRTY_RECTANGLES, &out);
    mfxExtDirtyRect tmpRect;
    memset(&tmpRect,0,sizeof(tmpRect));
    tmpRect.Header.BufferId = MFX_EXTBUFF_DIRTY_RECTANGLES;
    tmpRect.Header.BufferSz = sizeof(tmpRect);
    const mfxU32 w_block_size = m_width  / 16;
    const mfxU32 h_block_size = m_height / 16;

    //mfxU32 numROI = 0;
    //mfxU8 map[64*16];
    //memset(&map,0,sizeof(map));

    //for (int row = 0; row < 64; row++)
    //    memcpy(map + 16*4*row, pData + row * pitch, 16*4);

    for(mfxU32 i = 0; i < H_BLOCKS; ++i)
    {
        for(mfxU32 j = 0; j < W_BLOCKS; ++j)
        {
            if( *(pData + 4*(i*roi_W*4) + j * 4) )
            {
                //roi
                tmpRect.Rect[tmpRect.NumRect].Top = i * h_block_size;
                tmpRect.Rect[tmpRect.NumRect].Bottom = tmpRect.Rect[tmpRect.NumRect].Top + h_block_size;
                tmpRect.Rect[tmpRect.NumRect].Left = j * w_block_size;
                tmpRect.Rect[tmpRect.NumRect].Right = tmpRect.Rect[tmpRect.NumRect].Left + w_block_size;

                ++tmpRect.NumRect;
            }
        }
    }

    bool isDataPresent = false;
    for(UINT k = 0; k < physSize; ++k)
    {
        if(pData[k])
        {
            isDataPresent = true;
        }
    }

    if(mfxRect)
        *mfxRect = tmpRect;

    m_pCMDevice->DestroySurface2DUP(CmROIMap);
    CmROIMap = 0;

    if(pData)
    {
        CM_ALIGNED_FREE(pData);
        pData = 0;
    }

    return MFX_ERR_NONE;
}
#endif

mfxStatus CmDirtyRectFilter::RunFrameVPP(mfxFrameSurface1& in, mfxFrameSurface1& out)
{
    TaskContext* task = GetFreeTask();
    if(!task) return MFX_ERR_DEVICE_FAILED;

    memset(task->roiMap.physBuffer,0,task->roiMap.physBufferSz);

    int result = 0;

    task->pTask->Reset();

    result = task->pTask->AddKernel(task->pKernel);
    if(CM_SUCCESS != result)
        return MFX_ERR_DEVICE_FAILED;

    SurfaceIndex * pSurfIn  = GetCMSurfaceIndex(&in);
    if(!pSurfIn)
        return MFX_ERR_DEVICE_FAILED;
    SurfaceIndex * pSurfOut = GetCMSurfaceIndex(&out);
    if(!pSurfOut)
        return MFX_ERR_DEVICE_FAILED;

    result = task->pKernel->SetKernelArg(5, sizeof(SurfaceIndex), pSurfIn);
    if(CM_SUCCESS != result)
        return MFX_ERR_DEVICE_FAILED;

    result = task->pKernel->SetKernelArg(6, sizeof(SurfaceIndex), pSurfOut);
    if(CM_SUCCESS != result)
        return MFX_ERR_DEVICE_FAILED;

    CmEvent* pEvent = 0;
    result = task->pQueue->Enqueue(task->pTask, pEvent, task->pThreadSpace);
    if(CM_SUCCESS != result)
        return MFX_ERR_DEVICE_FAILED;

    CM_STATUS status;
    pEvent->GetStatus(status);
    while (status != CM_STATUS_FINISHED) {
        pEvent->GetStatus(status);
    }

    task->pQueue->DestroyEvent(pEvent);
    pEvent = 0;

    mfxExtDirtyRect* mfxRect = GetExtendedBuffer<mfxExtDirtyRect>(MFX_EXTBUFF_DIRTY_RECTANGLES, &out);
    mfxExtDirtyRect tmpRect;
    memset(&tmpRect,0,sizeof(tmpRect));
    tmpRect.Header.BufferId = MFX_EXTBUFF_DIRTY_RECTANGLES;
    tmpRect.Header.BufferSz = sizeof(tmpRect);
    const mfxU32 w_block_size = m_width  / 16;
    const mfxU32 h_block_size = m_height / 16;
    UINT roi_W = W_BLOCKS;
    //UINT roi_H = 4 * H_BLOCKS;

    for(mfxU32 i = 0; i < H_BLOCKS - 1; ++i)
    {
        for(mfxU32 j = 0; j < W_BLOCKS - 1; ++j)
        {
            if( *(task->roiMap.physBuffer + 4*(i*roi_W*4) + j * 4) )
            {
                //roi
                tmpRect.Rect[tmpRect.NumRect].Top = i * h_block_size;
                tmpRect.Rect[tmpRect.NumRect].Bottom = tmpRect.Rect[tmpRect.NumRect].Top + h_block_size;
                tmpRect.Rect[tmpRect.NumRect].Left = j * w_block_size;
                tmpRect.Rect[tmpRect.NumRect].Right = tmpRect.Rect[tmpRect.NumRect].Left + w_block_size;

                ++tmpRect.NumRect;
            }
        }
        mfxU32 j = W_BLOCKS - 1;
        if( *(task->roiMap.physBuffer + 4*(i*roi_W*4) + j * 4) )
        {
            //roi
            tmpRect.Rect[tmpRect.NumRect].Top = i * h_block_size;
            tmpRect.Rect[tmpRect.NumRect].Bottom = tmpRect.Rect[tmpRect.NumRect].Top + h_block_size;
            tmpRect.Rect[tmpRect.NumRect].Left = j * w_block_size;
            tmpRect.Rect[tmpRect.NumRect].Right = m_width;

            ++tmpRect.NumRect;
        }
    }
    //last row
    mfxU32 i = H_BLOCKS - 1;
    for(mfxU32 j = 0; j < W_BLOCKS - 1; ++j)
    {
        if( *(task->roiMap.physBuffer + 4*(i*roi_W*4) + j * 4) )
        {
            //roi
            tmpRect.Rect[tmpRect.NumRect].Top = i * h_block_size;
            tmpRect.Rect[tmpRect.NumRect].Bottom = m_height;
            tmpRect.Rect[tmpRect.NumRect].Left = j * w_block_size;
            tmpRect.Rect[tmpRect.NumRect].Right = tmpRect.Rect[tmpRect.NumRect].Left + w_block_size;

            ++tmpRect.NumRect;
        }
    }
    mfxU32 j = W_BLOCKS - 1;
    if( *(task->roiMap.physBuffer + 4*(i*roi_W*4) + j * 4) )
    {
        //roi
        tmpRect.Rect[tmpRect.NumRect].Top = i * h_block_size;
        tmpRect.Rect[tmpRect.NumRect].Bottom = m_height;
        tmpRect.Rect[tmpRect.NumRect].Left = j * w_block_size;
        tmpRect.Rect[tmpRect.NumRect].Right = m_width;

        ++tmpRect.NumRect;
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
