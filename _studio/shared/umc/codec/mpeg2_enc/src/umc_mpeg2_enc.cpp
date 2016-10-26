//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2002-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_ENCODER)
#if defined(__GNUC__)
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "ipps.h"
#include "umc_mpeg2_enc_defs.h"
#include "vm_time.h"
#include "vm_sys_info.h"
#include "umc_video_brc.h"

#include "mfx_timing.h"
using namespace UMC;

static const Ipp64f ratetab[8]=
  {24000.0/1001.0,24.0,25.0,30000.0/1001.0,30.0,50.0,60000.0/1001.0,60.0};

#if defined (__ICL)
//non-pointer conversion from "unsigned __int64" to "Ipp32s={signed int}" may lose significant bits
//#pragma warning(disable:2259)
#endif

#if defined (WIN32)
#pragma warning (push)
#pragma warning (disable:4351)  // value initialization is intended and disabling this warning recommended by MS: https://msdn.microsoft.com/en-us/en-en/library/1ywe7hcy.aspx
#endif

MPEG2VideoEncoderBase::MPEG2VideoEncoderBase ()
    : picture_coding_type()
    , m_UserData()
    , frame_history()
    , frame_history_index()
    , is_interlaced_queue()
    , encodeInfo()
    , frame_loader()
    , brc()
    , frames()
    , YRefFrame()
    , URefFrame()
    , VRefFrame()
    , YRecFrame()
    , URecFrame()
    , VRecFrame()
    , Y_src()
    , U_src()
    , V_src()
    , MBcountH()
    , MBcountV()
    , MBcount()
    , YFramePitchSrc()
    , UVFramePitchSrc()
    , YFramePitchRef()
    , UVFramePitchRef()
    , YFramePitchRec()
    , UVFramePitchRec()
    , YFrameVSize()
    , UVFrameVSize()
    , YFrameSize()
    , UVFrameSize()
    , YUVFrameSize()
    , aspectRatio_code()
    , frame_rate_code()
    , frame_rate_extension_n()
    , frame_rate_extension_d()
    , picture_structure(MPEG2_FRAME_PICTURE)
    , progressive_frame()
    , top_field_first()     // 1 when !progressive
    , repeat_first_field()
    , ipflag()
    , temporal_reference()
    , closed_gop()
    , pMotionData()
    , MotionDataCount()
    , mp_f_code()
    , curr_frame_dct()
    , curr_frame_pred()
    , curr_intra_vlc_format()
    , curr_scan()
    , intra_dc_precision()  // 8 bit
    , vbv_delay(19887)
    , rc_vbv_max()
    , rc_vbv_min()
    , rc_vbv_fullness()
    , rc_delay()
    , rc_ip_delay()
    , rc_ave_frame_bits()
    , qscale()
    , prsize()
    , prqscale()
    , quantiser_scale_value(-1)
    , q_scale_type()
    , quantiser_scale_code()
    , nLimitedMBMode()
    , bQuantiserChanged()
    , bSceneChanged()
    , bExtendGOP()
    , m_MinFrameSizeBits()
    , m_MinFieldSizeBits()
    , m_FirstGopSize()
    , m_InputBitsPerFrame()
#ifndef UMC_RESTRICTED_CODE_CME
#ifdef M2_USE_CME
    , bMEdone()
#endif 
#endif 
    , rc_weight()
    , rc_tagsize()
    , rc_dev()
    , varThreshold()
    , meanThreshold()
    , sadThreshold()
    , onlyIFrames()
    , B_count()
    , P_distance()
    , GOP_count()
    , m_GOP_Start()
    , m_GOP_Start_tmp()
    , m_FirstFrame()
    , m_PTS(-1)
    , m_DTS(-1)
    , m_bSH()
    , m_bEOS()
    , m_bBwdRef()
    , m_bSkippedMode()
    , block_count()
    , IntraQMatrix()
    , NonIntraQMatrix()
    , _InvIntraQMatrix()
    , _InvNonIntraQMatrix()
    , InvIntraQMatrix()
    , InvNonIntraQMatrix()
    , m_Inited()
    , pMBInfo()
#ifdef MPEG2_USE_DUAL_PRIME
    , dpflag()
#endif
#ifndef UMC_RESTRICTED_CODE_CME
#ifdef M2_USE_CME
    , m_pME()
#endif
#endif
    , threadsAllocated()
    , threadSpec()
    , threads()
    , vlcTableB5c_e()
    , vlcTableB15()
    , out_pointer()
    , output_buffer_size()
    , mEncodedSize()
    , thread_buffer_size()
    , block_offset_frm_src()
    , block_offset_fld_src()
    , block_offset_frm_ref()
    , block_offset_fld_ref()
    , block_offset_frm_rec()
    , block_offset_fld_rec()
    , frm_dct_step()
    , fld_dct_step()
    , frm_diff_off()
    , fld_diff_off()
    , func_getdiff_frame_c()
    , func_getdiff_field_c()
    , func_getdiff_frame_nv12_c()
    , func_getdiff_field_nv12_c()
    , func_getdiffB_frame_c()
    , func_getdiffB_field_c()
    , func_getdiffB_frame_nv12_c()
    , func_getdiffB_field_nv12_c()
    , func_mc_frame_c()
    , func_mc_field_c()
    , func_mcB_frame_c()
    , func_mcB_field_c()
    , func_mc_frame_nv12_c()
    , func_mc_field_nv12_c()
    , func_mcB_frame_nv12_c()
    , func_mcB_field_nv12_c()
    , BlkWidth_c()
    , BlkStride_c()
    , BlkHeight_c()
    , chroma_fld_flag()
    , curr_field()
    , second_field()
    , DC_Tbl()
    , cpu_freq()
    , motion_estimation_time()
#ifdef MPEG2_ENC_DEBUG
    , mpeg2_debug()
#endif
{
  encodeInfo.lFlags = FLAG_VENC_REORDER;
}

MPEG2VideoEncoderBase::~MPEG2VideoEncoderBase ()
{
  Close();
}

#ifndef _NEW_THREADING
static Ipp32u VM_THREAD_CALLCONVENTION ThreadWorkingRoutine(void* ptr)
{
  MPEG2VideoEncoderBase::threadInfo* th = (MPEG2VideoEncoderBase::threadInfo*)ptr;

  MFX::AutoTimer timer;
  timer.Start("MPEG2Enc_sleep");

  for (;;) {
    // wait for start
    if (VM_OK != vm_event_wait(&th->start_event)) {
      vm_debug_trace(VM_DEBUG_PROGRESS, VM_STRING("ThreadWorkingRoutine result wait start_event\n"));
    }

    if (VM_TIMEOUT != vm_event_timed_wait(&th->quit_event, 0)) {
      break;
    }

    timer.Stop(0);

    th->m_lpOwner->encodePicture(th->numTh);

    if (VM_OK != vm_event_signal(&th->stop_event)) {
      vm_debug_trace(VM_DEBUG_PROGRESS, VM_STRING("ThreadWorkingRoutine result signal stop_event\n"));
    }

    //vm_time_sleep(0);
  }

  return 0;
}
#endif
void GetBlockOffsetFrm (Ipp32s *pBlockOffset, Ipp32s pitch)
{
  pBlockOffset[0]  = 0;
  pBlockOffset[1]  = 8;
  pBlockOffset[2]  = 8*pitch;
  pBlockOffset[3]  = 8*pitch + 8;
  pBlockOffset[4]  = 0;
  pBlockOffset[5]  = 0;
  pBlockOffset[6]  = 8*pitch;
  pBlockOffset[7]  = 8*pitch;
  pBlockOffset[8]  = 8;
  pBlockOffset[9]  = 8;
  pBlockOffset[10] = 8*pitch + 8;
  pBlockOffset[11] = 8*pitch + 8;
}
void GetBlockOffsetFld (Ipp32s *pBlockOffset, Ipp32s pitch)
{
  pBlockOffset[0]  = 0;
  pBlockOffset[1]  = 8;
  pBlockOffset[2]  = pitch;
  pBlockOffset[3]  = pitch + 8;
  pBlockOffset[4]  = 0;
  pBlockOffset[5]  = 0;
  pBlockOffset[6]  = pitch;
  pBlockOffset[7]  = pitch;
  pBlockOffset[8]  = 8;
  pBlockOffset[9]  = 8;
  pBlockOffset[10] = pitch + 8;
  pBlockOffset[11] = pitch + 8;
}



