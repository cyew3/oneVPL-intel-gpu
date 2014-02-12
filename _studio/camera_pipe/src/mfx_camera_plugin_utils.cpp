/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: mfx_camera_plugin_utils.cpp

\* ****************************************************************************** */

#define MFX_VA

#define VPP_IN       (0)
#define VPP_OUT      (1)

#include "mfx_camera_plugin_utils.h"
#include "mfx_camera_plugin.h"


namespace MfxCameraPlugin
{
// from CanonLog10ToRec709_10_LUT_Ver.1.1, picked up 64 points
unsigned int gamma_point[64] =
{
    0,  94, 104, 114, 124, 134, 144, 154, 159, 164, 169, 174, 179, 184, 194, 199,
    204, 209, 214, 219, 224, 230, 236, 246, 256, 266, 276, 286, 296, 306, 316, 326,
    336, 346, 356, 366, 376, 386, 396, 406, 416, 426, 436, 446, 456, 466, 476, 486,
    496, 516, 526, 536, 546, 556, 566, 576, 586, 596, 606, 616, 626, 636, 646, 1023
};

unsigned int gamma_correct[64] =
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

MfxFrameAllocResponse::~MfxFrameAllocResponse()
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
    }
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


mfxStatus MFXCamera_Plugin::AllocateInternalSurfaces()
{
    mfxFrameAllocRequest request = { { 0 } };
    mfxU32 frNum = 0;
    mfxStatus sts;

    request.Info = m_mfxVideoParam.vpp.In;
    request.Info.Width = m_PaddedFrameWidth;
    request.Info.Height = m_PaddedFrameHeight;
    request.Type = MFX_MEMTYPE_D3D_INT;

    if (m_Caps.bWhiteBalance)
        frNum++;

    if (m_Caps.bDemosaic)
        frNum += 9;

    request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum); // + m_mfxVideoParam.AsyncDepth); // AsyncDepth ???
    //request.Info.FourCC = MFX_FOURCC_P010; // ??? doublecheck fourCC !!!
    request.Info.FourCC = CM_SURFACE_FORMAT_A8;
    request.Info.Width *= sizeof(mfxU16);

    sts = m_raw16padded.AllocCmSurfaces(m_cmDevice, request);
    MFX_CHECK_STS(sts);

    request.Info.Width = m_FrameWidth64;
    request.Info.Height = m_mfxVideoParam.vpp.In.CropH;
    request.Type = MFX_MEMTYPE_D3D_INT;
    // fourCC ??? !!!
    request.Info.FourCC = CM_SURFACE_FORMAT_A8;
    request.Info.Width *= sizeof(mfxU16);
    frNum = 3;
    request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum); // + m_mfxVideoParam.AsyncDepth);

    sts = m_raw16aligned.AllocCmSurfaces(m_cmDevice, request);
    MFX_CHECK_STS(sts);

    request.Info.Width = m_PaddedFrameWidth;
    request.Info.Height = m_PaddedFrameHeight;
    request.Type = MFX_MEMTYPE_D3D_INT;
    request.Info.FourCC = CM_SURFACE_FORMAT_A8;
    frNum = 3; // ???
    request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum); // + m_mfxVideoParam.AsyncDepth);

    sts = m_aux8.AllocCmSurfaces(m_cmDevice, request);
    MFX_CHECK_STS(sts);

    m_gammaCorrectSurf = CreateSurface(m_cmDevice, 32, 4, CM_SURFACE_FORMAT_A8);
    m_gammaPointSurf = CreateSurface(m_cmDevice, 32, 4, CM_SURFACE_FORMAT_A8);

    m_gammaCorrectSurf->WriteSurface((unsigned char *)m_GammaParams.gamma_hw_params.Correct, NULL);
    m_gammaPointSurf->WriteSurface((unsigned char *)m_GammaParams.gamma_hw_params.Points, NULL);


    if (m_Caps.InputMemoryOperationMode == MEM_FASTGPUCPY) {
        m_cmSurfIn = CreateSurface(m_cmDevice, m_mfxVideoParam.vpp.In.Width*sizeof(mfxU16),  m_mfxVideoParam.vpp.In.Height, CM_SURFACE_FORMAT_A8);
    }


    return sts;
}

