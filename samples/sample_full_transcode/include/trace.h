//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
#pragma once
#include "sample_utils.h"

class TraceStreamName {
    const msdk_string m_name;
public:
    TraceStreamName(const msdk_string & name) 
        : m_name(name) {
    }
    const msdk_string & Name() const {
        return m_name;
    }
};

inline msdk_ostream & operator << (msdk_ostream & ostr, const TraceStreamName & name) {
    if (name.Name() == MSDK_STRING("-")) {
        ostr<<MSDK_STRING("STDIN");
    } else {
        ostr<<name.Name();
    }
    return ostr;
}