Status MPEG2VideoEncoderBase::Init(BaseCodecParams *params)
{
  Ipp32s i, j;
  Ipp32s sizeChanged = !m_Inited;

  VideoEncoderParams *VideoParams = DynamicCast<VideoEncoderParams>(params);
  if(VideoParams != 0) { // at least VideoEncoder parameters
    MPEG2EncoderParams *info = DynamicCast<MPEG2EncoderParams>(params);
    if (VideoParams->info.clip_info.width <= 0 || VideoParams->info.clip_info.height <= 0) {
      return UMC_ERR_INVALID_PARAMS;
    }
    // trying to support size change
    sizeChanged = sizeChanged ||
      (VideoParams->info.clip_info.width != encodeInfo.info.clip_info.width) ||
      (VideoParams->info.clip_info.height != encodeInfo.info.clip_info.height);
    // good to have more checks before overwriting
    if(NULL != info) { // Mpeg2 encoder parameters
      // trying to support chroma_format change
      sizeChanged = sizeChanged ||
        (info->info.color_format != encodeInfo.info.color_format);
      encodeInfo = *info;
    } else
      *(VideoEncoderParams*) &encodeInfo = *VideoParams;
  } else if (NULL != params) {
    *(BaseCodecParams*) &encodeInfo = *params;
  } else if (!m_Inited) { // 0 when initialized means Reset
    return UMC_ERR_NULL_PTR;
  }

  if(params == NULL) { // Reset or SetParams(0)
    m_FirstFrame = 1;
    B_count      = 0;
    GOP_count    = 0;
    closed_gop = 1;
    return UMC_OK;
  }

  BaseCodec::Init(params); // to prepare allocator
  SetAspectRatio(encodeInfo.info.aspect_ratio_width, encodeInfo.info.aspect_ratio_height);

  // FrameRate to code and extensions and back to exact value
  if(UMC_OK != SetFrameRate(encodeInfo.info.framerate, encodeInfo.mpeg1))
  {
    vm_debug_trace(VM_DEBUG_WARNING, VM_STRING("invalid frame rate set to 30 fps\n"));
    frame_rate_code = 5;
    frame_rate_extension_n = 0;
    frame_rate_extension_d = 0;
    encodeInfo.info.framerate = 30;
  }
  encodeInfo.info.framerate = ratetab[frame_rate_code - 1]*(frame_rate_extension_n+1)/(frame_rate_extension_d+1);
  encodeInfo.Profile_and_Level_Checks();
  encodeInfo.RelationChecks();

#undef  VM_DEBUG_FUNC_NAME
#define VM_DEBUG_FUNC_NAME VM_STRING("Init")
  vm_debug_trace_i(VM_DEBUG_INFO, encodeInfo.numThreads);
  vm_debug_trace_i(VM_DEBUG_INFO, encodeInfo.info.clip_info.width);
  vm_debug_trace_i(VM_DEBUG_INFO, encodeInfo.info.clip_info.height);
  vm_debug_trace_f(VM_DEBUG_INFO, encodeInfo.info.framerate);
  vm_debug_trace_i(VM_DEBUG_INFO, encodeInfo.info.bitrate);
  vm_debug_trace_i(VM_DEBUG_INFO, encodeInfo.gopSize);
  vm_debug_trace_i(VM_DEBUG_INFO, encodeInfo.IPDistance);
  vm_debug_trace_i(VM_DEBUG_INFO, encodeInfo.profile);
  vm_debug_trace_i(VM_DEBUG_INFO, encodeInfo.level);
  vm_debug_trace_i(VM_DEBUG_INFO, encodeInfo.info.aspect_ratio_width);
  vm_debug_trace_i(VM_DEBUG_INFO, encodeInfo.info.aspect_ratio_height);
  vm_debug_trace_i(VM_DEBUG_INFO, encodeInfo.video_format);
  //vm_debug_trace_i(VM_DEBUG_INFO, encodeInfo.progressive_frame);
  vm_debug_trace_i(VM_DEBUG_INFO, encodeInfo.info.interlace_type);
  //vm_debug_trace_i(VM_DEBUG_INFO, encodeInfo.top_field_first);
  vm_debug_trace_i(VM_DEBUG_INFO, encodeInfo.FieldPicture);
  vm_debug_trace_i(VM_DEBUG_INFO, encodeInfo.info.color_format);
  vm_debug_trace_i(VM_DEBUG_INFO, encodeInfo.frame_pred_frame_dct[0]);
  vm_debug_trace_i(VM_DEBUG_INFO, encodeInfo.frame_pred_frame_dct[1]);
  vm_debug_trace_i(VM_DEBUG_INFO, encodeInfo.frame_pred_frame_dct[2]);
#undef  VM_DEBUG_FUNC_NAME
#define VM_DEBUG_FUNC_NAME NULL

  // allocate ranges, compute search ranges and f_code
  if (pMotionData && MotionDataCount < encodeInfo.IPDistance) {
    MP2_FREE(pMotionData);
    pMotionData = 0;
    MotionDataCount = 0;
  }
  if (pMotionData == 0) {
    pMotionData = MP2_ALLOC(MotionData, encodeInfo.IPDistance);
    if( !pMotionData )
      return UMC_ERR_ALLOC;
    MotionDataCount = encodeInfo.IPDistance;
  }
  for (i = 0; i < encodeInfo.IPDistance; i++) {
    for (j = 0; j < 2; j++) { // x/y
      if (i==0) { // P
        pMotionData[i].searchRange[0][j] = encodeInfo.rangeP[j];
        RANGE_TO_F_CODE(pMotionData[i].searchRange[0][j], pMotionData[i].f_code[0][j]);
        pMotionData[i].searchRange[0][j] = 4 << pMotionData[i].f_code[0][j];
        pMotionData[i].f_code[1][j] = 15;
        pMotionData[i].searchRange[1][j] = 0;
      } else { // B
        pMotionData[i].searchRange[0][j] =
          (encodeInfo.rangeB[0][0]*(encodeInfo.IPDistance-i) + encodeInfo.rangeB[1][0]*(i-1))/(encodeInfo.IPDistance-1);
        pMotionData[i].searchRange[1][j] =
          (encodeInfo.rangeB[1][0]*(encodeInfo.IPDistance-i) + encodeInfo.rangeB[0][0]*(i-1))/(encodeInfo.IPDistance-1);
        RANGE_TO_F_CODE(pMotionData[i].searchRange[0][j], pMotionData[i].f_code[0][j]);
        pMotionData[i].searchRange[0][j] = 4 << pMotionData[i].f_code[0][j];
        RANGE_TO_F_CODE(pMotionData[i].searchRange[1][j], pMotionData[i].f_code[1][j]);
        pMotionData[i].searchRange[1][j] = 4 << pMotionData[i].f_code[1][j];
      }
    }
  }
  progressive_frame = (encodeInfo.info.interlace_type == PROGRESSIVE) ?  1:0;
  if (!encodeInfo.FieldPicture || encodeInfo.info.interlace_type == PROGRESSIVE)
  {
    picture_structure = MPEG2_FRAME_PICTURE;
  }
  else
  {
    picture_structure = (encodeInfo.info.interlace_type == INTERLEAVED_BOTTOM_FIELD_FIRST)? MPEG2_BOTTOM_FIELD : MPEG2_TOP_FIELD;
  }

  if (encodeInfo.info.interlace_type == PROGRESSIVE ||
      encodeInfo.info.interlace_type == INTERLEAVED_BOTTOM_FIELD_FIRST)
    top_field_first = 0;
  else
    top_field_first = 1;

  if(!m_Inited) { // only first call
    DC_Tbl[0] = Y_DC_Tbl;
    DC_Tbl[1] = Cr_DC_Tbl;
    DC_Tbl[2] = Cr_DC_Tbl;
    ippiCreateRLEncodeTable(Table15, &vlcTableB15);
    ippiCreateRLEncodeTable(dct_coeff_next_RL, &vlcTableB5c_e);
  }

  // chroma format dependent
  BlkWidth_c = (encodeInfo.info.color_format != YUV444) ? 8 : 16;
  BlkStride_c = 16;//BlkWidth_c;
  BlkHeight_c = (encodeInfo.info.color_format == YUV420 || encodeInfo.info.color_format == NV12) ? 8 : 16;
  chroma_fld_flag = (encodeInfo.info.color_format == YUV420 || encodeInfo.info.color_format == NV12) ? 0 : 1;

  switch (encodeInfo.info.color_format) {

  case YUV420:
    block_count = 6;
    func_getdiff_frame_c = ippiGetDiff8x8_8u16s_C1;
    func_getdiff_field_c = ippiGetDiff8x4_8u16s_C1;
    func_getdiffB_frame_c = ippiGetDiff8x8B_8u16s_C1;
    func_getdiffB_field_c = ippiGetDiff8x4B_8u16s_C1;
    func_mc_frame_c = ippiMC8x8_8u_C1;
    func_mc_field_c = ippiMC8x4_8u_C1;
    func_mcB_frame_c = ippiMC8x8B_8u_C1;
    func_mcB_field_c = ippiMC8x4B_8u_C1;
    break;

  case YUV422:
    block_count = 8;
    func_getdiff_frame_c = ippiGetDiff8x16_8u16s_C1;
    func_getdiff_field_c = ippiGetDiff8x8_8u16s_C1;
    func_getdiffB_frame_c = ippiGetDiff8x16B_8u16s_C1;
    func_getdiffB_field_c = ippiGetDiff8x8B_8u16s_C1;
    func_mc_frame_c = ippiMC8x16_8u_C1;
    func_mc_field_c = ippiMC8x8_8u_C1;
    func_mcB_frame_c = ippiMC8x16B_8u_C1;
    func_mcB_field_c = ippiMC8x8B_8u_C1;
    break;

  case YUV444:
    block_count = 12;
    func_getdiff_frame_c = ippiGetDiff16x16_8u16s_C1;
    func_getdiff_field_c = ippiGetDiff16x8_8u16s_C1;
    func_getdiffB_frame_c = ippiGetDiff16x16B_8u16s_C1;
    func_getdiffB_field_c = ippiGetDiff16x8B_8u16s_C1;
    func_mc_frame_c = ippiMC16x16_8u_C1;
    func_mc_field_c = ippiMC16x8_8u_C1;
    func_mcB_frame_c = ippiMC16x16B_8u_C1;
    func_mcB_field_c = ippiMC16x8B_8u_C1;
    break;

  case NV12:
    block_count = 6;

#ifdef IPP_NV12

    func_getdiff_frame_nv12_c = ippiGetDiff8x8_8u16s_C2P2; // NV12 func!!!
    func_getdiff_field_nv12_c = ippiGetDiff8x4_8u16s_C2P2; // NV12 func!!!
    func_getdiffB_frame_nv12_c = ippiGetDiff8x8B_8u16s_C2P2; // NV12 func!!!
    func_getdiffB_field_nv12_c = ippiGetDiff8x4B_8u16s_C2P2; // NV12 func!!!
    func_mc_frame_nv12_c = ippiMC8x8_16s8u_P2C2R;// NV12 func!!!
    func_mc_field_nv12_c = ippiMC8x4_16s8u_P2C2R;// NV12 func!!!
    func_mcB_frame_nv12_c = ippiMC8x8B_16s8u_P2C2R; // NV12 func!!!
    func_mcB_field_nv12_c = ippiMC8x4B_16s8u_P2C2R; // NV12 func!!!
#else
    func_getdiff_frame_c = tmp_ippiGetDiff8x8_8u16s_C2P2; // NV12 func!!!
    func_getdiff_field_c = tmp_ippiGetDiff8x4_8u16s_C2P2; // NV12 func!!!
    func_getdiffB_frame_c = tmp_ippiGetDiff8x8B_8u16s_C2P2; // NV12 func!!!
    func_getdiffB_field_c = tmp_ippiGetDiff8x4B_8u16s_C2P2; // NV12 func!!!
    func_mc_frame_c = tmp_ippiMC8x8_8u_C2P2; // NV12 func!!!
    func_mc_field_c = tmp_ippiMC8x4_8u_C2P2; // NV12 func!!!
    func_mcB_frame_c = tmp_ippiMC8x8B_8u_C2P2; // NV12 func!!!
    func_mcB_field_c = tmp_ippiMC8x4B_8u_C2P2; // NV12 func!!!
#endif
    break;
  default:
    break;
  }

  for (i = 0; i < 4; i++) {
    frm_dct_step[i] = BlkStride_l*sizeof(Ipp16s);
    fld_dct_step[i] = 2*BlkStride_l*sizeof(Ipp16s);
  }
  for (i = 4; i < block_count; i++) {
    frm_dct_step[i] = (Ipp32s)(BlkStride_c*sizeof(Ipp16s));
    fld_dct_step[i] = (Ipp32s)(BlkStride_c*sizeof(Ipp16s) << chroma_fld_flag);
  }

  frm_diff_off[0] = 0;
  frm_diff_off[1] = 8;
  frm_diff_off[2] = BlkStride_l*8;
  frm_diff_off[3] = BlkStride_l*8 + 8;
  frm_diff_off[4] = OFF_U;
  frm_diff_off[5] = OFF_V;
  frm_diff_off[6] = OFF_U + BlkStride_c*8;
  frm_diff_off[7] = OFF_V + BlkStride_c*8;
  frm_diff_off[8] = OFF_U + 8;
  frm_diff_off[9] = OFF_V + 8;
  frm_diff_off[10] = OFF_U + BlkStride_c*8 + 8;
  frm_diff_off[11] = OFF_V + BlkStride_c*8 + 8;

  fld_diff_off[0] = 0;
  fld_diff_off[1] = 8;
  fld_diff_off[2] = BlkStride_l;
  fld_diff_off[3] = BlkStride_l + 8;
  fld_diff_off[4] = OFF_U;
  fld_diff_off[5] = OFF_V;
  fld_diff_off[6] = OFF_U + BlkStride_c;
  fld_diff_off[7] = OFF_V + BlkStride_c;
  fld_diff_off[8] = OFF_U + 8;
  fld_diff_off[9] = OFF_V + 8;
  fld_diff_off[10] = OFF_U + BlkStride_c + 8;
  fld_diff_off[11] = OFF_V + BlkStride_c + 8;

  // frame size dependent
  // macroblock aligned size
  MBcountH = (encodeInfo.info.clip_info.width + 15)/16;
  MBcountV = (encodeInfo.info.clip_info.height + 15)/16;
  if(encodeInfo.info.interlace_type != PROGRESSIVE) {
    MBcountV = (MBcountV + 1) & ~1;
  }
  MBcount = MBcountH * MBcountV;

  YFramePitchSrc = YFramePitchRef = YFramePitchRec = MBcountH * 16;
  YFrameVSize = MBcountV * 16;
  UVFramePitchSrc = UVFramePitchRef = UVFramePitchRec = (encodeInfo.info.color_format == YUV444) ? YFramePitchSrc : (YFramePitchSrc >> 1);
  UVFrameVSize = (encodeInfo.info.color_format == YUV420 || encodeInfo.info.color_format == NV12) ? (YFrameVSize >> 1) : YFrameVSize;
  YFrameSize = YFramePitchSrc*YFrameVSize;
  UVFrameSize = UVFramePitchSrc*UVFrameVSize;
  YUVFrameSize = YFrameSize + 2*UVFrameSize;
  encodeInfo.m_SuggestedInputSize = YUVFrameSize;

  m_MinFrameSizeBits[0] = 10 * block_count * MBcount + 140 + (YFramePitchSrc/16)*32;
  m_MinFrameSizeBits[1] = 1*MBcount + 140 + (YFramePitchSrc/16)*32;
  m_MinFrameSizeBits[2] = 1*MBcount + 140 + (YFramePitchSrc/16)*32;
  m_MinFieldSizeBits[0] = 12 * block_count * (MBcount/2) + 140 + (YFramePitchSrc/2/16)*32;
  m_MinFieldSizeBits[1] = 1*MBcount/2 + 140 + (YFramePitchSrc/2/16)*32;
  m_MinFieldSizeBits[2] = 1*MBcount/2 + 140 + (YFramePitchSrc/2/16)*32;

  GetBlockOffsetFrm (block_offset_frm_src, YFramePitchSrc);
  GetBlockOffsetFld (block_offset_fld_src, YFramePitchSrc);

  GetBlockOffsetFrm (block_offset_frm_ref, YFramePitchRef);
  GetBlockOffsetFld (block_offset_fld_ref, YFramePitchRef);

  GetBlockOffsetFrm (block_offset_frm_rec, YFramePitchRec);
  GetBlockOffsetFld (block_offset_fld_rec, YFramePitchRec);

  m_bSkippedMode = 0;

  // verify bitrate
  if (!brc)
  {
    Ipp32s BitRate = encodeInfo.info.bitrate;
    Ipp64f pixrate = encodeInfo.info.framerate * YUVFrameSize * 8;
    if((Ipp64f)encodeInfo.info.bitrate > pixrate)
      BitRate = (Ipp32s)(pixrate); // too high
    if(BitRate != 0 && (Ipp64f)encodeInfo.info.bitrate < pixrate/500)
      BitRate = (Ipp32s)(pixrate/500); // too low
    if ((Ipp32s)encodeInfo.info.bitrate != BitRate) {
      vm_debug_trace1(VM_DEBUG_WARNING, VM_STRING("BitRate value fixed to %d\n"), (BitRate+399)/400*400);
    }
    encodeInfo.info.bitrate = (BitRate+399)/400*400; // 400 bps step
  }

  if(sizeChanged) {
    if (pMBInfo != 0)
      MP2_FREE(pMBInfo);
    pMBInfo = MP2_ALLOC(MBInfo, MBcount);
    if (pMBInfo == 0) {
      return UMC_ERR_ALLOC;
    }
    if(UMC_OK != CreateFrameBuffer()) // deletes if neccessary
      return UMC_ERR_ALLOC;
  }
  ippsZero_8u((Ipp8u*)pMBInfo, (Ipp32s)(sizeof(MBInfo)*MBcount));

#ifndef _NEW_THREADING

  // threads preparation
  if (encodeInfo.numThreads != 1) {
    if (encodeInfo.numThreads == 0)
      encodeInfo.numThreads = vm_sys_info_get_cpu_num();
    if (encodeInfo.numThreads < 1) {
      encodeInfo.numThreads = 1;
    }
    Ipp32s max_threads = MBcountV;
    if (encodeInfo.info.interlace_type != PROGRESSIVE) {
      max_threads = MBcountV/2;
    }
    if (encodeInfo.numThreads > max_threads) {
      encodeInfo.numThreads = max_threads;
    }
  }

  // add threads if was fewer
  if(threadsAllocated < encodeInfo.numThreads) {
    threadSpecificData* cur_threadSpec = threadSpec;
    threadSpec = MP2_ALLOC(threadSpecificData, encodeInfo.numThreads);
    // copy existing
    if(threadsAllocated) {
      ippsCopy_8u((Ipp8u*)cur_threadSpec, (Ipp8u*)threadSpec,
        (Ipp32s)(threadsAllocated*sizeof(threadSpecificData)));
      MP2_FREE(cur_threadSpec);
    }

    if(encodeInfo.numThreads > 1) {
      threadInfo** cur_threads = threads;
      threads = MP2_ALLOC(threadInfo*, encodeInfo.numThreads - 1);
      if(threadsAllocated > 1) {
        for (i = 0; i < threadsAllocated - 1; i++)
          threads[i] = cur_threads[i];
        MP2_FREE(cur_threads);
      }
    }

    // init newly allocated
    for (i = threadsAllocated; i < encodeInfo.numThreads; i++) {
      threadSpec[i].pDst = 0;
      threadSpec[i].dstSz = 0;
      threadSpec[i].pDiff   = MP2_ALLOC(Ipp16s, 3*256);
      threadSpec[i].pDiff1  = MP2_ALLOC(Ipp16s, 3*256);
      threadSpec[i].pMBlock = MP2_ALLOC(Ipp16s, 3*256);
      threadSpec[i].me_matrix_size = 0;
      threadSpec[i].me_matrix_buff = NULL;
      threadSpec[i].me_matrix_id   = 0;
#ifdef MPEG2_USE_DUAL_PRIME
      threadSpec[i].fld_vec_count  = 0;
#endif //MPEG2_USE_DUAL_PRIME
      if(i>0) {
        threads[i-1] = MP2_ALLOC(threadInfo, 1);
        vm_event_set_invalid(&threads[i-1]->start_event);
        if (VM_OK != vm_event_init(&threads[i-1]->start_event, 0, 0))
          return UMC_ERR_ALLOC;
        vm_event_set_invalid(&threads[i-1]->stop_event);
        if (VM_OK != vm_event_init(&threads[i-1]->stop_event, 0, 0))
          return UMC_ERR_ALLOC;
        vm_event_set_invalid(&threads[i-1]->quit_event);
        if (VM_OK != vm_event_init(&threads[i-1]->quit_event, 0, 0))
          return UMC_ERR_ALLOC;
        vm_thread_set_invalid(&threads[i-1]->thread);
        if (0 == vm_thread_create(&threads[i-1]->thread,
                                  ThreadWorkingRoutine,
                                  threads[i-1])) {
          return UMC_ERR_ALLOC;
        }
        threads[i-1]->numTh = i;
        threads[i-1]->m_lpOwner = this;
      }
    }
    threadsAllocated = encodeInfo.numThreads;
  }

  Ipp32s mb_shift = (encodeInfo.info.interlace_type == PROGRESSIVE) ? 4 : 5;
  for (i = 0; i < encodeInfo.numThreads; i++) {
    threadSpec[i].start_row = ((16*MBcountV>>mb_shift)*i/encodeInfo.numThreads) << mb_shift;
  }
#endif // _NEW_THREADING
  // quantizer matrices
  IntraQMatrix = encodeInfo.IntraQMatrix;
  NonIntraQMatrix = (encodeInfo.CustomNonIntraQMatrix) ? encodeInfo.NonIntraQMatrix : NULL;

  if (encodeInfo.CustomIntraQMatrix) {
    InvIntraQMatrix = _InvIntraQMatrix;
    for (i = 0; i < 64; i++) {
      InvIntraQMatrix[i] = 1.f / (Ipp32f)IntraQMatrix[i];
    }
  } else {
    InvIntraQMatrix = NULL;
  }

  if (encodeInfo.CustomNonIntraQMatrix) {
    InvNonIntraQMatrix = _InvNonIntraQMatrix;
    for (i = 0; i < 64; i++) {
      InvNonIntraQMatrix[i] = 1.f / (Ipp32f)NonIntraQMatrix[i];
    }
  } else {
    InvNonIntraQMatrix = NULL;
  }

  onlyIFrames = 0;
  // field pictures use IP frames, so reconstruction is required for them
  if (encodeInfo.gopSize == 1 && encodeInfo.IPDistance == 1 && !encodeInfo.FieldPicture) {
    onlyIFrames = 1;
  }

  m_FirstGopSize = (encodeInfo.gopSize - 1)/encodeInfo.IPDistance * encodeInfo.IPDistance + 1;
  // initialize rate control
  if (!brc)
    InitRateControl(encodeInfo.info.bitrate);
  else {
    VideoBrcParams brcParams;
    brc->GetParams(&brcParams);
//    encodeInfo.info.bitrate = brcParams.targetBitrate;
    encodeInfo.info.bitrate = brcParams.maxBitrate;
    encodeInfo.VBV_BufferSize = (brcParams.HRDBufferSizeBytes * 8 + 16383) / 16384;
    vbv_delay = (encodeInfo.info.bitrate)?(Ipp32s)(brcParams.HRDInitialDelayBytes * 8 *90000.0/encodeInfo.info.bitrate):0; // bits to clocks
  }
  m_InputBitsPerFrame = (Ipp32s)(encodeInfo.info.bitrate / encodeInfo.info.framerate);

 #ifndef _NEW_THREADING
  // check/allocate  output buffers for each thread, exept first - it uses dst
  for (i = 1; i < encodeInfo.numThreads; i++) {
    Ipp32s reqSz = encodeInfo.VBV_BufferSize * 16384 / 8; // "16 kbit units" to bytes
    if (threadSpec[i].dstSz < reqSz) {
      if (threadSpec[i].dstSz > 0) {
        MP2_FREE(threadSpec[i].pDst);
        threadSpec[i].dstSz = 0;
      }
      threadSpec[i].pDst = MP2_ALLOC(Ipp8u, reqSz);
      if (!threadSpec[i].pDst)
        return UMC_ERR_ALLOC;
      threadSpec[i].dstSz = reqSz;
    }
  }
#endif //_NEW_THREADING

  // frames list
  m_GOP_Start  = 0;
  P_distance   = encodeInfo.IPDistance;
  B_count      = 0;
  GOP_count    = 0;
  m_FirstFrame = 1;
  closed_gop   = 1;
  m_Inited = 1;

  cpu_freq = 0;
  motion_estimation_time = 1e-7;
#ifdef MPEG2_DEBUG_CODE
  cpu_freq = (Ipp64f)(Ipp64s)(GET_FREQUENCY);
  motion_estimation_time = 0;
#endif
#ifdef MPEG2_ENC_DEBUG
  mpeg2_debug = new MPEG2EncDebug();
  mpeg2_debug->Init(this);
#endif
  return UMC_OK;
}

