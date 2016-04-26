/* ****************************************************************************** *\

 INTEL CORPORATION PROPRIETARY INFORMATION
 This software is supplied under the terms of a license agreement or nondisclosure
 agreement with Intel Corporation and may not be copied or disclosed except in
 accordance with the terms of that agreement
 Copyright(c) 2012-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifndef __VAAPI_EXT_INTERFACE_H__
#define __VAAPI_EXT_INTERFACE_H__

#define VAAPI_DRIVER_VPG

#define VATriggerCodecHangBufferType ((VABufferType)-16)
// Misc parameter for encoder
#define  VAEncMiscParameterTypePrivate     -2
// encryption parameters for PAVP
#define  VAEncryptionParameterBufferType   -3

#define EXTERNAL_ENCRYPTED_SURFACE_FLAG   (1<<16)
#define VA_CODED_BUF_STATUS_PRIVATE_DATA_HDCP (1<<8)
#define VA_HDCP_ENABLED (1<<12)
#define VA_CODED_BUF_STATUS_BAD_BITSTREAM 0x8000
#define VA_CODED_BUF_STATUS_HW_TEAR_DOWN 0x4000

// Private per frame encoder controls, once set they will persist for all future frames
// until it is updated again.
typedef struct _VAEncMiscParameterPrivate
{
    unsigned int target_usage; // Valid values 1-7 for AVC & MPEG2.
    union
    {   
        struct
        {
            // Use raw frames for reference instead of reconstructed frames.
            unsigned int useRawPicForRef                    : 1;
            // Disables skip check for ENC. 
            unsigned int skipCheckDisable                   : 1;
            // Indicates app will override default driver FTQ settings using FTQEnable.
            unsigned int FTQOverride                        : 1;
            // Enables/disables FTQ.
            unsigned int FTQEnable                          : 1;    
            // Indicates the app will provide the Skip Threshold LUT to use when FTQ is 
            // enabled (FTQSkipThresholdLUT), else default driver thresholds will be used.
            unsigned int FTQSkipThresholdLUTInput           : 1; 
            // Indicates the app will provide the Skip Threshold LUT to use when FTQ is 
            // disabled (NonFTQSkipThresholdLUT), else default driver thresholds will be used.
            unsigned int NonFTQSkipThresholdLUTInput        : 1; 
            // Indicates the app will provide the lambda LUT in lambdaValueLUT[], else default
            // driver lambda values will be used.
            unsigned int lambdaValueLUTInput                : 1;
            // Control to enable the ENC mode decision algorithm to bias to fewer B Direct/Skip types.
            // Applies only to B frames, all other frames will ignore this setting.
            unsigned int directBiasAdjustmentEnable         : 1;
            // Enables global motion bias.
            unsigned int globalMotionBiasAdjustmentEnable   : 1;
            // MV cost scaling ratio for HME predictors.  It is used when 
            // globalMotionBiasAdjustmentEnable == 1, else it is ignored.  Values are:
            //      0: set MV cost to be 0 for HME predictor.
            //      1: scale MV cost to be 1/2 of the default value for HME predictor.
            //      2: scale MV cost to be 1/4 of the default value for HME predictor.
            //      3: scale MV cost to be 1/8 of the default value for HME predictor.
            unsigned int HMEMVCostScalingFactor             : 2;
            //disable HME
            unsigned int HMEDisable                         : 1;
            //disable Super HME
            unsigned int SuperHMEDisable                    : 1;
            //disable Ultra HME
            unsigned int UltraHMEDisable                    : 1;

        };
        unsigned int encControls;
    };

    // Maps QP to skip thresholds when FTQ is enabled.  Valid range is 0-255.
    unsigned char FTQSkipThresholdLUT[52];
    // Maps QP to skip thresholds when FTQ is disabled.  Valid range is 0-65535.
    unsigned short NonFTQSkipThresholdLUT[52];
    // Maps QP to lambda values.  Valid range is 0-255.
    unsigned char lambdaValueLUT[52];

    unsigned int reserved[8];  // Reserved for future use.

} VAEncMiscParameterPrivate;

// Force MB's to be non skip for both ENC and PAK.  The width of the MB map
// Surface is (width of the Picture in MB unit) * 1 byte, multiple of 64 bytes. 
/// The height is (height of the picture in MB unit). The picture is either 
// frame or non-interleaved top or bottom field.  If the application provides this 
// surface, it will override the "skipCheckDisable" setting in VAEncMiscParameterPrivate.
#define VAEncMacroblockDisableSkipMapBufferType     -7


/** \brief Sequence display extension data structure used for */
/*         VAEncMiscParameterTypeExtensionData buffer type. */
/*         the element definition in this structure has 1 : 1 correspondence */
/*         with the same element defined in sequence_display_extension() */
/*         from mpeg2 spec */
typedef struct _VAEncMiscParameterExtensionDataSeqDisplayMPEG2
{
    unsigned char extension_start_code_identifier;
    unsigned char video_format;
    unsigned char colour_description;
    unsigned char colour_primaries;
    unsigned char transfer_characteristics;
    unsigned char matrix_coefficients;
    unsigned short display_horizontal_size;
    unsigned short display_vertical_size;
} VAEncMiscParameterExtensionDataSeqDisplayMPEG2;

