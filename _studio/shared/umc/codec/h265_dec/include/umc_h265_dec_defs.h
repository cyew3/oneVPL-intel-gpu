//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_DEC_DEFS_DEC_H__
#define __UMC_H265_DEC_DEFS_DEC_H__

#include <vector>
#include "umc_structures.h"

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

enum
{
    H265_PROFILE_MAIN = 1,
    H265_PROFILE_MAIN10 = 2,
    H265_PROFILE_MAINSP = 3,
    H265_PROFILE_FREXT = 4,
};

// HEVC level identifiers
enum
{
    H265_LEVEL_1    = 10,
    H265_LEVEL_2    = 20,
    H265_LEVEL_21   = 21,

    H265_LEVEL_3    = 30,
    H265_LEVEL_31   = 31,

    H265_LEVEL_4    = 40,
    H265_LEVEL_41   = 41,

    H265_LEVEL_5    = 50,
    H265_LEVEL_51   = 51,
    H265_LEVEL_52   = 52,

    H265_LEVEL_6    = 60,
    H265_LEVEL_61   = 61,
    H265_LEVEL_62   = 62,
    H265_LEVEL_MAX  = 62
};

// Hack to allow somewhat unconformant streams. Should be used only when decoding random ENC streams
#define INSTRUMENTED_CABAC 0

enum {
    AMVP_MAX_NUM_CAND       = 2,           // max number of final candidates
    MERGE_MAX_NUM_CAND      = 5,
    MAX_CHROMA_OFFSET_ELEMENTS = 7
};

#define REG_DCT 65535

#define CU_DQP_TU_CMAX 5 //max number bins for truncated unary
#define CU_DQP_EG_k 0 //expgolomb order

// Plane identifiers
enum ComponentPlane
{
    COMPONENT_LUMA = 0,
    COMPONENT_CHROMA_U,
    COMPONENT_CHROMA_V,
    COMPONENT_CHROMA_U1,
    COMPONENT_CHROMA_V1,
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

// Luma intra modes
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

// Display structure modes
enum DisplayPictureStruct_H265 {
    DPS_FRAME_H265     = 0,
    DPS_TOP_H265,         // one field
    DPS_BOTTOM_H265,      // one field
    DPS_TOP_BOTTOM_H265,
    DPS_BOTTOM_TOP_H265,
    DPS_TOP_BOTTOM_TOP_H265,
    DPS_BOTTOM_TOP_BOTTOM_H265,
    DPS_FRAME_DOUBLING_H265,
    DPS_FRAME_TRIPLING_H265,
    DPS_TOP_BOTTOM_PREV_H265, // 9 - Top field paired with previous bottom field in output order
    DPS_BOTTOM_TOP_PREV_H265, //10 - Bottom field paired with previous top field in output order
    DPS_TOP_BOTTOM_NEXT_H265, //11 - Top field paired with next bottom field in output order
    DPS_BOTTOM_TOP_NEXT_H265, //12 - Bottom field paired with next top field in output order
};

typedef Ipp8u PlaneY;
typedef Ipp8u PlaneUV;
typedef Ipp16s Coeffs;

typedef Coeffs *CoeffsPtr;
typedef PlaneY *PlanePtrY;
typedef PlaneUV *PlanePtrUV;

// HEVC NAL unit types
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

// Slice types
enum SliceType
{
  B_SLICE,
  P_SLICE,
  I_SLICE
};

// Reference list IDs
enum EnumRefPicList
{
    REF_PIC_LIST_0 = 0,   ///< reference list 0
    REF_PIC_LIST_1 = 1    ///< reference list 1
};

// Prediction unit IDs
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
};

