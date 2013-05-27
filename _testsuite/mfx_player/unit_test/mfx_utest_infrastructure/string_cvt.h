/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma  once

#include <string>
#include <iostream>
#include <sstream>


inline std::string convert_w2s(const std::wstring &str)
{
    std::string  sout;
    sout.reserve(str.length() + 1);

    for (std::wstring::const_iterator it = str.begin(); it != str.end(); it ++)
    {
        sout.push_back((char)((*it)&0xFF));
    }
    return sout;
}

