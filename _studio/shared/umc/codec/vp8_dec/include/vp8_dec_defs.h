//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2014 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VP8_VIDEO_DECODER)

#ifndef __VP8_DEC_H__
#define __VP8_DEC_H__

using namespace UMC;

#include "ippcore.h"
#include "umc_structures.h"


namespace UMC
{

#if (defined(__INTEL_COMPILER) || defined(_MSC_VER)) && !defined(_WIN32_WCE)
#define __ALIGN16(type, name, size) \
  __declspec (align(16)) type name[size]
#else
#if defined(LINUX32) || defined(LINUX64)
#define __ALIGN16(type, name, size) \
  type __attribute__((aligned(0x10))) name[size]
#else
#if defined(_WIN64) || defined(WIN64)
#define __ALIGN16(type, name, size) \
  Ipp8u _a16_##name[(size)*sizeof(type)+15]; type *name = (type*)(((Ipp64s)(_a16_##name) + 15) & ~15)
#else
#define __ALIGN16(type, name, size) \
  Ipp8u _a16_##name[(size)*sizeof(type)+15]; type *name = (type*)(((Ipp32s)(_a16_##name) + 15) & ~15)
#endif
#endif
#endif


#define VP8_MAX_NUM_OF_TOKEN_PARTITIONS 8

#define VP8_START_CODE_FOUND(ptr) ((ptr)[0] == 0x9d && (ptr)[1] == 0x01 && (ptr)[2] == 0x2a)

#define vp8_CLIP(x, min, max) if ((x) < (min)) (x) = (min); else if ((x) > (max)) (x) = (max)
#define vp8_CLIPR(x, max) if ((x) > (max)) (x) = (max)

#define vp8_CLIP255(x) ((x) > 255) ? 255 : ((x) < 0) ? 0 : (x)

#define VP8_MIN_QP 0
#define VP8_MAX_QP 127

#define VP8_MAX_LF_LEVEL 63

#define vp8dec_Malloc ippMalloc
#define vp8dec_Free   ippFree

enum
{
  VP8_MB_DC_PRED = 0, /* predict DC using row above and column to the left */
  VP8_MB_V_PRED,      /* predict rows using row above */
  VP8_MB_H_PRED,      /* predict columns using column to the left */
  VP8_MB_TM_PRED,     /* propagate second differences a la "true motion" */
  VP8_MB_B_PRED,      /* each Y block is independently predicted */
  VP8_NUM_MB_MODES_UV = VP8_MB_B_PRED, /* first four modes apply to chroma */
  VP8_NUM_MB_MODES_Y  /* all modes apply to luma */
};


enum
{
  VP8_MV_NEAREST   = VP8_NUM_MB_MODES_Y, /* use "nearest" motion vector for entire MB */
  VP8_MV_NEAR,                           /* use "next nearest" */
  VP8_MV_ZERO,                           /* use zero */
  VP8_MV_NEW,                            /* use explicit offset from implicit */
  VP8_MV_SPLIT,                          /* use multiple motion vectors */
  VP8_NUM_MV_MODES = VP8_MV_SPLIT + 1 - VP8_MV_NEAREST
};

enum
{
  VP8_B_DC_PRED = 0, /* predict DC using row above and column to the left */
  VP8_B_TM_PRED,     /* propagate second differences a la "true motion" */
  VP8_B_VE_PRED,     /* predict rows using row above */
  VP8_B_HE_PRED,     /* predict columns using column to the left */
  VP8_B_LD_PRED,     /* southwest (left and down) 45 degree diagonal prediction */
  VP8_B_RD_PRED,     /* southeast (right and down) "" */
  VP8_B_VR_PRED,     /* SSE (vertical right) diagonal prediction */
  VP8_B_VL_PRED,     /* SSW (vertical left) "" */
  VP8_B_HD_PRED,     /* ESE (horizontal down) "" */
  VP8_B_HU_PRED,     /* ENE (horizontal up) "" */
  VP8_NUM_INTRA_BLOCK_MODES,