Status MPEG2VideoEncoderBase::GetInfo(BaseCodecParams *Params)
{
  MPEG2EncoderParams *MPEG2Params = DynamicCast<MPEG2EncoderParams>(Params);
  encodeInfo.qualityMeasure = 100 - (qscale[0]+qscale[1]+qscale[2])*100/(3*112); // rough
  encodeInfo.info.stream_type = MPEG2_VIDEO;
  if (NULL != MPEG2Params) {
    *MPEG2Params = encodeInfo;
  } else {
    VideoEncoderParams *VideoParams = DynamicCast<VideoEncoderParams>(Params);
    if (NULL != VideoParams) {
      *VideoParams = *(VideoEncoderParams*)&encodeInfo;
    } else if (NULL != Params){
      *Params = *(BaseCodecParams*)&encodeInfo;
    } else
      return UMC_ERR_NULL_PTR;
  }
  return UMC_OK;
}

Status MPEG2VideoEncoderBase::Close()
{

  if (!threadSpec) {
    return UMC_ERR_NULL_PTR;
  }

#ifndef _NEW_THREADING

  Ipp32s i;

  if (threadsAllocated) {
    // let all threads to exit
    if (threadsAllocated > 1) {
      for (i = 0; i < threadsAllocated - 1; i++) {
        vm_event_signal(&threads[i]->quit_event);
        vm_event_signal(&threads[i]->start_event);
      }

      for (i = 0; i < threadsAllocated - 1; i++)
      {
        if (&threads[i]->thread) {
          vm_thread_close(&threads[i]->thread);
          vm_thread_set_invalid(&threads[i]->thread);
        }
        if (vm_event_is_valid(&threads[i]->start_event)) {
          vm_event_destroy(&threads[i]->start_event);
          vm_event_set_invalid(&threads[i]->start_event);
        }
        if (vm_event_is_valid(&threads[i]->stop_event)) {
          vm_event_destroy(&threads[i]->stop_event);
          vm_event_set_invalid(&threads[i]->stop_event);
        }
        if (vm_event_is_valid(&threads[i]->quit_event)) {
          vm_event_destroy(&threads[i]->quit_event);
          vm_event_set_invalid(&threads[i]->quit_event);
        }
        MP2_FREE(threads[i]);
      }

      MP2_FREE(threads);
    }

    for(i = 0; i < threadsAllocated; i++)
    {
      if (threadSpec[i].dstSz > 0)
      {
        MP2_FREE(threadSpec[i].pDst);
        threadSpec[i].dstSz = 0;
        threadSpec[i].pDst = 0;
      }
      if(threadSpec[i].pDiff)
      {
        MP2_FREE(threadSpec[i].pDiff);
        threadSpec[i].pDiff = NULL;
      }
      if(threadSpec[i].pDiff1)
      {
        MP2_FREE(threadSpec[i].pDiff1);
        threadSpec[i].pDiff1 = NULL;
      }
      if(threadSpec[i].pMBlock)
      {
        MP2_FREE(threadSpec[i].pMBlock);
        threadSpec[i].pMBlock = NULL;
      }
      if (threadSpec[i].me_matrix_buff) {
        MP2_FREE(threadSpec[i].me_matrix_buff);
        threadSpec[i].me_matrix_buff = NULL;
      }
    }

    MP2_FREE(threadSpec);
    threadsAllocated = 0;
  }
#endif

  if(pMBInfo)
  {
    MP2_FREE(pMBInfo);
    pMBInfo = NULL;
  }
  if (pMotionData) {
    MP2_FREE(pMotionData);
    pMotionData = NULL;
  }
  MotionDataCount = 0;

  if(vlcTableB15)
  {
    ippiHuffmanTableFree_32s(vlcTableB15);
    vlcTableB15 = NULL;
  }
  if(vlcTableB5c_e)
  {
    ippiHuffmanTableFree_32s(vlcTableB5c_e);
    vlcTableB5c_e = NULL;
  }

  DeleteFrameBuffer();

  if (frame_loader != NULL) {
    delete frame_loader;
    frame_loader = NULL;
  }

#ifndef UMC_RESTRICTED_CODE_CME
#ifdef M2_USE_CME
  if (m_pME != 0) {
    delete m_pME ;
    m_pME = 0;
  }
#endif // M2_USE_CME
#endif // UMC_RESTRICTED_CODE_CME

  m_Inited = 0;

  VideoEncoder::Close();
  return UMC_OK;
}

