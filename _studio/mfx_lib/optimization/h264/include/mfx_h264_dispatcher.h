/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2015 Intel Corporation. All Rights Reserved.
//
//
*/

#ifndef __MFX_H264_DISPATCHER_H__
#define __MFX_H264_DISPATCHER_H__

#include "mfxvideo.h"
#include "ippvc.h"

#pragma warning(disable: 4127)

#if defined(_WIN32) || defined(_WIN64)
  #define H264_FORCEINLINE __forceinline
  #define H264_NONLINE __declspec(noinline)
  #define H264_FASTCALL __fastcall
  #define ALIGN_DECL(X) __declspec(align(X))
#else
  #define H264_FORCEINLINE __attribute__((always_inline))
  #define H264_NONLINE __attribute__((noinline))
  #define H264_FASTCALL
  #define ALIGN_DECL(X) __attribute__ ((aligned(X)))
#endif

namespace MFX_H264_PP
{
    mfxStatus InitDispatcher(mfxU32 cpuFeature);

    class H264_Dispatcher
    {
    public:

        virtual void FilterDeblockingLumaEdge(Ipp16u* pSrcDst, Ipp32s  srcdstStep,
                                           Ipp8u*  pAlpha, Ipp8u*  pBeta,
                                           Ipp8u*  pThresholds, Ipp8u*  pBS,
                                           Ipp32s  bit_depth,
                                           Ipp32u  isDeblHor);

        virtual void FilterDeblockingLumaEdge(Ipp8u* pSrcDst, Ipp32s  srcdstStep,
                                           Ipp8u*  pAlpha, Ipp8u*  pBeta,
                                           Ipp8u*  pThresholds, Ipp8u*  pBS,
                                           Ipp32s  bit_depth,
                                           Ipp32u  isDeblHor);

        virtual void H264_FASTCALL FilterDeblockingChromaVerEdge_MBAFF(Ipp16u* pSrcDst, Ipp32s  srcdstStep,
                                                Ipp8u*  pAlpha, Ipp8u*  pBeta,
                                                Ipp8u*  pThresholds, Ipp8u*  pBS,
                                                Ipp32s  bit_depth, Ipp32u  chroma_format_idc);

        virtual void H264_FASTCALL FilterDeblockingChromaVerEdge_MBAFF(Ipp8u* pSrcDst, Ipp32s  srcdstStep,
                                                Ipp8u*  pAlpha, Ipp8u*  pBeta,
                                                Ipp8u*  pThresholds, Ipp8u*  pBS,
                                                Ipp32s  bit_depth, Ipp32u  chroma_format_idc);

        virtual void FilterDeblockingChromaEdge(Ipp8u* pSrcDst, Ipp32s  srcdstStep,
                                                Ipp8u*  pAlpha, Ipp8u*  pBeta,
                                                Ipp8u*  pThresholds, Ipp8u*  pBS,
                                                Ipp32s  bit_depth,
                                                Ipp32u  chroma_format_idc, Ipp32u  isDeblHor);

        virtual void FilterDeblockingChromaEdge(Ipp16u* pSrcDst, Ipp32s  srcdstStep,
                                                Ipp8u*  pAlpha, Ipp8u*  pBeta,
                                                Ipp8u*  pThresholds, Ipp8u*  pBS,
                                                Ipp32s  bit_depth,
                                                Ipp32u  chroma_format_idc, Ipp32u  isDeblHor);

        virtual void H264_FASTCALL InterpolateChromaBlock_16u(const IppVCInterpolateBlock_16u *interpolateInfo);

        virtual void ReconstructChromaIntra4x4(Ipp32s **ppSrcDstCoeff,
                                                Ipp16u *pSrcDstUVPlane,
                                                Ipp32u srcdstUVStep,
                                                IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                Ipp32u cbpU, Ipp32u cbpV,
                                                Ipp32u chromaQPU, Ipp32u chromaQPV,
                                                Ipp8u  edge_type,
                                                const Ipp16s *pQuantTableU, const Ipp16s *pQuantTableV,
                                                Ipp16s levelScaleDCU, Ipp16s levelScaleDCV,
                                                Ipp8u  bypass_flag,
                                                Ipp32u bit_depth,
                                                Ipp32u chroma_format);

        virtual void ReconstructChromaInter4x4(Ipp32s **ppSrcDstCoeff,
                                                        Ipp16u *pSrcDstUVPlane,
                                                        Ipp32u srcdstUVStep,
                                                        Ipp32u cbpU, Ipp32u cbpV,
                                                        Ipp32u chromaQPU, Ipp32u chromaQPV,
                                                        const Ipp16s *pQuantTableU, const Ipp16s *pQuantTableV,
                                                        Ipp16s levelScaleDCU, Ipp16s levelScaleDCV,
                                                        Ipp8u  bypass_flag,
                                                        Ipp32u bit_depth,
                                                        Ipp32u chroma_format);


        virtual void ReconstructChromaIntra4x4(Ipp16s **ppSrcDstCoeff,
                                                Ipp8u *pSrcDstUVPlane,
                                                Ipp32u srcdstUVStep,
                                                IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                Ipp32u cbpU, Ipp32u cbpV,
                                                Ipp32u chromaQPU, Ipp32u chromaQPV,
                                                Ipp8u  edge_type,
                                                const Ipp16s *pQuantTableU, const Ipp16s *pQuantTableV,
                                                Ipp16s levelScaleDCU, Ipp16s levelScaleDCV,
                                                Ipp8u  bypass_flag,
                                                Ipp32u bit_depth,
                                                Ipp32u chroma_format);

