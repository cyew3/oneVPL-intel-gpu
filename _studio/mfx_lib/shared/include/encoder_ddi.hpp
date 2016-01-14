/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_platform_headers.h"

#ifndef _ENCODER_DDI_HPP
#define _ENCODER_DDI_HPP

#if defined(_WIN32) || defined(_WIN64)

#define DDI_086

//#include "d3dumddi.h"

// structure to pass surface list to LRB Emulation DLL for Windows
typedef struct
{
    D3DDDIFORMAT        type;
    UINT                num_surf;
    IDirect3DSurface9*  surface[128];
} EmulSurfaceRegister;

/* EncoderDevice device functions */

#define D3DDDIFMT_INTELENCODE_SPSDATA           (D3DDDIFORMAT)160 // D3DDDIFMT_DXVA_RESERVED10
#define D3DDDIFMT_INTELENCODE_PPSDATA           (D3DDDIFORMAT)161 // D3DDDIFMT_DXVA_RESERVED11
#define D3DDDIFMT_INTELENCODE_SLICEDATA         (D3DDDIFORMAT)162 // D3DDDIFMT_DXVA_RESERVED12
#define D3DDDIFMT_INTELENCODE_QUANTDATA         (D3DDDIFORMAT)163 // D3DDDIFMT_DXVA_RESERVED13
#define D3DDDIFMT_INTELENCODE_BITSTREAMDATA     (D3DDDIFORMAT)164 // D3DDDIFMT_DXVA_RESERVED14
#define D3DDDIFMT_INTELENCODE_MBDATA            (D3DDDIFORMAT)165 // D3DDDIFMT_DXVA_RESERVED15
#define D3DDDIFMT_INTELENCODE_SEIDATA           (D3DDDIFORMAT)166 // D3DDDIFMT_DXVA_RESERVED16
#define D3DDDIFMT_INTELENCODE_VUIDATA           (D3DDDIFORMAT)167 // D3DDDIFMT_DXVA_RESERVED17
#define D3DDDIFMT_INTELENCODE_VMESTATE          (D3DDDIFORMAT)169 // D3DDDIFMT_DXVA_RESERVED19
#define D3DDDIFMT_INTELENCODE_VMEINPUT          (D3DDDIFORMAT)170 // D3DDDIFMT_DXVA_RESERVED20
#define D3DDDIFMT_INTELENCODE_VMEOUTPUT         (D3DDDIFORMAT)171 // D3DDDIFMT_DXVA_RESERVED21
#define D3DDDIFMT_INTELENCODE_EFEDATA           (D3DDDIFORMAT)172 // D3DDDIFMT_DXVA_RESERVED22
#define D3DDDIFMT_INTELENCODE_STATDATA          (D3DDDIFORMAT)173 // D3DDDIFMT_DXVA_RESERVED23
#define D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA  (D3DDDIFORMAT)178 // D3DDDIFMT_DXVA_RESERVED28
#define D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA   (D3DDDIFORMAT)179 // D3DDDIFMT_DXVA_RESERVED29

//from "Intel_VP8_Encoding_DDI_v0.2"
#define D3DDDIFMT_INTELENCODE_SEGMENTMAP        (D3DDDIFORMAT)178
#define D3DDDIFMT_INTELENCODE_COEFFPROB         (D3DDDIFORMAT)179
#define D3DDDIFMT_INTELENCODE_DISTORTIONDATA    (D3DDDIFORMAT)180 

// From "Intel_JPEG_Encoding_DDI_Rev0.103"
#define D3DDDIFMT_INTEL_JPEGENCODE_PPSDATA    (D3DDDIFORMAT)161 // D3DDDIFMT_DXVA_RESERVED11
#define D3DDDIFMT_INTEL_JPEGENCODE_QUANTDATA  (D3DDDIFORMAT)163 // D3DDDIFMT_DXVA_RESERVED12
#define D3DDDIFMT_INTEL_JPEGENCODE_HUFFTBLDATA (D3DDDIFORMAT)182 // D3DDDIFMT_DXVA_RESERVED13
#define D3DDDIFMT_INTEL_JPEGENCODE_SCANDATA   (D3DDDIFORMAT)162 // D3DDDIFMT_DXVA_RESERVED15
#define D3DDDIFMT_INTELENCODE_PAYLOADDATA     (D3DDDIFORMAT)174 // D3DDDIFMT_DXVA_RESERVED24

//from DDI 0.921 per MB QP
#define D3DDDIFMT_INTELENCODE_MBQPDATA          (D3DDDIFORMAT)183

// Decode Extension Functions for DXVA11 Encode
#define ENCODE_QUERY_ACCEL_CAPS_ID 0x110
#define ENCODE_ENCRYPTION_SET_ID 0x111
#define ENCODE_QUERY_MAX_MB_PER_SEC_ID 0x112

enum D3D11_DDI_VIDEO_ENCODER_BUFFER_TYPE
{
    D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA          = 0,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA          = 1,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA        = 2,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_QUANTDATA        = 3,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA    = 4,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_MBDATA           = 5,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_SEIDATA          = 6,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_VUIDATA          = 7,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_VMESTATE         = 9,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_VMEINPUT         = 10,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_VMEOUTPUT        = 11,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_EFEDATA          = 12,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_STATDATA         = 13,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_PAYLOADDATA      = 14,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_PAYLOADOUTPUT    = 15,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA = 16,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDSLICEDATA  = 17,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_HUFFTBLDATA      = 22,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_MBQPDATA         = 23,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_COEFFPROB        = 30,
    D3D11_DDI_VIDEO_ENCODER_BUFFER_DISTORTIONDATA   = 31
};

typedef struct tagENCODE_QUERY_PROCESSING_RATE_INPUT
{
    UCHAR       Profile;
    UCHAR       Level;
    UCHAR       TargetUsage;
    UCHAR       GopRefDist;
    USHORT      GopPicSize;
} ENCODE_QUERY_PROCESSING_RATE_INPUT;


typedef struct tagENCODE_FORMAT_COUNT
{
    UCHAR    UncompressedFormatCount;
    UCHAR    CompressedBufferInfoCount;
} ENCODE_FORMAT_COUNT;


typedef struct tagENCODE_COMP_BUFFER_INFO
{
    D3DFORMAT    Type;
    D3DFORMAT   CompressedFormats;
    USHORT        CreationWidth;
    USHORT        CreationHeight;
    UCHAR        NumBuffer;
    UCHAR        AllocationType;
} ENCODE_COMP_BUFFER_INFO;

typedef struct tagENCODE_FORMATS
{
    int                        UncompressedFormatSize;
    int                        CompressedBufferInfoSize;
    D3DFORMAT                  *pUncompressedFormats;
    ENCODE_COMP_BUFFER_INFO    *pCompressedBufferInfo;
} ENCODE_FORMATS;

typedef enum tagENCODE_FUNC
{
    ENCODE_ENC        = 0x0001,
    ENCODE_PAK        = 0x0002,
    ENCODE_ENC_PAK    = 0x0004,
    ENCODE_HybridPAK  = 0x0008,
    ENCODE_WIDI       = 0x8000
} ENCODE_FUNC;

typedef struct tagENCODE_UNCOMPRESSED_BUFFER_TYPE
{
    UCHAR    UncompressedBufferType;
    UCHAR    NumUncompressedBuffer;
} ENCODE_UNCOMPRESSED_BUFFER_TYPE;

typedef struct tagENCODE_INPUT_DESC
{
    UINT    IndexOriginal;
    UINT    ArraSliceOriginal;
    UINT    IndexRecon;
    UINT    ArraySliceRecon;
} ENCODE_INPUT_DESC;

typedef enum tagREF_FRAME_TYPE
{
    ENCODE_UNCOMPRESSED_BUFFER_RAW        = 1,
    ENCODE_UNCOMPRESSED_BUFFER_RECON    = 2
} REF_FRAME_TYPE;

#ifndef PAVP_ENCRYPTION_TYPE_AND_COUNTER_DEFINES
#define PAVP_ENCRYPTION_TYPE_AND_COUNTER_DEFINES

typedef struct tagPAVP_ENCRYPTION_MODE
{
    UINT    eEncryptionType;
    UINT    eCounterMode;
} PAVP_ENCRYPTION_MODE;

typedef struct tagENCODE_CREATEDEVICE
{
    DXVADDI_VIDEODESC *     pVideoDesc;
    USHORT                  CodingFunction;
    GUID                    EncryptionMode;
    UINT                    CounterAutoIncrement;
    D3DAES_CTR_IV *         pInitialCipherCounter;
    PAVP_ENCRYPTION_MODE *  pPavpEncryptionMode;
} ENCODE_CREATEDEVICE;

#if defined  (MFX_D3D11_ENABLED)
typedef struct tagENCODE_ENCRYPTION_SET
{
    UINT                    CounterAutoIncrement;
    D3D11_AES_CTR_IV        *pInitialCipherCounter;
    PAVP_ENCRYPTION_MODE    *pPavpEncryptionMode;
} ENCODE_ENCRYPTION_SET;
#endif