// Get source frame size (summary size of Y, U and V frames)
Ipp32s MPEG2VideoEncoderBase::GetYUVFrameSize()
{
  Ipp32s srcUVFrameHSize = (encodeInfo.info.color_format == YUV444) ?
    encodeInfo.info.clip_info.width : (encodeInfo.info.clip_info.width >> 1);
  Ipp32s srcUVFrameVSize = (encodeInfo.info.color_format == YUV420) ?
    (encodeInfo.info.clip_info.height >> 1) : encodeInfo.info.clip_info.height;
  return encodeInfo.info.clip_info.width*encodeInfo.info.clip_info.height +
    2*srcUVFrameHSize*srcUVFrameVSize;
}

// THIS METHOD IS TO BE DELETED!

// Get pointer to internal encoder memory, where
// next YUV frame can be stored before passing
// this frame to encode function
VideoData* MPEG2VideoEncoderBase::GetNextYUVPointer()
{
  Status ret;
  ret = frames.buf_aux->Lock();
  if(ret!=UMC_OK)
    return 0;
  frames.buf_aux->Unlock();
  return frames.buf_aux;
}

// Encode reordered frames (B frames following the corresponding I/P frames).
// pict_type must be supplied (I_PICTURE, P_PICTURE, B_PICTURE).
// No buffering because the frames are already reordered.
Status MPEG2VideoEncoderBase::EncodeFrameReordered()
{
//  vm_tick t_start;

  //if (frames.curenc == NULL) {
  //  return UMC_ERR_NULL_PTR;
  //}

  // prepare out buffers
  mEncodedSize = 0;
  if(output_buffer_size*8 < rc_ave_frame_bits*2) {
    return UMC_ERR_NOT_ENOUGH_BUFFER;
  }

  ipflag = 0;
//  t_start = GET_TICKS;

  if (picture_structure == MPEG2_FRAME_PICTURE) {
    curr_field = 0;
    second_field = 0;
    PutPicture();
  } else {
    Ipp8u *pSrc[3] = {Y_src, U_src, V_src};
    MBInfo *pMBInfo0 = pMBInfo;
//    Ipp64s field_endpos = 0;
    Ipp32s ifield;

    YFrameVSize >>= 1;
    UVFrameVSize >>= 1;
    MBcountV >>= 1;

    YFramePitchSrc<<=1;
    UVFramePitchSrc<<=1;
    YFramePitchRef<<=1;
    UVFramePitchRef<<=1;
    YFramePitchRec<<=1;
    UVFramePitchRec<<=1;


    for (ifield = 0; ifield < 2; ifield++) {
      picture_structure = (ifield != top_field_first) ? MPEG2_TOP_FIELD : MPEG2_BOTTOM_FIELD;
      curr_field   = (picture_structure == MPEG2_BOTTOM_FIELD) ? 1 :0;
      second_field = ifield;
      if (second_field && picture_coding_type == MPEG2_I_PICTURE) {
        ipflag = 1;
        picture_coding_type = MPEG2_P_PICTURE;
      }

      if (picture_structure == MPEG2_TOP_FIELD) {
        Y_src = pSrc[0];
        U_src = pSrc[1];
        V_src = pSrc[2];
        if (picture_coding_type == MPEG2_P_PICTURE && ifield) {
          YRefFrame[1][0] = YRefFrame[1][1];
          URefFrame[1][0] = URefFrame[1][1];
          VRefFrame[1][0] = VRefFrame[1][1];
          YRecFrame[1][0] = YRecFrame[1][1];
          URecFrame[1][0] = URecFrame[1][1];
          VRecFrame[1][0] = VRecFrame[1][1];
        }
        pMBInfo = pMBInfo0;
      } else {
        Y_src = pSrc[0] + (YFramePitchSrc >> 1);
        if (encodeInfo.info.color_format == NV12) {
          U_src = pSrc[1] + UVFramePitchSrc;
          V_src = pSrc[2] + UVFramePitchSrc;
        } else {
          U_src = pSrc[1] + (UVFramePitchSrc >> 1);
          V_src = pSrc[2] + (UVFramePitchSrc >> 1);
        }
        if (picture_coding_type == MPEG2_P_PICTURE && ifield) {
          YRefFrame[0][0] = YRefFrame[0][1];
          URefFrame[0][0] = URefFrame[0][1];
          VRefFrame[0][0] = VRefFrame[0][1];
          YRecFrame[0][0] = YRecFrame[0][1];
          URecFrame[0][0] = URecFrame[0][1];
          VRecFrame[0][0] = VRecFrame[0][1];
        }
        pMBInfo = pMBInfo0 + (YFrameSize/(2*16*16));
      }

      PutPicture();
//      field_endpos = 8*mEncodedSize;
      ipflag = 0;
    }

    Y_src = pSrc[0];
    U_src = pSrc[1];
    V_src = pSrc[2];

    // restore params
    pMBInfo = pMBInfo0;

    YFramePitchSrc>>=1;
    UVFramePitchSrc>>=1;
    YFramePitchRef>>=1;
    UVFramePitchRef>>=1;
    YFramePitchRec>>=1;
    UVFramePitchRec>>=1;

    YFrameVSize <<= 1;
    UVFrameVSize <<= 1;
    MBcountV <<= 1;
  }

#ifndef UMC_RESTRICTED_CODE_CME
#ifdef M2_USE_CME
  if (!bMEdone)
#endif // M2_USE_CME
#endif // UMC_RESTRICTED_CODE_CME
  if (encodeInfo.me_auto_range)
  { // adjust ME search range
    if (picture_coding_type == MPEG2_P_PICTURE) {
      AdjustSearchRange(0, 0);
    } else if (picture_coding_type == MPEG2_B_PICTURE) {
      AdjustSearchRange(B_count, 0);
      AdjustSearchRange(B_count, 1);
    }
  }

#ifdef MPEG2_DEBUG_CODE
/*
  t_end = GET_TICKS;
  encodeInfo.encode_time += (Ipp64f)(Ipp64s)(t_end-t_start)/cpu_freq;
  encodeInfo.performance = (Ipp64f)encodeInfo.numEncodedFrames/encodeInfo.encode_time;
  encodeInfo.motion_estimation_perf = (Ipp64f)encodeInfo.numEncodedFrames*cpu_freq/motion_estimation_time;
*/
//logi(encodeInfo.numEncodedFrames);
//logi(mEncodedSize);
  if (encodeInfo.numEncodedFrames > 600 && mEncodedSize > 60000) {
//logs("Save!!!!!!!!!!!!!!!!!!!!!!!!!");
    vm_char bmp_fname[256];
    vm_char frame_ch = (picture_coding_type == MPEG2_I_PICTURE) ? 'i' : (picture_coding_type == MPEG2_P_PICTURE) ? 'p' : 'b';

    vm_string_sprintf(bmp_fname, VM_STRING("C:\\Documents and Settings\\All Users\\Documents\\bmp\\frame_%03d%c.bmp"), encodeInfo.numEncodedFrames, frame_ch);
    save_bmp(bmp_fname, -1);
    if (encodeInfo.IPDistance > 1) {
      vm_string_sprintf(bmp_fname, VM_STRING("C:\\Documents and Settings\\All Users\\Documents\\bmp\\frame_%03d%c_f.bmp"), encodeInfo.numEncodedFrames, frame_ch);
      save_bmp(bmp_fname, 1);
    }
    vm_string_sprintf(bmp_fname, VM_STRING("C:\\Documents and Settings\\All Users\\Documents\\bmp\\frame_%03d%c_b.bmp"), encodeInfo.numEncodedFrames, frame_ch);
    save_bmp(bmp_fname, 0);
  }

  //DumpPSNR();
#endif /* MPEG2_DEBUG_CODE */

  if(encodeInfo.IPDistance>1) closed_gop = 0;
  encodeInfo.numEncodedFrames++;

  //if (encoded_size) *encoded_size = mEncodedSize;

  return (UMC_OK);
}

void MPEG2VideoEncoderBase::setUserData(Ipp8u* data, Ipp32s len)
{
  encodeInfo.UserData = data;
  encodeInfo.UserDataLen = len;
}

