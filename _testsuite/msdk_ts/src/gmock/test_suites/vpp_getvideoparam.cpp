#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

namespace vpp_getvideoparam
{

class TestSuite : tsVideoVPP
{
public:
    TestSuite()
        : tsVideoVPP(){ }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:

    mfxExtVPPDoUse* vpp_du;

    enum
    {
        MFX_PAR = 1,
        NULL_PAR,
        STREAM,
        NULL_SESSION,
        DOUSE
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
        mfxU32 alg_num;
        mfxU32 alg_list[5];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NULL_PTR, NULL_PAR, {}, 0 , {} },
    {/*01*/ MFX_ERR_NONE, 0, {MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 100}, 0 , {} },
    {/*02*/ MFX_ERR_NONE, STREAM, {}, 0 , {} },
    {/*03*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION, {}, 0 , {} },
    {/*04*/ MFX_ERR_NONE, MFX_PAR,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 50},
            {MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 50},
            {MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_PRESERVE_TIMESTAMP},
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 50.0F},
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Contrast, 5.0F},
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Hue, 100.0F},
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Saturation, 5.0F},
            {MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_BOB},
        },
        0 , {}
    }, //VCSD100021027
    {/*05*/ MFX_ERR_NONE, DOUSE, {}, 5 ,
    {MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_DETAIL, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION,
    MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DEINTERLACING} }, //VCSD100021027
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

mfxU32 GetBufSzById(mfxU32 BufId)
{
    const size_t maxBuffers = sizeof( g_StringsOfBuffers ) / sizeof( g_StringsOfBuffers[0] );

    for(size_t i(0); i < maxBuffers; ++i)
    {
        if( BufId == g_StringsOfBuffers[i].BufferId )
            return g_StringsOfBuffers[i].BufferSz;
    }
    return 0;
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    m_par.AsyncDepth = 1;

    SETPARS(&m_par, MFX_PAR);

    if (tc.mode == DOUSE)
    {
        m_par.AddExtBuffer(MFX_EXTBUFF_VPP_DOUSE, sizeof(mfxExtVPPDoUse));
        vpp_du = (mfxExtVPPDoUse*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DOUSE);
        vpp_du->NumAlg = tc.alg_num;
        vpp_du->AlgList = new mfxU32[tc.alg_num];

        memset(vpp_du->AlgList, 0, sizeof(mfxU32)*vpp_du->NumAlg);

        for (mfxU32 i = 0; i< tc.alg_num; i++)
            vpp_du->AlgList[i] = tc.alg_list[i];
    }

    CreateAllocators();
    SetFrameAllocator();
    SetHandle();
    AllocSurfaces();

    Init(m_session, m_pPar);

    tsExtBufType<mfxVideoParam> par_init;
    mfxVideoParam *out_pPar = &par_init;
    mfxSession m_session_tmp = m_session;

    if (tc.mode == NULL_PAR)
        out_pPar = 0;
    else if (tc.mode == STREAM)
    {
        std::string stream_name = "forBehaviorTest/valid_yuv.yuv";
        tsRawReader stream(g_tsStreamPool.Get(stream_name), m_pPar->mfx.FrameInfo);
        g_tsStreamPool.Reg();
        m_surf_in_processor = &stream;
        ProcessFrames(2);
    }
    else if (tc.mode == NULL_SESSION)
        m_session = 0;
    else if (tc.mode == MFX_PAR)
    {
        for (mfxU32 i = 0; i< m_par.NumExtParam; i++)
            par_init.AddExtBuffer(m_par.ExtParam[i]->BufferId, m_par.ExtParam[i]->BufferSz);
    }
    else if (tc.mode == DOUSE)
    {
        mfxExtVPPDoUse* vpp_du2;

        par_init.AddExtBuffer(MFX_EXTBUFF_VPP_DOUSE, sizeof(mfxExtVPPDoUse));
        vpp_du2 = (mfxExtVPPDoUse*)par_init.GetExtBuffer(MFX_EXTBUFF_VPP_DOUSE);
        vpp_du2->NumAlg = tc.alg_num;
        vpp_du2->AlgList = new mfxU32[tc.alg_num];

        memset(vpp_du2->AlgList, 0, sizeof(mfxU32)*vpp_du2->NumAlg);

        for (mfxU32 i = 0; i< tc.alg_num; i++)
            par_init.AddExtBuffer(tc.alg_list[i], GetBufSzById(tc.alg_list[i]));
    }

    g_tsStatus.expect(tc.sts);
    GetVideoParam(m_session, out_pPar);
    g_tsStatus.expect(MFX_ERR_NONE);

    if (tc.mode == DOUSE)
    {
        for (mfxU32 i = 0; i< vpp_du->NumAlg; i++)
        {
            mfxExtBuffer* buff = par_init.GetExtBuffer(vpp_du->AlgList[i]);

            // Creating empty buffer
            m_par.AddExtBuffer(vpp_du->AlgList[i], buff->BufferSz);
            mfxExtBuffer* empty = m_par.GetExtBuffer(vpp_du->AlgList[i]);

            EXPECT_FALSE(0 == memcmp(empty, buff, buff->BufferSz))
                << "ERROR: Filter's parameters before and after GetVideoPram() are not equal \n";
        }

        delete[] vpp_du->AlgList;
        vpp_du->AlgList = 0;
    }
    else if (tc.mode == MFX_PAR)
        EXPECT_EQ(m_par, par_init) << "ERROR: Filter's parameters before and after GetVideoPram() are not equal \n";

    if (tc.mode == NULL_SESSION)
        m_session = m_session_tmp;

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_getvideoparam);

}