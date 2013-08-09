/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_DEC_DEFS_DEC_H__
#define __UMC_H265_DEC_DEFS_DEC_H__

#include <vector>

#include "umc_memory_allocator.h"
#include "umc_structures.h"

#include "mfx_h265_optimization.h"
namespace UMC_HEVC_DECODER
{
  
#if defined(_WIN32) || defined(_WIN64)
  #define H265_FORCEINLINE __forceinline
  #define H265_NONLINE __declspec(noinline)
#else
  #define H265_FORCEINLINE __attribute__((always_inline))
  #define H265_NONLINE __attribute__((noinline))
#endif

// This better be placed in some general/common header
#ifdef __INTEL_COMPILER
# define H265_RESTRICT __restrict
#elif defined _MSC_VER
# if _MSC_VER >= 1400
#  define H265_RESTRICT __restrict
# else
#  define H265_RESTRICT
# endif
#else
# define H265_RESTRICT
#endif
  
#define BITS_PER_PLANE 8
// Hack to allow somewhat unconformant streams. Should be used only when decoding random ENC streams
#define RANDOM_ENC_TESTING 1
#define INSTRUMENTED_CABAC 0

// AMVP: advanced motion vector prediction
#define AMVP_MAX_NUM_CANDS          2           ///< max number of final candidates
#define AMVP_MAX_NUM_CANDS_MEM      3           ///< max number of candidates
// MERGE
#define MRG_MAX_NUM_CANDS           5
#define MRG_MAX_NUM_CANDS 5                            // MERGE

#define REG_DCT 65535

#define NUM_QT_CBF_CTX                5       ///< number of context models for QT CBF

#define CU_DQP_TU_CMAX 5 //max number bins for truncated unary
#define CU_DQP_EG_k 0 //expgolomb order

/// coefficient scanning type used in ACS
enum COEFF_SCAN_TYPE
{
    SCAN_DIAG = 0,      ///< typical zigzag scan
    SCAN_HOR,           ///< horizontal first scan
    SCAN_VER            ///< vertical first scan
};

enum
{
    SCAN_SET_SIZE = 16,
    LAST_MINUS_FIRST_SIG_SCAN_THRESHOLD = 3,

};

#define NUM_SIG_CG_FLAG_CTX           2       ///< number of context models for MULTI_LEVEL_SIGNIFICANCE
#define NUM_SIG_FLAG_CTX_LUMA         27      ///< number of context models for luma sig flag
#define NUM_CTX_LAST_FLAG_XY          15      ///< number of context models for last coefficient position
#define C1FLAG_NUMBER               8 // maximum number of largerThan1 flag coded in one chunk :  16 in HM5
#define NUM_ONE_FLAG_CTX_LUMA         16      ///< number of context models for greater than 1 flag of luma
#define NUM_ABS_FLAG_CTX_LUMA          4      ///< number of context models for greater than 2 flag of luma
#define NUM_TRANSFORMSKIP_FLAG_CTX    1       ///< number of context models for transform skipping

#define COEF_REMAIN_BIN_REDUCTION        3 ///< J0142: Maximum codeword length of coeff_abs_level_remaining reduced to 32.
                                           ///< COEF_REMAIN_BIN_REDUCTION is also used to indicate the level at which the VLC
                                           ///< transitions from Golomb-Rice to TU+EG(k)
#define PLANAR_IDX             0
#define VER_IDX                26                    // index for intra VERTICAL   mode
#define HOR_IDX                10                    // index for intra HORIZONTAL mode
#define DC_IDX                 1                     // index for intra DC mode
#define NUM_CHROMA_MODE        5                     // total number of chroma modes
#define DM_CHROMA_IDX          36                    // chroma mode index for derived from luma intra mode

#define MAX_TLAYER 8
#define MAX_CPB_CNT                     32  ///< Upper bound of (cpb_cnt_minus1 + 1)

#define MAX_CU_DEPTH 6 // log2(LCUSize)
#define MAX_CU_SIZE (1<<(MAX_CU_DEPTH)) // maximum allowable size of CU
#define MIN_PU_SIZE 4
#define MAX_NUM_SPU_W (MAX_CU_SIZE/MIN_PU_SIZE) // maximum number of SPU in horizontal line

#define MAX_VPS_NUM_HRD_PARAMETERS_ALLOWED_PLUS1  1024
#define MAX_VPS_NUH_RESERVED_ZERO_LAYER_ID_PLUS1  1

#define SAO_BO_BITS                    5
#define LUMA_GROUP_NUM                (1 << SAO_BO_BITS)

#if defined (__ICL)
    // remark #981: operands are evaluated in unspecified order
#pragma warning(disable: 981)
#elif defined ( _MSC_VER )
    // warning C4068: unknown pragma
    // warning C4127: conditional check is constant 
#pragma warning (disable: 4127 4068)
#endif

    struct H265SeqParamSet;
    class H265DecoderFrame;
    class H265DecYUVBufferPadded;
    class H265CodingUnit;
//    class H265CodingUnit;
//
// Define some useful macros
//

#if 0
    #define STRUCT_DECLSPEC_ALIGN __declspec(align(16))
#else
    #define STRUCT_DECLSPEC_ALIGN
#endif

const int MAX_CHROMA_FORMAT_IDC = 3;

// Although the standard allows for a minimum width or height of 4, this
// implementation restricts the minimum value to 32.

enum DisplayPictureStruct_H265 {
    DPS_FRAME_H265     = 0,
    DPS_TOP_H265,         // one field
    DPS_BOTTOM_H265,      // one field
    DPS_TOP_BOTTOM_H265,
    DPS_BOTTOM_TOP_H265,
    DPS_TOP_BOTTOM_TOP_H265,
    DPS_BOTTOM_TOP_BOTTOM_H265,
    DPS_FRAME_DOUBLING_H265,
    DPS_FRAME_TRIPLING_H265
};


// default plane & coeffs types:
#if BITS_PER_PLANE == 8
typedef Ipp8u H265PlaneYCommon;
typedef Ipp8u H265PlaneUVCommon;
#else
typedef Ipp16s H265PlaneYCommon;
typedef Ipp16s H265PlaneUVCommon;
#endif
typedef Ipp16s H265CoeffsCommon;
#define H265_COEFF_MIN (H265CoeffsCommon)-32768
#define H265_COEFF_MAX (H265CoeffsCommon)32767

typedef H265CoeffsCommon *H265CoeffsPtrCommon;
typedef H265PlaneYCommon *H265PlanePtrYCommon;
typedef H265PlaneUVCommon *H265PlanePtrUVCommon;

enum NalUnitType
{
  NAL_UNIT_CODED_SLICE_TRAIL_N = 0,   // 0
  NAL_UNIT_CODED_SLICE_TRAIL_R,   // 1

  NAL_UNIT_CODED_SLICE_TSA_N,     // 2
  NAL_UNIT_CODED_SLICE_TLA,       // 3   // Current name in the spec: TSA_R

  NAL_UNIT_CODED_SLICE_STSA_N,    // 4
  NAL_UNIT_CODED_SLICE_STSA_R,    // 5

  NAL_UNIT_CODED_SLICE_RADL_N,    // 6
  NAL_UNIT_CODED_SLICE_DLP,       // 7 // Current name in the spec: RADL_R

  NAL_UNIT_CODED_SLICE_RASL_N,    // 8
  NAL_UNIT_CODED_SLICE_TFD,       // 9 // Current name in the spec: RASL_R

  NAL_UNIT_RESERVED_10,
  NAL_UNIT_RESERVED_11,
  NAL_UNIT_RESERVED_12,
  NAL_UNIT_RESERVED_13,
  NAL_UNIT_RESERVED_14,
  NAL_UNIT_RESERVED_15,

  NAL_UNIT_CODED_SLICE_BLA,       // 16   // Current name in the spec: BLA_W_LP
  NAL_UNIT_CODED_SLICE_BLANT,     // 17   // Current name in the spec: BLA_W_DLP
  NAL_UNIT_CODED_SLICE_BLA_N_LP,  // 18
  NAL_UNIT_CODED_SLICE_IDR,       // 19  // Current name in the spec: IDR_W_DLP
  NAL_UNIT_CODED_SLICE_IDR_N_LP,  // 20
  NAL_UNIT_CODED_SLICE_CRA,       // 21
  NAL_UNIT_RESERVED_22,
  NAL_UNIT_RESERVED_23,

  NAL_UNIT_RESERVED_24,
  NAL_UNIT_RESERVED_25,
  NAL_UNIT_RESERVED_26,
  NAL_UNIT_RESERVED_27,
  NAL_UNIT_RESERVED_28,
  NAL_UNIT_RESERVED_29,
  NAL_UNIT_RESERVED_30,
  NAL_UNIT_RESERVED_31,

  NAL_UNIT_VPS,                   // 32
  NAL_UNIT_SPS,                   // 33
  NAL_UNIT_PPS,                   // 34
  NAL_UNIT_ACCESS_UNIT_DELIMITER, // 35
  NAL_UNIT_EOS,                   // 36
  NAL_UNIT_EOB,                   // 37
  NAL_UNIT_FILLER_DATA,           // 38
  NAL_UNIT_SEI,                   // 39 Prefix SEI
  NAL_UNIT_SEI_SUFFIX,            // 40 Suffix SEI

  NAL_UNIT_RESERVED_41,
  NAL_UNIT_RESERVED_42,
  NAL_UNIT_RESERVED_43,
  NAL_UNIT_RESERVED_44,
  NAL_UNIT_RESERVED_45,
  NAL_UNIT_RESERVED_46,
  NAL_UNIT_RESERVED_47,
  NAL_UNIT_UNSPECIFIED_48,
  NAL_UNIT_UNSPECIFIED_49,
  NAL_UNIT_UNSPECIFIED_50,
  NAL_UNIT_UNSPECIFIED_51,
  NAL_UNIT_UNSPECIFIED_52,
  NAL_UNIT_UNSPECIFIED_53,
  NAL_UNIT_UNSPECIFIED_54,
  NAL_UNIT_UNSPECIFIED_55,
  NAL_UNIT_UNSPECIFIED_56,
  NAL_UNIT_UNSPECIFIED_57,
  NAL_UNIT_UNSPECIFIED_58,
  NAL_UNIT_UNSPECIFIED_59,
  NAL_UNIT_UNSPECIFIED_60,
  NAL_UNIT_UNSPECIFIED_61,
  NAL_UNIT_UNSPECIFIED_62,
  NAL_UNIT_UNSPECIFIED_63,
  NAL_UNIT_INVALID
};

// Note!  The Picture Code Type values below are no longer used in the
// core encoder.   It only knows about slice types, and whether or not
// the frame is IDR, Reference or Disposable.  See enum above.

enum SliceType
{
  B_SLICE,
  P_SLICE,
  I_SLICE
};

enum EnumRefPicList
{
    REF_PIC_LIST_0 = 0,   ///< reference list 0
    REF_PIC_LIST_1 = 1    ///< reference list 1
};

enum EnumPartSize   // supported partition shape
{
    SIZE_2Nx2N,           ///< symmetric motion partition,  2Nx2N
    SIZE_2NxN,            ///< symmetric motion partition,  2Nx N
    SIZE_Nx2N,            ///< symmetric motion partition,   Nx2N
    SIZE_NxN,             ///< symmetric motion partition,   Nx N
    SIZE_2NxnU,           ///< asymmetric motion partition, 2Nx( N/2) + 2Nx(3N/2)
    SIZE_2NxnD,           ///< asymmetric motion partition, 2Nx(3N/2) + 2Nx( N/2)
    SIZE_nLx2N,           ///< asymmetric motion partition, ( N/2)x2N + (3N/2)x2N
    SIZE_nRx2N,           ///< asymmetric motion partition, (3N/2)x2N + ( N/2)x2N

