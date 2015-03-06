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

#include "utest_pch.h"

//setting statics for tests
namespace {

    class TraceLevelSetter {
    public:
        TraceLevelSetter(int level) {
            msdk_trace_set_level(level);
        }
    };

    TraceLevelSetter lset(MSDK_TRACE_LEVEL_SILENT);
}