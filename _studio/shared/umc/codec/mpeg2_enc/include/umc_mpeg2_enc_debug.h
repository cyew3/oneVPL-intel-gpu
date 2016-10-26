//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2002-2011 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_ENCODER)

#ifndef __UMC_MPEG2_ENC_DEBUG_H
#define __UMC_MPEG2_ENC_DEBUG_H
namespace UMC
{

//modify here
#define MP2_DBG_MANUAL_SETUP  0

#define MP2_DBG_DEBUG_LOG_FOLDER "C:\\tv\\enc_data1\\tmp\\encode_log"
#define MP2_DBG_EXTENDED_MODE 1//add encoded DCT values
#define MP2_DBG_ADD_REF_DATA  1//add referece data for Inter MBs

#define MP2_DBG_ADD_QUANT     1
#define MP2_DBG_ADD_MB_TYPE   1
#define MP2_DBG_ADD_MB_MVINFO 1
#define MP2_DBG_FRAME_MIN     0 //frame position in stream (not display order)
#define MP2_DBG_FRAME_MAX     1000


class MPEG2VideoEncoderBase;

typedef struct
{
  Ipp8u YBlockRec[2][2][128];//fwd|bwd, fld1|fld2, fld data
  Ipp8u UBlockRec[2][2][128];
  Ipp8u VBlockRec[2][2][128];

  Ipp16s encMbData[12][64];//reconstructed blocks
  int    Count[12];//coded_block_pattern
  Ipp32s motion_flag;
  Ipp32s quantizer;
} MbDebugInfo;

class MPEG2EncDebug{
  private:
  MPEG2VideoEncoderBase *encoder;
//additional MBinfo (debug data for each macroblock)
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
  public:
  MPEG2EncDebug()
  {
    debug_logs_folder = NULL;
    mb_debug_info = NULL;
  }
  ~MPEG2EncDebug()
  {
    Free();
  }
  void CreateDEBUGframeLog();
  void Init(MPEG2VideoEncoderBase *encoder_d);
  void Free();

  void GatherInterRefMBlocksData(int k,void *vector);
  void GatherInterBlockData(int k,Ipp16s *pMBlock,int blk,int Count);
  void GatherIntraBlockData(int k,Ipp16s *pMBlock,int blk,int Count,int intra_dc_shift);
  void SetMotionFlag(int k,int value);
};
}
#endif //__UMC_MPEG2_ENC_DEBUG_H
#endif //UMC_ENABLE_MPEG2_VIDEO_ENCODER