//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined(MFX_ENABLE_AV1_VIDEO_DECODE)

#include "mfx_session.h"
#include "mfx_av1_dec_decode.h"

#include "mfx_task.h"

#include "mfx_common_int.h"
#include "mfx_common_decode_int.h"
#include "mfx_vpx_dec_common.h"

#include "mfx_umc_alloc_wrapper.h"

#include "umc_av1_dec_defs.h"
#include "umc_av1_utils.h"
#include "umc_av1_frame.h"

#include "libmfx_core_hw.h"
#include "umc_va_dxva2.h"

#include "umc_av1_decoder_va.h"

#include <algorithm>

namespace MFX_VPX_Utility
{
    inline
    mfxU16 MatchProfile(mfxU32 fourcc)
    {
        fourcc;
        return MFX_PROFILE_VP9_0;
    }

    inline
    bool CheckGUID(VideoCORE* core, eMFXHWType type, mfxVideoParam const* par)
    {
        mfxVideoParam vp = *par;
        mfxU16 profile = vp.mfx.CodecProfile & 0xFF;
        if (profile == MFX_PROFILE_UNKNOWN)
            vp.mfx.CodecProfile = MatchProfile(vp.mfx.FrameInfo.FourCC);

    #if defined (MFX_VA_WIN)
        mfxU32 const va_profile =
            ChooseProfile(&vp, type) & (UMC::VA_CODEC | UMC::VA_ENTRY_POINT);

        GuidProfile const
            *f =     GuidProfile::GetGuidProfiles(),
            *l = f + GuidProfile::GetGuidProfileSize();
        GuidProfile const* p =
            std::find_if(f, l,
                [&](GuidProfile const& candidate)
                {
                    return
                        (static_cast<mfxU32>(candidate.profile) & (UMC::VA_CODEC | UMC::VA_ENTRY_POINT)) == va_profile &&
                        core->IsGuidSupported(candidate.guid, &vp) == MFX_ERR_NONE;
                }
            );

        return p != l;
    #elif defined (MFX_VA_LINUX)
        return false;
    #else
        core; type; par;
        return false;
    #endif
    }

    eMFXPlatform GetPlatform(VideoCORE* core, mfxVideoParam const* par)
    {
        VM_ASSERT(core);
        VM_ASSERT(par);

        if (!par)
            return MFX_PLATFORM_SOFTWARE;

        eMFXPlatform platform = core->GetPlatformType();
        return
            platform != MFX_PLATFORM_SOFTWARE && !CheckGUID(core, core->GetHWType(), par) ?
            MFX_PLATFORM_SOFTWARE : platform;
    }
}

VideoDECODEAV1::VideoDECODEAV1(VideoCORE* core, mfxStatus* sts)
    : m_core(core)
    , m_platform(MFX_PLATFORM_SOFTWARE)
    , m_opaque(false)
    , m_first_run(true)
    , m_response()
{
    if (sts)
    {
        *sts = MFX_ERR_NONE;
    }
}

VideoDECODEAV1::~VideoDECODEAV1()
{
    Close();
}

