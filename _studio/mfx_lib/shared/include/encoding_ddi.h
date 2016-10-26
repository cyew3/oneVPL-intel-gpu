//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __ENCODING_DDI_H__
#define __ENCODING_DDI_H__

#if defined(_WIN32) || defined(_WIN64)

#pragma warning(disable: 4201)

#include "mfx_platform_headers.h"
#include "mfxstructures.h"
#include "mfxstructurespro.h"

#define D3DDDIFORMAT        D3DFORMAT
#define DXVADDI_VIDEODESC   DXVA2_VideoDesc
#include "encoder_ddi.hpp"
#define NEW_STATUS_REPORTING_DDI_0915
//#pragma pack(push, 4)

static const mfxU32 NUM_MV_PER_MB = 2 * 16;

static const GUID DXVA2_Intel_Auxiliary_Device = 
{ 0xa74ccae2, 0xf466, 0x45ae, { 0x86, 0xf5, 0xab, 0x8b, 0xe8, 0xaf, 0x84, 0x83 } };

// GUIDs from DDI spec 0.73
static const GUID DXVA2_Intel_Encode_AVC = 
{ 0x97688186, 0x56a8, 0x4094, { 0xb5, 0x43, 0xfc, 0x9d, 0xaa, 0xa4, 0x9f, 0x4b } };
static const GUID DXVA2_Intel_Encode_MPEG2 = 
{ 0xc346e8a3, 0xcbed, 0x4d27, { 0x87, 0xcc, 0xa7, 0xe, 0xb4, 0xdc, 0x8c, 0x27 } };
static const GUID DXVA2_Intel_Encode_VC1 = 
{ 0x2750bd0e, 0x97ef, 0x44bf, { 0x92, 0x22, 0x5f, 0xf3, 0xb6, 0x9b, 0xe6, 0x77 } };
static const GUID DXVA2_Intel_Encode_SVC = 
{ 0xd41289c2, 0xecf3, 0x4ede, { 0x9a, 0x04, 0x3b, 0xbf, 0x90, 0x68, 0xa6, 0x29 } };
// GUIDs from DDI spec 0.103
static const GUID DXVA2_Intel_Encode_JPEG = 
{ 0x50925b7b, 0xe931, 0x4978, { 0xa1, 0x2a, 0x58, 0x66, 0x30, 0xf0, 0x95, 0xf9 } };
// GUID from DDI for VP8 Encoder spec 0.2
static const GUID DXVA2_Intel_Encode_VP8 = 
{ 0x2364d06a, 0xf67f, 0x4186, { 0xae, 0xd0, 0x62, 0xb9, 0x9e, 0x17, 0x84, 0xf1 } };


// {55BF20CF-6FF1-4fb4-B08B-33E97E15BA29}
static const GUID DXVA2_IntelEncode = 
{ 0x55bf20cf, 0x6ff1, 0x4fb4, { 0xb0, 0x8b, 0x33, 0xe9, 0x7e, 0x15, 0xba, 0x29 } };

// {97688186-56A8-4094-B543-FC9DAAA49F4B}
static const GUID INTEL_ENCODE_GUID_AVC_PAK_AND_VME = // !!! VME_AND_PAK
{ 0x97688186, 0x56a8, 0x4094, { 0xb5, 0x43, 0xfc, 0x9d, 0xaa, 0xa4, 0x9f, 0x4b } };

// {11291410-3AE6-4fc6-90D9-7B6A58075F31}
static const GUID INTEL_ENCODE_GUID_AVC_PAK = 
{ 0x11291410, 0x3ae6, 0x4fc6, { 0x90, 0xd9, 0x7b, 0x6a, 0x58, 0x7, 0x5f, 0x31 } };

// {9C0D06F8-6B3E-483c-9641-8FDDF07EEF83}
static const GUID INTEL_ENCODE_GUID_AVC_VME = 
{ 0x9c0d06f8, 0x6b3e, 0x483c, { 0x96, 0x41, 0x8f, 0xdd, 0xf0, 0x7e, 0xef, 0x83 } };

// {C346E8A3-CBED-4d27-87CC-A70EB4DC8C27}
static const GUID INTEL_ENCODE_GUID_MPEG2_VME = 
{ 0xc346e8a3, 0xcbed, 0x4d27, { 0x87, 0xcc, 0xa7, 0xe, 0xb4, 0xdc, 0x8c, 0x27 } };

// {2750BD0E-97EF-44bf-9222-5FF3B69BE677}
static const GUID INTEL_ENCODE_GUID_VC1_VME = 
{ 0x2750bd0e, 0x97ef, 0x44bf, { 0x92, 0x22, 0x5f, 0xf3, 0xb6, 0x9b, 0xe6, 0x77 } };

