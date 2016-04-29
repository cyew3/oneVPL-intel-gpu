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

#include "generic_types_to_text.h"

namespace {
    template <class T> void VerifyConvert (const std::string &str) {
        msdk_stringstream ss;
        ss << TypeToText<T> ();
        EXPECT_STREQ(str.c_str(), utest_cvt_msdk2string(ss.str()).c_str());
    }
};


TEST(GenericTTX, int64_t) {

    VerifyConvert<int64_t>("int64");
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
    VerifyConvert<unsigned long long>("unsigned int64");
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