#define VAEncMiscParameterTypeExtensionData        -8
/*VAEncrytpionParameterBuffer*/
typedef struct _VAEncryptionParameterBuffer
{
    //Not used currently
    unsigned int encryptionSupport;
    //Not used currently
    unsigned int hostEncryptMode;
    // For IV, Counter input
    unsigned int pavpAesCounter[2][4];
    // not used currently
    unsigned int pavpIndex;
    // PAVP mode, CTR, CBC, DEDE etc
    unsigned int pavpCounterMode;
    unsigned int pavpEncryptionType;
    // not used currently
    unsigned int pavpInputSize[2];
    // not used currently
    unsigned int pavpBufferSize[2];
    // not used currently
    VABufferID   pvap_buf;
    // set to TRUE if protected media
    unsigned int pavpHasBeenEnabled;
    // not used currently
    unsigned int IntermmediatedBufReq;
    // not used currently
    unsigned int uiCounterIncrement;
    // AppId: PAVP sessin Index from application
    unsigned int app_id;

} VAEncryptionParameterBuffer;

typedef struct _VAHDCPEncryptionParameterBuffer {
    unsigned int       status;
    int                bEncrypted;
    unsigned int       counter[4];
} VAHDCPEncryptionParameterBuffer;

// structure for encoder capability
typedef struct _VAEncQueryCapabilities
{
    /* these 2 fields should be preserved over all versions of this structure */
    unsigned int version; /* [in] unique version of the sturcture interface  */
    unsigned int size;    /* [in] size of the structure*/
    union
    {
        struct
        {
            unsigned int SliceStructure    :3; /* 1 - SliceDividerSnb; 2 - SliceDividerHsw; 3 - SliceDividerBluRay; the other - SliceDividerOneSlice  */
            unsigned int NoInterlacedField :1;
        } bits;
        unsigned int CodingLimits;
    } EncLimits;
    unsigned int MaxPicWidth;
    unsigned int MaxPicHeight;
    unsigned int MaxNum_ReferenceL0;
    unsigned int MaxNum_ReferenceL1;
    unsigned int MBBRCSupport;
} VAEncQueryCapabilities;

// VAAPI Extension to support querying encoder capability
typedef VAStatus (*vaExtQueryEncCapabilities)(
    VADisplay                   dpy,
    VAProfile                   profile,
    VAEncQueryCapabilities      *pVaEncCaps);

#define VPG_EXT_QUERY_ENC_CAPS  "vpgExtQueryEncCapabilities"


/* below is private VAAPI for VP8 Hybrid encoder */

// entry point for ENC + hybrid PAK
#define VAEntrypointHybridEncSlice          -1

// structure discribing MB data layout returned from driver
typedef struct _VAEncMbDataLayout
{
    unsigned char   MB_CODE_size;
    unsigned int    MB_CODE_offset;
    unsigned int    MB_CODE_stride;
    unsigned char   MV_number;
    unsigned int    MV_offset;
    unsigned int    MV_stride;
} VAEncMbDataLayout;

// this buffer representing VAEncMbDataLayout for function vpgQueryBufferAttributes()
#define VAEncMbDataBufferType               -4

#define FUNC_QUERY_BUFFER_ATTRIBUTES "hybridQueryBufferAttributes"
//#define FUNC_QUERY_BUFFER_ATTRIBUTES "media_QueryBufferAttributes"
//#define FUNC_QUERY_BUFFER_ATTRIBUTES "vpgQueryBufferAttributes"

// VAAPI private extension for quering buffer attributes for different buffer types
// at the moment supports only VAEncMbDataBufferType 
typedef VAStatus (*hybridQueryBufferAttributes)(
     VADisplay      dpy, 
     VAContextID    context, 
     VABufferType   bufferType,
     void           *outputData,
     unsigned int   *outputDataLen);

#define VAEncMiscParameterTypeVP8HybridFrameUpdate -3