    SIZE_NONE = 15
};

// supported prediction type
enum EnumPredMode
{
    MODE_INTER,           ///< inter-prediction mode
    MODE_INTRA,           ///< intra-prediction mode
    MODE_NONE = 15
};

#define SCALING_LIST_NUM 6         ///< list number for quantization matrix
#define SCALING_LIST_NUM_32x32 2   ///< list number for quantization matrix 32x32
#define SCALING_LIST_REM_NUM 6     ///< remainder of QP/6
#define SCALING_LIST_START_VALUE 8 ///< start value for dpcm mode
#define MAX_MATRIX_COEF_NUM 64     ///< max coefficient number for quantization matrix
#define MAX_MATRIX_SIZE_NUM 8      ///< max size number for quantization matrix
#define SCALING_LIST_DC 16         ///< default DC value

enum ScalingListSize
{
  SCALING_LIST_4x4 = 0,
  SCALING_LIST_8x8,
  SCALING_LIST_16x16,
  SCALING_LIST_32x32,
  SCALING_LIST_SIZE_NUM
};

enum SAOTypeLen
{
    SAO_EO_LEN    = 4,
    SAO_BO_LEN    = 4,
    SAO_MAX_BO_CLASSES = 32
};

enum SAOType
{
    SAO_EO_0 = 0,
    SAO_EO_1,
    SAO_EO_2,
    SAO_EO_3,
    SAO_BO,
    MAX_NUM_SAO_TYPE
};

enum SGUBorderID
{
    SGU_L = 0,
    SGU_R,
    SGU_T,
    SGU_B,
    SGU_TL,
    SGU_TR,
    SGU_BL,
    SGU_BR,
    NUM_SGU_BORDER
};

struct SAOLCUParam
{
    bool   m_mergeUpFlag;
    bool   m_mergeLeftFlag;
    Ipp32s m_typeIdx;
    Ipp32s m_subTypeIdx;
    Ipp32s m_offset[4];
    Ipp32s m_length;
};

class H265SampleAdaptiveOffset
{
public:
    H265SampleAdaptiveOffset();

    void init(const H265SeqParamSet* sps);
    void destroy();

    void SAOProcess(H265DecoderFrame* pFrame, Ipp32s start, Ipp32s toProcess);
protected:
    H265DecoderFrame*   m_Frame;

    Ipp32s              m_OffsetEo[LUMA_GROUP_NUM];
    Ipp32s              m_OffsetEo2[LUMA_GROUP_NUM];
    Ipp32s              m_OffsetEoChroma[LUMA_GROUP_NUM];
    Ipp32s              m_OffsetEo2Chroma[LUMA_GROUP_NUM];
    H265PlaneYCommon   *m_OffsetBo;
    H265PlaneYCommon   *m_OffsetBo2;
    H265PlaneUVCommon  *m_OffsetBoChroma;
    H265PlaneUVCommon  *m_OffsetBo2Chroma;
    H265PlaneYCommon   *m_ClipTable;
    H265PlaneYCommon   *m_ClipTableBase;
    H265PlaneYCommon   *m_lumaTableBo;

    H265PlaneYCommon   *m_TmpU[2];
    H265PlaneYCommon   *m_TmpL[2];

    Ipp32s              m_PicWidth;
    Ipp32s              m_PicHeight;
    Ipp32u              m_MaxCUWidth;
    Ipp32u              m_MaxCUHeight;
    Ipp32u              m_SaoBitIncreaseY, m_SaoBitIncreaseC;
    bool                m_UseNIF;
    bool                m_needPCMRestoration;
    bool                m_slice_sao_luma_flag;
    bool                m_slice_sao_chroma_flag;

    bool                m_isInitialized;

    void SetOffsetsLuma(SAOLCUParam &saoLCUParam, Ipp32s typeIdx);
    void SetOffsetsChroma(SAOLCUParam &saoLCUParamCb, SAOLCUParam &saoLCUParamCr, Ipp32s typeIdx);

    void processSaoCuOrgChroma(Ipp32s Addr, Ipp32s PartIdx, H265PlaneUVCommon *tmpL);
    void processSaoCuChroma(Ipp32s addr, Ipp32s saoType, H265PlaneUVCommon *tmpL);

    void createNonDBFilterInfo();
    void PCMCURestoration(H265CodingUnit* pcCU, Ipp32u AbsZorderIdx, Ipp32u Depth);
    void PCMSampleRestoration(H265CodingUnit* pcCU, Ipp32u AbsZorderIdx, Ipp32u Depth, EnumTextType Text);

    void processSaoUnits(Ipp32s first, Ipp32s toProcess);
    void processSaoLine(SAOLCUParam* saoLCUParam, SAOLCUParam* saoLCUParamCb, SAOLCUParam* saoLCUParamCr, Ipp32s firstCU, Ipp32s lastCU);
};

inline
UMC::FrameType SliceTypeToFrameType(SliceType slice_type)
{
    switch(slice_type)
    {
    case P_SLICE:
        return UMC::P_PICTURE;
    case B_SLICE:
        return UMC::B_PICTURE;
    case I_SLICE:
        return UMC::I_PICTURE;
    }

    return UMC::NONE_PICTURE;
}

typedef enum
{
    SEI_BUFFERING_PERIOD_TYPE                   = 0,
    SEI_PIC_TIMING_TYPE                         = 1,
    SEI_PAN_SCAN_RECT_TYPE                      = 2,
    SEI_FILLER_TYPE                             = 3,
    SEI_USER_DATA_REGISTERED_TYPE               = 4,
    SEI_USER_DATA_UNREGISTERED_TYPE             = 5,
    SEI_RECOVERY_POINT_TYPE                     = 6,
    SEI_SPARE_PIC_TYPE                          = 8,
    SEI_SCENE_INFO_TYPE                         = 9,
    SEI_SUB_SEQ_INFO_TYPE                       = 10,
    SEI_SUB_SEQ_LAYER_TYPE                      = 11,
    SEI_SUB_SEQ_TYPE                            = 12,
    SEI_FULL_FRAME_FREEZE_TYPE                  = 13,
    SEI_FULL_FRAME_FREEZE_RELEASE_TYPE          = 14,
    SEI_FULL_FRAME_SNAPSHOT_TYPE                = 15,
    SEI_PROGRESSIVE_REF_SEGMENT_START_TYPE      = 16,
    SEI_PROGRESSIVE_REF_SEGMENT_END_TYPE        = 17,
    SEI_MOTION_CONSTRAINED_SG_SET_TYPE          = 18,
    SEI_FILM_GRAIN_CHARACTERISTICS              = 19,

    SEI_DEBLOCKING_FILTER_DISPLAY_PREFERENCE    = 20,
    SEI_STEREO_VIDEO_INFO                       = 21,
    SEI_POST_FILTER_HINT                        = 22,
    SEI_TONE_MAPPING_INFO                       = 23,
    SEI_SCALABILITY_INFO                        = 24,
    SEI_SUB_PIC_SCALABLE_LAYER                  = 25,
    SEI_NON_REQUIRED_LAYER_REP                  = 26,
    SEI_PRIORITY_LAYER_INFO                     = 27,
    SEI_LAYERS_NOT_PRESENT                      = 28,
    SEI_LAYER_DEPENDENCY_CHANGE                 = 29,
    SEI_SCALABLE_NESTING                        = 30,
    SEI_BASE_LAYER_TEMPORAL_HRD                 = 31,
    SEI_QUALITY_LAYER_INTEGRITY_CHECK           = 32,
    SEI_REDUNDANT_PIC_PROPERTY                  = 33,
    SEI_TL0_DEP_REP_INDEX                       = 34,
    SEI_TL_SWITCHING_POINT                      = 35,
    SEI_PARALLEL_DECODING_INFO                  = 36,
    SEI_MVC_SCAOLABLE_NESTING                   = 37,
    SEI_VIEW_SCALABILITY_INFO                   = 38,
    SEI_MULTIVIEW_SCENE_INFO                    = 39,
    SEI_MULTIVIEW_ACQUISITION_INFO              = 40,
    SEI_NONT_REQUERED_VIEW_COMPONENT            = 41,
    SEI_VIEW_DEPENDENCY_CHANGE                  = 42,
    SEI_OPERATION_POINTS_NOT_PRESENT            = 43,
    SEI_BASE_VIEW_TEMPORAL_HRD                  = 44,
    SEI_FRAME_PACKING_ARRANGEMENT               = 45,

    SEI_RESERVED                                = 50,

    SEI_NUM_MESSAGES

} SEI_TYPE;

#define IS_SKIP_DEBLOCKING_MODE_PERMANENT (m_PermanentTurnOffDeblocking == 2)
#define IS_SKIP_DEBLOCKING_MODE_PREVENTIVE (m_PermanentTurnOffDeblocking == 3)

enum
{
    INVALID_DPB_DELAY_H265 = 0xffffffff,

    MAX_NUM_SEQ_PARAM_SETS_H265 = 32,
    MAX_NUM_PIC_PARAM_SETS_H265 = 256,

    MAX_NUM_REF_PICS            = 16,

    COEFFICIENTS_BUFFER_SIZE_H265    = 16 * 51,

    MINIMAL_DATA_SIZE_H265           = 4,

    DEFAULT_NU_TAIL_VALUE       = 0xff,
    DEFAULT_NU_TAIL_SIZE        = 8
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Memory class
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class RefCounter
{
public:

    RefCounter() : m_refCounter(0)
    {
    }

    void IncrementReference() const;

    void DecrementReference();

    void ResetRefCounter() {m_refCounter = 0;}

    Ipp32u GetRefCounter() {return m_refCounter;}

protected:
    mutable Ipp32s m_refCounter;

    virtual ~RefCounter()
    {
    }

    virtual void Free()
    {
    }
};

class HeapObject : public RefCounter
{
public:

    virtual ~HeapObject() {}

    virtual void Reset()
    {
    }

    virtual void Free();
};

#pragma pack(16)

struct H265ScalingList
{
    H265ScalingList() { init(); }
   ~H265ScalingList() { destroy(); }

  void     setScalingListPresentFlag    (bool b)                               { m_scaling_list_data_present_flag = b;    }
  bool     getScalingListPresentFlag    ()                                     { return m_scaling_list_data_present_flag; }

  int*      getScalingListAddress   (unsigned sizeId, unsigned listId)          { return m_scalingListCoef[sizeId][listId]; } //!< get matrix coefficient
  const int* getScalingListAddress   (unsigned sizeId, unsigned listId) const    { return m_scalingListCoef[sizeId][listId]; } //!< get matrix coefficient
  bool     checkPredMode                (unsigned sizeId, unsigned listId);
  void     setRefMatrixId               (unsigned sizeId, unsigned listId, unsigned u)   { m_refMatrixId[sizeId][listId] = u;    }     //!< set reference matrix ID
  unsigned getRefMatrixId               (unsigned sizeId, unsigned listId)           { return m_refMatrixId[sizeId][listId]; }     //!< get reference matrix ID
  int*     getScalingListDefaultAddress (unsigned sizeId, unsigned listId);                                                        //!< get default matrix coefficient
  void     processDefaultMarix          (unsigned sizeId, unsigned listId);
  void     setScalingListDC             (unsigned sizeId, unsigned listId, unsigned u)   { m_scalingListDC[sizeId][listId] = u; }      //!< set DC value
  int      getScalingListDC             (unsigned sizeId, unsigned listId) const     { return m_scalingListDC[sizeId][listId]; }   //!< get DC value
  void     checkDcOfMatrix              ();
  void     processRefMatrix             (unsigned sizeId, unsigned listId , unsigned refListId );

private:
    void init();
    void destroy();

  int      m_scalingListDC               [SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM]; //!< the DC value of the matrix coefficient for 16x16
  unsigned m_refMatrixId                 [SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM]; //!< RefMatrixID
  bool     m_scaling_list_data_present_flag;                                                //!< flag for using default matrix
  int      m_scalingListCoef             [SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM][MAX_MATRIX_COEF_NUM]; //!< quantization matrix
};

struct H265PTL
{
    int         PTL_profile_space;
    bool        PTL_tier_flag;
    int         PTL_profile_idc;
    unsigned    PTL_profile_compatibility_flags;    // bitfield
    int         PTL_level_idc;
    bool        PTL_progressive_source_flag;
    bool        PTL_interlaced_source_flag;
    bool        PTL_non_packed_constraint_flag;
    bool        PTL_frame_only_constraint_flag;

