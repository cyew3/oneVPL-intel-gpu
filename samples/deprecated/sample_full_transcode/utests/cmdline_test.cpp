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

namespace {
    void PrintMockHelp(msdk_ostream & os, size_t ) {
        os << MSDK_CHAR("-mock   prints mock help");
    }
    struct CmdLineParserTest : public ::testing::Test {
        CmdLineParser parser;
        std::auto_ptr<MockHandler> hdl;
        std::auto_ptr<MockHandler> hdl1;
        std::auto_ptr<MockHandler> hdl2;
        std::auto_ptr<MockHandler> hdl3;
        std::auto_ptr<MockHandler> hdl4;

        CmdLineParserTest()
            : hdl(new MockHandler)
            , hdl1(new MockHandler)
            , hdl2(new MockHandler)
            , hdl3(new MockHandler)
            , hdl4(new MockHandler)
            , parser(CmdLineParser(MSDK_STRING(""),MSDK_STRING(""))) {
        }
    };
}

TEST_F(CmdLineParserTest, register_option_print_help) {

    EXPECT_CALL(*hdl, GetName()).WillRepeatedly(Return(MSDK_STRING("-mock")));
    EXPECT_CALL(*hdl, PrintHelp(_, _)).WillOnce(Invoke(PrintMockHelp));
    msdk_stringstream os;

    parser.RegisterOption(hdl);

    hdl.reset(new MockHandler);
    EXPECT_CALL(*hdl, GetName()).WillRepeatedly(Return(MSDK_STRING("-mock")));
    EXPECT_CALL(*hdl.get(), PrintHelp(_, _)).WillOnce(Invoke(PrintMockHelp));

    parser.RegisterOption(hdl);

    parser.PrintHelp(os);
    EXPECT_STREQ("\n-mock   prints mock help\n-mock   prints mock help\n\n", utest_cvt_msdk2string(os.str()).c_str());
}

TEST_F(CmdLineParserTest, select_handler_for_parsing) {
    EXPECT_CALL(*hdl.get(), Handle(_)).WillOnce(Return(1));
    EXPECT_CALL(*hdl.get(), GetName()).WillRepeatedly(Return(MSDK_STRING("a")));

    parser.RegisterOption(hdl);
    EXPECT_EQ(true, parser.Parse(MSDK_CHAR("a")));
}

TEST_F(CmdLineParserTest, no_suitable_handler_for_parsing) {

    EXPECT_CALL(*hdl.get(), Handle(_)).WillOnce(Return(2));
    EXPECT_CALL(*hdl.get(), GetName()).WillRepeatedly(Return(MSDK_STRING("a")));

    parser.RegisterOption(hdl);
    EXPECT_EQ(false, parser.Parse(MSDK_CHAR("a b")));
}

TEST_F(CmdLineParserTest, option_meet_twice) {

    EXPECT_CALL(*hdl.get(), Handle(_)).WillOnce(Return(1));
    EXPECT_CALL(*hdl.get(), GetName()).WillRepeatedly(Return(MSDK_STRING("a")));

    parser.RegisterOption(hdl);
    EXPECT_EQ(false, parser.Parse(MSDK_CHAR("a a")));
}


TEST_F(CmdLineParserTest, select_from2handlers_for_parsing) {

    EXPECT_CALL(*hdl.get(), Handle(_)).WillOnce(Return(1));
    EXPECT_CALL(*hdl.get(), GetName()).WillRepeatedly(Return(MSDK_STRING("a")));

    EXPECT_CALL(*hdl1.get(), Handle(_)).WillOnce(Return(2));
    EXPECT_CALL(*hdl1.get(), GetName()).WillRepeatedly(Return(MSDK_STRING("b")));

    parser.RegisterOption(hdl);
    parser.RegisterOption(hdl1);

    EXPECT_EQ(true, parser.Parse(MSDK_CHAR("b a")));
}

TEST_F(CmdLineParserTest, integration_test_parse3_options) {
    std::auto_ptr<CmdLineParser> parser2(CreateCmdLineParser (Options()
        (ArgHandler<msdk_string>(MSDK_CHAR("-opt1"), MSDK_CHAR("description")))
        (ArgHandler<int>(MSDK_CHAR("-opt2"), MSDK_CHAR("description")))
        (ArgHandler<double>(MSDK_CHAR("-opt45"), MSDK_CHAR("description"))),
        MSDK_STRING(""),
        MSDK_STRING("")));

    CmdLineParser &parser = *parser2.get();

    EXPECT_EQ(true, parser.Parse(MSDK_CHAR("-opt2 32 -opt1 ab cd efj -opt45 34.12")));
    EXPECT_STREQ("ab cd efj", utest_cvt_msdk2string(parser[MSDK_CHAR("-opt1")].as<msdk_string>()).c_str());
    EXPECT_EQ(32, parser[MSDK_CHAR("-opt2")].as<int>());
    EXPECT_EQ(34.12, parser[MSDK_CHAR("-opt45")].as<double>());
}