Status MPEG2VideoEncoderBase::PutPicture()
{
  Ipp32s i = 0;
  Ipp32s isfield = 0;
  Ipp32s bitsize = 0;
  Ipp64f target_size = 0;
  Ipp32s wanted_size = 0;
  Ipp32s size = 0;
  Status status = UMC_OK;
  FrameType frType = I_PICTURE;

  Ipp32s prevq=0, oldq = 0;
  Ipp32s ntry = 0;
  Ipp32s recode = 0;

  bool  bIntraAdded = false;
  bool  bNotEnoughBuffer = false;

#ifndef UMC_RESTRICTED_CODE_CME
#ifdef M2_USE_CME
  bMEdone = false;
#endif // M2_USE_CME
#endif // UMC_RESTRICTED_CODE_CME
  bQuantiserChanged = false;
  bSceneChanged = false;
  bExtendGOP = false;

  if (!second_field || ipflag)
    nLimitedMBMode = 0;

  if ((picture_coding_type == MPEG2_I_PICTURE || picture_coding_type == MPEG2_P_PICTURE)&&!second_field)
  {
     m_bSkippedMode = 0;
  }
  else if (m_bSkippedMode == 1 && picture_coding_type == MPEG2_B_PICTURE)
  {
     nLimitedMBMode = 3;
  }

  if(picture_coding_type == MPEG2_I_PICTURE) {
    mp_f_code = 0;
    m_GOP_Start_tmp = m_GOP_Start;
    m_GOP_Start = encodeInfo.numEncodedFrames;
  } else {
    mp_f_code = pMotionData[B_count].f_code;
  }

  if (picture_structure == MPEG2_FRAME_PICTURE) {
    curr_frame_pred = curr_frame_dct = encodeInfo.frame_pred_frame_dct[picture_coding_type - 1];
  } else {
    curr_frame_pred = !encodeInfo.allow_prediction16x8;
    curr_frame_dct = 1;
  }
  if(curr_frame_dct == 1)
    curr_scan = encodeInfo.altscan_tab[picture_coding_type - 1] = (picture_structure == MPEG2_FRAME_PICTURE)? 0:1;
  else
    curr_scan = encodeInfo.altscan_tab[picture_coding_type - 1];

  curr_intra_vlc_format = encodeInfo.intraVLCFormat[picture_coding_type - 1];
  for (ntry=0; ; ntry++) {
    // Save position after headers (only 0th thread)
    Ipp32s DisplayFrameNumber;
    status = UMC_OK;
    bNotEnoughBuffer = false;

    PrepareBuffers();
    size = 0;

    // Mpeg2 sequence header
    try
    {
        if (m_FirstFrame || (picture_coding_type == MPEG2_I_PICTURE))
        {
          if(m_bSH || m_FirstFrame)
          {
            PutSequenceHeader();
            PutSequenceExt();
            PutSequenceDisplayExt(); // no need

            // optionally output some text data
            PutUserData(0);
          }

          PutGOPHeader(encodeInfo.numEncodedFrames);
          size = (32+7 + BITPOS(threadSpec[0]))>>3;
        }
    }
    catch (MPEG2_Exceptions exception)
    {
        return exception.GetType();
    }


    if (brc) {
      Ipp32s quant;
      Ipp64f hrdBufFullness;
      VideoBrcParams brcParams;
      frType = (picture_coding_type == MPEG2_I_PICTURE ? I_PICTURE : (picture_coding_type == MPEG2_P_PICTURE ? P_PICTURE : B_PICTURE));
      brc->SetPictureFlags(frType, picture_structure, repeat_first_field, top_field_first, second_field);
      brc->PreEncFrame(frType, recode);
      quant = brc->GetQP(frType);
      brc->GetHRDBufferFullness(&hrdBufFullness, recode);
      brc->GetParams(&brcParams);

      if (BRC_VBR == brcParams.BRCMode)
        vbv_delay = 0xffff; // TODO: check the possibility of VBR with vbv_delay != 0xffff
      if (vbv_delay != 0xffff)
        vbv_delay = (Ipp32s)((hrdBufFullness - size*8 - 32) * 90000.0 / brcParams.maxBitrate);

      changeQuant(quant);
    } else
      PictureRateControl(size*8+32);

    DisplayFrameNumber = encodeInfo.numEncodedFrames;
    if (picture_coding_type == MPEG2_B_PICTURE)
      DisplayFrameNumber--;
    else if(!closed_gop) { // closed can be only while I is encoded
      DisplayFrameNumber += P_distance-1; // can differ for the tail
    }
    temporal_reference = DisplayFrameNumber - m_GOP_Start;

    if(picture_coding_type == MPEG2_I_PICTURE) {
      if(quantiser_scale_value < 5)
        encodeInfo.intraVLCFormat[picture_coding_type - MPEG2_I_PICTURE] = 1;
      else if(quantiser_scale_value > 15)
        encodeInfo.intraVLCFormat[picture_coding_type - MPEG2_I_PICTURE] = 0;
      curr_intra_vlc_format = encodeInfo.intraVLCFormat[picture_coding_type - MPEG2_I_PICTURE];
    }

#ifndef UMC_RESTRICTED_CODE_CME
#ifdef M2_USE_CME
    if( picture_coding_type != MPEG2_I_PICTURE && bMEdone == 0 &&
      curr_frame_pred == 1 && UMC_OK == (status = externME()) ) {
      if (status == 21)
        goto toI;
      bMEdone = true;
    }
#endif // M2_USE_CME
#endif // UMC_RESTRICTED_CODE_CME
    try
    {
        PutPictureHeader();
        PutPictureCodingExt();
        PutUserData(2);
    }
    catch (MPEG2_Exceptions exception)
    {
        return exception.GetType();
    }

    GetBlockOffsetFrm (block_offset_frm_src, YFramePitchSrc);
    GetBlockOffsetFld (block_offset_fld_src, YFramePitchSrc);

    GetBlockOffsetFrm (block_offset_frm_ref, YFramePitchRef);
    GetBlockOffsetFld (block_offset_fld_ref, YFramePitchRef);

    GetBlockOffsetFrm (block_offset_frm_rec, YFramePitchRec);
    GetBlockOffsetFld (block_offset_fld_rec, YFramePitchRec);


#ifdef MPEG2_USE_DUAL_PRIME
    dpflag=encodeInfo.enable_Dual_prime && (P_distance==1) && !progressive_frame && !curr_frame_pred && !ipflag && !bQuantiserChanged && is_interlaced_queue;
#endif //MPEG2_USE_DUAL_PRIME
    //logi(encodeInfo.numThreads);
    if (encodeInfo.numThreads > 1) {
      // start additional thread(s)
      for (i = 0; i < encodeInfo.numThreads - 1; i++) {
        vm_event_signal(&threads[i]->start_event);
      }
    }


    size = 0;
    encodePicture(0);
    if (UMC_OK != threadSpec[0].sts)
    {
       status = threadSpec[0].sts;
    }
    else
    {
        FLUSH_BITSTREAM(threadSpec[0].bBuf.current_pointer, threadSpec[0].bBuf.bit_offset);
        size += BITPOS(threadSpec[0])>>3;
    }


    if (encodeInfo.numThreads > 1) {
      Ipp8u* p = out_pointer + size;
      for (i = 0; i < encodeInfo.numThreads - 1; i++)
      {
        // wait additional thread(s)
        vm_event_wait(&threads[i]->stop_event);
        if (UMC_OK != threadSpec[i+1].sts){
           status = threadSpec[i+1].sts;
        }
        if (status == UMC_OK && size < output_buffer_size){
            Ipp32s t = 0;
            FLUSH_BITSTREAM(threadSpec[i+1].bBuf.current_pointer, threadSpec[i+1].bBuf.bit_offset);
            t = BITPOS(threadSpec[i+1])>>3;
            size += t;
            if (size < output_buffer_size)            {
                ippsCopy_8u((Ipp8u*)threadSpec[i+1].bBuf.start_pointer, (Ipp8u*)p, t);
                p += t;
            }
        }
      }
    }
    if (status == UMC_ERR_NOT_ENOUGH_BUFFER || size >= output_buffer_size - 8){

        bNotEnoughBuffer = true;
        size = (size <= output_buffer_size) ? output_buffer_size + 1 : size; // for bitrate
    }
    else if (status != UMC_OK) {
        return status;
    }


    // flush buffer
    if (!bNotEnoughBuffer)
        status = FlushBuffers(&size);

    isfield = (picture_structure != MPEG2_FRAME_PICTURE);
    bitsize = size*8;

    if(!brc)
    {
        if(encodeInfo.rc_mode == RC_CBR) {
          target_size = rc_tagsize[picture_coding_type-MPEG2_I_PICTURE];
          wanted_size = (Ipp32s)(target_size - rc_dev / 3 * target_size / rc_tagsize[0]);
          wanted_size >>= isfield;
        } else {
          target_size = wanted_size = bitsize; // no estimations
        }
    }

    if(picture_coding_type == MPEG2_I_PICTURE) {
      if(bitsize > (MBcount*128>>isfield))
        encodeInfo.intraVLCFormat[0] = 1;
      else //if(bitsize < MBcount*192)
        encodeInfo.intraVLCFormat[0] = 0;
    }
#ifdef SCENE_DETECTION
    bSceneChanged = false;
    if ( picture_coding_type == MPEG2_P_PICTURE && second_field == 0 && bExtendGOP == false && encodeInfo.strictGOP <2) { //
      Ipp32s numIntra = 0;
#ifndef UMC_RESTRICTED_CODE
      Ipp32s weightIntra = 0;
      Ipp32s weightInter = 0;
#endif // UMC_RESTRICTED_CODE
      Ipp32s t;
      for (t = 0; t < encodeInfo.numThreads; t++) {
        numIntra += threadSpec[t].numIntra;
#ifndef UMC_RESTRICTED_CODE
        weightIntra += threadSpec[t].weightIntra;
        weightInter += threadSpec[t].weightInter;
#endif // UMC_RESTRICTED_CODE
      }
      if(picture_structure != MPEG2_FRAME_PICTURE)
        numIntra <<= 1; // MBcount for the frame picture

      if (numIntra*2 > MBcount*1 // 2/3 Intra: I frame instead of P
#ifndef UMC_RESTRICTED_CODE
        // || weightIntra*4 < weightInter*3 // 1/2 length: I frame instead of P
#endif // UMC_RESTRICTED_CODE
         ) {
#ifndef UMC_RESTRICTED_CODE_CME
#ifdef M2_USE_CME
toI:
#endif // M2_USE_CME
#endif // UMC_RESTRICTED_CODE_CME
        picture_coding_type = MPEG2_I_PICTURE;
//        frType = I_PICTURE;
        //curr_gop = 0;
        GOP_count = 0;
        bSceneChanged = true;
        mp_f_code = 0;
        m_GOP_Start_tmp = m_GOP_Start;
        m_GOP_Start = encodeInfo.numEncodedFrames;
        ntry = -1;
        bIntraAdded = true;
        //logi(encodeInfo.numEncodedFrames);
        continue;
      }
    }
    // scene change not detected
#endif
    if (!brc) {
      oldq = prevq;
      prevq = quantiser_scale_value;
      bQuantiserChanged = false;

      if (status == UMC_ERR_NOT_ENOUGH_BUFFER || bitsize > rc_vbv_max
        || (bitsize > wanted_size*2 && (ntry==0 || oldq<prevq))
        )
      {
        changeQuant(quantiser_scale_value + 1);
        if(prevq == quantiser_scale_value) {
          if (picture_coding_type == MPEG2_I_PICTURE &&
             !m_FirstFrame && encodeInfo.gopSize > 1 &&
             (status == UMC_ERR_NOT_ENOUGH_BUFFER || bitsize > rc_vbv_max) &&
             !encodeInfo.strictGOP) {
            picture_coding_type = MPEG2_P_PICTURE;
            m_GOP_Start = m_GOP_Start_tmp;
            mp_f_code = pMotionData[0].f_code;
            bExtendGOP = true;
            GOP_count -= encodeInfo.IPDistance; // add PB..B group
            ntry = -1;
            continue;
          }
          if (nLimitedMBMode > 2) {
            status = UMC_ERR_NOT_ENOUGH_BUFFER;
          } else {
            //Ipp32s maxMBSize = (wanted_size/MBcount);
            nLimitedMBMode ++;
            if ((picture_coding_type == MPEG2_P_PICTURE || picture_coding_type == MPEG2_B_PICTURE) && nLimitedMBMode == 2)
            {
                m_bSkippedMode = 1;
                nLimitedMBMode = 3;
            }
            continue;
          }

        } else {
          bQuantiserChanged = true;
          continue;
        }
      } else if (bitsize < rc_vbv_min) {
        if(ntry==0 || oldq>prevq)
          changeQuant(quantiser_scale_value - 1);
        if(prevq == quantiser_scale_value) {
          Ipp8u *p = out_pointer + size;
          while(bitsize < rc_vbv_min && size < output_buffer_size - 4) {
            *(Ipp32u*)p = 0;
            p += 4;
            size += 4;
            bitsize += 32;
          }
          status = UMC_OK; // already minimum;
        } else {
          bQuantiserChanged = true;
          continue;
        }
      }else if ( bNotEnoughBuffer){
        status = UMC_ERR_NOT_ENOUGH_BUFFER;
      }
    }

    if (brc) {
      BRCStatus hrdSts;
      Ipp32s framestoI;
      Ipp32s gopSize;
      if (encodeInfo.numEncodedFrames >= m_FirstGopSize) {
        gopSize = encodeInfo.gopSize;
        framestoI = gopSize - ((encodeInfo.numEncodedFrames - m_FirstGopSize) % gopSize) - 1;
      } else {
        gopSize = m_FirstGopSize;
        framestoI = gopSize - encodeInfo.numEncodedFrames - 1;
      }
      brc->SetQP(quantiser_scale_value, frType); // pass actually used qp to BRC
//      brc->SetPictureFlags(frType, picture_structure, repeat_first_field, top_field_first, second_field);

      hrdSts = brc->PostPackFrame(frType, size*8, 0, recode);
      /*  Debug  */
      Ipp32s maxSize = 0, minSize = 0;
      Ipp64f buffullness;
      Ipp32s buffullnessbyI;
      VideoBrcParams brcParams;
      brc->GetHRDBufferFullness(&buffullness, 0);
      brc->GetMinMaxFrameSize(&minSize, &maxSize);
      brc->GetParams(&brcParams);
//      printf("\n frame # %d / %d hrdSts %d |||||||||| fullness %d bIntraAdded %d \n", encodeInfo.numEncodedFrames, GOP_count, hrdSts, (Ipp32s)buffullness, bIntraAdded);
      if (hrdSts == BRC_OK && brcParams.maxBitrate!=0) {
        Ipp32s inputbitsPerPic = m_InputBitsPerFrame;
        Ipp32s minbitsPerPredPic, minbitsPerIPic;
        if (picture_structure != MPEG2_FRAME_PICTURE) {
          minbitsPerPredPic = m_MinFieldSizeBits[1];
          minbitsPerIPic = m_MinFieldSizeBits[0];
          inputbitsPerPic >>= 1;
          framestoI *= 2;
          if (!second_field)
            framestoI--;
        } else {
          minbitsPerPredPic = m_MinFrameSizeBits[1];
          minbitsPerIPic = m_MinFrameSizeBits[0];
        }
        buffullnessbyI = (Ipp32s)buffullness + framestoI * (inputbitsPerPic - minbitsPerPredPic); /// ??? +- 1, P/B?

        { //  I frame is coming
//          printf("\n buffullnessbyI %d \n", buffullnessbyI);
          if (buffullnessbyI < minbitsPerIPic ||
             (picture_structure != MPEG2_FRAME_PICTURE && !second_field && size*8*2 > inputbitsPerPic + maxSize && picture_coding_type != MPEG2_I_PICTURE)) {
            if (!nLimitedMBMode) {
              Ipp32s quant = quantiser_scale_value; // brc->GetQP(frType);
              changeQuant(quant+2);
              if (quantiser_scale_value == quant) // ??
              {
                nLimitedMBMode++;
                if ((picture_coding_type == MPEG2_P_PICTURE || picture_coding_type == MPEG2_B_PICTURE) && nLimitedMBMode == 2)
                {
                    m_bSkippedMode = 1;
                    nLimitedMBMode = 3;
                }
//                printf ("\n Recalculate 0: Frame %d. Limited mode: %d, max size = %d (current %d), max_size on MB = %d (current %d)\n",frType,nLimitedMBMode,maxSize,bitsize,maxSize/MBcount,bitsize/MBcount);


              }
              brc->SetQP(quantiser_scale_value, frType);
              recode = 2;
              bQuantiserChanged = true; // ???
              continue;
            } else {
              if (frType == I_PICTURE && !m_FirstFrame && encodeInfo.gopSize > 1 && (!encodeInfo.strictGOP || bIntraAdded)) {
                frType = P_PICTURE;
                picture_coding_type = MPEG2_P_PICTURE;
                m_GOP_Start = m_GOP_Start_tmp;
                mp_f_code = pMotionData[0].f_code;
                bExtendGOP = true;
                //curr_gop += encodeInfo.IPDistance;
                GOP_count -= encodeInfo.IPDistance; // add PB..B group
                ntry = -1;
                recode = 2;
                continue;
              } else if (nLimitedMBMode < 3) {
                recode = 2;
                nLimitedMBMode++;
                if ((picture_coding_type == MPEG2_P_PICTURE || picture_coding_type == MPEG2_B_PICTURE) && nLimitedMBMode == 2)
                {
                    m_bSkippedMode = 1;
                    nLimitedMBMode = 3;
                }
//                printf ("\n Recalculate 1: Frame %d. Limited mode: %d, max size = %d (current %d), max_size on MB = %d (current %d)\n",frType,nLimitedMBMode,maxSize,bitsize,maxSize/MBcount,bitsize/MBcount);
                continue;
              }
            } //  else if (maxSize < m_MinFrameSizeBits[0]) - can return with error already
          }
        }
      }


      /* ------ */
      if (BRC_OK != hrdSts) {
        if (!(hrdSts & BRC_NOT_ENOUGH_BUFFER)) {
          recode = 1;
          bQuantiserChanged = true;
          continue;
        } else {
          if (hrdSts & BRC_ERR_SMALL_FRAME) {
            Ipp32s maxSize, minSize;
            Ipp8u *p = out_pointer + size;
            brc->GetMinMaxFrameSize(&minSize, &maxSize);
            if (bitsize < minSize && size < output_buffer_size - 1)
            {
                Ipp32s nBytes = (minSize - bitsize + 7)/8;
                nBytes = (size + nBytes < output_buffer_size) ? nBytes : output_buffer_size - size;
                memset (p,0,nBytes);
                size    += nBytes;
                bitsize += (nBytes*8);
                p += nBytes;

            }
            brc->PostPackFrame(frType, bitsize, 0, recode);
            recode = 0;
//            status = UMC_OK;
          } else {
            if (frType == I_PICTURE && !m_FirstFrame && encodeInfo.gopSize > 1 && (!encodeInfo.strictGOP || bIntraAdded)) {
              frType = P_PICTURE; // ???
              picture_coding_type = MPEG2_P_PICTURE;
              m_GOP_Start = m_GOP_Start_tmp;
              mp_f_code = pMotionData[0].f_code;
              bExtendGOP = true;
              //curr_gop += encodeInfo.IPDistance;
              GOP_count -= encodeInfo.IPDistance; // add PB..B group
              ntry = -1;
              recode = 2; // ???
              if (bIntraAdded)
              {
                 nLimitedMBMode = 3;
                 m_bSkippedMode = 1;
              }
              continue;
            } else {
              if (nLimitedMBMode>2){
                status = UMC_ERR_NOT_ENOUGH_BUFFER;
//                printf ("\nError... Frame %d. Limited mode: %d, max size = %d (current %d), max_size on MB = %d (current %d)\n",frType,nLimitedMBMode,maxSize,bitsize,maxSize/MBcount,bitsize/MBcount);
              }
              else {
               nLimitedMBMode ++;
               recode = 2; // ???
                if ((picture_coding_type == MPEG2_P_PICTURE || picture_coding_type == MPEG2_B_PICTURE) && nLimitedMBMode == 2)
                {
                    m_bSkippedMode = 1;
                    nLimitedMBMode = 3;
                }
//                printf ("\n Recalculate 2: Frame %d. Limited mode: %d, max size = %d (current %d), max_size on MB = %d (current %d)\n",frType,nLimitedMBMode,maxSize,bitsize,maxSize/MBcount,bitsize/MBcount);


               continue;
              }
            }
          }
        }
      } else{
        if (bNotEnoughBuffer)
        {
            status = UMC_ERR_NOT_ENOUGH_BUFFER;
        }
        recode = 0;
//        printf ("\nOK... Frame %d. Limited mode: %d, max size = %d (%d), max_size on MB = %d (%d)\n",frType,nLimitedMBMode,maxSize,bitsize,maxSize/MBcount,bitsize/MBcount);
      }
    }//BRC

    // if (!bQuantiserChanged && !bSceneChanged)
    m_FirstFrame = 0;
    qscale[picture_coding_type-MPEG2_I_PICTURE] = quantiser_scale_value;
    out_pointer += size;
    output_buffer_size -= size;
    mEncodedSize += size;
    if (!brc)
      PostPictureRateControl(size*8);
#ifdef MPEG2_USE_DUAL_PRIME
    if (bQuantiserChanged != true && picture_coding_type == MPEG2_P_PICTURE && !ipflag &&
        encodeInfo.enable_Dual_prime && (P_distance==1) && !progressive_frame)
        IsInterlacedQueue();
#endif //MPEG2_USE_DUAL_PRIME
    return status;

#ifndef UMC_RESTRICTED_CODE
    /*
    printf("fr: %d, quantizer changed from %d to %d; tag:%d want:%d sz:%d\n", encodeInfo.numEncodedFrames, prevq, quantiser_scale_value,
      (Ipp32s)target_size, wanted_size, bitsize);
    printf("picture_coding_type = %d\n", picture_coding_type);
    */
#endif // UMC_RESTRICTED_CODE
    }




}


