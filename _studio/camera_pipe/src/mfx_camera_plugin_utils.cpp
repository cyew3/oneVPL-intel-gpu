//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2016 Intel Corporation. All Rights Reserved.
//

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


void QueryCaps(mfxCameraCaps &caps)
{
    caps.ModuleConfiguration = 0; // zero all filters
    caps.bDemosaic = 1;
    caps.bForwardGammaCorrection = 1;
    caps.bBlackLevelCorrection   = 1;
    caps.bWhiteBalance           = 1;
    caps.bColorConversionMatrix  = 1;
    caps.bVignetteCorrection     = 1;
    caps.bOutToARGB16  = 1;
    caps.bHotPixel     = 1;
    caps.bBayerDenoise = 1;
    caps.bLensCorrection = 1;
    caps.b3DLUT          = 1;

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
    MFX_EXTBUF_CAM_VIGNETTE_CORRECTION,
    MFX_EXTBUF_CAM_BAYER_DENOISE,
    MFX_EXTBUFF_VPP_DENOISE,
    MFX_EXTBUF_CAM_COLOR_CORRECTION_3X3,
    MFX_EXTBUF_CAM_LENS_GEOM_DIST_CORRECTION,
    MFX_EXTBUF_CAM_PADDING,
    MFX_EXTBUF_CAM_PIPECONTROL,
    MFX_EXTBUF_CAM_3DLUT
};

bool IsCameraFilterFound(const mfxU32* pList, mfxU32 len, mfxU32 filterName)
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
        if (!IsCameraFilterFound(g_TABLE_CAMERA_EXTBUFS, searchCount, pList[i]))
        {
            pList[i] = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }
    }
    return sts;
}