typedef struct tagENCODE_COMPBUFFERDESC
{
    void*        pCompBuffer;
    D3DFORMAT    CompressedBufferType;
    UINT        DataOffset;
    UINT        DataSize;
    void*        pReserved;
} ENCODE_COMPBUFFERDESC;

//#ifndef PAVP_ENCRYPTION_TYPE_AND_COUNTER_DEFINES
//#define PAVP_ENCRYPTION_TYPE_AND_COUNTER_DEFINES

    // Available encryption types
enum
{
    PAVP_ENCRYPTION_NONE        = 1,
    PAVP_ENCRYPTION_AES128_CTR  = 2,
    PAVP_ENCRYPTION_AES128_CBC  = 4
};

    // Available counter types
enum
{
    PAVP_COUNTER_TYPE_A = 1,
    PAVP_COUNTER_TYPE_B = 2,
    PAVP_COUNTER_TYPE_C = 4
};


typedef struct tagENCODE_EXECUTE_PARAMS
{
    UINT                    NumCompBuffers;
    ENCODE_COMPBUFFERDESC * pCompressedBuffers;
    D3DAES_CTR_IV *         pCipherCounter;
    PAVP_ENCRYPTION_MODE    PavpEncryptionMode;

} ENCODE_EXECUTE_PARAMS;

#endif // PAVP_ENCRYPTION_TYPE_AND_COUNTER_DEFINES
enum
{
    CODEC_TYPE_H264 = 0,
    CODEC_TYPE_MPEG2,
};

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define frame encoding type
///////////////////////////////////////////////////////////////////////////////
enum
{
    CODING_TYPE_I   = 1,
    CODING_TYPE_P   = 2,
    CODING_TYPE_B   = 3,
};

enum
{
    SLICE_TYPE_P = 0,
    SLICE_TYPE_B = 1,
    SLICE_TYPE_I = 2
};

enum
{
    DDI_RATECONTROL_CBR = 1,
    DDI_RATECONTROL_VBR = 2,
    DDI_RATECONTROL_CQP = 3
};

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define Intra MB mode
///////////////////////////////////////////////////////////////////////////////
enum    //for IntraMbMode Field
{
    INTRA_MB_MODE_16x16 = 0,
    INTRA_MB_MODE_8x8   = 1,
    INTRA_MB_MODE_4x4   = 2,
    INTRA_MB_MODE_IPCM  = 3,
};

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define Intra MB Type
///////////////////////////////////////////////////////////////////////////////
enum    //Intra MbType
{
    MBTYPE_I_4x4            = 0,
    MBTYPE_I_8x8            = 0,
    MBTYPE_I_16x16_000      = 1,
    MBTYPE_I_16x16_100      = 2,
    MBTYPE_I_16x16_200      = 3,
    MBTYPE_I_16x16_300      = 4,
    MBTYPE_I_16x16_010      = 5,
    MBTYPE_I_16x16_110      = 6,
    MBTYPE_I_16x16_210      = 7,
    MBTYPE_I_16x16_310      = 8,
    MBTYPE_I_16x16_020      = 0,
    MBTYPE_I_16x16_120      = 0xA,
    MBTYPE_I_16x16_220      = 0xB,
    MBTYPE_I_16x16_320      = 0xC,
    MBTYPE_I_16x16_001      = 0xD,
    MBTYPE_I_16x16_101      = 0xE,
    MBTYPE_I_16x16_201      = 0xF,
    MBTYPE_I_16x16_301      = 0x10,
    MBTYPE_I_16x16_011      = 0x11,
    MBTYPE_I_16x16_111      = 0x12,
    MBTYPE_I_16x16_211      = 0x13,
    MBTYPE_I_16x16_311      = 0x14,
    MBTYPE_I_16x16_021      = 0x15,
    MBTYPE_I_16x16_121      = 0x16,
    MBTYPE_I_16x16_221      = 0x17,
    MBTYPE_I_16x16_321      = 0x18,
    MBTYPE_I_IPCM           = 0x19,
};

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define Inter MB Type
///////////////////////////////////////////////////////////////////////////////
enum    //Inter MbType
{
    MBTYPE_BP_L0_16x16      = 1,
    MBTYPE_B_L1_16x16       = 2,
    MBTYPE_B_Bi_16x16       = 3,
    MBTYPE_BP_L0_L0_16x8    = 4,
    MBTYPE_BP_L0_L0_8x16    = 5,
    MBTYPE_B_L1_L1_16x8     = 6,
    MBTYPE_B_L1_L1_8x16     = 7,
    MBTYPE_B_L0_L1_16x8     = 8,
    MBTYPE_B_L0_L1_8x16     = 9,
    MBTYPE_B_L1_L0_16x8     = 0xA,
    MBTYPE_B_L1_L0_8x16     = 0xB,
    MBTYPE_B_L0_Bi_16x8     = 0xC,
    MBTYPE_B_L0_Bi_8x16     = 0xD,
    MBTYPE_B_L1_Bi_16x8     = 0xE,
    MBTYPE_B_L1_Bi_8x16     = 0xF,
    MBTYPE_B_Bi_L0_16x8     = 0x10,
    MBTYPE_B_Bi_L0_8x16     = 0x11,
    MBTYPE_B_Bi_L1_16x8     = 0x12,
    MBTYPE_B_Bi_L1_8x16     = 0x13,
    MBTYPE_B_Bi_Bi_16x8     = 0x14,
    MBTYPE_B_Bi_Bi_8x16     = 0x15,
    MBTYPE_BP_8x8           = 0x16,
};
////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define MB Type for MPEG2
///////////////////////////////////////////////////////////////////////////////
enum
{
    MBTYPE_I                    = 0x1A,
    MBTYPE_P_FWD_SKIP           = 0x01,
    MBTYPE_B_BWD_SKIP           = 0x02,
    MBTYPE_B_Bi_SKIP            = 0x03,
    MBTYPE_P_0MV                = 0x01,
    MBTYPE_P_FRM_FRM            = 0x01,
    MBTYPE_P_FRM_FLD            = 0x04,
    MBTYPE_P_FRM_DUALPRIME      = 0x19,
    MBTYPE_P_FLD_1_16x16        = 0x01,
    MBTYPE_P_FLD_2_16x8         = 0x04,
    MBTYPE_P_FLD_DUALPRIME      = 0x19,
    MBTYPE_B_FRM_FRM_FWD        = 0x01,
    MBTYPE_B_FRM_FRM_BWD        = 0x02,
    MBTYPE_B_FRM_FRM_Bi         = 0x03,
    MBTYPE_B_FRM_FLD_FWD        = 0x04,
    MBTYPE_B_FRM_FLD_BWD        = 0x03,
    MBTYPE_B_FRM_FLD_Bi         = 0x14,
    MBTYPE_B_FLD_1_16x16_FWD    = 0x01,
    MBTYPE_B_FLD_1_16x16_BWD    = 0x02,
    MBTYPE_B_FLD_1_16x16_Bi     = 0x03,
    MBTYPE_B_FLD_2_16x8_FWD     = 0x04,
    MBTYPE_B_FLD_2_16x8_BWD     = 0x06,
    MBTYPE_B_FLD_2_16x8_Bi      = 0x14,
};


