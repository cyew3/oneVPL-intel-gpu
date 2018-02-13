/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include <memory>

#include "ts_decoder.h"
#include "ts_struct.h"

namespace av1d_get_payload
{
    template <unsigned fourcc>
    char const* query_stream(unsigned int, std::integral_constant<unsigned, fourcc>);

    /* 8 bit */
    char const* query_stream(unsigned int id, std::integral_constant<unsigned, MFX_FOURCC_NV12>)
    { return "conformance/av1/avp_rev25_1_akiyo0_128x64_2frm_zeromv.ivf"; }

    /* 10 bit */
    char const* query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_P010>)
    { return "conformance/av1/DVK/Smaller_AV1_p2b10_432x240_209_extra_smaller_1.0.av1"; }

    template <unsigned fourcc>
    class TestSuite
        : public tsVideoDecoder
        , public tsSurfaceProcessor
    {

    public:

        TestSuite()
            : tsVideoDecoder(MFX_CODEC_AV1)
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

    TS_REG_TEST_SUITE(av1d_8b_420_nv12_get_payload,  TestSuite<MFX_FOURCC_NV12>::RunTest, 1);
    TS_REG_TEST_SUITE(av1d_10b_420_p010_get_payload, TestSuite<MFX_FOURCC_P010>::RunTest, 1);
}
