//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2012 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#include <libmfx_core_factory.h>
#include <libmfx_core.h>

#if defined (MFX_VA_WIN)
#include <libmfx_core_d3d9.h>
    #if defined (MFX_D3D11_ENABLED)
    #include "umc_va_dxva2_protected.h"
    #include <libmfx_core_d3d11.h>
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
    adapterNum;
    switch(va_type)
    {
    case MFX_HW_NO:
        return new CommonCORE(numThreadsAvailable, session);
#if defined (MFX_VA_WIN) 
#if defined(MFX_D3D9_ENABLED)
    case MFX_HW_D3D9:
        return new D3D9VideoCORE(adapterNum, numThreadsAvailable, session);
#endif
#if defined (MFX_D3D11_ENABLED)
    case MFX_HW_D3D11:
        return new D3D11VideoCORE(adapterNum, numThreadsAvailable, session);
#endif
#elif defined(MFX_VA_LINUX)
    case MFX_HW_VAAPI:
        return new VAAPIVideoCORE(adapterNum, numThreadsAvailable, session);
#elif defined(MFX_VA_OSX)
    case MFX_HW_VDAAPI:
        return new VDAAPIVideoCORE(adapterNum, numThreadsAvailable, session);
#endif    
    default:
        return NULL;
    }

} // VideoCORE* FactoryCORE::CreateCORE(eMFXVAType va_type)