// Supported prediction type
enum EnumPredMode
{
    MODE_INTER,           ///< inter-prediction mode
    MODE_INTRA,           ///< intra-prediction mode
    MODE_NONE
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

// Convert one enum to another
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

// SEI identifiers
typedef enum
{
    SEI_BUFFERING_PERIOD_TYPE                       =   0,
    SEI_PIC_TIMING_TYPE                             =   1,
    SEI_PAN_SCAN_RECT_TYPE                          =   2,
    SEI_FILLER_TYPE                                 =   3,
    SEI_USER_DATA_REGISTERED_TYPE                   =   4,
    SEI_USER_DATA_UNREGISTERED_TYPE                 =   5,
    SEI_RECOVERY_POINT_TYPE                         =   6,

    SEI_SCENE_INFO_TYPE                             =   9,

    SEI_PICTURE_SNAPSHOT                            =  15,
    SEI_PROGRESSIVE_REF_SEGMENT_START_TYPE          =  16,
    SEI_PROGRESSIVE_REF_SEGMENT_END_TYPE            =  17,
    SEI_FILM_GRAIN_CHARACTERISTICS                  =  19,

    SEI_POST_FILTER_HINT                            =  22,
    SEI_TONE_MAPPING_INFO                           =  23,

    SEI_FRAME_PACKING_ARRANGEMENT                   =  45,
    SEI_DISPLAY_ORIENTATION                         =  47,

    SEI_STRUCTURE_OF_PICTURES_INFO                  = 128,
    SEI_SEI_ACTIVE_PARAMETER_SET                    = 129,
    SEI_DECODING_UNIT_INFO                          = 130,
    SEI_TEMPORAL_SUB_LAYER_ZERO_INDEX               = 131,
    SEI_SCALABLE_NESTING                            = 133,
    SEI_REGION_REFRESH_INFO                         = 134,
    SEI_NO_DISPLAY                                  = 135,
    SEI_TIME_CODE                                   = 136,
    SEI_MASTERING_DISPLAY_COLOUR_VOLUME             = 137,
    SEI_SEGMENTED_RECT_FRAME_PACKING_ARRANGEMENT    = 138,
    SEI_TEMPORAL_MOTION_CONSTRAINED_TILE_SETS       = 139,
    SEI_CHROMA_RESAMPLING_FILTER_HINT               = 140,
    SEI_KNEE_FUNCTION_INFO                          = 141,
    SEI_COLOUR_REMAPPING_INFO                       = 142,
    SEI_DEINTERLACED_FIELD_IDENTIFICATION           = 143,

    SEI_LAYERS_NOT_PRESENT                          = 160,
    SEI_INTER_LAYER_CONSTRAINED_TILE_SETS           = 161,
    SEI_BSP_NESTING                                 = 162,
    SEI_BSP_INITIAL_ARRIVAL_TIME                    = 163,
    SEI_SUB_BITSTREAM_PROPERTY                      = 164,
    SEI_ALPHA_CHANNEL_INFO                          = 165,
    SEI_OVERLAY_INFO                                = 166,
    SEI_TEMPORAL_MV_PREDICTION_CONSTRAINTS          = 167,
    SEI_FRAME_FIELD_INFO                            = 168,

    SEI_THREE_DIMENSIONAL_REFERENCE_DISPLAYS_INFO   = 176,
    SEI_DEPTH_REPRESENTATION_INFO                   = 177,
    SEI_MULTIVIEW_SCENE_INFO                        = 178,
    SEI_MULTIVIEW_ACQUISITION_INFO                  = 179,
    SEI_MULTIVIEW_VIEW_POSITION                     = 180,

    SEI_RESERVED,

    SEI_NUM_MESSAGES

} SEI_TYPE;

#define IS_SKIP_DEBLOCKING_MODE_PERMANENT (m_PermanentTurnOffDeblocking == 2)
#define IS_SKIP_DEBLOCKING_MODE_PREVENTIVE (m_PermanentTurnOffDeblocking == 3)

enum
{
    MAX_NUM_VPS_PARAM_SETS_H265 = 16,
    MAX_NUM_SEQ_PARAM_SETS_H265 = 16,
    MAX_NUM_PIC_PARAM_SETS_H265 = 64,

