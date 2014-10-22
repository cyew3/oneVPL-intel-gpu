/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"

#if defined(MFX_ENABLE_VP9_VIDEO_DECODE_HW) && defined(MFX_VA)

#include "mfx_session.h"
#include "mfx_vp9_dec_decode.h"
#include "mfx_vp9_dec_decode_hw.h"

#include "umc_va_dxva2.h"

#if defined (MFX_VA_LINUX)
  #include "va/va_dec_vp9.h"
#endif

#if defined (_WIN32) || defined (_WIN64)
#pragma warning(disable : 4189)
#pragma warning(disable : 4101)
#endif

#include <iostream>

VideoDECODEVP9_HW::VideoDECODEVP9_HW(VideoCORE *p_core, mfxStatus *sts)
    : m_is_initialized(false),
      m_is_software_buffer(false),
      m_core(p_core),
      m_platform(MFX_PLATFORM_HARDWARE),
      m_va(NULL),
      m_baseQIndex(0),
      m_y_dc_delta_q(0),
      m_uv_dc_delta_q(0),
      m_uv_ac_delta_q(0),
      m_index(0)
{
    memset(&m_frameInfo, 0, sizeof(m_frameInfo));
    m_frameInfo.currFrame = -1;
    m_frameInfo.frameCountInBS = 0;
    m_frameInfo.currFrameInBS = 0;
    memset(&m_frameInfo.ref_frame_map, -1, sizeof(m_frameInfo.ref_frame_map)); // TODO: move to another place

    if (sts)
    {
        *sts = MFX_ERR_NONE;
    }
}

bool VideoDECODEVP9_HW::CheckHardwareSupport(VideoCORE *p_core, mfxVideoParam *p_video_param)
{
    #ifdef MFX_VA_WIN

    if (p_core->IsGuidSupported(sDXVA_Intel_ModeVP9_VLD, p_video_param) != MFX_ERR_NONE)
    {
        return false;
    }

    // todo : VA API alternative ?
    #endif

    return true;
}

mfxStatus VideoDECODEVP9_HW::Init(mfxVideoParam *par)
{
    mfxStatus sts   = MFX_ERR_NONE;
    UMC::Status umcSts   = UMC::UMC_OK;
    eMFXHWType type = m_core->GetHWType();

    m_platform = MFX_VP9_Utility::GetPlatform(m_core, par);

    if (MFX_ERR_NONE > CheckVideoParamDecoders(par, m_core->IsExternalFrameAllocator(), type))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (false == CheckHardwareSupport(m_core, par))
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    if (false == MFX_VP9_Utility::CheckVideoParam(par, type))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    m_FrameAllocator.reset(new mfx_UMC_FrameAllocator_D3D());
    if ( ! m_FrameAllocator.get() )
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    if (MFX_IOPATTERN_OUT_SYSTEM_MEMORY & par->IOPattern)
    {
        m_is_software_buffer = true;
    }

    m_on_init_video_params = *par;
    m_init_w = par->mfx.FrameInfo.Width;
    m_init_h = par->mfx.FrameInfo.Height;

    if (0 == m_on_init_video_params.mfx.FrameInfo.FrameRateExtN || 0 == m_on_init_video_params.mfx.FrameInfo.FrameRateExtD)
    {
        m_on_init_video_params.mfx.FrameInfo.FrameRateExtD = 1000;
        m_on_init_video_params.mfx.FrameInfo.FrameRateExtN = 30000;
    }

    m_in_framerate = (mfxF64) m_on_init_video_params.mfx.FrameInfo.FrameRateExtD / m_on_init_video_params.mfx.FrameInfo.FrameRateExtN;

    m_video_params = m_on_init_video_params;

    // allocate memory
    mfxFrameAllocRequest request;
    memset(&request, 0, sizeof(request));
    memset(&m_response, 0, sizeof(m_response));

    sts = QueryIOSurfInternal(m_platform, &m_video_params, &request);
    MFX_CHECK_STS(sts);

    sts = m_core->AllocFrames(&request, &m_response, false);
    MFX_CHECK_STS(sts);

    sts = m_core->CreateVA(&m_on_init_video_params, &request, &m_response);
    MFX_CHECK_STS(sts);

    m_core->GetVA((mfxHDL*)&m_va, MFX_MEMTYPE_FROM_DECODE);

    bool isUseExternalFrames = (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) != 0;
    if ( isUseExternalFrames )
    {
        m_FrameAllocator->SetExternalFramesResponse(&m_response);
    }

    umcSts = m_FrameAllocator->InitMfx(0, m_core, par, &request, &m_response, isUseExternalFrames, false);
    if (UMC::UMC_OK != umcSts)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    m_frameOrder = 0;
    m_is_initialized = true;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::DecodeHeader(VideoCORE * core, mfxBitstream *bs, mfxVideoParam *params)
{
    return MFX_VP9_Utility::DecodeHeader(core, bs, params);
}

static bool IsSameVideoParam(mfxVideoParam *newPar, mfxVideoParam *oldPar)
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

    if (newPar->mfx.NumThread > oldPar->mfx.NumThread && oldPar->mfx.NumThread > 0) //  need more surfaces for efficient decoding
    {
        return false;
    }

    return true;
}

