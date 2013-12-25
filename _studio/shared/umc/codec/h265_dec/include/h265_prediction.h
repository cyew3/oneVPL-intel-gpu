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

#ifndef __H265_PREDICTION_H
#define __H265_PREDICTION_H

#include "umc_h265_dec_defs_dec.h"
#include "umc_h265_frame.h"
#include "h265_motion_info.h"

#include "mfx_h265_optimization.h" // common data types here for interpolation

namespace UMC_HEVC_DECODER
{

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Class definition
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class DecodingContext;

// prediction class
class H265Prediction
{
public:
    
protected:
    H265PlanePtrYCommon m_temp_interpolarion_buffer;
    H265PlanePtrYCommon m_YUVExt;
    Ipp32u m_YUVExtStride;
    Ipp32u m_YUVExtHeight;

    DecodingContext* m_context;

    H265DecYUVBufferPadded m_YUVPred[2];    

    // motion compensation functions
    template <EnumTextType c_plane_type, bool bi, typename H265PlaneYCommon>
    void H265_FORCEINLINE PredInterUni(H265CodingUnit* pCU, H265PUInfo &PUi, EnumRefPicList RefPicList, H265DecYUVBufferPadded *YUVPred, MFX_HEVC_PP::EnumAddAverageType eAddAverage = MFX_HEVC_PP::AVERAGE_NO);

    bool CheckIdenticalMotion(H265CodingUnit* pCU, H265PUInfo &MVi);    

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

     template <typename PixType>
     static void CopyExtendPU(const PixType * in_pSrc,
                     Ipp32u in_SrcPitch, // in samples
                     Ipp16s* H265_RESTRICT in_pDst,
                     Ipp32u in_DstPitch, // in samples
                     Ipp32s width,
                     Ipp32s height,
                     int c_shift);

    template<typename PixType>
    void CopyWeighted(H265DecoderFrame* frame, H265DecYUVBufferPadded* src, Ipp32u CUAddr, Ipp32u PartIdx, Ipp32u Width, Ipp32u Height, Ipp32s *w, Ipp32s *o, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth);

    template<typename PixType>
    void CopyWeightedBidi(H265DecoderFrame* frame, H265DecYUVBufferPadded* src1, H265DecYUVBufferPadded* src2, Ipp32u CUAddr, Ipp32u PartIdx, Ipp32u Width, Ipp32u Height, Ipp32s *w0, Ipp32s *w1, Ipp32s *logWD, Ipp32s *round, Ipp32u bit_depth);

    Ipp32s GetAddrOffset(Ipp32u PartUnitIdx, Ipp32u width);

    template<typename PixType>
    void MotionCompensationInternal(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);

public:
    H265Prediction();
    virtual ~H265Prediction();

    void InitTempBuff(DecodingContext* context);

    void MotionCompensation(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);

    H265PlanePtrYCommon GetPredicBuf()
    {
        return m_YUVExt;
    }
    Ipp32s GetPredicBufWidth()
    {
        return m_YUVExtStride;
    }
    Ipp32s GetPredicBufHeight()
    {
        return m_YUVExtHeight;
    }
};

} // end namespace UMC_HEVC_DECODER

#endif // __H265_PREDICTION_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