TEST_F (CmdLineParserTest, not_registered_option_requested) {

    std::auto_ptr<CmdLineParser> parser2(CreateCmdLineParser (Options(),
        MSDK_STRING(""),
        MSDK_STRING("")));
    CmdLineParser &parser3 = *parser2.get();

    EXPECT_THROW(parser3[MSDK_CHAR("-opt45")].as<double>(), NonRegisteredOptionError);
}

TEST_F (CmdLineParserTest, not_parsed_option_requested) {

    std::auto_ptr<CmdLineParser> parser2(CreateCmdLineParser (Options()
        (ArgHandler<msdk_string>(MSDK_CHAR("-opt1"),  MSDK_CHAR("description")))
        (ArgHandler<msdk_string>(MSDK_CHAR("-opt2"),  MSDK_CHAR("description"))),
        MSDK_STRING(""),
        MSDK_STRING("")));
    CmdLineParser &parser3 = *parser2.get();
    parser3.Parse(MSDK_CHAR("-opt2 32"));

    EXPECT_THROW(parser3[MSDK_CHAR("-opt1")].as<double>(), NonParsedOptionError);
}

TEST_F (CmdLineParserTest, option_is_present) {

    std::auto_ptr<CmdLineParser> parser2(CreateCmdLineParser (Options()
        (ArgHandler<msdk_string>(MSDK_CHAR("-opt1"),  MSDK_CHAR("description")))
        (ArgHandler<msdk_string>(MSDK_CHAR("-opt2"),  MSDK_CHAR("description"))),
        MSDK_STRING(""),
        MSDK_STRING("")));

    CmdLineParser &parser3 = *parser2.get();
    parser3.Parse(MSDK_CHAR("-opt2 32"));

    //registered option parsed
    EXPECT_EQ(true, parser3.IsPresent(MSDK_CHAR("-opt2")));

    //registered option not parsed
    EXPECT_EQ(true, !parser3.IsPresent(MSDK_CHAR("-opt1")));

    //lets not registered option result to false as well
    EXPECT_EQ(true, !parser3.IsPresent(MSDK_CHAR("-opt45")));
}


TEST_F (CmdLineParserTest, integration_test_parse3_options_new_API) {
    std::auto_ptr<CmdLineParser> parser2(CreateCmdLineParser ( Options()
        (ArgHandler<msdk_string>(MSDK_CHAR("-opt1"),  MSDK_CHAR("description")))
        (ArgHandler<int>        (MSDK_CHAR("-opt2"),  MSDK_CHAR("description")))
        (ArgHandler<double>     (MSDK_CHAR("-opt45"), MSDK_CHAR("description"))),
        MSDK_STRING(""),
        MSDK_STRING("")));

    CmdLineParser &parser3 = *parser2.get();

    EXPECT_EQ(true, parser3.Parse(MSDK_CHAR("-opt2 32 -opt1 ab cd efj -opt45 34.12")));
    EXPECT_STREQ("ab cd efj", utest_cvt_msdk2string(parser3[MSDK_CHAR("-opt1")].as<msdk_string>()).c_str());
    EXPECT_EQ(32, parser3[MSDK_CHAR("-opt2")].as<int>());
    EXPECT_EQ(34.12, parser3[MSDK_CHAR("-opt45")].as<double>());
}

TEST_F(CmdLineParserTest, cast_to_vector_of_int) {
    std::auto_ptr<CmdLineParser> parser2(CreateCmdLineParser( Options()
        (ArgHandler<std::vector<msdk_string> >(MSDK_CHAR("-opt1"),  MSDK_CHAR("description")))
        (ArgHandler<std::vector<int> > (MSDK_CHAR("-opt2"),  MSDK_CHAR("description"))),
        MSDK_STRING(""),
        MSDK_STRING("")));

    CmdLineParser &parser3 = *parser2.get();

    EXPECT_EQ(true, parser3.Parse(MSDK_CHAR("-opt1 ab bc -opt1 cd -opt2 1 -opt1 3 -opt2 2")));
    std::vector<int> vec = parser3[MSDK_CHAR("-opt2")].as<std::vector<int> >();

    EXPECT_EQ(1, vec[0]);
    EXPECT_EQ(2, vec[1]);
}

TEST_F(CmdLineParserTest, cast_to_vector_of_str) {
    std::auto_ptr<CmdLineParser> parser2(CreateCmdLineParser ( Options()
        (ArgHandler<std::vector<msdk_string> >(MSDK_CHAR("-opt1"),  MSDK_CHAR("description")))
        (ArgHandler<std::vector<int> > (MSDK_CHAR("-opt2"),  MSDK_CHAR("description"))),
        MSDK_STRING(""),
        MSDK_STRING("")));

    CmdLineParser &parser3 = *parser2.get();

    EXPECT_EQ(true, parser3.Parse(MSDK_CHAR("-opt1 ab bc -opt1 cd -opt2 1 -opt1 3 -opt2 2")));
    std::vector<msdk_string> vec = parser3[MSDK_CHAR("-opt1")].as<std::vector<msdk_string> >();

    EXPECT_STREQ("ab bc", utest_cvt_msdk2string(vec[0]).c_str());
    EXPECT_STREQ("cd", utest_cvt_msdk2string(vec[1]).c_str());
    EXPECT_STREQ("3", utest_cvt_msdk2string(vec[2]).c_str());
}
