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
    (void)core;

    return new VAAPIEncoder;//( core );

#elif defined (MFX_VA_OSX)
    (void)core;

    return NULL;

#endif

} // DriverEncoder* MfxHwH264Encode::CreatePlatformH264Encoder( VideoCORE* core )

#if defined(MFX_ENABLE_MFE) && !defined(AS_H264LA_PLUGIN)

#if !defined(MFX_VA_WIN)

MFEVAAPIEncoder* MfxHwH264Encode::CreatePlatformMFEEncoder(VideoCORE* core)
{
    assert( core );

    // needs to search, thus use special GUID
    ComPtrCore<MFEVAAPIEncoder> *pVideoEncoder = QueryCoreInterface<ComPtrCore<MFEVAAPIEncoder> >(core, MFXMFEAVCENCODER_SEARCH_GUID);
    if (!pVideoEncoder) return NULL;
    if (!pVideoEncoder->get())
        *pVideoEncoder = new MFEVAAPIEncoder;

    return pVideoEncoder->get();

} // MFEVAAPIEncoder* MfxHwH264Encode::CreatePlatformMFEEncoder( VideoCORE* core )

#elif defined(PRE_SI_TARGET_PLATFORM_GEN12P5)

MFEDXVAEncoder* MfxHwH264Encode::CreatePlatformMFEEncoder(VideoCORE* core)
{
    assert(core);

    // needs to search, thus use special GUID
    ComPtrCore<MFEDXVAEncoder> *pVideoEncoder = QueryCoreInterface<ComPtrCore<MFEDXVAEncoder> >(core, MFXMFEAVCENCODER_SEARCH_GUID);
    if (!pVideoEncoder) return NULL;
    if (!pVideoEncoder->get())
        *pVideoEncoder = new MFEDXVAEncoder;

    return pVideoEncoder->get();

} // MFEVAAPIEncoder* MfxHwH264Encode::CreatePlatformMFEEncoder( VideoCORE* core )

#endif
#endif


#endif // #if defined (MFX_ENABLE_H264_VIDEO_ENCODE_HW)
/* EOF */
