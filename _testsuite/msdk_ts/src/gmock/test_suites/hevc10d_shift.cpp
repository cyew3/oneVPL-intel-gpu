/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2017 Intel Corporation. All Rights Reserved.
//
*/

#include "ts_decoder.h"
#include "ts_struct.h"

#include <ipp.h>

namespace hevc10d_shift
{

class TestSuite : tsVideoDecoder
{

public:

    TestSuite() : tsVideoDecoder(MFX_CODEC_HEVC){}
    ~TestSuite() { }

    int RunTest(unsigned int id);
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

class CRC32 : public tsSurfaceProcessor
{

private:

    mfxU16 shift;
    mfxU32 crc32[2];

    static int const ZERO_SHIFT_CRC = 0x87053A0C;
    static int const ONE_SHIFT_CRC  = 0x0DEF5FD5;

public:

    CRC32(mfxU16 shift)
        : shift(shift)
    {
        crc32[0] = ZERO_SHIFT_CRC;
        crc32[1] = ONE_SHIFT_CRC;
    };
    ~CRC32() {};

    mfxU32 calc_crc32(mfxFrameSurface1& s)
    {
        mfxU32 w = s.Info.CropW;
        mfxU32 h = s.Info.CropH;

        Ipp32u const nbytes = w*h*2 + w*h/2 + w*h/2;
        if (!nbytes)
        {
            g_tsLog << "ERROR: cannot calculate CRC32, input frame has zero dimensions\n";
            return 0;
        }

        std::vector<Ipp8u> frame(nbytes);
        IppiSize sz = { mfxI32(w * 2), mfxI32(h) };
        ippiCopy_8u_C1R(s.Data.Y, s.Data.Pitch, &frame[0], w*2, sz);

        sz.height /= 2;
        ippiCopy_8u_C1R(s.Data.UV, s.Data.Pitch, &frame[0] + w*h*2, w*2, sz);

        Ipp32u crc = 0;
        IppStatus sts = ippsCRC32_8u(&frame[0], nbytes, &crc);
        if (sts != ippStsNoErr)
        {
            g_tsLog << "ERROR: cannot calculate CRC32 IppStatus=" << sts << "\n";
            return 0;
        }

        return crc;
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        g_tsLog << "Checking CRC...\n";

        mfxU32 const crc = calc_crc32(s);
        if (crc != crc32[shift])
        {
            g_tsLog << "ERROR: reference CRC (" << crc32[shift]
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
            m_pSurfOut->Data.Locked ++;
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

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const char* sname = g_tsStreamPool.Get("conformance/hevc/10bit/DBLK_A_MAIN10_VIXS_3.bit");
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(sname, 1000000);
    m_bs_processor = &reader;
    tc = test_case[id];

    Load();
    SETPARS(m_pPar, MFXPAR);

    if ((!m_pVAHandle) && (m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
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
        CRC32 check_crc(tc.shift);
        m_surf_processor = &check_crc;

        DecodeFrames(1);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevc10d_shift);
}
