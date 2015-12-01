/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

File Name: mfx_mpeg2_decode.cpp

\* ****************************************************************************** */

#include "mfx_common.h"
#ifdef MFX_ENABLE_MPEG2_VIDEO_DECODE

static const mfxI32 MFX_MPEG2_DECODE_ALIGNMENT = 16;

#include "mfx_common_decode_int.h"
#include "mfx_mpeg2_decode.h"
#include "mfx_mpeg2_dec_common.h"
#include "mfx_enc_common.h"

#include "vm_sys_info.h"

#if defined (MFX_VA_WIN)
#include "umc_va_dxva2_protected.h"
#include "umc_va_dxva2.h"
#endif

//#if defined (MFX_VA_LINUX)
//#include "umc_va_base.h"
//#endif

#undef ELK_WORKAROUND

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

void UpdateMfxFrameParam(mfxFrameParam& fPar, const UMC::sSequenceHeader& sh, const UMC::sPictureHeader& ph)
{
    fPar.MPEG2.FrameType = GetMfxFrameType((mfxU8)ph.picture_coding_type, ph.picture_structure);

    /* next code is incorrect and does not make sense
    fPar.MPEG2.FrameWinMbMinus1 = (mfxU16)(sh.mb_width - 1); // should be (mfxU16)(*sh.mb_width - 1)
    fPar.MPEG2.FrameHinMbMinus1 = (mfxU16)(sh.mb_height - 1); // should be (mfxU16)(*sh.mb_height - 1)
    fPar.MPEG2.NumMb = (fPar.MPEG2.FrameWinMbMinus1 + 1) * (fPar.MPEG2.FrameHinMbMinus1 + 1);
    */

    fPar.MPEG2.SecondFieldFlag = 0;
    fPar.MPEG2.FieldPicFlag = (ph.picture_structure != FRAME_PICTURE);
    fPar.MPEG2.InterlacedFrameFlag = 0;
    fPar.MPEG2.BottomFieldFlag = (ph.picture_structure == BOTTOM_FIELD);
    fPar.MPEG2.ChromaFormatIdc = sh.chroma_format;
    fPar.MPEG2.RefPicFlag = (ph.picture_coding_type != (UMC::MPEG2FrameType)UMC::B_PICTURE);
    fPar.MPEG2.BackwardPredFlag = (ph.picture_coding_type == (UMC::MPEG2FrameType)UMC::B_PICTURE);
    fPar.MPEG2.NoResidDiffs = 0;
    fPar.MPEG2.BrokenLinkFlag = sh.broken_link;
    fPar.MPEG2.CloseEntryFlag = sh.closed_gop;

    fPar.MPEG2.NumRefFrame = GetMfxNumRefFrame((mfxU8)ph.picture_coding_type);
    fPar.MPEG2.IntraPicFlag = (ph.picture_coding_type == (UMC::MPEG2FrameType)UMC::I_PICTURE);
    fPar.MPEG2.TemporalReference = (mfxU16)ph.temporal_reference;
    fPar.MPEG2.ProgressiveFrame = ph.progressive_frame;
    fPar.MPEG2.Chroma420type = 0;
    fPar.MPEG2.RepeatFirstField = ph.repeat_first_field;
    fPar.MPEG2.AlternateScan = ph.alternate_scan;
    fPar.MPEG2.IntraVLCformat = ph.intra_vlc_format;
    fPar.MPEG2.QuantScaleType = ph.q_scale_type;
    fPar.MPEG2.ConcealmentMVs = ph.concealment_motion_vectors;
    fPar.MPEG2.FrameDCTprediction = ph.frame_pred_frame_dct;
    fPar.MPEG2.TopFieldFirst = ph.top_field_first;
    fPar.MPEG2.PicStructure = ph.picture_structure;
    fPar.MPEG2.IntraDCprecision = ph.intra_dc_precision;

    Ipp8u f_code_8u[4] = {
        (Ipp8u)ph.f_code[0],
        (Ipp8u)ph.f_code[1],
        (Ipp8u)ph.f_code[2],
        (Ipp8u)ph.f_code[3]
    };

    fPar.MPEG2.BitStreamFcodes = GetMfxBitStreamFcodes(f_code_8u);
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

bool DARtoPAR(mfxI32 width, mfxI32 height, mfxI32 dar_h, mfxI32 dar_v,
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


bool CalcAspectRatio(mfxI32 dar_code, mfxI32 width, mfxI32 height,
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
, m_implUmc()
, m_pCore(core)
, m_FrameAllocator(NULL)
#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
, m_FrameAllocator_d3d(NULL)
#endif
, m_FrameAllocator_nv12(NULL)
, m_isInitialized(false)
, m_isSWBuf(false)
, m_isSWImpl(false)
{
    int i;

    for(i = 0; i < NUM_FRAMES; i++)
    {
        m_frame[i].Data = NULL;
        m_frame_in_use[i] = false;
    }
    m_frame_curr = -1;
    m_frame_free = -1;
    m_frame_constructed = true;

    *sts = MFX_ERR_NONE;
    if (!m_pCore)
        *sts = MFX_ERR_NULL_PTR;

    for(i = 0; i < DPB; i++)
    {
        mid[i] = -1;
    }

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
    m_SkipLevel = SKIP_NONE;
    m_reset_done = false;
    m_resizing = false;
    m_InitW = 0;
    m_InitH = 0;
    m_InitPicStruct = 0;
    m_extendedPicStruct = 0;
    m_aspect_ratio_information = 0;

#ifdef UMC_VA_DXVA
    memset(m_pStatusReport, 0, sizeof(DXVA_Status_VC1) * MPEG2_STATUS_REPORT_NUM);
#endif

    m_found_SH = false;
    m_first_SH = true;
    m_new_bs   = true;

    for(int i = 0; i < NUM_FRAMES; i++)
        m_time[i] = (mfxF64)(-1);

    m_isDecodedOrder = false;
    m_isOpaqueMemory = false;
    m_isFrameRateFromInit = false;
    memset(&allocResponse, 0, sizeof(mfxFrameAllocResponse));
    memset(&m_opaqueResponse, 0, sizeof(mfxFrameAllocResponse));
#if (defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)) && defined (ELK_WORKAROUND)
    memset(&allocResponseAdd, 0, sizeof(mfxFrameAllocResponse));
#endif
    memset(m_last_bytes,0,NUM_REST_BYTES);
}

VideoDECODEMPEG2::~VideoDECODEMPEG2()
{
    Close();
}

mfxStatus VideoDECODEMPEG2::Init(mfxVideoParam *par)
{
    if (m_isInitialized)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }


    maxNumFrameBuffered = NUM_FRAMES_BUFFERED;

    m_frame_curr = -1;
    m_frame_free = -1;
    m_frame_constructed = true;
    m_found_SH = false;
    m_first_SH = true;
    memset(m_last_bytes,0,NUM_REST_BYTES);
    m_task_num = 0;
    m_prev_task_num = -1;

    MFX_CHECK_NULL_PTR1(par);

    // video conference mode
    // async depth == 1 means no bufferezation
    if (1 == par->AsyncDepth)
    {
        maxNumFrameBuffered = 1;
    }

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

    if (!(par->IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY |
                            MFX_IOPATTERN_IN_SYSTEM_MEMORY |
                            MFX_IOPATTERN_OUT_VIDEO_MEMORY|
                            MFX_IOPATTERN_OUT_SYSTEM_MEMORY |
                            MFX_IOPATTERN_IN_OPAQUE_MEMORY |
                            MFX_IOPATTERN_OUT_OPAQUE_MEMORY)))
    {
       return MFX_ERR_INVALID_VIDEO_PARAM;
    }

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

    mfxU16 IOPattern = par->IOPattern;

    m_isDecodedOrder = (1 == par->mfx.DecodedOrder) ? true : false;

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
            m_FrameAllocator_nv12 = new mfx_UMC_FrameAllocator_NV12;
            m_FrameAllocator = m_FrameAllocator_nv12;
        }
        else
        {
            if (IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
            {
                m_FrameAllocator_d3d = new mfx_UMC_FrameAllocator_D3D;
            }
            else
            {
                m_FrameAllocator_d3d = new mfx_UMC_FrameAllocator_D3D;
            }
            m_FrameAllocator = m_FrameAllocator_d3d;
        }
        if(IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
            m_isSWBuf = true;
#else
        IOPattern |= MFX_IOPATTERN_IN_SYSTEM_MEMORY;
        m_FrameAllocator_nv12 = new mfx_UMC_FrameAllocator_NV12;
        m_FrameAllocator = m_FrameAllocator_nv12;
        m_isSWImpl = true;
#endif

    if(m_isSWImpl && par->Protected)
    {
        if (MFX_HW_UNKNOWN == type)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

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

    // frames allocation
    memset(&allocResponse, 0, sizeof(mfxFrameAllocResponse));

    if (false == m_isSWImpl || (true == m_isSWImpl && ((IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) || IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)))
    {
        // positive condition means that decoder was configured as
        //  a. as hardware implementation or
        //  b. as software implementation with d3d9 output surfaces

        mfxExtOpaqueSurfaceAlloc *pOpqExt = NULL;

        QueryIOSurfInternal(m_pCore, par, &allocRequest);

        //const mfxU16 NUM = allocRequest.NumFrameSuggested;
        allocResponse.NumFrameActual = allocRequest.NumFrameSuggested;

        if (true == m_isSWImpl)
        {
            // software decoder implementation
            // output buffer is external d3d9 surfaces, so decoder should allocate internal system memory surfaces
            allocRequest.Type = MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
        }
        else
        {
            // hardware decoder implementation
            if (MFX_IOPATTERN_OUT_SYSTEM_MEMORY & IOPattern)
            {
                // output buffer is external system memory, so decoder should allocate d3d9 surfaces
                allocRequest.Type = MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
            }
            else if (MFX_IOPATTERN_OUT_VIDEO_MEMORY & IOPattern)
            {
                // output buffer is external d3d9 surfaces, so decoder should request d3d9 surfaces from external allocator
                allocRequest.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET;
                //m_FrameAllocator->SetExternalFramesResponse(&allocResponse);
            }
        }

        if (MFX_IOPATTERN_OUT_OPAQUE_MEMORY & IOPattern) // opaque memory case
        {
            pOpqExt = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

            if (NULL != pOpqExt)
            {
                if (allocRequest.NumFrameMin > pOpqExt->Out.NumSurface)
                {
                    return MFX_ERR_INVALID_VIDEO_PARAM;
                }

                m_isOpaqueMemory = true;

                // We shouldn't analyze MFX_MEMTYPE_FROM_DECODE
                //if (MFX_MEMTYPE_FROM_DECODE & pOpqExt->Out.Type)
                //{
                    /*
                    if (!(MFX_MEMTYPE_DXVA2_DECODER_TARGET & pOpqExt->Out.Type) && !(MFX_MEMTYPE_SYSTEM_MEMORY & pOpqExt->Out.Type))
                    {
                        return MFX_ERR_INVALID_VIDEO_PARAM;
                    }

                    if ((MFX_MEMTYPE_DXVA2_DECODER_TARGET & pOpqExt->Out.Type) && (MFX_MEMTYPE_SYSTEM_MEMORY & pOpqExt->Out.Type))
                    {
                        return MFX_ERR_INVALID_VIDEO_PARAM;
                    }*/

                if (true == m_isSWImpl)
                {
                    if (MFX_MEMTYPE_SYSTEM_MEMORY & pOpqExt->Out.Type)
                    {
                        {
                            // map surfaces with opaque
                            allocRequest.Type = (mfxU16)pOpqExt->Out.Type;
                            allocRequest.NumFrameMin = allocRequest.NumFrameSuggested = (mfxU16)pOpqExt->Out.NumSurface;
                        }
                    }
                    else
                    {
                        allocRequest.Type = MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_FROM_DECODE;
                        allocRequest.Type |= MFX_MEMTYPE_DXVA2_DECODER_TARGET;
                        allocRequest.NumFrameMin = allocRequest.NumFrameSuggested = (mfxU16)pOpqExt->Out.NumSurface;
                    }
                }
                else
                    allocRequest.NumFrameMin = allocRequest.NumFrameSuggested = (mfxU16)pOpqExt->Out.NumSurface;
                
                // We shouldn't analyze MFX_MEMTYPE_FROM_DECODE. Commented block below was the case when MFX_MEMTYPE_FROM_DECODE
                // was not set
                /* 
                }
                else
                {
                    mfxStatus sts;
                    mfxFrameAllocRequest trequest = allocRequest;
                    trequest.Type =  (mfxU16)pOpqExt->Out.Type;
                    trequest.NumFrameMin = trequest.NumFrameSuggested = (mfxU16)pOpqExt->Out.NumSurface;

                    sts = m_pCore->AllocFrames(&trequest, 
                                               &m_opaqueResponse,
                                               pOpqExt->In.Surfaces, 
                                               pOpqExt->In.NumSurface);

                    if (MFX_ERR_NONE != sts && MFX_ERR_UNSUPPORTED != sts)
                    {
                        // unsupported means that current Core couldn;t allocate the surfaces
                        return sts;
                    }
                    
                }
                */
            }
        }

        if (true == m_isOpaqueMemory && NULL != pOpqExt)
        {
            mfxSts  = m_pCore->AllocFrames(&allocRequest, 
                                           &allocResponse, 
                                           pOpqExt->Out.Surfaces, 
                                           pOpqExt->Out.NumSurface);
        }
        else
        {
            bool isNeedCopy = ((MFX_IOPATTERN_OUT_SYSTEM_MEMORY & IOPattern) && (allocRequest.Type & MFX_MEMTYPE_INTERNAL_FRAME)) || ((MFX_IOPATTERN_OUT_VIDEO_MEMORY & IOPattern) && (m_isSWImpl));
            allocRequest.AllocId = par->AllocId;
            mfxSts = m_pCore->AllocFrames(&allocRequest, &allocResponse, isNeedCopy);
            if(mfxSts)
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        if (mfxSts != MFX_ERR_NONE && mfxSts != MFX_ERR_NOT_FOUND)
        {
            // second status means that external allocator was not found, it is ok
            return mfxSts;
        }

        #if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
            if (false == m_isSWImpl)
            {
                // create directx video accelerator
                mfxSts = m_pCore->CreateVA(par, &allocRequest, &allocResponse, m_FrameAllocator);
                if(mfxSts)
                    return MFX_ERR_INVALID_VIDEO_PARAM;
            }
        #endif

        UMC::Status sts = UMC::UMC_OK;

        // no need to save surface descriptors if all memory is d3d
        if ((false == m_isSWImpl && (IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)) || (true == m_isSWImpl && (IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)))
        {
            sts = m_FrameAllocator->InitMfx(NULL, m_pCore, par, &allocRequest, &allocResponse, false, m_isSWImpl);
        }
        else
        {
             // means that memory is d3d9 surfaces
            m_FrameAllocator->SetExternalFramesResponse(&allocResponse);
            sts = m_FrameAllocator->InitMfx(NULL, m_pCore, par, &allocRequest, &allocResponse, true, m_isSWImpl);
        }

        if (UMC::UMC_OK != sts)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }
    }
    else
    {
        // positive condition means that decoder was configured as 
        // software implementation with system external output memory
        UMC::Status sts = m_FrameAllocator->InitMfx(NULL, m_pCore, par, &allocRequest, &allocResponse, true, m_isSWImpl);
        
        if (UMC::UMC_OK != sts)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }
    }

    m_extendedPicStruct = par->mfx.ExtendedPicStruct;

    m_vdPar.info.framerate = 0;

    m_InitW = par->mfx.FrameInfo.Width;
    m_InitH = par->mfx.FrameInfo.Height;

    mfxSts = InternalReset(par);
    MFX_CHECK_STS(mfxSts);
    
    m_vdPar.numThreads = (mfxU16)(par->AsyncDepth ? par->AsyncDepth : m_pCore->GetAutoAsyncDepth());

    if (m_pCore->GetPlatformType()== MFX_PLATFORM_HARDWARE)
    {
        if (true == m_isSWImpl)
        {
            m_vdPar.pVideoAccelerator = NULL;
        }
        else
        {
            m_vdPar.numThreads = 1;
        }
    }

    m_implUmc.Close();

    // save framerate value from init
    mfxU32 frameRateD = par->mfx.FrameInfo.FrameRateExtD;
    mfxU32 frameRateN = par->mfx.FrameInfo.FrameRateExtN;

    m_vdPar.info.aspect_ratio_width = par->mfx.FrameInfo.AspectRatioW;
    m_vdPar.info.aspect_ratio_height = par->mfx.FrameInfo.AspectRatioH;
    
    if (0 != frameRateD && 0 != frameRateN)
    {
        m_vdPar.info.framerate = (Ipp64f) frameRateN / frameRateD;
        m_isFrameRateFromInit = true;
    }

    UMC::Status umcRes = m_implUmc.Init(&m_vdPar);
    MFX_CHECK_UMC_STS(umcRes);
    
    umcRes = m_implUmc.GetInfo(&m_vdPar);
    MFX_CHECK_UMC_STS(umcRes);

    if (MFX_PLATFORM_HARDWARE == m_pCore->GetPlatformType() && true == m_isSWImpl)
    {
        mfxSts = MFX_WRN_PARTIAL_ACCELERATION;
    }

    m_out.Close();
    m_out.SetAlignment(MFX_MPEG2_DECODE_ALIGNMENT);
    umcRes = m_out.Init(m_vdPar.info.clip_info.width, m_vdPar.info.clip_info.height, m_vdPar.info.color_format);
    MFX_CHECK_UMC_STS(umcRes);

    m_isInitialized = true;

    return mfxSts;

} // mfxStatus VideoDECODEMPEG2::Init(mfxVideoParam *par)

mfxStatus VideoDECODEMPEG2::Reset(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    if (par->AsyncDepth != m_vPar.AsyncDepth)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (MFX_PLATFORM_SOFTWARE == m_pCore->GetPlatformType() || m_isSWImpl)
    {
        for (mfxU32 i = 0; i < DPB; i++)
        {
            if(mid[i] >= 0) m_FrameAllocator->Unlock(mid[i]);
        }
    }

    for (mfxU32 i = 0; i < DPB; i++)
    {
        if (mid[i] >= 0)
        {
            m_FrameAllocator->DecreaseReference(mid[i]);
        }
    }

    for (mfxU32 i = 0; i < DPB; i++)
    {
        mid[i] = -1;
    }

    prev_index      = -1;
    curr_index      = -1;
    next_index      = -1;
    display_index   = -1;
    display_order   =  0;
    dec_frame_count = 0;
    dec_field_count = 0;
    display_frame_count = 0;
    cashed_frame_count = 0;
    skipped_frame_count = 0;
    last_frame_count = 0;
    last_timestamp   = 0;
    m_SkipLevel = SKIP_NONE;

    maxNumFrameBuffered = NUM_FRAMES_BUFFERED;

    m_FrameAllocator->Reset();
    m_reset_done = true;
    m_resizing = false;

    m_frame_curr = -1;
    m_frame_free = -1;
    m_frame_constructed = true;
    m_found_SH = false;
    m_first_SH = true;
    m_new_bs   = true;

    m_isFrameRateFromInit = false;

    mfxExtOpaqueSurfaceAlloc *pOpqExt = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

    if (NULL != pOpqExt)
    {
        if (false == m_isOpaqueMemory)
        {
            // decoder was not initialized with opaque extended buffer
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }

        if (allocRequest.NumFrameMin != pOpqExt->Out.NumSurface)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    m_task_num = 0;
    m_prev_task_num = -1;

#ifdef UMC_VA_DXVA
    memset(m_pStatusReport, 0, sizeof(DXVA_Status_VC1) * MPEG2_STATUS_REPORT_NUM);
#endif

    for (mfxU32 i = 0; i < NUM_FRAMES; i++)
    {
        m_frame[i].DataLength = 0;
        m_frame[i].DataOffset = 0;
        m_frame_in_use[i] = false;
        m_time[i] = (mfxF64)(-1);
    }

    memset(m_last_bytes,0,NUM_REST_BYTES);

    // use framerate value from init
    mfxU32 frameRateD = par->mfx.FrameInfo.FrameRateExtD;
    mfxU32 frameRateN = par->mfx.FrameInfo.FrameRateExtN;
    
    if (0 != frameRateD && 0 != frameRateN)
    {
        m_vdPar.info.framerate = (Ipp64f) frameRateN / frameRateD;
    }

    mfxStatus mfxRes = InternalReset(par);
    MFX_CHECK_STS(mfxRes);

    if (m_isSWImpl && m_pCore->GetPlatformType() == MFX_PLATFORM_HARDWARE)
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2::Close()
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxU32 i;

    // immediately return, object is not initialized
    if (false == m_isInitialized)
    {
        return MFX_ERR_NONE;
    }

    if (MFX_PLATFORM_SOFTWARE == m_pCore->GetPlatformType() || m_isSWImpl)
    {
        for(i = 0; i < DPB; i++)
            if(mid[i] >= 0) m_FrameAllocator->Unlock(mid[i]);
    }

    for (i = 0; i < DPB; i++)
        if(mid[i] >= 0)
            m_FrameAllocator->DecreaseReference(mid[i]);

    for (i = 0; i < DPB; i++)
    {
        mid[i] = -1;
    }

    m_implUmc.Close();
    m_out.Close();

    m_extendedPicStruct = 0;

    for (mfxU32 i = 0; i < NUM_FRAMES; i++)
    {
        if(m_frame[i].Data) delete []m_frame[i].Data;
        m_frame[i].Data = NULL;
    }

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

#if (defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)) && defined (ELK_WORKAROUND)
    if(allocResponseAdd.NumFrameActual > 0)
    {
        mfxRes = m_pCore->FreeFrames(&allocResponseAdd);
        memset(&allocResponseAdd, 0, sizeof(mfxFrameAllocResponse));
    }
#endif
    if(m_FrameAllocator)
        m_FrameAllocator->Close();

    if(m_FrameAllocator_nv12)
    {
        delete m_FrameAllocator_nv12;
        m_FrameAllocator_nv12 = NULL;
    }
#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
    if(m_FrameAllocator_d3d)
    {
        delete m_FrameAllocator_d3d;
        m_FrameAllocator_d3d = NULL;
    }
#endif

    m_FrameAllocator = NULL;


    MFX_CHECK_STS(mfxRes);

    m_isInitialized = false;
    m_isOpaqueMemory = false;
    m_isFrameRateFromInit = false;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2::GetDecodeStat(mfxDecodeStat *stat)
{
    MFX_CHECK_NULL_PTR1(stat);
    
    if (false == m_isInitialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    stat->NumFrame = display_frame_count;
    stat->NumCachedFrame = cashed_frame_count;
    stat->NumSkippedFrame = skipped_frame_count;
    
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2::GetUserData(mfxU8 *ud, mfxU32 *sz, mfxU64 *ts, mfxU16 bufsize)
{
    MFX_CHECK_NULL_PTR3(ud, sz, ts);

    UMC::Status umcSts = UMC::UMC_OK;

    umcSts = m_implUmc.GetCCData(ud, sz, ts, bufsize);

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

#define MPEG2_SH_SIZE 34

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
        //  Skip MPEG1 bitstream
        //  par->mfx.CodecProfile = MFX_PROFILE_MPEG1;
        //  par->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
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

bool VideoDECODEMPEG2:: IsHWSupported(VideoCORE *pCore, mfxVideoParam *par)
{
    if((par->mfx.CodecProfile == MFX_PROFILE_MPEG1 
        && pCore->GetPlatformType()== MFX_PLATFORM_HARDWARE))
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

mfxStatus VideoDECODEMPEG2::QueryIOSurfInternal(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)
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

} // mfxStatus VideoDECODEMPEG2::QueryIOSurfInternal(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)

mfxStatus VideoDECODEMPEG2::QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
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
    mfxStatus sts = QueryIOSurfInternal(core, par, request);
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

    par->IOPattern = m_vPar.IOPattern;
    par->mfx       = m_vPar.mfx;
    par->Protected = m_vPar.Protected;
    par->AsyncDepth = m_vPar.AsyncDepth;
    par->mfx.FrameInfo.AspectRatioW = m_vPar.mfx.FrameInfo.AspectRatioW;
    par->mfx.FrameInfo.AspectRatioH = m_vPar.mfx.FrameInfo.AspectRatioH;
    par->mfx.FrameInfo.FrameRateExtD = m_vPar.mfx.FrameInfo.FrameRateExtD;
    par->mfx.FrameInfo.FrameRateExtN = m_vPar.mfx.FrameInfo.FrameRateExtN;
    
    mfxExtPAVPOption * buffer = (mfxExtPAVPOption*)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);
    if (buffer)
    {
        if (!IS_PROTECTION_PAVP_ANY(m_vPar.Protected))
            return MFX_ERR_INVALID_VIDEO_PARAM;

        memcpy_s(buffer, sizeof(mfxExtPAVPOption), &m_pavpOpt, sizeof(mfxExtPAVPOption));
    }

    // get sps/pps buffer
    mfxExtCodingOptionSPSPPS *spspps = (mfxExtCodingOptionSPSPPS *) GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS);

    if (NULL != spspps)
        if (NULL != spspps->SPSBuffer)
        {
            UMC::Status sts = m_implUmc.GetSequenceHeaderMemoryMask(spspps->SPSBuffer, spspps->SPSBufSize);
            if (UMC::UMC_OK != sts)
                return ConvertUMCStatusToMfx(sts);
        }
    // get signal info buffer
    mfxExtVideoSignalInfo *signal_info = (mfxExtVideoSignalInfo *) GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO);

    if (NULL != signal_info)
    {
        m_implUmc.GetSignalInfoInformation(signal_info);
    }
    memcpy_s(par->reserved,sizeof(m_vPar.reserved),m_vPar.reserved,sizeof(m_vPar.reserved));
    par->reserved2 = m_vPar.reserved2;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2::GetFrameParam(mfxFrameParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    *par = m_fPar;
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2::GetSliceParam(mfxSliceParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    *par = m_sPar;
    return MFX_ERR_NONE;
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
            m_implUmc.SetSkipMode(m_SkipLevel);
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
            m_implUmc.SetSkipMode(m_SkipLevel);
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
            m_implUmc.SetSkipMode(m_SkipLevel);
        }
        else
        {
            Reset(&m_vPar);
            m_implUmc.SetSkipMode(m_SkipLevel);
        }
        break;
    default:
        ret = MFX_ERR_UNDEFINED_BEHAVIOR;
        break;
    }
    return ret;
}

mfxStatus VideoDECODEMPEG2::SetOutputSurfaceParams(mfxFrameSurface1 *surface, int display_index)
{
    MFX_CHECK_NULL_PTR1(surface);

    surface->Data.TimeStamp = GetMfxTimeStamp(m_implUmc.GetCurrDecodedTime(display_index));
    surface->Data.DataFlag = (mfxU16)((m_implUmc.isOriginalTimeStamp(display_index)) ? MFX_FRAMEDATA_ORIGINAL_TIMESTAMP : 0);
    SetSurfacePicStruct(surface, display_index);
    UpdateOutputSurfaceCrops(surface, display_index);

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2::SetSurfacePicStruct(mfxFrameSurface1 *surface, int display_index)
{
    MFX_CHECK_NULL_PTR1(surface);

    if (display_index < 0 || display_index >= 2*DPB)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    const UMC::sPictureHeader& ph = m_implUmc.GetPictureHeader(display_index);
    const UMC::sSequenceHeader& sh = m_implUmc.GetSequenceHeader();

    surface->Info.PicStruct = GetMfxPicStruct(sh.progressive_sequence, ph.progressive_frame,
                                              ph.top_field_first, ph.repeat_first_field, ph.picture_structure,
                                              m_vPar.mfx.ExtendedPicStruct);
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2::UpdateWorkSurfaceCrops(int task_num)
{
    mfxFrameSurface1 *pSurface = m_FrameAllocator->GetSurfaceByIndex(mid[task_num]);
    MFX_CHECK_NULL_PTR1(pSurface);
    pSurface->Info.CropW = m_vPar.mfx.FrameInfo.CropW;
    pSurface->Info.CropH = m_vPar.mfx.FrameInfo.CropH;
    pSurface->Info.CropX = m_vPar.mfx.FrameInfo.CropX;
    pSurface->Info.CropY = m_vPar.mfx.FrameInfo.CropY;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2::UpdateOutputSurfaceCrops(mfxFrameSurface1 *outputSurface, int display_index)
{
    if (!m_isSWBuf && !m_isOpaqueMemory)
        return MFX_ERR_NONE;

    mfxFrameSurface1 *internalSurface = m_FrameAllocator->GetSurfaceByIndex(mid[display_index]);
    MFX_CHECK_NULL_PTR1(internalSurface);
    outputSurface->Info.CropW = internalSurface->Info.CropW;
    outputSurface->Info.CropH = internalSurface->Info.CropH;
    outputSurface->Info.CropX = internalSurface->Info.CropX;
    outputSurface->Info.CropY = internalSurface->Info.CropY;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2::UpdateCurrVideoParams(mfxFrameSurface1 *surface_work, int task_num)
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

    const UMC::sSequenceHeader& sh = m_implUmc.GetSequenceHeader();
    const UMC::sPictureHeader& ph = m_implUmc.GetPictureHeader(task_num);

    if (m_isFrameRateFromInit == false && ((Ipp32u)sh.frame_rate_extension_d != m_vPar.mfx.FrameInfo.FrameRateExtD || (Ipp32u)sh.frame_rate_extension_n != m_vPar.mfx.FrameInfo.FrameRateExtN))
    {
        sts = MFX_WRN_VIDEO_PARAM_CHANGED;
    }


    pSurface->Info.AspectRatioH = (mfxU16)m_implUmc.GetAspectRatioH();
    pSurface->Info.AspectRatioW = (mfxU16)m_implUmc.GetAspectRatioW();

    UpdateMfxVideoParam(m_vPar, sh, ph);
    UpdateMfxFrameParam(m_fPar, sh, ph);

    pSurface->Info.PicStruct = m_vPar.mfx.FrameInfo.PicStruct;    
    pSurface->Info.FrameRateExtD = m_vPar.mfx.FrameInfo.FrameRateExtD;
    pSurface->Info.FrameRateExtN = m_vPar.mfx.FrameInfo.FrameRateExtN;

    return sts;
}

static mfxStatus __CDECL MPEG2TaskRoutine(void *pState, void *pParam, mfxU32 /*threadNumber*/, mfxU32 /*callNumber*/)
{
    VideoDECODEMPEG2 *decoder = (VideoDECODEMPEG2*)pState;

    mfxStatus sts = decoder->TaskRoutine(pParam);
    return sts;
}

mfxStatus VideoDECODEMPEG2::TaskRoutine(void *pParam)
{
    MParam *parameters = (MParam *)pParam;

    UMC::Status res;
    Ipp32s th_idx;

    if (false == parameters->IsSWImpl)
    {
        mfxStatus sts = MFX_ERR_NONE;

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

        // there is nothing to do
        return MFX_TASK_DONE;
    }

    UMC::AutomaticUMCMutex guard(m_guard);

    THREAD_DEBUG_PRINTF(
        "(THREAD %x) task %x number, task num %d, curr thr idx %d, compl thr %d\n",
        GetCurrentThreadId(), pParam, parameters->task_num, parameters->m_curr_thread_idx, parameters->m_thread_completed)

    if (0 < parameters->last_frame_count && 0 == parameters->m_thread_completed)
    {

        Ipp32s display_index = parameters->display_index;

        if (0 <= display_index)
        {
            m_implUmc.PostProcessUserData(display_index);

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
    res = m_implUmc.DoDecodeSlices(th_idx, parameters->task_num);

    // up mutex
    guard.Lock();

    THREAD_DEBUG_PRINTF("(THREAD %x): finish decoding\n", GetCurrentThreadId())

    parameters->m_thread_completed++;

    if (UMC::UMC_OK != res && UMC::UMC_ERR_NOT_ENOUGH_DATA != res && UMC::UMC_ERR_SYNC != res)
    {
        m_implUmc.SetCorruptionFlag(parameters->curr_index);

        /*
        parameters->m_frame[parameters->m_frame_curr].DataLength = 0;
        parameters->m_frame[parameters->m_frame_curr].DataOffset = 0;
        parameters->m_frame_in_use[parameters->m_frame_curr] = false;
        parameters->m_thread_completed = parameters->NumThreads + 1;

        m_implUmc.UnLockTask(parameters->task_num);

        THREAD_DEBUG_PRINTF("(THREAD %x): UMC::UMC_OK != res && UMC::UMC_ERR_NOT_ENOUGH_DATA != res && UMC::UMC_ERR_SYNC != res\n", GetCurrentThreadId())

        return MFX_TASK_DONE;
        */
    }

    if (parameters->m_thread_completed == parameters->NumThreads)
    {
        Ipp32s display_index = parameters->display_index;

        //res = m_implUmc.PostProcessFrame(display_index, parameters->task_num);

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
                m_implUmc.UnLockTask(parameters->task_num);
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

static mfxStatus __CDECL MPEG2CompleteTasks(void *pState, void *pParam, mfxStatus /*sts*/)
{
    VideoDECODEMPEG2 *decoder = (VideoDECODEMPEG2*)pState;

    mfxStatus sts = decoder->CompleteTasks(pParam);
    return sts;
}

mfxStatus VideoDECODEMPEG2::CompleteTasks(void *pParam)
{
    MParam *parameters = (MParam *)pParam;
    UMC::AutomaticUMCMutex guard(m_guard);

    THREAD_DEBUG_PRINTF(
        "(THREAD %x) CompleteTasks: task %x number, task num %d, curr thr idx %d, compl thr %d\n",
        GetCurrentThreadId(), pParam, parameters->task_num, parameters->m_curr_thread_idx, parameters->m_thread_completed)

    Ipp32s display_index = parameters->display_index;

    if (0 <= display_index)
    {
        SetSurfacePictureType(parameters->surface_out, display_index, &m_implUmc);
        SetSurfaceTimeCode(parameters->surface_out, display_index, &m_implUmc);

        THREAD_DEBUG_PRINTF("(THREAD %x) Dumping\n", GetCurrentThreadId())

        if (false == parameters->IsSWImpl)
        {
            STATUS_REPORT_DEBUG_PRINTF("thread task idx %d, display address %x\n", parameters->task_num, parameters->surface_out)

            if (IsStatusReportEnable(m_pCore))
            {
                //parameters->surface_out->Data.Corrupted = 0;

                // request status report structures and wait until frame is not ready
                mfxStatus sts = GetStatusReport(parameters->surface_out, parameters->mid[display_index]);

                if (MFX_ERR_NONE != sts)
                {
                    parameters->m_FrameAllocator->DecreaseReference(parameters->mid[display_index]);
                    parameters->m_frame_in_use[parameters->m_frame_curr] = false;

                    return sts;
                }

                
                //if (false == parameters->m_is_wa)
                {
                    mfxU16 isCorrupted = parameters->surface_out->Data.Corrupted;
                    
                    mfxU32 frameType = m_implUmc.GetFrameType(display_index);
                    Ipp32s prev_index = m_implUmc.GetPrevDecodingIndex(display_index);
                    
                    if (isCorrupted && I_PICTURE == frameType)
                        m_implUmc.SetCorruptionFlag(display_index);

                    switch (frameType)
                    {
                        case I_PICTURE:

                            if (isCorrupted)
                            {
                                parameters->surface_out->Data.Corrupted = MFX_CORRUPTION_MAJOR;
                            }

                            break;
                        
                        default: // P_PICTURE

                            if (true == m_implUmc.GetCorruptionFlag(prev_index))
                            {
                                m_implUmc.SetCorruptionFlag(display_index);
                                parameters->surface_out->Data.Corrupted = MFX_CORRUPTION_REFERENCE_FRAME;
                            }
                    }
                }
                
            }
            
            if (B_PICTURE == m_implUmc.GetFrameType(display_index))
            {
                parameters->m_FrameAllocator->DecreaseReference(parameters->mid[display_index]);
                parameters->mid[display_index] = -1;

                m_implUmc.UnLockTask(display_index);
            }
            else // I or P
            {
                Ipp32s p_index = m_implUmc.GetPrevDecodingIndex(display_index);

                if (true == m_isDecodedOrder && 0 <= p_index)
                {
                     p_index = m_implUmc.GetPrevDecodingIndex(p_index);

                    if (0 <= p_index)
                    {
                        parameters->m_FrameAllocator->DecreaseReference(parameters->mid[p_index]);
                        parameters->mid[p_index] = -1;

                        m_implUmc.UnLockTask(p_index);
                    }
                }
                else if (0 <= p_index)
                {
                    parameters->m_FrameAllocator->DecreaseReference(parameters->mid[p_index]);
                    parameters->mid[p_index] = -1;

                    m_implUmc.UnLockTask(p_index);
                }
            }
        }
        else
        {
            if (B_PICTURE == m_implUmc.GetFrameType(display_index))
            {
                parameters->m_FrameAllocator->Unlock(parameters->mid[display_index]);
                parameters->m_FrameAllocator->DecreaseReference(parameters->mid[display_index]);
                parameters->mid[display_index] = -1;

                m_implUmc.UnLockTask(display_index);
            }
            else // I or P picture
            {
                bool isCorrupted = m_implUmc.GetCorruptionFlag(display_index);

                mfxU32 frameType = m_implUmc.GetFrameType(display_index);
                Ipp32s prev_index = m_implUmc.GetPrevDecodingIndex(display_index);
                
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

                        if (P_PICTURE == m_implUmc.GetFrameType(prev_index))
                        {
                            //prev_index = m_implUmc.GetPrevDecodingIndex(prev_index);
                        }

                        if (true == m_implUmc.GetCorruptionFlag(prev_index))
                        {
                            m_implUmc.SetCorruptionFlag(display_index);
                            parameters->surface_out->Data.Corrupted = MFX_CORRUPTION_REFERENCE_FRAME;
                        }
                }

                if (0 <= prev_index && true == parameters->m_isDecodedOrder)
                {
                    prev_index = m_implUmc.GetPrevDecodingIndex(prev_index);
                }

                if (0 <= prev_index)
                {
                    parameters->m_FrameAllocator->Unlock(parameters->mid[prev_index]);
                    parameters->m_FrameAllocator->DecreaseReference(parameters->mid[prev_index]);
                    parameters->mid[prev_index] = -1;

                    m_implUmc.UnLockTask(prev_index);
                }
            }
        }
    }

    //if (true == parameters->IsSWImpl)
    {
        parameters->m_frame_in_use[parameters->m_frame_curr] = false;

        STATUS_REPORT_DEBUG_PRINTF("m_frame_curr %d is %d\n", parameters->m_frame_curr, parameters->m_frame_in_use[parameters->m_frame_curr])
    }

    return MFX_TASK_DONE;
}

mfxStatus VideoDECODEMPEG2::CheckFrameData(const mfxFrameSurface1 *pSurface)
{
    MFX_CHECK_NULL_PTR1(pSurface);
    
    if (0 != pSurface->Data.Locked)
    {
        return MFX_ERR_MORE_SURFACE;
    }

    if (pSurface->Info.Width <  m_InitW ||
        pSurface->Info.Height < m_InitH)
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


    if (pSurface->Info.Width >  m_InitW ||
        pSurface->Info.Height > m_InitH)
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
    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR1(surface_disp);

    if (false == m_isInitialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    if (true == m_isOpaqueMemory)
    {
        if (surface_work->Data.MemId || surface_work->Data.Y || surface_work->Data.R || surface_work->Data.A || surface_work->Data.UV) // opaq surface
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        surface_work = GetOriginalSurface(surface_work);
    }

    sts = CheckFrameData(surface_work);
    MFX_CHECK_STS(sts);

#ifdef MFX_VA_WIN
    if (m_Protected && bs != NULL)
    {
        VM_ASSERT(m_implUmc.pack_w.m_va->GetProtectedVA());
        if (!m_implUmc.pack_w.m_va->GetProtectedVA() || !(bs->DataFlag & MFX_BITSTREAM_COMPLETE_FRAME))
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        m_implUmc.pack_w.m_va->GetProtectedVA()->SetModes(&m_vPar);
        m_implUmc.pack_w.m_va->GetProtectedVA()->SetBitstream(bs);

        m_implUmc.pack_w.IsProtectedBS = true;
        if (!bs->EncryptedData) m_implUmc.pack_w.IsProtectedBS = false;
    }
#endif

    if (m_frame_constructed && bs != NULL)
    {
        if (!SetFree_m_frame())
        {
            return MFX_WRN_DEVICE_BUSY;
        }

        memset(m_frame[m_frame_free].Data,0,m_frame[m_frame_free].MaxLength);
    }

    m_frame_constructed = false;

    if (NULL != bs)
    {
        sts = CheckBitstream(bs);
        MFX_CHECK_STS(sts);

        if (!bs->DataLength)
        {
            return MFX_ERR_MORE_DATA;
        }

        if (true == m_new_bs)
        {
            m_time[m_frame_free] = GetUmcTimeStamp(bs->TimeStamp);
            m_time[m_frame_free] = (m_time[m_frame_free]==last_good_timestamp)?-1.0:m_time[m_frame_free];    
        }

        sts = ConstructFrame(bs, &m_frame[m_frame_free], surface_work);

        if (MFX_ERR_NONE != sts)
        {
            m_frame_constructed = false;
            surface_disp = NULL;
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

    if (!(dec_field_count & 1))
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        m_task_num = m_implUmc.FindFreeTask();
    }

    if (-1 == m_task_num)
    {
        return MFX_WRN_DEVICE_BUSY;
    }

    if (last_frame_count > 0)
    {
        if(m_implUmc.GetRetBufferLen() <= 0)
            return MFX_ERR_MORE_DATA;
    }

    sts = m_FrameAllocator->SetCurrentMFXSurface(surface_work, m_isOpaqueMemory);
    MFX_CHECK_STS(sts);
    if (m_FrameAllocator->FindFreeSurface() == -1)
        return MFX_WRN_DEVICE_BUSY;

    if (false == SetCurr_m_frame() && NULL != bs)
    {
        return MFX_WRN_DEVICE_BUSY;
    }

    m_isShDecoded = false;

    if (MFX_PLATFORM_HARDWARE == m_pCore->GetPlatformType() && !m_isSWImpl)
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
            m_implUmc.SaveDecoderState();
            umcRes = m_implUmc.GetPictureHeader(&m_in[m_task_num], m_task_num, m_prev_task_num);

            IsField = !m_implUmc.IsFramePictureStructure(m_task_num);
            if (m_task_num >= DPB && !IsField)
            {
                m_frame[m_frame_curr].DataLength = 0;
                m_frame[m_frame_curr].DataOffset = 0;
                m_frame_in_use[m_frame_curr] = false;

                if (dec_field_count % 2 != 0)
                    dec_field_count += 1;

                m_implUmc.RestoreDecoderStateAndRemoveLastField();
                Ipp32s previous_field = m_task_num - DPB;
                m_FrameAllocator->DecreaseReference(mid[previous_field]);
                m_implUmc.UnLockTask(previous_field);

                return MFX_ERR_MORE_DATA;
            }

            if (UMC::UMC_OK != umcRes)
            {
                m_frame[m_frame_curr].DataLength = 0;
                m_frame[m_frame_curr].DataOffset = 0;
                m_frame_in_use[m_frame_curr] = false;
                {
                    UMC::AutomaticUMCMutex guard(m_guard);
                    m_implUmc.RestoreDecoderState();
                }
                IsSkipped = m_implUmc.IsFrameSkipped();

                if (IsSkipped && !(dec_field_count & 1))
                {
                    skipped_frame_count += 1;
                }

                if (false == m_reset_done && NULL == bs)
                {
                    display_index = m_implUmc.GetDisplayIndex();

                    if (0 > display_index)
                    {
                        umcRes = m_implUmc.GetPictureHeader(NULL, m_task_num, m_prev_task_num);
                            
                        if (UMC::UMC_ERR_INVALID_STREAM == umcRes)
                        {
                            return MFX_ERR_UNKNOWN;
                        }

                        display_index = m_implUmc.GetDisplayIndex();
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

                            m_task_param[m_task_num].NumThreads = m_implUmc.GetCurrThreadsNum(m_task_num);
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

            m_isShDecoded = m_implUmc.IsSHDecoded() && (dec_field_count > 0);

            umcRes = m_implUmc.GetInfo(&m_vdPar);

            sts = UpdateCurrVideoParams(surface_work, m_task_num);

            if (surface_work->Info.CropW > surface_work->Info.Width || surface_work->Info.CropH > surface_work->Info.Height)
            {
                return MFX_ERR_UNKNOWN;
            }

            if ((false == m_isDecodedOrder && maxNumFrameBuffered <= (Ipp32u)(m_implUmc.GetRetBufferLen())) ||
                 true == m_isDecodedOrder)
            {
                display_index = m_implUmc.GetDisplayIndex();
            }

            if (false == IsField || (true == IsField && !(dec_field_count & 1)))
            {
                curr_index = m_task_num;
                next_index = m_implUmc.GetNextDecodingIndex(curr_index);
                prev_index = m_implUmc.GetPrevDecodingIndex(curr_index);

                UMC::VideoDataInfo Info;

                if (1 == m_implUmc.GetSequenceHeader().progressive_sequence)
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
                umcRes = m_implUmc.pack_w.m_va->BeginFrame(mid[curr_index]);

                if (UMC::UMC_OK != umcRes)
                {
                    return MFX_ERR_DEVICE_FAILED;
                }

                m_implUmc.pack_w.va_index = mid[curr_index];
#endif
            }
            else
            {
#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
                umcRes = m_implUmc.pack_w.m_va->BeginFrame(mid[curr_index]);

                if (UMC::UMC_OK != umcRes)
                {
                    return MFX_ERR_DEVICE_FAILED;
                }

                m_implUmc.pack_w.va_index = mid[curr_index];
#endif
            }

            sts = UpdateWorkSurfaceCrops(curr_index);
            MFX_CHECK_STS(sts);

            umcRes = m_implUmc.ProcessRestFrame(m_task_num);
            if (UMC::UMC_OK != umcRes)
            {
                m_FrameAllocator->DecreaseReference(mid[curr_index]);
                m_frame[m_frame_curr].DataLength = 0;
                m_frame[m_frame_curr].DataOffset = 0;
                m_frame_in_use[m_frame_curr] = false;
#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
                umcRes = m_implUmc.pack_w.m_va->EndFrame();
                if (umcRes != UMC::UMC_OK)
                    return MFX_ERR_LOCK_MEMORY;
#endif
                m_implUmc.RestoreDecoderState();
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

            {
                UMC::AutomaticUMCMutex guard(m_guard);

                memset(&m_task_param[m_task_num],0,sizeof(MParam));
                m_implUmc.LockTask(m_task_num);

                umcRes = m_implUmc.DoDecodeSlices(0, m_task_num);


            }

            if (UMC::UMC_OK != umcRes &&
                UMC::UMC_ERR_NOT_ENOUGH_DATA != umcRes &&
                UMC::UMC_ERR_SYNC != umcRes)
            {
                m_FrameAllocator->DecreaseReference(mid[curr_index]);
                m_frame[m_frame_curr].DataLength = 0;
                m_frame[m_frame_curr].DataOffset = 0;
                m_frame_in_use[m_frame_curr] = false;
                m_implUmc.RestoreDecoderState();
                m_implUmc.UnLockTask(m_task_num);
#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
                umcRes = m_implUmc.pack_w.m_va->EndFrame();
                if (umcRes != UMC::UMC_OK)
                    return MFX_ERR_LOCK_MEMORY;
#endif
                dec_field_count -= IsField?1:2;
                return MFX_ERR_MORE_DATA;
            }
            else
            {
                umcRes = m_implUmc.PostProcessFrame(display_index, m_task_num);

                if (umcRes != UMC::UMC_OK && umcRes != UMC::UMC_ERR_NOT_ENOUGH_DATA)
                {
                    m_FrameAllocator->DecreaseReference(mid[curr_index]);
                    m_frame[m_frame_curr].DataLength = 0;
                    m_frame[m_frame_curr].DataOffset = 0;
                    m_frame_in_use[m_frame_curr] = false;
                    m_implUmc.RestoreDecoderState();
                    m_implUmc.UnLockTask(m_task_num);
#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
                    umcRes = m_implUmc.pack_w.m_va->EndFrame();
                    if (umcRes != UMC::UMC_OK)
                        return MFX_ERR_LOCK_MEMORY;
#endif
                    dec_field_count -= IsField?1:2;
                    return MFX_ERR_MORE_DATA;
                }

                m_frame[m_frame_curr].DataLength = 0;
                m_frame[m_frame_curr].DataOffset = 0;
            }

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

                m_task_param[m_task_num].NumThreads = m_implUmc.GetCurrThreadsNum(m_task_num);
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
                    if (B_PICTURE == m_implUmc.GetFrameType(display_index))
                    {
                        (*surface_disp)->Data.FrameOrder = display_order;
                        display_order++;
                    }
                    else // I or P
                    {
                        Ipp32s p_index = m_implUmc.GetPrevDecodingIndex(display_index);

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

                m_implUmc.PostProcessUserData(display_index);

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

            umcRes = m_implUmc.GetPictureHeader(NULL,m_task_num,m_prev_task_num);
            sts = UpdateCurrVideoParams(surface_work, m_task_num);

            if (UMC::UMC_ERR_INVALID_STREAM == umcRes)
            {
                return MFX_ERR_UNKNOWN;
            }

            display_index = m_implUmc.GetDisplayIndex();

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
                    m_implUmc.LockTask(m_task_num);

                    m_task_param[m_task_num].NumThreads = m_implUmc.GetCurrThreadsNum(m_task_num);
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

                    m_implUmc.PostProcessUserData(display_index);

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
    else // software implementation
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
            res = m_implUmc.GetPictureHeader(&m_in[m_task_num], m_task_num, m_prev_task_num);

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

                    isSkipped = m_implUmc.IsFrameSkipped();

                    if (true == isSkipped && !(dec_field_count & 1))
                    {
                        skipped_frame_count++;
                    }

                    if (NULL == bs)
                    {
                        display_index = m_implUmc.GetDisplayIndex();
                        
                        if (0 > display_index)
                        {
                            res = m_implUmc.GetPictureHeader(NULL,m_task_num,m_prev_task_num);
                            
                            if (UMC::UMC_ERR_INVALID_STREAM == res)
                            {
                                return MFX_ERR_UNKNOWN;
                            }

                            display_index = m_implUmc.GetDisplayIndex();
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

                            (*surface_disp)->Data.TimeStamp = GetMfxTimeStamp(m_implUmc.GetCurrDecodedTime(display_index));
                            (*surface_disp)->Data.DataFlag = (mfxU16)((m_implUmc.isOriginalTimeStamp(display_index)) ? MFX_FRAMEDATA_ORIGINAL_TIMESTAMP : 0);

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
                                m_implUmc.UnLockTask(display_index);
                            }

                            pEntryPoint->requiredNumThreads = m_NumThreads;
                            pEntryPoint->pRoutine = &MPEG2TaskRoutine;
                            pEntryPoint->pState = (void*)this;
                            pEntryPoint->pParam = (void *)(&(m_task_param[m_task_num]));
                            pEntryPoint->pCompleteProc = &MPEG2CompleteTasks;
                            pEntryPoint->pRoutineName = (char *)"DecodeMPEG2";

                            m_implUmc.PostProcessUserData(display_index);

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

            m_isShDecoded = m_implUmc.IsSHDecoded() && (dec_field_count > 0);

            res = m_implUmc.GetInfo(&m_vdPar);

            sts = UpdateCurrVideoParams(surface_work, m_task_num);

            isField = !m_implUmc.IsFramePictureStructure(m_task_num);

            if (m_task_num > DPB && !isField)
            {
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }

            if ((false == isField || (true == isField && !(dec_field_count & 1))))
            {
                curr_index = m_task_num;
                next_index = m_implUmc.GetNextDecodingIndex(curr_index);
                prev_index = m_implUmc.GetPrevDecodingIndex(curr_index);

                UMC::VideoDataInfo Info;
                mfxFrameSurface1 *pSurface = surface_work;

                if (1 == m_implUmc.GetSequenceHeader().progressive_sequence)
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

            m_implUmc.SetOutputPointers(&m_out, m_task_num);

            display_index = m_implUmc.GetDisplayIndex();

            UMC::Status umcSts = m_implUmc.ProcessRestFrame(m_task_num);

            if (umcSts == UMC::UMC_OK && display_index >= 0)
            {
                last_timestamp = GetMfxTimeStamp(m_implUmc.GetCurrDecodedTime(display_index));
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

                    if (m_implUmc.GetFrameType(display_index) == B_PICTURE)
                    {
                        (*surface_disp)->Data.FrameOrder = display_order;

                        display_order++;
                    }
                    else //I or P
                    {
                        Ipp32s p_index = m_implUmc.GetPrevDecodingIndex(display_index);
                        
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

            m_task_param[m_task_num].NumThreads = m_implUmc.GetCurrThreadsNum(m_task_num);
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
                (*surface_disp)->Data.DataFlag = (mfxU16)((m_implUmc.isOriginalTimeStamp(display_index)) ? MFX_FRAMEDATA_ORIGINAL_TIMESTAMP : 0);
            }

            m_implUmc.LockTask(m_task_num);

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
                m_implUmc.PostProcessUserData(display_index);

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

            res = m_implUmc.GetPictureHeader(NULL, m_task_num, m_prev_task_num);

            if (UMC::UMC_ERR_INVALID_STREAM == res)
            {
                return MFX_ERR_UNKNOWN;
            }

            display_index = m_implUmc.GetDisplayIndex();
            
            if (0 <= display_index)
            {
                if (false == m_isDecodedOrder)
                {
                    last_timestamp = GetMfxTimeStamp(m_implUmc.GetCurrDecodedTime(display_index));
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
                    m_task_param[m_task_num].NumThreads = m_implUmc.GetCurrThreadsNum(m_task_num);

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

                    m_implUmc.PostProcessUserData(display_index);

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

    } // end software part
}


mfxStatus VideoDECODEMPEG2::GetStatusReportByIndex(mfxFrameSurface1 *displaySurface, mfxU32 currIdx)
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
        sts = m_implUmc.pack_w.GetStatusReport(m_pStatusReport);
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

mfxFrameSurface1 *VideoDECODEMPEG2::GetOriginalSurface(mfxFrameSurface1 *pSurface)
{
    if (true == m_isOpaqueMemory)
    {
        return m_pCore->GetNativeSurface(pSurface);
    }

    return pSurface;
}

mfxStatus VideoDECODEMPEG2::GetOutputSurface(mfxFrameSurface1 **surface_out, mfxFrameSurface1 *surface_work, UMC::FrameMemID index)
{
    mfxFrameSurface1 *pNativeSurface =  m_FrameAllocator->GetSurface(index, surface_work, &m_vPar);
    
    if (pNativeSurface)
    {
        *surface_out = m_pCore->GetOpaqSurface(pNativeSurface->Data.MemId) ? m_pCore->GetOpaqSurface(pNativeSurface->Data.MemId) : pNativeSurface;
    }
    else
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2::GetStatusReport(mfxFrameSurface1 *displaySurface, UMC::FrameMemID surface_id)
{
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
        sts = m_implUmc.pack_w.GetStatusReport(m_pStatusReport);

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
    VAStatus        surfErr = VA_STATUS_SUCCESS;

#if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)

    sts = va->SyncTask(surface_id, &surfErr);

    STATUS_REPORT_DEBUG_PRINTF("index %d with surfErr: %d (sts:%d)\n", index, surfErr, sts)

    if (sts != UMC::UMC_OK)
        return MFX_ERR_DEVICE_FAILED;
#else
    VASurfaceStatus surfSts = VASurfaceSkipped;

    sts = va->QueryTaskStatus(surface_id, &surfSts, &surfErr);
    if (sts != UMC::UMC_OK)
        return MFX_ERR_DEVICE_FAILED;

#endif // #if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)

    displaySurface->Data.Corrupted = surfErr;

#endif // UMC_VA_LINUX

    return MFX_ERR_NONE;
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

mfxStatus VideoDECODEMPEG2::ConstructFrame(mfxBitstream *in, mfxBitstream *out, mfxFrameSurface1 *surface_work)
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

mfxStatus VideoDECODEMPEG2::ConstructFrameImpl(mfxBitstream *in, mfxBitstream *out, mfxFrameSurface1 *surface_work)
{
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
            if(tail < curr + 7) 
            {
                return MFX_ERR_MORE_DATA;
            }

            mfxU16 CropW = (curr[4] << 4) + ((curr[5] >> 4) & 0xf);
            mfxU16 CropH = ((curr[5] & 0xf) << 8) + curr[6];
            mfxU16 Width = (CropW + 15) & ~0x0f;
            mfxU16 Height = (CropH + 15) & ~0x0f;
            mfxU8  aspect_ratio_information = curr[7] >> 4;

            if (m_aspect_ratio_information != aspect_ratio_information)
            {
                m_aspect_ratio_information = aspect_ratio_information;
                if (!m_first_SH)
                    return MFX_WRN_VIDEO_PARAM_CHANGED;
            }

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
            {
                if (m_InitW >  Width || m_InitH > Height)
                {
                    m_resizing = true;  
                    return MFX_WRN_VIDEO_PARAM_CHANGED;
                }
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

mfxStatus VideoDECODEMPEG2::InternalResetUMC(mfxVideoParam* par, UMC::MediaData* data)
{
    //UMC::VideoDecoderParams vdPar;
    m_vdPar.info.stream_type = data ? UMC::UNDEF_VIDEO : UMC::MPEG2_VIDEO;
    m_vdPar.info.clip_info.width = par->mfx.FrameInfo.Width;
    m_vdPar.info.clip_info.height = par->mfx.FrameInfo.Height;

    mfxU32 frameRateD = par->mfx.FrameInfo.FrameRateExtD;
    mfxU32 frameRateN = par->mfx.FrameInfo.FrameRateExtN;

    if (0 != frameRateD && 0 != frameRateN)
    {
        m_vdPar.info.framerate = (Ipp64f) par->mfx.FrameInfo.FrameRateExtD / par->mfx.FrameInfo.FrameRateExtN;
    }
    else
    {
        m_vdPar.info.framerate = 0;
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

    UMC::VideoProcessingParams postProcessingParams;
    postProcessingParams.m_DeinterlacingMethod = (UMC::DeinterlacingMethod)0;
    postProcessingParams.InterpolationMethod = 1;
    m_PostProcessing.SetParams(&postProcessingParams);

    m_vdPar.pPostProcessing = &m_PostProcessing;
    m_vdPar.numThreads = par->mfx.NumThread;
    m_vdPar.m_pData = data ? data : 0;

    m_pCore->GetVA((mfxHDL*)&m_vdPar.pVideoAccelerator, MFX_MEMTYPE_FROM_DECODE);

    m_implUmc.Reset();

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2::InternalReset(mfxVideoParam* par)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    m_resizing = false;

    eMFXHWType type = MFX_HW_UNKNOWN;
#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
    type = m_pCore->GetHWType();
#endif

    mfxSts = CheckVideoParamDecoders(par, m_pCore->IsExternalFrameAllocator(), type);
    
    if (MFX_ERR_NONE != mfxSts)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (!(par->IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY |
                            MFX_IOPATTERN_IN_SYSTEM_MEMORY |
                            MFX_IOPATTERN_OUT_VIDEO_MEMORY|
                            MFX_IOPATTERN_OUT_SYSTEM_MEMORY |
                            MFX_IOPATTERN_IN_OPAQUE_MEMORY |
                            MFX_IOPATTERN_OUT_OPAQUE_MEMORY)))
       return MFX_ERR_INVALID_VIDEO_PARAM;

    if (m_isInitialized)
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

    mfxSts = InternalResetUMC(par, 0);
    MFX_CHECK_STS(mfxSts);

    //m_InitW = par->mfx.FrameInfo.Width;
    //m_InitH = par->mfx.FrameInfo.Height;
    m_InitPicStruct = par->mfx.FrameInfo.PicStruct;
    m_Protected = par->Protected;

    m_extendedPicStruct = par->mfx.ExtendedPicStruct;

    if (IS_PROTECTION_PAVP_ANY(par->Protected))
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

    if (m_pCore->GetPlatformType() == MFX_PLATFORM_HARDWARE)
    {
        m_NumThreads = 1;
    }
    else
    {
        m_NumThreads = (mfxU16)(par->AsyncDepth ? par->AsyncDepth : (mfxU16)vm_sys_info_get_cpu_num());
    }

    ResetFcState(m_fcState);
    m_vPar = *par;
    memset(&m_fPar, 0, sizeof(m_fPar));

    m_isDecodedOrder = false;

    return MFX_ERR_NONE;
}

#endif //MFX_ENABLE_MPEG2_VIDEO_DECODE
