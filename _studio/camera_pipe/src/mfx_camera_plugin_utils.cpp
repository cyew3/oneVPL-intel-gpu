/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: mfx_camera_plugin_utils.cpp

\* ****************************************************************************** */

#include "mfx_camera_plugin_utils.h"
#include "mfx_camera_plugin.h"
#include "vm_time.h"
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

    m_width = req.Info.Width;
    m_height = req.Info.Height;
    m_fourCC = req.Info.FourCC;

    return MFX_ERR_NONE;
}


mfxStatus MfxFrameAllocResponse::ReallocCmSurfaces(
    CmDevice *             device,
    mfxFrameAllocRequest & req)
{
    req.NumFrameSuggested = req.NumFrameMin;

    if (req.Info.Width > m_width || req.Info.Height > m_height || req.Info.FourCC != m_fourCC) {
        Free();
        AllocCmSurfaces(device, req);
    } else {
        if (req.NumFrameMin > NumFrameActual) {
            m_mids.resize(req.NumFrameMin, 0);
            m_locked.resize(req.NumFrameMin, 0);

            for (int i = NumFrameActual; i < req.NumFrameMin; i++)
                m_mids[i] = CreateSurface(device, m_width, m_height, req.Info.FourCC);

            NumFrameActual = req.NumFrameMin;
        }
    }
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

    mfxU16 vSliceWidth = (mfxU16)frameSizeExtra.vSliceWidth;
    mfxU16 frameWidth64 = (mfxU16)frameSizeExtra.frameWidth64;
    mfxU16 paddedFrameHeight = (mfxU16)frameSizeExtra.paddedFrameHeight;
    mfxU16 paddedFrameWidth = (mfxU16)frameSizeExtra.paddedFrameWidth;

    mfxFrameAllocRequest request = { { 0 } };

    //if (vSliceWidth != m_FrameSizeExtra.vSliceWidth || paddedFrameHeight != m_FrameSizeExtra.paddedFrameHeight || frNum > m_raw16padded.NumFrameActual) 
    {
        //m_raw16padded.Free();
        request.Info = newParam.vpp.In;
        request.Info.Width = (mfxU16)vSliceWidth;
        request.Info.Height = (mfxU16)paddedFrameHeight;

        request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum);
        request.Info.FourCC = CM_SURFACE_FORMAT_A8;
        request.Info.Width *= sizeof(mfxU16);

        //sts = m_raw16padded.AllocCmSurfaces(m_cmDevice, request);
        sts = m_raw16padded.ReallocCmSurfaces(m_cmDevice, request);
        MFX_CHECK_STS(sts);
    }


    //if (frameWidth64 != m_FrameSizeExtra.frameWidth64 || newParam.vpp.In.CropH != m_mfxVideoParam.vpp.In.CropH) 
    {
        //m_raw16aligned.Free();

        //request.Info = newParam.vpp.In;
        request.Info.Width = (mfxU16)frameWidth64;
        request.Info.Height = newParam.vpp.In.CropH;
        request.Info.FourCC = CM_SURFACE_FORMAT_A8;
        request.Info.Width *= sizeof(mfxU16);
        frNum = 3;
        request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum);

        //sts = m_raw16aligned.AllocCmSurfaces(m_cmDevice, request);
        sts = m_raw16aligned.ReallocCmSurfaces(m_cmDevice, request);
        MFX_CHECK_STS(sts);
    }

//    if (paddedFrameWidth != m_FrameSizeExtra.paddedFrameWidth || paddedFrameHeight != m_FrameSizeExtra.paddedFrameHeight) 
    {
//        m_aux8.Free();

       //request.Info = newParam.vpp.In;
        request.Info.Width = (mfxU16)paddedFrameWidth;
        request.Info.Height = (mfxU16)paddedFrameHeight;
        request.Info.FourCC = CM_SURFACE_FORMAT_A8;
        frNum = 2;
        request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum);

//        sts = m_aux8.AllocCmSurfaces(m_cmDevice, request);
        sts = m_aux8.ReallocCmSurfaces(m_cmDevice, request);
        MFX_CHECK_STS(sts);
    }

    if (vSliceWidth > m_FrameSizeExtra.vSliceWidth || paddedFrameHeight > m_FrameSizeExtra.paddedFrameHeight) {
        if (m_avgFlagSurf)
            m_cmDevice->DestroySurface(m_avgFlagSurf);
        m_avgFlagSurf = CreateSurface(m_cmDevice, vSliceWidth, paddedFrameHeight, CM_SURFACE_FORMAT_A8);
    }

    if (m_Caps.OutputMemoryOperationMode == MEM_GPUSHARED) {
//        if (newParam.vpp.In.CropH > m_mfxVideoParam.vpp.In.CropH || newParam.vpp.In.CropW > m_mfxVideoParam.vpp.In.CropW || !m_gammaOutSurf) 
        {
            if (m_gammaOutSurf)
                m_cmDevice->DestroySurface(m_gammaOutSurf);
            m_gammaOutSurf = CreateSurface(m_cmDevice, newParam.vpp.Out.CropW * 4,  newParam.vpp.Out.CropH, CM_SURFACE_FORMAT_A8);
        }
    } else if (m_gammaOutSurf)
        m_cmDevice->DestroySurface(m_gammaOutSurf);


