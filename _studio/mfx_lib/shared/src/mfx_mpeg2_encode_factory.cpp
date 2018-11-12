// Copyright (c) 2011-2018 Intel Corporation
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
