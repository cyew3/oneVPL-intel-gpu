/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: mfx_camera_plugin_utils.cpp

\* ****************************************************************************** */

#define MFX_VA

#include "mfx_camera_plugin_utils.h"
#include "mfx_camera_plugin.h"

#include "ippi.h"

#ifdef CAMP_PIPE_ITT
#include "ittnotify.h"


__itt_domain* CamPipeAccel = __itt_domain_create(L"CamPipeAccel");

//__itt_string_handle* CPU_file_fread;
//__itt_string_handle* CPU_raw_unpack_;
__itt_string_handle* task2 = __itt_string_handle_create(L"GPU_DM_Gamma");;
__itt_string_handle* task3 = __itt_string_handle_create(L"ColorConv");;
__itt_string_handle* taskc = __itt_string_handle_create(L"CPUGPUCopy");

__itt_string_handle* task21 = __itt_string_handle_create(L"21");
__itt_string_handle* task22 = __itt_string_handle_create(L"22");
__itt_string_handle* task23 = __itt_string_handle_create(L"23");

#endif


namespace MfxCameraPlugin
{
// from CanonLog10ToRec709_10_LUT_Ver.1.1, picked up 64 points

mfxU16 default_gamma_point[MFX_CAM_DEFAULT_NUM_GAMMA_POINTS] =
{
    0,  94, 104, 114, 124, 134, 144, 154, 159, 164, 169, 174, 179, 184, 194, 199,
    204, 209, 214, 219, 224, 230, 236, 246, 256, 266, 276, 286, 296, 306, 316, 326,
    336, 346, 356, 366, 376, 386, 396, 406, 416, 426, 436, 446, 456, 466, 476, 486,
    496, 516, 526, 536, 546, 556, 566, 576, 586, 596, 606, 616, 626, 636, 646, 1023
};

mfxU16 default_gamma_correct[MFX_CAM_DEFAULT_NUM_GAMMA_POINTS] =
{
    4,   4,  20,  37,  56,  75,  96, 117, 128, 140, 150, 161, 171, 180, 198, 207,
    216, 224, 232, 240, 249, 258, 268, 283, 298, 310, 329, 344, 359, 374, 389, 404,
    420, 435, 451, 466, 482, 498, 515, 531, 548, 565, 582, 599, 617, 635, 653, 671,
    690, 729, 749, 769, 790, 811, 832, 854, 876, 899, 922, 945, 969, 994, 1019,1019
};

}

using namespace MfxCameraPlugin;

MfxFrameAllocResponse::MfxFrameAllocResponse()
    : m_cmDestroy(0)
    , m_core(0)
    , m_cmDevice(0)
    , m_numFrameActualReturnedByAllocFrames(0)
{
    Zero((mfxFrameAllocResponse &)*this);
}

void MfxFrameAllocResponse::Free()
{
    if (m_core)
    {
        if (MFX_HW_D3D11  == m_core->GetVAType())
        {
            for (size_t i = 0; i < m_responseQueue.size(); i++)
                m_core->FreeFrames(&m_responseQueue[i]);
        }
        else
        {
            if (mids)
            {
                NumFrameActual = m_numFrameActualReturnedByAllocFrames;
                m_core->FreeFrames(this);
            }
        }
    }

    if (m_cmDevice)
    {
        for (size_t i = 0; i < m_mids.size(); i++)
            if (m_mids[i])
                m_cmDestroy(m_cmDevice, m_mids[i]);

        for (size_t i = 0; i < m_sysmems.size(); i++)
        {
            if (m_sysmems[i])
            {
                CM_ALIGNED_FREE(m_sysmems[i]);
                m_sysmems[i] = 0;
            }
        }
        m_cmDevice = 0;
    }
}


MfxFrameAllocResponse::~MfxFrameAllocResponse()
{
    Free();
}


void MfxFrameAllocResponse::DestroyBuffer(CmDevice * device, void * p)
{
    device->DestroySurface((CmBuffer *&)p);
}

void MfxFrameAllocResponse::DestroySurface(CmDevice * device, void * p)
{
    device->DestroySurface((CmSurface2D *&)p);
}

void MfxFrameAllocResponse::DestroyBufferUp(CmDevice * device, void * p)
{
    device->DestroyBufferUP((CmBufferUP *&)p);
}


mfxStatus MfxFrameAllocResponse::AllocCmBuffers(
    CmDevice *             device,
    mfxFrameAllocRequest & req)
{
    if (m_core || m_cmDevice)
        return MFX_ERR_MEMORY_ALLOC;

    req.NumFrameSuggested = req.NumFrameMin;
    mfxU32 size = req.Info.Width * req.Info.Height;

    m_mids.resize(req.NumFrameMin, 0);
    m_locked.resize(req.NumFrameMin, 0);
    for (int i = 0; i < req.NumFrameMin; i++)
        m_mids[i] = CreateBuffer(device, size);

    NumFrameActual = req.NumFrameMin;
    mids = &m_mids[0];

    m_core     = 0;
    m_cmDevice = device;
    m_cmDestroy = &DestroyBuffer;
    return MFX_ERR_NONE;
}



mfxStatus MfxFrameAllocResponse::AllocCmBuffersUp(
    CmDevice *             device,
    mfxFrameAllocRequest & req)
{
    if (m_core || m_cmDevice)
        return MFX_ERR_MEMORY_ALLOC;

    req.NumFrameSuggested = req.NumFrameMin;
    mfxU32 size = req.Info.Width * req.Info.Height;

    m_mids.resize(req.NumFrameMin, 0);
    m_locked.resize(req.NumFrameMin, 0);
    m_sysmems.resize(req.NumFrameMin, 0);   

    for (int i = 0; i < req.NumFrameMin; i++)
    {
        m_sysmems[i] = CM_ALIGNED_MALLOC(size, 0x1000);
        m_mids[i] = CreateBuffer(device, size, m_sysmems[i]);
    }

    NumFrameActual = req.NumFrameMin;
    mids = &m_mids[0];

    m_core     = 0;
    m_cmDevice = device;
    m_cmDestroy = &DestroyBufferUp;
    return MFX_ERR_NONE;
}