//    if (!m_Caps.bNoPadding)
    {
        if (paddedFrameWidth > m_FrameSizeExtra.paddedFrameWidth || paddedFrameHeight > m_FrameSizeExtra.paddedFrameHeight) {
            if (m_paddedSurf)
                m_cmDevice->DestroySurface(m_paddedSurf);
            m_paddedSurf = CreateSurface(m_cmDevice,  paddedFrameWidth*sizeof(mfxU16), paddedFrameHeight, CM_SURFACE_FORMAT_A8);
        }
    }

    if (m_Caps.InputMemoryOperationMode == MEM_GPUSHARED) {
//        if (paddedFrameWidth > m_FrameSizeExtra.paddedFrameWidth || paddedFrameHeight > m_FrameSizeExtra.paddedFrameHeight)
        {
            if (m_cmSurfIn)
                m_cmDevice->DestroySurface(m_cmSurfIn);
            int width = m_Caps.bNoPadding ? paddedFrameWidth : newParam.vpp.In.CropW;
            int height = m_Caps.bNoPadding ? paddedFrameHeight : newParam.vpp.In.CropH;

#ifdef CAMERA_DEBUG_PRINTF
    FILE *f = fopen("CanonOut.txt", "at");
    fprintf(f, "ReallocateInternalSurfaces:  m_cmSurfIn width %d height %d \n", width, height);
    fclose(f);
#endif // CAMERA_DEBUG_PRINTF

            m_cmSurfIn = CreateSurface(m_cmDevice, width * sizeof(mfxU16),  height, CM_SURFACE_FORMAT_A8);
//            m_cmSurfIn = CreateSurface(m_cmDevice,  paddedFrameWidth*sizeof(mfxU16), paddedFrameHeight, CM_SURFACE_FORMAT_A8);
        }
    } else if (m_cmSurfIn)
        m_cmDevice->DestroySurface(m_cmSurfIn);

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

    request.Info.Width  = (mfxU16)m_FrameSizeExtra.frameWidth64;
    request.Info.Height = m_mfxVideoParam.vpp.In.CropH;
    //request.Type = MFX_MEMTYPE_D3D_INT;
    request.Info.FourCC = CM_SURFACE_FORMAT_A8;
    request.Info.Width *= sizeof(mfxU16);
    frNum = 3;
    request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum);

    sts = m_raw16aligned.AllocCmSurfaces(m_cmDevice, request);
    MFX_CHECK_STS(sts);

    request.Info.Width  = (mfxU16)m_FrameSizeExtra.paddedFrameWidth;
    request.Info.Height = (mfxU16)m_FrameSizeExtra.paddedFrameHeight;
    //request.Type = MFX_MEMTYPE_D3D_INT;
    request.Info.FourCC = CM_SURFACE_FORMAT_A8;
    frNum = 2;

    request.NumFrameMin = request.NumFrameSuggested = (mfxU16)(frNum);

    sts = m_aux8.AllocCmSurfaces(m_cmDevice, request);
    MFX_CHECK_STS(sts);

    m_avgFlagSurf = CreateSurface(m_cmDevice, m_FrameSizeExtra.vSliceWidth, m_FrameSizeExtra.paddedFrameHeight, CM_SURFACE_FORMAT_A8);

    if (m_Caps.bForwardGammaCorrection)
    {
        m_gammaCorrectSurf = CreateSurface(m_cmDevice, 32, 4, CM_SURFACE_FORMAT_A8);
        m_gammaPointSurf   = CreateSurface(m_cmDevice, 32, 4, CM_SURFACE_FORMAT_A8);
        MFX_CHECK_NULL_PTR2(m_gammaCorrectSurf, m_gammaPointSurf)

        mfxU8 *pGammaPts = (mfxU8 *)m_GammaParams.gamma_lut.gammaPoints;
        mfxU8 *pGammaCor = (mfxU8 *)m_GammaParams.gamma_lut.gammaCorrect;

        if (!pGammaPts || !pGammaCor)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        m_gammaCorrectSurf->WriteSurface(pGammaCor, NULL);
        m_gammaPointSurf->WriteSurface(pGammaPts, NULL);
    }


    if (m_Caps.InputMemoryOperationMode == MEM_GPUSHARED) // need to allocate inputSurf in case the output memory is not 4k aligned
    {
        int width = m_Caps.bNoPadding ? m_FrameSizeExtra.paddedFrameWidth : m_mfxVideoParam.vpp.In.CropW;
        int height = m_Caps.bNoPadding ? m_FrameSizeExtra.paddedFrameHeight : m_mfxVideoParam.vpp.In.CropH;

#ifdef CAMERA_DEBUG_PRINTF
    FILE *f = fopen("CanonOut.txt", "at");
    fprintf(f, "AllocateInternalSurfaces:  m_cmSurfIn width %d height %d \n", width, height);
    fclose(f);
#endif // CAMERA_DEBUG_PRINTF

        m_cmSurfIn = CreateSurface(m_cmDevice, width * sizeof(mfxU16),  height, CM_SURFACE_FORMAT_A8);
//      m_cmSurfIn = CreateSurface(m_cmDevice, m_FrameSizeExtra.paddedFrameWidth*sizeof(mfxU16),  m_FrameSizeExtra.paddedFrameHeight, CM_SURFACE_FORMAT_A8);
    }

    if (m_Caps.OutputMemoryOperationMode == MEM_GPUSHARED) // need to allocate gammaSurf in case the output memory is not 4k aligned
    {
        if (!m_Caps.bOutToARGB16)
            m_gammaOutSurf = CreateSurface(m_cmDevice, m_mfxVideoParam.vpp.Out.CropW,  m_mfxVideoParam.vpp.Out.CropH, CM_SURFACE_FORMAT_A8R8G8B8);
            //m_gammaOutSurf = CreateSurface(m_cmDevice, m_mfxVideoParam.vpp.Out.CropW * 4,  m_mfxVideoParam.vpp.Out.CropH, CM_SURFACE_FORMAT_A8);
        else //if (m_Caps.bOutToARGB16)
            m_gammaOutSurf = CreateSurface(m_cmDevice, m_mfxVideoParam.vpp.Out.CropW*sizeof(mfxU16),  m_mfxVideoParam.vpp.Out.CropH, CM_SURFACE_FORMAT_A8R8G8B8);
    }

//    if (!m_Caps.bNoPadding)
    m_paddedSurf = CreateSurface(m_cmDevice,  m_FrameSizeExtra.paddedFrameWidth*sizeof(mfxU16), m_FrameSizeExtra.paddedFrameHeight, CM_SURFACE_FORMAT_A8);

    // create buffer pool for (m_Caps.OutputMemoryOperationMode == MEM_GPU) !!!

#ifdef CAMERA_DEBUG_PRINTF
    FILE *f = fopen("CanonOut.txt", "at");
    fprintf(f, "AllocateInternalSurfaces:  m_cmSurfIn %p sts %d \n", m_cmSurfIn, sts);
    fclose(f);
#endif // CAMERA_DEBUG_PRINTF


    return sts;
}