mfxStatus MFXCamera_Plugin::SetExternalSurfaces(mfxFrameSurface1 *surfIn, mfxFrameSurface1 *surfOut)
{
    if (surfIn->Data.MemId)
    {
        m_core->LockExternalFrame(surfIn->Data.MemId,  &surfIn->Data);
    }

    if (surfOut->Data.MemId)
    {
        m_core->LockExternalFrame(surfOut->Data.MemId,  &surfOut->Data);
    }

    mfxU32 inPitch = ((mfxU32)surfIn->Data.PitchHigh << 16) | ((mfxU32)surfIn->Data.PitchLow);
    mfxU32 inWidth = (mfxU32)surfIn->Info.Width;
    mfxU32 inHeight = (mfxU32)surfIn->Info.Height;

    // add external allocation support !!! ???

    // m_cmSurf[NUM_BUFFERS] ???

    if (m_Caps.InputMemoryOperationMode == MEM_GPUSHARED) {
        if (inPitch == sizeof(mfxU16)*inWidth && !((mfxU64)surfIn->Data.Y16 & 0xFFF)) {
            //m_cmSurfIn = (void *)CreateSurface(m_cmDevice, inWidth*sizeof(mfxU16),  inHeight, CM_SURFACE_FORMAT_A8, (void *)surfIn->Data.Y16); // 16bit fromats not supported; make this more flexible ???
            m_cmSurfUPIn = CreateSurface(m_cmDevice, inWidth*sizeof(mfxU16),  inHeight, CM_SURFACE_FORMAT_A8, (void *)surfIn->Data.Y16); // 16bit fromats not supported; make this more flexible ???
        } else {
            if (!m_memIn)
                m_memIn = CM_ALIGNED_MALLOC(inWidth*inHeight*sizeof(mfxU16), 0x1000);
            // to add !!! ???
            // Copy from surf to m_memIn
            // m_cmSurfIn = (void *)CreateSurfaceUP(m_cmDevice, inWidth,  inHeight, surfIn->Info.FourCC, (void *)surfIn->Data.Y16);
        }
    }
    else if (m_Caps.InputMemoryOperationMode == MEM_FASTGPUCPY) {
        //m_cmSurfIn = CreateSurface(m_cmDevice, inWidth*sizeof(mfxU16),  inHeight, CM_SURFACE_FORMAT_A8);
        if (inPitch == sizeof(mfxU16)*inWidth && !(inWidth*sizeof(mfxU16) & 0xF) && !((mfxU64)surfIn->Data.Y16 & 0xF)) {
            m_cmCtx->CopyMemToCmSurf(m_cmSurfIn, surfIn->Data.Y16);
        }
        // else  to implement!!!
    }
    // else // (m_Caps.InputMemoryOperationMode == MEM_CPUCPY) to implement !!!


    mfxU32 outPitch = ((mfxU32)surfOut->Data.PitchHigh << 16) | ((mfxU32)surfOut->Data.PitchLow);
    mfxU32 outWidth = (mfxU32)surfOut->Info.Width; // should be multiple of 32 (64) bytes !!! (???)
    mfxU32 outHeight = (mfxU32)surfOut->Info.Height;
    if (m_Caps.OutputMemoryOperationMode == MEM_GPUSHARED) {
        if (outPitch == 4*outWidth && !((mfxU64)surfOut->Data.B /* ??? */ & 0xFFF)) {
            m_memOut = (void *)surfOut->Data.B;
        } else {
            if (!m_memOutAllocPtr)
                m_memOutAllocPtr = CM_ALIGNED_MALLOC(outWidth*outHeight*4, 0x1000);
            m_memOut = m_memOutAllocPtr;
        }
        m_cmBufferUPOut = CreateBuffer(m_cmDevice, outWidth*outHeight*4, m_memOut);
    }
    // else if (m_Caps.InputMemoryOperationMode == MEM_FASTGPUCPY) {} // implement !!!
    // else {}

    if (surfIn->Data.MemId)
    {
        m_core->UnlockExternalFrame(surfIn->Data.MemId,  &surfIn->Data);
    }

    if (surfOut->Data.MemId)
    {
        m_core->UnlockExternalFrame(surfOut->Data.MemId,  &surfOut->Data);
    }


    return MFX_ERR_NONE;
}

