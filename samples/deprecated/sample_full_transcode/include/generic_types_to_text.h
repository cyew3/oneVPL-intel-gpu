//******************************************************************************\
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

#pragma once

#include <ostream>
#include "sample_utils.h"

template <class T>
struct TypeToText {
     msdk_string Type() const{
         return MSDK_CHAR("unspecified type");
     }
};

template <>
struct TypeToText <mfxI64> {
    msdk_string Type() const {
        return MSDK_CHAR("int64");
    }
};

template <>
struct TypeToText <mfxU64> {
    msdk_string Type() const {
        return MSDK_CHAR("unsigned int64");
    }
};

template <>
struct TypeToText <int>{
    msdk_string Type() const{
        return MSDK_CHAR("int");
    }
};

template <>
struct TypeToText <unsigned int>{
    msdk_string Type() const{
        return MSDK_CHAR("unsigned int");
    }
};

struct kbps_t {
};

template <>
struct TypeToText <kbps_t>{
    msdk_string Type() const{
        return MSDK_CHAR("Kbps");
    }
};

template <>
struct TypeToText <char>{
    msdk_string Type() const{
        return MSDK_CHAR("char");
    }
};

template <>
struct TypeToText <unsigned char>{
    msdk_string Type() const{
        return MSDK_CHAR("unsigned char");
    }
};

template <>
struct TypeToText <short>{
    msdk_string Type() const{
        return MSDK_CHAR("short");
    }
};

template <>
struct TypeToText <unsigned short>{
    msdk_string Type() const{
        return MSDK_CHAR("unsigned short");
    }
};

template <>
struct TypeToText <float>{
    msdk_string Type() const{
        return MSDK_CHAR("float");
    }
};

template <>
struct TypeToText <double>{
    msdk_string Type() const{
        return MSDK_CHAR("double");
    }
};

template <>
struct TypeToText <msdk_string>{
    msdk_string Type() const{
        return MSDK_CHAR("string");
    }
};

template <>
struct TypeToText <bool>{
    msdk_string Type() const{
        return MSDK_CHAR("bool");
    }
};

template<class T>
inline msdk_ostream & operator << (msdk_ostream & os, const TypeToText<T> &tt) {
    os<<tt.Type();
    return os;
}
