#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace hevce_spspps_get
{

class TestSuite : tsVideoEncoder
{
public:
    static const unsigned int n_cases;
    static const mfxU32 sps_buf_sz = 100;
    static const mfxU32 pps_buf_sz = 50;
    mfxU8 m_sp_buf[sps_buf_sz + pps_buf_sz];

    TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC)
    {
        memset(m_sp_buf, 0, sps_buf_sz + pps_buf_sz);
    }
    ~TestSuite() {}
    int RunTest(unsigned int id);

private:
    struct tc_struct
    {
        mfxStatus sts;
        struct
        {
            const tsStruct::Field* field;
            mfxU32           value;
        }init, reset;
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ MFX_ERR_NULL_PTR, {&tsStruct::mfxExtCodingOptionSPSPPS.SPSBuffer, 0}  },
    {/* 1*/ MFX_ERR_NULL_PTR, {&tsStruct::mfxExtCodingOptionSPSPPS.PPSBuffer, 0}  },
    {/* 2*/ MFX_ERR_NOT_ENOUGH_BUFFER, {&tsStruct::mfxExtCodingOptionSPSPPS.SPSBufSize, 5}  },
    {/* 3*/ MFX_ERR_NOT_ENOUGH_BUFFER, {&tsStruct::mfxExtCodingOptionSPSPPS.PPSBufSize, 5}  },
    {/* 4*/ MFX_ERR_NONE, {}, {&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 704} },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

class Verifier : public tsBitstreamProcessor, public tsParserHEVC
{
public:
    const mfxExtCodingOptionSPSPPS& m_sp;

    Verifier(mfxExtCodingOptionSPSPPS& sp) : m_sp(sp) { set_trace_level(BS_HEVC::TRACE_LEVEL_NALU); }
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        using namespace BS_HEVC;
        NALU *sps = 0, *pps = 0;

        SetBuffer(bs);
        UnitType& au = ParseOrDie();

        for(mfxU32 i = 0; i < au.NumUnits; i ++)
        {
            if(au.nalu[i]->nal_unit_type == SPS_NUT) sps = au.nalu[i];
            if(au.nalu[i]->nal_unit_type == PPS_NUT) pps = au.nalu[i];
        }

        g_tsLog << "SPS expected: " << hexstr(bs.Data + bs.DataOffset + sps->StartOffset, sps->NumBytesInNalUnit) << "\n";
        g_tsLog << "SPS actual  : " << hexstr(m_sp.SPSBuffer, m_sp.SPSBufSize) << "\n";
        g_tsLog << "PPS expected: " << hexstr(bs.Data + bs.DataOffset + pps->StartOffset, pps->NumBytesInNalUnit) << "\n";
        g_tsLog << "PPS actual  : " << hexstr(m_sp.PPSBuffer, m_sp.PPSBufSize) << "\n";

        EXPECT_EQ(sps->NumBytesInNalUnit, m_sp.SPSBufSize);
        EXPECT_EQ(pps->NumBytesInNalUnit, m_sp.PPSBufSize);
        EXPECT_EQ(0, memcmp(m_sp.SPSBuffer, bs.Data + bs.DataOffset + sps->StartOffset, TS_MIN(sps->NumBytesInNalUnit, m_sp.SPSBufSize)));
        EXPECT_EQ(0, memcmp(m_sp.PPSBuffer, bs.Data + bs.DataOffset + pps->StartOffset, TS_MIN(pps->NumBytesInNalUnit, m_sp.PPSBufSize)));

        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }
};

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    //define file reader
    const char* file_name = g_tsStreamPool.Get("YUV/720x480p_30.00_4mb_h264_cabac_180s.yuv");
    g_tsStreamPool.Reg();
    tsRawReader SurfProc = tsRawReader(file_name, m_pPar->mfx.FrameInfo, 1);
    g_tsStreamPool.Reg();
    m_filler = &SurfProc;

    auto& tc = test_case[id];

    if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))
    {
        //HEVCE_HW need aligned width and height for 32
        m_par.mfx.FrameInfo.Width = ((m_par.mfx.FrameInfo.Width + 32 - 1) & ~(32 - 1));
        m_par.mfx.FrameInfo.Height = ((m_par.mfx.FrameInfo.Height + 32 - 1) & ~(32 - 1));
    }
    Init();

    mfxExtCodingOptionSPSPPS& sp = m_par;
    Verifier v(sp);
    m_bs_processor = &v;
    sp.SPSBuffer  = m_sp_buf;
    sp.PPSBuffer  = m_sp_buf + sps_buf_sz;
    sp.SPSBufSize = sps_buf_sz;
    sp.PPSBufSize = pps_buf_sz;

    if(tc.init.field)
    {
        tsStruct::set(&sp, *tc.init.field, tc.init.value);
    }

    g_tsStatus.expect(tc.sts);
    GetVideoParam();

    if (g_tsStatus.get() < 0)
        return 0;

    EncodeFrames(1);

    if(tc.reset.field)
    {
        tsStruct::set(m_pPar, *tc.reset.field, tc.reset.value);
    }

    m_par.NumExtParam = 0;
    Reset();

    sp.SPSBufSize = sps_buf_sz;
    sp.PPSBufSize = pps_buf_sz;
    m_par.RefreshBuffers();

    GetVideoParam();
    EncodeFrames(1);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_spspps_get);
}