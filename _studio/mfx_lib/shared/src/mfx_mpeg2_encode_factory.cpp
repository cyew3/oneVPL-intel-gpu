//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2016 Intel Corporation. All Rights Reserved.
//

/* ****************************************************************************** */

#include "mfx_common.h"

#if defined(MFX_VA)
#if defined(MFX_ENABLE_MPEG2_VIDEO_ENCODE) || defined(MFX_ENABLE_MPEG2_VIDEO_ENC)

#include "mfx_mpeg2_encode_interface.h"

#if defined (MFX_VA_WIN)   
#include "mfxvideo++int.h"
#include "mfx_mpeg2_encode_d3d9.h"   

#if defined(MFX_D3D11_ENABLED)
    #include "mfx_mpeg2_encode_d3d11.h"
#endif

#elif defined (MFX_VA_LINUX)
    #include "mfx_mpeg2_encode_vaapi.h"
#endif


using namespace MfxHwMpeg2Encode;

DriverEncoder* MfxHwMpeg2Encode::CreatePlatformMpeg2Encoder( VideoCORE* core )
{    
    assert( core );

#if   defined (MFX_VA_WIN) 

    if(MFX_HW_D3D9 == core->GetVAType())
    {       
        return new D3D9Encoder( core );
    }
    #if defined  (MFX_D3D11_ENABLED)
        else if( MFX_HW_D3D11 == core->GetVAType() )
        {
            return new D3D11Encoder( core );
        }
    #endif
    else
    {
        return NULL;
    }

#elif defined (MFX_VA_LINUX)

    return new VAAPIEncoder(core);

#elif defined (MFX_VA_OSX)

    return NULL;//new MacosDdiEncoder( core );

#endif

} // DriverEncoder* MfxHwMpeg2Encode::CreatePlatformMpeg2Encoder( VideoCORE* core )

#endif // #if defined(MFX_ENABLE_MPEG2_VIDEO_ENCODE) || defined(MFX_ENABLE_MPEG2_VIDEO_ENC)
#endif // #if defined(MFX_VA)
/* EOF */
