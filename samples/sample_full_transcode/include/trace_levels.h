//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once
#include "sample_utils.h"

class TraceLevels {
public:
    static std::vector<msdk_string> g_strLevels;
};

inline msdk_ostream & operator << (msdk_ostream & os, const TraceLevels &/*levels*/) {
    for (size_t i =0; i < TraceLevels::g_strLevels.size(); i++) {
        os << TraceLevels::g_strLevels[i] << MSDK_CHAR(' ');
    }
    return os;
}

class TraceLevel {
public:
    static TraceLevel & FromString(const msdk_string &);

    virtual MsdkTraceLevel ToTraceLevel() = 0;
    virtual const msdk_string & ToString() = 0;
};


template <MsdkTraceLevel level >
class TraceLevelTmpl  : public TraceLevel{
public:
    virtual MsdkTraceLevel ToTraceLevel() {
        return level;
    }
    virtual const msdk_string & ToString() {
        return TraceLevels::g_strLevels[(size_t)(level + MSDK_TRACE_LEVEL_SILENT)];
    }
};

