/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include <memory>
#include "ts_decoder.h"
#include "ts_struct.h"

#define TEST_NAME vp9d_decode_header
namespace TEST_NAME
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_VP9) { }
    ~TestSuite() {}

    struct f_pair
    {
        const  tsStruct::Field* f;
        mfxU64 v;
        mfxU8  stage;
    };
    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        mfxU32 n_calls;
        f_pair set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];      //base cases
    static const unsigned int n_cases;

    template<mfxU32 fourcc>
    int RunTest_fourcc(const unsigned int id);

private:

    enum
    {
        NULL_SESSION = 1,
        NULL_BS,
        NULL_PAR,
        NOT_INITIALIZED,
        FAILED_INIT,
        INITED_DECODER,
        CLOSED_DECODER,
        NO_EXT_ALLOCATOR,
    };

    enum
    {
        INIT = 1,
        RUNTIME_BS,
        RUNTIME_PAR
    };

    void AllocOpaque();
    void ReadStream();

    int RunTest(const tc_struct& tc);
    std::unique_ptr<tsBitstreamProcessor> m_bs_reader;
    std::string m_input_stream_name;
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, 0, 1},
    {/*01*/ MFX_ERR_INVALID_HANDLE,  NULL_SESSION},
    {/*02*/ MFX_ERR_NULL_PTR,        NULL_BS     },
    {/*03*/ MFX_ERR_NULL_PTR,        NULL_PAR    },
    {/*04*/ MFX_ERR_NONE, NOT_INITIALIZED},
    {/*05*/ MFX_ERR_NONE, INITED_DECODER },
    {/*06*/ MFX_ERR_NONE, FAILED_INIT, 0, {&tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 777, INIT}},
    {/*07*/ MFX_ERR_NONE, CLOSED_DECODER },

};
const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

struct streamDesc
{
    mfxU16 w;
    mfxU16 h;
    const char *name;
};

const streamDesc streams[] = {
    { 352, 288, "forBehaviorTest/foreman_cif.ivf"                                                       },
    { 432, 240, "conformance/vp9/SBE/8bit_444/Stress_VP9_FC2p1ss444_432x240_250_extra_stress_2.2.0.vp9" },
    { 432, 240, "conformance/vp9/SBE/10bit/Stress_VP9_FC2p2b10_432x240_050_intra_stress_1.5.vp9"        },
    { 432, 240, "conformance/vp9/SBE/10bit_444/Syntax_VP9_FC2p3ss444_432x240_101_inter_basic_2.0.0.vp9" },
    { 160, 90,  "conformance/vp9/google/vp92-2-20-12bit-yuv420.ivf"                                     },
    { 160, 90,  "conformance/vp9/google/vp93-2-20-12bit-yuv444.ivf"                                     },
    {}
};

const streamDesc& getStreamDesc(const mfxU32& fourcc)
{
    switch(fourcc)
    {
        case MFX_FOURCC_NV12: return streams[0];
        case MFX_FOURCC_AYUV: return streams[1];
        case MFX_FOURCC_P010: return streams[2];
        case MFX_FOURCC_Y410: return streams[3];
        case MFX_FOURCC_P016: return streams[4];
        case MFX_FOURCC_Y416: return streams[5];

        default: assert(0); return streams[0];
    }
}

template<mfxU32 fourcc>
int TestSuite::RunTest_fourcc(const unsigned int id)
{
    const streamDesc& bsDesc = getStreamDesc(fourcc);
    m_input_stream_name = bsDesc.name;

    m_par.mfx.FrameInfo.FourCC = fourcc;
    set_chromaformat_mfx(&m_par);

    m_par.mfx.FrameInfo.Width  = (bsDesc.w + 15) & ~15;
    m_par.mfx.FrameInfo.Height = (bsDesc.h + 15) & ~15;

    switch (fourcc)
    {
        case MFX_FOURCC_P010:
        case MFX_FOURCC_Y410: m_par.mfx.FrameInfo.BitDepthChroma = m_par.mfx.FrameInfo.BitDepthLuma = 10;

        case MFX_FOURCC_P016:
        case MFX_FOURCC_Y416: m_par.mfx.FrameInfo.BitDepthChroma = m_par.mfx.FrameInfo.BitDepthLuma = 12;
    };

    if (   fourcc == MFX_FOURCC_P010
        || fourcc == MFX_FOURCC_P016
        || fourcc == MFX_FOURCC_Y416)
        m_par.mfx.FrameInfo.Shift = 1;

    m_par_set = true; //we are not testing DecodeHeader here

    const tc_struct& tc = test_case[id];
    return RunTest(tc);
}

int TestSuite::RunTest(const tc_struct& tc)
{
    TS_START;

    MFXInit();
    if(m_uid)
        Load();

    ReadStream();

    tsStruct::SetPars(m_par, tc);

    if(NO_EXT_ALLOCATOR != tc.mode)
    {
        bool isSW = !(!!(m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY || m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY));
        UseDefaultAllocator(isSW);
        m_pFrameAllocator = GetAllocator();
        SetFrameAllocator(m_session, GetAllocator());

        if (!isSW && !m_is_handle_set)
        {
            mfxHDL hdl = 0;
            mfxHandleType type = static_cast<mfxHandleType>(0);
            GetAllocator()->get_hdl(type, hdl);
            SetHandle(m_session, type, hdl);
        }
    }

    if(FAILED_INIT == tc.mode)
    {
        g_tsStatus.disable_next_check();
        Init();
    }
    if(INITED_DECODER == tc.mode)
    {
        Init();
    }

    if(CLOSED_DECODER == tc.mode)
    {
        Init();
        g_tsStatus.disable_next_check();
        Close();
    }

    //test section
    {
        tsStruct::SetPars(m_bitstream, tc, RUNTIME_BS);
        tsStruct::SetPars(m_par, tc, RUNTIME_PAR);

        auto ses = (NULL_SESSION  == tc.mode) ? nullptr : m_session;
        auto bs  = (NULL_BS       == tc.mode) ? nullptr : m_pBitstream;
        auto par = (NULL_PAR      == tc.mode) ? nullptr : &m_par;

        g_tsStatus.expect(tc.sts);
        DecodeHeader(ses, bs, par);
    }

    TS_END;
    return 0;
}

void TestSuite::ReadStream()
{
    const char* sname = g_tsStreamPool.Get(m_input_stream_name);
    g_tsStreamPool.Reg();
    m_bs_reader.reset(new tsBitstreamReaderIVF(sname, 1024*1024) );
    m_bs_processor = m_bs_reader.get();

    m_pBitstream = m_bs_processor->ProcessBitstream(m_bitstream);
}

TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_8b_420_decode_header,  RunTest_fourcc<MFX_FOURCC_NV12>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_10b_420_decode_header, RunTest_fourcc<MFX_FOURCC_P010>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_8b_444_decode_header,  RunTest_fourcc<MFX_FOURCC_AYUV>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_10b_444_decode_header, RunTest_fourcc<MFX_FOURCC_Y410>, n_cases);

TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_12b_420_p016_decode_header, RunTest_fourcc<MFX_FOURCC_P016>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_12b_444_y416_decode_header, RunTest_fourcc<MFX_FOURCC_Y416>, n_cases);

}
#undef TEST_NAME
