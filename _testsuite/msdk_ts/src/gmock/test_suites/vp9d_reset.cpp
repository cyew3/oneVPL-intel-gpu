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

#define TEST_NAME vp9d_reset
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
        mfxU8  isInvalid;
    };
    struct tc_struct
    {
        mfxStatus sts;
        mfxU32    mode;
        f_pair    set_par[MAX_NPARS];
        mfxU32    n_frames;
    };
    static const tc_struct test_case[];      //base cases

    template<mfxU32 fourcc>
    int RunTest_fourcc(const unsigned int id);

    static const unsigned int n_cases;

private:

    enum
    {
        NULL_SESSION = 1,
        NULL_PAR,
        MULTIPLE_INIT,
        FAILED_INIT,
        CLOSED,
        REPEAT_RESET
    };

    enum
    {
        INIT = 0,
        RESET,
    };

    void AllocOpaque();
    void ReadStream();

    int RunTest(const tc_struct& tc);
    std::unique_ptr<tsBitstreamProcessor> m_bs_reader;
    std::string m_input_stream_name;
};

static const tsStruct::Field* const AsyncDepth    (&tsStruct::mfxVideoParam.AsyncDepth);
static const tsStruct::Field* const PicStruct     (&tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct);
static const tsStruct::Field* const IOPattern     (&tsStruct::mfxVideoParam.IOPattern);
static const tsStruct::Field* const Width         (&tsStruct::mfxVideoParam.mfx.FrameInfo.Width);
static const tsStruct::Field* const Height        (&tsStruct::mfxVideoParam.mfx.FrameInfo.Height);
static const tsStruct::Field* const CropX         (&tsStruct::mfxVideoParam.mfx.FrameInfo.CropX);
static const tsStruct::Field* const CropY         (&tsStruct::mfxVideoParam.mfx.FrameInfo.CropY);
static const tsStruct::Field* const CropW         (&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW);
static const tsStruct::Field* const CropH         (&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH);
static const tsStruct::Field* const FourCC        (&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC);
static const tsStruct::Field* const ChromaFormat  (&tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat);
static const tsStruct::Field* const AspectRatioW  (&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW);
static const tsStruct::Field* const AspectRatioH  (&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH);
static const tsStruct::Field* const FrameRateExtN (&tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN);
static const tsStruct::Field* const FrameRateExtD (&tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD);
static const tsStruct::Field* const BitDepthLuma  (&tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma);
static const tsStruct::Field* const BitDepthChroma(&tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma);

