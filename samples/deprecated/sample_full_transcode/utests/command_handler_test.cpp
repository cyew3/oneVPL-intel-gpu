//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

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