    MAX_NUM_REF_PICS            = 16,

    COEFFICIENTS_BUFFER_SIZE_H265    = 16 * 51,

    MINIMAL_DATA_SIZE_H265           = 4,

    DEFAULT_NU_TAIL_VALUE       = 0xff,
    DEFAULT_NU_TAIL_SIZE        = 8
};

// Memory reference counting base class
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

// Basic ref counting heap object class
class HeapObject : public RefCounter
{
public:

    virtual ~HeapObject() {}

    virtual void Reset()
    {
    }

    virtual void Free();
};

// Scaling list data structure
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
    // Copy data from predefined scaling matrixes
    void     processRefMatrix(unsigned sizeId, unsigned listId, unsigned refListId);
    int      getScalingListDC         (unsigned sizeId, unsigned listId) const     { return m_scalingListDC[sizeId][listId]; }

    // Allocate and initialize scaling list tables
    void init();
    bool is_initialized(void) { return m_initialized; }
    // Initialize scaling list with default data
    void initFromDefaultScalingList(void);
    // Calculated coefficients used for dequantization
    void calculateDequantCoef(void);

    // Deallocate scaling list tables
    void destroy();

    Ipp16s *m_dequantCoef[SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM][SCALING_LIST_REM_NUM];

private:
    H265_FORCEINLINE Ipp16s* getDequantCoeff(Ipp32u list, Ipp32u qp, Ipp32u size)
    {
        return m_dequantCoef[size][list][qp];
    }
    // Calculated coefficients used for dequantization in one scaling list matrix
    __inline void processScalingListDec(Ipp32s *coeff, Ipp16s *dequantcoeff, Ipp32s invQuantScales, Ipp32u height, Ipp32u width, Ipp32u ratio, Ipp32u sizuNum, Ipp32u dc);
    // Returns default scaling matrix for specified parameters
    static const int *getScalingListDefaultAddress(unsigned sizeId, unsigned listId);

    int      m_scalingListDC               [SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM];
    unsigned m_refMatrixId                 [SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM];
    int      m_scalingListCoef             [SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM][MAX_MATRIX_COEF_NUM];
    bool     m_initialized;
};

// One profile, tier, level data structure
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

    Ipp8u       max_12bit_constraint_flag;
    Ipp8u       max_10bit_constraint_flag;
    Ipp8u       max_8bit_constraint_flag;
    Ipp8u       max_422chroma_constraint_flag;
    Ipp8u       max_420chroma_constraint_flag;
    Ipp8u       max_monochrome_constraint_flag;
    Ipp8u       intra_constraint_flag;
    Ipp8u       one_picture_only_constraint_flag;
    Ipp8u       lower_bit_rate_constraint_flag;

    H265PTL()   { memset(this, 0, sizeof(*this)); }
};

// Stream profile, tiler, level data structure
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

// HRD information data structure
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

// HRD VUI information
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

    H265HRD()
    {
        ::memset(this, 0, sizeof(*this));
    }

    H265HrdSubLayerInfo * GetHRDSubLayerParam(Ipp32u i) { return &m_HRD[i]; }
};

// VUI timing information
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

        delete[] m_hrdParameters;
        m_hrdParameters = 0;

        delete[] hrd_layer_set_idx;
        hrd_layer_set_idx = 0;

        delete[] cprms_present_flag;
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

// RPS data structure
struct ReferencePictureSet
{
    Ipp8u inter_ref_pic_set_prediction_flag;

    Ipp32s num_negative_pics;
    Ipp32s num_positive_pics;

    Ipp32s num_pics;

    Ipp32s num_lt_pics;

    Ipp32u num_long_term_pics;
    Ipp32u num_long_term_sps;

    Ipp32s m_DeltaPOC[MAX_NUM_REF_PICS];
    Ipp32s m_POC[MAX_NUM_REF_PICS];
    Ipp8u used_by_curr_pic_flag[MAX_NUM_REF_PICS];

