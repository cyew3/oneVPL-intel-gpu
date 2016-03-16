/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2015 Intel Corporation. All Rights Reserved.
//
*/

#include <limits>
#include "mfx_vp9_dec_decode.h"

#include "mfx_common.h"
#include "mfx_common_decode_int.h"
#include "mfx_enc_common.h"
#include "ipps.h"

void MoveBitstreamData2_VP9(mfxBitstream& bs, mfxU32 offset)
{
    VM_ASSERT(offset <= bs.DataLength);
    bs.DataOffset += offset;
    bs.DataLength -= offset;
}

#if defined(MFX_ENABLE_VP9_VIDEO_DECODE) || defined(MFX_ENABLE_VP9_VIDEO_DECODE_HW)

#include "umc_vp9_utils.h"
#include "umc_vp9_bitstream.h"

using namespace UMC_VP9_DECODER;

#endif

#ifdef MFX_ENABLE_VP9_VIDEO_DECODE

#include "umc_data_pointers_copy.h"

#include "vm_sys_info.h"
#include "ippcore.h"

#include "ippcc.h"

#include "mfx_thread_task.h"

#pragma warning(disable : 4505)

#include "vpx/vpx_codec.h"
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"

static mfxStatus vpx_convert_status(mfxI32 status)
{
    switch (status)
    {
        case VPX_CODEC_OK:
            return MFX_ERR_NONE;
        case VPX_CODEC_ERROR:
            return MFX_ERR_UNKNOWN;
        case VPX_CODEC_MEM_ERROR:
            return MFX_ERR_MEMORY_ALLOC;
        case VPX_CODEC_INVALID_PARAM:
            return MFX_ERR_INVALID_VIDEO_PARAM;
        case VPX_CODEC_ABI_MISMATCH:
        case VPX_CODEC_INCAPABLE:
        case VPX_CODEC_UNSUP_BITSTREAM:
        case VPX_CODEC_UNSUP_FEATURE:
        case VPX_CODEC_CORRUPT_FRAME:
        default:
            return MFX_ERR_UNSUPPORTED;
    }
}

#define CHECK_VPX_STATUS(status) \
    if (VPX_CODEC_OK != status) \
        return vpx_convert_status(status);

static mfxStatus Convert_YV12_to_NV12(mfxFrameData* inData,  mfxFrameInfo* inInfo,
                       mfxFrameData* outData, mfxFrameInfo* outInfo)
{
    MFX_CHECK_NULL_PTR2(inData, inInfo);
    MFX_CHECK_NULL_PTR2(outData, outInfo);

    IppiSize roiSize;

    mfxU32  inOffset0 = 0, inOffset1 = 0;
    mfxU32  outOffset0 = 0, outOffset1 = 0;

    roiSize.width = inInfo->CropW;
    if ((roiSize.width == 0) || (roiSize.width > inInfo->Width && inInfo->Width > 0))
        roiSize.width = inInfo->Width;

    roiSize.height = inInfo->CropH;
    if ((roiSize.height == 0) || (roiSize.height > inInfo->Height && inInfo->Height > 0))
        roiSize.height = inInfo->Height;

    inOffset0  = inInfo->CropX        + inInfo->CropY*inData->Pitch;
    inOffset1  = (inInfo->CropX >> 1) + (inInfo->CropY >> 1)*(inData->Pitch >> 1);

    outOffset0   = outInfo->CropX        + outInfo->CropY*outData->Pitch;
    outOffset1   = (outInfo->CropX) + (outInfo->CropY >> 1)*(outData->Pitch);

    const mfxU8* pSrc[3] = {(mfxU8*)inData->Y + inOffset0,
                          (mfxU8*)inData->V + inOffset1,
                          (mfxU8*)inData->U + inOffset1};
    /* [U<->V] because some reversing will be done ipp function */

    mfxI32 pSrcStep[3] = {inData->Pitch,
                        inData->Pitch >> 1,
                        inData->Pitch >> 1};

    mfxU8* pDst[2]   = {(mfxU8*)outData->Y + outOffset0,
                      (mfxU8*)outData->UV+ outOffset1};

    mfxI32 pDstStep[2] = {outData->Pitch,
                        outData->Pitch >> 0};

    IppStatus sts = ippiYCrCb420ToYCbCr420_8u_P3P2R(pSrc, pSrcStep,
                                                    pDst[0], pDstStep[0],
                                                    pDst[1], pDstStep[1],
                                                    roiSize);
    if (sts != ippStsNoErr)
        return MFX_ERR_UNKNOWN;

    return MFX_ERR_NONE;
}

VideoDECODEVP9::VideoDECODEVP9(VideoCORE *core, mfxStatus *sts)
    :VideoDECODE()
    ,m_core(core)
    ,m_isInit(false)
    ,m_is_opaque_memory(false)
    ,m_platform(MFX_PLATFORM_SOFTWARE)
    ,m_num_output_frames(0)
    ,m_in_framerate(0)
{
    memset(&m_bs.Data, 0, sizeof(m_bs.Data));

    // allocate vpx decoder
    m_vpx_codec = ippMalloc(sizeof(vpx_codec_ctx_t));

    if (!m_vpx_codec)
    {
        *sts = MFX_ERR_NOT_INITIALIZED;
    }

    if (sts)
    {
        *sts = MFX_ERR_NONE;
    }
}

VideoDECODEVP9::~VideoDECODEVP9(void)
{
    Close();
}