mfxStatus MfxFrameAllocResponse::AllocCmSurfaces(
    CmDevice *             device,
    mfxFrameAllocRequest & req)
{
    if (m_core || m_cmDevice)
        return MFX_ERR_MEMORY_ALLOC;

    req.NumFrameSuggested = req.NumFrameMin;

    m_mids.resize(req.NumFrameMin, 0);
    m_locked.resize(req.NumFrameMin, 0);
    for (int i = 0; i < req.NumFrameMin; i++)
        m_mids[i] = CreateSurface(device, req.Info.Width, req.Info.Height, req.Info.FourCC);

    NumFrameActual = req.NumFrameMin;
    mids = &m_mids[0];

    m_core     = 0;
    m_cmDevice = device;
    m_cmDestroy = &DestroySurface;
    return MFX_ERR_NONE;
}


mfxU32 MfxFrameAllocResponse::Lock(mfxU32 idx)
{
    if (idx >= m_locked.size())
        return 0;
    assert(m_locked[idx] < 0xffffffff);
    return ++m_locked[idx];
}

void MfxFrameAllocResponse::Unlock()
{
    std::fill(m_locked.begin(), m_locked.end(), 0);
}

mfxU32 MfxFrameAllocResponse::Unlock(mfxU32 idx)
{
    if (idx >= m_locked.size())
        return mfxU32(-1);
    assert(m_locked[idx] > 0);
    return --m_locked[idx];
}

mfxU32 MfxFrameAllocResponse::Locked(mfxU32 idx) const
{
    return (idx < m_locked.size()) ? m_locked[idx] : 1;
}

void * MfxFrameAllocResponse::GetSysmemBuffer(mfxU32 idx)
{
    return (idx < m_sysmems.size()) ? m_sysmems[idx] : 0;
}

mfxU32 MfxCameraPlugin::FindFreeResourceIndex(
    MfxFrameAllocResponse const & pool,
    mfxU32                        startingFrom)
{
    for (mfxU32 i = startingFrom; i < pool.NumFrameActual; i++)
        if (pool.Locked(i) == 0)
            return i;
    return NO_INDEX;
}

mfxMemId MfxCameraPlugin::AcquireResource(
    MfxFrameAllocResponse & pool,
    mfxU32                  index)
{
    if (index > pool.NumFrameActual)
        return 0; /// MID_INVALID; ???
    pool.Lock(index);
    return pool.mids[index];
}

mfxMemId MfxCameraPlugin::AcquireResource(
    MfxFrameAllocResponse & pool)
{
    return AcquireResource(pool, FindFreeResourceIndex(pool));
}

mfxHDLPair MfxCameraPlugin::AcquireResourceUp(
    MfxFrameAllocResponse & pool,
    mfxU32                  index)
{
    mfxHDLPair p = { 0, 0 };
    if (index > pool.NumFrameActual)
        return p;
    pool.Lock(index);
    p.first  = pool.mids[index];
    p.second = pool.GetSysmemBuffer(index);
    return p;
}

mfxHDLPair MfxCameraPlugin::AcquireResourceUp(
    MfxFrameAllocResponse & pool)
{
    return AcquireResourceUp(pool, FindFreeResourceIndex(pool));
}

void MfxCameraPlugin::ReleaseResource(
    MfxFrameAllocResponse & pool,
    mfxMemId                mid)
{
    for (mfxU32 i = 0; i < pool.NumFrameActual; i++)
    {
        if (pool.mids[i] == mid)
        {
            pool.Unlock(i);
            break;
        }
    }
}


mfxStatus MFXCamera_Plugin::ReallocateInternalSurfaces(mfxVideoParam &newParam, CameraFrameSizeExtra &frameSizeExtra)
{
    //m_Caps should be updated earlier

    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 frNum = 0;
    if (m_Caps.bWhiteBalance)
        frNum++;
    if (m_Caps.bDemosaic)
        frNum += 9;

    mfxU32 inputBitDepth = newParam.vpp.In.BitDepthLuma;

    mfxU16 vSliceWidth = frameSizeExtra.vSliceWidth;
    mfxU16 frameWidth64 = frameSizeExtra.frameWidth64;
    mfxU16 paddedFrameHeight = frameSizeExtra.paddedFrameHeight;
    mfxU16 paddedFrameWidth = frameSizeExtra.paddedFrameWidth;


    mfxFrameAllocRequest request = { { 0 } };

    if (vSliceWidth != m_FrameSizeExtra.vSliceWidth || paddedFrameHeight != m_FrameSizeExtra.paddedFrameHeight || frNum > m_raw16padded.NumFrameActual) {
        m_raw16padded.Free();
        request.Info = newParam.vpp.In;
        request.Info.Width = (mfxU16)vSliceWidth;
        request.Info.Height = (mfxU16)paddedFrameHeight;

        request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum);
        request.Info.FourCC = CM_SURFACE_FORMAT_A8;
        request.Info.Width *= sizeof(mfxU16);

        sts = m_raw16padded.AllocCmSurfaces(m_cmDevice, request);
        MFX_CHECK_STS(sts);
    }


    if (frameWidth64 != m_FrameSizeExtra.frameWidth64 || newParam.vpp.In.CropH != m_mfxVideoParam.vpp.In.CropH) {
        m_raw16aligned.Free();

        //request.Info = newParam.vpp.In;
        request.Info.Width = (mfxU16)frameWidth64;
        request.Info.Height = newParam.vpp.In.CropH;
        request.Info.FourCC = CM_SURFACE_FORMAT_A8;
        request.Info.Width *= sizeof(mfxU16);
        frNum = 3;
        request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum);

        sts = m_raw16aligned.AllocCmSurfaces(m_cmDevice, request);
        MFX_CHECK_STS(sts);
    }

    if (paddedFrameWidth != m_FrameSizeExtra.paddedFrameWidth || paddedFrameHeight != m_FrameSizeExtra.paddedFrameHeight) {
        m_aux8.Free();

       //request.Info = newParam.vpp.In;
        request.Info.Width = (mfxU16)paddedFrameWidth;
        request.Info.Height = (mfxU16)paddedFrameHeight;
        request.Info.FourCC = CM_SURFACE_FORMAT_A8;
#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE
        frNum = 2;
#else
        frNum = 3;
#endif
        request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum);

        sts = m_aux8.AllocCmSurfaces(m_cmDevice, request);
        MFX_CHECK_STS(sts);
    }


