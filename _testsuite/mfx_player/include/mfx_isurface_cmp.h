/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once


class IMFXSurfacesComparator
    : public ICloneable
{
    DECLARE_CLONE(IMFXSurfacesComparator);
public:
    virtual mfxStatus Reset() = 0;
    virtual mfxStatus Compare(mfxFrameSurface1 * pIn1, mfxFrameSurface1 * pIn2) = 0;
    virtual mfxStatus GetLastCmpResult(mfxF64 pResult[3]) = 0;
    virtual mfxStatus GetOveralResult (mfxF64 pResult[3], mfxU32 fourcc) = 0;
    virtual mfxStatus GetAverageResult(mfxF64 pResult[3]) = 0;
    virtual mfxStatus GetMaxResult(mfxF64 pResult[3]) = 0;
    virtual mfxStatus GetMinResult(mfxF64 pResult[3]) = 0;
    virtual vm_char*  GetMetricName() = 0;
};
