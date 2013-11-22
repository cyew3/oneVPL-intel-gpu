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
#include "ipps.h"

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

enum {
    AMVP_MAX_NUM_CAND       = 2,           // max number of final candidates
    MERGE_MAX_NUM_CAND      = 5
};

#define REG_DCT 65535

#define CU_DQP_TU_CMAX 5 //max number bins for truncated unary
#define CU_DQP_EG_k 0 //expgolomb order

enum ComponentPlane
{
    COMPONENT_LUMA = 0,
    COMPONENT_CHROMA_U,
    COMPONENT_CHROMA_V,
    COMPONENT_CHROMA = COMPONENT_CHROMA_U
};

/// coefficient scanning type used in ACS
enum COEFF_SCAN_TYPE
{
    SCAN_DIAG = 0,      ///< typical zigzag scan
    SCAN_HOR,           ///< horizontal first scan
    SCAN_VER            ///< vertical first scan
};

// coeffs decoding constants
enum
{
    SCAN_SET_SIZE = 16,
    LAST_MINUS_FIRST_SIG_SCAN_THRESHOLD = 3,

    NUM_CONTEXT_QT_CBF                      = 5,      // number of context models for QT CBF
    NUM_CONTEXT_SIG_COEFF_GROUP_FLAG        = 2,      // number of contexts for sig coeff group 
    NUM_CONTEXT_SIG_FLAG_LUMA               = 27,     // number of contexts for luma sig flag
    NUM_CONTEX_LAST_COEF_FLAG_XY            = 15,     // number of contexts for last coefficient position
    NUM_CONTEXT_ONE_FLAG_LUMA               = 16,     // number of contexts for greater than 1 flag of luma
    NUM_CONTEXT_ABS_FLAG_LUMA               = 4,      // number of contexts for greater than 2 flag of luma
    NUM_CONTEXT_TRANSFORMSKIP_FLAG          = 1,      // number of contexts for transform skipping flag

    LARGER_THAN_ONE_FLAG_NUMBER             = 8,       // maximum number of largerThan1 flag coded in one chunk
    COEFF_ABS_LEVEL_REMAIN_BIN_THRESHOLD    = 3        // maximum codeword length of coeff_abs_level_remaining reduced to 32.
};

enum
{
    INTRA_LUMA_PLANAR_IDX   = 0,                     // index for intra planar mode
    INTRA_LUMA_VER_IDX      = 26,                    // index for intra vertical   mode
    INTRA_LUMA_HOR_IDX      = 10,                    // index for intra hor mode
    INTRA_LUMA_DC_IDX       = 1,                     // index for intra dc mode
    INTRA_DM_CHROMA_IDX     = 36,                    // index for chroma from luma mode 

    INTRA_NUM_CHROMA_MODE   = 5                      // total number of chroma modes
};

#define MAX_CPB_CNT                     32  ///< Upper bound of (cpb_cnt_minus1 + 1)

enum
{
    MAX_TEMPORAL_LAYER  = 8,

    MAX_VPS_NUM_LAYER_SETS  = 1024,
    MAX_NUH_LAYER_ID        = 1,           // currently max nuh_layer_id value is 1

    MAX_CU_DEPTH = 6,
    MAX_CU_SIZE = (1 << MAX_CU_DEPTH), // maximum allowable size of CU
    MIN_PU_SIZE = 4,
    MAX_NUM_PU_IN_ROW = (MAX_CU_SIZE/MIN_PU_SIZE)
};

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


typedef Ipp8u H265PlaneYCommon;
typedef Ipp8u H265PlaneUVCommon;
typedef Ipp16s H265CoeffsCommon;

typedef H265CoeffsCommon *H265CoeffsPtrCommon;
typedef H265PlaneYCommon *H265PlanePtrYCommon;
typedef H265PlaneUVCommon *H265PlanePtrUVCommon;

enum NalUnitType
{
  NAL_UT_CODED_SLICE_TRAIL_N    = 0,
  NAL_UT_CODED_SLICE_TRAIL_R    = 1,

  NAL_UT_CODED_SLICE_TSA_N      = 2,
  NAL_UT_CODED_SLICE_TLA_R      = 3,

  NAL_UT_CODED_SLICE_STSA_N     = 4,
  NAL_UT_CODED_SLICE_STSA_R     = 5,