mfxStatus MFXCamera_Plugin::SetTasks() //mfxFrameSurface1 *surfIn, mfxFrameSurface1 *surfOut)
{
    m_cmCtx->CreateThreadSpace(m_PaddedFrameWidth, m_PaddedFrameHeight); // move to SetUp??? surfIn/Out as args???

    CmSurface2D *pOutSurf, *pInSurf;
    //mfxU32 taskBufId = 0; // ???

//    if (m_Caps.InputMemoryOperationMode == MEM_FASTGPUCPY) {
//        m_cmCtx->CopyMfxSurfToCmSurf(m_cmSurfIn, surfIn);
//    }


    SurfaceIndex *pInputSurfaceIndex;
    if (m_cmSurfUPIn)
        m_cmSurfUPIn->GetIndex(pInputSurfaceIndex);
    else if (m_cmSurfIn)
        m_cmSurfIn->GetIndex(pInputSurfaceIndex);
    else
        return MFX_ERR_NULL_PTR;

    if (m_Caps.bWhiteBalance && m_WBparams.bActive) {
        pOutSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
        m_cmCtx->CreateTask_ManualWhiteBalance(*pInputSurfaceIndex, pOutSurf, m_WBparams.RedCorrection, m_WBparams.GreenTopCorrection, m_WBparams.BlueCorrection, m_WBparams.GreenBottomCorrection, m_InputBitDepth);
        pOutSurf->GetIndex(pInputSurfaceIndex);
    }

    CmSurface2D *goodPixCntSurf = (CmSurface2D *)AcquireResource(m_aux8);
    CmSurface2D *bigPixCntSurf = (CmSurface2D *)AcquireResource(m_aux8);

    m_cmCtx->CreateTask_GoodPixelCheck(*pInputSurfaceIndex, goodPixCntSurf, bigPixCntSurf, m_InputBitDepth);


    CmSurface2D *greenHorSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
    CmSurface2D *greenVerSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
    CmSurface2D *greenAvgSurf = (CmSurface2D *)AcquireResource(m_raw16padded);
    CmSurface2D *avgFlagSurf = (CmSurface2D *)AcquireResource(m_aux8);


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

    SurfaceIndex *pOutputSurfaceIndex;
    if (m_cmBufferUPOut)
        m_cmBufferUPOut->GetIndex(pOutputSurfaceIndex);
    else if (m_cmBufferOut)
        m_cmBufferOut->GetIndex(pOutputSurfaceIndex);
    else
        return MFX_ERR_NULL_PTR;

    // if (m_Caps.bForwardGammaCorrection && m_GammaParams.bActive)   currently won't work otherwise ???
    {

        if (m_Caps.OutputMemoryOperationMode == MEM_GPUSHARED) // add other mem types support !!! ???
        {
            m_cmCtx->CreateTask_ForwardGamma(m_gammaCorrectSurf, m_gammaPointSurf, redOutSurf, greenOutSurf, blueOutSurf, *pOutputSurfaceIndex, m_InputBitDepth);
        }
    }

    return MFX_ERR_NONE;
}


mfxStatus MFXCamera_Plugin::EnqueueTasks()
{
    int result;
    CmEvent *e;

    e = m_cmCtx->EnqueueTasks();
    e->WaitForTaskFinished();

    m_raw16aligned.Unlock();
    m_raw16padded.Unlock();
    m_aux8.Unlock();

    return MFX_ERR_NONE;

}