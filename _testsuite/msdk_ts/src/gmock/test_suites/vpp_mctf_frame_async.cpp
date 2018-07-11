//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright (c) 2018 Intel Corporation. All Rights Reserved.
//
#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxplugin++.h"

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace vpp_mctf_frame_async
{
    class TestSuite : tsVideoVPP
    {
    public:
        TestSuite() : tsVideoVPP() {}
        ~TestSuite() {}
        int RunTest(unsigned int id);
        static const unsigned int n_cases;

    private:
//        static const mfxU32 n_par = 6;
        static const mfxU32 MAX_ITERS_IN_TEST = 16;

        enum
        {
            RUN_MCTF_NORMAL= 1,
            RUN_MCTF_EARLY_STOP = 2,
        };

        enum
        {
            MFX_NO_PAR = 2999,
            MFX_PAR = 3000
        };

        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 mode;
            mfxU32 iopattern;
            mfxU32 async;
            // when to stop
            bool InputNullPtr[MAX_ITERS_IN_TEST];
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];

            // for an each frame to be submitted error code;
            mfxStatus return_codes[MAX_ITERS_IN_TEST];
        };

        static const tc_struct test_case[];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/* 0*/ MFX_ERR_NONE,                   RUN_MCTF_NORMAL,
                                                MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY, // IOPattern
                                                1, //async
                                                {false},
                                                {{ MFX_NO_PAR, nullptr, 0},},
                                                { MFX_ERR_NONE},

        },

        {/* 1*/ MFX_ERR_NONE, RUN_MCTF_NORMAL,

                                        MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY, // IOPattern
                                        1, //async
                                           //when to set Input nullptr
                                        {
                                              false, false, false, false,
                                              true,  true,  true,  true,
                                              true,  true,  true,  true,
                                              true,  true,  true,  true,
                                        },

                                        {
                                            { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength, 0 },
                                       },
//                                            0-iter            1-iter             2-iter               3-iter
                                       { MFX_ERR_MORE_DATA, MFX_ERR_NONE ,     MFX_ERR_NONE,      MFX_ERR_NONE,
//                                            4-iter            5-iter             6-iter               7-iter
                                         MFX_ERR_NONE,      MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,
//                                            8-iter            9-iter             10-iter              11-iter
                                         MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,
//                                            12-iter           13-iter            14-iter              15-iter
                                         MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,
                                       },
        },

        {/* 2*/ MFX_ERR_NONE, RUN_MCTF_NORMAL,

                                        MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY, // IOPattern
                                        1, //async
                                           //when to set Input nullptr
                                        {
                                            false, false, false, false,
                                            true,  true,  false,  true,
                                            true,  true,  true,  true,
                                            true,  true,  true,  true,
                                        },

                                        {
                                            { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength, 0 },
                                        },
//                                            0-iter            1-iter             2-iter               3-iter
                                        { MFX_ERR_MORE_DATA, MFX_ERR_NONE ,     MFX_ERR_NONE,      MFX_ERR_NONE,
//                                            4-iter            5-iter             6-iter               7-iter
                                          MFX_ERR_NONE,      MFX_ERR_MORE_DATA, MFX_ERR_UNDEFINED_BEHAVIOR, MFX_ERR_MORE_DATA,
//                                            8-iter            9-iter             10-iter              11-iter
                                          MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,
//                                            12-iter           13-iter            14-iter              15-iter
                                          MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,
                                        },
        },
 
        {/* 3*/ MFX_ERR_NONE, RUN_MCTF_NORMAL,

                                        MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY, // IOPattern
                                        1, //async
                                           //when to set Input nullptr
                                        {
                                              false, false, false, false,
                                              true,  false,  true,  true,
                                              true,  true,  true,  true,
                                              true,  true,  true,  true,
                                        },

                                        {
                                            { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength, 0 },
// by default: MCTF uses 2ref prediction, which is 1-frame delay
//#if (MFX_VERSION >= MFX_VERSION_NEXT)
//                                            { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode, MFX_MCTF_TEMPORAL_MODE_2REF },
//#endif
                                       },
//                                            0-iter            1-iter             2-iter               3-iter
                                       { MFX_ERR_MORE_DATA, MFX_ERR_NONE ,     MFX_ERR_NONE,      MFX_ERR_NONE,
//                                            4-iter            5-iter             6-iter               7-iter
                                         MFX_ERR_NONE,      MFX_ERR_UNDEFINED_BEHAVIOR, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,
//                                            8-iter            9-iter             10-iter              11-iter
                                         MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,
//                                            12-iter           13-iter            14-iter              15-iter
                                         MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,
                                       },
        },

        {/* 4 */ MFX_ERR_NONE, RUN_MCTF_NORMAL,

                                        MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY, // IOPattern
                                        1, //async
                                           //when to set Input nullptr
                                        {
                                              false, false, false, false,
                                              true,  true,  false,  false,
                                              true,  true,  true,  true,
                                              true,  true,  true,  true,
                                        },

                                        {
                                            { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength, 0 },
// by default: MCTF uses 2ref prediction, which is 1-frame delay
//#if (MFX_VERSION >= MFX_VERSION_NEXT)
//                                            { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode, MFX_MCTF_TEMPORAL_MODE_2REF },
//#endif
                                       },
//                                            0-iter            1-iter             2-iter               3-iter
                                       { MFX_ERR_MORE_DATA, MFX_ERR_NONE ,     MFX_ERR_NONE,      MFX_ERR_NONE,
//                                            4-iter            5-iter             6-iter               7-iter
                                         MFX_ERR_NONE,      MFX_ERR_MORE_DATA, MFX_ERR_UNDEFINED_BEHAVIOR, MFX_ERR_UNDEFINED_BEHAVIOR,
//                                            8-iter            9-iter             10-iter              11-iter
                                         MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,
//                                            12-iter           13-iter            14-iter              15-iter
                                         MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,
                                       },
        },

        {/*5*/ MFX_ERR_NONE, RUN_MCTF_NORMAL,

                                        MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY, // IOPattern
                                        1, //async
                                           //when to set Input nullptr
                                        {
                                              true,  false, false, false,
                                              true,  true,  false,  false,
                                              true,  true,  true,  true,
                                              true,  true,  true,  true,
                                        },

                                        {
                                            { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength, 0 },
// by default: MCTF uses 2ref prediction, which is 1-frame delay
//#if (MFX_VERSION >= MFX_VERSION_NEXT)
//                                            { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode, MFX_MCTF_TEMPORAL_MODE_2REF },
//#endif
                                       },
//                                            0-iter            1-iter             2-iter               3-iter
                                       { MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA ,     MFX_ERR_NONE,      MFX_ERR_NONE,
//                                            4-iter            5-iter             6-iter               7-iter
                                         MFX_ERR_NONE,      MFX_ERR_MORE_DATA, MFX_ERR_UNDEFINED_BEHAVIOR, MFX_ERR_UNDEFINED_BEHAVIOR,
//                                            8-iter            9-iter             10-iter              11-iter
                                         MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,
//                                            12-iter           13-iter            14-iter              15-iter
                                         MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,
                                       },
        },

        {/*6*/ MFX_ERR_NONE, RUN_MCTF_NORMAL,

                                        MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY, // IOPattern
                                        1, //async
                                           //when to set Input nullptr
                                        {
                                              false, true, false, false,
                                              true,  true,  false,  false,
                                              true,  true,  true,  true,
                                              true,  true,  true,  true,
                                        },

                                        {
                                            { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength, 0 },
// by default: MCTF uses 2ref prediction, which is 1-frame delay
//#if (MFX_VERSION >= MFX_VERSION_NEXT)
//                                            { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode, MFX_MCTF_TEMPORAL_MODE_2REF },
//#endif
                                       },
//                                            0-iter            1-iter             2-iter               3-iter
                                       { MFX_ERR_MORE_DATA, MFX_ERR_UNDEFINED_BEHAVIOR,  MFX_ERR_NONE,      MFX_ERR_NONE,
//                                            4-iter            5-iter             6-iter               7-iter
                                         MFX_ERR_NONE,      MFX_ERR_MORE_DATA, MFX_ERR_UNDEFINED_BEHAVIOR, MFX_ERR_UNDEFINED_BEHAVIOR,
//                                            8-iter            9-iter             10-iter              11-iter
                                         MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,
//                                            12-iter           13-iter            14-iter              15-iter
                                         MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,
                                       },
        },

        {/*7*/ MFX_ERR_NONE, RUN_MCTF_NORMAL,

                                        MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY, // IOPattern
                                        1, //async
                                           //when to set Input nullptr
                                        {
                                              true, true, false, false,
                                              true,  true,  false,  false,
                                              true,  true,  true,  true,
                                              true,  true,  true,  true,
                                        },

                                        {
                                            { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength, 0 },
// by default: MCTF uses 2ref prediction, which is 1-frame delay
//#if (MFX_VERSION >= MFX_VERSION_NEXT)
//                                            { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode, MFX_MCTF_TEMPORAL_MODE_2REF },
//#endif
                                       },
//                                            0-iter            1-iter             2-iter               3-iter
                                       { MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,      MFX_ERR_NONE,
//                                            4-iter            5-iter             6-iter               7-iter
                                         MFX_ERR_NONE,      MFX_ERR_MORE_DATA, MFX_ERR_UNDEFINED_BEHAVIOR, MFX_ERR_UNDEFINED_BEHAVIOR,
//                                            8-iter            9-iter             10-iter              11-iter
                                         MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,
//                                            12-iter           13-iter            14-iter              15-iter
                                         MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,
                                       },
        },

        {/*8*/ MFX_ERR_NONE, RUN_MCTF_NORMAL,

                                        MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY, // IOPattern
                                        1, //async
                                           //when to set Input nullptr
                                        {
                                              true, true,   false, false,
                                              true,  true,  true, true,
                                              true,  true,  true,  true,
                                              true,  true,  true,  true,
                                        },

                                        {
                                            { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength, 0 },
// by default: MCTF uses 2ref prediction, which is 1-frame delay
//#if (MFX_VERSION >= MFX_VERSION_NEXT)
//                                            { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode, MFX_MCTF_TEMPORAL_MODE_2REF },
//#endif
                                       },
//                                            0-iter            1-iter             2-iter               3-iter
                                       { MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,  MFX_ERR_MORE_DATA, MFX_ERR_NONE,
//                                            4-iter            5-iter             6-iter               7-iter
                                         MFX_ERR_NONE,      MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,
//                                            8-iter            9-iter             10-iter              11-iter
                                         MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,
//                                            12-iter           13-iter            14-iter              15-iter
                                         MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA, MFX_ERR_MORE_DATA,
                                       },
        },

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];

        MFXInit();
        mfxStatus sts;

        // set up parameters for case
        for (mfxU32 i = 0; i < MAX_NPARS; i++)
        {
            if (tc.set_par[i].f && MFX_PAR != tc.set_par[i].ext_type)
            {
                tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
            }
        }


        SETPARS(&m_par, MFX_PAR);

        m_pPar->IOPattern = tc.iopattern;
        m_pPar->AsyncDepth = tc.async;

        m_spool_in.UseDefaultAllocator(!!(m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY));
        SetFrameAllocator(m_session, m_spool_in.GetAllocator());
        m_spool_out.SetAllocator(m_spool_in.GetAllocator(), true);
        AllocSurfaces();

        Init(m_session, m_pPar);

        ///////////////////////////////////////////////////////////////////////////
        g_tsStatus.expect(tc.sts);
        g_tsStatus.check();
        bool bNullInput(false);
        std::list<mfxSyncPoint> list_of_syncs;
        for(size_t i = 0; i < MAX_ITERS_IN_TEST; ++i)
        {
            bNullInput = tc.InputNullPtr[i];
            g_tsStatus.expect(tc.return_codes[i]);
            sts = RunFrameVPPAsync(bNullInput);
            g_tsStatus.check();
            if(*m_pSyncPoint)
                list_of_syncs.push_back(*m_pSyncPoint);

            if (tc.async == list_of_syncs.size())
            {
                for (auto &it : list_of_syncs)
                {
                    SyncOperation(it);
                    g_tsStatus.expect(mfxStatus(MFX_ERR_NONE));
                }
                list_of_syncs.clear();
            }
        }

        Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(vpp_mctf_frame_async);

}

