//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
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
#include "ippi.h"
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

static inline IppStatus _ippsSet(Ipp8u val, Ipp8u* pDst, int len )
{
    return ippsSet_8u(val, pDst, len);
}

static inline IppStatus _ippsSet(Ipp16u val, Ipp16u* pDst, int len )
{
    return ippsSet_16s(val, (Ipp16s*)pDst, len);
}

static inline IppStatus _ippsCopy( const Ipp8u* pSrc, Ipp8u* pDst, int len )
{
    return ippsCopy_8u(pSrc, pDst, len);
}

static inline IppStatus _ippsCopy( const Ipp16u* pSrc, Ipp16u* pDst, int len )
{
    return ippsCopy_16s((Ipp16s*)pSrc, (Ipp16s*)pDst, len);
}

static inline IppStatus _ippsCopy( const Ipp16s* pSrc, Ipp16s* pDst, int len )
{
    return ippsCopy_16s(pSrc, pDst, len);
}

static inline IppStatus _ippiCopy_C1R(const Ipp8u *pSrc, Ipp32s srcStepPix, Ipp8u *pDst, Ipp32s dstStepPix, IppiSize roiSize)
{
    return ippiCopy_8u_C1R(pSrc, srcStepPix, pDst, dstStepPix, roiSize);
}

static inline IppStatus _ippiCopy_C1R(const Ipp16u *pSrc, Ipp32s srcStepPix, Ipp16u *pDst, Ipp32s dstStepPix, IppiSize roiSize)
{
    return ippiCopy_16u_C1R(pSrc, srcStepPix*2, pDst, dstStepPix*2, roiSize);
}

static inline IppStatus _ippiTranspose_C1R(Ipp8u *pSrc, Ipp32s srcStepPix, Ipp8u *pDst, Ipp32s dstStepPix, IppiSize roiSize)
{
    return ippiTranspose_8u_C1R(pSrc, srcStepPix, pDst, dstStepPix, roiSize);
}

static inline IppStatus _ippiTranspose_C1R(Ipp16u *pSrc, Ipp32s srcStepPix, Ipp16u *pDst, Ipp32s dstStepPix, IppiSize roiSize)
{
    return ippiTranspose_16u_C1R(pSrc, srcStepPix*2, pDst, dstStepPix*2, roiSize);
}

//#define DEBUG_CABAC

#ifdef DEBUG_CABAC
extern int DEBUG_CABAC_PRINT;
#endif

//#define DUMP_COSTS_CU
//#define DUMP_COSTS_TU

#define BIT_COST(bits_shifted) ((bits_shifted)*m_rdLambda)
#define BIT_COST_INTER(bits_shifted) ((bits_shifted)*m_rdLambdaInter)
#define BIT_COST_INTER_MV(bits_shifted) ((bits_shifted)*m_rdLambdaInterMv)
#define BIT_COST_SHIFT 8
#define NUM_CAND_MAX_1 36
#define NUM_CAND_MAX_2 36

#define MAX_NUM_REF_FRAMES  32

#define HEVC_ANALYSE_CHROMA                    (1 << 0)
#define HEVC_COST_CHROMA                       (1 << 1)

#define DATA_ALIGN 64
#define H265ENC_UNREFERENCED_PARAMETER(X) X=X

#define     MAX_CU_DEPTH            6 // 7                           // log2(LCUSize)
#define     MAX_CU_SIZE             (1<<(MAX_CU_DEPTH))         // maximum allowable size of CU
#define     MAX_NUM_PARTITIONS 256

#define MAX_TOTAL_DEPTH (MAX_CU_DEPTH+4)


#define MLS_GRP_NUM                 64     ///< G644 : Max number of coefficient groups, max(16, 64)
#define MLS_CG_SIZE                 4      ///< G644 : Coefficient group size of 4x4

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

#define MAX_NUM_TILE_COLUMNS 20
#define MAX_NUM_TILE_ROWS 22
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
typedef Ipp64f CostType;
#define COST_MAX DBL_MAX


#define PGOP_PIC_SIZE 4

const Ipp32s MAX_NUM_AMVP_CANDS  = 2;
const Ipp32s MAX_NUM_MERGE_CANDS = 5;
const Ipp32s MAX_NUM_REF_IDX     = 4;
const Ipp32s OPT_LAMBDA_PYRAMID  = 1;

// PAQ/CALQ tools
#define  MAX_DQP (6)

enum {
    B_SLICE = 0,
    P_SLICE = 1,
    I_SLICE = 2,
};

#define SliceTypeIndex(type)    ((Ipp8u)(I_SLICE - type) & 0x3)

