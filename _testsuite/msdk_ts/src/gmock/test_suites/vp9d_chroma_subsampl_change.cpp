/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2017 Intel Corporation. All Rights Reserved.
//
*/
#include "ts_decoder.h"
#include "ts_struct.h"

#define TEST_NAME vp9d_chroma_subsampl_change

namespace TEST_NAME
{

#define EXPECT_NE_THROW(expected, actual, message)  \
do {                                                \
    if(expected == actual)                          \
    {                                               \
        EXPECT_NE(expected, actual) << message;     \
        throw tsFAIL;                               \
    }                                               \
} while (0,0)

#define EXPECT_EQ_THROW(expected, actual, message)  \
do {                                                \
    if(expected != actual)                          \
    {                                               \
        EXPECT_EQ(expected, actual) << message;     \
        throw tsFAIL;                               \
    }                                               \
} while (0,0)

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
    TestSuite() : tsVideoDecoder(MFX_CODEC_VP9){}
    ~TestSuite() { }
    static const size_t n_cases_8b;
    static const size_t n_cases_10b;

    template<mfxU16 bit_depth>
    int RunTest_BitDepth(const size_t id);

private:
    static const mfxU32 max_num_ctrl     = 3;

    struct tc_struct
    {
        const char* stream;
        mfxU32 n_frames;
        mfxStatus sts;
    };
    int RunTest(const tc_struct (&tcs)[max_num_ctrl]);

    static const tc_struct test_case_8b[][max_num_ctrl];
    static const tc_struct test_case_10b[][max_num_ctrl];
};

#define MAKE_TEST_TABLE(CHROMA, L, L_N, M, M_N, S, S_N)                                                                    \
const TestSuite::tc_struct TestSuite::test_case_##CHROMA[][max_num_ctrl] =                                                 \
{                                                                                                                          \
    {/* 0*/ {M, 0  },  {S, 0  },                                 {}                                       },               \
    {/* 1*/ {S, 0  },  {M, 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}, {}                                       },               \
    {/* 2*/ {M, M_N},  {S, S_N},                                 {}                                       },               \
    {/* 3*/ {M, 0  },  {S, 0  },                                 {L, 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM} },               \
    {/* 4*/ {L, 0  },  {M, 0  },                                 {S, 0  }                                 },               \
    {/* 5*/ {L, L_N},  {M, M_N},                                 {S, S_N}                                 },               \
    {/* 6*/ {L, 0  },  {S, 0  },                                 {M, 0  }                                 },               \
    {/* 7*/ {L, L_N},  {S, S_N},                                 {M, M_N}                                 },               \
    {/* 8*/ {L, L_N},  {M, M_N},                                 {L, L_N}                                 },               \
};                                                                                                                         \
const size_t TestSuite::n_cases_##CHROMA = sizeof(TestSuite::test_case_##CHROMA)/sizeof(TestSuite::tc_struct[max_num_ctrl]);

MAKE_TEST_TABLE(8b, "conformance/vp9/SBE/8bit/Syntax_VP9_432x240_001_intra_basic_1.3.vp9", 10,
                    "conformance/vp9/SBE/8bit_444/Syntax_VP9_FC2p1ss444_432x240_001_intra_basic_1.4.vp9", 10,
                    "conformance/vp9/SBE/8bit/Syntax_VP9_432x240_001_intra_basic_1.3.vp9", 10)

MAKE_TEST_TABLE(10b, "conformance/vp9/SBE/10bit/Syntax_VP9_FC2p2b10_432x240_001_intra_basic_1.3.vp9", 10,
                     "conformance/vp9/SBE/10bit_444/Syntax_VP9_FC2p3ss444_432x240_001_intra_basic_1.4.vp9", 10,
                     "conformance/vp9/SBE/10bit/Syntax_VP9_FC2p2b10_432x240_001_intra_basic_1.3.vp9", 10)

#undef MAKE_TEST_TABLE

class Verifier : public tsSurfaceProcessor
{
public:
    const mfxU32 CodecId;
    mfxU32 frame;
    Verifier(const char* fname, mfxU32 codec)
        : tsSurfaceProcessor()
        , CodecId(codec)
        , frame(0)
        , m_pVAHandle(0)
        , reader(fname, 100000)
        , surf_pool(0)
        , m_ses(0)
        , m_par()
        , surf_pool_size(0)
    {
        memset(&bs,0,sizeof(bs));

        Init();
    }

