/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_parser.h"

namespace hevcd_pts
{

const mfxU32 n_frames = 30;
enum
{
    SMALL = 20,
    NONE = 1000
};

struct tc_struct
{
    const std::string stream;
    mfxU32 frN;
    mfxU32 frD;
    mfxU32 bufSz;
    mfxI64 pts[n_frames];
};

class Reader : public tsBitstreamReader
{
public:
    std::map<mfxU64, mfxU64> pts_by_offset;
    std::map<mfxU64, mfxU64>::iterator m_it;
    mfxU64 m_pos;
    mfxU64 m_pts;
    mfxU64 m_buff_size;

    Reader(const tc_struct& tc)
        : tsBitstreamReader(g_tsStreamPool.Get(tc.stream), tc.bufSz * 1000)
        , m_pos(0)
        , m_pts(tc.pts[0])
        , m_buff_size(tc.bufSz)
    {
        tsParserHEVC p;
        std::map<mfxU64, mfxU64> offset_by_poc;
        std::map<mfxU64, mfxU64>::iterator it;
        mfxU64 inc = 0;
        mfxU64 max = 0;

        p.open(g_tsStreamPool.Get(tc.stream));

        while(auto pH = p.Parse())
        {
            if(pH->pic->PicOrderCntVal == 0)
                inc += max;

            offset_by_poc[pH->pic->PicOrderCntVal + inc] = pH->pic->slice[0]->StartOffset;
            max = TS_MAX(pH->pic->PicOrderCntVal + inc, max);
        }

        for(it = offset_by_poc.begin(), inc = 0; it != offset_by_poc.end(); it++, inc++)
        {
            pts_by_offset[it->second] = (inc < n_frames) ? tc.pts[inc] : -1;
        }
        m_it = pts_by_offset.begin();
    }
    ~Reader() {};

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        if(pts_by_offset.empty())
            return MFX_ERR_MORE_DATA;

        mfxStatus sts = MFX_ERR_NONE;
        std::map<mfxU64, mfxU64>::iterator curr = pts_by_offset.begin();
        m_pos -= bs.DataLength;

        if(!m_eos)
        {
            sts = tsBitstreamReader::ProcessBitstream(bs, nFrames);
        }

        if (m_pos)
        {
            if (m_buff_size == NONE)
            {
                std::map<mfxU64, mfxU64>::iterator it = pts_by_offset.begin();
                while (it->first <= m_pos)
                {
                    it++;
                    if (it == pts_by_offset.end()) break;

                    if (curr->second > it->second)
                    {
                        curr = it;
                    }
                }
                bs.TimeStamp = curr->second;
                pts_by_offset.erase(curr);
            }
            else
            {
                mfxU64 last = curr->first;
                curr++;
                mfxU64 next = curr->first;

                if (!(last < m_pos && next <= (m_pos + bs.DataLength)))
                {
                    curr--;
                    bs.TimeStamp = curr->second;
                }
                else
                {
                    bs.TimeStamp = curr->second;
                    curr--;
                    pts_by_offset.erase(curr);
                }
            }

        }
        m_pos += bs.DataLength;

        return sts;
    }
};

class Verifier : public tsSurfaceProcessor
{
public:
    const tc_struct& m_tc;
    mfxU64 m_prev_pts;

    Verifier(const tc_struct& tc) : m_tc(tc), m_prev_pts(0){}
    ~Verifier() {}

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        if(m_tc.pts[m_cur-1] != -1)
        {
            EXPECT_EQ(m_tc.pts[m_cur-1], (mfxI64)s.Data.TimeStamp);
            EXPECT_EQ(MFX_FRAMEDATA_ORIGINAL_TIMESTAMP, s.Data.DataFlag);
        }
        else
        {
            mfxI64 pts_expected = 0;
            mfxI64 d = 0;

            if(m_cur > 1)
                pts_expected = m_prev_pts + (90000 * m_tc.frD / m_tc.frN);

            d = pts_expected - s.Data.TimeStamp;
            if(d) d = TS_MIN(3, TS_MAX(-3, d));

            EXPECT_EQ(pts_expected - d, (mfxI64)s.Data.TimeStamp);
            EXPECT_EQ(0, s.Data.DataFlag);
        }
        m_prev_pts = s.Data.TimeStamp;

        return MFX_ERR_NONE;
    }
};

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_HEVC){}
    ~TestSuite() { }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;
    static const tc_struct test_case[];
};


const tc_struct TestSuite::test_case[] =
{
    {/* 0*/
        "/conformance/hevc/itu/DELTAQP_A_BRCM_4.bit",
        30000, 1000,
        NONE,
        {     0,  3600,  7200, 10800, 14400, 18000, 21600, 25200,  28800,  32400,
          36000, 39600, 43200, 46800, 50400, 54000, 57600, 61200,  64800,  68400,
          72000, 75600, 79200, 82800, 86400, 90000, 93600, 97200, 100800, 104400 }
    },
    {/* 1*/
        "/conformance/hevc/itu/DELTAQP_A_BRCM_4.bit",
        30000, 1001,
        NONE,
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }
    },
    {/* 2*/
        "/conformance/hevc/itu/DELTAQP_A_BRCM_4.bit",
        60000, 1001,
        NONE,
        {100000, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, 0, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, 1000, -1, -1, -1, }
    },
    {/* 3*/
        "/conformance/hevc/itu/DELTAQP_A_BRCM_4.bit",
        30000, 1000,
        SMALL,
        {     0,  3600,  7200, 10800, 14400, 18000, 21600, 25200,  28800,  32400,
          36000, 39600, 43200, 46800, 50400, 54000, 57600, 61200,  64800,  68400,
          72000, 75600, 79200, 82800, 86400, 90000, 93600, 97200, 100800, 104400 }
    },
    {/* 4*/
        "/conformance/hevc/itu/DELTAQP_A_BRCM_4.bit",
        30000, 1001,
        SMALL,
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, }
    },
    {/* 5*/
        "/conformance/hevc/itu/DELTAQP_A_BRCM_4.bit",
        60000, 1001,
        SMALL,
        {100000, -1, -1, -1, -1, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, 0, -1, -1, -1, -1, -1,
         -1, -1, -1, -1, -1, -1, 1000, -1, -1, -1,}
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    auto& tc = test_case[id];
    g_tsStreamPool.Get(tc.stream);
    g_tsStreamPool.Reg();
    Reader r(tc);
    m_bs_processor = &r;
    Verifier v(tc);
    m_surf_processor = &v;
    m_update_bs = true;

    DecodeHeader();
    m_par.mfx.FrameInfo.FrameRateExtN = tc.frN;
    m_par.mfx.FrameInfo.FrameRateExtD = tc.frD;
    DecodeFrames(n_frames);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevcd_pts);
}