  NAL_UT_CODED_SLICE_RADL_N     = 6, 
  NAL_UT_CODED_SLICE_RADL_R     = 7,

  NAL_UT_CODED_SLICE_RASL_N     = 8, 
  NAL_UT_CODED_SLICE_RASL_R     = 9,

  NAL_UT_CODED_SLICE_BLA_W_LP   = 16,
  NAL_UT_CODED_SLICE_BLA_W_RADL = 17,
  NAL_UT_CODED_SLICE_BLA_N_LP   = 18,
  NAL_UT_CODED_SLICE_IDR_W_RADL = 19,
  NAL_UT_CODED_SLICE_IDR_N_LP   = 20,
  NAL_UT_CODED_SLICE_CRA        = 21,

  NAL_UT_VPS                    = 32,
  NAL_UT_SPS                    = 33,
  NAL_UT_PPS                    = 34,
  NAL_UT_AU_DELIMITER           = 35,
  NAL_UT_EOS                    = 36,
  NAL_UT_EOB                    = 37,
  NAL_UT_FILLER_DATA            = 38,
  NAL_UT_SEI                    = 39, // Prefix SEI
  NAL_UT_SEI_SUFFIX             = 40, // Suffix SEI

  NAL_UT_INVALID = 64
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

    Ipp32u              m_PicWidth;
    Ipp32u              m_PicHeight;
    Ipp32u              m_MaxCUSize;
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
    void PCMSampleRestoration(H265CodingUnit* pcCU, Ipp32u AbsZorderIdx, Ipp32u Depth);

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

    MAX_NUM_VPS_PARAM_SETS_H265 = 16,
    MAX_NUM_SEQ_PARAM_SETS_H265 = 16,
    MAX_NUM_PIC_PARAM_SETS_H265 = 64,

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

class H265ScalingList
{
public:
    H265ScalingList() { m_initialized = false; }
    ~H265ScalingList()
    {
        if (m_initialized)
            destroy();
    }

    int*      getScalingListAddress   (unsigned sizeId, unsigned listId)          { return m_scalingListCoef[sizeId][listId]; }
    const int* getScalingListAddress  (unsigned sizeId, unsigned listId) const    { return m_scalingListCoef[sizeId][listId]; }
    void     setRefMatrixId           (unsigned sizeId, unsigned listId, unsigned u)   { m_refMatrixId[sizeId][listId] = u; }
    unsigned getRefMatrixId           (unsigned sizeId, unsigned listId)           { return m_refMatrixId[sizeId][listId]; }
    void     setScalingListDC         (unsigned sizeId, unsigned listId, unsigned u)   { m_scalingListDC[sizeId][listId] = u; }
    void     processRefMatrix         (unsigned sizeId, unsigned listId , unsigned refListId);
    int      getScalingListDC         (unsigned sizeId, unsigned listId) const     { return m_scalingListDC[sizeId][listId]; }

    void init();
    bool is_initialized(void) { return m_initialized; }
    void initFromDefaultScalingList(void);
    void calculateDequantCoef(void);

    Ipp16s *m_dequantCoef[SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM][SCALING_LIST_REM_NUM];

private:
    H265_FORCEINLINE Ipp16s* getDequantCoeff(Ipp32u list, Ipp32u qp, Ipp32u size)
    {
        return m_dequantCoef[size][list][qp];
    }
    __inline void processScalingListDec(Ipp32s *coeff, Ipp16s *dequantcoeff, Ipp32s invQuantScales, Ipp32u height, Ipp32u width, Ipp32u ratio, Ipp32u sizuNum, Ipp32u dc);
    static int *getScalingListDefaultAddress (unsigned sizeId, unsigned listId);

    void destroy();

    int      m_scalingListDC               [SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM];
    unsigned m_refMatrixId                 [SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM];
    int      m_scalingListCoef             [SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM][MAX_MATRIX_COEF_NUM];
    bool     m_initialized;
};

struct H265PTL
{
    Ipp32u      profile_space;
    Ipp8u       tier_flag;
    Ipp32u      profile_idc;
    Ipp32u      profile_compatibility_flags;    // bitfield, 32 flags
    Ipp8u       progressive_source_flag;
    Ipp8u       interlaced_source_flag;
    Ipp8u       non_packed_constraint_flag;
    Ipp8u       frame_only_constraint_flag;
    Ipp32u      level_idc;

