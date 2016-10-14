/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include <memory>
#include "ts_decoder.h"
#include "ts_struct.h"

#define TEST_NAME vp9d_query
namespace TEST_NAME
{
class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_VP9) { }
    ~TestSuite() {}

    enum
    {
        VALID = 0,
        INVALID = 1,
        IGNORED = 2,
    };
    struct f_pair
    {
        const  tsStruct::Field* f;
        mfxU64 v;
        mfxU8  isInvalid;
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

    template<mfxU32 fourcc>
    int RunTest_fourcc(const unsigned int id);

    static const unsigned int n_cases;
    static const unsigned int n_cases_nv12;
    static const unsigned int n_cases_ayuv;
    static const unsigned int n_cases_p010;
    static const unsigned int n_cases_y410;

private:

    enum
    {
        NULL_SESSION = 1,
        NULL_PAR_IN,
        NULL_PAR_OUT,
        NULL_PAR_BOTH,
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

template <typename TB>
void CheckParams(tsExtBufType<TB>& par, const TestSuite::f_pair (&pairs)[MAX_NPARS])
{
    const size_t length = sizeof(pairs)/sizeof(pairs[0]);
    for(size_t i(0); i < length; ++i)
    {
        if(pairs[i].f && TestSuite::IGNORED != pairs[i].isInvalid)
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
    {/*00*/ MFX_ERR_NONE},
    {/*01*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION},
    {/*02*/ MFX_ERR_NONE, NULL_PAR_IN},
    {/*03*/ MFX_ERR_NULL_PTR, NULL_PAR_OUT},
    {/*04*/ MFX_ERR_NULL_PTR, NULL_PAR_BOTH},
    {/*05*/ MFX_ERR_NONE, FAILED_INIT},
    {/*06*/ MFX_ERR_UNSUPPORTED, PLUGIN_NOT_LOADED},
    {/*07*/ MFX_ERR_NONE, NO_EXT_ALLOCATOR},
    {/*08*/ MFX_ERR_NONE, NO_EXT_ALLOCATOR, {IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}},

    {/*09*/ MFX_ERR_NONE, 0, {IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*10*/ MFX_ERR_NONE, 0, {{Width, 1920}, {Height, 1088}}},
    {/*11*/ MFX_ERR_NONE, 0, {{Width, 3840}, {Height, 2160}}},
    {/*12*/ MFX_ERR_NONE, 0, {{Width, 0}, {Height, 0}}},
    //should be ignored during init
    {/*13*/ MFX_ERR_NONE, 0, {{CropX, 65535, IGNORED}, {CropY, 2160, IGNORED}, {CropW, 23}, {CropH, 15}}},
    {/*14*/ MFX_ERR_NONE, 0, {PicStruct, MFX_PICSTRUCT_PROGRESSIVE, IGNORED}},
    {/*15*/ MFX_ERR_NONE, 0, {PicStruct, MFX_PICSTRUCT_FIELD_TFF, IGNORED}},
    {/*16*/ MFX_ERR_NONE, 0, {PicStruct, MFX_PICSTRUCT_FIELD_BFF, IGNORED}},
    {/*17*/ MFX_ERR_NONE, 0, {PicStruct, MFX_PICSTRUCT_FRAME_TRIPLING, IGNORED}},
    //Any valid values or unspecified (zero);
    {/*18*/ MFX_ERR_NONE, 0, {{AspectRatioW, 0}, {AspectRatioH, 0}}},
    {/*19*/ MFX_ERR_NONE, 0, {{AspectRatioW, 16}, {AspectRatioH, 9}}},
    {/*20*/ MFX_ERR_UNSUPPORTED, 0, {{AspectRatioW, 0}, {AspectRatioH, 15, INVALID}}},
    {/*21*/ MFX_ERR_NONE, 0, {{FrameRateExtN, 0}, {FrameRateExtD, 0}}},
    {/*22*/ MFX_ERR_NONE, 0, {{FrameRateExtN, 30}, {FrameRateExtD, 1}}},
    {/*23*/ MFX_ERR_UNSUPPORTED, 0, {{FrameRateExtN, 100, INVALID}, {FrameRateExtD, 0}}},

    {/*24*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_Y210, INVALID} }},
    {/*25*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_UYVY, INVALID} }},
    {/*26*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_RGB4, INVALID} }},
    {/*27*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_YUY2, INVALID} }},
    {/*28*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_NV16, INVALID} }},
    {/*29*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_ARGB16, INVALID} }},

    {/*30*/ MFX_ERR_UNSUPPORTED, 0, {{&tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_PAVP} }, },
    {/*31*/ MFX_ERR_UNSUPPORTED, 0, {{&tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_GPUCP_PAVP} }, },
};
const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

const TestSuite::tc_struct TestSuite::test_case_nv12[] =
{
    {/*32*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_NV12}, {BitDepthLuma, 0}}},
    {/*33*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_NV12}, {BitDepthLuma, 8, IGNORED}}},
    {/*34*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_NV12, IGNORED}, {BitDepthLuma, 10, INVALID}}},
    {/*35*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_NV12, INVALID}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444, INVALID}}},

    {/*36*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_AYUV, INVALID}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420, INVALID}}},
    {/*37*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_P010, IGNORED}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420, IGNORED}}},
    {/*38*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_Y410, INVALID}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420, INVALID}}},
};
const unsigned int TestSuite::n_cases_nv12 = sizeof(TestSuite::test_case_nv12)/sizeof(TestSuite::tc_struct) + n_cases;

const TestSuite::tc_struct TestSuite::test_case_ayuv[] =
{
    {/*32*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_AYUV}, {BitDepthLuma, 0}}},
    {/*33*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_AYUV}, {BitDepthLuma, 8}}},
    {/*34*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_AYUV, IGNORED}, {BitDepthLuma, 10, INVALID}}},
    {/*35*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_AYUV, INVALID}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420, INVALID}}},

    {/*36*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_NV12, INVALID}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444, INVALID}}},
    {/*37*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_P010, INVALID}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444, INVALID}}},
    {/*38*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_Y410, IGNORED}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444, IGNORED}}},
};
const unsigned int TestSuite::n_cases_ayuv = sizeof(TestSuite::test_case_ayuv)/sizeof(TestSuite::tc_struct) + n_cases;

const TestSuite::tc_struct TestSuite::test_case_p010[] =
{
    {/*32*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_P010, IGNORED}, {BitDepthLuma, 0, INVALID}}},
    {/*33*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_P010}, {BitDepthLuma, 10}}},
    {/*34*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_P010, IGNORED}, {BitDepthLuma, 8, INVALID}}},
    {/*35*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_P010, INVALID}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444, INVALID}}},

    {/*36*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_NV12, IGNORED}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420, IGNORED}}},
    {/*37*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_AYUV, INVALID}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420, INVALID}}},
    {/*38*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_Y410, INVALID}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420, INVALID}}},
};
const unsigned int TestSuite::n_cases_p010 = sizeof(TestSuite::test_case_p010)/sizeof(TestSuite::tc_struct) + n_cases;

const TestSuite::tc_struct TestSuite::test_case_y410[] =
{
    {/*32*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_Y410, IGNORED}, {BitDepthLuma, 0, INVALID}}},
    {/*33*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_Y410}, {BitDepthLuma, 10}}},
    {/*34*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_Y410, IGNORED}, {BitDepthLuma, 8, INVALID}}},
    {/*35*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_Y410, INVALID}, {ChromaFormat, MFX_CHROMAFORMAT_YUV420, INVALID}}},

    {/*36*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_NV12, INVALID}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444, INVALID}}},
    {/*37*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_AYUV, IGNORED}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444, IGNORED}}},
    {/*38*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_P010, INVALID}, {ChromaFormat, MFX_CHROMAFORMAT_YUV444, INVALID}}},
};
const unsigned int TestSuite::n_cases_y410 = sizeof(TestSuite::test_case_y410)/sizeof(TestSuite::tc_struct) + n_cases;

const TestSuite::tc_struct* getTestTable(const mfxU32& fourcc)
{
    switch(fourcc)
    {
    case MFX_FOURCC_NV12: return TestSuite::test_case_nv12;
    case MFX_FOURCC_AYUV: return TestSuite::test_case_ayuv;
    case MFX_FOURCC_P010: return TestSuite::test_case_p010;
    case MFX_FOURCC_Y410: return TestSuite::test_case_y410;
    default: assert(0); return 0;
    }
}

template<mfxU32 fourcc>
int TestSuite::RunTest_fourcc(const unsigned int id)
{
    m_par.mfx.FrameInfo.FourCC = fourcc;
    set_chromaformat_mfx(&m_par);
    if(MFX_FOURCC_P010 == fourcc || MFX_FOURCC_Y410 == fourcc)
        m_par.mfx.FrameInfo.BitDepthChroma = m_par.mfx.FrameInfo.BitDepthLuma = 10;
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
        if(NULL_PAR_IN == tc.mode)
        {
            tsExtBufType<mfxVideoParam> par_out;
            par_out.mfx.CodecId = MFX_CODEC_VP9;

            g_tsStatus.expect(tc.sts);
            Query(m_session, 0, &par_out);

            EXPECT_NE(0, par_out.AsyncDepth);
            EXPECT_NE(0, par_out.mfx.FrameInfo.BitDepthChroma);
            EXPECT_NE(0, par_out.mfx.FrameInfo.BitDepthLuma);
            EXPECT_NE(0, par_out.mfx.FrameInfo.FourCC);
            EXPECT_NE(0, par_out.mfx.FrameInfo.Width);
            EXPECT_NE(0, par_out.mfx.FrameInfo.Height);
            EXPECT_NE(0, par_out.mfx.FrameInfo.ChromaFormat);
        }
        else
        {
            const tsExtBufType<mfxVideoParam> tmpPar(m_par);
            tsExtBufType<mfxVideoParam> ParOut = tmpPar;

            mfxSession ses = (NULL_SESSION == tc.mode) ? nullptr : m_session;
            mfxVideoParam* pParIn = (NULL_PAR_IN == tc.mode) ? nullptr : &m_par;
            mfxVideoParam* pParOut = (NULL_PAR_OUT == tc.mode) ? nullptr : &ParOut;
            if(NULL_PAR_BOTH == tc.mode)
                pParIn = pParOut = nullptr;

            //1. Query(p_in, p_out)
            g_tsStatus.expect(tc.sts);
            Query(ses, pParIn, pParOut);
            EXPECT_EQ(tmpPar, m_par) << "ERROR: Decoder must not change input structure in DecodeQuery()!\n";
            CheckParams(ParOut, tc.set_par);

            if(NULL_PAR_OUT != tc.mode && NULL_PAR_IN != tc.mode)
            {
                //2. Query(p_in, p_in)
                g_tsStatus.expect(tc.sts);
                Query(ses, pParIn, pParIn);
                CheckParams(m_par, tc.set_par);
            }
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_8b_420_query,  RunTest_fourcc<MFX_FOURCC_NV12>, n_cases_nv12);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_10b_420_query, RunTest_fourcc<MFX_FOURCC_P010>, n_cases_p010);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_8b_444_query,  RunTest_fourcc<MFX_FOURCC_AYUV>, n_cases_ayuv);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_10b_444_query, RunTest_fourcc<MFX_FOURCC_Y410>, n_cases_y410);

}
#undef TEST_NAME
