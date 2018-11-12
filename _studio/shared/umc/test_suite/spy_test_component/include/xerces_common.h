// Copyright (c) 2003-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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
