//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_DEFS_H__
#define __MFX_H265_DEFS_H__

#pragma warning(disable: 4100; disable: 4505; disable: 4127; disable: 4324)

#if (defined(__INTEL_COMPILER) || defined(_MSC_VER)) && !defined(_WIN32_WCE)
#define __ALIGN64 __declspec (align(64))
#define __ALIGN32 __declspec (align(32))
#define __ALIGN16 __declspec (align(16))
#define __ALIGN8 __declspec (align(8))
#elif defined(__GNUC__)
#define __ALIGN64 __attribute__ ((aligned (64)))
#define __ALIGN32 __attribute__ ((aligned (32)))
#define __ALIGN16 __attribute__ ((aligned (16)))
#define __ALIGN8 __attribute__ ((aligned (8)))
#else
#define __ALIGN64
#define __ALIGN32
#define __ALIGN16
#define __ALIGN8
#endif


#include "float.h"
#include "assert.h"
#include "ippdefs.h"
#include "ipps.h"
#include "ippi.h"
#include "mfxdefs.h"
#include "mfxla.h"
#include "mfx_ext_buffers.h"
#include "mfxbrc.h"

#define AMT_MAX_FRAME_SIZE
//#define AMT_MAX_FRAME_SIZE_DEBUG

#if defined(MFX_VA)
#define AMT_NEW_ICRA
#endif

#define AMT_DQP_FIX
#define AMT_HROI_PSY_AQ

#define MEMOIZE_SUBPEL_EXT_W (8+8)
#define MEMOIZE_SUBPEL_EXT_H (8+2)

#define MEMOIZE_NUMCAND MAX_NUM_MERGE_CANDS


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

