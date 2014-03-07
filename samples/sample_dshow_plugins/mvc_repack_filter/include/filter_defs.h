/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2011 Intel Corporation. All Rights Reserved.
//
//
*/

#pragma once

#include "..\..\..\sample_common\include\current_date.h"

#define FILTER_NAME_STRING "Intel\xae Media SDK MVC Repacker"
#define DLL_NAME           "mvc_repack_filter.dll"
#define FILTER_NAME        _T(FILTER_NAME_STRING)

// {E65F1F08-2102-446d-A30B-9F6DFA0755AF}
static const GUID CLSID_MVCRepackFilter =
{ 0xe65f1f08, 0x2102, 0x446d, { 0xa3, 0xb, 0x9f, 0x6d, 0xfa, 0x7, 0x55, 0xaf } };


#define FILTER_GUID        CLSID_MVCRepackFilter
#define FILTER_CREATE      CMVCRepackFilter::CreateInstance
#define FILTER_INPUT       { &MEDIATYPE_Video, &MEDIASUBTYPE_H264 }
#define FILTER_OUTPUT      { &MEDIATYPE_Video, &MEDIASUBTYPE_H264 }

TCHAR * GetCodecRegistryPath()    {return _T(REGISTRY_PATH_PREFIX) _T("MVC Repacker");}