////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define MV packed format
///////////////////////////////////////////////////////////////////////////////
enum
{
    MV_FORMAT_NO_MV         = 0,
    MV_FORMAT_1_MV          = 1,
    MV_FORMAT_2_MV          = 2,
    MV_FORMAT_4_MV          = 3,
    MV_FORMAT_8_MV          = 4,
    MV_FORMAT_16_MV         = 5,
    MV_FORMAT_32_MV         = 6,
    MV_FORMAT_PACKED        = 7,
};

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define Intra 16x16 block intra prediction mode
///////////////////////////////////////////////////////////////////////////////
enum
{
    LUMA_INTRA_16x16_PRED_MODE_VERTICAL     = 0,
    LUMA_INTRA_16x16_PRED_MODE_HORIZONTAL,
    LUMA_INTRA_16x16_PRED_MODE_DC,
    LUMA_INTRA_16x16_PRED_MODE_PLANE,
};

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define intra 8x8 block intra prediction mode
///////////////////////////////////////////////////////////////////////////////
enum
{
    LUMA_INTRA_8x8_PRED_MODE_VERTICAL     = 0,
    LUMA_INTRA_8x8_PRED_MODE_HORIZONTAL,
    LUMA_INTRA_8x8_PRED_MODE_DC,
    LUMA_INTRA_8x8_PRED_MODE_DIAGONAL_DOWN_LEFT,
    LUMA_INTRA_8x8_PRED_MODE_DIAGONAL_DOWN_RIGHT,
    LUMA_INTRA_8x8_PRED_MODE_VERTICAL_RIGHT,
    LUMA_INTRA_8x8_PRED_MODE_HORIZONTAL_DOWN,
    LUMA_INTRA_8x8_PRED_MODE_VERTICAL_LEFT,
    LUMA_INTRA_8x8_PRED_MODE_HORIZONTAL_UP,
};

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define intra 4x4 block intra prediction mode
///////////////////////////////////////////////////////////////////////////////
enum
{
    LUMA_INTRA_4x4_PRED_MODE_VERTICAL     = 0,
    LUMA_INTRA_4x4_PRED_MODE_HORIZONTAL,
    LUMA_INTRA_4x4_PRED_MODE_DC,
    LUMA_INTRA_4x4_PRED_MODE_DIAGONAL_DOWN_LEFT,
    LUMA_INTRA_4x4_PRED_MODE_DIAGONAL_DOWN_RIGHT,
    LUMA_INTRA_4x4_PRED_MODE_VERTICAL_RIGHT,
    LUMA_INTRA_4x4_PRED_MODE_HORIZONTAL_DOWN,
    LUMA_INTRA_4x4_PRED_MODE_VERTICAL_LEFT,
    LUMA_INTRA_4x4_PRED_MODE_HORIZONTAL_UP,
};

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define chroma block intra prediction mode
///////////////////////////////////////////////////////////////////////////////
enum
{
    CHROMA_INTRA_PRED_MODE_DC           = 0,
    CHROMA_INTRA_PRED_MODE_HORIZONTAL,
    CHROMA_INTRA_PRED_MODE_VERTICAL,
    CHROMA_INTRA_PRED_MODE_PLANE,
};

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define Inter MB block size
///////////////////////////////////////////////////////////////////////////////
enum
{
    INTER_MB_MODE_16x16         = 0,
    INTER_MB_MODE_16x8          = 1,
    INTER_MB_MODE_8x16          = 2,
    INTER_MB_MODE_8x8           = 3,
};

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define inter MB sub partition shape in a 8x8 block
///////////////////////////////////////////////////////////////////////////////
enum
{
    SUB_MB_SHAPE_8x8    = 0,
    SUB_MB_SHAPE_8x4    = 1,
    SUB_MB_SHAPE_4x8    = 2,
    SUB_MB_SHAPE_4x4    = 3,
};

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define the Inter MB prediction type
///////////////////////////////////////////////////////////////////////////////
enum
{
    SUB_MB_PRED_MODE_L0     = 0,
    SUB_MB_PRED_MODE_L1     = 1,
    SUB_MB_PRED_MODE_Bi     = 2,
};




////////////////////////////////////////////////////////////////////////////////
// this structure is used to define the control structure for VME.
///////////////////////////////////////////////////////////////////////////////

typedef struct tagENCODE_CAPS
{
    union
    {
        struct
        {
            UINT    CodingLimitSet              : 1;
            UINT    NoFieldFrame                : 1;
            UINT    NoCabacSupport              : 1;
            UINT    NoGapInFrameCount           : 1;
            UINT    NoGapInPicOrder             : 1;
            UINT    NoUnpairedField             : 1;
            UINT    BitDepth8Only               : 1;
            UINT    ConsecutiveMBs              : 1;
            UINT    SliceStructure              : 3;
            UINT    SliceIPOnly                 : 1;
            UINT    SliceIPBOnly                : 1;
            UINT    NoWeightedPred              : 1;
            UINT    NoMinorMVs                  : 1;
            UINT    ClosedGopOnly               : 1;
            UINT    NoFrameCropping             : 1;
            UINT    FrameLevelRateCtrl          : 1;
            UINT    HeaderInsertion             : 1;
            UINT    RawReconRefToggle           : 1; // doesn't shpport change reference frame (original vs. recon) at a per frame basis.
            UINT    NoPAFF                      : 1; // doesn't support change frame/field coding at per frame basis.
            UINT    NoInterlacedField           : 1; // DDI 0.87
            UINT    BRCReset                    : 1;
            UINT    EnhancedEncInput            : 1;
            UINT    RollingIntraRefresh         : 1;
            UINT    UserMaxFrameSizeSupport     : 1;
            UINT    LayerLevelRateCtrl          : 1;
            UINT    SliceLevelRateCtrl          : 1;
            UINT    VCMBitrateControl           : 1;
            UINT    NoESS                       : 1;
            UINT    Color420Only                : 1;
            UINT    ICQBRCSupport               : 1;
        };
        UINT CodingLimits;
    };
    union
    {
        struct
        {
            USHORT EncFunc              : 1;
            USHORT PakFunc              : 1;
            USHORT EncodeFunc           : 1;  // Enc+Pak
            USHORT InputAnalysisFunc    : 1;  // Reserved for now
            USHORT reserved             :12;
        };
        USHORT CodingFunction;
    };
    UINT    MaxPicWidth;
    UINT    MaxPicHeight;
    UCHAR   MaxNum_Reference;
    UCHAR   MaxNum_SPS_id;
    UCHAR   MaxNum_PPS_id;
    UCHAR   MaxNum_Reference1;
    UCHAR   MaxNum_QualityLayer;
    UCHAR   MaxNum_DependencyLayer;
    UCHAR   MaxNum_DQLayer;
    UCHAR   MaxNum_TemporalLayer;
    UCHAR   MBBRCSupport;
    union {
        struct {
            UCHAR MaxNumOfROI : 5; // [0..16]
        UCHAR: 2;
            UCHAR ROIBRCPriorityLevelSupport : 1;
        };
        UCHAR ROICaps;
    };

    union {
        struct {
            UINT    SkipFrame               : 1;
            UINT    MbQpDataSupport         : 1;
            UINT    SliceLevelWeightedPred  : 1;
            UINT    LumaWeightedPred        : 1;
            UINT    ChromaWeightedPred      : 1;
            UINT    QVBRBRCSupport          : 1;
            UINT    SliceLevelReportSupport : 1;
            UINT    HMEOffsetSupport        : 1;
            UINT    DirtyRectSupport        : 1;
            UINT    MoveRectSupport         : 1;
            UINT    FrameSizeTolerance      : 1;
            UINT    HWCounterAutoIncrement  : 1;
            UINT                            : 20;
        };
        UINT      CodingLimits2;
    };
    UCHAR    MaxNum_WeightedPredL0;
    UCHAR    MaxNum_WeightedPredL1;
} ENCODE_CAPS;

////////////////////////////////////////////////////////////////////////////////
// this structure is used to define the caps for JPEG.
///////////////////////////////////////////////////////////////////////////////
typedef struct tagENCODE_CAPS_JPEG
{
    union {
        struct {
            UINT    Baseline        : 1;
            UINT    Extended        : 1;
            UINT    Lossless        : 1;
            UINT    Hierarchical    : 1;

            UINT    Sequential      : 1;
            UINT    Progressive     : 1;

            UINT    Huffman         : 1;
            UINT    Arithmetic      : 1;

            UINT    NonInterleaved  : 1;
            UINT    Interleaved     : 1;

            UINT    NonDifferential : 1;
            UINT    Differential    : 1;

            UINT    reserved        : 20;
        };
        UINT CodingLimits;
    };

    UINT    MaxPicWidth;
    UINT    MaxPicHeight;

    UINT    SampleBitDepth;
    UINT    MaxNumComponent;
    UINT    MaxNumScan;
    UINT    MaxNumHuffTable;
    UINT    MaxNumQuantTable;
    UINT    MaxSamplesPerSecond;
} ENCODE_CAPS_JPEG;

////////////////////////////////////////////////////////////////////////////////
// this structure is used to define the caps for VP8.
///////////////////////////////////////////////////////////////////////////////
typedef struct tagENCODE_CAPS_VP8 
{ 
    union { 
        struct { 
            UINT CodingLimitSet      : 1; 
            UINT Color420Only        : 1; 
            UINT SegmentationAllowed : 1; 
            UINT CoeffPartitionLimit : 2; 
            UINT FrameLevelRateCtrl  : 1; 
            UINT BRCReset            : 1; 
            UINT                     : 25; 
        }; 
        UINT CodingLimits;
    }; 
        
    union { 
        struct { 
            BYTE EncodeFunc    : 1; 
            BYTE HybridPakFunc : 1; // Hybrid Pak function for BDW 
            BYTE               : 6; 
        }; 
        BYTE CodingFunction; 
    }; 
        
    UINT MaxPicWidth; 
    UINT MaxPicHeight; 
} ENCODE_CAPS_VP8;

