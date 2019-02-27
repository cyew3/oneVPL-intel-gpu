/* ****************************************************************************** *\
INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2019 Intel Corporation. All Rights Reserved.
\* ****************************************************************************** */

/*
Per-frame deblocking_disabled flag "deblocking_filter_disabled_flag" in pps or slice header
mfxExtCodingOption2::DisableDeblockingIdc = 0..1
mfxExtCodingOption3::DeblockingAlphaTcOffset = -12..12
mfxExtCodingOption3::DeblockingBetaOffset = -12..12
*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace hevce_co2_disable_deblocking
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
    TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC)
        , mode(0)
        , n_frame(0)
    {
        m_filler = this;
        memset(buffers, 0, sizeof(mfxExtBuffer*)*200);
    }
    ~TestSuite()
    {
        for (mfxU32 i = 0; i < 100; i++) {
            delete[] buffers[i][0];
            delete[] buffers[i][1];
        }
    }

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

    template<mfxU32 fourcc>
    int RunTest_Subtype(const unsigned int id);

    int RunTest(tc_struct tc, unsigned int fourcc_id);
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

        HUGE_SIZE_4K = 1 << 5,
        HUGE_SIZE_8K = 1 << 6,

        INVALID_PARAMS = 1 << 7
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

        alpha = idc == 1 ? 0 : (n_frame % 12 - 6) * 2;
        beta = idc == 1 ? 0 : (n_frame % 12 - 6) * 2;

        if (mode & INVALID_PARAMS)
        {
            idc = 0;
            alpha = INVALID_VALUE_ALPHA;
            beta = INVALID_VALUE_BETA;
        }

        buffers[n_frame][0] = (mfxExtBuffer*) new mfxExtCodingOption2;
        mfxExtCodingOption2* CO2 = (mfxExtCodingOption2*) buffers[n_frame][0];
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

        if (m_par.NumExtParam && CO2->DisableDeblockingIdc)
            CO2->DisableDeblockingIdc = !((mfxExtCodingOption2&)m_par).DisableDeblockingIdc;

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
    /*06*/{MFX_ERR_NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INVALID_PARAMS | RUNTIME_ONLY, {}},
#endif
    /*07*/{MFX_ERR_NONE, MFX_ERR_NONE, QUERY, {EXT_COD2, &tsStruct::mfxExtCodingOption2.DisableDeblockingIdc, 2}},
    /*08*/{MFX_ERR_NONE, MFX_ERR_NONE, RUNTIME_ONLY, {}},
    /*09*/{MFX_ERR_NONE, MFX_ERR_NONE, EVERY_OTHER, {}},
    /*10*/{MFX_ERR_NONE, MFX_ERR_NONE, RUNTIME_ONLY|EVERY_OTHER, {}},
    /*11*/{MFX_ERR_NONE, MFX_ERR_NONE, EVERY_OTHER, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
                                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1}}},
    /*12*/{MFX_ERR_NONE, MFX_ERR_NONE, 0, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 10}}},
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
    /*20*/{ MFX_ERR_NONE, MFX_ERR_NONE, QUERY | HUGE_SIZE_4K,{
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  4096 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2160 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,  4096 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,  2160 } } },
    /*21*/{ MFX_ERR_NONE, MFX_ERR_NONE, QUERY | HUGE_SIZE_8K,{
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  8192 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4096 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,  8192 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,  4096 } } }
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

class BsDump : public tsBitstreamProcessor, tsParserHEVCAU
{
    mfxU32 n_frame;

    Bs8s m_alpha;
    Bs8s m_beta;
public:
    Bs8u m_expected;

