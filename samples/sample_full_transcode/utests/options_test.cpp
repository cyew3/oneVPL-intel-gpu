//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "utest_pch.h"

#include "cmdline_options.h"

namespace {
    void PrintMockHelp(msdk_ostream & os ) {
        os << MSDK_CHAR("-mock   prints mock help");
    }
    struct OptionsTest : public ::testing::Test {
        CmdLineParser parser;
        std::auto_ptr<MockHandler> hdl;
        std::auto_ptr<MockHandler> hdl1;
        std::auto_ptr<MockHandler> hdl2;
        std::auto_ptr<MockHandler> hdl3;
        std::auto_ptr<MockHandler> hdl4;

        std::auto_ptr<OptionHandler> chdl;
        std::auto_ptr<OptionHandler> chdl1;

        OptionsTest()
            : hdl(new MockHandler)
            , hdl1(new MockHandler)
            , hdl2(new MockHandler)
            , hdl3(new MockHandler)
            , hdl4(new MockHandler)
            , parser(CmdLineParser(MSDK_STRING(""),MSDK_STRING(""))) {
        }
    };
}

TEST_F(OptionsTest, integration_test_parse_io_option) {
    std::auto_ptr<OptionHandler> opt1(new ArgHandler<msdk_string>(OPTION_I, MSDK_CHAR("Input file")));
    std::auto_ptr<OptionHandler> opt2(new ArgHandler<msdk_string>(OPTION_O, MSDK_CHAR("Output file")));

    parser.RegisterOption(opt1);
    parser.RegisterOption(opt2);

    EXPECT_EQ(true, parser.Parse(MSDK_CHAR("-i z:\\my streams\\mystream.264 -o z:\\my output\\myoutput.out")));
    EXPECT_STREQ("z:\\my output\\myoutput.out", utest_cvt_msdk2string(parser[OPTION_O].as<msdk_string>()).c_str());
    EXPECT_STREQ("z:\\my streams\\mystream.264", utest_cvt_msdk2string(parser[OPTION_I].as<msdk_string>()).c_str());
}

TEST_F(OptionsTest, integration_test_parse_wh_option) {
    std::auto_ptr<OptionHandler> opt1(new ArgHandler<int>(OPTION_W, MSDK_CHAR("Frame width")));
    std::auto_ptr<OptionHandler> opt2(new ArgHandler<int>(OPTION_H, MSDK_CHAR("Frame height")));

    parser.RegisterOption(opt1);
    parser.RegisterOption(opt2);

    EXPECT_EQ(true, parser.Parse(MSDK_CHAR("-w 1920 -h 1080")));
    EXPECT_EQ(1920, parser[OPTION_W].as<int>());
    EXPECT_EQ(1080, parser[OPTION_H].as<int>());
}

TEST_F(OptionsTest, integration_test_parse_adepth_option) {
    std::auto_ptr<OptionHandler> opt(new ArgHandler<int>(OPTION_ADEPTH, MSDK_CHAR("Async depth")));

    parser.RegisterOption(opt);

    EXPECT_EQ(true, parser.Parse(MSDK_CHAR("-async 4")));
    EXPECT_EQ(4, parser[OPTION_ADEPTH].as<int>());
}
