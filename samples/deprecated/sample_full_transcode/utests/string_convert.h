//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once
#include "sample_utils.h"

#ifdef UNICODE
    #include <clocale>
    #include <locale>
#endif

namespace {

    msdk_string utest_cvt_string2msdk(const std::string & lhs)
    {
#ifndef UNICODE
        return lhs;
#else
        std::locale const loc("");
        std::vector<wchar_t> buffer(lhs.size() + 1);
        std::use_facet<std::ctype<wchar_t> >(loc).widen(&lhs.at(0), &lhs.at(0) + lhs.size(), &buffer[0]);
        return msdk_string(&buffer[0], &buffer[lhs.size()]);

#endif
    }


    std::string  utest_cvt_msdk2string(const msdk_string & lhs)
    {
#ifndef UNICODE
        return lhs;
#else
        std::locale const loc("");
        std::vector<char> buffer(lhs.size() + 1);
        std::use_facet<std::ctype<wchar_t> >(loc).narrow(lhs.c_str(), lhs.c_str() + lhs.size(), '?', &buffer[0]);
        return std::string(&buffer[0], &buffer[lhs.size()]);

#endif

    }
}