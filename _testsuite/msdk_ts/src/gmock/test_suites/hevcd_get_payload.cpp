/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace hevcd_get_payload
{

using namespace BS_HEVC;

class SEIStorage
{
public:
    void Put(mfxU64 ts, const AU& au)
    {
        mfxPayload pl = {};

        while(Get(ts, pl));

        for (mfxU32 i = 0; i < au.NumUnits; i++)
        {
            auto& nalu = *au.nalu[i];
            bool suffix = false;

            if (nalu.nal_unit_type == SUFFIX_SEI_NUT)
                suffix = true;
            else if (nalu.nal_unit_type != PREFIX_SEI_NUT)
                continue;

            for (mfxU32 j = 0; j < nalu.sei->NumMessage; j++)
            {
                auto& msg = nalu.sei->message[j];
                std::vector<mfxU8> data;
                size_t B = msg.payloadType;

                while (B > 255)
                {
                    data.push_back(255);
                    B -= 255;
                }
                data.push_back(mfxU8(B));

                B = msg.payloadSize;

                while (B > 255)
                {
                    data.push_back(255);
                    B -= 255;
                }
                data.push_back(mfxU8(B));

                B = data.size();

                data.resize(B + msg.payloadSize);
                memcpy(&data[B], msg.rawData, msg.payloadSize);

                pl.CtrlFlags  = suffix * MFX_PAYLOAD_CTRL_SUFFIX;
                pl.BufSize    = mfxU16(B + msg.payloadSize);
                pl.Data       = new mfxU8[pl.BufSize];
                pl.NumBit     = pl.BufSize * 8;
                pl.Type       = mfxU16(msg.payloadType);

                memcpy(pl.Data, &data[0], pl.BufSize);

                m_pl[ts].push_back(pl);
            }
        }
    }

    bool Get(mfxU64 ts, mfxPayload& pl)
    {
        Free();

        if (m_pl[ts].empty())
        {
            m_pl.erase(ts);
            return false;
        }

        pl = m_pl[ts].front();
        m_pl[ts].pop_front();
        m_data.push_back(pl.Data);

        return true;
    }

    bool Check(mfxU64 ts) { return !!m_pl.count(ts); }

    ~SEIStorage() { Free(); }

private:
    std::map<mfxU64, std::list<mfxPayload> > m_pl;
    std::vector<mfxU8*> m_data;

    void Free()
    {
        for (auto p : m_data)
            delete[] p;
        m_data.resize(0);
    }
};

class TestSuite : public tsVideoDecoder, public tsSurfaceProcessor, public tsParserHEVC
{
public:
    TestSuite()
        : tsVideoDecoder(MFX_CODEC_HEVC)
        , tsParserHEVC(INIT_MODE_STORE_SEI_DATA)
    {
        m_surf_processor = this;
        set_trace_level(0);
        m_prevPOC = -1;
        m_anchorPOC = 0;
    }
    ~TestSuite() { }

    int RunTest(unsigned int id);
    mfxFrameSurface1* ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa);

    static const unsigned int n_cases;

private:
    Bs32s m_prevPOC;
    Bs32s m_anchorPOC;
    SEIStorage m_sei;
    mfxU8 m_buf[1024];
    struct tc_struct
    {
        std::string stream;
    };
    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
/*00*/ {"conformance/hevc/itu/FILLER_A_Sony_1.bit"},
/*01*/ {"conformance/hevc/itu/HRD_A_Fujitsu_3.bin"},
/*02*/ {"conformance/hevc/cust/TheAvengers_1280x720_24fps_3Mbps.hevc"},
};
const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

mfxFrameSurface1* TestSuite::ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* /*pfa*/)
{
    mfxPayload pl = {};
    mfxPayload ref = {};
    mfxU64 ts = MFX_TIMESTAMP_UNKNOWN;
    std::vector<void*> locked;
    mfxU32 n = 0;

    pl.Data = m_buf;
    pl.BufSize = sizeof(m_buf);

    while (!m_sei.Check(ps->Data.TimeStamp))
    {
        auto& au = ParseOrDie();

        if (au.pic->PicOrderCntVal <= 0)
        {
            m_anchorPOC += m_prevPOC + 1;
            m_prevPOC = au.pic->PicOrderCntVal;
        }

        m_sei.Put(mfxU64(90000) * m_par.mfx.FrameInfo.FrameRateExtD * (m_anchorPOC + au.pic->PicOrderCntVal) / m_par.mfx.FrameInfo.FrameRateExtN, au);

        m_prevPOC = TS_MAX(m_prevPOC, au.pic->PicOrderCntVal);
    }

    while (m_sei.Get(ps->Data.TimeStamp, ref))
    {
        memset(m_buf, 0xff, sizeof(m_buf));

        g_tsLog << "TimeStamp " << ps->Data.TimeStamp << " Payload " << n++ << "\n";
        GetPayload(&ts, &pl);

        EXPECT_EQ(ps->Data.TimeStamp, ts);
        EXPECT_EQ(ref.CtrlFlags, pl.CtrlFlags);
        EXPECT_EQ(ref.Type, pl.Type);
        EXPECT_EQ(ref.NumBit, pl.NumBit);
        EXPECT_EQ(0, memcmp(ref.Data, pl.Data, ref.NumBit / 8))
            << "\n ref.Data = " << hexstr(ref.Data, ref.NumBit / 8)
            << "\n pl.Data  = " << hexstr(pl.Data, pl.NumBit / 8);
    }

    return ps;
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    auto& tc = test_case[id];
    auto stream = g_tsStreamPool.Get(tc.stream);
    g_tsStreamPool.Reg();

    tsBitstreamReader r(stream, 100000);
    tsParserHEVC::open(stream);

    m_bs_processor = &r;

    DecodeHeader();

    if (!m_par.mfx.FrameInfo.FrameRateExtN || !m_par.mfx.FrameInfo.FrameRateExtD )
    {
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;
    }
    m_bitstream.TimeStamp = MFX_TIMESTAMP_UNKNOWN;

    Init();

    DecodeFrames(1000);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevcd_get_payload);
}
