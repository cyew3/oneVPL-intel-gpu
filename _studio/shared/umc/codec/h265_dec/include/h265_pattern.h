/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"

#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __H265_PATTERN_H
#define __H265_PATTERN_H

#include "umc_h265_dec_defs_dec.h"
#include "h265_coding_unit.h"

namespace UMC_HEVC_DECODER
{

/// neighbouring pixel access class for one component
class H265PatternParam
{
private:
    Ipp32s m_OffsetLeft;
    Ipp32s m_OffsetAbove;

public:
    Ipp32s m_ROIWidth;
    Ipp32s m_ROIHeight;

    /// set parameters from Pel buffer for accessing neighbouring pixels
    void SetPatternParamPel(Ipp32s RoiWidth,
                            Ipp32s RoiHeight,
                            Ipp32s OffsetLeft,
                            Ipp32s OffsetAbove);

    /// set parameters of one color component from CU data for accessing neighbouring pixels
    void SetPatternParamCU(Ipp8u RoiWidth,
                           Ipp8u RoiHeight,
                           Ipp32s OffsetLeft,
                           Ipp32s OffsetAbove);
};

/// neighbouring pixel access class for all components
class H265Pattern
{
private:
    H265PatternParam  m_PatternY;
    H265PatternParam  m_PatternCb;
    H265PatternParam  m_PatternCr;
    static const Ipp8u m_IntraFilter[5];

public:

    H265PlanePtrYCommon GetPredictorPtr(Ipp32u DirMode, Ipp32u WidthBits, H265PlanePtrYCommon pAdiBuf);

    // -------------------------------------------------------------------------------------------------------------------
    // initialization functions
    // -------------------------------------------------------------------------------------------------------------------

    /// set parameters from Pel buffers for accessing neighbouring pixels
    void InitPattern(Ipp32s RoiWidth,
                     Ipp32s RoiHeight,
                     Ipp32s OffsetLeft,
                     Ipp32s OffsetAbove);

    /// set parameters from CU data for accessing neighbouring pixels
    void InitPattern(H265CodingUnit* pCU,
                      Ipp32u PartDepth,
                      Ipp32u AbsPartIdx);

    /// set luma parameters from CU data for accessing ADI data
    void InitAdiPatternLuma(H265CodingUnit* pCU,
                            Ipp32u ZorderIdxInPart,
                            Ipp32u PartDepth,
                            H265PlanePtrYCommon pAdiBuf,
                            Ipp32u OrgBufStride,
                            Ipp32u OrgBufHeight,
                            bool *NeighborFlags,
                            Ipp32s NumIntraNeighbor);

    /// set chroma parameters from CU data for accessing ADI data
    void InitAdiPatternChroma(H265CodingUnit* pCU,
                              Ipp32u ZorderIdxInPart,
                              Ipp32u PartDepth,
                              H265PlanePtrUVCommon pAdiBuf,
                              Ipp32u OrgBufStride,
                              Ipp32u OrgBufHeight,
                              bool *NeighborFlags,
                              Ipp32s NumIntraNeighbor);

private:

    /// padding of unavailable reference samples for intra prediction
    void FillReferenceSamplesLuma(Ipp32s bitDepth, H265PlanePtrYCommon pRoiOrigin, H265PlanePtrYCommon pAdiTemp, bool* NeighborFlags, Ipp32s NumIntraNeighbor, Ipp32u UnitSize, Ipp32s NumUnitsInCU, Ipp32s TotalUnits, Ipp32u CUSize, Ipp32u Size, Ipp32s PicStride);
    void FillReferenceSamplesChroma(Ipp32s bitDepth, H265PlanePtrUVCommon pRoiOrigin, H265PlanePtrUVCommon pAdiTemp, bool* NeighborFlags, Ipp32s NumIntraNeighbor, Ipp32u UnitSize, Ipp32s NumUnitsInCU, Ipp32s TotalUnits, Ipp32u CUSize, Ipp32u Size, Ipp32s PicStride);
};

} // end namespace UMC_HEVC_DECODER

#endif //__H265_PATTERN_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
