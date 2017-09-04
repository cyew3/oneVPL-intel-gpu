/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace hevce_default_lowpower
{

    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC)
        {
            // LowPower require Progressive
            m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        }
        ~TestSuite() { }
        int RunTest(unsigned int id);
        static const unsigned int n_cases;
    private:

        enum TYPE
        {
            QUERY =0x2,
            QUERY_IO_SUFF = 0x4,
            INIT = 0x8,
        };

        enum
        {
            MFXPAR = 1,
            QUERY_EXP = 0xEE00 | QUERY,
            QUERY_IO_EXP = 0xEE00 | QUERY_IO_SUFF,
            INIT_EXP = 0xEE00 | INIT,
        };

        enum
        {
            UNSUPPORTED = 0,
            DUALPIPE_DEAFULT = 0x1,
            LOWPOWER_DEFAULT = 0x2,
            DUALPIPE_SUPPORTED = 0x10,
            LOWPOWER_SUPPORTED = 0x20,
        };

        typedef bool(*IsTcApplcableFuncPtr)(void);

        struct tc_struct
        {
            mfxU32 type;
            mfxStatus stsQuery;
            mfxStatus stsInit;
            int target;
            int mask;

            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        static const tc_struct test_case[];

        static int  getPlatfromFlags()
        {
            int ret = DUALPIPE_SUPPORTED;
            switch (g_tsHWtype)
            {
            case MFX_HW_LKF:
                ret |= LOWPOWER_DEFAULT;
                break;
            default:
                ret |= DUALPIPE_DEAFULT;
                break;
            }

            if (g_tsHWtype >= MFX_HW_CNL)
                ret |= LOWPOWER_SUPPORTED;

            if (g_tsHWtype == MFX_HW_LKF)
                ret = ret & ~DUALPIPE_SUPPORTED;

            return ret;
        }
    };


    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        // default value
        {/*00*/  QUERY | INIT, MFX_ERR_NONE,MFX_ERR_NONE,
            DUALPIPE_DEAFULT,DUALPIPE_DEAFULT,
        {
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_UNKNOWN },
            { QUERY_EXP, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_UNKNOWN },
            { INIT_EXP, &tsStruct::mfxVideoParam.mfx.LowPower, 33 },
        }
        },

        {/*01*/  QUERY | INIT, MFX_ERR_NONE,MFX_ERR_NONE,
            LOWPOWER_DEFAULT,LOWPOWER_DEFAULT,
        {
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_UNKNOWN },
            { QUERY_EXP, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_UNKNOWN },
            { INIT_EXP, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
        }
        },

        // invalid value
        {/*02*/  QUERY | INIT, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
            DUALPIPE_DEAFULT,DUALPIPE_DEAFULT,
        {
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.LowPower, 10 },
            { QUERY_EXP, &tsStruct::mfxVideoParam.mfx.LowPower, 0 },
            { INIT_EXP, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_OFF },
        }
        },

        {/*03*/  QUERY | INIT, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
            LOWPOWER_DEFAULT,LOWPOWER_DEFAULT,
        {
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.LowPower, 10 },
            { QUERY_EXP, &tsStruct::mfxVideoParam.mfx.LowPower, 0 },
            { INIT_EXP, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
        }
        },

        // LP ON -  supported
        {/*04*/  QUERY | INIT, MFX_ERR_NONE,MFX_ERR_NONE,
            LOWPOWER_SUPPORTED,LOWPOWER_SUPPORTED,
        {
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
            { QUERY_EXP, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
            { INIT_EXP, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
        }
        },

        // LP ON -  unsupported
        {/*05*/  QUERY | INIT, MFX_ERR_UNSUPPORTED,MFX_ERR_DEVICE_FAILED,
            UNSUPPORTED,LOWPOWER_SUPPORTED,
        {
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
        }
        },

        // DP ON -  supported
        {/*06*/  QUERY | INIT, MFX_ERR_NONE,MFX_ERR_NONE,
            DUALPIPE_SUPPORTED,DUALPIPE_SUPPORTED,
        {
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_OFF },
            { QUERY_EXP, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_OFF },
            { INIT_EXP, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_OFF },
        }
        },

        // DP ON -  unsupported
        {/*07*/  QUERY | INIT, MFX_ERR_UNSUPPORTED,MFX_ERR_DEVICE_FAILED,
            UNSUPPORTED,DUALPIPE_SUPPORTED,
        {
            { MFXPAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_OFF },
        }
        },

    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];
        mfxStatus sts = MFX_ERR_NONE;

        MFXInit();
        m_session = tsSession::m_session;

        if ( (getPlatfromFlags() & tc.mask ) != tc.target)
        {
            g_tsLog << "\n\nWARNING: SKIP test unsupported platform\n\n";
            throw tsSKIP;
        }

        //load the plugin in advance.
        if (!m_loaded)
        {
            Load();
        }

        //This test is for hw plugin only
        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data))) {
            if (g_tsHWtype < MFX_HW_SKL)
            {
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                return 0;
            }
        }
        else {
            g_tsLog << "WARNING: only HEVCe HW plugin is tested\n";
            return 0;
        }

        if (tc.type & QUERY)
        {
            tsExtBufType<mfxVideoParam> out_par;

            SETPARS(m_par, MFXPAR);
            out_par = m_par;
            m_pParOut = &out_par;
            g_tsStatus.expect(tc.stsQuery);
            Query();
            EXPECT_TRUE(COMPAREPARS(out_par, QUERY_EXP)) << "ERROR: QUERY out parameters doesn't match expectation\n";
        }

        if (tc.type & INIT)
        {
            tsExtBufType<mfxVideoParam> out_par;

            SETPARS(m_par, MFXPAR);
            out_par = m_par;
            m_pParOut = &out_par;

            g_tsStatus.expect(tc.stsInit);
            Init();

            if (g_tsStatus.m_status >= MFX_ERR_NONE)
            {
                // run getVP only after succesefull init
                g_tsStatus.expect(MFX_ERR_NONE); // if init succesefull GetVP must not fails
                GetVideoParam(m_session, &out_par);
                EXPECT_TRUE(COMPAREPARS(out_par, INIT_EXP)) << "ERROR: GetVideoParam(after INIT) parameters doesn't match expectation\n";
            }
        }

        if (m_initialized)
        {
            Close();
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_default_lowpower);
}