    H265PTL()   { ::memset(this, 0, sizeof(*this)); }

    int   getProfileSpace() const   { return PTL_profile_space; }
    void  setProfileSpace(int x)    { PTL_profile_space = x; }

    bool  getTierFlag()     const   { return PTL_tier_flag; }
    void  setTierFlag(bool x)       { PTL_tier_flag = x; }

    int   getProfileIdc()   const   { return PTL_profile_idc; }

    bool  getProfileCompatibilityFlag(int i) const    { return (PTL_profile_compatibility_flags & (1 << i)) != 0; }

    int   getLevelIdc()   const   { return PTL_level_idc; }
    void  setLevelIdc(int x)      { PTL_level_idc = x; }

    bool getProgressiveSourceFlag() const { return PTL_progressive_source_flag; }
    void setProgressiveSourceFlag(bool b) { PTL_progressive_source_flag = b; }

    bool getInterlacedSourceFlag() const { return PTL_interlaced_source_flag; }
    void setInterlacedSourceFlag(bool b) { PTL_interlaced_source_flag = b; }

    bool getNonPackedConstraintFlag() const { return PTL_non_packed_constraint_flag; }
    void setNonPackedConstraintFlag(bool b) { PTL_non_packed_constraint_flag = b; }

    bool getFrameOnlyConstraintFlag() const { return PTL_frame_only_constraint_flag; }
    void setFrameOnlyConstraintFlag(bool b) { PTL_frame_only_constraint_flag = b; }
};


#define H265_MAX_SUBLAYER_PTL   6
struct H265ProfileTierLevel
{
    H265PTL m_generalPTL;
    H265PTL m_subLayerPTL[H265_MAX_SUBLAYER_PTL];
    unsigned sub_layer_profile_present_flags;       // bitfield [0:H265_MAX_SUBLAYER_PTL]
    unsigned sub_layer_level_present_flag;          // bitfield [0:H265_MAX_SUBLAYER_PTL]

    H265ProfileTierLevel()  {
        sub_layer_profile_present_flags = 0;
        sub_layer_level_present_flag = 0;
    }


    H265PTL* getGeneralPTL()        { return &m_generalPTL; }
    H265PTL* getSubLayerPTL(int i)  { return &m_subLayerPTL[i]; }
};

struct H265HrdSubLayerInfo
{
    bool        fixed_pic_rate_flag;
    bool        fixed_pic_rate_within_cvs_flag;
    unsigned    elemental_duration_in_tc;
    bool        low_delay_hrd_flag;
    unsigned    cpb_cnt;
    unsigned    bit_rate_value[MAX_CPB_CNT][2];
    unsigned    cpb_size_value[MAX_CPB_CNT][2];
    unsigned    cpb_size_du_value[MAX_CPB_CNT][2];
    unsigned    cbr_flag[MAX_CPB_CNT][2];
    unsigned    bit_rate_du_value[MAX_CPB_CNT][2];
};

struct H265HRD
{
    bool        nal_hrd_parameters_present_flag;
    bool        vcl_hrd_parameters_present_flag;
    bool        sub_pic_cpb_params_present_flag;
    unsigned    tick_divisor;
    unsigned    du_cpb_removal_delay_length;
    bool        sub_pic_cpb_params_in_pic_timing_sei_flag;
    unsigned    dpb_output_delay_du_length;
    unsigned    bit_rate_scale;
    unsigned    cpb_size_scale;
    unsigned    cpb_size_du_scale;
    unsigned    initial_cpb_removal_delay_length;
    unsigned    au_cpb_removal_delay_length;
    unsigned    dpb_output_delay_length;

    H265HrdSubLayerInfo m_HRD[MAX_TLAYER];


    H265HRD() {
        ::memset(this, 0, sizeof(*this));
    }

    void setNalHrdParametersPresentFlag(bool flag)  { nal_hrd_parameters_present_flag = flag; }
    bool getNalHrdParametersPresentFlag() const     { return nal_hrd_parameters_present_flag; }

    void setVclHrdParametersPresentFlag(bool flag)  { vcl_hrd_parameters_present_flag = flag; }
    bool getVclHrdParametersPresentFlag() const     { return vcl_hrd_parameters_present_flag; }

    void setSubPicCpbParamsPresentFlag(bool flag)   { sub_pic_cpb_params_present_flag = flag; }
    bool getSubPicCpbParamsPresentFlag() const      { return sub_pic_cpb_params_present_flag; }

    void setTickDivisor(unsigned value)             { tick_divisor = value; }
    unsigned getTickDivisor() const                 { return tick_divisor;  }

    void setDuCpbRemovalDelayLength(unsigned value) { du_cpb_removal_delay_length = value; }
    unsigned getDuCpbRemovalDelayLength() const     { return du_cpb_removal_delay_length;  }

    void setSubPicCpbParamsInPicTimingSEIFlag(bool flag)    { sub_pic_cpb_params_in_pic_timing_sei_flag = flag;   }
    bool getSubPicCpbParamsInPicTimingSEIFlag() const       { return sub_pic_cpb_params_in_pic_timing_sei_flag;   }

    void setDpbOutputDelayDuLength(unsigned value)  { dpb_output_delay_du_length = value; }
    unsigned getDpbOutputDelayDuLength() const      { return dpb_output_delay_du_length;  }

    void setBitRateScale(unsigned value)            { bit_rate_scale = value; }
    unsigned getBitRateScale() const                { return bit_rate_scale;  }

    void setCpbSizeScale(unsigned value)            { cpb_size_scale = value; }
    unsigned getCpbSizeScale() const                { return cpb_size_scale;  }

    void setDuCpbSizeScale(unsigned value)          { cpb_size_du_scale = value; }
    unsigned getDuCpbSizeScale()                    { return cpb_size_du_scale;  }

    void setInitialCpbRemovalDelayLength(unsigned value)    { initial_cpb_removal_delay_length = value; }
    unsigned getInitialCpbRemovalDelayLength() const        { return initial_cpb_removal_delay_length;  }

    void setCpbRemovalDelayLength(unsigned value)   { au_cpb_removal_delay_length = value; }
    unsigned getCpbRemovalDelayLength() const       { return au_cpb_removal_delay_length;  }

    void setDpbOutputDelayLength(unsigned value)    { dpb_output_delay_length = value; }
    unsigned getDpbOutputDelayLength() const        { return dpb_output_delay_length;  }

    void setFixedPicRateFlag(int layer, bool flag)  { m_HRD[layer].fixed_pic_rate_flag = flag; }
    bool getFixedPicRateFlag(int layer) const       { return m_HRD[layer].fixed_pic_rate_flag; }

    void setFixedPicRateWithinCvsFlag(int layer, bool flag) { m_HRD[layer].fixed_pic_rate_within_cvs_flag = flag; }
    bool getFixedPicRateWithinCvsFlag(int layer) const      { return m_HRD[layer].fixed_pic_rate_within_cvs_flag; }

    void setPicDurationInTc(int layer, unsigned value)  { m_HRD[layer].elemental_duration_in_tc = value; }
    unsigned getPicDurationInTc(int layer) const        { return m_HRD[layer].elemental_duration_in_tc; }

    void setLowDelayHrdFlag(int layer, bool flag)   { m_HRD[layer].low_delay_hrd_flag = flag; }
    bool getLowDelayHrdFlag(int layer) const        { return m_HRD[layer].low_delay_hrd_flag; }

    void setCpbCnt(int layer, unsigned value)   { m_HRD[layer].cpb_cnt = value; }
    unsigned getCpbCnt(int layer) const         { return m_HRD[layer].cpb_cnt; }

    void setBitRateValue(int layer, int cpbcnt, int nalOrVcl, unsigned value)   { m_HRD[layer].bit_rate_value[cpbcnt][nalOrVcl] = value; }
    unsigned getBitRateValue(int layer, int cpbcnt, int nalOrVcl) const         { return m_HRD[layer].bit_rate_value[cpbcnt][nalOrVcl];  }

    void setCpbSizeValue(int layer, int cpbcnt, int nalOrVcl, unsigned value)   { m_HRD[layer].cpb_size_value[cpbcnt][nalOrVcl] = value; }
    unsigned getCpbSizeValue(int layer, int cpbcnt, int nalOrVcl) const         { return m_HRD[layer].cpb_size_value[cpbcnt][nalOrVcl];  }

    void setDuCpbSizeValue(int layer, int cpbcnt, int nalOrVcl, unsigned value) { m_HRD[layer].cpb_size_du_value[cpbcnt][nalOrVcl] = value; }
    unsigned getDuCpbSizeValue(int layer, int cpbcnt, int nalOrVcl) const       { return m_HRD[layer].cpb_size_du_value[cpbcnt][nalOrVcl];  }

    void setDuBitRateValue(int layer, int cpbcnt, int nalOrVcl, unsigned value) { m_HRD[layer].bit_rate_du_value[cpbcnt][nalOrVcl] = value; }
    unsigned getDuBitRateValue(int layer, int cpbcnt, int nalOrVcl) const       { return m_HRD[layer].bit_rate_du_value[cpbcnt][nalOrVcl];  }

    void setCbrFlag(int layer, int cpbcnt, int nalOrVcl, unsigned value)        { m_HRD[layer].cbr_flag[cpbcnt][nalOrVcl] = value;            }
    bool getCbrFlag(int layer, int cpbcnt, int nalOrVcl) const                  { return m_HRD[layer].cbr_flag[cpbcnt][nalOrVcl] != 0; }

    bool getCpbDpbDelaysPresentFlag() { return getNalHrdParametersPresentFlag() || getVclHrdParametersPresentFlag(); }
};

struct H265TimingInfo
{
    bool vps_timing_info_present_flag;
    unsigned vps_num_units_in_tick;
    unsigned vps_time_scale;
    bool vps_poc_proportional_to_timing_flag;
    int  vps_num_ticks_poc_diff_one;

public:
    H265TimingInfo()
        : vps_timing_info_present_flag(false)
        , vps_num_units_in_tick(1001)
        , vps_time_scale(60000)
        , vps_poc_proportional_to_timing_flag(false)
        , vps_num_ticks_poc_diff_one(0) {}

    void setTimingInfoPresentFlag(bool flag)    { vps_timing_info_present_flag = flag;  }
    bool getTimingInfoPresentFlag() const       { return vps_timing_info_present_flag;  }

    void setNumUnitsInTick(unsigned value)      { vps_num_units_in_tick = value;        }
    unsigned getNumUnitsInTick() const          { return vps_num_units_in_tick;         }

    void setTimeScale(unsigned value)           { vps_time_scale = value;               }
    unsigned getTimeScale() const               { return vps_time_scale;                }

    void setPocProportionalToTimingFlag(bool x) { vps_poc_proportional_to_timing_flag = x;      }
    bool getPocProportionalToTimingFlag() const { return vps_poc_proportional_to_timing_flag;   }

    void setNumTicksPocDiffOne(int x)           { vps_num_ticks_poc_diff_one = x;       }
    int  getNumTicksPocDiffOne( ) const         { return vps_num_ticks_poc_diff_one;    }
};

/// VPS class
class H265VideoParamSet : public HeapObject
{
public:
    unsigned    vps_video_parameter_set_id;
    unsigned    vps_max_sub_layers;
    unsigned    m_uiMaxLayers;
    bool        vps_temporal_id_nesting_flag;

    unsigned    vps_num_reorder_pics[MAX_TLAYER];
    unsigned    vps_max_dec_pic_buffering[MAX_TLAYER];
    unsigned    vps_max_latency_increase[MAX_TLAYER];

    unsigned    vps_num_hrd_parameters;
    unsigned    vps_max_nuh_reserved_zero_layer_id;
    unsigned*   hrd_op_set_idx;
    bool*       cprms_present_flag;
    unsigned    vps_max_op_sets;
    bool        layer_id_included_flag[MAX_VPS_NUM_HRD_PARAMETERS_ALLOWED_PLUS1][MAX_VPS_NUH_RESERVED_ZERO_LAYER_ID_PLUS1];

