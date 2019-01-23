/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

/*
Per-frame Deblocking disabling disable_deblocking_filter_idc in slice_header
mfxExtCodingOption2::DisableDeblockingIdc = 0..2
mfxExtCodingOption3::DeblockingAlphaTcOffset = -12..12
mfxExtCodingOption3::DeblockingBetaOffset = -12..12
slice_header::disable_deblocking_filter_idc = 0..2
slice_header::slice_alpha_c0_offset_div2 = -6..6
slice_header::slice_beta_offset_div2 = -6..6
*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace avce_co2_disable_deblocking
{

enum
{
    FEATURE_DEBLOCKING_DISABLED = 16,
    FEATURE_ENABLED = 32
};

enum
{
    MAX_ALPHA = 12,
    MAX_BETA = 12,
    INVALID_VALUE_ALPHA = -14,
    INVALID_VALUE_BETA = 14
};

class TestSuite : tsVideoEncoder, tsSurfaceProcessor
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC)
        , mode(0)
        , n_frame(0)
    {
        m_filler = this;
        memset(buffers, 0, sizeof(mfxExtBuffer*)*200);
    }
    ~TestSuite()
    {
        for (mfxU32 i = 0; i < 100; i++)
            delete [] buffers[i];
    }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    mfxU32 mode;
    mfxU32 n_frame;
    mfxExtBuffer* buffers[100][2];
    struct tc_par;

    enum
    {
        MFX_PAR = 1,
        EXT_COD2,
        EXT_COD3
    };

    enum
    {
        QUERY = 1,

        RUNTIME_ONLY = 1 << 1,
        EVERY_OTHER  = 1 << 2,

        RESET_ON  = 1 << 3,
        RESET_OFF = 1 << 4,

        INVALID_PARAMS = 1 << 5
    };

    struct tc_struct
    {
        mfxStatus i_sts;
        mfxStatus e_sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
        char* skips;
    };

    static const tc_struct test_case[];

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        assert(n_frame < 100);

        mfxU16 idc = 1;
        mfxI16 alpha = 0;
        mfxI16 beta = 0;

        if (mode & EVERY_OTHER || mode & RUNTIME_ONLY)
        {
            idc = n_frame % 2 ? 0 : 1;
        }

        alpha = idc == 1 ? 0 : (n_frame % 7 - 6) * 2;
        beta = idc == 1 ? 0 : (n_frame % 7 - 6) * 2;

        if (mode & INVALID_PARAMS)
        {
            idc = 0;
            alpha = INVALID_VALUE_ALPHA;
            beta = INVALID_VALUE_BETA;
        }

        buffers[n_frame][0] = (mfxExtBuffer*) new mfxExtCodingOption2;
        mfxExtCodingOption2* CO2 = (mfxExtCodingOption2*)buffers[n_frame][0];
        memset(CO2, 0, sizeof(mfxExtCodingOption2));
        CO2->Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
        CO2->Header.BufferSz = sizeof(mfxExtCodingOption2);
        CO2->DisableDeblockingIdc = idc;

        buffers[n_frame][1] = (mfxExtBuffer*) new mfxExtCodingOption3;
        mfxExtCodingOption3* CO3 = (mfxExtCodingOption3*)buffers[n_frame][1];
        memset(CO3, 0, sizeof(mfxExtCodingOption3));
        CO3->Header.BufferId = MFX_EXTBUFF_CODING_OPTION3;
        CO3->Header.BufferSz = sizeof(mfxExtCodingOption3);
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        CO3->DeblockingAlphaTcOffset = alpha;
        CO3->DeblockingBetaOffset = beta;
#endif

        m_ctrl.ExtParam = 0;
        m_ctrl.NumExtParam = 0;

        if (mode & EVERY_OTHER || mode & RUNTIME_ONLY)
        {
            m_ctrl.ExtParam = buffers[n_frame];
            m_ctrl.NumExtParam = 2;
            s.Data.TimeStamp = FEATURE_ENABLED;
        }
        else if (m_par.NumExtParam)
        {
            mfxExtCodingOption2* co2 = (mfxExtCodingOption2*)m_par.ExtParam[0];
            s.Data.TimeStamp = co2->DisableDeblockingIdc ? FEATURE_DEBLOCKING_DISABLED : 0;
        }
        else
        {
            s.Data.TimeStamp = 0;
        }

        n_frame++;

        return MFX_ERR_NONE;
    }
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    /*00*/{MFX_ERR_NONE, MFX_ERR_NONE, 0, {}},
    /*01*/{MFX_ERR_NONE, MFX_ERR_NONE, QUERY, {}},
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    /*02*/{ MFX_ERR_NONE, MFX_ERR_NONE, QUERY, {{ EXT_COD2, &tsStruct::mfxExtCodingOption2.DisableDeblockingIdc, 0 },
                                  { EXT_COD3, &tsStruct::mfxExtCodingOption3.DeblockingAlphaTcOffset, 4},
                                  { EXT_COD3, &tsStruct::mfxExtCodingOption3.DeblockingBetaOffset, 10}} },
    /*03*/{ MFX_ERR_NONE, MFX_ERR_NONE, QUERY, {{ EXT_COD2, &tsStruct::mfxExtCodingOption2.DisableDeblockingIdc, 1 },
                                  { EXT_COD3, &tsStruct::mfxExtCodingOption3.DeblockingAlphaTcOffset, 4},
                                  { EXT_COD3, &tsStruct::mfxExtCodingOption3.DeblockingBetaOffset, 10}} },
    /*04*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {{ EXT_COD2, &tsStruct::mfxExtCodingOption2.DisableDeblockingIdc, 0 },
                                                      { EXT_COD3, &tsStruct::mfxExtCodingOption3.DeblockingAlphaTcOffset, 4},
                                                      { EXT_COD3, &tsStruct::mfxExtCodingOption3.DeblockingBetaOffset, 13}} },
    /*05*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {{ EXT_COD2, &tsStruct::mfxExtCodingOption2.DisableDeblockingIdc, 0 },
                                                      { EXT_COD3, &tsStruct::mfxExtCodingOption3.DeblockingAlphaTcOffset, 13},
                                                      { EXT_COD3, &tsStruct::mfxExtCodingOption3.DeblockingBetaOffset, 10}} },
    /*06*/{MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, INVALID_PARAMS|RUNTIME_ONLY, {}},
