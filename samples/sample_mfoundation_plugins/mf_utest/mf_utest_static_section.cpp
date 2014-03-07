/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: mf_hw_platform.cpp

\* ****************************************************************************** */

#include "mf_guids.h"

struct StaticSection {
    StaticSection() {
        MFX_TRACE_INIT(_T(MF_TRACE_OFILE_NAME), MFX_TRACE_OMODE_FILE);
    }
};

static StaticSection g_section;
