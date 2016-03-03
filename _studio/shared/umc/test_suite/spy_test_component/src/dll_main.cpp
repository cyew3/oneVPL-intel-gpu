/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2016 Intel Corporation. All Rights Reserved.
//
*/

#include "ippcore.h"

#if defined(__GNUC__)
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <sys/types.h>
#include <stdio.h>
#include <dlfcn.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved)
{
    // touch unreferenced parameters
    hModule = hModule;
    lpReserved = lpReserved;

    // initialize the IPP library
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        ippInit();
        break;

    default:
        break;
    }

    return TRUE;

} // BOOL APIENTRY DllMain(HMODULE hModule,

#else // #if defined(_WIN32) || defined(_WIN64)
void __attribute__ ((constructor)) dll_init(void)
{
    ippStaticInit();
}
#endif // #if defined(_WIN32) || defined(_WIN64)
