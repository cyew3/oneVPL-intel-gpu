//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined(MFX_VA)

#ifndef __LIBMFX_CORE__HW_H__
#define __LIBMFX_CORE__HW_H__

#include "umc_va_base.h"

mfxU32 ChooseProfile(mfxVideoParam * param, eMFXHWType hwType);
bool IsHwMvcEncSupported();

#endif

#endif