    BsDump() :
        tsParserHEVCAU()
        , n_frame(0)
        , m_alpha(0)
        , m_beta(0)
        , m_expected(1)
    {
        set_trace_level(BS_HEVC::TRACE_LEVEL_SLICE);
    }
    ~BsDump() {}

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        //g_tsLog << "CHECK: slice_hdr->disable_deblocking_filter_idc\n";
        set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);

        while (nFrames--)
        {
            UnitType& au = ParseOrDie();
            for (Bs32u i = 0; i < au.NumUnits; i++)
            {
                Bs16u type = au.nalu[i]->nal_unit_type;
                //check slice segment only
                if (type > 21 || ((type > 9) && (type < 16)))
                {
                    continue;
                }

                Bs8u expected_idc = bs.TimeStamp == FEATURE_DEBLOCKING_DISABLED ? m_expected : 0;
                Bs8s expected_alpha = m_alpha;
                Bs8s expected_beta = m_beta;
                if (bs.TimeStamp == FEATURE_ENABLED)
                {
                    expected_idc = n_frame % 2 ? 0 : 1;
                    expected_alpha = expected_idc == 1 ? 0 : (n_frame % 12 - 6) * 2;
                    expected_beta = expected_idc == 1 ? 0 : (n_frame % 12 - 6) * 2;
                }
                auto& slice = *au.nalu[i]->slice;

                Bs8u idc = 0; //deblocking filter is enabled by default.
                Bs8s alpha = 0;
                Bs8s beta = 0;

                if (slice.deblocking_filter_override_flag)
                {
                    idc = slice.deblocking_filter_disabled_flag;
                    alpha = slice.tc_offset_div2;
                    beta = slice.beta_offset_div2;
                }
                else if (slice.pps->deblocking_filter_control_present_flag)
                {
                    idc = slice.pps->deblocking_filter_disabled_flag;
                    alpha = slice.pps->tc_offset_div2;
                    beta = slice.pps->beta_offset_div2;
                }
                alpha *= 2;
                beta *= 2;

                if (idc != expected_idc)
                {
                    g_tsLog << "ERROR: deblocking_filter_disabled flag in encoded stream is not as expected.\n"
                        << "frame#" << n_frame << ": deblocking_filter_disabled_flag = " << (mfxU32)idc
                        << " != " << (mfxU32)expected_idc << " (expected value)\n";
                    return MFX_ERR_UNKNOWN;
                }
                if (alpha != expected_alpha && expected_idc != 1)
                {
                    g_tsLog << "ERROR: tc_offset_div2 in encoded stream is not as expected.\n"
                            << "frame#" << n_frame << ": tc_offset_div2 = " << (mfxI32)alpha
                            << " != " << (mfxI32)expected_alpha << " (expected value)\n";
                    return MFX_ERR_UNKNOWN;
                }
                if (beta != expected_beta && expected_idc != 1)
                {
                    g_tsLog << "ERROR: beta_offset_div2 in encoded stream is not as expected.\n"
                            << "frame#" << n_frame << ": beta_offset_div2 = " << (mfxI32)beta
                            << " != " << (mfxI32)expected_beta << " (expected value)\n";
                    return MFX_ERR_UNKNOWN;
                }
            }
            g_tsLog << "\n";
            n_frame++;
        }

        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }

    void setDeblockingParameters(const Bs8u &idc, const Bs8s &alpha, const Bs8s &beta, bool isNeedCheck = true)
    {
        m_expected = !!idc;
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

template<mfxU32 fourcc>
int TestSuite::RunTest_Subtype(const unsigned int id)
{
    const tc_struct& tc = test_case[id];
    return RunTest(tc, fourcc);
}

int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id)
{
    int err = 0;
    TS_START;

    if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))
    {
        if (g_tsHWtype < MFX_HW_SKL) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
        {
            g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            g_tsLog << "WARNING: Unsupported HW Platform!\n";
            Query();
            return 0;
        }
        if ((fourcc_id == MFX_FOURCC_P010)
            && (g_tsHWtype < MFX_HW_KBL)) {
            g_tsLog << "\n\nWARNING: P010 format only supported on KBL+!\n\n\n";
            throw tsSKIP;
        }
        if ((fourcc_id == MFX_FOURCC_Y210 || fourcc_id == MFX_FOURCC_YUY2 ||
            fourcc_id == MFX_FOURCC_AYUV || fourcc_id == MFX_FOURCC_Y410)
            && (g_tsHWtype < MFX_HW_ICL))
        {
            g_tsLog << "\n\nWARNING: RExt formats are only supported starting from ICL!\n\n\n";
            throw tsSKIP;
        }
        if ((fourcc_id == GMOCK_FOURCC_P012 || fourcc_id == GMOCK_FOURCC_Y212 || fourcc_id == GMOCK_FOURCC_Y412)
            && (g_tsHWtype < MFX_HW_TGL))
        {
            g_tsLog << "\n\nWARNING: 12b formats are only supported starting from TGL!\n\n\n";
            throw tsSKIP;
        }
        if ((fourcc_id == MFX_FOURCC_YUY2 || fourcc_id == MFX_FOURCC_Y210 || fourcc_id == GMOCK_FOURCC_Y212)
            && (g_tsConfig.lowpower == MFX_CODINGOPTION_ON))
        {
            g_tsLog << "\n\nWARNING: 4:2:2 formats are NOT supported on VDENC!\n\n\n";
            throw tsSKIP;
        }
        if ((g_tsHWtype < MFX_HW_CNL)
            && (g_tsConfig.lowpower == MFX_CODINGOPTION_ON))
        {
            g_tsLog << "\n\nWARNING: Platform less CNL are NOT supported on VDENC!\n\n\n";
            throw tsSKIP;
        }
        if ((fourcc_id == MFX_FOURCC_AYUV || fourcc_id == MFX_FOURCC_Y410 || fourcc_id == GMOCK_FOURCC_Y412)
            && (g_tsConfig.lowpower != MFX_CODINGOPTION_ON))
        {
            g_tsLog << "\n\nWARNING: 4:4:4 formats are only supported on VDENC!\n\n\n";
            throw tsSKIP;
        }
    }

    mode = tc.mode;

    MFXInit();

    BsDump bs;
    m_bs_processor = &bs;

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
        bs.m_expected = !!cod2.DisableDeblockingIdc;
