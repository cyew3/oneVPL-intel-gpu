/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"

#if defined(MFX_ENABLE_VP8_VIDEO_DECODE_HW) && (defined(MFX_VA_WIN) || defined(MFX_VA))

#include "mfx_session.h"
#include "mfx_common_decode_int.h"
#include "mfx_vp8_dec_decode.h"
#include "mfx_vp8_dec_decode_hw.h"
#include "mfx_enc_common.h"

#include "umc_va_base.h"
#include "umc_va_dxva2.h"

#include "vm_sys_info.h"

#ifndef MFX_VA_WIN

#include <va/va.h>
#include <va/va_dec_vp8.h>

#endif

#include <iostream>
#include <sstream>
#include <fstream>

#define VP8_START_CODE_FOUND(ptr) ((ptr)[0] == 0x9d && (ptr)[1] == 0x01 && (ptr)[2] == 0x2a)


void MoveBitstreamData2(mfxBitstream& bs, mfxU32 offset); // defined in SW decoder implementation

VideoDECODEVP8_HW::VideoDECODEVP8_HW(VideoCORE *p_core, mfxStatus *sts)
    : m_is_initialized(false),
      m_is_software_buffer(false),
      m_p_core(p_core),
      m_platform(MFX_PLATFORM_HARDWARE),
      m_p_video_accelerator(NULL)
{
    UMC_SET_ZERO(m_bs);
    UMC_SET_ZERO(m_frame_info);
    UMC_SET_ZERO(m_refresh_info);
    UMC_SET_ZERO(m_frameProbs);
    UMC_SET_ZERO(m_frameProbs_saved);
    UMC_SET_ZERO(m_quantInfo);
    m_pMbInfo = 0;

    curr_indx = 0;
    gold_indx = 0;
    altref_indx = 0;

    if (sts)
    {
        *sts = MFX_ERR_NONE;
    }
}

#pragma warning (disable : 4189)

bool VideoDECODEVP8_HW::CheckHardwareSupport(VideoCORE *p_core, mfxVideoParam *p_video_param)
{

    #ifdef MFX_VA_WIN

    if (p_core->IsGuidSupported(sDXVA_Intel_ModeVP8_VLD, p_video_param) != MFX_ERR_NONE)
    {
        return false;
    }

    // todo : VA API alternative ?
    #endif

    return true;

} // bool VideoDECODEVP8_HW::CheckHardwareSupport(VideoCORE *p_core, mfxVideoParam *p_video_param)

