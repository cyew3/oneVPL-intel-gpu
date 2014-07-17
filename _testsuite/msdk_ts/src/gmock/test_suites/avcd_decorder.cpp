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
    static const mfxU32 max_num_ctrl     = 6;
    static const mfxU32 max_num_ctrl_par = 10;
    static const mfxU32 n_par = 10;

    enum
    {
        INIT = 1,
        RESET,
        MODE_INIT,
        MODE_RESET
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct tctrl{
            mfxU32 type;
            const tsStruct::Field* field;
            mfxU32 par;
        } ctrl[max_num_ctrl];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ MFX_ERR_NONE, MODE_INIT, {{INIT, &tsStruct::mfxVideoParam.mfx.DecodedOrder, 1}}},
    {/* 1*/ MFX_ERR_NONE, MODE_RESET, {{INIT, &tsStruct::mfxVideoParam.mfx.DecodedOrder, 1},
                                       {RESET, &tsStruct::mfxVideoParam.mfx.DecodedOrder, 0}}},
    {/* 2*/ MFX_ERR_NONE, MODE_RESET, {{INIT, &tsStruct::mfxVideoParam.mfx.DecodedOrder, 0},
                                       {RESET, &tsStruct::mfxVideoParam.mfx.DecodedOrder, 1}}},
    {/* 3*/ MFX_ERR_NONE, MODE_RESET, {{INIT, &tsStruct::mfxVideoParam.mfx.DecodedOrder, 1},
                                       {INIT, &tsStruct::mfxVideoParam.AsyncDepth, 0}}},
    {/* 4*/ MFX_ERR_NONE, MODE_RESET, {{INIT, &tsStruct::mfxVideoParam.mfx.DecodedOrder, 1},
                                       {INIT, &tsStruct::mfxVideoParam.AsyncDepth, 5}}},
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

    for(mfxU32 i = 0; i < max_num_ctrl; i++)
    {
        if (tc.ctrl[i].type == INIT) {
            if(tc.ctrl[i].field)
                tsStruct::set(m_pPar, *tc.ctrl[i].field, tc.ctrl[i].par);
        }
    }

    UseDefaultAllocator(!!(m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY));

    g_tsStatus.expect(tc.sts);
    Init(m_session, m_pPar);

    if (tc.mode == MODE_RESET) {

        for(mfxU32 i = 0; i < max_num_ctrl; i++)
        {
            if (tc.ctrl[i].type == RESET) {
                if(tc.ctrl[i].field)
                    tsStruct::set(m_pPar, *tc.ctrl[i].field, tc.ctrl[i].par);
            }
        }
        g_tsStatus.expect(tc.sts);
        Reset(m_session, m_pPar);
    }

    tsExtBufType<mfxVideoParam> parOut;
    GetVideoParam(m_session, &parOut);

    if (tc.mode == MODE_INIT) {
        for(mfxU32 i = 0; i < max_num_ctrl; i++)
        {
            if (tc.ctrl[i].type == INIT)
                tsStruct::check_eq(&parOut, *tc.ctrl[i].field, tc.ctrl[i].par);
        }
    }

    if (tc.mode == MODE_RESET) {
        for(mfxU32 i = 0; i < max_num_ctrl; i++)
        {
            if (tc.ctrl[i].type == RESET)
                tsStruct::check_eq(&parOut, *tc.ctrl[i].field, tc.ctrl[i].par);
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avcd_decorder);

}