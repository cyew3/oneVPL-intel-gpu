//This file is included from [hevcd_stream_change.cpp]
//DO NOT include it into the project manually

#ifdef TS_REG_INCLUDE_HEVCD_STREAM_CHANGE

struct res_change_tag;
struct res_change_10b_422_rext_tag; //special case 10 420 but w/ profile_idc = 4

/* 8b 420 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_NV12, res_change_tag>::test_cases[][TestSuite::max_num_ctrl] =
{
    {/* 0*/ {"foster_720x576_dpb6.h265"},       {"news_352x288_dpb6.h265"},         {}},
    {/* 1*/ {"news_352x288_dpb6.h265"},         {"foster_720x576_dpb6.h265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}, {}},
    {/* 2*/ {"foster_720x576_dpb6.h265", 5},    {"news_352x288_dpb6.h265", 5},      {}},
    {/* 3*/ {"foster_720x576_dpb6.h265"},       {"news_352x288_dpb6.h265"},         {"matrix_1920x1088_dpb6.h265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 4*/ {"matrix_1920x1088_dpb6.h265"},     {"foster_720x576_dpb6.h265"},       {"news_352x288_dpb6.h265"}},
    {/* 5*/ {"matrix_1920x1088_dpb6.h265", 5},  {"foster_720x576_dpb6.h265", 5},    {"news_352x288_dpb6.h265", 5}},
    {/* 6*/ {"matrix_1920x1088_dpb6.h265"},     {"news_352x288_dpb6.h265"},         {"foster_720x576_dpb6.h265"}},
    {/* 7*/ {"matrix_1920x1088_dpb6.h265", 5},  {"news_352x288_dpb6.h265", 5},      {"foster_720x576_dpb6.h265", 5}},
    {/* 8*/ {"matrix_1920x1088_dpb6.h265", 5},  {"foster_720x576_dpb6.h265", 5},    {"matrix_1920x1088_dpb6.h265", 5}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_NV12, res_change_tag>::n_cases = sizeof(TestSuiteExt<MFX_FOURCC_NV12, res_change_tag>::test_cases) / sizeof(TestSuite::tc_struct[max_num_ctrl]);

/* 8b 422 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_YUY2, res_change_tag>::test_cases[][TestSuite::max_num_ctrl] =
{
    {/* 0*/ {"hevc/422format/Kimono1_704x576_24_422_8.265"}, {"hevc/422format/GENERAL_8b_444_RExt_Sony_1_converted_to_422.265"}},
    {/* 1*/ {"hevc/422format/GENERAL_8b_444_RExt_Sony_1_converted_to_422.265"}, {"hevc/422format/Kimono1_704x576_24_422_8.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 2*/ {"hevc/422format/Kimono1_704x576_24_422_8.265", 5}, {"hevc/422format/GENERAL_8b_444_RExt_Sony_1_converted_to_422.265", 5}},
    {/* 3*/ {"hevc/422format/Kimono1_704x576_24_422_8.265", 5}, {"hevc/422format/GENERAL_8b_444_RExt_Sony_1_converted_to_422.265", 5}, {"hevc/422format/TSCTX_10bit_RExt_SHARP_1_converted_to_422_8.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 4*/ {"hevc/422format/TSCTX_10bit_RExt_SHARP_1_converted_to_422_8.265", 5}, {"hevc/422format/Kimono1_704x576_24_422_8.265", 5}, {"hevc/422format/GENERAL_8b_444_RExt_Sony_1_converted_to_422.265", 5}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_YUY2, res_change_tag>::n_cases = sizeof(TestSuiteExt<MFX_FOURCC_YUY2, res_change_tag>::test_cases) / sizeof(TestSuite::tc_struct[max_num_ctrl]);

/* 8b 444 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_AYUV, res_change_tag>::test_cases[][TestSuite::max_num_ctrl] =
{
    {/* 0*/ {"hevc/444format/Kimono1_704x576_24_444_8.265"}, {"hevc/444format/GENERAL_8b_444_RExt_Sony_1.bit"}},
    {/* 1*/ {"hevc/444format/GENERAL_8b_444_RExt_Sony_1.bit"}, {"hevc/444format/Kimono1_704x576_24_444_8.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 2*/ {"hevc/444format/Kimono1_704x576_24_444_8.265", 5}, {"hevc/444format/GENERAL_8b_444_RExt_Sony_1.bit", 5}},
    {/* 3*/ {"hevc/444format/Kimono1_704x576_24_444_8.265", 5}, {"hevc/444format/GENERAL_8b_444_RExt_Sony_1.bit", 5}, {"hevc/444format/TSCTX_10bit_RExt_SHARP_1_converted_to_444_8.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 4*/ {"hevc/444format/TSCTX_10bit_RExt_SHARP_1_converted_to_444_8.265", 5}, {"hevc/444format/Kimono1_704x576_24_444_8.265", 5}, {"hevc/444format/GENERAL_8b_444_RExt_Sony_1.bit", 5}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_AYUV, res_change_tag>::n_cases = sizeof(TestSuiteExt<MFX_FOURCC_AYUV, res_change_tag>::test_cases) / sizeof(TestSuite::tc_struct[max_num_ctrl]);

/* 10b 420 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_P010, res_change_tag>::test_cases[][TestSuite::max_num_ctrl] =
{
    {/* 0*/ {"hevc/420format_10bit/Kimono1_704x576_24_420_10.265"}, {"hevc/420format_10bit/WP_A_MAIN10_Toshiba_3.bit"}},
    {/* 1*/ {"hevc/420format_10bit/WP_A_MAIN10_Toshiba_3.bit"}, {"hevc/420format_10bit/Kimono1_704x576_24_420_10.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 2*/ {"hevc/420format_10bit/Kimono1_704x576_24_420_10.265", 5}, {"hevc/420format_10bit/WP_A_MAIN10_Toshiba_3.bit", 5}},
    {/* 3*/ {"hevc/420format_10bit/Kimono1_704x576_24_420_10.265", 5}, {"hevc/420format_10bit/WP_A_MAIN10_Toshiba_3.bit", 5}, {"hevc/420format_10bit/TSCTX_10bit_RExt_SHARP_1_converted_to_420_10.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 4*/ {"hevc/420format_10bit/TSCTX_10bit_RExt_SHARP_1_converted_to_420_10.265", 5}, {"hevc/420format_10bit/Kimono1_704x576_24_420_10.265", 5}, {"hevc/420format_10bit/WP_A_MAIN10_Toshiba_3.bit", 5}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_P010, res_change_tag>::n_cases = sizeof(TestSuiteExt<MFX_FOURCC_P010, res_change_tag>::test_cases) / sizeof(TestSuite::tc_struct[max_num_ctrl]);

/* 10b 420 Rext */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_P010, res_change_10b_422_rext_tag>::test_cases[][TestSuite::max_num_ctrl] =
{
    {/* 0*/ {"hevc/420format_10bit/TSCTX_10bit_RExt_SHARP_1_converted_to_420_10.265"}, {"hevc/420format_10bit/Kimono1_704x576_24_420_10.265"}},
    {/* 1*/ {"hevc/420format_10bit/Kimono1_704x576_24_420_10.265"}, {"hevc/420format_10bit/TSCTX_10bit_RExt_SHARP_1_converted_to_420_10.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 2*/ {"hevc/420format_10bit/TSCTX_10bit_RExt_SHARP_1_converted_to_420_10.265", 5}, {"hevc/420format_10bit/Kimono1_704x576_24_420_10.265", 5}},
    {/* 3*/ {"hevc/420format_10bit/Kimono1_704x576_24_420_10.265", 5}, {"hevc/420format_10bit/TSCTX_10bit_RExt_SHARP_1_converted_to_420_10.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_P010, res_change_10b_422_rext_tag>::n_cases = sizeof(TestSuiteExt<MFX_FOURCC_P010, res_change_10b_422_rext_tag>::test_cases) / sizeof(TestSuite::tc_struct[max_num_ctrl]);

/* 10b 422 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_Y210, res_change_tag>::test_cases[][TestSuite::max_num_ctrl] =
{
    {/* 0*/ {"hevc/422format_10bit/Kimono1_704x576_24_422_10.265"}, {"hevc/422format_10bit/GENERAL_10b_422_RExt_Sony_1.bit"}},
    {/* 1*/ {"hevc/422format_10bit/GENERAL_10b_422_RExt_Sony_1.bit"}, {"hevc/422format_10bit/Kimono1_704x576_24_422_10.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 2*/ {"hevc/422format_10bit/Kimono1_704x576_24_422_10.265", 5}, {"hevc/422format_10bit/GENERAL_10b_422_RExt_Sony_1.bit", 5}},
    {/* 3*/ {"hevc/422format_10bit/Kimono1_704x576_24_422_10.265", 5}, {"hevc/422format_10bit/GENERAL_10b_422_RExt_Sony_1.bit", 5}, {"hevc/TSCTX_10bit_RExt_SHARP_1_converted_to_422_10.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 4*/ {"hevc/422format_10bit/TSCTX_10bit_RExt_SHARP_1_converted_to_422_10.265", 5}, {"hevc/422format_10bit/Kimono1_704x576_24_422_10.265", 5}, {"hevc/422format_10bit/GENERAL_10b_422_RExt_Sony_1.bit", 5}},
    
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_Y210, res_change_tag>::n_cases = sizeof(TestSuiteExt<MFX_FOURCC_Y210, res_change_tag>::test_cases) / sizeof(TestSuite::tc_struct[max_num_ctrl]);

/* 10b 444 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_Y410, res_change_tag>::test_cases[][TestSuite::max_num_ctrl] =
{
    {/* 0*/ {"hevc/444format_10bit/Kimono1_704x576_24_444_10.265"}, {"hevc/444format_10bit/GENERAL_10b_444_RExt_Sony_1.bit"}},
    {/* 1*/ {"hevc/444format_10bit/GENERAL_10b_444_RExt_Sony_1.bit"}, {"hevc/444format_10bit/Kimono1_704x576_24_444_10.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 2*/ {"hevc/444format_10bit/Kimono1_704x576_24_444_10.265", 5}, {"hevc/444format_10bit/GENERAL_10b_444_RExt_Sony_1.bit", 5}},
    {/* 3*/ {"hevc/444format_10bit/Kimono1_704x576_24_444_10.265", 5}, {"hevc/444format_10bit/GENERAL_10b_444_RExt_Sony_1.bit", 5}, {"hevc/444format_10bit/TSCTX_10bit_RExt_SHARP_1.bin", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 4*/ {"hevc/444format_10bit/TSCTX_10bit_RExt_SHARP_1.bin", 5}, {"hevc/444format_10bit/Kimono1_704x576_24_444_10.265", 5}, {"hevc/444format_10bit/GENERAL_10b_444_RExt_Sony_1.bit", 5}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_Y410, res_change_tag>::n_cases = sizeof(TestSuiteExt<MFX_FOURCC_Y410, res_change_tag>::test_cases) / sizeof(TestSuite::tc_struct[max_num_ctrl]);

TS_REG_TEST_SUITE(hevcd_res_change,        (TestSuiteExt<MFX_FOURCC_NV12, res_change_tag>::RunTest), (TestSuiteExt<MFX_FOURCC_NV12, res_change_tag>::n_cases));
TS_REG_TEST_SUITE(hevcd_422_res_change,    (TestSuiteExt<MFX_FOURCC_YUY2, res_change_tag>::RunTest), (TestSuiteExt<MFX_FOURCC_YUY2, res_change_tag>::n_cases));
TS_REG_TEST_SUITE(hevcd_444_res_change,    (TestSuiteExt<MFX_FOURCC_AYUV, res_change_tag>::RunTest), (TestSuiteExt<MFX_FOURCC_AYUV, res_change_tag>::n_cases));
TS_REG_TEST_SUITE(hevc10d_res_change,      (TestSuiteExt<MFX_FOURCC_P010, res_change_tag>::RunTest), (TestSuiteExt<MFX_FOURCC_P010, res_change_tag>::n_cases));
TS_REG_TEST_SUITE(hevc10d_rext_res_change, (TestSuiteExt<MFX_FOURCC_P010, res_change_10b_422_rext_tag>::RunTest), (TestSuiteExt<MFX_FOURCC_P010, res_change_10b_422_rext_tag>::n_cases));
TS_REG_TEST_SUITE(hevc10d_422_res_change,  (TestSuiteExt<MFX_FOURCC_Y210, res_change_tag>::RunTest), (TestSuiteExt<MFX_FOURCC_Y210, res_change_tag>::n_cases));
TS_REG_TEST_SUITE(hevc10d_444_res_change,  (TestSuiteExt<MFX_FOURCC_Y410, res_change_tag>::RunTest), (TestSuiteExt<MFX_FOURCC_Y410, res_change_tag>::n_cases));

#endif //TS_REG_INCLUDE_HEVCD_STREAM_CHANGE