    H265PTL()   { memset(this, 0, sizeof(*this)); }
};


#define H265_MAX_SUBLAYER_PTL   6
struct H265ProfileTierLevel
{
    H265PTL generalPTL;
    H265PTL subLayerPTL[H265_MAX_SUBLAYER_PTL];
    Ipp32u  sub_layer_profile_present_flags;       // bitfield [0:H265_MAX_SUBLAYER_PTL]
    Ipp32u  sub_layer_level_present_flag;          // bitfield [0:H265_MAX_SUBLAYER_PTL]

    H265ProfileTierLevel()
        : sub_layer_profile_present_flags(0)
        , sub_layer_level_present_flag(0)
    {
    }

    const H265PTL* GetGeneralPTL() const        { return &generalPTL; }
    const H265PTL* GetSubLayerPTL(Ipp32s i) const  { return &subLayerPTL[i]; }

    H265PTL* GetGeneralPTL()       { return &generalPTL; }
    H265PTL* GetSubLayerPTL(Ipp32s i) { return &subLayerPTL[i]; }
};

struct H265HrdSubLayerInfo
{
    Ipp8u       fixed_pic_rate_general_flag;
    Ipp8u       fixed_pic_rate_within_cvs_flag;
    Ipp32u      elemental_duration_in_tc;
    Ipp8u       low_delay_hrd_flag;
    Ipp32u      cpb_cnt;

    // sub layer hrd params
    Ipp32u      bit_rate_value[MAX_CPB_CNT][2];
    Ipp32u      cpb_size_value[MAX_CPB_CNT][2];
    Ipp32u      cpb_size_du_value[MAX_CPB_CNT][2];
    Ipp32u      bit_rate_du_value[MAX_CPB_CNT][2];
    Ipp8u       cbr_flag[MAX_CPB_CNT][2];
};

struct H265HRD
{
    Ipp8u       nal_hrd_parameters_present_flag;
    Ipp8u       vcl_hrd_parameters_present_flag;
    Ipp8u       sub_pic_hrd_params_present_flag;

    // sub_pic_hrd_params_present_flag
    Ipp32u      tick_divisor;
    Ipp32u      du_cpb_removal_delay_increment_length;
    Ipp8u       sub_pic_cpb_params_in_pic_timing_sei_flag;
    Ipp32u      dpb_output_delay_du_length;

    Ipp32u      bit_rate_scale;
    Ipp32u      cpb_size_scale;
    Ipp32u      cpb_size_du_scale;
    Ipp32u      initial_cpb_removal_delay_length;
    Ipp32u      au_cpb_removal_delay_length;
    Ipp32u      dpb_output_delay_length;

    H265HrdSubLayerInfo m_HRD[MAX_TEMPORAL_LAYER];

    H265HRD() {
        ::memset(this, 0, sizeof(*this));
    }

    H265HrdSubLayerInfo * GetHRDSubLayerParam(Ipp32u i) { return &m_HRD[i]; }
};

struct H265TimingInfo
{
    Ipp8u   vps_timing_info_present_flag;
    Ipp32u  vps_num_units_in_tick;
    Ipp32u  vps_time_scale;
    Ipp8u   vps_poc_proportional_to_timing_flag;
    Ipp32s  vps_num_ticks_poc_diff_one;

public:
    H265TimingInfo()
        : vps_timing_info_present_flag(false)
        , vps_num_units_in_tick(1000)
        , vps_time_scale(30000)
        , vps_poc_proportional_to_timing_flag(false)
        , vps_num_ticks_poc_diff_one(0)
    {}
};

/// VPS class
class H265VideoParamSet : public HeapObject
{
public:
    Ipp32u      vps_video_parameter_set_id;
    Ipp32u      vps_max_layers;
    Ipp32u      vps_max_sub_layers;
    Ipp8u       vps_temporal_id_nesting_flag;

    // profile_tier_level
    H265ProfileTierLevel    m_pcPTL;

    // vpd sub layer ordering info
    Ipp32u      vps_max_dec_pic_buffering[MAX_TEMPORAL_LAYER];
    Ipp32u      vps_num_reorder_pics[MAX_TEMPORAL_LAYER];
    Ipp32u      vps_max_latency_increase[MAX_TEMPORAL_LAYER];

    Ipp32u      vps_max_layer_id;
    Ipp32u      vps_num_layer_sets;
    Ipp8u       layer_id_included_flag[MAX_VPS_NUM_LAYER_SETS][MAX_NUH_LAYER_ID];