mfxStatus MFXCamera_Plugin::SetExternalSurfaces(AsyncParams *pParam)
{

    mfxFrameSurface1 *surfIn = pParam->surf_in;
    mfxFrameSurface1 *surfOut = pParam->surf_out;

#ifdef CAMERA_DEBUG_PRINTF
    FILE *f = fopen("CanonOut.txt", "at");
    fprintf(f, "SetExternalSurfaces device %p \n", m_cmDevice);
    fprintf(f, "SetExternalSurfaces surfIn: MemId=%p Y16=%p surfOut: MemId=%p B=%p memin=%d memout=%d \n", surfIn->Data.MemId, surfIn->Data.Y16, surfOut->Data.MemId, surfOut->Data.B, m_Caps.InputMemoryOperationMode, m_Caps.OutputMemoryOperationMode);
    fprintf(f, "SetExternalSurfaces surfIn: cropW=%d cropH=%d width=%d height=%d pitch=%d \n", surfIn->Info.CropW, surfIn->Info.CropH, surfIn->Info.Width, surfIn->Info.Height, surfIn->Data.Pitch);
    fprintf(f, "SetExternalSurfaces surfOut: cropW=%d cropH=%d width=%d height=%d pitch=%d \n", surfOut->Info.CropW, surfOut->Info.CropH, surfOut->Info.Width, surfOut->Info.Height, surfOut->Data.Pitch);
    fclose(f);
#endif // CAMERA_DEBUG_PRINTF
    if (m_Caps.InputMemoryOperationMode == MEM_GPUSHARED)
        if (surfIn->Data.MemId)
            m_core->LockExternalFrame(surfIn->Data.MemId,  &surfIn->Data);

    if (m_Caps.OutputMemoryOperationMode == MEM_GPUSHARED)
        if (surfOut->Data.MemId)
            m_core->LockExternalFrame(surfOut->Data.MemId,  &surfOut->Data);

    mfxU32 inPitch = ((mfxU32)surfIn->Data.PitchHigh << 16) | ((mfxU32)surfIn->Data.PitchLow);

    if (m_Caps.bNoPadding)
    {
        //mfxU32 inWidth = (mfxU32)surfIn->Info.CropW + 64;
        mfxU32 inWidth = (mfxU32)surfIn->Info.Width;
        mfxU32 inHeight = (mfxU32)surfIn->Info.Height;
//        surfIn->Data.Y16 += inPitch * surfIn->Info.CropY + surfIn->Info.CropX;

        if (m_Caps.InputMemoryOperationMode == MEM_GPUSHARED) {

            mfxU32 allocSize, allocPitch;
#ifdef CAMERA_DEBUG_PRINTF
    f = fopen("CanonOut.txt", "at");
    fprintf(f, "SetExternalSurfaces Padded: m_cmDevice =%p \n", m_cmDevice);
    fclose(f);
#endif // CAMERA_DEBUG_PRINTF

            m_cmDevice->GetSurface2DInfo(sizeof(mfxU16)*inWidth, inHeight, CM_SURFACE_FORMAT_A8, allocPitch, allocSize);
#ifdef CAMERA_DEBUG_PRINTF
    f = fopen("CanonOut.txt", "at");
    fprintf(f, "SetExternalSurfaces Padded: inPitch=%d allocPitch=%d inWidth=%d inHeight=%d \n", inPitch, allocPitch,inWidth, inHeight);
    fclose(f);
#endif // CAMERA_DEBUG_PRINTF

            if (inPitch == allocPitch && !((mfxU64)surfIn->Data.Y16 & 0xFFF)) {
                pParam->inSurf2DUP = (mfxMemId)CreateSurface(m_cmDevice, inWidth*sizeof(mfxU16),  inHeight, CM_SURFACE_FORMAT_A8, (void *)surfIn->Data.Y16);
            } else 
            {
                pParam->inSurf2DUP = 0;
            }
            pParam->inSurf2D = 0;
        }
        else if (m_Caps.InputMemoryOperationMode == MEM_GPU) {
            mfxHDLPair outHdl;
            mfxStatus sts = m_core->GetExternalFrameHDL(surfIn->Data.MemId, (mfxHDL*)&outHdl);
            if (sts != MFX_ERR_NONE)
                return sts;
            CmSurface2D *pSurf2D = m_cmCtx->CreateCmSurface2D(outHdl.first);
            if (!pSurf2D)
                return MFX_ERR_MEMORY_ALLOC;
            pParam->inSurf2D = (mfxMemId)pSurf2D;
            pParam->inSurf2DUP = 0;
        }
    } 
    else 
    { // need to do padding

        if (m_Caps.InputMemoryOperationMode == MEM_GPUSHARED) {

            mfxU32 inWidth = (mfxU32)surfIn->Info.Width;
            mfxU32 inHeight = (mfxU32)surfIn->Info.Height;
            mfxU32 allocSize, allocPitch;
//            surfIn->Data.Y16 += inPitch * surfIn->Info.CropY + surfIn->Info.CropX;


#ifdef CAMERA_DEBUG_PRINTF
    f = fopen("CanonOut.txt", "at");
    fprintf(f, "SetExternalSurfaces Non Padded: m_cmDevice =%p \n", m_cmDevice);
    fclose(f);
#endif // CAMERA_DEBUG_PRINTF

            m_cmDevice->GetSurface2DInfo(sizeof(mfxU16)*inWidth, inHeight, CM_SURFACE_FORMAT_A8, allocPitch, allocSize);

#ifdef CAMERA_DEBUG_PRINTF
    f = fopen("CanonOut.txt", "at");
    fprintf(f, "SetExternalSurfaces Non Padded: inPitch=%d allocPitch=%d inWidth=%d inHeight=%d \n", inPitch, allocPitch,inWidth, inHeight);
    fclose(f);
#endif // CAMERA_DEBUG_PRINTF


            if (inPitch == allocPitch && !((mfxU64)surfIn->Data.Y16 & 0xFFF)) {
                pParam->inSurf2DUP = (mfxMemId)CreateSurface(m_cmDevice, allocPitch,  inHeight, CM_SURFACE_FORMAT_A8, (void *)surfIn->Data.Y16);
            } 
            else 
            {
                pParam->inSurf2DUP = 0;
            }
            pParam->inSurf2D = 0;
        }
        else if (m_Caps.InputMemoryOperationMode == MEM_GPU) {
            mfxHDLPair outHdl;
            mfxStatus sts = m_core->GetExternalFrameHDL(surfIn->Data.MemId, (mfxHDL*)&outHdl);
            if (sts != MFX_ERR_NONE)
                return sts;
            CmSurface2D *pSurf2D = m_cmCtx->CreateCmSurface2D(outHdl.first);
            if (!pSurf2D)
                return MFX_ERR_MEMORY_ALLOC;
            pParam->inSurf2D = (mfxMemId)pSurf2D;
            pParam->inSurf2DUP = 0;
        }

    }

    if (m_Caps.OutputMemoryOperationMode == MEM_GPUSHARED) {

        mfxU32 outPitch = ((mfxU32)surfOut->Data.PitchHigh << 16) | ((mfxU32)surfOut->Data.PitchLow);
        mfxU32 outWidth = (mfxU32)surfOut->Info.Width;
        mfxU32 outHeight = (mfxU32)surfOut->Info.Height;
        mfxU32 allocSize, allocPitch;

        if (m_Caps.bOutToARGB16)  {
            m_cmDevice->GetSurface2DInfo(sizeof(mfxU16)*outWidth, outHeight, CM_SURFACE_FORMAT_A8R8G8B8, allocPitch, allocSize);
        } else {
            m_cmDevice->GetSurface2DInfo(4*outWidth, outHeight, CM_SURFACE_FORMAT_A8, allocPitch, allocSize);
        }
        //if (outPitch == allocPitch && !((mfxU64)surfOut->Data.B & 0xFFF) && !(surfOut->Data.FrameOrder & 0x1)) {
        if (outPitch == allocPitch && !((mfxU64)surfOut->Data.B & 0xFFF)) {
            if (m_Caps.bOutToARGB16)
                pParam->outSurf2DUP = (mfxMemId)CreateSurface(m_cmDevice, allocPitch>>2, outHeight, CM_SURFACE_FORMAT_A8R8G8B8, (void *)surfOut->Data.V16);
            else
                pParam->outSurf2DUP = (mfxMemId)CreateSurface(m_cmDevice, allocPitch, outHeight, CM_SURFACE_FORMAT_A8, (void *)surfOut->Data.B);
        } else {
            pParam->outSurf2DUP = 0;
        }
        pParam->outSurf2D = 0;

    } else if (m_Caps.OutputMemoryOperationMode == MEM_GPU) {
        mfxHDLPair outHdl;
        //m_core->GetFrameHDL(surfOut->Data.MemId, &outHdl);

#ifdef CAMERA_DEBUG_PRINTF
if (!m_core->IsExternalFrameAllocator()) {
    f = fopen("CanonOut.txt", "at");
    fprintf(f, "SetExternalSurfaces ExternalFrameAllocator NOT SET ! \n");
    fclose(f);
}
#endif // CAMERA_DEBUG_PRINTF

        mfxStatus sts = m_core->GetExternalFrameHDL(surfOut->Data.MemId, (mfxHDL*)&outHdl);

#ifdef CAMERA_DEBUG_PRINTF
    f = fopen("CanonOut.txt", "at");
    fprintf(f, "SetExternalSurfaces GetExternalFrameHDL sts = %d \n", sts);
    fclose(f);
#endif // CAMERA_DEBUG_PRINTF

        if (sts != MFX_ERR_NONE)
            return sts;
        CmSurface2D *pSurf2D = m_cmCtx->CreateCmSurface2D(outHdl.first);

#ifdef CAMERA_DEBUG_PRINTF
    f = fopen("CanonOut.txt", "at");
    fprintf(f, "SetExternalSurfaces CreateCmSurface2D pSurf2D = %p \n", pSurf2D);
    fclose(f);
#endif // CAMERA_DEBUG_PRINTF


        if (!pSurf2D)
            return MFX_ERR_MEMORY_ALLOC;
        pParam->outSurf2D = (mfxMemId)pSurf2D;
        pParam->outSurf2DUP = 0;
    }

    if (m_Caps.InputMemoryOperationMode == MEM_GPUSHARED)
        if (surfIn->Data.MemId)
            m_core->UnlockExternalFrame(surfIn->Data.MemId,  &surfIn->Data);

    if (m_Caps.OutputMemoryOperationMode == MEM_GPUSHARED)
        if (surfOut->Data.MemId)
            m_core->UnlockExternalFrame(surfOut->Data.MemId,  &surfOut->Data);

#ifdef CAMERA_DEBUG_PRINTF
    f = fopen("CanonOut.txt", "at");
    fprintf(f, "SetExternalSurfaces end device %p inSurf2DUP=%p inSurf2D=%p outSurf2DUP=%p outSurf2D=%p \n", m_cmDevice, pParam->inSurf2DUP, pParam->inSurf2D, pParam->outSurf2DUP, pParam->outSurf2D);
    fclose(f);
#endif // CAMERA_DEBUG_PRINTF

    return MFX_ERR_NONE;
}

