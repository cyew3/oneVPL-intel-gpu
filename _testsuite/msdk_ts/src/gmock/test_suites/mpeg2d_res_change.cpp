#include "ts_decoder.h"
#include "ts_struct.h"

#define TEST_NAME mpeg2d_res_change

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

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_MPEG2){}
    ~TestSuite() { }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 max_num_ctrl     = 3;

    struct tc_struct
    {
        const char* stream;
        mfxU32 n_frames;
        mfxStatus sts;
    };

    static const tc_struct test_case[][max_num_ctrl];
};

const TestSuite::tc_struct TestSuite::test_case[][max_num_ctrl] = 
{
    {/* 0*/ {"bbc_640x480_10.m2v"},    {"bbc_352x288_10.m2v"}, {} },
    {/* 1*/ {"bbc_352x288_10.m2v"},    {"bbc_640x480_10.m2v", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 2*/ {"bbc_640x480_10.m2v", 5}, {"bbc_352x288_10.m2v", 5}, {} },
    {/* 3*/ {"bbc_640x480_10.m2v"},    {"bbc_352x288_10.m2v"},    {"bbc_704x576_10.m2v", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM} },
    {/* 4*/ {"bbc_704x576_10.m2v"},    {"bbc_640x480_10.m2v"},    {"bbc_352x288_10.m2v"} },
    {/* 5*/ {"bbc_704x576_10.m2v", 5}, {"bbc_640x480_10.m2v", 5}, {"bbc_352x288_10.m2v", 5} },
    {/* 6*/ {"bbc_704x576_10.m2v"},    {"bbc_352x288_10.m2v"},    {"bbc_640x480_10.m2v"} },
    {/* 7*/ {"bbc_704x576_10.m2v", 5}, {"bbc_352x288_10.m2v", 5}, {"bbc_640x480_10.m2v", 5} },
    {/* 8*/ {"bbc_704x576_10.m2v", 5}, {"bbc_640x480_10.m2v", 5}, {"bbc_704x576_10.m2v", 5} },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct[max_num_ctrl]);

const char* stream_path = "forBehaviorTest/dif_resol/";

class Verifier : public tsSurfaceProcessor
{
public:
    const mfxU32 CodecId;
    mfxU32 frame;
    Verifier(const char* fname, mfxU32 codec)
        : reader(fname, 100000),
        CodecId(codec)
    {
        frame          = 0;
        surf_pool      = 0;
        m_ses          = 0;
        surf_pool_size = 0;
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
        for(size_t i(0); i < height; ++i)
        {
            if(memcmp(s_ptr + i * s_pitch, ref_ptr + i * ref_pitch, width))
                YDifferes = true;
        }
        height >>= 1;
        s_ptr = s.Data.UV + s.Info.CropX + (s.Info.CropY >> 1) * s_pitch;
        ref_ptr = ref_surf->Data.UV + ref_surf->Info.CropX + (ref_surf->Info.CropY >> 1) * ref_pitch;
        for(size_t i(0); i < height; ++i)
        {
            if(memcmp(s_ptr + i * s_pitch, ref_ptr + i * ref_pitch, width))
                UVDifferes = true;
        }

        /* https://jira01.devtools.intel.com/browse/MQ-507 */
        EXPECT_FALSE(YDifferes || UVDifferes)
            << "ERROR: Surface #"<< frame <<" decoded by resolution-changed session is not b2b with surface decoded by verification session!!!\n";

        frame++;
        return MFX_ERR_NONE;
    }

private:
    mfxStatus Init()
    {
        memset(&bs, 0, sizeof(bs));
        reader.ProcessBitstream(bs, 1);

        mfxStatus sts = MFXInit(MFX_IMPL_AUTO_ANY, 0, &m_ses);
        EXPECT_EQ_THROW(MFX_ERR_NONE, sts, "ERROR: Failed to Init MFX session of a verification instance of a decoder\n");

        mfxVideoParam par = {};
        par.mfx.CodecId = CodecId;
        par.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        par.AsyncDepth = 1;

        sts = MFXVideoDECODE_DecodeHeader(m_ses, &bs, &par);
        EXPECT_EQ_THROW(MFX_ERR_NONE, sts, "ERROR: Failed to DecodeHeader in a verification instance of a decoder\n");

        mfxFrameAllocRequest request = {};
        sts = MFXVideoDECODE_QueryIOSurf(m_ses, &par, &request);
        EXPECT_EQ_THROW(MFX_ERR_NONE, sts, "ERROR: Failed to QueryIOSurf in a verification instance of a decoder\n");
        surf_pool_size = request.NumFrameSuggested;

        surf_pool = new mfxFrameSurface1[surf_pool_size];
        memset(surf_pool, 0, sizeof(mfxFrameSurface1) * surf_pool_size);
        assert(MFX_FOURCC_NV12 == par.mfx.FrameInfo.FourCC);
        for(size_t i(0); i < surf_pool_size; ++i)
        {
            surf_pool[i].Info = par.mfx.FrameInfo;
            surf_pool[i].Data.Y = new mfxU8[surf_pool->Info.Width * request.Info.Height * 3 / 2];
            surf_pool[i].Data.UV = surf_pool[i].Data.Y + request.Info.Width * request.Info.Height;
            surf_pool[i].Data.Pitch = request.Info.Width;
        }

        par.mfx.FrameInfo.CropW = par.mfx.FrameInfo.CropH = 0;
        par.mfx.FrameInfo.AspectRatioH = par.mfx.FrameInfo.AspectRatioW = 0;
        sts = MFXVideoDECODE_Init(m_ses, &par);
        EXPECT_EQ_THROW(MFX_ERR_NONE, sts, "ERROR: Failed to Init verificaton decoder isntance\n");

        bs.DataLength += bs.DataOffset;
        bs.DataOffset = 0;

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
                if(surf_pool[i].Data.Y)
                {
                    delete[] surf_pool[i].Data.Y;
                    surf_pool[i].Data.Y = 0;
                }
            }
            delete[] surf_pool;
            surf_pool = 0;
            surf_pool_size = 0;
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

    tsBitstreamReader reader;
    mfxBitstream      bs;
    mfxFrameSurface1* surf_pool;
    mfxSession        m_ses;
    mfxU32            surf_pool_size;
};

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    auto& tcs = test_case[id];
    for (auto& tc : tcs)
        g_tsStreamPool.Get( tc.stream ? tc.stream : std::string(), stream_path);

    g_tsStreamPool.Reg();
    m_use_memid = true;

    for (auto& tc : tcs) 
    {
        if(0 == tc.stream)
            break;

        tsBitstreamReader r(g_tsStreamPool.Get(tc.stream), 100000);
        m_bs_processor = &r;
        
        m_bitstream.DataLength = 0;
        DecodeHeader();

        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.CropH = 0;
        m_par.mfx.FrameInfo.AspectRatioH = m_par.mfx.FrameInfo.AspectRatioW = 0;

        if(m_initialized)
        {
            mfxU32 nSurfPreReset = GetAllocator()->cnt_surf();

            g_tsStatus.expect(tc.sts);
            Reset();

            EXPECT_EQ(nSurfPreReset, GetAllocator()->cnt_surf()) << "ERROR: expected no surface allocation at Reset!\n";

            if(g_tsStatus.get() < 0)
                break;
        }
        else
        {
            SetPar4_DecodeFrameAsync();
        }

        Verifier v(g_tsStreamPool.Get(tc.stream), m_par.mfx.CodecId);
        m_surf_processor = &v;

        DecodeFrames(tc.n_frames);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(TEST_NAME);
#undef TEST_NAME

}
