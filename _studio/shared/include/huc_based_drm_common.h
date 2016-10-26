//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __HUC_BASED_DRM_COMMON_H__
#define __HUC_BASED_DRM_COMMON_H__

#include "huc_based_drm_avc_structs.h"
#include "huc_based_drm_hevc_structs.h"
#include "huc_based_drm_vp9_structs.h"

#ifdef UMC_VA
typedef struct _VADecryptSegmentInfo
{
    unsigned int    uiSegmentStartOffset;
    unsigned int    uiSegmentLength;
    unsigned int    uiPartialAesBlockSizeInBytes;
    unsigned int    uiInitByteLength;
    unsigned int    uiAesCbcIvOrCtr[4];
} VADecryptSegmentInfo;

typedef struct _VADecryptInputBuffer
{
    unsigned short          usStatusReportFeedbackNumber;
    unsigned int            uiNumSegments;
    VADecryptSegmentInfo    *pSegmentInfo;
    unsigned char           ucSizeOfLength;
    unsigned int            uiAppId;
} VADecryptInputBuffer;

typedef enum
{
    VA_DECRYPT_STATUS_SUCCESSFUL = 0,
    VA_DECRYPT_STATUS_INCOMPLETE,
    VA_DECRYPT_STATUS_ERROR,
    VA_DECRYPT_STATUS_UNAVAILABLE
} VADecryptStatus;
#endif // UMC_VA


#ifdef UMC_VA_DXVA
#define INTEL_DECODE_EXTENSION_DECRYPT_WIDEVINE_CLASSIC 0x1
#define INTEL_DECODE_EXTENSION_DECRYPT_GOOGLE_DASH      0x2
#define INTEL_DECODE_EXTENSION_DECRYPT_PLAYREADY        0x3
#define INTEL_DECODE_EXTENSION_GET_DECRYPT_STATUS       0x4
#endif


#ifdef UMC_VA_LINUX
#define VA_ENCRYPTION_TYPE_WIDEVINE_CLASSIC 0x00000001
#define VA_ENCRYPTION_TYPE_GOOGLE_DASH      0x00000002
#define VA_ENCRYPTION_TYPE_PLAYREADY        0x00000003

#define VADecryptStatusRequested     -2
#define VAStatusParameterBufferType  -5
#define VADecryptParameterBufferType -6

typedef struct
{
    VASurfaceStatus decrypt_status_requested;
    VAContextID     context;
    unsigned int    data_size;
    void*           data;
} VAIntelDecryptStatus;
#endif // UMC_VA_LINUX

#if !defined (__GNUC__)
#pragma warning(disable: 4201)
#endif

typedef struct _DECRYPT_QUERY_STATUS_PARAMS_AVC
{
    uint32_t                            status;                        // formatted as DECRYPT_STATUS above
    uint16_t                            usStatusReportFeedbackNumber;
    union
    {
        struct
        {
            uint8_t                     nal_ref_idc;
            uint8_t                     ui8FieldPicFlag;               // 1 - top field, 2 - bottom field, 3 - frame

            uint32_t                    frame_num;
            uint32_t                    slice_type;                    // 2&4 - I, 0&3 - P, 1 - B
            uint32_t                    idr_flag;
            int32_t                     iCurrFieldOrderCnt[2];

            seq_param_set_t             SeqParams;
            pic_param_set_t             PicParams;
            h264_Dec_Ref_Pic_Marking_t  RefPicMarking;

            h264_SEI_buffering_period_t SeiBufferingPeriod;
            h264_SEI_pic_timing_t       SeiPicTiming;
            h264_SEI_recovery_point_t   SeiRecoveryPoint;
        };
        struct
        {
            void*                       pvSliceHeaderSetBuf;
            uint32_t                    SliceHeaderSetBufLength;
        };
    };
} DECRYPT_QUERY_STATUS_PARAMS_AVC, *PDECRYPT_QUERY_STATUS_PARAMS_AVC;


typedef struct _DECRYPT_QUERY_STATUS_PARAMS_HEVC
{
    uint32_t                    uiStatus;

    uint16_t                    ui16StatusReportFeebackNumber;
    uint16_t                    ui16SlicePicOrderCntLsb;

    uint8_t                     ui8SliceType;
    uint8_t                     ui8NalUnitType;
    uint8_t                     ui8PicOutputFlag;
    uint8_t                     ui8NoOutputOfPriorPicsFlag;

    uint8_t                     ui8NuhTemporalId;
    uint8_t                     ui8HasEOS;
    uint16_t                    ui16Padding;

    h265_seq_param_set_t        SeqParams;
    h265_pic_param_set_t        PicParams;
    h265_ref_frames_t           RefFrames;
    h265_sei_buffering_period_t SeiBufferingPeriod;
    h265_sei_pic_timing_t       SeiPicTiming;
} DECRYPT_QUERY_STATUS_PARAMS_HEVC, *PDECRYPT_QUERY_STATUS_PARAMS_HEVC;


typedef struct _DECRYPT_QUERY_STATUS_PARAMS_VP9
{
    uint32_t uiStatus;         // MUST NOT BE MOVED as every codec assumes Status is first UINT in struct! Formatted as CODECHAL_STATUS.

    uint16_t ui16StatusReportFeedbackNumber;

    uint16_t picture_width_minus1;
    uint16_t picture_height_minus1;
    uint16_t display_width_minus1;
    uint16_t display_height_minus1;

    uint8_t  version;
    uint8_t  show_existing_frame_flag;
    uint8_t  frame_type;
    uint8_t  frame_toshow_slot;
    uint8_t  show_frame_flag;
    uint8_t  error_resilient_mode;
    uint8_t  color_space;
    uint8_t  subsampling_x;
    uint8_t  subsampling_y;
    uint8_t  refresh_frame_flags;
    uint8_t  display_different_size_flags;
    uint8_t  frame_parallel_decoding_mode;
} DECRYPT_QUERY_STATUS_PARAMS_VP9, *PDECRYPT_QUERY_STATUS_PARAMS_VP9;


#endif  // __HUC_BASED_DRM_COMMON_H__