#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE
    if (vSliceWidth != m_FrameSizeExtra.vSliceWidth || paddedFrameHeight != m_FrameSizeExtra.paddedFrameHeight) {
        m_cmDevice->DestroySurface(m_avgFlagSurf);
        m_avgFlagSurf = CreateSurface(m_cmDevice, m_FrameSizeExtra.vSliceWidth, m_FrameSizeExtra.paddedFrameHeight, CM_SURFACE_FORMAT_A8);
    }
#endif
    return sts;
}


mfxStatus MFXCamera_Plugin::AllocateInternalSurfaces()
{
    mfxFrameAllocRequest request = { { 0 } };
    mfxU32 frNum = 0;
    mfxStatus sts;

    request.Info = m_mfxVideoParam.vpp.In;
    request.Info.Width = (mfxU16)m_FrameSizeExtra.vSliceWidth;
    request.Info.Height = (mfxU16)m_FrameSizeExtra.paddedFrameHeight;
    //request.Type = MFX_MEMTYPE_D3D_INT; // not used  (???)

    if (m_Caps.bWhiteBalance)
        frNum++;

    if (m_Caps.bDemosaic)
        frNum += 9;

    request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum);
    request.Info.FourCC = CM_SURFACE_FORMAT_A8;
    request.Info.Width *= sizeof(mfxU16);

    sts = m_raw16padded.AllocCmSurfaces(m_cmDevice, request);
    MFX_CHECK_STS(sts);

    request.Info.Width = (mfxU16)m_FrameSizeExtra.frameWidth64;
    request.Info.Height = m_mfxVideoParam.vpp.In.CropH;
    //request.Type = MFX_MEMTYPE_D3D_INT;
    request.Info.FourCC = CM_SURFACE_FORMAT_A8;
    request.Info.Width *= sizeof(mfxU16);
    frNum = 3;
    request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum);

    sts = m_raw16aligned.AllocCmSurfaces(m_cmDevice, request);
    MFX_CHECK_STS(sts);

    request.Info.Width = (mfxU16)m_FrameSizeExtra.paddedFrameWidth;
    request.Info.Height = (mfxU16)m_FrameSizeExtra.paddedFrameHeight;
    //request.Type = MFX_MEMTYPE_D3D_INT;
    request.Info.FourCC = CM_SURFACE_FORMAT_A8;
#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE
    frNum = 2;
#else
    frNum = 3;
#endif
    request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum);

    sts = m_aux8.AllocCmSurfaces(m_cmDevice, request);
    MFX_CHECK_STS(sts);

#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE
    m_avgFlagSurf = CreateSurface(m_cmDevice, m_FrameSizeExtra.vSliceWidth, m_FrameSizeExtra.paddedFrameHeight, CM_SURFACE_FORMAT_A8);
#endif

    m_gammaCorrectSurf = CreateSurface(m_cmDevice, 32, 4, CM_SURFACE_FORMAT_A8);
    m_gammaPointSurf = CreateSurface(m_cmDevice, 32, 4, CM_SURFACE_FORMAT_A8);

    mfxU8 *pGammaPts = (mfxU8 *)m_GammaParams.gamma_lut.gammaPoints;
    mfxU8 *pGammaCor = (mfxU8 *)m_GammaParams.gamma_lut.gammaCorrect;

    if (!pGammaPts || !pGammaCor) {
        // implement !!! kta
    }

    m_gammaCorrectSurf->WriteSurface(pGammaCor, NULL);
    m_gammaPointSurf->WriteSurface((unsigned char *)pGammaPts, NULL);

    if (m_Caps.InputMemoryOperationMode == MEM_FASTGPUCPY) {
        request.Info = m_mfxVideoParam.vpp.In;
        request.Info.Width = m_mfxVideoParam.vpp.In.Width*sizeof(mfxU16);
        request.Info.Height = m_mfxVideoParam.vpp.In.Height;
        //request.Type = MFX_MEMTYPE_D3D_INT;
        request.Info.FourCC = CM_SURFACE_FORMAT_A8;
        request.NumFrameMin = request.NumFrameSuggested = 1 + m_mfxVideoParam.AsyncDepth;
        sts = m_rawIn.AllocCmSurfaces(m_cmDevice, request);
        MFX_CHECK_STS(sts);
//        m_cmSurfIn = CreateSurface(m_cmDevice, m_mfxVideoParam.vpp.In.Width*sizeof(mfxU16),  m_mfxVideoParam.vpp.In.Height, CM_SURFACE_FORMAT_A8);
    } // add MEM_GPUSHARED support !!!


    // create buffer pool for (m_Caps.OutputMemoryOperationMode == MEM_FASTGPUCPY) !!!
    
    return sts;
}

