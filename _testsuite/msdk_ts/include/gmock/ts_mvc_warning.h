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

#define CHECK_MVC_SUPPORT()                                                                                                               \
{                                                                                                                                         \
    if(g_tsOSFamily == MFX_OS_FAMILY_LINUX)                                                                                               \
    {                                                                                                                                     \
        g_tsLog << "[ SKIPPED ] MVC is not supported for linux platform\n";                                                               \
        return 0;                                                                                                                         \
    }                                                                                                                                     \
    else if(g_tsOSFamily == MFX_OS_FAMILY_UNKNOWN)                                                                                        \
    {                                                                                                                                     \
        g_tsLog << "WARNING:\n";                                                                                                          \
        g_tsLog << "The platform is not specified.\n";                                                                                    \
        g_tsLog << "If you are using Linux platform you have to get MVC tests failure because MVC is not supported for Linux platform!\n";\
        g_tsLog << "To skip the tests on the Linux platform you need to set environment variable \"TS_PLATFORM\" before running tests.\n";\
        g_tsLog << "For example for Linux paltform the variable can be \"TS_PLATFORM=c7.2_bdw_64\"\n";                                    \
    }                                                                                                                                     \
}