// this enumeration is used to define the block size for intra prediction. they
// are used as bit flags to control what block size will be checked for intra
// prediction.
///////////////////////////////////////////////////////////////////////////////
enum
{
    ENC_INTRA_BLOCK_NONE    = 0,
    ENC_INTRA_BLOCK_4x4     = (1 << 0),
    ENC_INTRA_BLOCK_8x8     = (1 << 1),
    ENC_INTRA_BLOCK_16x16   = (1 << 2),
    ENC_INTRA_BLOCK_PCM     = (1 << 3),
    ENC_INTRA_BLOCK_NOPCM   = (ENC_INTRA_BLOCK_4x4  |
                               ENC_INTRA_BLOCK_8x8  |
                               ENC_INTRA_BLOCK_16x16),
    ENC_INTRA_BLOCK_ALL     = (ENC_INTRA_BLOCK_4x4  |
                               ENC_INTRA_BLOCK_8x8  |
                               ENC_INTRA_BLOCK_16x16|
                               ENC_INTRA_BLOCK_PCM),
};


////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define the cost type used in intra/inter prediction.
///////////////////////////////////////////////////////////////////////////////
enum
{
    ENC_COST_TYPE_NONE          = 0,
    ENC_COST_TYPE_SAD           = (1 << 0), //sum of absolute difference
    ENC_COST_TYPE_SSD           = (1 << 1), //sum of squared difference
    ENC_COST_TYPE_SATD_HADAMARD = (1 << 2), //sum of absolute hadamard transformed difference
    ENC_COST_TYPE_SATD_HARR     = (1 << 3), //sum of absolute harr transformed difference
    ENC_COST_TYPE_PROPRIETARY   = (1 << 4), //propriteary cost type
};


////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define the entropy coding type used for RDO
// intra/inter prediction.
///////////////////////////////////////////////////////////////////////////////
enum
{
    ENC_ENTROPY_CODING_VLC      = 0,    //vlc
    ENC_ENTROPY_CODING_CABAC    = 1,    //cabac
};

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define the integer pel search algorithm.
///////////////////////////////////////////////////////////////////////////////
enum
{
    ENC_INTER_SEARCH_TYPE_NONE              = 0,
    ENC_INTER_SEARCH_TYPE_FULL              = (1 << 0),
    ENC_INTER_SEARCH_TYPE_UMH               = (1 << 1),
    ENC_INTER_SEARCH_TYPE_LOG               = (1 << 2),
    ENC_INTER_SEARCH_TYPE_TTS               = (1 << 3),
    ENC_INTER_SEARCH_TYPE_SQUARE            = (1 << 4),
    ENC_INTER_SEARCH_TYPE_DIAMOND           = (1 << 5),
    ENC_INTER_SEARCH_TYPE_PROPRIETARY       = (1 << 6),
};

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define the inter prediction block size. they are
// used as bit flags to control what block size should be checked in inter
// prediction.
// this also controls how many MV can exist in one MB.
///////////////////////////////////////////////////////////////////////////////
enum
{
    ENC_INTER_BLOCK_SIZE_NONE       = 0,
    ENC_INTER_BLOCK_SIZE_16x16      = (1 << 0),
    ENC_INTER_BLOCK_SIZE_16x8       = (1 << 1),
    ENC_INTER_BLOCK_SIZE_8x16       = (1 << 2),
    ENC_INTER_BLOCK_SIZE_8x8        = (1 << 3),
    ENC_INTER_BLOCK_SIZE_8x4        = (1 << 4),
    ENC_INTER_BLOCK_SIZE_4x8        = (1 << 5),
    ENC_INTER_BLOCK_SIZE_4x4        = (1 << 6),

    //all possible size
    ENC_INTER_BLOCK_SIZE_ALL        = (ENC_INTER_BLOCK_SIZE_16x16 |
    ENC_INTER_BLOCK_SIZE_16x8  |
    ENC_INTER_BLOCK_SIZE_8x16  |
    ENC_INTER_BLOCK_SIZE_8x8   |
    ENC_INTER_BLOCK_SIZE_8x4   |
    ENC_INTER_BLOCK_SIZE_4x8   |
    ENC_INTER_BLOCK_SIZE_4x4),

    //blocks are bigger than or equal 8x8
    ENC_INTER_BLOCK_SIZE_NO_MINOR   = (ENC_INTER_BLOCK_SIZE_16x16 |
    ENC_INTER_BLOCK_SIZE_16x8  |
    ENC_INTER_BLOCK_SIZE_8x16  |
    ENC_INTER_BLOCK_SIZE_8x8),

    //VC-1 4-MV mode, for P frame only
    ENC_INTER_BLOCK_SIZE_VC1_4MV    = (ENC_INTER_BLOCK_SIZE_16x16 |
    ENC_INTER_BLOCK_SIZE_8x8),

    //VC-1 1-MV mode, for P and B frame
    ENC_INTER_BLOCK_SIZE_VC1_1MV    = ENC_INTER_BLOCK_SIZE_16x16,

    //MPEG2 1-MV mode, for P and B frame
    ENC_INTER_BLOCK_SIZE_MPEG_1MV   = ENC_INTER_BLOCK_SIZE_16x16,

};


////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define the interpolation methods used to get the
// sub pixels.
//
// WMV4TAP: for 1/2 pel [-1, 9, 9, -1]/16
//          for 1/4 pel [-4, 53, 18, -3]/64
//          for 3/4 pel [-3, 18, 53, -4]/64
//
// VME4TAP: for 1/2 pel [-1, 5, 5, -1]/8
//          for 1/4 pel [-1, 13, 5, -1]/16
//          for 3/4 pel [-1, 5, 13, -1]/16
//
// BILINEAR: for 1/2 pel [0, 1, 1, 0]/2
//           for 1/4 pel [0, 3, 1, 0]/4
//           for 3/4 pel [0, 1, 3, 0]/4
//
// AVC6TAP:  for 1/2 pel [1, -5, 20, 20, -5, 1]/32
//           for 1/4 pel [1, -5, 52, 20, -5, 1]/64
//           for 3/4 pel [1, -5, 20, 52, -5, 1]/64
//
///////////////////////////////////////////////////////////////////////////////
enum
{
    ENC_INTERPOLATION_TYPE_NONE          = 0,
    ENC_INTERPOLATION_TYPE_VME4TAP       = (1 << 0),
    ENC_INTERPOLATION_TYPE_BILINEAR      = (1 << 1),
    ENC_INTERPOLATION_TYPE_WMV4TAP       = (1 << 2),
    ENC_INTERPOLATION_TYPE_AVC6TAP       = (1 << 3),
};

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define the MV precision.
///////////////////////////////////////////////////////////////////////////////
enum
{
    ENC_MV_PRECISION_NONE           = 0,
    ENC_MV_PRECISION_INTEGER        = (1 << 0),
    ENC_MV_PRECISION_HALFPEL        = (1 << 1),
    ENC_MV_PRECISION_QUARTERPEL     = (1 << 2),
};

typedef struct tagENCODE_ENC_CTRL_CAPS
{
    //DW 0
    UINT    IntraPredBlockSize          : 4;
    UINT    IntraPredCostType           : 5;
    UINT    InterPredBlockSize          : 8;
    UINT    MVPrecision                 : 4;
    UINT    MBZ0                        : 11;

    //DW 1
    UINT    MECostType                  : 5;
    UINT    MESearchType                : 8;
    UINT    MBZ1                        : 19;

    //DW 2
    UINT    MVSearchWindowX             : 16;
    UINT    MVSearchWindowY             : 16;

    //DW 3
    UINT    MEInterpolationMethod       : 4;
    UINT    MaxMVs                      : 6;
    UINT    SkipCheck                   : 1;
    UINT    DirectCheck                 : 1;
    UINT    BiDirSearch                 : 1;
    UINT    MBAFF                       : 1;
    UINT    FieldPrediction             : 1;
    UINT    RefOppositeField            : 1;
    UINT    ChromaInME                  : 1;
    UINT    WeightedPrediction          : 1;
    UINT    RateDistortionOpt           : 1;
    UINT    MVPrediction                : 1;
    UINT    DirectVME                   : 1;
    UINT    MBZ2                        : 5;
    UINT    MAD                         : 1;
    UINT    MBZ3                        : 5;

} ENCODE_ENC_CTRL_CAPS;


////////////////////////////////////////////////////////////////////////////////
// this structure is used to define the sequence level parameters
///////////////////////////////////////////////////////////////////////////////
typedef enum tagENCODE_ARGB_COLOR
{
    eColorSpace_P709 = 0,
    eColorSpace_P601 = 1,
    eColorSpace_P2020 = 2
}ENCODE_ARGB_COLOR;

typedef enum tagENCODE_FRAME_SIZE_TOLERANCE
{
    eFrameSizeTolerance_Normal = 0,//default
    eFrameSizeTolerance_Low = 1,//Sliding Window
    eFrameSizeTolerance_ExtremelyLow = 2//low delay
}ENCODE_FRAME_SIZE_TOLERANCE;

typedef enum tagENCODE_SCENARIO
{
    eScenario_Unknown = 0,
    eScenario_DisplayRemoting = 1,
    eScenario_VideoConference = 2,
    eScenario_Archive = 3,
    eScenario_LiveStreaming = 4,
    eScenario_VideoCapture = 5,
    eScenario_VideoSurveillance = 6
} ENCODE_SCENARIO;