//mfxStatus MFXCamera_Plugin::SetExternalSurfaces(mfxFrameSurface1 *surfIn, mfxFrameSurface1 *surfOut)
mfxStatus MFXCamera_Plugin::SetExternalSurfaces(AsyncParams *pParam)
{

    mfxFrameSurface1 *surfIn = pParam->surf_in;
    mfxFrameSurface1 *surfOut = pParam->surf_out;

    if (surfIn->Data.MemId)
    {
        m_core->LockExternalFrame(surfIn->Data.MemId,  &surfIn->Data);
    }

    if (m_Caps.OutputMemoryOperationMode == MEM_GPUSHARED)
        if (surfOut->Data.MemId)
            m_core->LockExternalFrame(surfOut->Data.MemId,  &surfOut->Data);

    mfxU32 inPitch = ((mfxU32)surfIn->Data.PitchHigh << 16) | ((mfxU32)surfIn->Data.PitchLow);
//    mfxU32 inWidth = (mfxU32)surfIn->Info.Width;
//    mfxU32 inHeight = (mfxU32)surfIn->Info.Height;

    mfxU32 inWidth = (mfxU32)surfIn->Info.CropW + 64;
    mfxU32 inHeight = (mfxU32)surfIn->Info.CropH + 16;
    surfIn->Data.Y16 += inPitch * surfIn->Info.CropY + surfIn->Info.CropX;

    if (m_Caps.InputMemoryOperationMode == MEM_GPUSHARED) {
        if (inPitch == sizeof(mfxU16)*inWidth && !((mfxU64)surfIn->Data.Y16 & 0xFFF)) {
            pParam->inSurf2DUP = (mfxMemId)CreateSurface(m_cmDevice, inWidth*sizeof(mfxU16),  inHeight, CM_SURFACE_FORMAT_A8, (void *)surfIn->Data.Y16);
            pParam->pMemIn = 0;
        } else {
            pParam->pMemIn = CM_ALIGNED_MALLOC(inWidth*inHeight*sizeof(mfxU16), 0x1000); // todo : implement a pool !!! ???

            IppiSize roi = {inWidth, inHeight};
            ippiCopy_16u_C1R((Ipp16u*)surfIn->Data.Y16, inPitch, (Ipp16u*)pParam->pMemIn, inWidth*sizeof(Ipp16u), roi);


            // todo: Copy from surfIn->Data.Y16 to pMemIn !!!
            pParam->inSurf2DUP = (mfxMemId)CreateSurface(m_cmDevice, inWidth*sizeof(mfxU16),  inHeight, CM_SURFACE_FORMAT_A8, (void *)pParam->pMemIn);
        }
        pParam->inSurf2D = 0;
    }
    else if (m_Caps.InputMemoryOperationMode == MEM_FASTGPUCPY) {
        //m_cmSurfIn = CreateSurface(m_cmDevice, inWidth*sizeof(mfxU16),  inHeight, CM_SURFACE_FORMAT_A8);

        CmSurface2D *in2DSurf = (CmSurface2D *)AcquireResource(m_rawIn);
        pParam->inSurf2D = (mfxMemId)in2DSurf;
        pParam->inSurf2DUP = 0;

#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipeAccel, __itt_null, __itt_null, taskc);
#endif

        if (inPitch == sizeof(mfxU16)*inWidth && !(inWidth*sizeof(mfxU16) & 0xF) && !((mfxU64)surfIn->Data.Y16 & 0xF)) {
            m_cmCtx->CopyMemToCmSurf(in2DSurf, surfIn->Data.Y16);
        }

#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipeAccel);
#endif


        // else  to implement!!!
    }
    // else // (m_Caps.InputMemoryOperationMode == MEM_CPUCPY) to implement !!!


    mfxU32 outPitch = ((mfxU32)surfOut->Data.PitchHigh << 16) | ((mfxU32)surfOut->Data.PitchLow);
    mfxU32 outWidth = (mfxU32)surfOut->Info.Width; // should be multiple of 32 (64) bytes !!! (???)
    mfxU32 outHeight = (mfxU32)surfOut->Info.Height;
    if (m_Caps.OutputMemoryOperationMode == MEM_GPUSHARED) {
        if (outPitch == 4*outWidth && !((mfxU64)surfOut->Data.B /* ??? */ & 0xFFF)) {
            pParam->pMemOut = 0;
            pParam->outBufUP = CreateBuffer(m_cmDevice, outWidth*outHeight*4, (void *)surfOut->Data.B);
        } else {
            pParam->pMemOut = CM_ALIGNED_MALLOC(outWidth*outHeight*4, 0x1000); // todo : implement a pool !!! ???
            pParam->outBufUP = CreateBuffer(m_cmDevice, outWidth*outHeight*4, pParam->pMemOut);
        }
        pParam->outBuf = 0;
    } else if (m_Caps.OutputMemoryOperationMode == MEM_FASTGPUCPY) {

        mfxHDLPair outHdl;
        //m_core->GetFrameHDL(surfOut->Data.MemId, &outHdl);
        m_core->GetExternalFrameHDL(surfOut->Data.MemId, (mfxHDL*)&outHdl);
        CmSurface2D *pSurf2D;
        m_cmDevice->CreateSurface2D(outHdl.first, pSurf2D);
        pParam->outSurf2D = (mfxMemId)pSurf2D;

        pParam->outBuf = 0;
        pParam->outBufUP = 0;
        //pParam->outBuf = AquireResource - add when pool implemented !!!
    } 
    // else {}

    if (surfIn->Data.MemId)
        m_core->UnlockExternalFrame(surfIn->Data.MemId,  &surfIn->Data);

    if (m_Caps.OutputMemoryOperationMode == MEM_GPUSHARED)
        if (surfOut->Data.MemId)
            m_core->UnlockExternalFrame(surfOut->Data.MemId,  &surfOut->Data);

    return MFX_ERR_NONE;
}


#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE

mfxStatus MFXCamera_Plugin::CreateEnqueueTasks(AsyncParams *pParam)
{
#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipeAccel, __itt_null, __itt_null, task2);
#endif    
    CmEvent *e = NULL;

    UMC::AutomaticUMCMutex guard(m_guard);

    SurfaceIndex *pInputSurfaceIndex;

    if (pParam->inSurf2D) {
        CmSurface2D *inSurf = (CmSurface2D *)pParam->inSurf2D;
        inSurf->GetIndex(pInputSurfaceIndex);
    } else if (pParam->inSurf2DUP) {
        CmSurface2DUP *inSurf = (CmSurface2DUP *)pParam->inSurf2DUP;
        inSurf->GetIndex(pInputSurfaceIndex);
    } else
        return MFX_ERR_NULL_PTR;

    CmSurface2D *goodPixCntSurf = (CmSurface2D *)AcquireResource(m_aux8);
    CmSurface2D *bigPixCntSurf = (CmSurface2D *)AcquireResource(m_aux8);

#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipeAccel, __itt_null, __itt_null, task21);
#endif    

    m_cmCtx->CreateEnqueueTask_GoodPixelCheck(*pInputSurfaceIndex, goodPixCntSurf, bigPixCntSurf, m_InputBitDepth);

    CmSurface2D *greenHorSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
    CmSurface2D *greenVerSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
    CmSurface2D *greenAvgSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
    CmSurface2D *avgFlagSurf = m_avgFlagSurf;

    m_cmCtx->CreateTask_RestoreGreen(*pInputSurfaceIndex, goodPixCntSurf, bigPixCntSurf, greenHorSurf, greenVerSurf, greenAvgSurf, avgFlagSurf, m_InputBitDepth);


    CmSurface2D *blueHorSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
    CmSurface2D *blueVerSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
    CmSurface2D *blueAvgSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
    CmSurface2D *redHorSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
    CmSurface2D *redVerSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
    CmSurface2D *redAvgSurf = (CmSurface2D *)AcquireResource(m_raw16padded);

    m_cmCtx->CreateTask_RestoreBlueRed(*pInputSurfaceIndex,
                                        greenHorSurf, greenVerSurf, greenAvgSurf,
                                        blueHorSurf, blueVerSurf, blueAvgSurf,
                                        redHorSurf, redVerSurf, redAvgSurf,
                                        avgFlagSurf, m_InputBitDepth);


    CmSurface2D *redOutSurf   = (CmSurface2D *)AcquireResource(m_raw16aligned);
    CmSurface2D *greenOutSurf = (CmSurface2D *)AcquireResource(m_raw16aligned);
    CmSurface2D *blueOutSurf  = (CmSurface2D *)AcquireResource(m_raw16aligned);

    m_cmCtx->CreateTask_SAD(redHorSurf, greenHorSurf, blueHorSurf, redVerSurf, greenVerSurf, blueVerSurf, redOutSurf, greenOutSurf, blueOutSurf);

    m_cmCtx->CreateTask_DecideAverage(redAvgSurf, greenAvgSurf, blueAvgSurf, avgFlagSurf, redOutSurf, greenOutSurf, blueOutSurf);

#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipeAccel);
#endif


#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipeAccel, __itt_null, __itt_null, task22);
#endif    

    for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++) {
        m_cmCtx->EnqueueSliceTasks(i);
    }
    //m_cmCtx->EnqueueTasks();


