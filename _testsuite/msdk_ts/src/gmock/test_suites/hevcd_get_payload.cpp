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

    class TestSuite
        : public tsVideoDecoder
        , public tsSurfaceProcessor
        , public tsParserHEVC
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

        int RunTest(const char* sname);
        mfxFrameSurface1* ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa);

        static const unsigned int n_cases;

    private:

        Bs32s m_prevPOC;
        Bs32s m_anchorPOC;
        SEIStorage m_sei;
        mfxU8 m_buf[1024];
    };

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

    int TestSuite::RunTest(const char* sname)
    {
        TS_START;

        g_tsStreamPool.Reg();

        tsBitstreamReader r(sname, 100000);
        tsParserHEVC::open(sname);

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

    template <unsigned fourcc>
    char const* query_stream(unsigned int, std::integral_constant<unsigned, fourcc>);

    /* 8 bit */
    char const* query_stream(unsigned int id, std::integral_constant<unsigned, MFX_FOURCC_NV12>)
    {
        assert(!(id > 2));

        static char const* streams[] =
        {
            "conformance/hevc/itu/FILLER_A_Sony_1.bit",
            "conformance/hevc/itu/HRD_A_Fujitsu_3.bin",
            "conformance/hevc/cust/TheAvengers_1280x720_24fps_3Mbps.hevc"
        };

        return streams[id];
    }

    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_YUY2>)
    { return "TODO"; }
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_AYUV>)
    { return "TODO"; }

    /* 10 bit */
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_P010>)
    { return "conformance/hevc/10bit/INITQP_B_Main10_Sony_1.bit"; }
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y216>)
    { return "TODO"; }
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y410>)
    { return "TODO"; }

    template <unsigned fourcc>
    struct TestSuiteEx
        : public TestSuite
    {
        static
        int RunTest(unsigned int id)
        {
            const char* sname =
                g_tsStreamPool.Get(query_stream(id, std::integral_constant<unsigned, fourcc>()));
            g_tsStreamPool.Reg();

            TestSuite suite;
            return suite.RunTest(sname);
        }
    };

    TS_REG_TEST_SUITE(hevcd_get_payload,     TestSuiteEx<MFX_FOURCC_NV12>::RunTest, 3);
    //TS_REG_TEST_SUITE(hevcd_422_get_payload, TestSuiteEx<MFX_FOURCC_YUY2>::RunTest, 1);
    //TS_REG_TEST_SUITE(hevcd_444_get_payload, TestSuiteEx<MFX_FOURCC_AYUV>::RunTest, 1);

    TS_REG_TEST_SUITE(hevc10d_get_payload,     TestSuiteEx<MFX_FOURCC_P010>::RunTest, 1);
    //TS_REG_TEST_SUITE(hevc10d_422_get_payload, TestSuiteEx<MFX_FOURCC_Y216>::RunTest, 1);
    //TS_REG_TEST_SUITE(hevc10d_444_get_payload, TestSuiteEx<MFX_FOURCC_Y410>::RunTest, 1);
}
