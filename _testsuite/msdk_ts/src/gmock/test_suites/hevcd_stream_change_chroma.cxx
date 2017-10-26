//This file is included from [hevcd_stream_change.cpp]
//DO NOT include it into the project manually

#ifdef TS_REG_INCLUDE_HEVCD_STREAM_CHANGE

struct chroma_change_tag;

/* 8b 420 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_NV12, chroma_change_tag>::test_cases[][TestSuite::max_num_ctrl] =
{
    {/* 0*/ {"hevc/420format/Kimono1_704x576_24_420_8.265"}, {"hevc/422format/Kimono1_704x576_24_422_8.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 1*/ {"hevc/420format/Kimono1_704x576_24_420_8.265", 5}, {"hevc/422format/Kimono1_704x576_24_422_8.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 2*/ {"hevc/420format/Kimono1_704x576_24_420_8.265"}, {"hevc/444format/Kimono1_704x576_24_444_8.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 3*/ {"hevc/420format/Kimono1_704x576_24_420_8.265", 5}, {"hevc/444format/Kimono1_704x576_24_444_8.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_NV12, chroma_change_tag>::n_cases = sizeof(TestSuiteExt<MFX_FOURCC_NV12, chroma_change_tag>::test_cases) / sizeof(TestSuite::tc_struct[max_num_ctrl]);

/* 8b 422 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_YUY2, chroma_change_tag>::test_cases[][TestSuite::max_num_ctrl] =
{
    {/* 0*/ {"hevc/422format/Kimono1_704x576_24_422_8.265"}, {"hevc/420format/Kimono1_704x576_24_420_8.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 1*/ {"hevc/422format/Kimono1_704x576_24_422_8.265", 5}, {"hevc/420format/Kimono1_704x576_24_420_8.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 2*/ {"hevc/422format/Kimono1_704x576_24_422_8.265"}, {"hevc/444format/Kimono1_704x576_24_444_8.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 3*/ {"hevc/422format/Kimono1_704x576_24_422_8.265", 5}, {"hevc/444format/Kimono1_704x576_24_444_8.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_YUY2, chroma_change_tag>::n_cases = sizeof(TestSuiteExt<MFX_FOURCC_YUY2, chroma_change_tag>::test_cases) / sizeof(TestSuite::tc_struct[max_num_ctrl]);

/* 8b 444 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_AYUV, chroma_change_tag>::test_cases[][TestSuite::max_num_ctrl] =
{
    {/* 0*/ {"hevc/444format/Kimono1_704x576_24_444_8.265"}, {"hevc/420format/Kimono1_704x576_24_420_8.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 1*/ {"hevc/444format/Kimono1_704x576_24_444_8.265", 5}, {"hevc/420format/Kimono1_704x576_24_420_8.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 2*/ {"hevc/444format/Kimono1_704x576_24_444_8.265"}, {"hevc/422format/Kimono1_704x576_24_422_8.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 3*/ {"hevc/444format/Kimono1_704x576_24_444_8.265", 5}, {"hevc/422format/Kimono1_704x576_24_422_8.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_AYUV, chroma_change_tag>::n_cases = sizeof(TestSuiteExt<MFX_FOURCC_AYUV, chroma_change_tag>::test_cases) / sizeof(TestSuite::tc_struct[max_num_ctrl]);

/* 10b 420 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_P010, chroma_change_tag>::test_cases[][TestSuite::max_num_ctrl] =
{
    {/* 0*/ {"hevc/420format_10bit/Kimono1_704x576_24_420_10.265"}, {"hevc/422format_10bit/Kimono1_704x576_24_422_10.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 1*/ {"hevc/420format_10bit/Kimono1_704x576_24_420_10.265", 5}, {"hevc/422format_10bit/Kimono1_704x576_24_422_10.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 2*/ {"hevc/420format_10bit/Kimono1_704x576_24_420_10.265"}, {"hevc/444format_10bit/Kimono1_704x576_24_444_10.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 3*/ {"hevc/420format_10bit/Kimono1_704x576_24_420_10.265", 5}, {"hevc/444format_10bit/Kimono1_704x576_24_444_10.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_P010, chroma_change_tag>::n_cases = sizeof(TestSuiteExt<MFX_FOURCC_P010, chroma_change_tag>::test_cases) / sizeof(TestSuite::tc_struct[max_num_ctrl]);

/* 10b 422 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_Y210, chroma_change_tag>::test_cases[][TestSuite::max_num_ctrl] =
{
    {/* 0*/ {"hevc/422format_10bit/Kimono1_704x576_24_422_10.265"}, {"hevc/420format_10bit/Kimono1_704x576_24_420_10.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 1*/ {"hevc/422format_10bit/Kimono1_704x576_24_422_10.265", 5}, {"hevc/420format_10bit/Kimono1_704x576_24_420_10.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 2*/ {"hevc/422format_10bit/Kimono1_704x576_24_422_10.265"}, {"hevc/444format_10bit/Kimono1_704x576_24_444_10.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 3*/ {"hevc/422format_10bit/Kimono1_704x576_24_422_10.265", 5}, {"hevc/444format_10bit/Kimono1_704x576_24_444_10.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_Y210, chroma_change_tag>::n_cases = sizeof(TestSuiteExt<MFX_FOURCC_Y210, chroma_change_tag>::test_cases) / sizeof(TestSuite::tc_struct[max_num_ctrl]);

/* 10b 444 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_Y410, chroma_change_tag>::test_cases[][TestSuite::max_num_ctrl] =
{
    {/* 0*/ {"hevc/444format_10bit/Kimono1_704x576_24_444_10.265"}, {"hevc/420format_10bit/Kimono1_704x576_24_420_10.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 1*/ {"hevc/444format_10bit/Kimono1_704x576_24_444_10.265", 5}, {"hevc/420format_10bit/Kimono1_704x576_24_420_10.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 2*/ {"hevc/444format_10bit/Kimono1_704x576_24_444_10.265"}, {"hevc/422format_10bit/Kimono1_704x576_24_422_10.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 3*/ {"hevc/444format_10bit/Kimono1_704x576_24_444_10.265", 5}, {"hevc/422format_10bit/Kimono1_704x576_24_422_10.265", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_Y410, chroma_change_tag>::n_cases = sizeof(TestSuiteExt<MFX_FOURCC_Y410, chroma_change_tag>::test_cases) / sizeof(TestSuite::tc_struct[max_num_ctrl]);

/* 12b 420 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_P016, chroma_change_tag>::test_cases[][TestSuite::max_num_ctrl] =
{
    {/* 0*/ {"hevc/420format_12bit/GENERAL_12b_420_RExt_Sony_1.bit"},    {"hevc/422format_12bit/GENERAL_12b_422_RExt_Sony_1.bit", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 1*/ {"hevc/420format_12bit/GENERAL_12b_420_RExt_Sony_1.bit", 5}, {"hevc/422format_12bit/GENERAL_12b_422_RExt_Sony_1.bit", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 2*/ {"hevc/420format_12bit/GENERAL_12b_420_RExt_Sony_1.bit"},    {"hevc/444format_12bit/GENERAL_12b_444_RExt_Sony_1.bit", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 3*/ {"hevc/420format_12bit/GENERAL_12b_420_RExt_Sony_1.bit", 5}, {"hevc/444format_12bit/GENERAL_12b_444_RExt_Sony_1.bit", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_P016, chroma_change_tag>::n_cases = sizeof(TestSuiteExt<MFX_FOURCC_P016, chroma_change_tag>::test_cases) / sizeof(TestSuite::tc_struct[max_num_ctrl]);

/* 12b 422 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_Y216, chroma_change_tag>::test_cases[][TestSuite::max_num_ctrl] =
{
    {/* 0*/ {"hevc/422format_12bit/GENERAL_12b_422_RExt_Sony_1.bit"},    {"hevc/420format_12bit/GENERAL_12b_420_RExt_Sony_1.bit", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 1*/ {"hevc/422format_12bit/GENERAL_12b_422_RExt_Sony_1.bit", 5}, {"hevc/420format_12bit/GENERAL_12b_420_RExt_Sony_1.bit", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 2*/ {"hevc/422format_12bit/GENERAL_12b_422_RExt_Sony_1.bit"},    {"hevc/444format_12bit/GENERAL_12b_444_RExt_Sony_1.bit", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 3*/ {"hevc/422format_12bit/GENERAL_12b_422_RExt_Sony_1.bit", 5}, {"hevc/444format_12bit/GENERAL_12b_444_RExt_Sony_1.bit", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_Y216, chroma_change_tag>::n_cases = sizeof(TestSuiteExt<MFX_FOURCC_Y216, chroma_change_tag>::test_cases) / sizeof(TestSuite::tc_struct[max_num_ctrl]);

/* 12b 444 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_Y416, chroma_change_tag>::test_cases[][TestSuite::max_num_ctrl] =
{
    {/* 0*/ {"hevc/444format_12bit/GENERAL_12b_444_RExt_Sony_1.bit"},    {"hevc/420format_12bit/GENERAL_12b_420_RExt_Sony_1.bit", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 1*/ {"hevc/444format_12bit/GENERAL_12b_444_RExt_Sony_1.bit", 5}, {"hevc/420format_12bit/GENERAL_12b_420_RExt_Sony_1.bit", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 2*/ {"hevc/444format_12bit/GENERAL_12b_444_RExt_Sony_1.bit"},    {"hevc/422format_12bit/GENERAL_12b_422_RExt_Sony_1.bit", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
    {/* 3*/ {"hevc/444format_12bit/GENERAL_12b_444_RExt_Sony_1.bit", 5}, {"hevc/422format_12bit/GENERAL_12b_422_RExt_Sony_1.bit", 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_Y416, chroma_change_tag>::n_cases = sizeof(TestSuiteExt<MFX_FOURCC_Y416, chroma_change_tag>::test_cases) / sizeof(TestSuite::tc_struct[max_num_ctrl]);

TS_REG_TEST_SUITE(hevcd_chroma_change,       (TestSuiteExt<MFX_FOURCC_NV12, chroma_change_tag>::RunTest), (TestSuiteExt<MFX_FOURCC_NV12, chroma_change_tag>::n_cases));
TS_REG_TEST_SUITE(hevcd_422_chroma_change,   (TestSuiteExt<MFX_FOURCC_YUY2, chroma_change_tag>::RunTest), (TestSuiteExt<MFX_FOURCC_YUY2, chroma_change_tag>::n_cases));
TS_REG_TEST_SUITE(hevcd_444_chroma_change,   (TestSuiteExt<MFX_FOURCC_AYUV, chroma_change_tag>::RunTest), (TestSuiteExt<MFX_FOURCC_AYUV, chroma_change_tag>::n_cases));

TS_REG_TEST_SUITE(hevc10d_chroma_change,     (TestSuiteExt<MFX_FOURCC_P010, chroma_change_tag>::RunTest), (TestSuiteExt<MFX_FOURCC_P010, chroma_change_tag>::n_cases));
TS_REG_TEST_SUITE(hevc10d_422_chroma_change, (TestSuiteExt<MFX_FOURCC_Y210, chroma_change_tag>::RunTest), (TestSuiteExt<MFX_FOURCC_Y210, chroma_change_tag>::n_cases));
TS_REG_TEST_SUITE(hevc10d_444_chroma_change, (TestSuiteExt<MFX_FOURCC_Y410, chroma_change_tag>::RunTest), (TestSuiteExt<MFX_FOURCC_Y410, chroma_change_tag>::n_cases));

TS_REG_TEST_SUITE(hevcd_12b_420_p016_chroma_change, (TestSuiteExt<MFX_FOURCC_P016, chroma_change_tag>::RunTest), (TestSuiteExt<MFX_FOURCC_P016, chroma_change_tag>::n_cases));
TS_REG_TEST_SUITE(hevcd_12b_422_y216_chroma_change, (TestSuiteExt<MFX_FOURCC_Y216, chroma_change_tag>::RunTest), (TestSuiteExt<MFX_FOURCC_Y216, chroma_change_tag>::n_cases));
TS_REG_TEST_SUITE(hevcd_12b_444_y416_chroma_change, (TestSuiteExt<MFX_FOURCC_Y416, chroma_change_tag>::RunTest), (TestSuiteExt<MFX_FOURCC_Y416, chroma_change_tag>::n_cases));

#endif //TS_REG_INCLUDE_HEVCD_STREAM_CHANGE