#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipeAccel);
#endif

    SurfaceIndex *pOutputSurfaceIndex;

    if (pParam->outBufUP) {
        CmBufferUP *outBuffer = (CmBufferUP *)pParam->outBufUP;
        outBuffer->GetIndex(pOutputSurfaceIndex);
    } else if (pParam->outBuf) {
        CmBuffer *outBuffer = (CmBuffer *)pParam->outBuf;
        outBuffer->GetIndex(pOutputSurfaceIndex);
    } else if (pParam->outSurf2D) {
        CmSurface2D *outBuffer = (CmSurface2D *)pParam->outSurf2D;
        outBuffer->GetIndex(pOutputSurfaceIndex);
    } else
        return MFX_ERR_NULL_PTR;


#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipeAccel, __itt_null, __itt_null, task23);
#endif    

    // if (m_Caps.bForwardGammaCorrection && m_GammaParams.bActive)   currently won't work otherwise ???
    {
        e = m_cmCtx->CreateEnqueueTask_ForwardGamma(m_gammaCorrectSurf, m_gammaPointSurf, redOutSurf, greenOutSurf, blueOutSurf, *pOutputSurfaceIndex, m_InputBitDepth);
    }

#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipeAccel);
#endif

    pParam->pEvent = e;

#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipeAccel);
#endif

    if (pParam->outBufUP) {
        CmBufferUP *buf = (CmBufferUP *)pParam->outBufUP;
        if (pParam->pMemOut) {
            // copy mem to out mfx surface - to implement !!!
            CM_ALIGNED_FREE(pParam->pMemOut);  // remove/change when pool implemented !!! ???
            pParam->pMemOut = 0;
        }
        m_cmDevice->DestroyBufferUP(buf);
        pParam->outBufUP = 0;
    } else if (pParam->outBuf) {
        // copy from CmBuffer to out mfx surface - to implement !!! 
    }

    return MFX_ERR_NONE;
}

#else

mfxStatus MFXCamera_Plugin::CreateEnqueueTasks(AsyncParams *pParam) //mfxFrameSurface1 *surfIn, mfxFrameSurface1 *surfOut)
{
#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipeAccel, __itt_null, __itt_null, task2);
#endif    
    int result;
    CmEvent *e = NULL;

    UMC::AutomaticUMCMutex guard(m_guard);

    SurfaceIndex *pInputSurfaceIndex;

    if (pParam->inSurf2D) {
        CmSurface2D *inSurf = (CmSurface2D *)pParam->inSurf2D;
        inSurf->GetIndex(pInputSurfaceIndex);
    } else if (pParam->inSurf2DUP) {
        CmSurface2DUP *inSurf = (CmSurface2DUP *)pParam->inSurf2DUP;
        inSurf->GetIndex(pInputSurfaceIndex);
    } else
        return MFX_ERR_NULL_PTR;

    //if (m_Caps.bWhiteBalance && m_WBparams.bActive) {
    //    pOutSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
    //    m_cmCtx->CreateTask_ManualWhiteBalance(*pInputSurfaceIndex, pOutSurf, m_WBparams.RedCorrection, m_WBparams.GreenTopCorrection, m_WBparams.BlueCorrection, m_WBparams.GreenBottomCorrection, m_InputBitDepth);
    //    pOutSurf->GetIndex(pInputSurfaceIndex);
    //}

    CmSurface2D *goodPixCntSurf = (CmSurface2D *)AcquireResource(m_aux8);
    CmSurface2D *bigPixCntSurf = (CmSurface2D *)AcquireResource(m_aux8);

    m_cmCtx->CreateEnqueueTask_GoodPixelCheck(*pInputSurfaceIndex, goodPixCntSurf, bigPixCntSurf, m_InputBitDepth);

    CmSurface2D *greenHorSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
    CmSurface2D *greenVerSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
    CmSurface2D *greenAvgSurf = (CmSurface2D *)AcquireResource(m_raw16padded);

#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE
    CmSurface2D *avgFlagSurf = m_avgFlagSurf;
#else
    CmSurface2D *avgFlagSurf = (CmSurface2D *)AcquireResource(m_aux8);
#endif

    m_cmCtx->CreateEnqueueTask_RestoreGreen(*pInputSurfaceIndex, goodPixCntSurf, bigPixCntSurf, greenHorSurf, greenVerSurf, greenAvgSurf, avgFlagSurf, m_InputBitDepth);

    CmSurface2D *blueHorSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
    CmSurface2D *blueVerSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
    CmSurface2D *blueAvgSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
    CmSurface2D *redHorSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
    CmSurface2D *redVerSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
    CmSurface2D *redAvgSurf = (CmSurface2D *)AcquireResource(m_raw16padded);

    m_cmCtx->CreateEnqueueTask_RestoreBlueRed(*pInputSurfaceIndex,
                                               greenHorSurf, greenVerSurf, greenAvgSurf,
                                               blueHorSurf, blueVerSurf, blueAvgSurf,
                                               redHorSurf, redVerSurf, redAvgSurf,
                                               avgFlagSurf, m_InputBitDepth);


    CmSurface2D *redOutSurf   = (CmSurface2D *)AcquireResource(m_raw16aligned);
    CmSurface2D *greenOutSurf = (CmSurface2D *)AcquireResource(m_raw16aligned);
    CmSurface2D *blueOutSurf  = (CmSurface2D *)AcquireResource(m_raw16aligned);

    m_cmCtx->CreateEnqueueTask_SAD(redHorSurf, greenHorSurf, blueHorSurf, redVerSurf, greenVerSurf, blueVerSurf, redOutSurf, greenOutSurf, blueOutSurf);

    m_cmCtx->CreateEnqueueTask_DecideAverage(redAvgSurf, greenAvgSurf, blueAvgSurf, avgFlagSurf, redOutSurf, greenOutSurf, blueOutSurf);

    SurfaceIndex *pOutputSurfaceIndex;

    if (pParam->outBufUP) {
        CmBufferUP *outBuffer = (CmBufferUP *)pParam->outBufUP;
        outBuffer->GetIndex(pOutputSurfaceIndex);
    } else if (pParam->outBuf) {
        CmBuffer *outBuffer = (CmBuffer *)pParam->outBuf;
        outBuffer->GetIndex(pOutputSurfaceIndex);
    } else
        return MFX_ERR_NULL_PTR;

    // if (m_Caps.bForwardGammaCorrection && m_GammaParams.bActive)   currently won't work otherwise ???
    {

        if (m_Caps.OutputMemoryOperationMode == MEM_GPUSHARED) // add other mem types support !!! ???
        {
            e = m_cmCtx->CreateEnqueueTask_ForwardGamma(m_gammaCorrectSurf, m_gammaPointSurf, redOutSurf, greenOutSurf, blueOutSurf, *pOutputSurfaceIndex, m_InputBitDepth);
        }
    }

  
    pParam->pEvent = e;

#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipeAccel);
#endif

    if (pParam->outBufUP) {
        CmBufferUP *buf = (CmBufferUP *)pParam->outBufUP;
        if (pParam->pMemOut) {
            // copy mem to out mfx surface - to implement !!!
            CM_ALIGNED_FREE(pParam->pMemOut);  // remove/change when pool implemented !!! ???
            pParam->pMemOut = 0;
        }
        m_cmDevice->DestroyBufferUP(buf);
        pParam->outBufUP = 0;
    } else if (pParam->outBuf) {
        // copy from CmBuffer to out mfx surface - to implement !!! 
    }

    return MFX_ERR_NONE;
}
#endif