mfxStatus GetConfigurableCameraFilterList(mfxVideoParam* par, mfxU32* pList, mfxU32* pLen)
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
        if (IsCameraFilterFound(g_TABLE_CAMERA_EXTBUFS, searchCount, curId))
        {
            if (pList && !IsCameraFilterFound(pList, *pLen, curId))
            {
                pList[(*pLen)++] = curId;
            }
        }
        else
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
    if (caps.bLensCorrection)
    {
        list.push_back(MFX_EXTBUF_CAM_LENS_GEOM_DIST_CORRECTION);
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
        list.push_back(MFX_EXTBUFF_VPP_DENOISE);
    }
    if(caps.bNoPadding)
    {
        list.push_back(MFX_EXTBUF_CAM_PADDING);
    }
    if(caps.b3DLUT)
    {
        list.push_back(MFX_EXTBUF_CAM_3DLUT);
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
            else
            {
                gammaBuf->Mode = gammaBuf->NumPoints = (mfxU16)action;
                gammaBuf->GammaValue = (mfxF64)action;
            }
        }
        break;
    case MFX_EXTBUF_CAM_HOT_PIXEL_REMOVAL:
        {
            mfxExtCamHotPixelRemoval *hotPelBuf = (mfxExtCamHotPixelRemoval *)extBuf;
            if (action >= MFX_CAM_QUERY_CHECK_RANGE)
            {
                //sts = HotPixelRemovalCheckParam(hotPelBuf);
            }
            else
            {
                hotPelBuf->PixelCountThreshold = hotPelBuf->PixelThresholdDifference = (mfxU16)action;
            }
        }
        break;
    case MFX_EXTBUF_CAM_VIGNETTE_CORRECTION:
        {
            mfxExtCamVignetteCorrection *vignetteBuf = (mfxExtCamVignetteCorrection *)extBuf;
            if (action >= MFX_CAM_QUERY_CHECK_RANGE)
            {
                //sts = VignetteCorrectionCheckParam(vignetteBuf);
            }
            else
            {
                vignetteBuf->Height = vignetteBuf->Width = vignetteBuf->Pitch = action;
            }
        }
        break;
    case MFX_EXTBUF_CAM_WHITE_BALANCE:
        {
            mfxExtCamWhiteBalance *wbBuf = (mfxExtCamWhiteBalance *)extBuf;
            if (action >= MFX_CAM_QUERY_CHECK_RANGE)
            {
                //sts = WhiteBalanceCheckParam(wbBuf);
            }
            else
            {
                wbBuf->R = wbBuf->G0 = wbBuf->B = wbBuf->G1 =  (mfxF64)action;
                wbBuf->Mode = action;
            }
        }
        break;
    case MFX_EXTBUF_CAM_BLACK_LEVEL_CORRECTION:
        {
            mfxExtCamBlackLevelCorrection *blackLevelBuf = (mfxExtCamBlackLevelCorrection *)extBuf;
            if (action >= MFX_CAM_QUERY_CHECK_RANGE)
            {
                //sts = BlackLevelCheckParam(blackLevelBuf);
            }
            else
            {
                blackLevelBuf->R = blackLevelBuf->G0 = blackLevelBuf->B = blackLevelBuf->G1 =  (mfxU16)action;
            }
        }
        break;
    case MFX_EXTBUF_CAM_COLOR_CORRECTION_3X3:
        {
            mfxExtCamColorCorrection3x3 *cc3x3Buf = (mfxExtCamColorCorrection3x3 *)extBuf;
            if (action >= MFX_CAM_QUERY_CHECK_RANGE)
            {
                //sts = ColorCorrection3x3CheckParam(cc3x3Buf);
            }
            else
            {
                mfxU32 i, j;
                for (i = 0; i < 3; i++)
                    for (j = 0; j < 3; j++)
                        cc3x3Buf->CCM[i][j] = (mfxF64)action;
            }
        }
        break;
    case MFX_EXTBUF_CAM_LENS_GEOM_DIST_CORRECTION:
        {
            mfxExtCamLensGeomDistCorrection *lensBuf = (mfxExtCamLensGeomDistCorrection *)extBuf;
            if (action >= MFX_CAM_QUERY_CHECK_RANGE)
            {
            }
            else
            {
                mfxU32 i;
                for (i = 0; i < 3; i++)
                {
                    lensBuf->a[i] = (mfxF32)action;
                    lensBuf->b[i] = (mfxF32)action;
                    lensBuf->c[i] = (mfxF32)action;
                    lensBuf->d[i] = (mfxF32)action;
                }
            }
        }
        break;
    case MFX_EXTBUF_CAM_BAYER_DENOISE:
    case MFX_EXTBUFF_VPP_DENOISE:
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
            if (action >= MFX_CAM_QUERY_CHECK_RANGE)
            {
                //sts = PaddingCheckParam(padBuf);
            }
            else
            {
                padBuf->Top = padBuf->Bottom = padBuf->Left = padBuf->Right = (mfxU16)action;
            }
        }
        break;
    case MFX_EXTBUF_CAM_PIPECONTROL:
        {
            mfxExtCamPipeControl *pipeBuf = (mfxExtCamPipeControl *)extBuf;
            if (action >= MFX_CAM_QUERY_CHECK_RANGE)
            {
                //sts = PaddingCheckParam(padBuf);
            }
            else
            {
                pipeBuf->RawFormat = (mfxU16)action;
            }
        }
        break;
     case MFX_EXTBUF_CAM_3DLUT:
        {
            mfxExtCam3DLut *pipeBuf = (mfxExtCam3DLut *)extBuf;
            MFX_CHECK_NULL_PTR1(pipeBuf);
            if (action >= MFX_CAM_QUERY_CHECK_RANGE)
            {
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

   if(in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY && in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
   {
        out->IOPattern = 0;
        return error_status;
   }

   if(in->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY && in->vpp.In.FourCC != MFX_FOURCC_ARGB16 && in->vpp.In.FourCC != MFX_FOURCC_ABGR16)
   {
       // Video input is supported for 16bit RGB 
       out->IOPattern = 0;
       return error_status;
   }

   if(in->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
   {
        out->IOPattern = 0;
        return error_status;
   }

   if(!((in->IOPattern & 0x0f) && (in->IOPattern & 0xf0)))
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
        if (!IsCameraFilterFound(g_TABLE_CAMERA_EXTBUFS, searchCount, pExtList[i]))
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
        if (!IsCameraFilterFound(g_TABLE_CAMERA_EXTBUFS, searchCount, curId))
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
        if (!IsCameraFilterFound(&capsList[0], (mfxU32)capsList.size(), pExtList[i]))
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
        if (!IsCameraFilterFound(&capsList[0], (mfxU32)capsList.size(), curId))
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
        else if (IsCameraFilterFound(&pipelineList[0], (mfxU32)pipelineList.size(), curId))
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
