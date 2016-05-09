/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"

#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __H265_PREDICTION_H
#define __H265_PREDICTION_H

#include "umc_h265_dec_defs.h"
#include "umc_h265_yuv.h"
#include "umc_h265_motion_info.h"
#include "mfx_h265_optimization.h" // common data types here for interpolation

namespace UMC_HEVC_DECODER
{

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Class definition
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class DecodingContext;
class H265DecoderFrame;

// Prediction unit information data for motion compensation
struct H265PUInfo
{
    H265MVInfo *interinfo;
    H265DecoderFrame *refFrame[2];
    Ipp32u PartAddr;
    Ipp32u Width, Height;
};

// prediction class
class H265Prediction
{
public:

protected:
    PlanePtrY m_temp_interpolarion_buffer;
    Ipp32u m_MaxCUSize;

    DecodingContext* m_context;

    H265DecYUVBufferPadded m_YUVPred[2];

    // Interpolate one reference frame block
    template <EnumTextType c_plane_type, bool bi, typename H265PlaneYCommon>
    void inline H265_FORCEINLINE PredInterUni(H265CodingUnit* pCU, H265PUInfo &PUi, EnumRefPicList RefPicList, H265DecYUVBufferPadded *YUVPred, MFX_HEVC_PP::EnumAddAverageType eAddAverage = MFX_HEVC_PP::AVERAGE_NO);

    // Returns true if reference indexes in ref lists point to the same frame and motion vectors in these references are equal
    bool CheckIdenticalMotion(H265CodingUnit* pCU, const H265MVInfo &mvInfo);

    // Calculate mean average from two references
    template<typename PixType>
    static void WriteAverageToPic(
                     const PixType * in_pSrc0,
                     Ipp32u in_Src0Pitch,      // in samples
                     const PixType * in_pSrc1,
                     Ipp32u in_Src1Pitch,      // in samples
                     PixType* H265_RESTRICT in_pDst,
                     Ipp32u in_DstPitch,       // in samples
                     Ipp32s width,
                     Ipp32s height);

    // Copy prediction unit extending its bits for later addition with PU from another reference
    template <typename PixType>
    static void CopyExtendPU(const PixType * in_pSrc,
                     Ipp32u in_SrcPitch, // in samples
                     Ipp16s* H265_RESTRICT in_pDst,
                     Ipp32u in_DstPitch, // in samples
                     Ipp32s width,
                     Ipp32s height,
                     int c_shift);

    // Do weighted prediction from one reference frame
    template<typename PixType>
    void CopyWeighted(H265DecoderFrame* frame, H265DecYUVBufferPadded* src, Ipp32u CUAddr, Ipp32u PartIdx, Ipp32u Width, Ipp32u Height,
        Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma);

    // Do weighted prediction from two reference frames
    template<typename PixType>
    void CopyWeightedBidi(H265DecoderFrame* frame, H265DecYUVBufferPadded* src1, H265DecYUVBufferPadded* src2, Ipp32u CUAddr, Ipp32u PartIdx, Ipp32u Width, Ipp32u Height,
        Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth, Ipp32u bit_depth_chroma);

    // Calculate address offset inside of source frame
    Ipp32s GetAddrOffset(Ipp32u PartUnitIdx, Ipp32u width);

    // Motion compensation with bit depth constant
    template<typename PixType>
    void MotionCompensationInternal(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);

    // Perform weighted addition from one or two reference sources
    template<typename PixType>
    void WeightedPrediction(H265CodingUnit* pCU, const H265PUInfo & PUi);

public:
    H265Prediction();
    virtual ~H265Prediction();

    // Allocate temporal buffers which may be necessary to store interpolated reference frames
    void InitTempBuff(DecodingContext* context);

    void MotionCompensation(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
};

} // end namespace UMC_HEVC_DECODER

#endif // __H265_PREDICTION_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