void QueryCaps(mfxCameraCaps &caps)
{
    caps.ModuleConfiguration = 0; // zero all filters
    caps.bDemosaic = 1;
    caps.bForwardGammaCorrection = 1;
    caps.bOutToARGB8 = 1; // ???

    caps.InputMemoryOperationMode = MEM_GPUSHARED; //MEM_FASTGPUCPY; 
    caps.OutputMemoryOperationMode = MEM_GPUSHARED; //MEM_FASTGPUCPY;

    caps.bNoPadding = 0; // we don't currently support input w/out padding (change bNoPadding to bPadding ???)
}


const mfxU32 g_TABLE_CAMERA_ALGS [] =
{
    MFX_EXTBUF_CAM_GAMMA_CORRECTION,
    MFX_EXTBUF_CAM_WHITE_BALANCE,
    MFX_EXTBUF_CAM_HOT_PIXEL_REMOVAL,
    MFX_EXTBUF_CAM_BLACK_LEVEL_CORRECTION,
    MFX_EXTBUF_CAM_VIGNETTE_CORRECTION,
    MFX_EXTBUF_CAM_BAYER_DENOISE,
    MFX_EXTBUF_CAM_COLOR_CORRECTION_3X3,
    MFX_EXTBUF_CAM_PADDING
};


bool IsFilterFound(const mfxU32* pList, mfxU32 len, mfxU32 filterName)
{
    mfxU32 i;

    if (0 == len)
        return false;

    for (i = 0; i < len; i++)
        if (filterName == pList[i])
            return true;

    return false;

} // bool IsFilterFound( mfxU32* pList, mfxU32 len, mfxU32 filterName )


void GetDoUseFilterList(mfxVideoParam* par, mfxU32** ppList, mfxU32* pLen)
{
    mfxU32 i = 0;
    mfxExtVPPDoUse* pVPPHint = NULL;

    /* robustness */
    *ppList = NULL;
    *pLen = 0;

    for (i = 0; i < par->NumExtParam; i++)
    {
        if (MFX_EXTBUFF_VPP_DOUSE == par->ExtParam[i]->BufferId)
        {
            pVPPHint  = (mfxExtVPPDoUse*)(par->ExtParam[i]);
            *ppList = pVPPHint->AlgList;
            *pLen  = pVPPHint->NumAlg;
            break;
        }
    }
}

mfxStatus CorrectDoUseFilters(mfxU32* pList, mfxU32 len)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 i = 0;
    mfxU32 searchCount = sizeof(g_TABLE_CAMERA_ALGS) / sizeof(*g_TABLE_CAMERA_ALGS);
    for (i = 0; i < len; i++)
    {
        if (!IsFilterFound(g_TABLE_CAMERA_ALGS, searchCount, pList[i])) 
        {
            pList[i] = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }
    }
    return sts;
}


mfxStatus GetConfigurableFilterList(mfxVideoParam* par, mfxU32* pList, mfxU32* pLen)
{
    mfxU32 fIdx = 0;
    mfxStatus sts = MFX_ERR_NONE;

    /* robustness */
    *pLen = 0;

    mfxU32 fCount = par->NumExtParam;
    mfxU32 searchCount = sizeof(g_TABLE_CAMERA_ALGS) / sizeof(*g_TABLE_CAMERA_ALGS);

    for (fIdx = 0; fIdx < fCount; fIdx++ )
    {
        mfxU32 curId = par->ExtParam[fIdx]->BufferId;
        if (MFX_EXTBUFF_VPP_DOUSE == curId)
            continue;
        if (IsFilterFound(g_TABLE_CAMERA_ALGS, searchCount, curId)) {
            if (pList && !IsFilterFound(pList, *pLen, curId)) {
                pList[(*pLen)++] = curId;
            }
        } else
            sts = MFX_ERR_UNSUPPORTED;
    }
    return sts;
}