// !!!
static const GUID INTEL_ENCODE_GUID_MPEG2_PAK = 
{ 0x1110bd0e, 0x97ef, 0x44bf, { 0x92, 0x22, 0x5f, 0xf3, 0xb6, 0x9b, 0xe6, 0x77 } };

// !!! This device GUID is not reported by the GetVideoProcessorDeviceGuids() method.
// {44dee63b-77e6-4dd2-b2e1-443b130f2e2f}
static const GUID DXVA2_Registration_Device = 
{ 0x44dee63b, 0x77e6, 0x4dd2, { 0xb2, 0xe1, 0x44, 0x3b, 0x13, 0x0f, 0x2e, 0x2f } };

// Fast Copy Device GUID
static const GUID DXVA2_Intel_Fast_Copy_Device = 
{ 0x4b34d630, 0xd43c, 0x48a2, { 0x9a, 0xdd, 0xa3, 0xcf, 0x8f, 0xa2, 0xb4, 0x24 } };

// Fast Compositing Device GUID
static const GUID DXVA2_Intel_Fast_Compositing_Device = 
{ 0x7dd2d599, 0x41db, 0x4456, { 0x93, 0x95, 0x8b, 0x3d, 0x18, 0xce, 0xdc, 0xae } };

static const GUID DXVA2_INTEL_PAVP = 
{ 0x07460004,0x7533, 0x4e1a, { 0xbd, 0xe3, 0xff, 0x20, 0x6b, 0xf5, 0xce, 0x47 } };

// {1424D4DC-7CF5-4BB1-9CD7-B63717A72A6B}
static const GUID DXVA2_INTEL_LOWPOWERENCODE_AVC = 
{0x1424d4dc, 0x7cf5, 0x4bb1, { 0x9c, 0xd7, 0xb6, 0x37, 0x17, 0xa7, 0x2a, 0x6b} };

// GUIDs from DDI for HEVC Encoder spec 0.88
static const GUID DXVA2_Intel_Encode_HEVC_Main =
{ 0x28566328, 0xf041, 0x4466, { 0x8b, 0x14, 0x8f, 0x58, 0x31, 0xe7, 0x8f, 0x8b } };
static const GUID DXVA2_Intel_Encode_HEVC_Main10 =
{ 0x6b4a94db, 0x54fe, 0x4ae1, { 0x9b, 0xe4, 0x7a, 0x7d, 0xad, 0x0, 0x46, 0x0 } };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main =
{ 0xb8b28e0c, 0xecab, 0x4217, { 0x8c, 0x82, 0xea, 0xaa, 0x97, 0x55, 0xaa, 0xf0 } };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main10 =
{ 0x8732ecfd, 0x9747, 0x4897, { 0xb4, 0x2a, 0xe5, 0x34, 0xf9, 0xff, 0x2b, 0x7a } };

#define AVC_D3D9_DDI_VERSION 928
#define AVC_D3D11_DDI_VERSION 370  
#define INTEL_AVC_ENCODE_DDI_VERSION (AVC_D3D9_DDI_VERSION|(AVC_D3D11_DDI_VERSION<<16))
#define INTEL_MPEG2_ENCODE_DDI_VERSION 928
#define INTEL_MJPEG_ENCODE_DDI_VERSION 35

// enum and struct from DXVA FastCopy DDI 0.7
typedef enum
{
    FASTCOPY_QUERY_REGISTRATION_HANDLE  =   1,

} FASTCOPY_QUERYTYPE;

typedef struct tagFASTCOPY_BLT_PARAMS
{
    IDirect3DSurface9 *pSource;
    IDirect3DSurface9 *pDest;

} FASTCOPY_BLT_PARAMS;

typedef enum REGISTRATION_OP
{
    REG_IGNORE      = 0,
    REG_REGISTER    = 1,
    REG_UNREGISTER  = 2
};

#ifdef NEW_STATUS_REPORTING_DDI_0915
// new encode query status interface (starting from DDI 0.915)
typedef struct tagENCODE_QUERY_STATUS_PARAMS_DESCR
{
    UINT StatusParamType;
    UINT SizeOfStatusParamStruct;
    UINT reserved[2];
} ENCODE_QUERY_STATUS_PARAMS_DESCR;

// new encode query status interface (starting from DDI 0.915)
typedef enum tagENCODE_QUERY_STATUS_PARAM_TYPE
{ 
    QUERY_STATUS_PARAM_FRAME = 0, // Frame level reporting, the current default.
    QUERY_STATUS_PARAM_SLICE = 1 // Slice level reporting, not yet supported.
} ENCODE_QUERY_STATUS_PARAM_TYPE;
#endif // NEW_STATUS_REPORTING_DDI_0915

