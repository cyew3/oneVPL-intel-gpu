/* ****************************************************************************** *\
INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2017 Intel Corporation. All Rights Reserved.
\* ****************************************************************************** */

/*
Per-frame deblocking_disabled flag "deblocking_filter_disabled_flag" in pps or slice header
mfxExtCodingOption2::DisableDeblockingIdc = 0..1
*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace hevce_co2_disable_deblocking
{

enum
{
    FEATURE_ENABLED = 42
};

class TestSuite : tsVideoEncoder, tsSurfaceProcessor
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC)
        , mode(0)
        , n_frame(0)
    {
        m_filler = this;
        memset(buffers, 0, sizeof(mfxExtBuffer*)*100);
    }
    ~TestSuite()
    {
        for (mfxU32 i = 0; i < 100; i++)
            delete [] buffers[i];
    }

    struct tc_struct
    {
        mfxStatus sts;
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
    mfxExtBuffer* buffers[100];
    struct tc_par;

    enum
    {
        MFX_PAR = 1,
        EXT_COD2,
    };

    enum
    {
        QUERY = 1,

        RUNTIME_ONLY = 1 << 1,
        EVERY_OTHER  = 1 << 2,

        RESET_ON  = 1 << 3,
        RESET_OFF = 1 << 4
    };

    static const tc_struct test_case[];

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        assert(n_frame < 100);
        buffers[n_frame] = (mfxExtBuffer*) new mfxExtCodingOption2;
        mfxExtCodingOption2* buf_a = (mfxExtCodingOption2*) buffers[n_frame];
        memset(buf_a, 0, sizeof(mfxExtCodingOption2));
        buf_a->Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
        buf_a->Header.BufferSz = sizeof(mfxExtCodingOption2);

        if (mode & EVERY_OTHER) buf_a->DisableDeblockingIdc = n_frame % 2 ? 0 : 1;
        else                    buf_a->DisableDeblockingIdc = 1;

        if (m_par.NumExtParam && buf_a->DisableDeblockingIdc)
            buf_a->DisableDeblockingIdc = !((mfxExtCodingOption2&)m_par).DisableDeblockingIdc;

        m_ctrl.ExtParam = 0;
        m_ctrl.NumExtParam = 0;

        if (mode & EVERY_OTHER || mode & RUNTIME_ONLY)
        {
            m_ctrl.ExtParam = &buffers[n_frame];
            m_ctrl.NumExtParam = 1;
            s.Data.TimeStamp = buf_a->DisableDeblockingIdc ? FEATURE_ENABLED : 0;
        }
        else if (m_par.NumExtParam)
        {
            mfxExtCodingOption2* co2 = (mfxExtCodingOption2*)m_par.ExtParam[0];
            s.Data.TimeStamp = co2->DisableDeblockingIdc ? FEATURE_ENABLED : 0;
        }

        n_frame++;

        return MFX_ERR_NONE;
    }
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    /*00*/{MFX_ERR_NONE, 0, {}},
    /*01*/{MFX_ERR_NONE, QUERY, {}},
    /*02*/{MFX_ERR_NONE, QUERY, {EXT_COD2, &tsStruct::mfxExtCodingOption2.DisableDeblockingIdc, 1 } },
    /*03*/{MFX_ERR_NONE, QUERY, {EXT_COD2, &tsStruct::mfxExtCodingOption2.DisableDeblockingIdc, 2}},
    /*04*/{MFX_ERR_NONE, RUNTIME_ONLY, {}},
    /*05*/{MFX_ERR_NONE, EVERY_OTHER, {}},
    /*06*/{MFX_ERR_NONE, RUNTIME_ONLY|EVERY_OTHER, {}},
    /*07*/{MFX_ERR_NONE, EVERY_OTHER, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
                                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1}}},
    /*08*/{MFX_ERR_NONE, 0, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 10}}},
    /*09*/{MFX_ERR_NONE, EVERY_OTHER, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
                                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8}}},
    /*10*/{MFX_ERR_NONE, EVERY_OTHER, {
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 700},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 0},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR}}},
    /*11*/{MFX_ERR_NONE, RESET_ON, {}},
    /*12*/{MFX_ERR_NONE, RESET_ON|RUNTIME_ONLY, {}},
    /*13*/{MFX_ERR_NONE, RESET_ON|EVERY_OTHER, {}},
    /*14*/{MFX_ERR_NONE, RESET_OFF, {}},
    /*15*/{MFX_ERR_NONE, RESET_OFF|RUNTIME_ONLY, {}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

class BsDump : public tsBitstreamProcessor, tsParserHEVCAU
{
    mfxU32 n_frame;
public:
    Bs8u m_expected;

    BsDump() :
        tsParserHEVCAU()
        , n_frame(0)
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

                Bs8u expected = bs.TimeStamp == FEATURE_ENABLED ? m_expected : 0;
                Bs8u real = 0; //deblocking filter is enabled by default.
                auto& s = *au.nalu[i]->slice;

                if (s.deblocking_filter_override_flag)
                    real = s.deblocking_filter_disabled_flag;
                else if (s.pps->deblocking_filter_control_present_flag)
                    real = s.pps->deblocking_filter_disabled_flag;

                if (real != expected)
                {
                    g_tsLog << "ERROR: deblocking_filter_disabled flag in encoded stream is not as expected.\n"
                        << "frame#" << n_frame << ": deblocking_filter_disabled_flag = " << (mfxU32)real
                        << " != " << (mfxU32)expected << " (expected value)\n";
                    return MFX_ERR_UNKNOWN;
                }
            }
            n_frame++;
        }

        bs.DataLength = 0;

        return MFX_ERR_NONE;
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

    mode = tc.mode;

    MFXInit();

    BsDump bs;
    m_bs_processor = &bs;

    if (!(tc.mode & RUNTIME_ONLY) && !(tc.mode & RESET_ON))
    {
        mfxExtCodingOption2& cod2 = m_par;
        cod2.DisableDeblockingIdc = 1;
        SETPARS(&cod2, EXT_COD2);
        bs.m_expected = !!cod2.DisableDeblockingIdc;
    }

    SETPARS(m_pPar, MFX_PAR);

    if (fourcc_id == MFX_FOURCC_NV12)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
    }
    else if (fourcc_id == MFX_FOURCC_P010)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.Shift = 1;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
    }
    else if (fourcc_id == MFX_FOURCC_AYUV)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
    }
    else if (fourcc_id == MFX_FOURCC_Y410)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
    }
    else if (fourcc_id == MFX_FOURCC_YUY2)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
    }
    else if (fourcc_id == MFX_FOURCC_Y210)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
    }
    else
    {
        g_tsLog << "WARNING: invalid fourcc_id parameter: " << fourcc_id << "\n";
        return 0;
    }

    //load the plugin in advance.
    if(!m_loaded)
    {
        Load();
    }

    if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data))) {
        if (g_tsHWtype < MFX_HW_SKL)
        {
            g_tsLog << "WARNING: Unsupported HW Platform!\n";
            return 0;
        }
        else if (m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_P010 && g_tsHWtype < MFX_HW_KBL) {
            g_tsLog << "\n\nWARNING: P010 format only supported on KBL+!\n\n\n";
            throw tsSKIP;
        }
        else if ((m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_Y210 || m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_YUY2)
            && (g_tsHWtype < MFX_HW_ICL || g_tsConfig.lowpower == MFX_CODINGOPTION_ON))
        {
            g_tsLog << "\n\nWARNING: 422 formats only supported on ICL+ and ENC+PAK!\n\n\n";
            throw tsSKIP;
        }
        else if ((m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_AYUV || m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_Y410)
            && (g_tsHWtype < MFX_HW_ICL || g_tsConfig.lowpower != MFX_CODINGOPTION_ON))
        {
            g_tsLog << "\n\nWARNING: 444 formats only supported on ICL+ and VDENC!\n\n\n";
            throw tsSKIP;
        }
    } else {
        g_tsLog << "WARNING: only HEVCe HW plugin is tested\n";
        return 0;
    }

    g_tsStatus.expect(tc.sts);

    if (tc.mode & QUERY)
    {
        Query();
    }

    if (tc.sts == MFX_ERR_NONE)
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
            mode = tc.mode;
            if (!(tc.mode & RUNTIME_ONLY))
            {
                mfxExtCodingOption2& cod2 = m_par;
                cod2.DisableDeblockingIdc = 1;
                SETPARS(&cod2, EXT_COD2);
                bs.m_expected = !!cod2.DisableDeblockingIdc;
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
                cod2.DisableDeblockingIdc = 0;
                bs.m_expected = 1;
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
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_co2_disable_deblocking, RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_co2_disable_deblocking, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_co2_disable_deblocking, RunTest_Subtype<MFX_FOURCC_YUY2>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_co2_disable_deblocking, RunTest_Subtype<MFX_FOURCC_Y210>, n_cases);
};
