//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2002-2011 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE)

#ifndef __MFX_MPEG2_ENCODE_DEBUG_HW_H
#define __MFX_MPEG2_ENCODE_DEBUG_HW_H

//modify here
#define MP2_DBG_MANUAL_SETUP  0

#define MP2_DBG_DEBUG_LOG_FOLDER "C:\\tv\\enc_data1\\tmp\\enc_hw_log"
#define MP2_DBG_EXTENDED_MODE 1//add encoded DCT values
#define MP2_DBG_ADD_REF_DATA  1//add referece data for Inter MBs

#define MP2_DBG_ADD_QUANT     1
#define MP2_DBG_ADD_MB_TYPE   1
#define MP2_DBG_ADD_MB_MVINFO 1
#define MP2_DBG_FRAME_MIN     0 //frame position in stream (not display order)
#define MP2_DBG_FRAME_MAX     1000

class MFXVideoENCODEMPEG2_HW;

typedef struct 
{
  int ref_vfield_select[3][2];//fld1|fld2|frame, fwd|bwd 

  Ipp8u YBlockRec[2][2][128];//fwd|bwd, fld1|fld2, fld data
  Ipp8u UBlockRec[2][2][128];
  Ipp8u VBlockRec[2][2][128];

  Ipp16s encMbData[12][64];//reconstructed blocks
  Ipp32s Count[12];//coded_block_pattern
  Ipp32s no_motion_flag;
  Ipp32s quant_skip_flag;
  Ipp32s quantizer;
  Ipp32s skipped;

} MbDebugInfo;


class MPEG2EncodeDebug_HW
{

  typedef struct _MBInfo  // macroblock information
  {
    mfxI32 mb_type;               // intra/forward/backward/interpolated
    mfxI32 dct_type;              // field/frame DCT
    mfxI32 prediction_type;       // MC_FRAME/MC_FIELD
    IppiPoint MV[2][2];           // motion vectors [vecnum][F/B]
    IppiPoint MV_P[2];            // motion vectors from P frame [vecnum]
    mfxI32 mv_field_sel[3][2];    // motion vertical field select:
    // the first index: 0-top field, 1-bottom field, 2-frame;
    // the second index:0-forward, 1-backward
    mfxI32 skipped;

  } MBInfo;


  typedef struct _MPEG2FrameState
  {
    // Input frame
    mfxU8       *Y_src;
    mfxU8       *U_src;
    mfxU8       *V_src;
    // Input reference reconstructed frames
    Ipp8u       *YRecFrame[2][2];   // [top/bottom][fwd/bwd]
    Ipp8u       *URecFrame[2][2];
    Ipp8u       *VRecFrame[2][2];
    // Output reconstructed frame
    mfxU8       *Y_out;
    mfxU8       *U_out;
    mfxU8       *V_out;
    mfxI32      YFrameHSize;
    mfxI32      UVFrameHSize;
    mfxU32      needFrameUnlock[4]; // 0-src, 1,2-Rec, 3-out

  } MPEG2FrameState;


private:
//additional MBinfo (debug data for each macroblock)
  MFXVideoENCODEMPEG2_HW *encode_hw;
  MbDebugInfo *mb_debug_info;

//debug parameters
  char *debug_logs_folder;
  int  use_extended_log;
  int  use_reference_log;
  int  use_manual_setup;
  int  use_mb_type;
  int  use_mb_MVinfo;
  int  use_quantizer_info;
  int  minFrameIndex;
  int  maxFrameIndex;
  int  mb_num;

public:
  MPEG2EncodeDebug_HW()
  {
    debug_logs_folder = NULL;
    mb_debug_info = NULL;
  }


  ~MPEG2EncodeDebug_HW()
  {
    Free();
  }


  void CreateDEBUGframeLog();
  void Init(MFXVideoENCODEMPEG2_HW *pEncode_hw);
  void Free();

  void GatherInterRefMBlocksData(int k,void *vector_in,void* state,void *mbinfo);
  void GatherBlockData(int k,int blk,int picture_coding_type,int quantiser_scale_value,Ipp16s *quantMatrix,Ipp16s *pMBlock,int Count,int intra_flag,int intra_dc_shift);
  void SetSkippedMb(int k);
  void SetNoMVMb(int k);
  void SetMotionFlag(int k,int value);
};

#endif //__MFX_MPEG2_ENCODE_DEBUG_HW_H
#endif //MFX_ENABLE_MPEG2_VIDEO_PAK
/* EOF */