        virtual void ReconstructChromaInter4x4(Ipp16s **ppSrcDstCoeff,
                                                        Ipp8u *pSrcDstUVPlane,
                                                        Ipp32u srcdstUVStep,
                                                        Ipp32u cbpU, Ipp32u cbpV,
                                                        Ipp32u chromaQPU, Ipp32u chromaQPV,
                                                        const Ipp16s *pQuantTableU, const Ipp16s *pQuantTableV,
                                                        Ipp16s levelScaleDCU, Ipp16s levelScaleDCV,
                                                        Ipp8u  bypass_flag,
                                                        Ipp32u bit_depth,
                                                        Ipp32u chroma_format);

        virtual void ReconstructChromaIntraHalfs4x4MB_NV12(Ipp32s **ppSrcDstCoeff,
                                                      Ipp16u *pSrcDstUVPlane,
                                                      Ipp32u srcdstUVStep,
                                                      IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                      Ipp32u cbpU, Ipp32u cbpV,
                                                      Ipp32u chromaQPU, Ipp32u chromaQPV,
                                                      Ipp8u  edge_type_top, Ipp8u  edge_type_bottom,
                                                      const Ipp16s *pQuantTableU, const Ipp16s *pQuantTableV,
                                                      Ipp16s levelScaleDCU, Ipp16s levelScaleDCV,
                                                      Ipp8u  bypass_flag,
                                                      Ipp32s bit_depth,
                                                      Ipp32u chroma_format);

        virtual void ReconstructChromaIntraHalfs4x4MB_NV12(Ipp16s **ppSrcDstCoeff,
                                                      Ipp8u *pSrcDstUVPlane,
                                                      Ipp32u srcdstUVStep,
                                                      IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                      Ipp32u cbpU, Ipp32u cbpV,
                                                      Ipp32u chromaQPU, Ipp32u chromaQPV,
                                                      Ipp8u  edge_type_top, Ipp8u  edge_type_bottom,
                                                      const Ipp16s *pQuantTableU, const Ipp16s *pQuantTableV,
                                                      Ipp16s levelScaleDCU, Ipp16s levelScaleDCV,
                                                      Ipp8u  bypass_flag,
                                                      Ipp32s bit_depth,
                                                      Ipp32u chroma_format);

        virtual void UniDirWeightBlock_NV12(Ipp16u *pSrcDst, Ipp32u srcDstStep,
                                       Ipp32u ulog2wd,
                                       Ipp32s iWeightU, Ipp32s iOffsetU,
                                       Ipp32s iWeightV, Ipp32s iOffsetV,
                                       IppiSize roi, Ipp32u bitDepth);

        virtual void UniDirWeightBlock_NV12(Ipp8u *pSrcDst, Ipp32u srcDstStep,
                                       Ipp32u ulog2wd,
                                       Ipp32s iWeightU, Ipp32s iOffsetU,
                                       Ipp32s iWeightV, Ipp32s iOffsetV,
                                       IppiSize roi, Ipp32u bitDepth);

        virtual void BiDirWeightBlock_NV12(const Ipp8u *pSrc0, Ipp32u src0Pitch,
                                              const Ipp8u *pSrc1, Ipp32u src1Pitch,
                                              Ipp8u *pDst, Ipp32u dstPitch,
                                              Ipp32u ulog2wd,
                                              Ipp32s iWeightU1, Ipp32s iOffsetU1,
                                              Ipp32s iWeightU2, Ipp32s iOffsetU2,
                                              Ipp32s iWeightV1, Ipp32s iOffsetV1,
                                              Ipp32s iWeightV2, Ipp32s iOffsetV2,
                                              IppiSize roi, Ipp32u bitDepth);

        virtual void BiDirWeightBlock_NV12(const Ipp16u *pSrc0, Ipp32u src0Pitch,
                                              const Ipp16u *pSrc1, Ipp32u src1Pitch,
                                              Ipp16u *pDst, Ipp32u dstPitch,
                                              Ipp32u ulog2wd,
                                              Ipp32s iWeightU1, Ipp32s iOffsetU1,
                                              Ipp32s iWeightU2, Ipp32s iOffsetU2,
                                              Ipp32s iWeightV1, Ipp32s iOffsetV1,
                                              Ipp32s iWeightV2, Ipp32s iOffsetV2,
                                              IppiSize roi, Ipp32u bitDepth);

    };

    class H264_Dispatcher_sse : public H264_Dispatcher
    {
    public:

        virtual void FilterDeblockingChromaEdge(Ipp8u* pSrcDst, Ipp32s  srcdstStep,
                                                Ipp8u*  pAlpha, Ipp8u*  pBeta,
                                                Ipp8u*  pThresholds, Ipp8u*  pBS,
                                                Ipp32s  bit_depth,
                                                Ipp32u  chroma_format_idc, Ipp32u  isDeblHor);
    };

    H264_Dispatcher * GetH264Dispatcher();

}; // namespace MFX_H264_PP

#endif //__MFX_H264_DISPATCHER_H__
/* EOF */
