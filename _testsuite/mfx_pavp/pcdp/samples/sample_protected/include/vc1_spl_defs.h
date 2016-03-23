//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010-2016 Intel Corporation. All Rights Reserved.
//
//
//*/


#ifndef _VC1_SPL_DEFS_H__
#define _VC1_SPL_DEFS_H__

#include "abstract_splitter.h"
#include <vector>
namespace ProtectedLibrary
{

        typedef enum
    {
        VC1_STS_ERR_NONE          = 0,
        VC1_STS_END_OF_STREAM     = 1,
        VC1_STS_NOT_ENOUGH_DATA   = 2,
        VC1_STS_NOT_ENOUGH_BUFFER = 3,
        VC1_STS_INVALID_STREAM    = 4

    } VC1_Sts_Type;

    typedef enum
    {
        VC1_EndOfSequence              = 0x0A,
        VC1_Slice                      = 0x0B,
        VC1_Field                      = 0x0C,
        VC1_FrameHeader                = 0x0D,
        VC1_EntryPointHeader           = 0x0E,
        VC1_SequenceHeader             = 0x0F,
        VC1_SliceLevelUserData         = 0x1B,
        VC1_FieldLevelUserData         = 0x1C,
        VC1_FrameLevelUserData         = 0x1D,
        VC1_EntryPointLevelUserData    = 0x1E,
        VC1_SequenceLevelUserData      = 0x1F
    } VC1StartCode;

    struct VC1_StartCode
    {
        mfxU32 value;
        mfxU32 offset;
    };

    enum
    {
        BUFFER_SIZE = 1024 * 1024,
        MAX_START_CODE_NUM = 512
    };