typedef struct tagENCODE_AES128_CIPHER_COUNTER
{
    UINT64 IV;
    UINT64 Counter;
} ENCODE_AES128_CIPHER_COUNTER;

// new encode query status interface (starting from DDI 0.915)
typedef struct tagENCODE_QUERY_STATUS_PARAMS
{
    UINT    StatusReportFeedbackNumber;
    ENCODE_PICENTRY CurrOriginalPic; 
    UCHAR   field_pic_flag; 
    UCHAR   bStatus;
    CHAR    reserved0;
    UINT    Func;
    UINT    bitstreamSize;
    CHAR    QpY;
    CHAR    SuggestedQpYDelta;
    UCHAR   NumberPasses;
    UCHAR   AverageQP;
#ifdef NEW_STATUS_REPORTING_DDI_0915
    union
    {
        struct
        {
              UINT PanicMode            : 1;
              UINT SliceSizeOverflow    : 1;
              UINT NumSliceNonCompliant : 1;
              UINT LongTermReference    : 1;
              UINT                      : 28;
        };
        UINT QueryStatusFlags;
    };
    UINT    MAD;
    USHORT    NumberSlices;

    USHORT    PSNRx100[3];
    USHORT    NextFrameWidthMinus1;
    USHORT    NextFrameHeightMinus1;

    ENCODE_AES128_CIPHER_COUNTER aes_counter;

    UINT    Reserved1;
    UINT    Reserved2;
    UINT    Reserved3;
    UINT    Reserved4;

#endif // NEW_STATUS_REPORTING_DDI_0915

} ENCODE_QUERY_STATUS_PARAMS, *PENCODE_QUERY_STATUS_PARAMS;

// new encode query status interface for slice level report (starting from DDI 0.935)
typedef struct tagENCODE_QUERY_STATUS_SLICE_PARAMS
{
    ENCODE_QUERY_STATUS_PARAMS FrameLevelStatus;
    UINT reserved[4];
    USHORT *SliceSizes;
} ENCODE_QUERY_STATUS_SLICE_PARAMS, *PENCODE_QUERY_STATUS_SLICE_PARAMS;

// from "Intel DXVA Encoding DDI for Vista rev 0.77"
enum
{
    ENCODE_ENC_ID                           = 0x100,
    ENCODE_PAK_ID                           = 0x101,
    ENCODE_ENC_PAK_ID                       = 0x102,
    ENCODE_VPP_ID                           = 0x103, // reserved for now

    ENCODE_FORMAT_COUNT_ID                  = 0x104,
    ENCODE_FORMATS_ID                       = 0x105,
    ENCODE_ENC_CTRL_CAPS_ID                 = 0x106,
    ENCODE_ENC_CTRL_GET_ID                  = 0x107,
    ENCODE_ENC_CTRL_SET_ID                  = 0x108,
    MBDATA_LAYOUT_ID                        = 0x109,
    ENCODE_INSERT_DATA_ID                   = 0x120,
    ENCODE_QUERY_STATUS_ID                  = 0x121
};

// From "Intel DXVA2 Auxiliary Functionality Device rev 0.6"
typedef enum
{
    AUXDEV_GET_ACCEL_GUID_COUNT             = 1,
    AUXDEV_GET_ACCEL_GUIDS                  = 2,
    AUXDEV_GET_ACCEL_RT_FORMAT_COUNT        = 3,
    AUXDEV_GET_ACCEL_RT_FORMATS             = 4,
    AUXDEV_GET_ACCEL_FORMAT_COUNT           = 5,
    AUXDEV_GET_ACCEL_FORMATS                = 6,
    AUXDEV_QUERY_ACCEL_CAPS                 = 7,
    AUXDEV_CREATE_ACCEL_SERVICE             = 8,
    AUXDEV_DESTROY_ACCEL_SERVICE            = 9
} AUXDEV_FUNCTION_ID;

