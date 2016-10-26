//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#ifdef MFX_ENABLE_MPEG2_VIDEO_DECODE

#include "mfx_common_decode_int.h"
#include "mfx_mpeg2_decode.h"
#include "mfx_mpeg2_dec_common.h"
#include "mfx_enc_common.h"

#include "vm_sys_info.h"

#if defined (MFX_VA_WIN)
#include "umc_va_dxva2_protected.h"
#include "umc_va_dxva2.h"
#endif

#if !defined(MFX_ENABLE_HW_ONLY_MPEG2_DECODER) || !defined (MFX_VA)
#include "umc_mpeg2_dec_sw.h"
#endif

#include "umc_mpeg2_dec_hw.h"

//#define _status_report_debug
//#define _threading_deb

#if !defined(_WIN32) && !defined(_WIN64)
#if defined(_threading_deb)
    #define fopen_s(PF,FN,MD) ((*(PF))=fopen((FN),(MD)))==NULL
    #define GetCurrentThreadId (mfxU32)pthread_self
#endif
#if defined(_status_report_debug)
    #define OutputDebugStringA(STR) {fputs((STR),stderr); fflush(stderr);}
#endif
#endif

#if defined(_threading_deb)
#define THREAD_DEBUG_PRINTF(...) \
{ \
    FILE *file = NULL; \
    fopen_s(&file, "mpeg2_thr_dbg.txt", "ab+"); \
    if (file) { \
        fprintf(file, __VA_ARGS__); \
        fclose(file); \
    } \
}

#define THREAD_DEBUG_PRINTF__HOLDING_MUTEX(_guard, ...) \
{ \
    UMC::AutomaticUMCMutex guard(_guard); \
    THREAD_DEBUG_PRINTF(__VA_ARGS__) \
}
#else
#define THREAD_DEBUG_PRINTF(...)
#define THREAD_DEBUG_PRINTF__HOLDING_MUTEX(_guard, ...)
#endif

#ifdef _status_report_debug
#define STATUS_REPORT_DEBUG_PRINTF(...) \
{ \
    char cStr[256]; \
    sprintf(cStr, __VA_ARGS__); \
    OutputDebugStringA(cStr); \
}
#else
#define STATUS_REPORT_DEBUG_PRINTF(...)
#endif

enum
{
    ePIC   = 0x00,
    eUSER  = 0xb2,
    eSEQ   = 0xb3,
    eEXT   = 0xb5,
    eEND   = 0xb7,
    eGROUP = 0xb8
};

bool IsHWSupported(VideoCORE *pCore, mfxVideoParam *par)
{
    if((par->mfx.CodecProfile == MFX_PROFILE_MPEG1 && pCore->GetPlatformType()== MFX_PLATFORM_HARDWARE))
    {
        return false;
    }

#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
    if (MFX_ERR_NONE != pCore->IsGuidSupported(sDXVA2_ModeMPEG2_VLD, par))
    {
        return false;
    }
#endif

    return true;
}

mfxU8 GetMfxCodecLevel(mfxI32 level)
{
    switch (level)
    {
        case MPEG2_LEVEL_LOW:  return MFX_LEVEL_MPEG2_LOW;
        case MPEG2_LEVEL_MAIN: return MFX_LEVEL_MPEG2_MAIN;
        case MPEG2_LEVEL_H14:  return MFX_LEVEL_MPEG2_HIGH1440;
        case MPEG2_LEVEL_HIGH: return MFX_LEVEL_MPEG2_HIGH;
        default:               return MFX_LEVEL_UNKNOWN;
    }
}

mfxU8 GetMfxCodecProfile(mfxI32 profile)
{
    switch (profile)
    {
        case MPEG2_PROFILE_SIMPLE:  return MFX_PROFILE_MPEG2_SIMPLE;
        case MPEG2_PROFILE_MAIN:    return MFX_PROFILE_MPEG2_MAIN;
        case MPEG2_PROFILE_HIGH:    return MFX_PROFILE_MPEG2_HIGH;
        default:                    return MFX_PROFILE_UNKNOWN;
    }
}

mfxU16 GetMfxPicStruct(mfxU32 progressiveSequence, mfxU32 progressiveFrame, mfxU32 topFieldFirst, mfxU32 repeatFirstField, mfxU32 pictureStructure, mfxU16 extendedPicStruct)
{
    mfxU16 picStruct = MFX_PICSTRUCT_UNKNOWN;

    if (1 == progressiveSequence)
    {
        picStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }
    else // interlace sequence
    {
        picStruct |= ((topFieldFirst) ? MFX_PICSTRUCT_FIELD_TFF : MFX_PICSTRUCT_FIELD_BFF);

        if (0 == topFieldFirst && 1 == pictureStructure)
        {
            picStruct = MFX_PICSTRUCT_FIELD_TFF;
        }

        if (progressiveFrame)
        {
            picStruct |= MFX_PICSTRUCT_PROGRESSIVE;
        }
    }

    if (progressiveSequence == 1 || progressiveFrame == 1)
        if (repeatFirstField)
        {
            if (0 == progressiveSequence)
            {
                picStruct |= MFX_PICSTRUCT_FIELD_REPEATED;
            }
            else // progressive sequence
            {
                if (topFieldFirst)
                {
                    picStruct |= MFX_PICSTRUCT_FRAME_TRIPLING;
                }
                else
                {
                    picStruct |= MFX_PICSTRUCT_FRAME_DOUBLING;
                }
            }
        }

    if (0 == extendedPicStruct)
    {
        // cut decorative flags
        if (MFX_PICSTRUCT_PROGRESSIVE & picStruct)
        {
            picStruct = MFX_PICSTRUCT_PROGRESSIVE;
        }
        else if (MFX_PICSTRUCT_FIELD_TFF & picStruct)
        {
            picStruct = MFX_PICSTRUCT_FIELD_TFF;
        }
        else
        {
            picStruct = MFX_PICSTRUCT_FIELD_BFF;
        }
    }

    return picStruct;
}

void SetSurfaceTimeCode(mfxFrameSurface1* surface, Ipp32s display_index, UMC::MPEG2VideoDecoderBase *implUmc)
{
    if (!surface || !implUmc)
        return;

    mfxExtTimeCode* pExtTimeCode = (mfxExtTimeCode *)GetExtendedBuffer(surface->Data.ExtParam, surface->Data.NumExtParam, MFX_EXTBUFF_TIME_CODE);
    if (!pExtTimeCode)
        return;

    const UMC::sPictureHeader& ph = implUmc->GetPictureHeader(display_index);
    pExtTimeCode->DropFrameFlag    = ph.time_code.gop_drop_frame_flag;
    pExtTimeCode->TimeCodePictures = ph.time_code.gop_picture;
    pExtTimeCode->TimeCodeHours    = ph.time_code.gop_hours;
    pExtTimeCode->TimeCodeMinutes  = ph.time_code.gop_minutes;
    pExtTimeCode->TimeCodeSeconds  = ph.time_code.gop_seconds;
}

mfxU16 GetMfxPictureType(mfxU32 frameType)
{
    switch (frameType)
    {
        case I_PICTURE: return MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;
        case P_PICTURE: return MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
        case B_PICTURE: return MFX_FRAMETYPE_B;
        default:        return MFX_FRAMETYPE_UNKNOWN;
    }
}

mfxU16 GetMfxFieldType(mfxU32 frameType)
{
    switch (frameType)
    {
        case I_PICTURE: return MFX_FRAMETYPE_xI | MFX_FRAMETYPE_xREF;
        case P_PICTURE: return MFX_FRAMETYPE_xP | MFX_FRAMETYPE_xREF;
        case B_PICTURE: return MFX_FRAMETYPE_xB;
        default:        return MFX_FRAMETYPE_UNKNOWN;
    }
}

void SetSurfacePictureType(mfxFrameSurface1* surface, Ipp32s display_index, UMC::MPEG2VideoDecoderBase *implUmc)
{
    if (!surface || !implUmc)
        return;

    mfxExtDecodedFrameInfo* pDecodedFrameInfoExt = (mfxExtDecodedFrameInfo*)GetExtendedBuffer(surface->Data.ExtParam, surface->Data.NumExtParam, MFX_EXTBUFF_DECODED_FRAME_INFO);
    if (!pDecodedFrameInfoExt)
        return;

    const UMC::sPictureHeader& ph = implUmc->GetPictureHeader(display_index);
    const UMC::sSequenceHeader& sh = implUmc->GetSequenceHeader();

    mfxU32 frameType = implUmc->GetFrameType(display_index);
    pDecodedFrameInfoExt->FrameType = GetMfxPictureType(frameType);
    if (ph.first_in_sequence)
        pDecodedFrameInfoExt->FrameType |= MFX_FRAMETYPE_IDR;

    if (sh.progressive_sequence)
        return;

    if (ph.picture_structure == FRAME_PICTURE)
        pDecodedFrameInfoExt->FrameType |= GetMfxFieldType(frameType);
    else
        pDecodedFrameInfoExt->FrameType |= GetMfxFieldType(implUmc->GetFrameType(display_index+DPB));
}

inline
mfxU8 Mpeg2GetMfxChromaFormatFromUmcMpeg2(mfxU32 umcChromaFormat)
{
    switch (umcChromaFormat)
    {
        case 2: return MFX_CHROMAFORMAT_YUV420;
        case 4: return MFX_CHROMAFORMAT_YUV422;
        case 8: return MFX_CHROMAFORMAT_YUV444;
        default:return 0;
    }
}

void UpdateMfxVideoParam(mfxVideoParam& vPar, const UMC::sSequenceHeader& sh, const UMC::sPictureHeader& ph)
{
    vPar.mfx.CodecLevel = GetMfxCodecLevel(sh.level);
    vPar.mfx.CodecProfile = GetMfxCodecProfile(sh.profile);
  //  if(vPar.mfx.CodecProfile == MFX_PROFILE_UNKNOWN)
   //     vPar.mfx.CodecProfile = MFX_PROFILE_MPEG1;
    vPar.mfx.FrameInfo.AspectRatioW = sh.aspect_ratio_w;
    vPar.mfx.FrameInfo.AspectRatioH = sh.aspect_ratio_h;

    vPar.mfx.FrameInfo.CropW = (mfxU16)sh.width;
    vPar.mfx.FrameInfo.CropH = (mfxU16)sh.height;
    vPar.mfx.FrameInfo.CropX = 0;
    vPar.mfx.FrameInfo.CropY = 0;
    vPar.mfx.FrameInfo.Width = AlignValue(vPar.mfx.FrameInfo.CropW, MFX_MB_WIDTH);
    vPar.mfx.FrameInfo.Height = AlignValue(vPar.mfx.FrameInfo.CropH, MFX_MB_WIDTH);
    vPar.mfx.FrameInfo.ChromaFormat = Mpeg2GetMfxChromaFormatFromUmcMpeg2(sh.chroma_format);
    GetMfxFrameRate((mfxU8)sh.frame_rate_code, &vPar.mfx.FrameInfo.FrameRateExtN, &vPar.mfx.FrameInfo.FrameRateExtD);
    
    //vPar.mfx.FrameInfo.FrameRateExtD *= sh.frame_rate_extension_d + 1;
    //vPar.mfx.FrameInfo.FrameRateExtN *= sh.frame_rate_extension_n + 1;
    vPar.mfx.FrameInfo.PicStruct = GetMfxPicStruct(sh.progressive_sequence, ph.progressive_frame,
                                                   ph.top_field_first, ph.repeat_first_field, ph.picture_structure,
                                                   vPar.mfx.ExtendedPicStruct);
}

UMC::ColorFormat GetUmcColorFormat(mfxU8 colorFormat)
{
    switch (colorFormat)
    {
        case MFX_CHROMAFORMAT_YUV420: return UMC::YUV420;
        case MFX_CHROMAFORMAT_YUV422: return UMC::YUV422;
        case MFX_CHROMAFORMAT_YUV444: return UMC::YUV444;
        default: return UMC::NONE;
    }
}

static bool DARtoPAR(mfxI32 width, mfxI32 height, mfxI32 dar_h, mfxI32 dar_v,
                mfxI32 *par_h, mfxI32 *par_v)
{
  if(0 == width || 0 == height) 
    return false;

  mfxI32 simple_tab[] = {2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59};
  mfxI32 i, denom;

  // suppose no overflow of 32s
  mfxI32 h = dar_h * height;
  mfxI32 v = dar_v * width;

  // remove common multipliers
  while( ((h|v)&1) == 0 )
  {
    h>>=1;
    v>>=1;
  }

  for(i=0;i<(mfxI32)(sizeof(simple_tab)/sizeof(simple_tab[0]));i++)
  {
    denom = simple_tab[i];
    while(h%denom==0 && v%denom==0) {
      v /= denom;
      h /= denom;
    }
    if(v<=denom || h<=denom)
      break;
  }
  *par_h = h;
  *par_v = v;
  // can don't reach minimum, no problem
 // if(i<sizeof(simple_tab)/sizeof(simple_tab[0]))
 //   return false;
  return true;
}


static bool CalcAspectRatio(mfxI32 dar_code, mfxI32 width, mfxI32 height,
                                              mfxI32* AspectRatioW, mfxI32* AspectRatioH)
{
      bool ret = true;

      if(dar_code == 2)
      {
        ret = DARtoPAR(width, height, 4, 3, AspectRatioW, AspectRatioH);
      }
      else if(dar_code == 3)
      {
        ret = DARtoPAR(width, height, 16, 9, AspectRatioW, AspectRatioH);
      }
      else if(dar_code == 4)
      {
        ret = DARtoPAR(width, height, 221, 100, AspectRatioW, AspectRatioH);
      }
      else // dar_code == 1 or unknown
      {
        *AspectRatioW = 1;
        *AspectRatioH = 1;
      }
      return ret;
}

VideoDECODEMPEG2::VideoDECODEMPEG2(VideoCORE* core, mfxStatus *sts)
    : VideoDECODE()
    , m_pCore(core)
    , m_isInitialized(false)
    , m_isSWImpl(false)
{
    *sts = MFX_ERR_NONE;
    if (!m_pCore)
        *sts = MFX_ERR_NULL_PTR;

    m_SkipLevel = SKIP_NONE;
}

VideoDECODEMPEG2::~VideoDECODEMPEG2()
{
    Close();
}

mfxStatus VideoDECODEMPEG2::Init(mfxVideoParam *par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "VideoDECODEMPEG2::Init");
    if (m_isInitialized)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    MFX_CHECK_NULL_PTR1(par);

    mfxStatus mfxSts = MFX_ERR_NONE;

    eMFXHWType type = MFX_HW_UNKNOWN;

#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
        type = m_pCore->GetHWType();
#endif

    mfxSts = CheckVideoParamDecoders(par, m_pCore->IsExternalFrameAllocator(), type);

    if (mfxSts != MFX_ERR_NONE)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    mfxU16 IOPattern = par->IOPattern;

#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
        
        if (!IsHWSupported(m_pCore, par))
        {
#ifdef MFX_ENABLE_HW_ONLY_MPEG2_DECODER
            return MFX_ERR_UNSUPPORTED;
#else
            m_isSWImpl = true;
            type = MFX_HW_UNKNOWN;
#endif
        }

        if(m_isSWImpl)
        {
#ifndef MFX_ENABLE_HW_ONLY_MPEG2_DECODER
            internalImpl.reset(new VideoDECODEMPEG2Internal_SW);
#endif
        }
        else
        {
            internalImpl.reset(new VideoDECODEMPEG2Internal_HW);
        }

        if(IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
            internalImpl->m_isSWBuf = true;
#else
        IOPattern |= MFX_IOPATTERN_IN_SYSTEM_MEMORY;
        internalImpl.reset(new VideoDECODEMPEG2Internal_SW);
        m_isSWImpl = true;
#endif

    mfxSts = internalImpl->Init(par, m_pCore);
    if (mfxSts != MFX_ERR_NONE)
        return mfxSts;

    UMC::Status umcRes = internalImpl->m_implUmc->Init(&internalImpl->m_vdPar);
    MFX_CHECK_UMC_STS(umcRes);
    
    umcRes = internalImpl->m_implUmc->GetInfo(&internalImpl->m_vdPar);
    MFX_CHECK_UMC_STS(umcRes);

    if (MFX_PLATFORM_HARDWARE == m_pCore->GetPlatformType() && true == m_isSWImpl)
    {
        mfxSts = MFX_WRN_PARTIAL_ACCELERATION;
    }

    m_isInitialized = true;

    return mfxSts;

} // mfxStatus VideoDECODEMPEG2::Init(mfxVideoParam *par)

