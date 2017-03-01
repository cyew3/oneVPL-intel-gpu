/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016-2017 Intel Corporation. All Rights Reserved.
//
*/

#include "ts_decoder.h"
#include "ts_struct.h"
#include "ts_ext_buffers.h"

#include "mfx_ext_buffers.h"

#include <vector>

namespace decoders_gpu_hang
{

class decoder
    : public tsVideoDecoder
{

    mfxU32                     m_frames_submitted;
    mfxU32                     m_frames_synced;
    mfxU32                     m_frame_to_hang;
    bool                       m_check_sync_operation;

    tsExtBufType<mfxFrameData> m_trigger;
    std::vector<mfxSyncPoint>  m_sync_points;

public:

    decoder(mfxU32 codec, mfxU32 frame_to_hang, bool check_sync_operation)
        : tsVideoDecoder(codec)
        , m_frames_submitted(0)
        , m_frames_synced(0)
        , m_frame_to_hang(frame_to_hang)
        , m_check_sync_operation(check_sync_operation)
    {
        m_trigger.AddExtBuffer(MFX_EXTBUFF_GPU_HANG, sizeof(mfxExtIntGPUHang));
    }

    void init()
    {
        if (!m_loaded && m_uid)
            Load();

        SetFrameAllocator(m_session, m_pVAHandle);
        SetAllocator(m_pVAHandle, true);
        AllocSurfaces();
        Init(m_session, m_pPar);
    }

    void run()
    {
        mfxU32 submitted = 0;
        for (;;)
        {
            mfxStatus sts = DecodeFrameAsync();
            if (m_frames_submitted > m_frame_to_hang && sts == MFX_ERR_GPU_HANG)
            {
                // Treat test passed if hang reported before synchronization of triggered surface (and after triggering of course)
                // Don't treat test failed if no reporting at this stage

                g_tsStatus.expect(MFX_ERR_NONE); // overwrite expected status MFX_ERR_GPU_HANG for components closing statuses
                throw tsOK;
            }
            else if(m_frames_synced > m_frame_to_hang)
            {
                // Fail if hang is not reported on synchronization of triggered surface
                g_tsStatus.expect(MFX_ERR_GPU_HANG);
                g_tsStatus.check();
                g_tsStatus.expect(MFX_ERR_NONE); // overwrite expected status MFX_ERR_GPU_HANG for components closing statuses
                throw tsOK;
            }

            if(MFX_ERR_MORE_DATA == sts)
            {
                if(m_pBitstream)
                    continue;
                else
                    break; // no retrieval of cached frames needed in test
            }

            if(sts != MFX_ERR_MORE_SURFACE && sts < MFX_ERR_NONE)
                 g_tsStatus.check(); // test will finish here, not expected status

            ++m_frames_submitted;
            if (sts != MFX_ERR_NONE || *m_pSyncPoint == nullptr)
                continue;

            m_sync_points.push_back(*m_pSyncPoint);

            if(++submitted >= m_pPar->AsyncDepth)
            {
                sync();
                submitted = 0;
            }
        }

        g_tsLog << m_frames_synced << " FRAMES DECODED\n";
        ADD_FAILURE() << "GPU hang not reported";
        throw tsFAIL;
    }

private:

    mfxStatus DecodeFrameAsync()
    {
        if (m_default)
        {
            SetPar4_DecodeFrameAsync();
        }

        mfxStatus sts = DecodeFrameAsync(m_session, m_pBitstream, m_pSurf, &m_pSurfOut, m_pSyncPoint);
        if (sts == MFX_ERR_NONE || (sts == MFX_WRN_VIDEO_PARAM_CHANGED && *m_pSyncPoint != NULL))
        {
            m_surf_out.insert(std::make_pair(*m_pSyncPoint, m_pSurfOut));
            if (m_pSurfOut)
            {
                m_pSurfOut->Data.Locked ++;
            }
        }

        return sts;
    }

    mfxStatus DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
    {
        if (m_frames_submitted == m_frame_to_hang)
        {
            surface_work->Data.NumExtParam = m_trigger.NumExtParam;
            surface_work->Data.ExtParam    = m_trigger.ExtParam;

            g_tsLog << "Frame #" << m_frames_submitted << " - Fire GPU hang trigger\n";
        }

        mfxStatus sts =
            tsVideoDecoder::DecodeFrameAsync(session, bs, surface_work, surface_out, syncp);

        if (m_frames_submitted == m_frame_to_hang)
        {
            //clear GPU hang trigger to avoid subsequently triggers
            surface_work->Data.NumExtParam = 0;
            surface_work->Data.ExtParam    = 0;
        }

        return sts;
    }

private:

    void sync()
    {
        while (!m_sync_points.empty())
        {
            g_tsStatus.disable_next_check(); // check statuses manually below
            auto status = SyncOperation(m_sync_points.back());

            if (m_check_sync_operation)
            {
                if (status == MFX_ERR_GPU_HANG)
                {
                    // due to surfaces caching (e.g. in MPEG2d) GPU hang may be reported for
                    // submitted and not yet synced frames which came to decoder before
                    // triggering GPU hang
                    if (m_frames_submitted >= m_frame_to_hang)
                        throw tsOK;
                    else
                    {
                        assert(m_frames_synced < m_frames_submitted);
                        g_tsStatus.expect(MFX_ERR_NONE);
                        g_tsStatus.check(status);
                    }
                }
                else if (status == MFX_ERR_NONE)
                {
                    if (m_frames_synced >= m_frame_to_hang)
                    {
                        g_tsStatus.expect(MFX_ERR_GPU_HANG);
                        g_tsStatus.check(status);
                    }
                }
                else
                {
                    // unexpected failure
                    g_tsStatus.expect(MFX_ERR_NONE);
                    g_tsStatus.check(status);
                }
            }

            m_sync_points.pop_back();
            ++m_frames_synced;
        }
    }
};

template <int>
class TestSuite
{

public:

