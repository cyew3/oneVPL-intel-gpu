/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_ddi.cpp

\* ****************************************************************************** */

#include "mfx_screen_capture_ddi.h"
#include "mfx_screen_capture_d3d9.h"
#include "mfx_screen_capture_d3d11.h"
#include "mfx_screen_capture_sw_d3d9.h"
#include "mfx_screen_capture_sw_d3d11.h"

namespace MfxCapture
{

Capturer* CreatePlatformCapturer(mfxCoreInterface* core)
{
    try
    {
        if (core)
        {
            mfxCoreParam par = {}; 

            if (core->GetCoreParam(core->pthis, &par))
                return 0;

            if(MFX_IMPL_SOFTWARE != MFX_IMPL_BASETYPE(par.Impl))
            {
                switch(par.Impl & 0xF00)
                {
                case MFX_IMPL_VIA_D3D9:
                    return new D3D9_Capturer(core);
                case MFX_IMPL_VIA_D3D11:
                    return new D3D11_Capturer(core);
                default:
                    return 0;
                }
            }
            else
            {
                return new SW_D3D9_Capturer(core);
            }
        }
    }
    catch(...)
    {
        return 0;
    }

    return 0;
}

Capturer* CreateSWCapturer(mfxCoreInterface* core)
{
    try
    {
        if (core)
        {
            return new SW_D3D9_Capturer(core);
        }
    }
    catch(...)
    {
        return 0;
    }

    return 0;
}

DXGI_FORMAT MfxFourccToDxgiFormat(const mfxU32& fourcc)
{
    switch (fourcc)
    {
        case MFX_FOURCC_NV12:
            return DXGI_FORMAT_NV12;
        case MFX_FOURCC_RGB4:
        case DXGI_FORMAT_AYUV:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        default:
            return DXGI_FORMAT_UNKNOWN;
    }
}

D3DFORMAT MfxFourccToD3dFormat(const mfxU32& fourcc)
{
    switch (fourcc)
    {
        case MFX_FOURCC_NV12:
            return D3DFMT_NV12;
        case MFX_FOURCC_RGB4:
            return D3DFMT_A8R8G8B8;
        default:
            return D3DFMT_UNKNOWN;
    }
}

} //namespace MfxCapture