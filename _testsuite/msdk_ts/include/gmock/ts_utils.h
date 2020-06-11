/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018-2020 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#pragma once

#if !defined(MSDK_ALIGN16)
#define MSDK_ALIGN16(value)                      (((value + 15) >> 4) << 4) // round up to a multiple of 16
#endif
#if !defined(MSDK_ALIGN32)
#define MSDK_ALIGN32(value)                      (((value + 31) >> 5) << 5) // round up to a multiple of 32
#endif

#include <ipp.h>

double CalcMSE(const Ipp8u* ref_frame, int ref_pitch, const Ipp8u* src_frame, int src_pitch, IppiSize roi_size);

double MSEToPSNR(double mse, double max_err);