typedef struct _VAEncMiscParameterVP8HybridFrameUpdate
{
    // Previous frame bitstream size in bytes used when BRC is enabled.
    // If it is unknown, like for the first couple frames, it is set to 0.
    unsigned int prev_frame_size;
    // Depending on whether app is running in synchronous or asynchronouse mode,
    // it may have the immediate previous frame size or the 2 prior frame size.
    // BRC algorithm is different for the 2 due to different frame types, below flag
    // indicates if prev_frame_size is from 2 previous frames.
    bool         two_prev_frame_flag;

    // Previous frame costs to use for current frame.  If set to 0 the driver will use default values.
    // MBs encoded by Intra(0), LastRef(1), GoldRef(2), or AltRef(3).
    unsigned short ref_frame_cost[4];
    // Mode costs, one per segment. INTRA_NONPRED(0), INTRA_16x16(1), INTRA_8x8(2), INTRA_4x4(3)
    // Note 0-3 correspond to Universal VME Input Message DW2.0.  See BSPEC for details.
    unsigned short intra_mode_cost[4];
    // MB's encoded by 16x16(0), 16x8(1), 8x8(2), 4x4(3).
    unsigned short inter_mode_cost[4];
    // one per segment
    unsigned short intra_non_dc_penalty_16x16;
    // one per segment
    unsigned short intra_non_dc_penalty_4x4;

} VAEncMiscParameterVP8HybridFrameUpdate;


// Segment map parameters needed by BRC when app provides segmentation map.
#define VAEncMiscParameterTypeVP8SegmentMapParams   -4

typedef struct _VAEncMiscParameterVP8SegmentMapParams
{
    // Specifies the QIndex delta of Y1_ac value for BRC to use, one per segment,
    // when the application provides the VP8 MB Segmentation Buffer using
    // VAEncMacroblockMapBufferType.
    char yac_quantization_index_delta[4];
} VAEncMiscParameterVP8SegmentMapParams;
#if 0
/*======================= FEI ==================================*/
    /**
     * \brief VAEntrypointEncFEIIntel
     *
     * The purpose of FEI (Flexible Encoding Infrastructure) is to allow applications to
     * have more controls and trade off quality for speed with their own IPs. A pre-processing
     * function for getting some statistics and motion vectors is added
     * and some extra controls for Encode pipeline are provided.
     * The application can optionally call the statistics function
     * to get motion vectors and statistics before calling encode function.
     * The application can also optionally provide input to VME for extra
     * encode control and get the output from VME. Application can chose to
     * modify the VME output/PAK input during encoding, but the performance
     * impact is significant.
     *
     * On top of the existing buffers for normal encode, there will be
     * one extra input buffer (VAEncMiscParameterIntelFEIFrameControl) and
     * three extra output buffers (VAIntelEncFEIMVBufferType, VAIntelEncFEIModeBufferType
     * and VAIntelEncFEIDistortionBufferType) for VAEntrypointIntelEncFEI entry function.
     * If separate PAK is set, two extra input buffers
     * (VAIntelEncFEIMVBufferType, VAIntelEncFEIModeBufferType) are needed for PAK input.
     *
     **/
#define VAEntrypointEncFEIIntel     1001
    /**
     * \brief VAEntrypointStatisticsIntel
     *
     * Statistics, like variances, distortions, motion vectors can be obtained
     * via this entry point. Checking whether Statistics is supported can be
     * performed with vaQueryConfigEntrypoints() and the profile argument
     * set to #VAProfileNone. If Statistics entry point is supported,
     * then the list of returned entry-points will include #VAEntrypointIntelStatistics.
     * Supported pixel format, maximum resolution and statistics specific attributes
     * can be obtained via normal attribute query.
     * One input buffer (VAIntelStatsStatisticsParameterBufferType) and one or two
     * output buffers (VAIntelStatsStatisticsBufferType and VAIntelStatsMotionVectorBufferType)
     * are needed for this entry point.
     *
     **/
#define VAEntrypointStatisticsIntel 1002
#endif //0
    /**
     * \brief Encode function type.
     *
     * This attribute conveys whether the driver supports different function types for encode.
     * It can be ENC, PAK, or ENC + PAK. Currently it is for FEI entry point only.
     * Default is ENC + PAK.
     */
#define VAConfigAttribEncFunctionTypeIntel   1001
    /**
     * \brief Maximum number of MV predictors. Read-only.
     *
     * This attribute determines the maximum number of MV predictors the driver
     * can support to encode a single frame. 0 means no MV predictor is supported.
     */
#define VAConfigAttribEncMVPredictorsIntel   1002
    /**
     * \brief Statistics attribute. Read-only.
     *
     * This attribute exposes a number of capabilities of the VAEntrypointStatistics entry
     * point. The attribute value is partitioned into fields as defined in the
     * VAConfigAttribValStatistics union.
     */
#define VAConfigAttribStatisticsIntel        1003

#define VAEncQpBufferType                         30
    /**
     * \brief Intel specific buffer types start at 1001
     */