    Ipp8u delta_poc_msb_present_flag[MAX_NUM_REF_PICS];
    Ipp8u delta_poc_msb_cycle_lt[MAX_NUM_REF_PICS];
    Ipp32s poc_lbs_lt[MAX_NUM_REF_PICS];

    ReferencePictureSet();

    void sortDeltaPOC();

    void setInterRPSPrediction(bool f)      { inter_ref_pic_set_prediction_flag = f; }
    Ipp32u getNumberOfPictures() const    { return num_pics; }
    Ipp32u getNumberOfNegativePictures() const    { return num_negative_pics; }
    Ipp32u getNumberOfPositivePictures() const    { return num_positive_pics; }
    Ipp32u getNumberOfLongtermPictures() const    { return num_lt_pics; }
    void setNumberOfLongtermPictures(Ipp32s val)  { num_lt_pics = val; }
    int getDeltaPOC(int index) const        { return m_DeltaPOC[index]; }
    void setDeltaPOC(int index, int val)    { m_DeltaPOC[index] = val; }
    Ipp8u getUsed(int index) const           { return used_by_curr_pic_flag[index]; }

    void setPOC(int bufferNum, int POC)     { m_POC[bufferNum] = POC; }
    int getPOC(int index) const             { return m_POC[index]; }

    Ipp8u getCheckLTMSBPresent(Ipp32s bufferNum) const { return delta_poc_msb_present_flag[bufferNum]; }
};

// Reference picture list data structure
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

// Reference picture list data structure
struct RefPicListModification
{
    Ipp32u ref_pic_list_modification_flag_l0;
    Ipp32u ref_pic_list_modification_flag_l1;

    Ipp32u list_entry_l0[MAX_NUM_REF_PICS + 1];
    Ipp32u list_entry_l1[MAX_NUM_REF_PICS + 1];
};

// Sequence parameter set structure, corresponding to the HEVC bitstream definition.
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

    Ipp8u   aspect_ratio_info_present_flag;
    Ipp32u  aspect_ratio_idc;
    Ipp32u  sar_width;
    Ipp32u  sar_height;

    Ipp8u   overscan_info_present_flag;
    Ipp8u   overscan_appropriate_flag;

    Ipp8u   video_signal_type_present_flag;
    Ipp32u  video_format;
    Ipp8u   video_full_range_flag;
    Ipp8u   colour_description_present_flag;
    Ipp32u  colour_primaries;
    Ipp32u  transfer_characteristics;
    Ipp32u  matrix_coeffs;

    Ipp8u   chroma_loc_info_present_flag;
    Ipp32u  chroma_sample_loc_type_top_field;
    Ipp32u  chroma_sample_loc_type_bottom_field;

    Ipp8u   neutral_chroma_indication_flag;
    Ipp8u   field_seq_flag;
    Ipp8u   frame_field_info_present_flag;

    Ipp8u   default_display_window_flag;
    Ipp32u  def_disp_win_left_offset;
    Ipp32u  def_disp_win_right_offset;
    Ipp32u  def_disp_win_top_offset;
    Ipp32u  def_disp_win_bottom_offset;

    Ipp8u           vui_timing_info_present_flag;
    H265TimingInfo  m_timingInfo;

    Ipp8u           vui_hrd_parameters_present_flag;
    H265HRD         m_hrdParameters;

    Ipp8u   bitstream_restriction_flag;
    Ipp8u   tiles_fixed_structure_flag;
    Ipp8u   motion_vectors_over_pic_boundaries_flag;
    Ipp8u   restricted_ref_pic_lists_flag;
    Ipp32u  min_spatial_segmentation_idc;
    Ipp32u  max_bytes_per_pic_denom;
    Ipp32u  max_bits_per_min_cu_denom;
    Ipp32u  log2_max_mv_length_horizontal;
    Ipp32u  log2_max_mv_length_vertical;

    // sps extension
    Ipp8u sps_range_extension_flag;

    Ipp8u transform_skip_rotation_enabled_flag;
    Ipp8u transform_skip_context_enabled_flag;
    Ipp8u implicit_residual_dpcm_enabled_flag;
    Ipp8u explicit_residual_dpcm_enabled_flag;
    Ipp8u extended_precision_processing_flag;
    Ipp8u intra_smoothing_disabled_flag;
    Ipp8u high_precision_offsets_enabled_flag;
    Ipp8u fast_rice_adaptation_enabled_flag;
    Ipp8u cabac_bypass_alignment_enabled_flag;

    ///////////////////////////////////////////////////////
    // calculated params
    // These fields are calculated from values above.  They are not written to the bitstream
    ///////////////////////////////////////////////////////

    Ipp32u MaxCUSize;
    Ipp32u MaxCUDepth;
    Ipp32u MinCUSize;
    Ipp32s AddCUDepth;
    Ipp32u WidthInCU;
    Ipp32u HeightInCU;
    Ipp32u NumPartitionsInCU, NumPartitionsInCUSize, NumPartitionsInFrameWidth;
    Ipp32u m_maxTrSize;

    Ipp32s m_AMPAcc[MAX_CU_DEPTH]; //AMP Accuracy

    int m_QPBDOffsetY;
    int m_QPBDOffsetC;

    Ipp32u ChromaArrayType;

    Ipp32u chromaShiftW;
    Ipp32u chromaShiftH;

    Ipp32u need16bitOutput;

    void Reset()
    {
        H265SeqParamSetBase sps = {0};
        *this = sps;
    }

};    // H265SeqParamSetBase