    ~Verifier()
    {
        Close();
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        const mfxFrameSurface1* ref_surf = DecodeNextFrame();
        EXPECT_NE_THROW(nullptr, ref_surf, "ERROR: failed to decode next frame by a verification decoder instance\n");

        //compare frames
        EXPECT_EQ(ref_surf->Info.CropH, s.Info.CropH);
        EXPECT_EQ(ref_surf->Info.CropW, s.Info.CropW);
        EXPECT_EQ(ref_surf->Info.AspectRatioH, s.Info.AspectRatioH);
        EXPECT_EQ(ref_surf->Info.AspectRatioW, s.Info.AspectRatioW);

        bool YDifferes = false;
        bool UVDifferes = false;
        EXPECT_EQ(ref_surf->Info.FourCC, s.Info.FourCC);
        const mfxU16& s_pitch = s.Data.Pitch;
        mfxU8* s_ptr = s.Data.Y + s.Info.CropX + s.Info.CropY * s_pitch;
        const mfxU16& ref_pitch = ref_surf->Data.Pitch;
        mfxU8* ref_ptr = ref_surf->Data.Y + ref_surf->Info.CropX + ref_surf->Info.CropY * ref_pitch;

        size_t height = std::min(ref_surf->Info.CropH, s.Info.CropH);
        size_t width  = std::min(ref_surf->Info.CropW, s.Info.CropW);

        if(MFX_FOURCC_P010 == m_par.mfx.FrameInfo.FourCC)
            width *= 2;
        if(MFX_FOURCC_AYUV == m_par.mfx.FrameInfo.FourCC)
            width *= 4;

        for(size_t i(0); i < height; ++i)
        {
            if(memcmp(s_ptr + i * s_pitch, ref_ptr + i * ref_pitch, width))
                YDifferes = true;
        }

        if(MFX_FOURCC_AYUV != m_par.mfx.FrameInfo.FourCC)
        {
            height >>= 1;
            s_ptr = s.Data.UV + s.Info.CropX + (s.Info.CropY >> 1) * s_pitch;
            ref_ptr = ref_surf->Data.UV + ref_surf->Info.CropX + (ref_surf->Info.CropY >> 1) * ref_pitch;
            for(size_t i(0); i < height; ++i)
            {
                if(memcmp(s_ptr + i * s_pitch, ref_ptr + i * ref_pitch, width))
                    UVDifferes = true;
            }
        }

        /* https://jira01.devtools.intel.com/browse/MQ-507 */
        EXPECT_FALSE(YDifferes || UVDifferes)
            << "ERROR: Frame decoded by resolution-changed session is not b2b with frame decoded by verification session!!!\nFrame #" << frame << "\n";

        frame++;
        return MFX_ERR_NONE;
    }

private:
    mfxStatus SetVAHandle()
    {
        mfxStatus sts = MFX_ERR_NONE;
#if (defined(LINUX32) || defined(LINUX64))
        // SetHandle on Linux is always required for HW
        if (g_tsImpl != MFX_IMPL_SOFTWARE)
        {
            mfxHDL hdl;
            mfxHandleType type;
            if (!m_pVAHandle)
            {
                m_pVAHandle = new frame_allocator(
                        frame_allocator::HARDWARE,
                        frame_allocator::ALLOC_MAX,
                        frame_allocator::ENABLE_ALL,
                        frame_allocator::ALLOC_EMPTY);
            }
            m_pVAHandle->get_hdl(type, hdl);
            sts = MFXVideoCORE_SetHandle(m_ses, type, hdl);
        }
#endif
        return sts;
    }

    mfxStatus Init()
    {
        memset(&bs, 0, sizeof(bs));
        reader.ProcessBitstream(bs, 1);

        mfxStatus sts = MFXInit(g_tsImpl, 0, &m_ses);
        EXPECT_EQ_THROW(MFX_ERR_NONE, sts, "ERROR: Failed to Init MFX session of a verification instance of a decoder\n");

        sts = SetVAHandle();
        EXPECT_EQ_THROW(MFX_ERR_NONE, sts, "ERROR: Failed to SetHandle in a verification instance of a decoder\n");

        mfxVideoParam par = {};
        par.mfx.CodecId = CodecId;
        par.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        par.AsyncDepth = 1;

        mfxPluginUID*  uid = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_DECODE, CodecId);
        if(uid)
        {
            sts = MFXVideoUSER_Load(m_ses, uid, 1);
            EXPECT_EQ_THROW(MFX_ERR_NONE, sts, "ERROR: Failed to Load plugin into a verification instance of a decoder\n");
        }

