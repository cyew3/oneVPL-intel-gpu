/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_d3d9.cpp

\* ****************************************************************************** */

#include "mfx_screen_capture_cm_dirty_rect.h"
#include "mfx_utils.h"
#include "mfxstructures.h"
#include <memory>
#include <list>
#include <algorithm> //min

#include "dirty_rect_genx_hsw_isa.h"

#define H_BLOCKS 16
#define W_BLOCKS 16

#if !defined(MSDK_ALIGN32)
#define MSDK_ALIGN32(value)                      (((value + 31) >> 5) << 5) // round up to a multiple of 32
#endif

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
    m_pCMDevice   = 0;
    m_pQueue      = 0;
    m_pKernelNV12 = 0;
    m_pKernelRGB4 = 0;

    m_width = 0;
    m_height = 0;
}

CmDirtyRectFilter::~CmDirtyRectFilter()
{
    ;
    //FreeSurfs();
}

mfxStatus CmDirtyRectFilter::Init(const mfxVideoParam* par, bool isSysMem)
{
    if(isSysMem)
        return MFX_ERR_UNSUPPORTED;

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

    result = m_pCMDevice->CreateKernel(pProgram, CM_KERNEL_FUNCTION(DirtyRectNV12), m_pKernelNV12, 0);
    if(CM_SUCCESS != result || 0 == m_pKernelNV12)
        return MFX_ERR_DEVICE_FAILED;

    result = m_pCMDevice->CreateKernel(pProgram, CM_KERNEL_FUNCTION(DirtyRectRGB4), m_pKernelRGB4, 0);
    if(CM_SUCCESS != result || 0 == m_pKernelRGB4)
        return MFX_ERR_DEVICE_FAILED;

    if(MFX_FOURCC_NV12 == par->mfx.FrameInfo.FourCC)
        m_pKernel = m_pKernelNV12;
    else
        m_pKernel = m_pKernelRGB4;

    const mfxU16 width  = m_width  = par->mfx.FrameInfo.CropW ? par->mfx.FrameInfo.CropW : par->mfx.FrameInfo.Width;
    const mfxU16 height = m_height = par->mfx.FrameInfo.CropH ? par->mfx.FrameInfo.CropH : par->mfx.FrameInfo.Height;

    result = m_pKernel->SetKernelArg(5, sizeof(width), &width);
    if(CM_SUCCESS != result)
        return MFX_ERR_DEVICE_FAILED;

    result = m_pKernel->SetKernelArg(6, sizeof(height), &height);
    if(CM_SUCCESS != result)
        return MFX_ERR_DEVICE_FAILED;

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
                tmpRect.Rect[tmpRect.NumRect].Bottom = tmpRect.Rect[tmpRect.NumRect].Top + h_block_size - 1;
                tmpRect.Rect[tmpRect.NumRect].Left = j * w_block_size;
                tmpRect.Rect[tmpRect.NumRect].Right = tmpRect.Rect[tmpRect.NumRect].Left + w_block_size - 1;

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


mfxStatus CmDirtyRectFilter::GetParam(mfxFrameInfo& in, mfxFrameInfo& out)
{
    in;
    out;

    return MFX_ERR_UNSUPPORTED;
}

mfxFrameSurface1* CmDirtyRectFilter::AllocSurfs(const mfxVideoParam* par)
{
    UMC::AutomaticUMCMutex guard(m_guard);
    if(par)
    {
        //Create own surface pool
        for(mfxU16 i = 0; i < (std::max)(1,(int)par->AsyncDepth); ++i)
        {
            mfxFrameSurface1 inSurf;
            mfxFrameSurface1 outSurf;
            memset(&inSurf,0,sizeof(inSurf));

            inSurf.Info = par->mfx.FrameInfo;
            inSurf.Info.Width = MSDK_ALIGN32(inSurf.Info.Width);
            inSurf.Info.Height = MSDK_ALIGN32(inSurf.Info.Height);

            outSurf = inSurf;
            if(MFX_FOURCC_NV12 == inSurf.Info.FourCC)
            {
                inSurf.Data.Y = new mfxU8[inSurf.Info.Width * inSurf.Info.Height * 3 / 2];
                inSurf.Data.UV = inSurf.Data.Y + inSurf.Info.Width * inSurf.Info.Height;
                inSurf.Data.Pitch = inSurf.Info.Width;
            }
            else if(MFX_FOURCC_RGB4 == inSurf.Info.FourCC || 
                    MFX_FOURCC_AYUV_RGB4 == inSurf.Info.FourCC || 
                    /*DXGI_FORMAT_AYUV*/100 == inSurf.Info.FourCC)
            {
                inSurf.Data.B = new mfxU8[inSurf.Info.Width * inSurf.Info.Height * 4];
                inSurf.Data.G = inSurf.Data.B + 1;
                inSurf.Data.R = inSurf.Data.B + 2;
                inSurf.Data.A = inSurf.Data.B + 3;

                inSurf.Data.Pitch = 4*inSurf.Info.Width;
            }
            m_InSurfPool.push_back(inSurf);

            if(MFX_FOURCC_NV12 == outSurf.Info.FourCC)
            {
                outSurf.Data.Y = new mfxU8[outSurf.Info.Width * outSurf.Info.Height * 3 / 2];
                outSurf.Data.UV = outSurf.Data.Y + outSurf.Info.Width * outSurf.Info.Height;
                outSurf.Data.Pitch = outSurf.Info.Width;
            }
            else if(MFX_FOURCC_RGB4 == outSurf.Info.FourCC || 
                    MFX_FOURCC_AYUV_RGB4 == outSurf.Info.FourCC || 
                    /*DXGI_FORMAT_AYUV*/100 == outSurf.Info.FourCC)
            {
                outSurf.Data.B = new mfxU8[outSurf.Info.Width * outSurf.Info.Height * 4];
                outSurf.Data.G = outSurf.Data.B + 1;
                outSurf.Data.R = outSurf.Data.B + 2;
                outSurf.Data.A = outSurf.Data.B + 3;

                outSurf.Data.Pitch = 4*outSurf.Info.Width;
            }
            m_OutSurfPool.push_back(outSurf);
        }
    }
    return 0;
}

//void CmDirtyRectFilter::FreeSurfs()
//{
//    UMC::AutomaticUMCMutex guard(m_guard);
//
//    FreeSurfPool(m_InSurfPool);
//    FreeSurfPool(m_OutSurfPool);
//}
//
//mfxFrameSurface1* CmDirtyRectFilter::GetFreeSurf(std::list<mfxFrameSurface1>& lSurfs)
//{
//    mfxFrameSurface1* s_out;
//    if(0 == lSurfs.size())
//        return 0;
//    else
//    {
//        UMC::AutomaticUMCMutex guard(m_guard);
//
//        for(std::list<mfxFrameSurface1>::iterator it = lSurfs.begin(); it != lSurfs.end(); ++it)
//        {
//            if(0 == (*it).Data.Locked)
//            {
//                s_out = &(*it);
//                m_pmfxCore->IncreaseReference(m_pmfxCore->pthis,&s_out->Data);
//                return s_out;
//            }
//        }
//    }
//    return 0;
//}
//
//void CmDirtyRectFilter::ReleaseSurf(mfxFrameSurface1& surf)
//{
//    m_pmfxCore->DecreaseReference(m_pmfxCore->pthis,&surf.Data);
//}

} //namespace MfxCapture