#if 0 // defined in va.h
#define VAEncFEIMVBufferTypeIntel                 1001
#define VAEncFEIModeBufferTypeIntel               1002
#define VAEncFEIDistortionBufferTypeIntel         1003
#define VAEncFEIMBControlBufferTypeIntel          1004
#define VAEncFEIMVPredictorBufferTypeIntel        1005
#define VAStatsStatisticsParameterBufferTypeIntel 1006
#define VAStatsStatisticsBufferTypeIntel          1007
#define VAStatsMotionVectorBufferTypeIntel        1008
#define VAStatsMVPredictorBufferTypeIntel         1009

    /* Intel specific types start at 1001
     VAEntrypointEncFEIIntel */
#define VAEncMiscParameterTypeFEIFrameControlIntel 1001
#endif //0

#define VAStatsStatisticsBotFieldBufferTypeIntel  999


// move above definition to va_private.h file

#include <va/va_enc.h>

//// change in va_enc_h264.h
//typedef struct _VAEncQpBufferH264 {
//    /*
//     * \brief This structure holds luma Qp per 16x16 macroblock. Buffer size shall be
//     * sufficient to fit the slice or frame to be encoded depending on if it is a slice level
//     * or frame level encoding.
//     */
//    unsigned char qp_y;
//} VAEncQpBufferH264;

#define VA_CONFIG_ATTRIB_FEI_INTERFACE_REV_INTEL  0x014

/**
     * \brief FEI interface Revision attribute. Read-only.
     *
     * This attribute exposes a version number for both VAEntrypointStatistics entry and VAEntrypointEncFEIIntel entry
     * point. The attribute value is used by MSDK team to track va FEI interface change version
     */
#define VAConfigAttribFeiInterfaceRevIntel        1004
#if 0 // defined in va.h
/**
 * \brief Intel specific attribute definitions
 */
/** @name Attribute values for VAConfigAttribEncFunctionTypeIntel
 *
 * The desired type should be passed to driver when creating the configuration.
 * If VA_ENC_FUNCTION_ENC_PAK is set, VA_ENC_FUNCTION_ENC and VA_ENC_FUNCTION_PAK
 * will be ignored if set also.  VA_ENC_FUNCTION_ENC and VA_ENC_FUNCTION_PAK operations
 * shall be called separately if ENC and PAK (VA_ENC_FUNCTION_ENC | VA_ENC_FUNCTION_PAK)
 * is set for configuration. VA_ENC_FUNCTION_ENC_PAK is recommended for best performance
 * If only VA_ENC_FUNCTION_ENC is set, there will be no bitstream output.
 * If VA_ENC_FUNCTION_ENC_PAK is not set and VA_ENC_FUNCTION_PAK is set, then two extra
 * input buffers for PAK are needed: VAEncFEIMVBufferType and VAEncFEIModeBufferType
 **/
/**@{*/
/** \brief Only default is supported */
#define VA_ENC_FUNCTION_DEFAULT_INTEL                         0x00000000
/** \brief ENC only is supported */
#define VA_ENC_FUNCTION_ENC_INTEL                             0x00000001
/** \brief PAK only is supported */
#define VA_ENC_FUNCTION_PAK_INTEL                             0x00000002
/** \brief ENC_PAK is supported */
#define VA_ENC_FUNCTION_ENC_PAK_INTEL                         0x00000004

/**@}*/

/** \brief Attribute value for VAConfigAttribStatisticsIntel */
typedef union _VAConfigAttribValStatisticsIntel {
    struct {
        /** \brief Max number of past reference frames that are supported. */
        unsigned int    max_num_past_references   : 4;
        /** \brief Max number of future reference frames that are supported. */
        unsigned int    max_num_future_references : 4;
        /** \brief Number of output surfaces that are supported */
        unsigned int    num_outputs               : 3;
        /** \brief Interlaced content is supported */
        unsigned int    interlaced                : 1;
        unsigned int    reserved                  : 20;
    } bits;
    unsigned int value;
} VAConfigAttribValStatisticsIntel;

/** \brief VAEncFEIMVBufferTypeIntel and VAStatsMotionVectorBufferTypeIntel. Motion vector buffer layout
 * Motion vector output is per 4x4 block. For each 4x4 block there is a pair of past and future
 * reference MVs as defined in VAMotionVectorIntel. Depending on Subblock partition,
 * for the shape that is not 4x4, the MV is replicated so each 4x4 block has a pair of MVs.
 * If only past reference is used, future MV should be ignored, and vice versa.
 * The 16x16 block is in raster scan order, within the 16x16 block, each 4x4 block MV is ordered as below in memory.
 * The buffer size shall be greater than or equal to the number of 16x16 blocks multiplied by (sizeof(VAMotionVector) * 16)
 *
 *                      16x16 Block
 *        -----------------------------------------
 *        |    1    |    2    |    5    |    6    |
 *        -----------------------------------------
 *        |    3    |    4    |    7    |    8    |
 *        -----------------------------------------
 *        |    9    |    10   |    13   |    14   |
 *        -----------------------------------------
 *        |    11   |    12   |    15   |    16   |
 *        -----------------------------------------
 *
 **/