    // vps timing info
    H265TimingInfo          m_timingInfo;

    // hrd parameters
    Ipp32u    vps_num_hrd_parameters;
    Ipp32u*   hrd_layer_set_idx;
    Ipp8u*    cprms_present_flag;
    H265HRD*  m_hrdParameters;

public:
    H265VideoParamSet()
        : HeapObject()
        , vps_num_hrd_parameters(0)
        , hrd_layer_set_idx(0)
        , cprms_present_flag(0)
        , m_hrdParameters(0)
    {
        Reset();
    }

   ~H265VideoParamSet() {
        delete[] m_hrdParameters;
        delete[] hrd_layer_set_idx;
        delete[] cprms_present_flag;
    }

    void createHrdParamBuffer()
    {
        delete[] m_hrdParameters;
        m_hrdParameters = new H265HRD[vps_num_hrd_parameters];

        delete[] hrd_layer_set_idx;
        hrd_layer_set_idx = new unsigned[vps_num_hrd_parameters];

        delete[] cprms_present_flag;
        cprms_present_flag = new Ipp8u[vps_num_hrd_parameters];
    }

    void Reset()
    {
        vps_video_parameter_set_id = 0;
        vps_max_sub_layers = 0;
        vps_temporal_id_nesting_flag = false;
        vps_num_hrd_parameters = 0;

        delete m_hrdParameters;
        m_hrdParameters = 0;

        delete hrd_layer_set_idx;
        hrd_layer_set_idx = 0;

        delete cprms_present_flag;
        cprms_present_flag = 0;

        for (int i = 0; i < MAX_TEMPORAL_LAYER; i++)
        {
            vps_num_reorder_pics[i] = 0;
            vps_max_dec_pic_buffering[i] = 0;
            vps_max_latency_increase[i] = 0;
        }
    }

    Ipp32s GetID() const
    {
        return vps_video_parameter_set_id;
    }

    H265HRD* getHrdParameters   ( unsigned i )             { return &m_hrdParameters[ i ]; }
    H265ProfileTierLevel* getPTL() { return &m_pcPTL; }
    H265TimingInfo* getTimingInfo() { return &m_timingInfo; }
};

typedef Ipp32u IntraType;

struct ReferencePictureSet
{
    Ipp8u inter_ref_pic_set_prediction_flag;

    Ipp32s num_negative_pics;
    Ipp32s num_positive_pics;

    Ipp32s num_pics;
    
    Ipp32s num_lt_pics;

    Ipp32s num_long_term_pics;
    Ipp32s num_long_term_sps;

    Ipp32s m_DeltaPOC[MAX_NUM_REF_PICS];
    Ipp32s m_POC[MAX_NUM_REF_PICS];
    Ipp8u used_by_curr_pic_flag[MAX_NUM_REF_PICS];
    
    Ipp8u delta_poc_msb_present_flag[MAX_NUM_REF_PICS];
    Ipp8u delta_poc_msb_cycle_lt[MAX_NUM_REF_PICS];
    Ipp32s poc_lbs_lt[MAX_NUM_REF_PICS];

    ReferencePictureSet();

    void sortDeltaPOC();

    void setInterRPSPrediction(bool f)      { inter_ref_pic_set_prediction_flag = f; }
    Ipp32s getNumberOfPictures() const    { return num_pics; }
    Ipp32s getNumberOfNegativePictures() const    { return num_negative_pics; }
    Ipp32s getNumberOfPositivePictures() const    { return num_positive_pics; }
    Ipp32s getNumberOfLongtermPictures() const    { return num_lt_pics; }
    void setNumberOfLongtermPictures(Ipp32s val)  { num_lt_pics = val; }
    int getDeltaPOC(int index) const        { return m_DeltaPOC[index]; }
    void setDeltaPOC(int index, int val)    { m_DeltaPOC[index] = val; }
    Ipp8u getUsed(int index) const           { return used_by_curr_pic_flag[index]; }

    void setPOC(int bufferNum, int POC)     { m_POC[bufferNum] = POC; }
    int getPOC(int index)                   { return m_POC[index]; }

    Ipp8u getCheckLTMSBPresent(Ipp32s bufferNum) { return delta_poc_msb_present_flag[bufferNum]; }
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
    Ipp32u ref_pic_list_modification_flag_l0;
    Ipp32u ref_pic_list_modification_flag_l1;

