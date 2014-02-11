//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_DEFS_H__
#define __MFX_H265_DEFS_H__

#pragma warning(disable: 4100; disable: 4505; disable: 4127; disable: 4324)

#if (defined(__INTEL_COMPILER) || defined(_MSC_VER)) && !defined(_WIN32_WCE)
#define __ALIGN32 __declspec (align(32))
#define __ALIGN16 __declspec (align(16))
#define __ALIGN8 __declspec (align(8))
#elif defined(__GNUC__)
#define __ALIGN32 __attribute__ ((aligned (32)))
#define __ALIGN16 __attribute__ ((aligned (16)))
#define __ALIGN8 __attribute__ ((aligned (8)))
#else
#define __ALIGN32
#define __ALIGN16
#define __ALIGN8
#endif

#include "mfxdefs.h"
#include "ippdefs.h"
#include "ipps.h"
#include <float.h>

namespace H265Enc {

#define H265_Malloc  ippMalloc
#define H265_Free    ippFree

#if defined(_WIN32) || defined(_WIN64)
  #define H265_FORCEINLINE __forceinline
  #define H265_NONLINE __declspec(noinline)
#else
  #define H265_FORCEINLINE __attribute__((always_inline))
  #define H265_NONLINE __attribute__((noinline))
#endif

static void H265_FORCEINLINE small_memcpy( void* dst, const void* src, int len )
{
#if 0 // defined( __INTEL_COMPILER ) // || defined( __GNUC__ )  // TODO: check with GCC // AL: temporarely disable optimized path for Beta
    // 128-bit loads/stores first with then REP MOVSB, aligning dst on 16-bit to avoid costly store splits
    int peel = (0xf & (-(size_t)dst));
    __asm__ ( "cld" );
    if (peel) {
        if (peel > len)
            peel = len;
        len -= peel;
        __asm__ ( "rep movsb" : "+c" (peel), "+S" (src), "+D" (dst) :: "memory" );
    }
    while (len > 15) {
        __m128i v_tmp;
        __asm__ ( "movdqu (%[src_]), %%xmm0; movdqu %%xmm0, (%[dst_])" : : [src_] "S" (src), [dst_] "D" (dst) : "%xmm0", "memory" );
        src = 16 + (const Ipp8u*)src; dst = 16 + (Ipp8u*)dst; len -= 16;
    }
    if (len > 0)
        __asm__ ( "rep movsb" : "+c" (len), "+S" (src), "+D" (dst) :: "memory" );
#else
    ippsCopy_8u((const Ipp8u*)src, (Ipp8u*)dst, len);
#endif
}

//#define DEBUG_CABAC

#ifdef DEBUG_CABAC
extern int DEBUG_CABAC_PRINT;
#endif

#define BIT_COST(bits_shifted) ((bits_shifted)*m_rdLambda)
#define BIT_COST_INTER(bits_shifted) ((bits_shifted)*m_rdLambdaInter)
#define BIT_COST_INTER_MV(bits_shifted) ((bits_shifted)*m_rdLambdaInterMv)
#define BIT_COST_SHIFT 8
#define NUM_CAND_MAX_1 36
#define NUM_CAND_MAX_2 36

#define MAX_NUM_REF_FRAMES  32

#define HEVC_ANALYSE_CHROMA                    (1 << 0)

#define DATA_ALIGN 64
#define H265ENC_UNREFERENCED_PARAMETER(X) X=X

#define BIT_DEPTH_LUMA 8
#define BIT_DEPTH_CHROMA 8

#define     MAX_CU_DEPTH            6 // 7                           // log2(LCUSize)
#define     MAX_CU_SIZE             (1<<(MAX_CU_DEPTH))         // maximum allowable size of CU
#define     MAX_NUM_PARTITIONS 256

#define AMVP_MAX_NUM_CANDS          2           ///< max number of final candidates
#define MRG_MAX_NUM_CANDS           5           // from 2 to 5
//#define AMVP_MAX_NUM_CANDS_MEM      3           ///< max number of candidates
//#define MRG_MAX_NUM_CANDS_SIGNALED         5   //<G091: value of maxNumMergeCand signaled in slice header

#define MLS_GRP_NUM                         64     ///< G644 : Max number of coefficient groups, max(16, 64)
#define MLS_CG_SIZE                         4      ///< G644 : Coefficient group size of 4x4

#define C1FLAG_NUMBER               8 // maximum number of largerThan1 flag coded in one chunk :  16 in HM5
#define C2FLAG_NUMBER               1 // maximum number of largerThan2 flag coded in one chunk:  16 in HM5

#define INTRA_TRANSFORMSKIP 1
#define LOSSLESS_CODING                   1  ///< H0530: lossless and lossy (mixed) coding
#define CU_LEVEL_TRANSQUANT_BYPASS        1  ///< I0529: CU level flag for transquant bypass
#define ADAPTIVE_QP_SELECTION               1      ///< G382: Adaptive reconstruction levels, non-normative part for adaptive QP selection
#define AMP_MRG                               1           ///< encoder only force merge for AMP partition (no motion search for AMP)
#define AHG6_ALF_OPTION2                 1  ///< I0157: AHG6 ALF baseline Option 2 RA- Variant 2
#define PRED_QP_DERIVATION               1  ///< I0219: Changing cu_qp_delta parsing to enable CU-level processing
#define ADAPTIVE_QP_SELECTION            1      ///< G382: Adaptive reconstruction levels, non-normative part for adaptive QP selection
#define BIPRED_RESTRICT_SMALL_PU         1  ///< I0297: bi-prediction restriction by 4x8/8x4 PU
#define FIXED_SBH_THRESHOLD              1  ///< I0156: use fixed controlling threshold for Multiple Sign Bit Hiding (SBH)
#define SBH_THRESHOLD                    4  ///< I0156: value of the fixed SBH controlling threshold
#define LAST_CTX_DERIVATION              1  //< I0331: table removal of LAST context derivation
#define COEF_REMAIN_BINARNIZATION        1  ///< I0487: Binarization for coefficient level coding
#define SIMPLE_PARAM_UPDATE              1   ///<I0124 : Simplification of param update
#define POS_BASED_SIG_COEFF_CTX          1  ///< I0296: position based context index derivation for significant coeff flag for large TU
#define REMOVE_LASTTU_CBFDERIV           1  ///< I0152: CBF coding for last TU without derivation process
#define CBF_UV_UNIFICATION               1 // < I0332: Unified CBFU and CBFV coding

#define SLICEHEADER_SYNTAX_FIX 0

#define COEF_REMAIN_BIN 3
#define CU_DQP_TU_CMAX 5 //max number bins for truncated unary
#define CU_DQP_EG_k 0 //expgolomb order

#define MAX_NUM_TILE_COLUMNS 16
#define MAX_NUM_TILE_ROWS 16
#define MAX_NUM_LONG_TERM_PICS 8
#define MAX_PIC_HEIGHT_IN_CTBS 270
#define MAX_TEMPORAL_LAYERS 16
#define MAX_NUM_ENTRY_POINT_OFFSETS MAX(MAX_NUM_TILE_COLUMNS*MAX_NUM_TILE_ROWS,MAX_PIC_HEIGHT_IN_CTBS)

#undef  ABS
#define ABS(a)     (((a) < 0) ? (-(a)) : (a))
#undef  MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#define MAX_DOUBLE                  1.7e+308    ///< max. value of double-type value
#define Saturate(min_val, max_val, val) MAX((min_val), MIN((max_val), (val)))
#define MAX_UINT                    0xFFFFFFFFU ///< max. value of unsigned 32-bit integer


typedef Ipp16s CoeffsType;
typedef Ipp8u PixType;
typedef Ipp8s T_RefIdx;
typedef Ipp64f CostType;
#define COST_MAX DBL_MAX


#define H265_MAXNUMREF  4
#define PGOP_PIC_SIZE 4

const Ipp8u OPT_LAMBDA_PYRAMID = 1;

enum EnumSliceType {
    B_SLICE     = 0,
    P_SLICE     = 1,
    I_SLICE     = 2,
};
// texture component type
enum EnumTextType
{
    TEXT_LUMA,            ///< luma
    TEXT_CHROMA,          ///< chroma (U+V)
    TEXT_CHROMA_U,        ///< chroma U
    TEXT_CHROMA_V,        ///< chroma V
    TEXT_ALL,             ///< Y+U+V
    TEXT_NONE = 15
};

/// reference list index
enum EnumRefPicList
{
  REF_PIC_LIST_0 = 0,   ///< reference list 0
  REF_PIC_LIST_1 = 1,   ///< reference list 1
  REF_PIC_LIST_C = 2,   ///< combined reference list for uni-prediction in B-Slices
  REF_PIC_LIST_X = 100  ///< special mark
};

/// coefficient scanning type used in ACS
enum CoeffScanType
{
    COEFF_SCAN_ZIGZAG = 0,
    COEFF_SCAN_HOR,
    COEFF_SCAN_VER,
    COEFF_SCAN_DIAG
};

// supported prediction type
enum PredictionMode
{
    MODE_INTER,           ///< inter-prediction mode
    MODE_INTRA,           ///< intra-prediction mode
    MODE_NONE = 15
};

/// supported partition shape
enum PartitionSize
{
  PART_SIZE_2Nx2N,           ///< symmetric motion partition,  2Nx2N
  PART_SIZE_2NxN,            ///< symmetric motion partition,  2Nx N
  PART_SIZE_Nx2N,            ///< symmetric motion partition,   Nx2N
  PART_SIZE_NxN,             ///< symmetric motion partition,   Nx N
  PART_SIZE_2NxnU,           ///< asymmetric motion partition, 2Nx( N/2) + 2Nx(3N/2)
  PART_SIZE_2NxnD,           ///< asymmetric motion partition, 2Nx(3N/2) + 2Nx( N/2)
  PART_SIZE_nLx2N,           ///< asymmetric motion partition, ( N/2)x2N + (3N/2)x2N
  PART_SIZE_nRx2N,           ///< asymmetric motion partition, (3N/2)x2N + ( N/2)x2N
  PART_SIZE_NONE = 15
};


enum SplitDecisionMode
{
    SPLIT_NONE,
    SPLIT_TRY,
    SPLIT_MUST
};

enum CostOpt
{
    COST_PRED_TR_0,
    COST_REC_TR_0,
    COST_REC_TR_ALL,
    COST_REC_TR_MAX
};

enum IntraPredOpt
{
    INTRA_PRED_CALC,
    INTRA_PRED_IN_REC,
    INTRA_PRED_IN_BUF,
};

enum EnumPicClass
{
    DISPOSABLE_PIC = 0, // No references to this picture from others, need not be decoded
    REFERENCE_PIC  = 1, // Other pictures may refer to this one, must be decoded
    IDR_PIC        = 2  // Instantaneous Decoder Refresh.  All previous references discarded.
};

/// motion vector predictor direction used in AMVP
enum MVPDir
{
  MD_LEFT = 0,          ///< MVP of left block
  MD_ABOVE,             ///< MVP of above block
  MD_ABOVE_RIGHT,       ///< MVP of above right block
  MD_BELOW_LEFT,        ///< MVP of below left block
  MD_ABOVE_LEFT         ///< MVP of above left block
};

#define INTRA_PLANAR              0
#define INTRA_DC                  1
#define INTRA_VER                26
#define INTRA_HOR                10
#define INTRA_DM_CHROMA          36
#define NUM_CHROMA_MODE        5                     // total number of chroma modes

#define NUM_INTRA_MODE 36

#define SCAN_SET_SIZE                     16
#define LOG2_SCAN_SET_SIZE                4

enum InterDir {
    INTER_DIR_PRED_L0 = 1,
    INTER_DIR_PRED_L1 = 2,
};

enum NalUnitType
{
  NAL_UT_CODED_SLICE_TRAIL_N = 0,   // 0
  NAL_UT_CODED_SLICE_TRAIL_R,   // 1

