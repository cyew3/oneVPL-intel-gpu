//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2013 Intel Corporation. All Rights Reserved.
//

#ifndef VIDEOVME_H
#define VIDEOVME_H

//#include "VideoDefs.h"
#include "mfxdefs.h"
#include "videovme7_5io.h"

/*********************************************************************************\
Error message
\*********************************************************************************/
#define        NO_ERR                0
#define        ERR_OK                0
#define        ERR_UNKNOWN            0x2000
#define        ERR_UNDERDEVELOP    0x2001
#define        ERR_UNSUPPORTED     0x2002
#define        ERR_ILLEGAL            0x2003
#define        ERR_DATAMISSING        0x2100
#define        ERR_FRAMESTATE      0x2101
#define        ERR_SEARCHPATH        0x2102
#define        ERR_MEMORY            0x2200
#define        WARNING                0x1000
#define        WRN_UNKNOWN            0x1000
#define        WRN_MV_OUTREF        0x1101

/*********************************************************************************\
Extra data types
\*********************************************************************************/

typedef struct 
{
    mfxU8    x,y;    // unsigned char pair
}mfxU8PAIR;

typedef mfxU8PAIR U8PAIR;
typedef mfxI16 I16;
typedef mfxU16 U16;
typedef mfxI16Pair mfxI16PAIR;
typedef mfxI16Pair I16PAIR;
typedef mfxU8  U8;
typedef mfxI32 I32;
typedef mfxU32 U32;
typedef int    Status;

/*********************************************************************************\
Constants for costing look-up-tables
\*********************************************************************************/
#define        LUTMODE_INTRA_NONPRED        0x00  // extra penalty for non-predicted modes
#define        LUTMODE_INTRA                0x01
#define        LUTMODE_INTRA_16x16            0x01
#define        LUTMODE_INTRA_8x8            0x02
#define        LUTMODE_INTRA_4x4            0x03
#define        LUTMODE_INTER_BWD            0x09
#define        LUTMODE_REF_ID                0x0A
#define        LUTMODE_INTRA_CHROMA        0x0B
#define        LUTMODE_INTER                0x08
#define        LUTMODE_INTER_16x16            0x08
#define        LUTMODE_INTER_16x8            0x04
#define        LUTMODE_INTER_8x16            0x04
#define        LUTMODE_INTER_8x8q            0x05
#define        LUTMODE_INTER_8x4q            0x06
#define        LUTMODE_INTER_4x8q            0x06
#define        LUTMODE_INTER_4x4q            0x07
#define        LUTMODE_INTER_16x8_FIELD    0x06
#define        LUTMODE_INTER_8x8_FIELD        0x07

#define     DISTBIT4X4      11
#define     DISTBIT4X8      12
#define     DISTBIT8X8      13
#define     DISTBIT8X16     14
#define     DISTBIT16X16    15

/*********************************************************************************/
// For input:
#define        DO_INTER            0x01
#define        DO_INTRA            0x02

// VmeFlags
#define        VF_PB_SKIP_ENABLE    0x01
#define        VF_ADAPTIVE_ENABLE    0x02
#define        VF_BIMIX_DISABLE    0x04
#define        VF_EARLY_IME_GOOD      0x20
#define        VF_T8X8_FOR_INTER    0x80    

// SrcType
#define        ST_SRC_16X16        0x00
#define        ST_SRC_16X8         0x01
#define        ST_SRC_8X16         0x02
#define        ST_SRC_8X8          0x03
#define        MBTYPE_REMAP_FWD    0x10
#define        MBTYPE_REMAP_BWD    0x20
#define        ST_SRC_FIELD        0x40
#define        ST_REF_FIELD        0x80

// VmeModes
#define        VM_MODE_SSS            0x00        // single starter, single path, single reference 
#define        VM_MODE_DSS            0x01        // dual starters,  single path, single reference 
#define        VM_MODE_DDS            0x03        // dual starters,  dual paths,  single reference
#define        VM_MODE_DDD            0x07        // dual starters,  dual paths,  dual references
#define        VM_2PATH_ENABLE        0x08
#define        VM_INTEGER_PEL        0x00
#define        VM_HALF_PEL            0x10
#define        VM_QUARTER_PEL        0x30
#define        VM_SKIP_8X8            0x40

// SadType
#define        INTER_CHROMA_MODE    0x01
#define        FWD_TX_SKIP_CHECK    0x02
#define        DISABLE_BME            0x04
#define        BLOCK_BASED_SKIP    0x08
#define        DIST_INTER_MSE        0x10    // not supported in HW 
#define        DIST_INTER_HAAR        0x20
#define        DIST_INTER_HADAMARD    0x30    // not supported in HW
#define        DIST_INTRA_MSE        0x40    // not supported in HW
#define        DIST_INTRA_HAAR        0x80
#define        DIST_INTRA_HADAMARD    0xC0    // not supported in HW

// ShapeMask
#define        SHP_NO_16X16        0x01
#define        SHP_NO_16X8         0x02
#define        SHP_NO_8X16         0x04
#define        SHP_NO_8X8          0x08
#define        SHP_NO_8X4            0x10
#define        SHP_NO_4X8          0x20
#define        SHP_NO_4X4          0x40
//#define    SHP_BIDIR_AVS        0x80

// BidirMask
#define        BID_NO_16X16          0x01
#define        BID_NO_16X8_8X16    0x02
#define        BID_NO_16X8          0x02
#define        BID_NO_8X16          0x02
#define        BID_NO_8X8          0x04
#define        BID_NO_MINORS          0x08
#define     UNIDIR_NO_MIX        0x10

