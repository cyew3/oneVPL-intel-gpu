#include "ts_encoder.h"
#include "ts_enc.h"
#include "ts_struct.h"

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace lookahead_1N
{

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 n_par = 10;

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[n_par];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ MFX_ERR_NONE, 0},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    if (m_uid) // load plugin
        Load();

    for (mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par[i].f)
        {
            tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
        }
    }

    g_tsStatus.expect(tc.sts);

    /*
     * ENC
     */
    tsVideoENC enc(MFX_CODEC_AVC, true, (tsVideoEncoder*)this);
    /*mfxExtLAControl& lactrl = enc.m_par;
    lactrl.LookAheadDepth = 8;
    lactrl.DependencyDepth = 4;
    lactrl.NumOutStream = 2;
    for (mfxU16 i = 0; i < lactrl.NumOutStream; i++)
    {
        lactrl.OutStream[i].Width = 720;
        lactrl.OutStream[i].Height = 480;
    }*/
    enc.Init(m_session, enc.m_pPar);

    /*
     * Encoder
     */
    tsVideoEncoder::Init(m_session, m_pPar);

    ///////////////////////////////////////////////////////////////////////////
    enc.Encode1N(2);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(lookahead_1N);

}