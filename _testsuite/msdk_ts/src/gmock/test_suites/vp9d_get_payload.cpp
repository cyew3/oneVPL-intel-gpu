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

namespace vp9d_get_payload
{
    template <unsigned fourcc>
    char const* query_stream(unsigned int, std::integral_constant<unsigned, fourcc>);

    /* 8 bit */
    char const* query_stream(unsigned int id, std::integral_constant<unsigned, MFX_FOURCC_NV12>)
    { return "forBehaviorTest/foreman_cif.ivf"; }

    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_AYUV>)
    { return "conformance/vp9/SBE/8bit_444/Stress_VP9_FC2p1ss444_432x240_250_extra_stress_2.2.0.vp9"; }

    /* 10 bit */
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_P010>)
    { return "conformance/vp9/SBE/10bit/Stress_VP9_FC2p2b10_432x240_050_intra_stress_1.5.vp9"; }
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y410>)
    { return "conformance/vp9/SBE/10bit_444/Syntax_VP9_FC2p3ss444_432x240_101_inter_basic_2.0.0.vp9"; }

    /* 12 bit */
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_P016>)
    { return "conformance/vp9/google/vp92-2-20-12bit-yuv420.ivf"; }
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y416>)
    { return "conformance/vp9/google/vp93-2-20-12bit-yuv444.ivf"; }

    template <unsigned fourcc>
    class TestSuite
        : public tsVideoDecoder
        , public tsSurfaceProcessor
    {

    public:

        TestSuite()
            : tsVideoDecoder(MFX_CODEC_VP9)
        { m_surf_processor = this; }

        ~TestSuite()
        {}

        static
        int RunTest(unsigned int id)
        {
            const char* sname =
                g_tsStreamPool.Get(query_stream(id, std::integral_constant<unsigned, fourcc>()));
            g_tsStreamPool.Reg();

            TestSuite suite;
            return suite.RunTest(sname);
        }

        int RunTest(char const* name)
        {
            TS_START;

            tsBitstreamReaderIVF r{name, 1024 * 1024};
            m_bs_processor = &r;

            DecodeFrames(1);

            TS_END;
            return 0;
        }

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            mfxU64 ts;
            mfxPayload payload;

            g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            GetPayload(&ts, &payload);

            return MFX_ERR_NONE;
        }

    };

    TS_REG_TEST_SUITE(vp9d_8b_420_nv12_get_payload,  TestSuite<MFX_FOURCC_NV12>::RunTest, 1);
    TS_REG_TEST_SUITE(vp9d_8b_444_ayuv_get_payload,  TestSuite<MFX_FOURCC_AYUV>::RunTest, 1);

    TS_REG_TEST_SUITE(vp9d_10b_420_p010_get_payload, TestSuite<MFX_FOURCC_P010>::RunTest, 1);
    TS_REG_TEST_SUITE(vp9d_10b_444_y410_get_payload, TestSuite<MFX_FOURCC_Y410>::RunTest, 1);

    TS_REG_TEST_SUITE(vp9d_12b_420_p016_get_payload, TestSuite<MFX_FOURCC_P016>::RunTest, 1);
    TS_REG_TEST_SUITE(vp9d_12b_444_y416_get_payload, TestSuite<MFX_FOURCC_Y416>::RunTest, 1);
}
