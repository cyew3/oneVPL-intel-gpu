/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2018 Intel Corporation. All Rights Reserved.
//
*/

#include "ts_decoder.h"
#include "ts_struct.h"

#include <ipp.h>

namespace hevc10d_shift
{

class TestSuite
    : public tsVideoDecoder
{

public:

    TestSuite() : tsVideoDecoder(MFX_CODEC_HEVC){}
    ~TestSuite() { }

    int RunTest(unsigned int id, char const* sname, unsigned crc0, unsigned crc1);
    static const unsigned int n_cases;

private:

    mfxStatus DecodeFrameAsync();
    mfxStatus DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp);

private:

    enum
    {
        SHIFT_INIT,
        SHIFT_DECODE,
    };

    struct tc_struct
    {
        mfxStatus sts;

        mfxI32    mode;
        mfxU32    shift;

        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    enum
    {
        MFXPAR = 0,
    };

    static const tc_struct test_case[];

    tc_struct tc;
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    { /* 0 */ MFX_ERR_NONE, SHIFT_INIT, 0,
        { { MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY } }
    },
    { /* 1 */ MFX_ERR_NONE, SHIFT_INIT, 1,
        { { MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY } }
    },

    { /* 2 */ MFX_ERR_NONE, SHIFT_DECODE, 0,
        { { MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY } }
    },
    { /* 3 */ MFX_ERR_NONE, SHIFT_DECODE, 1,
        { { MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY } }
    },

    { /* 4 */ MFX_ERR_INVALID_VIDEO_PARAM, SHIFT_INIT, 0,
        { { MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY } }
    },
    { /* 5 */ MFX_ERR_INVALID_VIDEO_PARAM, SHIFT_DECODE, 0,
        { { MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY } }
    },

