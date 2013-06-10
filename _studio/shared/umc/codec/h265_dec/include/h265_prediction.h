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
#include "h265_pattern.h"

namespace UMC_HEVC_DECODER
{

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Class definition
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// prediction class
class H265Prediction
{
protected:
    H265PlanePtrYCommon m_temp_interpolarion_buffer;
    H265PlanePtrYCommon m_YUVExt;
    Ipp32s m_YUVExtStride;
    Ipp32s m_YUVExtHeight;

    H265DecYUVBufferPadded m_YUVPred[2];

    void PredIntraAngLuma(Ipp32s bitDepth, H265PlanePtrYCommon pSrc, Ipp32s srcStride, H265PlanePtrYCommon Dst, Ipp32s dstStride, Ipp32u width, Ipp32u height, Ipp32u dirMode, bool Filter);
    void PredIntraPlanarLuma(H265PlanePtrYCommon pSrc, Ipp32s srcStride, H265PlanePtrYCommon rpDst, Ipp32s dstStride, Ipp32u blkSize);

    void PredIntraAngChroma(Ipp32s bitDepth, H265PlanePtrYCommon pSrc, Ipp32s srcStride, H265PlanePtrUVCommon Dst, Ipp32s dstStride, Ipp32u width, Ipp32u height, Ipp32u dirMode);
    void PredIntraPlanarChroma(H265PlanePtrYCommon pSrc, Ipp32s srcStride, H265PlanePtrUVCommon rpDst, Ipp32s dstStride, Ipp32u blkSize);

    // motion compensation functions
    void PredInterUni(H265CodingUnit* pCU, Ipp32u PartAddr, Ipp32s Width, Ipp32s Height, EnumRefPicList RefPicList, H265DecYUVBufferPadded *YUVPred, bool bi = false);
    bool CheckIdenticalMotion(H265CodingUnit* pCU, Ipp32u PartAddr);
    
#if (HEVC_OPT_CHANGES & 8)
    enum EnumInterpType
    {
        INTERP_HOR = 0,
        INTERP_VER

    };

    template < EnumTextType plane_type, typename t_src, typename t_dst >
    void Interpolate(   EnumInterpType interp_type,
                        const t_src* in_pSrc,
                        Ipp32u in_SrcPitch, // in samples
                        t_dst* in_pDst,
                        Ipp32u in_DstPitch, // in samples
                        Ipp32s tab_index,
                        Ipp32s width,
                        Ipp32s height,
                        Ipp32s shift,
                        Ipp16s offset);
#endif

#if !defined(HEVC_OPT_CHANGES) || !(HEVC_OPT_CHANGES & 8) || (HEVC_OPT_CHANGES  & 0x10000)
    void InterpolateHorLuma(const H265PlanePtrYCommon in_pSrc,
                            Ipp32u in_SrcPitch, // in samples
                            Ipp16s *in_pDst,
                            Ipp32u in_DstPitch, // in samples
                            Ipp32s tab_index,
                            Ipp32s width,
                            Ipp32s height,
                            Ipp32s shift,
                            Ipp32s offset);

    void InterpolateVert0Luma(const H265PlanePtrYCommon in_pSrc,
                              Ipp32u in_SrcPitch, // in samples
                              Ipp16s *in_pDst,
                              Ipp32u in_DstPitch, // in samples
                              Ipp32s tab_index,
                              Ipp32s width,
                              Ipp32s height,
                              Ipp32s shift,
                              Ipp32s offset);

    void InterpolateVertLuma(const Ipp16s *in_pSrc,
                             Ipp32u in_SrcPitch, // in samples
                             Ipp16s *in_pDst,
                             Ipp32u in_DstPitch, // in samples
                             Ipp32s tab_index,
                             Ipp32s width,
                             Ipp32s height,
                             Ipp32s shift,
                             Ipp32s offset);

    void InterpolateHorChroma(const H265PlanePtrYCommon in_pSrc,
                              Ipp32u in_SrcPitch, // in samples
                              Ipp16s *in_pDst,
                              Ipp32u in_DstPitch, // in samples
                              Ipp32s tab_index,
                              Ipp32s width,
                              Ipp32s height,
                              Ipp32s shift,
                              Ipp32s offset);

    void InterpolateVert0Chroma(const H265PlanePtrUVCommon in_pSrc,
                                Ipp32u in_SrcPitch, // in samples
                                Ipp16s *in_pDst,
                                Ipp32u in_DstPitch, // in samples
                                Ipp32s tab_index,
                                Ipp32s width,
                                Ipp32s height,
                                Ipp32s shift,
                                Ipp32s offset);

    void InterpolateVertChroma(const Ipp16s *in_pSrc,
                               Ipp32u in_SrcPitch, // in samples
                               Ipp16s *in_pDst,
                               Ipp32u in_DstPitch, // in samples
                               Ipp32s tab_index,
                               Ipp32s width,
                               Ipp32s height,
                               Ipp32s shift,
                               Ipp32s offset);
#endif // HEVC_OPT_CHANGES

     void CopyPULuma(const H265PlanePtrYCommon in_pSrc,
                     Ipp32u in_SrcPitch, // in samples
                     Ipp16s *in_pDst,
                     Ipp32u in_DstPitch, // in samples
                     Ipp32s width,
                     Ipp32s height,
                     Ipp32s shift);
     void CopyPUChroma(const H265PlanePtrUVCommon in_pSrc,
                       Ipp32u in_SrcPitch, // in samples
                       Ipp16s *in_pDst,
                       Ipp32u in_DstPitch, // in samples
                       Ipp32s width,
                       Ipp32s height,
                       Ipp32s shift);

    void DCPredFiltering(H265PlanePtrYCommon pSrc, Ipp32s SrcStride, H265PlanePtrYCommon Dst, Ipp32s DstStride, Ipp32u Width, Ipp32u Height);

public:
    H265Prediction();
    virtual ~H265Prediction();

    void InitTempBuff();

    // inter
    void MotionCompensation(H265CodingUnit* pCU, H265DecYUVBufferPadded* YUVPred, Ipp32u AbsPartIdx);

    // motion vector prediction
    void GetMVPredAMVP(H265CodingUnit* pCU, Ipp32u PartIdx, Ipp32u PartAddr, EnumRefPicList RefPicList, Ipp32s RefIdx, H265MotionVector& m_MVPred);

    // Angular Intra
    void PredIntraLumaAng(H265Pattern* pPattern, Ipp32u DirMode, H265PlanePtrYCommon pPred, Ipp32u Stride, Ipp32s Width, Ipp32s Height);
    void PredIntraChromaAng(H265PlanePtrUVCommon pSrc, Ipp32u DirMode, H265PlanePtrUVCommon pPred, Ipp32u Stride, Ipp32s Width, Ipp32s Height);

    H265PlaneYCommon PredIntraGetPredValDC(H265PlanePtrYCommon pSrc, Ipp32s SrcStride, Ipp32s Width, Ipp32s Height);

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
