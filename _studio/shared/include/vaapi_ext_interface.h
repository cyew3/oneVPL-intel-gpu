//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2017 Intel Corporation. All Rights Reserved.
//

#ifndef __VAAPI_EXT_INTERFACE_H__
#define __VAAPI_EXT_INTERFACE_H__

#include "mfx_config.h"

#ifdef MFX_EXTBUFF_GPU_HANG_ENABLE
#define VATriggerCodecHangBufferType ((VABufferType)-16)
#endif // MFX_EXTBUFF_GPU_HANG_ENABLE

#ifndef MFX_PROTECTED_FEATURE_DISABLE
// encryption parameters for PAVP
#define  VAEncryptionParameterBufferType   -3
#define EXTERNAL_ENCRYPTED_SURFACE_FLAG   (1<<16)
#define VA_CODED_BUF_STATUS_PRIVATE_DATA_HDCP (1<<8)
#define VA_HDCP_ENABLED (1<<12)
#endif // #ifndef MFX_PROTECTED_FEATURE_DISABLE

#define VA_CODED_BUF_STATUS_HW_TEAR_DOWN 0x4000

#ifndef MFX_PROTECTED_FEATURE_DISABLE
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
#endif

#ifdef MFX_ENABLE_VP8_VIDEO_ENCODE_HW
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
#endif // #ifdef MFX_ENABLE_VP8_VIDEO_ENCODE_HW


/*======================= FEI ==================================*/
#ifdef OPEN_SOURCE

#define VA_ENC_FUNCTION_DEFAULT_INTEL 0x00000000
#define VA_ENC_FUNCTION_ENC_INTEL     0x00000001
#define VA_ENC_FUNCTION_PAK_INTEL     0x00000002
#define VA_ENC_FUNCTION_ENC_PAK_INTEL 0x00000004

#define VAEntrypointEncFEIIntel     1001
#define VAEntrypointStatisticsIntel 1002

#define VAEncMiscParameterTypeFEIFrameControlIntel 1001

#define VAEncFEIMVBufferTypeIntel                   1001
#define VAEncFEIModeBufferTypeIntel                 1002
#define VAEncFEIDistortionBufferTypeIntel           1003
#define VAEncFEIMBControlBufferTypeIntel            1004
#define VAEncFEIMVPredictorBufferTypeIntel          1005
#define VAStatsStatisticsParameterBufferTypeIntel   1006
#define VAStatsStatisticsBufferTypeIntel            1007
#define VAStatsMotionVectorBufferTypeIntel          1008
#define VAStatsMVPredictorBufferTypeIntel           1009

typedef struct _VAMotionVectorIntel {
    short  mv0[2];
    short  mv1[2];
} VAMotionVectorIntel;

#endif // OPEN_SOURCE

#endif // __VAAPI_EXT_INTERFACE_H__