// Sequence parameter set structure, corresponding to the HEVC bitstream definition.
struct H265SeqParamSet : public HeapObject, public H265SeqParamSetBase
{
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

        m_scalingList.destroy();
    }

    int SubWidthC() const
    {
        static Ipp32s subWidth[] = {1, 2, 2, 1};
        VM_ASSERT (chroma_format_idc >= 0 && chroma_format_idc <= 4);
        return subWidth[chroma_format_idc];
    }

    int SubHeightC() const
    {
        static Ipp32s subHeight[] = {1, 2, 1, 1};
        VM_ASSERT (chroma_format_idc >= 0 && chroma_format_idc <= 4);
        return subHeight[chroma_format_idc];
    }

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

    H265ProfileTierLevel* getPTL() { return &m_pcPTL; }
    const H265ProfileTierLevel* getPTL() const    { return &m_pcPTL; }

    H265HRD* getHrdParameters                 ()             { return &m_hrdParameters; }

    const H265TimingInfo* getTimingInfo() const { return &m_timingInfo; }
    H265TimingInfo* getTimingInfo() { return &m_timingInfo; }
};    // H265SeqParamSet

// Tiles description
struct TileInfo
{
    Ipp32s firstCUAddr;
    Ipp32s endCUAddr;
    Ipp32s width;
};

// Picture parameter set structure, corresponding to the HEVC bitstream definition.
struct H265PicParamSetBase
{
    Ipp32u  pps_pic_parameter_set_id;
    Ipp32u  pps_seq_parameter_set_id;

    Ipp8u   dependent_slice_segments_enabled_flag;
    Ipp8u   output_flag_present_flag;
    Ipp32u  num_extra_slice_header_bits;
    Ipp8u   sign_data_hiding_enabled_flag;
    Ipp8u   cabac_init_present_flag;

    Ipp32u  num_ref_idx_l0_default_active;
    Ipp32u  num_ref_idx_l1_default_active;

    Ipp8s   init_qp;                     // default QP for I,P,B slices
    Ipp8u   constrained_intra_pred_flag;
    Ipp8u   transform_skip_enabled_flag;
    Ipp8u   cu_qp_delta_enabled_flag;
    Ipp32u  diff_cu_qp_delta_depth;
    Ipp32s  pps_cb_qp_offset;
    Ipp32s  pps_cr_qp_offset;