// Intra Flags
#define        INTRA_NO_16X16        0x01
#define        INTRA_NO_8X8        0x02
#define        INTRA_NO_4X4        0x04
#define        S2D_ADD_LUTMV         0x20
#define        S2D_ADD_LUTMODE     0x40
#define        INTRA_TBSWAP_FLAG    0x80

// Intra Avail
#define        INTRA_AVAIL_AEF        0xE0
#define        INTRA_AVAIL_AE        0x60
#define        INTRA_AVAIL_F        0x80
#define        INTRA_AVAIL_E        0x40
#define        INTRA_AVAIL_A        0x20
#define        INTRA_AVAIL_B        0x10
#define        INTRA_AVAIL_C        0x08
#define        INTRA_AVAIL_D        0x04
 
//Multicall
#define        STREAM_OUT            0x01
#define        STREAM_IN            0x02

// Mv Flags 
#define        INTER_BILINEAR        0x04
#define        SRC_FIELD_POLARITY    0x08
#define        WEIGHTED_SAD_HAAR    0x10
#define        AC_ONLY_HAAR        0x20
#define        REFID_COST_MODE        0x40
#define        EXTENDED_FORM_FLAG    0x80
 
// Software Flags
#define        INTER_VME4TAP        0x00 
#define        INTER_AVC6TAP        0x02  // not in HW
#define        INTER_WMA4TAP        0x04  // NA 
#define        INTRA_WEIGH_ENABLE    0x10  // not in HW  
#define     OUTPUT_ALLMVS         0x80  // not in HW

//Intra Compute Flags
#define        INTRA_LUMA_CHROMA    0x00
#define        INTRA_LUMA_ONLY        0x01
#define        INTRA_DISABLED        0x02

/*********************************************************************************/
// For output:

// MbMode
#define     MODE_INTRA_16X16           0x00
#define     MODE_INTRA_8X8           0x10
#define     MODE_INTRA_4X4           0x20
#define     MODE_INTRA_PCM           0x30 // NA
#define     MODE_INTER_16X16        0x00
#define     MODE_INTER_16X8            0x01
#define     MODE_INTER_8X16            0x02
#define     MODE_INTER_8X8            0x03
#define     MODE_INTER_MINOR        0x03
#define     MODE_INTER_16X8_FIELD    0x09
#define     MODE_INTER_8X8_FIELD    0x0B
#define     MODE_SKIP_FLAG            0x04
#define     MODE_FIELD_FLAG            0x08
#define        MODE_BOTTOM_FIELD        0x80

// MbType
#define     TYPE_IS_INTRA           0x20
#define     TYPE_IS_FIELD            0x40
#define     TYPE_TRANSFORM8X8       0x80
#define     TYPE_INTRA_16X16           0x35 // 0x21-0x38
#define     TYPE_INTRA_8X8           0xA0
#define     TYPE_INTRA_4X4           0x20
#define     TYPE_INTRA_PCM           0x39 // NA
#define     TYPE_INTER_16X16_0        0x01
#define     TYPE_INTER_16X16_1        0x02
#define     TYPE_INTER_16X16_2        0x03
#define     TYPE_INTER_16X8         0x04 // even of 0x04-0x15
#define     TYPE_INTER_8X16         0x05 // odd of 0x04-0x15
#define     TYPE_INTER_8X8          0x16
#define     TYPE_INTER_OTHER           0x16
#define     TYPE_INTER_MIX_INTRA     0x16 // NA

// RefBorderMark
#define     BORDER_REF0_LEFT        0x01
#define     BORDER_REF0_RIGHT        0x02
#define     BORDER_REF0_TOP            0x04
#define     BORDER_REF0_BOTTOM        0x08
#define     BORDER_REF1_LEFT        0x10
#define     BORDER_REF1_RIGHT        0x20
#define     BORDER_REF1_TOP            0x40
#define     BORDER_REF1_BOTTOM        0x80

// VmeDecisionLog
#define     OCCURRED_IME_STOP        0x01
#define     CAPPED_MAXMV             0x02


#define     MVSZ_1_16X16P           0x10
#define     MVSZ_2_16X16B           0x20
#define     MVSZ_4_8X8P               0x30
#define     MVSZ_8_8X8B               0x40
#define     MVSZ_16_4X4P               0x50
#define     MVSZ_32_4X4B               0x60
#define     MVSZ_PACKED               0x70

#if 1
typedef enum
{
    Early_IME_Stop  = 0,
    Early_Skip_Exit = 1, 
    Early_Fme_Exit  = 2, 
    Ime_Too_Good    = 3, 
    Ime_Too_Bad     = 4,
    NoEarlyExit     = 5,
    NoQuitOnThreshold = 6
}ExitCriteria;
#endif

/*********************************************************************************/
//class VideoVME
/*********************************************************************************/
class VideoVME //: public VideoVMEInf
{
public:
    VideoVME( );
    ~VideoVME( );
    
    // Main VME call - Running ME Engine with outputs:
    Status    RunSIC( void *mfxVME7_5UNIIn, void *mfxVME7_5SICIn, void *mfxVME7_5UNIOutput);
    Status    RunIME( void *mfxVME7_5UNIIn, void *mfxVME7_5IMEIn, void *mfxVME7_5UNIOutput, void *mfxVME7_5IMEOutput);
    Status    RunFBR( void *mfxVME7_5UNIIn, void *mfxVME7_5FBRIn, void *mfxVME7_5UNIOutput);
    
private:
    mfxHDL    p;
};

#endif //VIDEOVME_H