typedef enum tagENCODE_CONTENT
{
    eContent_Unknown = 0,
    eContent_FullScreenVideo = 1,
    eContent_NonVideoScreen = 2
} ENCODE_CONTENT;

typedef struct tagENCODE_SET_SEQUENCE_PARAMETERS_H264
{
    USHORT  FrameWidth;
    USHORT  FrameHeight;
    UCHAR   Profile;
    UCHAR   Level;

    USHORT  GopPicSize;
    UCHAR   GopRefDist;
    UCHAR   GopOptFlag : 2;
    UCHAR   Reserved2  : 6;

    UCHAR   TargetUsage;
    UCHAR   RateControlMethod;
    UINT    TargetBitRate;
    UINT    MaxBitRate;
    UINT    MinBitRate;
    USHORT  FramesPer100Sec;
    ULONG   InitVBVBufferFullnessInBit;
    ULONG   VBVBufferSizeInBit;
    UCHAR   NumRefFrames;

    UCHAR   seq_parameter_set_id;
    UCHAR   chroma_format_idc;
    UCHAR   bit_depth_luma_minus8;
    UCHAR   bit_depth_chroma_minus8;
    UCHAR   log2_max_frame_num_minus4;
    UCHAR   pic_order_cnt_type;
    UCHAR   log2_max_pic_order_cnt_lsb_minus4;
    UCHAR   num_ref_frames_in_pic_order_cnt_cycle;
    INT     offset_for_non_ref_pic;
    INT     offset_for_top_to_bottom_field;
    INT     offset_for_ref_frame[256];
    USHORT  frame_crop_left_offset;
    USHORT  frame_crop_right_offset;
    USHORT  frame_crop_top_offset;
    USHORT  frame_crop_bottom_offset;
    USHORT  seq_scaling_matrix_present_flag         : 1;
    USHORT  seq_scaling_list_present_flag           : 1;
    USHORT  delta_pic_order_always_zero_flag        : 1;
    USHORT  frame_mbs_only_flag                     : 1;
    USHORT  direct_8x8_inference_flag               : 1;
    USHORT  vui_parameters_present_flag             : 1;
    USHORT  frame_cropping_flag                     : 1;
    USHORT  EnableSliceLevelRateCtrl                : 1;
    USHORT  MBZ1                                    : 8;

    union
    {
        struct
        {
            UINT    bResetBRC                       : 1;
            UINT    bNoAccelerationSPSInsertion     : 1;
            UINT    GlobalSearch                    : 2;
            UINT    LocalSearch                     : 4;
            UINT    EarlySkip                       : 2;
            UINT    Reserved0                       : 2;
            UINT    MBBRC                           : 4;
            UINT    Trellis                         : 4;
            UINT    bTemporalScalability            : 1;//for VDEnc SVC NAL unit insertion will be done after encoding, this flag is to signal driver that it need count SVC NAL unit bits in BRC.
            UINT    Reserved1                       :11;
        };

        UINT sFlags;
    };

    UINT    UserMaxFrameSize;
    USHORT  Reserved3;//AVBRAccuracy;
    USHORT  Reserved4;//AVBRConvergence;
    USHORT  ICQQualityFactor;
    ENCODE_ARGB_COLOR ARGBInputColor;
    ENCODE_SCENARIO ScenarioInfo;
    ENCODE_CONTENT  ContentInfo;
    ENCODE_FRAME_SIZE_TOLERANCE FrameSizeTolerance;

} ENCODE_SET_SEQUENCE_PARAMETERS_H264;

typedef struct tagENCODE_SET_SEQUENCE_PARAMETERS_MPEG2
{
    USHORT   FrameWidth;
    USHORT   FrameHeight;
    UCHAR    Profile;
    UCHAR    Level;
    UCHAR    ChromaFormat;
    UCHAR    TargetUsage;


    // ENC + PAK related parameters
    union
    {
        USHORT  AratioFrate;
        struct
        {
            USHORT  AspectRatio             : 4;
            USHORT  FrameRateCode           : 4;
            USHORT  FrameRateExtN           : 3;
            USHORT  FrameRateExtD           : 5;
        };
    };

    UINT    bit_rate;           // bits/sec
    UINT    vbv_buffer_size;    // 16 Kbit

    UCHAR   progressive_sequence            : 1;
    UCHAR   low_delay                       : 1;
    UCHAR   bResetBRC                       : 1;
    UCHAR   bNoSPSInsertion                 : 1;
    UCHAR   Reserved                        : 4;
    UCHAR   RateControlMethod;
    USHORT  Reserved16bits;
    UINT    MaxBitRate;
    UINT    MinBitRate;
    UINT    UserMaxFrameSize;
    UINT    InitVBVBufferFullnessInBit;
    USHORT  AVBRAccuracy;
    USHORT  AVBRConvergence;
} ENCODE_SET_SEQUENCE_PARAMETERS_MPEG2;


/* H.264/AVC picture entry data structure */
typedef struct _ENCODE_PICENTRY {
    union {
        struct {
            UCHAR  Index7Bits      : 7;
            UCHAR  AssociatedFlag  : 1;
        };
        UCHAR  bPicEntry;
    };
} ENCODE_PICENTRY;  /* 1 byte */

typedef struct tagENCODE_RECT
{
    USHORT Top;                // [0..(FrameHeight+ M-1)/M -1]
    USHORT Bottom;             // [0..(FrameHeight+ M-1)/M -1]
    USHORT Left;               // [0..(FrameWidth+15)/16-1]
    USHORT Right;              // [0..(FrameWidth+15)/16-1]
} ENCODE_RECT;

typedef struct tagENCODE_ROI
{
    ENCODE_RECT Roi;
    CHAR   PriorityLevelOrDQp; // [-3..3] or [-51..51]
} ENCODE_ROI;




typedef struct tagMOVE_RECT
{
    USHORT SourcePointX;
    USHORT SourcePointY;
    ENCODE_RECT DestRect;
} MOVE_RECT;

typedef enum tagENCODE_INPUT_TYPE
{
    eType_DRM_NONE = 0,
    eType_DRM_SECURE = 1,
    eType_DRM_UNKNOWN
}ENCODE_INPUT_TYPE;


typedef struct tagENCODE_SET_PICTURE_PARAMETERS_H264
{
    ENCODE_PICENTRY     CurrOriginalPic;
    ENCODE_PICENTRY     CurrReconstructedPic;
    UCHAR   CodingType;

    UCHAR   FieldCodingFlag         : 1;
    UCHAR   FieldFrameCodingFlag    : 1;
    UCHAR   MBZ0                    : 2;
    UCHAR   InterleavedFieldBFF     : 1;
    UCHAR   ProgressiveField        : 1;
    UCHAR   MBZ1                    : 2;

    UCHAR           NumSlice;
    CHAR            QpY;
    ENCODE_PICENTRY RefFrameList[16];
    UINT            UsedForReferenceFlags;
    INT             CurrFieldOrderCnt[2];
    INT             FieldOrderCntList[16][2];
    USHORT          frame_num;
    BOOL            bLastPicInSeq;
    BOOL            bLastPicInStream;
    union
    {
        struct
        {
            UINT        bUseRawPicForRef                         : 1;
            UINT        bDisableHeaderPacking                    : 1;
            UINT        bEnableDVMEChromaIntraPrediction         : 1;
            UINT        bEnableDVMEReferenceLocationDerivation   : 1;
            UINT        bEnableDVMWSkipCentersDerivation         : 1;
            UINT        bEnableDVMEStartCentersDerivation        : 1;
            UINT        bEnableDVMECostCentersDerivation         : 1;
            UINT        bDisableSubMBPartition                   : 1;
            UINT        bEmulationByteInsertion                  : 1;
            UINT        bEnableRollingIntraRefresh               : 2;
            UINT        bSliceLevelReport                        : 1;
            UINT        bDisableSubpixel                         : 1;
            UINT        bReserved                                : 19;

        };
        BOOL    UserFlags;
    };
    UINT            StatusReportFeedbackNumber;
    UCHAR           bIdrPic;
    UCHAR           pic_parameter_set_id;
    UCHAR           seq_parameter_set_id;
    UCHAR           num_ref_idx_l0_active_minus1;
    UCHAR           num_ref_idx_l1_active_minus1;
    CHAR            chroma_qp_index_offset;
    CHAR            second_chroma_qp_index_offset;
    USHORT          entropy_coding_mode_flag            : 1;
    USHORT          pic_order_present_flag              : 1;
    USHORT          weighted_pred_flag                  : 1;
    USHORT          weighted_bipred_idc                 : 2;
    USHORT          constrained_intra_pred_flag         : 1;
    USHORT          transform_8x8_mode_flag             : 1;
    USHORT          pic_scaling_matrix_present_flag     : 1;
    USHORT          pic_scaling_list_present_flag       : 1;
    USHORT          RefPicFlag                          : 1;
    USHORT          BRCPrecision                        : 2;
    USHORT          DisplayFormatSwizzle                : 1;
    USHORT          MBZ2                                : 3;
    USHORT          IntraInsertionLocation;
    USHORT          IntraInsertionSize;
    CHAR            QpDeltaForInsertedIntra;
    UINT            SliceSizeInBytes;
    UCHAR           NumROI;// [0..16]
    ENCODE_ROI      ROI[16];
    CHAR            MaxDeltaQp; // [-51..51]
    CHAR            MinDeltaQp; // [-51..51]

    // Skip Frames
    UCHAR           SkipFrameFlag;
    UCHAR           NumSkipFrames;
    UINT            SizeSkipFrames;

    // Max/Min QP settings for BRC
    UCHAR           BRCMaxQp;
    UCHAR           BRCMinQp;

    // HME Offset
    UCHAR           bEnableHMEOffset;
    SHORT           HMEOffset[16][2][2];

    // Hints
    UCHAR            NumDirtyRects;
    ENCODE_RECT     *pDirtyRect;
    UCHAR            NumMoveRects;
    MOVE_RECT       *pMoveRect;

    ENCODE_INPUT_TYPE InputType;

} ENCODE_SET_PICTURE_PARAMETERS_H264;

