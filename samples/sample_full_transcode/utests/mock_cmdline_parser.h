//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "cmdline_parser.h"
#include <gmock/gmock.h>

class MockHandler : public OptionHandler {
public:
    MOCK_CONST_METHOD2(PrintHelp, void(msdk_ostream & os, size_t));
    MOCK_METHOD1(Handle, size_t(const msdk_string & ));
    MOCK_CONST_METHOD0(GetName, msdk_string());
    MOCK_CONST_METHOD1(GetValue, msdk_string(int));
    MOCK_CONST_METHOD0(Exist, bool ());
    MOCK_CONST_METHOD0(CanHaveDelimiters, bool ());
};

class MockCmdLineParser : public CmdLineParser {
public:
    MockCmdLineParser() :
        CmdLineParser(MSDK_STRING(""),MSDK_STRING("")) {

    }
    MOCK_METHOD1(RegisterOption, void (const OptionHandler &));
    MOCK_METHOD1(Parse, bool (const msdk_string & params)); 
    MOCK_CONST_METHOD1(at, const OptionHandler & (const msdk_string & option));
    MOCK_CONST_METHOD2(PrintHelp, void (std::ostream &, size_t ));
    MOCK_CONST_METHOD1(IsPresent, bool (const msdk_string & option));
};
