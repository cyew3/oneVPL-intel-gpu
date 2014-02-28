/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: mf_hw_platform.h
 
\* ****************************************************************************** */

#pragma once

#include "mfxdefs.h"
#include <map>

#if MFX_D3D11_SUPPORT
    #include "d3d11.h"
    #include "atlbase.h"
#endif


struct  HWPlatform  {
    enum HWType
    {
        UNKNOWN   = 0,
        LAKE      = 0x100000,
        LRB       = 0x200000,
        SNB       = 0x300000,

        IVB       = 0x400000,

        HSW       = 0x500000,
        HSW_ULT   = 0x500001,

        VLV       = 0x600000,

        BDW       = 0x700000,
        SCL       = 0x800000
    };
    enum {
        //default system adapter
        ADAPTER_DEFAULT = 0,
        //enumerator used to find-out suitable intel adapter 
        ADAPTER_ANY = 0xFFFFFFFF,
    };

    // Declare functions to check hardware
    static HWType Type(const mfxU32 adapterNum = ADAPTER_ANY);

private:
#if MFX_D3D11_SUPPORT
    static CComPtr<ID3D11Device> CreateD3D11Device(IDXGIAdapter * pAdapter);
    static mfxStatus GetIntelDataPrivateReport(ID3D11VideoDevice * device, const GUID guid, D3D11_VIDEO_DECODER_CONFIG & config);
#endif
    enum 
    {
        EAGLELAKE_G45_ID0  = 0x02e20,
        EAGLELAKE_G45_ID1  = 0x02e21,
        EAGLELAKE_G45_ID2  = 0x02e22,

        EAGLELAKE_GM45_ID0 = 0x02a40,
        EAGLELAKE_GM45_ID1 = 0x02a41,

        IRONLAKE_P57_ID0   = 0x00042,
        IRONLAKE_P57_ID1   = 0x00046,

        LRB_ID0            = 0x02240,

        SNB_ID0            = 0x02a42
    };

    typedef enum {
        IGFX_UNKNOWN = 0,
        IGFX_GRANTSDALE_G,
        IGFX_ALVISO_G,
        IGFX_LAKEPORT_G,
        IGFX_CALISTOGA_G,
        IGFX_BROADWATER_G,
        IGFX_CRESTLINE_G,
        IGFX_BEARLAKE_G,
        IGFX_CANTIGA_G,
        IGFX_CEDARVIEW_G,
        IGFX_EAGLELAKE_G,
        IGFX_IRONLAKE_G,
        IGFX_GT,
        IGFX_IVYBRIDGE,    
        IGFX_HASWELL,
        IGFX_VALLEYVIEW,
        IGFX_BROADWELL,
        IGFX_CHERRYVIEW,
        IGFX_SKYLAKE,
        IGFX_GENNEXT,
        PRODUCT_FAMILY_FORCE_ULONG = 0x7fffffff
    } PRODUCT_FAMILY;

    static PRODUCT_FAMILY GetPlatformFromDriverViaD3D11(mfxU32 nAdapter);
    static PRODUCT_FAMILY GetPlatfromFromDriverViaD3D9(mfxU32 nAdapter) ;
    static std::map<mfxU32, mfxU32> m_platformForAdapter;
};

