/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2009 Intel Corporation. All Rights Reserved.
//
*/

#if !defined(_LRB_VME_API_H_)
#define _LRB_VME_API_H_

#include "datatypes.h"

#ifdef __cplusplus
    extern "C" {
#endif

#if defined( _WIN32 ) || defined ( _WIN64 )
    #define __INT64     __int64
    #define __UINT64    unsigned __int64
    #define __ALIGN16   __declspec (align(16))
    #define __ALIGN8    __declspec (align(8))
    #define __ALIGN64   __declspec (align(64))
#elif defined(_FREEBSD)
    #define __INT64     long
    #define __UINT64    unsigned long
    #define __ALIGN16   __attribute__ ((aligned (16)))
    #define __ALIGN8    __attribute__ ((aligned (8)))
    #define __ALIGN64   __attribute__ ((aligned (64)))
#elif defined(_MCRT64)
    #define __INT64     int64
    #define __UINT64    uint64
    #define __ALIGN16   __attribute__ ((aligned (16)))
    #define __ALIGN8    __attribute__ ((aligned (8)))
    #define __ALIGN64   __attribute__ ((aligned (64)))
#else
    #error unsupported platform
#endif

typedef unsigned char   Ipp8u;
typedef unsigned short  Ipp16u;
typedef unsigned int    Ipp32u;
typedef signed char     Ipp8s;
typedef signed short    Ipp16s;
typedef signed int      Ipp32s;
typedef float           Ipp32f;
typedef __INT64         Ipp64s;
typedef __UINT64        Ipp64u;
typedef double          Ipp64f;
typedef void*           IppHandle;
typedef unsigned int    IppFlag;
typedef Ipp32u          IppBool;

typedef struct 
{
    Ipp32s width;
    Ipp32s height;
} IppiSize;


#define MAX_PIC_WIDTH   2048
#define MAX_PIC_HEIGHT  2048
#define MAX_REF_FRAMES  16

#define MAKELRBFOURCC(a, b, c, d)   (((Ipp32u)(d << 24)) | ((Ipp32u)(c << 16)) | ((Ipp32u)(b << 8))| ((Ipp32u)a)) 

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define codec type
///////////////////////////////////////////////////////////////////////////////
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
// this structure is used to define VME capabilities 
///////////////////////////////////////////////////////////////////////////////

typedef struct 
{
    union 
    {
        struct 
        {
            Ipp32u  CodingLimitSet      : 1;
            Ipp32u  NoFieldFrame          : 1;
            Ipp32u  NoCabacSupport        : 1;
            Ipp32u  NoGapInFrameCount     : 1;
            Ipp32u  NoGapInPicOrder     : 1;
            Ipp32u  NoUnpairedField     : 1;
            Ipp32u  BitDepth8Only         : 1;
            Ipp32u  ConsecutiveMBs         : 1;
            Ipp32u  Color420Only        : 1;
            Ipp32u  OneSliceOnly         : 1;
            Ipp32u  RowSliceOnly        : 1;
            Ipp32u  SliceIPOnly         : 1;
            Ipp32u  SliceIPBOnly         : 1;
            Ipp32u  NoWeightedPred         : 1;
            Ipp32u  NoMinorMVs             : 1;
            Ipp32u  ClosedGopOnly         : 1;
            Ipp32u  NoFrameCropping     : 1;
            Ipp32u  Reserved1           : 15;
        };
        Ipp32u CodingLimits;
    };
    union 
    {
        struct 
        {
            Ipp16u  EncFunc                : 1;
            Ipp16u  PakFunc                : 1;
            Ipp16u  EncodeFunc            : 1;  // Enc+Pak
            Ipp16u  InputAnalysisFunc    : 1;  // Reserved for now
            Ipp16u  Reserved2           : 12;
        };
        Ipp16u CodingFunction;
    };
    Ipp32u    MaxPicWidth;
    Ipp32u  MaxPicHeight;
    Ipp8u    MaxNum_Reference;
    Ipp8u   MaxNum_SPS_id;
    Ipp8u    MaxNum_PPS_id;
} LrbEncodeCaps;



////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define the block size for intra prediction. they
// are used as bit flags to control what block size will be checked for intra
// prediction. 
///////////////////////////////////////////////////////////////////////////////
enum 
{
    LRB_ENC_INTRA_BLOCK_NONE    = 0,
    LRB_ENC_INTRA_BLOCK_4x4     = (1 << 0),
    LRB_ENC_INTRA_BLOCK_8x8     = (1 << 1),
    LRB_ENC_INTRA_BLOCK_16x16   = (1 << 2),
    LRB_ENC_INTRA_BLOCK_PCM     = (1 << 3),
    LRB_ENC_INTRA_BLOCK_ALL     = (LRB_ENC_INTRA_BLOCK_4x4  | 
                                   LRB_ENC_INTRA_BLOCK_8x8  | 
                                   LRB_ENC_INTRA_BLOCK_16x16|
                                   LRB_ENC_INTRA_BLOCK_PCM),
};


////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define the cost type used in intra/inter prediction. 
///////////////////////////////////////////////////////////////////////////////
enum 
{
    LRB_ENC_COST_TYPE_NONE          = 0,
    LRB_ENC_COST_TYPE_SAD           = (1 << 0), //sum of absolute difference
    LRB_ENC_COST_TYPE_SSD           = (1 << 1), //sum of squared difference
    LRB_ENC_COST_TYPE_SATD_HADAMARD = (1 << 2), //sum of absolute hadamard transformed difference
    LRB_ENC_COST_TYPE_SATD_HARR     = (1 << 3), //sum of absolute harr transformed difference    
    LRB_ENC_COST_TYPE_PROPRIETARY   = (1 << 4), //propriteary cost type    
};


////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define the entropy coding type used for RDO
// intra/inter prediction. 
///////////////////////////////////////////////////////////////////////////////
enum 
{
    LRB_ENC_ENTROPY_CODING_VLC      = 0,    //vlc
    LRB_ENC_ENTROPY_CODING_CABAC    = 1,    //cabac
};

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define the integer pel search algorithm.
///////////////////////////////////////////////////////////////////////////////
enum
{
    LRB_ENC_INTER_SEARCH_TYPE_NONE              = 0,
    LRB_ENC_INTER_SEARCH_TYPE_FULL              = (1 << 0),            
    LRB_ENC_INTER_SEARCH_TYPE_UMH               = (1 << 1),
    LRB_ENC_INTER_SEARCH_TYPE_LOG               = (1 << 2),
    LRB_ENC_INTER_SEARCH_TYPE_TTS               = (1 << 3),
    LRB_ENC_INTER_SEARCH_TYPE_SQUARE            = (1 << 4),
    LRB_ENC_INTER_SEARCH_TYPE_DIAMOND           = (1 << 5),
    LRB_ENC_INTER_SEARCH_TYPE_PROPRIETARY       = (1 << 6),
};

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define the fractional pel search algorithm.
///////////////////////////////////////////////////////////////////////////////
enum
{
    LRB_ENC_FRACTIONAL_SEARCH_TYPE_NONE             = 0,
    LRB_ENC_FRACTIONAL_SEARCH_TYPE_FULL             = (1 << 0),
    LRB_ENC_FRACTIONAL_SEARCH_TYPE_HALF             = (1 << 1),
    LRB_ENC_FRACTIONAL_SEARCH_TYPE_SQUARE           = (1 << 2),
    LRB_ENC_FRACTIONAL_SEARCH_TYPE_HQ               = (1 << 3),
    LRB_ENC_FRACTIONAL_SEARCH_TYPE_DIAMOND          = (1 << 4),
};

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define the inter prediction block size. they are
// used as bit flags to control what block size should be checked in inter 
// prediction.
// this also controls how many MV can exist in one MB.
///////////////////////////////////////////////////////////////////////////////
enum
{
    LRB_ENC_INTER_BLOCK_SIZE_NONE       = 0,
    LRB_ENC_INTER_BLOCK_SIZE_16x16      = (1 << 0),
    LRB_ENC_INTER_BLOCK_SIZE_16x8       = (1 << 1),
    LRB_ENC_INTER_BLOCK_SIZE_8x16       = (1 << 2),
    LRB_ENC_INTER_BLOCK_SIZE_8x8        = (1 << 3),
    LRB_ENC_INTER_BLOCK_SIZE_8x4        = (1 << 4),
    LRB_ENC_INTER_BLOCK_SIZE_4x8        = (1 << 5),
    LRB_ENC_INTER_BLOCK_SIZE_4x4        = (1 << 6),

    //all possible size 
    LRB_ENC_INTER_BLOCK_SIZE_ALL        = (LRB_ENC_INTER_BLOCK_SIZE_16x16 | 
                                       LRB_ENC_INTER_BLOCK_SIZE_16x8  |
                                       LRB_ENC_INTER_BLOCK_SIZE_8x16  | 
                                       LRB_ENC_INTER_BLOCK_SIZE_8x8   |
                                       LRB_ENC_INTER_BLOCK_SIZE_8x4   | 
                                       LRB_ENC_INTER_BLOCK_SIZE_4x8   |
                                       LRB_ENC_INTER_BLOCK_SIZE_4x4),
                                       
    //blocks are bigger than or equal 8x8 
    LRB_ENC_INTER_BLOCK_SIZE_NO_MINOR   = (LRB_ENC_INTER_BLOCK_SIZE_16x16 | 
                                       LRB_ENC_INTER_BLOCK_SIZE_16x8  |
                                       LRB_ENC_INTER_BLOCK_SIZE_8x16  | 
                                       LRB_ENC_INTER_BLOCK_SIZE_8x8),

    //VC-1 4-MV mode, for P frame only
    LRB_ENC_INTER_BLOCK_SIZE_VC1_4MV    = (LRB_ENC_INTER_BLOCK_SIZE_16x16 | 
                                       LRB_ENC_INTER_BLOCK_SIZE_8x8),

    //VC-1 1-MV mode, for P and B frame
    LRB_ENC_INTER_BLOCK_SIZE_VC1_1MV    = LRB_ENC_INTER_BLOCK_SIZE_16x16,

    //MPEG2 1-MV mode, for P and B frame
    LRB_ENC_INTER_BLOCK_SIZE_MPEG_1MV   = LRB_ENC_INTER_BLOCK_SIZE_16x16,

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
    LRB_ENC_INTERPOLATION_TYPE_NONE          = 0,   
    LRB_ENC_INTERPOLATION_TYPE_VME4TAP       = (1 << 0),
    LRB_ENC_INTERPOLATION_TYPE_BILINEAR      = (1 << 1),
    LRB_ENC_INTERPOLATION_TYPE_WMV4TAP       = (1 << 2),
    LRB_ENC_INTERPOLATION_TYPE_AVC6TAP       = (1 << 3),
};

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define the MV precision.
///////////////////////////////////////////////////////////////////////////////
enum
{
    LRB_ENC_MV_PRECISION_NONE           = 0,
    LRB_ENC_MV_PRECISION_INTEGER        = (1 << 0),
    LRB_ENC_MV_PRECISION_HALFPEL        = (1 << 1),
    LRB_ENC_MV_PRECISION_QUARTERPEL     = (1 << 2),
};

////////////////////////////////////////////////////////////////////////////////
// this structure is used to define the control structure for VME.
///////////////////////////////////////////////////////////////////////////////
typedef struct
{
    Ipp32u    IntraPredBlockSize        : 4;
    Ipp32u    IntraPredCostType        : 5; 
    Ipp32u    InterPredBlockSize        : 8;
    Ipp32u    MVPrecision                : 4;
    Ipp32u    MECostType                : 5; 
    Ipp32u    MESearchType            : 8; 
    Ipp32u    MVSearchWindowX            : 16;
    Ipp32u    MVSearchWindowY         : 16; 
    Ipp32u    MEInterpolationMethod    : 4;
    Ipp32u  MEFractionalSearchType  : 5;
    Ipp32u    MaxMVs                    : 6;
    Ipp32u    SkipCheck                : 1;
    Ipp32u    DirectCheck                : 1;
    Ipp32u    BiDirSearch                : 1;
    Ipp32u    MBAFF                    : 1;
    Ipp32u    FieldPrediction            : 1;
    Ipp32u    RefOppositeField        : 1;
    Ipp32u    ChromaInME                : 1;
    Ipp32u    WeightedPrediction        : 1; 
    Ipp32u    RateDistortionOpt        : 1;
    Ipp32u    MVPrediction            : 1; 
    Ipp32u  UseLNI                  : 1;
    Ipp32u  Multithreaded           : 1;
    Ipp32u  OutputFakeData          : 1;
} LrbVMEControls;

////////////////////////////////////////////////////////////////////////////////
// this structure is used to define the sequence level parameters
///////////////////////////////////////////////////////////////////////////////
typedef struct
{
    Ipp16u     FrameWidth;
    Ipp16u    FrameHeight;
    Ipp8u    Profile;
    Ipp8u    Level;

    union 
    {
        // AVC
        struct 
        {
            Ipp8u    SeqParameterSetId;
            Ipp8u    ChromaFormatIdc;
            Ipp8u    BitDepthLumaMinus8;
            Ipp8u    BitDepthChromaMinus8;
            Ipp8u    Log2MaxFrameNumMinus4;
            Ipp8u    PicOrderCntType;
            Ipp8u    Log2MaxPicOrderCntLsbMinus4;
            Ipp8u   Padding0;
            Ipp16u  Padding1;
            Ipp16u    SeqScalingMatrixPresentFlag        : 1;
            Ipp16u    SeqScalingListPresentFlag        : 1;
            Ipp16u    DeltaPicOrderAlwaysZeroFlag         : 1;
            Ipp16u    FrameMbsOnlyFlag                 : 1;
            Ipp16u    Direct8x8InferenceFlag            : 1;
            Ipp16u    VuiParametersPresentFlag        : 1;
            Ipp16u    Reserved1                           : 10; 
        };
    };

    Ipp16u    GopPicSize;
    Ipp8u    GopRefDist;
    Ipp8u    GopOptFlag : 2;
    Ipp8u    Reserved2  : 6;
    Ipp8u    TargetUsage;
    Ipp8u    RateControlMethod;
    Ipp16u    TargetBitRate;
    Ipp16u    MaxBitRate;
    Ipp8u    NumRefFrames;
} SequenceParameterSetH264;

typedef struct
{
    Ipp16u     FrameWidth;
    Ipp16u    FrameHeight;
    Ipp8u    Profile;
    Ipp8u    Level;
} SequenceParameterSetMPEG2;

typedef struct 
{
    union 
    {
        struct 
        {
            Ipp8u Index7Bits        : 7;
            Ipp8u AssociatedFlag    : 1;
        };
        Ipp8u PicEntry;
    };
}  PicEntry;

////////////////////////////////////////////////////////////////////////////////
// this structure is used to define the picture level parameters
///////////////////////////////////////////////////////////////////////////////
typedef struct
{
    PicEntry     CurrOriginalPic;
    PicEntry     CurrReconstructedPic;
    Ipp8u        CodingType;
    Ipp8u        PicStructure;
    Ipp8u        NumSlice;

    union 
    {
        // AVC
        struct 
        {
            Ipp8u    bIdrPic;
            Ipp8u    PicParameterSetId;
            Ipp8u    SeqParameterSetId;
            Ipp8u    NumRefIdxL0ActiveMinus1;
            Ipp8u    NumRefIdxL1ActiveMinus1;
            Ipp8s    ChromaQpIndexOffset;
            Ipp8s    SecondChromaQpIndexOffset;
            Ipp8u   Padding0;
            Ipp16u  Padding1;
            Ipp16u    EntropyCodingModeFlag            : 1;
            Ipp16u    PicOrderPresentFlag                : 1;
            Ipp16u     WeightedPredFlag                : 1;
            Ipp16u     WeightedBipredIdc                : 2;
            Ipp16u  ConstrainedIntraPredFlag         : 1;
            Ipp16u    Transform8x8ModeFlag            : 1;
            Ipp16u    PicScalingMatrixPresentFlag        : 1;
            Ipp16u    PicScalingListPresentFlag        : 1;
            Ipp16u  RefPicFlag                         : 1;
            Ipp16u  Reserved1                          : 6;
        };
    };

    Ipp8s        QpY;
    IppBool     bUseOrignalAsRef;
    PicEntry       RefFrameList[16];
    Ipp32u        UsedForReferenceFlags;
    Ipp32s        CurrFieldOrderCnt[2];
    Ipp32s        FieldOrderCntList[16][2];
    Ipp16u        FrameNum;
    IppBool        bLastPictureInSequence;
    IppBool     bLastPictureInStream;
} PictureParameterSetH264;

typedef struct
{
    PicEntry     CurrOriginalPic;
    PicEntry     CurrReconstructedPic;
    Ipp8u        CodingType;
    Ipp8u        PicStructure;
    Ipp8u        NumSlice;
} PictureParameterSetMPEG2;

////////////////////////////////////////////////////////////////////////////////
// this structure is used define the slice level parameters. 
///////////////////////////////////////////////////////////////////////////////
typedef struct
{
    Ipp16u    FirstMbInSlice;
    Ipp16u    NumMbsForSlice;
    Ipp8u    SliceType;

    union 
    {
        // AVC
        struct 
        {
            Ipp8u    PicParameterSetId; 
            Ipp8u   padding0[3];
            Ipp16u    DirectSpatialMvPredFlag        : 1;
            Ipp16u    NumRefIdxActiveOverrideFlag    : 1;
            Ipp16u    LongTermReferenceFlag        : 1;
            Ipp16u    Reserved1                    : 13;
            Ipp16u  padding1;
            Ipp16u    IdrPicId;
            Ipp16u    PicOrderCntLsb;
            Ipp32s    DeltaPicOrderCntBottom;
            Ipp32s    DeltaPicOrderCnt[2];
            Ipp8u   NumRefIdxL0ActiveMinus1;
            Ipp8u   NumRefIdxL1ActiveMinus1;
            Ipp8u    LumaLog2WeightDenom;
            Ipp8u    ChromaLog2WeightDenom;
            Ipp8u    CabacInitIdc;
            Ipp8u    DisableDeblockingFilterIdc;
            Ipp8s    SliceAlphaC0OffsetDiv2;
            Ipp8s    SliceBetaOffsetDiv2;
        };
    };

    PicEntry    RefPicList[2][32];
    Ipp16s        Weights[2][32][3][2];
    Ipp16u        SliceId;
} SliceParameterSetH264;

typedef struct
{
    Ipp16u    FirstMbInSlice;
    Ipp16u    NumMbsForSlice;
    Ipp8u    SliceType;
} SliceParameterSetMPEG2;

////////////////////////////////////////////////////////////////////////////////
// this structure is used to define the MB output data for PAK engine
///////////////////////////////////////////////////////////////////////////////
typedef struct
{
    //DW0 ~ DW7
    Ipp16u ReservedNOOP1;
    Ipp8u  RefFieldOrderCntLsb_L0_0;
    Ipp8u  RefFieldOrderCntLsb_L1_0;
    Ipp16u ReservedNOOP2;
    Ipp8u  RefFieldOrderCntLsb_L0_1;
    Ipp8u  RefFieldOrderCntLsb_L1_1;
    Ipp16u ReservedNOOP3;
    Ipp8u  RefFieldOrderCntLsb_L0_2;
    Ipp8u  RefFieldOrderCntLsb_L1_2;
    Ipp16u ReservedNOOP4;
    Ipp8u  RefFieldOrderCntLsb_L0_3;
    Ipp8u  RefFieldOrderCntLsb_L1_3;
    Ipp32u ReservedNOOP;
    Ipp32u Reserved1[3];

    //DW 8
    union
    {
        struct 
        {
            Ipp32u  InterMbMode            : 2;
            Ipp32u  SkipMbFlag            : 1;
            Ipp32u  MBZ1                : 1;
            Ipp32u  IntraMbMode            : 2;
            Ipp32u    MBZ2                : 1;
            Ipp32u  FieldMbPolarityFlag    : 1;
            Ipp32u  MbType                : 5;
            Ipp32u  IntraMbFlag            : 1;
            Ipp32u  FieldMbFlag            : 1;
            Ipp32u  Transform8x8Flag    : 1;
            Ipp32u  MBZ3                : 1;
            Ipp32u  CbpDcV                : 1;
            Ipp32u  CbpDcU                : 1;
            Ipp32u  CbpDcY                : 1;
            Ipp32u  MvFormat             : 3;
            Ipp32u  MBZ4                : 1;
            Ipp32u  PackedMvNum         : 6;
            Ipp32u  MBZ5                : 2;
        };
        Ipp32u dwMBtype;
    };

    //DW 9
    Ipp8u  MbYCnt;
    Ipp8u  MbXCnt;
    Ipp16u Cbp4x4Y;
    
    //DW10
    Ipp16u Cbp4x4V;
    Ipp16u Cbp4x4U;

    //DW 11
    struct
    {
        Ipp32u QpPrimeY            : 8;
        Ipp32u MBZ6             : 17;
        Ipp32u SkipMbConvDisable: 1;
        Ipp32u LastMbFlag        : 1;
        Ipp32u EnableCoeffClamp    : 1;
        Ipp32u Skip8x8Pattern     : 4;
    };

    //DW 12 ~ DW 14
    union
    {
        // Intra MBs
        struct
        {
            Ipp16u  LumaIntraPredModes[4];
            union
            {
                struct
                {
                    Ipp32u  ChromaIntraPredMode : 2;
                    Ipp32u  IntraPredAvilFlagD  : 1;    //above left
                    Ipp32u  IntraPredAvilFlagC  : 1;    //above right
                    Ipp32u  IntraPredAvilFlagB  : 1;    //above
                    Ipp32u  IntraPredAvilFlagE  : 1;    //left bottom half
                    Ipp32u  IntraPredAvilFlagA  : 1;    //left top half
                    Ipp32u  IntraPredAvilFlagF  : 1;    //left row (-1, 7)
                };
                Ipp8u  IntraStruct;
            };
            Ipp32u  MBZ7        : 24;
        };

        // Inter MBs
        struct
        {
            Ipp8u     SubMbShape;
            Ipp8u     SubMbPredMode;
            Ipp16u    MBZ8;
            Ipp8u   RefIdxL0[4];
            Ipp8u   RefIdxL1[4];
        };
    };

    //DW 15
    Ipp16u MBZ9;
    Ipp8u  TargetSizeInWord;
    Ipp8u  MaxSizeInWord;
} LrbVMEMBDataH264;


typedef struct
{
    Ipp16s  x;
    Ipp16s  y;
} MV;

typedef struct
{
    Ipp32s Dummy;
    
} LrbVMEMBDataMPEG2;

////////////////////////////////////////////////////////////////////////////////
// this structure is used to define the motion vectors for one MB. 
///////////////////////////////////////////////////////////////////////////////
typedef struct
{
    MV  MVL0[16];
    MV  MVL1[16];
} LrbVMEMBMvH264;

////////////////////////////////////////////////////////////////////////////////
// this enumeration is used to define the input picture structure. VME can only
// support frame picture encoding type, i.e, in the interlaced case, input can 
// only be interleaved fields, and they are encoded together. VME doesn't support
// individual field input and doesn't support encoding individual pield picture.
///////////////////////////////////////////////////////////////////////////////
typedef enum
{
    PICSTRUCT_PROGRESSIVE   = 0,        //progressive frame
    PICSTRUCT_INTERLACED_TFF,           //interlaced frame, top field first
    PICSTRUCT_INTERLACED_BFF,           //interlaced frame, bottom field first
} PicStructType;

////////////////////////////////////////////////////////////////////////////////
// this struct is used to define the surface information. only NV12 is supported
// surface can be padded.
///////////////////////////////////////////////////////////////////////////////
typedef struct 
{
    Ipp32u          SurfaceFourCC;      //fourcc code for codec, must be NV12
    Ipp16u          PicWidth;           //picture width
    Ipp16u          PicHeight;          //picture height
    Ipp16u          YStride;            //stride for Y plane in byte. 
    Ipp16u          UVStride;           //stride for UV plane in byte.
    Ipp32u          PicIndex;           //pic index in the surface buffer list, this is maintained by the caller of VME
    Ipp8u           *Y;                 //pointer to the upper left of Y plane
    Ipp8u           *U;                 //pointer to the upper left of U plane
    Ipp8u           *V;                 //pointer to the upper left of V plane
    Ipp8u           *pSurf;             //pointer to the start address of the whole surface
    PicStructType   PicStruct;          //progressive or interlaced frame
} LrbVMESurfaceInfo;


////////////////////////////////////////////////////////////////////////////////
// LrbVMEQueryCaps function is used to query Lrb encode capabilities
//
// params:
//      CodecType   : codec types
//      VMEControls : VME control caps
///////////////////////////////////////////////////////////////////////////////
Result_t LrbVMEQueryCaps(
    IN   Ipp32u         CodecType,
    OUT  LrbVMEControls *VMEControls);

////////////////////////////////////////////////////////////////////////////////
// LrbVMEQueryMemorySize function is used to query the memory requirement for the 
// VME engine. this memory doesn't include any memory for picture surface and 
// prdiction buffer. this memory is intended to be used for VME to maintain internal
// data structure only.
//
// params:
//      CodecType   : codec type.
//      SPS         : sequence parameter set
//      VMEControls : VME control setting
//      NumSlice    : number of slices per frame
//      MemSize     : returned memory size information.
//
///////////////////////////////////////////////////////////////////////////////
Result_t LrbVMEQueryMemorySize(
    IN  Ipp32u                  CodecType,
    IN  void                    *SPS,
    IN  LrbVMEControls          *VMEControls, 
    IN  Ipp32u                  NumSlice,
    OUT Ipp32u                  *MemSize);

////////////////////////////////////////////////////////////////////////////////
// LrbVMECreateEngine function is used to create the VME engine. the memory required
// should be allocated by the application. 
//
// params:
//      CodecType   : codec type
//      SPS         : sequence parameter set. 
//      VMEControls : VME control setting
//      NumSlice    : number of slice per frame
//      Mem         : memory that has been allocated by application for VME engine to use.
//      MemSize     : size of the memory allocated
//      LrbVMEEngine: handle to the VME engine
///////////////////////////////////////////////////////////////////////////////
Result_t LrbVMECreateEngine(
    IN  Ipp32u                  CodecType,
    IN  void                    *SPS,
    IN  LrbVMEControls          *VMEControls,
    IN  Ipp32u                  NumSlice,
    IN  Ipp8u                   *Mem,
    IN  Ipp32u                  MemSize,
    OUT IppHandle               *LrbVMEEngine);

////////////////////////////////////////////////////////////////////////////////
// LrbVMEDestroyEngine function is used to destroy the VME engine. the memory used 
// by VME engine should be released by the application after the VME engine is destroyed. 
//
// params:
//      LrbVMEEngine   : handle to the VME engine
///////////////////////////////////////////////////////////////////////////////
Result_t LrbVMEDestroyEngine(
    IN  IppHandle LrbVMEEngine);

////////////////////////////////////////////////////////////////////////////////
// LrbVMEPredProcess function is used to pre process an input picture and collect the statistic data
// 
//
// params:
//      LrbVMEEngine    : handle to the VME engine
//      SPS             : sequence parameter set
//      PPS             : picture parameter set
//      SliceParameter  : slice parameter array
//      VMEControls     : VME controls.
//      CurPic          : current picture surface
//      RefPicList      : reference picture surface list
//      RefPicMBData    : reference picture MB data surface list
//      RefPicMv        : reference picture MV surface list
//
///////////////////////////////////////////////////////////////////////////////
Result_t LrbVMEProcess(
    IN  IppHandle               LrbVMEEngine, 
    IN  void                    *SPS,
    IN  void                    *PPS,
    IN  void                    *SliceParameterList,
    IN  LrbVMEControls          *VMEControls,
    IN  LrbVMESurfaceInfo       *CurPic,
    IN  LrbVMESurfaceInfo       **RefPicList,
    IN  void                    **RefPicMBDataList,
    IN  void                    **RefPicMvList);

#ifdef __cplusplus
    }
#endif

#endif