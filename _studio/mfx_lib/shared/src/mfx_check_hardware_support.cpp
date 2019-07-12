// Copyright (c) 2008-2019 Intel Corporation
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
    IGFX_KABYLAKE,
    IGFX_COFFEELAKE,
    IGFX_WILLOWVIEW,
    IGFX_APOLLOLAKE,
    IGFX_GEMINILAKE,
    IGFX_GLENVIEW,
    IGFX_GOLDWATERLAKE,
    IGFX_CANNONLAKE,
    IGFX_CNX_G,
    IGFX_ICELAKE,
    IGFX_ICELAKE_LP,
    IGFX_LAKEFIELD,
    IGFX_JASPERLAKE,
    IGFX_LAKEFIELD_R,
    IGFX_TIGERLAKE_LP,
    IGFX_RYEFIELD,
    IGFX_ROCKETLAKE,
    IGFX_ALDERLAKE_S,
    IGFX_ALDERLAKE_UH,
    IGFX_DG1 = 1210,
    IGFX_TIGERLAKE_HP = 1250,
    IGFX_DG2 = 1270,

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
    case IGFX_COFFEELAKE:
        return MFX_HW_CFL;
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
    case IGFX_CNX_G:
        return MFX_HW_CNX_G;
    case IGFX_LAKEFIELD:
    case IGFX_LAKEFIELD_R:
        return MFX_HW_LKF;
    case IGFX_JASPERLAKE:
        return MFX_HW_JSL;
    case IGFX_TIGERLAKE_LP:
        return MFX_HW_TGL_LP;
    case IGFX_RYEFIELD:
        return MFX_HW_RYF;
    case IGFX_ROCKETLAKE:
        return MFX_HW_RKL;
    case IGFX_DG1:
        return MFX_HW_DG1;
    case IGFX_TIGERLAKE_HP:
        return MFX_HW_TGL_HP;
    case IGFX_DG2:
        return MFX_HW_DG2;
    default:
        break;
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
