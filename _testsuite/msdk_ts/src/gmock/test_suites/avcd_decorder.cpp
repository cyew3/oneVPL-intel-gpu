#include "ts_decoder.h"
#include "ts_struct.h"

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace avcd_decorder
{

typedef void (*callback)(tsExtBufType<mfxVideoParam>&, mfxU32, mfxU32);

void ext_buf(tsExtBufType<mfxVideoParam>& par, mfxU32 id, mfxU32 size)
{
    par.AddExtBuffer(id, size);
}

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_AVC) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 n_par = 10;

    enum
    {
        INIT = 1,
        RESET = 2,
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[n_par];
        struct
        {
            callback func;
            mfxU32 buf;
            mfxU32 size;
        } set_buf;
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{   // Async_depth
    {/*32*/ MFX_ERR_NONE, INIT},
    {/*33*/ MFX_ERR_NONE, RESET},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    m_par_set = true;
    const tc_struct& tc = test_case[id];

    MFXInit();

        if(m_uid) // load plugin
            Load();

    for(mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par[i].f)
        {
            tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
        }
    }

    UseDefaultAllocator(!!(m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY));

    m_par.mfx.DecodedOrder = 1;
    g_tsStatus.expect(tc.sts);
    Init(m_session, m_pPar);

    if (tc.mode == RESET) {
        m_par.mfx.DecodedOrder = 0;
        g_tsStatus.expect(tc.sts);
        Reset(m_session, m_pPar);
    }

    tsExtBufType<mfxVideoParam> parOut;
    GetVideoParam(m_session, &parOut);

        if(tc.mode == INIT)
            tsStruct::check_eq(&parOut, tsStruct::mfxVideoParam.mfx.DecodedOrder, 1);
        if(tc.mode == RESET)
            tsStruct::check_eq(&parOut, tsStruct::mfxVideoParam.mfx.DecodedOrder, 0);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avcd_decorder);

}