mfxStatus VideoDECODEVP8_HW::Init(mfxVideoParam *p_video_param)
{
    mfxStatus sts = MFX_ERR_NONE;
    eMFXHWType type = m_p_core->GetHWType();

    m_platform = MFX_VP8_Utility::GetPlatform(m_p_core, p_video_param);

    if (MFX_ERR_NONE > CheckVideoParamDecoders(p_video_param, m_p_core->IsExternalFrameAllocator(), type))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (false == CheckHardwareSupport(m_p_core, p_video_param))
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    // how "D3D" cound be related to VA API code !? (crappy naming!)
    m_p_frame_allocator.reset(new mfx_UMC_FrameAllocator_D3D());

   if (MFX_IOPATTERN_OUT_SYSTEM_MEMORY & p_video_param->IOPattern)
   {
        m_is_software_buffer = true;
   }

    if (MFX_VP8_Utility::CheckVideoParam(p_video_param, type) == false)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    m_on_init_video_params = *p_video_param;
    m_init_w = p_video_param->mfx.FrameInfo.Width;
    m_init_h = p_video_param->mfx.FrameInfo.Height;

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

    request.Type = MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET;
    sts = m_p_core->AllocFrames(&request, &m_response);

    MFX_CHECK_STS(sts);

    sts = m_p_core->CreateVA(&m_on_init_video_params, &request, &m_response);

    MFX_CHECK_STS(sts);

    UMC::Status umcSts = UMC::UMC_OK;
    umcSts = m_p_frame_allocator->InitMfx(0, m_p_core, p_video_param, &request, &m_response, false, false);

    if (UMC::UMC_OK != umcSts)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    m_p_core->GetVA((mfxHDL*)&m_p_video_accelerator, MFX_MEMTYPE_FROM_DECODE);

    m_frameOrder = (mfxU16)MFX_FRAMEORDER_UNKNOWN;
    m_is_initialized = true;

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8_HW::Init(mfxVideoParam *p_video_param)

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

mfxStatus VideoDECODEVP8_HW::Reset(mfxVideoParam *p_video_param)
{
    if (true == m_is_initialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    MFX_CHECK_NULL_PTR1(p_video_param);

    eMFXHWType type = m_p_core->GetHWType();

    if (MFX_ERR_NONE > CheckVideoParamDecoders(p_video_param, m_p_core->IsExternalFrameAllocator(), type))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (false == MFX_VP8_Utility::CheckVideoParam(p_video_param, type))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (!IsSameVideoParam(p_video_param, &m_on_init_video_params))
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    // need to sw acceleration
    if (m_platform != m_p_core->GetPlatformType())
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (m_p_frame_allocator->Reset() != UMC::UMC_OK)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    m_frameOrder = (mfxU16)MFX_FRAMEORDER_UNKNOWN;
    memset(&m_stat, 0, sizeof(m_stat));

    m_on_init_video_params = *p_video_param;
    m_video_params = m_on_init_video_params;

    if (false == CheckHardwareSupport(m_p_core, p_video_param))
    {
        //return MFX_WRN_PARTIAL_ACCELERATION;
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8_HW::Reset(mfxVideoParam *p_video_param)

mfxStatus VideoDECODEVP8_HW::Close()
{

    if (false == m_is_initialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    m_is_initialized = false;

    m_p_frame_allocator->Close();

    if (0 < m_response.NumFrameActual)
    {
        m_p_core->FreeFrames(&m_response);
    }

    m_is_initialized = false;
    m_is_software_buffer = false;

    m_frameOrder = (mfxU16)MFX_FRAMEORDER_UNKNOWN;

    m_p_video_accelerator = 0;

    memset(&m_stat, 0, sizeof(m_stat));

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8_HW::Close()

mfxStatus VideoDECODEVP8_HW::Query(VideoCORE *p_core, mfxVideoParam *p_in, mfxVideoParam *p_out)
{
    MFX_CHECK_NULL_PTR1(p_out);

    eMFXHWType type = p_core->GetHWType();

    #ifdef MFX_VA_WIN

    if (p_core->IsGuidSupported(sDXVA_Intel_ModeVP8_VLD, p_in) != MFX_ERR_NONE)
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    // todo : VA API alternative?!

    #endif

    return MFX_VP8_Utility::Query(p_core, p_in, p_out, type);

} // mfxStatus VideoDECODEVP8_HW::Query(VideoCORE *p_core, mfxVideoParam *p_in, mfxVideoParam *p_out)

mfxStatus VideoDECODEVP8_HW::QueryIOSurfInternal(eMFXPlatform platform, mfxVideoParam *p_params, mfxFrameAllocRequest *p_request)
{
    memcpy(&p_request->Info, &p_params->mfx.FrameInfo, sizeof(mfxFrameInfo));

    p_request->NumFrameMin = mfxU16 (4);

    p_request->NumFrameMin += p_params->AsyncDepth ? p_params->AsyncDepth : MFX_AUTO_ASYNC_DEPTH_VALUE;
    p_request->NumFrameSuggested = p_request->NumFrameMin;

    if (MFX_PLATFORM_SOFTWARE == platform)
    {
        p_request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }
    else
    {
        p_request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8::QueryIOSurfInternal(eMFXPlatform platform, mfxVideoParam *p_params, mfxFrameAllocRequest *p_request)

mfxStatus VideoDECODEVP8_HW::QueryIOSurf(VideoCORE *p_core, mfxVideoParam *p_video_param, mfxFrameAllocRequest *p_request)
{
    MFX_CHECK_NULL_PTR3(p_core, p_video_param, p_request);

    eMFXPlatform platform = MFX_VP8_Utility::GetPlatform(p_core, p_video_param);

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (MFX_PLATFORM_HARDWARE == platform)
    {
        type = p_core->GetHWType();
    }

    mfxVideoParam p_params;
    memcpy(&p_params, p_video_param, sizeof(mfxVideoParam));

    if (!(p_params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && !(p_params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if ((p_params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (p_params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    memcpy(&p_request->Info, &p_video_param->mfx.FrameInfo, sizeof(mfxFrameInfo));

    p_request->NumFrameMin = mfxU16 (5);
    p_request->NumFrameSuggested = p_request->NumFrameMin;
    p_request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;

    Ipp32s isInternalManaging = (MFX_PLATFORM_SOFTWARE == platform) ?
        (p_params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) : (p_params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

    if (isInternalManaging)
    {
        p_request->NumFrameSuggested = p_request->NumFrameMin = 4;
    }

    if (p_params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        p_request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }
    else
    {
        p_request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
    }

    #ifdef MFX_VA_WIN

    if (p_core->IsGuidSupported(sDXVA_Intel_ModeVP8_VLD, p_video_param) != MFX_ERR_NONE)
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }
    // todo : VA API alternative ?

    #endif

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8_HW::QueryIOSurf(VideoCORE *p_core, mfxVideoParam *p_par, mfxFrameAllocRequest *p_request)

mfxStatus VideoDECODEVP8_HW::DecodeFrameCheck(mfxBitstream *p_bs, mfxFrameSurface1 *p_surface_work, mfxFrameSurface1 **pp_surface_out)
{
    p_bs; p_surface_work; pp_surface_out;

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8_HW::DecodeFrameCheck(mfxBitstream *p_bs, mfxFrameSurface1 *p_surface_work, mfxFrameSurface1 **pp_surface_out)

#define VP8_START_CODE_FOUND(ptr) ((ptr)[0] == 0x9d && (ptr)[1] == 0x01 && (ptr)[2] == 0x2a)

mfxStatus VideoDECODEVP8_HW::PreDecodeFrame(mfxBitstream *p_bs, mfxFrameSurface1 *p_surface)
{
    mfxU8 *p_bitstream = p_bs->Data + p_bs->DataOffset;
    mfxU8 *p_bitstream_end = p_bs->Data + p_bs->DataOffset + p_bs->DataLength;

    while (p_bitstream < p_bitstream_end)
    {
        if (VP8_START_CODE_FOUND(p_bitstream)) // (0x9D && 0x01 && 0x2A)
        {
            break;
        }

        p_bitstream += 1;
    }

    mfxU32 width, height;

    width = ((p_bitstream[4] << 8) | p_bitstream[3]) & 0x3fff;
    height = ((p_bitstream[6] << 8) | p_bitstream[5]) & 0x3fff;

    width = (width + 15) & ~0x0f;
    height = (height + 15) & ~0x0f;

    if (0 == p_surface->Info.CropW)
    {
        p_surface->Info.CropW = m_on_init_video_params.mfx.FrameInfo.CropW;
    }

    if (0 == p_surface->Info.CropH)
    {
        p_surface->Info.CropH = m_on_init_video_params.mfx.FrameInfo.CropH;
    }

    if (m_init_w != width && m_init_h != height)
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (p_surface->Info.Width < width || p_surface->Info.Height < height)
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8_HW::PreDecodeFrame(mfxBitstream *p_bs, mfxFrameSurface1 *p_surface)

static void MoveBitstreamData(mfxBitstream& bs, mfxU32 offset)
{
    VM_ASSERT(offset <= bs.DataLength);
    bs.DataOffset += offset;
    bs.DataLength -= offset;

} // void MoveBitstreamData2(mfxBitstream& bs, mfxU32 offset)

mfxStatus VideoDECODEVP8_HW::ConstructFrame(mfxBitstream *p_in, mfxBitstream *p_out, IVF_FRAME& frame)
{
    MFX_CHECK_NULL_PTR1(p_out);

    if (0 == p_in->DataLength)
    {
        return MFX_ERR_MORE_DATA;
    }

    mfxU8 *p_bs_start = p_in->Data + p_in->DataOffset;

    if (p_out->Data)
    {
        ippFree(p_out->Data);
        p_out->DataLength = 0;
    }

    p_out->Data = (Ipp8u*)ippMalloc(p_in->DataLength);

    memcpy(p_out->Data, p_bs_start, p_in->DataLength);
    p_out->DataLength = p_in->DataLength;
    p_out->DataOffset = 0;

    frame.frame_size = p_in->DataLength;

    MoveBitstreamData(*p_in, p_in->DataLength);

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8_HW::ConstructFrame(mfxBitstream *p_in, mfxBitstream *p_out, IVF_FRAME& frame)

void VideoDECODEVP8_HW::CalculateTimeSteps(mfxFrameSurface1 *p_surface)
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

UMC::FrameMemID VideoDECODEVP8_HW::GetMemIdToUnlock()
{

    size_t size = m_frames.size();

    if (1 == size)
    {
        return -1;
    }

    UMC::FrameMemID memId = -1;
    sFrameInfo info;

    std::vector<sFrameInfo>::iterator i;

    for(i = m_frames.begin();i != m_frames.end() && (i + 1) != m_frames.end();i++)
    {

        if(i->currIndex != gold_indx && i->currIndex != altref_indx)
        {
            info = *i;
            memId = info.frmData->GetFrameMID();
            m_frames.erase(i);
            break;
        }
    }

    if(memId == -1) return -1;

    std::map<FrameData *, bool>::iterator it;

    // unlock surface
    for (it = m_surface_list.begin(); it != m_surface_list.end(); it++)
    {
        if ((*it).second == true && info.frmData == (*it).first)
        {
            (*it).second = false;

            break;
        }
    }

    return memId;
}

static mfxStatus __CDECL VP8DECODERoutine(void *p_state, void *pp_param, mfxU32 thread_number, mfxU32)
{
    p_state; pp_param; thread_number;
    return MFX_ERR_NONE;
}

static mfxStatus VP8CompleteProc(void *, void *pp_param, mfxStatus)
{
    pp_param;
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP8_HW::DecodeFrameCheck(mfxBitstream *p_bs, mfxFrameSurface1 *p_surface_work, mfxFrameSurface1 **pp_surface_out, MFX_ENTRY_POINT * p_entry_point)
{

    mfxStatus sts = MFX_ERR_NONE;

    if (false == m_is_initialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    MFX_CHECK_NULL_PTR2(p_surface_work, pp_surface_out);

    if (0 != p_surface_work->Data.Locked)
    {
        return MFX_ERR_MORE_SURFACE;
    }

    sts = CheckFrameInfoCodecs(&p_surface_work->Info, MFX_CODEC_VP8);
    MFX_CHECK_STS(sts);

    sts = CheckFrameData(p_surface_work);
    MFX_CHECK_STS(sts);

    sts = p_bs ? CheckBitstream(p_bs) : MFX_ERR_NONE;
    MFX_CHECK_STS(sts);

    if (NULL == p_bs)
    {
        return MFX_ERR_MORE_DATA;
    }

    bool show_frame;
    FrameType frame_type;

    if (0 == p_bs->DataLength)
        return MFX_ERR_MORE_DATA;

    mfxU8 *pTemp = p_bs->Data + p_bs->DataOffset;
    frame_type = (pTemp[0] & 1) ? P_PICTURE : I_PICTURE; // 1 bits
    show_frame = (pTemp[0] >> 4) & 0x1;

    if (0 == p_surface_work->Info.CropW)
    {
        p_surface_work->Info.CropW = m_on_init_video_params.mfx.FrameInfo.CropW;
    }

    if (0 == p_surface_work->Info.CropH)
    {
        p_surface_work->Info.CropH = m_on_init_video_params.mfx.FrameInfo.CropH;
    }

    if (I_PICTURE == frame_type)
    {
        sts = PreDecodeFrame(p_bs, p_surface_work);
        MFX_CHECK_STS(sts);
    }

    IVF_FRAME frame;
    memset(&frame, 0, sizeof(IVF_FRAME));

    sts = ConstructFrame(p_bs, &m_bs, frame);
    MFX_CHECK_STS(sts);

    *pp_surface_out = 0;

    sts = m_p_frame_allocator->SetCurrentMFXSurface(p_surface_work, false);
    MFX_CHECK_STS(sts);

    DecodeFrameHeader(&m_bs);

    std::map<FrameData *, bool>::iterator it;

    for (it = m_surface_list.begin(); it != m_surface_list.end(); it++)
    {
        if ((*it).second == false)
        {
            m_p_curr_frame = (*it).first;
            (*it).second = true;

            break;
        }
    }

    UMC::FrameMemID mem_id = m_p_curr_frame->GetFrameMID();

    curr_indx = mem_id;

    sFrameInfo info;
    info.frameType = m_frame_info.frameType;
    info.frmData = m_p_curr_frame;
    info.currIndex = curr_indx;
    info.goldIndex = gold_indx;
    info.altrefIndex = altref_indx;
    info.lastrefIndex = lastrefIndex;

    if (I_PICTURE == m_frame_info.frameType)
    {
        gold_indx = altref_indx = lastrefIndex = curr_indx;
    }
    else
    {
        switch (m_refresh_info.copy2Golden)
        {
            case 1:
                gold_indx = lastrefIndex;
                break;

            case 2:
                gold_indx = altref_indx;

            case 0:
            default:
                break;
        }

        switch (m_refresh_info.copy2Altref)
        {
            case 1:
                altref_indx = lastrefIndex;
                break;

            case 2:
                altref_indx = gold_indx;

            case 0:
            default:
                break;
        }

        if (0 != (m_refresh_info.refreshRefFrame & 2))
            gold_indx = curr_indx;

        if (0 != (m_refresh_info.refreshRefFrame & 1))
            altref_indx = curr_indx;

        if (m_refresh_info.refreshLastFrame)
           lastrefIndex = curr_indx;

    }

    m_frames.push_back(info);

    m_p_frame_allocator->IncreaseReference(mem_id);

    PackHeaders(&m_bs);

    if (UMC_OK == m_p_video_accelerator->BeginFrame(mem_id))
    {
        m_p_video_accelerator->Execute();

        m_p_video_accelerator->EndFrame();

    }

    *pp_surface_out = m_p_frame_allocator->GetSurface(m_p_curr_frame->GetFrameMID(), p_surface_work, &m_video_params);
    m_p_frame_allocator->PrepareToOutput(*pp_surface_out, m_p_curr_frame->GetFrameMID(), &m_on_init_video_params, false);

    mem_id = GetMemIdToUnlock();

    if (-1 != mem_id)
    {
        m_p_frame_allocator.get()->DecreaseReference(mem_id);
    }

    (*pp_surface_out)->Data.Corrupted = 0;

    (*pp_surface_out)->Data.FrameOrder = m_frameOrder;
    m_frameOrder++;

    p_entry_point->pRoutine = &VP8DECODERoutine;
    p_entry_point->pCompleteProc = &VP8CompleteProc;
    p_entry_point->pState = this;
    p_entry_point->requiredNumThreads = 1;

    return show_frame ? MFX_ERR_NONE : MFX_ERR_MORE_DATA;

} // mfxStatus VideoDECODEVP8_HW::DecodeFrameCheck(mfxBitstream *p_bs, mfxFrameSurface1 *p_surface_work, mfxFrameSurface1 **pp_surface_out, MFX_ENTRY_POINT *p_entry_point)

mfxStatus VideoDECODEVP8_HW::AllocateFrame()
{
    IppiSize size = m_frame_info.frameSize;

    VideoDataInfo info;
    info.Init(size.width, size.height, NV12, 8);

    for (Ipp32u i = 0; i < VP8_NUM_OF_REF_FRAMES; i += 1)
    {
        FrameMemID frmMID;
        Status sts = m_p_frame_allocator->Alloc(&frmMID, &info, 0);

        if (UMC_OK != sts)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }

        m_frame_data[i].Init(&info, frmMID, m_p_frame_allocator.get());

        m_surface_list.insert(std::pair<FrameData *, bool>(&m_frame_data[i], false));
    }

    FrameMemID frmMID;
    m_p_frame_allocator->Alloc(&frmMID, &info, 0);
    m_last_frame_data.Init(&info, frmMID, m_p_frame_allocator.get());

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8_HW::AllocateFrame()

void VideoDECODEVP8_HW::UpdateSegmentation(MFX_VP8_BoolDecoder &dec)
{
    Ipp32u bits;

    Ipp32s i;
    Ipp32s j;
    Ipp32s res;

    bits = dec.decode(2);

    m_frame_info.updateSegmentMap  = (Ipp8u)bits >> 1;
    m_frame_info.updateSegmentData = (Ipp8u)bits & 1;

    if (m_frame_info.updateSegmentData)
    {
        m_frame_info.segmentAbsMode = (Ipp8u)dec.decode();//VP8_DECODE_BOOL_PROB128(pBooldec, m_frame_info.segmentAbsMode);
        UMC_SET_ZERO(m_frame_info.segmentFeatureData);

        for (i = 0; i < VP8_NUM_OF_MB_FEATURES; i++)
        {
            for (j = 0; j < VP8_MAX_NUM_OF_SEGMENTS; j++)
            {
                bits = (Ipp8u)dec.decode();

                if (bits)
                {
                    bits = (Ipp8u)dec.decode(8 - i); // 7 bits for ALT_QUANT, 6 - for ALT_LOOP_FILTER; +sign
                    res = bits >> 1;

                    if (bits & 1)
                        res = -res;

                    m_frame_info.segmentFeatureData[i][j] = (Ipp8s)res;
                }
            }
        }
    }

    if (m_frame_info.updateSegmentMap)
    {
        for (i = 0; i < VP8_NUM_OF_SEGMENT_TREE_PROBS; i++)
        {
            bits = (Ipp8u)dec.decode();

            if (bits)
                m_frame_info.segmentTreeProbabilities[i] = (Ipp8u)dec.decode(8);
            else
                m_frame_info.segmentTreeProbabilities[i] = 255;
        }
    }
} // VideoDECODEVP8_HW::UpdateSegmentation()

void VideoDECODEVP8_HW::UpdateLoopFilterDeltas(MFX_VP8_BoolDecoder &dec)
{
  Ipp8u  bits;
  Ipp32s i;
  Ipp32s res;

    for (i = 0; i < VP8_NUM_OF_REF_FRAMES; i++)
    {
        if (dec.decode())
        {
            bits = (Ipp8u)dec.decode(7);
            res = bits >> 1;

            if (bits & 1)
                res = -res;

            m_frame_info.refLoopFilterDeltas[i] = (Ipp8s)res;
        }
    }

    for (i = 0; i < VP8_NUM_OF_MODE_LF_DELTAS; i++)
    {
        if (dec.decode())
        {
            bits = (Ipp8u)dec.decode(7);
            res = bits >> 1;

            if (bits & 1)
                res = -res;

            m_frame_info.modeLoopFilterDeltas[i] = (Ipp8s)res;
        }
    }
} // VideoDECODEVP8_HW::UpdateLoopFilterDeltas()

#define DECODE_DELTA_QP(dec, res, shift) \
{ \
  Ipp32u mask = (1 << (shift - 1)); \
  Ipp32s val; \
  if (bits & mask) \
  { \
    bits = (bits << 5) | dec.decode(5); \
    val  = (Ipp32s)((bits >> shift) & 0xF); \
    res  = (bits & mask) ? -val : val; \
  } \
}

static const Ipp32s vp8_quant_ac[VP8_MAX_QP + 1 + 32] =
{
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,

  4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
  20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
  36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
  52, 53, 54, 55, 56, 57, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76,
  78, 80, 82, 84, 86, 88, 90, 92, 94, 96, 98, 100, 102, 104, 106, 108,
  110, 112, 114, 116, 119, 122, 125, 128, 131, 134, 137, 140, 143, 146, 149, 152,
  155, 158, 161, 164, 167, 170, 173, 177, 181, 185, 189, 193, 197, 201, 205, 209,
  213, 217, 221, 225, 229, 234, 239, 245, 249, 254, 259, 264, 269, 274, 279, 284,

  284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284
};


static const Ipp32s vp8_quant_ac2[VP8_MAX_QP + 1 + 32] =
{
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,

  8, 8, 6*155/100, 7*155/100, 8*155/100, 9*155/100, 10*155/100, 11*155/100, 12*155/100, 13*155/100, 14*155/100, 15*155/100, 16*155/100, 17*155/100, 18*155/100, 19*155/100,
  20*155/100, 21*155/100, 22*155/100, 23*155/100, 24*155/100, 25*155/100, 26*155/100, 27*155/100, 28*155/100, 29*155/100, 30*155/100, 31*155/100, 32*155/100, 33*155/100, 34*155/100, 35*155/100,
  36*155/100, 37*155/100, 38*155/100, 39*155/100, 40*155/100, 41*155/100, 42*155/100, 43*155/100, 44*155/100, 45*155/100, 46*155/100, 47*155/100, 48*155/100, 49*155/100, 50*155/100, 51*155/100,
  52*155/100, 53*155/100, 54*155/100, 55*155/100, 56*155/100, 57*155/100, 58*155/100, 60*155/100, 62*155/100, 64*155/100, 66*155/100, 68*155/100, 70*155/100, 72*155/100, 74*155/100, 76*155/100,
  78*155/100, 80*155/100, 82*155/100, 84*155/100, 86*155/100, 88*155/100, 90*155/100, 92*155/100, 94*155/100, 96*155/100, 98*155/100, 100*155/100, 102*155/100, 104*155/100, 106*155/100, 108*155/100,
  110*155/100, 112*155/100, 114*155/100, 116*155/100, 119*155/100, 122*155/100, 125*155/100, 128*155/100, 131*155/100, 134*155/100, 137*155/100, 140*155/100, 143*155/100, 146*155/100, 149*155/100, 152*155/100,
  155*155/100, 158*155/100, 161*155/100, 164*155/100, 167*155/100, 170*155/100, 173*155/100, 177*155/100, 181*155/100, 185*155/100, 189*155/100, 193*155/100, 197*155/100, 201*155/100, 205*155/100, 209*155/100,
  213*155/100, 217*155/100, 221*155/100, 225*155/100, 229*155/100, 234*155/100, 239*155/100, 245*155/100, 249*155/100, 254*155/100, 259*155/100, 264*155/100, 269*155/100, 274*155/100, 279*155/100, 284*155/100,

  284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284*155/100, 284
};

static const Ipp32s vp8_quant_dc[VP8_MAX_QP + 1 + 32] =
{
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,

  4, 5, 6, 7, 8, 9, 10, 10, 11, 12, 13, 14, 15, 16, 17, 17,
  18, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 25, 25, 26, 27, 28,
  29, 30, 31, 32, 33, 34, 35, 36, 37, 37, 38, 39, 40, 41, 42, 43,
  44, 45, 46, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58,
  59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74,
  75, 76, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
  91, 93, 95, 96, 98, 100, 101, 102, 104, 106, 108, 110, 112, 114, 116, 118,
  122, 124, 126, 128, 130, 132, 134, 136, 138, 140, 143, 145, 148, 151, 154, 157,

  157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157
};

static const Ipp32s vp8_quant_dc2[VP8_MAX_QP + 1 + 32] =
{
  4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2, 4*2,

  4*2, 5*2, 6*2, 7*2, 8*2, 9*2, 10*2, 10*2, 11*2, 12*2, 13*2, 14*2, 15*2, 16*2, 17*2, 17*2,
  18*2, 19*2, 20*2, 20*2, 21*2, 21*2, 22*2, 22*2, 23*2, 23*2, 24*2, 25*2, 25*2, 26*2, 27*2, 28*2,
  29*2, 30*2, 31*2, 32*2, 33*2, 34*2, 35*2, 36*2, 37*2, 37*2, 38*2, 39*2, 40*2, 41*2, 42*2, 43*2,
  44*2, 45*2, 46*2, 46*2, 47*2, 48*2, 49*2, 50*2, 51*2, 52*2, 53*2, 54*2, 55*2, 56*2, 57*2, 58*2,
  59*2, 60*2, 61*2, 62*2, 63*2, 64*2, 65*2, 66*2, 67*2, 68*2, 69*2, 70*2, 71*2, 72*2, 73*2, 74*2,
  75*2, 76*2, 76*2, 77*2, 78*2, 79*2, 80*2, 81*2, 82*2, 83*2, 84*2, 85*2, 86*2, 87*2, 88*2, 89*2,
  91*2, 93*2, 95*2, 96*2, 98*2, 100*2, 101*2, 102*2, 104*2, 106*2, 108*2, 110*2, 112*2, 114*2, 116*2, 118*2,
  122*2, 124*2, 126*2, 128*2, 130*2, 132*2, 134*2, 136*2, 138*2, 140*2, 143*2, 145*2, 148*2, 151*2, 154*2, 157*2,

  157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2, 157*2
};


static const Ipp32s vp8_quant_dc_uv[VP8_MAX_QP + 1 + 32] =
{
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,

  4, 5, 6, 7, 8, 9, 10, 10, 11, 12, 13, 14, 15, 16, 17, 17,
  18, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 25, 25, 26, 27, 28,
  29, 30, 31, 32, 33, 34, 35, 36, 37, 37, 38, 39, 40, 41, 42, 43,
  44, 45, 46, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58,
  59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74,
  75, 76, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
  91, 93, 95, 96, 98, 100, 101, 102, 104, 106, 108, 110, 112, 114, 116, 118,
  122, 124, 126, 128, 130, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132,

  132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132
};


void VideoDECODEVP8_HW::DecodeInitDequantization(MFX_VP8_BoolDecoder &dec)
{
  m_quantInfo.yacQP = (Ipp32s)dec.decode(7);

  m_quantInfo.ydcDeltaQP  = 0;
  m_quantInfo.y2dcDeltaQP = 0;
  m_quantInfo.y2acDeltaQP = 0;
  m_quantInfo.uvdcDeltaQP = 0;
  m_quantInfo.uvacDeltaQP = 0;

  Ipp32u bits = (Ipp8u)dec.decode(5);

  if (bits)
  {
    DECODE_DELTA_QP(dec, m_quantInfo.ydcDeltaQP,  5)
    DECODE_DELTA_QP(dec, m_quantInfo.y2dcDeltaQP, 4)
    DECODE_DELTA_QP(dec, m_quantInfo.y2acDeltaQP, 3)
    DECODE_DELTA_QP(dec, m_quantInfo.uvdcDeltaQP, 2)
    DECODE_DELTA_QP(dec, m_quantInfo.uvacDeltaQP, 1)
  }

  Ipp32s qp;

  for(Ipp32s segment_id = 0; segment_id < VP8_MAX_NUM_OF_SEGMENTS; segment_id++)
  {
    if (m_frame_info.segmentationEnabled)
    {
      if (m_frame_info.segmentAbsMode)
        qp = m_frame_info.segmentFeatureData[VP8_ALT_QUANT][segment_id];
      else
      {
        qp = m_quantInfo.yacQP + m_frame_info.segmentFeatureData[VP8_ALT_QUANT][segment_id];
        vp8_CLIP(qp, 0, VP8_MAX_QP);
      }
    }
    else
      qp = m_quantInfo.yacQP;

    #ifndef MFX_VA_WIN

    m_quantInfo.yacQ[segment_id]  = qp;
    m_quantInfo.ydcQ[segment_id]  = qp + m_quantInfo.ydcDeltaQP;

    m_quantInfo.y2acQ[segment_id] = qp + m_quantInfo.y2acDeltaQP;
    m_quantInfo.y2dcQ[segment_id] = qp + m_quantInfo.y2dcDeltaQP;

    m_quantInfo.uvacQ[segment_id] = qp + m_quantInfo.uvacDeltaQP;
    m_quantInfo.uvdcQ[segment_id] = qp + m_quantInfo.uvdcDeltaQP;

    #else

    m_quantInfo.yacQ[segment_id]  = ::vp8_quant_ac[qp  + 16];
    m_quantInfo.ydcQ[segment_id]  = ::vp8_quant_dc[qp  + 16 + m_quantInfo.ydcDeltaQP];

    m_quantInfo.y2acQ[segment_id] = ::vp8_quant_ac2[qp + 16 + m_quantInfo.y2acDeltaQP];
    m_quantInfo.y2dcQ[segment_id] = ::vp8_quant_dc2[qp + 16 + m_quantInfo.y2dcDeltaQP];

    m_quantInfo.uvacQ[segment_id] = ::vp8_quant_ac[qp    + 16 + m_quantInfo.uvacDeltaQP];
    m_quantInfo.uvdcQ[segment_id] = ::vp8_quant_dc_uv[qp + 16 + m_quantInfo.uvdcDeltaQP];

    #endif

  }
} // VP8VideoDecoderSoftware::DecodeInitDequantization()

#define CHECK_N_REALLOC_BUFFERS \
{ \
  Ipp32u mbPerCol, mbPerRow; \
  mbPerCol = m_frame_info.frameHeight >> 4; \
  mbPerRow = m_frame_info.frameWidth  >> 4; \
  if (m_frame_info.mbPerCol * m_frame_info.mbPerRow  < mbPerRow * mbPerCol) \
  { \
    if (m_pMbInfo) \
      vp8dec_Free(m_pMbInfo); \
      \
    m_pMbInfo = (vp8_MbInfo*)vp8dec_Malloc(mbPerRow * mbPerCol * sizeof(vp8_MbInfo)); \
  } \
  if (m_frame_info.mbPerRow <  mbPerRow) \
  { \
    if (m_frame_info.blContextUp) \
      vp8dec_Free(m_frame_info.blContextUp); \
      \
    m_frame_info.blContextUp = (Ipp8u*)vp8dec_Malloc(mbPerRow * mbPerCol * sizeof(vp8_MbInfo)); \
  } \
  m_frame_info.mbPerCol = mbPerCol; \
  m_frame_info.mbPerRow = mbPerRow; \
  m_frame_info.mbStep   = mbPerRow; \
}

mfxStatus VideoDECODEVP8_HW::DecodeFrameHeader(mfxBitstream *in)
{

    mfxU8* data_in     = 0;
    mfxU8* data_in_end = 0;
    mfxU8  version;
    int width       = 0;
    int height      = 0;

    data_in = (Ipp8u*)in->Data;

    if(!data_in)
        return MFX_ERR_NULL_PTR;

    data_in_end = data_in + in->DataLength;

    //suppose that Intraframes -> I_PICTURE ( == VP8_KEY_FRAME)
    //             Interframes -> P_PICTURE
    m_frame_info.frameType = (data_in[0] & 1) ? P_PICTURE : I_PICTURE; // 1 bits
    version = (data_in[0] >> 1) & 0x7; // 3 bits
    m_frame_info.version = version;
    m_frame_info.showFrame = (data_in[0] >> 4) & 0x01; // 1 bits

    switch (version)
    {
        case 1:
        case 2:
            m_frame_info.interpolationFlags = VP8_BILINEAR_INTERP;
            break;

        case 3:
            m_frame_info.interpolationFlags = VP8_BILINEAR_INTERP | VP8_CHROMA_FULL_PEL;
            break;

        case 0:
        default:
            m_frame_info.interpolationFlags = 0;
            break;
    }

    //Ipp32u pTemp = (data_in[2] << 16) | (data_in[1] << 8) | data_in[0];
    //Ipp32u realSize = (pTemp >> 5) & 0x7FFFF;
    mfxU32 first_partition_size = (data_in[0] >> 5) |           // 19 bit
                                  (data_in[1] << 3) |
                                  (data_in[2] << 11);

    m_frame_info.firstPartitionSize = first_partition_size;
    m_frame_info.partitionSize[VP8_FIRST_PARTITION] = first_partition_size;

    data_in   += 3;

    if (!m_refresh_info.refreshProbabilities)
    {
        memcpy(&m_frameProbs, &m_frameProbs_saved, sizeof(m_frameProbs));
        memcpy(m_frameProbs.mvContexts, vp8_default_mv_contexts, sizeof(vp8_default_mv_contexts));
    }

    if (m_frame_info.frameType == I_PICTURE)  // if VP8_KEY_FRAME
    {
        if (first_partition_size > in->DataLength - 10)
            return MFX_ERR_MORE_DATA;

        if (!(VP8_START_CODE_FOUND(data_in))) // (0x9D && 0x01 && 0x2A)
            return MFX_ERR_UNKNOWN;

        width               = ((data_in[4] << 8) | data_in[3]) & 0x3FFF;
        m_frame_info.h_scale = data_in[4] >> 6;
        height              = ((data_in[6] << 8) | data_in[5]) & 0x3FFF;
        m_frame_info.v_scale = data_in[6] >> 6;

        m_frame_info.frameSize.width  = width;
        m_frame_info.frameSize.height = height;

        width  = (m_frame_info.frameSize.width  + 15) & ~0xF;
        height = (m_frame_info.frameSize.height + 15) & ~0xF;

        if (width != m_frame_info.frameWidth ||  height != m_frame_info.frameHeight)
        {
            m_frame_info.frameWidth  = (Ipp16s)width;
            m_frame_info.frameHeight = (Ipp16s)height;

            // alloc frames
            mfxStatus sts = AllocateFrame();
            MFX_CHECK_STS(sts);

            for(Ipp8u i = 0; i < VP8_NUM_OF_REF_FRAMES; i++)
            {
                m_RefFrameIndx[i] = i;
            }
        }

        CHECK_N_REALLOC_BUFFERS;

        data_in += 7;

        memcpy((Ipp8u*)(m_frameProbs.coeff_probs),
               (Ipp8u*)vp8_default_coeff_probs,
               sizeof(vp8_default_coeff_probs)); //???

        UMC_SET_ZERO(m_frame_info.segmentFeatureData);
        m_frame_info.segmentAbsMode = 0;

        UMC_SET_ZERO(m_frame_info.refLoopFilterDeltas);
        UMC_SET_ZERO(m_frame_info.modeLoopFilterDeltas);

        m_refresh_info.refreshRefFrame = 3; // refresh alt+gold
        m_refresh_info.copy2Golden = 0;
        m_refresh_info.copy2Altref = 0;

        // restore default probabilities for Inter frames
        for (int i = 0; i < VP8_NUM_MB_MODES_Y - 1; i++)
            m_frameProbs.mbModeProbY[i] = vp8_mb_mode_y_probs[i];

        for (int i = 0; i < VP8_NUM_MB_MODES_UV - 1; i++)
            m_frameProbs.mbModeProbUV[i] = vp8_mb_mode_uv_probs[i];

        // restore default MV contexts
        memcpy(m_frameProbs.mvContexts, vp8_default_mv_contexts, sizeof(vp8_default_mv_contexts));

    }

    m_boolDecoder[VP8_FIRST_PARTITION].init(data_in, (Ipp32s) (data_in_end - data_in));

    if (m_frame_info.frameType == I_PICTURE)  // if VP8_KEY_FRAME
    {
        Ipp32u bits = m_boolDecoder[VP8_FIRST_PARTITION].decode(2);

        m_frame_info.color_space_type = (Ipp8u)(bits >> 1);
        m_frame_info.clamping_type    = (Ipp8u)(bits & 1);

        // supported only color_space_type == 0
        // see "VP8 Data Format and Decoding Guide" ch.9.2
        if(m_frame_info.color_space_type)
            return MFX_ERR_UNSUPPORTED;
    }

    m_frame_info.segmentationEnabled = (Ipp8u)m_boolDecoder[VP8_FIRST_PARTITION].decode();

    if (m_frame_info.segmentationEnabled)
    {
        UpdateSegmentation(m_boolDecoder[VP8_FIRST_PARTITION]);
    }
    else
    {
        m_frame_info.updateSegmentData = 0;
        m_frame_info.updateSegmentMap  = 0;
    }

    mfxU8 bits = (Ipp8u)m_boolDecoder[VP8_FIRST_PARTITION].decode(7);

    m_frame_info.loopFilterType  = bits >>    6;
    m_frame_info.loopFilterLevel = bits  & 0x3F;

    bits = (Ipp8u)m_boolDecoder[VP8_FIRST_PARTITION].decode(4);

    m_frame_info.sharpnessLevel     = bits >> 1;
    m_frame_info.mbLoopFilterAdjust = bits  & 1;

    m_frame_info.modeRefLoopFilterDeltaUpdate = 0;

    if (m_frame_info.mbLoopFilterAdjust)
    {


        m_frame_info.modeRefLoopFilterDeltaUpdate = (Ipp8u)m_boolDecoder[VP8_FIRST_PARTITION].decode();
        if (m_frame_info.modeRefLoopFilterDeltaUpdate)
        {
            UpdateLoopFilterDeltas(m_boolDecoder[VP8_FIRST_PARTITION]);
        }
    }

    mfxU32 partitions;

    bits = (Ipp8u)m_boolDecoder[VP8_FIRST_PARTITION].decode(2);

    m_CodedCoeffTokenPartition = bits;

    partitions                     = 1 << bits;
    m_frame_info.numTokenPartitions = 1 << bits;

    m_frame_info.numPartitions = m_frame_info.numTokenPartitions;// + 1; // ??? do we need 1st partition here?
    partitions =  m_frame_info.numPartitions;
    mfxU8 *pTokenPartition = data_in + first_partition_size;

    if (partitions > 1)
    {
        m_frame_info.partitionStart[0] = pTokenPartition + (partitions - 1) * 3;

        for (Ipp32u i = 0; i < partitions - 1; i++)
        {
            m_frame_info.partitionSize[i] = (Ipp32s)(pTokenPartition[0])      |
                                             (pTokenPartition[1] << 8) |
                                             (pTokenPartition[2] << 16);
            pTokenPartition += 3;

            m_frame_info.partitionStart[i+1] = m_frame_info.partitionStart[i] + m_frame_info.partitionSize[i];

            if (m_frame_info.partitionStart[i+1] > data_in_end)
                return MFX_ERR_MORE_DATA; //???

            m_boolDecoder[i + 1].init(m_frame_info.partitionStart[i], m_frame_info.partitionSize[i]);
        }
    }
    else
    {
        m_frame_info.partitionStart[0] = pTokenPartition;
    }

    m_frame_info.partitionSize[partitions - 1] = Ipp32s(data_in_end - m_frame_info.partitionStart[partitions - 1]);

    m_boolDecoder[partitions].init(m_frame_info.partitionStart[partitions - 1], m_frame_info.partitionSize[partitions - 1]);

    DecodeInitDequantization(m_boolDecoder[VP8_FIRST_PARTITION]);

    if (m_frame_info.frameType != I_PICTURE) // data in header for non-KEY frames
    {
        m_refresh_info.refreshRefFrame = (Ipp8u)m_boolDecoder[VP8_FIRST_PARTITION].decode(2);

        if (!(m_refresh_info.refreshRefFrame & 2))
            m_refresh_info.copy2Golden = (Ipp8u)m_boolDecoder[VP8_FIRST_PARTITION].decode(2);

        if (!(m_refresh_info.refreshRefFrame & 1))
            m_refresh_info.copy2Altref = (Ipp8u)m_boolDecoder[VP8_FIRST_PARTITION].decode(2);

        Ipp8u bias = (Ipp8u)m_boolDecoder[VP8_FIRST_PARTITION].decode(2);

        m_refresh_info.refFrameBiasTable[1] = (bias & 1)^(bias >> 1); // ALTREF and GOLD (3^2 = 1)
        m_refresh_info.refFrameBiasTable[2] = (bias & 1);             // ALTREF and LAST
        m_refresh_info.refFrameBiasTable[3] = (bias >> 1);            // GOLD and LAST
    }

    m_refresh_info.refreshProbabilities = (Ipp8u)m_boolDecoder[VP8_FIRST_PARTITION].decode();

    if (!m_refresh_info.refreshProbabilities)
        memcpy(&m_frameProbs_saved, &m_frameProbs, sizeof(m_frameProbs));

    if (m_frame_info.frameType != I_PICTURE)
    {
        m_refresh_info.refreshLastFrame = (Ipp8u)m_boolDecoder[VP8_FIRST_PARTITION].decode();
    }
    else
        m_refresh_info.refreshLastFrame = 1;

    for (int i = 0; i < VP8_NUM_COEFF_PLANES; i++)
    {
        for (int j = 0; j < VP8_NUM_COEFF_BANDS; j++)
        {
            for (int k = 0; k < VP8_NUM_LOCAL_COMPLEXITIES; k++)
            {
                for (int l = 0; l < VP8_NUM_COEFF_NODES; l++)
                {
                    mfxU8 prob = vp8_coeff_update_probs[i][j][k][l];
                    mfxU8 flag = (Ipp8u)m_boolDecoder[VP8_FIRST_PARTITION].decode(1, prob);

                    if (flag)
                        m_frameProbs.coeff_probs[i][j][k][l] = (Ipp8u)m_boolDecoder[VP8_FIRST_PARTITION].decode(8);
                }
            }
        }
    }

    memset(m_frame_info.blContextUp, 0, m_frame_info.mbPerRow*9);

    m_frame_info.mbSkipEnabled = (Ipp8u)m_boolDecoder[VP8_FIRST_PARTITION].decode();
    m_frame_info.skipFalseProb = 0;

    if (m_frame_info.mbSkipEnabled)
        m_frame_info.skipFalseProb = (Ipp8u)m_boolDecoder[VP8_FIRST_PARTITION].decode(8);

    if (m_frame_info.frameType != I_PICTURE)
    {
        m_frame_info.intraProb = (Ipp8u)m_boolDecoder[VP8_FIRST_PARTITION].decode(8);
        m_frame_info.lastProb  = (Ipp8u)m_boolDecoder[VP8_FIRST_PARTITION].decode(8);
        m_frame_info.goldProb  = (Ipp8u)m_boolDecoder[VP8_FIRST_PARTITION].decode(8);

        if (m_boolDecoder[VP8_FIRST_PARTITION].decode())
        {
            int i = 0;

            do
            {
                m_frameProbs.mbModeProbY[i] = m_boolDecoder[VP8_FIRST_PARTITION].decode(8);
            }
            while (++i < 4);
        }

        if (m_boolDecoder[VP8_FIRST_PARTITION].decode())
        {
            int i = 0;

            do
            {
                m_frameProbs.mbModeProbUV[i] = m_boolDecoder[VP8_FIRST_PARTITION].decode(8);
            }
            while (++i < 3);
        }

        int i = 0;

        do
        {
            mfxU8 *up = (Ipp8u *)&vp8_mv_update_probs[i];
            mfxU8 *p = m_frameProbs.mvContexts[i];
            mfxU8 *pstop = p + 19;

            do
            {
                if (m_boolDecoder[VP8_FIRST_PARTITION].decode(1, *up++))
                {
                    const Ipp8u x = (Ipp8u)m_boolDecoder[VP8_FIRST_PARTITION].decode(7);

                    *p = x ? x << 1 : 1;
                }
            }
            while (++p < pstop);
        }
        while (++i < 2);
    }
    else
    {
        //m_frame_info.intraProb = 0;
        //m_frame_info.lastProb = 0;
        //m_frame_info.goldProb = 0;
    }

    int frame_data_offset = m_boolDecoder[VP8_FIRST_PARTITION].pos() +
        ((m_frame_info.frameType == I_PICTURE) ? 10 : 3);
    m_frame_info.entropyDecSize = frame_data_offset * 8 - 16 - m_boolDecoder[VP8_FIRST_PARTITION].bitcount();

    //set to zero Mb coeffs
    for (mfxU32 i = 0; i < m_frame_info.mbPerCol; i++)
    {
        for (mfxU32 j = 0; j < m_frame_info.mbPerRow; j++)
        {
            UMC_SET_ZERO(m_pMbInfo[i*m_frame_info.mbStep + j].coeffs);
        }
    }

    #ifdef MFX_VA_WIN

    unsigned char P0EntropyCount = m_boolDecoder[VP8_FIRST_PARTITION].bitcount();
    UINT offset_counter = ((P0EntropyCount & 0x18) >> 3) + (((P0EntropyCount & 0x07) != 0) ? 1 : 0);

    // + Working DDI .40

    m_frame_info.firstPartitionSize += offset_counter;

    if (m_frame_info.frameType == I_PICTURE)
    {
        m_frame_info.firstPartitionSize = m_frame_info.firstPartitionSize - (m_boolDecoder[VP8_FIRST_PARTITION].input() - ((Ipp8u*)in->Data) - 10) + 2;
        m_frame_info.entropyDecSize = m_frame_info.entropyDecSize / 8 - 10;
    }
    else
    {
        m_frame_info.firstPartitionSize = m_frame_info.firstPartitionSize - (m_boolDecoder[VP8_FIRST_PARTITION].input() - ((Ipp8u*)in->Data) - 3) + 2;
        m_frame_info.entropyDecSize = m_frame_info.entropyDecSize / 8 - 3;
    }

    #endif // MFX_VA_WIN

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP8_HW::GetFrame(MediaData* /*in*/, FrameData** /*out*/)
{
    return MFX_ERR_NONE;
}

mfxTaskThreadingPolicy VideoDECODEVP8_HW::GetThreadingPolicy()
{
    return MFX_TASK_THREADING_DEDICATED_WAIT;
}

mfxStatus VideoDECODEVP8_HW::GetVideoParam(mfxVideoParam *pPar)
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

mfxStatus VideoDECODEVP8_HW::GetDecodeStat(mfxDecodeStat *pStat)
{
    if (!m_is_initialized)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(pStat);

    m_stat.NumSkippedFrame = 0;
    m_stat.NumCachedFrame = 0;

    memcpy(pStat, &m_stat, sizeof(m_stat));
    return MFX_ERR_NONE;

}

mfxStatus VideoDECODEVP8_HW::DecodeFrame(mfxBitstream *, mfxFrameSurface1 *, mfxFrameSurface1 *)
{
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP8_HW::GetUserData(mfxU8 *pUserData, mfxU32 *pSize, mfxU64 *pTimeStamp)
{
    if (!m_is_initialized)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR3(pUserData, pSize, pTimeStamp);

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VideoDECODEVP8_HW::GetPayload(mfxU64 *pTimeStamp, mfxPayload *pPayload)
{
    if (!m_is_initialized)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR3(pTimeStamp, pPayload, pPayload->Data);

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VideoDECODEVP8_HW::SetSkipMode(mfxSkipMode /*mode*/)
{
    if (!m_is_initialized)
        return MFX_ERR_NOT_INITIALIZED;

    return MFX_ERR_NONE;
}

#ifdef MFX_VA_WIN

// Todo: maybe, move VAAPI and DXVA2 implementations to separate files

mfxStatus VideoDECODEVP8_HW::PackHeaders(mfxBitstream *p_bistream)
{

    UMCVACompBuffer* compBufPic;
    VP8_DECODE_PICTURE_PARAMETERS *picParams = (VP8_DECODE_PICTURE_PARAMETERS*)m_p_video_accelerator->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS, &compBufPic);
    memset(picParams, 0, sizeof(VP8_DECODE_PICTURE_PARAMETERS));

    picParams->wFrameWidthInMbsMinus1 = (USHORT)(((m_frame_info.frameSize.width + 15) / 16) - 1);
    picParams->wFrameHeightInMbsMinus1 = (USHORT)(((m_frame_info.frameSize.height + 15) / 16) - 1);

    sFrameInfo info = m_frames.back();

    picParams->CurrPicIndex = (UCHAR)info.currIndex;

    char cStr[256];

    if (I_PICTURE == m_frame_info.frameType)
    {
        picParams->key_frame = 1;

        picParams->LastRefPicIndex = (UCHAR)info.currIndex;
        picParams->GoldenRefPicIndex = (UCHAR)info.currIndex;
        picParams->AltRefPicIndex = (UCHAR)info.currIndex;
        picParams->DeblockedPicIndex = (UCHAR)info.currIndex;

        sprintf(cStr, "key: cpi %d\n", picParams->CurrPicIndex);
    }
    else // inter frame
    {
        picParams->key_frame = 0;

        picParams->LastRefPicIndex = (UCHAR)info.lastrefIndex;
        picParams->GoldenRefPicIndex = (UCHAR)info.goldIndex;
        picParams->AltRefPicIndex = (UCHAR)info.altrefIndex;
        picParams->DeblockedPicIndex = (UCHAR)info.currIndex;

        sprintf(cStr, "inter: cpi %d | lf %d | gf %d | altf %d\n", picParams->CurrPicIndex,
            picParams->LastRefPicIndex,
            picParams->GoldenRefPicIndex,
            picParams->AltRefPicIndex);
    }

    curr_indx += 1;

    OutputDebugStringA(cStr);

    picParams->version = m_frame_info.version;
    picParams->segmentation_enabled = m_frame_info.segmentationEnabled;
    picParams->update_mb_segmentation_map = m_frame_info.updateSegmentMap;
    picParams->update_segment_feature_data = m_frame_info.updateSegmentData;

    picParams->filter_type = m_frame_info.loopFilterType;

    if (I_PICTURE != m_frame_info.frameType)
    {
        picParams->sign_bias_golden = m_refresh_info.refFrameBiasTable[3];
        picParams->sign_bias_alternate = m_refresh_info.refFrameBiasTable[2];
    }

    picParams->mb_no_coeff_skip = m_frame_info.mbSkipEnabled;
    picParams->loop_filter_adj_enable = m_frame_info.mbLoopFilterAdjust;
    picParams->mode_ref_lf_delta_update = m_frame_info.modeRefLoopFilterDeltaUpdate;

    for (Ipp32u i = 0; i < VP8_NUM_OF_REF_FRAMES; i += 1)
    {
        picParams->ref_lf_delta[i] = m_frame_info.refLoopFilterDeltas[i];
    }

    for (Ipp32u i = 0; i < VP8_NUM_OF_MODE_LF_DELTAS; i += 1)
    {
        picParams->mode_lf_delta[i] = m_frame_info.modeLoopFilterDeltas[i];
    }

    /*

    // partition 0 is always MB header this partition always exists. there is no need for a flag to indicate this.
    // if CodedCoeffTokenPartition == 0, it means Partition #1 also exists. That is, there is a total of 2 partitions.
    // if CodedCoeffTokenPartition == 1, it means Partition #1-2 also exists. That is, there is a total of 3 partitions.
    // if CodedCoeffTokenPartition == 2, it means Partition #1-4 also exists. That is, there is a total of 5 partitions.
    // if CodedCoeffTokenPartition == 3, it means Partition #1-8 also exists. That is, there is a total of 9 partitions.

    //picParams->CodedCoeffTokenPartition = m_frame_info.numPartitions - 1; // or m_frame_info.numTokenPartitions

    */

    picParams->CodedCoeffTokenPartition = m_CodedCoeffTokenPartition;

    if (m_frame_info.segmentationEnabled)
    {

        for (int i = 0; i < 4; i++)
        {
            if (m_frame_info.segmentAbsMode)
                picParams->loop_filter_level[i] = m_frame_info.segmentFeatureData[VP8_ALT_LOOP_FILTER][i];
            else
            {
                picParams->loop_filter_level[i] = m_frame_info.loopFilterLevel + m_frame_info.segmentFeatureData[VP8_ALT_LOOP_FILTER][i];
                picParams->loop_filter_level[i] = (picParams->loop_filter_level[i] >= 0) ?
                    ((picParams->loop_filter_level[i] <= 63) ? picParams->loop_filter_level[i] : 63) : 0;
            }
        }

    }
    else
    {
        picParams->loop_filter_level[0] = m_frame_info.loopFilterLevel;
        picParams->loop_filter_level[1] = m_frame_info.loopFilterLevel;
        picParams->loop_filter_level[2] = m_frame_info.loopFilterLevel;
        picParams->loop_filter_level[3] = m_frame_info.loopFilterLevel;
    }

    picParams->LoopFilterDisable = 0;
    if (0 == m_frame_info.loopFilterLevel || (2 == m_frame_info.version || 3 == m_frame_info.version))
    {
        picParams->LoopFilterDisable = 1;
    }

    picParams->sharpness_level = m_frame_info.sharpnessLevel;

    picParams->mb_segment_tree_probs[0] = m_frame_info.segmentTreeProbabilities[0];
    picParams->mb_segment_tree_probs[1] = m_frame_info.segmentTreeProbabilities[1];
    picParams->mb_segment_tree_probs[2] = m_frame_info.segmentTreeProbabilities[2];

    picParams->prob_skip_false = m_frame_info.skipFalseProb;
    picParams->prob_intra = m_frame_info.intraProb;
    picParams->prob_last = m_frame_info.lastProb;
    picParams->prob_golden = m_frame_info.goldProb;

    const mfxU8 *prob_y_table;
    const mfxU8 *prob_uv_table;

    if (I_PICTURE == m_frame_info.frameType)
    {
        prob_y_table = vp8_kf_mb_mode_y_probs;
        prob_uv_table = vp8_kf_mb_mode_uv_probs;
    }
    else
    {
        prob_y_table = m_frameProbs.mbModeProbY;
        prob_uv_table = m_frameProbs.mbModeProbUV;
    }

    for (Ipp32u i = 0; i < VP8_NUM_MB_MODES_Y - 1; i += 1)
    {
        picParams->y_mode_probs[i] = prob_y_table[i];
    }

    for (Ipp32u i = 0; i < VP8_NUM_MB_MODES_UV - 1; i += 1)
    {
        picParams->uv_mode_probs[i] = prob_uv_table[i];
    }

    for (Ipp32u i = 0; i < VP8_NUM_MV_PROBS; i += 1)
    {
        picParams->mv_update_prob[0][i] = m_frameProbs.mvContexts[0][i];
        picParams->mv_update_prob[1][i] = m_frameProbs.mvContexts[1][i];
    }

    memset(picParams->PartitionSize, 0, sizeof(Ipp32u)* 9);

    picParams->PartitionSize[0] = m_frame_info.firstPartitionSize;

    for (Ipp32s i = 1; i < m_frame_info.numPartitions + 1; i += 1)
    {
        picParams->PartitionSize[i] = m_frame_info.partitionSize[i - 1];
    }

    picParams->FirstMbBitOffset = m_frame_info.entropyDecSize;
    picParams->P0EntropyCount = (UCHAR)m_boolDecoder[0].bitcount();
    picParams->P0EntropyRange = (UINT)m_boolDecoder[0].range();
    picParams->P0EntropyValue = (UCHAR)(m_boolDecoder[0].value() >> 24);

    UCHAR P0EntropyCount = picParams->P0EntropyCount;

    picParams->P0EntropyCount = 8 - (picParams->P0EntropyCount & 0x07);

    /*{
        static int i = 0;
        std::stringstream ss;
        ss << "c:/temp/mfx" << i;
        std::ofstream ofs(ss.str(), std::ofstream::binary);
        i++;
        ofs.write((char*)picParams, sizeof(VP8_DECODE_PICTURE_PARAMETERS));
    }*/

    compBufPic->SetDataSize(sizeof(VP8_DECODE_PICTURE_PARAMETERS));

    ////////////////////////////////////////////////////////////////

    UMCVACompBuffer* compBufQm;
    VP8_DECODE_QM_TABLE *qmTable = (VP8_DECODE_QM_TABLE*)m_p_video_accelerator->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX, &compBufQm);

    if (0 == m_frame_info.segmentationEnabled)
    {
        // when segmentation is disabled, use the first entry 0 for the quantization values
        qmTable->Qvalue[0][0] = (USHORT)m_quantInfo.ydcQ[0];
        qmTable->Qvalue[0][1] = (USHORT)m_quantInfo.yacQ[0];
        qmTable->Qvalue[0][2] = (USHORT)m_quantInfo.uvdcQ[0];
        qmTable->Qvalue[0][3] = (USHORT)m_quantInfo.uvacQ[0];
        qmTable->Qvalue[0][4] = (USHORT)m_quantInfo.y2dcQ[0];
        qmTable->Qvalue[0][5] = (USHORT)m_quantInfo.y2acQ[0];
    }
    else
    {
        for (Ipp32u i = 0; i < 4; i += 1)
        {
            qmTable->Qvalue[i][0] = (USHORT)m_quantInfo.ydcQ[i];
            qmTable->Qvalue[i][1] = (USHORT)m_quantInfo.yacQ[i];
            qmTable->Qvalue[i][2] = (USHORT)m_quantInfo.uvdcQ[i];
            qmTable->Qvalue[i][3] = (USHORT)m_quantInfo.uvacQ[i];
            qmTable->Qvalue[i][4] = (USHORT)m_quantInfo.y2dcQ[i];
            qmTable->Qvalue[i][5] = (USHORT)m_quantInfo.y2acQ[i];
        }
    }

    compBufQm->SetDataSize(sizeof(VP8_DECODE_QM_TABLE));

    /*{
        static int i = 0;
        std::stringstream ss;
        ss << "c:/temp/mfxqm" << i;
        std::ofstream ofs(ss.str(), std::ofstream::binary);
        i++;
        ofs.write((char*)qmTable, sizeof(VP8_DECODE_QM_TABLE));
    }*/

    UMCVACompBuffer* compBufBs;
    Ipp8u *bistreamData = (Ipp8u *)m_p_video_accelerator->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_BITSTREAM_DATA, &compBufBs);
    Ipp8u *pBuffer = (Ipp8u*)p_bistream->Data;
    Ipp32s size = p_bistream->DataLength;
    Ipp32u offset = 0;

    if (I_PICTURE == m_frame_info.frameType)
    {
        offset = 10;
    }
    else
    {
        offset = 3;
    }

    memcpy(bistreamData, pBuffer + offset, size - offset);
    compBufBs->SetDataSize((Ipp32s)size - offset);

    UMCVACompBuffer* compBufCp;
    Ipp8u(*coeffProbs)[8][3][11] = (Ipp8u(*)[8][3][11])m_p_video_accelerator->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_VP8_COEFFICIENT_PROBABILITIES, &compBufCp);

    // [4][8][3][11]
    memcpy(coeffProbs, m_frameProbs.coeff_probs, sizeof(Ipp8u)* 4 * 8 * 3 * 11);


    /*{
         static int i = 0;
         std::stringstream ss;
         ss << "c:/temp/mfxcp" << i;
         std::ofstream ofs(ss.str(), std::ofstream::binary);
         i++;
         ofs.write((char*)coeffProbs, sizeof(Ipp8u)* 4 * 8 * 3 * 11);
    }*/

    compBufCp->SetDataSize(sizeof(Ipp8u)* 4 * 8 * 3 * 11);

    return MFX_ERR_NONE;

} // Status VP8VideoDecoderHardware::PackHeaders(MediaData* src)

#else

mfxStatus VideoDECODEVP8_HW::PackHeaders(mfxBitstream *p_bistream)
{

     mfxStatus sts = MFX_ERR_NONE;

     sFrameInfo info = m_frames.back();

   /////////////////////////////////////////////////////////////////////////////////////////
     UMCVACompBuffer* compBufPic;
     VAPictureParameterBufferVP8 *picParams
         = (VAPictureParameterBufferVP8*)m_p_video_accelerator->GetCompBuffer(VAPictureParameterBufferType, &compBufPic,
                                                                              sizeof(VAPictureParameterBufferVP8));

     //frame width in pixels
     picParams->frame_width = m_frame_info.frameSize.width;
     //frame height in pixels
     picParams->frame_height = m_frame_info.frameSize.height;

     //specifies the "last" reference frame
     picParams->last_ref_frame = 0xffffffff;

     //specifies the "golden" reference frame
     picParams->golden_ref_frame = 0xffffffff;

     //specifies the "alternate" referrence frame
     picParams->alt_ref_frame = 0xffffffff;

     // specifies the out-of-loop deblocked frame, not used currently
     picParams->out_of_loop_frame = 0xffffffff;

     picParams->pic_fields.value = 0;

     static int refI = 0;


     if (I_PICTURE == m_frame_info.frameType)
     {
         refI = info.currIndex;
         //same as key_frame in bitstream syntax
         picParams->pic_fields.bits.key_frame = 0;
     }
     else // inter frame
     {

         picParams->pic_fields.bits.key_frame = 1;

         picParams->last_ref_frame    =  m_p_video_accelerator->GetSurfaceID(lastrefIndex);
         picParams->golden_ref_frame  =  m_p_video_accelerator->GetSurfaceID(info.goldIndex);
         picParams->alt_ref_frame     =  m_p_video_accelerator->GetSurfaceID(info.altrefIndex);

         picParams->out_of_loop_frame =  0xffffffff;
     }

     //same as version in bitstream syntax
     picParams->pic_fields.bits.version = m_frame_info.version;

     //same as segmentation_enabled in bitstream syntax
     picParams->pic_fields.bits.segmentation_enabled = m_frame_info.segmentationEnabled;

     //same as update_mb_segmentation_map in bitstream syntax
     picParams->pic_fields.bits.update_mb_segmentation_map = m_frame_info.updateSegmentMap;

     //same as update_segment_feature_data in bitstream syntax
     picParams->pic_fields.bits.update_segment_feature_data = m_frame_info.updateSegmentData;

     //same as filter_type in bitstream syntax
     picParams->pic_fields.bits.filter_type = m_frame_info.loopFilterType;

     //same as sharpness_level in bitstream syntax
     picParams->pic_fields.bits.sharpness_level = m_frame_info.sharpnessLevel;

     //same as loop_filter_adj_enable in bitstream syntax
     picParams->pic_fields.bits.loop_filter_adj_enable = m_frame_info.mbLoopFilterAdjust;

     //same as mode_ref_lf_delta_update in bitstream syntax
     picParams->pic_fields.bits.mode_ref_lf_delta_update =  m_frame_info.modeRefLoopFilterDeltaUpdate;

     //same as sign_bias_golden in bitstream syntax
     picParams->pic_fields.bits.sign_bias_golden = 0;

     //same as sign_bias_alternate in bitstream syntax
     picParams->pic_fields.bits.sign_bias_alternate = 0;

     if (I_PICTURE != m_frame_info.frameType)
     {
         picParams->pic_fields.bits.sign_bias_golden = m_refresh_info.refFrameBiasTable[3];
         picParams->pic_fields.bits.sign_bias_alternate = m_refresh_info.refFrameBiasTable[2];
     }

     //same as mb_no_coeff_skip in bitstream syntax
     picParams->pic_fields.bits.mb_no_coeff_skip = m_frame_info.mbSkipEnabled;

     //see section 11.1 for mb_skip_coeff
     picParams->pic_fields.bits.mb_skip_coeff = 0; //?

     //flag to indicate that loop filter should be disabled
     picParams->pic_fields.bits.loop_filter_disable = 0;

     if ((picParams->pic_fields.bits.version == 0) || (picParams->pic_fields.bits.version == 1))
     {
         picParams->pic_fields.bits.loop_filter_disable = m_frame_info.loopFilterLevel > 0 ? 1 : 0;
     }

     // probabilities of the segment_id decoding tree and same as
     // mb_segment_tree_probs in the spec.
     picParams->mb_segment_tree_probs[0] = m_frame_info.segmentTreeProbabilities[0];
     picParams->mb_segment_tree_probs[1] = m_frame_info.segmentTreeProbabilities[1];
     picParams->mb_segment_tree_probs[2] = m_frame_info.segmentTreeProbabilities[2];

     //Post-adjustment loop filter levels for the 4 segments
     // TO DO


     int baseline_filter_level[VP8_MAX_NUM_OF_SEGMENTS];

     #define SEGMENT_ABSDATA 1
     #define MAX_LOOP_FILTER 63

    // Macroblock level features
    typedef enum
    {
       MB_LVL_ALT_Q = 0,   /* Use alternate Quantizer .... */
       MB_LVL_ALT_LF = 1,  /* Use alternate loop filter value... */
       MB_LVL_MAX = 2      /* Number of MB level features supported */
    } MB_LVL_FEATURES;

     if (m_frame_info.segmentationEnabled)
     {
         for (int i = 0;i < VP8_MAX_NUM_OF_SEGMENTS;i++)
         {
             if (m_frame_info.segmentAbsMode)
             {
                 baseline_filter_level[i] = m_frame_info.segmentFeatureData[MB_LVL_ALT_LF][i];
             }
             else
             {
                 baseline_filter_level[i] = m_frame_info.loopFilterLevel + m_frame_info.segmentFeatureData[MB_LVL_ALT_LF][i];
                 baseline_filter_level[i] = (baseline_filter_level[i] >= 0) ? ((baseline_filter_level[i] <= MAX_LOOP_FILTER) ? baseline_filter_level[i] : MAX_LOOP_FILTER) : 0;  /* Clamp to valid range */
             }
         }
     }
     else
     {
         for (int i = 0; i < VP8_MAX_NUM_OF_SEGMENTS; i++)
         {
             baseline_filter_level[i] = m_frame_info.loopFilterLevel;
         }
     }

     for (int i = 0; i < VP8_MAX_NUM_OF_SEGMENTS; i++)
     {
         picParams->loop_filter_level[i] = baseline_filter_level[i];
     }

     //loop filter deltas for reference frame based MB level adjustment
     picParams->loop_filter_deltas_ref_frame[0] = m_frame_info.refLoopFilterDeltas[0];
     picParams->loop_filter_deltas_ref_frame[1] = m_frame_info.refLoopFilterDeltas[1];
     picParams->loop_filter_deltas_ref_frame[2] = m_frame_info.refLoopFilterDeltas[2];
     picParams->loop_filter_deltas_ref_frame[3] = m_frame_info.refLoopFilterDeltas[3];

     //loop filter deltas for coding mode based MB level adjustment
     picParams->loop_filter_deltas_mode[0] = m_frame_info.modeLoopFilterDeltas[0];
     picParams->loop_filter_deltas_mode[1] = m_frame_info.modeLoopFilterDeltas[1];
     picParams->loop_filter_deltas_mode[2] = m_frame_info.modeLoopFilterDeltas[2];
     picParams->loop_filter_deltas_mode[3] = m_frame_info.modeLoopFilterDeltas[3];

     //same as prob_skip_false in bitstream syntax
     picParams->prob_skip_false = m_frame_info.skipFalseProb;

     //same as prob_intra in bitstream syntax
     picParams->prob_intra = m_frame_info.intraProb;

     //same as prob_last in bitstream syntax
     picParams->prob_last = m_frame_info.lastProb;

     //same as prob_gf in bitstream syntax
     picParams->prob_gf = m_frame_info.goldProb;

     //list of 4 probabilities of the luma intra prediction mode decoding
     //tree and same as y_mode_probs in frame header
     //list of 3 probabilities of the chroma intra prediction mode decoding
     //tree and same as uv_mode_probs in frame header

     const mfxU8 *prob_y_table;
     const mfxU8 *prob_uv_table;

     if (false /*I_PICTURE != m_frame_info.frameType*/)
     {
         prob_y_table = vp8_kf_mb_mode_y_probs;
         prob_uv_table = vp8_kf_mb_mode_uv_probs;
     }
     else
     {
         prob_y_table = m_frameProbs.mbModeProbY;
         prob_uv_table = m_frameProbs.mbModeProbUV;
     }

     for (Ipp32u i = 0; i < VP8_NUM_MB_MODES_Y - 1; i += 1)
     {
         picParams->y_mode_probs[i] = prob_y_table[i];
     }

     for (Ipp32u i = 0; i < VP8_NUM_MB_MODES_UV - 1; i += 1)
     {
         picParams->uv_mode_probs[i] = prob_uv_table[i];
     }

     //updated mv decoding probabilities and same as mv_probs in frame header
     for (Ipp32u i = 0; i < VP8_NUM_MV_PROBS; i += 1)
     {
         picParams->mv_probs[0][i] = m_frameProbs.mvContexts[0][i];
         picParams->mv_probs[1][i] = m_frameProbs.mvContexts[1][i];
     }

     picParams->bool_coder_ctx.range = m_boolDecoder[VP8_FIRST_PARTITION].range();
     picParams->bool_coder_ctx.value = (m_boolDecoder[VP8_FIRST_PARTITION].value() >> 24) & 0xff;
     picParams->bool_coder_ctx.count = m_boolDecoder[VP8_FIRST_PARTITION].bitcount();

     compBufPic->SetDataSize(sizeof(VAPictureParameterBufferVP8));

     //////////////////////////////////////////////////////////////////
     UMCVACompBuffer* compBufCp;
     VAProbabilityDataBufferVP8 *coeffProbs = (VAProbabilityDataBufferVP8*)m_p_video_accelerator->
             GetCompBuffer(VAProbabilityBufferType, &compBufCp, sizeof(VAProbabilityDataBufferVP8));

     ////[4][8][3][11]
     memcpy(coeffProbs, m_frameProbs.coeff_probs, sizeof(Ipp8u) * 4 * 8 * 3 * 11);

     compBufCp->SetDataSize(sizeof(Ipp8u) * 4 * 8 * 3 * 11);

     //////////////////////////////////////////////////////////////////
     UMCVACompBuffer* compBufQm;
     VAIQMatrixBufferVP8 *qmTable = (VAIQMatrixBufferVP8*)m_p_video_accelerator->
             GetCompBuffer(VAIQMatrixBufferType, &compBufQm, sizeof(VAIQMatrixBufferVP8));

     if (false/*0 == m_frame_info.segmentationEnabled*/)
     {

         // when segmentation is disabled, use the first entry 0 for the quantization values
         qmTable->quantization_index[0][1] = (unsigned char)m_quantInfo.ydcQ[0];
         qmTable->quantization_index[0][0] = (unsigned char)m_quantInfo.yacQ[0];
         qmTable->quantization_index[0][4] = (unsigned char)m_quantInfo.uvdcQ[0];
         qmTable->quantization_index[0][5] = (unsigned char)m_quantInfo.uvacQ[0];
         qmTable->quantization_index[0][2] = (unsigned char)m_quantInfo.y2dcQ[0];
         qmTable->quantization_index[0][3] = (unsigned char)m_quantInfo.y2acQ[0];
     }
     else
     {

         for (Ipp32u i = 0; i < 4; i += 1)
         {
             qmTable->quantization_index[i][1] = (unsigned char)m_quantInfo.ydcQ[i];
             qmTable->quantization_index[i][0] = (unsigned char)m_quantInfo.yacQ[i];
             qmTable->quantization_index[i][4] = (unsigned char)m_quantInfo.uvdcQ[i];
             qmTable->quantization_index[i][5] = (unsigned char)m_quantInfo.uvacQ[i];
             qmTable->quantization_index[i][2] = (unsigned char)m_quantInfo.y2dcQ[i];
             qmTable->quantization_index[i][3] = (unsigned char)m_quantInfo.y2acQ[i];
         }
     }

     compBufQm->SetDataSize(sizeof(VAIQMatrixBufferVP8));

     //////////////////////////////////////////////////////////////////

     Ipp32u offset = 0;
     Ipp32u m_offset = 0;

     if (I_PICTURE == m_frame_info.frameType)
         m_offset = 10;
     else
         m_offset = 3;

     offset = 0;

     Ipp32s size = p_bistream->DataLength;

     UMCVACompBuffer* compBufSlice;

     VASliceParameterBufferVP8 *sliceParams
         = (VASliceParameterBufferVP8*)m_p_video_accelerator->
             GetCompBuffer(VASliceParameterBufferType, &compBufSlice, sizeof(VASliceParameterBufferVP8));


     #if defined(ANDROID)

     // number of bytes in the slice data buffer for the partitions
     sliceParams->slice_data_size = (Ipp32s)size - offset;//?

     //offset to the first byte of partition data
     sliceParams->slice_data_offset = 0; //?

     //see VA_SLICE_DATA_FLAG_XXX definitions
     sliceParams->slice_data_flag = VA_SLICE_DATA_FLAG_ALL;

     #endif

     //offset to the first bit of MB from the first byte of partition data
     sliceParams->macroblock_offset = m_frame_info.entropyDecSize;

// sliceParams->macroblock_offset = m_frame_info.entropyDecOffset;

     // Partitions
     sliceParams->num_of_partitions = m_frame_info.numPartitions;

     for (Ipp32s i = 0; i < m_frame_info.numPartitions; i += 1)
     {
         sliceParams->partition_size[i] = m_frame_info.partitionSize[i];
     }

     sliceParams->partition_size[0] = m_frame_info.firstPartitionSize;

     compBufSlice->SetDataSize(sizeof(VASliceParameterBufferVP8));

     UMCVACompBuffer* compBufBs;
     Ipp8u *bistreamData = (Ipp8u *)m_p_video_accelerator->GetCompBuffer(VASliceDataBufferType, &compBufBs, p_bistream->DataLength - offset);

     Ipp8u *pBuffer = (Ipp8u*) p_bistream->Data;
     memcpy(bistreamData, pBuffer + offset, size - offset);

     compBufBs->SetDataSize((Ipp32s)size - offset);

     //////////////////////////////////////////////////////////////////

     return sts;

} // Status VP8VideoDecoderHardware::PackHeaders(MediaData* src)

#endif

//////////////////////////////////////////////////////////////////////////////
// MFX_VP8_BoolDecoder

const int MFX_VP8_BoolDecoder::range_normalization_shift[64] =
{
  7, 6, 5, 5, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

#endif