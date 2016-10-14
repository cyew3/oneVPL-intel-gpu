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

#define TEST_NAME vp9d_close
namespace TEST_NAME
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

        inter_buf_lenth += Read(inter_buf+inter_buf_lenth, (inter_buf_size - inter_buf_lenth));
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

            inter_buf_lenth += Read(inter_buf+inter_buf_lenth, (inter_buf_size - inter_buf_lenth));

            frame_header = fr_hdr_next;
        }

        return MFX_ERR_NONE;
    }

};

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
        f_pair set_par[MAX_NPARS];
    };
    static const tc_struct test_case[];      //base cases

    template<mfxU32 fourcc>
    int RunTest_fourcc(const unsigned int id);

    static const unsigned int n_cases;

private:

    enum
    {
        NULL_SESSION = 1,
        NOT_INITIALIZED,
        NOT_SYNCHRONIZED,
        CLOSE_BY_MFXCLOSE,
        FAILED_INIT,
        CLOSED_DECODER,
    };

    enum
    {
        INIT = 1,
    };

    void ReadStream();
    void DecodeFramesNoSync();

    int RunTest(const tc_struct& tc);
    std::unique_ptr<tsBitstreamProcessor> m_bs_reader;
    std::string m_input_stream_name;
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, 0},
    {/*01*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION},
    {/*02*/ MFX_ERR_NOT_INITIALIZED, NOT_INITIALIZED},
    {/*03*/ MFX_ERR_NOT_INITIALIZED, FAILED_INIT},
    {/*04*/ MFX_ERR_NOT_INITIALIZED, CLOSED_DECODER},
    {/*05*/ MFX_ERR_NONE, NOT_SYNCHRONIZED},
    {/*06*/ MFX_ERR_NONE, CLOSE_BY_MFXCLOSE},
};
const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

struct streamDesc
{
    mfxU16 w;
    mfxU16 h;
    const char *name;
};

const streamDesc streams[] = {
    {352,288,"forBehaviorTest/foreman_cif.ivf"                                                      },
    {432,240,"conformance/vp9/SBE/8bit_444/Stress_VP9_FC2p1ss444_432x240_250_extra_stress_2.2.0.vp9"},
    {432,240,"conformance/vp9/SBE/10bit/Stress_VP9_FC2p2b10_432x240_050_intra_stress_1.5.vp9"       },
    {432,240,"conformance/vp9/SBE/10bit_444/Syntax_VP9_FC2p3ss444_432x240_101_inter_basic_2.0.0.vp9"},
};

const streamDesc& getStreamDesc(const mfxU32& fourcc)
{
    switch(fourcc)
    {
    case MFX_FOURCC_NV12: return streams[0];
    case MFX_FOURCC_AYUV: return streams[1];
    case MFX_FOURCC_P010: return streams[2];
    case MFX_FOURCC_Y410: return streams[3];
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
    m_par.mfx.FrameInfo.Width = bsDesc.w;
    m_par.mfx.FrameInfo.Height = bsDesc.h;
    if(MFX_FOURCC_P010 == fourcc || MFX_FOURCC_Y410 == fourcc)
        m_par.mfx.FrameInfo.BitDepthChroma = m_par.mfx.FrameInfo.BitDepthLuma = 10;
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

    tsStruct::SetPars(m_par, tc, INIT);
    
    if(NOT_INITIALIZED == tc.mode)
    {
        ;
    }
    else if (FAILED_INIT == tc.mode)
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

    if(CLOSED_DECODER == tc.mode)
    {
        g_tsStatus.disable_next_check();
        Close();
    }

    if(NOT_SYNCHRONIZED == tc.mode)
    {
        DecodeFramesNoSync();
    }

    //test section
    {
        if (CLOSE_BY_MFXCLOSE != tc.mode)
        {
            mfxSession ses = (NULL_SESSION == tc.mode) ? nullptr : m_session;

            g_tsStatus.expect(tc.sts);
            Close(ses);
        }
        else
        {
            MFXClose();
            m_initialized = false;
        }
    }

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

void TestSuite::ReadStream()
{
    const char* sname = g_tsStreamPool.Get(m_input_stream_name);
    g_tsStreamPool.Reg();
    m_bs_reader.reset(new tsIvfReader(sname, 1024*1024) );
    m_bs_processor = m_bs_reader.get();

    m_pBitstream = m_bs_processor->ProcessBitstream(m_bitstream);
}

TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_8b_420_close,  RunTest_fourcc<MFX_FOURCC_NV12>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_10b_420_close, RunTest_fourcc<MFX_FOURCC_P010>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_8b_444_close,  RunTest_fourcc<MFX_FOURCC_AYUV>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_10b_444_close, RunTest_fourcc<MFX_FOURCC_Y410>, n_cases);

}
#undef TEST_NAME