  NAL_UT_CODED_SLICE_TSA_N,     // 2
  NAL_UT_CODED_SLICE_TLA,       // 3   // Current name in the spec: TSA_R

  NAL_UT_CODED_SLICE_STSA_N,    // 4
  NAL_UT_CODED_SLICE_STSA_R,    // 5

  NAL_UT_CODED_SLICE_RADL_N,    // 6
  NAL_UT_CODED_SLICE_DLP,       // 7 // Current name in the spec: RADL_R

  NAL_UT_CODED_SLICE_RASL_N,    // 8
  NAL_UT_CODED_SLICE_TFD,       // 9 // Current name in the spec: RASL_R

  NAL_UT_RESERVED_10,
  NAL_UT_RESERVED_11,
  NAL_UT_RESERVED_12,
  NAL_UT_RESERVED_13,
  NAL_UT_RESERVED_14,
  NAL_UT_RESERVED_15,

  NAL_UT_CODED_SLICE_BLA,       // 16   // Current name in the spec: BLA_W_LP
  NAL_UT_CODED_SLICE_BLANT,     // 17   // Current name in the spec: BLA_W_DLP
  NAL_UT_CODED_SLICE_BLA_N_LP,  // 18
  NAL_UT_CODED_SLICE_IDR,       // 19  // Current name in the spec: IDR_W_DLP
  NAL_UT_CODED_SLICE_IDR_N_LP,  // 20
  NAL_UT_CODED_SLICE_CRA,       // 21
  NAL_UT_RESERVED_22,
  NAL_UT_RESERVED_23,