static inline void H265_FORCEINLINE small_memcpy( void* dst, const void* src, int len )
{
#if  0 // defined( __INTEL_COMPILER ) // || defined( __GNUC__ )  // TODO: check with GCC // AL: temporarely disable optimized path for Beta
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

template<class T> inline T AlignValue(T value, mfxU32 alignment)
{
    assert((alignment & (alignment - 1)) == 0); // should be 2^n
    return static_cast<T>((value + alignment - 1) & ~(alignment - 1));
}

static inline IppStatus _ippsSet(Ipp8u val, Ipp8u* pDst, int len )
{
    return ippsSet_8u(val, pDst, len);
}

static inline IppStatus _ippsSet(Ipp16u val, Ipp16u* pDst, int len )
{
    return ippsSet_16s(val, (Ipp16s*)pDst, len);
}

static inline IppStatus _ippsSet(Ipp32u val, Ipp32u* pDst, int len )
{
    return ippsSet_32s(val, (Ipp32s*)pDst, len);
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

enum {
    MAX_NUM_ACTIVE_REFS = 15,
    MAX_DPB_SIZE = 16
};

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

#define MAX_NUM_LONG_TERM_PICS 8
#define MAX_PIC_HEIGHT_IN_CTBS 270
#define MAX_TEMPORAL_LAYERS 16

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
typedef Ipp32f CostType;
//#define COST_MAX DBL_MAX
#define COST_MAX IPP_MAXABS_32F


#define PGOP_PIC_SIZE 4

const Ipp32s MAX_NUM_AMVP_CANDS  = 2;
const Ipp32s MAX_NUM_MERGE_CANDS = 5;
const Ipp32s MAX_NUM_REF_IDX     = 4;
const Ipp32s OPT_LAMBDA_PYRAMID  = 1;

// PAQ/CALQ tools
#define  MAX_DQP (6)
// size of block for lookahead algorithms
#define SIZE_BLK_LA (8)

const Ipp32s LOG2_MIN_TU_SIZE = 2;

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
    MODE_OUT_OF_PIC = 2
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

enum {
    SEI_BUFFERING_PERIOD      = 0,
    SEI_PIC_TIMING            = 1,
    SEI_ACTIVE_PARAMETER_SETS = 129
};


enum {
    AMT_DQP_CAQ = 0x1,
    AMT_DQP_CAL = 0x2,
    AMT_DQP_PAQ = 0x4,
    AMT_DQP_PSY = 0x8,
    AMT_DQP_HROI = 0x10
};

#define AMT_DQP_PSY_HROI (AMT_DQP_PSY|AMT_DQP_HROI)

class Frame;

struct RefPicList
{
    Frame *m_refFrames[MAX_NUM_ACTIVE_REFS];
    Ipp8s m_deltaPoc[MAX_NUM_ACTIVE_REFS];
    Ipp8u m_isLongTermRef[MAX_NUM_ACTIVE_REFS];
    // extra details for frame threading
    Ipp32s m_refFramesCount; // number of reference frames in m_refFrames[]. must be the MAX (slice[sliceIdx]->num_ref_idx[ listIdx ], ...)
    Ipp32s m_listModFlag;
    Ipp8s m_listMod[MAX_NUM_ACTIVE_REFS];
};


typedef enum {
    TT_INIT_NEW_FRAME,
    TT_PAD_RECON,
    TT_GPU_SUBMIT,
    TT_GPU_WAIT,
    TT_ENCODE_CTU,
    TT_POST_PROC_CTU,
    TT_POST_PROC_ROW,
    TT_ENC_COMPLETE,
    TT_HUB,
    TT_PREENC_START,
    TT_PREENC_ROUTINE,
    TT_PREENC_END,
    TT_COMPLETE
} ThreadingTaskSpecifier;


class H265Encoder;
class H265FrameEncoder;
class Lookahead;

#define MAX_NUM_DEPENDENCIES 16
//#define DEBUG_NTM

#ifdef DEBUG_NTM
//#define VM_ASSERT_(f) if (!(f)) exit(1);
#define VM_ASSERT_ assert
#else
#define VM_ASSERT_ VM_ASSERT
#endif

struct ThreadingTask
{
    ThreadingTaskSpecifier action;

    union {
        H265FrameEncoder *fenc;     // for encode/postproc
        Lookahead *la;              // for lookahead
        Frame *frame;               // for gpu-submit/init-new-frame/enc-complete
        void *syncpoint;            // for gpu-wait
        void *ptr0;                 // reserved for zeroing
    };
    Ipp32s poc;                     // for all tasks, useful in debug
    union {
        struct {
            Ipp32s col;             // for encode, postproc, lookahead
            Ipp32s row;             // for encode, postproc, lookahead
        };
        struct {
            Ipp32s feiOp;           // for gpu-submit/gpu-wait
            Ipp16s listIdx;         // for gpu-submit/gpu-wait
            Ipp16s refIdx;          // for gpu-submit/gpu-wait
        };
        mfxFrameSurface1 *indata;   // for init-new-frame

        void *ptr1;                 // reserved for zeroing
    };

    volatile unsigned int numUpstreamDependencies;
    int numDownstreamDependencies;
    ThreadingTask *downstreamDependencies[MAX_NUM_DEPENDENCIES];

    volatile Ipp32u finished;
    // very useful for debug
#ifdef DEBUG_NTM
    ThreadingTask *upstreamDependencies[MAX_NUM_DEPENDENCIES];
#endif

    void Init(ThreadingTaskSpecifier action_, Ipp32s row_, Ipp32s col_, Ipp32s poc_, Ipp32s numUpstreamDependencies_, Ipp32s numDownstreamDependencies_) {
        assert(action_ != TT_GPU_SUBMIT && action_ != TT_GPU_WAIT && action_ != TT_INIT_NEW_FRAME);
        action = action_;
        row = row_;
        col = col_;
        poc = poc_;
        numUpstreamDependencies = numUpstreamDependencies_;
        numDownstreamDependencies = numDownstreamDependencies_;
        finished = 0;
    }

    void InitGpuSubmit(Frame *frame_, Ipp32s feiOp_, Ipp32s poc_) {
        action = TT_GPU_SUBMIT;
        frame = frame_;
        feiOp = feiOp_;
        poc = poc_;
        listIdx = 0;
        refIdx = 0;
        numUpstreamDependencies = 0;
        numDownstreamDependencies = 0;
        finished = 0;
    }

    void InitGpuWait(Ipp32s feiOp_, Ipp32s poc_) {
        action = TT_GPU_WAIT;
        feiOp = feiOp_;
        poc = poc_;
        listIdx = 0;
        refIdx = 0;
        syncpoint = NULL;
        numUpstreamDependencies = 0;
        numDownstreamDependencies = 0;
        finished = 0;
    }
    
    void InitNewFrame(Frame *frame_, mfxFrameSurface1 *indata_, Ipp32s poc_) {
        action = TT_INIT_NEW_FRAME;
        frame = frame_;
        indata = indata_;
        poc = poc_;
        numUpstreamDependencies = 0;
        numDownstreamDependencies = 0;
        finished = 0;
    }

    void InitPadRecon(Frame *frame_, Ipp32s poc_) {
        action = TT_PAD_RECON;
        frame = frame_;
        poc = poc_;
        numUpstreamDependencies = 0;
        numDownstreamDependencies = 0;
        finished = 0;
    }

    void InitEncComplete(Frame *frame_, Ipp32s poc_) {
        action = TT_ENC_COMPLETE;
        poc = poc_;
        frame = frame_;
        ptr1 = NULL;
        numUpstreamDependencies = 0;
        numDownstreamDependencies = 0;
        finished = 0;
    }

    void InitComplete(Ipp32s poc_) {
        action = TT_COMPLETE;
        poc = poc_;
        ptr0 = NULL;
        ptr1 = NULL;
        numUpstreamDependencies = 0;
        numDownstreamDependencies = 0;
        finished = 0;
    }
};

typedef enum {
    THREADING_WORKING,
    THREADING_PAUSE,
    THREADING_ERROR,
} ThreadingStage;

typedef enum {
    THREADING_ITASK_INI,
    THREADING_ITASK_WORKING,
    THREADING_ITASK_COMPLETE,
} ThreadingInputTaskStage;

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


template <class T> void Throw(const T &ex)
{
    assert(!"exception");
    throw ex;
}

template <class T> void ThrowIf(bool cond, const T &ex)
{
    if (cond)
        Throw(ex);
}

class NonCopyable
{
protected:
    NonCopyable() {}
    ~NonCopyable() {}
private:
    NonCopyable(const NonCopyable&);
    const NonCopyable& operator =(const NonCopyable&);
};

namespace MfxEnumShortAliases {
    enum { NV12=MFX_FOURCC_NV12, NV16=MFX_FOURCC_NV16, P010=MFX_FOURCC_P010, P210=MFX_FOURCC_P210 };

    enum { YUV420=MFX_CHROMAFORMAT_YUV420, YUV422=MFX_CHROMAFORMAT_YUV422 };

    enum { MAIN=MFX_PROFILE_HEVC_MAIN, MAIN10=MFX_PROFILE_HEVC_MAIN10, REXT=MFX_PROFILE_HEVC_REXT };

    enum { M10=MFX_LEVEL_HEVC_1, M20=MFX_LEVEL_HEVC_2, M21=MFX_LEVEL_HEVC_21, M30=MFX_LEVEL_HEVC_3, M31=MFX_LEVEL_HEVC_31, M40=MFX_LEVEL_HEVC_4, M41=MFX_LEVEL_HEVC_41,
           M50=MFX_LEVEL_HEVC_5, M51=MFX_LEVEL_HEVC_51, M52=MFX_LEVEL_HEVC_52, M60=MFX_LEVEL_HEVC_6, M61=MFX_LEVEL_HEVC_61, M62=MFX_LEVEL_HEVC_62,
           H40=M40|256, H41=M41|256, H50=M50|256, H51=M51|256, H52=M52|256, H60=M60|256, H61=M61|256, H62=M62|256 };

    enum { CBR=MFX_RATECONTROL_CBR, VBR=MFX_RATECONTROL_VBR, AVBR=MFX_RATECONTROL_AVBR, CQP=MFX_RATECONTROL_CQP, LA_EXT=MFX_RATECONTROL_LA_EXT };

    enum { ON=MFX_CODINGOPTION_ON, OFF=MFX_CODINGOPTION_OFF, UNK=MFX_CODINGOPTION_UNKNOWN };

    enum { SYSMEM=MFX_IOPATTERN_IN_SYSTEM_MEMORY, VIDMEM=MFX_IOPATTERN_IN_VIDEO_MEMORY, OPAQMEM=MFX_IOPATTERN_IN_OPAQUE_MEMORY };

    enum { MAX_12BIT=MFX_HEVC_CONSTR_REXT_MAX_12BIT, MAX_10BIT=MFX_HEVC_CONSTR_REXT_MAX_10BIT, MAX_8BIT=MFX_HEVC_CONSTR_REXT_MAX_8BIT,
           MAX_422=MFX_HEVC_CONSTR_REXT_MAX_422CHROMA, MAX_420=MFX_HEVC_CONSTR_REXT_MAX_420CHROMA, MAX_MONO=MFX_HEVC_CONSTR_REXT_MAX_MONOCHROME,
           CONSTR_INTRA=MFX_HEVC_CONSTR_REXT_INTRA, CONSTR_1PIC=MFX_HEVC_CONSTR_REXT_ONE_PICTURE_ONLY, CONSTR_LOWER_RATE=MFX_HEVC_CONSTR_REXT_LOWER_BIT_RATE };

    enum { MAIN_422_10=MAX_12BIT|MAX_10BIT|MAX_422|CONSTR_LOWER_RATE };

    enum { PROGR=MFX_PICSTRUCT_PROGRESSIVE, TFF=MFX_PICSTRUCT_FIELD_TFF, BFF=MFX_PICSTRUCT_FIELD_BFF };
};

    namespace CodecLimits {
        using namespace MfxEnumShortAliases;
#ifdef MFX_VA
        const Ipp32s MIN_TARGET_USAGE = 4;
        const Ipp32s MAX_TARGET_USAGE = 7;
        const Ipp32s MAX_GOP_REF_DIST = 8;
        const Ipp32s MAX_WIDTH        = 8192;
        const Ipp32s MAX_HEIGHT       = 4320;
        const Ipp32s SUP_ENABLE_CM[]  = { OFF, ON };
#else
        const Ipp32s MIN_TARGET_USAGE = 1;
        const Ipp32s MAX_TARGET_USAGE = 7;
        const Ipp32s MAX_GOP_REF_DIST = 16;
        const Ipp32s MAX_WIDTH        = 8192;
        const Ipp32s MAX_HEIGHT       = 4320;
        const Ipp32s SUP_ENABLE_CM[]  = { OFF };
#endif

        const Ipp32s MAX_NUM_TILE_COLS      = 20;
        const Ipp32s MAX_NUM_TILE_ROWS      = 22;
        const Ipp32s MIN_TILE_WIDTH         = 256;
        const Ipp32s MIN_TILE_HEIGHT        = 64;
        const Ipp32s MAX_NUM_SLICE          = (MAX_HEIGHT + 63) >> 6;
        const Ipp32s SUP_GOP_OPT_FLAG[]     = { MFX_GOP_CLOSED, MFX_GOP_STRICT, MFX_GOP_CLOSED|MFX_GOP_STRICT };
        const Ipp32s SUP_PROFILE[]          = { MAIN, MAIN10, REXT };
        const Ipp32s SUP_LEVEL[]            = { M10, M20, M21, M30, M31, M40, M41, M50, M51, M52, M60, M61, M62, H40, H41, H50, H51, H52, H60, H61, H62 };
        const Ipp32s SUP_RC_METHOD[]        = { CBR, VBR, AVBR, LA_EXT, CQP};
        const Ipp32s SUP_FOURCC[]           = { NV12, NV16, P010, P210 };
        const Ipp32s SUP_CHROMA_FORMAT[]    = { YUV420, YUV422 };
        const Ipp32s SUP_PIC_STRUCT[]       = { PROGR, TFF, BFF };
        const Ipp32s SUP_INTRA_ANG_MODE_I[] = { 1, 2, 3, 99 };
        const Ipp32s SUP_INTRA_ANG_MODE[]   = { 1, 2, 3, 99, 100 };
        const Ipp32s SUP_PATTERN_INT_PEL[]  = { 1, 100};
    };

    template<class T> inline void Zero(T &obj) { memset(&obj, 0, sizeof(obj)); }
    template<class T> inline void Copy(T &dst, const T &src) { memmove_s(&dst, sizeof(dst), &src, sizeof(src)); }

    template<class T> struct Type2Id;
    template<> struct Type2Id<mfxExtOpaqueSurfaceAlloc> { enum { id = MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION }; };
    template<> struct Type2Id<mfxExtCodingOptionHEVC>   { enum { id = MFX_EXTBUFF_HEVCENC }; };
#ifdef MFX_UNDOCUMENTED_DUMP_FILES
    template<> struct Type2Id<mfxExtDumpFiles>          { enum { id = MFX_EXTBUFF_DUMP }; };
#endif
    template<> struct Type2Id<mfxExtHEVCTiles>          { enum { id = MFX_EXTBUFF_HEVC_TILES }; };
    template<> struct Type2Id<mfxExtHEVCRegion>         { enum { id = MFX_EXTBUFF_HEVC_REGION }; };
    template<> struct Type2Id<mfxExtHEVCParam>          { enum { id = MFX_EXTBUFF_HEVC_PARAM }; };
    template<> struct Type2Id<mfxExtCodingOption>       { enum { id = MFX_EXTBUFF_CODING_OPTION }; };
    template<> struct Type2Id<mfxExtCodingOption2>      { enum { id = MFX_EXTBUFF_CODING_OPTION2 }; };
    template<> struct Type2Id<mfxExtCodingOption3>      { enum { id = MFX_EXTBUFF_CODING_OPTION3 }; };
    template<> struct Type2Id<mfxExtCodingOptionSPSPPS> { enum { id = MFX_EXTBUFF_CODING_OPTION_SPSPPS }; };
    template<> struct Type2Id<mfxExtCodingOptionVPS>    { enum { id = MFX_EXTBUFF_CODING_OPTION_VPS }; };
    template<> struct Type2Id<mfxExtLAFrameStatistics>  { enum { id = MFX_EXTBUFF_LOOKAHEAD_STAT}; };
    template<> struct Type2Id<mfxExtEncoderROI>         { enum { id = MFX_EXTBUFF_ENCODER_ROI}; };
    template<> struct Type2Id<mfxExtBRC>                { enum { id = MFX_EXTBUFF_BRC }; };

    template <class T> struct RemoveConst          { typedef T type; };
    template <class T> struct RemoveConst<const T> { typedef T type; };

    inline mfxExtBuffer *GetExtBufferById(mfxExtBuffer **extBuffer, Ipp32s numExtBuffer, Ipp32u id)
    {
        if (extBuffer)
            for (Ipp32s i = 0; i < numExtBuffer; i++)
                if (extBuffer[i]->BufferId == id)
                    return extBuffer[i];
        return NULL;
    }

    struct ExtBufferProxy;
    template <class T> ExtBufferProxy GetExtBuffer(const T &par);
    struct ExtBufferProxy
    {
        template <class U> operator U *() {
            return reinterpret_cast<U *>(GetExtBufferById(m_extBuffer, m_numExtBuffer, Type2Id<typename RemoveConst<U>::type>::id));
        }

        template <class U> operator U &() {
            if (U *p = this->operator U *())
                return *p;
            throw std::runtime_error("ext buffer not found");
        }
    private:
        template <class T> explicit ExtBufferProxy(const T &par) : m_extBuffer(par.ExtParam), m_numExtBuffer(par.NumExtParam) {}
        template <class T> friend ExtBufferProxy GetExtBuffer(const T &par);

        mfxExtBuffer **m_extBuffer;
        Ipp32s         m_numExtBuffer;
    };
    template <class T> ExtBufferProxy GetExtBuffer(const T &par) { return ExtBufferProxy(par); }
} // namespace

//#include "mfx_h265_encode.h"

#endif // __MFX_H265_DEFS_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
