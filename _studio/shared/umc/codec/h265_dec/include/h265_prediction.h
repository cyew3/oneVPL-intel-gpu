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

namespace UMC_HEVC_DECODER
{

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Class definition
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// prediction class
class H265Prediction
{
public:
    enum EnumAddAverageType
    {
        AVERAGE_NO = 0,
        AVERAGE_FROM_PIC,
        AVERAGE_FROM_BUF
    };

protected:
    H265PlanePtrYCommon m_temp_interpolarion_buffer;
    H265PlanePtrYCommon m_YUVExt;
    Ipp32s m_YUVExtStride;
    Ipp32s m_YUVExtHeight;

    H265DecYUVBufferPadded m_YUVPred[2];

    enum EnumInterpType
    {
        INTERP_HOR = 0,
        INTERP_VER
    };

    void PredIntraAngLuma(Ipp32s bitDepth, H265PlanePtrYCommon pSrc, Ipp32s srcStride, H265PlanePtrYCommon Dst, Ipp32s dstStride, Ipp32s blkSize, Ipp32u dirMode, bool Filter);
    void PredIntraPlanarLuma(H265PlanePtrYCommon pSrc, Ipp32s srcStride, H265PlanePtrYCommon rpDst, Ipp32s dstStride, Ipp32s blkSize);

    void PredIntraAngChroma(Ipp32s bitDepth, H265PlanePtrYCommon pSrc, Ipp32s srcStride, H265PlanePtrUVCommon Dst, Ipp32s dstStride, Ipp32s blkSize, Ipp32u dirMode);
    void PredIntraPlanarChroma(H265PlanePtrYCommon pSrc, Ipp32s srcStride, H265PlanePtrUVCommon rpDst, Ipp32s dstStride, Ipp32s blkSize);

    // motion compensation functions
    template <EnumTextType c_plane_type, bool bi>
    void H265_FORCEINLINE PredInterUni(H265CodingUnit* pCU, H265PUInfo &PUi, EnumRefPicList RefPicList, H265DecYUVBufferPadded *YUVPred, EnumAddAverageType eAddAverage = AVERAGE_NO);

    bool CheckIdenticalMotion(H265CodingUnit* pCU, H265PUInfo &MVi);
    
    template < EnumTextType plane_type, typename t_src, typename t_dst >
    static void H265_FORCEINLINE Interpolate( EnumInterpType interp_type,
                        const t_src* in_pSrc,
                        Ipp32u in_SrcPitch, // in samples
                        t_dst* H265_RESTRICT in_pDst,
                        Ipp32u in_DstPitch, // in samples
                        Ipp32s tab_index,
                        Ipp32s width,
                        Ipp32s height,
                        Ipp32s shift,
                        Ipp16s offset,
                        EnumAddAverageType eAddAverage = AVERAGE_NO,
                        const void* in_pSrc2 = NULL,
                        int   in_Src2Pitch = 0 ); // in samples

     static void WriteAverageToPic(
                     const H265PlaneYCommon * in_pSrc0,
                     Ipp32u in_Src0Pitch,      // in samples
                     const H265PlaneYCommon * in_pSrc1,
                     Ipp32u in_Src1Pitch,      // in samples
                     H265PlaneYCommon* H265_RESTRICT in_pDst,
                     Ipp32u in_DstPitch,       // in samples
                     Ipp32s width,
                     Ipp32s height );

     template <int c_shift>
     static void CopyExtendPU(const H265PlaneYCommon * in_pSrc,
                     Ipp32u in_SrcPitch, // in samples
                     Ipp16s* H265_RESTRICT in_pDst,
                     Ipp32u in_DstPitch, // in samples
                     Ipp32s width,
                     Ipp32s height);

    void DCPredFiltering(H265PlanePtrYCommon pSrc, Ipp32s SrcStride, H265PlanePtrYCommon Dst, Ipp32s DstStride, Ipp32s Size);

public:
    H265Prediction();
    virtual ~H265Prediction();

    void InitTempBuff();

    // inter
    void MotionCompensation(H265CodingUnit* pCU, H265DecYUVBufferPadded* YUVPred, Ipp32u AbsPartIdx, H265PUInfo *PUInfo);

    // Angular Intra
    void PredIntraLumaAng(Ipp32u DirMode, H265PlanePtrYCommon pPred, Ipp32u Stride, Ipp32s Size);
    void PredIntraChromaAng(H265PlanePtrUVCommon pSrc, Ipp32u DirMode, H265PlanePtrUVCommon pPred, Ipp32u Stride, Ipp32s Size);
    H265PlanePtrYCommon GetPredictorPtr(Ipp32u DirMode, Ipp32u WidthBits, H265PlanePtrYCommon pAdiBuf);

    H265PlaneYCommon PredIntraGetPredValDC(H265PlanePtrYCommon pSrc, Ipp32s SrcStride, Ipp32s Size);

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

//=================================================================================================
// general template for Interpolate kernel
template
< 
    typename     t_vec,
    EnumTextType c_plane_type,
    typename     t_src, 
    typename     t_dst,
    int        tab_index
>
class t_InterpKernel_intrin
{
public:
    static void func(
        t_dst* H265_RESTRICT pDst, 
        const t_src* pSrc, 
        int    in_SrcPitch, // in samples
        int    in_DstPitch, // in samples
        int    width,
        int    height,
        int    accum_pitch,
        int    shift,
        int    offset,
        H265Prediction::EnumAddAverageType eAddAverage = H265Prediction::AVERAGE_NO,
        const void* in_pSrc2 = NULL,
        int   in_Src2Pitch = 0 // in samples
    );
};


} // end namespace UMC_HEVC_DECODER

#endif // __H265_PREDICTION_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