    Ipp32u list_entry_l0[MAX_NUM_REF_PICS + 1];
    Ipp32u list_entry_l1[MAX_NUM_REF_PICS + 1];
};

// Sequence parameter set structure, corresponding to the H.264 bitstream definition.
struct H265SeqParamSetBase
{
    // bitstream params
    Ipp32s  sps_video_parameter_set_id;
    Ipp32u  sps_max_sub_layers;
    Ipp8u   sps_temporal_id_nesting_flag;

    H265ProfileTierLevel     m_pcPTL;

    Ipp8u   sps_seq_parameter_set_id;
    Ipp8u   chroma_format_idc;

    Ipp8u   separate_colour_plane_flag;

    Ipp32u  pic_width_in_luma_samples;
    Ipp32u  pic_height_in_luma_samples;

    // cropping params
    Ipp8u   conformance_window_flag;
    Ipp32u  conf_win_left_offset;
    Ipp32u  conf_win_right_offset;
    Ipp32u  conf_win_top_offset;
    Ipp32u  conf_win_bottom_offset;

    Ipp32u  bit_depth_luma;
    Ipp32u  bit_depth_chroma;

    Ipp32u  log2_max_pic_order_cnt_lsb;
    Ipp8u   sps_sub_layer_ordering_info_present_flag;

    Ipp32u  sps_max_dec_pic_buffering[MAX_TEMPORAL_LAYER];
    Ipp32u  sps_max_num_reorder_pics[MAX_TEMPORAL_LAYER];
    Ipp32u  sps_max_latency_increase[MAX_TEMPORAL_LAYER];

    Ipp32u  log2_min_luma_coding_block_size;
    Ipp32u  log2_max_luma_coding_block_size;
    Ipp32u  log2_min_transform_block_size;
    Ipp32u  log2_max_transform_block_size;
    Ipp32u  max_transform_hierarchy_depth_inter;
    Ipp32u  max_transform_hierarchy_depth_intra;

    Ipp8u   scaling_list_enabled_flag;
    Ipp8u   sps_scaling_list_data_present_flag;

    Ipp8u   amp_enabled_flag;
    Ipp8u   sample_adaptive_offset_enabled_flag;

    Ipp8u   pcm_enabled_flag;

    // pcm params
    Ipp32u  pcm_sample_bit_depth_luma;
    Ipp32u  pcm_sample_bit_depth_chroma;
    Ipp32u  log2_min_pcm_luma_coding_block_size;
    Ipp32u  log2_max_pcm_luma_coding_block_size;
    Ipp8u   pcm_loop_filter_disabled_flag;

    Ipp32u  num_short_term_ref_pic_sets;
    ReferencePictureSetList m_RPSList;

    Ipp8u   long_term_ref_pics_present_flag;
    Ipp32u  num_long_term_ref_pics_sps;
    Ipp32u  lt_ref_pic_poc_lsb_sps[33];
    Ipp8u   used_by_curr_pic_lt_sps_flag[33];

    Ipp8u   sps_temporal_mvp_enabled_flag;
    Ipp8u   sps_strong_intra_smoothing_enabled_flag;

    // vui part
    Ipp8u   vui_parameters_present_flag;         // Zero indicates default VUI parameters

    bool    aspect_ratio_info_present_flag;
    Ipp32u  aspect_ratio_idc;
    Ipp32u  sar_width;
    Ipp32u  sar_height;

    bool    overscan_info_present_flag;
    bool    overscan_appropriate_flag;

    bool    video_signal_type_present_flag;
    Ipp32u  video_format;
    bool    video_full_range_flag;
    bool    colour_description_present_flag;
    Ipp32u  colour_primaries;
    Ipp32u  transfer_characteristics;
    Ipp32u  matrix_coeffs;

    bool    chroma_loc_info_present_flag;
    Ipp32u  chroma_sample_loc_type_top_field;
    Ipp32u  chroma_sample_loc_type_bottom_field;

    bool    neutral_chroma_indication_flag;
    bool    field_seq_flag;
    bool    frame_field_info_present_flag;

    bool    default_display_window_flag;
    Ipp32u  def_disp_win_left_offset;
    Ipp32u  def_disp_win_right_offset;
    Ipp32u  def_disp_win_top_offset;
    Ipp32u  def_disp_win_bottom_offset;