const Ipp32s sorted_ratio[][2] = {
  {1,32},{1,31},{1,30},{1,29},{1,28},{1,27},{1,26},{1,25},{1,24},{1,23},{1,22},{1,21},{1,20},{1,19},{1,18},{1,17},
  {1,16},{2,31},{1,15},{2,29},{1,14},{2,27},{1,13},{2,25},{1,12},{2,23},{1,11},{3,32},{2,21},{3,31},{1,10},{3,29},
  {2,19},{3,28},{1, 9},{3,26},{2,17},{3,25},{1, 8},{4,31},{3,23},{2,15},{3,22},{4,29},{1, 7},{4,27},{3,20},{2,13},
  {3,19},{4,25},{1, 6},{4,23},{3,17},{2,11},{3,16},{4,21},{1, 5},{4,19},{3,14},{2, 9},{3,13},{4,17},{1, 4},{4,15},
  {3,11},{2, 7},{3,10},{4,13},{1, 3},{4,11},{3, 8},{2, 5},{3, 7},{4, 9},{1, 2},{4, 7},{3, 5},{2, 3},{3, 4},{4, 5},
  {1,1},{4,3},{3,2},{2,1},{3,1},{4,1}
};

// sets framerate code and extensions
Status MPEG2VideoEncoderBase::SetFrameRate(Ipp64f new_fr, Ipp32s is_mpeg1)
{
  const Ipp32s srsize = sizeof(sorted_ratio)/sizeof(sorted_ratio[0]);
  const Ipp32s rtsize = sizeof(ratetab)/sizeof(ratetab[0]);
  Ipp32s i, j, besti=0, bestj=0;
  Ipp64f ratio, bestratio = 1.5;
  Ipp32s fr1001 = (Ipp32s)(new_fr*1001+.5);

  frame_rate_code = 5;
  frame_rate_extension_n = frame_rate_extension_d = 0;
  for(j=0;j<rtsize;j++) {
    Ipp32s try1001 = (Ipp32s)(ratetab[j]*1001+.5);
    if(fr1001 == try1001) {
      frame_rate_code = j+1;
      return UMC_OK;
    }
  }
  if(is_mpeg1) { // for mpeg2 will find frame_rate_extention
    return UMC_ERR_FAILED; // requires quite precise values: 0.05% or 0.02 Hz
  }

  if(new_fr < ratetab[0]/sorted_ratio[0][1]*0.7)
    return UMC_ERR_FAILED;

  for(j=0;j<rtsize;j++) {
    ratio = ratetab[j] - new_fr; // just difference here
    if(ratio < 0.0001 && ratio > -0.0001) { // was checked above with bigger range
      frame_rate_code = j+1;
      frame_rate_extension_n = frame_rate_extension_d = 0;
      return UMC_OK;
    }
    if(!is_mpeg1)
    for(i=0;i<srsize+1;i++) { // +1 because we want to analyze last point as well
      if(i==srsize || ratetab[j]*sorted_ratio[i][0] > new_fr*sorted_ratio[i][1]) {
        if(i>0) {
          ratio = ratetab[j]*sorted_ratio[i-1][0] / (new_fr*sorted_ratio[i-1][1]); // up to 1
          if(1/ratio < bestratio) {
            besti = i-1;
            bestj = j;
            bestratio = 1/ratio;
          }
        }
        if(i<srsize) {
          ratio = ratetab[j]*sorted_ratio[i][0] / (new_fr*sorted_ratio[i][1]); // down to 1
          if(ratio < bestratio) {
            besti = i;
            bestj = j;
            bestratio = ratio;
          }
        }
        break;
      }
    }
  }

  if(bestratio > 1.49)
    return UMC_ERR_FAILED;

  frame_rate_code = bestj+1;
  frame_rate_extension_n = sorted_ratio[besti][0]-1;
  frame_rate_extension_d = sorted_ratio[besti][1]-1;
  return UMC_OK;
}