  VP8_B_MV_LEFT = VP8_NUM_INTRA_BLOCK_MODES,
  VP8_B_MV_ABOVE,
  VP8_B_MV_ZERO,
  VP8_B_MV_NEW,
  VP8_NUM_B_MV_MODES = VP8_B_MV_NEW + 1 - VP8_NUM_INTRA_BLOCK_MODES
};


enum
{
  VP8_COEFF_PLANE_Y_AFTER_Y2 = 0,
  VP8_COEFF_PLANE_Y2,
  VP8_COEFF_PLANE_UV,
  VP8_COEFF_PLANE_Y,
  VP8_NUM_COEFF_PLANES
};


#define VP8_NUM_COEFF_BANDS 8
#define VP8_NUM_LOCAL_COMPLEXITIES 3


enum
{
  VP8_COEFF_NODE_EOB = 0,
  VP8_COEFF_NODE_0,
  VP8_COEFF_NODE_1,
};


#define VP8_NUM_COEFF_NODES 11


enum
{
  VP8_MV_IS_SHORT  = 0,
  VP8_MV_SIGN,
  VP8_MV_SHORT,
  VP8_MV_LONG      = 9,
  VP8_MV_LONG_BITS = 10,
  VP8_NUM_MV_PROBS = VP8_MV_LONG + VP8_MV_LONG_BITS
};


extern const Ipp8u  vp8_range_normalization_shift[64];

extern const Ipp32s vp8_quant_dc[VP8_MAX_QP + 1 + 32];
extern const Ipp32s vp8_quant_ac[VP8_MAX_QP + 1 + 32];
extern const Ipp32s vp8_quant_dc2[VP8_MAX_QP + 1 + 32];

extern const Ipp8u  vp8_kf_mb_mode_y_probs[VP8_NUM_MB_MODES_Y - 1];
extern const Ipp8u  vp8_kf_mb_mode_uv_probs[VP8_NUM_MB_MODES_UV - 1];
extern const Ipp8u  vp8_kf_block_mode_probs[VP8_NUM_INTRA_BLOCK_MODES][VP8_NUM_INTRA_BLOCK_MODES][VP8_NUM_INTRA_BLOCK_MODES-1];

extern const Ipp8u  vp8_mb_mode_y_probs[VP8_NUM_MB_MODES_Y - 1];
extern const Ipp8u  vp8_mb_mode_uv_probs[VP8_NUM_MB_MODES_UV - 1];
extern const Ipp8u  vp8_block_mode_probs [VP8_NUM_INTRA_BLOCK_MODES - 1];

extern const Ipp8u  vp8_default_coeff_probs[VP8_NUM_COEFF_PLANES][VP8_NUM_COEFF_BANDS][VP8_NUM_LOCAL_COMPLEXITIES][VP8_NUM_COEFF_NODES];
extern const Ipp8u  vp8_coeff_update_probs[VP8_NUM_COEFF_PLANES][VP8_NUM_COEFF_BANDS][VP8_NUM_LOCAL_COMPLEXITIES][VP8_NUM_COEFF_NODES];

extern const Ipp8s  vp8_mb_mode_y_tree[2*(VP8_NUM_MB_MODES_Y - 1)];
extern const Ipp8s  vp8_kf_mb_mode_y_tree[2*(VP8_NUM_MB_MODES_Y - 1)];
extern const Ipp8s  vp8_mb_mode_uv_tree[2*(VP8_NUM_MB_MODES_UV - 1)];
extern const Ipp8s  vp8_block_mode_tree[2*(VP8_NUM_INTRA_BLOCK_MODES - 1)];

extern const Ipp32u  vp8_mbmode_2_blockmode_u32[VP8_NUM_MB_MODES_Y];

extern const Ipp8u  vp8_mv_update_probs[2][VP8_NUM_MV_PROBS];
extern const Ipp8u  vp8_default_mv_contexts[2][VP8_NUM_MV_PROBS];


// get this from h264 sources - need test?????
extern const Ipp8u vp8_ClampTbl[768];

#define clip(x) IPP_MIN(256, IPP_MAX(x, -256))
#define vp8_GetClampTblValue(x) vp8_ClampTbl[256 + (x)]
#define vp8_ClampTblLookup(x, y) vp8_GetClampTblValue((x) + clip(y))

#define vp8_clamp8s(x) vp8_ClampTblLookup((x), 128) - 128
#define vp8_clamp8u(x) vp8_ClampTblLookup((x),  0)


enum {
  VP8_ALT_QUANT = 0,
  VP8_ALT_LOOP_FILTER,
  VP8_NUM_OF_MB_FEATURES
};

#define VP8_MAX_NUM_OF_SEGMENTS 4

enum {
  VP8_ALT_LOOP_FILTER_BITS = 6,
  VP8_ALT_QUANT_BITS = 7
};


enum {
  VP8_LOOP_FILTER_NORMAL = 0,
  VP8_LOOP_FILTER_SIMPLE
};


#define VP8_NUM_OF_SEGMENT_TREE_PROBS 3


enum {
  VP8_INTRA_FRAME = 0,
  VP8_LAST_FRAME,
  VP8_GOLDEN_FRAME,
  VP8_ALTREF_FRAME,
  VP8_NUM_OF_REF_FRAMES
};


#define VP8_NUM_OF_MODE_LF_DELTAS 4


#define VP8_BILINEAR_INTERP 1
#define VP8_CHROMA_FULL_PEL 2


enum
{
  VP8_MV_TOP_BOTTOM = 0, /* two pieces {0...7} and {8...15} */
  VP8_MV_LEFT_RIGHT,     /* {0,1,4,5,8,9,12,13} and {2,3,6,7,10,11,14,15} */
  VP8_MV_QUARTERS,       /* {0,1,4,5}, {2,3,6,7}, {8,9,12,13}, {10,11,14,15} */
  VP8_MV_16,             /* every subblock gets its own vector {0} ... {15} */
  VP8_MV_NUM_PARTITIONS
};


typedef struct _vp8_FrameInfo
{
  //Ipp8u frameType;
  FrameType frameType;// ???

  Ipp8u showFrame;

  Ipp8u interpolationFlags; // bit0: bilinear interpolation (1 - on, 0 - off);
                            // bit1: chroma full pel (1 - on, 0 - off)

  Ipp8u segmentationEnabled;
  Ipp8u updateSegmentMap;
  Ipp8u updateSegmentData;
  Ipp8u segmentAbsMode;

  Ipp8s segmentFeatureData[VP8_NUM_OF_MB_FEATURES][VP8_MAX_NUM_OF_SEGMENTS];
  Ipp8u segmentTreeProbabilities[VP8_NUM_OF_SEGMENT_TREE_PROBS];

  Ipp8u loopFilterType;
  Ipp8u loopFilterLevel;
  Ipp8u sharpnessLevel;

  Ipp8u mbLoopFilterAdjust;
  Ipp8u modeRefLoopFilterDeltaUpdate;

  Ipp8s refLoopFilterDeltas[VP8_NUM_OF_REF_FRAMES];
  Ipp8s modeLoopFilterDeltas[VP8_NUM_OF_MODE_LF_DELTAS];

  Ipp8u numTokenPartitions;

  Ipp32u mbPerCol;
  Ipp32u mbPerRow;
  Ipp32u mbStep;

  Ipp8u mbSkipEnabled;
  Ipp8u skipFalseProb;
  Ipp8u intraProb;
  Ipp8u goldProb;
  Ipp8u lastProb;

  Ipp8u *blContextUp;
  Ipp8u  blContextLeft[9];

  Ipp32s numPartitions;
  Ipp32s partitionSize[VP8_MAX_NUM_OF_TOKEN_PARTITIONS];
  Ipp8u *partitionStart[VP8_MAX_NUM_OF_TOKEN_PARTITIONS];

  Ipp32s m_DPBSize; //???? to ask T.K. what is whis

  //Ipp8u version;
  Ipp8u color_space_type;
  Ipp8u clamping_type;

  Ipp8u h_scale;  // not used by the decoder
  Ipp8u v_scale;  // not used by the decoder

  Ipp16s frameWidth;  // width  + cols_padd
  Ipp16s frameHeight; // height + rows_padd

  IppiSize frameSize; // actual width/height without padding cols/rows

  Ipp32u firstPartitionSize;
  Ipp16u version;

  Ipp32u entropyDecSize;
  Ipp32u entropyDecOffset;

} vp8_FrameInfo;


typedef struct _vp8_QuantInfo
{
  Ipp32s yacQP; // abs value, always specified

  Ipp32s y2acQ[VP8_MAX_NUM_OF_SEGMENTS];
  Ipp32s y2dcQ[VP8_MAX_NUM_OF_SEGMENTS];
  Ipp32s yacQ[VP8_MAX_NUM_OF_SEGMENTS];
  Ipp32s ydcQ[VP8_MAX_NUM_OF_SEGMENTS];
  Ipp32s uvacQ[VP8_MAX_NUM_OF_SEGMENTS];
  Ipp32s uvdcQ[VP8_MAX_NUM_OF_SEGMENTS];

  // q deltas
  Ipp32s ydcDeltaQP;
  Ipp32s y2acDeltaQP;
  Ipp32s y2dcDeltaQP;
  Ipp32s uvacDeltaQP;
  Ipp32s uvdcDeltaQP;

  Ipp32s lastGoldenKeyQP;

  const Ipp32s *pYacQ;
  const Ipp32s *pY2acQ;
  const Ipp32s *pUVacQ;
  const Ipp32s *pYdcQ;
  const Ipp32s *pY2dcQ;
  const Ipp32s *pUVdcQ;

} vp8_QuantInfo;

#pragma warning (disable: 4201)
union vp8_MotionVector
{
  struct {
    Ipp16s  mvx;
    Ipp16s  mvy;
  };
  Ipp32s s32;
};


/*
typedef struct _vp8_BlockInfo
{
  Ipp8u            mode;
  vp8_MotionVector mv;

} vp8_BlockInfo;
*/


typedef struct _vp8_LoopFilterInfo
{
  Ipp8u  filterLevel;
  Ipp8u  interiorLimit;
  Ipp8u  skipBlockFilter;

} vp8_LoopFilterInfo;


#pragma warning (disable: 4324)
typedef struct _vp8_MbInfo
{
  __ALIGN16(Ipp16s, coeffs, 25*16);

  Ipp8u            blockMode[16]; // should be 4-byte aligned
  vp8_MotionVector blockMV[16];
  vp8_MotionVector mv;
  Ipp8u            partitions;

  Ipp8u segmentID;
  Ipp8u skipCoeff;

  Ipp8u numNNZ[25];

  Ipp8u mode;
  Ipp8u modeUV;

  Ipp8u refFrame;    

  vp8_LoopFilterInfo lfInfo;
//  vp8_BlockInfo blockInfo[16];
} vp8_MbInfo;


typedef struct _vp8_RefreshInfo
{
  Ipp8u refreshRefFrame; // (refreshRefFrame & 2) - refresh golden frame, (refreshRefFrame & 1) - altref frame
  Ipp8u copy2Golden;
  Ipp8u copy2Altref;
  Ipp8u refFrameBiasTable[4];

  Ipp8u refreshProbabilities;
  Ipp8u refreshLastFrame;

} vp8_RefreshInfo;


typedef struct _vp8_FrameProbabilities
{
  Ipp8u mbModeProbY[VP8_NUM_MB_MODES_Y - 1];   // vp8_mb_mode_y_probs
  Ipp8u mbModeProbUV[VP8_NUM_MB_MODES_UV - 1]; // vp8_mb_mode_uv_probs
  Ipp8u mvContexts[2][VP8_NUM_MV_PROBS];       // vp8_default_mv_contexts


  Ipp8u coeff_probs[VP8_NUM_COEFF_PLANES][VP8_NUM_COEFF_BANDS][VP8_NUM_LOCAL_COMPLEXITIES][VP8_NUM_COEFF_NODES];
} vp8_FrameProbabilities;


enum {
  VP8_FIRST_PARTITION = 0,
  VP8_TOKEN_PARTITION_0,
  VP8_TOKEN_PARTITION_1,
  VP8_TOKEN_PARTITION_2,
  VP8_TOKEN_PARTITION_3,
  VP8_TOKEN_PARTITION_4,
  VP8_TOKEN_PARTITION_5,
  VP8_TOKEN_PARTITION_6,
  VP8_TOKEN_PARTITION_7,
  VP8_TOKEN_PARTITION_8,
  VP8_MAX_NUMBER_OF_PARTITIONS
};


typedef struct _vp8_FrameData
{
  Ipp8u* data_y;
  Ipp8u* data_u;
  Ipp8u* data_v;

  Ipp8u* base_y;
  Ipp8u* base_u;
  Ipp8u* base_v;

  IppiSize size;

  Ipp32s step_y;
  Ipp32s step_uv;

  Ipp32s border_size;

} vp8_FrameData;


} // namespace UMC

#endif // __VP8_DEC_H__

#endif // UMC_ENABLE_VP8_VIDEO_DECODER