    bool            vui_timing_info_present_flag;
    H265TimingInfo  m_timingInfo;
    
    bool            vui_hrd_parameters_present_flag;
    H265HRD         m_hrdParameters;

    bool    bitstream_restriction_flag;
    bool    tiles_fixed_structure_flag;
    bool    motion_vectors_over_pic_boundaries_flag;
    bool    restricted_ref_pic_lists_flag;
    Ipp32u  min_spatial_segmentation_idc;
    Ipp32u  max_bytes_per_pic_denom;
    Ipp32u  max_bits_per_min_cu_denom;
    Ipp32u  log2_max_mv_length_horizontal;
    Ipp32u  log2_max_mv_length_vertical;



    ///////////////////////////////////////////////////////
    // calculated params
    // These fields are calculated from values above.  They are not written to the bitstream
    ///////////////////////////////////////////////////////

    Ipp32u MaxCUSize;
    Ipp32u MaxCUDepth;
    Ipp32s AddCUDepth;
    Ipp32u WidthInCU;
    Ipp32u HeightInCU;
    Ipp32u NumPartitionsInCU, NumPartitionsInCUSize, NumPartitionsInFrameWidth, NumPartitionsInFrameHeight;
    Ipp32u m_maxTrSize;

    Ipp32s m_AMPAcc[MAX_CU_DEPTH]; //AMP Accuracy

    int m_QPBDOffsetY;
    int m_QPBDOffsetC;
    
   
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
        return sps_seq_parameter_set_id;
    }

    virtual void Reset()
    {
        H265SeqParamSetBase::Reset();

        m_RPSList.m_NumberOfReferencePictureSets = 0;

        sps_video_parameter_set_id = MAX_NUM_VPS_PARAM_SETS_H265;
        sps_seq_parameter_set_id = MAX_NUM_SEQ_PARAM_SETS_H265;

        // set some parameters by default
        video_format = 5; // unspecified
        video_full_range_flag = 0;
        colour_primaries = 2; // unspecified
        transfer_characteristics = 2; // unspecified
        matrix_coeffs = 2; // unspecified

        conformance_window_flag = 0;
        conf_win_left_offset = 0;
        conf_win_right_offset = 0;
        conf_win_top_offset = 0;
        conf_win_bottom_offset = 0;
    }

    static int getWinUnitX (int /*chromaFormatIdc*/) { /*assert (chromaFormatIdc > 0 && chromaFormatIdc <= MAX_CHROMA_FORMAT_IDC);*/ return 1/*m_cropUnitX[chromaFormatIdc]*/; } // TODO
    static int getWinUnitY (int /*chromaFormatIdc*/) { /*assert (chromaFormatIdc > 0 && chromaFormatIdc <= MAX_CHROMA_FORMAT_IDC);*/ return 1/*m_cropUnitY[chromaFormatIdc]*/; }

    int getQpBDOffsetY() const                  { return m_QPBDOffsetY; }
    void setQpBDOffsetY(int val)                { m_QPBDOffsetY = val; }
    int getQpBDOffsetC() const                  { return m_QPBDOffsetC; }
    void setQpBDOffsetC(int val)                { m_QPBDOffsetC = val; }

    void createRPSList(Ipp32s numRPS)
    {
        m_RPSList.allocate(numRPS);
    }

    H265ScalingList* getScalingList()           { return &m_scalingList; }
    H265ScalingList* getScalingList() const     { return const_cast<H265ScalingList *>(&m_scalingList); }
    ReferencePictureSetList *getRPSList()       { return &m_RPSList; }
    const ReferencePictureSetList *getRPSList() const       { return &m_RPSList; }

    H265ProfileTierLevel* getPTL()     { return &m_pcPTL; }

    H265HRD* getHrdParameters                 ()             { return &m_hrdParameters; }

    const H265TimingInfo* getTimingInfo() const { return &m_timingInfo; }
    H265TimingInfo* getTimingInfo() { return &m_timingInfo; }
};    // H265SeqParamSet

struct TileInfo
{
    Ipp32s firstCUAddr;
    Ipp32s endCUAddr;
};

// Picture parameter set structure, corresponding to the H.264 bitstream definition.
struct H265PicParamSetBase
{
    Ipp32u  pps_pic_parameter_set_id;
    Ipp32u  pps_seq_parameter_set_id;

