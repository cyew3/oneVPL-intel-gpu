/******************************************************************************\
Copyright (c) 2005-2016, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#pragma once

#include "mfx_filter_guid.h"
#include "current_date.h"
#include "mfx_filter_externals_main.h"

#ifdef FILTER_NAME

#ifdef FILTER_INPUT
static const AMOVIESETUP_MEDIATYPE sudIn1PinTypes[] = { FILTER_INPUT };
#endif
#ifdef FILTER_INPUT2
static const AMOVIESETUP_MEDIATYPE sudIn2PinTypes[] = { FILTER_INPUT2 };
#endif
#ifdef FILTER_OUTPUT
static const AMOVIESETUP_MEDIATYPE sudOut1PinTypes[] = { FILTER_OUTPUT };
#endif
#ifdef FILTER_OUTPUT2
static const AMOVIESETUP_MEDIATYPE sudOut2PinTypes[] = { FILTER_OUTPUT2 };
#endif

static const AMOVIESETUP_PIN psudPins[] =
{
#ifdef FILTER_INPUT
    {
        L"Input",           // String pin name
        FALSE,              // Is it rendered
        FALSE,              // Is it an output
        FALSE,              // Allowed none
        FALSE,              // Allowed many
        &CLSID_NULL,        // Connects to filter
        L"Output",          // Connects to pin
        ARRAY_SIZE(sudIn1PinTypes), // Number of types
        sudIn1PinTypes       // The pin details
    },
#endif
#ifdef FILTER_INPUT2
    {
        L"Input2",           // String pin name
        FALSE,              // Is it rendered
        FALSE,              // Is it an output
        FALSE,              // Allowed none
        FALSE,              // Allowed many
        &CLSID_NULL,        // Connects to filter
        L"Output",          // Connects to pin
        ARRAY_SIZE(sudIn2PinTypes), // Number of types
        sudIn2PinTypes       // The pin details
    },
#endif
#ifdef FILTER_OUTPUT
    {
        L"Output",          // String pin name
        FALSE,              // Is it rendered
        TRUE,               // Is it an output
        FALSE,              // Allowed none
        FALSE,              // Allowed many
        &CLSID_NULL,        // Connects to filter
        L"Input",           // Connects to pin
        ARRAY_SIZE(sudOut1PinTypes), // Number of types
        sudOut1PinTypes      // The pin details
    },
#endif
#ifdef FILTER_OUTPUT2
    {
        L"Output2",         // String pin name
        FALSE,              // Is it rendered
        TRUE,               // Is it an output
        FALSE,              // Allowed none
        FALSE,              // Allowed many
        &CLSID_NULL,        // Connects to filter
        L"Input",           // Connects to pin
        ARRAY_SIZE(sudOut2PinTypes), // Number of types
        sudOut2PinTypes     // The pin details
    }
#endif
};

#ifndef FILTER_MERIT
#define FILTER_MERIT MERIT_PREFERRED
#endif

static const AMOVIESETUP_FILTER sudFilter =
{
    &FILTER_GUID,
    FILTER_NAME,
    FILTER_MERIT,
    ARRAY_SIZE(psudPins),   // Number of pins
    psudPins                    // Pin details
};

// List of class IDs and creator functions for the class factory. This
// provides the link between the OLE entry point in the DLL and an object
// being created. The class factory will call the static CreateInstance

CFactoryTemplate g_Templates[] =
{
    {
        FILTER_NAME,
        &FILTER_GUID,
        FILTER_CREATE,
        NULL,
        &sudFilter
    }
#ifdef FILTER_PROPERTIES1
    , FILTER_PROPERTIES1
#endif
#ifdef FILTER_PROPERTIES2
    , FILTER_PROPERTIES2
#endif
#ifdef FILTER_PROPERTIES3
    , FILTER_PROPERTIES3
#endif
};

// Number of objects listed in g_Templates
int g_cTemplates = ARRAY_SIZE(g_Templates);

#endif //ifdef FILTER_NAME

/******************************************************************************/

STDAPI DllRegisterServer()
{
    return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer()
{
    return AMovieDllRegisterServer2(FALSE);
}

//
// DllEntryPoint
//
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
    return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}
