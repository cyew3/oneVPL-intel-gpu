#include "ts_encoder.h"
#include "ts_struct.h"

namespace hevce_query
{
class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC){}
    ~TestSuite() { }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:

    enum
    {
        MFX_PAR
    };

    struct tc_struct
    {
        mfxStatus sts;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1}},
    {/*01*/ MFX_ERR_NONE, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 2}},
    {/*02*/ MFX_ERR_NONE, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 3}},
    {/*03*/ MFX_ERR_NONE, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 4}},
    {/*04*/ MFX_ERR_NONE, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 5}},
    {/*05*/ MFX_ERR_NONE, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 6}},
    {/*06*/ MFX_ERR_NONE, {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 7}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    m_pParOut = new mfxVideoParam;
    *m_pParOut = m_par;

    MFXInit();

    static const mfxPluginUID  HEVCE_HW = {{0x6f,0xad,0xc7,0x91,0xa0,0xc2,0xeb,0x47,0x9a,0xb6,0xdc,0xd5,0xea,0x9d,0xa3,0x47}};

    Load();

    SETPARS(m_pPar, MFX_PAR);

    g_tsStatus.expect(tc.sts);

    if (0 == memcmp(m_uid->Data, HEVCE_HW.Data, sizeof(HEVCE_HW.Data)))
    {
        if (g_tsHWtype < MFX_HW_SKL) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
        {
            g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            g_tsLog << "WARNING: Unsupported HW Platform!\n";
            Query();
            return 0;
        }
        //HEVCE_HW need aligned width and height for 32
        m_par.mfx.FrameInfo.Width = ((m_par.mfx.FrameInfo.Width + 32 - 1) & ~(32 - 1));
        m_par.mfx.FrameInfo.Height = ((m_par.mfx.FrameInfo.Height + 32 - 1) & ~(32 - 1));

        // HW: supported only TU = {1,4,7}. Mapping: 2->1; 3->4; 5->4; 6->7
        if(tc.set_par[0].v == 1 || tc.set_par[0].v == 4 || tc.set_par[0].v == 7)
        {
            Query(m_session, m_pPar, m_pParOut);
            tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, tsStruct::get(m_pPar, *tc.set_par[0].f));
        }
        else
        {
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            Query(m_session, m_pPar, m_pParOut);

            if (tc.set_par[0].v % 3 == 0)
                tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, tsStruct::get(m_pPar, *tc.set_par[0].f)+1);
            else
                tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, tsStruct::get(m_pPar, *tc.set_par[0].f)-1);
        }
    }
    else if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_GACC.Data, sizeof(MFX_PLUGINID_HEVCE_GACC.Data)))
    {
        // GACC: supported only TU = {4,7}. Mapping: {1,2,3,5}->4; 6->7
        if(tc.set_par[0].v == 4 || tc.set_par[0].v == 7)
        {
            Query(m_session, m_pPar, m_pParOut);
            tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, tsStruct::get(m_pPar, *tc.set_par[0].f));
        }
        else
        {
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            Query(m_session, m_pPar, m_pParOut);

            if (tc.set_par[0].v == 6)
                tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, tsStruct::get(m_pPar, *tc.set_par[0].f)+1);
            else
                tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, 4);
        }
    }
    else // SW: supported TU = {1,2,3,4,5,6,7}
    {
        Query(m_session, m_pPar, m_pParOut);
        tsStruct::check_eq(m_pParOut, *tc.set_par[0].f, tsStruct::get(m_pPar, *tc.set_par[0].f));
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_query);
}