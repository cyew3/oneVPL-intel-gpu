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

namespace vp9d_all_basic_tests
{

class tsIvfReader : public tsBitstreamProcessor, public tsReader
{
    #pragma pack(push, 4)
    typedef struct
    {
        union{
            mfxU32 _signature;
            mfxU8  signature[4]; //DKIF
        };
        mfxU16 version;
        mfxU16 header_length; //bytes
        mfxU32 FourCC;        //CodecId
        mfxU16 witdh;
        mfxU16 height;
        mfxU32 frame_rate;
        mfxU32 time_scale;
        mfxU32 n_frames;
        mfxU32 unused;
    } IVF_file_header;

    typedef struct
    {
        mfxU32 frame_size; //bytes
        mfxU64 time_stamp;
    } IVF_frame_header;
    #pragma pack(pop)

    IVF_file_header file_header;
    IVF_frame_header frame_header;
    size_t count;

public:
    bool    m_eos;
    mfxU8*  m_buf;
    size_t  m_buf_size;
    mfxU8*  inter_buf;
    size_t  inter_buf_lenth;
    size_t  inter_buf_size;

    tsIvfReader(const char* fname, mfxU32 buf_size)
        : tsReader(fname)
        , count(0)
        , m_eos(false)
        , m_buf(new mfxU8[buf_size])
        , m_buf_size(buf_size)
        , inter_buf(new mfxU8[buf_size])
        , inter_buf_lenth(0)
        , inter_buf_size(buf_size)
    {
        inter_buf_lenth += Read(inter_buf, inter_buf_size);
        assert(inter_buf_lenth >= (sizeof(IVF_file_header) + sizeof(IVF_frame_header)));
        for(size_t i(0); i < inter_buf_lenth; ++i)
        {
            if(0 == strcmp((char*)(inter_buf + i), "DKIF"))
            {
                inter_buf_lenth -= i;
                memmove(inter_buf, inter_buf + i, inter_buf_lenth);
                break;
            }
        }
        IVF_file_header const * const fl_hdr = (IVF_file_header*) inter_buf;
        file_header = *fl_hdr;
        IVF_frame_header const * const fr_hdr = (IVF_frame_header*) (inter_buf + file_header.header_length);
        frame_header = *fr_hdr;

        inter_buf_lenth -= file_header.header_length + sizeof(IVF_frame_header);
        memmove(inter_buf, inter_buf + file_header.header_length + sizeof(IVF_frame_header), inter_buf_lenth);

        inter_buf_lenth += Read(inter_buf, (inter_buf_size - inter_buf_lenth));
    };
    virtual ~tsIvfReader()
    {
        if(m_buf)
        {
            delete[] m_buf;
            m_buf = 0;
        }
        if(inter_buf)
        {
            delete[] inter_buf;
            inter_buf = 0;
        }
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        if(m_eos)
            return MFX_ERR_MORE_DATA;
        if(bs.DataLength + bs.DataOffset > m_buf_size)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        if(bs.DataLength)
            return MFX_ERR_NONE;

        bs.Data      = m_buf;
        bs.MaxLength = m_buf_size;

        bs.DataOffset = 0;
        bs.DataLength = frame_header.frame_size;
        bs.TimeStamp  = frame_header.time_stamp;
        memcpy(bs.Data, inter_buf, bs.DataLength);
        ++count;

        if(count == file_header.n_frames)
        {
            m_eos = true;
        }
        else
        {
            const IVF_frame_header fr_hdr_next = *((IVF_frame_header*) (inter_buf + frame_header.frame_size));
            assert(inter_buf_lenth >= (frame_header.frame_size + sizeof(IVF_frame_header)) );
            inter_buf_lenth -= frame_header.frame_size + sizeof(IVF_frame_header);
            memmove(inter_buf, inter_buf + frame_header.frame_size + sizeof(IVF_frame_header), inter_buf_lenth);

            inter_buf_lenth += Read(inter_buf, (inter_buf_size - inter_buf_lenth));

            frame_header = fr_hdr_next;
        }

        return MFX_ERR_NONE;
    }

};

typedef void (*callback)(tsExtBufType<mfxVideoParam>*);
void set_fourcc_params(tsExtBufType<mfxVideoParam>* p)
{
    set_chromaformat(p->mfx.FrameInfo);

    const mfxU32& FourCC = p->mfx.FrameInfo.FourCC;
    mfxU16& BitDepthChroma = p->mfx.FrameInfo.BitDepthChroma;
    mfxU16& BitDepthLuma = p->mfx.FrameInfo.BitDepthLuma;
    switch( FourCC ) {
        case MFX_FOURCC_P010      :
        case MFX_FOURCC_P210      :
        case MFX_FOURCC_Y210      :
        case MFX_FOURCC_Y410      :
            BitDepthLuma = BitDepthChroma = 10;
            break;
        default:
            break;
    }
}

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

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_VP9) { }
    ~TestSuite() {}

    int RunTest_DecodeDecodeHeader(unsigned int id);
    int RunTest_DecodeQuery(unsigned int id);
    int RunTest_DecodeQueryIOSurf(unsigned int id);
    int RunTest_DecodeInit(unsigned int id);
    int RunTest_DecodeDecodeFrameAsync(unsigned int id);
    int RunTest_DecodeGetDecodeStat(unsigned int id);
    int RunTest_DecodeClose(unsigned int id);

    void AllocOpaque();
    void DecodeFramesNoSync();
    void ReadForemanCifStream();

    static const unsigned int n_cases_query;
    static const unsigned int n_cases_init;
    static const unsigned int n_cases_close;
    static const unsigned int n_cases_getdecodestat;
    static const unsigned int n_cases_queryiosurf;