Status MPEG2VideoEncoderBase::SetAspectRatio(Ipp32s hor, Ipp32s ver) // sets aspect code from h/v value
{
  if(hor*3 == ver*4) {
    aspectRatio_code = 2;
    encodeInfo.info.aspect_ratio_width = 4;
    encodeInfo.info.aspect_ratio_height = 3;
  } else if(hor*9 == ver*16) {
    aspectRatio_code = 3;
    encodeInfo.info.aspect_ratio_width = 16;
    encodeInfo.info.aspect_ratio_height = 9;
  } else if(hor*100 == ver*221) {
    aspectRatio_code = 4;
    encodeInfo.info.aspect_ratio_width = 221;
    encodeInfo.info.aspect_ratio_height = 100;
  } else {
    Ipp64f ratio = (Ipp64f)hor/ver;
    Ipp32s parx, pary;
    Ipp32s w = encodeInfo.info.clip_info.width;
    Ipp32s h = encodeInfo.info.clip_info.height;

    // in video_data
    DARtoPAR(w, h, hor, ver, &parx, &pary);
    DARtoPAR(h, w, parx, pary,
      &encodeInfo.info.aspect_ratio_width,
      &encodeInfo.info.aspect_ratio_height); // PAR to DAR

    if (parx == pary) aspectRatio_code = 1;
    else if (ratio > 1.25 && ratio < 1.4) aspectRatio_code = 2;
    else if (ratio > 1.67 && ratio < 1.88) aspectRatio_code = 3;
    else if (ratio > 2.10 && ratio < 2.30) aspectRatio_code = 4;
    else {
      aspectRatio_code = 1;
      vm_debug_trace(VM_DEBUG_WARNING, VM_STRING("Improper display aspect ratio"));
    }
  }
  return UMC_OK;
}

#define SHIFT_PTR(oldptr, oldbase, newbase) \
  ( (newbase) + ( (Ipp8u*)(oldptr) - (Ipp8u*)(oldbase) ) )

Status MPEG2VideoEncoderBase::LockBuffers(void)
{
  VideoDataBuffer*  vin = DynamicCast<VideoDataBuffer> (frames.curenc);
  if(vin)
    vin->Lock(); // input was copied to buffer

  frames.buf_ref[1][0].Lock(); // dst recon for I,P or bwd recon ref for B
#ifdef ME_REF_ORIGINAL
  if(picture_coding_type == MPEG2_B_PICTURE) {
    frames.buf_ref[1][1].Lock(); // bwd orig ref for B
  }
#endif
  // lock ref frames
  if(picture_coding_type != MPEG2_I_PICTURE) {
    frames.buf_ref[0][0].Lock();
#ifdef ME_REF_ORIGINAL
    frames.buf_ref[0][1].Lock();
#endif
  }

  if(picture_coding_type != MPEG2_B_PICTURE) {
    frames.buf_ref[1][0].SetInvalid(1); // to be reconstucted
  }
  // can return UMC_OK;

  return UMC_OK;
}

Status MPEG2VideoEncoderBase::UnlockBuffers(void)
{
  Status ret = UMC_OK;
  VideoDataBuffer*  vin = DynamicCast<VideoDataBuffer> (frames.curenc);
  if(vin)
    vin->Unlock(); // input was copied to buffer

  frames.buf_ref[1][0].Unlock(); // dst recon for I,P or bwd recon ref for B
#ifdef ME_REF_ORIGINAL
  if(picture_coding_type == MPEG2_B_PICTURE) {
    frames.buf_ref[1][1].Unlock(); // bwd orig ref for B
  }
#endif
  if(picture_coding_type != MPEG2_I_PICTURE) {
    frames.buf_ref[0][0].Unlock();
#ifdef ME_REF_ORIGINAL
    frames.buf_ref[0][1].Unlock();
#endif
  }

  if(picture_coding_type != MPEG2_B_PICTURE) {
    frames.buf_ref[1][0].SetInvalid(0); // just reconstucted
  }

  return ret;
}

// VideoData + Memory allocator interface for internal buffers
// Only single alloc and lock
// It is simple and should be used accurately

MPEG2VideoEncoderBase::VideoDataBuffer::VideoDataBuffer()
{
  m_mid = MID_INVALID;
  m_pMemoryAllocator = 0;
  m_bLocked = 0;
}

MPEG2VideoEncoderBase::VideoDataBuffer::~VideoDataBuffer()
{
  if(m_mid != MID_INVALID || m_pMemoryAllocator != 0) {
    if(m_bLocked)
    {
      m_pMemoryAllocator->Unlock(m_mid);
      m_pMemoryAllocator->Free(m_mid);
    }
  }
  m_mid = MID_INVALID;
  m_pMemoryAllocator = 0;
}

Status MPEG2VideoEncoderBase::VideoDataBuffer::SetAllocator(MemoryAllocator * allocator)
{
  if(m_pMemoryAllocator != 0)
    return UMC_ERR_FAILED; // Set only once
  m_pMemoryAllocator = allocator;
  return UMC_OK;
}

Status MPEG2VideoEncoderBase::VideoDataBuffer::Alloc(size_t /*requredSize = 0*/)
{
  Status ret;
  if(m_pMemoryAllocator == 0)
    return UMC_ERR_NULL_PTR;
  if(m_mid != MID_INVALID)
    return UMC_ERR_FAILED; // single alloc
  size_t sz = GetMappingSize();
  if(sz==0)
    return UMC_ERR_NOT_INITIALIZED;
  ret = m_pMemoryAllocator->Alloc(&m_mid, sz, UMC_ALLOC_PERSISTENT, GetAlignment());
  //if(ret != UMC_OK)
  return ret;
}

Status MPEG2VideoEncoderBase::VideoDataBuffer::Lock()
{
  void * ptr;
  if(m_mid == MID_INVALID || m_pMemoryAllocator == 0)
    return UMC_ERR_NULL_PTR;
  if(m_bLocked)
    return UMC_ERR_FAILED; // single lock

  ptr = m_pMemoryAllocator->Lock(m_mid);
  if(ptr == 0)
    return UMC_ERR_FAILED;
  m_bLocked = 1;
  return SetBufferPointer((Ipp8u*)ptr, GetMappingSize());
}
Status MPEG2VideoEncoderBase::VideoDataBuffer::Unlock()
{
  if(m_mid == MID_INVALID || m_pMemoryAllocator == 0)
    return UMC_ERR_NULL_PTR;
  if(!m_bLocked)
    return UMC_ERR_FAILED; // single lock
  m_bLocked = 0;
  return  m_pMemoryAllocator->Unlock(m_mid);
}

Status MPEG2VideoEncoderBase::VideoDataBuffer::SetMemSize(Ipp32s Ysize, Ipp32s UVsize)
{
  if (2 > GetNumPlanes())
    return UMC_ERR_NOT_INITIALIZED;
  m_pPlaneData[0].m_nMemSize = Ysize;
  m_pPlaneData[1].m_nMemSize = UVsize;
  if(2 < GetNumPlanes())
    m_pPlaneData[2].m_nMemSize = UVsize;
  return UMC_OK;
}

Status MPEG2VideoEncoderBase::CreateFrameBuffer()
{
  Ipp32s i;
  Status ret;
  if(frames.frame_buffer != 0)
    DeleteFrameBuffer();

#ifdef ME_REF_ORIGINAL
  const Ipp32s morig = 1;
#else /* ME_REF_ORIGINAL */
  const Ipp32s morig = 0;
#endif /* ME_REF_ORIGINAL */
  frames.frame_buffer_size = 2*(1+morig) + (encodeInfo.IPDistance - 1) + 1;
  frames.frame_buffer = (VideoDataBuffer*) new VideoDataBuffer [frames.frame_buffer_size];
  if(frames.frame_buffer == 0)
    return UMC_ERR_ALLOC;
  frames.buf_ref[0] = frames.frame_buffer;
  frames.buf_ref[1] = frames.frame_buffer + (1+morig);
  frames.buf_B      = frames.frame_buffer + 2*(1+morig);
  frames.buf_aux    = frames.buf_B + (encodeInfo.IPDistance - 1);

  for(i=0; i<frames.frame_buffer_size; i++) {
    frames.frame_buffer[i].SetAlignment(16);
    ret = frames.frame_buffer[i].Init(encodeInfo.info.clip_info.width, encodeInfo.info.clip_info.height,
      encodeInfo.info.color_format, 8);
    if(ret != UMC_OK)
      return ret;
    // !!! what alignment for downsampled? 16 or 8 ???
    frames.frame_buffer[i].SetPlanePitch(YFramePitchSrc, 0);
    if(encodeInfo.info.color_format == NV12) {
      frames.frame_buffer[i].SetPlanePitch(UVFramePitchSrc*2, 1);
      ret = frames.frame_buffer[i].SetMemSize(YFrameSize, UVFrameSize*2);
      if(ret != UMC_OK)
          return ret;
    } else {
      frames.frame_buffer[i].SetPlanePitch(UVFramePitchSrc, 1);
      frames.frame_buffer[i].SetPlanePitch(UVFramePitchSrc, 2);
      ret = frames.frame_buffer[i].SetMemSize(YFrameSize, UVFrameSize);
      if(ret != UMC_OK)
          return ret;
    }
    frames.frame_buffer[i].SetTime(-1);
    frames.frame_buffer[i].SetAllocator(m_pMemoryAllocator);
    ret = frames.frame_buffer[i].Alloc(); // Need to Lock() prior to use
    if(ret != UMC_OK)
      return ret;
    frames.frame_buffer[i].SetInvalid(1); // no data in it /// better to Init() func
  }

  return UMC_OK;
}

Status MPEG2VideoEncoderBase::DeleteFrameBuffer()
{
  if(frames.frame_buffer != 0) {
    delete[] frames.frame_buffer;
    frames.frame_buffer = 0;
  }
  return UMC_OK;

}

// no use
Status MPEG2VideoEncoderBase::RotateReferenceFrames()
{
  if(picture_coding_type == MPEG2_P_PICTURE ||
     picture_coding_type == MPEG2_I_PICTURE) {
    VideoDataBuffer* ptr = frames.buf_ref[0];
    frames.buf_ref[0] = frames.buf_ref[1];
    frames.buf_ref[1] = ptr; // rotated
  }
  return UMC_OK;
}

