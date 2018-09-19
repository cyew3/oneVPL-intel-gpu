/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_struct.h"

namespace hevcd_decode_order
{
    template <unsigned fourcc>
    std::tuple<char const*, size_t> query_stream(unsigned int, std::integral_constant<unsigned, fourcc>);

    /* 8 bit */
    std::tuple<char const*, size_t> query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_NV12>)
    { return std::make_tuple("forBehaviorTest/dif_resol/hevc/420format_8bit/Kimono1_704x576_24_420_8.265", 8); }
    std::tuple<char const*, size_t> query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_YUY2>)
    { return std::make_tuple("forBehaviorTest/dif_resol/hevc/422format/Kimono1_704x576_24_422_8.265", 16); }
    std::tuple<char const*, size_t> query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_AYUV>)
    { return std::make_tuple("forBehaviorTest/dif_resol/hevc/444format/Kimono1_704x576_24_444_8.265", 16); }

    /* 10b bit */
    std::tuple<char const*, size_t> query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_P010>)
    { return std::make_tuple("forBehaviorTest/dif_resol/hevc/420format_10bit/Kimono1_704x576_24_420_10.265", 8); }
    std::tuple<char const*, size_t> query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y210>)
    { return std::make_tuple("forBehaviorTest/dif_resol/hevc/422format_10bit/Kimono1_704x576_24_422_10.265", 16); }
    std::tuple<char const*, size_t> query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y410>)
    { return std::make_tuple("forBehaviorTest/dif_resol/hevc/444format_10bit/Kimono1_704x576_24_444_10.265", 16); }

    /* 12 bit */
    std::tuple<char const*, size_t> query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_P016>)
    { return std::make_tuple("forBehaviorTest/dif_resol/hevc/420format_12bit/Kimono1_704x576_24_420_12.265", 8); }
    std::tuple<char const*, size_t> query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y216>)
    { return std::make_tuple("forBehaviorTest/dif_resol/hevc/422format_12bit/Kimono1_704x576_24_422_12.265", 16); }
    std::tuple<char const*, size_t> query_stream(unsigned int, std::integral_constant<unsigned, MFX_FOURCC_Y416>)
    { return std::make_tuple("forBehaviorTest/dif_resol/hevc/444format_12bit/Kimono1_704x576_24_444_12.265", 16); }

    template <unsigned fourcc, unsigned profile>
    std::tuple<char const*, size_t> query_stream(unsigned int id, std::integral_constant<unsigned, fourcc>, std::integral_constant<unsigned, profile>)
    { return query_stream(id, std::integral_constant<unsigned, fourcc>{}); }

    int test(unsigned int, char const* sname, size_t frames)
    {
        TS_START;

        tsVideoDecoder dec(MFX_CODEC_HEVC);

        tsSplitterHEVCES reader(sname, 100000);
        dec.m_bs_processor = &reader;

        dec.DecodeHeader();

        dec.m_pPar->AsyncDepth = 1;
        dec.m_pPar->mfx.DecodedOrder = 1;

        for (size_t n = 0; n < frames; ++n)
        {
            g_tsStatus.expect(MFX_ERR_NONE);

            dec.DecodeFrameAsync();
            if(MFX_WRN_VIDEO_PARAM_CHANGED == g_tsStatus.get())
                continue;
            g_tsStatus.check();
            dec.m_update_bs = true;

            dec.SyncOperation();
        }

        TS_END;
        return 0;
    }

    template <unsigned fourcc, unsigned profile = MFX_PROFILE_UNKNOWN>
    int test(unsigned int id)
    {
        auto stream = 
            query_stream(id, std::integral_constant<unsigned, fourcc>{}, std::integral_constant<unsigned, profile>{});

        auto sname = g_tsStreamPool.Get(std::get<0>(stream));
        g_tsStreamPool.Reg();

        return
            test(id, sname, std::get<1>(stream));
    }

    TS_REG_TEST_SUITE( hevcd_8b_420_nv12_decode_order, test<MFX_FOURCC_NV12>, 1);
    TS_REG_TEST_SUITE( hevcd_8b_422_yuy2_decode_order, test<MFX_FOURCC_YUY2>, 1);
    TS_REG_TEST_SUITE( hevcd_8b_444_ayuv_decode_order, test<MFX_FOURCC_AYUV>, 1);

    TS_REG_TEST_SUITE(hevcd_10b_420_p010_decode_order, test<MFX_FOURCC_P010>, 1);
    TS_REG_TEST_SUITE(hevcd_10b_422_y210_decode_order, test<MFX_FOURCC_Y210>, 1);
    TS_REG_TEST_SUITE(hevcd_10b_444_y410_decode_order, test<MFX_FOURCC_Y410>, 1);

    TS_REG_TEST_SUITE(hevcd_12b_420_p016_decode_order, test<MFX_FOURCC_P016>, 1);
    TS_REG_TEST_SUITE(hevcd_12b_422_y216_decode_order, test<MFX_FOURCC_Y216>, 1);
    TS_REG_TEST_SUITE(hevcd_12b_444_y416_decode_order, test<MFX_FOURCC_Y416>, 1);
}