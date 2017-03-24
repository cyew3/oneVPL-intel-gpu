//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2017 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_VA_WIN)
//#define SET_ALWAYS_DEFAULT
// MFX_HW_IVB = 0x400000, MFX_HW_HSW = 0x500000, etc
//#define DEFAULT_HW_TYPE (eMFXHWType)0x400000

#ifndef DEFAULT_HW_TYPE
#define DEFAULT_HW_TYPE (eMFXHWType)0
#endif

#include <mfx_check_hardware_support.h>

#include <mfxdefs.h>
#include <mfx_dxva2_device.h>
#include <mfx_common.h>

#pragma warning(disable : 4702)

// declare static data
namespace
{

const
mfxU32 IntelVendorID = 0x08086;

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

enum PRODUCT_FAMILY
{
    IGFX_UNKNOWN       = 0,

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
    IGFX_KABYLAKE,
    IGFX_WILLOWVIEW,
    IGFX_APOLLOLAKE,
    IGFX_GEMINILAKE,
    IGFX_GLENVIEW,
    IGFX_GOLDWATERLAKE,
    IGFX_CANNONLAKE,
    IGFX_CNX_G,
    IGFX_ICELAKE,
    IGFX_ICELAKE_LP,
    IGFX_RESERVED_1,
    IGFX_RESERVED_2,
    IGFX_TIGERLAKE,

    IGFX_SOFIA_LTE1 = 1001,
    IGFX_SOFIA_LTE2 = 1002,

    IGFX_GENNEXT               = 0x7ffffffe,

    PRODUCT_FAMILY_FORCE_ULONG = 0x7fffffff
};

} // namespace

namespace MFX
{

eMFXHWType GetHardwareType(const mfxU32 adapterNum, mfxU32 platformFromDriver)
{

    switch(platformFromDriver)
    {
    case IGFX_HASWELL:
        return MFX_HW_HSW_ULT;
    case IGFX_IVYBRIDGE:
        return MFX_HW_IVB;
    case IGFX_VALLEYVIEW:
        return MFX_HW_VLV;
    case IGFX_SKYLAKE:
        return MFX_HW_SCL;
    case IGFX_BROADWELL:
        return MFX_HW_BDW;
    case IGFX_CHERRYVIEW:
        return MFX_HW_CHT;
    case IGFX_GT:
        return MFX_HW_IVB; // sandybridge
    case IGFX_KABYLAKE:
        return MFX_HW_KBL;
    case IGFX_APOLLOLAKE:
        return MFX_HW_APL;
    case IGFX_GEMINILAKE:
        return MFX_HW_GLK;
    case IGFX_CANNONLAKE:
        return MFX_HW_CNL;
    case IGFX_ICELAKE:
        return MFX_HW_ICL;
    case IGFX_ICELAKE_LP:
        return MFX_HW_ICL_LP;
    case IGFX_TIGERLAKE:
        return MFX_HW_TGL;
#if defined (PRE_SI_TARGET_PLATFORM_GEN12)
    default:
      return MFX_HW_TGL;  // temporarily for Gen12 pre-si
#elif defined (PRE_SI_TARGET_PLATFORM_GEN11)
    default:
      return MFX_HW_ICL;  // temporarily for Gen11 pre-si
#endif
    }


    // if we were not able to find right device via PrivateGUID let base on defines and check HW capabilities
    DXGI1Device dxgiDevice;
    //mfxU32 deviceID;
    eMFXHWType type = (eMFXHWType)DEFAULT_HW_TYPE;

    // initialize the DXVA2 device
    if (false == dxgiDevice.Init(adapterNum))
    {
        return MFX_HW_UNKNOWN;
    }

    // investigate the device
    if (IntelVendorID != dxgiDevice.GetVendorID())
    {
        return MFX_HW_UNKNOWN;
    }


#ifdef MFX_SNB_PLATFORM
    if (DEFAULT_HW_TYPE == type)
        return MFX_HW_SNB;
    else // something wrong in pipeline
        return MFX_HW_UNKNOWN;
#endif

#ifdef MFX_IVB_PLATFORM
    if (DEFAULT_HW_TYPE == type)
        return MFX_HW_IVB;
    else // something wrong in pipeline
        return MFX_HW_UNKNOWN;
#endif

#ifdef MFX_HSW_PLATFORM
    if (DEFAULT_HW_TYPE == type)
        return MFX_HW_HSW;
    else // something wrong in pipeline
        return MFX_HW_UNKNOWN;
#endif

#ifdef MFX_HSW_ULT_PLATFORM
    if (DEFAULT_HW_TYPE == type)
        return MFX_HW_HSW_ULT;
    else // something wrong in pipeline
        return MFX_HW_UNKNOWN;
#endif

#ifdef MFX_VLV_PLATFORM
    if (DEFAULT_HW_TYPE == type)
        return MFX_HW_VLV;
    else // something wrong in pipeline
        return MFX_HW_UNKNOWN;
#endif

// HSW by default for trunk
    type = MFX_HW_HSW_ULT;

#ifdef SET_ALWAYS_DEFAULT
    type = (eMFXHWType)DEFAULT_HW_TYPE;
#endif


    return type;

} // eMFXHWType GetHardwareType(const mfxU32 adapterNum)

} // namespace MFX

#endif // #if defined (MFX_VA_WIN)