    static
    int RunTest(unsigned int id);

public:

    static unsigned int const   n_cases;

    struct tc_struct
    {
        mfxU32      codec;
        const char* name;

        mfxU16      async;
        mfxU32      frame_to_hang;
        bool        call_sync_operation;
    };

    static const tc_struct test_case[];
};

template <int N>
unsigned int const TestSuite<N>::n_cases =
    sizeof(TestSuite<N>::test_case) / sizeof(TestSuite<N>::test_case[0]);

template <int N>
int TestSuite<N>::RunTest(unsigned int id)
{
    TS_START;

    tc_struct const& tc = test_case[id];

    char const* name = g_tsStreamPool.Get(tc.name);
    g_tsStreamPool.Reg();

    //-----------------------------------------//
    bool check_sync_op = tc.call_sync_operation;
    tsSurfaceProcessor* p = 0;

    //-----------------------------------------//

    decoder dec(tc.codec, tc.frame_to_hang, check_sync_op);

    dec.MFXInit();
    dec.m_pPar->IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    dec.m_pPar->AsyncDepth = tc.async;

    tsBitstreamReader reader(name, 100000);
    dec.m_bs_processor = &reader;

    dec.m_surf_processor = p;

    dec.init();
    dec.run();

    TS_END;

    return 0;
}

enum
{
    AVC_TEST_SUITE  = 0,
    HEVC_TEST_SUITE,
    MPEG2_TEST_SUITE,
};

template <>
TestSuite<AVC_TEST_SUITE>::tc_struct const TestSuite<AVC_TEST_SUITE>::test_case[] =
{
    /* 0 - AVC  .DecodeFrameAsync */ { MFX_CODEC_AVC, "conformance/h264/bluesky.h264", 1, 5, false },
    /* 1 - AVC  .DecodeFrameAsync */ { MFX_CODEC_AVC, "conformance/h264/bluesky.h264", 1, 5, true  }
};

template <>
TestSuite<HEVC_TEST_SUITE>::tc_struct const TestSuite<HEVC_TEST_SUITE>::test_case[] =
{
    /* 0 - HEVC .DecodeFrameAsync */ { MFX_CODEC_HEVC, "conformance/hevc/itu/WP_A_Toshiba_3.bit", 1, 5, false },
    /* 1 - HEVC .DecodeFrameAsync */ { MFX_CODEC_HEVC, "conformance/hevc/itu/WP_A_Toshiba_3.bit", 1, 5, true }
};

template <>
TestSuite<MPEG2_TEST_SUITE>::tc_struct const TestSuite<MPEG2_TEST_SUITE>::test_case[] =
{
    /* 0 - MPEG2 .DecodeFrameAsync */ { MFX_CODEC_MPEG2, "conformance/mpeg2/ibm-bw.BITS", 1, 5, false },
    /* 1 - MPEG2 .DecodeFrameAsync */ { MFX_CODEC_MPEG2, "conformance/mpeg2/ibm-bw.BITS", 1, 5, true }
};

TS_REG_TEST_SUITE(avcd_gpu_hang,   TestSuite<AVC_TEST_SUITE>::RunTest,   TestSuite<AVC_TEST_SUITE>::n_cases);
TS_REG_TEST_SUITE(hevcd_gpu_hang,  TestSuite<HEVC_TEST_SUITE>::RunTest,  TestSuite<HEVC_TEST_SUITE>::n_cases);
TS_REG_TEST_SUITE(mpeg2d_gpu_hang, TestSuite<MPEG2_TEST_SUITE>::RunTest, TestSuite<MPEG2_TEST_SUITE>::n_cases);

}