    { /* 6 */ MFX_ERR_NONE, SHIFT_INIT, 1,
        { { MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY } }
    },
    { /* 7 */ MFX_ERR_NONE, SHIFT_DECODE, 1,
        { { MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY } }
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

class CRC32
    : public tsSurfaceCRC32
{

private:

    mfxU32 reference;

public:

    CRC32(mfxU32 crc)
        : reference(crc)
    {};

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        g_tsLog << "Checking CRC...\n";

        tsSurfaceCRC32::ProcessSurface(s);

        mfxU32 const crc = GetCRC();
        if (crc != reference)
        {
            g_tsLog << "ERROR: reference CRC (" << reference
                    << ") != test CRC (" << crc << ")\n";
            return MFX_ERR_NOT_FOUND;
        }

        return MFX_ERR_NONE;
    }
};

mfxStatus TestSuite::DecodeFrameAsync()
{
    if (m_default)
    {
        SetPar4_DecodeFrameAsync();
    }

    mfxStatus sts = DecodeFrameAsync(m_session, m_pBitstream, m_pSurf, &m_pSurfOut, m_pSyncPoint);
    if (sts == MFX_ERR_NONE || (sts == MFX_WRN_VIDEO_PARAM_CHANGED && *m_pSyncPoint != NULL))
    {
        m_surf_out.insert(std::make_pair(*m_pSyncPoint, m_pSurfOut));
        if (m_pSurfOut)
        {
            msdk_atomic_inc16(&m_pSurfOut->Data.Locked);
        }
    }

    return sts;
}

mfxStatus TestSuite::DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
{
    if (tc.mode == SHIFT_DECODE)
    {
        surface_work->Info.Shift = tc.shift;
        g_tsStatus.expect(tc.sts);
    }

    return
        tsVideoDecoder::DecodeFrameAsync(session, bs, surface_work, surface_out, syncp);
}


int TestSuite::RunTest(unsigned int id, char const* sname, unsigned crc0, unsigned crc1)
{
    TS_START;

    tsBitstreamReader reader(sname, 1000000);
    m_bs_processor = &reader;
    tc = test_case[id];

    m_init_par.Implementation = m_impl;
    m_init_par.Version = m_version;
    m_init_par.GPUCopy = g_tsConfig.GPU_copy_mode;
    MFXInitEx();

    Load();
    SETPARS(m_pPar, MFXPAR);

    if (m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
    {
        if (m_pVAHandle)
        {
            SetAllocator(m_pVAHandle, true);
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();
        }
        else
        {
            mfxHDL hdl;
            mfxHandleType type;
            if (!GetAllocator())
            {
                UseDefaultAllocator(
                    (m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
                    || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
                    );
            }
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();
            m_pVAHandle = m_pFrameAllocator;
            m_pVAHandle->get_hdl(type, hdl);
            SetHandle(m_session, type, hdl);
            m_is_handle_set = (g_tsStatus.get() >= 0);
        }
    }// must be called before DecodeHeader()

    DecodeHeader();
    QueryIOSurf();

    if (tc.mode == SHIFT_INIT)
    {
        m_request.Info.Shift = m_par.mfx.FrameInfo.Shift = tc.shift;
        g_tsStatus.expect(tc.sts);
    }

    Init();

    if (m_initialized)
    {
        CRC32 check_crc(tc.shift ? crc1 : crc0);
        m_surf_processor = &check_crc;

        DecodeFrames(1);
    }

    TS_END;
    return 0;
}

template <unsigned fourcc>
std::tuple<char const*, unsigned, unsigned>
query_param(unsigned int, std::integral_constant<unsigned, fourcc>);

std::tuple<char const*, unsigned, unsigned>
query_param(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_P010>)
{
    unsigned constexpr ZERO_SHIFT_CRC = 0x87053A0C;
    unsigned constexpr ONE_SHIFT_CRC  = 0x0DEF5FD5;

    return std::make_tuple(
        "conformance/hevc/10bit/DBLK_A_MAIN10_VIXS_3.bit",
        ZERO_SHIFT_CRC,
        ONE_SHIFT_CRC
    );
}

std::tuple<char const*, unsigned, unsigned>
query_param(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y210>)
{
    unsigned constexpr ZERO_SHIFT_CRC = 0xE06C857B;
    unsigned constexpr ONE_SHIFT_CRC  = 0xFE97129C;

    return std::make_tuple(
        "conformance/hevc/10bit/GENERAL_10b_422_RExt_Sony_1.bit",
        ZERO_SHIFT_CRC,
        ONE_SHIFT_CRC
    );
}

std::tuple<char const*, unsigned, unsigned>
query_param(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_P016>)
{
    unsigned constexpr ZERO_SHIFT_CRC = 0xABD92322;
    unsigned constexpr ONE_SHIFT_CRC  = 0x94F61388;

    return std::make_tuple(
        "conformance/hevc/12bit/420format/GENERAL_12b_420_RExt_Sony_1.bit",
        ZERO_SHIFT_CRC,
        ONE_SHIFT_CRC
    );
}

std::tuple<char const*, unsigned, unsigned>
query_param(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y216>)
{
    unsigned constexpr ZERO_SHIFT_CRC = 0x613A8120;
    unsigned constexpr ONE_SHIFT_CRC  = 0xB03BB3DE;

    return std::make_tuple(
        "conformance/hevc/12bit/422format/GENERAL_12b_422_RExt_Sony_1.bit",
        ZERO_SHIFT_CRC,
        ONE_SHIFT_CRC
    );
}

std::tuple<char const*, unsigned, unsigned>
query_param(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y416>)
{
    unsigned constexpr ZERO_SHIFT_CRC = 0xDFEAA2A1;
    unsigned constexpr ONE_SHIFT_CRC  = 0x7C429C44;

    return std::make_tuple(
        "conformance/hevc/12bit/444format/GENERAL_12b_444_RExt_Sony_2.bit",
        ZERO_SHIFT_CRC,
        ONE_SHIFT_CRC
    );
}

template <unsigned fourcc, unsigned profile>
std::tuple<char const*, unsigned, unsigned>
query_param(unsigned int id, std::integral_constant<unsigned, fourcc>, std::integral_constant<unsigned, profile>)
{ return query_param(id, std::integral_constant<unsigned, fourcc>{}); }

#if !defined(OPEN_SOURCE)
std::tuple<char const*, unsigned, unsigned>
query_param(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_P010>, std::integral_constant<unsigned, MFX_PROFILE_HEVC_SCC>)
{
    unsigned constexpr ZERO_SHIFT_CRC = 0xADD776A7;
    unsigned constexpr ONE_SHIFT_CRC  = 0x2159B29D;

    return std::make_tuple(
        "conformance/hevc/self_coded/sbe_scc420_10bit_1920x1080.h265",
        ZERO_SHIFT_CRC,
        ONE_SHIFT_CRC
    );
}
#endif

template <unsigned fourcc, unsigned profile = MFX_PROFILE_UNKNOWN>
struct TestSuiteEx
    : public TestSuite
{
    static
    int RunTest(unsigned int id)
    {
        auto const& p =
            query_param(id, std::integral_constant<unsigned, fourcc>{}, std::integral_constant<unsigned, profile>{});

        char const* sname = g_tsStreamPool.Get(std::get<0>(p));
        g_tsStreamPool.Reg();

        TestSuite suite;
        return suite.RunTest(id, sname, std::get<1>(p), std::get<2>(p));
    }
};

TS_REG_TEST_SUITE(hevc10d_shift,            TestSuiteEx<MFX_FOURCC_P010>::RunTest, TestSuiteEx<MFX_FOURCC_P010>::n_cases);
TS_REG_TEST_SUITE(hevcd_10b_422_y210_shift, TestSuiteEx<MFX_FOURCC_Y210>::RunTest, TestSuiteEx<MFX_FOURCC_Y210>::n_cases);

TS_REG_TEST_SUITE(hevcd_12b_420_p016_shift, TestSuiteEx<MFX_FOURCC_P016>::RunTest, TestSuiteEx<MFX_FOURCC_P016>::n_cases);
TS_REG_TEST_SUITE(hevcd_12b_422_y216_shift, TestSuiteEx<MFX_FOURCC_Y216>::RunTest, TestSuiteEx<MFX_FOURCC_Y216>::n_cases);
TS_REG_TEST_SUITE(hevcd_12b_444_y416_shift, TestSuiteEx<MFX_FOURCC_Y416>::RunTest, TestSuiteEx<MFX_FOURCC_Y416>::n_cases);

#if !defined(OPEN_SOURCE)
TS_REG_TEST_SUITE(hevcd_10b_420_p010_scc_shift, (TestSuiteEx<MFX_FOURCC_P010, MFX_PROFILE_HEVC_SCC>::RunTest), (TestSuiteEx<MFX_FOURCC_P010, MFX_PROFILE_HEVC_SCC>::n_cases));
#endif
}