    Ipp8u   pps_slice_chroma_qp_offsets_present_flag;
    Ipp8u   weighted_pred_flag;              // Nonzero indicates weighted prediction applied to P and SP slices
    Ipp8u   weighted_bipred_flag;            // 0: no weighted prediction in B slices,  1: explicit weighted prediction
    Ipp8u   transquant_bypass_enabled_flag;
    Ipp8u   tiles_enabled_flag;
    Ipp8u   entropy_coding_sync_enabled_flag;  // presence of wavefronts flag

    // tiles info
    Ipp32u  num_tile_columns;
    Ipp32u  num_tile_rows;
    Ipp32u  uniform_spacing_flag;
    Ipp32u* column_width;
    Ipp32u* row_height;
    Ipp8u   loop_filter_across_tiles_enabled_flag;

    Ipp8u   pps_loop_filter_across_slices_enabled_flag;

    Ipp8u   deblocking_filter_control_present_flag;
    Ipp8u   deblocking_filter_override_enabled_flag;
    Ipp8u   pps_deblocking_filter_disabled_flag;
    Ipp32s  pps_beta_offset;
    Ipp32s  pps_tc_offset;

    Ipp8u   pps_scaling_list_data_present_flag;

    Ipp8u   lists_modification_present_flag;
    Ipp32u  log2_parallel_merge_level;
    Ipp8u   slice_segment_header_extension_present_flag;

    // pps range extension
    Ipp8u  pps_range_extensions_flag;
    Ipp32u log2_max_transform_skip_block_size;

    Ipp8u cross_component_prediction_enabled_flag;
    Ipp8u chroma_qp_offset_list_enabled_flag;
    Ipp32u diff_cu_chroma_qp_offset_depth;
    Ipp32u chroma_qp_offset_list_len;
    Ipp32s cb_qp_offset_list[MAX_CHROMA_OFFSET_ELEMENTS];
    Ipp32s cr_qp_offset_list[MAX_CHROMA_OFFSET_ELEMENTS];

    Ipp32u log2_sao_offset_scale_luma;
    Ipp32u log2_sao_offset_scale_chroma;

    ///////////////////////////////////////////////////////
    // calculated params
    // These fields are calculated from values above.  They are not written to the bitstream
    ///////////////////////////////////////////////////////
    Ipp32u getColumnWidth(Ipp32u columnIdx) { return *( column_width + columnIdx ); }
    Ipp32u getRowHeight(Ipp32u rowIdx)    { return *( row_height + rowIdx ); }

    Ipp32u* m_CtbAddrRStoTS;
    Ipp32u* m_CtbAddrTStoRS;
    Ipp32u* m_TileIdx;

    void Reset()
    {
        H265PicParamSetBase pps = {0};
        *this = pps;
    }
};  // H265PicParamSetBase

// Picture parameter set structure, corresponding to the H.264 bitstream definition.
struct H265PicParamSet : public HeapObject, public H265PicParamSetBase
{
    H265ScalingList m_scalingList;
    std::vector<TileInfo> tilesInfo;

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

        tilesInfo.clear();
        m_scalingList.destroy();

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

// Explicit weighted prediction parameters parsed in slice header,
// or Implicit weighted prediction parameters (8 bits depth values).
typedef struct {
  Ipp8u       present_flag;
  Ipp32u      log2_weight_denom;
  Ipp32s      weight;
  Ipp32s      offset;
  Ipp32s      delta_weight; // for HW decoder
} wpScalingParam;

// Slice header structure, corresponding to the HEVC bitstream definition.
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

    Ipp8u       slice_temporal_mvp_enabled_flag;

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

    Ipp8u       cu_chroma_qp_offset_enabled_flag;

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

