/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_struct.h"
#include "ts_utils.h"

#ifdef _LOG_TO_FILE
#include <fstream>
#endif // _LOG_TO_FILE

#define TEST_NAME jpegd_res_change

#define STOP_IF_ANY            1
#define STOP_IF_ALL            2
#define PSNR_STOP_CRITERIA     STOP_IF_ANY
#define PSNR_THRESHOLD         55.0

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
    TestSuite() : tsVideoDecoder(MFX_CODEC_JPEG){}
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
    {/* 0*/ {"bbc_640x480_10.mjpeg", 5}, {"bbc_352x288_10.mjpeg", 5}, {} },
    {/* 1*/ {"bbc_352x288_10.mjpeg"},    {"bbc_640x480_10.mjpeg", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 2*/ {"bbc_640x480_10.mjpeg"},    {"bbc_352x288_10.mjpeg"},    {"bbc_704x576_10.mjpeg", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM} },
    {/* 3*/ {"bbc_704x576_10.mjpeg", 5}, {"bbc_640x480_10.mjpeg", 5}, {"bbc_352x288_10.mjpeg", 5} },
    {/* 4*/ {"bbc_704x576_10.mjpeg", 5}, {"bbc_352x288_10.mjpeg", 5}, {"bbc_640x480_10.mjpeg", 5} },
    {/* 5*/ {"bbc_704x576_10.mjpeg", 5}, {"bbc_640x480_10.mjpeg", 5}, {"bbc_704x576_10.mjpeg", 5} },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct[max_num_ctrl]);

const char* stream_path = "forBehaviorTest/dif_resol/";

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

        double Y_MSE = 0.0, UV_MSE = 0.0, A_MSE = 0.0;
        double Y_PSNR = 0.0, UV_PSNR = 0.0, A_PSNR = 0.0;
        size_t i = 0;
        IppiSize size;
        size.width = std::min(ref_surf->Info.CropW, s.Info.CropW);
        size.height = std::min(ref_surf->Info.CropH, s.Info.CropH);

        Y_MSE = CalcMSE(ref_ptr, ref_pitch, s_ptr, s_pitch, size);
        Y_PSNR = MSEToPSNR(Y_MSE, 255.0);

#if PSNR_STOP_CRITERIA == STOP_IF_ANY
        if (Y_PSNR < PSNR_THRESHOLD)
            YDifferes = true;
#endif

#ifdef _LOG_TO_FILE
        FILE * decoded_surface;
        FILE * ref_decoded_surface;
        decoded_surface = fopen("decoded_surface.yuv", "ab");
        ref_decoded_surface = fopen("ref_decoded_surface.yuv", "ab");

        for (i = 0; i < size.height; ++i)
        {
            fwrite(s_ptr + i * s_pitch, sizeof(mfxU8), size.width, decoded_surface);
            fwrite(ref_ptr + i * ref_pitch, sizeof(mfxU8), size.width, ref_decoded_surface);
        }
#endif // _LOG_TO_FILE

        size.height >>= 1;
        s_ptr = s.Data.UV + s.Info.CropX + (s.Info.CropY >> 1) * s_pitch;
        ref_ptr = ref_surf->Data.UV + ref_surf->Info.CropX + (ref_surf->Info.CropY >> 1) * ref_pitch;

        UV_MSE = CalcMSE(ref_ptr, ref_pitch, s_ptr, s_pitch, size);
        UV_PSNR = MSEToPSNR(UV_MSE, 255.0);

#if PSNR_STOP_CRITERIA == STOP_IF_ANY
        if (UV_PSNR < PSNR_THRESHOLD)
            UVDifferes = true;
#endif

        A_MSE = (4.0 * Y_MSE + UV_MSE) / 6.0;
        A_PSNR = MSEToPSNR(A_MSE, 255.0);

#if PSNR_STOP_CRITERIA == STOP_IF_ALL
        if (A_PSNR < PSNR_THRESHOLD) {
            YDifferes = true;
            UVDifferes = true;
        }
#endif

#ifdef _LOG_TO_FILE
        for (i = 0; i < size.height; ++i)
        {
            fwrite(s_ptr + i * s_pitch, sizeof(mfxU8), size.width, decoded_surface);
            fwrite(ref_ptr + i * ref_pitch, sizeof(mfxU8), size.width, ref_decoded_surface);
        }

        fclose(decoded_surface);
        fclose(ref_decoded_surface);
#endif // _LOG_TO_FILE

        /* https://jira01.devtools.intel.com/browse/MQ-507 */
        EXPECT_FALSE(YDifferes || UVDifferes)
            << "ERROR: Surface #"<< frame <<" decoded by resolution-changed session has PSNR which is lower than the threshold value (compared with surface decoded by verification session)!!!\n";

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

        mfxStatus sts = MFXInit(MFX_IMPL_AUTO_ANY, 0, &m_ses);
        EXPECT_EQ_THROW(MFX_ERR_NONE, sts, "ERROR: Failed to Init MFX session of a verification instance of a decoder\n");

        sts = SetVAHandle();
        EXPECT_EQ_THROW(MFX_ERR_NONE, sts, "ERROR: Failed to SetHandle in a verification instance of a decoder\n");

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
