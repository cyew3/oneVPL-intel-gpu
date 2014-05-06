/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2013 Intel Corporation. All Rights Reserved.
//
//
//          Platform switcher of Device Driver Interface for MJPEG Encoder
//          Platform specific driver implementation encapsulated by corresponding class
//          HW MPEG Encoder component work with driver through abstract (DriverEncoder) interface 
*/
/* ****************************************************************************** */

#include "mfx_common.h"

#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined(MFX_VA_WIN)

#include "mfx_mjpeg_encode_interface.h"

#if defined (MFX_VA_WIN)   
#include "mfxvideo++int.h"
#include "mfx_mjpeg_encode_d3d9.h"   

#if defined  (MFX_D3D11_ENABLED)
    #include "mfx_mjpeg_encode_d3d11.h"    
#endif
#endif


using namespace MfxHwMJpegEncode;
// platform switcher

DriverEncoder* MfxHwMJpegEncode::CreatePlatformMJpegEncoder( VideoCORE* core )
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
#else
    return NULL;
#endif

} // DriverEncoder* MfxHwMJpegEncode::CreatePlatformMJpegEncoder( VideoCORE* core )

#endif // #if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE)
/* EOF */
