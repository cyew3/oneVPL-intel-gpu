/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_utils.h"

#include <cmath>
#include <algorithm>

double CalcMSE(const Ipp8u* ref_frame, int ref_pitch, const Ipp8u* src_frame, int src_pitch, IppiSize roi_size)
{
    double mse = 0.0;
    ippiNormDiff_L2_8u_C1R(ref_frame, ref_pitch, src_frame, src_pitch, roi_size, &mse);

    return (mse * mse) / (double)(roi_size.width * roi_size.height);
}

double MSEToPSNR(double mse, double max_err)
{
    if (mse < 0)    return -1.0;
    if (mse == 0.0) return 1000.0; // MAX_PSNR = 1000.0;

    return (std::min) (10.0 * log10(max_err * max_err / mse), 1000.0);
}