typedef struct tagENCODE_SET_PICTURE_PARAMETERS_MPEG2
{
    ENCODE_PICENTRY CurrOriginalPic;
    ENCODE_PICENTRY CurrReconstructedPic;
    UCHAR           picture_coding_type;
    UCHAR           FieldCodingFlag         : 1;
    UCHAR           FieldFrameCodingFlag    : 1;
    UCHAR           MBZ0                    : 2;
    UCHAR           InterleavedFieldBFF     : 1;
    UCHAR           ProgressiveField        : 1;
    UCHAR           MBZ1                    : 2;
    UCHAR           NumSlice;
    UCHAR           bPicBackwardPrediction;
    UCHAR           bBidirectionalAveragingMode;
    UCHAR           bPic4MVallowed;
    ENCODE_PICENTRY RefFrameList[2];
    union
    {
        struct
           {
            BOOL        bUseRawPicForRef                         : 1;
            BOOL        bDisableHeaderPacking                    : 1;
            BOOL        bEnableDVMEChromaIntraPrediction         : 1;
            BOOL        bEnableDVMEReferenceLocationDerivation   : 1;
            BOOL        bEnableDVMESkipCentersDerivation         : 1;
            BOOL        bEnableDVMEStartCentersDerivation        : 1;
            BOOL        bEnableDVMECostCentersDerivation         : 1;
            BOOL        bReserved                                : 25;
        };
        BOOL    UserFlags;
    };
    UINT            StatusReportFeedbackNumber;
    UINT            alternate_scan              : 1;
    UINT            intra_vlc_format            : 1;
    UINT            q_scale_type                : 1;
    UINT            concealment_motion_vectors  : 1;
    UINT            frame_pred_frame_dct        : 1;



    UINT            DisableMismatchControl      : 1;
    UINT            intra_dc_precision          : 2;
    UINT            f_code00                    : 4;
    UINT            f_code01                    : 4;
    UINT            f_code10                    : 4;
    UINT            f_code11                    : 4;


    UINT            Reserved1                   : 8;

    // ENC + PAK related parameters
    BOOL            bLastPicInStream;
    BOOL            bNewGop;

    USHORT          GopPicSize;
    UCHAR           GopRefDist;
    UCHAR           GopOptFlag                  : 2;
    UCHAR                                       : 6;

    UINT            time_code                   : 25;
    UINT                                        : 7;

    USHORT          temporal_reference          : 10;
    USHORT                                      : 6;

    USHORT          vbv_delay;

    UINT            repeat_first_field          : 1;
    UINT            composite_display_flag      : 1;
    UINT            v_axis                      : 1;
    UINT            field_sequence              : 3;
    UINT            sub_carrier                 : 1;
    UINT            burst_amplitude             : 7;
    UINT            sub_carrier_phase           : 8;
    UINT                                        : 10;


} ENCODE_SET_PICTURE_PARAMETERS_MPEG2;
typedef struct tagENCODE_SET_PICTURE_PARAMETERS_JPEG
{
    struct {
        UINT    Profile      : 2; // 0 - Baseline, 1 - Extended, 2 - Lossless, 3 - Hierarchical
        UINT    Progressive  : 1; // 1 - Progressive, 0 - Sequential
        UINT    Huffman      : 1; // 1 - Huffman , 0 - Arithmetic
        UINT    Interleaved  : 1; // 1 - Interleaved, 0 - NonInterleaved
        UINT    Differential : 1; // 1 - Differential, 0 - NonDifferential
    };

    UINT    PicWidth;
    UINT    PicHeight;

    UINT    InputSurfaceFormat;
    UINT    SampleBitDepth;

    UINT    NumComponent;
    UCHAR   ComponentID [4];
    UCHAR   QuantTableSelector[4];

    UINT    Quality;

    UINT    NumScan;
    UINT    NumQuantTable;
    UINT    NumCodingTable;

    UINT    StatusReportFeedbackNumber;
} ENCODE_SET_PICTURE_PARAMETERS_JPEG;

typedef struct tagENCODE_SET_SLICE_HEADER_H264
{
    UINT                NumMbsForSlice;
    ENCODE_PICENTRY     RefPicList[2][32];
    SHORT               Weights[2][32][3][2];
    UINT                first_mb_in_slice;

    UCHAR               slice_type;
    UCHAR               pic_parameter_set_id;
    USHORT              direct_spatial_mv_pred_flag         : 1;
    USHORT              num_ref_idx_active_override_flag    : 1;
    USHORT              long_term_reference_flag            : 1;
    USHORT              MBZ0                                : 13;
    USHORT              idr_pic_id;
    USHORT              pic_order_cnt_lsb;
    INT                 delta_pic_order_cnt_bottom;
    INT                 delta_pic_order_cnt[2];
    UCHAR               num_ref_idx_l0_active_minus1;
    UCHAR               num_ref_idx_l1_active_minus1;
    UCHAR               luma_log2_weight_denom;
    UCHAR               chroma_log2_weight_denom;
    UCHAR               cabac_init_idc;
    CHAR                slice_qp_delta;
    UCHAR               disable_deblocking_filter_idc;
    CHAR                slice_alpha_c0_offset_div2;
    CHAR                slice_beta_offset_div2;
    UINT                slice_id;
	
	UINT	luma_weight_flag[2];		// for l0/l1
	UINT	chroma_weight_flag[2];	// for l0/l1

} ENCODE_SET_SLICE_HEADER_H264;

typedef struct tagENCODE_PACKEDHEADER_DATA
{
    UCHAR * pData;
    UINT    BufferSize;
    UINT    DataLength;
    UINT    DataOffset;
    UINT    SkipEmulationByteCount;
    UINT    Reserved;
} ENCODE_PACKEDHEADER_DATA;

typedef struct tagENCODE_SET_SLICE_HEADER_MPEG2
{
    USHORT  NumMbsForSlice;
    USHORT  FirstMbX;
    USHORT  FirstMbY;
    USHORT  IntraSlice;
    UCHAR   quantiser_scale_code;
} ENCODE_SET_SLICE_HEADER_MPEG2;

typedef struct tagENCODE_SET_SCAN_PARAMETERS_JPEG
{
    UINT     RestartInterval;

    UINT     NumComponent;
    UCHAR    ComponentSelector[4];
    UCHAR    DcCodingTblSelector[4];
    UCHAR    AcCodingTblSelector[4];

    UINT     FirstDCTCoeff;
    UINT     LastDCTCoeff;
    UINT     Ah;
    UINT     Al;
} ENCODE_SET_SCAN_PARAMETERS_JPEG;

typedef struct tagENCODE_QUANT_TABLE_JPEG
{
    UINT     TableID;
    UINT     Precision;
    USHORT   Qm[64];
} ENCODE_QUANT_TABLE_JPEG;

typedef struct tagENCODE_HUFFMAN_TABLE_JPEG
{
    UINT     TableClass;
    UINT     TableID;
    UCHAR    BITS[16];
    UCHAR    HUFFVAL[162];
} ENCODE_HUFFMAN_TABLE_JPEG;

