/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: libmf_core_hw_vda.cpp

\* ****************************************************************************** */

#include "mfx_common.h"

#if defined(MFX_VA_OSX)

#include "umc_va_dxva2.h"
#include "libmfx_core_vdaapi.h"
#include "mfx_utils.h"
#include "mfx_session.h"
#include "mfx_check_hardware_support.h"
#include "ippi.h"

using namespace std;
using namespace UMC;

VideoAccelerationHW ConvertMFXToUMCType(eMFXHWType type)
{
    VideoAccelerationHW umcType = VA_HW_UNKNOWN;

    switch(type)
    {
    case MFX_HW_LAKE:
        umcType = VA_HW_LAKE;
        break;
    case MFX_HW_LRB:
        umcType = VA_HW_LRB;
        break;
    case MFX_HW_SNB:
        umcType = VA_HW_SNB;
        break;
    }

    return umcType;
}

VDAAPIVideoCORE::VDAAPIVideoCORE(const mfxU32 adapterNum, const mfxU32 numThreadsAvailable, const mfxSession session)
: CommonCORE(numThreadsAvailable, session)
, m_adapterNum(adapterNum)
, m_HWType(MFX_HW_SNB)
{
}

VDAAPIVideoCORE::~VDAAPIVideoCORE()
{
    Close();
}

bool IsHwMvcEncSupported()
{
    return false;
}

#endif //#if defined (MFX_VA_OSX)
