#include "ts_decoder.h"
#include "ts_parser.h"
#include "ts_struct.h"


namespace hevcd_close
{


    class TestSuite : tsVideoDecoder
    {
    public:
        static const unsigned int n_cases;

        TestSuite() : tsVideoDecoder(MFX_CODEC_HEVC)
        {

        }
        ~TestSuite() {}
        int RunTest(unsigned int id);

    private:
        enum
        {
            MFX_PAR,
            NULL_SESSION,
            NOT_INIT,
            FAILED_INIT,
            CLOSED,
            NONE
        };

        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 type;
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

        {/*00*/ MFX_ERR_NONE, NONE },
        {/*01*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION },
        {/*02*/ MFX_ERR_NOT_INITIALIZED, NOT_INIT },
        {/*03*/ MFX_ERR_NOT_INITIALIZED, FAILED_INIT },
        {/*04*/ MFX_ERR_NOT_INITIALIZED, CLOSED },

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_case[id];

        MFXInit();
        Load();

        //set default param
        const char* sname = g_tsStreamPool.Get("conformance/hevc/itu/RPS_B_qualcomm_5.bit");
        g_tsStreamPool.Reg();
        tsBitstreamReader reader(sname, 100000);
        m_bs_processor = &reader;

        SETPARS(m_pPar, MFX_PAR);

        //init
        if (tc.type == FAILED_INIT)
        {
            g_tsStatus.expect(MFX_ERR_NULL_PTR);
            Init(m_session, NULL);
        }
        else if (tc.type != NOT_INIT)
        {
            Init();
            DecodeFrames(1);
        }

        if (tc.type == CLOSED)
        {
            Close();
        }

        g_tsStatus.expect(tc.sts);

        // Call test function
        if (tc.type == NULL_SESSION)
        {
            Close(NULL);
            g_tsStatus.expect(MFX_ERR_NONE);
            Close();
        }
        else
        {
            Close();
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevcd_close);
};