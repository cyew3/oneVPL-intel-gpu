/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#pragma once

#include <ipp.h>

double CalcMSE(const Ipp8u* ref_frame, int ref_pitch, const Ipp8u* src_frame, int src_pitch, IppiSize roi_size);

double MSEToPSNR(double mse, double max_err);