        sts = MFXVideoDECODE_DecodeHeader(m_ses, &bs, &par);
        EXPECT_EQ_THROW(MFX_ERR_NONE, sts, "ERROR: Failed to DecodeHeader in a verification instance of a decoder\n");

        mfxFrameAllocRequest request = {};
        sts = MFXVideoDECODE_QueryIOSurf(m_ses, &par, &request);
        EXPECT_EQ_THROW(MFX_ERR_NONE, sts, "ERROR: Failed to QueryIOSurf in a verification instance of a decoder\n");
        surf_pool_size = request.NumFrameSuggested;

        surf_pool = new mfxFrameSurface1[surf_pool_size];
        memset(surf_pool, 0, sizeof(mfxFrameSurface1) * surf_pool_size);

        for(size_t i(0); i < surf_pool_size; ++i)
        {
            surf_pool[i].Info = par.mfx.FrameInfo;
            if(MFX_FOURCC_NV12 == par.mfx.FrameInfo.FourCC)
            {
                surf_pool[i].Data.Y = new mfxU8[surf_pool->Info.Width * request.Info.Height * 3 / 2];
                surf_pool[i].Data.UV = surf_pool[i].Data.Y + request.Info.Width * request.Info.Height;
                surf_pool[i].Data.Pitch = request.Info.Width;
            }
            else if(MFX_FOURCC_P010 == par.mfx.FrameInfo.FourCC)
            {
                surf_pool[i].Data.Y16 = new mfxU16[surf_pool->Info.Width * request.Info.Height * 3 / 2];
                surf_pool[i].Data.U16 = surf_pool[i].Data.Y16 + request.Info.Width * request.Info.Height;
                surf_pool[i].Data.V16 = surf_pool[i].Data.U16 + 1;
                surf_pool[i].Data.Pitch = request.Info.Width * 2;
            }
            else if(MFX_FOURCC_AYUV == par.mfx.FrameInfo.FourCC)
            {

                surf_pool[i].Data.Y = new mfxU8[surf_pool->Info.Width * request.Info.Height * 4];
                surf_pool[i].Data.U = surf_pool[i].Data.Y + 1;
                surf_pool[i].Data.V = surf_pool[i].Data.Y + 2;
                surf_pool[i].Data.A = surf_pool[i].Data.Y + 3;
                surf_pool[i].Data.Pitch = request.Info.Width * 4;
            }
            else
            {
                g_tsLog << "ERROR: format is unsupported by test.\n";
                throw tsFAIL;
            }
        }

        par.mfx.FrameInfo.CropW = par.mfx.FrameInfo.CropH = 0;
        par.mfx.FrameInfo.AspectRatioH = par.mfx.FrameInfo.AspectRatioW = 0;
        sts = MFXVideoDECODE_Init(m_ses, &par);
        EXPECT_EQ_THROW(MFX_ERR_NONE, sts, "ERROR: Failed to Init verificaton decoder isntance\n");

        bs.DataLength += bs.DataOffset;
        bs.DataOffset = 0;

        m_par = par;