mfxStatus VideoDECODEAV1::Init(mfxVideoParam* par)
{
    MFX_CHECK_NULL_PTR1(par);

    UMC::AutomaticUMCMutex guard(m_guard);

    if (m_decoder)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    m_platform = MFX_VPX_Utility::GetPlatform(m_core, par);
    if (!MFX_VPX_Utility::CheckVideoParam(par, MFX_CODEC_AV1, m_platform))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (m_platform == MFX_PLATFORM_SOFTWARE)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    else
    {
#if !defined (MFX_VA)
        return MFX_ERR_UNSUPPORTED;
#else
        m_decoder.reset(new UMC_AV1_DECODER::AV1DecoderVA());
        m_allocator.reset(new mfx_UMC_FrameAllocator_D3D());
#endif
    }

    mfxFrameAllocRequest request{};
    mfxStatus sts = MFX_VPX_Utility::QueryIOSurfInternal(par, &request);
    MFX_CHECK_STS(sts);

    //mfxFrameAllocResponse response{};
    bool internal = m_platform == !!(MFX_PLATFORM_SOFTWARE ?
        (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) : (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY));

    if (!(par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
    {
        if (!internal)
            request.AllocId = par->AllocId;

        sts = m_core->AllocFrames(&request, &m_response, internal);
    }
    else
    {
        auto opaq =
            reinterpret_cast<mfxExtOpaqueSurfaceAlloc*>(GetExtendedBuffer(par->ExtParam, par->NumExtParam,MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION));

        if (!opaq || request.NumFrameMin > opaq->Out.NumSurface)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        m_opaque = true;

        request.Type = MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_OPAQUE_FRAME;
        request.Type |= (opaq->Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY) ?
            MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_DXVA2_DECODER_TARGET;

        request.NumFrameMin       = opaq->Out.NumSurface;
        request.NumFrameSuggested = request.NumFrameMin;

        sts = m_core->AllocFrames(&request, &m_response,
            opaq->Out.Surfaces, opaq->Out.NumSurface);
    }

    MFX_CHECK_STS(sts);

    m_allocator->SetExternalFramesResponse(&m_response);

    UMC::Status umcSts = m_allocator->InitMfx(0, m_core, par, &request, &m_response, !internal, m_platform == MFX_PLATFORM_SOFTWARE);
    if (umcSts != UMC::UMC_OK)
        return MFX_ERR_MEMORY_ALLOC;

    UMC_AV1_DECODER::AV1DecoderParams vp{};
    vp.allocator = m_allocator.get();
    vp.async_depth = par->AsyncDepth;
    if (!vp.async_depth)
        vp.async_depth = MFX_AUTO_ASYNC_DEPTH_VALUE;

#if defined (MFX_VA)
    sts = m_core->CreateVA(par, &request, &m_response, m_allocator.get());
    MFX_CHECK_STS(sts);

    m_core->GetVA((mfxHDL*)&vp.pVideoAccelerator, MFX_MEMTYPE_FROM_DECODE);
#endif

    ConvertMFXParamsToUMC(par, &vp);

    umcSts = m_decoder->Init(&vp);
    if (umcSts != UMC::UMC_OK)
        return MFX_ERR_NOT_INITIALIZED;

    m_first_run = true;
    m_init_video_par = *par;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEAV1::Reset(mfxVideoParam* par)
{
    MFX_CHECK_NULL_PTR1(par);

    UMC::AutomaticUMCMutex guard(m_guard);

    MFX_CHECK(m_core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEAV1::Close()
{
    UMC::AutomaticUMCMutex guard(m_guard);

    MFX_CHECK(m_core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    return MFX_ERR_NONE;
}

mfxTaskThreadingPolicy VideoDECODEAV1::GetThreadingPolicy()
{
    return MFX_TASK_THREADING_SHARED;
}

mfxStatus VideoDECODEAV1::Query(VideoCORE* core, mfxVideoParam* in, mfxVideoParam* out)
{
    MFX_CHECK(core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK_NULL_PTR1(out);

    mfxStatus sts = MFX_VPX_Utility::Query(core, in, out, MFX_CODEC_AV1, core->GetHWType());
    if (sts != MFX_ERR_NONE)
        return sts;

    eMFXPlatform platform = MFX_VPX_Utility::GetPlatform(core, out);
    if (platform != core->GetPlatformType())
    {
#ifdef MFX_VA
        sts = MFX_ERR_UNSUPPORTED;
#endif
    }

    return sts;
}

mfxStatus VideoDECODEAV1::QueryIOSurf(VideoCORE* core, mfxVideoParam* par, mfxFrameAllocRequest* request)
{
    MFX_CHECK(core, MFX_ERR_UNDEFINED_BEHAVIOR)
    MFX_CHECK_NULL_PTR2(par, request);

    if (!(par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)  &&
        !(par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) &&
        !(par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if ((par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) &&
        (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if ((par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) &&
        (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if ((par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) &&
        (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxStatus sts = MFX_ERR_NONE;
    if (!(par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        sts = MFX_VPX_Utility::QueryIOSurfInternal(par, request);
    else
    {
        request->Info = par->mfx.FrameInfo;
        request->NumFrameMin = 1;
        request->NumFrameSuggested = request->NumFrameMin + (par->AsyncDepth ? par->AsyncDepth : MFX_AUTO_ASYNC_DEPTH_VALUE);
        request->Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }

    request->Type |= par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY ?
        MFX_MEMTYPE_OPAQUE_FRAME : MFX_MEMTYPE_EXTERNAL_FRAME;

    return sts;
}

mfxStatus VideoDECODEAV1::DecodeHeader(VideoCORE* core, mfxBitstream* bs, mfxVideoParam* par)
{
    MFX_CHECK(core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK_NULL_PTR3(bs, bs->Data, par);

    mfxStatus sts = MFX_ERR_NONE;
    try
    {
        MFXMediaDataAdapter in(bs);

        UMC::VideoDecoderParams vp;
        UMC::Status res = UMC_AV1_DECODER::AV1Decoder::DecodeHeader(&in, &vp);
        if (res != UMC::UMC_OK)
            return ConvertStatusUmc2Mfx(res);

        sts = FillVideoParam(core, &vp, par);
    }
    catch (UMC_AV1_DECODER::av1_exception & ex)
    {
        return ConvertStatusUmc2Mfx(ex.GetStatus());
    }

    MFX_CHECK_STS(sts);

    if (MFX_VPX_Utility::GetPlatform(core, par) != MFX_PLATFORM_SOFTWARE)
    {
        if (   par->mfx.FrameInfo.FourCC == MFX_FOURCC_P010
#ifdef PRE_SI_TARGET_PLATFORM_GEN11
            || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y210
#endif
#ifdef PRE_SI_TARGET_PLATFORM_GEN12
            || par->mfx.FrameInfo.FourCC == MFX_FOURCC_P016
            || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y216
            || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y416
#endif
            )

            par->mfx.FrameInfo.Shift = 1;
    }

    return sts;
}

mfxStatus VideoDECODEAV1::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    UMC::AutomaticUMCMutex guard(m_guard);

    return MFX_ERR_NONE;
}


mfxStatus VideoDECODEAV1::GetDecodeStat(mfxDecodeStat* stat)
{
    MFX_CHECK_NULL_PTR1(stat);

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEAV1::DecodeFrameCheck(mfxBitstream* bs, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out, MFX_ENTRY_POINT* entry_point)
{
    MFX_CHECK_NULL_PTR1(entry_point);

    UMC::AutomaticUMCMutex guard(m_guard);

    MFX_CHECK(m_core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    mfxThreadTask task;
    mfxStatus sts = SubmitFrame(bs, surface_work, surface_out, &task);
    MFX_CHECK_STS(sts);

    entry_point->pRoutine = &DecodeRoutine;
    entry_point->pCompleteProc = &CompleteProc;
    entry_point->pState = this;
    entry_point->requiredNumThreads = 1;
    entry_point->pParam = task;

    return sts;
}

mfxStatus VideoDECODEAV1::SetSkipMode(mfxSkipMode /*mode*/)
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VideoDECODEAV1::GetPayload(mfxU64* /*time_stamp*/, mfxPayload* /*pPayload*/)
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VideoDECODEAV1::SubmitFrame(mfxBitstream* bs, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out, mfxThreadTask* task)
{
    MFX_CHECK_NULL_PTR1(task);

    UMC::AutomaticUMCMutex guard(m_guard);

    MFX_CHECK(m_core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = SubmitFrame(bs, surface_work, surface_out);
    if (sts != MFX_ERR_NONE)
        return sts;

    auto info = std::make_unique<TaskInfo>();
    info->surface_work = GetOriginalSurface(surface_work);
    if (*surface_out)
        info->surface_out = GetOriginalSurface(*surface_out);

    *task =
        reinterpret_cast<mfxThreadTask>(info.release());

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEAV1::ExecuteFrame(mfxThreadTask task, mfxU32, mfxU32)
{
    MFX_CHECK_NULL_PTR1(task);

    MFX_CHECK(m_core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    UMC::Status sts = m_decoder->RunDecoding();

    return sts == UMC::UMC_WRN_INFO_NOT_READY ?
        MFX_TASK_WORKING : ConvertStatusUmc2Mfx(sts);
}

mfxStatus VideoDECODEAV1::CompleteFrame(mfxThreadTask task, mfxStatus)
{
    MFX_CHECK_NULL_PTR1(task);

    MFX_CHECK(m_core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    auto info =
        reinterpret_cast<TaskInfo*>(task);

    mfxFrameSurface1* surface_out = info->surface_out;
    MFX_CHECK(surface_out, MFX_ERR_UNDEFINED_BEHAVIOR);

    UMC::FrameMemID id = m_allocator->FindSurface(surface_out, m_opaque);
    UMC_AV1_DECODER::AV1DecoderFrame* frame =
        m_decoder->FindFrameByMemID(id);

    if (!frame)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    surface_out->Data.Corrupted = 0;
    Ipp32s const error = frame->GetError();

    if (error & UMC::ERROR_FRAME_DEVICE_FAILURE)
    {
        surface_out->Data.Corrupted |= MFX_CORRUPTION_MAJOR;
        if (error == UMC::UMC_ERR_GPU_HANG)
            return MFX_ERR_GPU_HANG;
        else
            return MFX_ERR_DEVICE_FAILED;
    }
    else
    {
        if (error & UMC::ERROR_FRAME_MINOR)
            surface_out->Data.Corrupted |= MFX_CORRUPTION_MINOR;

        if (error & UMC::ERROR_FRAME_MAJOR)
            surface_out->Data.Corrupted |= MFX_CORRUPTION_MAJOR;

        if (error & UMC::ERROR_FRAME_REFERENCE_FRAME)
            surface_out->Data.Corrupted |= MFX_CORRUPTION_REFERENCE_FRAME;

        if (error & UMC::ERROR_FRAME_DPB)
            surface_out->Data.Corrupted |= MFX_CORRUPTION_REFERENCE_LIST;

        if (error & UMC::ERROR_FRAME_RECOVERY)
            surface_out->Data.Corrupted |= MFX_CORRUPTION_MAJOR;

        if (error & UMC::ERROR_FRAME_TOP_FIELD_ABSENT)
            surface_out->Data.Corrupted |= MFX_CORRUPTION_ABSENT_TOP_FIELD;

        if (error & UMC::ERROR_FRAME_BOTTOM_FIELD_ABSENT)
            surface_out->Data.Corrupted |= MFX_CORRUPTION_ABSENT_BOTTOM_FIELD;
    }

    mfxStatus sts = m_allocator->PrepareToOutput(surface_out, id, &m_video_par, m_opaque);
#ifdef MFX_VA
    frame->Displayed(true);
#else
    frame->Reset();
#endif

    return sts;
}

mfxStatus VideoDECODEAV1::SubmitFrame(mfxBitstream* bs, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out)
{
    mfxStatus sts = CheckFrameInfoCodecs(&surface_work->Info, MFX_CODEC_AV1, m_platform != MFX_PLATFORM_SOFTWARE);
    MFX_CHECK_STS(sts);

    sts = CheckFrameData(surface_work);
    MFX_CHECK_STS(sts);

    sts = bs ? CheckBitstream(bs) : MFX_ERR_NONE;
    MFX_CHECK_STS(sts);

    if (!bs || !bs->DataLength)
    {
        return MFX_ERR_MORE_DATA;
    }

    if (surface_work->Data.Locked)
        return MFX_ERR_MORE_SURFACE;

    sts = m_allocator->SetCurrentMFXSurface(surface_work, m_opaque);
    if (sts != MFX_ERR_NONE)
        return sts;

    try
    {
        bool force = false;
        MFXMediaDataAdapter src(bs);

        for (;;)
        {
            UMC::Status umcRes = m_allocator->FindFreeSurface() != -1 ?
                m_decoder->GetFrame(bs ? &src : 0, nullptr) : UMC::UMC_ERR_NEED_FORCE_OUTPUT;

             if (umcRes == UMC::UMC_NTF_NEW_RESOLUTION ||
                 umcRes == UMC::UMC_WRN_REPOSITION_INPROGRESS ||
                 umcRes == UMC::UMC_ERR_UNSUPPORTED)
            {
                 UMC::VideoDecoderParams vp;
                 umcRes = m_decoder->GetInfo(&vp);
                 FillVideoParam(m_core, &vp, &m_video_par);
            }

            switch (umcRes)
            {
                case UMC::UMC_ERR_INVALID_STREAM:
                    umcRes = UMC::UMC_OK;
                    break;

                case UMC::UMC_NTF_NEW_RESOLUTION:
                    return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

#if defined(MFX_VA)
                case UMC::UMC_ERR_DEVICE_FAILED:
                    sts = MFX_ERR_DEVICE_FAILED;
                    break;

                case UMC::UMC_ERR_GPU_HANG:
                    sts = MFX_ERR_GPU_HANG;
                    break;
#endif
            }

            if (umcRes == UMC::UMC_WRN_REPOSITION_INPROGRESS)
            {
                if (!m_first_run)
                {
                    sts = MFX_WRN_VIDEO_PARAM_CHANGED;
                }
                else
                {
                    umcRes = UMC::UMC_OK;
                    m_first_run = false;
                }
            }

            if (umcRes == UMC::UMC_OK && m_allocator->FindFreeSurface() == -1)
            {
                sts = MFX_ERR_MORE_SURFACE;
            }

            if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || umcRes == UMC::UMC_WRN_INFO_NOT_READY || umcRes == UMC::UMC_ERR_NEED_FORCE_OUTPUT)
            {
                force = (umcRes == UMC::UMC_ERR_NEED_FORCE_OUTPUT);
                sts = umcRes == UMC::UMC_ERR_NOT_ENOUGH_BUFFER ? (mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK: MFX_WRN_DEVICE_BUSY;
            }

            if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_DATA || umcRes == UMC::UMC_ERR_SYNC)
            {
                if (!bs || bs->DataFlag == MFX_BITSTREAM_EOS)
                    force = true;
                sts = MFX_ERR_MORE_DATA;
            }


            if (umcRes != UMC::UMC_NTF_NEW_RESOLUTION)
            {
                src.Save(bs);
            }

            if (sts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
                return sts;

            //return these errors immediatelly unless we have [input == 0]
            if (sts == MFX_ERR_DEVICE_FAILED || sts == MFX_ERR_GPU_HANG)
            {
                if (!bs || bs->DataFlag == MFX_BITSTREAM_EOS)
                    force = true;
                else
                    return sts;
            }

            UMC_AV1_DECODER::AV1DecoderFrame* frame = GetFrameToDisplay();
            if (frame)
            {
                FillOutputSurface(surface_work, surface_out, frame);
                return MFX_ERR_NONE;
            }

            if (umcRes != UMC::UMC_OK)
                break;

        } // for (;;)
    }
    catch(UMC_AV1_DECODER::av1_exception const&)
    {
    }
    catch (std::bad_alloc const&)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }

    return sts;
}

mfxStatus VideoDECODEAV1::DecodeRoutine(void* state, void* param, mfxU32 uid_p, mfxU32 uid_a)
{
    auto decoder = reinterpret_cast<VideoDECODEAV1*>(state);
    if (!decoder)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    mfxThreadTask task =
        reinterpret_cast<TaskInfo*>(param);
    if (!task)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    mfxStatus sts;
    return
        sts = decoder->ExecuteFrame(task, uid_p, uid_a);
}

mfxStatus VideoDECODEAV1::CompleteProc(void* state, void* param, mfxStatus sts)
{
    auto decoder = reinterpret_cast<VideoDECODEAV1*>(state);
    if (!decoder)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

     mfxThreadTask task =
        reinterpret_cast<TaskInfo*>(param);
    if (!task)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    sts = decoder->CompleteFrame(task, sts);
    MFX_CHECK_STS(sts);

    auto info =
        reinterpret_cast<TaskInfo*>(task);
    delete info;

    return sts;
}

inline
mfxU16 color_format2bit_depth(UMC::ColorFormat format)
{
    switch (format)
    {
        case UMC::NV12:
        case UMC::YUY2:
        case UMC::YUV444: return 8;

        case UMC::P010:
        case UMC::Y210:
        case UMC::Y410:   return 10;

        case UMC::P016:
        case UMC::Y216:
        case UMC::Y416:   return 12;

        default:          return 0;
    }
}

inline
mfxU16 color_format2chroma_format(UMC::ColorFormat format)
{
    switch (format)
    {
        case UMC::NV12:
        case UMC::P010:
        case UMC::P016:   return MFX_CHROMAFORMAT_YUV420;

        case UMC::YUY2:
        case UMC::Y210:
        case UMC::Y216:   return MFX_CHROMAFORMAT_YUV422;

        case UMC::YUV444:
        case UMC::Y410:
        case UMC::Y416:   return MFX_CHROMAFORMAT_YUV444;

        default:          return MFX_CHROMAFORMAT_YUV420;
    }
}

mfxStatus VideoDECODEAV1::FillVideoParam(VideoCORE* /*core*/, UMC::VideoDecoderParams const* vp, mfxVideoParam* par)
{
    VM_ASSERT(vp);
    VM_ASSERT(par);

    mfxVideoParam p{};
    ConvertUMCParamsToMFX(vp, &p);

    p.mfx.FrameInfo.BitDepthLuma =
    p.mfx.FrameInfo.BitDepthChroma =
        color_format2bit_depth(vp->info.color_format);

    p.mfx.FrameInfo.ChromaFormat =
        color_format2chroma_format(vp->info.color_format);

    par->mfx.FrameInfo            = p.mfx.FrameInfo;
    par->mfx.CodecProfile         = p.mfx.CodecProfile;
    par->mfx.CodecLevel           = p.mfx.CodecLevel;
    par->mfx.DecodedOrder         = p.mfx.DecodedOrder;
    par->mfx.MaxDecFrameBuffering = p.mfx.MaxDecFrameBuffering;

    return MFX_ERR_NONE;
}

UMC_AV1_DECODER::AV1DecoderFrame* VideoDECODEAV1::GetFrameToDisplay()
{
    VM_ASSERT(m_decoder);

    UMC_AV1_DECODER::AV1DecoderFrame* frame
        = m_decoder->GetFrameToDisplay();

    if (!frame)
        return nullptr;

    frame->Outputted(true);
    return frame;
}

void VideoDECODEAV1::FillOutputSurface(mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out, UMC_AV1_DECODER::AV1DecoderFrame* frame)
{
    VM_ASSERT(surface_work);
    VM_ASSERT(frame);

    UMC::FrameData const* fd = frame->GetFrameData();
    VM_ASSERT(fd);

    mfxVideoParam vp;
    *surface_out = m_allocator->GetSurface(fd->GetFrameMID(), surface_work, &vp);
    if (m_opaque)
       *surface_out = m_core->GetOpaqSurface((*surface_out)->Data.MemId);

    mfxFrameSurface1* surface = *surface_out;
    VM_ASSERT(surface);

    surface->Info.FrameId.TemporalId = 0;

    UMC::VideoDataInfo const* vi = fd->GetInfo();
    VM_ASSERT(vi);
    vi;
}

#endif