#ifdef DDI_086
typedef struct tagENCODE_SET_VUI_PARAMETER
{
    UINT    AspectRatioInfoPresentFlag          : 1;
    UINT    OverscanInfoPresentFlag              : 1;
    UINT    OverscanAppropriateFlag               : 1;
    UINT    VideoSignalTypePresentFlag          : 1;
    UINT    VideoFullRangeFlag                   : 1;
    UINT    ColourDescriptionPresentFlag         : 1;
    UINT    ChromaLocInfoPresentFlag            : 1;
    UINT    TimingInfoPresentFlag                : 1;
    UINT    FixedFrameRateFlag                   : 1;
    UINT    NalHrdParametersPresentFlag         : 1;
    UINT    VclHrdParametersPresentFlag         : 1;
    UINT    LowDelayHrdFlag                      : 1;
    UINT    PicStructPresentFlag                 : 1;
    UINT    BitstreamRestrictionFlag              : 1;
    UINT    MotionVectorsOverPicBoundariesFlag : 1;
    UINT                                            : 17;
    USHORT  SarWidth;
    USHORT  SarHeight;
    UCHAR   AspectRatioIdc;
    UCHAR   VideoFormat;
    UCHAR   ColourPrimaries;
    UCHAR   TransferCharacteristics;
    UCHAR   MatrixCoefficients;
    UCHAR   ChromaSampleLocTypeTopField;
    UCHAR   ChromaSampleLocTypeBottomField;
    UCHAR   MaxBytesPerPicDenom;
    UCHAR   MaxBitsPerMbDenom;
    UCHAR   Log2MaxMvLength_horizontal;
    UCHAR   Log2MaxMvLength_vertical;
    UCHAR   NumReorderFrames;
    UINT    NumUnitsInTick;
    UINT    TimeScale;
    UCHAR   MaxDecFrameBuffering;

    // HRD parameters
    UCHAR   CpbCnt_minus1;
    UCHAR   BitRate_scale;
    UCHAR   CpbSize_scale;
    UINT    BitRateValueMinus1[32];
    UINT    CpbSizeValueMinus1[32];
    UINT    CbrFlag; // bit 0 represent SchedSelIdx 0 and so on
    UCHAR   InitialCpbRemovalDelayLengthMinus1;
    UCHAR   CpbRemovalDelayLengthMinus1;
    UCHAR   DpbOutputDelayLengthMinus1;
    UCHAR   TimeOffsetLength;

} ENCODE_SET_VUI_PARAMETER;
#else
typedef struct tagENCODE_SET_VUI_PARAMETER
{
    UINT    AspectRatioInfoPresentFlag      : 1;
    UINT    OverscanInfoPresentFlag         : 1;
    UINT    OverscanAppropriateFlag         : 1;
    UINT    VideoSignalTypePresentFlag      : 1;
    UINT    VideoFullRangeFlag              : 1;
    UINT    ColourDescriptionPresentFlag    : 1;
    UINT    ChromaLocInfoPresentFlag        : 1;
    UINT    TimingInfoPresentFlag           : 1;
    UINT    FixedFrameRateFlag              : 1;
    UINT    NalHrdParametersPresentFlag     : 1;
    UINT    VclHrdParametersPresentFlag     : 1;
    UINT    LowDelayHrdFlag                 : 1;
    UINT    PicStructPresentFlag            : 1;
    UINT    CbrFlag                         : 1;
    UINT                                    : 18;
    UCHAR   AspectRatioIdc;
    USHORT  SarWidth;
    USHORT  SarHeight;
    UCHAR   VideoFormat;
    UCHAR   ColourPrimaries;
    UCHAR   TransferCharacteristics;
    UCHAR   MatrixCoefficients;
    UCHAR   ChromaSampleLocTypeTopField;
    UCHAR   ChromaSampleLocTypeBottomField;
    UINT    NumUnitsInTick;
    UINT    TimeScale;

    // HRD parameters assume cpb_cnt == 0
    UCHAR   InitialCpbRemovalDelayLengthMinus1;
    UCHAR   CpbRemovalDelayLengthMinus1;
    UCHAR   DpbOutputDelayLengthMinus1;
    UCHAR   TimeOffsetLength;
    UCHAR   BitRateValueMinus1;
    UCHAR   CpbSizeValueMinus1;
} ENCODE_SET_VUI_PARAMETER;
#endif

// statuses returned by ENCODE_QUERY_STATUS_ID
enum
{
    ENCODE_OK           = 0,
    ENCODE_NOTREADY     = 1,
    ENCODE_NOTAVAILABLE = 2,
    ENCODE_ERROR        = 3
};

enum
{
    DDI_PROGRESSIVE_PICTURE = 0,
    DDI_TOP_FIELD           = 1,
    DDI_BOTTOM_FIELD        = 2,
    DDI_MBAFF_PICTURE       = 3
};


typedef struct tagFASTCOPY_QUERYCAPS
{
    FASTCOPY_QUERYTYPE Type;

    union
    {
        HANDLE  hRegistration;
    };

} FASTCOPY_QUERYCAPS;

// Fast Video Compositing Device for Blu-Ray
// {7DD2D599-41DB-4456-9395-8B3D18CEDCAE}
static const GUID DXVA2_FastCompositing = 
{ 0x7dd2d599, 0x41db, 0x4456, { 0x93, 0x95, 0x8b, 0x3d, 0x18, 0xce, 0xdc, 0xae } };