private:
    std::unique_ptr<tsBitstreamProcessor> m_bs_reader;

    enum
    {
        NULL_SESSION = 1,
        NULL_PTR,
        NOT_INITIALIZED,
        NOT_SYNCHRONIZED,
        NO_EXT_ALLOCATOR,
        PLUGIN_NOT_LOADED,
        CLOSE_BY_MFXCLOSE,
        MULTIPLE_INIT,
        QUERY_MODE1, //see manual
        VIDEO_MEMORY  //otherwise System by default
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        mfxU32 n_frames;
    };

    struct f_pair
    {
        const  tsStruct::Field* f;
        mfxU64 v;
        bool   isInvalid; //if true then check that this field was zero-ed by Query function
        mfxU8  stage;
    };
    struct tc_struct1
    {
        mfxStatus sts;
        mfxU32 mode;
        f_pair set_par[MAX_NPARS];
        callback func;
    };


    static const tc_struct1 test_case_init[];
    static const tc_struct1 test_case_query[];
    static const tc_struct test_case_close[];
    static const tc_struct test_case_getdecodestat[];
    static const tc_struct1 test_case_queryiosurf[];

public:
    template <typename TB>
    friend void CheckParams(tsExtBufType<TB>& par, const TestSuite::f_pair (&pairs)[MAX_NPARS]);
};

