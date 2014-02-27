#include "ts_encoder.h"

namespace
{

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_VP8) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    struct GVPpar
    {
        mfxSession      session;
        tsExtBufType<mfxVideoParam>*  pPar;
    } m_GVPpar;

    struct tc_par;
    typedef void (TestSuite::*callback)(tc_par&);


    struct tc_par
    {
        callback set_par;
        mfxU32   p0;
        mfxU32   p1;
        mfxU32   p2;
        mfxU32   p3;
    };

    struct tc_struct
    {
        mfxStatus sts;
        tc_par pre_init;
        tc_par post_init;
    };

    static const tc_struct test_case[];

    enum 
    {
        GVP,
        MFX_IN,
        MFX_GET,
    };

    mfxU8* GetP8(mfxU32 id)
    {
        switch(id)
        {
        case GVP:       return (mfxU8*)&m_GVPpar;
        case MFX_IN:    return (mfxU8*)&m_par;
        case MFX_GET:   return (mfxU8*)m_GVPpar.pPar;
        default:        return 0;
        }
    }
    
    void set_par(tc_par& arg)
    {
        memcpy(GetP8(arg.p0) + arg.p1, &arg.p3, TS_MIN(4, arg.p2)); 
    }

    void close_encoder(tc_par&)
    {
        Close();
    }
    
    void CBR(tc_par&)
    {
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
        m_par.mfx.InitialDelayInKB = 0;
        m_par.mfx.TargetKbps = 3000;
    }

    void VBR(tc_par&)
    {
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
        m_par.mfx.InitialDelayInKB = 0;
        m_par.mfx.TargetKbps = 2000;
        m_par.mfx.MaxKbps = 3000;
    }
    
    void COVP8(tc_par& arg)
    {
        tsExtBufType<mfxVideoParam>* par = ((arg.p0 == MFX_IN) ? &m_par : m_GVPpar.pPar);
        mfxExtCodingOptionVP8& co = *par;
        co.EnableAutoAltRef       = arg.p1;      
        co.TokenPartitions        = arg.p2;       
        co.EnableMultipleSegments = arg.p3;
    }
};


const TestSuite::tc_struct TestSuite::test_case[] = 
{
    {/* 0*/ MFX_ERR_INVALID_HANDLE,  {}, {&set_par, GVP, offsetof(GVPpar, session), sizeof(mfxSession), 0} },
    {/* 1*/ MFX_ERR_NOT_INITIALIZED, {}, {&close_encoder} },
    {/* 2*/ MFX_ERR_NULL_PTR,        {}, {&set_par, GVP, offsetof(GVPpar, pPar), sizeof(mfxVideoParam), 0} },
    {/* 3*/ MFX_ERR_NONE, {}, {} },
    {/* 4*/ MFX_ERR_NONE, {&set_par, MFX_IN, offsetof(mfxVideoParam, AsyncDepth), sizeof(mfxU16), 4}, {} },
    {/* 5*/ MFX_ERR_NONE, {&set_par, MFX_IN, offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_VIDEO_MEMORY }, {} },
    {/* 6*/ MFX_ERR_NONE, {&set_par, MFX_IN,   offsetof(mfxVideoParam, mfx)
                                             + offsetof(mfxInfoMFX, CodecProfile), sizeof(mfxU16), MFX_PROFILE_VP8_3 }, {} },
    {/* 7*/ MFX_ERR_NONE, {&set_par, MFX_IN,   offsetof(mfxVideoParam, mfx)
                                             + offsetof(mfxInfoMFX, TargetUsage), sizeof(mfxU16), 0 }, {} },
    {/* 8*/ MFX_ERR_NONE, {&set_par, MFX_IN,   offsetof(mfxVideoParam, mfx)
                                             + offsetof(mfxInfoMFX, GopPicSize), sizeof(mfxU16), 30}, {} },
    {/* 9*/ MFX_ERR_NONE, {&CBR}, {} },
    {/*10*/ MFX_ERR_NONE, {&VBR}, {} },
    {/*11*/ MFX_ERR_NONE, {}, {&COVP8, MFX_GET}, },
    {/*12*/ MFX_ERR_NONE, {&COVP8, MFX_IN, MFX_CODINGOPTION_ON, MFX_TOKENPART_VP8_4, MFX_CODINGOPTION_ON}, {&COVP8, MFX_GET}, },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);


#define _CHECK_SET(field, base, check) if(base.field) { EXPECT_EQ(base.field, check.field); } else { EXPECT_NE(0, check.field); }
#define CHECK_SET(field) _CHECK_SET(field, m_par, par0)
#define CHECK_EXTCO(field) _CHECK_SET(field, ((mfxExtCodingOptionVP8&)m_par), (*extco))

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    tsExtBufType<mfxVideoParam> par0;
    tc_struct tc = test_case[id];
    mfxExtCodingOptionVP8* extco = 0;
    
    if(tc.pre_init.set_par)
    {
        (this->*tc.pre_init.set_par)(tc.pre_init);
    }

    Init();

    m_GVPpar.session = m_session;
    m_GVPpar.pPar    = &par0;
    
    if(tc.post_init.set_par)
    {
        (this->*tc.post_init.set_par)(tc.post_init);
    }
    
    extco = par0;

    g_tsStatus.expect(tc.sts);
    if( 0 <= GetVideoParam(m_GVPpar.session, m_GVPpar.pPar))
    {
        CHECK_SET(AsyncDepth);
        CHECK_SET(mfx.CodecId);
        CHECK_SET(mfx.CodecProfile);
        CHECK_SET(mfx.TargetUsage);
        CHECK_SET(mfx.GopPicSize);
        CHECK_SET(mfx.BufferSizeInKB);
        CHECK_SET(mfx.RateControlMethod);
        CHECK_SET(mfx.BRCParamMultiplier);
        CHECK_SET(mfx.FrameInfo.FourCC);
        CHECK_SET(mfx.FrameInfo.Width);
        CHECK_SET(mfx.FrameInfo.Height);
        CHECK_SET(mfx.FrameInfo.FrameRateExtN);
        CHECK_SET(mfx.FrameInfo.FrameRateExtD);
        EXPECT_EQ(m_par.IOPattern, par0.IOPattern);
        EXPECT_EQ(MFX_PICSTRUCT_PROGRESSIVE, par0.mfx.FrameInfo.PicStruct);
        EXPECT_EQ(1, par0.mfx.FrameInfo.AspectRatioW);
        EXPECT_EQ(1, par0.mfx.FrameInfo.AspectRatioH);
        EXPECT_EQ(0, par0.mfx.CodecLevel);
        EXPECT_EQ(0, par0.mfx.GopRefDist);
        EXPECT_EQ(0, par0.mfx.IdrInterval);
        EXPECT_EQ(0, par0.mfx.NumRefFrame);
        EXPECT_EQ(0, par0.mfx.NumSlice);

        if(m_par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        {
            CHECK_SET(mfx.QPI);
            CHECK_SET(mfx.QPP);
            EXPECT_EQ(0, par0.mfx.QPB);
        } else {
            CHECK_SET(mfx.InitialDelayInKB);
            CHECK_SET(mfx.TargetKbps);
            CHECK_SET(mfx.MaxKbps);
        }

        if(extco)
        {
            CHECK_EXTCO(Header.BufferId);
            CHECK_EXTCO(Header.BufferSz);
            CHECK_EXTCO(EnableAutoAltRef);
            CHECK_EXTCO(TokenPartitions);
            CHECK_EXTCO(EnableMultipleSegments);
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vp8e_get_video_param);
};