/*---------------------------------------------\
|  SURFACE REGISTRATION STRUCTURES AND ENUMS   |
\---------------------------------------------*/

// Surface registration entry
typedef struct _DXVA2_SAMPLE_REG
{
    REGISTRATION_OP     Operation;
    IDirect3DSurface9   *pD3DSurface;
} DXVA2_SAMPLE_REG, *PDXVA2_SAMPLE_REG;

// Surface registration request
typedef struct _DXVA2_SURFACE_REGISTRATION
{
    HANDLE              RegHandle;
    DXVA2_SAMPLE_REG    *pRenderTargetRequest;
    UINT                nSamples;
    DXVA2_SAMPLE_REG    *pSamplesRequest;
} DXVA2_SURFACE_REGISTRATION, *PDXVA2_SURFACE_REGISTRATION;

// Structures use 64-bit alignment
#pragma pack(push, 8)

// Fast Compositing modes
typedef enum _FASTCOMP_MODE
{
    FASTCOMP_MODE_COMPOSITING = 0,
    FASTCOMP_MODE_PRE_PROCESS = 1
} FASTCOMP_MODE;


/*-----------------------------------------\
|  FAST COMPOSITING STRUCTURES AND ENUMS   |
\-----------------------------------------*/

// Fast Compositing DDI Function ID
enum
{
    FASTCOMP_BLT                     = 0x100,
    FASTCOMP_QUERY_SCENE_DETECTION   = 0x200,
    FASTCOMP_QUERY_STATUS_FUNC_ID    = 0x300, // aya: tmp solution
    FASTCOMP_QUERY_VARIANCE          = 0x400
};

#define FASTCOMP_DEPTH_BACKGROUND  -1
#define FASTCOMP_DEPTH_MAIN_VIDEO   0
#define FASTCOMP_DEPTH_SUB_VIDEO    1
#define FASTCOMP_DEPTH_SUBSTREAM    2
#define FASTCOMP_DEPTH_GRAPHICS     3

// Query Caps Types
typedef enum tagFASTCOMP_QUERYTYPE
{
    FASTCOMP_QUERY_CAPS                = 1,
    FASTCOMP_QUERY_FORMAT_COUNT        = 2,
    FASTCOMP_QUERY_FORMATS             = 3,
    FASTCOMP_QUERY_REGISTRATION_HANDLE = 4,
    FASTCOMP_QUERY_FRAME_RATE          = 5,
    FASTCOMP_QUERY_CAPS2               = 6,
    FASTCOMP_QUERY_VPP_CAPS            = 7,
    FASTCOMP_QUERY_VPP_CAPS2           = 8,
    FASTCOMP_QUERY_VPP_FORMAT_COUNT    = 9,
    FASTCOMP_QUERY_VPP_FORMATS         = 10,
    FASTCOMP_QUERY_FRC_CAPS            = 11,
    FASTCOMP_QUERY_VPP_VARIANCE_CAPS   = 12
} FASTCOMP_QUERYTYPE;

typedef enum _FASTCOMP_TARGET_MODE
{
    FASTCOMP_TARGET_PROGRESSIVE        = 0,
    FASTCOMP_TARGET_NO_DEINTERLACING   = 1
/*
    FASTCOMP_TARGET_INTERLACED_TFF     = 2,
    FASTCOMP_TARGET_INTERLACED_BFF     = 3
*/
} FASTCOMP_TARGET_MODE;

#define FASTCOMP_TARGET_PROGRESSIVE_MASK        (0x01 << FASTCOMP_TARGET_PROGRESSIVE)
#define FASTCOMP_TARGET_NO_DEINTERLACING_MASK   (0x01 << FASTCOMP_TARGET_NO_DEINTERLACING)

// Source Caps
typedef struct tagFASTCOMP_SRC_CAPS
{
    INT     iDepth;                   // Depth of the source plane

    UINT    bSimpleDI           :1;   // Simple DI (BOB) is available
    UINT    bAdvancedDI         :1;   // Advanced DI is available
    UINT    bProcAmp            :1;   // ProcAmp (color control) is available
    UINT    bInverseTelecine    :1;   // Inverse telecine is available
    UINT    bDenoiseFilter      :1;   // Denoise filter is available
    UINT    bInverseTelecineTS  :1;   // Inverse telecine timestamping is available
    UINT                        :3;   // reserved
    UINT    bInterlacedScaling  :1;   // Interlaced Scaling is supported
    UINT                        :22;  // Reserved

    UINT    bSourceBlending     :1;   // Source blending is available  D = A * S + (1-A) * RT
    UINT    bPartialBlending    :1;   // Partial blending is available D = S     + (1-A) * RT
    UINT                        :30;  // Reserved

    UINT    bConstantBlending   :1;   // Constant blending is available
    UINT                        :31;  // Reserved

    UINT    bLumaKeying         :1;   // Luma keying is available
    UINT    bExtendedGamut      :1;   // Source may be extended gamut xvYCC
    UINT                        :30;  // Reserved

    INT     iMaxWidth;                // Max source width
    INT     iMaxHeight;               // Max source height
    INT     iNumBackwardSamples;      // Backward Samples
    INT     iNumForwardSamples;       // Forward Samples
} FASTCOMP_SRC_CAPS;