#endif
    }

    SETPARS(m_pPar, MFX_PAR);

    if (fourcc_id == MFX_FOURCC_NV12)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.Shift = 0;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
    }
    else if (fourcc_id == MFX_FOURCC_P010)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.Shift = 1;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
    }
    else if (fourcc_id == GMOCK_FOURCC_P012)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P016;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.Shift = 1;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
    }
    else if (fourcc_id == MFX_FOURCC_YUY2)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        m_par.mfx.FrameInfo.Shift = 0;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
    }
    else if (fourcc_id == MFX_FOURCC_Y210)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        m_par.mfx.FrameInfo.Shift = 1;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
    }
    else if (fourcc_id == GMOCK_FOURCC_Y212)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y216;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        m_par.mfx.FrameInfo.Shift = 1;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
    }
    else if (fourcc_id == MFX_FOURCC_AYUV)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        m_par.mfx.FrameInfo.Shift = 0;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
    }
    else if (fourcc_id == MFX_FOURCC_Y410)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        m_par.mfx.FrameInfo.Shift = 0;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
    }
    else if (fourcc_id == GMOCK_FOURCC_Y412)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y416;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        m_par.mfx.FrameInfo.Shift = 1;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
    }
    else
    {
        g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
        return 0;
    }

    //load the plugin in advance.
    if(!m_loaded)
    {
        Load();
    }

    if (tc.mode & HUGE_SIZE_8K && (g_tsHWtype <= MFX_HW_CNL && m_par.mfx.LowPower != MFX_CODINGOPTION_ON) )
        g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
    else
        g_tsStatus.expect(tc.i_sts);

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
        if ( !(tc.mode & (HUGE_SIZE_4K | HUGE_SIZE_8K) && (g_tsConfig.sim)) )
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
                bs.m_expected = !!cod2.DisableDeblockingIdc;
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

    TS_END;
    return err;
}

TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_co2_disable_deblocking, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_co2_disable_deblocking, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_420_p016_co2_disable_deblocking, RunTest_Subtype<GMOCK_FOURCC_P012>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_co2_disable_deblocking, RunTest_Subtype<MFX_FOURCC_YUY2>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_co2_disable_deblocking, RunTest_Subtype<MFX_FOURCC_Y210>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_422_y216_co2_disable_deblocking, RunTest_Subtype<GMOCK_FOURCC_Y212>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_co2_disable_deblocking, RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_co2_disable_deblocking, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_444_y416_co2_disable_deblocking, RunTest_Subtype<GMOCK_FOURCC_Y412>, n_cases);
};
