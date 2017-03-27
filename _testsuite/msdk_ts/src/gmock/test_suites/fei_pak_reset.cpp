/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2016-2017 Intel Corporation. All Rights Reserved.
//
//
*/

#include "ts_pak.h"
#include "ts_struct.h"

namespace pak_reset
{

    class TestSuite : tsVideoPAK
    {
        public:
            TestSuite(): tsVideoPAK(MFX_FEI_FUNCTION_PAK, MFX_CODEC_AVC, true) {}
            ~TestSuite() {}
            int RunTest(unsigned int id);
            static const unsigned int n_cases;

        private:
            enum
            {
                RESET = 1,
                INIT,
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
        {/*00*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
            {
                {INIT,  &tsStruct::mfxVideoParam.mfx.GopRefDist,                               3},
                {RESET, &tsStruct::mfxVideoParam.mfx.GopRefDist,                              10},
            },
        },
        {/*01*/ MFX_ERR_NONE,
            {
                {INIT,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,                       1920},
                {INIT,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,                      1088},
                {INIT,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,                       1920},
                {INIT,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,                       1088},
                {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,                        720},
                {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,                       576},
                {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,                        720},
                {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,                        576},
            },
        },
        {/*02*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
            {
                {INIT,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,                        720},
                {INIT,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,                       576},
                {INIT,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,                        720},
                {INIT,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,                        576},
                {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,                       1920},
                {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,                      1088},
                {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,                       1920},
                {RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,                       1088},
            },
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];

        MFXInit();

        SetFrameAllocator(m_session, m_pVAHandle);

        tsExtBufType<mfxVideoParam> def(m_par);

        SETPARS(&m_par, INIT);

        g_tsStatus.expect(MFX_ERR_NONE);
        Init(m_session, &m_par);

        tsExtBufType<mfxVideoParam> par_init(def);

        g_tsStatus.expect(MFX_ERR_NONE);
        GetVideoParam(m_session, &par_init);

        tsExtBufType<mfxVideoParam> par_reset(def);
        SETPARS(&par_reset, RESET);

        g_tsStatus.expect(tc.sts);
        Reset(m_session, &par_reset);

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(fei_pak_reset);

}
