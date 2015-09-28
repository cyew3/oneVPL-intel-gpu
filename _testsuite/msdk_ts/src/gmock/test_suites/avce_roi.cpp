#include "ts_encoder.h"

namespace 
{

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
    UnAlign = 0x20
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
    mfxU32 k = (p2 == 0) ? 16 : p2;
    for(mfxU32 i = 0; i < p0; ++i)
    {
        roi.ROI[i].Left       = 0+i*k;
        roi.ROI[i].Top        = 0+i*k;
        roi.ROI[i].Right      = k+i*k;
        roi.ROI[i].Bottom     = k+i*k;
        roi.ROI[i].Priority   = p1;
    }
}
void ROI_ctrl(tsVideoEncoder& enc, mfxVideoParam* , mfxU32 p0, mfxU32 p1, mfxU32 p2)
{
    mfxExtEncoderROI& roi = enc.m_ctrl;
    roi.NumROI            = p0;
    mfxU32 k = (p2 == 0) ? 16 : p2;
    for(mfxU32 i = 0; i < p0; ++i)
    {
        roi.ROI[i].Left       = 0+i*k;
        roi.ROI[i].Top        = 0+i*k;
        roi.ROI[i].Right      = k+i*k;
        roi.ROI[i].Bottom     = k+i*k;
        roi.ROI[i].Priority   = p1;
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
    mfxU32      p4;
    mfxU32      p5;
} tc_struct;


#define FRAME_INFO_OFFSET(field) (offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, FrameInfo) + offsetof(mfxFrameInfo, field))
#define MFX_OFFSET(field)        (offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, field))
#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

#if defined(_WIN32) || defined(_WIN64)
tc_struct test_case[] = 
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
    {/*38*/ Init,          MFX_ERR_INVALID_VIDEO_PARAM,  CQP, 24,   24,   24,  ROI_1, 2, -25, 15 },
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
tc_struct test_case[] = 
{
    //Query function
    {/*00*/ Query|Inplace, MFX_ERR_UNSUPPORTED,  CQP, 24,   24,   24,  ROI_1, 2, -25, 0 },
    {/*01*/ Query|InOut,   MFX_ERR_UNSUPPORTED,  CQP, 24,   24,   24,  ROI_1, 2, -25, 0 },
    {/*02*/ Query|DifIO,   MFX_ERR_UNSUPPORTED,  CQP, 24,   24,   24,  ROI_1, 2, -25, 0 },
    {/*03*/ Query|NullIn,  MFX_ERR_NONE,         CQP, 24,   24,   24,  ROI_1, 2, -25, 0 },
    {/*04*/ Query|NullIn,  MFX_ERR_NONE,         CQP, 24,   24,   24,  ROI_1, 2, -25, 15 },
    //{/*05*/ Query|Max,     MFX_ERR_UNSUPPORTED,  CQP, 24,   24,   24,  ROI_1, 0, -51, 0 },
    //QueryIOSurf - Just check that won't fail
    {/*05*/ QueryIOSurf,   MFX_ERR_NONE,  CQP, 24,   24,   24,  ROI_1, 1, -22, 0 },
    //Init function
    {/*06*/ Init,          MFX_ERR_INVALID_VIDEO_PARAM,  CQP, 24,   24,   24,  ROI_1, 2, -22, 0 },
    {/*07*/ Init,          MFX_ERR_INVALID_VIDEO_PARAM,  CQP, 24,   24,   24,  ROI_1, 4, -25, 0 },
    {/*08*/ Init,          MFX_ERR_INVALID_VIDEO_PARAM,  CQP, 24,   24,   24,  ROI_1, 2, -25, 15 },
    //Reset function
    //{/*09*/ Reset|WithROIInit, MFX_ERR_NONE,  CQP, 24,   24,   24,  ROI_1, 3, -22, 0 },
    {/*09*/ Reset|WoROIInit,   MFX_ERR_INVALID_VIDEO_PARAM,  CQP, 24,   24,   24,  ROI_1, 3, -25, 0 },
    //EncodeFrameAsync function
    //{/*10*/ EncodeFrameAsync|WithROIInit, MFX_ERR_NONE,  CQP, 24,   24,   24,  ROI_1, 3, -22, 0 },
    {/*10*/ EncodeFrameAsync|WoROIInit,   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  CQP, 24,   24,   24,  ROI_1, 3, -25, 0 },
    {/*11*/ EncodeFrameAsync|WoROIInit,   MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,  CQP, 24,   24,   24,  ROI_1, 3, -25, 15 },
    //{/*12*/ EncodeFrameAsync|WithROIInit|WOBuf,   MFX_ERR_NONE,  CQP, 24,   24,   24,  ROI_1, 3, -25, 0 },
};

#endif //#if defined(_WIN32) || defined(_WIN64)

int test(unsigned int id)
{
    TS_START;
    tsVideoEncoder enc(MFX_CODEC_AVC);
    tc_struct& tc =  test_case[id];

    enc.MFXInit();
    //enc.m_par.AddExtBuffer(EXT_BUF_PAR(mfxExtEncoderROI));

    InitPar par = {enc.m_session, &enc.m_par};
    
    g_tsStatus.expect(tc.sts);
    if(Query & tc.func)
    {
        if(tc.set_rc)
            (*tc.set_rc)(enc, &enc.m_par, tc.p0, tc.p1, tc.p2);
        if(tc.set_par)
            (*tc.set_par)(enc, &enc.m_par, tc.p3, tc.p4, tc.p5);
        if(Inplace & tc.func)
            enc.Query(enc.m_session, &enc.m_par, &enc.m_par);
        if(InOut & tc.func)
        {
            tsExtBufType<mfxVideoParam> pout;
            pout.mfx.CodecId = enc.m_par.mfx.CodecId;
            mfxExtEncoderROI& roi = pout; 
            enc.Query(enc.m_session, &enc.m_par, &pout);
            if(MFX_ERR_NONE == tc.sts) //Check that buffer was copied
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
            mfxExtEncoderROI& roi = enc.m_par;
            roi.NumROI = 256;
            mfxU32 temp_trace = g_tsTrace;
            g_tsStatus.expect(MFX_ERR_UNSUPPORTED);

            g_tsTrace = 0;
            enc.Query(enc.m_session, &enc.m_par, &enc.m_par);
            std::cout << "Max supported NumROI for this HW: " << roi.NumROI << ".\n";
            mfxU32 maxSupportedNumROI = roi.NumROI;
            g_tsTrace = temp_trace;

            ROI_1(enc, &enc.m_par, roi.NumROI, tc.p4, tc.p5);
            g_tsStatus.expect(MFX_ERR_NONE);
            enc.Query(enc.m_session, &enc.m_par, &enc.m_par);

            ROI_1(enc, &enc.m_par, roi.NumROI+1, tc.p4, tc.p5);
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            enc.Query(enc.m_session, &enc.m_par, &enc.m_par);
            EXPECT_EQ(maxSupportedNumROI,roi.NumROI);
        }
    }
    else if(Init & tc.func)
    {
        if(tc.set_rc)
            (*tc.set_rc)(enc, &enc.m_par, tc.p0, tc.p1, tc.p2);
        if(tc.set_par)
            (*tc.set_par)(enc, &enc.m_par, tc.p3, tc.p4, tc.p5);
        enc.Init(enc.m_session, &enc.m_par);
        if(MFX_ERR_NONE == tc.sts) //Check that buffer was copied
        {
            tsExtBufType<mfxVideoParam> pout;
            mfxExtEncoderROI& roi = pout; 
            enc.GetVideoParam(enc.m_session, &pout);
            EXPECT_EQ(0,(memcmp(*enc.m_pPar->ExtParam, &roi, sizeof(mfxExtEncoderROI))) );
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
        enc.Init(enc.m_session, &enc.m_par);

        if(WoROIInit & tc.func)
        {
            if(tc.set_par)
                (*tc.set_par)(enc, &enc.m_par, tc.p3, tc.p4, tc.p5);
        }
        g_tsStatus.expect(tc.sts);
        enc.Reset(enc.m_session, &enc.m_par);
        if(MFX_ERR_NONE == tc.sts) //Check that buffer was copied
        {
            tsExtBufType<mfxVideoParam> pout;
            mfxExtEncoderROI& roi = pout; 
            enc.GetVideoParam(enc.m_session, &pout);
            EXPECT_EQ(0,(memcmp(*enc.m_pPar->ExtParam, &roi, sizeof(mfxExtEncoderROI))) );
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
        enc.Init(enc.m_session, &enc.m_par);
        enc.AllocSurfaces();
        enc.AllocBitstream();
        EFApar par = {enc.m_session, enc.m_pCtrl, enc.GetSurface(), enc.m_pBitstream, enc.m_pSyncPoint};

        g_tsStatus.expect(tc.sts);
        for(;;)
        {
            if(!(WOBuf & tc.func))
                ROI_ctrl(enc,0, tc.p3, tc.p4, tc.p5);
            enc.EncodeFrameAsync(enc.m_session, par.pCtrl, par.pSurf, enc.m_pBitstream, enc.m_pSyncPoint);

            if(g_tsStatus.get() == MFX_ERR_MORE_DATA && par.pSurf && g_tsStatus.m_expected >= 0)
            {
                par.pSurf = enc.GetSurface();
                continue;
            }
            break;
        }
        g_tsStatus.check();
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
