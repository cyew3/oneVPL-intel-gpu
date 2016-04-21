/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2011-2016 Intel Corporation. All Rights Reserved.
//
//
//          Platform switcher of Device Driver Interface for VPP
//          Platform specific driver implementation encapsulated by corresponding class
//          HW VPP component work with driver through abstract (DdiVPP) interface 
*/
/* ****************************************************************************** */

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP) 
#include "mfx_vpp_interface.h"

 
#if defined (MFX_VA_WIN)
    #include "fast_compositing_ddi.h"

    
    #if defined (MFX_D3D11_ENABLED)
        #include "d3d11_video_processor.h"
    #endif

#elif defined (MFX_VA_LINUX)
    #include "mfx_vpp_vaapi.h"

#elif defined (MFX_VA_MACOS)
    #include "mfx_vpp_ddi_macos.h"

#endif

using namespace MfxHwVideoProcessing;

// platform switcher
DriverVideoProcessing* MfxHwVideoProcessing::CreateVideoProcessing(VideoCORE* core)
{
    //MFX_CHECK_NULL_PTR1( core );
    //assert( core );
    core;
    
#if   defined (MFX_VA_WIN) // Windows DirectX9

    if (MFX_HW_D3D9 == core->GetVAType())
    {
        return new FastCompositingDDI(FASTCOMP_MODE_PRE_PROCESS);
    }
    #if defined (MFX_D3D11_ENABLED)
        else if( MFX_HW_D3D11 == core->GetVAType() )
        {
            return new D3D11VideoProcessor();
        }
    #endif
    else
    {
        return NULL;
    }
#elif defined (MFX_VA_LINUX)

    return new VAAPIVideoProcessing;

#elif defined (MFX_VA_MACOS)

    return new MacosVideoProcessing;

#else
    
    return NULL;

#endif
} // mfxStatus CreateVideoProcessing( VideoCORE* core )


mfxStatus VPPHWResMng::CreateDevice(VideoCORE * core){
    MFX_CHECK_NULL_PTR1(core);
    mfxStatus sts;
    Close();
    m_ddi.reset(MfxHwVideoProcessing::CreateVideoProcessing(core));

    if ( ! m_ddi.get() )
        return MFX_ERR_DEVICE_FAILED;

    mfxVideoParam par    = {0};
    par.vpp.In.Width     = 352;
    par.vpp.In.Height    = 288;
    par.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    par.vpp.In.FourCC    = MFX_FOURCC_NV12;
    par.vpp.Out          = par.vpp.In;

    sts = m_ddi->CreateDevice(core, &par, true);
    MFX_CHECK_STS(sts);

    sts = m_ddi->QueryCapabilities(m_caps);
    return sts;
}

#endif // ifdefined (MFX_ENABLE_VPP)
/* EOF */