template <typename TB>
void CheckParams(tsExtBufType<TB>& par, const TestSuite::f_pair (&pairs)[MAX_NPARS], mfxU32 stage)
{
    const size_t length = sizeof(pairs)/sizeof(pairs[0]);
    for(size_t i(0); i < length; ++i)
    {
        if(pairs[i].f && stage == pairs[i].stage)
        {
            void* ptr = 0;

            if((typeid(TB) == typeid(mfxVideoParam) && ( pairs[i].f->name.find("mfxVideoParam") != std::string::npos) ) || 
               (typeid(TB) == typeid(mfxBitstream)  && ( pairs[i].f->name.find("mfxBitstream")  != std::string::npos) ) || 
               (typeid(TB) == typeid(mfxEncodeCtrl) && ( pairs[i].f->name.find("mfxEncodeCtrl") != std::string::npos) ))
            {
                ptr = (TB*) &par;
            }
            else
            {
                mfxU32 bufId = 0, bufSz = 0;
                GetBufferIdSz(pairs[i].f->name, bufId, bufSz);
                if(0 == bufId + bufSz)
                {
                    EXPECT_NE((mfxU32)0, bufId + bufSz) << "ERROR: (in test) failed to get Ext buffer Id or Size\n";
                    throw tsFAIL;
                }
                ptr = par.GetExtBuffer(bufId);
                if(!ptr)
                {
                    const std::string& buffer_name = pairs[i].f->name.substr(0, pairs[i].f->name.find(":"));
                    EXPECT_NE(nullptr, ptr) << "ERROR: extended buffer is missing!\n" << "Missing buffer: " << buffer_name <<"\n";
                    throw tsFAIL;
                }
            }

            if(pairs[i].isInvalid)
                tsStruct::check_ne(ptr, *pairs[i].f, pairs[i].v);
            else
                tsStruct::check_eq(ptr, *pairs[i].f, pairs[i].v);
        }
    }
}

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_INVALID_HANDLE,  NULL_SESSION},
    {/*01*/ MFX_ERR_NULL_PTR,        NULL_PAR},
    {/*02*/ MFX_ERR_NOT_INITIALIZED, CLOSED},
    {/*03*/ MFX_ERR_NOT_INITIALIZED, FAILED_INIT},
    {/*04*/ MFX_ERR_NONE,            REPEAT_RESET},
    {/*05*/ MFX_ERR_NONE, 0, {IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY, INIT}, 5},
    {/*06*/ MFX_ERR_NONE, 0, {IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY , INIT} , 5},
    {/*07*/ MFX_ERR_NONE, 0, {IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY, INIT}, 5},
    {/*08*/ MFX_ERR_NONE, 0, {{IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY, INIT},
                              {IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY, RESET}}, 0},
    {/*09*/ MFX_ERR_NONE, 0, {{Width, 1920}, {Height, 1088}}, 0},
    {/*10*/ MFX_ERR_NONE, 0, {{Width, 3840}, {Height, 2160}}, 0},
    {/*11*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, {{IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY, INIT},
                                                  {IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY, RESET}}, 5},
    {/*12*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, {{IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY, INIT},
                                                  {IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY, RESET}}, 5},
    {/*13*/ MFX_ERR_INVALID_VIDEO_PARAM, 0,      {{IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY, INIT},
                                                  {IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY, RESET}}, 5},
    {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{ChromaFormat, MFX_CHROMAFORMAT_YUV411, RESET}}, 0},
    {/*15*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_YV12, RESET}}, 0},
    {/*16*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {&tsStruct::mfxVideoParam.Protected, 2, RESET}},
    {/*17*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, {{Width, 1920, INIT}, {Width, 1920+32, RESET}}, 0},
    {/*18*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, {{Height, 1088, INIT}, {Height, 1088+32, RESET}}, 0},
    {/*19*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, {{AsyncDepth, 1, INIT}, {AsyncDepth, 5, RESET}}, 0},
    {/*20*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, {{AsyncDepth, 5, INIT}, {AsyncDepth, 1, RESET}}, 0},
    {/*21*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, {{Width,  1920, INIT}, {Width,  1920+32, RESET}}, 0},
    {/*22*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, {{Height, 1088, INIT}, {Height, 1088+32, RESET}}, 0},
    {/*23*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{Width,  1920, INIT}, {Width,  1920-8, RESET}}, 0},
    {/*24*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{Height, 1088, INIT}, {Height, 1088-8, RESET}}, 0},
    {/*25*/ MFX_ERR_NONE, 0, {{CropX, 10, RESET}, {CropY, 10, RESET}, {CropW, 352, RESET}, {CropH, 288, RESET}}, 0},
    {/*26*/ MFX_ERR_NONE, 0, {{AspectRatioW, 22, RESET}, {AspectRatioH, 9, RESET}}, 0},
    {/*27*/ MFX_ERR_NONE, 0, {{FrameRateExtN, 60, INIT}, {FrameRateExtD, 1, INIT}, {FrameRateExtN, 30, RESET}}, 0},
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
        case MFX_FOURCC_NV12:
        case MFX_FOURCC_AYUV: m_par.mfx.FrameInfo.BitDepthChroma = m_par.mfx.FrameInfo.BitDepthLuma = 8; break;

        case MFX_FOURCC_P010:
        case MFX_FOURCC_Y410: m_par.mfx.FrameInfo.BitDepthChroma = m_par.mfx.FrameInfo.BitDepthLuma = 10; break;

        case MFX_FOURCC_P016:
        case MFX_FOURCC_Y416: m_par.mfx.FrameInfo.BitDepthChroma = m_par.mfx.FrameInfo.BitDepthLuma = 12; break;
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

    ReadStream();

    MFXInit();
    if(m_uid)
        Load();

    tsStruct::SetPars(m_par, tc, INIT);

    bool isSW = !!(m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);
    UseDefaultAllocator(isSW);
    m_pFrameAllocator = GetAllocator();
    if (!isSW && !m_is_handle_set)
    {
        mfxHDL hdl = 0;
        mfxHandleType type = static_cast<mfxHandleType>(0);
        GetAllocator()->get_hdl(type, hdl);
        SetHandle(m_session, type, hdl);
    }
    if(!(m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
    {
        SetFrameAllocator(m_session, GetAllocator());
    }
    else
    {
        AllocOpaque();
    }

    if (FAILED_INIT == tc.mode)
    {
        tsExtBufType<mfxVideoParam> tmpPar(m_par);
        tmpPar.mfx.FrameInfo.Width = tmpPar.mfx.FrameInfo.Height = 5;
        g_tsStatus.expect(MFX_ERR_INVALID_VIDEO_PARAM);
        Init(m_session, &tmpPar);
    }
    else
    {
        tsExtBufType<mfxVideoParam> tmpPar(m_par); //component must not rely on provided buffer after Init function exit
        Init(m_session, &tmpPar);

        DecodeFrames(tc.n_frames);
    }

    if (CLOSED == tc.mode)
    {
        g_tsStatus.expect(MFX_ERR_NONE);
        Close();
    }

    //test section
    {
        tsStruct::SetPars(m_par, tc, RESET);

        mfxSession ses     = (NULL_SESSION  == tc.mode) ? nullptr : m_session;
        mfxVideoParam* par = (NULL_PAR      == tc.mode) ? nullptr : &m_par;

        g_tsStatus.expect(tc.sts);
        Reset(ses, par);

        if(REPEAT_RESET == tc.mode)
        {
            g_tsStatus.expect(tc.sts);
            Reset(ses, par);
        }

    }

    g_tsStatus.disable_next_check();
    Close();

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

void TestSuite::AllocOpaque()
{
    if(m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        //AllocSurfaces();
        m_par.AddExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION, sizeof(mfxExtOpaqueSurfaceAlloc));
        mfxExtOpaqueSurfaceAlloc *osa = (mfxExtOpaqueSurfaceAlloc*)m_par.GetExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

        mfxFrameAllocRequest request = {};
        g_tsStatus.disable_next_check();
        QueryIOSurf(m_session, m_pPar, &request);
        
        tsSurfacePool::AllocOpaque(request, *osa);
    }
}

TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_8b_420_reset,  RunTest_fourcc<MFX_FOURCC_NV12>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_10b_420_reset, RunTest_fourcc<MFX_FOURCC_P010>, n_cases);

TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_8b_444_ayuv_reset,  RunTest_fourcc<MFX_FOURCC_AYUV>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_10b_444_y410_reset, RunTest_fourcc<MFX_FOURCC_Y410>, n_cases);

TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_12b_420_p016_reset, RunTest_fourcc<MFX_FOURCC_P016>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_12b_444_y416_reset, RunTest_fourcc<MFX_FOURCC_Y416>, n_cases);

}
#undef TEST_NAME
