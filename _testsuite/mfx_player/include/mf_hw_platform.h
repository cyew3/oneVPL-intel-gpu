/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2017 Intel Corporation. All Rights Reserved.

File Name: mf_hw_platform.h

\* ****************************************************************************** */

#pragma once

#if defined(_WIN32) || defined(_WIN64)

#include "mfxdefs.h"
#include <map>

#include "mfx_pipeline_features.h"
//#define MFX_D3D11_SUPPORT 1

#if MFX_D3D11_SUPPORT
    #include "d3d11.h"
    #include "atlbase.h"
#endif

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
    IGFX_WILLOWVIEW,
    IGFX_APOLLOLAKE,
    IGFX_CANNONLAKE,
    IGFX_SOFIA_LTE1 = 1001,
    IGFX_SOFIA_LTE2 = 1002,
    IGFX_GENNEXT                            = 0x7ffffffe,
    PRODUCT_FAMILY_FORCE_ULONG = 0x7fffffff
} PRODUCT_FAMILY;

typedef enum {
    IGFX_SKU_UNKNOWN            = 0,
    IGFX_SKU_ULX                = 1,
    IGFX_SKU_ULT                = 2,
    IGFX_SKU_T                  = 3,
    IGFX_SKU_ALL                = 0xff
} PLATFORM_SKU;

#define IS_PLATFORM_SOFIA(P) ((P) == IGFX_SOFIA_LTE1 || (P) == IGFX_SOFIA_LTE2)

struct PLATFORM_TYPE {
    PRODUCT_FAMILY  ProductFamily;
    PLATFORM_SKU    PlatformSku;
    //PLATFORM_TYPE(PRODUCT_FAMILY family = IGFX_UNKNOWN, PLATFORM_SKU sku = IGFX_SKU_UNKNOWN) : ProductFamily(family), PlatformSku(sku) {}
    PLATFORM_TYPE(PRODUCT_FAMILY family, PLATFORM_SKU sku) : ProductFamily(family), PlatformSku(sku) {}
    PLATFORM_TYPE() : ProductFamily(IGFX_UNKNOWN), PlatformSku(IGFX_SKU_UNKNOWN) {}
    bool operator==(const PLATFORM_TYPE& a) const
    {
        return (ProductFamily == a.ProductFamily && PlatformSku == a.PlatformSku);
    }
    bool operator!=(const PLATFORM_TYPE& a) const
    {
        return !(operator==(a));
    }
};
extern const PLATFORM_TYPE IGFX_UNKNOWN_PLATFORMTYPE;

class  HWPlatform  {
public:
    enum {
        //default system adapter
        ADAPTER_DEFAULT = 0,
        //enumerator used to find-out suitable intel adapter
        ADAPTER_ANY = 0xFFFFFFFF,
    };

    static PLATFORM_TYPE CachePlatformType(const mfxU32 adapterNum, IUnknown* device = NULL);

    // Declare functions to check hardware
    static PLATFORM_TYPE GetPlatformType(const mfxU32 adapterNum = ADAPTER_ANY); //ProductFamily, PlatformSku
    inline static PRODUCT_FAMILY GetProductFamily(const mfxU32 adapterNum = ADAPTER_ANY) {return GetPlatformType(adapterNum).ProductFamily;}
    inline static PLATFORM_SKU   GetPlatformSku  (const mfxU32 adapterNum = ADAPTER_ANY) {return GetPlatformType(adapterNum).PlatformSku;}

#if MFX_D3D11_SUPPORT
    static CComPtr<ID3D11Device> CreateD3D11Device(IDXGIAdapter * pAdapter);
    static mfxStatus GetIntelDataPrivateReport(ID3D11VideoDevice * device, const GUID guid, D3D11_VIDEO_DECODER_CONFIG & config);
#endif
private:
    static PLATFORM_TYPE GetPlatformFromDriverViaD3D11(IUnknown* deviceUnknown);
    static PLATFORM_TYPE GetPlatformFromDriverViaD3D11(mfxU32 nAdapter);
    static PLATFORM_TYPE GetPlatfromFromDriverViaD3D9(IUnknown* deviceUnknown);
    static PLATFORM_TYPE GetPlatfromFromDriverViaD3D9(mfxU32 nAdapter);

    static mfxU32 m_AdapterAny;
    typedef std::map<mfxU32, PLATFORM_TYPE> CachedPlatformTypes;
    static CachedPlatformTypes m_platformForAdapter;
};
#endif
