#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"


namespace hevce_close
{


    class TestSuite : tsVideoEncoder
    {
    public:
        static const unsigned int n_cases;

        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC)
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
            NOT_SYNC,
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
        {/*05*/ MFX_ERR_NONE, NOT_SYNC },


    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_case[id];
        mfxHDL hdl;
        mfxHandleType type;

        MFXInit();
        Load();

        //set defoult param
        m_par.mfx.FrameInfo.Width = 736;
        m_par.mfx.FrameInfo.Height = 576;

        if (!GetAllocator())
        {
            UseDefaultAllocator(
                (m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
                );
        }


        SETPARS(m_pPar, MFX_PAR);

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))
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


            //set handle
            if (!((m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)))
            {
                m_pFrameAllocator = GetAllocator();
                SetFrameAllocator();
                m_pVAHandle = m_pFrameAllocator;
                m_pVAHandle->get_hdl(type, hdl);
                SetHandle(m_session, type, hdl);
                m_is_handle_set = (g_tsStatus.get() >= 0);
            }

        }

        //init
        if (tc.type == FAILED_INIT)
        {
            g_tsStatus.expect(MFX_ERR_NULL_PTR);
            Init(m_session, NULL);
        }
        else if (tc.type != NOT_INIT)
        {
            Init();
        }

        if ((tc.sts >= MFX_ERR_NONE) && (tc.type != NOT_INIT) && (tc.type != FAILED_INIT))//encode 1 frame
        {
            int encoded = 0;
            while (encoded < 1)
            {
                if (MFX_ERR_MORE_DATA == EncodeFrameAsync())
                {
                    continue;
                }

                g_tsStatus.check(); TS_CHECK_MFX;


                if (tc.type != NOT_SYNC)
                    SyncOperation(); TS_CHECK_MFX;
                encoded++;
            }
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

    TS_REG_TEST_SUITE_CLASS(hevce_close);
};