/** \brief Motion vector data structure. */
typedef struct _VAMotionVectorIntel {
    /** \mv0[0]: horizontal motion vector for past reference */
    /** \mv0[1]: vertical motion vector for past reference */
    /** \mv1[0]: horizontal motion vector for future reference */
    /** \mv1[1]: vertical motion vector for future reference */
    short  mv0[2];  /* past reference */
    short  mv1[2];  /* future reference */
} VAMotionVectorIntel;

#endif // 0

//va_intel_fei.h
/**
 * \defgroup api_intel_fei Intel FEI (Flexible Encoding Infrastructure) encoding API
 *
 * @{
 */

/** \brief FEI frame level control buffer for H.264 */
typedef struct _VAEncMiscParameterFEIFrameControlH264Intel {
    unsigned int      function; /* one of the VAConfigAttribEncFunctionType values */
    /** \brief MB (16x16) control input buffer. It is valid only when (mb_input | mb_size_ctrl)
     * is set to 1. The data in this buffer correspond to the input source. 16x16 MB is in raster scan order,
     * each MB control data structure is defined by VAEncFEIMBControlBufferH264.
     * Buffer size shall not be less than the number of 16x16 blocks multiplied by
     * sizeof(VAEncFEIMBControlBufferH264Intel).
     * Note: if mb_qp is set, VAEncQpBufferH264 is expected.
     */
    VABufferID        mb_ctrl;
    /** \brief distortion output of MB ENC or ENC_PAK.
     * Each 16x16 block has one distortion data with VAEncFEIDistortionBufferH264Intel layout
     * Buffer size shall not be less than the number of 16x16 blocks multiplied by
     * sizeof(VAEncFEIDistortionBufferH264Intel).
     */
    VABufferID        distortion;
    /** \brief MVs data output of MB ENC.
     * Each 16x16 block has one MVs data with layout VAMotionVectorIntel
     * Buffer size shall not be less than the number of 16x16 blocks multiplied by
     * sizeof(VAMotionVectorIntel).
     */
    VABufferID        mv_data;
    /** \brief MBCode data output of MB ENC.
     * Each 16x16 block has one MB Code data with layout VAEncFEIModeBufferH264Intel
     * Buffer size shall not be less than the number of 16x16 blocks multiplied by
     * sizeof(VAEncFEIModeBufferH264Intel).
     */
    VABufferID        mb_code_data;
    /** \brief Qp input buffer. It is valid only when mb_qp is set to 1.
     * The data in this buffer correspond to the input source.
     * One Qp per 16x16 block in raster scan order, each Qp is a signed char (8-bit) value.
     **/
    VABufferID        qp;
    /** \brief MV predictor. It is valid only when mv_predictor_enable is set to 1.
     * Each 16x16 block has one or more pair of motion vectors and the corresponding
     * reference indexes as defined by VAEncMVPredictorBufferH264. 16x16 block is in raster scan order.
     * Buffer size shall not be less than the number of 16x16 blocks multiplied by
     * sizeof(VAEncMVPredictorBufferH264). */
    VABufferID        mv_predictor;

    /** \brief number of MV predictors. It must not be greater than maximum supported MV predictor. */
    unsigned int      num_mv_predictors_l0      : 16;
    unsigned int      num_mv_predictors_l1      : 16;

    /** \brief control parameters */
    unsigned int      search_path                : 8;
    unsigned int      len_sp                    : 8;
    unsigned int      reserved0                 : 16;

    unsigned int      sub_mb_part_mask          : 7;
    unsigned int      intra_part_mask           : 5;
    unsigned int      multi_pred_l0             : 1;
    unsigned int      multi_pred_l1             : 1;
    unsigned int      sub_pel_mode              : 2;
    unsigned int      inter_sad                 : 2;
    unsigned int      intra_sad                 : 2;
    unsigned int      distortion_type           : 1;
    unsigned int      repartition_check_enable  : 1;
    unsigned int      adaptive_search           : 1;
    unsigned int      mv_predictor_enable       : 1;
    unsigned int      mb_qp                     : 1;
    unsigned int      mb_input                  : 1;
    unsigned int      mb_size_ctrl              : 1;
    unsigned int      colocated_mb_distortion   : 1;
    unsigned int      reserved1                 : 4;

    unsigned int      ref_width                 : 8;
    unsigned int      ref_height                : 8;
    unsigned int      search_window             : 4;
    unsigned int      reserved2                 : 12;

    unsigned int      max_frame_size;
    unsigned int      num_passes;     //number of QPs
    unsigned char     *delta_qp;      //list of detla QPs
    unsigned int      reserved3;
} VAEncMiscParameterFEIFrameControlH264Intel;