typedef struct ENCODE_MB_CONTROL_DATA_H264
{
    union
    {
        struct
        {
            UINT  InterMbMode            : 2;
            UINT  SkipMbFlag            : 1;
            UINT  MBZ1                    : 1;
            UINT  IntraMbMode            : 2;
            UINT    MBZ2                : 1;
            UINT  FieldMbPolarityFlag    : 1;
            UINT  MbType                : 5;
            UINT  IntraMbFlag            : 1;
            UINT  FieldMbFlag            : 1;
            UINT  Transform8x8Flag        : 1;
            UINT  MBZ3                    : 1;
            UINT  CbpDcV                : 1;
            UINT  CbpDcU                : 1;
            UINT  CbpDcY                : 1;
            UINT  MvFormat                 : 3;
            UINT  MBZ4                    : 1;
            UINT  PackedMvNum             : 6;
            UINT  MBZ5                    : 2;
        };
        UINT dwMBtype;
    };

    //DW 9
    UCHAR MbYCnt;
    UCHAR MbXCnt;
    USHORT Cbp4x4Y;

    //DW10
    USHORT Cbp4x4V;
    USHORT Cbp4x4U;

    //DW 11
    struct
    {
        UINT QpPrimeY            : 8;
        UINT MBZ6                 : 17;
        UINT SkipMbConvDisable  : 1;
        UINT LastMbFlag            : 1;
        UINT EnableCoeffClamp    : 1;
        UINT Skip8x8Pattern     : 4;
    };

    //DW 12 ~ DW 14
    union
    {
        // Intra MBs
        struct
        {
            USHORT  LumaIntraPredModes[4];
            union
            {
                struct
                {
                    UCHAR  ChromaIntraPredMode : 2;
                    UCHAR  IntraPredAvilFlagD  : 1;    //above left
                    UCHAR  IntraPredAvilFlagC  : 1;    //above right
                    UCHAR  IntraPredAvilFlagB  : 1;    //above
                    UCHAR  IntraPredAvilFlagE  : 1;    //left bottom half
                    UCHAR  IntraPredAvilFlagA  : 1;    //left top half
                    UCHAR  IntraPredAvilFlagF  : 1;    //left row (-1, 7)
                };
                UCHAR  IntraStruct;
            };
            UCHAR  MBZ7[3];
        };

        // Inter MBs
        struct
        {
            UCHAR     SubMbShape;
            UCHAR     SubMbPredMode;
            USHORT    MBZ8;
            UCHAR   RefIdxL0[4];
            UCHAR   RefIdxL1[4];
        };
    };

    //DW 15
    USHORT MBZ9;
    UCHAR TargetSizeInWord;
    UCHAR MaxSizeInWord;

} ENCODE_MB_CONTROL_DATA_H264;

typedef struct
{
    SHORT  x;
    SHORT  y;
} ENC_MV, ENCODE_MV_DATA;

typedef struct tagENCODE_ENC_MB_DATA_MPEG2
{
    struct
    {
        UINT InterMBMode            : 2;
        UINT SkipMbFlag                : 1;
        UINT MBZ1                    : 5;
        UINT macroblock_type        : 5;
        UINT macroblock_intra        : 1;
        UINT FieldMBFlag            : 1;
        UINT dct_type               : 1;
        UINT MBZ2                    : 1;
        UINT CbpDcV                    : 1;
        UINT CbpDcU                    : 1;
        UINT CbpDcY                    : 1;
        UINT MvFormat                : 3;
        UINT MBZ3                    : 1;
        UINT PackedMvNum            : 3;
        UINT MBZ4                    : 5;
    };

    struct
    {
        UINT MbX  : 16;
        UINT MbY  : 16;
    };

    struct
    {
        UINT coded_block_pattern    : 12;
        UINT MBZ5                     : 4;
        UINT TargetSizeInWord       : 8;
        UINT MaxSizeInWord          : 8;
    };

    struct
    {
        UINT QpScaleCode            : 5;
        UINT MBZ6                    : 11;
        UINT MvFieldSelect          : 4;
        UINT MBZ7                   : 4;
        UINT FirstMbInSG            : 1;
        UINT MbSkipConvDisable      : 1;
        UINT LastMbInSG             : 1;
        UINT EnableCoeffClamp       : 1;
        UINT MBZ8                   : 2;
        UINT FirstMbInSlice         : 1;
        UINT LastMbInSlice          : 1;
    };

    ENC_MV  MVL0[2];
    ENC_MV  MVL1[2];
    UINT reserved[8];
} ENCODE_ENC_MB_DATA_MPEG2;

typedef struct tagENCODE_ENC_MV_H264
{
    ENC_MV  MVL0[16];
    ENC_MV  MVL1[16];
} ENCODE_ENC_MV_H264;

typedef struct tagENCODE_MBDATA_LAYOUT
{
    UCHAR         MB_CODE_size;
    UINT          MB_CODE_offset;
    UINT          MB_CODE_BottomField_offset;
    UINT          MB_CODE_stride;
    UCHAR         MV_number;
    UINT          MV_offset;
    UINT          MV_BottomField_offset;
    UINT          MV_stride;

} ENCODE_MBDATA_LAYOUT;

//typedef DXVA_QmatrixData ENCODE_SET_PICTURE_QUANT_MATRIX_MPEG2;
typedef struct tagENCODE_SET_PICTURE_QUANT
{
    BOOL bPicQuant;
    UCHAR paramID;
    union
    {
        DXVA_Qmatrix_H264 QmatrixAVC;
        DXVA_QmatrixData QmatrixMPEG2;
    };
}ENCODE_SET_PICTURE_QUANT; 

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define the input picture structure. VME can only
// support frame picture encoding type, i.e, in the interlaced case, input can
// only be interleaved fields, and they are encoded together. VME doesn't support
// individual field input and doesn't support encoding individual pield picture.
///////////////////////////////////////////////////////////////////////////////
typedef enum
{
    PICSTRUCT_PROGRESSIVE           = (1 << 0),
    PICSTRUCT_INTERLACED_TFF        = (1 << 1),
    PICSTRUCT_INTERLACED_BFF        = (1 << 2),
    PICSTRUCT_TOPFIELD              = (1 << 3),
    PICSTRUCT_BOTTOMFIELD           = (1 << 4),
    PICSTRUCT_PAFF                  = (1 << 5),
    PICSTRUCT_MBAFF                 = (1 << 6)
} PicStructType;


typedef struct tagENCODE_SET_SEQUENCE_PARAMETERS_SVC
{
    USHORT  FrameWidth;
    USHORT  FrameHeight;
    UCHAR   Profile;
    UCHAR   Level;
    USHORT  GopPicSize;
    UCHAR   GopRefDist;

    UCHAR   GopOptFlag  : 2;
    UCHAR   Reserved2   : 6;

    UCHAR   TargetUsage;
    UCHAR     RateControlMethod;
    UINT      TargetBitRate;
    UINT      MaxBitRate;
    UINT      MinBitRate;
    USHORT    FrameRateNumerator;
    USHORT    FrameRateDenominator;
    ULONG     InitVBVBufferFullnessInBit;
    ULONG     VBVBufferSizeInBit;            // do we need it??
    UCHAR   NumRefFrames;
    union
    {
        struct
        {
            UINT    bResetBRC                       : 1;
            UINT    bNoAcceleratorSPSInsertion      : 1;    // always 0
            UINT    GlobalSearch                    : 2;
            UINT    LocalSearch                     : 4;
            UINT    EarlySkip                       : 2;
            UINT    Reserved0                       : 2;
            UINT    MBBRC                           : 4;
            UINT    Trellis                         : 4;
            UINT    Reserved1                       :12;
        };
        UINT    sFlags;
    };
    UINT        UserMaxLayerSize;
    USHORT        AVBRAccuracy;
    USHORT        AVBRConvergence;
    UCHAR   chroma_format_idc;
    UCHAR   bit_depth_luma_minus8;
    UCHAR   bit_depth_chroma_minus8;

    UINT        separate_colour_plane_flag              : 1;
    UINT        qpprime_y_zero_transform_bypass_flag    : 1;
    UINT    seq_scaling_matrix_present_flag                     : 1;
    UINT    frame_mbs_only_flag                                 : 1;
    UINT    direct_8x8_inference_flag                           : 1;
    UINT    extended_spatial_scalability_idc                    : 2;
    UINT    chroma_phase_x_plus1_flag                           : 1;
    UINT    chroma_phase_y_plus1                                : 2;
    UINT    Reserved3                                           : 22;

    USHORT  seq_scaling_list_present_flag                       : 12;
    USHORT  Reserved4                                           : 6;

// HRD parameter related
    UCHAR       fixed_frame_rate_flag                   : 1;
    UCHAR       nalHrdConformanceRequired               : 1;
    UCHAR       vclHrdConformanceRequired               : 1;
    UCHAR       low_delay_hrd_flag                      : 1;
    UCHAR       Reserved5                               : 4;
    UINT        num_units_in_tick;
    UINT        time_scale;
    UCHAR       bit_rate_scale;
    UCHAR       cpb_size_scale;
    UINT        bit_rate_value_minus1;
    UINT        cpb_size_value_minus1;
    UCHAR       cpb_cnt_minus1;

} ENCODE_SET_SEQUENCE_PARAMETERS_SVC;