    H265HRD*                m_hrdParameters;
    H265TimingInfo          m_timingInfo;
    H265ProfileTierLevel    m_pcPTL;


public:
    H265VideoParamSet() : HeapObject(),
        hrd_op_set_idx(0),
        cprms_present_flag(0),
        m_hrdParameters(0)
    {
        Reset();
    }

   ~H265VideoParamSet() {
        delete m_hrdParameters;
        delete hrd_op_set_idx;
        delete cprms_present_flag;
    }

    void createHrdParamBuffer()
    {
        if(m_hrdParameters)
            delete m_hrdParameters;
        m_hrdParameters = new H265HRD[ getNumHrdParameters() ];

        if(hrd_op_set_idx)
            delete hrd_op_set_idx;
        hrd_op_set_idx = new unsigned[ getNumHrdParameters() ];

        if(cprms_present_flag)
            delete cprms_present_flag;
        cprms_present_flag = new bool[ getNumHrdParameters() ];
    }

    void Reset()
    {
        vps_video_parameter_set_id = 0;
        vps_max_sub_layers = 0;
        m_uiMaxLayers = 0;
        vps_temporal_id_nesting_flag = false;
        vps_num_hrd_parameters = 0;
        vps_max_nuh_reserved_zero_layer_id = 0;

        delete m_hrdParameters;
        m_hrdParameters = 0;

        delete hrd_op_set_idx;
        hrd_op_set_idx = 0;

        delete cprms_present_flag;
        cprms_present_flag = 0;

        for (int i = 0; i < MAX_TLAYER; i++)
        {
            vps_num_reorder_pics[i] = 0;
            vps_max_dec_pic_buffering[i] = 0;
            vps_max_latency_increase[i] = 0;
        }
    }

    int GetID()   { return 0; }

    H265HRD* getHrdParameters   ( unsigned i )             { return &m_hrdParameters[ i ]; }
    unsigned getHrdOpSetIdx      ( unsigned i )             { return hrd_op_set_idx[ i ]; }
    void    setHrdOpSetIdx      ( unsigned val, unsigned i )   { hrd_op_set_idx[ i ] = val;  }
    bool    getCprmsPresentFlag ( unsigned i )             { return cprms_present_flag[ i ]; }
    void    setCprmsPresentFlag ( bool val, unsigned i )   { cprms_present_flag[ i ] = val;  }

    int     getVPSId       ()                   { return vps_video_parameter_set_id;          }
    void    setVPSId       (int i)              { vps_video_parameter_set_id = i;             }

    unsigned    getMaxTLayers  ()                   { return vps_max_sub_layers;   }
    void    setMaxTLayers  (unsigned t)             { vps_max_sub_layers = t; }

    unsigned    getMaxLayers   ()                   { return m_uiMaxLayers;   }
    void    setMaxLayers   (unsigned l)             { m_uiMaxLayers = l; }

    bool    getTemporalNestingFlag   ()         { return vps_temporal_id_nesting_flag;   }
    void    setTemporalNestingFlag   (unsigned t)   { vps_temporal_id_nesting_flag = (t != 0); }

    void    setNumReorderPics(unsigned v, unsigned tLayer)                { vps_num_reorder_pics[tLayer] = v;    }
    unsigned    getNumReorderPics(unsigned tLayer)                        { return vps_num_reorder_pics[tLayer]; }

    void    setMaxDecPicBuffering(unsigned v, unsigned tLayer)            { vps_max_dec_pic_buffering[tLayer] = v;    }
    unsigned    getMaxDecPicBuffering(unsigned tLayer)                    { return vps_max_dec_pic_buffering[tLayer]; }

    void    setMaxLatencyIncrease(unsigned v, unsigned tLayer)            { vps_max_latency_increase[tLayer] = v;    }
    unsigned    getMaxLatencyIncrease(unsigned tLayer)                    { return vps_max_latency_increase[tLayer]; }

    unsigned getNumHrdParameters()const     { return vps_num_hrd_parameters; }
    void    setNumHrdParameters(unsigned v) { vps_num_hrd_parameters = v;    }

    unsigned getMaxNuhReservedZeroLayerId()                        { return vps_max_nuh_reserved_zero_layer_id; }
    void    setMaxNuhReservedZeroLayerId(unsigned v)                  { vps_max_nuh_reserved_zero_layer_id = v;    }

    unsigned getMaxOpSets()                                        { return vps_max_op_sets; }
    void    setMaxOpSets(unsigned v)                                  { vps_max_op_sets = v;    }
    unsigned getLayerIdIncludedFlag(unsigned opIdx, unsigned id)         { return layer_id_included_flag[opIdx][id]; }
    void    setLayerIdIncludedFlag(bool v, unsigned opIdx, unsigned id) { layer_id_included_flag[opIdx][id] = v;    }

    H265ProfileTierLevel* getPTL() { return &m_pcPTL; }
    H265TimingInfo* getTimingInfo() { return &m_timingInfo; }
};

typedef Ipp32u IntraType;

struct ReferencePictureSet
{
    Ipp32s m_NumberOfPictures;
    Ipp32s m_NumberOfNegativePictures;
    Ipp32s m_NumberOfPositivePictures;
    Ipp32s m_NumberOfLongtermPictures;
    Ipp32s m_DeltaPOC[MAX_NUM_REF_PICS];
    Ipp32s m_POC[MAX_NUM_REF_PICS];
    bool m_Used[MAX_NUM_REF_PICS];
    bool inter_ref_pic_set_prediction_flag;
    Ipp32s m_DeltaRIdxMinus1;
    Ipp32s m_DeltaRPS;
    Ipp32s m_NumRefIdc;
    Ipp32s m_RefIdc[MAX_NUM_REF_PICS + 1];
    bool m_CheckLTMSB[MAX_NUM_REF_PICS];
    bool m_bCheckLTMSB[MAX_NUM_REF_PICS];

    ReferencePictureSet();

    void sortDeltaPOC();

    void setInterRPSPrediction(bool f)      { inter_ref_pic_set_prediction_flag = f; }
    Ipp32s getNumberOfPictures() const    { return m_NumberOfPictures; }
    void setNumberOfPictures(Ipp32s val)  { m_NumberOfPictures = val; }
    Ipp32s getNumberOfNegativePictures() const    { return m_NumberOfNegativePictures; }
    void setNumberOfNegativePictures(Ipp32s val)  { m_NumberOfNegativePictures = val; }
    Ipp32s getNumberOfPositivePictures() const    { return m_NumberOfPositivePictures; }
    void setNumberOfPositivePictures(Ipp32s val)  { m_NumberOfPositivePictures = val; }
    Ipp32s getNumberOfLongtermPictures() const    { return m_NumberOfLongtermPictures; }
    void setNumberOfLongtermPictures(Ipp32s val)  { m_NumberOfLongtermPictures = val; }
    int getDeltaPOC(int index) const        { return m_DeltaPOC[index]; }
    void setDeltaPOC(int index, int val)    { m_DeltaPOC[index] = val; }
    bool getUsed(int index) const           { return m_Used[index]; }
    void setUsed(int index, bool f)         { m_Used[index] = f; }

    void setNumRefIdc(int val)              { m_NumRefIdc = val; }
    void setRefIdc(int index, int val)      { m_RefIdc[index] = val; }

    void setPOC(int bufferNum, int POC)     { m_POC[bufferNum] = POC; }
    int getPOC(int index)                   { return m_POC[index]; }

    void setCheckLTMSBPresent(int bufferNum, bool b) { m_bCheckLTMSB[bufferNum] = b; }
    bool getCheckLTMSBPresent(int bufferNum) { return m_bCheckLTMSB[bufferNum]; }
};

struct ReferencePictureSetList
{
    unsigned m_NumberOfReferencePictureSets;

    ReferencePictureSetList();

    void allocate(unsigned NumberOfEntries);

    ReferencePictureSet* getReferencePictureSet(int index) const { return &referencePictureSet[index]; }
    unsigned getNumberOfReferencePictureSets() const { return m_NumberOfReferencePictureSets; }

private:
    mutable std::vector<ReferencePictureSet> referencePictureSet;
};

struct RefPicListModification
{
    Ipp32u m_RefPicListModificationFlagL0;
    Ipp32u m_RefPicListModificationFlagL1;
    Ipp32u m_RefPicSetIdxL0[MAX_NUM_REF_PICS + 1];
    Ipp32u m_RefPicSetIdxL1[MAX_NUM_REF_PICS + 1];

    Ipp32u getRefPicListModificationFlagL0() const      { return m_RefPicListModificationFlagL0; }
    void setRefPicListModificationFlagL0(Ipp32u val)    { m_RefPicListModificationFlagL0 = val; }

    Ipp32u getRefPicListModificationFlagL1() const      { return m_RefPicListModificationFlagL1; }
    void setRefPicListModificationFlagL1(Ipp32u val)    { m_RefPicListModificationFlagL1 = val; }

    void setRefPicSetIdxL0(unsigned idx, Ipp32u refPicSetIdx) { m_RefPicSetIdxL0[idx] = refPicSetIdx; }
    Ipp32u getRefPicSetIdxL0(unsigned idx) { return m_RefPicSetIdxL0[idx]; }

    void   setRefPicSetIdxL1(unsigned idx, Ipp32u refPicSetIdx) { m_RefPicSetIdxL1[idx] = refPicSetIdx; }
    Ipp32u getRefPicSetIdxL1(unsigned idx)                      { return m_RefPicSetIdxL1[idx]; }
};

// Sequence parameter set structure, corresponding to the H.264 bitstream definition.
struct H265SeqParamSetBase
{
    //h265
    Ipp32s sps_video_parameter_set_id;
    Ipp32u sps_max_sub_layers;
    bool   sps_temporal_id_nesting_flag;
    Ipp8u  seq_parameter_set_id;                // id of this sequence parameter set
    Ipp8u  chroma_format_idc;

    Ipp32s separate_colour_plane_flag;

    Ipp32u pic_width_in_luma_samples;
    Ipp32u pic_height_in_luma_samples;
    Ipp8u  frame_cropping_flag;
    Ipp32u frame_cropping_rect_left_offset;
    Ipp32u frame_cropping_rect_right_offset;
    Ipp32u frame_cropping_rect_top_offset;
    Ipp32u frame_cropping_rect_bottom_offset;

    Ipp32u bit_depth_luma;
    Ipp32u bit_depth_chroma;

    Ipp32u log2_max_pic_order_cnt_lsb;
    bool   sps_sub_layer_ordering_info_present_flag;

    Ipp32u sps_max_dec_pic_buffering[MAX_TLAYER];
    Ipp32u sps_max_num_reorder_pics[MAX_TLAYER];
    Ipp32u sps_max_latency_increase[MAX_TLAYER];


    Ipp32u pcm_bit_depth_luma;
    Ipp32u pcm_bit_depth_chroma;
    Ipp32u MaxCUWidth;
    Ipp32u MaxCUHeight;
    Ipp32u MaxCUDepth;
    Ipp32s AddCUDepth;
    Ipp32u WidthInCU;
    Ipp32u HeightInCU;
    Ipp32u NumPartitionsInCU, NumPartitionsInCUSize, NumPartitionsInFrameWidth, NumPartitionsInFrameHeight;
    Ipp32u log2_min_transform_block_size;
    Ipp32u log2_max_transform_block_size;
    Ipp32u m_maxTrSize;
    Ipp32u log2_min_pcm_luma_coding_block_size;
    Ipp32u log2_max_pcm_luma_coding_block_size;
    Ipp32u max_transform_hierarchy_depth_inter;
    Ipp32u max_transform_hierarchy_depth_intra;
    Ipp32u MinTrDepth;
    Ipp32u MaxTrDepth;
    bool pcm_enabled_flag;
    bool sample_adaptive_offset_enabled_flag; //sample_adaptive_offset
    bool pcm_loop_filter_disable_flag;
    bool m_UseLDC;
    bool sps_temporal_mvp_enable_flag;
    bool m_enableTMVPFlag;

