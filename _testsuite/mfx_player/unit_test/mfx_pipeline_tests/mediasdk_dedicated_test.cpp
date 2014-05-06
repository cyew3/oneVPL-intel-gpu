/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */


#include "utest_common.h"

SUITE(Mediasdk)
{
#ifdef MFX_TEST_LIBARARY_LOADER
    TEST(d3d11_single_texture)
    {
        for (size_t i = 0; i < 10; i++)
        {
            mfxSession session = 0;
            HMODULE hdl = LoadLibraryA("libmfxhw32_d.dll");
            FreeLibrary(hdl);

            CHECK_EQUAL(MFX_ERR_NONE, MFXInit(MFX_IMPL_HARDWARE, NULL, &session));
            CHECK_EQUAL(MFX_ERR_NONE, MFXClose(session));
        }
    }
#endif
}