#endif
    /*07*/{MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {EXT_COD2, &tsStruct::mfxExtCodingOption2.DisableDeblockingIdc, 3}},
    /*08*/{MFX_ERR_NONE, MFX_ERR_NONE, RUNTIME_ONLY, {}},
    /*09*/{MFX_ERR_NONE, MFX_ERR_NONE, EVERY_OTHER, {}},
    /*10*/{MFX_ERR_NONE, MFX_ERR_NONE, RUNTIME_ONLY|EVERY_OTHER, {}},
    /*11*/{MFX_ERR_NONE, MFX_ERR_NONE, EVERY_OTHER, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
                                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1}}},
    /*12*/{MFX_ERR_NONE, MFX_ERR_NONE, 0, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
                             {MFX_PAR, &tsStruct::mfxVideoParam  .mfx.GopRefDist, 10}}},
    /*13*/{MFX_ERR_NONE, MFX_ERR_NONE, EVERY_OTHER, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
                                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8}}},
    /*14*/{MFX_ERR_NONE, MFX_ERR_NONE, EVERY_OTHER, {
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 700},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 0},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR}}},
    /*15*/{MFX_ERR_NONE, MFX_ERR_NONE, RESET_ON, {}},
    /*16*/{MFX_ERR_NONE, MFX_ERR_NONE, RESET_ON|RUNTIME_ONLY, {}},
    /*17*/{MFX_ERR_NONE, MFX_ERR_NONE, RESET_ON|EVERY_OTHER, {}},
    /*18*/{MFX_ERR_NONE, MFX_ERR_NONE, RESET_OFF, {}},
    /*19*/{MFX_ERR_NONE, MFX_ERR_NONE, RESET_OFF|RUNTIME_ONLY, {}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

class BsDump : public tsBitstreamProcessor, tsParserH264AU
{
    mfxU32 n_frame;

    Bs32s m_alpha;
    Bs32s m_beta;
public:
    Bs32u m_expected;

    BsDump() :
        tsParserH264AU()
        , n_frame(0)
        , m_alpha(0)
        , m_beta(0)
        , m_expected(1)
    {
        set_trace_level(BS_H264_TRACE_LEVEL_SLICE);
    }
    ~BsDump() {}

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);

        while (nFrames--)
        {
            UnitType& au = ParseOrDie();
            for (Bs32u i = 0; i < au.NumUnits; i++)
            {
                if (!(au.NALU[i].nal_unit_type == 0x01 || au.NALU[i].nal_unit_type == 0x05))
                {
                    continue;
                }

                Bs32u expected_idc = bs.TimeStamp == FEATURE_DEBLOCKING_DISABLED ? m_expected : 0;
                Bs32s expected_alpha = m_alpha;
                Bs32s expected_beta = m_beta;
                if (bs.TimeStamp == FEATURE_ENABLED)
                {
                    expected_idc = n_frame % 2 ? 0 : 1;
                    expected_alpha = expected_idc == 1 ? 0 : (n_frame % 7 - 6) * 2;
                    expected_beta = expected_idc == 1 ? 0 : (n_frame % 7 - 6) * 2;
                }

                Bs32u idc = au.NALU[i].slice_hdr->disable_deblocking_filter_idc;
                Bs32s alpha = au.NALU[i].slice_hdr->slice_alpha_c0_offset_div2 * 2;
                Bs32s beta = au.NALU[i].slice_hdr->slice_beta_offset_div2 * 2;

                g_tsLog << "disable_deblocking_filter_idc = " << idc << " (expected = " << expected_idc << ")\n";
                g_tsLog << "slice_alpha_c0_offset_div2 = " << alpha << " (expected = " << expected_alpha << ")\n";
                g_tsLog << "slice_beta_offset_div2 = " << beta << " (expected = " << expected_beta << ")\n";

                if (idc != expected_idc)
                {
                    g_tsLog << "ERROR: frame#" << n_frame << " slice_hdr->disable_deblocking_filter_idc="
                            << idc << " != " << expected_idc << " (expected value)\n";
                    return MFX_ERR_UNKNOWN;
                }
                if (alpha != expected_alpha && expected_idc != 1)
                {
                    g_tsLog << "ERROR: frame#" << n_frame << " slice_hdr->slice_alpha_c0_offset_div2="
                            << alpha << " != " << expected_alpha << " (expected value)\n";
                    return MFX_ERR_UNKNOWN;
                }
                if (beta != expected_beta && expected_idc != 1)
                {
                    g_tsLog << "ERROR: frame#" << n_frame << " slice_hdr->slice_beta_offset_div2="
                            << beta << " != " << expected_beta << " (expected value)\n";
                    return MFX_ERR_UNKNOWN;
                }
            }
            g_tsLog << "\n";
            n_frame++;
        }

        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }

    void setDeblockingParameters(const Bs32u &idc, const Bs32s &alpha, const Bs32s &beta, bool isNeedCheck = true)
    {
        m_expected = idc;
        if (m_expected && isNeedCheck)
        {
            m_alpha = 0;
            m_beta = 0;
            return;
        }
        m_alpha = alpha;
        m_beta = beta;
    }
};

