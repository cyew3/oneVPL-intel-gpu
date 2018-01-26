//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2018 Intel Corporation. All Rights Reserved.
//

/* ****************************************************************************** */

#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined (MFX_VA)

#include "mfx_h264_encode_interface.h"
#ifdef MFX_ENABLE_MFE
#include "libmfx_core_interface.h"
#endif
#if defined (MFX_VA_WIN)
#include "mfxvideo++int.h"
#include "mfx_h264_encode_d3d9.h"

#if defined  (MFX_D3D11_ENABLED)
    #include "mfx_h264_encode_d3d11.h"
#endif

#elif defined (MFX_VA_LINUX)
    #include "mfx_h264_encode_vaapi.h"

#elif defined (MFX_VA_OSX)
    //#include "mfx_h264_encode_macos.h"

#endif


using namespace MfxHwH264Encode;
// platform switcher

// tmp solution
#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE_HW
DriverEncoder* MfxHwH264Encode::CreatePlatformSvcEncoder( VideoCORE * core )
{
    assert( core );
#ifdef MFX_VA_WIN
    if(MFX_HW_D3D9 == core->GetVAType())
    {
        return new D3D9SvcEncoder;//( core );
    }
  #ifdef MFX_D3D11_ENABLED
    else if (MFX_HW_D3D11 == core->GetVAType())
    {
        return new D3D11SvcEncoder;
    }
  #endif // MFX_D3D11_ENABLED
#endif // MFX_VA_WIN

    return 0;
}
#endif

DriverEncoder* MfxHwH264Encode::CreatePlatformH264Encoder( VideoCORE* core )
{
    //MFX_CHECK_NULL_PTR1( core );
    assert( core );

#if   defined (MFX_VA_WIN)
    if(MFX_HW_D3D9 == core->GetVAType())
    {
        return new D3D9Encoder;//( core );
    }
    #if defined  (MFX_D3D11_ENABLED)
        else if( MFX_HW_D3D11 == core->GetVAType() )
        {
            return new D3D11Encoder;//( core );
        }
    #endif
    else
    {
        return NULL;
    }
#elif defined (MFX_VA_LINUX)

    return new VAAPIEncoder;//( core );

#elif defined (MFX_VA_OSX)

    return NULL;

#endif

} // DriverEncoder* MfxHwH264Encode::CreatePlatformH264Encoder( VideoCORE* core )

#if defined(MFX_ENABLE_MFE) && !defined(AS_H264LA_PLUGIN)

#if !defined(MFX_VA_WIN)

MFEVAAPIEncoder* MfxHwH264Encode::CreatePlatformMFEEncoder(VideoCORE* core)
{
    assert( core );

    // needs to search, thus use special GUID
    ComPtrCore<MFEVAAPIEncoder> *pVideoEncoder = QueryCoreInterface<ComPtrCore<MFEVAAPIEncoder> >(core, MFXMFEDDIENCODER_SEARCH_GUID);
    if (!pVideoEncoder) return NULL;
    if (!pVideoEncoder->get())
        *pVideoEncoder = new MFEVAAPIEncoder;

    return pVideoEncoder->get();

} // MFEVAAPIEncoder* MfxHwH264Encode::CreatePlatformMFEEncoder( VideoCORE* core )

#else

MFEDXVAEncoder* MfxHwH264Encode::CreatePlatformMFEEncoder(VideoCORE* core)
{
    assert(core);

    // needs to search, thus use special GUID
    ComPtrCore<MFEDXVAEncoder> *pVideoEncoder = QueryCoreInterface<ComPtrCore<MFEDXVAEncoder> >(core, MFXMFEDDIENCODER_SEARCH_GUID);
    if (!pVideoEncoder) return NULL;
    if (!pVideoEncoder->get())
        *pVideoEncoder = new MFEDXVAEncoder;

    return pVideoEncoder->get();

} // MFEVAAPIEncoder* MfxHwH264Encode::CreatePlatformMFEEncoder( VideoCORE* core )

#endif
#endif


#endif // #if defined (MFX_ENABLE_H264_VIDEO_ENCODE_HW)
/* EOF */