    Ipp32u m_sliceSegmentCurStartCUAddr;
    Ipp32u m_sliceSegmentCurEndCUAddr;

    Ipp32s SliceQP;

    Ipp32s SliceCurStartCUAddr;

    bool m_CheckLDC;
    Ipp32s m_numRefIdx[3]; //  for multiple reference of current slice. IT SEEMS BE SAME AS num_ref_idx_l0_active, l1, lc

    ReferencePictureSet m_rps;
    RefPicListModification m_RefPicListModification;

    // flag equal 1 means that the slice belong to IDR or anchor access unit
    Ipp32u IdrPicFlag;

    Ipp32s wNumBitsForShortTermRPSInSlice;  // used in h/w decoder
}; // H265SliceHeader

// SEI data storage
struct H265SEIPayLoadBase
{
    SEI_TYPE payLoadType;
    Ipp32u   payLoadSize;

    union SEIMessages
    {
        struct PicTiming
        {
            DisplayPictureStruct_H265 pic_struct;
            Ipp32u pic_dpb_output_delay;
        } pic_timing;

        struct RecoveryPoint
        {
            Ipp32s recovery_poc_cnt;
            Ipp8u exact_match_flag;
            Ipp8u broken_link_flag;
        }recovery_point;

    }SEI_messages;

    void Reset()
    {
        memset(this, 0, sizeof(H265SEIPayLoadBase));

        payLoadType = SEI_RESERVED;
        payLoadSize = 0;
    }
};

// SEI heap object
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

// POC container
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

// Error exception
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

// Allocate an array or throw exception
template <typename T>
inline T * h265_new_array_throw(Ipp32s size)
{
    T * t = new T[size];
    if (!t)
        throw h265_exception(UMC::UMC_ERR_ALLOC);
    return t;
}

enum
{
    CHROMA_FORMAT_400       = 0,
    CHROMA_FORMAT_420       = 1,
    CHROMA_FORMAT_422       = 2,
    CHROMA_FORMAT_444       = 3
};

// Color format constants conversion
inline UMC::ColorFormat GetUMCColorFormat_H265(Ipp32s color_format)
{
    UMC::ColorFormat format;
    switch(color_format)
    {
    case CHROMA_FORMAT_400:
        format = UMC::GRAY;
        break;
    case CHROMA_FORMAT_422:
        format = UMC::YUV422;
        break;
    case CHROMA_FORMAT_444:
        format = UMC::YUV444;
        break;
    case CHROMA_FORMAT_420:
    default:
        format = UMC::YUV420;
        break;
    }

    return format;
}

// Color format constants conversion
inline Ipp32s GetH265ColorFormat(UMC::ColorFormat color_format)
{
    Ipp32s format;
    switch(color_format)
    {
    case UMC::GRAY:
    case UMC::GRAYA:
        format = CHROMA_FORMAT_400;
        break;
    case UMC::YUV422A:
    case UMC::YUV422:
        format = CHROMA_FORMAT_422;
        break;
    case UMC::YUV444:
    case UMC::YUV444A:
        format = CHROMA_FORMAT_444;
        break;
    case UMC::YUV420:
    case UMC::YUV420A:
    case UMC::NV12:
    case UMC::YV12:
    default:
        format = CHROMA_FORMAT_420;
        break;
    }

    return format;
}

// Calculate maximum allowed bitstream NAL unit size based on picture resolution
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

    return 2*size;
}

// ML: OPT: significant overhead if not inlined (ICC does not honor implied 'inline' with -Qipo)
// ML: OPT: TODO: Make sure compiler recognizes saturation idiom for vectorization when used
#define Clip3( m_Min, m_Max, m_Value) ( (m_Value) < (m_Min) ? \
                                                  (m_Min) : \
                                                ( (m_Value) > (m_Max) ? \
                                                              (m_Max) : \
                                                              (m_Value) ) )

} // end namespace UMC_HEVC_DECODER

#endif // H265_GLOBAL_ROM_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