        return MFX_ERR_NONE;
    }
    mfxStatus Close()
    {
        if(m_ses)
        {
            MFXVideoDECODE_Close(m_ses);
            MFXClose(m_ses);
            m_ses = 0;
        }
        if(surf_pool)
        {
            for(size_t i(0); i < surf_pool_size; ++i)
            {
                if(MFX_FOURCC_NV12 == m_par.mfx.FrameInfo.FourCC)
                {
                    if(surf_pool[i].Data.Y)
                    {
                        delete[] surf_pool[i].Data.Y;
                        surf_pool[i].Data.Y = 0;
                    }
                }
                else if(MFX_FOURCC_P010 == m_par.mfx.FrameInfo.FourCC)
                {
                    if(surf_pool[i].Data.Y16)
                    {
                        delete[] surf_pool[i].Data.Y16;
                        surf_pool[i].Data.Y16 = 0;
                    }
                }
                else if(MFX_FOURCC_AYUV == m_par.mfx.FrameInfo.FourCC)
                {
                    if(surf_pool[i].Data.Y)
                    {
                        delete[] surf_pool[i].Data.Y;
                        surf_pool[i].Data.Y = 0;
                    }
                }
            }
            delete[] surf_pool;
            surf_pool = 0;
            surf_pool_size = 0;
        }
        if(m_pVAHandle)
        {
            delete m_pVAHandle;
            m_pVAHandle = 0;
        }
        return MFX_ERR_NONE;
    }

    mfxFrameSurface1* DecodeNextFrame()
    {
        if(!m_ses)
            Init();
        mfxStatus sts = MFX_ERR_NONE;

        mfxFrameSurface1* surf_out = 0;

        while(0 == surf_out)
        {
            mfxSyncPoint syncp;
            mfxFrameSurface1* work_surf = GetFreeSurf();
            mfxBitstream* pbs = (reader.m_eos && bs.DataLength < 4) ? 0 : &bs;
            if(pbs)
                reader.ProcessBitstream(bs, 1);

            EXPECT_NE_THROW(nullptr, work_surf, "ERROR: failed to find free surface in a verification instance of a decoder\n");

            sts = MFXVideoDECODE_DecodeFrameAsync(m_ses, &bs, work_surf, &surf_out, &syncp);

            if(MFX_ERR_MORE_DATA == sts)
            {
                if(!pbs)
                    return 0;
                continue;
            }
            if(MFX_ERR_MORE_SURFACE == sts || sts > 0)
            {
                continue;
            }
            if(MFX_ERR_NONE == sts)
            {
                sts = MFXVideoCORE_SyncOperation(m_ses, syncp, MFX_INFINITE);
                EXPECT_EQ_THROW(MFX_ERR_NONE, sts, "ERROR: Failed to SyncOperation in a verification instance of a decoder\n");

                return surf_out;
            }

            return 0;
        }
        return surf_out;
    }

    mfxFrameSurface1* GetFreeSurf()
    {
        for(size_t i(0); i < surf_pool_size; ++i)
        {
            if(0 == surf_pool[i].Data.Locked)
                return &surf_pool[i];
        }
        return 0;
    }

    frame_allocator*  m_pVAHandle;
    tsIvfReader reader;
    mfxBitstream      bs;
    mfxFrameSurface1* surf_pool;
    mfxSession        m_ses;
    mfxVideoParam     m_par;
    mfxU32            surf_pool_size;
};

template<mfxU16 bit_depth>
int TestSuite::RunTest_BitDepth(const size_t id)
{
    switch (bit_depth)
    {
        case 8:  return RunTest(test_case_8b[id]);
        case 10: return RunTest(test_case_10b[id]);
        default: assert(!"Wrong/unsupported format requested!"); break;
    }
    throw tsFAIL;
}

int TestSuite::RunTest(const tc_struct (&tcs)[max_num_ctrl])
{
    TS_START;
    for (auto& tc : tcs)
        g_tsStreamPool.Get( tc.stream ? tc.stream : std::string(), "");

    g_tsStreamPool.Reg();
    m_use_memid = true;

    for (auto& tc : tcs) 
    {
        if(0 == tc.stream)
            break;

        g_tsLog << "New stream: " << tc.stream << "\n";

        tsIvfReader r(g_tsStreamPool.Get(tc.stream), 100000);
        m_bs_processor = &r;
        
        m_bitstream.DataLength = 0;
        DecodeHeader();

        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.CropH = 0;
        m_par.mfx.FrameInfo.AspectRatioH = m_par.mfx.FrameInfo.AspectRatioW = 0;

        if(m_initialized)
        {
            mfxU32 nSurfPreReset = GetAllocator()->cnt_surf();

            g_tsStatus.disable_next_check();
            Reset();
            if (MFX_ERR_NONE == g_tsStatus.get())
            {
                EXPECT_EQ(nSurfPreReset, GetAllocator()->cnt_surf()) << "ERROR: expected no surface allocation at Reset!\n";
            }
            else if (MFX_ERR_INVALID_VIDEO_PARAM == g_tsStatus.get() ||
                     MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == g_tsStatus.get())
            {
                g_tsLog << "Reset is unavailable, performing decoder Close()-Init() (without session close)\n";

                Close();
                SetPar4_DecodeFrameAsync();
            }

            if(g_tsStatus.get() < 0)
                break;
        }
        else
        {
            SetPar4_DecodeFrameAsync();
        }

        if (tc.n_frames)
        {
            Verifier v(g_tsStreamPool.Get(tc.stream), m_par.mfx.CodecId);
            m_surf_processor = &v;

            DecodeFrames(tc.n_frames);
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_8b_420_444_chroma_subsampl_change, RunTest_BitDepth<8>, n_cases_8b);
TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9d_10b_420_444_chroma_subsampl_change, RunTest_BitDepth<10>, n_cases_10b);

#undef TEST_NAME

}
