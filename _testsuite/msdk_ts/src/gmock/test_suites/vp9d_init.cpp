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

#define TEST_NAME vp9d_init
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
        mfxU32    mode;
        f_pair    set_par[MAX_NPARS];
    };
    static const tc_struct test_case[];      //base cases
    static const tc_struct test_case_nv12[]; //8b 420
    static const tc_struct test_case_ayuv[]; //8b 444
    static const tc_struct test_case_p010[]; //10b 420
    static const tc_struct test_case_y410[]; //10b 444
    static const tc_struct test_case_p016[]; //12b 420
    static const tc_struct test_case_y416[]; //12b 444

    template<mfxU32 fourcc>
    int RunTest_fourcc(const unsigned int id);

    static const unsigned int n_cases;
    static const unsigned int n_cases_nv12;
    static const unsigned int n_cases_ayuv;
    static const unsigned int n_cases_p010;
    static const unsigned int n_cases_y410;
    static const unsigned int n_cases_p016;
    static const unsigned int n_cases_y416;

private:

    enum
    {
        NULL_SESSION = 1,
        NULL_PAR,
        PLUGIN_NOT_LOADED,
        MULTIPLE_INIT,
        FAILED_INIT,
        NO_EXT_ALLOCATOR,
    };

    enum
    {
        INIT = 0,
    };

    void AllocOpaque();

    int RunTest(const tc_struct& tc);
};

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

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_INVALID_HANDLE,      NULL_SESSION},
    {/*01*/ MFX_ERR_NULL_PTR,            NULL_PAR},
    {/*02*/ MFX_ERR_UNDEFINED_BEHAVIOR,  MULTIPLE_INIT},
    {/*03*/ MFX_ERR_INVALID_VIDEO_PARAM, PLUGIN_NOT_LOADED},
    {/*04*/ MFX_ERR_NONE,                NO_EXT_ALLOCATOR},
    {/*05*/ MFX_ERR_INVALID_VIDEO_PARAM, NO_EXT_ALLOCATOR, {IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/*06*/ MFX_ERR_NONE,                FAILED_INIT},

    {/*07*/ MFX_ERR_NONE, 0, {IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
    {/*08*/ MFX_ERR_NONE, 0, {IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/*09*/ MFX_ERR_NONE, 0, {IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*10*/ MFX_ERR_NONE, 0, {IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},

    {/*11*/ MFX_ERR_NONE, 0, {{Width, 1920}, {Height, 1088}}},
    {/*12*/ MFX_ERR_NONE, 0, {{Width, 3840}, {Height, 2160}}},
    {/*13*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{Width, 0}, {Height, 0}}},
    {/*14*/ MFX_ERR_NONE, 0, {{CropX, 65535}, {CropY, 2160}, {CropW, 23}, {CropH, 15}}}, //should be ignored during init
    {/*15*/ MFX_ERR_NONE, 0, {PicStruct, MFX_PICSTRUCT_FRAME_TRIPLING}}, //should be ignored during init
    {/*16*/ MFX_ERR_NONE, 0, {{AspectRatioW, 0}, {AspectRatioH, 0}}},
    {/*17*/ MFX_ERR_NONE, 0, {{AspectRatioW, 16}, {AspectRatioH, 9}}},
    {/*18*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{AspectRatioW, 0}, {AspectRatioH, 15}}},
    {/*19*/ MFX_ERR_NONE, 0, {{FrameRateExtN, 0}, {FrameRateExtD, 0}}},
    {/*20*/ MFX_ERR_NONE, 0, {{FrameRateExtN, 30}, {FrameRateExtD, 1}}},
    {/*21*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FrameRateExtN, 100}, {FrameRateExtD, 0}}},
    {/*22*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_Y210} }},
    {/*23*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_UYVY} }},
    {/*24*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_RGB4} }},
    {/*25*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_GPUCP_AES128_CTR} }, },
    {/*26*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_WIDEVINE_GOOGLE_DASH} }, },
};
const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

const TestSuite::tc_struct TestSuite::test_case_nv12[] =
{
    {/*27*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_NV12}, {BitDepthLuma, 0}}},
    {/*28*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_NV12}, {BitDepthLuma, 8}}},
    {/*29*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_NV12}, {BitDepthLuma, 10}}},
    {/*30*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_NV12}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},

    {/*31*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_AYUV}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420}}},
    {/*32*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_P010}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420}}},
    {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_Y410}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420}}},
};
const unsigned int TestSuite::n_cases_nv12 = sizeof(TestSuite::test_case_nv12)/sizeof(TestSuite::tc_struct) + n_cases;

const TestSuite::tc_struct TestSuite::test_case_ayuv[] =
{
    {/*27*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_AYUV}, {BitDepthLuma, 0}}},
    {/*28*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_AYUV}, {BitDepthLuma, 8}}},
    {/*29*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_AYUV}, {BitDepthLuma, 10}}},
    {/*30*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_AYUV}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420}}},

    {/*31*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_NV12}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},
    {/*32*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_P010}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},
    {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_Y410}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},
};
const unsigned int TestSuite::n_cases_ayuv = sizeof(TestSuite::test_case_ayuv)/sizeof(TestSuite::tc_struct) + n_cases;

const TestSuite::tc_struct TestSuite::test_case_p010[] =
{
    {/*27*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_P010}, {BitDepthLuma, 0}}},
    {/*28*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_P010}, {BitDepthLuma, 10}}},
    {/*29*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_P010}, {BitDepthLuma, 8}}},
    {/*30*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_P010}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},

    {/*31*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_NV12}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420}}},
    {/*32*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_AYUV}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420}}},
    {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_Y410}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420}}},
};
const unsigned int TestSuite::n_cases_p010 = sizeof(TestSuite::test_case_p010)/sizeof(TestSuite::tc_struct) + n_cases;

const TestSuite::tc_struct TestSuite::test_case_y410[] =
{
    {/*27*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_Y410}, {BitDepthLuma, 0}}},
    {/*28*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_Y410}, {BitDepthLuma, 10}}},
    {/*29*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_Y410}, {BitDepthLuma, 8}}},
    {/*30*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_Y410}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420}}},

    {/*31*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_NV12}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},
    {/*32*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_AYUV}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},
    {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_P010}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},
};
const unsigned int TestSuite::n_cases_y410 = sizeof(TestSuite::test_case_y410)/sizeof(TestSuite::tc_struct) + n_cases;

const TestSuite::tc_struct TestSuite::test_case_p016[] =
{
    {/*27*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_P016}, {BitDepthLuma, 0}}},
    {/*28*/ MFX_ERR_NONE, 0,                {{FourCC, MFX_FOURCC_P016}, {BitDepthLuma, 12}}},
    {/*29*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_P016}, {BitDepthLuma, 8}}},
    {/*30*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_P016}, {BitDepthLuma, 10}}},
    {/*31*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_P016}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},

    {/*32*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_AYUV}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420}}},
    {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_Y410}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420}}},
    {/*34*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_Y416}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420}}},
};
const unsigned int TestSuite::n_cases_p016 = sizeof(TestSuite::test_case_p016)/sizeof(TestSuite::tc_struct) + n_cases;

const TestSuite::tc_struct TestSuite::test_case_y416[] =
{
    {/*27*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_Y416}, {BitDepthLuma, 0}}},
    {/*28*/ MFX_ERR_NONE, 0,                {{FourCC, MFX_FOURCC_Y416}, {BitDepthLuma, 12}}},
    {/*29*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_Y416}, {BitDepthLuma, 8}}},
    {/*29*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_Y416}, {BitDepthLuma, 10}}},
    {/*30*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_Y416}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420}}},

    {/*31*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_NV12}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},
    {/*32*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_AYUV}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},
    {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_P010}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},
    {/*34*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_P016}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},
};
const unsigned int TestSuite::n_cases_y416 = sizeof(TestSuite::test_case_y416)/sizeof(TestSuite::tc_struct) + n_cases;

const TestSuite::tc_struct* getTestTable(const mfxU32& fourcc)
{
    switch(fourcc)
    {
        case MFX_FOURCC_NV12: return TestSuite::test_case_nv12;
        case MFX_FOURCC_AYUV: return TestSuite::test_case_ayuv;
        case MFX_FOURCC_P010: return TestSuite::test_case_p010;
        case MFX_FOURCC_Y410: return TestSuite::test_case_y410;
        case MFX_FOURCC_P016: return TestSuite::test_case_p016;
        case MFX_FOURCC_Y416: return TestSuite::test_case_y416;

        default: assert(0); return 0;
    }
}

template<mfxU32 fourcc>
int TestSuite::RunTest_fourcc(const unsigned int id)
{
    m_par.mfx.FrameInfo.FourCC = fourcc;
    set_chromaformat_mfx(&m_par);

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

    const tc_struct* fourcc_table = getTestTable(fourcc);
    
    const unsigned int real_id = (id < n_cases) ? (id) : (id - n_cases);
    const tc_struct& tc = (real_id == id) ? test_case[real_id] : fourcc_table[real_id];

    return RunTest(tc);
}

int TestSuite::RunTest(const tc_struct& tc)
{
    TS_START;

    MFXInit();
    if(m_uid && PLUGIN_NOT_LOADED != tc.mode)
        Load();

    tsStruct::SetPars(m_par, tc, INIT);

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

    if(PLUGIN_NOT_LOADED != tc.mode)
        AllocSurfaces();

    if (MULTIPLE_INIT == tc.mode)
        Init(m_session, m_pPar);

    if (FAILED_INIT == tc.mode)
    {
        tsExtBufType<mfxVideoParam> tmpPar(m_par);
        tmpPar.mfx.FrameInfo.Width = tmpPar.mfx.FrameInfo.Height = 5;
        if(MFX_ERR_NONE <= tc.sts)
            g_tsStatus.expect(MFX_ERR_INVALID_VIDEO_PARAM);
        Init(m_session, &tmpPar);
    }

    //test section
    {
        const tsExtBufType<mfxVideoParam> tmpPar(m_par);
        mfxSession ses     = (NULL_SESSION  == tc.mode) ? nullptr : m_session;
        mfxVideoParam* par = (NULL_PAR      == tc.mode) ? nullptr : &m_par;

        g_tsStatus.expect(tc.sts);
        Init(ses, par);

        EXPECT_EQ(tmpPar, m_par) << "ERROR: Decoder must not change input structure in DecodeInit()!\n";
    }

    g_tsStatus.disable_next_check();
    Close();

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_8b_420_init,       RunTest_fourcc<MFX_FOURCC_NV12>, n_cases_nv12);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_10b_420_init,      RunTest_fourcc<MFX_FOURCC_P010>, n_cases_p010);

TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_8b_444_ayuv_init,  RunTest_fourcc<MFX_FOURCC_AYUV>, n_cases_ayuv);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_10b_444_y410_init, RunTest_fourcc<MFX_FOURCC_Y410>, n_cases_y410);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_12b_420_p016_init, RunTest_fourcc<MFX_FOURCC_P016>, n_cases_p016);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_12b_444_y416_init, RunTest_fourcc<MFX_FOURCC_Y416>, n_cases_y416);

}
#undef TEST_NAME
