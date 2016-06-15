/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
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

    mfxU32                     m_frame;
    mfxU32                     m_frame_to_hang;

    tsExtBufType<mfxFrameData> m_trigger;
    std::vector<mfxSyncPoint>  m_sync_points;

public:

    decoder(mfxU32 codec, mfxU32 frame_to_hang)
        : tsVideoDecoder(codec)
        , m_frame(0)
        , m_frame_to_hang(frame_to_hang)
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
            mfxStatus expected =
                m_frame > m_frame_to_hang ? MFX_ERR_GPU_HANG : MFX_ERR_NONE;
            g_tsStatus.expect(expected);

            mfxStatus sts = DecodeFrameAsync();

            if (sts == MFX_ERR_MORE_DATA ||
                sts == MFX_ERR_MORE_SURFACE)
                continue;

            g_tsStatus.check();
            if (expected == MFX_ERR_GPU_HANG)
                break;

            if (!*m_pSyncPoint)
                continue;

            m_sync_points.push_back(*m_pSyncPoint);
            ++m_frame;

            if(++submitted >= m_pPar->AsyncDepth)
            {
                sync();
                submitted = 0;
            }
        }

        g_tsLog << m_frame << " FRAMES DECODED\n";
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
        if (m_frame == m_frame_to_hang)
        {
            surface_work->Data.NumExtParam = m_trigger.NumExtParam;
            surface_work->Data.ExtParam    = m_trigger.ExtParam;

            g_tsLog << "Frame #" << m_frame << " - Fire GPU hang trigger\n";
        }

        mfxStatus sts =
            tsVideoDecoder::DecodeFrameAsync(session, bs, surface_work, surface_out, syncp);

        if (m_frame == m_frame_to_hang)
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
            SyncOperation(m_sync_points.back());
            m_sync_points.pop_back();
            g_tsStatus.check();
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

    decoder dec(tc.codec, tc.frame_to_hang);
    dec.MFXInit();
    dec.m_pPar->IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    dec.m_pPar->AsyncDepth = tc.async;

    tsBitstreamReader reader(name, 100000);
    dec.m_bs_processor = &reader;

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
    /* 0 - AVC  .DecodeFrameAsync */ { MFX_CODEC_AVC, "conformance/h264/bluesky.h264", 1, 5 },
};

template <>
TestSuite<HEVC_TEST_SUITE>::tc_struct const TestSuite<HEVC_TEST_SUITE>::test_case[] =
{
    /* 0 - HEVC .DecodeFrameAsync */ { MFX_CODEC_HEVC, "conformance/hevc/itu/WP_A_Toshiba_3.bit", 1, 5 }
};

template <>
TestSuite<MPEG2_TEST_SUITE>::tc_struct const TestSuite<MPEG2_TEST_SUITE>::test_case[] =
{
    /* 0 - MPEG2 .DecodeFrameAsync */ { MFX_CODEC_MPEG2, "conformance/mpeg2/ibm-bw.BITS", 1, 5 }
};

TS_REG_TEST_SUITE(avcd_gpu_hang,   TestSuite<AVC_TEST_SUITE>::RunTest,   TestSuite<AVC_TEST_SUITE>::n_cases);
TS_REG_TEST_SUITE(hevcd_gpu_hang,  TestSuite<HEVC_TEST_SUITE>::RunTest,  TestSuite<HEVC_TEST_SUITE>::n_cases);
TS_REG_TEST_SUITE(mpeg2d_gpu_hang, TestSuite<MPEG2_TEST_SUITE>::RunTest, TestSuite<MPEG2_TEST_SUITE>::n_cases);

}
