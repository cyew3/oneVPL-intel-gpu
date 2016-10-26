//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2012 Intel Corporation. All Rights Reserved.
//

#ifndef __TEST_COMPONENT_EXTERNAL_H__
#define __TEST_COMPONENT_EXTERNAL_H__

// uncomment this line to enable Xerces support
//#define XERCES_XML
//#define ENABLE_FINGERPRINT

#define TC_MAX_PATH 1024

#include "ippdefs.h"
#include "vm_strings.h"
#include "umc_base_codec.h"

enum TestOutlineModes
{
    TEST_OUTLINE_NONE = 0,
    TEST_OUTLINE_WRITE = 1,
    TEST_OUTLINE_READ = 2,
};

// test component params class
struct TestComponentParams
{
    const vm_char *m_OutlineFilename;
    const vm_char *m_RefOutlineFilename;
    const vm_char *m_LogFilename;

    Ipp32s m_Mode;
};

enum TestComponentType
{
    AUDIO = 0,
    VIDEO = 1,
};

#endif// __TEST_COMPONENT_EXTERNAL_H__
