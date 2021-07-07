// Copyright (c) 2011-2019 Intel Corporation
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

/* ****************************************************************************** */

#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_ENCODE)

#include "mfx_h264_encode_interface.h"
#if defined (MFX_VA_WIN)
#include "mfxvideo++int.h"
#include "mfx_h264_encode_d3d9.h"

#if defined  (MFX_D3D11_ENABLED)
    #include "mfx_h264_encode_d3d11.h"
#endif

#else
    #include "mfx_h264_encode_vaapi.h"
#endif


using namespace MfxHwH264Encode;
// platform switcher

// tmp solution
#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
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
#else
    (void)core;

    return new VAAPIEncoder;//( core );
#endif

} // DriverEncoder* MfxHwH264Encode::CreatePlatformH264Encoder( VideoCORE* core )

#endif // #if defined (MFX_ENABLE_H264_VIDEO_ENCODE)
/* EOF */