    unsigned    m_log2MinCUSize;
    unsigned    m_log2CtbSize;

    H265ProfileTierLevel     m_pcPTL;

    bool amp_enabled_flag;
    Ipp32s m_AMPAcc[MAX_CU_DEPTH]; //AMP Accuracy

    bool m_DisInter4x4Flag;

    bool m_UseNewRefSettingFlag;

    bool m_EnableTMVPFlag;

    bool scaling_list_enabled_flag;
    bool sps_scaling_list_data_present_flag;

    bool loop_filter_across_tiles_enabled_flag;

    Ipp32u m_MaxNumberOfReferencePictures;
    bool  sps_strong_intra_smoothing_enable_flag;

    int m_QPBDOffsetY;
    int m_QPBDOffsetC;
    bool m_RestrictedRefPicListsFlag;
    ReferencePictureSetList m_RPSList;
    bool long_term_ref_pics_present_flag;

    Ipp32u      num_long_term_ref_pic_sps;
    Ipp32u      lt_ref_pic_poc_lsb_sps[33];
    bool        used_by_curr_pic_lt_sps_flag[33];

    Ipp8u        profile_space;
    Ipp8u        profile_idc;                        // baseline, main, etc.
    Ipp16u       reserved_indicator_flags;
    Ipp8u        level_idc;
    Ipp32u       profile_compatibility;
    Ipp8u        residual_colour_transform_flag;
    Ipp8u        qpprime_y_zero_transform_bypass_flag;

    int          def_disp_win_left_offset;
    int          def_disp_win_right_offset;
    int          def_disp_win_top_offset;
    int          def_disp_win_bottom_offset;
    Ipp8u        log2_max_frame_num;                  // Number of bits to hold the frame_num

    bool         vui_parameters_present_flag;         // Zero indicates default VUI parameters
    Ipp32s       offset_for_non_ref_pic;

    Ipp32u       num_ref_frames_in_pic_order_cnt_cycle;
    Ipp32u       num_ref_frames;                      // total number of pics in decoded pic buffer

    // These fields are calculated from values above.  They are not written to the bitstream

    // vui part
    bool aspect_ratio_info_present_flag;
    int  aspect_ratio_idc;
    int  sar_width;
    int  sar_height;
    bool overscan_info_present_flag;
    bool overscan_appropriate_flag;
    bool video_signal_type_present_flag;
    int  video_format;
    bool video_full_range_flag;
    bool colour_description_present_flag;
    int  colour_primaries;
    int  transfer_characteristics;
    int  matrix_coefficients;
    bool chroma_loc_info_present_flag;
    int  chroma_sample_loc_type_top_field;
    int  chroma_sample_loc_type_bottom_field;
    bool neutral_chroma_indication_flag;
    bool field_seq_flag;
    bool frame_field_info_present_flag;
    bool hrd_parameters_present_flag;
    bool bitstream_restriction_flag;
    bool tiles_fixed_structure_flag;
    bool motion_vectors_over_pic_boundaries_flag;
    bool m_restrictedRefPicListsFlag;
    int  min_spatial_segmentation_idc;
    int  max_bytes_per_pic_denom;
    int  max_bits_per_mincu_denom;
    int  log2_max_mv_length_horizontal;
    int  log2_max_mv_length_vertical;
    H265HRD m_hrdParameters;
    H265TimingInfo m_timingInfo;

    void Reset()
    {
        H265SeqParamSetBase sps = {0};
        *this = sps;
    }

};    // H265SeqParamSetBase

// Sequence parameter set structure, corresponding to the H.264 bitstream definition.
struct H265SeqParamSet : public HeapObject, public H265SeqParamSetBase
{
/// SCALING_LIST class

    H265ScalingList m_scalingList;

    H265SeqParamSet()
        : HeapObject()
        , H265SeqParamSetBase()
    {
        Reset();
    }

    ~H265SeqParamSet()
    {
    }

    Ipp32s GetID() const
    {
        return seq_parameter_set_id;
    }

    virtual void Reset()
    {
        H265SeqParamSetBase::Reset();

        m_RPSList.m_NumberOfReferencePictureSets = 0;

        seq_parameter_set_id = MAX_NUM_SEQ_PARAM_SETS_H265;

        // set some parameters by default
        video_format = 5; // unspecified
        video_full_range_flag = 0;
        colour_primaries = 2; // unspecified
        transfer_characteristics = 2; // unspecified
        matrix_coefficients = 2; // unspecified
        frame_cropping_flag = 0;
        frame_cropping_rect_left_offset = 0;
        frame_cropping_rect_right_offset = 0;
        frame_cropping_rect_top_offset = 0;
        frame_cropping_rect_bottom_offset = 0;
    }

    static int getWinUnitX (int /*chromaFormatIdc*/) { /*assert (chromaFormatIdc > 0 && chromaFormatIdc <= MAX_CHROMA_FORMAT_IDC);*/ return 1/*m_cropUnitX[chromaFormatIdc]*/; } // TODO
    static int getWinUnitY (int /*chromaFormatIdc*/) { /*assert (chromaFormatIdc > 0 && chromaFormatIdc <= MAX_CHROMA_FORMAT_IDC);*/ return 1/*m_cropUnitY[chromaFormatIdc]*/; }

    void setProfileSpace(unsigned val)          { profile_space = (Ipp8u)val; }
    void setRsvdIndFlags(unsigned flags)        { reserved_indicator_flags = (Ipp16u)flags; }
    void setLevelIdc(unsigned val)              { level_idc = (Ipp8u)val; }
    void setProfileCompat(unsigned val)         { profile_compatibility = val; }
    void setSPSId(unsigned id)                  { seq_parameter_set_id = (Ipp8u)id; }
    void setVPSId(unsigned id)                  { sps_video_parameter_set_id = (Ipp8u)id; }
    unsigned getChromaFormatIdc() const         { return chroma_format_idc; }
    void setChromaFormatIdc(unsigned val)       { chroma_format_idc = (Ipp8u)val; }
    unsigned getMaxTLayers() const              { return sps_max_sub_layers; }
    void setMaxTLayers(unsigned val)            { sps_max_sub_layers = val; }
    unsigned getPicWidthInLumaSamples() const   { return pic_width_in_luma_samples; }
    void setPicWidthInLumaSamples(unsigned val) { pic_width_in_luma_samples = val; }
    unsigned getPicHeightInLumaSamples() const   { return pic_height_in_luma_samples; }
    void setPicHeightInLumaSamples(unsigned val){ pic_height_in_luma_samples = val; }

    bool getPicCroppingFlag() const             { return frame_cropping_flag != 0; }
    void setPicCroppingFlag(bool val)           { frame_cropping_flag = (Ipp8u)val; }
    void setPicCropLeftOffset(unsigned val)     { frame_cropping_rect_left_offset = val; }
    void setPicCropRightOffset(unsigned val)    { frame_cropping_rect_right_offset = val; }
    void setPicCropTopOffset(unsigned val)      { frame_cropping_rect_top_offset = val; }
    void setPicCropBottomOffset(unsigned val)   { frame_cropping_rect_bottom_offset = val; }

    Ipp32u getNumLongTermRefPicSPS() const      { return num_long_term_ref_pic_sps; }
    void setNumLongTermRefPicSPS(Ipp32u val)    { num_long_term_ref_pic_sps = val; }

    Ipp32u getLtRefPicPocLsbSps(Ipp32u index) const { return lt_ref_pic_poc_lsb_sps[index]; }
    void setLtRefPicPocLsbSps(Ipp32u index, Ipp32u val)     { lt_ref_pic_poc_lsb_sps[index] = val; }

    bool getUsedByCurrPicLtSPSFlag(Ipp32s i) const {return used_by_curr_pic_lt_sps_flag[i];}
    void setUsedByCurrPicLtSPSFlag(Ipp32s i, bool x)      { used_by_curr_pic_lt_sps_flag[i] = x;}

    int      getBitDepthY() const { return bit_depth_luma; }
    void     setBitDepthY(int u) { bit_depth_luma = u; }

    int      getBitDepthC() const { return bit_depth_chroma; }
    void     setBitDepthC(int u) { bit_depth_chroma = u; }

    int getQpBDOffsetY() const                  { return m_QPBDOffsetY; }
    void setQpBDOffsetY(int val)                { m_QPBDOffsetY = val; }
    int getQpBDOffsetC() const                  { return m_QPBDOffsetC; }
    void setQpBDOffsetC(int val)                { m_QPBDOffsetC = val; }

    bool getUsePCM() const                      { return pcm_enabled_flag; }
    void setUsePCM(bool flag)                   { pcm_enabled_flag = flag; }

    unsigned getPCMBitDepthLuma() const         { return pcm_bit_depth_luma; }
    void setPCMBitDepthLuma(unsigned val)       { pcm_bit_depth_luma = val; }
    unsigned getPCMBitDepthChroma() const       { return pcm_bit_depth_chroma; }
    void setPCMBitDepthChroma(unsigned val)     { pcm_bit_depth_chroma = val; }

    unsigned getMaxCUWidth() const              { return MaxCUWidth; }
    void setMaxCUWidth(unsigned val)            { MaxCUWidth = val; }
    unsigned getMaxCUHeight() const             { return MaxCUHeight; }
    void setMaxCUHeight(unsigned val)           { MaxCUHeight = val; }
    unsigned getMaxCUDepth() const              { return MaxCUDepth; }
    void setMaxCUDepth(unsigned val)            { MaxCUDepth = val; }

    void createRPSList(Ipp32s numRPS)
    {
        m_RPSList.allocate(numRPS);
    }

    unsigned getMaxTrSize() const               { return m_maxTrSize; }
    void setMaxTrSize(unsigned val)             { m_maxTrSize = val; }
    unsigned getMinTrDepth() const              { return MinTrDepth; }
    void setMinTrDepth(unsigned val)            { MinTrDepth = val; }
    unsigned getMaxTrDepth() const              { return MaxTrDepth; }
    void setMaxTrDepth(unsigned val)            { MaxTrDepth = val; }

    bool getScalingListFlag() const             { return scaling_list_enabled_flag; }
    void setScalingListFlag(bool f)             { scaling_list_enabled_flag = f; }
    bool getScalingListPresentFlag()            { return sps_scaling_list_data_present_flag; }
    bool getScalingListPresentFlag() const      { return sps_scaling_list_data_present_flag; }
    void setScalingListPresentFlag(bool f)      { sps_scaling_list_data_present_flag  = f; }

    unsigned getPCMLog2MinSize() const          { return log2_min_pcm_luma_coding_block_size; }
    void setPCMLog2MinSize(unsigned val)        { log2_min_pcm_luma_coding_block_size = val; }
    unsigned getPCMLog2MaxSize() const          { return log2_max_pcm_luma_coding_block_size; }
    void setPCMLog2MaxSize(unsigned val)        { log2_max_pcm_luma_coding_block_size = val; }

    bool getUseAMP() const                      { return amp_enabled_flag; }
    void setUseAMP(bool f)                      { amp_enabled_flag = f; }
    bool getUseSAO() const                      { return sample_adaptive_offset_enabled_flag; }
    void setUseSAO(bool f)                      { sample_adaptive_offset_enabled_flag = f; }
    bool getPCMFilterDisableFlag() const        { return pcm_loop_filter_disable_flag; }
    void setPCMFilterDisableFlag(bool f)        { pcm_loop_filter_disable_flag = f; }
    bool getTemporalIdNestingFlag() const       { return sps_temporal_id_nesting_flag; }
    void setTemporalIdNestingFlag(bool f)       { sps_temporal_id_nesting_flag = f; }

    bool getLongTermRefsPresent() const         { return long_term_ref_pics_present_flag; }
    void setLongTermRefsPresent(bool f)         { long_term_ref_pics_present_flag = f; }
    bool getTMVPFlagsPresent() const            { return sps_temporal_mvp_enable_flag; }
    void setTMVPFlagsPresent(bool f)            { sps_temporal_mvp_enable_flag = f; }
    bool getEnableTMVPFlag() const              { return m_enableTMVPFlag; }
    void setEnableTMVPFlag(bool f)              { m_enableTMVPFlag = f; }

