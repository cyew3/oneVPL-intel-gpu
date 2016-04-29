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
#pragma once
#include <memory>

template <class T>
inline std::auto_ptr<T> & instant_auto_ptr(T *obj) {
    static std::auto_ptr<T> ptr;
    ptr.reset(obj);
    return ptr;
}

template <class T, class TObj>
inline std::auto_ptr<T> & instant_auto_ptr2(TObj *obj) {
    static std::auto_ptr<T> ptr;
    ptr.reset(obj);
    return ptr;
}

//requires mock_parser object
#define DECL_OPTION_IN_PARSER(option_name, option_value, hdl)\
EXPECT_CALL(hdl, GetValue(0)).WillRepeatedly(Return(MSDK_CHAR(option_value)));\
EXPECT_CALL(hdl, Exist()).WillRepeatedly(Return(true));\
EXPECT_CALL(mock_parser, at(msdk_string(option_name))).WillRepeatedly(ReturnRef(hdl));\
EXPECT_CALL(mock_parser, IsPresent(msdk_string(option_name))).WillRepeatedly(Return(true));

#define DECL_NO_OPTION_IN_PARSER(option_name)\
    EXPECT_CALL(mock_parser, IsPresent(msdk_string(option_name))).WillRepeatedly(Return(false));