template <typename TB>
void CheckParams(tsExtBufType<TB>& par, const TestSuite::f_pair (&pairs)[MAX_NPARS])
{
    const size_t length = sizeof(pairs)/sizeof(pairs[0]);
    for(size_t i(0); i < length; ++i)
    {
        if(pairs[i].f)
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

const TestSuite::tc_struct1 TestSuite::test_case_query[] =
{
    {/*00*/ MFX_ERR_NONE},
    {/*01*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION},
    {/*02*/ MFX_ERR_NULL_PTR, NULL_PTR},
    {/*03*/ MFX_ERR_NONE, MFX_ERR_NONE},
    {/*04*/ MFX_ERR_UNSUPPORTED, PLUGIN_NOT_LOADED},
    {/*05*/ MFX_ERR_NONE, NO_EXT_ALLOCATOR},
    {/*06*/ MFX_ERR_NONE, NO_EXT_ALLOCATOR, {IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}},

    {/*07*/ MFX_ERR_NONE, 0, {IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*08*/ MFX_ERR_NONE, 0, {{Width, 1920}, {Height, 1080}}},
    {/*09*/ MFX_ERR_NONE, 0, {{Width, 3840}, {Height, 2160}}},
    {/*10*/ MFX_ERR_NONE, 0, {{Width, 0}, {Height, 0}}},
    {/*11*/ MFX_ERR_UNSUPPORTED, 0, {{CropX, 65535, 1}, {CropY, 2160, 1}, {CropW, 23}, {CropH, 15}}},

    {/*12*/ MFX_ERR_NONE, 0, {PicStruct, MFX_PICSTRUCT_PROGRESSIVE}},
    {/*13*/ MFX_ERR_UNSUPPORTED, 0, {PicStruct, MFX_PICSTRUCT_FIELD_TFF, 1}},
    {/*14*/ MFX_ERR_UNSUPPORTED, 0, {PicStruct, MFX_PICSTRUCT_FIELD_BFF, 1}},
    {/*15*/ MFX_ERR_UNSUPPORTED, 0, {PicStruct, MFX_PICSTRUCT_FRAME_TRIPLING, 1}},
    {/*16*/ MFX_ERR_NONE, 0, {{AspectRatioW, 0}, {AspectRatioH, 0}}},
    {/*17*/ MFX_ERR_NONE, 0, {{AspectRatioW, 16}, {AspectRatioH, 9}}},
    {/*18*/ MFX_ERR_UNSUPPORTED, 0, {{AspectRatioW, 0}, {AspectRatioH, 15}}},
    {/*19*/ MFX_ERR_NONE, 0, {{FrameRateExtN, 0}, {FrameRateExtD, 0}}},
    {/*20*/ MFX_ERR_NONE, 0, {{FrameRateExtN, 30}, {FrameRateExtD, 1}}},
    {/*21*/ MFX_ERR_UNSUPPORTED, 0, {{FrameRateExtN, 100, 1}, {FrameRateExtD, 0}}},

    {/*22*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_NV12}, {BitDepthLuma, 0}}, set_fourcc_params},
    {/*23*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_NV12}, {BitDepthLuma, 8}}, set_fourcc_params},
    {/*24*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_AYUV} }, set_fourcc_params},
    {/*25*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_P010} }, set_fourcc_params},
    {/*26*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_Y410} }, set_fourcc_params},

    {/*27*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_Y210, 1} }, set_fourcc_params},
    {/*28*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_UYVY, 1} }, set_fourcc_params},
    {/*29*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_RGB4, 1} }, set_fourcc_params},
    {/*30*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_YUY2, 1} }, set_fourcc_params},
    {/*31*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_NV16, 1} }, set_fourcc_params},
    {/*32*/ MFX_ERR_UNSUPPORTED, 0, {{FourCC, MFX_FOURCC_ARGB16, 1} }, set_fourcc_params},

    {/*33*/ MFX_ERR_UNSUPPORTED, 0, {{&tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_PAVP} }, },
    {/*34*/ MFX_ERR_UNSUPPORTED, 0, {{&tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_GPUCP_PAVP} }, },

};
const unsigned int TestSuite::n_cases_query = sizeof(TestSuite::test_case_query)/sizeof(TestSuite::test_case_query[0]);

int TestSuite::RunTest_DecodeQuery(unsigned int id)
{
    TS_START;
    const tc_struct1& tc = test_case_init[id];

    MFXInit();
    if(m_uid && PLUGIN_NOT_LOADED != tc.mode)
        Load();

    const mfxSession m_session_tmp = m_session;
    if (NULL_SESSION == tc.mode)
        m_session = 0;

    tsStruct::SetPars(m_par, tc);
    if(tc.func)
        tc.func(&m_par);

    if(NO_EXT_ALLOCATOR != tc.mode)
    {
        bool isSW = !(!!(m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY || m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY));
        UseDefaultAllocator(isSW);
        SetFrameAllocator(m_session, GetAllocator());

        if (!isSW && !m_is_handle_set)
        {
            mfxHDL hdl;
            mfxHandleType type;
            GetAllocator()->get_hdl(type, hdl);
            SetHandle(m_session, type, hdl);
        }
    }

    AllocOpaque();

    if(NULL_PTR == tc.mode)
    {
        g_tsStatus.expect(tc.sts);
        Query(m_session, m_pPar, 0);

    }
    else if(QUERY_MODE1 == tc.mode)
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
        const tsExtBufType<mfxVideoParam> tmpPar = m_par;
        tsExtBufType<mfxVideoParam> par_out = tmpPar;

        //1. Query(p_in, p_out)
        g_tsStatus.expect(tc.sts);
        Query(m_session, m_pPar, &par_out);
        EXPECT_EQ(tmpPar, m_par) << "ERROR: Decoder must not change input structure in DecodeQuery()!\n";
        CheckParams(par_out, tc.set_par);

        //2. Query(p_in, p_in)
        g_tsStatus.expect(tc.sts);
        Query(m_session, m_pPar, m_pPar);
        CheckParams(m_par, tc.set_par);
    }

    if (tc.mode == NULL_SESSION)
        m_session = m_session_tmp;

    g_tsStatus.disable_next_check();
    Close();

    TS_END;
    return 0;
}

const TestSuite::tc_struct1 TestSuite::test_case_init[] =
{
    {/*00*/ MFX_ERR_NONE},
    {/*01*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION},
    {/*02*/ MFX_ERR_NULL_PTR, NULL_PTR},
    {/*03*/ MFX_ERR_UNDEFINED_BEHAVIOR, MULTIPLE_INIT},
    {/*04*/ MFX_ERR_INVALID_VIDEO_PARAM, PLUGIN_NOT_LOADED},
    {/*05*/ MFX_ERR_NONE, NO_EXT_ALLOCATOR},
    {/*06*/ MFX_ERR_INVALID_VIDEO_PARAM, NO_EXT_ALLOCATOR, {IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}},

    {/*07*/ MFX_ERR_NONE, 0, {IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*08*/ MFX_ERR_NONE, 0, {{Width, 1920}, {Height, 1080}}},
    {/*09*/ MFX_ERR_NONE, 0, {{Width, 3840}, {Height, 2160}}},
    {/*10*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{Width, 0}, {Height, 0}}},
    {/*11*/ MFX_ERR_NONE, 0, {{CropX, 65535}, {CropY, 2160}, {CropW, 23}, {CropH, 15}}}, //should be ignored during init
    {/*12*/ MFX_ERR_NONE, 0, {PicStruct, MFX_PICSTRUCT_FRAME_TRIPLING}}, //should be ignored during init
    {/*13*/ MFX_ERR_NONE, 0, {{AspectRatioW, 0}, {AspectRatioH, 0}}},
    {/*14*/ MFX_ERR_NONE, 0, {{AspectRatioW, 16}, {AspectRatioH, 9}}},
    {/*15*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{AspectRatioW, 0}, {AspectRatioH, 15}}},
    {/*16*/ MFX_ERR_NONE, 0, {{FrameRateExtN, 0}, {FrameRateExtD, 0}}},
    {/*17*/ MFX_ERR_NONE, 0, {{FrameRateExtN, 30}, {FrameRateExtD, 1}}},
    {/*18*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FrameRateExtN, 100}, {FrameRateExtD, 0}}},

    {/*19*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_NV12}, {BitDepthLuma, 0}}, set_fourcc_params},
    {/*20*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_NV12}, {BitDepthLuma, 8}}, set_fourcc_params},
    {/*21*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_AYUV} }, set_fourcc_params},
    {/*22*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_P010} }, set_fourcc_params},
    {/*23*/ MFX_ERR_NONE, 0, {{FourCC, MFX_FOURCC_Y410} }, set_fourcc_params},

    {/*24*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_Y210} }, set_fourcc_params},
    {/*25*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_UYVY} }, set_fourcc_params},
    {/*26*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{FourCC, MFX_FOURCC_RGB4} }, set_fourcc_params},

    {/*27*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_GPUCP_AES128_CTR} }, },
    {/*28*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_WIDEVINE_GOOGLE_DASH} }, },

};
const unsigned int TestSuite::n_cases_init = sizeof(TestSuite::test_case_init)/sizeof(TestSuite::test_case_init[0]);

int TestSuite::RunTest_DecodeInit(unsigned int id)
{
    TS_START;
    const tc_struct1& tc = test_case_init[id];

    MFXInit();
    if(m_uid && PLUGIN_NOT_LOADED != tc.mode)
        Load();

    const mfxSession m_session_tmp = m_session;
    if (NULL_SESSION == tc.mode)
        m_session = 0;
    else if(NULL_PTR == tc.mode)
        m_pPar = 0;

    tsStruct::SetPars(m_par, tc);
    if(tc.func)
        tc.func(&m_par);

    if(NO_EXT_ALLOCATOR != tc.mode)
    {
        bool isSW = !(!!(m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY || m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY));
        UseDefaultAllocator(isSW);
        SetFrameAllocator(m_session, GetAllocator());

        if (!isSW && !m_is_handle_set)
        {
            mfxHDL hdl;
            mfxHandleType type;
            GetAllocator()->get_hdl(type, hdl);
            SetHandle(m_session, type, hdl);
        }
    }

    AllocOpaque();

    if (MULTIPLE_INIT == tc.mode)
        Init(m_session, m_pPar);

    const tsExtBufType<mfxVideoParam> tmpPar = m_par;
    g_tsStatus.expect(tc.sts);
    Init(m_session, m_pPar);
    EXPECT_EQ(tmpPar, m_par) << "ERROR: Decoder must not change input structure in DecodeInit()!\n";

    if (tc.mode == NULL_SESSION)
        m_session = m_session_tmp;

    g_tsStatus.disable_next_check();
    Close();

    TS_END;
    return 0;
}

const TestSuite::tc_struct TestSuite::test_case_getdecodestat[] =
{
    {/*00*/ MFX_ERR_NONE, 0, 1},
    {/*01*/ MFX_ERR_NONE, 0, 2},
    {/*02*/ MFX_ERR_NONE, 0, 5},
    {/*03*/ MFX_ERR_NONE, 0, 30},
    {/*04*/ MFX_ERR_NONE, VIDEO_MEMORY, 30},
    {/*05*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION},
    {/*06*/ MFX_ERR_NULL_PTR, NULL_PTR},
    {/*07*/ MFX_ERR_NOT_INITIALIZED, NOT_INITIALIZED},
    {/*08*/ MFX_ERR_NONE, NOT_SYNCHRONIZED},
};
const unsigned int TestSuite::n_cases_getdecodestat = sizeof(TestSuite::test_case_getdecodestat)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest_DecodeGetDecodeStat(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case_getdecodestat[id];

    MFXInit();
    if(m_uid)
        Load();

    if(VIDEO_MEMORY == tc.mode)
    {
        ;//todo
    }

    ReadForemanCifStream();

    const mfxSession m_session_tmp = m_session;
    if (NULL_SESSION == tc.mode)
        m_session = 0;
    else if(NULL_PTR == tc.mode)
        m_pStat = 0;
    else if (NOT_INITIALIZED == tc.mode)
    {
        DecodeHeader();
        m_par.mfx.FrameInfo.Height = -10;

        g_tsStatus.disable_next_check();
        Init(m_session, m_pPar);
    }
    else
    {
        DecodeHeader();
        Init();
    }

    if(NOT_SYNCHRONIZED == tc.mode)
    {
        DecodeFramesNoSync();
    }
    else
    {
        DecodeFrames(tc.n_frames);
    }

    g_tsStatus.expect(tc.sts);
    GetDecodeStat();

    if(tc.n_frames)
        EXPECT_EQ(tc.n_frames, m_stat.NumFrame) << "ERROR: GetDecodeStat function reported wrong decoded frames count (NumFrame)!\n";

    bool has_cached = false;
    for(std::map<mfxSyncPoint,mfxFrameSurface1*>::iterator it = m_surf_out.begin(); it != m_surf_out.end(); ++it)
    {
        if((*it).second->Data.Locked)
        {
            has_cached = true;
            break;
        }
    }
    if(has_cached)
        EXPECT_NE(0, m_stat.NumCachedFrame) << "ERROR: decoder has locked some frames, but reported NumCahedFrame != 0\n";


    if (tc.mode == NULL_SESSION)
        m_session = m_session_tmp;

    g_tsStatus.disable_next_check();
    Close();

    TS_END;
    return 0;
}

const TestSuite::tc_struct TestSuite::test_case_close[] =
{
    {/*00*/ MFX_ERR_NONE, 0},
    {/*01*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION},
    {/*02*/ MFX_ERR_NOT_INITIALIZED, NOT_INITIALIZED},
    {/*03*/ MFX_ERR_NONE, NOT_SYNCHRONIZED},
    {/*04*/ MFX_ERR_NONE, CLOSE_BY_MFXCLOSE},
};
const unsigned int TestSuite::n_cases_close = sizeof(TestSuite::test_case_close)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest_DecodeClose(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case_close[id];

    MFXInit();
    if(m_uid)
        Load();

    ReadForemanCifStream();

    const mfxSession m_session_tmp = m_session;
    if (NULL_SESSION == tc.mode)
        m_session = 0;
    else if (NOT_INITIALIZED == tc.mode)
    {
        DecodeHeader();
        m_par.mfx.FrameInfo.Height = -10;

        g_tsStatus.disable_next_check();
        Init(m_session, m_pPar);
    }
    else
    {
        DecodeHeader();
        Init();
    }

    if(NOT_SYNCHRONIZED == tc.mode)
    {
        DecodeFramesNoSync();
    }

    if (CLOSE_BY_MFXCLOSE != tc.mode)
    {
        g_tsStatus.expect(tc.sts);
        Close();
    }
    else
    {
        MFXClose();
        m_initialized = false;
    }

    if (tc.mode == NULL_SESSION)
        m_session = m_session_tmp;

    TS_END;
    return 0;
}

void TestSuite::DecodeFramesNoSync()
{
    mfxU32 submitted = 0;
    const mfxU32 async = TS_MAX(1, m_par.AsyncDepth);

    while(submitted < async)
    {
        mfxStatus res = DecodeFrameAsync();

        if(MFX_ERR_MORE_DATA == res)
        {
            tsBitstreamReader* reader = dynamic_cast<tsBitstreamReader*>(m_bs_processor);
            if(reader && reader->m_eos)
            {
                reader->SeekToStart();
            }
            continue;
        }
        if(MFX_ERR_MORE_SURFACE == res || (res > 0 && *m_pSyncPoint == NULL))
        {
            continue;
        }
        if(MFX_ERR_NONE == res)
        {
            ++submitted;
        }
        if(res < 0) g_tsStatus.check();
    }

    //free surfaces locked by test
    for(std::map<mfxSyncPoint,mfxFrameSurface1*>::iterator it = m_surf_out.begin(); it != m_surf_out.end(); ++it)
    {
        (*it).second->Data.Locked --;
    }
}

void TestSuite::AllocOpaque()
{
    if(m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        //AllocSurfaces();
        m_par.AddExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION, sizeof(mfxExtOpaqueSurfaceAlloc));
        mfxExtOpaqueSurfaceAlloc *osa = (mfxExtOpaqueSurfaceAlloc*)m_par.GetExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

        mfxFrameAllocRequest request = {};
        MFXVideoDECODE_QueryIOSurf(m_session, m_pPar, &request);

        tsSurfacePool::AllocOpaque(request, *osa);
    }
}

void TestSuite::ReadForemanCifStream()
{
    std::string stream = "forBehaviorTest/foreman_cif.ivf";
    const char* sname = g_tsStreamPool.Get(stream);
    g_tsStreamPool.Reg();
    m_bs_reader.reset(new tsIvfReader(sname, 1024*1024) );
    m_bs_processor = m_bs_reader.get();

    m_pBitstream = m_bs_processor->ProcessBitstream(m_bitstream);
}

TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_init, RunTest_DecodeInit, n_cases_init);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_query, RunTest_DecodeQuery, n_cases_query);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_close, RunTest_DecodeClose, n_cases_close);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_getdecodestat, RunTest_DecodeGetDecodeStat, n_cases_getdecodestat);

}
