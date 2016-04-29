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

#include "option_handler.h"

TEST(OptionHandlerTest, checkName) {
    ArgHandler<int> p1(MSDK_CHAR("-p1"), MSDK_CHAR("description"));
    msdk_stringstream ss;
    p1.PrintHelp(ss, 0);
    EXPECT_STREQ("-p1", utest_cvt_msdk2string(p1.GetName()).c_str());
}

TEST(OptionHandlerTest, printhelp) {
    ArgHandler<int> p1(MSDK_CHAR("-p1"), MSDK_CHAR("description"));
    msdk_stringstream ss;
    p1.PrintHelp(ss, 5);
    EXPECT_STREQ("-p1   [int]    description", utest_cvt_msdk2string(ss.str()).c_str());
}

TEST(OptionHandlerTest, handle_last_option) {
    ArgHandler<int> p1(MSDK_CHAR("-p1"), MSDK_CHAR("description"));
    EXPECT_EQ(5, p1.Handle(MSDK_CHAR("-p1 1")));
}

TEST(OptionHandlerTest, handle_not_last_option) {
    ArgHandler<int> p1(MSDK_CHAR("-p1"), MSDK_CHAR("description"));
    EXPECT_EQ(6, p1.Handle(MSDK_CHAR("-p1 1 -p3 2")));
}

TEST(OptionHandlerTest, handling_not_my_option) {
    ArgHandler<int> p1(MSDK_CHAR("-p1"), MSDK_CHAR("description"));
    EXPECT_EQ(0, p1.Handle(MSDK_CHAR("-p2")));
}

TEST(OptionHandlerTest, twice_handling) {
    ArgHandler<int> p1(MSDK_CHAR("-p1"), MSDK_CHAR("description"));
    EXPECT_EQ(5, p1.Handle(MSDK_CHAR("-p1 1")));
    EXPECT_EQ(0, p1.Handle(MSDK_CHAR("-p1 1")));
}

TEST(OptionHandlerTest, option_exist_FlagSet) {
    ArgHandler<int> p1(MSDK_CHAR("-p1"), MSDK_CHAR("description"));
    EXPECT_EQ(false, p1.Exist());
        p1.Handle(MSDK_CHAR("-p1 1"));
    EXPECT_EQ(true, p1.Exist());
}

TEST(OptionHandlerTest, option_cannot_have_delimiters) {
    ArgHandler<int> p1(MSDK_CHAR("-p1"), MSDK_CHAR("description"));
    EXPECT_EQ(false, p1.Exist());
    EXPECT_EQ(0, p1.Handle(MSDK_CHAR("-p1 1 2")));
    EXPECT_EQ(0, p1.Handle(MSDK_CHAR("-p1")));
    EXPECT_EQ(0, p1.Handle(MSDK_CHAR("-p1 ")));
    EXPECT_EQ(0, p1.Handle(MSDK_CHAR("-p1 -p1 2")));
}

TEST(OptionHandlerTest, convert_to_int) {
    ArgHandler<int> p1(MSDK_CHAR("-p1"), MSDK_CHAR("description"));
    EXPECT_EQ(6, p1.Handle(MSDK_CHAR("-p1 15")));

    EXPECT_EQ(15, p1.as<int>());
}

TEST(OptionHandlerTest, convert_to_double) {
    ArgHandler<double> p1(MSDK_CHAR("-s"), MSDK_CHAR("description"));
    EXPECT_EQ(7, p1.Handle(MSDK_CHAR("-s 15.2")));
    EXPECT_EQ(15.2, p1.as<double>());
}

TEST(OptionHandlerTest, string_handler) {
    ArgHandler<msdk_string> p1(MSDK_CHAR("-s"), MSDK_CHAR("description"));
    msdk_string param = MSDK_CHAR("-s some str to handle");
    EXPECT_EQ(param.size(), p1.Handle(param));
    EXPECT_STREQ("some str to handle", utest_cvt_msdk2string(p1.as<msdk_string>()).c_str());
}
