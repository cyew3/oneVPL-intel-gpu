/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2003-2016 Intel Corporation. All Rights Reserved.
//
*/
#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace hevce_hrd_violation
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
            NONE
        };

        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 val;
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
        {/* 0*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_CODINGOPTION_ON, {} },
        {/* 1*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_CODINGOPTION_OFF, {} },
        {/* 2*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_CODINGOPTION_ADAPTIVE, {} },
        {/* 3*/ MFX_ERR_NONE, MFX_CODINGOPTION_UNKNOWN, {} },

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);



    int TestSuite::RunTest(unsigned int id)
    {
        mfxExtCodingOption *buff_in = NULL;
        try
        {

            const tc_struct& tc = test_case[id];

            MFXInit();
            Load();

            SETPARS(m_pPar, MFX_PAR);
            g_tsStatus.expect(tc.sts);

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

                // HEVCE_HW heve don't support NalHrdConformance
                if (tc.sts != MFX_ERR_NONE)
                    g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
                if ((tc.val == MFX_CODINGOPTION_OFF) || (tc.val == MFX_CODINGOPTION_UNKNOWN))
                    g_tsStatus.expect(MFX_ERR_NONE);
            }

            //init mfxExtCodingOption
            buff_in = new mfxExtCodingOption();
            memset(buff_in, 0, sizeof(mfxExtCodingOption));
            buff_in->Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
            buff_in->Header.BufferSz = sizeof(mfxExtCodingOption);
            buff_in->NalHrdConformance = tc.val;
            m_par.NumExtParam = 1;
            m_par.ExtParam = (mfxExtBuffer**)&buff_in;
            m_pParOut = new mfxVideoParam;
            *m_pParOut = m_par;

            //call tested funcion
            Init(m_session, m_pPar);
            if (buff_in)
                delete  buff_in;
        }
        catch (tsRes r)
        {
            if (buff_in)
                delete buff_in;
            return r;
        }
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_hrd_violation);
}