    bool    dependent_slice_segments_enabled_flag;
    bool    output_flag_present_flag;
    Ipp32u  num_extra_slice_header_bits;
    bool    sign_data_hiding_enabled_flag;
    bool    cabac_init_present_flag;

    Ipp32u  num_ref_idx_l0_default_active;
    Ipp32u  num_ref_idx_l1_default_active;

    Ipp8s   init_qp;                     // default QP for I,P,B slices
    bool    constrained_intra_pred_flag;
    bool    transform_skip_enabled_flag;
    bool    cu_qp_delta_enabled_flag;
    Ipp32u  diff_cu_qp_delta_depth;
    Ipp32s  pps_cb_qp_offset;
    Ipp32s  pps_cr_qp_offset;

    bool    pps_slice_chroma_qp_offsets_present_flag;
    bool    weighted_pred_flag;              // Nonzero indicates weighted prediction applied to P and SP slices
    bool    weighted_bipred_flag;            // 0: no weighted prediction in B slices,  1: explicit weighted prediction
    bool    transquant_bypass_enabled_flag;
    bool    tiles_enabled_flag;
    bool    entropy_coding_sync_enabled_flag;  // presence of wavefronts flag

    // tiles info
    Ipp32u  num_tile_columns;
    Ipp32u  num_tile_rows;
    Ipp32u  uniform_spacing_flag;
    Ipp32u* column_width;
    Ipp32u* row_height;
    bool    loop_filter_across_tiles_enabled_flag;

    bool    pps_loop_filter_across_slices_enabled_flag;

    bool    deblocking_filter_control_present_flag;
    bool    deblocking_filter_override_enabled_flag;
    bool    pps_deblocking_filter_disabled_flag;
    Ipp32u  pps_beta_offset;
    Ipp32u  pps_tc_offset;

    bool    pps_scaling_list_data_present_flag;
    H265ScalingList m_scalingList;

    bool    lists_modification_present_flag;
    Ipp32u  log2_parallel_merge_level;
    bool    slice_segment_header_extension_present_flag;


    ///////////////////////////////////////////////////////
    // calculated params
    // These fields are calculated from values above.  They are not written to the bitstream
    ///////////////////////////////////////////////////////
    Ipp32u getColumnWidth(Ipp32u columnIdx) { return *( column_width + columnIdx ); }
    void setColumnWidth(Ipp32u* columnWidth);
    Ipp32u getRowHeight(Ipp32u rowIdx)    { return *( row_height + rowIdx ); }
    void setRowHeight(Ipp32u* rowHeight);

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
        column_width = 0;
        row_height = 0;
        m_CtbAddrRStoTS = 0;
        m_CtbAddrTStoRS = 0;
        m_TileIdx = 0;

        Reset();
    }

    void Reset()
    {
        delete[] column_width;
        column_width = 0;

        delete[] row_height;
        row_height = 0;

        delete[] m_CtbAddrRStoTS;
        m_CtbAddrRStoTS = 0;

        delete[] m_CtbAddrTStoRS;
        m_CtbAddrTStoRS = 0;

        delete[] m_TileIdx;
        m_TileIdx = 0;

        H265PicParamSetBase::Reset();

        pps_pic_parameter_set_id = MAX_NUM_PIC_PARAM_SETS_H265;
        pps_seq_parameter_set_id = MAX_NUM_SEQ_PARAM_SETS_H265;

        loop_filter_across_tiles_enabled_flag = true;
        pps_loop_filter_across_slices_enabled_flag = true;
    }

    ~H265PicParamSet()
    {
    }

    Ipp32s GetID() const
    {
        return pps_pic_parameter_set_id;
    }

    Ipp32u getNumTiles() const { return num_tile_rows*num_tile_columns; }
    H265ScalingList* getScalingList()               { return &m_scalingList; }
    H265ScalingList* getScalingList() const         { return const_cast<H265ScalingList *>(&m_scalingList); }
};    // H265PicParamSet

typedef struct {
  // Explicit weighted prediction parameters parsed in slice header,
  // or Implicit weighted prediction parameters (8 bits depth values).
  bool        present_flag;
  Ipp32u      log2_weight_denom;
  Ipp32s      weight;
  Ipp32s      offset;
  Ipp32s      delta_weight; // for HW decoder
} wpScalingParam;

// Slice header structure, corresponding to the H.264 bitstream definition.
struct H265SliceHeader
{
    // from nal unit header
    NalUnitType nal_unit_type;
    Ipp32u      nuh_temporal_id;