enum EnumSliceTypeIndex {
    B_NONREF     = 3
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
  PART_SIZE_NONE
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
};

enum IntraPredOpt
{
    INTRA_PRED_CALC,
    INTRA_PRED_IN_REC,
    INTRA_PRED_IN_BUF,
    INTER_PRED_IN_BUF,
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

static const Ipp32s MVP_LX_FLAG_BITS = 1;

enum InterDir {
    INTER_DIR_PRED_L0 = 1,
    INTER_DIR_PRED_L1 = 2,
};

enum EnumIntraAngMode {
    INTRA_ANG_MODE_ALL              = 1,
    INTRA_ANG_MODE_EVEN             = 2,
    INTRA_ANG_MODE_GRADIENT         = 3,
    INTRA_ANG_MODE_DC_PLANAR_ONLY   = 99,
    INTRA_ANG_MODE_DISABLE          = 100,

    INTRA_ANG_MODE_NUMBER           =  5
};

enum NalUnitType
{
  NAL_TRAIL_N = 0,
  NAL_TRAIL_R = 1,
  NAL_TSA_N   = 2,
  NAL_TSA_R   = 3,
  NAL_STSA_N  = 4,
  NAL_STSA_R  = 5,
  NAL_RADL_N  = 6,
  NAL_RADL_R  = 7,
  NAL_RASL_N  = 8,
  NAL_RASL_R  = 9,

  NAL_RSV_VCL_N10 = 10,
  NAL_RSV_VCL_R11 = 11,
  NAL_RSV_VCL_N12 = 12,
  NAL_RSV_VCL_R13 = 13,
  NAL_RSV_VCL_N14 = 14,
  NAL_RSV_VCL_R15 = 15,

  NAL_BLA_W_LP   = 16,
  NAL_BLA_W_RADL = 17,
  NAL_BLA_N_LP   = 18,
  NAL_IDR_W_RADL = 19,
  NAL_IDR_N_LP   = 20,
  NAL_CRA        = 21,

  NAL_RSV_IRAP_VCL22 = 22,
  NAL_RSV_IRAP_VCL23 = 23,

  NAL_RSV_VCL_24 = 24,
  NAL_RSV_VCL_25 = 25,
  NAL_RSV_VCL_26 = 26,
  NAL_RSV_VCL_27 = 27,
  NAL_RSV_VCL_28 = 28,
  NAL_RSV_VCL_29 = 29,
  NAL_RSV_VCL_30 = 30,
  NAL_RSV_VCL_31 = 31,

  NAL_VPS = 32,
  NAL_SPS = 33,
  NAL_PPS = 34,
  NAL_AUD = 35,
  NAL_EOS = 36,
  NAL_EOB = 37,
  NAL_FD  = 38,

  NAL_UT_PREFIX_SEI = 39,
  NAL_UT_SUFFIX_SEI = 40,

  NAL_RSV_NVCL_41 = 41,
  NAL_RSV_NVCL_42 = 42,
  NAL_RSV_NVCL_43 = 43,
  NAL_RSV_NVCL_44 = 44,
  NAL_RSV_NVCL_45 = 45,
  NAL_RSV_NVCL_46 = 46,
  NAL_RSV_NVCL_47 = 47,
};

class H265Frame;

struct RefPicList
{
    H265Frame *m_refFrames[MAX_NUM_REF_FRAMES + 1];
    Ipp8s m_deltaPoc[MAX_NUM_REF_FRAMES + 1];
    Ipp8u m_isLongTermRef[MAX_NUM_REF_FRAMES + 1];
    // extra details for frame threading
    Ipp32s m_refFramesCount; // number of reference frames in m_refFrames[]. must be the MAX (slice[sliceIdx]->num_ref_idx[ listIdx ], ...)
};

inline Ipp32s H265_CeilLog2(Ipp32s a) {
    Ipp32s r = 0;
    while(a>(1<<r)) r++;
    return r;
}

enum {
    SUBPEL_NO               = 1, // intpel only
    SUBPEL_BOX_HPEL_ONLY    = 2, // no quaterpel step
    SUBPEL_BOX              = 3, // halfpel & quaterpel
    SUBPEL_DIA              = 4, // halfpel & quaterpel
    SUBPEL_DIA_2STEP        = 5, // 2*halfpel & 2*quaterpel
    SUBPEL_FASTBOX_DIA_ORTH = 6, // Fast Box Half Pel + Dia Quarter + Orthogonal Update
};

} // namespace

//#include "mfx_h265_encode.h"

#endif // __MFX_H265_DEFS_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
