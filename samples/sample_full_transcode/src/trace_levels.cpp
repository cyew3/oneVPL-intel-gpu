//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include <algorithm>
#include "exceptions.h"
#include <iostream>
#include "sample_defs.h"
#include "trace_levels.h"

std::vector<msdk_string> FillStringLevels();

std::vector<msdk_string> TraceLevels::g_strLevels = FillStringLevels();

std::vector<msdk_string> FillStringLevels() {
    std::vector<msdk_string> v;
    v.push_back(MSDK_STRING("silent"));
    v.push_back(MSDK_STRING("critical"));
    v.push_back(MSDK_STRING("error"));
    v.push_back(MSDK_STRING("warning"));
    v.push_back(MSDK_STRING("info"));
    v.push_back(MSDK_STRING("debug"));
    return v;
}

namespace {
    TraceLevelTmpl<MSDK_TRACE_LEVEL_SILENT>  log_level_silent;
    TraceLevelTmpl<MSDK_TRACE_LEVEL_CRITICAL>  log_level_critical;
    TraceLevelTmpl<MSDK_TRACE_LEVEL_ERROR>  log_level_error;
    TraceLevelTmpl<MSDK_TRACE_LEVEL_WARNING>  log_level_warning;
    TraceLevelTmpl<MSDK_TRACE_LEVEL_INFO>  log_level_info;
    TraceLevelTmpl<MSDK_TRACE_LEVEL_DEBUG>  log_level_debug;
}

TraceLevel & TraceLevel :: FromString(const msdk_string & str) {
    std::vector<msdk_string>::iterator i =
        std::find(TraceLevels::g_strLevels.begin(), TraceLevels::g_strLevels.end(), str);
    if (TraceLevels::g_strLevels.end() == i) {
        MSDK_TRACE_ERROR(MSDK_STRING("Unknown logger level: ") << str);
        throw UnknownLoggerLevelError();
    }
    MsdkTraceLevel idx = (MsdkTraceLevel)(i - TraceLevels::g_strLevels.begin() - 1);
    switch (idx) {
        case MSDK_TRACE_LEVEL_SILENT :
            return log_level_silent;
        case MSDK_TRACE_LEVEL_CRITICAL :
            return log_level_critical;
        case MSDK_TRACE_LEVEL_ERROR :
            return log_level_error;
        case MSDK_TRACE_LEVEL_WARNING :
            return log_level_warning;
        case MSDK_TRACE_LEVEL_INFO :
            return log_level_info;
        case MSDK_TRACE_LEVEL_DEBUG :
            return log_level_debug;

        default:
            return log_level_info;
    }
}