    H265ScalingList* getScalingList()           { return &m_scalingList; }
    const H265ScalingList* getScalingList() const     { return &m_scalingList; }
    ReferencePictureSetList *getRPSList()       { return &m_RPSList; }
    const ReferencePictureSetList *getRPSList() const       { return &m_RPSList; }

    unsigned getLog2MinCUSize() const       { return m_log2MinCUSize; }
    void setLog2MinCUSize(unsigned val)     { m_log2MinCUSize = val; }
    unsigned getLog2CtbSize() const         { return m_log2CtbSize; }
    void setLog2CtbSize(unsigned val)       { m_log2CtbSize = val; }

    void setUseStrongIntraSmoothing (bool bVal)  {sps_strong_intra_smoothing_enable_flag = bVal;}
    bool getUseStrongIntraSmoothing () const     {return sps_strong_intra_smoothing_enable_flag;}

    bool getVuiParametersPresentFlag() { return vui_parameters_present_flag; }
    void setVuiParametersPresentFlag(bool b) { vui_parameters_present_flag = b; }

    H265ProfileTierLevel* getPTL()     { return &m_pcPTL; }

    // vui part
    bool getOverscanInfoPresentFlag() { return overscan_info_present_flag; }
    void setOverscanInfoPresentFlag(bool i) { overscan_info_present_flag = i; }

    bool getOverscanAppropriateFlag() { return overscan_appropriate_flag; }
    void setOverscanAppropriateFlag(bool i) { overscan_appropriate_flag = i; }

    bool getVideoSignalTypePresentFlag() { return video_signal_type_present_flag; }
    void setVideoSignalTypePresentFlag(bool i) { video_signal_type_present_flag = i; }

    int getVideoFormat() const { return video_format; }
    void setVideoFormat(int i) { video_format = i; }

    bool getVideoFullRangeFlag() const { return video_full_range_flag; }
    bool getVideoFullRangeFlag() { return video_full_range_flag; }
    void setVideoFullRangeFlag(bool i) { video_full_range_flag = i; }

    bool getColourDescriptionPresentFlag() const { return colour_description_present_flag; }
    void setColourDescriptionPresentFlag(bool i) { colour_description_present_flag = i; }

    int getColourPrimaries() const { return colour_primaries; }
    void setColourPrimaries(int i) { colour_primaries = i; }

    int getTransferCharacteristics() const { return transfer_characteristics; }
    void setTransferCharacteristics(int i) { transfer_characteristics = i; }

    int getMatrixCoefficients() const { return matrix_coefficients; }
    void setMatrixCoefficients(int i) { matrix_coefficients = i; }

    bool getChromaLocInfoPresentFlag() { return chroma_loc_info_present_flag; }
    void setChromaLocInfoPresentFlag(bool i) { chroma_loc_info_present_flag = i; }

    int getChromaSampleLocTypeTopField() { return chroma_sample_loc_type_top_field; }
    void setChromaSampleLocTypeTopField(int i) { chroma_sample_loc_type_top_field = i; }

    int getChromaSampleLocTypeBottomField() { return chroma_sample_loc_type_bottom_field; }
    void setChromaSampleLocTypeBottomField(int i) { chroma_sample_loc_type_bottom_field = i; }

    bool getNeutralChromaIndicationFlag() { return neutral_chroma_indication_flag; }
    void setNeutralChromaIndicationFlag(bool i) { neutral_chroma_indication_flag = i; }

    bool getFieldSeqFlag() const { return field_seq_flag; }
    void setFieldSeqFlag(bool i) { field_seq_flag = i; }

    bool getFrameFieldInfoPresentFlag() const { return frame_field_info_present_flag; }
    void setFrameFieldInfoPresentFlag(bool i) { frame_field_info_present_flag = i; }

    int           getDisplayWindowLeftOffset() const       { return def_disp_win_left_offset; }
    void          setDisplayWindowLeftOffset(int val)      { def_disp_win_left_offset = val; }
    int           getDisplayWindowRightOffset() const      { return def_disp_win_right_offset; }
    void          setDisplayWindowRightOffset(int val)     { def_disp_win_right_offset = val; }
    int           getDisplayWindowTopOffset() const        { return def_disp_win_top_offset; }
    void          setDisplayWindowTopOffset(int val)       { def_disp_win_top_offset = val; }
    int           getDisplayWindowBottomOffset() const     { return def_disp_win_bottom_offset; }
    void          setDisplayWindowBottomOffset(int val)    { def_disp_win_bottom_offset = val; }

    bool getHrdParametersPresentFlag() { return hrd_parameters_present_flag; }
    void setHrdParametersPresentFlag(bool i) { hrd_parameters_present_flag = i; }

    bool getBitstreamRestrictionFlag() { return bitstream_restriction_flag; }
    void setBitstreamRestrictionFlag(bool i) { bitstream_restriction_flag = i; }

    bool getTilesFixedStructureFlag() const { return tiles_fixed_structure_flag; }
    void setTilesFixedStructureFlag(bool i) { tiles_fixed_structure_flag = i; }

    bool getMotionVectorsOverPicBoundariesFlag() { return motion_vectors_over_pic_boundaries_flag; }
    void setMotionVectorsOverPicBoundariesFlag(bool i) { motion_vectors_over_pic_boundaries_flag = i; }

    bool getRestrictedRefPicListsFlag() { return m_restrictedRefPicListsFlag; }
    void setRestrictedRefPicListsFlag(bool b) { m_restrictedRefPicListsFlag = b; }

    int getMinSpatialSegmentationIdc() { return min_spatial_segmentation_idc; }
    void setMinSpatialSegmentationIdc(int i) { min_spatial_segmentation_idc = i; }

    int getMaxBytesPerPicDenom() { return max_bytes_per_pic_denom; }
    void setMaxBytesPerPicDenom(int i) { max_bytes_per_pic_denom = i; }

    int getMaxBitsPerMinCuDenom() { return max_bits_per_mincu_denom; }
    void setMaxBitsPerMinCuDenom(int i) { max_bits_per_mincu_denom = i; }

    int getLog2MaxMvLengthHorizontal() { return log2_max_mv_length_horizontal; }
    void setLog2MaxMvLengthHorizontal(int i) { log2_max_mv_length_horizontal = i; }

    int getLog2MaxMvLengthVertical() { return log2_max_mv_length_vertical; }
    void setLog2MaxMvLengthVertical(int i) { log2_max_mv_length_vertical = i; }

    H265HRD* getHrdParameters                 ()             { return &m_hrdParameters; }

    const H265TimingInfo* getTimingInfo() const { return const_cast<const H265TimingInfo*>(&m_timingInfo); }
    H265TimingInfo* getTimingInfo() { return &m_timingInfo; }

    //bool m_ListsModificationPresentFlag;
    //bool getListsModificationPresentFlag() const    { return m_ListsModificationPresentFlag; }
    //void setListsModificationPresentFlag(bool f)    { m_ListsModificationPresentFlag = f; }
};    // H265SeqParamSet

struct TileInfo
{
    Ipp32s firstCUAddr;
    Ipp32s endCUAddr;
};

// Picture parameter set structure, corresponding to the H.264 bitstream definition.
struct H265PicParamSetBase
{
    bool    transquant_bypass_enable_flag;
    bool    constrained_intra_pred_flag;
    Ipp32u  diff_cu_qp_delta_depth;
    Ipp32u  MinCUDQPSize;

    bool    pps_slice_chroma_qp_offsets_present_flag;

    bool    cu_qp_delta_enabled_flag;

    bool    tiles_enabled_flag;              //!< Indicates the presence of tiles
    bool    entropy_coding_sync_enabled_flag;  //!< Indicates the presence of wavefronts
    Ipp32u  uniform_spacing_flag;
    Ipp32u  num_tile_columns;

    Ipp32u  num_tile_rows;

    Ipp32u num_ref_idx_l0_default_active;
    Ipp32u num_ref_idx_l1_default_active;

    bool loop_filter_across_tiles_enabled_flag;
    bool loop_filter_across_slices_enabled_flag;

    bool sign_data_hiding_flag;

    bool deblocking_filter_control_present_flag;
    bool deblocking_filter_override_enabled_flag;

    bool cabac_init_present_flag;
    bool output_flag_present_flag;
    Ipp32u log2_parallel_merge_level;
    int num_extra_slice_header_bits;

    bool transform_skip_enabled_flag;

    Ipp16u       pic_parameter_set_id;            // of this picture parameter set
    Ipp8u        seq_parameter_set_id;            // of seq param set used for this pic param set

    Ipp8u        pic_order_present_flag;          // Zero indicates only delta_pic_order_cnt[0] is
                                                  // present in slice header; nonzero indicates
                                                  // delta_pic_order_cnt[1] is also present.

    bool         weighted_pred_flag;              // Nonzero indicates weighted prediction applied to
                                                  // P and SP slices
    bool         weighted_bipred_idc;             // 0: no weighted prediction in B slices
                                                  // 1: explicit weighted prediction
    Ipp8s        pic_init_qp;                     // default QP for I,P,B slices
    Ipp8s        pic_init_qs;                     // default QP for SP, SI slices

    int        chroma_qp_index_offset[2];       // offset to add to QP for chroma

    Ipp8u        deblocking_filter_variables_present_flag;    // If nonzero, deblock filter params are
                                                  // present in the slice header.
    Ipp8u        redundant_pic_cnt_present_flag;  // Nonzero indicates presence of redundant_pic_cnt

    bool         dependent_slices_enabled_flag;
    bool         cabac_independent_flag;
    bool         pps_deblocking_filter_flag;
    bool         disable_deblocking_filter_flag;

    int          pps_beta_offset_div2;
    int          pps_tc_offset_div2;

    bool lists_modification_present_flag;

    bool         pps_scaling_list_data_present_flag;
    bool         slice_header_extension_present_flag;
    H265ScalingList m_scalingList;

                                                  // in slice header
    Ipp32u       num_ref_idx_l0_active;           // num of ref pics in list 0 used to decode the picture
    Ipp32u       num_ref_idx_l1_active;           // num of ref pics in list 1 used to decode the picture

    Ipp32u getColumnWidth(Ipp32u columnIdx) { return *( m_ColumnWidthArray + columnIdx ); }
    void getColumnWidth(Ipp16u *buffer) const;    // copies columns width array into the given uint16 buffer
    void getColumnWidthMinus1(Ipp16u *buffer) const;    // copies columns width-1 array into the given uint16 buffer
    void setColumnWidth(Ipp32u* columnWidth);
    Ipp32u getRowHeight(Ipp32u rowIdx)    { return *( m_RowHeightArray + rowIdx ); }
    void getRowHeight(Ipp16u *buffer) const;      // copies rows height array into the given uint16 buffer
    void getRowHeightMinus1(Ipp16u *buffer) const;      // copies rows height-1 array into the given uint16 buffer
    void setRowHeight(Ipp32u* rowHeight);

    Ipp32u* m_RowHeightArray;
    Ipp32u* m_ColumnWidthArray;
    Ipp32u* m_CtbAddrRStoTS;
    Ipp32u* m_CtbAddrTStoRS;
    Ipp32u* m_TileIdx;

    std::vector<TileInfo> tilesInfo;

    void Reset()
    {
        H265PicParamSetBase pps = {0};
        *this = pps;
    }
};  // H265PicParamSetBase

// Picture parameter set structure, corresponding to the H.264 bitstream definition.
struct H265PicParamSet : public HeapObject, public H265PicParamSetBase
{
    H265PicParamSet()
        : H265PicParamSetBase()
    {
        m_ColumnWidthArray = NULL;
        m_RowHeightArray = NULL;
        m_CtbAddrRStoTS = NULL;
        m_CtbAddrTStoRS = NULL;
        m_TileIdx = NULL;

        Reset();
    }