    #define IS_VC1_USER_DATA(x) (x <= VC1_SequenceLevelUserData && x >= VC1_SliceLevelUserData)

typedef struct
{
    mfxU32*     pBitstream;
    mfxI32      bitOffset;
} VC1SplBitstream;

//frame coding mode
enum
{
    VC1_Progressive           = 0,
    VC1_FrameInterlace        = 1,
    VC1_FieldInterlace        = 2
};

enum
{
    VC1_I_FRAME = 0,
    VC1_P_FRAME = 1,
    VC1_B_FRAME = 2,
    VC1_BI_FRAME= 3,
    VC1_SKIPPED_FRAME = 4
};

enum //bitplane modes definitions
{
    VC1_BITPLANE_RAW_MODE        = 0,//Raw           0000
    VC1_BITPLANE_NORM2_MODE      = 1,//Norm-2        10
    VC1_BITPLANE_DIFF2_MODE      = 2,//Diff-2        001
    VC1_BITPLANE_NORM6_MODE      = 3,//Norm-6        11
    VC1_BITPLANE_DIFF6_MODE      = 4,//Diff-6        0001
    VC1_BITPLANE_ROWSKIP_MODE    = 5,//Rowskip       010
    VC1_BITPLANE_COLSKIP_MODE    = 6 //Colskip       011
};

enum
{
    VC1_COND_OVER_FLAG_NONE = 0, //no 8x8 block boundaries are smoothed,
                             //see standart, p163
    VC1_COND_OVER_FLAG_ALL = 2,  //all 8x8 block boundaries are smoothed,
                             //see standart, p163
    VC1_COND_OVER_FLAG_SOME = 3  //some 8x8 block boundaries are smoothed,
                             //see standart, p163
};

//for extended differantial MV range flag(inerlace P picture)
enum
{
    VC1_DMVRANGE_NONE = 0,
    VC1_DMVRANGE_HORIZONTAL_RANGE,
    VC1_DMVRANGE_VERTICAL_RANGE,
    VC1_DMVRANGE_HORIZONTAL_VERTICAL_RANGE
};

#define VC1_MAX_BITPANE_CHUNCKS 7

enum
{
    VC1_ALTPQUANT_LEFT            = 1,
    VC1_ALTPQUANT_TOP             = 2,
    VC1_ALTPQUANT_RIGTHT          = 4,
    VC1_ALTPQUANT_BOTTOM          = 8,
    VC1_ALTPQUANT_LEFT_TOP        = 3,
    VC1_ALTPQUANT_TOP_RIGTHT      = 6,
    VC1_ALTPQUANT_RIGTHT_BOTTOM   = 12,
    VC1_ALTPQUANT_BOTTOM_LEFT     = 9,
    VC1_ALTPQUANT_EDGES           = 15,
    VC1_ALTPQUANT_ALL             = 16,
    VC1_ALTPQUANT_NO              = 0,
    VC1_ALTPQUANT_MB_LEVEL        = 32,
    VC1_ALTPQUANT_ANY_VALUE       = 64
};

enum
{
    VC1_DQPROFILE_ALL4EDGES  = 0,    //00    All four Edges
    VC1_DQPROFILE_DBLEDGES   = 1,    //01    Double Edges
    VC1_DQPROFILE_SNGLEDGES  = 2,    //10    Single Edges
    VC1_DQPROFILE_ALLMBLKS   = 3     //11    All Macroblocks
};

enum
{
    VC1_MVMODE_HPELBI_1MV    = 0,    //0000      1 MV Half-pel bilinear
    VC1_MVMODE_1MV           = 1,    //1         1 MV
    VC1_MVMODE_MIXED_MV      = 2,    //01        Mixed MV
    VC1_MVMODE_HPEL_1MV      = 3,    //001       1 MV Half-pel
    VC1_MVMODE_INTENSCOMP    = 4     //0001      Intensity Compensation
};

#define VC1_IS_INT_TOP_FIELD(value)    (value & VC1_INTCOMP_TOP_FIELD)
#define VC1_IS_INT_BOTTOM_FIELD(value) (value & VC1_INTCOMP_BOTTOM_FIELD)
#define VC1_IS_INT_BOTH_FIELD(value)   (value & VC1_INTCOMP_BOTH_FIELD)

enum
{
    VC1_BLK_INTER8X8   = 0x1,
    VC1_BLK_INTER8X4   = 0x2,
    VC1_BLK_INTER4X8   = 0x4,
    VC1_BLK_INTER4X4   = 0x8,
    VC1_BLK_INTER      = 0xf,
    VC1_BLK_INTRA_TOP  = 0x10,
    VC1_BLK_INTRA_LEFT = 0x20,
    VC1_BLK_INTRA      = 0x30
};

//intensity comprnsation
enum
{
    VC1_INTCOMP_TOP_FIELD    = 1,
    VC1_INTCOMP_BOTTOM_FIELD = 2,
    VC1_INTCOMP_BOTH_FIELD   = 3
};

enum //quantizer deadzone definitions
{
    VC1_QUANTIZER_UNIFORM    = 0,
    VC1_QUANTIZER_NONUNIFORM = 1
};


typedef struct
{
    mfxU8              m_invert;
    mfxI32             m_imode;
    mfxU8*             m_databits;
}VC1SplBitplane;

#define VC1_MAX_BITPANE_CHUNCKS 7

typedef struct
{
    //common field
    mfxU32 PROFILE;            //2

    //Advanced profile fields
    mfxU32 LEVEL;              //3

    //common fields
    mfxU32 FRMRTQ_POSTPROC;  //3
    mfxU32 BITRTQ_POSTPROC;  //5

    mfxU32 FRAMERATENR;  //8
    mfxU32 FRAMERATEDR;  //4

    //Advanced profile fields
    mfxU32 POSTPROCFLAG;      //1
    mfxU32 MAX_CODED_WIDTH;   //12
    mfxU32 MAX_CODED_HEIGHT;  //12
    mfxI32 AspectRatioW;
    mfxI32 AspectRatioH;
    mfxU32 PULLDOWN;          //1
    mfxU32 INTERLACE;         //1
    mfxU32 TFCNTRFLAG;        //1

    //Simple/Main profile fields
    mfxU32 LOOPFILTER;       //1 uimsbf
    mfxU32 MULTIRES;         //1 uimsbf
    mfxU32 FASTUVMC;         //1 uimsbf
    mfxU32 EXTENDED_MV;      //1 uimsbf
    mfxU32 DQUANT;           //2 uimsbf
    mfxU32 VSTRANSFORM;      //1 uimsbf
    mfxU32 OVERLAP;          //1 uimsbf
    mfxU32 SYNCMARKER;       //1 uimsbf
    mfxU32 RANGERED;         //1 uimsbf
    mfxU32 MAXBFRAMES;       //3 uimsbf
    mfxU32 QUANTIZER;        //2 uimsbf

    //common fields
    mfxU32 FINTERPFLAG;        //1
    mfxU32 HRD_PARAM_FLAG;     //1

    //HRD PARAMS
    mfxU32 HRD_NUM_LEAKY_BUCKETS; //5

    mfxU16 widthMB;
    mfxU16 heightMB;

    mfxU32 IsResize;
//entry point
    mfxU32 BROKEN_LINK;
    mfxU32 CLOSED_ENTRY;
    mfxU32 PANSCAN_FLAG;
    mfxU32 REFDIST_FLAG;

    mfxU32 CODED_WIDTH;
    mfxU32 CODED_HEIGHT;
    mfxU32 EXTENDED_DMV;
    mfxU32 RANGE_MAPY_FLAG;
    mfxI32 RANGE_MAPY;
    mfxU32 RANGE_MAPUV_FLAG;
    mfxI32 RANGE_MAPUV;
    mfxU32 RNDCTRL;     // 1 rounding control bit
    mfxI32 bp_round_count;
    VC1SplBitplane pBitplane;
}VC1SequenceHeader;

typedef struct
{
  //common fields
    mfxU32  PTYPE;
    mfxU32  PQINDEX;
    mfxU32  HALFQP;
//common fields.Slice parameters
    mfxU32  is_slice;

//interlace fields
    mfxU8  TFF;           // 1 top field first
    mfxU8  RFF;           // repeat field
    mfxU8  PTypeField1;
    mfxU8  PTypeField2;
    mfxU8  CurrField;
    mfxU8  BottomField;    //Is current field top or nor
    mfxU32  INTCOMFIELD;     //variable size intensity compensation field

    //range reduce simple/main profile
    mfxI32 RANGEREDFRM;      //1

//I P B picture. VopDQuant
    mfxU32  m_DQProfile;
    mfxU32  m_DQuantFRM;
    mfxU32  m_DQBILevel;
    mfxU32  m_AltPQuant;
    mfxU32  PQUANT;
    mfxU32  m_PQuant_mode;
    mfxU32  QuantizationType;      //0 - uniform, 1 - nonuniform

//only for advanced profile
    mfxU32  FCM;           // variable size frame coding mode

//I BI picture
    VC1SplBitplane    ACPRED;    // variable size  AC Prediction
    mfxU32   CONDOVER;  // variable size  conditional overlap flag
    VC1SplBitplane    OVERFLAGS; // variable size  conditional overlap macroblock pattern flags
//I BI picture. interlace frame
    VC1SplBitplane    FIELDTX;     //variable size field transform flag

// P B picture
    mfxU32   TTMBF;
    mfxU32   TTFRM;
    mfxU32    TTFRM_ORIG; //for future H/W support needs
    VC1SplBitplane    SKIPMB;
    VC1SplBitplane    MVTYPEMB;
    mfxU32   MVMODE;

// P B picture. Interlace frame
    mfxU32   DMVRANGE;              //variable size extended differential MV Range Flag
// P B picture. Interlace field
    mfxI32   REFDIST;               //variable size  P Reference Distance

//P only. Interlace field
    mfxU32 NUMREF;                //1     Number of reference picture
    mfxU32 REFFIELD;              //1     Reference field picture indicator

//B only
    VC1SplBitplane      m_DirectMB;
// B only. Interlace field
    VC1SplBitplane      FORWARDMB;         //variable size B Field forward mode
                                        //MB bit syntax element
    mfxU32 RNDCTRL;     // 1 rounding control bit

    mfxU32 TRANSDCTAB;
    mfxU32 MVMODE2;
    mfxU32 MVTAB;
    mfxU32 CBPTAB;
    mfxU32 MBMODETAB;
    mfxU32 MV2BPTAB;   //2             2 mv block pattern table
    mfxU32 MV4BPTAB;   //2             4 mv block pattern table
    mfxU32 PQUANTIZER;
    mfxU32 MVRANGE;
    mfxU32 DQSBEdge;
    // P picture
    mfxU32 LUMSCALE;
    mfxU32 LUMSHIFT;
    mfxU32 LUMSCALE1;
    mfxU32 LUMSHIFT1;

    mfxU32 POSTPROC;

    mfxU32 TRANSACFRM;
    mfxU32 TRANSACFRM2;

    mfxU32  RPTFRM;        // 2 repeat frame count
    mfxU32  UVSAMP;

    mfxU32 MV4SWITCH;
}VC1PictureLayerHeader;

#define VC1_LUT_SET(value,lut) (lut[value])

enum //profile definitions
{
    VC1_PROFILE_SIMPLE = 0, //disables X8 Intraframe, Loop filter, DQuant, and
                            //Multires while enabling the Fast Transform
    VC1_PROFILE_MAIN   = 1, //The main profile is has all the simple profile
                            //tools plus loop filter, dquant, and multires
    VC1_PROFILE_RESERVED = 2,
    VC1_PROFILE_ADVANCED= 3    //The complex profile has X8 Intraframe can use
                      //the normal IDCT transform or the VC1 Inverse Transform
};

}//namespace
#endif // _VC1_SPL_DEFS_H__