/** \brief FEI MB level control data structure */
typedef struct _VAEncFEIMBControlH264Intel {
    /** \brief when set, correposndent MB is coded as intra */
    unsigned int force_to_intra      : 1;
    /** \brief when set, correposndent MB is coded as skip */
    unsigned int force_to_skip       : 1;
    unsigned int force_to_nonskip    : 1;
    unsigned int reserved0           : 29;

    unsigned int reserved1;

    unsigned int reserved2;

    /** \brief when mb_size_ctrl is set, size here is used to budget accumulatively. Set to 0xFF if don't care. */
    unsigned int reserved3           : 16;
    unsigned int target_size_in_word : 8;
    unsigned int max_size_in_word    : 8;
} VAEncFEIMBControlH264Intel;


/** \brief Application can use this definition as reference to allocate the buffer
 * based on MaxNumPredictor returned from attribute VAConfigAttribEncMVPredictorsIntel query.
 **/
typedef struct _VAEncMVPredictorH264Intel {
    /** \brief Reference index corresponding to the entry of RefPicList0 & RefPicList1 in VAEncSliceParameterBufferH264.
     * Note that RefPicList0 & RefPicList1 needs to be the same for all slices.
     * ref_idx_l0_x : index to RefPicList0; ref_idx_l1_x : index to RefPicList1; x : 0 - MaxNumPredictor.
     **/
    unsigned int ref_idx_l0_0 : 4;
    unsigned int ref_idx_l1_0 : 4;
    unsigned int ref_idx_l0_1 : 4;
    unsigned int ref_idx_l1_1 : 4;
    unsigned int ref_idx_l0_2 : 4;
    unsigned int ref_idx_l1_2 : 4;
    unsigned int ref_idx_l0_3 : 4;
    unsigned int ref_idx_l1_3 : 4;
    unsigned int reserved;
    /** \brief MV. MaxNumPredictor must be the returned value from attribute VAConfigAttribEncMVPredictors query.
     * Even application doesn't use the maximum predictors, the VAEncMVPredictorH264 structure size
     * has to be defined as maximum so each MB can be at a fixed location.
     * Note that 0x8000 must be used for correspondent intra block.
     **/
    struct _mv
    {
    /** \brief Motion vector corresponding to ref0x_index.
     * mv0[0] is horizontal motion vector and mv0[1] is vertical motion vector. */
        short    mv0[2];
    /** \brief Motion vector corresponding to ref1x_index.
     * mv1[0] is horizontal motion vector and mv1[1] is vertical motion vector. */
        short    mv1[2];
    } mv[4]; /* MaxNumPredictor is 4 */
} VAEncMVPredictorH264Intel;


/** \brief FEI output */
/**
 * Motion vector output is per 4x4 block. For each 4x4 block there is a pair of MVs
 * for RefPicList0 and RefPicList1 and each MV is 4 bytes including horizontal and vertical directions.
 * Depending on Subblock partition, for the shape that is not 4x4, the MV is replicated
 * so each 4x4 block has a pair of MVs. The 16x16 block has 32 MVs (128 bytes).
 * 0x8000 is used for correspondent intra block. The 16x16 block is in raster scan order,
 * within the 16x16 block, each 4x4 block MV is ordered as below in memory.
 * The buffer size shall be greater than or equal to the number of 16x16 blocks multiplied by 128 bytes.
 * Note that, when separate ENC and PAK is enabled, the exact layout of this buffer is needed for PAK input.
 * App can reuse this buffer, or copy to a different buffer as PAK input.
 *                      16x16 Block
 *        -----------------------------------------
 *        |    1    |    2    |    5    |    6    |
 *        -----------------------------------------
 *        |    3    |    4    |    7    |    8    |
 *        -----------------------------------------
 *        |    9    |    10   |    13   |    14   |
 *        -----------------------------------------
 *        |    11   |    12   |    15   |    16   |
 *        -----------------------------------------
 **/

/** \brief VAEncFEIModeBufferIntel defines the data structure for VAEncFEIModeBufferTypeIntel per 16x16 MB block.
 * The 16x16 block is in raster scan order. Buffer size shall not be less than the number of 16x16 blocks
 * multiplied by sizeof(VAEncFEIModeBufferH264Intel). Note that, when separate ENC and PAK is enabled,
 * the exact layout of this buffer is needed for PAK input. App can reuse this buffer,
 * or copy to a different buffer as PAK input, reserved elements must not be modified when used as PAK input.
 **/