void ConvertCaps2ListDoUse(mfxCameraCaps& caps, std::vector<mfxU32>& list)
{
    if (caps.bForwardGammaCorrection)
    {
        list.push_back(MFX_EXTBUF_CAM_GAMMA_CORRECTION);
    }
    if (caps.bBlackLevelCorrection)
    {
        list.push_back(MFX_EXTBUF_CAM_BLACK_LEVEL_CORRECTION);
    }

    if (caps.bVignetteCorrection)
    {
        list.push_back(MFX_EXTBUF_CAM_VIGNETTE_CORRECTION);
    }

    if (caps.bWhiteBalance)
    {
        list.push_back(MFX_EXTBUF_CAM_WHITE_BALANCE);
    }

    if (caps.bHotPixel)
    {
        list.push_back(MFX_EXTBUF_CAM_HOT_PIXEL_REMOVAL);
    }

    if (caps.bColorConversionMatrix)
    {
        list.push_back(MFX_EXTBUF_CAM_COLOR_CORRECTION_3X3);
    }

    if(caps.bBayerDenoise)
    {
        list.push_back(MFX_EXTBUF_CAM_BAYER_DENOISE);
    }
}

#define MFX_CAM_DEFAULT_GAMMA_VALUE 2.2
#define MFX_CAM_GAMMA_VALUE_MIN 0.1 //??? 1.0?

#define MFX_CAM_GAMMA_DEPTH_MIN 8
#define MFX_CAM_GAMMA_DEPTH_MAX 16

#define MFX_CAM_GAMMA_NUM_POINTS_MIN 8 // ???

