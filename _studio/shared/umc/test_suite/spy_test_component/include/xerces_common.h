//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2012 Intel Corporation. All Rights Reserved.
//

#ifndef __XERCES_COMMON_H__
#define __XERCES_COMMON_H__

#include "test_component_external.h"

#ifdef XERCES_XML

 // dynamic or static linkage
#if 0

#ifdef _DEBUG
#pragma comment(lib, "xerces-c_dynamic_3d")
#else
#pragma comment(lib, "xerces-c_dynamic_3")
#endif

#else

#ifdef _DEBUG
#pragma comment(lib, "xerces-c_static_3d")
#else
#pragma comment(lib, "xerces-c_static_3")
#endif

// this define is needed when we link to a static xerces library
#define XERCES_STATIC_LIBRARY

#endif

#include "vm_strings.h"

#include <xercesc/dom/DOM.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/framework/LocalFileFormatTarget.hpp>

#if defined(XERCES_NEW_IOSTREAMS)
#include <iostream>
#else
#include <iostream.h>
#endif

XERCES_CPP_NAMESPACE_USE

#pragma warning(disable: 4127)

template <typename T> XMLCh * transcodeToXML(const T * str)
{
    if (!str)
    {
        return 0;
    }

    if (sizeof(T) == 2)
    {
        return (XMLCh *)str;
    }
    else
    {
        return XMLString::transcode((const char *)str);
    }
}

template <typename T> T * transcodeFromXML(const XMLCh * xml_str)
{
    if (!xml_str)
        return 0;

    if (sizeof(T) == 2)
    {
        return (T*)xml_str;
    }
    else
    {
        return (T*)XMLString::transcode(xml_str);
    }
}

class Xerces_Initializer
{
    Xerces_Initializer()
    {
        XMLPlatformUtils::Initialize();
    }

    ~Xerces_Initializer()
    {
        XMLPlatformUtils::Terminate();
    }
};

#pragma warning(default: 4127)

#endif // #ifdef XERCES_XML

#endif // #ifndef __XERCES_COMMON_H__