    // slice spec members
    Ipp32s      first_slice_segment_in_pic_flag;
    Ipp8u       no_output_of_prior_pics_flag;       // nonzero: remove previously decoded pictures from decoded picture buffer
    Ipp16u      slice_pic_parameter_set_id;
    Ipp8u       dependent_slice_segment_flag;

    Ipp32u      slice_segment_address;
    SliceType   slice_type;
    Ipp8u       pic_output_flag;

    Ipp32u      colour_plane_id; // if separate_colour_plane_flag = = 1 only

    Ipp32s      slice_pic_order_cnt_lsb;                    // picture order count (mod MaxPicOrderCntLsb)
    Ipp8u       short_term_ref_pic_set_sps_flag;

    Ipp8u       slice_enable_temporal_mvp_flag;

    Ipp8u       slice_sao_luma_flag;
    Ipp8u       slice_sao_chroma_flag;

    Ipp8u       num_ref_idx_active_override_flag;
    Ipp32s      num_ref_idx_l0_active;
    Ipp32s      num_ref_idx_l1_active;

    Ipp8u       mvd_l1_zero_flag;
    Ipp8u       cabac_init_flag;
    Ipp32u      collocated_from_l0_flag;
    Ipp32u      collocated_ref_idx;

    // pred_weight params
    Ipp32u      luma_log2_weight_denom;
    Ipp32u      chroma_log2_weight_denom;
    wpScalingParam  pred_weight_table[2][MAX_NUM_REF_PICS][3]; // [REF_PIC_LIST_0 or REF_PIC_LIST_1][refIdx][0:Y, 1:U, 2:V]

    Ipp32s      max_num_merge_cand;

    Ipp32s      slice_qp_delta;                       // to calculate default slice QP
    Ipp32s      slice_cb_qp_offset;
    Ipp32s      slice_cr_qp_offset;

    Ipp8u       deblocking_filter_override_flag;
    Ipp8u       slice_deblocking_filter_disabled_flag;
    Ipp32s      slice_beta_offset;
    Ipp32s      slice_tc_offset;
    Ipp8u       slice_loop_filter_across_slices_enabled_flag;

    Ipp32u      num_entry_point_offsets;

    ///////////////////////////////////////////////////////
    // calculated params
    // These fields are calculated from values above.  They are not written to the bitstream
    ///////////////////////////////////////////////////////

    Ipp32u m_HeaderBitstreamOffset;
    //h265 elements of slice header ----------------------------------------------------------------------------------------------------------------
    const H265SeqParamSet* m_SeqParamSet;
    const H265PicParamSet* m_PicParamSet;
    Ipp32u *m_TileByteLocation;
    Ipp32s m_TileCount;

    Ipp32u* m_SubstreamSizes;

    Ipp32u m_sliceSegmentCurStartCUAddr;
    Ipp32u m_sliceSegmentCurEndCUAddr;
    
    Ipp32s SliceQP;

    Ipp32s SliceCurStartCUAddr;
    
    bool m_CheckLDC;
    Ipp32s m_numRefIdx[3]; //  for multiple reference of current slice. IT SEEMS BE SAME AS num_ref_idx_l0_active, l1, lc

    Ipp32s RefPOCList[2][MAX_NUM_REF_PICS + 1];

    ReferencePictureSet *m_pRPS;
    ReferencePictureSet m_localRPS;
    RefPicListModification m_RefPicListModification;

    // flag equal 1 means that the slice belong to IDR or anchor access unit
    Ipp32u IdrPicFlag;

    Ipp32s wNumBitsForShortTermRPSInSlice;  // used in h/w decoder
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

class PocDecoding
{
public:
    Ipp32s prevPocPicOrderCntLsb;
    Ipp32s prevPicOrderCntMsb;

    PocDecoding()
        : prevPocPicOrderCntLsb(0)
        , prevPicOrderCntMsb(0)
    {
    }

    void Reset()
    {
        prevPocPicOrderCntLsb = 0;
        prevPicOrderCntMsb = 0;
    }
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

static void H265_FORCEINLINE  small_memcpy( void* dst, const void* src, int len )
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
    MFX_INTERNAL_CPY(dst, src, len);
#endif
}

} // end namespace UMC_HEVC_DECODER

#include "umc_h265_dec_ippwrap.h"



#endif // H265_GLOBAL_ROM_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
