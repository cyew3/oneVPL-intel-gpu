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

#include "option_handler.h"
#include "sample_defs.h"
#include <iostream>

using namespace detail;

size_t OptionHandlerBase::Handle(const msdk_string & str) {
    //second attempt to parse this value
    if (m_bHandled) {
        MSDK_TRACE_ERROR( MSDK_STRING("Duplicate option found : ") << str );
        return 0;
    }
    size_t stpos = str.find(GetName());
    if (stpos != 0) {
        MSDK_TRACE_ERROR(MSDK_STRING("not suitable option for this handler: ") << str );
        return 0;
    }
    stpos += GetName().size();
    if (stpos > str.size() || str[stpos] != MSDK_CHAR(' ') || stpos + 1 > str.size() || str[stpos+1]==MSDK_CHAR('-')) {
        MSDK_TRACE_ERROR( MSDK_STRING("option \"")<<GetName()<<MSDK_STRING("\" should have exactly one argument: ") << str );
        return 0;
    }
    stpos ++;
    size_t next_opt_pos = str.find(MSDK_STRING(" -"), stpos);
    m_value = str.substr(stpos, next_opt_pos-stpos);

    //check against delimiters - suitable for non string arguments
    if (m_value.empty() || (!CanHaveDelimiters() && (m_value.find_first_of(MSDK_CHAR(' ')) != msdk_string::npos))) {
        MSDK_TRACE_ERROR( MSDK_STRING("option \"")<<GetName()<<MSDK_STRING("\" should have exactly one argument: ") << str );
        return 0;
    }
    m_bHandled = true;
    return next_opt_pos == msdk_string::npos ? GetName().size() + m_value.size() + 1 : next_opt_pos + 1;
}

size_t ArgHandler<bool>::Handle(const msdk_string & str) {
    //second attempt to parse this value
    m_bHandled = true;
    size_t stpos = str.find(GetName());
    size_t next_opt_pos = str.find(MSDK_STRING(" -"), stpos);
    return next_opt_pos == msdk_string::npos ? GetName().size() + m_value.size() : next_opt_pos + 1;
}