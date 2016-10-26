//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2012 Intel Corporation. All Rights Reserved.
//

#if !defined(_MFX_CHECK_HARDWARE_SUPPORT_H)
#define _MFX_CHECK_HARDWARE_SUPPORT_H

#if defined (MFX_VA)
#include "mfxvideo++int.h"

namespace MFX
{

// Declare functions to check hardware
eMFXHWType GetHardwareType(const mfxU32 adapterNum, mfxU32 platformFromDriver);

} // namespace MFX

#endif // #if defined (MFX_VA)

#endif // _MFX_CHECK_HARDWARE_SUPPORT_H