mfxStatus VideoDECODEVP9::Init(mfxVideoParam *params)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_isInit)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    MFX_CHECK_NULL_PTR1(params);

    m_platform = MFX_PLATFORM_SOFTWARE;
    eMFXHWType type = MFX_HW_UNKNOWN;

    if (MFX_ERR_NONE > CheckVideoParamDecoders(params, m_core->IsExternalFrameAllocator(), type))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!MFX_VP9_Utility::CheckVideoParam(params, type))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    m_vInitPar = *params;

    if (0 == m_vInitPar.mfx.FrameInfo.FrameRateExtN || 0 == m_vInitPar.mfx.FrameInfo.FrameRateExtD)
    {
        m_vInitPar.mfx.FrameInfo.FrameRateExtD = 1000;
        m_vInitPar.mfx.FrameInfo.FrameRateExtN = 30000;
    }

    m_in_framerate = (mfxF64) m_vInitPar.mfx.FrameInfo.FrameRateExtD / m_vInitPar.mfx.FrameInfo.FrameRateExtN;

    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&m_vInitPar);

    if (0 == m_vInitPar.mfx.NumThread)
    {
        m_vInitPar.mfx.NumThread = (mfxU16)vm_sys_info_get_cpu_num();
    }

    m_FrameAllocator.reset(new mfx_UMC_FrameAllocator);

    Ipp32s useInternal = (m_vInitPar.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) || (m_vInitPar.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY);

    // allocate memory
    memset(&m_request, 0, sizeof(m_request));
    memset(&m_response, 0, sizeof(m_response));

    sts = QueryIOSurfInternal(m_platform, &m_vInitPar, &m_request);
    MFX_CHECK_STS(sts);

    if (useInternal)
    {
        m_request.Type = MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_SYSTEM_MEMORY;

        mfxExtOpaqueSurfaceAlloc *p_opq_ext = NULL;
        if (MFX_IOPATTERN_OUT_OPAQUE_MEMORY & params->IOPattern) // opaque memory case
        {
            p_opq_ext = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(params->ExtParam, params->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

            if (NULL != p_opq_ext)
            {
                if (m_request.NumFrameMin > p_opq_ext->Out.NumSurface)
                {
                    return MFX_ERR_INVALID_VIDEO_PARAM;
                }

                m_is_opaque_memory = true;

                if (MFX_MEMTYPE_FROM_DECODE & p_opq_ext->Out.Type)
                {
                    if (MFX_MEMTYPE_SYSTEM_MEMORY & p_opq_ext->Out.Type)
                    {
                        // map surfaces with opaque
                        m_request.Type = (mfxU16)p_opq_ext->Out.Type;
                        m_request.NumFrameMin = m_request.NumFrameSuggested = (mfxU16)p_opq_ext->Out.NumSurface;

                    }
                    else
                    {
                        m_request.Type = MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_FROM_DECODE;
                        m_request.Type |= MFX_MEMTYPE_DXVA2_DECODER_TARGET;
                        m_request.NumFrameMin = m_request.NumFrameSuggested = (mfxU16)p_opq_ext->Out.NumSurface;
                    }
                }
                else
                {
                    mfxStatus sts;
                    mfxFrameAllocRequest trequest = m_request;
                    trequest.Type =  (mfxU16)p_opq_ext->Out.Type;
                    trequest.NumFrameMin = trequest.NumFrameSuggested = (mfxU16)p_opq_ext->Out.NumSurface;

                    trequest.AllocId = params->AllocId;

                    sts = m_core->AllocFrames(&trequest,
                                                &m_opaque_response,
                                                p_opq_ext->In.Surfaces,
                                                p_opq_ext->In.NumSurface);

                    if (MFX_ERR_NONE != sts && MFX_ERR_UNSUPPORTED != sts)
                    {
                        // unsupported means that current core couldn;t allocate the surfaces
                        return sts;
                    }
                }
            }
        }

        if (true == m_is_opaque_memory)
        {
            sts  = m_core->AllocFrames(&m_request,
                                         &m_response,
                                         p_opq_ext->Out.Surfaces,
                                         p_opq_ext->Out.NumSurface);
        }
        else
        {
            m_request.AllocId = params->AllocId;

            sts = m_core->AllocFrames(&m_request, &m_response);
            MFX_CHECK_STS(sts);
        }

        if (sts != MFX_ERR_NONE && sts != MFX_ERR_NOT_FOUND)
        {
            // second status means that external allocator was not found, it is ok
            return sts;
        }

        UMC::Status umcSts = UMC::UMC_OK;

        // no need to save surface descriptors if all memory is d3d
        if (params->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
        {
            umcSts = m_FrameAllocator->InitMfx(NULL, m_core, params, &m_request, &m_response, false, true);
        }
        else
        {
             // means that memory is d3d9 surfaces
            m_FrameAllocator->SetExternalFramesResponse(&m_response);
            umcSts = m_FrameAllocator->InitMfx(NULL, m_core, params, &m_request, &m_response, true, true);
        }

        if (UMC::UMC_OK != umcSts)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }
    }
    else
    {
        m_request.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_SYSTEM_MEMORY;

        UMC::Status umcSts = m_FrameAllocator->InitMfx(0, m_core, params, &m_request, &m_response, !useInternal, MFX_PLATFORM_SOFTWARE == m_platform);
        if (UMC::UMC_OK != umcSts)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }
    }

    vpx_codec_dec_cfg_t cfg;
    cfg.threads = m_vInitPar.mfx.NumThread;

    mfxI32 vpx_sts = vpx_codec_dec_init((vpx_codec_ctx_t *)m_vpx_codec, vpx_codec_vp9_dx(), &cfg, 0);
    CHECK_VPX_STATUS(vpx_sts);

    m_isInit = true;

    if (true == isNeedChangeVideoParamWarning)
    {
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::Reset(mfxVideoParam *params)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(params);

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (m_platform == MFX_PLATFORM_HARDWARE)
    {
        type = m_core->GetHWType();
    }

    if (CheckVideoParamDecoders(params, m_core->IsExternalFrameAllocator(), type) < MFX_ERR_NONE)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (false == MFX_VP9_Utility::CheckVideoParam(params, type))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (false == IsSameVideoParam(params, &m_vInitPar))
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

     mfxExtOpaqueSurfaceAlloc *p_opq_ext = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(params->ExtParam, params->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

    if (NULL != p_opq_ext)
    {
        if (false == m_is_opaque_memory)
        {
            // decoder was not initialized with opaque extended buffer
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }

        if (m_request.NumFrameMin != p_opq_ext->Out.NumSurface)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    // need to sw acceleration
    if (m_platform != m_core->GetPlatformType())
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (m_FrameAllocator->Reset() != UMC::UMC_OK)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    memset(&m_stat, 0, sizeof(m_stat));
    m_vInitPar = *params;

    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&m_vInitPar);
    m_vInitPar = m_vInitPar;

    if (isNeedChangeVideoParamWarning)
    {
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::Close(void)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    m_FrameAllocator->Close();

    if (0 != m_response.NumFrameActual)
    {
        m_core->FreeFrames(&m_response);
    }

    m_isInit = false;
    m_is_opaque_memory = false;

    memset(&m_stat, 0, sizeof(m_stat));

    vpx_codec_destroy((vpx_codec_ctx_t *)m_vpx_codec);

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::DecodeHeader(VideoCORE * core, mfxBitstream *bs, mfxVideoParam *params)
{
    return MFX_VP9_Utility::DecodeHeader(core, bs, params);
}

bool VideoDECODEVP9::IsSameVideoParam(mfxVideoParam *newPar, mfxVideoParam *oldPar)
{
    if (newPar->IOPattern != oldPar->IOPattern)
    {
        return false;
    }

    if (newPar->Protected != oldPar->Protected)
    {
        return false;
    }

    Ipp32s asyncDepth = IPP_MIN(newPar->AsyncDepth, MFX_MAX_ASYNC_DEPTH_VALUE);
    if (asyncDepth != oldPar->AsyncDepth)
    {
        return false;
    }

    if (newPar->mfx.FrameInfo.Height != oldPar->mfx.FrameInfo.Height)
    {
        return false;
    }

    if (newPar->mfx.FrameInfo.Width != oldPar->mfx.FrameInfo.Width)
    {
        return false;
    }

    if (newPar->mfx.FrameInfo.ChromaFormat != oldPar->mfx.FrameInfo.ChromaFormat)
    {
        return false;
    }

    if (newPar->mfx.NumThread > oldPar->mfx.NumThread && oldPar->mfx.NumThread > 0)
    {
        return false;
    }

    return true;
}

mfxTaskThreadingPolicy VideoDECODEVP9::GetThreadingPolicy(void)
{
    return MFX_TASK_THREADING_INTRA;
}

mfxStatus VideoDECODEVP9::Query(VideoCORE *core, mfxVideoParam *p_in, mfxVideoParam *p_out)
{
    MFX_CHECK_NULL_PTR1(p_out);

    eMFXHWType type = core->GetHWType();
    return MFX_VP9_Utility::Query(core, p_in, p_out, type);
}

mfxStatus VideoDECODEVP9::QueryIOSurfInternal(eMFXPlatform platform, mfxVideoParam *p_params, mfxFrameAllocRequest *request)
{
    MFX_INTERNAL_CPY(&request->Info, &p_params->mfx.FrameInfo, sizeof(mfxFrameInfo));

    mfxU32 threads = p_params->mfx.NumThread;

    if (!threads)
    {
        threads = vm_sys_info_get_cpu_num();
    }

    if (platform != MFX_PLATFORM_SOFTWARE)
    {
        threads = 1;
    }

    if (p_params->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
    {
        request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
    }
    else if (p_params->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }
    else // opaque memory case
    {
        if (MFX_PLATFORM_SOFTWARE == platform)
        {
            request->Type = MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
        }
        else
        {
            request->Type = MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
        }
    }

    request->NumFrameMin = mfxU16 (4);

    request->NumFrameMin += p_params->AsyncDepth ? p_params->AsyncDepth : MFX_AUTO_ASYNC_DEPTH_VALUE;
    request->NumFrameSuggested = request->NumFrameMin;

    if (MFX_PLATFORM_SOFTWARE == platform)
    {
        request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }
    else
    {
        request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::QueryIOSurf(VideoCORE *core, mfxVideoParam *p_params, mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR2(p_params, request);

    if (!(p_params->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) &&
        !(p_params->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) &&
        !(p_params->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if ((p_params->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) &&
        (p_params->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if ((p_params->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) &&
        (p_params->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if ((p_params->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) &&
        (p_params->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    eMFXPlatform platform = MFX_VP9_Utility::GetPlatform(core, p_params);

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (platform == MFX_PLATFORM_HARDWARE)
    {
        type = core->GetHWType();
    }

    mfxVideoParam params;
    MFX_INTERNAL_CPY(&params, p_params, sizeof(mfxVideoParam));
    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&params);

    mfxStatus sts = QueryIOSurfInternal(platform, &params, request);
    MFX_CHECK_STS(sts);

    Ipp32s isInternalManaging = (MFX_PLATFORM_SOFTWARE == platform) ?
        (params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) : (params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

    if (isInternalManaging)
    {
        request->NumFrameSuggested = request->NumFrameMin = 4;
    }

    if (params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }
    else
    {
        request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
    }

    if (platform != core->GetPlatformType())
    {
        VM_ASSERT(platform == MFX_PLATFORM_SOFTWARE);
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    if (true == isNeedChangeVideoParamWarning)
    {
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::GetVideoParam(mfxVideoParam *par)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par);

    par->mfx = m_vInitPar.mfx;

    par->Protected = m_vInitPar.Protected;
    par->IOPattern = m_vInitPar.IOPattern;
    par->AsyncDepth = m_vInitPar.AsyncDepth;

    par->mfx.FrameInfo.FrameRateExtD = m_vInitPar.mfx.FrameInfo.FrameRateExtD;
    par->mfx.FrameInfo.FrameRateExtN = m_vInitPar.mfx.FrameInfo.FrameRateExtN;

    par->mfx.FrameInfo.AspectRatioH = m_vInitPar.mfx.FrameInfo.AspectRatioH;
    par->mfx.FrameInfo.AspectRatioW = m_vInitPar.mfx.FrameInfo.AspectRatioW;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::GetDecodeStat(mfxDecodeStat *stat)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(stat);

    m_stat.NumSkippedFrame = 0;
    m_stat.NumCachedFrame = 0;

    MFX_INTERNAL_CPY(stat, &m_stat, sizeof(m_stat));

    return MFX_ERR_NONE;
}

mfxFrameSurface1 * VideoDECODEVP9::GetOriginalSurface(mfxFrameSurface1 *p_surface)
{
    if (true == m_is_opaque_memory)
    {
        return m_core->GetNativeSurface(p_surface);
    }

    return p_surface;
}

mfxStatus VideoDECODEVP9::GetOutputSurface(mfxFrameSurface1 **surface_out, mfxFrameSurface1 *surface_work, UMC::FrameMemID index)
{
    mfxFrameSurface1 *native_surface =  m_FrameAllocator->GetSurface(index, surface_work, &m_vInitPar);

    if (native_surface)
    {
        *surface_out = m_core->GetOpaqSurface(native_surface->Data.MemId) ? m_core->GetOpaqSurface(native_surface->Data.MemId) : native_surface;
    }
    else
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::GetUserData(mfxU8 *ud, mfxU32 *sz, mfxU64 *ts)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR3(ud, sz, ts);

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VideoDECODEVP9::GetPayload(mfxU64 *ts, mfxPayload *payload)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;


    MFX_CHECK_NULL_PTR3(ts, payload, payload->Data);

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VideoDECODEVP9::SetSkipMode(mfxSkipMode mode)
{
    mode;

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::ReadFrameInfo(mfxU8 *pData, mfxU32 size, VP9BaseFrameInfo *out)
{
    if (!pData || !size)
        return MFX_ERR_MORE_DATA;

    if (!out)
        return MFX_ERR_NULL_PTR;

    mfxU32 frameSizes[8] = { 0 };
    mfxU32 frameCount = 0;

    ParseSuperFrameIndex(pData, size, frameSizes, &frameCount);

    if (0 == frameCount)
    {
        frameCount = 1;
        frameSizes[0] = size;
    }

    if (frameCount > 0)
    {
        for (mfxU32 i = 0; i < frameCount; ++i)
        {
            const mfxU32 frameSize = frameSizes[i];

            VP9Bitstream bs(pData, frameSize);

            if (VP9_FRAME_MARKER != bs.GetBits(2))
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            out->Version = (mfxU16)bs.GetBit();
            if (out->Version > 1)
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            bs.GetBit(); // skip unused version bit

            bs.GetBit(); // show_existing_frame

            out->FrameType = (VP9_FRAME_TYPE) bs.GetBit();

            out->ShowFrame = (mfxU16)bs.GetBit();

            bs.GetBit(); // error_resilient_mode

            if (KEY_FRAME == out->FrameType)
            {
                if (0x49 != bs.GetBits(8) || 0x83 != bs.GetBits(8) || 0x42 != bs.GetBits(8))
                    return MFX_ERR_UNDEFINED_BEHAVIOR;

                COLOR_SPACE colorSpace = (COLOR_SPACE) bs.GetBits(3);
                if (SRGB != colorSpace)
                {
                    bs.GetBit();
                    if (1 == out->Version)
                        bs.GetBits(3);
                }
                else
                {
                    if (1 == out->Version)
                        bs.GetBit();
                    else
                        return MFX_ERR_UNDEFINED_BEHAVIOR;
                }
            }
            pData += frameSize;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::PreDecodeFrame(mfxBitstream *bs, mfxFrameSurface1 *surface_work)
{
    vpx_codec_stream_info_t si;
    memset(&si, 0, sizeof(vpx_codec_stream_info_t));
    si.sz = sizeof(vpx_codec_stream_info_t);
    mfxI32 vpx_sts = vpx_codec_peek_stream_info(vpx_codec_vp9_dx(), bs->Data + bs->DataOffset, bs->DataLength, &si);
    CHECK_VPX_STATUS(vpx_sts);

    mfxFrameSurface1 *p_surface = surface_work;

    if (true == m_is_opaque_memory)
    {
        p_surface = m_core->GetOpaqSurface(surface_work->Data.MemId, true);
    }

    if (si.w && si.h)
    {
        p_surface->Info.CropW = (mfxU16)si.w;
        p_surface->Info.CropH = (mfxU16)si.h;

        si.w = (si.w + 15) & ~0x0f;
        si.h = (si.h + 15) & ~0x0f;

        if (m_vInitPar.mfx.FrameInfo.Width < si.w && m_vInitPar.mfx.FrameInfo.Height < si.h)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    return MFX_ERR_NONE;
}

static mfxStatus VP9CompleteProc(void *, void *p_param, mfxStatus )
{
    delete p_param;
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::RunThread(void *p_params, mfxU32 thread_number)
{
    p_params; thread_number;

    return MFX_TASK_DONE;
}

static mfxStatus __CDECL VP9DECODERoutine(void *state, void *param, mfxU32 /*thread_number*/, mfxU32)
{
    mfxStatus mfxSts = MFX_ERR_NONE;

    THREAD_TASK_INFO_VP9 *thread_info = (THREAD_TASK_INFO_VP9 *) param;

    vpx_codec_ctx_t *decoder = (vpx_codec_ctx_t *)state;

    mfxI32 vpx_sts = 0;
    vpx_sts = vpx_codec_decode(decoder, thread_info->m_p_bitstream, thread_info->m_frame.frame_size, NULL, 0);
    CHECK_VPX_STATUS(vpx_sts);

    vpx_codec_iter_t iter = NULL;
    vpx_image_t *img = vpx_codec_get_frame(decoder, &iter);

    if (img && thread_info->m_p_surface_out)
    {
        mfxFrameData inData = { 0 };
        inData.Y = img->planes[VPX_PLANE_Y];
        inData.U = img->planes[VPX_PLANE_U];
        inData.V = img->planes[VPX_PLANE_V];
        inData.Pitch = (mfxU16)img->stride[VPX_PLANE_Y];

        mfxFrameInfo inInfo = { 0 };
        inInfo.Width  = (mfxU16)img->w;
        inInfo.Height = (mfxU16)img->h;
        inInfo.CropW  = (mfxU16)img->d_w;
        inInfo.CropH  = (mfxU16)img->d_h;

        mfxFrameData outData = { 0 };
        outData.Y  = (mfxU8*)thread_info->m_p_video_data->GetPlanePointer(0);
        outData.UV = (mfxU8*)thread_info->m_p_video_data->GetPlanePointer(1);
        outData.Pitch = (mfxU16)thread_info->m_p_video_data->GetPlanePitch(0);

        mfxSts = Convert_YV12_to_NV12(&inData, &inInfo, &outData, &thread_info->m_p_surface_out->Info);

        if (MFX_ERR_NONE == mfxSts)
        {
            thread_info->m_p_mfx_umc_frame_allocator->PrepareToOutput(thread_info->m_p_surface_out, thread_info->m_memId, &thread_info->m_video_params, false);
            thread_info->m_p_mfx_umc_frame_allocator->DecreaseReference(thread_info->m_memId);
            thread_info->m_p_mfx_umc_frame_allocator->Unlock(thread_info->m_memId);
        }
    }

    delete [] thread_info->m_p_bitstream;
    delete thread_info->m_p_video_data;

    return mfxSts;
}

mfxStatus VideoDECODEVP9::DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, MFX_ENTRY_POINT *p_entry_point)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR2(surface_work, surface_out);

    if (0 != surface_work->Data.Locked)
    {
        return MFX_ERR_MORE_SURFACE;
    }

    if (true == m_is_opaque_memory)
    {
        if (surface_work->Data.MemId || surface_work->Data.Y || surface_work->Data.R || surface_work->Data.A || surface_work->Data.UV) // opaq surface
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        surface_work = GetOriginalSurface(surface_work);
    }

    sts = CheckFrameInfoCodecs(&surface_work->Info, MFX_CODEC_VP9, false);
    MFX_CHECK_STS(sts);

    sts = CheckFrameData(surface_work);
    MFX_CHECK_STS(sts);

    sts = bs ? CheckBitstream(bs) : MFX_ERR_NONE;
    MFX_CHECK_STS(sts);

    if (NULL == bs)
    {
        return MFX_ERR_MORE_DATA;
    }

    if (0 == bs->DataLength)
    {
        return MFX_ERR_MORE_DATA;
    }

    memset(&m_frameInfo, 0, sizeof(m_frameInfo));
    sts = ReadFrameInfo(bs->Data + bs->DataOffset, bs->DataLength, &m_frameInfo);
    MFX_CHECK_STS(sts);

    if (bs)
    {
        sts = PreDecodeFrame(bs, surface_work);
        MFX_CHECK_STS(sts);
    }

    IVF_FRAME_VP9 frame;
    memset(&frame, 0, sizeof(IVF_FRAME_VP9));

    sts = ConstructFrame(bs, &m_bs, frame);
    MFX_CHECK_STS(sts);

    *surface_out = 0;

    sts = m_FrameAllocator->SetCurrentMFXSurface(surface_work, m_is_opaque_memory);
    MFX_CHECK_STS(sts);

    UMC::VideoDataInfo info;
    info.Init(surface_work->Info.Width, surface_work->Info.Height, UMC::YUV420);

    UMC::FrameMemID memId = 0;

    UMC::FrameData *p_frame_data = NULL;
    UMC::VideoData *video_data = new UMC::VideoData;

    if (m_frameInfo.ShowFrame)
    {
        m_FrameAllocator->Alloc(&memId, &info, 0);

        // get to decode frame data
        p_frame_data = (UMC::FrameData *) m_FrameAllocator->Lock(memId);

        if (NULL == p_frame_data)
        {
            delete video_data;
            return MFX_ERR_LOCK_MEMORY;
        }

        m_FrameAllocator->IncreaseReference(memId);

        UMC::Status umcSts = video_data->Init(surface_work->Info.Width, surface_work->Info.Height, UMC::YUV420);

        {
            const UMC::FrameData::PlaneMemoryInfo *p_info;

            p_info = p_frame_data->GetPlaneMemoryInfo(0);

            video_data->SetPlanePointer(p_info->m_planePtr, 0);
            Ipp32u pitch = (Ipp32u) p_info->m_pitch;

            p_info = p_frame_data->GetPlaneMemoryInfo(1);
            video_data->SetPlanePointer(p_info->m_planePtr, 1);

            video_data->SetPlanePitch(pitch, 0);
            video_data->SetPlanePitch(pitch, 1);
        }

        // get output surface
        sts = GetOutputSurface(surface_out, surface_work, memId);
        MFX_CHECK_STS(sts);
            //*surface_out = m_p_frame_allocator->GetSurface(memId, surface_work, &m_video_params);

        SetOutputParams(surface_work);

        (*surface_out)->Data.TimeStamp = bs->TimeStamp;
    }

    THREAD_TASK_INFO_VP9 *p_info = new THREAD_TASK_INFO_VP9;
    MFX_CHECK_NULL_PTR1(p_info);
    p_info->m_p_surface_work = surface_work;
    p_info->m_p_surface_out = GetOriginalSurface(*surface_out);
    p_info->m_p_mfx_umc_frame_allocator = m_FrameAllocator.get();
    p_info->m_memId = memId;
    p_info->m_frame = frame;
    p_info->m_video_params = m_vInitPar;

    p_info->m_p_video_data = video_data;

    p_info->m_p_bitstream = NULL;
    p_info->m_p_bitstream = new mfxU8[m_bs.DataLength];
    MFX_CHECK_NULL_PTR1(p_info->m_p_bitstream);
    MFX_INTERNAL_CPY(p_info->m_p_bitstream, m_bs.Data, m_bs.DataLength);

    p_entry_point->pRoutine = &VP9DECODERoutine;
    p_entry_point->pCompleteProc = &VP9CompleteProc;
    p_entry_point->pState = m_vpx_codec;
    p_entry_point->requiredNumThreads = 1;
    p_entry_point->pParam = p_info;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::ConstructFrame(mfxBitstream *p_in, mfxBitstream *p_out, IVF_FRAME_VP9& frame)
{
    MFX_CHECK_NULL_PTR1(p_out);

    if (0 == p_in->DataLength)
        return MFX_ERR_MORE_DATA;

    mfxU8 *bs_start = p_in->Data + p_in->DataOffset;

    if (p_out->Data)
    {
        ippFree(p_out->Data);
        p_out->DataLength = 0;
    }

    p_out->Data = (Ipp8u*)ippMalloc(p_in->DataLength);

    ippsCopy_8u(bs_start, p_out->Data, p_in->DataLength);
    p_out->DataLength = p_in->DataLength;
    p_out->DataOffset = 0;

    frame.frame_size = p_in->DataLength;

    MoveBitstreamData2_VP9(*p_in, p_in->DataLength);

    return MFX_ERR_NONE;
}

void VideoDECODEVP9::SetOutputParams(mfxFrameSurface1 *surface_work)
{
    mfxFrameSurface1 *surface = surface_work;

    if (true == m_is_opaque_memory)
    {
        surface = m_core->GetOpaqSurface(surface_work->Data.MemId, true);
    }

    surface->Data.FrameOrder = m_num_output_frames;
    m_num_output_frames += 1;

    surface->Info.FrameRateExtD = m_vInitPar.mfx.FrameInfo.FrameRateExtD;
    surface->Info.FrameRateExtN = m_vInitPar.mfx.FrameInfo.FrameRateExtN;

    surface->Info.CropX = 0;
    surface->Info.CropY = 0;

    //surface->Info.CropW = m_frameInfo.Width;
    //surface->Info.CropH = m_frameInfo.Height;

    surface->Info.PicStruct = m_vInitPar.mfx.FrameInfo.PicStruct;

    surface->Info.AspectRatioH = 1;
    surface->Info.AspectRatioW = 1;
}

#endif // MFX_ENABLE_VP9_VIDEO_DECODE

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MFX_VP9_Utility implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(MFX_ENABLE_VP9_VIDEO_DECODE) || defined(MFX_ENABLE_VP9_VIDEO_DECODE_HW)

bool MFX_VP9_Utility::IsNeedPartialAcceleration(mfxVideoParam *params)
{
    if (!params)
        return false;

    if (params->mfx.FrameInfo.Width > 4096 || params->mfx.FrameInfo.Height > 4096)
        return true;

    return false;
}

eMFXPlatform MFX_VP9_Utility::GetPlatform(VideoCORE *core, mfxVideoParam *params)
{
    eMFXPlatform platform = core->GetPlatformType();

    if (params && IsNeedPartialAcceleration(params) && platform != MFX_PLATFORM_SOFTWARE)
    {
        return MFX_PLATFORM_SOFTWARE;
    }

    return platform;
}

mfxStatus MFX_VP9_Utility::Query(VideoCORE *core, mfxVideoParam *p_in, mfxVideoParam *p_out, eMFXHWType type)
{
    MFX_CHECK_NULL_PTR1(p_out);
    mfxStatus  sts = MFX_ERR_NONE;

    if (p_in == p_out)
    {
        mfxVideoParam in1;
        MFX_INTERNAL_CPY(&in1, p_in, sizeof(mfxVideoParam));
        return Query(core, &in1, p_out, type);
    }

    memset(&p_out->mfx, 0, sizeof(mfxInfoMFX));

    if (p_in)
    {
        if (p_in->mfx.CodecId == MFX_CODEC_VP9)
            p_out->mfx.CodecId = p_in->mfx.CodecId;

        switch (p_in->mfx.CodecLevel)
        {
            case MFX_LEVEL_UNKNOWN:
                p_out->mfx.CodecLevel = p_in->mfx.CodecLevel;
                break;
        }

        if (p_in->mfx.NumThread < 128)
            p_out->mfx.NumThread = p_in->mfx.NumThread;

        if (p_in->AsyncDepth < MFX_MAX_ASYNC_DEPTH_VALUE) // Actually AsyncDepth > 5-7 is for debugging only.
            p_out->AsyncDepth = p_in->AsyncDepth;

        if ((p_in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) || (p_in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        {
            if ( !((p_in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (p_in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)) )
                p_out->IOPattern = p_in->IOPattern;
        }

        if (p_in->mfx.FrameInfo.FourCC)
        {
            // mfxFrameInfo
            if (p_in->mfx.FrameInfo.FourCC == MFX_FOURCC_NV12 ||
                p_in->mfx.FrameInfo.FourCC == MFX_FOURCC_P010)
                p_out->mfx.FrameInfo.FourCC = p_in->mfx.FrameInfo.FourCC;
            else
                sts = MFX_ERR_UNSUPPORTED;
        }

        switch(p_in->mfx.FrameInfo.ChromaFormat)
        {
            case MFX_CHROMAFORMAT_YUV420:
                p_out->mfx.FrameInfo.ChromaFormat = p_in->mfx.FrameInfo.ChromaFormat;
                break;
            default:
                if (p_in->mfx.FrameInfo.FourCC)
                    sts = MFX_ERR_UNSUPPORTED;

                break;
        }

        if (!p_in->mfx.FrameInfo.ChromaFormat && !(!p_in->mfx.FrameInfo.FourCC && !p_in->mfx.FrameInfo.ChromaFormat))
            sts = MFX_ERR_UNSUPPORTED;

        if (p_in->mfx.FrameInfo.Width % 16 == 0 && p_in->mfx.FrameInfo.Width <= 4096)
            p_out->mfx.FrameInfo.Width = p_in->mfx.FrameInfo.Width;
        else
            sts = MFX_ERR_UNSUPPORTED;

        if (p_in->mfx.FrameInfo.Height % 16 == 0 && p_in->mfx.FrameInfo.Height <= 2304)
            p_out->mfx.FrameInfo.Height = p_in->mfx.FrameInfo.Height;
        else
            sts = MFX_ERR_UNSUPPORTED;

        if (p_in->mfx.FrameInfo.CropX <= p_out->mfx.FrameInfo.Width)
            p_out->mfx.FrameInfo.CropX = p_in->mfx.FrameInfo.CropX;

        if (p_in->mfx.FrameInfo.CropY <= p_out->mfx.FrameInfo.Height)
            p_out->mfx.FrameInfo.CropY = p_in->mfx.FrameInfo.CropY;

        if (p_out->mfx.FrameInfo.CropX + p_in->mfx.FrameInfo.CropW <= p_out->mfx.FrameInfo.Width)
            p_out->mfx.FrameInfo.CropW = p_in->mfx.FrameInfo.CropW;

        if (p_out->mfx.FrameInfo.CropY + p_in->mfx.FrameInfo.CropH <= p_out->mfx.FrameInfo.Height)
            p_out->mfx.FrameInfo.CropH = p_in->mfx.FrameInfo.CropH;

        p_out->mfx.FrameInfo.FrameRateExtN = p_in->mfx.FrameInfo.FrameRateExtN;
        p_out->mfx.FrameInfo.FrameRateExtD = p_in->mfx.FrameInfo.FrameRateExtD;

        p_out->mfx.FrameInfo.AspectRatioW = p_in->mfx.FrameInfo.AspectRatioW;
        p_out->mfx.FrameInfo.AspectRatioH = p_in->mfx.FrameInfo.AspectRatioH;

        mfxStatus stsExt = CheckDecodersExtendedBuffers(p_in);
        if (stsExt < MFX_ERR_NONE)
            sts = MFX_ERR_UNSUPPORTED;

        if (p_in->Protected)
        {
            sts = MFX_ERR_UNSUPPORTED;
        }

        mfxExtOpaqueSurfaceAlloc *opaque_in = (mfxExtOpaqueSurfaceAlloc *)GetExtBuffer(p_in->ExtParam, p_in->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        mfxExtOpaqueSurfaceAlloc *opaque_out = (mfxExtOpaqueSurfaceAlloc *)GetExtBuffer(p_out->ExtParam, p_out->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

        if (opaque_in && opaque_out)
        {
            opaque_out->In.Type = opaque_in->In.Type;
            opaque_out->In.NumSurface = opaque_in->In.NumSurface;
            MFX_INTERNAL_CPY(opaque_out->In.Surfaces, opaque_in->In.Surfaces, opaque_in->In.NumSurface);

            opaque_out->Out.Type = opaque_in->Out.Type;
            opaque_out->Out.NumSurface = opaque_in->Out.NumSurface;
            MFX_INTERNAL_CPY(opaque_out->Out.Surfaces, opaque_in->Out.Surfaces, opaque_in->Out.NumSurface);
        }
        else
        {
            if (opaque_out || opaque_in)
            {
                sts = MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }

        if (GetPlatform(core, p_out) != core->GetPlatformType() && sts == MFX_ERR_NONE)
        {
            VM_ASSERT(GetPlatform(core, p_out) == MFX_PLATFORM_SOFTWARE);
            sts = MFX_WRN_PARTIAL_ACCELERATION;
        }
    }
    else
    {
        p_out->mfx.CodecId = MFX_CODEC_VP9;
        p_out->mfx.CodecProfile = 1;
        p_out->mfx.CodecLevel = 1;

        p_out->mfx.NumThread = 1;

        p_out->AsyncDepth = 1;

        // mfxFrameInfo
        p_out->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        p_out->mfx.FrameInfo.Width = 16;
        p_out->mfx.FrameInfo.Height = 16;

        p_out->mfx.FrameInfo.FrameRateExtN = 1;
        p_out->mfx.FrameInfo.FrameRateExtD = 1;

        p_out->mfx.FrameInfo.AspectRatioW = 1;
        p_out->mfx.FrameInfo.AspectRatioH = 1;

        p_out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

        if (type == MFX_HW_UNKNOWN)
        {
            p_out->IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        }
        else
        {
            p_out->IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        }

        mfxExtOpaqueSurfaceAlloc * opaqueOut = (mfxExtOpaqueSurfaceAlloc *)GetExtBuffer(p_out->ExtParam, p_out->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        if (opaqueOut)
        {
        }
    }

    return sts;
}

bool MFX_VP9_Utility::CheckVideoParam(mfxVideoParam *p_in, eMFXHWType )
{
    if (!p_in)
        return false;

    if (p_in->Protected)
       return false;

    if (MFX_CODEC_VP9 != p_in->mfx.CodecId)
        return false;

    if (p_in->mfx.FrameInfo.Width > 4096 || (p_in->mfx.FrameInfo.Width % 16))
        return false;

    if (p_in->mfx.FrameInfo.Height > 4096 || (p_in->mfx.FrameInfo.Height % 16))
        return false;

#if 0
    // ignore Crop paramsameters at Init/Reset stage
    if (in->mfx.FrameInfo.CropX > in->mfx.FrameInfo.Width)
        return false;

    if (in->mfx.FrameInfo.CropY > in->mfx.FrameInfo.Height)
        return false;

    if (in->mfx.FrameInfo.CropX + in->mfx.FrameInfo.CropW > in->mfx.FrameInfo.Width)
        return false;

    if (in->mfx.FrameInfo.CropY + in->mfx.FrameInfo.CropH > in->mfx.FrameInfo.Height)
        return false;
#endif

    if (p_in->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12 &&
        p_in->mfx.FrameInfo.FourCC != MFX_FOURCC_P010)
        return false;

    // both zero or not zero
    if ((p_in->mfx.FrameInfo.AspectRatioW || p_in->mfx.FrameInfo.AspectRatioH) && !(p_in->mfx.FrameInfo.AspectRatioW && p_in->mfx.FrameInfo.AspectRatioH))
        return false;

    switch (p_in->mfx.FrameInfo.PicStruct)
    {
        case MFX_PICSTRUCT_UNKNOWN:
        case MFX_PICSTRUCT_PROGRESSIVE:
            break;

        default:
            return false;
    }

    if (MFX_CHROMAFORMAT_YUV420 != p_in->mfx.FrameInfo.ChromaFormat)
    {
        return false;
    }

    if (!(p_in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && !(p_in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) && !(p_in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        return false;

    if ((p_in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (p_in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) && (p_in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        return false;

    return true;
}

mfxStatus MFX_VP9_Utility::DecodeHeader(VideoCORE* core, mfxBitstream* bs, mfxVideoParam* params)
{
    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR2(bs, params);

    sts = CheckBitstream(bs);
    MFX_CHECK_STS(sts);

    if (bs->DataLength < 3)
    {
        MoveBitstreamData2_VP9(*bs, bs->DataLength);
        return MFX_ERR_MORE_DATA;
    }

    bool bHeaderRead = false;

    mfxU32 profile = 0;
    mfxU16 width = 0;
    mfxU16 height = 0;

    mfxU32 bit_depth = 8;
    for (;;)
    {
        VP9Bitstream bsReader(bs->Data + bs->DataOffset,  bs->DataLength - bs->DataOffset);

        if (VP9_FRAME_MARKER != bsReader.GetBits(2))
            break; // invalid

        profile = bsReader.GetBit();
        profile |= bsReader.GetBit() << 1;
        if (profile > 2)
            profile += bsReader.GetBit();

        if (profile >= 4)
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        if (bsReader.GetBit()) // show_existing_frame
            break;

        VP9_FRAME_TYPE frameType = (VP9_FRAME_TYPE) bsReader.GetBit();
        mfxU32 showFrame = bsReader.GetBit();
        mfxU32 errorResilientMode = bsReader.GetBit();

        if (KEY_FRAME == frameType)
        {
            if (!CheckSyncCode(&bsReader))
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            if (profile >= 2)
            {
                bit_depth = bsReader.GetBit() ? 12 : 10;
            }

            if (SRGB != (COLOR_SPACE)bsReader.GetBits(3)) // color_space
            {
                bsReader.GetBit();
                if (1 == profile || 3 == profile)
                    bsReader.GetBits(3);
            }
            else
            {
                if (1 == profile || 3 == profile)
                    bsReader.GetBit();
                else
                    break; // invalid
            }

            width = (mfxU16)bsReader.GetBits(16) + 1;
            height = (mfxU16)bsReader.GetBits(16) + 1;

            bHeaderRead = true;
        }
        else
        {
            mfxU32 intraOnly = showFrame ? 0 : bsReader.GetBit();
            if (!intraOnly)
                break;

            if (!errorResilientMode)
                bsReader.GetBits(2);

            if (!CheckSyncCode(&bsReader))
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            if (profile >= 2)
                bit_depth = bsReader.GetBit() ? 12 : 10;

            if (profile > 0)
            {
                if (SRGB != (COLOR_SPACE)bsReader.GetBits(3)) // color_space
                {
                    bsReader.GetBit();
                    if (1 == profile || 3 == profile)
                        bsReader.GetBits(3);
                }
                else
                {
                    if (1 == profile || 3 == profile)
                        bsReader.GetBit();
                    else
                        break; // invalid
                }
            }

            bsReader.GetBits(NUM_REF_FRAMES);

            width = (mfxU16)bsReader.GetBits(16) + 1;
            height = (mfxU16)bsReader.GetBits(16) + 1;

            bHeaderRead = true;
        }

        break;
    }

    if (!bHeaderRead)
    {
        MoveBitstreamData2_VP9(*bs, bs->DataLength);
        return MFX_ERR_MORE_DATA;
    }

    params->mfx.CodecProfile = mfxU16(profile + 1);

    params->mfx.FrameInfo.AspectRatioW = 1;
    params->mfx.FrameInfo.AspectRatioH = 1;

    params->mfx.FrameInfo.CropW = width;
    params->mfx.FrameInfo.CropH = height;

    params->mfx.FrameInfo.Width = (params->mfx.FrameInfo.CropW + 15) & ~0x0f;
    params->mfx.FrameInfo.Height = (params->mfx.FrameInfo.CropH + 15) & ~0x0f;

    switch (bit_depth)
    {
        case  8:
            params->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            break;

        case 10:
            params->mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            params->mfx.FrameInfo.BitDepthLuma = 10;
            params->mfx.FrameInfo.BitDepthChroma = 10;
            break;

        case 12:
            params->mfx.FrameInfo.BitDepthLuma = 12;
            params->mfx.FrameInfo.BitDepthChroma = 12;
            params->mfx.FrameInfo.FourCC = 0;
            break;
    }

    params->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

    if (GetPlatform(core, params) != MFX_PLATFORM_SOFTWARE)
    {
        if (params->mfx.FrameInfo.FourCC == MFX_FOURCC_P010 ||
            params->mfx.FrameInfo.FourCC == MFX_FOURCC_P210)
                params->mfx.FrameInfo.Shift = 1;
    }

    return MFX_ERR_NONE;
}

#endif // #if defined(MFX_ENABLE_VP9_VIDEO_DECODE) || defined(MFX_ENABLE_VP9_VIDEO_DECODE_HW)