// Destination Caps
typedef struct tagFASTCOMP_DST_CAPS
{
    INT     iMaxWidth;
    INT     iMaxHeight;
} FASTCOMP_DST_CAPS;

// Fast Compositing Source/Destination Caps
typedef struct tagFASTCOMP_CAPS
{
    INT                 iReseved;
    INT                 iMaxSubstreams;
    FASTCOMP_SRC_CAPS   sPrimaryVideoCaps;
    FASTCOMP_SRC_CAPS   sSecondaryVideoCaps;
    FASTCOMP_SRC_CAPS   sSubstreamCaps;
    FASTCOMP_SRC_CAPS   sGraphicsCaps;
    FASTCOMP_DST_CAPS   sRenderTargetCaps;
} FASTCOMP_CAPS;

// Fast Compositing Extra Caps
typedef struct tagFASTCOMP_CAPS2
{
    UINT            dwSize;
    UINT            TargetInterlacingModes  : 8;
    UINT            bTargetInSysMemory      : 1;
    UINT            bProcampControl         : 1;
    UINT            bDeinterlacingControl   : 1;
    UINT            bDenoiseControl         : 1;
    UINT            bDetailControl          : 1;
    UINT            bFmdControl             : 1;
    UINT            bVariances              : 1;
    UINT            Reserved1               : 1;
    UINT            bFieldWeavingControl    : 1;
    UINT            bISControl              : 1;
    UINT            bFRCControl             : 1;
    UINT            bScalingModeControl     : 1;
    UINT            Reserved2               : 12;
} FASTCOMP_CAPS2;

// Fast Compositing Sample Formats (User output)
typedef struct _FASTCOMP_SAMPLE_FORMATS
{
    INT         iPrimaryVideoFormatCount;   // Primary Video
    INT         iSecondaryVideoFormatCount;
    INT         iSubstreamFormatCount;     // Presentation Graphics (BD)
    INT         iGraphicsFormatCount;      // Interactive Graphics (BD)
    INT         iRenderTargetFormatCount;
    INT         iBackgroundFormatCount;
    D3DFORMAT   *pPrimaryVideoFormats;
    D3DFORMAT   *pSecondaryVideoFormats;
    D3DFORMAT   *pSubstreamFormats;         // Presentation Graphics (BD)
    D3DFORMAT   *pGraphicsFormats;          // Interactive Graphics (BD)
    D3DFORMAT   *pRenderTargetFormats;
    D3DFORMAT   *pBackgroundFormats;
} FASTCOMP_SAMPLE_FORMATS;

// Fast Composition device creation parameters
typedef struct _FASTCOMP_CREATEDEVICE
{
    DXVA2_VideoDesc      *pVideoDesc;            // Main video parameters
    D3DFORMAT            TargetFormat;           // Render Target Format
    UINT                 iSubstreams     : 8;    // Max number of substreams to use
    UINT                 Reserved        : 24;    // Must be zero
} FASTCOMP_CREATEDEVICE;

// Fast Composition video sample
typedef struct _FASTCOMP_VideoSample
{
    REFERENCE_TIME          Start;
    REFERENCE_TIME          End;
    DXVA2_ExtendedFormat    SampleFormat;
    IDirect3DSurface9       *SrcSurface;
    RECT                    SrcRect;
    RECT                    DstRect;
    DXVA2_AYUVSample8       Palette[256];
    FLOAT                   Alpha;
    INT                     Depth;
    UINT                    bLumaKey         : 1;
    UINT                    bPartialBlending : 1;
    UINT                    bExtendedGamut   : 1;
    UINT                    uReserved        : 29;
    UINT                    iLumaLow;
    UINT                    iLumaHigh;
} FASTCOMP_VideoSample;

typedef enum tagFASTCOMP_PARAMS_TYPE
{
    FASTCOMP_RESERVED = 1,
    FASTCOMP_QUERY_STATUS_V1_0,
    FASTCOMP_FEATURES_IS_V1_0,
    FASTCOMP_FEATURES_FRC_V1_0,
    FASTCOMP_FEATURES_SCALINGMODE_V1_0
} FASTCOMP_PARAMS_TYPE;

