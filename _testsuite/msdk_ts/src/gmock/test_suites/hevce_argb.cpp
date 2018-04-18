/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

#define NONE 0xffff
#define N_FRAMES 5

namespace hevce_argb
{
    enum TYPE {
        QUERY  = 0x1,
        INIT   = 0x2,
        ENCODE = 0x4,
        RESET  = 0x8
    };
    struct tc_struct
    {
        struct status {
            mfxStatus query;
            mfxStatus init;
            mfxStatus encode;
            mfxStatus reset;
        } sts;
        mfxU32 type;
        mfxU32 FourCC;
        mfxU32 ChromaFormat;
        mfxU16 TargetChromaFormatPlus1;
    };

    class TestSuite : tsVideoEncoder {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC) {}
        ~TestSuite() {}
        int RunTest(unsigned int id);
        void SetPars(tc_struct tc);
        static const unsigned int n_cases;
        static const tc_struct test_case[];
    };

    const tc_struct TestSuite::test_case[] =
    {

        // RGB4 test-cases

        {/*00*/
        { MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, },
        QUERY | INIT | ENCODE | RESET,
        MFX_FOURCC_RGB4,        MFX_CHROMAFORMAT_YUV444, NONE
        },

        {/*01*/
        { MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, },
        QUERY | INIT | ENCODE | RESET,
        MFX_FOURCC_RGB4,        MFX_CHROMAFORMAT_YUV444, MFX_CHROMAFORMAT_YUV420 + 1
        }, //also TargetChromaPlus1 = MFX_CHROMAFORMAT_YUV422 + 1 and TargetChromaPlus1 = MFX_CHROMAFORMAT_YUV422 + 1 may be checked, but need to query caps before (to get expected status)

        {/*02*/
        { MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM },
        QUERY | INIT,
        MFX_FOURCC_RGB4,        MFX_CHROMAFORMAT_YUV420, MFX_CHROMAFORMAT_YUV444 + 1
        },

        {/*03*/
        { MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM },
        QUERY | INIT,
        MFX_FOURCC_RGB4,        MFX_CHROMAFORMAT_YUV420, MFX_CHROMAFORMAT_YUV420 + 1
        },

        {/*04*/
        { MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM },
        QUERY | INIT,
        MFX_FOURCC_RGB4,        MFX_CHROMAFORMAT_YUV420, NONE
        },

        //A2RGB10 test-cases

        {/*05*/
        { MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, },
        QUERY | INIT | ENCODE | RESET,
        MFX_FOURCC_A2RGB10,     MFX_CHROMAFORMAT_YUV444, NONE
        },

        {/*06*/
        { MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM },
        QUERY | INIT,
        MFX_FOURCC_A2RGB10,     MFX_CHROMAFORMAT_YUV420,  MFX_CHROMAFORMAT_YUV420 + 1
        },

        {/*07*/
        { MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM },
        QUERY | INIT,
        MFX_FOURCC_A2RGB10,     MFX_CHROMAFORMAT_YUV420, NONE
        },

        {/*08*/
        { MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM },
        QUERY | INIT,
        MFX_FOURCC_A2RGB10,     MFX_CHROMAFORMAT_YUV422, NONE
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    void TestSuite::SetPars(tc_struct tc)
    {
        m_par.mfx.FrameInfo.FourCC = tc.FourCC;
        m_par.mfx.FrameInfo.ChromaFormat = tc.ChromaFormat;
        if (tc.TargetChromaFormatPlus1 != NONE)
        {
            mfxExtCodingOption3& cod3 = m_par;
            cod3.TargetChromaFormatPlus1 = tc.TargetChromaFormatPlus1;
        }
    }

    int TestSuite::RunTest(unsigned int id) {
        TS_START;
        mfxStatus sts;
        auto& tc = test_case[id];

        if (g_tsOSFamily != MFX_OS_FAMILY_WINDOWS) {
            g_tsLog << "[ SKIPPED ] This test is only for Windows Platform\n";
            throw tsSKIP;
        }

        if (g_tsConfig.lowpower == MFX_CODINGOPTION_ON && g_tsHWtype < MFX_HW_CNL)
        {
            g_tsLog << "[ SKIPPED ] HEVCe VDEnc is unsupported before CNL\n";
            throw tsSKIP;
        }

        SetPars(tc);

        if (m_par.mfx.FrameInfo.FourCC == MFX_FOURCC_A2RGB10 && (g_tsHWtype < MFX_HW_ICL || g_tsConfig.lowpower == MFX_CODINGOPTION_OFF) )
        {
            g_tsLog << "[ SKIPPED ] A2RGB10 is supported only for HEVCe VDEnc on ICL+\n";
            throw tsSKIP;
        }

        MFXInit();
        Load();

        if (tc.type & QUERY) {
            g_tsStatus.expect(tc.sts.query);
            Query();
        }

        if (tc.type & INIT) {
            SetPars(tc);
            g_tsStatus.expect(tc.sts.init);
            sts = Init();
        }

        if (tc.type & ENCODE) {
            SetPars(tc);
            g_tsStatus.expect(tc.sts.encode);
            EncodeFrames(N_FRAMES);
        }

        if (tc.type & RESET) {
            SetPars(tc);
            g_tsStatus.expect(tc.sts.reset);
            Reset();

            SetPars(tc);
            g_tsStatus.expect(tc.sts.encode);
            EncodeFrames(N_FRAMES);
        }

        if (m_initialized) {
            Close();
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_argb);
}