    void Reset()
    {
        H265PicParamSetBase::Reset();

        pic_parameter_set_id = MAX_NUM_PIC_PARAM_SETS_H265;
        seq_parameter_set_id = MAX_NUM_SEQ_PARAM_SETS_H265;

        loop_filter_across_tiles_enabled_flag = true;
        loop_filter_across_slices_enabled_flag = true;

        if (NULL != m_ColumnWidthArray)
        {
            delete[] m_ColumnWidthArray;
            m_ColumnWidthArray = NULL;
        }

        if (NULL != m_RowHeightArray)
        {
            delete[] m_RowHeightArray;
            m_RowHeightArray = NULL;
        }

        if (NULL != m_CtbAddrRStoTS)
        {
            delete[] m_CtbAddrRStoTS;
            m_CtbAddrRStoTS = NULL;
        }

        if (NULL != m_CtbAddrTStoRS)
        {
            delete[] m_CtbAddrTStoRS;
            m_CtbAddrTStoRS = NULL;
        }

        if (NULL != m_TileIdx)
        {
            delete[] m_TileIdx;
            m_TileIdx = NULL;
        }
    }

    ~H265PicParamSet()
    {
    }

    Ipp32s GetID() const
    {
        return pic_parameter_set_id;
    }

    Ipp32u getNumTiles() const { return num_tile_rows*num_tile_columns; }

    unsigned getPPSId() const           { return pic_parameter_set_id; }
    void setPPSId(unsigned val)         { pic_parameter_set_id = (Ipp8u)val; }

    unsigned getSPSId() const           { return seq_parameter_set_id; }
    void setSPSId(unsigned val)         { seq_parameter_set_id = (Ipp8u)val; }

    bool getSignHideFlag() const        { return sign_data_hiding_flag; }
    void setSignHideFlag(bool f)        { sign_data_hiding_flag = f; }

    bool getCabacInitPresentFlag() const    { return cabac_init_present_flag; }
    void setCabacInitPresentFlag(bool f)    { cabac_init_present_flag = f; }

    unsigned getNumRefIdxL0DefaultActive() const    { return num_ref_idx_l0_active; }
    void setNumRefIdxL0DefaultActive(unsigned val)  { num_ref_idx_l0_active = val; }
    unsigned getNumRefIdxL1DefaultActive() const    { return num_ref_idx_l1_active; }
    void setNumRefIdxL1DefaultActive(unsigned val)  { num_ref_idx_l1_active = val; }
    int getPicInitQP() const         { return pic_init_qp; }
    void setPicInitQP(int val)       { pic_init_qp = (Ipp8s)val; }
    bool getUseDQP () const             { return cu_qp_delta_enabled_flag;        }
    void setUseDQP(bool f)              { cu_qp_delta_enabled_flag = f; }
    bool getConstrainedIntraPred() const    { return constrained_intra_pred_flag; }
    void setConstrainedIntraPred(bool f)    { constrained_intra_pred_flag = f; }

    bool getSliceChromaQpFlag () const   { return  pps_slice_chroma_qp_offsets_present_flag; }
    void setSliceChromaQpFlag ( bool b ) { pps_slice_chroma_qp_offsets_present_flag = b;     }

    unsigned getMaxCuDQPDepth() const   { return diff_cu_qp_delta_depth; }
    void setMaxCuDQPDepth(unsigned val) { diff_cu_qp_delta_depth = val; }

    void setChromaCbQpOffset(int val)   { chroma_qp_index_offset[0] = val; }
    Ipp32s getChromaCbQpOffset() const  { return chroma_qp_index_offset[0]; }
    void setChromaCrQpOffset(int val)   { chroma_qp_index_offset[1] = val; }
    Ipp32s getChromaCrQpOffset() const  { return chroma_qp_index_offset[1]; }

    bool getUseWP() const       { return weighted_pred_flag != 0; }
    void setUseWP(bool f)       { weighted_pred_flag = f? 1 : 0; }
    bool getWPBiPred() const    { return weighted_bipred_idc != 0; }
    void setWPBiPred(bool f)    { weighted_bipred_idc = f? 1 : 0; }
    bool getOutputFlagPresentFlag() const       { return output_flag_present_flag; }
    void setOutputFlagPresentFlag(bool f)       { output_flag_present_flag = f; }
    bool getDependentSliceSegmentEnabledFlag() const  { return dependent_slices_enabled_flag; }
    void setDependentSliceSegmentEnabledFlag(bool f)  { dependent_slices_enabled_flag = f; }
    bool getTransquantBypassEnableFlag() const  { return transquant_bypass_enable_flag; }
    void setTransquantBypassEnableFlag(bool f)  { transquant_bypass_enable_flag = f; }
    bool getTilesEnabledFlag() const                      { return tiles_enabled_flag; }
    void setTilesEnabledFlag(bool val)                    { tiles_enabled_flag = val; }
    bool getEntropyCodingSyncEnabledFlag() const          { return entropy_coding_sync_enabled_flag; }
    void setEntropyCodingSyncEnabledFlag(bool val)        { entropy_coding_sync_enabled_flag = val; }
    unsigned getNumColumns() const    { return num_tile_columns; }
    void setNumColumns(unsigned val)  { num_tile_columns = val; }
    unsigned getNumRows() const       { return num_tile_rows; }
    void setNumRows(unsigned val)     { num_tile_rows = val; }

    unsigned getUniformSpacingFlag() const   { return uniform_spacing_flag; }
    void setUniformSpacingFlag(unsigned val) { uniform_spacing_flag = val; }

    bool    getUseTransformSkip       () const   { return transform_skip_enabled_flag;     }
    void    setUseTransformSkip       ( bool b ) { transform_skip_enabled_flag  = b;       }

    bool getCabacIndependentFlag() const    { return cabac_independent_flag; }
    void setCabacIndependentFlag(bool f)    { cabac_independent_flag = f; }

    bool getDeblockingFilterControlPresentFlag() const  { return deblocking_filter_control_present_flag; }
    void setDeblockingFilterControlPresentFlag(bool f)  { deblocking_filter_control_present_flag = f; }

    void setDeblockingFilterOverrideEnabledFlag( bool val ) { deblocking_filter_override_enabled_flag = val; }
    bool getDeblockingFilterOverrideEnabledFlag() const     { return deblocking_filter_override_enabled_flag; }

    void setPicDisableDeblockingFilterFlag(bool val)        { disable_deblocking_filter_flag = val; }       //!< set offset for deblocking filter disabled
    bool getPicDisableDeblockingFilterFlag() const          { return disable_deblocking_filter_flag; }      //!< get offset for deblocking filter disabled

    int getDeblockingFilterBetaOffsetDiv2() const     { return pps_beta_offset_div2; }
    void setDeblockingFilterBetaOffsetDiv2(int val)   { pps_beta_offset_div2 = val; }

    int getDeblockingFilterTcOffsetDiv2() const       { return pps_tc_offset_div2; }
    void setDeblockingFilterTcOffsetDiv2(int val)     { pps_tc_offset_div2 = val; }

    bool getScalingListPresentFlag() const  { return pps_scaling_list_data_present_flag; }
    void setScalingListPresentFlag(bool f)  { pps_scaling_list_data_present_flag = f; }

    bool getListsModificationPresentFlag() const    { return lists_modification_present_flag; }
    void setListsModificationPresentFlag(bool f)    { lists_modification_present_flag = f; }

    unsigned getLog2ParallelMergeLevel() const  { return log2_parallel_merge_level; }
    void setLog2ParallelMergeLevel(unsigned val)  { log2_parallel_merge_level = val; }

    int getNumExtraSliceHeaderBits() const { return num_extra_slice_header_bits; }
    void setNumExtraSliceHeaderBits(int i) { num_extra_slice_header_bits = i; }

    void setLoopFilterAcrossSlicesEnabledFlag     ( bool   bValue  )    { loop_filter_across_slices_enabled_flag = bValue; }
    bool getLoopFilterAcrossSlicesEnabledFlag () const { return loop_filter_across_slices_enabled_flag;   }

    bool getSliceHeaderExtensionPresentFlag() const     { return slice_header_extension_present_flag; }
    void setSliceHeaderExtensionPresentFlag(bool f)     { slice_header_extension_present_flag = f; }

    H265ScalingList* getScalingList()               { return &m_scalingList; }
    const H265ScalingList* getScalingList() const   { return &m_scalingList; }
};    // H265PicParamSet

typedef Ipp32s H264DecoderMBAddr;

typedef struct {
  // Explicit weighted prediction parameters parsed in slice header,
  // or Implicit weighted prediction parameters (8 bits depth values).
  bool        bPresentFlag;
  unsigned    uiLog2WeightDenom;
  int         iWeight;
  int         iOffset;
  int         iDeltaWeight; // for HW decoder

  // Weighted prediction scaling values built from above parameters (bitdepth scaled):
  int         w, o, offset, shift, round;
} wpScalingParam;

// Slice header structure, corresponding to the H.264 bitstream definition.
struct H265SliceHeader
{
    Ipp32s first_slice_segment_in_pic_flag;
    Ipp32u m_HeaderBitstreamOffset;
    //h265 elements of slice header ----------------------------------------------------------------------------------------------------------------
    const H265SeqParamSet* m_SeqParamSet;
    const H265PicParamSet* m_PicParamSet;
    Ipp32u *m_TileByteLocation;
    Ipp32s m_TileCount;

    Ipp32u* m_SubstreamSizes;

    Ipp32u m_sliceSegmentCurStartCUAddr;
    Ipp32u m_sliceSegmentCurEndCUAddr;
    bool m_DependentSliceSegmentFlag;
    Ipp32s SliceQP;
    Ipp32s slice_qp_delta;                       // to calculate default slice QP
    Ipp32s slice_cb_qp_offset;
    Ipp32s slice_cr_qp_offset;
    bool m_deblockingFilterDisable;
    bool m_deblockingFilterOverrideFlag;      //< offsets for deblocking filter inherit from PPS
    Ipp32s m_deblockingFilterBetaOffsetDiv2;    //< beta offset for deblocking filter
    Ipp32s m_deblockingFilterTcOffsetDiv2;      //< tc offset for deblocking filter

    Ipp32u SliceCurStartCUAddr;
    Ipp32u SliceCurEndCUAddr;
    Ipp32u collocated_from_l0_flag;   // direction to get colocated CUs
    Ipp32u m_ColRefIdx;
    bool m_CheckLDC;
    int m_numRefIdx[3]; //  for multiple reference of current slice. IT SEEMS BE SAME AS num_ref_idx_l0_active, l1, lc

    Ipp32s RefPOCList[2][MAX_NUM_REF_PICS + 1];
    bool RefLTList[2][MAX_NUM_REF_PICS + 1];

    bool slice_sao_luma_flag;
    bool slice_sao_chroma_flag;      ///< SAO chroma enabled flag
    ReferencePictureSet *m_pRPS;
    ReferencePictureSet m_localRPS;
    RefPicListModification m_RefPicListModification;

    Ipp32s m_MaxNumMergeCand;
    bool m_MvdL1Zero;

    // flag equal 1 means that the slice belong to IDR or anchor access unit
    Ipp32u IdrPicFlag;

    // specifies the type of RBSP data structure contained in the NAL unit as
    // specified in Table 7-1 of h264 standard
    NalUnitType nal_unit_type;
    Ipp32u m_nuh_temporal_id;

    Ipp16u        pic_parameter_set_id;                 // of pic param set used for this slice
    Ipp8u         num_ref_idx_active_override_flag;     // nonzero: use ref_idx_active from slice header
                                                        // instead of those from pic param set
    Ipp8u         no_output_of_prior_pics_flag;         // nonzero: remove previously decoded pictures
                                                        // from decoded picture buffer
    Ipp8u         long_term_reference_flag;             // How to set MaxLongTermFrameIdx
    Ipp32u        cabac_init_idc;                      // CABAC initialization table index (0..2)
    bool          m_CabacInitFlag;
    SliceType     slice_type;
    Ipp32s        pic_order_cnt_lsb;                    // picture order count (mod MaxPicOrderCntLsb)
    Ipp32s        num_ref_idx_l0_active;                // num of ref pics in list 0 used to decode the slice,
                                                        // see num_ref_idx_active_override_flag
    Ipp32s        num_ref_idx_l1_active;                // num of ref pics in list 1 used to decode the slice
                                                        // see num_ref_idx_active_override_flag