typedef struct  tagFASTCOMP_BLT_PARAMS_OBJECT
{
    // pointer to the passed in parameters
    PVOID                pParams;
    FASTCOMP_PARAMS_TYPE type;
    UINT                 iSizeofParams;

} FASTCOMP_BLT_PARAMS_OBJECT;

typedef struct tagFASTCOMP_QUERY_STATUS_PARAMS_V1_0
{
    UINT iQueryStatusID;
    UINT Reserved;

} FASTCOMP_QUERY_STATUS_PARAMS_V1_0;


// Fast Compositing Blt request
typedef struct _FASTCOMP_BLT_PARAMS
{
    IDirect3DSurface9       *pRenderTarget;
    UINT                    SampleCount;
    FASTCOMP_VideoSample    *pSamples;
    REFERENCE_TIME          TargetFrame;
    RECT                    TargetRect;
    UINT                    Reserved1               : 1;    
    UINT                    TargetTransferMatrix    : 3;    // Render Target Matrix
    UINT                    bTargetExtendedGamut    : 1;    // Render Target Extended Gamut
    UINT                    iTargetInterlacingMode  : 3;    // Render Target Interlacing Mode
    UINT                    bTargetInSysMemory      : 1;    // Render Target In System Memory
    UINT                    Reserved2               : 23;   // MBZ
    UINT                    iSizeOfStructure        : 16;   // Size of the structure
    UINT                    Reserved3               : 16;
    void                    *pReserved1;
    DXVA2_AYUVSample16      BackgroundColor;
    void                    *pReserved2;                    // MUST BE NULL

    // Rev 1.4 parameters
    DXVA2_ProcAmpValues     ProcAmpValues;                  // Procamp parameters
    UINT                    bDenoiseAutoAdjust      : 1;    // Denoise Auto Adjust
    UINT                    bFMDEnable              : 1;    // Enable FMD
    UINT                    Reserved4               : 1;    // reserved
    UINT                    iDeinterlacingAlgorithm : 4;    // DI algorithm
    UINT                    bFieldWeaving           : 1;
    UINT                    bVarianceQuery          : 1;
    UINT                    bTargetYuvFullRange     : 1;
    UINT                    Reserved5               : 22;   // MBZ
    WORD                    wDenoiseFactor;                 // Denoise factor (same as CUI 2.75)
    WORD                    wDetailFactor;                  // Detail factor (same as CUI 2.75)
    UINT                    Reserved6;
    UINT                    Reserved7;
    VOID                    *pReserved3;                    // [OUT] Array of variances
    UINT                    iCadence                : 12;   // [OUT] Cadence type
    UINT                    bRepeatFrame            : 1;    // [OUT] Repeat frame flag
    UINT                    Reserved8               : 19;
    UINT                    Reserved;

    // Rev 1.5 parameters
    FASTCOMP_BLT_PARAMS_OBJECT         QueryStatusObject;    
    FASTCOMP_BLT_PARAMS_OBJECT         ISObject;
    FASTCOMP_BLT_PARAMS_OBJECT         FRCObject;

    // Rev 1.5.2 parameters
    FASTCOMP_BLT_PARAMS_OBJECT    ScalingModeObject;
} FASTCOMP_BLT_PARAMS;


typedef struct tagFASTCOMP_QUERY_STATUS
{
    UINT QueryStatusID;
    UINT Status : 8;
    UINT : 24; // reserved

    UINT Reserved1;
    UINT Reserved2;
    UINT Reserved3;
    UINT Reserved4;

} FASTCOMP_QUERY_STATUS;


// Fast Compositing Query Format Count (DDI)
typedef struct tagFASTCOMP_FORMAT_COUNT
{
    INT          iPrimaryFormats;               // Main video
    INT          iSecondaryFormats;             // Sub-video
    INT          iSubstreamFormats;             // Presentation Graphics (BD)
    INT          iGraphicsFormats;              // Interactive Graphics (BD)
    INT          iRenderTargetFormats;          // Render Target
    INT          iBackgroundFormats;            // Background layer
} FASTCOMP_FORMAT_COUNT;


