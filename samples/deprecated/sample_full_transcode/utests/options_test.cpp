/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

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