mfxStatus MFXCamera_Plugin::CreateEnqueueTasks(AsyncParams *pParam)
{
    CmEvent *e = NULL;

    //UMC::AutomaticUMCMutex guard(m_guard);

    SurfaceIndex *pInputSurfaceIndex;

#ifdef CAMERA_DEBUG_PRINTF
    FILE *f = fopen("CanonOut.txt", "at");
    fprintf(f, "CreateEnqueueTasks device %p inSurf2DUP=%p inSurf2D=%p outSurf2DUP=%p outSurf2D=%p \n",m_cmDevice,  pParam->inSurf2DUP, pParam->inSurf2D, pParam->outSurf2DUP, pParam->outSurf2D);
    fclose(f);
#endif // CAMERA_DEBUG_PRINTF

    if (pParam->inSurf2D) {
        CmSurface2D *inSurf = (CmSurface2D *)pParam->inSurf2D;
        inSurf->GetIndex(pInputSurfaceIndex);
    } else if (pParam->inSurf2DUP) {
        CmSurface2DUP *inSurf = (CmSurface2DUP *)pParam->inSurf2DUP;
        inSurf->GetIndex(pInputSurfaceIndex);
    } else if (m_Caps.InputMemoryOperationMode == MEM_GPUSHARED) {
        m_cmSurfIn->GetIndex(pInputSurfaceIndex);
    } else
        return MFX_ERR_NULL_PTR;

#ifdef CAMERA_DEBUG_PRINTF
    f = fopen("CanonOut.txt", "at");
    fprintf(f, "CreateEnqueueTasks device %p pInputSurfaceIndex=%p %d \n",m_cmDevice, pInputSurfaceIndex, *pInputSurfaceIndex);
    fclose(f);
#endif // CAMERA_DEBUG_PRINTF


    {
        UMC::AutomaticUMCMutex guard(m_guard);
        int doShiftAndSwap = 1;

#ifdef CAMP_PIPE_ITT
        __itt_task_begin(CamPipeAccel, __itt_null, __itt_null, task21);
#endif

        SurfaceIndex *pPaddedSurfaceIndex;
        m_paddedSurf->GetIndex(pPaddedSurfaceIndex);
        if (!m_Caps.bNoPadding) {
            m_cmCtx->CreateTask_Padding(*pInputSurfaceIndex, *pPaddedSurfaceIndex, m_FrameSizeExtra.paddedFrameWidth, m_FrameSizeExtra.paddedFrameHeight, (int)m_InputBitDepth, (int)m_Caps.BayerPatternType);
            //pInputSurfaceIndex = pPaddedSurfaceIndex;
            m_paddedSurf->GetIndex(pInputSurfaceIndex);
            doShiftAndSwap = 0;
        }

        CmSurface2D *goodPixCntSurf = (CmSurface2D *)AcquireResource(m_aux8, 0);
        CmSurface2D *bigPixCntSurf = (CmSurface2D *)AcquireResource(m_aux8, 1);

        m_cmCtx->CreateTask_GoodPixelCheck(*pInputSurfaceIndex, *pPaddedSurfaceIndex, goodPixCntSurf, bigPixCntSurf, (int)m_InputBitDepth, doShiftAndSwap, (int)m_Caps.BayerPatternType);
        pInputSurfaceIndex = pPaddedSurfaceIndex;

        CmSurface2D *greenHorSurf = (CmSurface2D *)AcquireResource(m_raw16padded, 0);
        CmSurface2D *greenVerSurf = (CmSurface2D *)AcquireResource(m_raw16padded, 1);
        CmSurface2D *greenAvgSurf = (CmSurface2D *)AcquireResource(m_raw16padded, 2);
        CmSurface2D *avgFlagSurf = m_avgFlagSurf;

        m_cmCtx->CreateTask_RestoreGreen(*pInputSurfaceIndex, goodPixCntSurf, bigPixCntSurf, greenHorSurf, greenVerSurf, greenAvgSurf, avgFlagSurf, m_InputBitDepth);

        CmSurface2D *blueHorSurf = (CmSurface2D *)AcquireResource(m_raw16padded, 3);
        CmSurface2D *blueVerSurf = (CmSurface2D *)AcquireResource(m_raw16padded, 4);
        CmSurface2D *blueAvgSurf = (CmSurface2D *)AcquireResource(m_raw16padded, 5);
        CmSurface2D *redHorSurf = (CmSurface2D *)AcquireResource(m_raw16padded, 6);
        CmSurface2D *redVerSurf = (CmSurface2D *)AcquireResource(m_raw16padded, 7);
        CmSurface2D *redAvgSurf = (CmSurface2D *)AcquireResource(m_raw16padded, 8);

        m_cmCtx->CreateTask_RestoreBlueRed(*pInputSurfaceIndex,
                                            greenHorSurf, greenVerSurf, greenAvgSurf,
                                            blueHorSurf, blueVerSurf, blueAvgSurf,
                                            redHorSurf, redVerSurf, redAvgSurf,
                                            avgFlagSurf, m_InputBitDepth);


        CmSurface2D *redOutSurf   = (CmSurface2D *)AcquireResource(m_raw16aligned, 0);
        CmSurface2D *greenOutSurf = (CmSurface2D *)AcquireResource(m_raw16aligned, 1);
        CmSurface2D *blueOutSurf  = (CmSurface2D *)AcquireResource(m_raw16aligned, 2);

        m_cmCtx->CreateTask_SAD(redHorSurf, greenHorSurf, blueHorSurf, redVerSurf, greenVerSurf, blueVerSurf, redOutSurf, greenOutSurf, blueOutSurf, (int)m_Caps.BayerPatternType);

        m_cmCtx->CreateTask_DecideAverage(redAvgSurf, greenAvgSurf, blueAvgSurf, avgFlagSurf, redOutSurf, greenOutSurf, blueOutSurf, (int)m_Caps.BayerPatternType);


        SurfaceIndex *pOutputSurfaceIndex;

        if (m_Caps.OutputMemoryOperationMode == MEM_GPUSHARED && !pParam->outSurf2DUP) {
            //m_gammaOutSurf->GetIndex(pOutputSurfaceIndex);
            pOutputSurfaceIndex = NULL;
        } else if (pParam->outSurf2DUP) {
            CmSurface2DUP *outSurfUP = (CmSurface2DUP *)pParam->outSurf2DUP;
            outSurfUP->GetIndex(pOutputSurfaceIndex);
        } else if (pParam->outSurf2D) {
            CmSurface2D *outSurf2D = (CmSurface2D *)pParam->outSurf2D;
            outSurf2D->GetIndex(pOutputSurfaceIndex);
        } else
            return MFX_ERR_NULL_PTR;

        if (m_Caps.BayerPatternType == MFX_CAM_BAYER_BGGR || m_Caps.BayerPatternType == MFX_CAM_BAYER_GRBG) {
            CmSurface2D *tmpp = redOutSurf;
            redOutSurf = blueOutSurf;
            blueOutSurf = tmpp;
        }

        if (m_Caps.bForwardGammaCorrection) {
            if (pOutputSurfaceIndex)
                m_cmCtx->CreateTask_ForwardGamma(m_gammaCorrectSurf, m_gammaPointSurf, redOutSurf, greenOutSurf, blueOutSurf, *pOutputSurfaceIndex, m_InputBitDepth);
            else
                m_cmCtx->CreateTask_ForwardGamma(m_gammaCorrectSurf, m_gammaPointSurf, redOutSurf, greenOutSurf, blueOutSurf, m_gammaOutSurf, m_InputBitDepth);
        }

        if (m_Caps.bOutToARGB16 || !m_Caps.bForwardGammaCorrection || m_InputBitDepth == 16) {
            if (!pOutputSurfaceIndex)
                 m_gammaOutSurf->GetIndex(pOutputSurfaceIndex);
            m_cmCtx->CreateTask_ARGB(redOutSurf, greenOutSurf, blueOutSurf, *pOutputSurfaceIndex, m_InputBitDepth);
        }

#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipeAccel);
#endif
    //}

    //{
    //    UMC::AutomaticUMCMutex guard(m_guard);

#ifdef CAMP_PIPE_ITT
        __itt_task_begin(CamPipeAccel, __itt_null, __itt_null, task22);
#endif

        if (m_Caps.InputMemoryOperationMode == MEM_GPUSHARED && !pParam->inSurf2DUP)
        {

#ifdef CAMERA_DEBUG_PRINTF
    f = fopen("CanonOut.txt", "at");
    fprintf(f, "CreateEnqueueTasks EnqueueCopyCPUToGPU Width*2=%d Pitch=%d device %p \n", pParam->surf_in->Info.Width*2, pParam->surf_in->Data.Pitch, m_cmDevice);
    fclose(f);
#endif // CAMERA_DEBUG_PRINTF

            if (pParam->surf_in->Info.Width*2 == pParam->surf_in->Data.Pitch)
                m_cmCtx->EnqueueCopyCPUToGPU(m_cmSurfIn, pParam->surf_in->Data.Y16);
            else
                m_cmCtx->EnqueueCopyCPUToGPU(m_cmSurfIn, pParam->surf_in->Data.Y16, pParam->surf_in->Data.Pitch);
        }

#ifdef CAMERA_DEBUG_PRINTF
    f = fopen("CanonOut.txt", "at");
    fprintf(f, "CreateEnqueueTasks after EnqueueCopyCPUToGPU device %p\n", m_cmDevice);
    fclose(f);
#endif // CAMERA_DEBUG_PRINTF


        if (!m_Caps.bNoPadding)
            m_cmCtx->EnqueueTask_Padding();

        m_cmCtx->EnqueueTask_GoodPixelCheck();
        for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++) {
            m_cmCtx->EnqueueSliceTasks(i);
        }


#ifdef CAMERA_DEBUG_PRINTF
    f = fopen("CanonOut.txt", "at");
    fprintf(f, "CreateEnqueueTasks after EnqueueSliceTasks  \n");
    fclose(f);
#endif // CAMERA_DEBUG_PRINTF


        if (m_Caps.bForwardGammaCorrection)
            e = m_cmCtx->EnqueueTask_ForwardGamma();

        if (m_Caps.bOutToARGB16 || !m_Caps.bForwardGammaCorrection || m_InputBitDepth == 16) {
            if (e != NULL)
                m_cmCtx->DestroyEventWithoutWait(e);
            e = m_cmCtx->EnqueueTask_ARGB();
        }

#ifdef CAMERA_DEBUG_PRINTF
    f = fopen("CanonOut.txt", "at");
    fprintf(f, "CreateEnqueueTasks after EnqueueTask_ARGB \n");
    fclose(f);
#endif // CAMERA_DEBUG_PRINTF


#ifdef CAMP_PIPE_ITT
        __itt_task_end(CamPipeAccel);
#endif

        if (m_Caps.OutputMemoryOperationMode == MEM_GPUSHARED && !pParam->outSurf2DUP)
        {
// cropW instead of width ??? 
            m_cmCtx->DestroyEventWithoutWait(e);
            if (m_Caps.bOutToARGB16) {
                if (pParam->surf_out->Info.Width*8 == pParam->surf_out->Data.Pitch)
//                    m_gammaOutSurf->ReadSurface(pParam->surf_out->Data.B, e);
                    e = m_cmCtx->EnqueueCopyGPUToCPU(m_gammaOutSurf, pParam->surf_out->Data.B);
                else
                    e = m_cmCtx->EnqueueCopyGPUToCPU(m_gammaOutSurf, pParam->surf_out->Data.B, pParam->surf_out->Data.Pitch);
            } else {
                if (pParam->surf_out->Info.Width*4 == pParam->surf_out->Data.Pitch)
                    e = m_cmCtx->EnqueueCopyGPUToCPU(m_gammaOutSurf, pParam->surf_out->Data.B);
                else
                    e = m_cmCtx->EnqueueCopyGPUToCPU(m_gammaOutSurf, pParam->surf_out->Data.B, pParam->surf_out->Data.Pitch);
            }
        }
        pParam->pEvent = e;
    }

