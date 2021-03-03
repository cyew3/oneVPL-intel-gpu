// Copyright (c) 2011-2020 Intel Corporation
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

#include "mfx_common.h"
#include <libmfx_core_factory.h>
#include <libmfx_core.h>

#if defined (MFX_VA_WIN)
#include <libmfx_core_d3d9.h>
    #if defined (MFX_D3D11_ENABLED)
    #include "umc_va_dxva2_protected.h"
    #include <libmfx_core_d3d11.h>
        #if defined(MFX_DX9ON11)
            #include <libmfx_core_d3d9on11.h>
        #endif
    #endif
#elif defined(MFX_VA_LINUX)
#include <libmfx_core_vaapi.h>
#elif defined(MFX_VA_OSX)
#include <libmfx_core_vdaapi.h>
#endif


VideoCORE* FactoryCORE::CreateCORE(eMFXVAType va_type,
                                   mfxU32 adapterNum,
                                   mfxU32 numThreadsAvailable,
                                   mfxSession session)
{
    (void)adapterNum;

    bool create_msdk20_core = mfx::GetEnv("MFX_CORE_INTERNAL_ALLOCATION_API_ENABLE", true);
    std::ignore = create_msdk20_core;

    switch (va_type)
    {
    case MFX_HW_NO:
#if defined(MFX_ONEVPL)
        if (create_msdk20_core)
            return new CommonCORE20(numThreadsAvailable, session);
#endif

        return new CommonCORE(numThreadsAvailable, session);

#if defined (MFX_VA_WIN)
#if defined(MFX_D3D9_ENABLED)
    case MFX_HW_D3D9:
        return new D3D9VideoCORE(adapterNum, numThreadsAvailable, session);
#endif
#if defined (MFX_D3D11_ENABLED)
    case MFX_HW_D3D11:
#if defined(MFX_ONEVPL)
        if (create_msdk20_core)
            return new D3D11VideoCORE20(adapterNum, numThreadsAvailable, session);
#endif

        return new D3D11VideoCORE(adapterNum, numThreadsAvailable, session);
#if defined (MFX_DX9ON11)
    case MFX_HW_D3D9ON11:
        return new D3D9ON11VideoCORE(adapterNum, numThreadsAvailable, session);
#endif
#endif
#elif defined(MFX_VA_LINUX)
    case MFX_HW_VAAPI:
#if defined(MFX_ONEVPL)
        if (create_msdk20_core)
            return new VAAPIVideoCORE20(adapterNum, numThreadsAvailable, session);
#endif

        return new VAAPIVideoCORE(adapterNum, numThreadsAvailable, session);
#elif defined(MFX_VA_OSX)
    case MFX_HW_VDAAPI:
        return new VDAAPIVideoCORE(adapterNum, numThreadsAvailable, session);
#endif
    default:
        return nullptr;
    }

} // VideoCORE* FactoryCORE::CreateCORE(eMFXVAType va_type)