mfxStatus GammaCorrectionCheckParam(mfxExtCamGammaCorrection * pGammaBuf)
{
    mfxStatus sts = MFX_ERR_NONE;
    if (pGammaBuf->Mode != MFX_CAM_GAMMA_VALUE && pGammaBuf->Mode != MFX_CAM_GAMMA_LUT && pGammaBuf->Mode != MFX_CAM_GAMMA_DEFAULT) {
        pGammaBuf->Mode = MFX_CAM_GAMMA_DEFAULT;
        pGammaBuf->BitDepth = 0;
        pGammaBuf->GammaValue = (mfxF64)0;
        pGammaBuf->NumPoints = 0;
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM; // ??? CLIP and MFX_ERR_UNSUPPORTED ???
    } else if (pGammaBuf->Mode == MFX_CAM_GAMMA_VALUE) {
        if (pGammaBuf->GammaValue < MFX_CAM_GAMMA_VALUE_MIN) {
            pGammaBuf->GammaValue = MFX_CAM_GAMMA_VALUE_MIN;
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM; //MFX_WRN_INCOMPATIBLE_VIDEO_PARAM ???
        }
    } else if (pGammaBuf->Mode == MFX_CAM_GAMMA_LUT) {
        if (pGammaBuf->BitDepth < MFX_CAM_GAMMA_DEPTH_MIN || pGammaBuf->BitDepth > MFX_CAM_GAMMA_DEPTH_MAX) {
            CAMERA_CLIP(pGammaBuf->BitDepth, MFX_CAM_GAMMA_DEPTH_MIN, MFX_CAM_GAMMA_DEPTH_MAX);
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM; //MFX_WRN_INCOMPATIBLE_VIDEO_PARAM ??? 
        }
        mfxU32 maxNumPoints = sizeof(pGammaBuf->GammaPoint)/sizeof(mfxU16);
        if (pGammaBuf->NumPoints < MFX_CAM_GAMMA_NUM_POINTS_MIN || pGammaBuf->NumPoints > maxNumPoints) {
            CAMERA_CLIP(pGammaBuf->NumPoints, MFX_CAM_GAMMA_DEPTH_MIN, MFX_CAM_GAMMA_DEPTH_MAX);
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
    }
    return sts;
}

mfxStatus QueryExtBuf(mfxExtBuffer *extBuf, mfxU32 action)
{
    mfxStatus sts = MFX_ERR_NONE;
    switch (extBuf->BufferId)
    {
    case MFX_EXTBUF_CAM_GAMMA_CORRECTION:
        {
            mfxExtCamGammaCorrection *gammaBuf = (mfxExtCamGammaCorrection *)extBuf;
            if (action >= MFX_CAM_QUERY_CHECK_RANGE)
                sts = GammaCorrectionCheckParam(gammaBuf);
            else {
                gammaBuf->Mode = gammaBuf->BitDepth = gammaBuf->NumPoints = (mfxU16)action;
                gammaBuf->GammaValue = (mfxF64)action;
            }
        }
        break;
    case MFX_EXTBUF_CAM_HOT_PIXEL_REMOVAL:
        {
            mfxExtCamHotPixelRemoval *hotPelBuf = (mfxExtCamHotPixelRemoval *)extBuf;
            if (action >= MFX_CAM_QUERY_CHECK_RANGE) {
                //sts = HotPixelRemovalCheckParam(hotPelBuf);
            } else {
                hotPelBuf->PixelCountThreshold = hotPelBuf->PixelThresholdDifference = (mfxU16)action;
            }
        }
        break;
    case MFX_EXTBUF_CAM_VIGNETTE_CORRECTION:
        {
            mfxExtCamVignetteCorrection *vignetteBuf = (mfxExtCamVignetteCorrection *)extBuf;
            if (action >= MFX_CAM_QUERY_CHECK_RANGE) {
                //sts = VignetteCorrectionCheckParam(vignetteBuf);
            } else {
                vignetteBuf->Height = vignetteBuf->Width = vignetteBuf->Pitch = vignetteBuf->MaskPrecision = action;
            }
        }
        break;
    case MFX_EXTBUF_CAM_WHITE_BALANCE:
        {
            mfxExtCamWhiteBalance *wbBuf = (mfxExtCamWhiteBalance *)extBuf;
            if (action >= MFX_CAM_QUERY_CHECK_RANGE) {
                //sts = WhiteBalanceCheckParam(wbBuf);
            } else {
                wbBuf->R = wbBuf->G0 = wbBuf->B = wbBuf->G1 =  (mfxF64)action;
                wbBuf->Mode = action;
            }
        }
        break;
    case MFX_EXTBUF_CAM_BLACK_LEVEL_CORRECTION:
        {
            mfxExtCamBlackLevelCorrection *blackLevelBuf = (mfxExtCamBlackLevelCorrection *)extBuf;
            if (action >= MFX_CAM_QUERY_CHECK_RANGE) {
                //sts = BlackLevelCheckParam(blackLevelBuf);
            } else {
                blackLevelBuf->R = blackLevelBuf->G0 = blackLevelBuf->B = blackLevelBuf->G1 =  (mfxU16)action;
            }
        }
        break;
    case MFX_EXTBUF_CAM_COLOR_CORRECTION_3X3:
        {
            mfxExtCamColorCorrection3x3 *cc3x3Buf = (mfxExtCamColorCorrection3x3 *)extBuf;
            if (action >= MFX_CAM_QUERY_CHECK_RANGE) {
                //sts = ColorCorrection3x3CheckParam(cc3x3Buf);
            } else {
                mfxU32 i, j;
                for (i = 0; i < 3; i++)
                    for (j = 0; j < 3; j++)
                        cc3x3Buf->CCM[i][j] = (mfxF64)action;
            }
        }
        break;
    case MFX_EXTBUF_CAM_BAYER_DENOISE:
        {
            mfxExtCamBayerDenoise *denoiseBuf = (mfxExtCamBayerDenoise *)extBuf;
            if (action >= MFX_CAM_QUERY_CHECK_RANGE) {
                //sts = BayerDenoiseCheckParam(denoiseBuf);
            } else {
                denoiseBuf->Threshold = (mfxU16)action;
            }
        }
        break;
    case MFX_EXTBUF_CAM_PADDING:
        {
            mfxExtCamPadding *padBuf = (mfxExtCamPadding *)extBuf;
            if (action >= MFX_CAM_QUERY_CHECK_RANGE) {
                //sts = PaddingCheckParam(padBuf);
            } else {
                padBuf->Top = padBuf->Bottom = padBuf->Left = padBuf->Right = (mfxU16)action;
            }
        }
        break;

    default:
        sts = MFX_ERR_UNSUPPORTED;
        break;
    }
    return sts;
}


mfxStatus MfxCameraPlugin::CheckExtBuffers(mfxVideoParam *param, mfxVideoParam *out, mfxU32 mode)
{
    if (0 != param->NumExtParam && NULL == param->ExtParam)
        return MFX_ERR_NULL_PTR;

    mfxStatus sts = MFX_ERR_NONE;

    mfxU32*   pExtList = NULL;
    mfxU32    extCount = 0;
    mfxU32    i;
    mfxU32*   pOutList = NULL;
    mfxU32    outCount = 0;

    GetDoUseFilterList(param, &pExtList, &extCount);
    if (out) {
        GetDoUseFilterList(out, &pOutList, &outCount);
        if (extCount != outCount || (pExtList && !pOutList) || (!pExtList && pOutList) || (extCount && !pExtList)) // just in case - should be checked by now
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    mfxU32 searchCount = sizeof(g_TABLE_CAMERA_ALGS) / sizeof(*g_TABLE_CAMERA_ALGS);
    for (i = 0; i < extCount; i++) {
        if (!IsFilterFound(g_TABLE_CAMERA_ALGS, searchCount, pExtList[i])) {
            if (mode != MFX_CAM_QUERY_RETURN_STATUS) {
                if (!out) 
                    pExtList[i] = 0;
                else
                    pOutList[i] = 0;
            }
            sts = MFX_ERR_UNSUPPORTED;
        } else if (out) {
            pOutList[i] = pExtList[i];
        }
    }
    MFX_CHECK_STS(sts);

    std::vector<mfxU32> pipelineList(1);

    for (i = 0; i < param->NumExtParam; i++)
    {
        mfxU32 curId = param->ExtParam[i]->BufferId;
        if (MFX_EXTBUFF_VPP_DOUSE == curId)
            continue;
        if (!IsFilterFound(g_TABLE_CAMERA_ALGS, searchCount, curId)) {
            sts = MFX_ERR_UNSUPPORTED;
            break;
        }
    }
    MFX_CHECK_STS(sts);

    // no non-camera ext buffers; now to checking which of them are supported

    std::vector<mfxU32> capsList(1);
    mfxCameraCaps caps;
    QueryCaps(caps);
    ConvertCaps2ListDoUse(caps, capsList);

    for (i = 0; i < extCount; i++) {
        if (!IsFilterFound(&capsList[0], (mfxU32)capsList.size(), pExtList[i])) {
            if (mode != MFX_CAM_QUERY_RETURN_STATUS) {
                if (!out)
                    pExtList[i] = 0;
                else
                    pOutList[i] = 0;
            }
            sts = MFX_WRN_FILTER_SKIPPED;
        } else if (out) {
            pOutList[i] = pExtList[i];
        }
    }

    for (i = 0; i < param->NumExtParam; i++) {
        mfxU32 curId = param->ExtParam[i]->BufferId;
        if (MFX_EXTBUFF_VPP_DOUSE == curId)
            continue;
        if (!IsFilterFound(&capsList[0], (mfxU32)capsList.size(), curId)) 
        {
            if (mode != MFX_CAM_QUERY_RETURN_STATUS) {
                mfxExtBuffer *pBuf = out ? out->ExtParam[i] : param->ExtParam[i];
                QueryExtBuf(pBuf, MFX_CAM_QUERY_SET0);
                sts = MFX_WRN_FILTER_SKIPPED;
            } else {
                sts = MFX_WRN_FILTER_SKIPPED;
                break;
            }
        } 
        else if (IsFilterFound(&pipelineList[0], (mfxU32)pipelineList.size(), curId)) 
        {
            if (mode != MFX_CAM_QUERY_RETURN_STATUS) {
                mfxExtBuffer *pBuf = out ? out->ExtParam[i] : param->ExtParam[i];
                QueryExtBuf(pBuf, MFX_CAM_QUERY_SET0);
            }
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        } 
        else 
        {
            pipelineList.push_back(curId);
            if (mode != MFX_CAM_QUERY_RETURN_STATUS) {
                mfxU32 action = mode;
                if (action == MFX_CAM_QUERY_SIGNAL)
                    action = MFX_CAM_QUERY_SET1;

                mfxExtBuffer *pBuf = param->ExtParam[i];
                if (out) {
                    memcpy(out->ExtParam[i], param->ExtParam[i], param->ExtParam[i]->BufferSz); // can we rely on BufferSz ? or use GetConfigSize instead?
                    pBuf = out->ExtParam[i];
                }
                sts = QueryExtBuf(pBuf, action);
                // sts can be MFX_ERR_INCOMPATIBLE_VIDEO_PARAM if (action == MFX_CAM_QUERY_CHECK_RANGE)
                if (sts < MFX_ERR_NONE) // NB: by this time we may have some filters skipped, so another call with (mode != MFX_CAM_QUERY_CHECK_RANGE) is required
                    break;
            }
        }
    }
    return sts;
}
