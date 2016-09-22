/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//         Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

#define CHECK_FEI_SUPPORT()                                                                                                                     \
{                                                                                                                                               \
    if(g_tsOSFamily == MFX_OS_FAMILY_WINDOWS)                                                                                                   \
    {                                                                                                                                           \
        g_tsLog << "[ SKIPPED ] FEI is not supported for windows platform\n";                                                                   \
        return 0;                                                                                                                               \
    }                                                                                                                                           \
    else if(g_tsOSFamily == MFX_OS_FAMILY_UNKNOWN)                                                                                              \
    {                                                                                                                                           \
        g_tsLog << "WARNING:\n";                                                                                                                \
        g_tsLog << "The platform is not specified.\n";                                                                                          \
        g_tsLog << "If you are using Windows platform you have to get FEI tests failure because FEI is not supported for Windows platform!\n";  \
        g_tsLog << "To skip the tests on the Windows platform you need to set environment variable \"TS_PLATFORM=win\" before running tests.\n";\
        g_tsLog << "You can do it in the msdk_gmock project properties: Configuration Properties - Debbuging tab - Environment field\n";        \
    }                                                                                                                                           \
}