  NAL_UT_RESERVED_24,
  NAL_UT_RESERVED_25,
  NAL_UT_RESERVED_26,
  NAL_UT_RESERVED_27,
  NAL_UT_RESERVED_28,
  NAL_UT_RESERVED_29,
  NAL_UT_RESERVED_30,
  NAL_UT_RESERVED_31,

  NAL_UT_VPS,                   // 32
  NAL_UT_SPS,                   // 33
  NAL_UT_PPS,                   // 34
  NAL_UT_ACCESS_UNIT_DELIMITER, // 35
  NAL_UT_EOS,                   // 36
  NAL_UT_EOB,                   // 37
  NAL_UT_FILLER_DATA,           // 38
  NAL_UT_SEI,                   // 39 Prefix SEI
  NAL_UT_SEI_SUFFIX,            // 40 Suffix SEI
};

struct H265VideoParam;
class H265Encoder;
class H265Frame;

struct EncoderRefPicListStruct
{
    H265Frame *m_RefPicList[MAX_NUM_REF_FRAMES + 1];
    Ipp8s m_Tb[MAX_NUM_REF_FRAMES+1];
    Ipp8u m_IsLongTermRef[MAX_NUM_REF_FRAMES+1];
};

struct EncoderRefPicList
{
    EncoderRefPicListStruct m_RefPicListL0;
    EncoderRefPicListStruct m_RefPicListL1;
};    // EncoderRefPicList

inline Ipp32s H265_CeilLog2(Ipp32s a) {
    Ipp32s r = 0;
    while(a>(1<<r)) r++;
    return r;
}

} // namespace

//#include "mfx_h265_encode.h"

#endif // __MFX_H265_DEFS_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
