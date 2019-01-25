/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2019 Intel Corporation. All Rights Reserved.

File Name: avce_init.cpp

\*
This test checks that AVC encoder has correct parameters after init.

Algorithm:
- Initializing Media SDK lib
- Initializing suite and case parameters
- Run Init()
- Check Init() status
- Check that input parameters are not changed
- Check BufferSizeInKB and InitialDelayInKB
*/

#include "ts_encoder.h"
#include "ts_struct.h"
#include <climits>

#define WIDTH 2048
#define HEIGHT 2048
// Formula was taken from GetMaxCodedFrameSizeInKB function (MSDK AVC encoder implementation)
#define MAX_MB_BYTES 3200 / 8
#define MULTIPLIER 1
#define MAX_CODED_FRAME_SIZE_IN_KB std::min(UINT_MAX, (WIDTH * HEIGHT * MULTIPLIER / (16u * 16u) * MAX_MB_BYTES + 999u) / 1000u)


namespace avce_init
{
    class TestSuite : tsVideoEncoder
    {
    private:
        enum
        {
            MFX_PAR,
            BUFFER_SIZE,
            BUFFER_SIZE_DEFAULT,
            INITIAL_DELAY_DEFAULT,
            DEFAULTS,
            NONE
        };

        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 type;
            mfxU32 sub_type;
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        static const tc_struct test_case[];

    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
        ~TestSuite() { }
        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);
        int RunTest(const tc_struct& tc, unsigned int fourcc_id);
        tsExtBufType<mfxVideoParam> InitParams();
        static const unsigned int n_cases;

    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        // BRC modes with different BufferSizeInKB and InitialDelayInKB values
        {/*00*/ MFX_ERR_NONE, BUFFER_SIZE, BUFFER_SIZE_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
        {/*01*/ MFX_ERR_NONE, BUFFER_SIZE, BUFFER_SIZE_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
        {/*02*/ MFX_ERR_NONE, BUFFER_SIZE, BUFFER_SIZE_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT } },
        {/*03*/ MFX_ERR_NONE, BUFFER_SIZE, BUFFER_SIZE_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*04*/ MFX_ERR_NONE, BUFFER_SIZE, BUFFER_SIZE_DEFAULT,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.ICQQuality, 50 }
            }
        },
        {/*05*/ MFX_ERR_NONE, BUFFER_SIZE, BUFFER_SIZE_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
        {/*06*/ MFX_ERR_NONE, BUFFER_SIZE, BUFFER_SIZE_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD } },
        {/*07*/ MFX_ERR_NONE, BUFFER_SIZE, BUFFER_SIZE_DEFAULT,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.ICQQuality, 50 }
            }
        },

        {/*08*/ MFX_ERR_NONE, BUFFER_SIZE, INITIAL_DELAY_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
        {/*09*/ MFX_ERR_NONE, BUFFER_SIZE, INITIAL_DELAY_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
        {/*10*/ MFX_ERR_NONE, BUFFER_SIZE, INITIAL_DELAY_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT } },
        {/*11*/ MFX_ERR_NONE, BUFFER_SIZE, INITIAL_DELAY_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*12*/ MFX_ERR_NONE, BUFFER_SIZE, INITIAL_DELAY_DEFAULT,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.ICQQuality, 50 }
            }
        },
        {/*13*/ MFX_ERR_NONE, BUFFER_SIZE, INITIAL_DELAY_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
        {/*14*/ MFX_ERR_NONE, BUFFER_SIZE, INITIAL_DELAY_DEFAULT, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD } },
        {/*15*/ MFX_ERR_NONE, BUFFER_SIZE, INITIAL_DELAY_DEFAULT,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.ICQQuality, 50 }
            }
        },

        {/*16*/ MFX_ERR_NONE, BUFFER_SIZE, DEFAULTS, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
        {/*17*/ MFX_ERR_NONE, BUFFER_SIZE, DEFAULTS, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
        {/*18*/ MFX_ERR_NONE, BUFFER_SIZE, DEFAULTS, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT } },
        {/*19*/ MFX_ERR_NONE, BUFFER_SIZE, DEFAULTS, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*20*/ MFX_ERR_NONE, BUFFER_SIZE, DEFAULTS,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.ICQQuality, 50 }
            }
        },
        {/*21*/ MFX_ERR_NONE, BUFFER_SIZE, DEFAULTS, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
        {/*22*/ MFX_ERR_NONE, BUFFER_SIZE, DEFAULTS, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD } },
        {/*23*/ MFX_ERR_NONE, BUFFER_SIZE, DEFAULTS,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.ICQQuality, 50 }
            }
        },

        {/*24*/ MFX_ERR_NONE, BUFFER_SIZE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
        {/*25*/ MFX_ERR_NONE, BUFFER_SIZE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
        {/*26*/ MFX_ERR_NONE, BUFFER_SIZE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT } },
        {/*27*/ MFX_ERR_NONE, BUFFER_SIZE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*28*/ MFX_ERR_NONE, BUFFER_SIZE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.ICQQuality, 50 }
            }
        },
        {/*29*/ MFX_ERR_NONE, BUFFER_SIZE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA } },
        {/*30*/ MFX_ERR_NONE, BUFFER_SIZE, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD } },
        {/*31*/ MFX_ERR_NONE, BUFFER_SIZE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.ICQQuality, 50 }
            }
        }
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    tsExtBufType<mfxVideoParam> TestSuite::InitParams() {
        tsExtBufType<mfxVideoParam> par;
        par.mfx.CodecId = MFX_CODEC_AVC;
        par.mfx.TargetKbps = 10000;
        par.mfx.MaxKbps = par.mfx.TargetKbps;
        par.mfx.FrameInfo.FrameRateExtN = 30;
        par.mfx.FrameInfo.FrameRateExtD = 1;
        par.mfx.FrameInfo.ChromaFormat = 1;
        par.mfx.FrameInfo.PicStruct = 1;
        par.mfx.FrameInfo.Width = WIDTH;
        par.mfx.FrameInfo.Height = HEIGHT;
        par.mfx.GopRefDist = 1;
        par.IOPattern = 2;
        par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION2, sizeof(mfxExtCodingOption2));
        return par;
    }

    int TestSuite::RunTest(const tc_struct& tc, unsigned int fourcc_id)
    {
        TS_START;

        mfxStatus sts;

        MFXInit();

        m_par = InitParams();

        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else
        {
            g_tsLog << "WARNING: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        SETPARS(m_pPar, MFX_PAR);

        if (!GetAllocator())
        {
            UseDefaultAllocator(
                (m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
                );
        }

        if (!m_pFrameAllocator && (m_par.IOPattern & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET | MFX_IOPATTERN_IN_VIDEO_MEMORY)))
        {
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();
        }

        if (tc.type == BUFFER_SIZE)
        {
            switch (tc.sub_type)
            {
            case BUFFER_SIZE_DEFAULT:
            {
                // We need to set a quite large InitialDelayInKB,
                // it should be more than bufferSizeInKB = m_par.mfx.TargetKbps * DEFAULT_CPB_IN_SECONDS(=2) / 8
                // (from MSDK AVC encoder implementation, MfxHwH264Encode::SetDefaults(...))
                // so DEFAULT_CPB_IN_SECONDS will be increased
                // to make sure that BufferSizeInKB >= InitialDelayInKB after MFXVideoENCODE_Init
                m_par.mfx.BufferSizeInKB = 0;
                m_par.mfx.InitialDelayInKB = m_par.mfx.TargetKbps * 3 / 8;
                break;
            }
            case INITIAL_DELAY_DEFAULT:
            {
                // If InitialDelayInKB == 0 it is calculated as BufferSizeInKB / 2
                // BufferSizeInKB should > MAX_CODED_FRAME_SIZE_IN_KB
                m_par.mfx.BufferSizeInKB = MAX_CODED_FRAME_SIZE_IN_KB + 1;
                m_par.mfx.InitialDelayInKB = 0;
                break;
            }
            case DEFAULTS:
            {
                m_par.mfx.BufferSizeInKB = 0;
                m_par.mfx.InitialDelayInKB = 0;
                break;
            }
            case NONE:
            {
                // BufferSizeInKB should > MAX_CODED_FRAME_SIZE_IN_KB
                // InitialDelayInKB should < BufferSizeInKB
                m_par.mfx.BufferSizeInKB = MAX_CODED_FRAME_SIZE_IN_KB + 1;
                m_par.mfx.InitialDelayInKB = MAX_CODED_FRAME_SIZE_IN_KB;
                break;
            }
            default: break;
            }
        }

        if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        {
            m_par.mfx.QPI = 26;
            m_par.mfx.QPP = 26;
            m_par.mfx.QPB = 26;
        }

        sts = tc.sts;

        mfxVideoParam *orig_par = NULL;

        orig_par = new mfxVideoParam();
        memcpy(orig_par, m_pPar, sizeof(mfxVideoParam));

        //call tested function
        g_tsStatus.expect(sts);
        TRACE_FUNC2(MFXVideoENCODE_Init, m_session, m_pPar);
        sts = MFXVideoENCODE_Init(m_session, m_pPar);

        g_tsStatus.check(sts);
        if ((orig_par) && (tc.sts == MFX_ERR_NONE))
        {
            EXPECT_EQ(0, memcmp(orig_par, m_pPar, sizeof(mfxVideoParam)))
                << "ERROR: Input parameters must not be changed in Init()";
            delete orig_par;
        }

        if (tc.type == BUFFER_SIZE)
        {
            mfxVideoParam get_par = {};
            GetVideoParam(m_session, &get_par);
            if (m_par.mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
                m_par.mfx.RateControlMethod != MFX_RATECONTROL_ICQ &&
                m_par.mfx.RateControlMethod != MFX_RATECONTROL_LA_ICQ &&
                m_par.mfx.RateControlMethod != MFX_RATECONTROL_LA)
                //InitialDelayInKB is not used if RateControlMethod is not HRD compliant
            {
                if (tc.sub_type == NONE)
                {
                    EXPECT_EQ(get_par.mfx.BufferSizeInKB, m_par.mfx.BufferSizeInKB)
                        << "ERROR: BufferSizeInKB must not be changed";
                    EXPECT_EQ(get_par.mfx.InitialDelayInKB, m_par.mfx.InitialDelayInKB)
                        << "ERROR: InitialDelayInKB must not be changed";
                }
                else
                {
                    EXPECT_GE(get_par.mfx.BufferSizeInKB, get_par.mfx.InitialDelayInKB)
                        << "ERROR: BufferSizeInKB must not be less than InitialDelayInKB";
                }
            }
            else
            {
                EXPECT_GE(get_par.mfx.BufferSizeInKB, 0u)
                        << "ERROR: BufferSizeInKB must be greater than 0";
            }
        }

        g_tsStatus.expect(MFX_ERR_NONE);
        Close();


        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(avce_init, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
}