mfxStatus VideoDECODEMPEG2::Reset(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    if (par->AsyncDepth != internalImpl->m_vPar.AsyncDepth)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    eMFXHWType type = m_pCore->GetHWType();

    mfxStatus mfxSts = CheckVideoParamDecoders(par, m_pCore->IsExternalFrameAllocator(), type);
    if (MFX_ERR_NONE != mfxSts)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    mfxExtOpaqueSurfaceAlloc *pOpqExt = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
    if (pOpqExt)
    {
        if (false == internalImpl->m_isOpaqueMemory)
        {
            // decoder was not initialized with opaque extended buffer
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }

        if (internalImpl->allocRequest.NumFrameMin != pOpqExt->Out.NumSurface)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    m_SkipLevel = SKIP_NONE;

    mfxSts = internalImpl->Reset(par);
    MFX_CHECK_STS(mfxSts);

    internalImpl->m_reset_done = true;

    if (m_isSWImpl && m_pCore->GetPlatformType() == MFX_PLATFORM_HARDWARE)
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2::Close()
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "VideoDECODEMPEG2::Close");
    // immediately return, object is not initialized
    if (false == m_isInitialized)
    {
        return MFX_ERR_NONE;
    }

    internalImpl->Close();
    m_isInitialized = false;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2::GetDecodeStat(mfxDecodeStat *stat)
{
    MFX_CHECK_NULL_PTR1(stat);
    
    if (false == m_isInitialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    stat->NumFrame = internalImpl->display_frame_count;
    stat->NumCachedFrame = internalImpl->cashed_frame_count;
    stat->NumSkippedFrame = internalImpl->skipped_frame_count;
    
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2::GetUserData(mfxU8 *ud, mfxU32 *sz, mfxU64 *ts, mfxU16 bufsize)
{
    MFX_CHECK_NULL_PTR3(ud, sz, ts);

    UMC::Status umcSts = UMC::UMC_OK;

    umcSts = internalImpl->m_implUmc->GetCCData(ud, sz, ts, bufsize);

    if (UMC::UMC_OK == umcSts)
    {
        // we store pts in float
        mfxF64 pts;

        memcpy_s(&pts, sizeof(mfxF64), ts, sizeof(mfxF64));

        *ts = GetMfxTimeStamp(pts);

        return MFX_ERR_NONE;
    }

    if (UMC::UMC_ERR_NOT_ENOUGH_BUFFER == umcSts)
    {
        return MFX_ERR_NOT_ENOUGH_BUFFER;
    }

    return MFX_ERR_UNDEFINED_BEHAVIOR;
}

mfxStatus VideoDECODEMPEG2::GetPayload( mfxU64 *ts, mfxPayload *payload )
{
    mfxStatus sts = MFX_ERR_NONE;

    if (false == m_isInitialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    MFX_CHECK_NULL_PTR2( ts, payload);

    sts = GetUserData(payload->Data, &payload->NumBit, ts, payload->BufSize);
    MFX_CHECK_STS(sts);

    if(0 < payload->NumBit)
    {
        // user data start code type
        payload->Type = USER_DATA_START_CODE;
    }
    else
    {
        payload->Type = 0;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2::DecodeHeader(VideoCORE *core, mfxBitstream* bs, mfxVideoParam* par)
{
    MFX_CHECK_NULL_PTR3(bs,bs->Data,par);
    core;
    
    mfxU8* ptr;
    mfxU8* beg = bs->Data + bs->DataOffset;
    mfxU8* end = bs->Data + bs->DataOffset + bs->DataLength;

    bool find_seq_header = false;
    bool find_seq_ext    = false;
    bool find_seq_d_ext    = false;
    mfxI32 ShiftSH = 0;

    if (bs->MaxLength < bs->DataOffset)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    if(4 > bs->DataLength)
    {
        return MFX_ERR_MORE_DATA;
    }

    mfxU32 dar_code = 0;
    mfxU32 frame_rate_code = 0;
    mfxI32 frame_rate_extension_d = 0;
    mfxI32 frame_rate_extension_n = 0;
    par->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    Ipp32s W = 0, H = 0;

    mfxU8 *pShStart = NULL;
    mfxU8 *pShEnd = NULL;

    Ipp16u video_format = 5; // unspecified video format
    Ipp16u colour_primaries = 1;
    Ipp16u transfer_characteristics = 1;
    Ipp16u matrix_coefficients = 1;
    Ipp16u color_description_present = 0;

    for (ptr = beg; ptr + 4 < end; ++ptr)
    {
        if (ptr[0] == 0 && ptr[1] == 0 && ptr[2] == 1 && ptr[3] == 0xb3)
        {
            // seq header already found
            if (find_seq_header)
                break;

            pShStart = ptr;

            if(ptr + 7 >= end)
                return MFX_ERR_MORE_DATA;

            ShiftSH = (mfxI32)(ptr - beg);
           // memset(par, 0, sizeof(mfxVideoParam));
            par->mfx.CodecId = MFX_CODEC_MPEG2;
            par->mfx.FrameInfo.CropX = 0;
            par->mfx.FrameInfo.CropY = 0;
            par->mfx.FrameInfo.CropW = (ptr[4] << 4) + ((ptr[5] >> 4) & 0xf);
            par->mfx.FrameInfo.CropH = ((ptr[5] & 0xf) << 8) + ptr[6];

            frame_rate_code = ptr[7]  & 0xf;
            dar_code = ((ptr[7] >> 4) & 0xf);

            par->mfx.FrameInfo.Width = (par->mfx.FrameInfo.CropW + 15) & ~0x0f;
            par->mfx.FrameInfo.Height = (par->mfx.FrameInfo.CropH + 15) & ~0x0f;
            par->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

            if (par->mfx.FrameInfo.FourCC != MFX_FOURCC_YV12 && 
                par->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12)
            {
                par->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            }

            find_seq_header = true;

            if(find_seq_ext && find_seq_d_ext)
                break;
        } // 0xb3 code
        else if (ptr[0] == 0 && ptr[1] == 0 && ptr[2] == 1 && ptr[3] == 0xb5)
        {
            // extensions section

            // get extension id
            Ipp32u code = (ptr[4] & 0xf0);
            
            if (0x10 == code)
            {
                // sequence extension section
                if (find_seq_ext)
                    break;

                pShEnd = ptr;

                if (end <= ptr + 9)
                    return MFX_ERR_MORE_DATA;

                code = (ptr[4] << 24) | (ptr[5] << 16) | (ptr[6] << 8) | ptr[7];
                
                par->mfx.CodecProfile = GetMfxCodecProfile((code >> 24) & 0x7);
                par->mfx.CodecLevel = GetMfxCodecLevel((code >> 20) & 0xf);
                Ipp32u progr = (code >> 19) & 1;
                progr != 0 ? par->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE:
                             par->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_UNKNOWN;

                Ipp32s ChromaFormat = 1 << ((code >> 17) & ((1 << 2) - 1));
                
                switch (ChromaFormat)
                {
                    case 2:
                        par->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
                        break;
                    case 4:
                        par->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
                        break;
                    case 8:
                        par->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
                        break;
                    default:
                        break;
                }

                par->mfx.FrameInfo.CropW  = (mfxU16)((par->mfx.FrameInfo.CropW  & 0xfff)
                                                      | ((code >> (15-12)) & 0x3000));
                par->mfx.FrameInfo.CropH = (mfxU16)((par->mfx.FrameInfo.CropH & 0xfff)
                                                      | ((code >> (13-12)) & 0x3000));

                par->mfx.FrameInfo.Width = (par->mfx.FrameInfo.CropW + 15) & ~0x0f;
                par->mfx.FrameInfo.Height = (par->mfx.FrameInfo.CropH + 15) & ~0x0f;

                // 4k case
                if (0 == par->mfx.FrameInfo.CropW)
                {
                    par->mfx.FrameInfo.CropW = par->mfx.FrameInfo.Width;
                }

                if (0 == par->mfx.FrameInfo.CropH)
                {
                    par->mfx.FrameInfo.CropH = par->mfx.FrameInfo.Height;
                }

                code = (ptr[8] << 8) | ptr[9];
                frame_rate_extension_d = code & 31;
                frame_rate_extension_n = (code >> 5) & 3;

                find_seq_ext = true;

                if (true == find_seq_header && true == find_seq_d_ext)
                    break;
            }

            else if (0x20 == code) 
            { 
                // sequence display extension section
                video_format = (ptr[4] >> 1) & 0x07;
                Ipp8u colour_description = ptr[4] & 0x01;
                color_description_present = colour_description;
                if (colour_description) // 4+24 bits skip
                {
                    if (ptr + 11 >= end)
                        return MFX_ERR_MORE_DATA;
                    
                    colour_primaries = ptr[5];
                    transfer_characteristics = ptr[6];
                    matrix_coefficients = ptr[7];

                    code = ((ptr[8]) << 24) | (ptr[9] << 16) | (ptr[10] << 8) | ptr[11];//28 bit
                }
                else // 4 bit skip (28 bit)
                {
                    if (ptr + 8 >= end)
                        return MFX_ERR_MORE_DATA;

                    code = ((ptr[5]) << 24) | (ptr[6] << 16) | (ptr[7] << 8) | ptr[8];
                }

                W = (code >> 18);
                H = (code >> 3) & 0x00003fff;

                find_seq_d_ext = true;

                if(true == find_seq_header && true == find_seq_ext)
                    break;
            }
        }
        else if (IsMpeg2StartCodeEx(ptr) && find_seq_header)
            break;
    }
    if (find_seq_header)
    {
        // fill extension buffer

        mfxExtCodingOptionSPSPPS *spspps = (mfxExtCodingOptionSPSPPS *) GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS);

        if (NULL != spspps)
        {
            mfxU16 size = 0;

            size = (mfxU16)(pShEnd - pShStart);

            if (find_seq_ext)
            {
                size += 10;
            }

            if (find_seq_d_ext)
            {
                if (color_description_present)
                    size += 12;
                else
                    size += 9;
            }

            memcpy_s(spspps->SPSBuffer, size, pShStart, size);
            spspps->SPSBufSize = size;
        }

        mfxExtVideoSignalInfo *signalInfo = (mfxExtVideoSignalInfo *) GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO);

        if (NULL != signalInfo)
        {
            signalInfo->VideoFormat = video_format;
            signalInfo->ColourPrimaries = colour_primaries;
            signalInfo->TransferCharacteristics = transfer_characteristics;
            signalInfo->MatrixCoefficients = matrix_coefficients;
            signalInfo->ColourDescriptionPresent = color_description_present;
        }

        if(!find_seq_d_ext || W == 0 || H == 0)
        {
            W = (mfxI32) par->mfx.FrameInfo.CropW;
            H = (mfxI32) par->mfx.FrameInfo.CropH;
        }

        mfxI32 AspectRatioW, AspectRatioH;

        if(!CalcAspectRatio((mfxI32)dar_code,
            W, H,
            &AspectRatioW, &AspectRatioH))
            return MFX_ERR_UNSUPPORTED;

        par->mfx.FrameInfo.AspectRatioW = (mfxU16)AspectRatioW;
        par->mfx.FrameInfo.AspectRatioH = (mfxU16)AspectRatioH;

        bs->DataOffset += ShiftSH;
        bs->DataLength -= ShiftSH;
        GetMfxFrameRate((mfxU8)frame_rate_code, &par->mfx.FrameInfo.FrameRateExtN, &par->mfx.FrameInfo.FrameRateExtD);
        par->mfx.FrameInfo.FrameRateExtD *= frame_rate_extension_d + 1;
        par->mfx.FrameInfo.FrameRateExtN *= frame_rate_extension_n + 1;

        if (false == find_seq_ext)
        {
            par->mfx.CodecProfile = MFX_PROFILE_MPEG1;
            par->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        }

        if (MFX_PICSTRUCT_PROGRESSIVE != par->mfx.FrameInfo.PicStruct)
        {
            mfxU16 h = (par->mfx.FrameInfo.CropH + 31) & ~(31);
            
            if (par->mfx.FrameInfo.Height < h)
            {
                par->mfx.FrameInfo.Height = h;
            }
        }

        //if (!IsHWSupported(core, par))
        //{
        //    return MFX_WRN_PARTIAL_ACCELERATION;
        //}

        return MFX_ERR_NONE;
    }

    ShiftSH = (mfxI32)(ptr - beg + 4);

    if(bs->DataLength >= (mfxU32)ShiftSH)
    {
        bs->DataOffset += ShiftSH;
        bs->DataLength -= ShiftSH;
    }

    return MFX_ERR_MORE_DATA;
}

mfxStatus VideoDECODEMPEG2::CheckProtectionSettings(mfxVideoParam *input, mfxVideoParam *output, VideoCORE *core)
{
    if (0 == input->Protected)
    {   
        // no protected capablities were requested
        return MFX_ERR_NONE;
    }

    if (0 == (input->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
    {
        output->IOPattern = 0;
        output->Protected = 0;

        return MFX_ERR_UNSUPPORTED;
    }

    output->Protected = input->Protected;

    if (false == IsHWSupported(core, input))
    {
        output->Protected = 0;
        return MFX_ERR_UNSUPPORTED;
    }


    if (IS_PROTECTION_ANY(input->Protected))
    {
        if (IS_PROTECTION_PAVP_ANY(input->Protected))
        {
            mfxExtPAVPOption *extended_buffer = (mfxExtPAVPOption *) GetExtendedBuffer(input->ExtParam, input->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);

            if (NULL == extended_buffer)
            {
                return MFX_ERR_UNSUPPORTED;
            }

            mfxExtPAVPOption *extended_buffer_out = (mfxExtPAVPOption *) GetExtendedBuffer(output->ExtParam, output->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);

            if (NULL == extended_buffer_out)
            {
                return MFX_ERR_UNSUPPORTED;
            }

            if (MFX_PAVP_AES128_CTR != extended_buffer->EncryptionType)
            {
                extended_buffer_out->EncryptionType = 0;

                return MFX_ERR_UNSUPPORTED;
            }

            if (MFX_PAVP_CTR_TYPE_B == extended_buffer->CounterType)
            {
                if (MFX_PROTECTION_GPUCP_PAVP == input->Protected)
                {
                    extended_buffer_out->CounterType = 0;

                    return MFX_ERR_UNSUPPORTED;
                }
            }
            else if ((MFX_PAVP_CTR_TYPE_A != extended_buffer->CounterType)&&
                (MFX_PAVP_CTR_TYPE_C != extended_buffer->CounterType))
            {
                    extended_buffer_out->CounterType = 0;

                    return MFX_ERR_UNSUPPORTED;
            }

            return MFX_ERR_NONE;
        }
        else
        {
            if (GetExtendedBuffer(input->ExtParam, input->NumExtParam, MFX_EXTBUFF_PAVP_OPTION) ||
                GetExtendedBuffer(output->ExtParam, output->NumExtParam, MFX_EXTBUFF_PAVP_OPTION))
                return MFX_ERR_UNSUPPORTED;

            if (input->Protected == MFX_PROTECTION_GPUCP_AES128_CTR && core->GetVAType() != MFX_HW_D3D11)
                return MFX_ERR_UNSUPPORTED;
        }
    }
    else
    {
        output->Protected = 0;

        return MFX_ERR_UNSUPPORTED;
    }
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2::Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "VideoDECODEMPEG2::Query");
    // touch unreferenced parameter
    core = core;

    MFX_CHECK_NULL_PTR1(out);
    mfxStatus res = MFX_ERR_NONE;

    eMFXHWType type = core->GetHWType();
    if (!in)
    { // configurability mode
        memset(out, 0, sizeof(*out));
        out->mfx.CodecId = MFX_CODEC_MPEG2;
        out->mfx.FrameInfo.FourCC = 1;
        out->mfx.FrameInfo.Width = 1;
        out->mfx.FrameInfo.Height = 1;
        out->mfx.FrameInfo.CropX = 1;
        out->mfx.FrameInfo.CropY = 1;
        out->mfx.FrameInfo.CropW = 1;
        out->mfx.FrameInfo.CropH = 1;
        out->mfx.FrameInfo.AspectRatioH = 1;
        out->mfx.FrameInfo.AspectRatioW = 1;

        out->mfx.FrameInfo.FrameRateExtN = 1;
        out->mfx.FrameInfo.FrameRateExtD = 1;
        out->mfx.FrameInfo.PicStruct = 1;
        out->mfx.CodecProfile = 1;
        out->mfx.CodecLevel = 1;
        out->mfx.ExtendedPicStruct = 1;
        out->mfx.TimeStampCalc = 1;

        if (type >= MFX_HW_SNB)
        {
            out->Protected = MFX_PROTECTION_GPUCP_PAVP;

            mfxExtPAVPOption * pavpOptOut = (mfxExtPAVPOption*)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);
            if (pavpOptOut)
            {
                mfxExtBuffer header = pavpOptOut->Header;
                memset(pavpOptOut, 0, sizeof(mfxExtPAVPOption));
                pavpOptOut->Header = header;
                pavpOptOut->CounterType = (mfxU16)((type == MFX_HW_IVB)||(type == MFX_HW_VLV) ? MFX_PAVP_CTR_TYPE_C : MFX_PAVP_CTR_TYPE_A);
                pavpOptOut->EncryptionType = MFX_PAVP_AES128_CTR;
                pavpOptOut->CounterIncrement = 0;
                pavpOptOut->CipherCounter.Count = 0;
                pavpOptOut->CipherCounter.IV = 0;
            }
        }

        out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

        if (type == MFX_HW_UNKNOWN)
        {
            out->IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        }
        else
        {
            out->IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        }
        // DECODE's configurables
       // out->mfx.ConstructNFrames = 1;
        out->mfx.NumThread = 1;
        out->AsyncDepth = 3;
    }
    else
    { // checking mode
#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)

    mfxStatus sts = CheckProtectionSettings(in, out, core);
    MFX_CHECK_STS(sts);


#else
        if(in->Protected)
            return MFX_ERR_UNSUPPORTED;
#endif

        if (1 == in->mfx.DecodedOrder)
        {
            return MFX_ERR_UNSUPPORTED;
        }

        if(in->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12)
        {
            out->mfx.FrameInfo.FourCC = 0;
            return MFX_ERR_UNSUPPORTED;
        }

        if(in->NumExtParam != 0 && (!IS_PROTECTION_ANY(in->Protected)))
        {
            mfxExtOpaqueSurfaceAlloc *pOpaq = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

            // ignore opaque extension buffer
            if (in->NumExtParam != 1 || !(in->NumExtParam == 1 && NULL != pOpaq))
                return MFX_ERR_UNSUPPORTED;
        }

        if(in->ExtParam != NULL && (!IS_PROTECTION_ANY(in->Protected)))
        {
            mfxExtOpaqueSurfaceAlloc *pOpaq = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

            // ignore opaque extension buffer
            if (NULL == pOpaq)
            {
                return MFX_ERR_UNSUPPORTED;
            }
        }

        mfxExtBuffer **pExtBuffer = out->ExtParam;
        mfxU16 numDistBuf = out->NumExtParam;
        memcpy_s(out, sizeof(mfxVideoParam), in, sizeof(mfxVideoParam));

        if (in->AsyncDepth == 0)
            out->AsyncDepth = 3;
        out->ExtParam = pExtBuffer;
        out->NumExtParam = numDistBuf;

        if (!((in->mfx.FrameInfo.Width % 16 == 0)&&
            (in->mfx.FrameInfo.Width <= 4096)))
        {
            out->mfx.FrameInfo.Width = 0;
            return MFX_ERR_UNSUPPORTED;
        }

        if (!((in->mfx.FrameInfo.Height % 16 == 0)&&
            (in->mfx.FrameInfo.Height <= 4096)))
        {
            out->mfx.FrameInfo.Height = 0;
            return MFX_ERR_UNSUPPORTED;
        }

        if (!(in->mfx.FrameInfo.CropX <= out->mfx.FrameInfo.Width))
        {
            out->mfx.FrameInfo.CropX = 0;
            return MFX_ERR_UNSUPPORTED;
        }

        if (!(in->mfx.FrameInfo.CropY <= out->mfx.FrameInfo.Height))
        {
            out->mfx.FrameInfo.CropY = 0;
            return MFX_ERR_UNSUPPORTED;
        }

        if (!(out->mfx.FrameInfo.CropX + in->mfx.FrameInfo.CropW <= out->mfx.FrameInfo.Width))
        {
            out->mfx.FrameInfo.CropW = 0;
            return MFX_ERR_UNSUPPORTED;
        }

        if(!(out->mfx.FrameInfo.CropY + in->mfx.FrameInfo.CropH <= out->mfx.FrameInfo.Height))
        {
            out->mfx.FrameInfo.CropH = 0;
            return MFX_ERR_UNSUPPORTED;
        }

        if(!(in->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ||
           in->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF ||
           in->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF ||
           in->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_UNKNOWN))
        {
           out->mfx.FrameInfo.PicStruct = 0;
           return MFX_ERR_UNSUPPORTED;
        }

        if (!(in->mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV420 ||
            in->mfx.FrameInfo.ChromaFormat == 0))
        {
            out->mfx.FrameInfo.ChromaFormat = 0;
            return MFX_ERR_UNSUPPORTED;
        }

        if(in->mfx.CodecId != MFX_CODEC_MPEG2)
            out->mfx.CodecId = 0;


        if (!(in->mfx.CodecLevel == MFX_LEVEL_MPEG2_LOW ||
            in->mfx.CodecLevel == MFX_LEVEL_MPEG2_MAIN ||
            in->mfx.CodecLevel == MFX_LEVEL_MPEG2_HIGH1440 ||
            in->mfx.CodecLevel == MFX_LEVEL_MPEG2_HIGH ||
            in->mfx.CodecLevel == 0))
        {
            out->mfx.CodecLevel = 0;
            return MFX_ERR_UNSUPPORTED;
        }

        if(!(in->mfx.CodecProfile == MFX_PROFILE_MPEG2_SIMPLE ||
           in->mfx.CodecProfile == MFX_PROFILE_MPEG2_MAIN ||
           in->mfx.CodecProfile == MFX_PROFILE_MPEG2_HIGH ||
           in->mfx.CodecProfile == MFX_PROFILE_MPEG1 ||
           in->mfx.CodecProfile == 0))
        {
            out->mfx.CodecProfile = 0;
            return MFX_ERR_UNSUPPORTED;
        }

        if ((in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) ||
            (in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        {
            out->IOPattern = in->IOPattern;
        }
        else if (MFX_PLATFORM_SOFTWARE == core->GetPlatformType())
        {
            out->IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
           // res = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
        else
        {
            out->IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
           // res = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

       // Mpeg2CheckConfigurableCommon(*out);

         if (!IsHWSupported(core, in))
         {
#ifdef MFX_ENABLE_HW_ONLY_MPEG2_DECODER
             return MFX_ERR_UNSUPPORTED;
#else
             return MFX_WRN_PARTIAL_ACCELERATION;
#endif
         }

    }

    return res;
}

mfxStatus VideoDECODEMPEG2::QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "VideoDECODEMPEG2::QueryIOSurf");
    MFX_CHECK_NULL_PTR2(par, request);

    if (!(par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) &&
        !(par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) &&
        !(par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if ((par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && 
        (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if ((par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && 
        (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if ((par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && 
        (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    // call internal query io surface
    mfxStatus sts = VideoDECODEMPEG2InternalBase::QueryIOSurfInternal(core, par, request);
    MFX_CHECK_STS(sts);

    // ?? TODO: what is about opaque memory
    if (MFX_PLATFORM_SOFTWARE == core->GetPlatformType())
    {
        if ((par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && 
             false == par->mfx.DecodedOrder)
        {
            request->NumFrameSuggested = request->NumFrameMin = 1;
        }
    }
    else if (MFX_PLATFORM_HARDWARE == core->GetPlatformType())
    {
        if (false == IsHWSupported(core, par))
        {
#ifdef MFX_ENABLE_HW_ONLY_MPEG2_DECODER
            return MFX_ERR_UNSUPPORTED;
#else
            return MFX_WRN_PARTIAL_ACCELERATION;
#endif
        }

        if ((par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) && 
             false == par->mfx.DecodedOrder)
        {
            request->NumFrameSuggested = request->NumFrameMin = 1 + par->AsyncDepth;
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEMPEG2::QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)

mfxStatus VideoDECODEMPEG2::GetVideoParam(mfxVideoParam *par)
{
    if(!m_isInitialized)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par);
    return internalImpl->GetVideoParam(par);
}

mfxStatus VideoDECODEMPEG2::SetSkipMode(mfxSkipMode mode)
{
    mfxStatus ret = MFX_ERR_NONE;

    if (mode == 0x23)
    {
#if defined MFX_VA_WIN
        UMC::VideoAccelerator *va;
        m_pCore->GetVA((mfxHDL*)&va, MFX_MEMTYPE_FROM_DECODE);
        va->SetStatusReportUsing(false);
#endif
        return MFX_ERR_NONE;
    }

    switch(mode)
    {
    case MFX_SKIPMODE_MORE:
        if(m_SkipLevel < SKIP_ALL)
        {
            m_SkipLevel++;
            internalImpl->m_implUmc->SetSkipMode(m_SkipLevel);
        }
        else
        {
            ret = MFX_WRN_VALUE_NOT_CHANGED;
        }
        break;
    case MFX_SKIPMODE_LESS:
        if(m_SkipLevel > SKIP_NONE)
        {
            m_SkipLevel--;
            internalImpl->m_implUmc->SetSkipMode(m_SkipLevel);
        }
        else
        {
            ret = MFX_WRN_VALUE_NOT_CHANGED;
        }
        break;
    case MFX_SKIPMODE_NOSKIP:
        if(m_SkipLevel <= SKIP_B)
        {
            if(m_SkipLevel == SKIP_NONE)
            {
                ret = MFX_WRN_VALUE_NOT_CHANGED;
                break;
            }
            m_SkipLevel = SKIP_NONE;
            internalImpl->m_implUmc->SetSkipMode(m_SkipLevel);
        }
        else
        {
            Reset(&internalImpl->m_vPar);
            internalImpl->m_implUmc->SetSkipMode(m_SkipLevel);
        }
        break;
    default:
        ret = MFX_ERR_UNDEFINED_BEHAVIOR;
        break;
    }
    return ret;
}

static mfxStatus __CDECL MPEG2TaskRoutine(void *pState, void *pParam, mfxU32 /*threadNumber*/, mfxU32 /*callNumber*/)
{
    VideoDECODEMPEG2InternalBase *decoder = (VideoDECODEMPEG2InternalBase*)pState;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "VideoDECODEMPEG2::TaskRoutine");
    mfxStatus sts = decoder->TaskRoutine(pParam);
    return sts;
}

#if !defined(MFX_ENABLE_HW_ONLY_MPEG2_DECODER) || !defined (MFX_VA)
static mfxStatus __CDECL MPEG2CompleteTasks(void *pState, void *pParam, mfxStatus /*sts*/)
{
    VideoDECODEMPEG2InternalBase *decoder = (VideoDECODEMPEG2InternalBase*)pState;
    
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "VideoDECODEMPEG2::CompleteTasks");
    mfxStatus sts = decoder->CompleteTasks(pParam);
    return sts;
}
#endif

mfxStatus VideoDECODEMPEG2::CheckFrameData(const mfxFrameSurface1 *pSurface)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "VideoDECODEMPEG2::CheckFrameData");
    MFX_CHECK_NULL_PTR1(pSurface);
    
    if (0 != pSurface->Data.Locked)
    {
        return MFX_ERR_MORE_SURFACE;
    }

    if (pSurface->Info.Width <  internalImpl->m_InitW ||
        pSurface->Info.Height < internalImpl->m_InitH)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    // robustness checking
    if (pSurface->Info.CropW > pSurface->Info.Width || 
        pSurface->Info.CropH > pSurface->Info.Height)
    {
        return MFX_ERR_UNKNOWN;
    }

    if (NULL == pSurface->Data.MemId)
    {
        switch (pSurface->Info.FourCC)
        {
            case MFX_FOURCC_NV12:

                if (NULL == pSurface->Data.Y || NULL == pSurface->Data.UV)
                {
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
                }

                break;

            case MFX_FOURCC_YV12:
            case MFX_FOURCC_YUY2:
                
                if (NULL == pSurface->Data.Y || NULL == pSurface->Data.U || NULL == pSurface->Data.V)
                {
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
                }

                break;

            case MFX_FOURCC_RGB3:

                if (NULL == pSurface->Data.R || NULL == pSurface->Data.G || NULL == pSurface->Data.B)
                {
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
                }

                break;

            case MFX_FOURCC_RGB4:

                if (NULL == pSurface->Data.A || 
                    NULL == pSurface->Data.R || 
                    NULL == pSurface->Data.G || 
                    NULL == pSurface->Data.B)
                {
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
                }

                break;

            default:

                break;
            }

        if (0x7FFF < pSurface->Data.Pitch)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
    }


    if (pSurface->Info.Width >  internalImpl->m_InitW ||
        pSurface->Info.Height > internalImpl->m_InitH)
    {
        //return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2::DecodeFrameCheck(mfxBitstream *bs,
                                             mfxFrameSurface1 *surface_work,
                                             mfxFrameSurface1 **surface_disp,
                                             MFX_ENTRY_POINT *pEntryPoint)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "VideoDECODEMPEG2::DecodeFrameCheck");
    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR1(surface_disp);

    if (false == m_isInitialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    if (true == internalImpl->m_isOpaqueMemory)
    {
        if (surface_work->Data.MemId || surface_work->Data.Y || surface_work->Data.R || surface_work->Data.A || surface_work->Data.UV) // opaq surface
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        surface_work = internalImpl->GetOriginalSurface(surface_work);
    }

    if (bs)
    {
        sts = CheckBitstream(bs);
        MFX_CHECK_STS(sts);
    }

    sts = CheckFrameData(surface_work);
    MFX_CHECK_STS(sts);

    sts = internalImpl->ConstructFrame(bs, surface_work);
    MFX_CHECK_STS(sts);

    return internalImpl->DecodeFrameCheck(bs, surface_work, surface_disp, pEntryPoint);
}

inline bool IsMpeg2StartCode(const mfxU8* p)
{
    return p[0] == 0 && p[1] == 0 && p[2] == 1 && (p[3] == ePIC || p[3] == eSEQ || p[3] == eEND || p[3] == eGROUP);
}
inline bool IsMpeg2StartCodeEx(const mfxU8* p)
{
    return p[0] == 0 && p[1] == 0 && p[2] == 1 && (p[3] == ePIC || p[3] == eEXT || p[3] == eSEQ || p[3] == eEND || p[3] == eGROUP);
}

inline bool IsMpeg2UserDataStartCode(const mfxU8* p)
{
    return p[0] == 0 && p[1] == 0 && p[2] == 1 && p[3] == eUSER;
}

const mfxU8* FindUserDataStartCode(const mfxU8* begin, const mfxU8* end)
{
    for (; begin + 3 < end; ++begin)
        if (IsMpeg2UserDataStartCode(begin))
            break;
    return begin;
}

inline bool IsMpeg2AnyStartCode(const mfxU8* p)
{
    return p[0] == 0 && p[1] == 0 && p[2] == 1;
}

const mfxU8* FindAnyStartCode(const mfxU8* begin, const mfxU8* end)
{
    for (; begin + 3 < end; ++begin)
        if (IsMpeg2AnyStartCode(begin))
            break;
    return begin;
}

const mfxU8* FindStartCode(const mfxU8* begin, const mfxU8* end)
{
    for (; begin + 3 < end; ++begin)
        if (IsMpeg2StartCode(begin))
            break;
    return begin;
}
const mfxU8* FindStartCodeEx(const mfxU8* begin, const mfxU8* end)
{
    for (; begin + 3 < end; ++begin)
        if (IsMpeg2StartCodeEx(begin))
            break;
    return begin;
}

mfxStatus AppendBitstream(mfxBitstream& bs, const mfxU8* ptr, mfxU32 bytes)
{
    if (bs.DataOffset + bs.DataLength + bytes > bs.MaxLength)
        return MFX_ERR_NOT_ENOUGH_BUFFER;
    ippsCopy_8u(ptr, bs.Data + bs.DataOffset + bs.DataLength, bytes);
    bs.DataLength += bytes;
    return MFX_ERR_NONE;
}

void MoveBitstreamData(mfxBitstream& bs, mfxU32 offset)
{
    VM_ASSERT(offset <= bs.DataLength);
    bs.DataOffset += offset;
    bs.DataLength -= offset;
}

mfxStatus CutUserData(mfxBitstream *in, mfxBitstream *out, const mfxU8 *tail)
{
    const mfxU8* head = in->Data + in->DataOffset;
    const mfxU8* UserDataStart = FindUserDataStartCode(head, tail);
    while ( UserDataStart + 3 < tail)
    {
        mfxStatus sts = AppendBitstream(*out, head, (mfxU32)(UserDataStart - head));
        MFX_CHECK_STS(sts);
        MoveBitstreamData(*in, (mfxU32)(UserDataStart - head) + 4);

        head = in->Data + in->DataOffset;
        const mfxU8* UserDataEnd = FindAnyStartCode(head, tail);
        MoveBitstreamData(*in, (mfxU32)(UserDataEnd - head));

        head = in->Data + in->DataOffset;
        UserDataStart = FindUserDataStartCode(head, tail);
    }
    return MFX_ERR_NONE;
}

VideoDECODEMPEG2InternalBase::VideoDECODEMPEG2InternalBase()
{
    m_pCore = 0;
    m_isSWBuf = false;
    m_isSWDecoder = true;

    for(int i = 0; i < DPB; i++)
    {
        mid[i] = -1;
    }

    for(int i = 0; i < NUM_FRAMES; i++)
    {
        m_frame[i].Data = NULL;
        m_frame_in_use[i] = false;
    }

    m_FrameAllocator_nv12 = new mfx_UMC_FrameAllocator_NV12;
    m_FrameAllocator = m_FrameAllocator_nv12;

    m_isDecodedOrder = false;
    m_reset_done = false;

    m_isOpaqueMemory = false;
    m_isFrameRateFromInit = false;

    memset(&allocResponse, 0, sizeof(mfxFrameAllocResponse));
    memset(&m_opaqueResponse, 0, sizeof(mfxFrameAllocResponse));

    Reset(0);
}

VideoDECODEMPEG2InternalBase::~VideoDECODEMPEG2InternalBase()
{
}

mfxStatus VideoDECODEMPEG2InternalBase::Reset(mfxVideoParam *par)
{
    for (mfxU32 i = 0; i < DPB; i++)
    {
        if (mid[i] >= 0)
        {
            m_FrameAllocator->DecreaseReference(mid[i]);
            mid[i] = -1;
        }
    }

    m_FrameAllocator->Reset();

    prev_index      = -1;
    curr_index      = -1;
    next_index      = -1;
    display_index   = -1;
    display_order   =  0;
    dec_frame_count = 0;
    dec_field_count = 0;
    last_frame_count = 0;
    display_frame_count = 0;
    cashed_frame_count = 0;
    skipped_frame_count = 0;
    last_timestamp   = 0;
    last_good_timestamp = 0.0;

    m_found_SH = false;
    m_first_SH = true;
    m_new_bs   = true;

    ResetFcState(m_fcState);

    m_task_num = 0;
    m_prev_task_num = -1;

    for(int i = 0; i < NUM_FRAMES; i++)
    {
        m_frame[i].DataLength = 0;
        m_frame[i].DataOffset = 0;
        m_frame_in_use[i] = false;
        m_time[i] = (mfxF64)(-1);
    }

    m_frame_curr = -1;
    m_frame_free = -1;
    m_frame_constructed = true;

    memset(m_last_bytes,0,NUM_REST_BYTES);

    m_resizing = false;
    m_InitPicStruct = 0;

    m_vdPar.info.framerate = 0;

    if (par)
    {
        m_vPar = *par;

        m_vdPar.info.stream_type = UMC::MPEG2_VIDEO;
        m_vdPar.info.clip_info.width = par->mfx.FrameInfo.Width;
        m_vdPar.info.clip_info.height = par->mfx.FrameInfo.Height;

        m_vdPar.info.aspect_ratio_width = par->mfx.FrameInfo.AspectRatioW;
        m_vdPar.info.aspect_ratio_height = par->mfx.FrameInfo.AspectRatioH;
    
        mfxU32 frameRateD = par->mfx.FrameInfo.FrameRateExtD;
        mfxU32 frameRateN = par->mfx.FrameInfo.FrameRateExtN;

        if (0 != frameRateD && 0 != frameRateN)
        {
            m_vdPar.info.framerate = (Ipp64f) frameRateN / frameRateD;
            m_isFrameRateFromInit = true;
        }

        m_vdPar.info.color_format = GetUmcColorFormat((mfxU8)par->mfx.FrameInfo.ChromaFormat);

        if (UMC::NONE == m_vdPar.info.color_format)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        m_vdPar.lFlags |= par->mfx.DecodedOrder ? 0 : UMC::FLAG_VDEC_REORDER;
        m_vdPar.lFlags |= UMC::FLAG_VDEC_EXTERNAL_SURFACE_USE;

        switch(par->mfx.TimeStampCalc)
        {
            case MFX_TIMESTAMPCALC_TELECINE:
                m_vdPar.lFlags |= UMC::FLAG_VDEC_TELECINE_PTS;
                break;

            case MFX_TIMESTAMPCALC_UNKNOWN:
                break;

            default:
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        m_vdPar.numThreads = (mfxU16)(par->AsyncDepth ? par->AsyncDepth : m_pCore->GetAutoAsyncDepth());

        if (m_InitH || m_InitW)
        {
            if (2048 < par->mfx.FrameInfo.Width || 2048 < par->mfx.FrameInfo.Height)
            { 
                return MFX_ERR_INVALID_VIDEO_PARAM;
            }

            if (m_InitW < par->mfx.FrameInfo.Width || m_InitH < par->mfx.FrameInfo.Height)
            { 
                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
            }
        }

        m_isDecodedOrder = (1 == par->mfx.DecodedOrder) ? true : false;
    }
    else
    {
        m_InitW = 0;
        m_InitH = 0;
    }

    if (m_implUmc.get())
        m_implUmc->Reset();

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2InternalBase::Close()
{
    m_implUmc->Close();

    for (int i = 0; i < DPB; i++)
        if(mid[i] >= 0)
            m_FrameAllocator->DecreaseReference(mid[i]);

    for (int i = 0; i < DPB; i++)
    {
        mid[i] = -1;
    }

    for (int i = 0; i < NUM_FRAMES; i++)
    {
        if(m_frame[i].Data) delete []m_frame[i].Data;
        m_frame[i].Data = NULL;
    }

    if(m_FrameAllocator)
        m_FrameAllocator->Close();

    if(m_FrameAllocator_nv12)
    {
        delete m_FrameAllocator_nv12;
        m_FrameAllocator_nv12 = 0;
    }

    m_FrameAllocator = 0;

    mfxStatus mfxRes = MFX_ERR_NONE;

    if (0 < allocResponse.NumFrameActual)
    {
        mfxRes = m_pCore->FreeFrames(&allocResponse);
        memset(&allocResponse, 0, sizeof(mfxFrameAllocResponse));
    }

    if (0 < m_opaqueResponse.NumFrameActual && true == m_isOpaqueMemory)
    {
        mfxRes = m_pCore->FreeFrames(&m_opaqueResponse);
        memset(&m_opaqueResponse, 0, sizeof(mfxFrameAllocResponse));
    }

    m_isOpaqueMemory = false;

    return mfxRes;
}

mfxStatus VideoDECODEMPEG2InternalBase::Init(mfxVideoParam *par, VideoCORE * core)
{
    m_pCore = core;

    m_implUmc->Close();

    mfxStatus sts = AllocFrames(par);
    MFX_CHECK_STS(sts);

    Reset(par);

    Ipp32u size = Ipp32u (par->mfx.FrameInfo.Width * par->mfx.FrameInfo.Height *3L/2);
    Ipp32u MaxLength = 1024 * 500;

    if (size > MaxLength)
    {
        MaxLength = size;
    }

    for (int i = 0; i < NUM_FRAMES; i++)
    {
        memset(&m_frame[i], 0, sizeof(mfxBitstream));
        m_frame[i].MaxLength = MaxLength;
        m_frame[i].Data = new mfxU8[MaxLength];
        m_frame[i].DataLength = 0;
        m_frame[i].DataOffset = 0;
        m_frame_in_use[i] = false;
    }

    m_InitW = par->mfx.FrameInfo.Width;
    m_InitH = par->mfx.FrameInfo.Height;

    m_InitPicStruct = par->mfx.FrameInfo.PicStruct;
    m_Protected = par->Protected;
    m_isDecodedOrder = (1 == par->mfx.DecodedOrder) ? true : false;

    maxNumFrameBuffered = NUM_FRAMES_BUFFERED;
    // video conference mode
    // async depth == 1 means no bufferezation
    if (1 == par->AsyncDepth)
    {
        maxNumFrameBuffered = 1;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2InternalBase::QueryIOSurfInternal(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    if (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
    {
        request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
    }
    else if (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }
    else // opaque memory case
    {
        if (MFX_PLATFORM_SOFTWARE == core->GetPlatformType())
        {
            request->Type = MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
        }
        else
        {
            request->Type = MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
        }
    }

    if (MFX_PLATFORM_HARDWARE == core->GetPlatformType())
    {
        // check hardware restrictions
        if (false == IsHWSupported(core, par))
        {
            //request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;

            if (par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
            {
                request->Type = MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
            }
        }
    }

    memcpy_s(&(request->Info), sizeof(mfxFrameInfo), &(par->mfx.FrameInfo), sizeof(mfxFrameInfo));
    
    // output color format is NV12
    request->Info.FourCC = MFX_FOURCC_NV12;
    request->NumFrameMin = NUM_FRAMES_BUFFERED + 3;
    request->NumFrameSuggested = request->NumFrameMin = request->NumFrameMin + (par->AsyncDepth ? par->AsyncDepth : core->GetAutoAsyncDepth());
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2InternalBase::AllocFrames(mfxVideoParam *par)
{
    memset(&allocResponse, 0, sizeof(mfxFrameAllocResponse));

    mfxU16 IOPattern = par->IOPattern;

    // frames allocation
    int useInternal = m_isSWDecoder ? (IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) : (IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

    if (IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        mfxExtOpaqueSurfaceAlloc *pOpaqAlloc = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        if (!pOpaqAlloc)
            return MFX_ERR_INVALID_VIDEO_PARAM;

        useInternal = m_isSWDecoder ? !(pOpaqAlloc->Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY) : (pOpaqAlloc->Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY);
    }

    if (!m_isSWDecoder || useInternal || (IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
    {
        // positive condition means that decoder was configured as
        //  a. as hardware implementation or
        //  b. as software implementation with d3d9 output surfaces
        QueryIOSurfInternal(m_pCore, par, &allocRequest);

        //const mfxU16 NUM = allocRequest.NumFrameSuggested;
        mfxExtOpaqueSurfaceAlloc *pOpqExt = 0;
        if (MFX_IOPATTERN_OUT_OPAQUE_MEMORY & IOPattern) // opaque memory case
        {
            pOpqExt = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
            if (!pOpqExt || allocRequest.NumFrameMin > pOpqExt->Out.NumSurface)
                return MFX_ERR_INVALID_VIDEO_PARAM;

            m_isOpaqueMemory = true;

            if (m_isSWDecoder)
            {
                allocRequest.Type = (MFX_MEMTYPE_SYSTEM_MEMORY & pOpqExt->Out.Type) ? (mfxU16)pOpqExt->Out.Type :
                    (mfxU16)(MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET);
            }

            allocRequest.NumFrameMin = allocRequest.NumFrameSuggested = (mfxU16)pOpqExt->Out.NumSurface;
        }
        else
        {
            allocRequest.Type = MFX_MEMTYPE_FROM_DECODE;
            allocRequest.Type |= m_isSWDecoder ? MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_DXVA2_DECODER_TARGET;
            allocRequest.Type |= useInternal ? MFX_MEMTYPE_INTERNAL_FRAME : MFX_MEMTYPE_EXTERNAL_FRAME;
        }

        allocResponse.NumFrameActual = allocRequest.NumFrameSuggested;

        mfxStatus mfxSts;
        if (m_isOpaqueMemory)
        {
            mfxSts  = m_pCore->AllocFrames(&allocRequest, 
                                           &allocResponse, 
                                           pOpqExt->Out.Surfaces, 
                                           pOpqExt->Out.NumSurface);
        }
        else
        {
            bool isNeedCopy = ((MFX_IOPATTERN_OUT_SYSTEM_MEMORY & IOPattern) && (allocRequest.Type & MFX_MEMTYPE_INTERNAL_FRAME)) ||
                ((MFX_IOPATTERN_OUT_VIDEO_MEMORY & IOPattern) && (m_isSWDecoder));
            allocRequest.AllocId = par->AllocId;
#ifdef MFX_VA_WIN
            allocRequest.Type |= MFX_MEMTYPE_SHARED_RESOURCE;
#endif
            mfxSts = m_pCore->AllocFrames(&allocRequest, &allocResponse, isNeedCopy);
            if(mfxSts)
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        if (mfxSts != MFX_ERR_NONE && mfxSts != MFX_ERR_NOT_FOUND)
        {
            // second status means that external allocator was not found, it is ok
            return mfxSts;
        }

        if (!useInternal)
            m_FrameAllocator->SetExternalFramesResponse(&allocResponse);
    }

    UMC::Status sts = m_FrameAllocator->InitMfx(NULL, m_pCore, par, &allocRequest, &allocResponse, !useInternal, m_isSWDecoder);
    if (UMC::UMC_OK != sts)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2InternalBase::GetVideoParam(mfxVideoParam *par)
{
    par->IOPattern = m_vPar.IOPattern;
    par->mfx       = m_vPar.mfx;
    par->Protected = m_vPar.Protected;
    par->AsyncDepth = m_vPar.AsyncDepth;
    par->mfx.FrameInfo.AspectRatioW = m_vPar.mfx.FrameInfo.AspectRatioW;
    par->mfx.FrameInfo.AspectRatioH = m_vPar.mfx.FrameInfo.AspectRatioH;
    par->mfx.FrameInfo.FrameRateExtD = m_vPar.mfx.FrameInfo.FrameRateExtD;
    par->mfx.FrameInfo.FrameRateExtN = m_vPar.mfx.FrameInfo.FrameRateExtN;

    mfxExtCodingOptionSPSPPS *spspps = (mfxExtCodingOptionSPSPPS *) GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS);

    if (spspps && spspps->SPSBuffer)
    {
        UMC::Status sts = m_implUmc->GetSequenceHeaderMemoryMask(spspps->SPSBuffer, spspps->SPSBufSize);
        if (UMC::UMC_OK != sts)
            return ConvertUMCStatusToMfx(sts);
    }

    // get signal info buffer
    mfxExtVideoSignalInfo *signal_info = (mfxExtVideoSignalInfo *) GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
    if (signal_info)
    {
        m_implUmc->GetSignalInfoInformation(signal_info);
    }

    memcpy_s(par->reserved,sizeof(m_vPar.reserved),m_vPar.reserved,sizeof(m_vPar.reserved));
    par->reserved2 = m_vPar.reserved2;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2InternalBase::UpdateWorkSurfaceParams(int task_num)
{
    mfxFrameSurface1 *pSurface = m_FrameAllocator->GetSurfaceByIndex(mid[task_num]);
    MFX_CHECK_NULL_PTR1(pSurface);
    pSurface->Info.CropW = m_vPar.mfx.FrameInfo.CropW;
    pSurface->Info.CropH = m_vPar.mfx.FrameInfo.CropH;
    pSurface->Info.CropX = m_vPar.mfx.FrameInfo.CropX;
    pSurface->Info.CropY = m_vPar.mfx.FrameInfo.CropY;
    pSurface->Info.AspectRatioH = m_vPar.mfx.FrameInfo.AspectRatioH;
    pSurface->Info.AspectRatioW = m_vPar.mfx.FrameInfo.AspectRatioW;
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2InternalBase::UpdateCurrVideoParams(mfxFrameSurface1 *surface_work, int task_num)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameSurface1 *pSurface = surface_work;

    if (true == m_isOpaqueMemory)
    {
        pSurface = m_pCore->GetOpaqSurface(surface_work->Data.MemId, true);
    }

    if (dec_field_count & 1)
    {
        return MFX_ERR_NONE;
    }

    const UMC::sSequenceHeader& sh = m_implUmc->GetSequenceHeader();
    const UMC::sPictureHeader& ph = m_implUmc->GetPictureHeader(task_num);

    if (m_vPar.mfx.FrameInfo.AspectRatioH != 0 || m_vPar.mfx.FrameInfo.AspectRatioW != 0)
        if(sh.aspect_ratio_w != m_vPar.mfx.FrameInfo.AspectRatioW || sh.aspect_ratio_h != m_vPar.mfx.FrameInfo.AspectRatioH)
            sts = MFX_WRN_VIDEO_PARAM_CHANGED;

    if (m_isFrameRateFromInit == false && ((Ipp32u)sh.frame_rate_extension_d != m_vPar.mfx.FrameInfo.FrameRateExtD || (Ipp32u)sh.frame_rate_extension_n != m_vPar.mfx.FrameInfo.FrameRateExtN))
    {
        sts = MFX_WRN_VIDEO_PARAM_CHANGED;
    }

    //if (m_isSWImpl)
    {
        pSurface->Info.CropW = m_vPar.mfx.FrameInfo.CropW;
        pSurface->Info.CropH = m_vPar.mfx.FrameInfo.CropH;
        pSurface->Info.CropX = m_vPar.mfx.FrameInfo.CropX;
        pSurface->Info.CropY = m_vPar.mfx.FrameInfo.CropY;
        pSurface->Info.AspectRatioH = m_vPar.mfx.FrameInfo.AspectRatioH;
        pSurface->Info.AspectRatioW = m_vPar.mfx.FrameInfo.AspectRatioW;
    }

    UpdateMfxVideoParam(m_vPar, sh, ph);

    pSurface->Info.PicStruct = m_vPar.mfx.FrameInfo.PicStruct;    
    pSurface->Info.FrameRateExtD = m_vPar.mfx.FrameInfo.FrameRateExtD;
    pSurface->Info.FrameRateExtN = m_vPar.mfx.FrameInfo.FrameRateExtN;

    return sts;
}

mfxFrameSurface1 *VideoDECODEMPEG2InternalBase::GetOriginalSurface(mfxFrameSurface1 *pSurface)
{
    if (true == m_isOpaqueMemory)
    {
        return m_pCore->GetNativeSurface(pSurface);
    }

    return pSurface;
}

mfxStatus VideoDECODEMPEG2InternalBase::SetOutputSurfaceParams(mfxFrameSurface1 *surface, int display_index)
{
    MFX_CHECK_NULL_PTR1(surface);

    surface->Data.TimeStamp = GetMfxTimeStamp(m_implUmc->GetCurrDecodedTime(display_index));
    surface->Data.DataFlag = (mfxU16)((m_implUmc->isOriginalTimeStamp(display_index)) ? MFX_FRAMEDATA_ORIGINAL_TIMESTAMP : 0);
    SetSurfacePicStruct(surface, display_index);
    UpdateOutputSurfaceParamsFromWorkSurface(surface, display_index);
    SetSurfacePictureType(surface, display_index, m_implUmc.get());
    SetSurfaceTimeCode(surface, display_index, m_implUmc.get());

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2InternalBase::SetSurfacePicStruct(mfxFrameSurface1 *surface, int display_index)
{
    MFX_CHECK_NULL_PTR1(surface);

    if (display_index < 0 || display_index >= 2*DPB)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    const UMC::sPictureHeader& ph = m_implUmc->GetPictureHeader(display_index);
    const UMC::sSequenceHeader& sh = m_implUmc->GetSequenceHeader();

    surface->Info.PicStruct = GetMfxPicStruct(sh.progressive_sequence, ph.progressive_frame,
                                              ph.top_field_first, ph.repeat_first_field, ph.picture_structure,
                                              m_vPar.mfx.ExtendedPicStruct);
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2InternalBase::UpdateOutputSurfaceParamsFromWorkSurface(mfxFrameSurface1 *outputSurface, int display_index)
{
    if (!m_isSWBuf && !m_isOpaqueMemory)
        return MFX_ERR_NONE;

    mfxFrameSurface1 *internalSurface = m_FrameAllocator->GetSurfaceByIndex(mid[display_index]);
    MFX_CHECK_NULL_PTR1(internalSurface);
    outputSurface->Info.CropW = internalSurface->Info.CropW;
    outputSurface->Info.CropH = internalSurface->Info.CropH;
    outputSurface->Info.CropX = internalSurface->Info.CropX;
    outputSurface->Info.CropY = internalSurface->Info.CropY;
    outputSurface->Info.AspectRatioH = internalSurface->Info.AspectRatioH;
    outputSurface->Info.AspectRatioW = internalSurface->Info.AspectRatioW;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2InternalBase::GetOutputSurface(mfxFrameSurface1 **surface_out, mfxFrameSurface1 *surface_work, UMC::FrameMemID index)
{
    mfxFrameSurface1 *pNativeSurface =  m_FrameAllocator->GetSurface(index, surface_work, &m_vPar);
    
    if (pNativeSurface)
    {
        if (m_isOpaqueMemory)
            *surface_out = m_pCore->GetOpaqSurface(pNativeSurface->Data.MemId);
        else 
            *surface_out = pNativeSurface;
    }
    else
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2InternalBase::ConstructFrame(mfxBitstream *bs, mfxFrameSurface1 *surface_work)
{
    if (NeedToReturnCriticalStatus(bs))
        return ReturningCriticalStatus();

    if (m_frame_constructed && bs != NULL)
    {
        if (!SetFree_m_frame())
        {
            return MFX_WRN_DEVICE_BUSY;
        }

        m_frame[m_frame_free].DataLength = 0;
        m_frame[m_frame_free].DataOffset = 0;
    }

    m_frame_constructed = false;

    if (bs)
    {
        if (!bs->DataLength)
        {
            return MFX_ERR_MORE_DATA;
        }

        if (true == m_new_bs)
        {
            m_time[m_frame_free] = GetUmcTimeStamp(bs->TimeStamp);
            m_time[m_frame_free] = (m_time[m_frame_free]==last_good_timestamp)?-1.0:m_time[m_frame_free];    
        }

        mfxStatus sts = ConstructFrame(bs, &m_frame[m_frame_free], surface_work);

        if (MFX_ERR_NONE != sts)
        {
            m_frame_constructed = false;
            m_new_bs   = false;
            return sts;
        }

        last_good_timestamp = m_time[m_frame_free] == -1.0?last_good_timestamp:m_time[m_frame_free];
        m_frame_in_use[m_frame_free] = true;
        m_new_bs = true;
    }
    else
    {
        m_time[m_frame_free] = (mfxF64)(-1);

        if (0 < m_last_bytes[3])
        {
            Ipp8u *pData = m_frame[m_frame_free].Data + m_frame[m_frame_free].DataOffset + m_frame[m_frame_free].DataLength;
            memcpy_s(pData, m_last_bytes[3], m_last_bytes, m_last_bytes[3]);

            m_frame[m_frame_free].DataLength += m_last_bytes[3];
            memset(m_last_bytes, 0, NUM_REST_BYTES);
        }
    }

    m_found_SH = false;
    m_frame_constructed = true;

    if (!(dec_field_count & 1)) {
        UMC::AutomaticUMCMutex guard(m_guard);
        m_task_num = m_implUmc->FindFreeTask();
    }

    if (-1 == m_task_num)
    {
        return MFX_WRN_DEVICE_BUSY;
    }

    if (last_frame_count > 0)
    {
        if(m_implUmc->GetRetBufferLen() <= 0)
            return MFX_ERR_MORE_DATA;
    }

    mfxStatus sts = m_FrameAllocator->SetCurrentMFXSurface(surface_work, m_isOpaqueMemory);
    MFX_CHECK_STS(sts);
    if (m_FrameAllocator->FindFreeSurface() == -1)
        return MFX_WRN_DEVICE_BUSY;

    if (false == SetCurr_m_frame() && bs)
    {
        return MFX_WRN_DEVICE_BUSY;
    }

    return sts;
}

mfxStatus VideoDECODEMPEG2InternalBase::ConstructFrame(mfxBitstream *in, mfxBitstream *out, mfxFrameSurface1 *surface_work)
{
    mfxStatus sts = MFX_ERR_NONE;
    do
    {
        sts = ConstructFrameImpl(in, out, surface_work);

        if (sts == MFX_ERR_NOT_ENOUGH_BUFFER)
        {
            out->DataLength = 0;
            out->DataOffset = 0;
            memset(m_last_bytes, 0, NUM_REST_BYTES);
            ResetFcState(m_fcState);
        }
    } while (MFX_ERR_NOT_ENOUGH_BUFFER == sts);

    return sts;
}

mfxStatus VideoDECODEMPEG2InternalBase::ConstructFrameImpl(mfxBitstream *in, mfxBitstream *out, mfxFrameSurface1 *surface_work)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoDECODEMPEG2::ConstructFrameImpl");
    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR2(in, out);

    MFX_CHECK_NULL_PTR2(in->Data, out->Data);

    if (MINIMAL_BITSTREAM_LENGTH > in->DataLength)
    {
        AppendBitstream(*out, in->Data + in->DataOffset, in->DataLength);
        MoveBitstreamData(*in, (mfxU32)(in->DataLength));
        memset(m_last_bytes,0,NUM_REST_BYTES);

        return MFX_ERR_MORE_DATA;
    }

    // calculate tail of bitstream chunk
    const mfxU8 *tail = in->Data + in->DataOffset + in->DataLength;

    while (!m_fcState.picStart)
    {
        const mfxU8* head = in->Data + in->DataOffset;
        const mfxU8* curr = FindStartCode(head, tail);

        if (curr >= tail - 3)
        {
            MoveBitstreamData(*in, (mfxU32)(curr - head));
            memset(m_last_bytes, 0, NUM_REST_BYTES);

            return MFX_ERR_MORE_DATA;
        }
        if (eSEQ == curr[3] && false == m_found_SH)
        {
            if(tail < curr + 6) 
            {
                return MFX_ERR_MORE_DATA;
            }

            mfxU16 CropW = (curr[4] << 4) + ((curr[5] >> 4) & 0xf);
            mfxU16 CropH = ((curr[5] & 0xf) << 8) + curr[6];
            mfxU16 Width = (CropW + 15) & ~0x0f;
            mfxU16 Height = (CropH + 15) & ~0x0f;

            const mfxU8* ptr = FindStartCodeEx(curr + 4, tail);

            // check that data length is enough to read whole sequence extension
            if(tail < ptr + 7) 
            {
                return MFX_ERR_MORE_DATA;
            }

            if (eEXT == ptr[3])
            {
                Ipp32u code = (ptr[4] & 0xf0);

                if(0x10 == code)
                {
                    code = (ptr[4] << 24) | (ptr[5] << 16) | (ptr[6] << 8) | ptr[7];
                    Ipp32u progressive_seq = (code >> 19) & 1;

                    CropW = (mfxU16)((CropW  & 0xfff) | ((code >> (15-12)) & 0x3000));
                    CropH = (mfxU16)((CropH & 0xfff) | ((code >> (13-12)) & 0x3000));
                    Width = (CropW + 15) & ~0x0f;
                    Height = (CropH + 15) & ~0x0f;
                    if(0 == progressive_seq)
                    {
                        Height = (CropH + 31) & ~(31);
                    }

                    mfxU8 profile_and_level = (code >> 20) & 0xff;
//                  mfxU8 profile = (profile_and_level >> 4) & 0x7;
                    mfxU8 level = profile_and_level & 0xf;

                    switch(level)
                    {
                        case  4:
                        case  6:
                        case  8:
                        case 10:
                            break;
                        default:
                            MoveBitstreamData(*in, (mfxU32)(curr - head) + 4);
                            memset(m_last_bytes, 0, NUM_REST_BYTES);
                            continue;
                    }

                    mfxU8 chroma = (code >> 17) & 3;
                    const int chroma_yuv420 = 1;
                    if (chroma != chroma_yuv420)
                    {
                        MoveBitstreamData(*in, (mfxU32)(curr - head) + 4);
                        memset(m_last_bytes, 0, NUM_REST_BYTES);
                        continue;
                    }
                }
            }
            else
            {
                if(m_InitPicStruct != MFX_PICSTRUCT_PROGRESSIVE)
                {
                    Height = (CropH + 31) & ~(31);
                }
            }
#ifdef MFX_ENABLE_HW_ONLY_MPEG2_DECODER
            mfxVideoParam vpCopy = m_vPar;
            vpCopy.mfx.FrameInfo.Width = Width;
            vpCopy.mfx.FrameInfo.Height = Height;

            if (!IsHWSupported(m_pCore, &vpCopy) || Width == 0 || Height == 0)
            {
                MoveBitstreamData(*in, (mfxU32)(curr - head) + 4);
                memset(m_last_bytes, 0, NUM_REST_BYTES);
                continue;
            }
#endif
            if (m_InitW <  Width || m_InitH < Height || surface_work->Info.Width <  Width || surface_work->Info.Height < Height)
            {
                m_resizing = true;

                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
            }

            m_found_SH = true;

            if (false == m_first_SH && (surface_work->Info.CropW <  CropW || surface_work->Info.CropH < CropH))
            {
                m_resizing = true;

                return MFX_WRN_VIDEO_PARAM_CHANGED;
            }

            if (false == m_first_SH)
                if (m_InitW >  Width || m_InitH > Height)
                {
                    m_resizing = true;  
                    return MFX_WRN_VIDEO_PARAM_CHANGED;
                }

            m_first_SH = false;

            if (eEXT == ptr[3])
            {
                sts = AppendBitstream(*out, curr, (mfxU32)(ptr - curr));
                MFX_CHECK_STS(sts);
                curr = ptr;
            }
        }

        MoveBitstreamData(*in, (mfxU32)(curr - head));

        if (curr[3] == eSEQ || 
            curr[3] == eEXT ||  
            curr[3] == ePIC ||
            curr[3] == eGROUP)
        {
            sts = AppendBitstream(*out, curr, 4);
            MFX_CHECK_STS(sts);

            MoveBitstreamData(*in, 4);

            m_fcState.picStart = 1;

            if (ePIC == curr[3])
            {
                m_fcState.picHeader = FcState::FRAME;

                if (MFX_BITSTREAM_COMPLETE_FRAME == in->DataFlag)
                {
                    Ipp32u len = in->DataLength;

                    const mfxU8* head = in->Data + in->DataOffset;
                    const mfxU8* curr = FindStartCode(head, tail);

                    MFX_CHECK(curr >= head, MFX_ERR_UNDEFINED_BEHAVIOR);

                    // if start code found
                    if( (tail - curr) > 3 && 
                        (curr[3] == eSEQ || 
                         curr[3] == eEXT || 
                         curr[3] == ePIC || 
                         curr[3] == eEND || 
                         curr[3] == eGROUP)
                      )
                    {
                        len = (mfxU32)(curr - head);
                    }

                    MFX_CHECK(out->MaxLength >= out->DataLength, MFX_ERR_UNDEFINED_BEHAVIOR);
                    
                    if(out->MaxLength - out->DataLength < len)
                    {
                        len = out->MaxLength - out->DataLength;
                    }

                    sts = AppendBitstream(*out, head, len);
                    MFX_CHECK_STS(sts);

                    MoveBitstreamData(*in, len);
                    
                    m_fcState.picStart = 0;
                    m_fcState.picHeader = FcState::NONE;
                    memset(m_last_bytes, 0, NUM_REST_BYTES);

                    if(m_resizing)
                    {              
                        m_resizing = false; 
                        //return MFX_WRN_VIDEO_PARAM_CHANGED;
                    }

                    return MFX_ERR_NONE;
                }
            }
        }
        else
        {
            MoveBitstreamData(*in, 4);
        }
    }

    for ( ; ; )
    {
        const mfxU8* head = in->Data + in->DataOffset;
        const mfxU8* curr = FindStartCode(head, tail);
        
        if (curr + 3 >= tail)
        {
            // Not enough buffer, it is possible due to long user data. Try to "cut" it off
            if (out->DataOffset + out->DataLength + (curr - head) > out->MaxLength)
            {
                sts = CutUserData(in, out, curr);
                MFX_CHECK_STS(sts);
            }

            head = in->Data + in->DataOffset;
            sts = AppendBitstream(*out, head, (mfxU32)(curr - head));
            MFX_CHECK_STS(sts);
            
            MoveBitstreamData(*in, (mfxU32)(curr - head));
            
            mfxU8 *p = (mfxU8 *) curr;

            m_last_bytes[3] = 0;
            
            while(p < tail)
            {
                m_last_bytes[m_last_bytes[3]] = *p;
                p++;
                m_last_bytes[3]++;
            }

            if(m_resizing)
            {          
                m_resizing = false; 
                //return MFX_WRN_VIDEO_PARAM_CHANGED;
            }

            return MFX_ERR_MORE_DATA;
        }
        else
        {
            if (eEND == curr[3] && m_fcState.picHeader == FcState::FRAME)
            {
                // append end_sequence_code to the end of current picture
                curr += 4;
            }

            // Not enough buffer, it is possible due to long user data. Try to "cut" it off
            if (out->DataOffset + out->DataLength + (curr - head) > out->MaxLength)
            {
                sts = CutUserData(in, out, curr);
                MFX_CHECK_STS(sts);
            }

            head = in->Data + in->DataOffset;
            sts = AppendBitstream(*out, head, (mfxU32)(curr - head));
            MFX_CHECK_STS(sts);

            MoveBitstreamData(*in, (mfxU32)(curr - head));

            if (FcState::FRAME == m_fcState.picHeader)
            {
                // CQ21432: If buffer contains less than 8 bytes it means there is no full frame in that buffer
                // and we need to find next start code
                // It is possible in case of corruption when start code is found inside of frame
                if (out->DataLength > 8)
                {
                    m_fcState.picStart = 0;
                    m_fcState.picHeader = FcState::NONE;
                    memset(m_last_bytes, 0, NUM_REST_BYTES);

                    return MFX_ERR_NONE;
                }
            }

            if (ePIC == curr[3])
            {
                // FIXME: for now assume that all pictures are frames
                m_fcState.picHeader = FcState::FRAME;

                if(in->DataFlag == MFX_BITSTREAM_COMPLETE_FRAME)
                {
                    sts = AppendBitstream(*out, curr, 4);
                    MFX_CHECK_STS(sts);

                    MoveBitstreamData(*in, 4);

                    Ipp32u len = in->DataLength;

                    const mfxU8* head = in->Data + in->DataOffset;
                    const mfxU8* curr = FindStartCode(head, tail);

                    MFX_CHECK(curr >= head, MFX_ERR_UNDEFINED_BEHAVIOR);

                    // start code was found
                    if( (tail - curr) > 3 &&
                        (curr[3] == eSEQ  ||
                         curr[3] == eEXT ||
                         curr[3] == ePIC ||
                         curr[3] == eEND ||
                         curr[3] == eGROUP))
                    {
                        len = (Ipp32u)(curr - head);
                    }

                    MFX_CHECK(out->MaxLength >= out->DataLength, MFX_ERR_UNDEFINED_BEHAVIOR);

                    if(out->MaxLength - out->DataLength < len)
                    {
                        len = out->MaxLength - out->DataLength;
                    }

                    sts = AppendBitstream(*out, head, len);
                    MFX_CHECK_STS(sts);

                    MoveBitstreamData(*in, len);
                    m_fcState.picStart = 0;
                    m_fcState.picHeader = FcState::NONE;
                    memset(m_last_bytes, 0, NUM_REST_BYTES);

                    return MFX_ERR_NONE;
                }
            }

            sts = AppendBitstream(*out, curr, 4);
            MFX_CHECK_STS(sts);

            MoveBitstreamData(*in, 4);
        }
    }
}

#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)

static bool IsStatusReportEnable(VideoCORE * core)
{
    core; // touch unreferenced parameter
    UMC::VideoAccelerator *va;
    core->GetVA((mfxHDL*)&va, MFX_MEMTYPE_FROM_DECODE);
    
    if (true == va->IsUseStatusReport())
    {
        //eMFXHWType type = core->GetHWType();
        //return (type == MFX_HW_SNB) || (type == MFX_HW_IVB) || (type == MFX_HW_HSW) || (type == MFX_HW_HSW_ULT) || (type == MFX_HW_VLV) || (type == MFX_HW_BDW) || (type == MFX_HW_SCL);
        return true;
    }

    return false;
}

mfxStatus VideoDECODEMPEG2Internal_HW::ConstructFrame(mfxBitstream *bs, mfxFrameSurface1 *surface_work)
{
#ifdef MFX_VA_WIN
    if (m_Protected && bs != NULL)
    {
        VM_ASSERT(m_implUmcHW->pack_w.m_va->GetProtectedVA());
        if (!m_implUmcHW->pack_w.m_va->GetProtectedVA() || !(bs->DataFlag & MFX_BITSTREAM_COMPLETE_FRAME))
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        m_implUmcHW->pack_w.m_va->GetProtectedVA()->SetModes(&m_vPar);
        m_implUmcHW->pack_w.m_va->GetProtectedVA()->SetBitstream(bs);

        m_implUmcHW->pack_w.IsProtectedBS = true;
        if (!bs->EncryptedData) m_implUmcHW->pack_w.IsProtectedBS = false;
    }
#endif

    return VideoDECODEMPEG2InternalBase::ConstructFrame(bs, surface_work);
}

VideoDECODEMPEG2Internal_HW::VideoDECODEMPEG2Internal_HW() : m_pavpOpt()
{
    m_FrameAllocator_d3d = new mfx_UMC_FrameAllocator_D3D;
    m_FrameAllocator = m_FrameAllocator_d3d;
    m_isSWDecoder = false;

#ifdef UMC_VA_DXVA
    memset(m_pStatusReport, 0, sizeof(DXVA_Status_VC1) * MPEG2_STATUS_REPORT_NUM);
#endif

    m_implUmcHW = new UMC::MPEG2VideoDecoderHW();
    m_implUmc.reset(m_implUmcHW);
}

mfxStatus VideoDECODEMPEG2Internal_HW::Init(mfxVideoParam *par, VideoCORE * core)
{
    mfxStatus sts = VideoDECODEMPEG2InternalBase::Init(par, core);
    MFX_CHECK_STS(sts);

    if (IS_PROTECTION_ANY(par->Protected))
    {
        mfxExtPAVPOption * pavpOpt = (mfxExtPAVPOption*)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);
        if (IS_PROTECTION_PAVP_ANY(par->Protected) && !pavpOpt)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        if (!IS_PROTECTION_PAVP_ANY(par->Protected) && pavpOpt)
            return MFX_ERR_INVALID_VIDEO_PARAM;

        if (pavpOpt)
            memcpy_s(&m_pavpOpt, sizeof(mfxExtPAVPOption), pavpOpt, sizeof(mfxExtPAVPOption));
    }

    m_vdPar.numThreads = 1;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2Internal_HW::Reset(mfxVideoParam *par)
{
    mfxStatus sts = VideoDECODEMPEG2InternalBase::Reset(par);
    MFX_CHECK_STS(sts);

#ifdef UMC_VA_DXVA
    memset(m_pStatusReport, 0, sizeof(DXVA_Status_VC1) * MPEG2_STATUS_REPORT_NUM);
#endif

    m_NumThreads = 1;

    if (par && IS_PROTECTION_PAVP_ANY(par->Protected))
    {
        mfxExtPAVPOption * pavpOpt = (mfxExtPAVPOption*)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);
        if (!pavpOpt)
            return MFX_ERR_INVALID_VIDEO_PARAM;

        memcpy_s(&m_pavpOpt, sizeof(mfxExtPAVPOption), pavpOpt, sizeof(mfxExtPAVPOption));

#if defined (MFX_VA_WIN)
        if (IS_PROTECTION_PAVP_ANY(par->Protected))
        {
            if (m_vdPar.pVideoAccelerator->GetProtectedVA())
            {
                if (m_vdPar.pVideoAccelerator->GetProtectedVA()->SetModes(par) != UMC::UMC_OK)
                    return MFX_ERR_INVALID_VIDEO_PARAM;
            }
            else
            {
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }

        }
#endif
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2Internal_HW::Close()
{
    mfxStatus sts = VideoDECODEMPEG2InternalBase::Close();
    if (m_FrameAllocator_d3d)
    {
        delete m_FrameAllocator_d3d;
        m_FrameAllocator_d3d = 0;
    }

    return sts;
}

mfxStatus VideoDECODEMPEG2Internal_HW::AllocFrames(mfxVideoParam *par)
{
    mfxStatus sts = VideoDECODEMPEG2InternalBase::AllocFrames(par);
    MFX_CHECK_STS(sts);

    // create directx video accelerator
    if (m_pCore->CreateVA(par, &allocRequest, &allocResponse, m_FrameAllocator) != MFX_ERR_NONE)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    m_pCore->GetVA((mfxHDL*)&m_vdPar.pVideoAccelerator, MFX_MEMTYPE_FROM_DECODE);
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2Internal_HW::GetVideoParam(mfxVideoParam *par)
{
    mfxExtPAVPOption * buffer = (mfxExtPAVPOption*)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);
    if (buffer)
    {
        if (!IS_PROTECTION_PAVP_ANY(m_vPar.Protected))
            return MFX_ERR_INVALID_VIDEO_PARAM;

        memcpy_s(buffer, sizeof(mfxExtPAVPOption), &m_pavpOpt, sizeof(mfxExtPAVPOption));
    }

    return VideoDECODEMPEG2InternalBase::GetVideoParam(par);
}


mfxStatus VideoDECODEMPEG2Internal_HW::RestoreDecoder(Ipp32s frame_buffer_num, UMC::FrameMemID mem_id_to_unlock, Ipp32s task_num_to_unlock, bool end_frame, bool remove_2frames, int decrease_dec_field_count)
{
    end_frame;
    m_frame[frame_buffer_num].DataLength = 0;
    m_frame[frame_buffer_num].DataOffset = 0;
    m_frame_in_use[frame_buffer_num] = false;
    
    if (mem_id_to_unlock >= 0 && mem_id_to_unlock < DPB)
        m_FrameAllocator->DecreaseReference(mem_id_to_unlock);

    if (task_num_to_unlock >= 0 && task_num_to_unlock < 2*DPB) {
        UMC::AutomaticUMCMutex guard(m_guard);
        m_implUmc->UnLockTask(task_num_to_unlock);
    }

#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
    if (end_frame)
        m_implUmcHW->pack_w.m_va->EndFrame();
#endif

    if (remove_2frames)
        m_implUmcHW->RestoreDecoderStateAndRemoveLastField();
    else
        m_implUmcHW->RestoreDecoderState();

    dec_field_count -= decrease_dec_field_count;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2Internal_HW::DecodeFrameCheck(mfxBitstream *bs,
                               mfxFrameSurface1 *surface_work,
                               mfxFrameSurface1 **surface_disp,
                               MFX_ENTRY_POINT *pEntryPoint)
{
    int display_index = -1;

    m_in[m_task_num].SetBufferPointer(m_frame[m_frame_curr].Data + m_frame[m_frame_curr].DataOffset,
                                        m_frame[m_frame_curr].DataLength);

    m_in[m_task_num].SetDataSize(m_frame[m_frame_curr].DataLength);
    m_in[m_task_num].SetTime(m_time[m_frame_curr]);

    UMC::Status umcRes = UMC::UMC_OK;

    bool IsField = false;
    bool IsSkipped = false;

    if (8 < m_frame[m_frame_curr].DataLength)
    {
        m_implUmcHW->SaveDecoderState();
        umcRes = m_implUmc->GetPictureHeader(&m_in[m_task_num], m_task_num, m_prev_task_num);

        //VM_ASSERT( m_implUmc.PictureHeader[m_task_num].picture_coding_type != 3 || ( mid[ m_implUmc.frame_buffer.latest_next ] != -1 && mid[ m_implUmc.frame_buffer.latest_prev ] != -1 ));

        IsField = !m_implUmc->IsFramePictureStructure(m_task_num);
        if (m_task_num >= DPB && !IsField)
        {
            int decrease_dec_field_count = dec_field_count % 2 == 0 ? 0 : 1;
            Ipp32s previous_field = m_task_num - DPB;
            MFX_CHECK_STS(RestoreDecoder(m_frame_curr, mid[previous_field], previous_field, NO_END_FRAME, REMOVE_LAST_2_FRAMES, decrease_dec_field_count))
            return MFX_ERR_MORE_DATA;
        }

        if (UMC::UMC_OK != umcRes)
        {
            MFX_CHECK_STS(RestoreDecoder(m_frame_curr, NO_SURFACE_TO_UNLOCK, NO_TASK_TO_UNLOCK, NO_END_FRAME, REMOVE_LAST_FRAME, 0))

            IsSkipped = m_implUmc->IsFrameSkipped();

            if (IsSkipped && !(dec_field_count & 1))
            {
                skipped_frame_count += 1;
            }

            if (false == m_reset_done && NULL == bs)
            {
                display_index = m_implUmc->GetDisplayIndex();

                if (0 > display_index)
                {
                    umcRes = m_implUmc->GetPictureHeader(NULL, m_task_num, m_prev_task_num);
                            
                    if (UMC::UMC_ERR_INVALID_STREAM == umcRes)
                    {
                        return MFX_ERR_UNKNOWN;
                    }

                    display_index = m_implUmc->GetDisplayIndex();
                }

                if (0 <= display_index)
                {
                    mfxStatus sts = GetOutputSurface(surface_disp, surface_work, mid[display_index]);
                    if (sts < MFX_ERR_NONE)
                        return sts;

                    if (true == m_isDecodedOrder)
                    {
                        (*surface_disp)->Data.FrameOrder = 0xffffffff;
                    }

                    SetOutputSurfaceParams(*surface_disp, display_index);

                    if (false == m_isDecodedOrder)
                    {
                        (*surface_disp)->Data.FrameOrder = display_frame_count;
                    }

                    display_frame_count++;

                    if (true == m_isDecodedOrder)
                    {
                        (*surface_disp)->Data.FrameOrder = display_order;
                        display_order++;
                    }

                    if ((true == IsField && !(dec_field_count & 1)) || false == IsField)
                    {
                        pEntryPoint->requiredNumThreads = m_NumThreads;
                        pEntryPoint->pRoutine = &MPEG2TaskRoutine;
                        pEntryPoint->pState = (void*)this;
                        pEntryPoint->pRoutineName = (char*)"DecodeMPEG2";

                        m_task_param[m_task_num].NumThreads = m_implUmc->GetCurrThreadsNum(m_task_num);
                        m_task_param[m_task_num].m_frame_curr = m_frame_curr;
                        m_task_param[m_task_num].m_isDecodedOrder = m_isDecodedOrder;
                        m_task_param[m_task_num].curr_index = curr_index;
                        m_task_param[m_task_num].prev_index = prev_index;
                        m_task_param[m_task_num].next_index = next_index;
                        m_task_param[m_task_num].surface_out = GetOriginalSurface(*surface_disp);
                        m_task_param[m_task_num].surface_work = surface_work;
                        m_task_param[m_task_num].display_index = display_index;
                        m_task_param[m_task_num].m_FrameAllocator = m_FrameAllocator;
                        m_task_param[m_task_num].mid = mid;
                        m_task_param[m_task_num].in = &m_in[m_task_num];
                        m_task_param[m_task_num].m_frame = m_frame;
                        m_task_param[m_task_num].m_frame_in_use = m_frame_in_use;
                        m_task_param[m_task_num].task_num = m_task_num;
                        m_task_param[m_task_num].m_isSoftwareBuffer = m_isSWBuf;

                        pEntryPoint->pParam = (void *)(&(m_task_param[m_task_num]));
                    }

                    return MFX_ERR_NONE;
                }
            }

            return MFX_ERR_MORE_DATA;
        }

        m_reset_done = false;

        umcRes = m_implUmc->GetInfo(&m_vdPar);

        mfxStatus sts = UpdateCurrVideoParams(surface_work, m_task_num);
        if (sts != MFX_ERR_NONE && sts != MFX_WRN_VIDEO_PARAM_CHANGED)
            return sts;

        if (surface_work->Info.CropW > surface_work->Info.Width || surface_work->Info.CropH > surface_work->Info.Height)
        {
            return MFX_ERR_UNKNOWN;
        }

        if ((false == m_isDecodedOrder && maxNumFrameBuffered <= (Ipp32u)(m_implUmc->GetRetBufferLen())) ||
                true == m_isDecodedOrder)
        {
            display_index = m_implUmc->GetDisplayIndex();
        }

        if (false == IsField || (true == IsField && !(dec_field_count & 1)))
        {
            curr_index = m_task_num;
            next_index = m_implUmc->GetNextDecodingIndex(curr_index);
            prev_index = m_implUmc->GetPrevDecodingIndex(curr_index);

            UMC::VideoDataInfo Info;

            if (1 == m_implUmc->GetSequenceHeader().progressive_sequence)
            {
                Info.Init(surface_work->Info.Width,
                            (surface_work->Info.CropH + 15) & ~0x0f,
                            m_vdPar.info.color_format);
            }
            else
            {
                Info.Init(surface_work->Info.Width,
                            surface_work->Info.Height,
                            m_vdPar.info.color_format);
            }

            m_FrameAllocator->Alloc(&mid[curr_index], &Info, 0);

            if (0 > mid[curr_index])
            {
                return MFX_ERR_LOCK_MEMORY;
            }

            m_FrameAllocator->IncreaseReference(mid[curr_index]);

#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
            umcRes = m_implUmcHW->pack_w.m_va->BeginFrame(mid[curr_index]);

            if (UMC::UMC_OK != umcRes)
            {
                return MFX_ERR_DEVICE_FAILED;
            }

            m_implUmcHW->pack_w.va_index = mid[curr_index];
#endif
        }
        else
        {
#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
            umcRes = m_implUmcHW->pack_w.m_va->BeginFrame(mid[curr_index]);

            if (UMC::UMC_OK != umcRes)
            {
                return MFX_ERR_DEVICE_FAILED;
            }

            m_implUmcHW->pack_w.va_index = mid[curr_index];
#endif
        }

        mfxStatus s = UpdateWorkSurfaceParams(curr_index);
        MFX_CHECK_STS(s);

        umcRes = m_implUmc->ProcessRestFrame(m_task_num);
        if (UMC::UMC_OK != umcRes)
        {
            MFX_CHECK_STS(RestoreDecoder(m_frame_curr, mid[curr_index], NO_TASK_TO_UNLOCK, END_FRAME, REMOVE_LAST_FRAME, 0))
            return MFX_ERR_MORE_DATA;
        }

        if (true == IsField)
        {
            dec_field_count++;
        }
        else
        {
            dec_field_count += 2;
            cashed_frame_count++;
        }

        dec_frame_count++;

        memset(&m_task_param[m_task_num],0,sizeof(MParam));
        m_implUmc->LockTask(m_task_num);

        umcRes = m_implUmc->DecodeSlices(0, m_task_num);

        if (UMC::UMC_OK != umcRes &&
            UMC::UMC_ERR_NOT_ENOUGH_DATA != umcRes &&
            UMC::UMC_ERR_SYNC != umcRes)
        {
            MFX_CHECK_STS(RestoreDecoder(m_frame_curr, mid[curr_index], m_task_num, END_FRAME, REMOVE_LAST_FRAME, IsField?1:2))
            return MFX_ERR_MORE_DATA;
        }

        umcRes = m_implUmcHW->PostProcessFrame(display_index, m_task_num);

        if (umcRes != UMC::UMC_OK && umcRes != UMC::UMC_ERR_NOT_ENOUGH_DATA)
        {
            MFX_CHECK_STS(RestoreDecoder(m_frame_curr, mid[curr_index], m_task_num, END_FRAME, REMOVE_LAST_FRAME, IsField?1:2))
            return MFX_ERR_MORE_DATA;
        }

        m_frame[m_frame_curr].DataLength = 0;
        m_frame[m_frame_curr].DataOffset = 0;

        m_prev_task_num = m_task_num;

        if (true == IsField && (dec_field_count & 1))
        {
            if (DPB <= m_task_num)
            {
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }

            m_task_num += DPB;
        }

        STATUS_REPORT_DEBUG_PRINTF("use task idx %d\n", m_task_num)
           
        if (0 <= display_index)
        {
            mfxStatus sts = GetOutputSurface(surface_disp, surface_work, mid[display_index]);
            if (sts < MFX_ERR_NONE)
                return sts;
        }

        if ((true == IsField && !(dec_field_count & 1)) || false == IsField)
        {
            pEntryPoint->requiredNumThreads = m_NumThreads;
            pEntryPoint->pRoutine = &MPEG2TaskRoutine;
            pEntryPoint->pState = (void*)this;
            pEntryPoint->pRoutineName = (char *)"DecodeMPEG2";

            m_task_param[m_task_num].NumThreads = m_implUmc->GetCurrThreadsNum(m_task_num);
            m_task_param[m_task_num].m_isDecodedOrder = m_isDecodedOrder;
            m_task_param[m_task_num].m_frame_curr = m_frame_curr;

            STATUS_REPORT_DEBUG_PRINTF("frame curr %d\n", m_frame_curr)

            m_task_param[m_task_num].curr_index = curr_index;
            m_task_param[m_task_num].prev_index = prev_index;
            m_task_param[m_task_num].next_index = next_index;
            m_task_param[m_task_num].surface_out = GetOriginalSurface(*surface_disp);
            m_task_param[m_task_num].surface_work = surface_work;
            m_task_param[m_task_num].display_index = display_index;
            m_task_param[m_task_num].m_FrameAllocator = m_FrameAllocator;
            m_task_param[m_task_num].mid = mid;
            m_task_param[m_task_num].in = &m_in[m_task_num];
            m_task_param[m_task_num].m_frame = m_frame;
            m_task_param[m_task_num].m_frame_in_use = m_frame_in_use;
            m_task_param[m_task_num].task_num = m_task_num;
            m_task_param[m_task_num].m_isSoftwareBuffer = m_isSWBuf;
            m_task_param[m_task_num].IsSWImpl = false;

            pEntryPoint->pParam = (void *)(&(m_task_param[m_task_num]));
        }
        else
        {
            m_frame_in_use[m_frame_curr] = false;
        }

        if (0 <= display_index)
        {
            SetOutputSurfaceParams(*surface_disp, display_index);

            if (false == m_isDecodedOrder)
            {
                (*surface_disp)->Data.FrameOrder = display_frame_count;
            }
            else
            {
                (*surface_disp)->Data.FrameOrder = 0xffffffff;
            }

            display_frame_count++;

            if (true == m_isDecodedOrder)
            {
                if (B_PICTURE == m_implUmc->GetFrameType(display_index))
                {
                    (*surface_disp)->Data.FrameOrder = display_order;
                    display_order++;
                }
                else // I or P
                {
                    Ipp32s p_index = m_implUmc->GetPrevDecodingIndex(display_index);

                    if (0 <= p_index)
                    {
                        mfxFrameSurface1 *pSurface;

                        pSurface = m_FrameAllocator->GetSurface(mid[p_index], surface_work, &m_vPar);

                        if (NULL == pSurface)
                        {
                            return MFX_ERR_UNDEFINED_BEHAVIOR;
                        }

                        pSurface->Data.FrameOrder = display_order;
                        display_order++;
                    }
                }
            }

            m_implUmc->PostProcessUserData(display_index);

            return sts;

        } // display_index >=0

        if (true == m_isDecodedOrder)
        {
            return MFX_ERR_MORE_DATA;
        }

        return MFX_ERR_MORE_SURFACE;

    }
    else // if (8 >= m_frame[m_frame_curr].DataLength)
    {
        last_frame_count++;

        umcRes = m_implUmc->GetPictureHeader(NULL,m_task_num,m_prev_task_num);
        UpdateCurrVideoParams(surface_work, m_task_num);

        if (UMC::UMC_ERR_INVALID_STREAM == umcRes)
        {
            return MFX_ERR_UNKNOWN;
        }

        display_index = m_implUmc->GetDisplayIndex();

        if (0 <= display_index)
        {
            if (false == m_isDecodedOrder && ((true == IsField && !(dec_field_count & 1)) || false == IsField))
            {
                mfxStatus sts = GetOutputSurface(surface_disp, surface_work, mid[display_index]);
                if (sts < MFX_ERR_NONE)
                    return sts;

                pEntryPoint->requiredNumThreads = m_NumThreads;
                pEntryPoint->pRoutine = &MPEG2TaskRoutine;
                pEntryPoint->pState = (void*)this;
                pEntryPoint->pRoutineName = (char *)"DecodeMPEG2";

                SetOutputSurfaceParams(*surface_disp, display_index);

                memset(&m_task_param[m_task_num],0,sizeof(MParam));
                m_implUmc->LockTask(m_task_num);

                m_task_param[m_task_num].NumThreads = m_implUmc->GetCurrThreadsNum(m_task_num);
                m_task_param[m_task_num].m_isDecodedOrder = m_isDecodedOrder;
                m_task_param[m_task_num].m_frame_curr = m_frame_curr;
                m_task_param[m_task_num].curr_index = curr_index;
                m_task_param[m_task_num].prev_index = prev_index;
                m_task_param[m_task_num].next_index = next_index;
                m_task_param[m_task_num].surface_out = GetOriginalSurface(*surface_disp);
                m_task_param[m_task_num].surface_work = surface_work;
                m_task_param[m_task_num].display_index = display_index;
                m_task_param[m_task_num].m_FrameAllocator = m_FrameAllocator;
                m_task_param[m_task_num].mid = mid;
                m_task_param[m_task_num].in = &m_in[m_task_num];
                m_task_param[m_task_num].m_frame = m_frame;
                m_task_param[m_task_num].m_frame_in_use = m_frame_in_use;
                m_task_param[m_task_num].task_num = m_task_num;
                m_task_param[m_task_num].m_isSoftwareBuffer = m_isSWBuf;

                pEntryPoint->pParam = (void *)(&(m_task_param[m_task_num]));

                m_prev_task_num = m_task_num;

                m_implUmc->PostProcessUserData(display_index);

                display_frame_count++;
                return sts;
            }
            else
            {
                mfxFrameSurface1 *pSurface;
                pSurface = m_FrameAllocator->GetSurface(mid[display_index], surface_work, &m_vPar);

                if (NULL == pSurface)
                {
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
                }

                pSurface->Data.FrameOrder = display_order;
                pSurface->Data.TimeStamp = last_timestamp;

                return MFX_ERR_MORE_DATA;
            }
        }

        return MFX_ERR_MORE_DATA;
    }
}

mfxStatus VideoDECODEMPEG2Internal_HW::TaskRoutine(void *pParam)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoDECODEMPEG2Internal_HW::TaskRoutine");
    MParam *parameters = (MParam *)pParam;

    mfxStatus sts = PerformStatusCheck(pParam);
    MFX_CHECK_STS(sts);
     
    if (0 <= parameters->display_index && true == parameters->m_isSoftwareBuffer)
    {
        sts = parameters->m_FrameAllocator->PrepareToOutput(parameters->surface_out,
                                                            parameters->mid[parameters->display_index],
                                                            &parameters->m_vPar,
                                                            m_isOpaqueMemory);

        MFX_CHECK_STS(sts);
    }

    sts = CompleteTasks(pParam);
    MFX_CHECK_STS(sts);

    return MFX_TASK_DONE;
}

mfxStatus VideoDECODEMPEG2Internal_HW::CompleteTasks(void *pParam)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoDECODEMPEG2Internal_HW::CompleteTasks");
    MParam *parameters = (MParam *)pParam;

    UMC::AutomaticUMCMutex guard(m_guard);

    THREAD_DEBUG_PRINTF(
        "(THREAD %x) CompleteTasks: task %x number, task num %d, curr thr idx %d, compl thr %d\n",
        GetCurrentThreadId(), pParam, parameters->task_num, parameters->m_curr_thread_idx, parameters->m_thread_completed)

    Ipp32s display_index = parameters->display_index;

    if (0 <= display_index)
    {
        THREAD_DEBUG_PRINTF("(THREAD %x) Dumping\n", GetCurrentThreadId())
        STATUS_REPORT_DEBUG_PRINTF("thread task idx %d, display address %x\n", parameters->task_num, parameters->surface_out)

        if (B_PICTURE == m_implUmc->GetFrameType(display_index))
        {
            parameters->m_FrameAllocator->DecreaseReference(parameters->mid[display_index]);
            parameters->mid[display_index] = -1;

            m_implUmc->UnLockTask(display_index);
        }
        else // I or P
        {
            Ipp32s p_index = m_implUmc->GetPrevDecodingIndex(display_index);

            if (true == m_isDecodedOrder && 0 <= p_index)
            {
                    p_index = m_implUmc->GetPrevDecodingIndex(p_index);

                if (0 <= p_index)
                {
                    parameters->m_FrameAllocator->DecreaseReference(parameters->mid[p_index]);
                    parameters->mid[p_index] = -1;

                    m_implUmc->UnLockTask(p_index);
                }
            }
            else if (0 <= p_index)
            {
                parameters->m_FrameAllocator->DecreaseReference(parameters->mid[p_index]);
                parameters->mid[p_index] = -1;

                m_implUmc->UnLockTask(p_index);
            }
        }
    }

    parameters->m_frame_in_use[parameters->m_frame_curr] = false;
    STATUS_REPORT_DEBUG_PRINTF("m_frame_curr %d is %d\n", parameters->m_frame_curr, parameters->m_frame_in_use[parameters->m_frame_curr])

    return MFX_TASK_DONE;
}

mfxStatus VideoDECODEMPEG2Internal_HW::GetStatusReportByIndex(mfxFrameSurface1 *displaySurface, mfxU32 currIdx)
{
    displaySurface; currIdx;

#ifdef MFX_VA_WIN

    UMC::Status sts = UMC::UMC_OK;

    DXVA_Status_VC1 currentTaskStatus = {};
    bool isStatusExist = false;
    
    std::list<DXVA_Status_VC1>::iterator iterator;

    STATUS_REPORT_DEBUG_PRINTF("Queried task index %d \n", currIdx)

    // check if status is already cached
    if (0 != m_pStatusList.size())
    {
        for (iterator = m_pStatusList.begin(); iterator != m_pStatusList.end(); iterator++)
        {
            if (currIdx == (*iterator).wDecodedPictureIndex)
            {
                isStatusExist = true;

                currentTaskStatus = (*iterator);
                m_pStatusList.erase(iterator);

                break;
            }
        }
    }

    if (false == isStatusExist)
    {
        //TDR handling.
        for(int i=0; i< MPEG2_STATUS_REPORT_NUM;i++){
            m_pStatusReport[i].bStatus = 3;
        }
        // ask driver to provide tasks status
        sts = m_implUmcHW->pack_w.GetStatusReport(m_pStatusReport);
        MFX_CHECK_STS((mfxStatus)sts);

        // cache statutes into buffer
        for (mfxU32 i = 0; i < MPEG2_STATUS_REPORT_NUM; i += 1)
        {
            if (0 == m_pStatusReport[i].StatusReportFeedbackNumber)
            {
                break;
            }

            m_pStatusList.push_front(m_pStatusReport[i]);
        }

        // clear temp buffer
        memset(m_pStatusReport, 0, sizeof(DXVA_Status_VC1) * MPEG2_STATUS_REPORT_NUM);

        // find status
        for (iterator = m_pStatusList.begin(); iterator != m_pStatusList.end(); iterator++)
        {
            if (currIdx == (*iterator).wDecodedPictureIndex)
            {
                currentTaskStatus = (*iterator);
                break;
            }
        }

        m_pStatusList.erase(iterator);
    }

    switch (currentTaskStatus.bStatus)
    {
        case STATUS_REPORT_MINOR_PROBLEM:
        case STATUS_REPORT_OPERATION_SUCCEEDED:
            break;

        case STATUS_REPORT_SIGNIFICANT_PROBLEM:
            displaySurface->Data.Corrupted = 1;
            break;

        case STATUS_REPORT_SEVERE_PROBLEM:
        case STATUS_REPORT_SEVERE_PROBLEM_OTHER:
            return MFX_ERR_DEVICE_FAILED;

        default:
            break;
    }

#endif

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2Internal_HW::GetStatusReport(mfxFrameSurface1 *displaySurface, UMC::FrameMemID surface_id)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoDECODEMPEG2Internal_HW::GetStatusReport");
    displaySurface;surface_id;
#ifdef MFX_VA_WIN
    UMC::Status sts = UMC::UMC_OK;

    DXVA_Status_VC1 currentTaskStatus = {};

    STATUS_REPORT_DEBUG_PRINTF("status report count %d\n", m_pStatusList.size())

    // check if status is already cached
    if (0 != m_pStatusList.size())
    {
        currentTaskStatus = m_pStatusList.front(); 
        m_pStatusList.pop_front();
    }
    else
    {
        //TDR handling.
        for(int i=0; i< MPEG2_STATUS_REPORT_NUM;i++){
            m_pStatusReport[i].bStatus = 3;
        }
        // ask driver to provide tasks status
        sts = m_implUmcHW->pack_w.GetStatusReport(m_pStatusReport);

        MFX_CHECK_STS((mfxStatus)sts);

        // cache statutes into buffer
        for (mfxU32 i = 0; i < 8; i += 1)
        {
            if (0 == m_pStatusReport[i].StatusReportFeedbackNumber)
            {
                break;
            }

            m_pStatusList.push_front(m_pStatusReport[i]);

            STATUS_REPORT_DEBUG_PRINTF("Got status report %d\n", m_pStatusReport[i].wDecodedPictureIndex)
        }

        // clear temp buffer
        memset(m_pStatusReport, 0, sizeof(DXVA_Status_VC1) * MPEG2_STATUS_REPORT_NUM);

        if (0 == m_pStatusList.size())
        {
            // something wrong, but who is care
            return MFX_ERR_NONE;
        }

        // get status report
        currentTaskStatus = m_pStatusList.front();
        m_pStatusList.pop_front();
    }

    STATUS_REPORT_DEBUG_PRINTF("wDecodedPictureIndex %d is ready\n", currentTaskStatus.wDecodedPictureIndex)

    switch (currentTaskStatus.bStatus)
    {
        case STATUS_REPORT_OPERATION_SUCCEEDED:
            break;
        
        case STATUS_REPORT_MINOR_PROBLEM:
        case STATUS_REPORT_SIGNIFICANT_PROBLEM:
            displaySurface->Data.Corrupted = 1;
            break;

        case STATUS_REPORT_SEVERE_PROBLEM:
        case STATUS_REPORT_SEVERE_PROBLEM_OTHER:
            displaySurface->Data.Corrupted = 2;
            return MFX_ERR_DEVICE_FAILED;

        default:
            break;
    }

#endif

#ifdef UMC_VA_LINUX
    UMC::VideoAccelerator *va;
    m_pCore->GetVA((mfxHDL*)&va, MFX_MEMTYPE_FROM_DECODE);

    UMC::Status sts = UMC::UMC_OK;
    mfxU16 surfCorruption = 0;

#if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)

    sts = va->SyncTask(surface_id, &surfCorruption);

    STATUS_REPORT_DEBUG_PRINTF("index %d with corruption: %d (sts:%d)\n", surface_id, surfCorruption, sts)

    if (sts != UMC::UMC_OK)
    {
        mfxStatus CriticalErrorStatus = (sts == UMC::UMC_ERR_GPU_HANG) ? MFX_ERR_GPU_HANG : MFX_ERR_DEVICE_FAILED;
        SetCriticalErrorOccured(CriticalErrorStatus);
        return CriticalErrorStatus;
    }

#else
    VASurfaceStatus surfSts = VASurfaceSkipped;

    sts = va->QueryTaskStatus(surface_id, &surfSts, &surfCorruption);

    if (sts == UMC::UMC_ERR_GPU_HANG)
        return MFX_ERR_GPU_HANG;

    if (sts != UMC::UMC_OK)
        return MFX_ERR_DEVICE_FAILED;

#endif // #if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)

    displaySurface->Data.Corrupted = surfCorruption;

#endif // UMC_VA_LINUX

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2Internal_HW::PerformStatusCheck(void *pParam)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoDECODEMPEG2Internal_HW::PerformStatusCheck");
    MFX_CHECK_NULL_PTR1(pParam);
    MParam *parameters = (MParam *)pParam;
    Ipp32s display_index = parameters->display_index;
    if (display_index < 0)
        return MFX_ERR_NONE;

    if (!IsStatusReportEnable(m_pCore))
        return MFX_ERR_NONE;

    mfxStatus sts = GetStatusReport(parameters->surface_out, parameters->mid[display_index]);

    if (MFX_ERR_NONE != sts)
    {
        parameters->m_FrameAllocator->DecreaseReference(parameters->mid[display_index]);
        parameters->m_frame_in_use[parameters->m_frame_curr] = false;
        return sts;
    }

    mfxU16 isCorrupted = parameters->surface_out->Data.Corrupted;
    mfxU32 frameType = m_implUmc->GetFrameType(display_index);
    Ipp32s prev_index = m_implUmc->GetPrevDecodingIndex(display_index);

    if (isCorrupted && I_PICTURE == frameType)
        m_implUmc->SetCorruptionFlag(display_index);

    switch (frameType)
    {
        case I_PICTURE:

            if (isCorrupted)
            {
                parameters->surface_out->Data.Corrupted = MFX_CORRUPTION_MAJOR;
            }

            break;

        default: // P_PICTURE

            if (true == m_implUmc->GetCorruptionFlag(prev_index))
            {
                m_implUmc->SetCorruptionFlag(display_index);
                parameters->surface_out->Data.Corrupted = MFX_CORRUPTION_REFERENCE_FRAME;
            }
    }

    return MFX_ERR_NONE;
}
#endif // #if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)

#if !defined(MFX_ENABLE_HW_ONLY_MPEG2_DECODER) || !defined (MFX_VA)
static const mfxI32 MFX_MPEG2_DECODE_ALIGNMENT = 16;

VideoDECODEMPEG2Internal_SW::VideoDECODEMPEG2Internal_SW()
{
    m_implUmcSW = new UMC::MPEG2VideoDecoderSW();
    m_implUmc.reset(m_implUmcSW);
}

mfxStatus VideoDECODEMPEG2Internal_SW::Init(mfxVideoParam *par, VideoCORE * core)
{
    mfxStatus sts = VideoDECODEMPEG2InternalBase::Init(par, core);
    MFX_CHECK_STS(sts);

    m_out.Close();
    m_out.SetAlignment(MFX_MPEG2_DECODE_ALIGNMENT);
    UMC::Status umcRes = m_out.Init(m_vdPar.info.clip_info.width, m_vdPar.info.clip_info.height, m_vdPar.info.color_format);
    MFX_CHECK_UMC_STS(umcRes);

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2Internal_SW::Close()
{
    m_implUmc->Close();

    for(int i = 0; i < DPB; i++)
        if(mid[i] >= 0) m_FrameAllocator->Unlock(mid[i]);

    m_out.Close();
    return VideoDECODEMPEG2InternalBase::Close();
}

mfxStatus VideoDECODEMPEG2Internal_SW::Reset(mfxVideoParam *par)
{
    for (mfxU32 i = 0; i < DPB; i++)
    {
        if(mid[i] >= 0) m_FrameAllocator->Unlock(mid[i]);
    }

    m_NumThreads = (mfxU16)(par->AsyncDepth ? par->AsyncDepth : (mfxU16)vm_sys_info_get_cpu_num());

    return VideoDECODEMPEG2InternalBase::Reset(par);
}

mfxStatus VideoDECODEMPEG2Internal_SW::DecodeFrameCheck(mfxBitstream *bs,
                               mfxFrameSurface1 *surface_work,
                               mfxFrameSurface1 **surface_disp,
                               MFX_ENTRY_POINT *pEntryPoint)
{
    int display_index = -1;

    m_in[m_task_num].SetBufferPointer(m_frame[m_frame_curr].Data + m_frame[m_frame_curr].DataOffset, m_frame[m_frame_curr].DataLength);
    m_in[m_task_num].SetDataSize(m_frame[m_frame_curr].DataLength);
    m_in[m_task_num].SetTime(m_time[m_frame_curr]);

    UMC::Status res = UMC::UMC_OK;
    bool isField = false;
    bool isSkipped = false;

    if (8 < m_frame[m_frame_curr].DataLength)
    {
        res = m_implUmc->GetPictureHeader(&m_in[m_task_num], m_task_num, m_prev_task_num);

        if (UMC::UMC_ERR_INVALID_STREAM == res)
        {
            //return MFX_ERR_UNKNOWN;
            //m_implUmc.SetCorruptionFlag(m_task_num);
            res = 0;
        }

        if (UMC::UMC_OK != res)
        {
            if (false == m_reset_done)
            {
                m_frame[m_frame_curr].DataLength = 0;
                m_frame[m_frame_curr].DataOffset = 0;
                m_frame_in_use[m_frame_curr] = false;

                isSkipped = m_implUmc->IsFrameSkipped();

                if (true == isSkipped && !(dec_field_count & 1))
                {
                    skipped_frame_count++;
                }

                if (NULL == bs)
                {
                    display_index = m_implUmc->GetDisplayIndex();
                        
                    if (0 > display_index)
                    {
                        res = m_implUmc->GetPictureHeader(NULL,m_task_num,m_prev_task_num);
                            
                        if (UMC::UMC_ERR_INVALID_STREAM == res)
                        {
                            return MFX_ERR_UNKNOWN;
                        }

                        display_index = m_implUmc->GetDisplayIndex();
                    }
                    if (0 <= display_index)
                    {
                        *surface_disp = m_FrameAllocator->GetSurface(mid[display_index], surface_work, &m_vPar);
                            
                        if (true == m_isDecodedOrder)
                        {
                            (*surface_disp)->Data.FrameOrder = 0xffffffff;
                        }

                        mfxStatus sts = m_FrameAllocator->PrepareToOutput(surface_work, mid[display_index], &m_vPar, m_isOpaqueMemory);
                        MFX_CHECK_STS(sts);

                        (*surface_disp)->Data.TimeStamp = GetMfxTimeStamp(m_implUmc->GetCurrDecodedTime(display_index));
                        (*surface_disp)->Data.DataFlag = (mfxU16)((m_implUmc->isOriginalTimeStamp(display_index)) ? MFX_FRAMEDATA_ORIGINAL_TIMESTAMP : 0);

                        if (true == m_isDecodedOrder)
                        {
                            (*surface_disp)->Data.FrameOrder = display_order;
                            display_order++;
                        }
                        else
                        {
                            (*surface_disp)->Data.FrameOrder = display_frame_count;
                        }

                        display_frame_count++;

                        m_FrameAllocator->Unlock(mid[display_index]);
                        m_FrameAllocator->DecreaseReference(mid[display_index]);
                        mid[display_index] = -1;

                        {
                            UMC::AutomaticUMCMutex guard(m_guard);
                            m_implUmc->UnLockTask(display_index);
                        }

                        pEntryPoint->requiredNumThreads = m_NumThreads;
                        pEntryPoint->pRoutine = &MPEG2TaskRoutine;
                        pEntryPoint->pState = (void*)this;
                        pEntryPoint->pParam = (void *)(&(m_task_param[m_task_num]));
                        pEntryPoint->pCompleteProc = &MPEG2CompleteTasks;
                        pEntryPoint->pRoutineName = (char *)"DecodeMPEG2";

                        m_implUmc->PostProcessUserData(display_index);

                        return MFX_ERR_NONE;
                    }
                }

                return MFX_ERR_MORE_DATA;
            }
            else
            {
                m_frame[m_frame_curr].DataLength = 0;
                m_frame[m_frame_curr].DataOffset = 0;
                m_frame_in_use[m_frame_curr] = false;

                return MFX_ERR_MORE_SURFACE;
            }
        }

        m_reset_done = false;

        res = m_implUmc->GetInfo(&m_vdPar);

        mfxStatus sts = UpdateCurrVideoParams(surface_work, m_task_num);

        isField = !m_implUmc->IsFramePictureStructure(m_task_num);

        if (m_task_num > DPB && !isField)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        if ((false == isField || (true == isField && !(dec_field_count & 1))))
        {
            curr_index = m_task_num;
            next_index = m_implUmc->GetNextDecodingIndex(curr_index);
            prev_index = m_implUmc->GetPrevDecodingIndex(curr_index);

            UMC::VideoDataInfo Info;
            mfxFrameSurface1 *pSurface = surface_work;

            if (1 == m_implUmc->GetSequenceHeader().progressive_sequence)
            {
                Info.Init(pSurface->Info.Width,
                            (pSurface->Info.CropH + 15) & ~0x0f,
                            m_vdPar.info.color_format);
            }
            else
            {
                Info.Init(pSurface->Info.Width,
                            pSurface->Info.Height,
                            m_vdPar.info.color_format);
            }

            m_FrameAllocator->Alloc(&mid[curr_index], &Info, 0);

            if (0 > mid[curr_index])
            {
                return MFX_ERR_LOCK_MEMORY;
            }

            m_FrameData[curr_index] = (UMC::FrameData *)m_FrameAllocator->Lock(mid[curr_index]);

            if (NULL == m_FrameData[curr_index])
            {
                return MFX_ERR_LOCK_MEMORY;
            }

            m_FrameAllocator->IncreaseReference(mid[curr_index]);

            m_out.Close();
            m_out.SetAlignment(MFX_MPEG2_DECODE_ALIGNMENT);

            res = m_out.Init(pSurface->Info.Width, pSurface->Info.Height, m_vdPar.info.color_format);
            MFX_CHECK_UMC_STS(res);

            const UMC::FrameData::PlaneMemoryInfo *pInfo;

            pInfo = m_FrameData[curr_index]->GetPlaneMemoryInfo(0);

            m_out.SetPlanePointer(pInfo->m_planePtr, 0);
            Ipp32s pitch = (Ipp32s)pInfo->m_pitch;
                
            pInfo = m_FrameData[curr_index]->GetPlaneMemoryInfo(1);
            m_out.SetPlanePointer(pInfo->m_planePtr, 1);
                
            pInfo = m_FrameData[curr_index]->GetPlaneMemoryInfo(2);
                
            m_out.SetPlanePointer(pInfo->m_planePtr, 2);

            m_out.SetPlanePitch(pitch, 0);
            m_out.SetPlanePitch(pitch / 2, 1);
            m_out.SetPlanePitch(pitch / 2, 2);
        }

        m_implUmcSW->SetOutputPointers(&m_out, m_task_num);

        display_index = m_implUmc->GetDisplayIndex();

        UMC::Status umcSts = m_implUmc->ProcessRestFrame(m_task_num);

        if (umcSts == UMC::UMC_OK && display_index >= 0)
        {
            last_timestamp = GetMfxTimeStamp(m_implUmc->GetCurrDecodedTime(display_index));
        }

        if (true == isField)
        {
            dec_field_count++;
        }
        else
        {
            dec_field_count += 2;
            cashed_frame_count++;
        }

        dec_frame_count++;

        if (0 <= display_index)
        {
            mfxStatus sts = GetOutputSurface(surface_disp, surface_work, mid[display_index]);
            if (sts < MFX_ERR_NONE)
                return sts;

            if (true == m_isDecodedOrder)
            {
                (*surface_disp)->Data.FrameOrder = 0xffffffff;

                if (m_implUmc->GetFrameType(display_index) == B_PICTURE)
                {
                    (*surface_disp)->Data.FrameOrder = display_order;

                    display_order++;
                }
                else //I or P
                {
                    Ipp32s p_index = m_implUmc->GetPrevDecodingIndex(display_index);
                        
                    if (0 <= p_index)
                    {
                        mfxFrameSurface1 *pSurface;

                        pSurface = m_FrameAllocator->GetSurface(mid[p_index], surface_work, &m_vPar);

                        if (NULL == pSurface)
                        {
                            return MFX_ERR_UNDEFINED_BEHAVIOR;
                        }

                        pSurface->Data.FrameOrder = display_order;
                        display_order++;
                    }
                }
            }
            else
            {
                if (!*surface_disp)
                {
                    return MFX_ERR_UNKNOWN;
                }

                (*surface_disp)->Data.FrameOrder = display_frame_count;
            }

            display_frame_count++;
        }

        memset(&m_task_param[m_task_num],0,sizeof(MParam));

        pEntryPoint->requiredNumThreads = m_NumThreads;
        pEntryPoint->pRoutine = &MPEG2TaskRoutine;
        pEntryPoint->pCompleteProc = &MPEG2CompleteTasks;
        pEntryPoint->pState = (void*)this;
        pEntryPoint->pRoutineName = (char *)"DecodeMPEG2";

        m_task_param[m_task_num].m_thread_completed = 0;

        m_task_param[m_task_num].IsSWImpl = true;

        m_task_param[m_task_num].NumThreads = m_implUmc->GetCurrThreadsNum(m_task_num);
        m_task_param[m_task_num].m_isDecodedOrder = m_isDecodedOrder;
        m_task_param[m_task_num].m_frame_curr = m_frame_curr;
        m_task_param[m_task_num].last_timestamp = last_timestamp;
        m_task_param[m_task_num].curr_index = curr_index;
        m_task_param[m_task_num].prev_index = prev_index;
        m_task_param[m_task_num].next_index = next_index;
        m_task_param[m_task_num].last_frame_count = last_frame_count;
        m_task_param[m_task_num].surface_out = GetOriginalSurface(*surface_disp);
        m_task_param[m_task_num].surface_work = surface_work;
        m_task_param[m_task_num].display_index = display_index;
        m_task_param[m_task_num].m_FrameAllocator = m_FrameAllocator;
        m_task_param[m_task_num].mid = mid;
        m_task_param[m_task_num].in = &m_in[m_task_num];
        m_task_param[m_task_num].m_frame = m_frame;
        m_task_param[m_task_num].m_frame_in_use = m_frame_in_use;
        m_task_param[m_task_num].task_num = m_task_num;

        memcpy_s(&(m_task_param[m_task_num].m_vPar.mfx.FrameInfo),sizeof(mfxFrameInfo),&m_vPar.mfx.FrameInfo,sizeof(mfxFrameInfo));

        THREAD_DEBUG_PRINTF__HOLDING_MUTEX(m_guard, "task_num %d was added\n", m_task_param[m_task_num].task_num)

        pEntryPoint->pParam = (void *)(&(m_task_param[m_task_num]));

        if (NULL != *surface_disp)
        {
            (*surface_disp)->Data.TimeStamp = last_timestamp;
            (*surface_disp)->Data.DataFlag = (mfxU16)((m_implUmc->isOriginalTimeStamp(display_index)) ? MFX_FRAMEDATA_ORIGINAL_TIMESTAMP : 0);
        }

        m_implUmc->LockTask(m_task_num);

        m_prev_task_num = m_task_num;

        if (true == isField && (dec_field_count & 1))
        {
            if(m_task_num >= DPB)
            {
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
                
            m_task_num += DPB;
        }

        if (0 <= display_index)
        {
            m_implUmc->PostProcessUserData(display_index);

            return sts;
        }

        if (true == m_isDecodedOrder)
        {
            return MFX_ERR_MORE_DATA;
        }

        return MFX_ERR_MORE_SURFACE;
    }
    else
    {
        last_frame_count++;

        res = m_implUmc->GetPictureHeader(NULL, m_task_num, m_prev_task_num);

        if (UMC::UMC_ERR_INVALID_STREAM == res)
        {
            return MFX_ERR_UNKNOWN;
        }

        display_index = m_implUmc->GetDisplayIndex();
            
        if (0 <= display_index)
        {
            if (false == m_isDecodedOrder)
            {
                last_timestamp = GetMfxTimeStamp(m_implUmc->GetCurrDecodedTime(display_index));
                mfxStatus sts = GetOutputSurface(surface_disp, surface_work, mid[display_index]);
                if (sts < MFX_ERR_NONE)
                    return sts;

                (*surface_disp)->Data.TimeStamp = last_timestamp;
                memset(&m_task_param[m_task_num],0,sizeof(MParam));

                pEntryPoint->requiredNumThreads = m_NumThreads;
                pEntryPoint->pRoutine = &MPEG2TaskRoutine;
                pEntryPoint->pCompleteProc = &MPEG2CompleteTasks;
                pEntryPoint->pState = (void*)this;
                pEntryPoint->pRoutineName = (char *)"DecodeMPEG2";

                m_task_param[m_task_num].m_thread_completed = 0;
                m_task_param[m_task_num].IsSWImpl = true;
                m_task_param[m_task_num].NumThreads = m_implUmc->GetCurrThreadsNum(m_task_num);

                m_task_param[m_task_num].m_isDecodedOrder = m_isDecodedOrder;
                m_task_param[m_task_num].m_frame_curr = m_frame_curr;
                m_task_param[m_task_num].last_timestamp = last_timestamp;
                m_task_param[m_task_num].curr_index = curr_index;
                m_task_param[m_task_num].prev_index = prev_index;
                m_task_param[m_task_num].next_index = next_index;
                m_task_param[m_task_num].last_frame_count = last_frame_count;
                m_task_param[m_task_num].surface_out = GetOriginalSurface(*surface_disp);
                m_task_param[m_task_num].surface_work = surface_work;
                m_task_param[m_task_num].display_index = display_index;
                m_task_param[m_task_num].m_FrameAllocator = m_FrameAllocator;
                m_task_param[m_task_num].mid = mid;
                m_task_param[m_task_num].in = NULL;
                m_task_param[m_task_num].m_frame = m_frame;
                m_task_param[m_task_num].m_frame_in_use = m_frame_in_use;
                m_task_param[m_task_num].task_num = m_task_num;

                memcpy_s(&(m_task_param[m_task_num].m_vPar.mfx.FrameInfo), sizeof(mfxFrameInfo), &m_vPar.mfx.FrameInfo, sizeof(mfxFrameInfo));

                pEntryPoint->pParam = (void *)(&(m_task_param[m_task_num]));

                m_prev_task_num = m_task_num;

                m_implUmc->PostProcessUserData(display_index);

                (*surface_disp)->Data.TimeStamp = last_timestamp;

                return MFX_ERR_NONE;
            }
            else
            {
                mfxFrameSurface1 *pSurface;
                pSurface = m_FrameAllocator->GetSurface(mid[display_index], surface_work, &m_vPar);
                    
                if (NULL == pSurface)
                {
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
                }

                pSurface->Data.FrameOrder = display_order;
                pSurface->Data.TimeStamp = last_timestamp;

                return MFX_ERR_MORE_DATA;
            }
        }

        return MFX_ERR_MORE_DATA;
    }
}

mfxStatus VideoDECODEMPEG2Internal_SW::CompleteTasks(void *pParam)
{
    MParam *parameters = (MParam *)pParam;

    UMC::AutomaticUMCMutex guard(m_guard);

    THREAD_DEBUG_PRINTF(
        "(THREAD %x) CompleteTasks: task %x number, task num %d, curr thr idx %d, compl thr %d\n",
        GetCurrentThreadId(), pParam, parameters->task_num, parameters->m_curr_thread_idx, parameters->m_thread_completed)

    Ipp32s display_index = parameters->display_index;

    if (0 <= display_index)
    {
        THREAD_DEBUG_PRINTF("(THREAD %x) Dumping\n", GetCurrentThreadId())

        if (B_PICTURE == m_implUmc->GetFrameType(display_index))
        {
            parameters->m_FrameAllocator->Unlock(parameters->mid[display_index]);
            parameters->m_FrameAllocator->DecreaseReference(parameters->mid[display_index]);
            parameters->mid[display_index] = -1;

            m_implUmc->UnLockTask(display_index);
        }
        else // I or P picture
        {
            bool isCorrupted = m_implUmc->GetCorruptionFlag(display_index);

            mfxU32 frameType = m_implUmc->GetFrameType(display_index);
            Ipp32s prev_index = m_implUmc->GetPrevDecodingIndex(display_index);
                
            parameters->surface_out->Data.Corrupted = 0;
                
            switch (frameType)
            {
                case I_PICTURE:

                    if (isCorrupted)
                    {
                        parameters->surface_out->Data.Corrupted = MFX_CORRUPTION_MAJOR;
                    }

                    break;
                    
                default: // P_PICTURE

                    if (P_PICTURE == m_implUmc->GetFrameType(prev_index))
                    {
                        //prev_index = m_implUmc.GetPrevDecodingIndex(prev_index);
                    }

                    if (true == m_implUmc->GetCorruptionFlag(prev_index))
                    {
                        m_implUmc->SetCorruptionFlag(display_index);
                        parameters->surface_out->Data.Corrupted = MFX_CORRUPTION_REFERENCE_FRAME;
                    }
            }

            if (0 <= prev_index && true == parameters->m_isDecodedOrder)
            {
                prev_index = m_implUmc->GetPrevDecodingIndex(prev_index);
            }

            if (0 <= prev_index)
            {
                parameters->m_FrameAllocator->Unlock(parameters->mid[prev_index]);
                parameters->m_FrameAllocator->DecreaseReference(parameters->mid[prev_index]);
                parameters->mid[prev_index] = -1;

                m_implUmc->UnLockTask(prev_index);
            }
        }
    }

    parameters->m_frame_in_use[parameters->m_frame_curr] = false;
    STATUS_REPORT_DEBUG_PRINTF("m_frame_curr %d is %d\n", parameters->m_frame_curr, parameters->m_frame_in_use[parameters->m_frame_curr])

    return MFX_TASK_DONE;
}

mfxStatus VideoDECODEMPEG2Internal_SW::TaskRoutine(void *pParam)
{
    MParam *parameters = (MParam *)pParam;

    UMC::Status res;
    Ipp32s th_idx;

    UMC::AutomaticUMCMutex guard(m_guard);

    THREAD_DEBUG_PRINTF(
        "(THREAD %x) task %x number, task num %d, curr thr idx %d, compl thr %d\n",
        GetCurrentThreadId(), pParam, parameters->task_num, parameters->m_curr_thread_idx, parameters->m_thread_completed)

    if (0 < parameters->last_frame_count && 0 == parameters->m_thread_completed)
    {

        Ipp32s display_index = parameters->display_index;

        if (0 <= display_index)
        {
            m_implUmc->PostProcessUserData(display_index);

            mfxStatus sts = parameters->m_FrameAllocator->PrepareToOutput(parameters->surface_work,parameters->mid[display_index],&parameters->m_vPar, m_isOpaqueMemory);

            if (MFX_ERR_NONE > sts)
            {
                return sts;
            }
        }

        parameters->m_thread_completed = parameters->NumThreads + 1;

        THREAD_DEBUG_PRINTF("(THREAD %x): 0 < parameters->last_frame_count && 0 == parameters->m_thread_completed\n", GetCurrentThreadId())

        return MFX_TASK_DONE;
    }

    parameters->m_curr_thread_idx++;

    if (parameters->m_curr_thread_idx > parameters->NumThreads || parameters->m_thread_completed >= parameters->NumThreads)
    {
        /*THREAD_DEBUG_PRINTF(
            "(THREAD %x): (parameters->m_curr_thread_idx || parameters->m_thread_completed) > parameters->NumThreads: %d || %d > %d\n",
            GetCurrentThreadId(), parameters->m_curr_thread_idx, parameters->m_thread_completed, parameters->NumThreads)*/
        THREAD_DEBUG_PRINTF("(THREAD %x): was waiting but done\n", GetCurrentThreadId())

        return MFX_TASK_DONE;
    }

    th_idx = parameters->m_curr_thread_idx - 1;

    THREAD_DEBUG_PRINTF("(THREAD %x): start decoding\n", GetCurrentThreadId())

    // down mutex
    guard.Unlock();

    // decode slices
    res = m_implUmc->DecodeSlices(th_idx, parameters->task_num);

    // up mutex
    guard.Lock();

    THREAD_DEBUG_PRINTF("(THREAD %x): finish decoding\n", GetCurrentThreadId())

    parameters->m_thread_completed++;

    if (UMC::UMC_OK != res && UMC::UMC_ERR_NOT_ENOUGH_DATA != res && UMC::UMC_ERR_SYNC != res)
    {
        m_implUmc->SetCorruptionFlag(parameters->curr_index);
    }

    if (parameters->m_thread_completed == parameters->NumThreads)
    {
        Ipp32s display_index = parameters->display_index;

        parameters->m_frame[parameters->m_frame_curr].DataLength = 0;
        parameters->m_frame[parameters->m_frame_curr].DataOffset = 0;

        if (0 <= display_index)
        {
            THREAD_DEBUG_PRINTF("(THREAD %x): dump frame\n", GetCurrentThreadId())

            mfxStatus sts = parameters->m_FrameAllocator->PrepareToOutput(parameters->surface_out,
                                                                          parameters->mid[display_index],
                                                                          &parameters->m_vPar,
                                                                          m_isOpaqueMemory);

            if (MFX_ERR_NONE > sts)
            {
                m_implUmc->UnLockTask(parameters->task_num);
                return sts;
            }
        }
    }
    else
    {
        THREAD_DEBUG_PRINTF("(THREAD %x): i'm working status\n", GetCurrentThreadId())
        return MFX_TASK_WORKING;
    }

    THREAD_DEBUG_PRINTF("(THREAD %x): exit routine\n\n", GetCurrentThreadId())

    return MFX_TASK_DONE;
}
#endif

#endif //MFX_ENABLE_MPEG2_VIDEO_DECODE