typedef struct tagENCODE_SET_PICTURE_PARAMETERS_SVC
{
    ENCODE_PICENTRY CurrOriginalPic;
    ENCODE_PICENTRY CurrReconstructedPic;
    UCHAR           CodingType;

    UCHAR           FieldCodingFlag         : 1;
    UCHAR           FieldFrameCodingFlag    : 1;
    UCHAR           InterleavedFieldBFF     : 1;
    UCHAR           ProgressiveField        : 1;
    UCHAR           bLastLayerInPicture     : 1;
    UCHAR           bLastPicInSeq           : 1;
    UCHAR           bLastPicInStream        : 1;
    UCHAR           Reserved1               : 1;

    UCHAR           NumSlice;


    CHAR            QpY;
    ENCODE_PICENTRY RefFrameList[16];
    ENCODE_PICENTRY RefLayer;

    UINT            UsedForReferenceFlags;
    INT             CurrFieldOrderCnt[2];
    INT             FieldOrderCntList[16][2];

    union
    {
        struct
        {
            UINT    bUseRawPicForRef                        : 1;
            UINT    bDisableAcceleratorHeaderPacking        : 1;
            UINT    bEnableDVMEChromaIntraPrediction        : 1;
            UINT    bEnableDVMEReferenceLocationDerivation  : 1;
            UINT    bEnableDVMESkipCentersDerivation        : 1;
            UINT    bEnableDVMEStartCentersDerivation       : 1;
            UINT    bEnableDVMECostCentersDerivation        : 1;
            UINT    bDisableSubMBPartition                  : 1;
            UINT    bEmulationByteInsertion                 : 1;
            UINT    bEnableRollingIntraRefresh              : 2;
        };
        UINT    UserFlags;
    };

    UINT            StatusReportFeedbackNumber;
    CHAR            chroma_qp_index_offset;
    CHAR            second_chroma_qp_index_offset;

    UINT            entropy_coding_mode_flag            : 1;
    UINT            weighted_pred_flag                  : 1;
    UINT            weighted_bipred_idc                 : 2;
    UINT            constrained_intra_pred_flag         : 1;
    UINT            transform_8x8_mode_flag             : 1;
    UINT            pic_scaling_matrix_present_flag     : 1;
    UINT            pic_scaling_list_present_flag       : 1;
    UINT            RefPicFlag                          : 1;
    USHORT    BRCPrecision                    : 2;

    UINT            dependency_id                       : 3;
    UINT            quality_id                          : 4;
    UINT            temporal_id                         : 3;
    UINT            constrained_intra_resampling_flag   : 1;
    UINT            ref_layer_dependency_id             : 3;
    UINT            ref_layer_quality_id                : 4;
    UINT            tcoeff_level_prediction_flag        : 1;
    UINT            use_ref_base_pic_flag               : 1;
    UINT            store_ref_base_pic_flag             : 1;

    UCHAR           disable_inter_layer_deblocking_filter_idc   : 3;
    UCHAR           next_layer_resolution_change_flag           : 1;
    UCHAR           Reserved2                                   : 4; 

    CHAR            inter_layer_slice_alpha_c0_offset_div2;
    CHAR            inter_layer_slice_beta_offset_div2;

    UCHAR           ref_layer_chroma_phase_x_plus1_flag;
    UCHAR           ref_layer_chroma_phase_y_plus1;
    SHORT           scaled_ref_layer_left_offset;
    SHORT           scaled_ref_layer_top_offset;
    SHORT           scaled_ref_layer_right_offset;
    SHORT           scaled_ref_layer_bottom_offset;

    SHORT           refPicScaledRefLayerLeftOffset[16];
    SHORT           refPicScaledRefLayerTopOffset[16];
    SHORT           refPicScaledRefLayerRightOffset[16];
    SHORT           refPicScaledRefLayerBottomOffset[16];

} ENCODE_SET_PICTURE_PARAMETERS_SVC;


typedef struct tagENCODE_SET_SLICE_HEADER_SVC
{
    UINT            NumMbsForSlice;
    ENCODE_PICENTRY RefPicList[2][32];
    SHORT           Weights[2][32][3][2];

    UCHAR           luma_log2_weight_denom;
    UCHAR           chroma_log2_weight_denom;

    UINT            first_mb_in_slice;
    UCHAR           slice_type;

    UCHAR           num_ref_idx_l0_active_minus1;
    UCHAR           num_ref_idx_l1_active_minus1;
    UCHAR           cabac_init_idc;
    CHAR            slice_qp_delta;
    UCHAR           disable_deblocking_filter_idc;
    CHAR            slice_alpha_c0_offset_div2;
    CHAR            slice_beta_offset_div2;
    UINT            slice_id;

    UINT            MaxSliceSize;

    USHORT          no_inter_layer_pred_flag            : 1;
    USHORT          direct_spatial_mv_pred_flag         : 1;
    USHORT          base_pred_weight_table_flag         : 1;
    USHORT          scan_idx_start                      : 4;
    USHORT          scan_idx_end                        : 4;
    USHORT          colour_plane_id                     : 2;
    USHORT          Reserved1                           : 3;

} ENCODE_SET_SLICE_HEADER_SVC;


typedef struct tagENCODE_SET_SEQUENCE_PARAMETERS_VP8 
{ 
    USHORT wFrameWidth        :   14; 
    USHORT wFrameWidthScale :    2; 
    USHORT wFrameHeight        :   14;
    USHORT wFrameHeightScale:    2;

    USHORT GopPicSize; 
   
    UCHAR TargetUsage; 
    UCHAR RateControlMethod; 
    UINT  TargetBitRate[256]; 
    UINT  MaxBitRate; 
    UINT  MinBitRate; 
    ULONG InitVBVBufferFullnessInBit; 
    ULONG VBVBufferSizeInBit; 
    
    union 
    { 
        struct 
        { 
            BOOL bResetBRC               : 1; 
            BOOL bNoFrameHeaderInsertion : 1; 
            BOOL bUseRawReconRef         : 1;
            BOOL MBBRC                   : 4;
            BOOL bReserved               : 25; 
        }; 
        BOOL sFlags; 
    }; 
    
    UINT   UserMaxFrameSize; 
    USHORT AVBRAccuracy; 
    USHORT AVBRConvergence;
    USHORT FramesPer100Sec[256];
} ENCODE_SET_SEQUENCE_PARAMETERS_VP8;


typedef struct tagENCODE_SET_PICTURE_PARAMETERS_VP8 
{ 
    ENCODE_PICENTRY CurrOriginalPic; 
    ENCODE_PICENTRY CurrReconstructedPic; 
    ENCODE_PICENTRY LastRefPic; 
    ENCODE_PICENTRY GoldenRefPic; 
    ENCODE_PICENTRY AltRefPic; 
    
    union 
    { 
        UINT uPicFlags; 

        struct 
        { 
            UINT frame_type               : 1; 
            UINT version                  : 3;
            UINT show_frame               : 1;
            UINT color_space              : 1;
            UINT clamping_type            : 1;

            UINT segmentation_enabled       : 1; 
            UINT update_mb_segmentation_map : 1; 
            UINT update_segment_feature_data: 1;             
            UINT filter_type                : 1; 

            UINT loop_filter_adj_enable   : 1; 
            UINT CodedCoeffTokenPartition : 2;
            UINT refresh_golden_frame     : 1; 
            UINT refresh_alternate_frame  : 1; 
            UINT copy_buffer_to_golden    : 2;            
            UINT copy_buffer_to_alternate : 2;

            UINT sign_bias_golden         : 1; 
            UINT sign_bias_alternate      : 1; 
            UINT refresh_entropy_probs    : 1; 
            UINT refresh_last             : 1; 

            UINT mb_no_coeff_skip         : 1; 
            UINT forced_lf_adjustment     : 1;
            UINT ref_frame_ctrl           : 3;
            UINT                          : 3; 
        }; 
    }; 

    CHAR    loop_filter_level[4]; 
    CHAR    ref_lf_delta[4]; 
    CHAR    mode_lf_delta[4]; 
    UCHAR   sharpness_level;
    USHORT  RefFrameCost[4];
    UINT    StatusReportFeedbackNumber;
    UCHAR   ClampQindexHigh;
    UCHAR   ClampQindexLow;
    UCHAR   temporal_id;
} ENCODE_SET_PICTURE_PARAMETERS_VP8;


typedef struct tagENCODE_SET_QUANT_DATA_VP8
{
    UCHAR    QIndex[4];
    CHAR     QIndexDelta[5];
}ENCODE_SET_QUANT_DATA_VP8;

typedef struct tagENCODE_CPUPAK_FRAMEUPDATE_VP8
{
    UINT    PrevFrameSize;
    BOOL    TwoPrevFrameFlag;
    USHORT  RefFrameCost[4];
    USHORT  IntraModeCost[4];
    USHORT  InterModeCost[4];
    USHORT  IntraNonDCPenalty16x16;
    USHORT  IntraNonDCPenalty4x4;
} ENCODE_CPUPAK_FRAMEUPDATE_VP8;


#endif /* defined(_WIN32) || defined(_WIN64) */
#endif /* _ENCODER_DDI_HPP_ */