    bool        pic_output_flag;
    unsigned    m_uMaxTLayers;
    unsigned    m_uBitsForPOC;
    bool        m_enableTMVPFlag;
    bool        slice_loop_filter_across_slices_enabled_flag;

    wpScalingParam  m_weightPredTable[2][MAX_NUM_REF_PICS][3]; // [REF_PIC_LIST_0 or REF_PIC_LIST_1][refIdx][0:Y, 1:U, 2:V]

    int m_numEntryPointOffsets;
    unsigned slice_segment_address;
    unsigned m_RPSIndex;

    unsigned m_uiLog2WeightDenomLuma;
    unsigned m_uiLog2WeightDenomChroma;

    void setPPSId(unsigned val) { pic_parameter_set_id = (Ipp16u)val; }

}; // H265SliceHeader

struct H265SEIPayLoadBase
{
    SEI_TYPE payLoadType;
    Ipp32u   payLoadSize;

    union SEIMessages
    {
        struct BufferingPeriod
        {
            Ipp32u initial_cbp_removal_delay[2][16];
            Ipp32u initial_cbp_removal_delay_offset[2][16];
        }buffering_period;

        struct PicTiming
        {
            Ipp32u cbp_removal_delay;
            Ipp32u dpb_output_delay;
            DisplayPictureStruct_H265 pic_struct;
            Ipp8u  clock_timestamp_flag[16];
            struct ClockTimestamps
            {
                Ipp8u ct_type;
                Ipp8u nunit_field_based_flag;
                Ipp8u counting_type;
                Ipp8u full_timestamp_flag;
                Ipp8u discontinuity_flag;
                Ipp8u cnt_dropped_flag;
                Ipp8u n_frames;
                Ipp8u seconds_value;
                Ipp8u minutes_value;
                Ipp8u hours_value;
                Ipp8u time_offset;
            }clock_timestamps[16];
        }pic_timing;

        struct PanScanRect
        {
            Ipp8u  pan_scan_rect_id;
            Ipp8u  pan_scan_rect_cancel_flag;
            Ipp8u  pan_scan_cnt;
            Ipp32u pan_scan_rect_left_offset[32];
            Ipp32u pan_scan_rect_right_offset[32];
            Ipp32u pan_scan_rect_top_offset[32];
            Ipp32u pan_scan_rect_bottom_offset[32];
            Ipp8u  pan_scan_rect_repetition_period;
        }pan_scan_rect;

        struct UserDataRegistered
        {
            Ipp8u itu_t_t35_country_code;
            Ipp8u itu_t_t35_country_code_extension_byte;
        } user_data_registered;

        struct RecoveryPoint
        {
            Ipp8u recovery_frame_cnt;
            Ipp8u exact_match_flag;
            Ipp8u broken_link_flag;
            Ipp8u changing_slice_group_idc;
        }recovery_point;

        struct SparePic
        {
            Ipp32u target_frame_num;
            Ipp8u  spare_field_flag;
            Ipp8u  target_bottom_field_flag;
            Ipp8u  num_spare_pics;
            Ipp8u  delta_spare_frame_num[16];
            Ipp8u  spare_bottom_field_flag[16];
            Ipp8u  spare_area_idc[16];
            Ipp8u  *spare_unit_flag[16];
            Ipp8u  *zero_run_length[16];
        }spare_pic;

        struct SceneInfo
        {
            Ipp8u scene_info_present_flag;
            Ipp8u scene_id;
            Ipp8u scene_transition_type;
            Ipp8u second_scene_id;
        }scene_info;

        struct SubSeqInfo
        {
            Ipp8u sub_seq_layer_num;
            Ipp8u sub_seq_id;
            Ipp8u first_ref_pic_flag;
            Ipp8u leading_non_ref_pic_flag;
            Ipp8u last_pic_flag;
            Ipp8u sub_seq_frame_num_flag;
            Ipp8u sub_seq_frame_num;
        }sub_seq_info;

        struct SubSeqLayerCharacteristics
        {
            Ipp8u  num_sub_seq_layers;
            Ipp8u  accurate_statistics_flag[16];
            Ipp16u average_bit_rate[16];
            Ipp16u average_frame_rate[16];
        }sub_seq_layer_characteristics;

        struct SubSeqCharacteristics
        {
            Ipp8u  sub_seq_layer_num;
            Ipp8u  sub_seq_id;
            Ipp8u  duration_flag;
            Ipp8u  sub_seq_duration;
            Ipp8u  average_rate_flag;
            Ipp8u  accurate_statistics_flag;
            Ipp16u average_bit_rate;
            Ipp16u average_frame_rate;
            Ipp8u  num_referenced_subseqs;
            Ipp8u  ref_sub_seq_layer_num[16];
            Ipp8u  ref_sub_seq_id[16];
            Ipp8u  ref_sub_seq_direction[16];
        }sub_seq_characteristics;

        struct FullFrameFreeze
        {
            Ipp32u full_frame_freeze_repetition_period;
        }full_frame_freeze;

        struct FullFrameSnapshot
        {
            Ipp8u snapshot_id;
        }full_frame_snapshot;

        struct ProgressiveRefinementSegmentStart
        {
            Ipp8u progressive_refinement_id;
            Ipp8u num_refinement_steps;
        }progressive_refinement_segment_start;

        struct FilmGrainCharacteristics
        {
            Ipp8u film_grain_characteristics_cancel_flag;
            Ipp8u model_id;
            Ipp8u separate_colour_description_present_flag;
            Ipp8u film_grain_bit_depth_luma;
            Ipp8u film_grain_bit_depth_chroma;
            Ipp8u film_grain_full_range_flag;
            Ipp8u film_grain_colour_primaries;
            Ipp8u film_grain_transfer_characteristics;
            Ipp8u film_grain_matrix_coefficients;
            Ipp8u blending_mode_id;
            Ipp8u log2_scale_factor;
            Ipp8u comp_model_present_flag[3];
            Ipp8u num_intensity_intervals[3];
            Ipp8u num_model_values[3];
            Ipp8u intensity_interval_lower_bound[3][256];
            Ipp8u intensity_interval_upper_bound[3][256];
            Ipp8u comp_model_value[3][3][256];
            Ipp8u film_grain_characteristics_repetition_period;
        }film_grain_characteristics;

        struct DeblockingFilterDisplayPreference
        {
            Ipp8u deblocking_display_preference_cancel_flag;
            Ipp8u display_prior_to_deblocking_preferred_flag;
            Ipp8u dec_frame_buffering_constraint_flag;
            Ipp8u deblocking_display_preference_repetition_period;
        }deblocking_filter_display_preference;

        struct StereoVideoInfo
        {
            Ipp8u field_views_flag;
            Ipp8u top_field_is_left_view_flag;
            Ipp8u current_frame_is_left_view_flag;
            Ipp8u next_frame_is_second_view_flag;
            Ipp8u left_view_self_contained_flag;
            Ipp8u right_view_self_contained_flag;
        }stereo_video_info;

        struct ScalabilityInfo
        {
            Ipp32u num_layers;
        } scalability_info;

    }SEI_messages;

    void Reset()
    {
        memset(this, 0, sizeof(H265SEIPayLoadBase));

        payLoadType = SEI_RESERVED;
        payLoadSize = 0;
    }
};

struct H265SEIPayLoad : public HeapObject, public H265SEIPayLoadBase
{
    std::vector<Ipp8u> user_data; // for UserDataRegistered or UserDataUnRegistered

    H265SEIPayLoad()
        : H265SEIPayLoadBase()
    {
    }

    virtual void Reset()
    {
        H265SEIPayLoadBase::Reset();
        user_data.clear();
    }

    Ipp32s GetID() const
    {
        return payLoadType;
    }
};

enum
{
    CHROMA_FORMAT_400_H265       = 0,
    CHROMA_FORMAT_420_H265       = 1,
    CHROMA_FORMAT_422_H265       = 2,
    CHROMA_FORMAT_444_H265       = 3
};

class h265_exception
{
public:
    h265_exception(Ipp32s status = -1)
        : m_Status(status)
    {
    }

    virtual ~h265_exception()
    {
    }

    Ipp32s GetStatus() const
    {
        return m_Status;
    }

private:
    Ipp32s m_Status;
};


#pragma pack(1)

extern Ipp32s lock_failed_H265;

#pragma pack()

template <typename T>
inline T * h265_new_array_throw(Ipp32s size)
{
    T * t = new T[size];
    if (!t)
        throw h265_exception(UMC::UMC_ERR_ALLOC);
    return t;
}

template <typename T>
inline T * h265_new_throw()
{
    T * t = new T();
    if (!t)
        throw h265_exception(UMC::UMC_ERR_ALLOC);
    return t;
}

template <typename T, typename T1>
inline T * h265_new_throw_1(T1 t1)
{
    T * t = new T(t1);
    if (!t)
        throw h265_exception(UMC::UMC_ERR_ALLOC);
    return t;
}

struct H265IntraTypesProp
{
    H265IntraTypesProp()
    {
        Reset();
    }

    Ipp32s m_nSize;                                             // (Ipp32s) size of allocated intra type array
    UMC::MemID m_mid;                                                // (MemID) mem id of allocated buffer for intra types

    void Reset(void)
    {
        m_nSize = 0;
        m_mid = MID_INVALID;
    }
};

inline UMC::ColorFormat GetUMCColorFormat_H265(Ipp32s color_format)
{
    UMC::ColorFormat format;
    switch(color_format)
    {
    case 0:
        format = UMC::GRAY;
        break;
    case 2:
        format = UMC::YUV422;
        break;
    case 3:
        format = UMC::YUV444;
        break;
    case 1:
    default:
        format = UMC::YUV420;
        break;
    }

    return format;
}

inline Ipp32s GetH265ColorFormat(UMC::ColorFormat color_format)
{
    Ipp32s format;
    switch(color_format)
    {
    case UMC::GRAY:
    case UMC::GRAYA:
        format = 0;
        break;
    case UMC::YUV422A:
    case UMC::YUV422:
        format = 2;
        break;
    case UMC::YUV444:
    case UMC::YUV444A:
        format = 3;
        break;
    case UMC::YUV420:
    case UMC::YUV420A:
    case UMC::NV12:
    case UMC::YV12:
    default:
        format = 1;
        break;
    }

    return format;
}

inline UMC::ColorFormat ConvertColorFormatToAlpha_H265(UMC::ColorFormat cf)
{
    UMC::ColorFormat cfAlpha = cf;
    switch(cf)
    {
    case UMC::GRAY:
        cfAlpha = UMC::GRAYA;
        break;
    case UMC::YUV420:
        cfAlpha = UMC::YUV420A;
        break;
    case UMC::YUV422:
        cfAlpha = UMC::YUV422A;
        break;
    case UMC::YUV444:
        cfAlpha = UMC::YUV444A;
        break;
    default:
        break;
    }

    return cfAlpha;
}

inline size_t CalculateSuggestedSize(const H265SeqParamSet * sps)
{
    size_t base_size = sps->pic_width_in_luma_samples * sps->pic_height_in_luma_samples;
    size_t size = 0;

    switch (sps->chroma_format_idc)
    {
    case 0:  // YUV400
        size = base_size;
        break;
    case 1:  // YUV420
        size = (base_size * 3) / 2;
        break;
    case 2: // YUV422
        size = base_size + base_size;
        break;
    case 3: // YUV444
        size = base_size + base_size + base_size;
        break;
    };

#if RANDOM_ENC_TESTING
    return size * 10;
#else
    return size;
#endif
}

static void H265_FORCEINLINE small_memcpy( void* dst, const void* src, int len )
{
#if defined( __INTEL_COMPILER ) // || defined( __GNUC__ )  // TODO: check with GCC
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
    memcpy(dst, src, len);
#endif
}

} // end namespace UMC_HEVC_DECODER

#include "umc_h265_dec_ippwrap.h"



#endif // H265_GLOBAL_ROM_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
