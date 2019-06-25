/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"

namespace
{

#define WIDTH 720;
#define HEIGHT 480;
#define EXTREMELY_NUM_ROI 256

typedef struct
{
    mfxSession      session;
    mfxVideoParam*  pPar;
}InitPar;

typedef struct
{
    mfxSession          session;
    mfxEncodeCtrl*      pCtrl;
    mfxFrameSurface1*   pSurf;
    mfxBitstream*       pBs;
    mfxSyncPoint*       pSP;
} EFApar;

enum TestedFunc
{
    Init             = 0x100,
    Query            = 0x200,
    Reset            = 0x400,
    QueryIOSurf      = 0x800,
    EncodeFrameAsync = 0x1000
};
enum QueryMode
{
    Inplace = 0x1,
    InOut   = 0x2,
    DifIO   = 0x4,
    NullIn  = 0x8,
    Max     = 0x10,
    UnAlign = 0x20,
    OutOfFrame = 0x40
};
enum ResetMode
{
    WithROIInit = 0x1,
    WoROIInit   = 0x2,
    WOBuf       = 0x4
};

typedef void (*callback)(tsVideoEncoder&, mfxVideoParam*, mfxU32, mfxU32, mfxU32);

void set_par(tsVideoEncoder&, mfxVideoParam* p, mfxU32 offset, mfxU32 size, mfxU32 value)
{
    mfxU8* p8 = (mfxU8*)p + offset;
    memcpy(p8, &value, TS_MIN(4, size));
}
void set_roi(tsVideoEncoder&, mfxVideoParam* p, mfxU32 offset, mfxU32 size, mfxU32 value)
{
    mfxU8* p8 = (mfxU8*)p + offset;
    memcpy(p8, &value, TS_MIN(4, size));
}
void CBR(tsVideoEncoder& enc, mfxVideoParam*, mfxU32 p0, mfxU32 p1, mfxU32 p2)
{
    enc.m_par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
    enc.m_par.mfx.InitialDelayInKB  = p0;
    enc.m_par.mfx.TargetKbps        = p1;
    enc.m_par.mfx.MaxKbps           = p2;
}
void VBR(tsVideoEncoder& enc, mfxVideoParam*, mfxU32 p0, mfxU32 p1, mfxU32 p2)
{
    enc.m_par.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
    enc.m_par.mfx.InitialDelayInKB  = p0;
    enc.m_par.mfx.TargetKbps        = p1;
    enc.m_par.mfx.MaxKbps           = p2;
}
void CQP(tsVideoEncoder& enc, mfxVideoParam*, mfxU32 p0, mfxU32 p1, mfxU32 p2)
{
    enc.m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    enc.m_par.mfx.QPI = p0;
    enc.m_par.mfx.QPP = p1;
    enc.m_par.mfx.QPB = p2;
}
void AVBR(tsVideoEncoder& enc, mfxVideoParam*, mfxU32 p0, mfxU32 p1, mfxU32 p2)
{
    enc.m_par.mfx.RateControlMethod = MFX_RATECONTROL_AVBR;
    enc.m_par.mfx.Accuracy      = p0;
    enc.m_par.mfx.TargetKbps    = p1;
    enc.m_par.mfx.Convergence   = p2;
}
void ROI_1(tsVideoEncoder& enc, mfxVideoParam* , mfxU32 p0, mfxU32 p1, mfxU32 p2)
{
    mfxExtEncoderROI& roi = enc.m_par;
    roi.NumROI            = p0;
#if MFX_VERSION > 1022
    roi.ROIMode           = MFX_ROI_MODE_QP_DELTA;
#endif // #if  MFX_VERSION > 1022
    mfxU32 k = (p2 == 0) ? 16 : p2;
    for(mfxU32 i = 0; i < p0; ++i)
    {
        roi.ROI[i].Left       = 0+i*k;
        roi.ROI[i].Top        = 0+i*k;
        roi.ROI[i].Right      = k+i*k;
        roi.ROI[i].Bottom     = k+i*k;
#if MFX_VERSION > 1022
        roi.ROI[i].DeltaQP    = p1;
#else
        roi.ROI[i].Priority   = p1;
#endif // MFX_VERSION > 1022
    }
}
void ROI_ctrl(tsVideoEncoder& enc, mfxVideoParam* , mfxU32 p0, mfxU32 p1, mfxU32 p2)
{
    mfxExtEncoderROI& roi = enc.m_ctrl;
    roi.NumROI            = p0;
#if MFX_VERSION > 1022
    roi.ROIMode           = MFX_ROI_MODE_QP_DELTA;
#endif // MFX_VERSION > 1022
    mfxU32 k = (p2 == 0) ? 16 : p2;
    for(mfxU32 i = 0; i < p0; ++i)
    {
        roi.ROI[i].Left       = 0+i*k;
        roi.ROI[i].Top        = 0+i*k;
        roi.ROI[i].Right      = k+i*k;
        roi.ROI[i].Bottom     = k+i*k;
#if MFX_VERSION > 1022
        roi.ROI[i].DeltaQP    = p1;
#else
        roi.ROI[i].Priority   = p1;
#endif // MFX_VERSION > 1022
    }
}

typedef struct
{
    mfxU32      func;
    mfxStatus   sts;
    callback    set_rc;
    mfxU32      p0;
    mfxU32      p1;
    mfxU32      p2;
    callback    set_par;
    mfxU32      p3;
    mfxI32      p4;
    mfxU32      p5;
} tc_struct;


#define FRAME_INFO_OFFSET(field) (offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, FrameInfo) + offsetof(mfxFrameInfo, field))
#define MFX_OFFSET(field)        (offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, field))
#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

// When LowPower is ON, hwCaps.ROIBRCDeltaQPLevelSupport and hwCaps.ROIBRCPriorityLevelSupport don't
// supported in driver for all RateMethodControl except CQP. So need to correct expected status(corrected in run time)

tc_struct test_case[] =
#if (defined(_WIN32) || defined(_WIN64))
{
    //Query function
//    {/*00*/ Query|Inplace, MFX_ERR_NONE,  CBR,  0, 2000,    0,  ROI_1, 2,   2, 0 },
//    {/*01*/ Query|Inplace, MFX_ERR_NONE,  CBR,  0, 5000,    0,  ROI_1, 2,   2, 0 },
//    {/*02*/ Query|Inplace, MFX_ERR_NONE,  VBR,  0, 5000, 6000,  ROI_1, 2,   2, 0 },
//    {/*03*/ Query|Inplace, MFX_ERR_NONE, AVBR,  4, 5000,    5,  ROI_1, 2,   2, 0 },
    {/*04*/ Query|Inplace, MFX_ERR_NONE,  CQP, 24,   24,   24,  ROI_1, 2, -25, 0 },
//    {/*05*/ Query|InOut,   MFX_ERR_NONE,  CBR,  0, 5000,    0,  ROI_1, 2,   2, 0 },
//    {/*06*/ Query|InOut,   MFX_ERR_NONE,  VBR,  0, 5000, 6000,  ROI_1, 2,   2, 0 },
//    {/*07*/ Query|InOut,   MFX_ERR_NONE, AVBR,  4, 5000,    5,  ROI_1, 2,   2, 0 },
    {/*08*/ Query|InOut,   MFX_ERR_NONE,  CQP, 24,   24,   24,  ROI_1, 2, -25, 0 },
//    {/*09*/ Query|DifIO,   MFX_ERR_UNSUPPORTED,  CBR,  0, 5000,    0,  ROI_1, 2,   2, 0 },
//    {/*10*/ Query|DifIO,   MFX_ERR_UNSUPPORTED,  VBR,  0, 5000, 6000,  ROI_1, 2,   2, 0 },
//    {/*11*/ Query|DifIO,   MFX_ERR_UNSUPPORTED, AVBR,  4, 5000,    5,  ROI_1, 2,   2, 0 },
    {/*12*/ Query|DifIO,   MFX_ERR_UNSUPPORTED,  CQP, 24,   24,   24,  ROI_1, 2, -25, 0 },
//    {/*13*/ Query|NullIn,  MFX_ERR_NONE,  CBR,  0, 5000,    0,  ROI_1, 2,   2, 0 },
//    {/*14*/ Query|NullIn,  MFX_ERR_NONE,  VBR,  0, 5000, 6000,  ROI_1, 2,   2, 0 },
//    {/*15*/ Query|NullIn,  MFX_ERR_NONE, AVBR,  4, 5000,    5,  ROI_1, 2,   2, 0 },
    {/*16*/ Query|NullIn,  MFX_ERR_NONE,  CQP, 24,   24,   24,  ROI_1, 2, -25, 0 },

//    {/*17*/ Query|Inplace, MFX_ERR_UNSUPPORTED,  CBR,  0, 5000,    0,  ROI_1, 2,   2, 15 },
//    {/*18*/ Query|InOut,   MFX_ERR_UNSUPPORTED, AVBR,  4, 5000,    5,  ROI_1, 3,   2, 15 },
    {/*19*/ Query|NullIn,  MFX_ERR_NONE,         CQP, 24,   24,   24,  ROI_1, 2, -25, 15 },

//    {/*20*/ Query|Max,  MFX_ERR_NONE,  CBR,  0, 5000,    0,  ROI_1, 0, 2, 0 },
//    {/*21*/ Query|Max,  MFX_ERR_NONE,  VBR,  0, 5000, 6000,  ROI_1, 0, 3, 0 },
//    {/*22*/ Query|Max,  MFX_ERR_NONE, AVBR,  4, 5000,    5,  ROI_1, 0, -3, 0 },
    {/*23*/ Query|Max,  MFX_ERR_NONE,  CQP, 24,   24,   24,  ROI_1, 0, -51, 0 },
    //QueryIOSurf - Just check that won't fail
//    {/*24*/ QueryIOSurf,   MFX_ERR_NONE,  CBR,  0, 5000,    0,  ROI_1, 1,   3, 0 },
//    {/*25*/ QueryIOSurf,   MFX_ERR_NONE,  VBR,  0, 5000, 6000,  ROI_1, 1,   3, 0 },
//    {/*26*/ QueryIOSurf,   MFX_ERR_NONE, AVBR,  4, 5000,    5,  ROI_1, 1,   3, 0 },
    {/*27*/ QueryIOSurf,   MFX_ERR_NONE,  CQP, 24,   24,   24,  ROI_1, 1, -22, 0 },
    //Init function
//    {/*28*/ Init,          MFX_ERR_NONE,  CBR,  0, 5000,    0,  ROI_1, 1,   3, 0 },
//    {/*29*/ Init,          MFX_ERR_NONE,  VBR,  0, 5000, 6000,  ROI_1, 1,   3, 0 },
//    {/*30*/ Init,          MFX_ERR_NONE, AVBR,  4, 5000,    5,  ROI_1, 2,   3, 0 },
    {/*31*/ Init,          MFX_ERR_NONE,  CQP, 24,   24,   24,  ROI_1, 2, -22, 0 },
//    {/*32*/ Init,          MFX_ERR_NONE,  CBR,  0, 5000,    0,  ROI_1, 3,   2, 0 },
//    {/*33*/ Init,          MFX_ERR_NONE,  VBR,  0, 5000, 6000,  ROI_1, 3,   2, 0 },
//    {/*34*/ Init,          MFX_ERR_NONE, AVBR,  4, 5000,    5,  ROI_1, 4,   2, 0 },
    {/*35*/ Init,          MFX_ERR_NONE,  CQP, 24,   24,   24,  ROI_1, 4, -25, 0 },
//    {/*36*/ Init,          MFX_ERR_INVALID_VIDEO_PARAM,  CBR,  0, 5000,    0,  ROI_1, 2,   2, 15 },
//    {/*37*/ Init,          MFX_ERR_INVALID_VIDEO_PARAM, AVBR,  4, 5000,    5,  ROI_1, 3,   2, 15 },
    {/*38*/ Init,          MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  CQP, 24,   24,   24,  ROI_1, 2, -25, 15 },
    //Reset function
//    {/*39*/ Reset|WithROIInit, MFX_ERR_NONE,  CBR,  0, 5000,    0,  ROI_1, 1,   3, 0 },
//    {/*40*/ Reset|WithROIInit, MFX_ERR_NONE,  VBR,  0, 5000, 6000,  ROI_1, 1,   3, 0 },
//    {/*41*/ Reset|WithROIInit, MFX_ERR_NONE, AVBR,  4, 5000,    5,  ROI_1, 3,   3, 0 },
    {/*42*/ Reset|WithROIInit, MFX_ERR_NONE,  CQP, 24,   24,   24,  ROI_1, 3, -22, 0 },
//    {/*43*/ Reset|WoROIInit,   MFX_ERR_NONE,  CBR,  0, 5000,    0,  ROI_1, 2,   2, 0 },
//    {/*44*/ Reset|WoROIInit,   MFX_ERR_NONE,  VBR,  0, 5000, 6000,  ROI_1, 2,   2, 0 },
//    {/*45*/ Reset|WoROIInit,   MFX_ERR_NONE, AVBR,  4, 5000,    5,  ROI_1, 3,   2, 0 },
    {/*46*/ Reset|WoROIInit,   MFX_ERR_NONE,  CQP, 24,   24,   24,  ROI_1, 3, -25, 0 },
    //EncodeFrameAsync function
//    {/*47*/ EncodeFrameAsync|WithROIInit, MFX_ERR_NONE,  CBR,  0, 5000,    0,  ROI_1, 1,   3, 0 },
//    {/*48*/ EncodeFrameAsync|WithROIInit, MFX_ERR_NONE,  VBR,  0, 5000, 6000,  ROI_1, 1,   3, 0 },
//    {/*49*/ EncodeFrameAsync|WithROIInit, MFX_ERR_NONE, AVBR,  4, 5000,    5,  ROI_1, 3,   3, 0 },
    {/*50*/ EncodeFrameAsync|WithROIInit, MFX_ERR_NONE,  CQP, 24,   24,   24,  ROI_1, 3, -22, 0 },
//    {/*51*/ EncodeFrameAsync|WoROIInit,   MFX_ERR_NONE,  CBR,  0, 5000,    0,  ROI_1, 2,   2, 0 },
//    {/*52*/ EncodeFrameAsync|WoROIInit,   MFX_ERR_NONE,  VBR,  0, 5000, 6000,  ROI_1, 2,   2, 0 },
//    {/*53*/ EncodeFrameAsync|WoROIInit,   MFX_ERR_NONE, AVBR,  4, 5000,    5,  ROI_1, 3,   2, 0 },
    {/*54*/ EncodeFrameAsync|WoROIInit,   MFX_ERR_NONE,  CQP, 24,   24,   24,  ROI_1, 3, -25, 0 },
//    {/*55*/ EncodeFrameAsync|WoROIInit,   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  CBR,  0, 5000,    0,  ROI_1, 2,   2, 15 },
//    {/*56*/ EncodeFrameAsync|WoROIInit,   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  VBR,  0, 5000, 6000,  ROI_1, 2,   2, 15 },
//    {/*57*/ EncodeFrameAsync|WoROIInit,   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, AVBR,  4, 5000,    5,  ROI_1, 3,   2, 15 },
    {/*58*/ EncodeFrameAsync|WoROIInit,   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  CQP, 24,   24,   24,  ROI_1, 3, -25, 15 },
//    {/*59*/ EncodeFrameAsync|WithROIInit|WOBuf,   MFX_ERR_NONE,  CBR,  0, 5000,    0,  ROI_1, 2,   2, 0 },
//    {/*60*/ EncodeFrameAsync|WithROIInit|WOBuf,   MFX_ERR_NONE,  VBR,  0, 5000, 6000,  ROI_1, 2,   2, 0 },
//    {/*61*/ EncodeFrameAsync|WithROIInit|WOBuf,   MFX_ERR_NONE, AVBR,  4, 5000,    5,  ROI_1, 3,   2, 0 },
    {/*62*/ EncodeFrameAsync|WithROIInit|WOBuf,   MFX_ERR_NONE,  CQP, 24,   24,   24,  ROI_1, 3, -25, 0 },
};
#else
{
    //Query function
    {/*00*/ Query|Inplace, MFX_ERR_UNSUPPORTED,               CBR,  0, 5000,    0,  ROI_1, 2,   -52, 0 }, // invalid DeltaQP
    {/*01*/ Query|Inplace, MFX_ERR_UNSUPPORTED,               VBR,  0, 5000, 6000,  ROI_1, 2,   -52, 0 },
    {/*02*/ Query|Inplace, MFX_ERR_UNSUPPORTED,               CQP, 24,   24,   24,  ROI_1, 2,   -52, 0 },
    {/*03*/ Query|InOut,   MFX_ERR_UNSUPPORTED,               CBR,  0, 5000,    0,  ROI_1, 2,   -52, 0 },
    {/*04*/ Query|InOut,   MFX_ERR_UNSUPPORTED,               VBR,  0, 5000, 6000,  ROI_1, 2,   -52, 0 },
    {/*05*/ Query|InOut,   MFX_ERR_UNSUPPORTED,               CQP, 24,   24,   24,  ROI_1, 2,   -52, 0 },
    {/*06*/ Query|DifIO,   MFX_ERR_UNSUPPORTED,               CBR,  0, 5000,    0,  ROI_1, 2,     2, 0 },
    {/*07*/ Query|DifIO,   MFX_ERR_UNSUPPORTED,               VBR,  0, 5000, 6000,  ROI_1, 2,     2, 0 },
    {/*08*/ Query|DifIO,   MFX_ERR_UNSUPPORTED,               CQP, 24,   24,   24,  ROI_1, 2,   -25, 0 },
    {/*09*/ Query|NullIn,  MFX_ERR_NONE,                      CBR,  0, 5000,    0,  ROI_1, 2,     2, 0 },
    {/*10*/ Query|NullIn,  MFX_ERR_NONE,                      VBR,  0, 5000, 6000,  ROI_1, 2,     2, 0 },
    {/*11*/ Query|NullIn,  MFX_ERR_NONE,                      CQP, 24,   24,   24,  ROI_1, 2,   -25, 0 },
    {/*12*/ Query|NullIn,  MFX_ERR_NONE,                      CBR,  0, 5000,    0,  ROI_1, 2,     2, 15 },
    {/*13*/ Query|NullIn,  MFX_ERR_NONE,                      VBR,  0, 5000, 6000,  ROI_1, 2,     2, 15 },
    {/*14*/ Query|NullIn,  MFX_ERR_NONE,                      CQP, 24,   24,   24,  ROI_1, 2,   -25, 15 },
    {/*15*/ Query|Max,     MFX_ERR_UNSUPPORTED,               CBR,  0, 5000,    0,  ROI_1, 2,     2, 0 }, // invalid number of ROI
    {/*16*/ Query|Max,     MFX_ERR_UNSUPPORTED,               VBR,  0, 5000, 6000,  ROI_1, 2,     2, 0 },
    {/*17*/ Query|Max,     MFX_ERR_UNSUPPORTED,               CQP, 24,   24,   24,  ROI_1, 0,   -22, 0 },
    {/*18*/ Query|Inplace, MFX_ERR_NONE,                      CBR,  0, 5000,    0,  ROI_1, 2,     2, 0 },
    {/*19*/ Query|Inplace, MFX_ERR_NONE,                      VBR,  0, 5000, 6000,  ROI_1, 2,     2, 0 },
    {/*20*/ Query|Inplace, MFX_ERR_NONE,                      CQP, 24,   24,   24,  ROI_1, 2,   -22, 0 },
    {/*21*/ Query|InOut,   MFX_ERR_NONE,                      CBR,  0, 5000,    0,  ROI_1, 2,     2, 0 },
    {/*22*/ Query|InOut,   MFX_ERR_NONE,                      VBR,  0, 5000, 6000,  ROI_1, 2,     2, 0 },
    {/*23*/ Query|InOut,   MFX_ERR_NONE,                      CQP, 24,   24,   24,  ROI_1, 2,   -22, 0 },
    {/*24*/ Query|Inplace, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  CBR,  0, 5000,    0,  ROI_1, 2,     2, 15 }, // ROI blocks size must be multiples of 16
    {/*25*/ Query|Inplace, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  VBR,  0, 5000, 6000,  ROI_1, 2,     2, 15 },
    {/*26*/ Query|Inplace, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  CQP,  24,  24,   24,  ROI_1, 2,   -22, 15 },
    {/*27*/ Query|OutOfFrame, MFX_ERR_UNSUPPORTED,            CBR,  0, 5000,    0,  ROI_1, 2,     2, 241 }, // the last ROI block out of frame ((HEIGHT / NumROI + 1))
    {/*28*/ Query|OutOfFrame, MFX_ERR_UNSUPPORTED,            VBR,  0, 5000, 6000,  ROI_1, 2,     2, 241 },
    {/*29*/ Query|OutOfFrame, MFX_ERR_UNSUPPORTED,            CQP, 24,   24,   24,  ROI_1, 2,   -10, 241 },

    //QueryIOSurf - Just check that won't fail
    {/*30*/ QueryIOSurf,   MFX_ERR_NONE,                      CBR,  0, 5000,    0,  ROI_1, 1,     3, 0 },
    {/*31*/ QueryIOSurf,   MFX_ERR_NONE,                      VBR,  0, 5000, 6000,  ROI_1, 1,     3, 0 },
    {/*32*/ QueryIOSurf,   MFX_ERR_NONE,                      CQP, 24,   24,   24,  ROI_1, 1,   -22, 0 },

    //Init function
    {/*33*/ Init, MFX_ERR_NONE,                      CBR,  0, 5000,    0,  ROI_1, 2,     2, 0 },
    {/*34*/ Init, MFX_ERR_NONE,                      VBR,  0, 5000, 6000,  ROI_1, 2,     2, 0 },
    {/*35*/ Init, MFX_ERR_NONE,                      CQP, 24,   24,   24,  ROI_1, 2,   -22, 0 },
    {/*36*/ Init, MFX_ERR_INVALID_VIDEO_PARAM,       CBR,  0, 5000,    0,  ROI_1, 2,   -52, 0 }, // invalid DeltaQP
    {/*37*/ Init, MFX_ERR_INVALID_VIDEO_PARAM,       VBR,  0, 5000, 6000,  ROI_1, 2,   -52, 0 },
    {/*38*/ Init, MFX_ERR_INVALID_VIDEO_PARAM,       CQP, 24,   24,   24,  ROI_1, 4,   -52, 0 },
    {/*39*/ Init, MFX_ERR_INVALID_VIDEO_PARAM,       CBR,  0, 5000,    0,  ROI_1, 2,     2, 241 }, // the last ROI block out of frame ((HEIGHT / NumROI + 1))
    {/*40*/ Init, MFX_ERR_INVALID_VIDEO_PARAM,       VBR,  0, 5000, 6000,  ROI_1, 2,     2, 241 },
    {/*41*/ Init, MFX_ERR_INVALID_VIDEO_PARAM,       CQP, 24,   24,   24,  ROI_1, 2,   -10, 241 },
    {/*42*/ Init, MFX_ERR_NONE,                      CBR,  0, 5000,    0,  ROI_1, 1,     3, 0 },
    {/*43*/ Init, MFX_ERR_NONE,                      VBR,  0, 5000, 6000,  ROI_1, 1,     3, 0 },
    {/*44*/ Init, MFX_ERR_NONE,                      CQP, 24,   24,   24,  ROI_1, 1,   -22, 0 },
    {/*45*/ Init, MFX_ERR_NONE,                      CBR,  0, 5000,    0,  ROI_1, 3,     2, 0 },
    {/*46*/ Init, MFX_ERR_NONE,                      VBR,  0, 5000, 6000,  ROI_1, 3,     2, 0 },
    {/*47*/ Init, MFX_ERR_NONE,                      CQP, 24,   24,   24,  ROI_1, 4,   -22, 0 },
    {/*48*/ Init, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  CBR,  0, 5000,    0,  ROI_1, 2,     2, 15 }, // ROI blocks size must be multiples of 16
    {/*59*/ Init, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  VBR,  0, 5000,    0,  ROI_1, 2,     2, 15 },
    {/*50*/ Init, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  CQP, 24,   24,   24,  ROI_1, 2,   -22, 15 },

    //Reset function
    {/*51*/ Reset|WithROIInit, MFX_ERR_NONE,                 CBR,  0, 5000,    0,  ROI_1, 1,     3, 0 },
    {/*52*/ Reset|WithROIInit, MFX_ERR_NONE,                 VBR,  0, 5000, 6000,  ROI_1, 1,     3, 0 },
    {/*53*/ Reset|WithROIInit, MFX_ERR_NONE,                 CQP, 24,   24,   24,  ROI_1, 3,   -22, 0 },
    {/*54*/ Reset|WoROIInit,   MFX_ERR_INVALID_VIDEO_PARAM,  CBR,  0, 5000,    0,  ROI_1, 1,   -52, 0 }, // invalid DeltaQP
    {/*55*/ Reset|WoROIInit,   MFX_ERR_INVALID_VIDEO_PARAM,  VBR,  0, 5000, 6000,  ROI_1, 1,   -52, 0 },
    {/*56*/ Reset|WoROIInit,   MFX_ERR_INVALID_VIDEO_PARAM,  CQP, 24,   24,   24,  ROI_1, 3,   -52, 0 },
    {/*57*/ Reset|WithROIInit, MFX_ERR_NONE,                 CBR,  0, 5000,    0,  ROI_1, 1,     3, 0 },
    {/*58*/ Reset|WithROIInit, MFX_ERR_NONE,                 VBR,  0, 5000, 6000,  ROI_1, 1,     3, 0 },
    {/*59*/ Reset|WithROIInit, MFX_ERR_NONE,                 CQP, 24,   24,   24,  ROI_1, 1,   -22, 0 },
    {/*60*/ Reset|WoROIInit,   MFX_ERR_NONE,                 CBR,  0, 5000,    0,  ROI_1, 2,     2, 0 },
    {/*61*/ Reset|WoROIInit,   MFX_ERR_NONE,                 VBR,  0, 5000, 6000,  ROI_1, 2,     2, 0 },
    {/*62*/ Reset|WoROIInit,   MFX_ERR_NONE,                 CQP, 24,   24,   24,  ROI_1, 3,   -22, 0 },

    //EncodeFrameAsync function
    {/*63*/ EncodeFrameAsync|WithROIInit,         MFX_ERR_NONE,                      CBR,  0, 5000,    0,  ROI_1, 2,     2, 0 },
    {/*64*/ EncodeFrameAsync|WithROIInit,         MFX_ERR_NONE,                      VBR,  0, 5000, 6000,  ROI_1, 2,     2, 0 },
    {/*65*/ EncodeFrameAsync|WithROIInit,         MFX_ERR_NONE,                      CQP, 24,   24,   24,  ROI_1, 3,   -22, 0 },
    {/*66*/ EncodeFrameAsync|WithROIInit|WOBuf,   MFX_ERR_NONE,                      CBR,  0, 5000,    0,  ROI_1, 2,     2, 0 },
    {/*67*/ EncodeFrameAsync|WithROIInit|WOBuf,   MFX_ERR_NONE,                      VBR,  0, 5000, 6000,  ROI_1, 2,     2, 0 },
    {/*68*/ EncodeFrameAsync|WithROIInit|WOBuf,   MFX_ERR_NONE,                      CQP, 24,   24,   24,  ROI_1, 3,   -22, 0 },
    {/*69*/ EncodeFrameAsync|WoROIInit,           MFX_ERR_NONE,                      CBR,  0, 5000,    0,  ROI_1, 2,     2, 0 },
    {/*70*/ EncodeFrameAsync|WoROIInit,           MFX_ERR_NONE,                      VBR,  0, 5000, 6000,  ROI_1, 2,     2, 0 },
    {/*71*/ EncodeFrameAsync|WoROIInit,           MFX_ERR_NONE,                      CQP, 24,   24,   24,  ROI_1, 3,   -25, 0 },
    {/*72*/ EncodeFrameAsync|WoROIInit,           MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  CBR,  0, 5000,    0,  ROI_1, 2,     2, 15 }, // ROI blocks size must be multiples of 16
    {/*73*/ EncodeFrameAsync|WoROIInit,           MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  VBR,  0, 5000, 6000,  ROI_1, 2,     2, 15 },
    {/*74*/ EncodeFrameAsync|WoROIInit,           MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  CQP, 24,   24,   24,  ROI_1, 3,   -25, 15 },

};
#endif

bool isRateControlMethodSupported(tsVideoEncoder& enc)
{
    // CBR and VBR aren't supported with LowPower
    if (enc.m_par.mfx.LowPower == MFX_CODINGOPTION_ON
        && enc.m_par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
        return false;
    return true;
}

int test(unsigned int id)
{
    TS_START;

    if (g_tsHWtype < MFX_HW_SKL)
    {
        g_tsLog << "\n\nWARNING: SKIP test - ROI delta QP isn't supported on platforms < SKL\n\n";
        throw tsSKIP;
    }

    tsVideoEncoder enc(MFX_CODEC_AVC);
    tc_struct& tc =  test_case[id];

    enc.MFXInit();
    //enc.m_par.AddExtBuffer(EXT_BUF_PAR(mfxExtEncoderROI));

    if (enc.m_par.mfx.LowPower == MFX_CODINGOPTION_ON) // LowPower mode unsupported NumRoi > 3
        tc.p3 = TS_MIN(3, tc.p3);

    enc.m_par.mfx.FrameInfo.Width  = enc.m_par.mfx.FrameInfo.CropW = WIDTH;
    enc.m_par.mfx.FrameInfo.Height = enc.m_par.mfx.FrameInfo.CropH = HEIGHT;

    mfxExtEncoderROI& roi = enc.m_par;
    tsExtBufType<mfxVideoParam> pout;
    pout.mfx.CodecId = enc.m_par.mfx.CodecId;
    mfxExtEncoderROI& roi_out = pout;

    InitPar par = {enc.m_session, &enc.m_par};

    mfxStatus sts = MFX_ERR_NONE;
    g_tsStatus.expect(tc.sts);
    if (!isRateControlMethodSupported(enc))
    {
        tc.sts = MFX_ERR_UNSUPPORTED;
    }

    if(Query & tc.func)
    {
        if(tc.set_rc)
            (*tc.set_rc)(enc, &enc.m_par, tc.p0, tc.p1, tc.p2);
        if(tc.set_par)
            (*tc.set_par)(enc, &enc.m_par, tc.p3, tc.p4, tc.p5);
        if (Inplace & tc.func)
            enc.Query(enc.m_session, &enc.m_par, &enc.m_par);
        if(InOut & tc.func)
        {
            sts = enc.Query(enc.m_session, &enc.m_par, &pout);
            if(MFX_ERR_NONE == sts) //Check that buffer was copied
            {
                EXPECT_EQ(0,(memcmp(*enc.m_pPar->ExtParam, &roi, sizeof(mfxExtEncoderROI))) );
            }
        }
        if(DifIO & tc.func)
        {
            tsExtBufType<mfxVideoParam> pout;
            pout.mfx.CodecId = enc.m_par.mfx.CodecId;
            enc.Query(enc.m_session, &enc.m_par, &pout);
        }
        if(NullIn & tc.func)
            enc.Query(enc.m_session, 0, &enc.m_par);
        if(Max & tc.func)
        {
            roi.NumROI = EXTREMELY_NUM_ROI;
            mfxU32 temp_trace = g_tsTrace;
            g_tsStatus.expect(MFX_ERR_UNSUPPORTED);

            g_tsTrace = 0;
            enc.Query(enc.m_session, &enc.m_par, &pout);
            std::cout << "Max supported NumROI for this HW: " << roi.NumROI << ".\n";
            mfxU32 maxSupportedNumROI = roi_out.NumROI;
            g_tsTrace = temp_trace;

            ROI_1(enc, &enc.m_par, maxSupportedNumROI, tc.p4, tc.p5);
            g_tsStatus.expect(MFX_ERR_NONE);
            enc.Query(enc.m_session, &enc.m_par, &pout);

            ROI_1(enc, &enc.m_par, maxSupportedNumROI+1, tc.p4, tc.p5);
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            enc.Query(enc.m_session, &enc.m_par, &pout);
            EXPECT_EQ(maxSupportedNumROI, roi_out.NumROI);
        }
        if (OutOfFrame & tc.func)
        {
            enc.Query(enc.m_session, &enc.m_par, &pout);
        }
    }
    else if(Init & tc.func)
    {
        if(tc.set_rc)
            (*tc.set_rc)(enc, &enc.m_par, tc.p0, tc.p1, tc.p2);
        if(tc.set_par)
            (*tc.set_par)(enc, &enc.m_par, tc.p3, tc.p4, tc.p5);

        sts = enc.Init(enc.m_session, &enc.m_par);
        if(MFX_ERR_NONE == sts) //Check that buffer was copied
        {
            enc.GetVideoParam(enc.m_session, &pout);
            EXPECT_EQ(0,(memcmp(*enc.m_pPar->ExtParam, &roi_out, sizeof(mfxExtEncoderROI))) );
        }
    }
    else if(QueryIOSurf & tc.func)
    {
        if(tc.set_rc)
            (*tc.set_rc)(enc, &enc.m_par, tc.p0, tc.p1, tc.p2);
        if(tc.set_par)
            (*tc.set_par)(enc, &enc.m_par, tc.p3, tc.p4, tc.p5);
        enc.QueryIOSurf();
    }
    else if(Reset & tc.func)
    {
        if(WithROIInit & tc.func)
        {
            if(tc.set_par)
                (*tc.set_par)(enc, &enc.m_par, tc.p3, tc.p4, tc.p5);
        }
        if(tc.set_rc)
            (*tc.set_rc)(enc, &enc.m_par, tc.p0, tc.p1, tc.p2);

        g_tsStatus.expect(MFX_ERR_NONE);
        if (!isRateControlMethodSupported(enc))
        {
            g_tsStatus.expect(MFX_ERR_INVALID_VIDEO_PARAM);
        }
        sts = enc.Init(enc.m_session, &enc.m_par);

        if(WoROIInit & tc.func)
        {
            if(tc.set_par)
                (*tc.set_par)(enc, &enc.m_par, tc.p3, tc.p4, tc.p5);
        }

        g_tsStatus.expect(tc.sts);
        if (sts == MFX_ERR_INVALID_VIDEO_PARAM)
        {
            g_tsStatus.expect(MFX_ERR_NOT_INITIALIZED);
        }

        sts = enc.Reset(enc.m_session, &enc.m_par);
        if(MFX_ERR_NONE == sts) //Check that buffer was copied
        {
            enc.GetVideoParam(enc.m_session, &pout);
            EXPECT_EQ(0,(memcmp(*enc.m_pPar->ExtParam, &roi_out, sizeof(mfxExtEncoderROI))) );
        }
    }
    else if(EncodeFrameAsync & tc.func)
    {
        if(WithROIInit & tc.func)
        {
            if(tc.set_rc)
                (*tc.set_rc)(enc, &enc.m_par, tc.p0, tc.p1, tc.p2);
            if(tc.set_par)
                (*tc.set_par)(enc, &enc.m_par, tc.p3, tc.p4, tc.p5);
        }
        g_tsStatus.expect(MFX_ERR_NONE);
        if (!isRateControlMethodSupported(enc))
        {
            g_tsStatus.expect(MFX_ERR_INVALID_VIDEO_PARAM);
        }
        sts = enc.Init(enc.m_session, &enc.m_par);
        if (sts != MFX_ERR_INVALID_VIDEO_PARAM)
        {
            enc.AllocSurfaces();
            enc.AllocBitstream();
            EFApar par = { enc.m_session, enc.m_pCtrl, enc.GetSurface(), enc.m_pBitstream, enc.m_pSyncPoint };

            g_tsStatus.expect(tc.sts);
            for (;;)
            {
                if (!(WOBuf & tc.func))
                    ROI_ctrl(enc, 0, tc.p3, tc.p4, tc.p5);
                sts = enc.EncodeFrameAsync(enc.m_session, par.pCtrl, par.pSurf, enc.m_pBitstream, enc.m_pSyncPoint);

                if (sts == MFX_ERR_MORE_DATA && par.pSurf && g_tsStatus.m_expected >= 0)
                {
                    par.pSurf = enc.GetSurface();
                    continue;
                }
                break;
            }
            g_tsStatus.check();
        }
    }
    else
    {
        std::cout << "Test error\n";
        EXPECT_EQ(1,0);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(avce_roi, test, sizeof(test_case)/sizeof(tc_struct));

};
