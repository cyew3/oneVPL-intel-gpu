//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
//*/

#include "utest_pch.h"

#include "generic_types_to_text.h"

namespace {
    template <class T> void VerifyConvert (const std::string &str) {
        msdk_stringstream ss;
        ss << TypeToText<T> ();
        EXPECT_STREQ(str.c_str(), utest_cvt_msdk2string(ss.str()).c_str());
    }
};


TEST(GenericTTX, __int64) {

    VerifyConvert<__int64>("int64");
}
TEST(GenericTTX, int) {
    VerifyConvert<int>("int");
}
TEST(GenericTTX, short) {
    VerifyConvert<short>("short");
}
TEST(GenericTTX, char) {
    VerifyConvert<char>("char");
}

TEST(GenericTTX, unsigned_int64) {
    VerifyConvert<unsigned __int64>("unsigned int64");
}
TEST(GenericTTX, unsigned_int) {
    VerifyConvert<unsigned int>("unsigned int");
}
TEST(GenericTTX, unsigned_short) {
    VerifyConvert<unsigned short>("unsigned short");
}
TEST(GenericTTX, unsigned_char) {
    VerifyConvert<unsigned char>("unsigned char");
}
TEST(GenericTTX, double) {
    VerifyConvert<double>("double");
}
TEST(GenericTTX, float) {
    VerifyConvert<float>("float");
}
TEST(GenericTTX, msdk_string) {
    VerifyConvert<msdk_string>("string");
}
TEST(GenericTTX, bool) {
    VerifyConvert<bool>("bool");
}
namespace {
    struct UnknownType {};
}

TEST(GenericTTX, UnknownType) {
    VerifyConvert<UnknownType>("unspecified type");
}