int TestSuite::RunTest(unsigned int id)
{
    int err = 0;
    TS_START;
    const tc_struct& tc = test_case[id];
    mode = tc.mode;

    MFXInit();

    BsDump bs;
    m_bs_processor = &bs;
    m_par.AsyncDepth = 1;

    if (!(tc.mode & RUNTIME_ONLY) && !(tc.mode & RESET_ON))
    {
        mfxExtCodingOption2& cod2 = m_par;
        mfxExtCodingOption3& cod3 = m_par;

        cod2.DisableDeblockingIdc = 1;
        SETPARS(&cod2, EXT_COD2);

#if (MFX_VERSION >= MFX_VERSION_NEXT)
        cod3.DeblockingAlphaTcOffset = MAX_ALPHA;
        cod3.DeblockingBetaOffset = MAX_BETA;
        SETPARS(&cod3, EXT_COD3);

        bs.setDeblockingParameters(cod2.DisableDeblockingIdc, cod3.DeblockingAlphaTcOffset,
                                   cod3.DeblockingBetaOffset);
#else
        bs.m_expected = cod2.DisableDeblockingIdc;
#endif
    }

    SETPARS(m_pPar, MFX_PAR);

    g_tsStatus.expect(tc.i_sts);

    if ((m_par.mfx.LowPower == MFX_CODINGOPTION_ON) && (m_par.mfx.GopRefDist > 1))
    {
        g_tsLog << "WARNING: CASE WAS SKIPPED\n";
    }
    else
    {

        if (tc.mode & QUERY)
        {
            Query();
        }

        if (mode & INVALID_PARAMS)
        {
            m_max = 1;
            m_cur = 0;
            Init();

            AllocSurfaces();
            AllocBitstream();
            m_pSurf = GetSurface();

            g_tsStatus.expect(tc.e_sts);
            g_tsStatus.disable_next_check();
            g_tsStatus.check(EncodeFrameAsync());
        }

        if (tc.i_sts == MFX_ERR_NONE && !(mode & INVALID_PARAMS))
        {
            if (tc.mode & RESET_ON)
            {
                mode = 0;
                m_par.ExtParam = 0;
                m_par.NumExtParam = 0;
            }
            m_max = 2;
            m_cur = 0;
            EncodeFrames(2);

            if (tc.mode & RESET_ON)
            {
                if (!(tc.mode & RUNTIME_ONLY))
                {
                    mfxExtCodingOption2& cod2 = m_par;
                    mfxExtCodingOption3& cod3 = m_par;

                    cod2.DisableDeblockingIdc = 1;
                    SETPARS(&cod2, EXT_COD2);
#if (MFX_VERSION >= MFX_VERSION_NEXT)
                    cod3.DeblockingAlphaTcOffset = MAX_ALPHA;
                    cod3.DeblockingBetaOffset = MAX_BETA;
                    SETPARS(&cod3, EXT_COD3);

                    bs.setDeblockingParameters(cod2.DisableDeblockingIdc, cod3.DeblockingAlphaTcOffset,
                                               cod3.DeblockingBetaOffset);
#else
                    bs.m_expected = cod2.DisableDeblockingIdc;
#endif
                }
                Reset();
                m_max = 2;
                m_cur = 0;
                EncodeFrames(2);
            }
            else if (tc.mode & RESET_OFF)
            {
                mode = 0;
                if (!(tc.mode & RUNTIME_ONLY))
                {
                    mfxExtCodingOption2& cod2 = m_par;
                    mfxExtCodingOption3& cod3 = m_par;

                    cod2.DisableDeblockingIdc = 0;
#if (MFX_VERSION >= MFX_VERSION_NEXT)
                    cod3.DeblockingAlphaTcOffset = MAX_ALPHA;
                    cod3.DeblockingBetaOffset = MAX_BETA;

                    bs.setDeblockingParameters(1, cod3.DeblockingAlphaTcOffset,
                                               cod3.DeblockingBetaOffset, false);
#else
                    bs.m_expected = 1;
#endif
                }
                Reset();
                m_max = 4;
                m_cur = 0;
                EncodeFrames(4);
            }
        }
    }
    TS_END;
    return err;
}

TS_REG_TEST_SUITE_CLASS(avce_co2_disable_deblocking);
};