mfxStatus VideoDECODEVP9_HW::Reset(mfxVideoParam *p_video_param)
{
    if (false == m_is_initialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    MFX_CHECK_NULL_PTR1(p_video_param);

    eMFXHWType type = m_core->GetHWType();

    if (MFX_ERR_NONE > CheckVideoParamDecoders(p_video_param, m_core->IsExternalFrameAllocator(), type))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (false == MFX_VP9_Utility::CheckVideoParam(p_video_param, type))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (!IsSameVideoParam(p_video_param, &m_on_init_video_params))
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    // need to sw acceleration
    if (m_platform != m_core->GetPlatformType())
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (m_FrameAllocator->Reset() != UMC::UMC_OK)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    m_va->Reset();

    m_frameOrder = 0;
    memset(&m_stat, 0, sizeof(m_stat));

    m_on_init_video_params = *p_video_param;
    m_video_params = m_on_init_video_params;

    if (false == CheckHardwareSupport(m_core, p_video_param))
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::Close()
{

    if (false == m_is_initialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    m_FrameAllocator->Close();

    if (0 < m_response.NumFrameActual)
    {
        m_core->FreeFrames(&m_response);
    }

    m_is_initialized = false;
    m_is_software_buffer = false;

    m_frameOrder = (mfxU16)MFX_FRAMEORDER_UNKNOWN;

    m_va = NULL;

    memset(&m_stat, 0, sizeof(m_stat));

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::Query(VideoCORE *p_core, mfxVideoParam *p_in, mfxVideoParam *p_out)
{
    MFX_CHECK_NULL_PTR1(p_out);

    eMFXHWType type = p_core->GetHWType();

    #ifdef MFX_VA_WIN

    if (p_core->IsGuidSupported(sDXVA_Intel_ModeVP9_VLD, p_in) != MFX_ERR_NONE)
    {
        //return MFX_WRN_PARTIAL_ACCELERATION;
    }

    // todo : VA API alternative?!

    #endif

    return MFX_VP9_Utility::Query(p_core, p_in, p_out, type);
}

mfxStatus VideoDECODEVP9_HW::QueryIOSurfInternal(eMFXPlatform /* platform */, mfxVideoParam *p_params, mfxFrameAllocRequest *p_request)
{

    p_request->Info = p_params->mfx.FrameInfo;

    p_request->NumFrameMin = mfxU16 (8);

    p_request->NumFrameMin += p_params->AsyncDepth ? p_params->AsyncDepth : MFX_AUTO_ASYNC_DEPTH_VALUE;
    p_request->NumFrameSuggested = p_request->NumFrameMin;


    if(p_params->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        p_request->Type = MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE;
    }
    else if (p_params->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
    {
        p_request->Type = MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE;
    }
    else
    {
        // Opaq is not supported yet.
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;

}

mfxStatus VideoDECODEVP9_HW::QueryIOSurf(VideoCORE *p_core, mfxVideoParam *p_video_param, mfxFrameAllocRequest *p_request)
{

    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR3(p_core, p_video_param, p_request);

    eMFXPlatform platform = MFX_VP9_Utility::GetPlatform(p_core, p_video_param);

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (platform == MFX_PLATFORM_HARDWARE)
    {
        type = p_core->GetHWType();
    }

    mfxVideoParam p_params = *p_video_param;

    if (!(p_params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && !(p_params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if ((p_params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (p_params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if(p_params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        p_request->NumFrameMin = 1;
        p_request->NumFrameSuggested = p_request->NumFrameMin + (p_params.AsyncDepth ? p_params.AsyncDepth : MFX_AUTO_ASYNC_DEPTH_VALUE);
        p_request->Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE;
    }
    else
    {
        sts = QueryIOSurfInternal(platform, p_video_param, p_request);
    }

    #ifdef MFX_VA_WIN

    if (p_core->IsGuidSupported(sDXVA_Intel_ModeVP9_VLD, p_video_param) != MFX_ERR_NONE)
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }
    // todo : VA API alternative ?

    #endif

    return sts;
}

mfxStatus VideoDECODEVP9_HW::DecodeFrameCheck(mfxBitstream * /* bs */, mfxFrameSurface1 * /* surface_work */, mfxFrameSurface1 ** /* surface_out */)
{
    return MFX_ERR_NONE;
}

void VideoDECODEVP9_HW::CalculateTimeSteps(mfxFrameSurface1 *p_surface)
{
    p_surface->Data.TimeStamp = GetMfxTimeStamp(m_num_output_frames * m_in_framerate);
    p_surface->Data.FrameOrder = m_num_output_frames;
    m_num_output_frames += 1;

    p_surface->Info.FrameRateExtD = m_on_init_video_params.mfx.FrameInfo.FrameRateExtD;
    p_surface->Info.FrameRateExtN = m_on_init_video_params.mfx.FrameInfo.FrameRateExtN;

    p_surface->Info.CropX = m_on_init_video_params.mfx.FrameInfo.CropX;
    p_surface->Info.CropY = m_on_init_video_params.mfx.FrameInfo.CropY;
    p_surface->Info.PicStruct = m_on_init_video_params.mfx.FrameInfo.PicStruct;

    p_surface->Info.AspectRatioH = 1;
    p_surface->Info.AspectRatioW = 1;
}

mfxStatus MFX_CDECL VP9DECODERoutine(void *p_state, void * /* pp_param */, mfxU32 /* thread_number */, mfxU32)
{
    VideoDECODEVP9_HW::VP9DECODERoutineData& data = *(VideoDECODEVP9_HW::VP9DECODERoutineData*)p_state;
    VideoDECODEVP9_HW& decoder = *data.decoder;

    #ifdef MFX_VA_LINUX
    if(UMC::UMC_OK != decoder.m_va->SyncTask(data.currFrameId))
        return MFX_ERR_DEVICE_FAILED;
    #else

    #endif

    if (decoder.m_video_params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        decoder.m_FrameAllocator->PrepareToOutput(data.surface_work, data.currFrameId, &decoder.m_on_init_video_params, false);
    }

    if (data.currFrameId != -1)
       decoder.m_FrameAllocator.get()->DecreaseReference(data.currFrameId);

    return MFX_ERR_NONE;

}

mfxStatus VP9CompleteProc(void * /* p_state */, void * /* pp_param */, mfxStatus)
{
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, MFX_ENTRY_POINT * p_entry_point)
{

    mfxStatus sts = MFX_ERR_NONE;

    if (false == m_is_initialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    MFX_CHECK_NULL_PTR2(surface_work, surface_out);

    if (0 != surface_work->Data.Locked)
    {
        return MFX_ERR_MORE_SURFACE;
    }
    m_index++;

    sts = CheckFrameInfoCodecs(&surface_work->Info, MFX_CODEC_VP9);
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
        return MFX_ERR_MORE_DATA;

    mfxU8 *pBegin = bs->Data + bs->DataOffset;

    InputBitstream bsReader(pBegin, pBegin+ bs->DataLength);
    bsReader.GetBits(4);

    if (bsReader.GetBit()) // showExistingFrame
        return MFX_ERR_UNSUPPORTED; // need to implement

    VP9_FRAME_TYPE frameType = (VP9_FRAME_TYPE) bsReader.GetBit();
    mfxU8 showFrame = (mfxU8)bsReader.GetBit();

    if (0 == surface_work->Info.CropW)
    {
        surface_work->Info.CropW = m_on_init_video_params.mfx.FrameInfo.CropW;
    }

    if (0 == surface_work->Info.CropH)
    {
        surface_work->Info.CropH = m_on_init_video_params.mfx.FrameInfo.CropH;
    }

    *surface_out = 0;

    sts = m_FrameAllocator->SetCurrentMFXSurface(surface_work, false);
    MFX_CHECK_STS(sts);

    UMC::FrameMemID currMid = 0;
    UMC::VideoDataInfo videoInfo;
    if (UMC::UMC_OK != videoInfo.Init(m_init_w, m_init_h, UMC::NV12, 8))
        return MFX_ERR_MEMORY_ALLOC;

    if (UMC::UMC_OK != m_FrameAllocator->Alloc(&currMid, &videoInfo, 0))
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    m_frameInfo.currFrame = currMid;

    sts = DecodeSuperFrame(bs, m_frameInfo);
    MFX_CHECK_STS(sts);

    sts = DecodeFrameHeader(&m_bs, m_frameInfo);
    MFX_CHECK_STS(sts);

    if (UMC::UMC_OK != m_FrameAllocator->IncreaseReference(m_frameInfo.currFrame))
    {
        return MFX_ERR_UNKNOWN;
    }

    sts = PackHeaders(&m_bs, m_frameInfo);
    MFX_CHECK_STS(sts);

    if (UMC::UMC_OK != m_va->BeginFrame(m_frameInfo.currFrame))
        return MFX_ERR_DEVICE_FAILED;

    if (UMC::UMC_OK != m_va->Execute())
        return MFX_ERR_DEVICE_FAILED;

    if (UMC::UMC_OK != m_va->EndFrame())
        return MFX_ERR_DEVICE_FAILED;
    UpdateRefFrames(m_frameInfo.refreshFrameFlags, m_frameInfo); // move to async part

    sts = GetOutputSurface(surface_out, surface_work, m_frameInfo.currFrame);

    *surface_out = surface_work;

    MFX_CHECK_STS(sts);

    (*surface_out)->Data.Corrupted = 0;
    (*surface_out)->Data.FrameOrder = m_frameOrder;

    if(showFrame)
        m_frameOrder++;

    p_entry_point->pRoutine = &VP9DECODERoutine;
    p_entry_point->pCompleteProc = &VP9CompleteProc;

    VP9DECODERoutineData* routineData = new VP9DECODERoutineData;
    routineData->decoder = this;
    routineData->currFrameId = m_frameInfo.currFrame;
    routineData->surface_work = surface_work;
    routineData->index        = m_index;

    p_entry_point->pState = routineData;
    p_entry_point->requiredNumThreads = 1;

    if (showFrame)
        return MFX_ERR_NONE;
    return bs->DataLength ? MFX_ERR_MORE_SURFACE : MFX_ERR_MORE_DATA;
}

mfxStatus VideoDECODEVP9_HW::GetOutputSurface(mfxFrameSurface1 **surface_out, mfxFrameSurface1 *surface_work, UMC::FrameMemID index)
{
    mfxFrameSurface1 *pNativeSurface = m_FrameAllocator->GetSurface(index, surface_work, &m_on_init_video_params);
    if (pNativeSurface)
    {
        *surface_out = m_core->GetOpaqSurface(pNativeSurface->Data.MemId) ? m_core->GetOpaqSurface(pNativeSurface->Data.MemId) : pNativeSurface;
    }
    else
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    return MFX_ERR_NONE;
}

/*
void VideoDECODEVP9_HW::UpdateSegmentation(MFX_VP9_BoolDecoder &dec)
{

}


void VideoDECODEVP9_HW::UpdateLoopFilterDeltas(MFX_VP9_BoolDecoder &dec)
{

}

void VideoDECODEVP9_HW::DecodeInitDequantization(MFX_VP9_BoolDecoder &dec)
{

}
*/

mfxStatus VideoDECODEVP9_HW::DecodeSuperFrame(mfxBitstream *in, VP9FrameInfo & info)
{
    if (!in || !in->Data)
        return MFX_ERR_NULL_PTR;

    //memset(&info, 0, sizeof(VP9FrameInfo));

    mfxU32 frameSizes[8] = { 0 };
    mfxU32 frameCount = 0;

    m_bs = *in;

    ParseSuperFrameIndex(in->Data + in->DataOffset, in->DataLength, frameSizes, &frameCount);

    if (frameCount > 1)
    {
        if (info.frameCountInBS == 0) // first call we meet super frame
        {
            info.frameCountInBS = frameCount;
            info.currFrameInBS = 0;
        }

        if (info.frameCountInBS != frameCount) // it is not what we met before
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        m_bs.DataLength = frameSizes[info.currFrameInBS];
        m_bs.DataOffset = in->DataOffset;
        for (mfxU32 i = 0; i < info.currFrameInBS; i++)
            m_bs.DataOffset += frameSizes[i];

        info.currFrameInBS++;
        if (info.currFrameInBS < info.frameCountInBS)
            return MFX_ERR_NONE;
    }

    info.currFrameInBS = 0;
    info.frameCountInBS = 0;

    in->DataOffset += in->DataLength;
    in->DataLength = 0;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::DecodeFrameHeader(mfxBitstream *in, VP9FrameInfo & info)
{
    if (!in || !in->Data)
        return MFX_ERR_NULL_PTR;

    InputBitstream bsReader(in->Data + in->DataOffset, in->Data + in->DataOffset + in->DataLength);

    if (VP9_FRAME_MARKER != bsReader.GetBits(2))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    info.version = bsReader.GetBit();
    if (info.version > 1)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    bsReader.GetBit(); // skip unused version bit

    if (bsReader.GetBit()) // showExistingFrame
    {
        return MFX_ERR_UNSUPPORTED; // need to implement
    }

    info.frameType = (VP9_FRAME_TYPE) bsReader.GetBit();
    info.showFrame = bsReader.GetBit();
    info.errorResilientMode = bsReader.GetBit();

    if (KEY_FRAME == info.frameType)
    {
        if (0x49 != bsReader.GetBits(8) || 0x83 != bsReader.GetBits(8) || 0x42 != bsReader.GetBits(8))
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        if (SRGB != bsReader.GetBits(3))
        {
            bsReader.GetBit(); // 0: [16, 235] (i.e. xvYCC), 1: [0, 255]
            if (1 == info.version)
            {
                info.subsamplingX = bsReader.GetBit();
                info.subsamplingY = bsReader.GetBit();
                bsReader.GetBit(); // has extra plane
            }
            else
                info.subsamplingY = info.subsamplingX = 1;
        }
        else
        {
            if (1 == info.version)
            {
                info.subsamplingY = info.subsamplingX = 0;
                bsReader.GetBit();  // has extra plane
            }
            else
                return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        info.refreshFrameFlags = (1 << NUM_REF_FRAMES) - 1;

        for (mfxU8 i = 0; i < REFS_PER_FRAME; ++i)
        {
#if 0
            info.activeRefIdx[i] = info.currFrame;
#else
            info.activeRefIdx[i] = 0;
#endif
        }
        GetFrameSize(&bsReader, info);
    }
    else
    {
        info.intraOnly = info.showFrame ? 0 : bsReader.GetBit();

        info.resetFrameContext = info.errorResilientMode ? 0 : bsReader.GetBits(2);

        if (info.intraOnly)
        {
            if (0x49 != bsReader.GetBits(8) || 0x83 != bsReader.GetBits(8) || 0x42 != bsReader.GetBits(8))
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            info.refreshFrameFlags = (mfxU8)bsReader.GetBits(NUM_REF_FRAMES);

            GetFrameSize(&bsReader, info);
        }
        else
        {
            info.refreshFrameFlags = (mfxU8)bsReader.GetBits(NUM_REF_FRAMES);

            for (mfxU8 i = 0; i < REFS_PER_FRAME; ++i)
            {
                const mfxI32 ref = bsReader.GetBits(REF_FRAMES_LOG2);
#if 0
                const UMC::FrameMemID idx = info.ref_frame_map[ref];
                info.activeRefIdx[i] = idx;
#else

                info.activeRefIdx[i] = ref;
#endif
                info.refFrameSignBias[LAST_FRAME + i] = bsReader.GetBit();
            }

            GetFrameSizeWithRefs(&bsReader, info);

            info.allowHighPrecisionMv = bsReader.GetBit();

            static const INTERP_FILTER literal2Filter[] =
                { EIGHTTAP_SMOOTH, EIGHTTAP, EIGHTTAP_SHARP, BILINEAR };
            info.interpFilter = bsReader.GetBit() ? SWITCHABLE : literal2Filter[bsReader.GetBits(2)];
        }
    }

    if (!info.errorResilientMode)
    {
        info.refreshFrameContext = bsReader.GetBit();
        info.frameParallelDecodingMode = bsReader.GetBit();
    }
    else
    {
        info.refreshFrameContext = 0;
        info.frameParallelDecodingMode = 1;
    }

    // This flag will be overridden by the call to vp9_setup_past_independence
    // below, forcing the use of context 0 for those frame types.
    info.frameContextIdx = bsReader.GetBits(FRAME_CONTEXTS_LOG2);

    if (info.frameType == KEY_FRAME || info.intraOnly || info.errorResilientMode)
    {
       SetupPastIndependence(info);
    }

    SetupLoopFilter(&bsReader, info.lf);

    //setup_quantization()
    {
        info.baseQIndex = bsReader.GetBits(QINDEX_BITS);
        mfxI32 old_y_dc_delta_q = m_y_dc_delta_q;
        mfxI32 old_uv_dc_delta_q = m_uv_dc_delta_q;
        mfxI32 old_uv_ac_delta_q = m_uv_ac_delta_q;

        if (bsReader.GetBit())
        {
            info.y_dc_delta_q = bsReader.GetBits(4);
            info.y_dc_delta_q = bsReader.GetBit() ? -info.y_dc_delta_q : info.y_dc_delta_q;
        }
        else
            info.y_dc_delta_q = 0;

        if (bsReader.GetBit())
        {
            info.uv_dc_delta_q = bsReader.GetBits(4);
            info.uv_dc_delta_q = bsReader.GetBit() ? -info.uv_dc_delta_q : info.uv_dc_delta_q;
        }
        else
            info.uv_dc_delta_q = 0;

        if (bsReader.GetBit())
        {
            info.uv_ac_delta_q = bsReader.GetBits(4);
            info.uv_ac_delta_q = bsReader.GetBit() ? -info.uv_ac_delta_q : info.uv_ac_delta_q;
        }
        else
            info.uv_ac_delta_q = 0;

        m_y_dc_delta_q = info.y_dc_delta_q;
        m_uv_dc_delta_q = info.uv_dc_delta_q;
        m_uv_ac_delta_q = info.uv_ac_delta_q;

        if (old_y_dc_delta_q  != info.y_dc_delta_q  ||
            old_uv_dc_delta_q != info.uv_dc_delta_q ||
            old_uv_ac_delta_q != info.uv_ac_delta_q ||
            0 == m_frameOrder)
        {
            InitDequantizer(info);
        }

        info.lossless = (0 == info.baseQIndex &&
                         0 == info.y_dc_delta_q &&
                         0 == info.uv_dc_delta_q &&
                         0 == info.uv_ac_delta_q);
    }

    // setup_segmentation()
    {
        info.segmentation.updateMap = 0;
        info.segmentation.updateData = 0;

        info.segmentation.enabled = (mfxU8)bsReader.GetBit();
        if (info.segmentation.enabled)
        {
            // Segmentation map update
            info.segmentation.updateMap = (mfxU8)bsReader.GetBit();
            if (info.segmentation.updateMap)
            {
                for (mfxU8 i = 0; i < VP9_NUM_OF_SEGMENT_TREE_PROBS; ++i)
                    info.segmentation.treeProbs[i] = (mfxU8) (bsReader.GetBit() ? bsReader.GetBits(8) : VP9_MAX_PROB);

                info.segmentation.temporalUpdate = (mfxU8)bsReader.GetBit();
                if (info.segmentation.temporalUpdate)
                {
                    for (mfxU8 i = 0; i < VP9_NUM_OF_PREDICTION_PROBS; ++i)
                        info.segmentation.predProbs[i] = (mfxU8) (bsReader.GetBit() ? bsReader.GetBits(8) : VP9_MAX_PROB);
                }
                else
                {
                    for (mfxU8 i = 0; i < VP9_NUM_OF_PREDICTION_PROBS; ++i)
                        info.segmentation.predProbs[i] = VP9_MAX_PROB;
                }
            }

            info.segmentation.updateData = (mfxU8)bsReader.GetBit();
            if (info.segmentation.updateData)
            {
                info.segmentation.absDelta = (mfxU8)bsReader.GetBit();

                ClearAllSegFeatures(info.segmentation);

                mfxI32 data = 0;
                mfxU32 nBits = 0;
                for (mfxU8 i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; ++i)
                {
                    for (mfxU8 j = 0; j < SEG_LVL_MAX; ++j)
                    {
                        data = 0;
                        if (bsReader.GetBit()) // feature_enabled
                        {
                            EnableSegFeature(info.segmentation, i, (SEG_LVL_FEATURES) j);

                            nBits = GetUnsignedBits(SEG_FEATURE_DATA_MAX[j]);
                            data = bsReader.GetBits(nBits);
                            data = data > SEG_FEATURE_DATA_MAX[j] ? SEG_FEATURE_DATA_MAX[j] : data;

                            if (IsSegFeatureSigned( (SEG_LVL_FEATURES) j))
                                data = bsReader.GetBit() ? -data : data;
                        }

                        SetSegData(info.segmentation, i, (SEG_LVL_FEATURES) j, data);
                    }
                }
            }
#if 0
            printf("=====segmentation======\n");
            printf("enabled %d\n", info.segmentation.enabled);
            printf("update_map %d\n", info.segmentation.updateMap);
            printf("update_data %d\n", info.segmentation.updateData);
            printf("abs_delta %d\n", info.segmentation.absDelta);
            printf("temporal_update %d\n", info.segmentation.temporalUpdate);
            for (mfxU8 i = 0; i < VP9_NUM_OF_SEGMENT_TREE_PROBS; ++i)
                printf("tree_probs[%d] %d\n", i, info.segmentation.treeProbs[i]);

            for (mfxU8 i = 0; i < VP9_NUM_OF_PREDICTION_PROBS; ++i)
                printf("pred_probs[%d] %d\n", i, info.segmentation.predProbs[i]);

            for (mfxU8 i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; ++i)
            {
                printf("feature_mask[%d] %d\n", i, info.segmentation.featureMask[i]);
                for (mfxU8 j = 0; j < SEG_LVL_MAX; ++j)
                {
                    printf("feature_data[%d][%d] %d\n", i, j, info.segmentation.featureData[i][j]);
                }
            }
            printf("=========================\n");
#endif
        }
    }

    // setup_tile_info()
    {
        const mfxI32 alignedWidth = ALIGN_POWER_OF_TWO(info.width, MI_SIZE_LOG2);
        int minLog2TileColumns, maxLog2TileColumns, maxOnes;
        mfxU32 miCols = alignedWidth >> MI_SIZE_LOG2;
        GetTileNBits(miCols, minLog2TileColumns, maxLog2TileColumns);

        maxOnes = maxLog2TileColumns - minLog2TileColumns;
        info.log2TileColumns = minLog2TileColumns;
        while (maxOnes-- && bsReader.GetBit())
            info.log2TileColumns++;

        info.log2TileRows = bsReader.GetBit();
        if (info.log2TileRows)
            info.log2TileRows += bsReader.GetBit();
    }

    info.firstPartitionSize = bsReader.GetBits(16);
    if (0 == info.firstPartitionSize)
        return MFX_ERR_UNSUPPORTED;

#if 0
    printf("first_partition_size %d\n", info.firstPartitionSize);
#endif

    info.frameHeaderLength = (bsReader.NumBitsRead() / 8 + (bsReader.NumBitsRead() % 8 > 0));
    info.frameDataSize = in->DataLength;

    // vp9_loop_filter_frame_init()
    if (info.lf.filterLevel)
    {
        const mfxI32 scale = 1 << (info.lf.filterLevel >> 5);

        LoopFilterInfo & lf_info = info.lf_info;

        for (mfxU8 segmentId = 0; segmentId < VP9_MAX_NUM_OF_SEGMENTS; ++segmentId)
        {
            mfxI32 segmentFilterLevel = info.lf.filterLevel;
            if (IsSegFeatureActive(info.segmentation, segmentId, SEG_LVL_ALT_LF))
            {
                const mfxI32 data = GetSegData(info.segmentation, segmentId, SEG_LVL_ALT_LF);
                segmentFilterLevel = clamp(info.segmentation.absDelta == SEGMENT_ABSDATA ? data : info.lf.filterLevel + data,
                                           0,
                                           MAX_LOOP_FILTER);
            }

            if (!info.lf.modeRefDeltaEnabled)
            {
                memset(lf_info.level[segmentId], segmentFilterLevel, sizeof(lf_info.level[segmentId]) );
            }
            else
            {
                const mfxI32 intra_lvl = segmentFilterLevel + info.lf.refDeltas[INTRA_FRAME] * scale;
                lf_info.level[segmentId][INTRA_FRAME][0] = (mfxU8) clamp(intra_lvl, 0, MAX_LOOP_FILTER);

                for (mfxU8 ref = LAST_FRAME; ref < MAX_REF_FRAMES; ++ref)
                {
                    for (mfxU8 mode = 0; mode < MAX_MODE_LF_DELTAS; ++mode)
                    {
                        const mfxI32 inter_lvl = segmentFilterLevel + info.lf.refDeltas[ref] * scale
                                                        + info.lf.modeDeltas[mode] * scale;
                        lf_info.level[segmentId][ref][mode] = (mfxU8) clamp(inter_lvl, 0, MAX_LOOP_FILTER);
                    }
                }
            }
        }
#if 0
        for (mfxU8 segmentId = 0; segmentId < VP9_MAX_NUM_OF_SEGMENTS; ++segmentId)
        {
            printf("segmentId %d\n", segmentId);
            for (mfxU8 ref = 0; ref < MAX_REF_FRAMES; ++ref)
            {
                for (mfxU8 mode = 0; mode < MAX_MODE_LF_DELTAS; ++mode)
                {
                    printf("lvl[%d][%d] %d, ", ref, mode, info.lf_info.level[segmentId][ref][mode]);
                }
                printf("\n");
            }
            printf("\n");
        }
#endif
    }

    return MFX_ERR_NONE;
}

mfxTaskThreadingPolicy VideoDECODEVP9_HW::GetThreadingPolicy()
{
    return MFX_TASK_THREADING_SHARED;
}

mfxStatus VideoDECODEVP9_HW::GetVideoParam(mfxVideoParam *pPar)
{
    if (!m_is_initialized)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(pPar);

    memcpy(&pPar->mfx, &m_on_init_video_params.mfx, sizeof(mfxInfoMFX));

    pPar->Protected = m_on_init_video_params.Protected;
    pPar->IOPattern = m_on_init_video_params.IOPattern;
    pPar->AsyncDepth = m_on_init_video_params.AsyncDepth;

    if (!pPar->mfx.FrameInfo.FrameRateExtD && !pPar->mfx.FrameInfo.FrameRateExtN)
    {
        pPar->mfx.FrameInfo.FrameRateExtD = m_video_params.mfx.FrameInfo.FrameRateExtD;
        pPar->mfx.FrameInfo.FrameRateExtN = m_video_params.mfx.FrameInfo.FrameRateExtN;

        if (!pPar->mfx.FrameInfo.FrameRateExtD && !pPar->mfx.FrameInfo.FrameRateExtN)
        {
            pPar->mfx.FrameInfo.FrameRateExtN = 30;
            pPar->mfx.FrameInfo.FrameRateExtD = 1;
        }
    }

    if (!pPar->mfx.FrameInfo.AspectRatioH && !pPar->mfx.FrameInfo.AspectRatioW)
    {
        pPar->mfx.FrameInfo.AspectRatioH = m_video_params.mfx.FrameInfo.AspectRatioH;
        pPar->mfx.FrameInfo.AspectRatioW = m_video_params.mfx.FrameInfo.AspectRatioW;

        if (!pPar->mfx.FrameInfo.AspectRatioH && !pPar->mfx.FrameInfo.AspectRatioW)
        {
            pPar->mfx.FrameInfo.AspectRatioH = 1;
            pPar->mfx.FrameInfo.AspectRatioW = 1;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::GetDecodeStat(mfxDecodeStat *pStat)
{
    if (!m_is_initialized)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(pStat);

    m_stat.NumSkippedFrame = 0;
    m_stat.NumCachedFrame = 0;

    memcpy(pStat, &m_stat, sizeof(m_stat));

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::DecodeFrame(mfxBitstream *, mfxFrameSurface1 *, mfxFrameSurface1 *)
{
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::GetUserData(mfxU8 *pUserData, mfxU32 *pSize, mfxU64 *pTimeStamp)
{
    if (!m_is_initialized)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR3(pUserData, pSize, pTimeStamp);

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VideoDECODEVP9_HW::GetPayload(mfxU64 *pTimeStamp, mfxPayload *pPayload)
{
    if (!m_is_initialized)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR3(pTimeStamp, pPayload, pPayload->Data);

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VideoDECODEVP9_HW::SetSkipMode(mfxSkipMode /*mode*/)
{
    if (!m_is_initialized)
        return MFX_ERR_NOT_INITIALIZED;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::UpdateRefFrames(const mfxU8 refreshFrameFlags, VP9FrameInfo & info)
{
    mfxI32 ref_index = 0;

    for (mfxU8 mask = refreshFrameFlags; mask; mask >>= 1)
    {
        if (mask & 1)
        {
            const UMC::FrameMemID oldMid = info.ref_frame_map[ref_index];
            if (oldMid >= 0)
            {
                if (m_FrameAllocator->DecreaseReference(oldMid) != UMC::UMC_OK)
                    return MFX_ERR_UNKNOWN;
            }

            info.ref_frame_map[ref_index] = info.currFrame;
            if (m_FrameAllocator->IncreaseReference(info.currFrame) != UMC::UMC_OK)
                return MFX_ERR_UNKNOWN;
        }
        ++ref_index;
    }

    for (ref_index = 0; ref_index < 3; ref_index++)
        info.activeRefIdx[ref_index] = -1;

    return MFX_ERR_NONE;
}

namespace
{
#define DUMP_STRUCT_MEMBER(S,M) printf("%-50s %d\n", #M, S.M)
#ifdef MFX_VA_LINUX
    void DumpInfo(VADecPictureParameterBufferVP9 const & picParam, mfxU32 frameNum)
    {
        printf("Frame #%d\n", frameNum);

        DUMP_STRUCT_MEMBER(picParam, frame_width);
        DUMP_STRUCT_MEMBER(picParam, frame_height);
//        DUMP_STRUCT_MEMBER(picParam, curr_pic);
        for (mfxU8 ref = 0; ref < NUM_REF_FRAMES; ++ref)
        {
            printf("picParam.reference_frames[%d]                       %d\n", ref, picParam.reference_frames[ref]);
        }
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.subsampling_x);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.subsampling_y);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.frame_type);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.show_frame);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.error_resilient_mode);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.intra_only);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.allow_high_precision_mv);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.mcomp_filter_type);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.frame_parallel_decoding_mode);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.reset_frame_context);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.refresh_frame_context);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.frame_context_idx);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.segmentation_enabled);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.segmentation_temporal_update);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.segmentation_update_map);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.last_ref_frame);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.last_ref_frame_sign_bias);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.golden_ref_frame);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.golden_ref_frame_sign_bias);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.alt_ref_frame);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.alt_ref_frame_sign_bias);
        DUMP_STRUCT_MEMBER(picParam, pic_fields.bits.lossless_flag);

        DUMP_STRUCT_MEMBER(picParam, filter_level);
        DUMP_STRUCT_MEMBER(picParam, sharpness_level);
        DUMP_STRUCT_MEMBER(picParam, log2_tile_rows);
        DUMP_STRUCT_MEMBER(picParam, log2_tile_columns);
        DUMP_STRUCT_MEMBER(picParam, frame_header_length_in_bytes);
        DUMP_STRUCT_MEMBER(picParam, first_partition_size);
//        DUMP_STRUCT_MEMBER(picParam, frame_data_size);
        for (mfxU8 i = 0; i < VP9_NUM_OF_SEGMENT_TREE_PROBS; ++i)
            printf("picParam.mb_segment_tree_probs[%d]                  %d\n", i, picParam.mb_segment_tree_probs[i]);
        for (mfxU8 i = 0; i < VP9_NUM_OF_PREDICTION_PROBS; ++i)
            printf("picParam.segment_pred_probs[%d]                     %d\n", i, picParam.segment_pred_probs[i]);
        DUMP_STRUCT_MEMBER(picParam, version);

        printf("\n");
    }

    void DumpInfo(VASliceParameterBufferVP9 const & sliceParam)
    {
        for (mfxU8 segmentId = 0; segmentId < VP9_MAX_NUM_OF_SEGMENTS; ++segmentId)
        {
            printf("sliceParam.seg_param[%d].segment_reference_enabled  %d\n",
                   segmentId, sliceParam.seg_param[segmentId].segment_flags.fields.segment_reference_enabled);

            printf("sliceParam.seg_param[%d].segment_reference          %d\n",
                   segmentId, sliceParam.seg_param[segmentId].segment_flags.fields.segment_reference);

            printf("sliceParam.seg_param[%d].segment_reference_skipped  %d\n",
                   segmentId, sliceParam.seg_param[segmentId].segment_flags.fields.segment_reference_skipped);

            for (mfxU8 ref = INTRA_FRAME; ref < MAX_REF_FRAMES; ++ref)
                for (mfxU8 mode = 0; mode < MAX_MODE_LF_DELTAS; ++mode)
                    printf("sliceParam.seg_param[%d].filter_level[%d][%d]         %d\n",
                           segmentId, ref, mode, sliceParam.seg_param[segmentId].filter_level[ref][mode]);
        }

        printf("\n");
    }
#endif
#ifdef  MFX_VA_WIN
void DumpInfo(DXVA_Intel_PicParams_VP9 const & picParam, DXVA_Intel_Segment_VP9 const & segParam, mfxU32 frameNum)
{
    {
        printf("\n------------------Frame Number: %d----------------------------\n", frameNum);
#ifdef REV80
      printf("\nPicParamsVP9.FrameWidthInMinBlocksMinus1        : %d = 0x%x",picParam.FrameWidthInMinBlocksMinus1, picParam.FrameWidthInMinBlocksMinus1);
        printf("\nPicParamsVP9.FrameHeightInMinBlocksMinus1        : %d = 0x%x",picParam.FrameHeightInMinBlocksMinus1, picParam.FrameHeightInMinBlocksMinus1);
#else
      printf("\nPicParamsVP9.FrameWidthInMinBlocksMinus1        : %d = 0x%x",picParam.FrameWidthMinus1, picParam.FrameWidthMinus1);
        printf("\nPicParamsVP9.FrameHeightInMinBlocksMinus1        : %d = 0x%x",picParam.FrameHeightMinus1, picParam.FrameHeightMinus1);
#endif
        printf("\nPicParamsVP9.PicFlags.fields.frame_type        : %d = 0x%x",picParam.PicFlags.fields.frame_type, picParam.PicFlags.fields.frame_type);
        printf("\nPicParamsVP9.PicFlags.fields.show_frame        : %d = 0x%x",picParam.PicFlags.fields.show_frame, picParam.PicFlags.fields.show_frame);
        printf("\nPicParamsVP9.PicFlags.fields.error_resilient_mode    : %d = 0x%x",picParam.PicFlags.fields.error_resilient_mode, picParam.PicFlags.fields.error_resilient_mode);
        printf("\nPicParamsVP9.PicFlags.fields.intra_only        : %d = 0x%x",picParam.PicFlags.fields.intra_only, picParam.PicFlags.fields.intra_only);
        printf("\nPicParamsVP9.PicFlags.fields.LastRefSignBias        : %d = 0x%x",picParam.PicFlags.fields.LastRefSignBias, picParam.PicFlags.fields.LastRefSignBias);
        printf("\nPicParamsVP9.PicFlags.fields.GoldenRefSignBias        : %d = 0x%x",picParam.PicFlags.fields.GoldenRefSignBias, picParam.PicFlags.fields.GoldenRefSignBias);
        printf("\nPicParamsVP9.PicFlags.fields.AltRefSignBias        : %d = 0x%x",picParam.PicFlags.fields.AltRefSignBias, picParam.PicFlags.fields.AltRefSignBias);
        printf("\nPicParamsVP9.PicFlags.fields.LASTRefIdx[0]        : %d = 0x%x",picParam.PicFlags.fields.LastRefIdx, picParam.PicFlags.fields.LastRefIdx);
        printf("\nPicParamsVP9.PicFlags.fields.GOLDENRefIdx[1]        : %d = 0x%x",picParam.PicFlags.fields.GoldenRefIdx, picParam.PicFlags.fields.GoldenRefIdx);
        printf("\nPicParamsVP9.PicFlags.fields.ALTRefIdx[2]        : %d = 0x%x",picParam.PicFlags.fields.AltRefIdx, picParam.PicFlags.fields.AltRefIdx);

        for (int i = 0; i < NUM_REF_FRAMES; i++) {
            printf("\n_PicParamsVP9.RefFrameList[%d]                : %d = 0x%x", i, picParam.RefFrameList[i], picParam.RefFrameList[i]);
        }
/*
        //non dxva params
        fprintf(fdump,"\n-----------------------------------------------------");
        fprintf(fdump,"\n non-DXVA Parameters");
        for (int i = 0; i < NUM_YV12_BUFFERS; i++) {
            printf("\n_surf_idx_used[%d]                    : %d = 0x%x -> (Picture = %d)", i, _surf_idx_used[i], _surf_idx_used[i], _surfIdxUsedFrmNum[i]);
        }
        fprintf(fdump,"\n-----------------------------------------------------");
        for (int i = 0; i < ALLOWED_REFS_PER_FRAME; i++) {
            printf("\nactive_ref_idx[%d]                    : %d = 0x%x -> (Picture = %d)", i, active_ref_idx[i], active_ref_idx[i], _surfIdxUsedFrmNum[i]);
        }
        fprintf(fdump,"\n-----------------------------------------------------");
        //end non dxva params
*/

        printf("\nPicParamsVP9.PicFlags.fields.allow_high_precision_mv    : %d = 0x%x",picParam.PicFlags.fields.allow_high_precision_mv, picParam.PicFlags.fields.allow_high_precision_mv);
        printf("\nPicParamsVP9.PicFlags.fields.mcomp_filter_type            : %d = 0x%x",picParam.PicFlags.fields.mcomp_filter_type, picParam.PicFlags.fields.mcomp_filter_type);
        printf("\nPicParamsVP9.PicFlags.fields.frame_parallel_decoding_mode    : %d = 0x%x",picParam.PicFlags.fields.frame_parallel_decoding_mode, picParam.PicFlags.fields.frame_parallel_decoding_mode);
        printf("\nPicParamsVP9.PicFlags.fields.segmentation_enabled        : %d = 0x%x",picParam.PicFlags.fields.segmentation_enabled, picParam.PicFlags.fields.segmentation_enabled);
        printf("\nPicParamsVP9.PicFlags.fields.segmentation_temporal_update    : %d = 0x%x",picParam.PicFlags.fields.segmentation_temporal_update, picParam.PicFlags.fields.segmentation_temporal_update);
        printf("\nPicParamsVP9.PicFlags.fields.segmentation_update_map    : %d = 0x%x",picParam.PicFlags.fields.segmentation_update_map, picParam.PicFlags.fields.segmentation_update_map);
        printf("\nPicParamsVP9.PicFlags.fields.reset_frame_context        : %d = 0x%x",picParam.PicFlags.fields.reset_frame_context, picParam.PicFlags.fields.reset_frame_context);
        printf("\nPicParamsVP9.PicFlags.fields.refresh_frame_context        : %d = 0x%x",picParam.PicFlags.fields.refresh_frame_context, picParam.PicFlags.fields.refresh_frame_context);
        printf("\nPicParamsVP9.PicFlags.fields.frame_context_idx            : %d = 0x%x",picParam.PicFlags.fields.frame_context_idx, picParam.PicFlags.fields.frame_context_idx);
        printf("\nPicParamsVP9.PicFlags.fields.LosslessFlag                : %d = 0x%x",picParam.PicFlags.fields.LosslessFlag, picParam.PicFlags.fields.LosslessFlag);
        printf("\nPicParamsVP9.PicFlags.fields.ReservedField1                : %d = 0x%x",picParam.PicFlags.fields.ReservedField,picParam.PicFlags.fields.ReservedField);
        printf("\nPicParamsVP9.CurrPic                                    : %d = 0x%x",picParam.CurrPic, picParam.CurrPic);
        printf("\nPicParamsVP9.filter_level                                : %d = 0x%x",picParam.filter_level, picParam.filter_level);
        printf("\nPicParamsVP9.sharpness_level                            : %d = 0x%x",picParam.sharpness_level, picParam.sharpness_level);
        printf("\nPicParamsVP9.log2_tile_rows                                : %d = 0x%x",picParam.log2_tile_rows, picParam.log2_tile_rows);
        printf("\nPicParamsVP9.log2_tile_columns                            : %d = 0x%x",picParam.log2_tile_columns, picParam.log2_tile_columns);
        printf("\nPicParamsVP9.UncompressedHeaderLengthInBytes            : %d = 0x%x",picParam.UncompressedHeaderLengthInBytes, picParam.UncompressedHeaderLengthInBytes);
        printf("\nPicParamsVP9.FirstPartitionSize                            : %d = 0x%x",picParam.FirstPartitionSize, picParam.FirstPartitionSize);
        printf("\nPicParamsVP9.BSBytesInBuffer                            : %d = 0x%x",picParam.BSBytesInBuffer, picParam.BSBytesInBuffer);


        for (int i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; i++) {
            printf("\n_Segment_VP9.SegData[%d].SegmentFlags.fields.SegmentReferenceEnabled : %d = 0x%x", i, segParam.SegData[i].SegmentFlags.fields.SegmentReferenceEnabled, segParam.SegData[i].SegmentFlags.fields.SegmentReferenceEnabled);
            printf("\n_Segment_VP9.SegData[%d].SegmentFlags.fields.SegmentReference        : %d = 0x%x", i, segParam.SegData[i].SegmentFlags.fields.SegmentReference, segParam.SegData[i].SegmentFlags.fields.SegmentReference);
            printf("\n_Segment_VP9.SegData[%d].SegmentFlags.fields.SegmentReferenceSkipped : %d = 0x%x", i, segParam.SegData[i].SegmentFlags.fields.SegmentReferenceSkipped, segParam.SegData[i].SegmentFlags.fields.SegmentReferenceSkipped);
            printf("\n_Segment_VP9.SegData[%d].SegmentFlags.fields.ReservedField3        : %d = 0x%x", i, segParam.SegData[i].SegmentFlags.fields.ReservedField3, segParam.SegData[i].SegmentFlags.fields.ReservedField3);

            for (int j = 0; j < MAX_REF_LF_DELTAS; j++)
        {
                for (int k = 0; k < MAX_MODE_LF_DELTAS; k++)
                    printf("\n_Segment_VP9.SegData[%d].FilterLevel[%d][%d]            : %d = 0x%x", i, j, k, segParam.SegData[i].FilterLevel[j][k], segParam.SegData[i].FilterLevel[j][k]);

          printf("\n_Segment_VP9.SegData[%d].LumaACQuantScale            : %d = 0x%x", i, segParam.SegData[i].LumaACQuantScale, segParam.SegData[i].LumaACQuantScale);
                printf("\n_Segment_VP9.SegData[%d].LumaDCQuantScale            : %d = 0x%x", i, segParam.SegData[i].LumaDCQuantScale, segParam.SegData[i].LumaDCQuantScale);
                printf("\n_Segment_VP9.SegData[%d].ChromaACQuantScale            : %d = 0x%x", i, segParam.SegData[i].ChromaACQuantScale, segParam.SegData[i].ChromaACQuantScale);
                printf("\n_Segment_VP9.SegData[%d].ChromaDCQuantScale            : %d = 0x%x", i, segParam.SegData[i].ChromaDCQuantScale, segParam.SegData[i].ChromaDCQuantScale);
            }
        }
    }
    fflush(NULL);
}
#endif
};

#ifdef MFX_VA_LINUX
mfxStatus VideoDECODEVP9_HW::PackHeaders(mfxBitstream *bs, VP9FrameInfo const & info)
{
    if ((NULL == bs) || (NULL == bs->Data))
        return MFX_ERR_NULL_PTR;

    //picParam
    UMC::UMCVACompBuffer* pCompBuf = NULL;
    VADecPictureParameterBufferVP9 *picParam =
        (VADecPictureParameterBufferVP9*)m_va->GetCompBuffer(VAPictureParameterBufferType, &pCompBuf, sizeof(VADecPictureParameterBufferVP9));

    if (NULL == picParam)
        return MFX_ERR_MEMORY_ALLOC;

    memset(picParam, 0, sizeof(VADecPictureParameterBufferVP9));

    picParam->frame_width = info.width;
    picParam->frame_height = info.height;

//    picParam->curr_pic = m_va->GetSurfaceID(info.currFrame);
    if (KEY_FRAME == info.frameType)
        memset(picParam->reference_frames, VA_INVALID_SURFACE, sizeof(picParam->reference_frames));
    else
    {

//        memset(picParam->reference_frames, VA_INVALID_SURFACE, sizeof(picParam->reference_frames));
        for (mfxU8 ref = 0; ref < MAX_REF_FRAMES; ++ref)
            picParam->reference_frames[ref] = m_va->GetSurfaceID(info.ref_frame_map[ref]);
    }

    picParam->pic_fields.bits.subsampling_x = info.subsamplingX;
    picParam->pic_fields.bits.subsampling_y = info.subsamplingY;
    picParam->pic_fields.bits.frame_type = info.frameType;
    picParam->pic_fields.bits.show_frame = info.showFrame;
    picParam->pic_fields.bits.error_resilient_mode = info.errorResilientMode;
    picParam->pic_fields.bits.intra_only = info.intraOnly;
    picParam->pic_fields.bits.allow_high_precision_mv = info.allowHighPrecisionMv;
    picParam->pic_fields.bits.mcomp_filter_type = info.interpFilter;
    picParam->pic_fields.bits.frame_parallel_decoding_mode = info.frameParallelDecodingMode;
    picParam->pic_fields.bits.reset_frame_context = info.resetFrameContext;
    picParam->pic_fields.bits.refresh_frame_context = info.refreshFrameContext;
    picParam->pic_fields.bits.frame_context_idx = info.frameContextIdx;
    picParam->pic_fields.bits.segmentation_enabled = info.segmentation.enabled;
    picParam->pic_fields.bits.segmentation_temporal_update = info.segmentation.temporalUpdate;
    picParam->pic_fields.bits.segmentation_update_map = info.segmentation.updateMap;
    if (KEY_FRAME != info.frameType)
    {
        picParam->pic_fields.bits.last_ref_frame = info.activeRefIdx[0];
        picParam->pic_fields.bits.last_ref_frame_sign_bias = info.refFrameSignBias[LAST_FRAME];
        picParam->pic_fields.bits.golden_ref_frame = info.activeRefIdx[1];
        picParam->pic_fields.bits.golden_ref_frame_sign_bias = info.refFrameSignBias[GOLDEN_FRAME];
        picParam->pic_fields.bits.alt_ref_frame = info.activeRefIdx[2];
        picParam->pic_fields.bits.alt_ref_frame_sign_bias = info.refFrameSignBias[ALTREF_FRAME];
    }
    picParam->pic_fields.bits.lossless_flag = info.lossless;

    picParam->filter_level = info.lf.filterLevel;
    picParam->sharpness_level = info.lf.sharpnessLevel;
    picParam->log2_tile_rows = info.log2TileRows;
    picParam->log2_tile_columns = info.log2TileColumns;
    picParam->frame_header_length_in_bytes = info.frameHeaderLength;
    picParam->first_partition_size = info.firstPartitionSize;
//    picParam->frame_data_size = info.frameDataSize;

    for (mfxU8 i = 0; i < VP9_NUM_OF_SEGMENT_TREE_PROBS; ++i)
        picParam->mb_segment_tree_probs[i] = info.segmentation.treeProbs[i];

    for (mfxU8 i = 0; i < VP9_NUM_OF_PREDICTION_PROBS; ++i)
        picParam->segment_pred_probs[i] = info.segmentation.predProbs[i];

    picParam->version = info.version;
    //::DumpInfo(*picParam, m_frameOrder);

    // sliceParam
    pCompBuf = NULL;
    VASliceParameterBufferVP9 *sliceParam =
        (VASliceParameterBufferVP9*)m_va->GetCompBuffer(VASliceParameterBufferType, &pCompBuf, sizeof(VASliceParameterBufferVP9));

    if (NULL == sliceParam)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    memset(sliceParam, 0, sizeof(VASliceParameterBufferVP9));

    sliceParam->slice_data_size = info.frameDataSize;
    sliceParam->slice_data_offset = info.frameHeaderLength;
    sliceParam->slice_data_flag = VA_SLICE_DATA_FLAG_ALL;

    for (mfxU8 segmentId = 0; segmentId < VP9_MAX_NUM_OF_SEGMENTS; ++segmentId)
    {
        sliceParam->seg_param[segmentId].segment_flags.fields.segment_reference_enabled =
            IsSegFeatureActive(info.segmentation, segmentId, SEG_LVL_REF_FRAME);

        sliceParam->seg_param[segmentId].segment_flags.fields.segment_reference =
            GetSegData(info.segmentation, segmentId, SEG_LVL_REF_FRAME);

        sliceParam->seg_param[segmentId].segment_flags.fields.segment_reference_skipped =
            IsSegFeatureActive(info.segmentation, segmentId, SEG_LVL_SKIP);

        for (mfxU8 ref = INTRA_FRAME; ref < MAX_REF_FRAMES; ++ref)
            for (mfxU8 mode = 0; mode < MAX_MODE_LF_DELTAS; ++mode)
                sliceParam->seg_param[segmentId].filter_level[ref][mode] = info.lf_info.level[segmentId][ref][mode];

        const mfxI32 qIndex = GetQIndex(info.segmentation, segmentId, info.baseQIndex);
        if (qIndex < 0)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        sliceParam->seg_param[segmentId].luma_ac_quant_scale = info.yDequant[qIndex][1];
        sliceParam->seg_param[segmentId].luma_dc_quant_scale = info.yDequant[qIndex][0];
        sliceParam->seg_param[segmentId].chroma_ac_quant_scale = info.uvDequant[qIndex][1];
        sliceParam->seg_param[segmentId].chroma_dc_quant_scale = info.uvDequant[qIndex][0];

        if (!info.segmentation.enabled)
            break;
    }
    //::DumpInfo(*sliceParam);
    pCompBuf->SetDataSize(sizeof(VASliceParameterBufferVP9));


    // dataBuffer
    pCompBuf = NULL;
    mfxU8 *bistreamData = (mfxU8*)m_va->GetCompBuffer(VASliceDataBufferType, &pCompBuf, bs->DataLength);

    if (NULL == bistreamData)
        return MFX_ERR_MEMORY_ALLOC;

    memcpy(bistreamData, bs->Data + bs->DataOffset, bs->DataLength);
    pCompBuf->SetDataSize(bs->DataLength);

    return MFX_ERR_NONE;
}

#else

mfxStatus VideoDECODEVP9_HW::PackHeaders(mfxBitstream *bs, VP9FrameInfo const & info)
{
    if ((NULL == bs) || (NULL == bs->Data))
        return MFX_ERR_NULL_PTR;

    UMC::UMCVACompBuffer* compBufPic = NULL;
    DXVA_Intel_PicParams_VP9 *picParam = (DXVA_Intel_PicParams_VP9*)m_va->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS_VP9, &compBufPic);
    if ((NULL == picParam) || (NULL == compBufPic) || (compBufPic->GetBufferSize() < sizeof(DXVA_Intel_PicParams_VP9)))
        return MFX_ERR_MEMORY_ALLOC;

    memset(picParam, 0, sizeof(DXVA_Intel_PicParams_VP9));

    picParam->FrameHeightMinus1 = (USHORT)info.height - 1;
    picParam->FrameWidthMinus1 = (USHORT)info.width - 1;

    picParam->PicFlags.fields.frame_type = info.frameType;
    picParam->PicFlags.fields.show_frame = info.showFrame;
    picParam->PicFlags.fields.error_resilient_mode = info.errorResilientMode;
    picParam->PicFlags.fields.intra_only = info.intraOnly;

    if (KEY_FRAME == info.frameType || info.intraOnly)
    {
        picParam->PicFlags.fields.LastRefIdx = UCHAR_MAX;
        picParam->PicFlags.fields.LastRefSignBias = UCHAR_MAX;
        picParam->PicFlags.fields.GoldenRefIdx = UCHAR_MAX;
        picParam->PicFlags.fields.GoldenRefSignBias = UCHAR_MAX;
        picParam->PicFlags.fields.AltRefIdx = UCHAR_MAX;
        picParam->PicFlags.fields.AltRefSignBias = UCHAR_MAX;
    }
    else
    {
        picParam->PicFlags.fields.LastRefIdx = info.activeRefIdx[0];
        picParam->PicFlags.fields.LastRefSignBias = info.refFrameSignBias[LAST_FRAME];
        picParam->PicFlags.fields.GoldenRefIdx = info.activeRefIdx[1];
        picParam->PicFlags.fields.GoldenRefSignBias = info.refFrameSignBias[GOLDEN_FRAME];
        picParam->PicFlags.fields.AltRefIdx = info.activeRefIdx[2];
        picParam->PicFlags.fields.AltRefSignBias = info.refFrameSignBias[ALTREF_FRAME];
    }

    picParam->PicFlags.fields.allow_high_precision_mv = info.allowHighPrecisionMv;
    picParam->PicFlags.fields.mcomp_filter_type = info.interpFilter;
    picParam->PicFlags.fields.frame_parallel_decoding_mode = info.frameParallelDecodingMode;
    picParam->PicFlags.fields.segmentation_enabled = info.segmentation.enabled;;
    picParam->PicFlags.fields.segmentation_temporal_update = info.segmentation.temporalUpdate;
    picParam->PicFlags.fields.segmentation_update_map = info.segmentation.updateMap;
    picParam->PicFlags.fields.reset_frame_context = info.resetFrameContext;
    picParam->PicFlags.fields.refresh_frame_context = info.refreshFrameContext;
    picParam->PicFlags.fields.frame_context_idx = info.frameContextIdx;
    picParam->PicFlags.fields.LosslessFlag = info.lossless;

    if (KEY_FRAME == info.frameType)
        for (int i = 0; i < NUM_REF_FRAMES; i++)
        {
            picParam->RefFrameList[i] = UCHAR_MAX;
        }
    else
    {
        for (mfxU8 ref = 0; ref < NUM_REF_FRAMES; ++ref)
            picParam->RefFrameList[ref] = (UCHAR)info.ref_frame_map[ref];
    }

    picParam->CurrPic = (UCHAR)info.currFrame;
    picParam->filter_level = (UCHAR)info.lf.filterLevel;
    picParam->sharpness_level = (UCHAR)info.lf.sharpnessLevel;
    picParam->log2_tile_rows = (UCHAR)info.log2TileRows;
    picParam->log2_tile_columns = (UCHAR)info.log2TileColumns;
    picParam->UncompressedHeaderLengthInBytes = (UCHAR)info.frameHeaderLength;
    picParam->FirstPartitionSize = (USHORT)info.firstPartitionSize;


    for (mfxU8 i = 0; i < VP9_NUM_OF_SEGMENT_TREE_PROBS; ++i)
        picParam->mb_segment_tree_probs[i] = info.segmentation.treeProbs[i];

    for (mfxU8 i = 0; i < VP9_NUM_OF_PREDICTION_PROBS; ++i)
        picParam->segment_pred_probs[i] = info.segmentation.predProbs[i];

    picParam->BSBytesInBuffer = info.frameDataSize;
    picParam->StatusReportFeedbackNumber = m_frameOrder + 1;

    UMC::UMCVACompBuffer* compBufSeg = NULL;
    DXVA_Intel_Segment_VP9 *segParam = (DXVA_Intel_Segment_VP9*)m_va->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX_VP9, &compBufSeg);
    if ((NULL == segParam) || (NULL == compBufSeg) || (compBufSeg->GetBufferSize() < sizeof(DXVA_Intel_Segment_VP9)))
        return MFX_ERR_MEMORY_ALLOC;

    memset(segParam, 0, sizeof(DXVA_Intel_Segment_VP9));

    for (mfxU8 segmentId = 0; segmentId < VP9_MAX_NUM_OF_SEGMENTS; ++segmentId)
    {
        segParam->SegData[segmentId].SegmentFlags.fields.SegmentReferenceEnabled =
            IsSegFeatureActive(info.segmentation, segmentId, SEG_LVL_REF_FRAME);

        segParam->SegData[segmentId].SegmentFlags.fields.SegmentReference =
            GetSegData(info.segmentation, segmentId, SEG_LVL_REF_FRAME);

        segParam->SegData[segmentId].SegmentFlags.fields.SegmentReferenceSkipped =
            IsSegFeatureActive(info.segmentation, segmentId, SEG_LVL_SKIP);

        for (mfxU8 ref = INTRA_FRAME; ref < MAX_REF_FRAMES; ++ref)
            for (mfxU8 mode = 0; mode < MAX_MODE_LF_DELTAS; ++mode)
                segParam->SegData[segmentId].FilterLevel[ref][mode] = info.lf_info.level[segmentId][ref][mode];

        const mfxI32 qIndex = GetQIndex(info.segmentation, segmentId, info.baseQIndex);
        if (qIndex < 0)
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        segParam->SegData[segmentId].LumaACQuantScale = info.yDequant[qIndex][1];
        segParam->SegData[segmentId].LumaDCQuantScale = info.yDequant[qIndex][0];
        segParam->SegData[segmentId].ChromaACQuantScale = info.uvDequant[qIndex][1];
        segParam->SegData[segmentId].ChromaDCQuantScale = info.uvDequant[qIndex][0];

        //if (!info.segmentation.enabled)
        //    break;
    }

    UMC::UMCVACompBuffer* compBufBs = NULL;
    mfxU8 *bistreamData = (mfxU8 *)m_va->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_BITSTREAM_DATA_VP9, &compBufBs);
    if ((NULL == bistreamData) || (NULL == compBufBs) || (compBufBs->GetBufferSize() < (mfxI32)bs->DataLength))
        return MFX_ERR_MEMORY_ALLOC;

    memcpy(bistreamData, bs->Data + bs->DataOffset, bs->DataLength);
    compBufBs->SetDataSize(bs->DataLength);

    UMC::UMCVACompBuffer* compTmpBuff = NULL;
    void *tmpBuffer = (void*)m_va->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_COEFFICIENT_PROBABILITIES, &compTmpBuff);
    if ((NULL == tmpBuffer) || (NULL == compTmpBuff))
        return MFX_ERR_MEMORY_ALLOC;

    //::DumpInfo(*picParam,*segParam, m_frameOrder);

    return MFX_ERR_NONE;
}

#endif

#endif // MFX_ENABLE_VP9_VIDEO_DECODE_HW && MFX_VA
