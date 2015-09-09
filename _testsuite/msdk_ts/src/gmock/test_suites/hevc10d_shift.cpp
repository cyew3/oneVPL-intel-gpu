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

    enum
    {
        CHANGE_SHIFT = 1
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
    };

    enum
    {
        MFXPAR = 0,
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}},
    {/* 1*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}}},

    // Change shift flag
    {/* 2*/ MFX_ERR_INVALID_VIDEO_PARAM, CHANGE_SHIFT, {
        {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}},
    {/* 3*/ MFX_ERR_INVALID_VIDEO_PARAM, CHANGE_SHIFT, {
        {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

class CRC32 : public tsSurfaceProcessor
{
private:
    mfxU16 shift;
    mfxU32 crc32[2];
public:
    CRC32(mfxFrameInfo& finfo)
        : shift(finfo.Shift)
    {
        crc32[0] = 0x87053A0C;
        crc32[1] = 0x0DEF5FD5;
    };
    ~CRC32() {};
    mfxU32 calc_crc32(mfxFrameSurface1& s)
    {
        Ipp32u crc = 0;
        mfxU32 w = s.Info.CropW;
        mfxU32 h = s.Info.CropH;

        Ipp32u nbytes = w*h*2 + w*h/2 + w*h/2;

        Ipp8u* frame = new Ipp8u[nbytes];
        IppiSize sz;
        sz.height = h;
        sz.width = w * 2;
        ippiCopy_8u_C1R(s.Data.Y, s.Data.Pitch, frame, w*2, sz);
        sz.height /= 2;
        ippiCopy_8u_C1R(s.Data.UV, s.Data.Pitch, frame + w*h*2, w*2, sz);

        /*FILE* o = fopen("C:\\0_work\\msdk\\build\\win_Win32\\bin\\test.p010.yuv", "wb");
        fwrite(frame, 1, nbytes, o);
        fclose(o);*/

        IppStatus sts = ippsCRC32_8u(frame, nbytes, &crc);
        if (sts != ippStsNoErr)
        {
            g_tsLog << "ERROR: cannot calculate CRC32 IppStatus=" << sts << "\n";
            return 0;
        }

        delete [] frame;

        return crc;
    }
    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        g_tsLog << "Checking CRC...\n";
        mfxU32 crc = calc_crc32(s);
        if (crc != crc32[shift]) {
            g_tsLog << "ERROR: reference CRC (" << crc32[shift]
                    << ") != test CRC (" << crc << ")\n";
            return MFX_ERR_NOT_FOUND;
        }
        return MFX_ERR_NONE;
    }
};

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const char* sname = g_tsStreamPool.Get("conformance/hevc/10bit/DBLK_A_MAIN10_VIXS_3.bit");
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(sname, 1000000);
    m_bs_processor = &reader;
    auto tc = test_case[id];

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

    if (tc.mode == CHANGE_SHIFT)
    {
        if (m_par.mfx.FrameInfo.Shift == 1)
        {
            m_par.mfx.FrameInfo.Shift = 0;
        }
        else if (m_par.mfx.FrameInfo.Shift == 0)
        {
            m_par.mfx.FrameInfo.Shift = 1;
        }
        else
        {
            g_tsLog << "ERROR: m_par.mfx.FrameInfo.Shift == " << m_par.mfx.FrameInfo.Shift
                    << ". Should be 0 or 1.\n";
            g_tsStatus.check(MFX_ERR_UNSUPPORTED);  // exit here
        }
    }

    g_tsStatus.expect(tc.sts);
    Init();
    if (tc.sts >= 0)
    {
        CRC32 check_crc(m_par.mfx.FrameInfo);
        m_surf_processor = &check_crc;

        DecodeFrames(1);
    }
    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevc10d_shift);
}