// sets frames.curenc and picture_coding_type
Status MPEG2VideoEncoderBase::SelectPictureType(VideoData *in)
{
  FrameType pictype;
  frames.curenc = in;
  if(!in) { // tail processing
    if(B_count>0) { // encode frames from B buffer
      picture_coding_type = MPEG2_B_PICTURE;
      frames.curenc = &frames.buf_B[B_count-1]; // take buffered B
      if(frames.curenc->GetInvalid()) { //
        return UMC_ERR_NOT_ENOUGH_DATA; // no buffered yet
      }
      frames.curenc->SetInvalid(1); // to avoid reuse
    }
    else { // if(B_count==0) - should have been P - take last valid from B-buffer
      Ipp32s i = P_distance - 2;
      for( ; i>=0; i--) {
        if(!frames.buf_B[i].GetInvalid()) {
          picture_coding_type = MPEG2_P_PICTURE; // reset B_count to 1 after
          P_distance = i+1; // P is closer than usually
          frames.curenc = &frames.buf_B[i];
          frames.curenc->SetInvalid(1); // to avoid reuse
          B_count = 0; // will be +1 next time
          return UMC_OK;
        }
      }
      return UMC_ERR_NOT_ENOUGH_DATA; // no more frames
    }
    //return UMC_ERR_FAILED;
  } else if(encodeInfo.lFlags & FLAG_VENC_REORDER) {
    if(B_count > 0) {
      picture_coding_type = MPEG2_B_PICTURE;
      frames.curenc = &frames.buf_B[B_count-1]; // take buffered B
      if(frames.curenc->GetInvalid())
        return UMC_ERR_NOT_ENOUGH_DATA; // no buffered yet
      frames.curenc->SetInvalid(1); // to avoid reuse
    } else {
      if (GOP_count == 0)
        picture_coding_type = MPEG2_I_PICTURE;
      else {
        picture_coding_type = MPEG2_P_PICTURE;
        P_distance = encodeInfo.IPDistance; // normal distance
      }
    }
  } else {
    pictype = in->GetFrameType(); // must match IPDistance if !reorder
    switch(pictype) {
      case B_PICTURE: picture_coding_type = MPEG2_B_PICTURE; break;
      case P_PICTURE: picture_coding_type = MPEG2_P_PICTURE; break;
      case I_PICTURE: picture_coding_type = MPEG2_I_PICTURE; break;
      default: return UMC_ERR_INVALID_PARAMS;
    }
  }
  return UMC_OK;
}

Ipp32s MPEG2VideoEncoderBase::FormatMismatch(VideoData *in)
{
  if (in == 0 || (in->GetColorFormat() != encodeInfo.info.color_format) ||
      2 > in->GetNumPlanes() ||
      ((Ipp32s)in->GetPlanePitch(0) != YFramePitchSrc) ||
      (picture_coding_type != MPEG2_B_PICTURE && in->GetHeight() < 16*MBcountV)) // B is in buffer already
    return 1;

  if (encodeInfo.info.color_format == NV12)
    return ((Ipp32s)in->GetPlanePitch(1) != UVFramePitchSrc * 2);
  else
    return ( ((Ipp32s)in->GetPlanePitch(1) != UVFramePitchSrc) ||
             ((Ipp32s)in->GetPlanePitch(2) != UVFramePitchSrc) );
}

Status MPEG2VideoEncoderBase::TakeNextFrame(VideoData *in)
{
  Status ret;

  ret = SelectPictureType(in); // sets frames.curenc and picture_coding_type
  if(ret != UMC_OK)
    return ret;
  RotateReferenceFrames();

#ifdef ME_REF_ORIGINAL
  if(picture_coding_type != MPEG2_B_PICTURE) {
    LoadToBuffer(frames.buf_ref[1]+1, in); // store original frame
    frames.curenc = frames.buf_ref[1]+1;
  }
#endif /* ME_REF_ORIGINAL */

  if( FormatMismatch(frames.curenc) ) {
    LoadToBuffer(frames.buf_aux, frames.curenc); // copy to internal
    frames.curenc = frames.buf_aux;
  }

  return UMC_OK;
}

// in the end of GetFrame()
Status MPEG2VideoEncoderBase::FrameDone(VideoData *in)
{
  if(in == 0 && frames.curenc != 0) // no input, encoded from B buffer
    frames.curenc->SetInvalid(1);
  if(picture_coding_type == MPEG2_B_PICTURE) {
    if(in != 0) {
      LoadToBuffer(&frames.buf_B[B_count-1], in); // take new to B buffer
      frames.buf_B[B_count-1].SetTime(m_PTS, m_DTS);
    }
  }
  // Update GOP counters
  if( ++B_count >= encodeInfo.IPDistance )
    B_count = 0;
  if( ++GOP_count >= encodeInfo.gopSize )
    GOP_count = 0;
  return UMC_OK;
}

Status MPEG2VideoEncoderBase::PrepareFrame()
{
  Ipp32s i;

  Y_src = (Ipp8u*)frames.curenc->GetPlanePointer(0);
  U_src = (Ipp8u*)frames.curenc->GetPlanePointer(1);
  if (encodeInfo.info.color_format == NV12)
    V_src = U_src+1;
  else
    V_src = (Ipp8u*)frames.curenc->GetPlanePointer(2);
  if (Y_src == NULL || U_src == NULL || V_src == NULL) {
    return UMC_ERR_NULL_PTR;
  }
  // can set picture coding params here according to VideoData
  PictureStructure ps = frames.curenc->GetPictureStructure();
  top_field_first = (ps == PS_TOP_FIELD_FIRST || progressive_frame) ? 1 : 0;

#ifdef ME_REF_ORIGINAL
  const Ipp32s morig = 1;
#else /* ME_REF_ORIGINAL */
  const Ipp32s morig = 0;
#endif /* ME_REF_ORIGINAL */
  for(i=0; i<2; i++) { // forward/backward
    YRefFrame[0][i] = (Ipp8u*)(frames.buf_ref[i][0].GetPlanePointer(0));
    YRecFrame[0][i] = (Ipp8u*)(frames.buf_ref[i][morig].GetPlanePointer(0));
    URefFrame[0][i] = (Ipp8u*)(frames.buf_ref[i][0].GetPlanePointer(1));
    URecFrame[0][i] = (Ipp8u*)(frames.buf_ref[i][morig].GetPlanePointer(1));
    if (encodeInfo.info.color_format == NV12) {
      VRefFrame[0][i] = URefFrame[0][i] + 1;
      VRecFrame[0][i] = URecFrame[0][i] + 1;
    } else {
      VRefFrame[0][i] = (Ipp8u*)(frames.buf_ref[i][0].GetPlanePointer(2));
      VRecFrame[0][i] = (Ipp8u*)(frames.buf_ref[i][morig].GetPlanePointer(2));
    }
    YRefFrame[1][i] = YRefFrame[0][i] + YFramePitchRef;
    YRecFrame[1][i] = YRecFrame[0][i] + YFramePitchRef;
    if (encodeInfo.info.color_format == NV12) {
      URefFrame[1][i] = URefFrame[0][i] + 2*UVFramePitchRef;
      URecFrame[1][i] = URecFrame[0][i] + 2*UVFramePitchRec;
      VRefFrame[1][i] = VRefFrame[0][i] + 2*UVFramePitchRef;
      VRecFrame[1][i] = VRecFrame[0][i] + 2*UVFramePitchRec;
    } else {
      URefFrame[1][i] = URefFrame[0][i] + UVFramePitchRef;
      URecFrame[1][i] = URecFrame[0][i] + UVFramePitchRec;
      VRefFrame[1][i] = VRefFrame[0][i] + UVFramePitchRef;
      VRecFrame[1][i] = VRecFrame[0][i] + UVFramePitchRec;
    }
  }
  return UMC_OK;
}

Status MPEG2VideoEncoderBase::LoadToBuffer(VideoDataBuffer* frame, VideoData *in)
{
  Status ret;
  if(frame == 0 || in == 0)
    return UMC_ERR_NULL_PTR;
  frame->Lock();
  if(frame_loader == NULL) {
    frame_loader = new VideoProcessing;
  }
  ret = frame_loader->GetFrame(in, frame);
  frame->Unlock();
  frame->SetInvalid(0);
  return ret;
}

Status MPEG2VideoEncoderBase::FlushBuffers(Ipp32s * size_bytes)
{
    // flush buffer
    Status status = UMC_OK;
    Ipp8u* p = out_pointer + *size_bytes;
    Ipp32s fieldCount = 0;
    Ipp32s i;

    // move encoded data from all threads to first
    for (i = 0; i < encodeInfo.numThreads; i++) {
      fieldCount += threadSpec[i].fieldCount;
    }

    if(!curr_frame_dct)
      encodeInfo.altscan_tab[picture_coding_type-MPEG2_I_PICTURE] =
        (fieldCount > 0) ? 1 : 0;

    // put EOS if needed
    if(m_bEOS && (picture_structure == MPEG2_FRAME_PICTURE || second_field)) {
      if ((Ipp32s)(p - out_pointer)+4 > output_buffer_size) {
        status = UMC_ERR_NOT_ENOUGH_BUFFER;
        p += 4;
      } else {
        *p++ = 0; *p++ = 0; *p++ = 1; *p++ = (Ipp8u)(SEQ_END_CODE & 0xff);
      }
    }
    // align encoded size to 4
    while ((size_t)p & 3) {
      if ((Ipp32s)(p - out_pointer)+1 > output_buffer_size) {
        status = UMC_ERR_NOT_ENOUGH_BUFFER;
        p ++;
      } else
        *p++ = 0;
    }
#ifdef MPEG2_DEBUG_CODE
    if (status != UMC_OK) {
      for (i = 0; i < encodeInfo.numThreads; i++) {
        size = (Ipp8u*)threadSpec[i].current_pointer - (Ipp8u*)threadSpec[i].start_pointer - (threadSpec[i].bit_offset>>3);
        printf("%d: %d (%d)\n", i, size, thread_buffer_size);
      }
      //exit(1);
    }
#endif

    // summary size
    *size_bytes = (Ipp32s)(p - out_pointer);

    return status;
}

Status MPEG2VideoEncoderBase::ComputeTS(VideoData *in, MediaData *out)
{
  Ipp64f PTS, DTS;
  if( frames.curenc == 0 )
    return UMC_ERR_NOT_ENOUGH_DATA;
  if( encodeInfo.lFlags & FLAG_VENC_REORDER ) { // consequent input PTS
    if(picture_coding_type == MPEG2_B_PICTURE) {
      if(in != 0) { // this frame will be buffered
        in->GetTime(PTS, DTS);
        if(PTS > 0) m_PTS = PTS;
        else        m_PTS += 1./encodeInfo.info.framerate;
        m_DTS = -1;
      }
      frames.curenc->GetTime(PTS, DTS);
      out->SetTime(PTS, DTS);
      //frames.curenc->SetTime(m_PTS, m_DTS); // moved to be set after copy to buffer
    } else { // I or P
      frames.curenc->GetTime(PTS, DTS);
      if(PTS > 0) m_PTS = PTS;
      else {
        if(m_PTS < 0)
          m_PTS = 0;
        m_PTS += 1./encodeInfo.info.framerate;
      }
      if(DTS > 0) m_DTS = DTS;
      else        m_DTS = m_PTS - P_distance/encodeInfo.info.framerate;
      out->SetTime(m_PTS, m_DTS);
    }
  } else { // consequent input DTS
    frames.curenc->GetTime(PTS, DTS);
    if(PTS > 0) m_DTS = DTS;
    else {
      if(m_DTS < 0)
        m_DTS = 0;
      else
        m_DTS += 1./encodeInfo.info.framerate;
    }
    if(PTS > 0) m_PTS = PTS;
    else {
      if(picture_coding_type == MPEG2_B_PICTURE)
        m_PTS = m_DTS;
      else
        m_PTS = m_DTS + P_distance/encodeInfo.info.framerate;
    }
    out->SetTime(m_PTS, m_DTS);
  }

  return UMC_OK;
}
#endif // UMC_ENABLE_MPEG2_VIDEO_ENCODER


#if defined (WIN32)
#pragma warning (pop)
#endif