/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

/*
Per-frame Deblocking disabling disable_deblocking_filter_idc in slice_header
mfxExtCodingOption2::DisableDeblockingIdc = 0..2
slice_header:: disable_deblocking_filter_idc = 0..2
*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace avce_co2_disable_deblocking
{

enum
{
    FEATURE_ENABLED = 42
};

class TestSuite : tsVideoEncoder, tsSurfaceProcessor
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC)
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
    int RunTest(unsigned int id);
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
    /*00*/{MFX_ERR_NONE, 0, {}},
    /*01*/{MFX_ERR_NONE, QUERY, {}},
    /*02*/{ MFX_ERR_NONE, QUERY, { EXT_COD2, &tsStruct::mfxExtCodingOption2.DisableDeblockingIdc, 2 } },
    /*03*/{MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {EXT_COD2, &tsStruct::mfxExtCodingOption2.DisableDeblockingIdc, 3}},
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

class BsDump : public tsBitstreamProcessor, tsParserH264AU
{
    mfxU32 n_frame;
public:
    Bs32u m_expected;

    BsDump() :
        tsParserH264AU()
        , n_frame(0)
        , m_expected(1)
    {
        set_trace_level(BS_H264_TRACE_LEVEL_SLICE);
    }
    ~BsDump() {}

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        g_tsLog << "CHECK: slice_hdr->disable_deblocking_filter_idc\n";
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

                Bs32u expected = bs.TimeStamp == FEATURE_ENABLED ? m_expected : 0;
                if (au.NALU[i].slice_hdr->disable_deblocking_filter_idc != expected)
                {
                    g_tsLog << "ERROR: frame#" << n_frame << " slice_hdr->disable_deblocking_filter_idc="
                        << au.NALU[i].slice_hdr->disable_deblocking_filter_idc
                        << " != " << expected << " (expected value)\n";
                    return MFX_ERR_UNKNOWN;
                }
            }
            n_frame++;
        }

        bs.DataLength = 0;

        return MFX_ERR_NONE;
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
        cod2.DisableDeblockingIdc = 1;
        SETPARS(&cod2, EXT_COD2);
        bs.m_expected = cod2.DisableDeblockingIdc;
    }

    SETPARS(m_pPar, MFX_PAR);

    g_tsStatus.expect(tc.sts);

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
                    bs.m_expected = cod2.DisableDeblockingIdc;
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
    }
    TS_END;
    return err;
}

TS_REG_TEST_SUITE_CLASS(avce_co2_disable_deblocking);
};