#ifdef CAMERA_DEBUG_PRINTF
    f = fopen("CanonOut.txt", "at");
    fprintf(f, "CreateEnqueueTasks after EnqueueCopyGPUToCPU e=%p p=%p\n", pParam->pEvent, pParam->surf_out->Data.B);
    fclose(f);
#endif // CAMERA_DEBUG_PRINTF

    CAMERA_LOG("CreateEnqueueTasks after EnqueueCopyGPUToCPU device=%p e=%p p=%p\n", m_cmDevice,  pParam->pEvent, pParam->surf_out->Data.B);

    return MFX_ERR_NONE;
}

void QueryCaps(mfxCameraCaps &caps)
{
    caps.ModuleConfiguration = 0; // zero all filters
    caps.bDemosaic = 1;
    caps.bForwardGammaCorrection = 1;
    //caps.bOutToARGB8 = 1;
    caps.bOutToARGB16 = 1;

    caps.InputMemoryOperationMode = MEM_GPUSHARED; //MEM_GPU; 
    caps.OutputMemoryOperationMode = MEM_GPUSHARED; //MEM_GPU;

    caps.bNoPadding = 1;
}


const mfxU32 g_TABLE_CAMERA_EXTBUFS [] =
{
    MFX_EXTBUF_CAM_GAMMA_CORRECTION,
    MFX_EXTBUF_CAM_WHITE_BALANCE,
    MFX_EXTBUF_CAM_HOT_PIXEL_REMOVAL,
    MFX_EXTBUF_CAM_BLACK_LEVEL_CORRECTION,
//    MFX_EXTBUF_CAM_VIGNETTE_CORRECTION,
//    MFX_EXTBUF_CAM_BAYER_DENOISE,
    MFX_EXTBUF_CAM_COLOR_CORRECTION_3X3,
    MFX_EXTBUF_CAM_PADDING,
    MFX_EXTBUF_CAM_PIPECONTROL
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


void GetDoUseFilterList(mfxVideoParam* par, mfxU32** ppList, mfxU32** ppLen)
{
    mfxU32 i = 0;
    mfxExtVPPDoUse* pVPPHint = NULL;

    /* robustness */
    *ppList = NULL;
    *ppLen = NULL;

    for (i = 0; i < par->NumExtParam; i++)
    {
        if (MFX_EXTBUFF_VPP_DOUSE == par->ExtParam[i]->BufferId)
        {
            pVPPHint  = (mfxExtVPPDoUse*)(par->ExtParam[i]);
            *ppList = pVPPHint->AlgList;
            *ppLen  = &pVPPHint->NumAlg;
            break;
        }
    }
}

mfxStatus CorrectDoUseFilters(mfxU32* pList, mfxU32 len)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 i = 0;
    mfxU32 searchCount = sizeof(g_TABLE_CAMERA_EXTBUFS) / sizeof(*g_TABLE_CAMERA_EXTBUFS);
    for (i = 0; i < len; i++)
    {
        if (!IsFilterFound(g_TABLE_CAMERA_EXTBUFS, searchCount, pList[i])) 
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
    mfxU32 searchCount = sizeof(g_TABLE_CAMERA_EXTBUFS) / sizeof(*g_TABLE_CAMERA_EXTBUFS);

    for (fIdx = 0; fIdx < fCount; fIdx++ )
    {
        mfxU32 curId = par->ExtParam[fIdx]->BufferId;
        if (MFX_EXTBUFF_VPP_DOUSE == curId)
            continue;
        if (IsFilterFound(g_TABLE_CAMERA_EXTBUFS, searchCount, curId)) {
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
/*    if (caps.bVignetteCorrection)
    {
        list.push_back(MFX_EXTBUF_CAM_VIGNETTE_CORRECTION);
    }*/
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
/*    if(caps.bBayerDenoise)
    {
        list.push_back(MFX_EXTBUF_CAM_BAYER_DENOISE);
    }*/
    if(caps.bNoPadding)
    {
        list.push_back(MFX_EXTBUF_CAM_PADDING);
    }
}

#define MFX_CAM_DEFAULT_GAMMA_VALUE 2.2
#define MFX_CAM_GAMMA_VALUE_MIN 0.1 //??? 1.0?
#define MFX_CAM_GAMMA_NUM_POINTS_MIN 8 // ???

mfxStatus GammaCorrectionCheckParam(mfxExtCamGammaCorrection * pGammaBuf, mfxU32 /*bitdepth*/)
{
    mfxStatus sts = MFX_ERR_NONE;
    if (pGammaBuf->Mode != MFX_CAM_GAMMA_VALUE && pGammaBuf->Mode != MFX_CAM_GAMMA_LUT)
    {
        pGammaBuf->GammaValue = (mfxF64)0;
        pGammaBuf->NumPoints = 0;
        return MFX_ERR_UNSUPPORTED; // ??? CLIP and MFX_ERR_UNSUPPORTED ???
    } 
    else if (pGammaBuf->Mode == MFX_CAM_GAMMA_VALUE)
    {
        if (pGammaBuf->GammaValue < MFX_CAM_GAMMA_VALUE_MIN)
        {
            pGammaBuf->GammaValue = MFX_CAM_GAMMA_VALUE_MIN;
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM; //MFX_WRN_INCOMPATIBLE_VIDEO_PARAM ???
        }
    }
    else if (pGammaBuf->Mode == MFX_CAM_GAMMA_LUT)
    {
        mfxU32 numPoints = MFX_CAM_GAMMA_NUM_POINTS_SKL;
        if (pGammaBuf->NumPoints != numPoints)
        {
            pGammaBuf->NumPoints = (mfxU16)numPoints;
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }

        if (!(pGammaBuf->GammaPoint && pGammaBuf->GammaCorrected))
            return MFX_ERR_NULL_PTR;
        //mfxU16 *points = pGammaBuf->GammaPoint;
        //if (points[0] != 0) {
        //    points[0] = 0;
        //    return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        //}
        //if (points[numPoints-1] != (1 << bitdepth) - 1) {
        //    points[numPoints-1] = (1 << bitdepth) - 1;
        //    return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        //}
        //for (mfxU32 i = 0; i < numPoints - 1; i++) {
        //    if (points[i+1] <= points[i]) {
        //        mfxU16 tmp = points[i];
        //        points[i] = points[i+1];
        //        points[i+1] = tmp;
        //        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        //    }
        //}
    }
    return sts;
}

mfxStatus QueryExtBuf(mfxExtBuffer *extBuf, mfxU32 bitdepth, mfxU32 action)
{
    mfxStatus sts = MFX_ERR_NONE;
    switch (extBuf->BufferId)
    {
    case MFX_EXTBUF_CAM_GAMMA_CORRECTION:
        {
            mfxExtCamGammaCorrection *gammaBuf = (mfxExtCamGammaCorrection *)extBuf;
            if (action >= MFX_CAM_QUERY_CHECK_RANGE)
                sts = GammaCorrectionCheckParam(gammaBuf, bitdepth);
            else {
                gammaBuf->Mode = gammaBuf->NumPoints = (mfxU16)action;
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
    /*case MFX_EXTBUF_CAM_VIGNETTE_CORRECTION:
        {
            mfxExtCamVignetteCorrection *vignetteBuf = (mfxExtCamVignetteCorrection *)extBuf;
            if (action >= MFX_CAM_QUERY_CHECK_RANGE) {
                //sts = VignetteCorrectionCheckParam(vignetteBuf);
            } else {
                vignetteBuf->Height = vignetteBuf->Width = vignetteBuf->Pitch = vignetteBuf->MaskPrecision = action;
            }
        }
        break;*/
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
    /*case MFX_EXTBUF_CAM_BAYER_DENOISE:
        {
            mfxExtCamBayerDenoise *denoiseBuf = (mfxExtCamBayerDenoise *)extBuf;
            if (action >= MFX_CAM_QUERY_CHECK_RANGE) {
                //sts = BayerDenoiseCheckParam(denoiseBuf);
            } else {
                denoiseBuf->Threshold = (mfxU16)action;
            }
        }
        break;*/
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
    case MFX_EXTBUF_CAM_PIPECONTROL:
        {
            mfxExtCamPipeControl *pipeBuf = (mfxExtCamPipeControl *)extBuf;
            if (action >= MFX_CAM_QUERY_CHECK_RANGE) {
                //sts = PaddingCheckParam(padBuf);
            } else {
                pipeBuf->RawFormat = (mfxU16)action;
            }
        }
        break;
    default:
        sts = MFX_ERR_UNSUPPORTED;
        break;
    }
    return sts;
}

void correctDoUseList(mfxU32 *pList, mfxU32 extCount, mfxU32 supportedCount)
{
    mfxU32 i, j;
    for (i = 0; i < supportedCount; i++)
    {
        if (pList[i] == 0)
        {
            for (j = i + 1; j < extCount; j++)
            {
                if (pList[j])
                {
                    pList[i] = pList[j];
                    pList[j] = 0;
                    break;
                }
            }
        }
    }
}

mfxStatus MfxCameraPlugin::CheckIOPattern(mfxVideoParam *in, mfxVideoParam *out, mfxU32 mode)
{

   mfxStatus error_status = (mode==0) ? MFX_ERR_UNSUPPORTED : MFX_ERR_INVALID_VIDEO_PARAM;

   if(in->IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_IN_OPAQUE_MEMORY))
   {
        out->IOPattern = 0;
        return error_status;
   }

   if(!((in->IOPattern & 0x0f) && (in->IOPattern & 0xf0)))
   {
        out->IOPattern = 0;
        return error_status;
   }

   if(in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY && in->vpp.In.FourCC == MFX_FOURCC_ARGB16)
   {
        out->IOPattern = 0;
        return error_status;
   }

   if(in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
   {
        out->IOPattern = 0;
        return error_status;
   }
   return MFX_ERR_NONE;
}

mfxStatus MfxCameraPlugin::CheckExtBuffers(mfxVideoParam *param, mfxVideoParam *out, mfxU32 mode)
{
    if (0 != param->NumExtParam && NULL == param->ExtParam)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    mfxStatus sts = MFX_ERR_NONE;

    mfxU32*   pExtList = NULL;
    mfxU32    extCount = 0;
    mfxU32    i;
    mfxU32*   pOutList = NULL;
    mfxU32    outCount = 0;
    mfxU32    *pNumAlg = NULL;

    GetDoUseFilterList(param, &pExtList, &pNumAlg);
    if (pNumAlg)
        extCount = *pNumAlg;
    if (out)
    {
        GetDoUseFilterList(out, &pOutList, &pNumAlg);
        if (pNumAlg)
            outCount = *pNumAlg;
        if (extCount != outCount || (pExtList && !pOutList) || (!pExtList && pOutList) || (extCount && !pExtList)) // just in case - should be checked by now
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    mfxU32 searchCount = sizeof(g_TABLE_CAMERA_EXTBUFS) / sizeof(*g_TABLE_CAMERA_EXTBUFS);
    mfxU32 supportedCount = extCount;
    bool camPipefound = false;
    for (i = 0; i < extCount; i++)
    {
        if (!IsFilterFound(g_TABLE_CAMERA_EXTBUFS, searchCount, pExtList[i]))
        {
            if (mode != MFX_CAM_QUERY_RETURN_STATUS)
            {
                if (!out) 
                    pExtList[i] = 0;
                else
                    pOutList[i] = 0;
                supportedCount--;
            }
            sts = MFX_ERR_UNSUPPORTED;
        }
        else if (out)
        {
            pOutList[i] = pExtList[i];
        }
    }

    if (supportedCount != extCount)
    {
        mfxU32 *pList = out ? pOutList : pExtList;
        correctDoUseList(pList, extCount, supportedCount);
        *pNumAlg = supportedCount;
    }
    MFX_CHECK_STS(sts);

    std::vector<mfxU32> pipelineList(1);

    for (i = 0; i < param->NumExtParam; i++)
    {
        mfxU32 curId = param->ExtParam[i]->BufferId;
        if (MFX_EXTBUFF_VPP_DOUSE == curId)
            continue;
        if(curId == MFX_EXTBUF_CAM_PIPECONTROL)
            camPipefound = true;
        if (!IsFilterFound(g_TABLE_CAMERA_EXTBUFS, searchCount, curId))
        {
            sts = MFX_ERR_UNSUPPORTED;
            break;
        }
    }

    if(!camPipefound)
        sts = MFX_ERR_UNSUPPORTED;

    MFX_CHECK_STS(sts);

    // no non-camera ext buffers; now to checking which of them are supported
    std::vector<mfxU32> capsList(1);
    mfxCameraCaps caps;
    QueryCaps(caps);
    ConvertCaps2ListDoUse(caps, capsList);
    capsList.push_back(MFX_EXTBUF_CAM_PIPECONTROL);

    for (i = 0; i < extCount; i++)
    {
        if (!IsFilterFound(&capsList[0], (mfxU32)capsList.size(), pExtList[i]))
        {
            if (mode != MFX_CAM_QUERY_RETURN_STATUS)
            {
                if (!out)
                    pExtList[i] = 0;
                else
                    pOutList[i] = 0;
                supportedCount--;
            }
            sts = MFX_WRN_FILTER_SKIPPED;
        }
        else if (out)
        {
            pOutList[i] = pExtList[i];
        }
    }

    if (supportedCount != extCount)
    {
        mfxU32 *pList = out ? pOutList : pExtList;
        correctDoUseList(pList, extCount, supportedCount);
        *pNumAlg = supportedCount;
    }

    for (i = 0; i < param->NumExtParam; i++)
    {
        mfxU32 curId = param->ExtParam[i]->BufferId;
        if (MFX_EXTBUFF_VPP_DOUSE == curId)
            continue;
        if (!IsFilterFound(&capsList[0], (mfxU32)capsList.size(), curId)) 
        {
            if (mode != MFX_CAM_QUERY_RETURN_STATUS)
            {
                mfxExtBuffer *pBuf = out ? out->ExtParam[i] : param->ExtParam[i];
                QueryExtBuf(pBuf, param->vpp.In.BitDepthLuma, MFX_CAM_QUERY_SET0);
                sts = MFX_WRN_FILTER_SKIPPED;
            }
            else
            {
                sts = MFX_WRN_FILTER_SKIPPED;
                break;
            }
        } 
        else if (IsFilterFound(&pipelineList[0], (mfxU32)pipelineList.size(), curId)) 
        {
            if (mode != MFX_CAM_QUERY_RETURN_STATUS)
            {
                mfxExtBuffer *pBuf = out ? out->ExtParam[i] : param->ExtParam[i];
                QueryExtBuf(pBuf, param->vpp.In.BitDepthLuma, MFX_CAM_QUERY_SET0);
            }
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        } 
        else 
        {
            pipelineList.push_back(curId);
            if (mode != MFX_CAM_QUERY_RETURN_STATUS)
            {
                mfxU32 action = mode;
                if (action == MFX_CAM_QUERY_SIGNAL)
                    action = MFX_CAM_QUERY_SET1;

                mfxExtBuffer *pBuf = param->ExtParam[i];
                if (out)
                {
                    memcpy_s(out->ExtParam[i], out->ExtParam[i]->BufferSz, param->ExtParam[i], param->ExtParam[i]->BufferSz); // can we rely on BufferSz ? or use GetConfigSize instead?
                    pBuf = out->ExtParam[i];
                }
                sts = QueryExtBuf(pBuf, param->vpp.In.BitDepthLuma, action);
                // sts can be MFX_ERR_INCOMPATIBLE_VIDEO_PARAM if (action == MFX_CAM_QUERY_CHECK_RANGE)
                if (sts < MFX_ERR_NONE) // NB: by this time we may have some filters skipped, so another call with (mode != MFX_CAM_QUERY_CHECK_RANGE) is required
                    break;
            }
        }
    }
    return sts;
}

mfxStatus MFXCamera_Plugin::WaitForActiveThreads()
{
    mfxStatus sts = MFX_WRN_IN_EXECUTION;
    for (mfxU32 i = 0; i < 200; i++)
    {
        if (m_activeThreadCount == 0)
        {
            sts = MFX_ERR_NONE;
            break;
        }
        vm_time_sleep(10);
    }
    return sts;
};