typedef struct _VAEncFEIModeBufferH264Intel {
    unsigned int    reserved0;
    unsigned int    reserved1[3];

    unsigned int    inter_mb_mode            : 2;
    unsigned int    mb_skip_flag             : 1;
    unsigned int    reserved00               : 1;
    unsigned int    intra_mb_mode            : 2;
    unsigned int    reserved01               : 1;
    unsigned int    field_mb_polarity_flag   : 1;
    unsigned int    mb_type                  : 5;
    unsigned int    intra_mb_flag            : 1;
    unsigned int    field_mb_flag            : 1;
    unsigned int    transform8x8_flag        : 1;
    unsigned int    reserved02               : 1;
    unsigned int    dc_block_coded_cr_flag   : 1;
    unsigned int    dc_block_coded_cb_flag   : 1;
    unsigned int    dc_block_coded_y_flag    : 1;
    unsigned int    reserved03               : 12;

    unsigned int    horz_origin              : 8;
    unsigned int    vert_origin              : 8;
    unsigned int    cbp_y                    : 16;

    unsigned int    cbp_cb                   : 16;
    unsigned int    cbp_cr                   : 16;

    unsigned int    qp_prime_y               : 8;
    unsigned int    reserved30               : 17;
    unsigned int    mb_skip_conv_disable     : 1;
    unsigned int    is_last_mb               : 1;
    unsigned int    enable_coefficient_clamp : 1;
    unsigned int    direct8x8_pattern        : 4;

    union
    {
        /* Intra MBs */
        struct
        {
            unsigned int   luma_intra_pred_modes0 : 16;
            unsigned int   luma_intra_pred_modes1 : 16;

            unsigned int   luma_intra_pred_modes2 : 16;
            unsigned int   luma_intra_pred_modes3 : 16;

            unsigned int   mb_intra_struct        : 8;
            unsigned int   reserved60             : 24;
        } intra_mb;

        /* Inter MBs */
        struct
        {
            unsigned int   sub_mb_shapes          : 8;
            unsigned int   sub_mb_pred_modes      : 8;
            unsigned int   reserved40             : 16;

            unsigned int   ref_idx_l0_0           : 8;
            unsigned int   ref_idx_l0_1           : 8;
            unsigned int   ref_idx_l0_2           : 8;
            unsigned int   ref_idx_l0_3           : 8;

            unsigned int   ref_idx_l1_0           : 8;
            unsigned int   ref_idx_l1_1           : 8;
            unsigned int   ref_idx_l1_2           : 8;
            unsigned int   ref_idx_l1_3           : 8;
        } inter_mb;
    } mb_mode;

    unsigned int   reserved70                : 16;
    unsigned int   target_size_in_word       : 8;
    unsigned int   max_size_in_word          : 8;

    unsigned int   reserved2[4];
} VAEncFEIModeBufferH264Intel;

/** \brief VAEncFEIDistortionBufferIntel defines the data structure for
 * VAEncFEIDistortionBufferType per 16x16 MB block. The 16x16 block is in raster scan order.
 * Buffer size shall not be less than the number of 16x16 blocks multiple by sizeof(VAEncFEIDistortionBufferIntel).
 **/
typedef struct _VAEncFEIDistortionBufferH264Intel {
    /** \brief Inter-prediction-distortion associated with motion vector i (co-located with subblock_4x4_i).
     * Its meaning is determined by sub-shape. It must be zero if the corresponding sub-shape is not chosen.
     **/
    unsigned short  inter_distortion[16];
    unsigned int    best_inter_distortion     : 16;
    unsigned int    best_intra_distortion     : 16;
    unsigned int    colocated_mb_distortion   : 16;
    unsigned int    reserved                  : 16;
    unsigned int    reserved1[2];
} VAEncFEIDistortionBufferH264Intel;


//va_intel_statastics.h

typedef struct _VAPictureFEI
{
    VASurfaceID picture_id;
    /*
     *      * see flags below.
     *           */
    unsigned int flags;
} VAPictureFEI;
/* flags in VAPictureFEI could be one of the following */
#define VA_PICTURE_FEI_INVALID                   0xffffffff
#define VA_PICTURE_FEI_PROGRESSIVE               0x00000000
#define VA_PICTURE_FEI_TOP_FIELD                 0x00000001
#define VA_PICTURE_FEI_BOTTOM_FIELD              0x00000002
#define VA_PICTURE_FEI_CONTENT_UPDATED           0x00000010

/** \brief Motion Vector and Statistics frame level controls.
 * VAStatsStatisticsParameterBufferTypeIntel for 16x16 block
 **/