// Fast Compositing Query Formats (DDI)
typedef struct tagFASTCOMP_FORMATS
{
    INT          iPrimaryFormatSize;            // Main video
    INT          iSecondaryFormatSize;          // Sub-video
    INT          iSubstreamFormatSize;          // Presentation Graphics (BD)
    INT          iGraphicsFormatSize;           // Interactive Graphics (BD)
    INT          iRenderTargetFormatSize;       // Render Target
    INT          iBackgroundFormatSize;         // Background layer
    D3DFORMAT *  pPrimaryFormats;
    D3DFORMAT *  pSecondaryFormats;
    D3DFORMAT *  pSubstreamFormats;
    D3DFORMAT *  pGraphicsFormats;
    D3DFORMAT *  pRenderTargetFormats;
    D3DFORMAT *  pBackgroundFormats;
} FASTCOMP_FORMATS;


// Fast Compositing Query Frame Rate
typedef struct tagFASTCOMP_FRAME_RATE
{
    INT          iMaxSrcWidth;       // [in]
    INT          iMaxSrcHeight;      // [in]
    INT          iMaxDstWidth;       // [in]
    INT          iMaxDstHeight;      // [in]
    INT          iMaxLayers;         // [in]
    INT          iFrameRate;         // [out]
} FASTCOMP_FRAME_RATE;


typedef struct tagFASTCOMP_FRC_RATIONAL
{
    UINT    Numerator;
    UINT    Denominator;
} FASTCOMP_FRC_RATIONAL;


typedef struct tagFASTCOMP_FRC_CUSTOM_RATE_DATA
{
    FASTCOMP_FRC_RATIONAL   CustomRate;
    UINT                    OutputFrames;
    UINT                    InputInterlaced;
    UINT                    InputFramesOrFields;
    USHORT                  usNumBackwardSamples; 
    USHORT                  usNumForwardSamples; 
} FASTCOMP_FRC_CUSTOM_RATE_DATA;



typedef struct tagFASTCOMP_FRC_CAPS
{
    UINT                           iRateConversionCount;
    FASTCOMP_FRC_CUSTOM_RATE_DATA  sCustomRateData[6];
} FASTCOMP_FRC_CAPS;


typedef enum tagFASTCOMP_ISTAB_MODE
{
    ISTAB_MODE_NONE = 0,
    ISTAB_MODE_BLACKEN,
    ISTAB_MODE_UPSCALE
} FASTCOMP_ISTAB_MODE;


typedef struct tagFASTCOMP_IS_PARAMS_V1_0
{
    FASTCOMP_ISTAB_MODE IStabMode;
    UINT                Reserved1;
    UINT                Reserved2;

} FASTCOMP_IS_PARAMS_V1_0;


typedef struct tagFASTCOMP_FRC_PARAMS_V1_0
{
    UINT                    RepeatFrame;
    FASTCOMP_FRC_RATIONAL   CustomRate;
    UINT                    OutputIndex;
    UINT                    InputFrameOrField;
} FASTCOMP_FRC_PARAMS_V1_0;


typedef struct tagFASTCOMP_SCALINGMODE_PARAMS_V1_0
{
    BOOL    FastMode;
} FASTCOMP_SCALINGMODE_PARAMS_V1_0;


typedef struct tagFASTCOMP_FRAMERATE
{
    int iMaxSrcWidth;
    int iMaxSrcHeight;
    int iMaxDstWidth;
    int iMaxDstHeight;
    int iMaxLayers;
    int iFrameRate;

} FASTCOMP_FRAMERATE;

typedef enum tagVARIANCE_TYPE
{
    Type_1  = 0,
} VARIANCE_TYPE;


typedef struct tagFASTCOMP_VARIANCE_CAPS
{
    VARIANCE_TYPE    Type;              // [OUT]
    UINT             VarianceSize;      // [OUT]
    UINT             VarianceCount;     //  [OUT]
} FASTCOMP_VARIANCE_CAPS;
// Fast Composition Query Caps union

typedef struct tagFASTCOMP_QUERYCAPS
{
    FASTCOMP_QUERYTYPE    Type;
    union
    {
        FASTCOMP_CAPS           sCaps;
        FASTCOMP_CAPS2          sCaps2;
        FASTCOMP_FORMAT_COUNT   sFmtCount;
        FASTCOMP_FORMATS        sFormats;
        HANDLE                  hRegistration;
        FASTCOMP_FRAMERATE      sRate;
        FASTCOMP_FRC_CAPS       sFrcCaps;
        FASTCOMP_VARIANCE_CAPS  sVarianceCaps;
    };
} FASTCOMP_QUERYCAPS;

typedef struct tagFASTCOMP_QUERY_VARIANCE_PARAMS
{
    UINT    FrameNumber;
    VOID    *pVariances;
    UINT    VarianceBufferSize;
} FASTCOMP_QUERY_VARIANCE_PARAMS;

#pragma pack(pop)

#endif /* defined(_WIN32) || defined(_WIN64) */
#endif //__ENCODING_DDI_H__