typedef struct _VAStatsStatisticsParameter16x16Intel
{
    /** \brief Source surface ID.  */
    VAPictureFEI     input;

    VAPictureFEI    *past_references;
    unsigned int    num_past_references;
    VABufferID      *past_ref_stat_buf;
    VAPictureFEI    *future_references;
    unsigned int    num_future_references;
    VABufferID      *future_ref_stat_buf;

    /** \brief ID of the output buffer.
     * The number of outputs is determined by below DisableMVOutput and DisableStatisticsOutput.
     * The output layout is defined by VAStatsStatisticsBufferType and VAStatsMotionVectorBufferType.
     **/
    VABufferID      *outputs;

    /** \brief MV predictor. It is valid only when mv_predictor_ctrl is not 0.
     * Each 16x16 block has a pair of MVs, one for past and one for future reference
     * as defined by VAMotionVector. The 16x16 block is in raster scan order.
     * Buffer size shall not be less than the number of 16x16 blocks multiplied by sizeof(VAMotionVector).
     **/
    VABufferID      mv_predictor;

    /** \brief Qp input buffer. It is valid only when mb_qp is set to 1.
     * The data in this buffer correspond to the input source.
     * One Qp per 16x16 block in raster scan order, each Qp is a signed char (8-bit) value.
     **/
    VABufferID      qp;

    unsigned int    frame_qp                    : 8;
    unsigned int    len_sp                      : 8;
    unsigned int    search_path                  : 8;
    unsigned int    reserved0                   : 8;

    unsigned int    sub_mb_part_mask            : 7;
    unsigned int    sub_pel_mode                : 2;
    unsigned int    inter_sad                   : 2;
    unsigned int    intra_sad                   : 2;
    unsigned int    adaptive_search             : 1;
    /** \brief indicate if future or/and past MV in mv_predictor buffer is valid.
     * 0: MV predictor disabled
     * 1: MV predictor enabled for past reference
     * 2: MV predictor enabled for future reference
     * 3: MV predictor enabled for both past and future references
     **/
    unsigned int    mv_predictor_ctrl           : 3;
    unsigned int    mb_qp                       : 1;
    unsigned int    ft_enable                   : 1;
    /** \brief luma intra mode partition mask
     * xxxx1: luma_intra_16x16 disabled
     * xxx1x: luma_intra_8x8 disabled
     * xx1xx: luma_intra_4x4 disabled
     * xx111: intra prediction is disabled
     **/
    unsigned int    intra_part_mask             : 5;
    unsigned int    reserved1                   : 8;

    unsigned int    ref_width                   : 8;
    unsigned int    ref_height                  : 8;
    unsigned int    search_window               : 4;
    unsigned int    reserved2                   : 12;

    /** \brief MVOutput. When set to 1, MV output is NOT provided */
    unsigned int    disable_mv_output           : 1;
    /** \brief StatisticsOutput. When set to 1, Statistics output is NOT provided. */
    unsigned int    disable_statistics_output   : 1;
    unsigned int    enable_8x8statistics        : 1;
    unsigned int    reserved3                   : 29;

} VAStatsStatisticsParameter16x16Intel;

/** \brief VAStatsStatisticsBufferTypeIntel. Statistics buffer layout.
 * Statistics output is per 16x16 block. Data structure per 16x16 block is defined below.
 * The 16x16 block is in raster scan order. The buffer size shall be greater than or equal to
 * the number of 16x16 blocks multiplied by sizeof(VAStatsStatistics16x16Intel).
 **/
typedef struct _VAStatsStatistics16x16Intel
{
    /** \brief past reference  */
    unsigned int    best_inter_distortion0 : 16;
    unsigned int    inter_mode0            : 16;

    /** \brief future reference  */
    unsigned int    best_inter_distortion1 : 16;
    unsigned int    inter_mode1            : 16;

    unsigned int    best_intra_distortion  : 16;
    unsigned int    intra_mode             : 16;

    unsigned int    num_non_zero_coef      : 16;
    unsigned int    reserved               : 16;

    unsigned int    sum_coef;

    /** \brief DWORD 5 flat info **/
    unsigned int    mb_is_flat             : 1;
    unsigned int    reserved1              : 31;

    /** \brief DWORD 6 variance for block16x16**/
    unsigned int    variance16x16;
    /** \brief DWORD 7 ~ 10, variance for block8x8 **/
    unsigned int    variance8x8[4];

    /** \brief DWORD 11 pixel_average for block16x16 **/
    unsigned int    pixel_average16x16;
    /** \brief DWORD 12 ~ 15, pixel_average for block8x8 **/
    unsigned int    pixel_average8x8[4];
} VAStatsStatistics16x16Intel;  // size is 64 bytes


#endif // __VAAPI_EXT_INTERFACE_H__
