/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_DEC_IPP_WRAP_H
#define __UMC_H264_DEC_IPP_WRAP_H

#include "ippi.h"
#include "umc_h264_dec_ipplevel.h"
#include "vm_debug.h"
#include "umc_h264_dec_defs_dec.h"

#if defined(LINUX) && defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

typedef struct _IppiBidir1_8u
{
    const Ipp8u * pSrc[2];
    Ipp32s   srcStep[2];
    Ipp8u*   pDst;
    Ipp32s   dstStep;
    IppiSize roiSize;
} IppVCBidir1_8u;

typedef struct _IppiBidir1_16u
{
    const Ipp16u * pSrc[2];
    Ipp32s   srcStep[2];
    Ipp16u*   pDst;
    Ipp32s   dstStep;
    IppiSize roiSize;
    Ipp32s   bitDepth;
} IppVCBidir1_16u;

/*pParams->m_pSrcY[0],
    pParams->m_pSrcY[1],
    pParams->m_pDstY,
    pParams->m_lumaInterpolateInfo.sizeBlock.width,
    pParams->m_lumaInterpolateInfo.sizeBlock.height,
    pParams->m_iSrcPitchLuma[0],
    pParams->m_iSrcPitchLuma[1],
    pParams->m_iDstPitchLuma,
    pParams->m_lumaInterpolateInfo.bitDepth*/


IppStatus MyippiDecodeCAVLCCoeffs_H264_1u16s (Ipp32u ** const ppBitStream,
                                                     Ipp32s *pOffset,
                                                     Ipp16s *pNumCoeff,
                                                     Ipp16s **ppPosCoefbuf,
                                                     Ipp32u uVLCSelect,
                                                     Ipp16s coeffLimit,
                                                     Ipp16s uMaxNumCoeff,
                                                     const Ipp32s *pScanMatrix,
                                                     Ipp32s scanIdxStart);

IppStatus MyippiDecodeCAVLCCoeffs_H264_1u32s (Ipp32u ** const ppBitStream,
                                                     Ipp32s *pOffset,
                                                     Ipp16s *pNumCoeff,
                                                     Ipp32s **ppPosCoefbuf,
                                                     Ipp32u uVLCSelect,
                                                     Ipp16s uMaxNumCoeff,
                                                     const Ipp32s *pScanMatrix);

IppStatus MyippiDecodeCAVLCChromaDcCoeffs_H264_1u16s(Ipp32u **ppBitStream,
                                                         Ipp32s *pOffset,
                                                         Ipp16s *pNumCoeff,
                                                         Ipp16s **ppPosCoefbuf);


IppStatus MyippiDecodeCAVLCChromaDcCoeffs_H264_1u32s(Ipp32u **ppBitStream,
                                                         Ipp32s *pOffset,
                                                         Ipp16s *pNumCoeff,
                                                         Ipp32s **ppPosCoefbuf);

namespace UMC
{
#pragma warning(disable: 4100)

    inline IppStatus SetPlane(Ipp8u val, Ipp8u* pDst, Ipp32s len)
    {
        if (!pDst)
            return ippStsNullPtrErr;

        return ippsSet_8u(val, pDst, len);
    }

    inline IppStatus CopyPlane(const Ipp8u *pSrc, Ipp8u *pDst, Ipp32s len)
    {
        if (!pSrc || !pDst)
            return ippStsNullPtrErr;

        return ippsCopy_8u(pSrc, pDst, len);
    }

    inline  IppStatus ippiInterpolateLuma(const Ipp8u *, const IppVCInterpolateBlock_16u *interpolateInfo_)
    {
        const IppVCInterpolateBlock_8u * interpolateInfo = (const IppVCInterpolateBlock_8u *) interpolateInfo_;
        return ippiInterpolateLuma_H264_8u_C1R(
            interpolateInfo->pSrc[0], interpolateInfo->srcStep, interpolateInfo->pDst[0], interpolateInfo->dstStep,
            interpolateInfo->pointVector.x, interpolateInfo->pointVector.y, interpolateInfo->sizeBlock);
    }

    inline IppStatus ippiInterpolateLumaBlock(const Ipp8u *, const IppVCInterpolateBlock_16u *interpolateInfo)
    {
        return ippiInterpolateLumaBlock_H264_8u_P1R((const IppVCInterpolateBlock_8u *)interpolateInfo);
    }

    inline IppStatus ippiInterpolateChromaBlock(const Ipp8u *, const IppVCInterpolateBlock_16u *interpolateInfo)
    {
        return ippiInterpolateChromaBlock_H264_8u_P2R((const IppVCInterpolateBlock_8u *)interpolateInfo);
    }

    inline IppStatus MyDecodeCAVLCChromaDcCoeffs_H264(Ipp32u **ppBitStream,
                                                    Ipp32s *pOffset,
                                                    Ipp16s *pNumCoeff,
                                                    Ipp16s **ppDstCoeffs)
    {
        return MyippiDecodeCAVLCChromaDcCoeffs_H264_1u16s(ppBitStream,
                                                        pOffset,
                                                        pNumCoeff,
                                                        ppDstCoeffs);
    }

    inline IppStatus DecodeCAVLCChromaDcCoeffs422_H264(Ipp32u **ppBitStream,
                                                       Ipp32s *pOffset,
                                                       Ipp16s *pNumCoeff,
                                                       Ipp16s **ppDstCoeffs,
                                                       const Ipp32s *pTblCoeffToken,
                                                       const Ipp32s **ppTblTotalZerosCR,
                                                       const Ipp32s **ppTblRunBefore)
    {
        return ippiDecodeCAVLCChroma422DcCoeffs_H264_1u16s(ppBitStream,
                                                            pOffset,
                                                            pNumCoeff,
                                                            ppDstCoeffs,
                                                            pTblCoeffToken,
                                                            ppTblTotalZerosCR,
                                                            ppTblRunBefore);
    }

    inline IppStatus DecodeCAVLCCoeffs_H264(Ipp32u **ppBitStream,
                                            Ipp32s *pOffset,
                                            Ipp16s *pNumCoeff,
                                            Ipp16s **ppDstCoeffs,
                                            Ipp32u uVLCSelect,
                                            Ipp16s uMaxNumCoeff,
                                            const Ipp32s **ppTblCoeffToken,
                                            const Ipp32s **ppTblTotalZeros,
                                            const Ipp32s **ppTblRunBefore,
                                            const Ipp32s *pScanMatrix)
    {
        return ippiDecodeCAVLCCoeffs_H264_1u16s(ppBitStream,
                                                pOffset,
                                                pNumCoeff,
                                                ppDstCoeffs,
                                                uVLCSelect,
                                                uMaxNumCoeff,
                                                ppTblCoeffToken,
                                                ppTblTotalZeros,
                                                ppTblRunBefore,
                                                pScanMatrix);
    }

    inline IppStatus MyDecodeCAVLCCoeffs_H264(Ipp32u **ppBitStream,
                                            Ipp32s *pOffset,
                                            Ipp16s *pNumCoeff,
                                            Ipp16s **ppDstCoeffs,
                                            Ipp32u uVLCSelect,
                                            Ipp16s coeffLimit,
                                            Ipp16s uMaxNumCoeff,
                                            const Ipp32s *pScanMatrix,
                                            Ipp32s scanIdxStart)
    {
        return MyippiDecodeCAVLCCoeffs_H264_1u16s(ppBitStream,
                                                pOffset,
                                                pNumCoeff,
                                                ppDstCoeffs,
                                                uVLCSelect,
                                                coeffLimit,
                                                uMaxNumCoeff,
                                                pScanMatrix,
                                                scanIdxStart);
    }

    inline IppStatus SetPlane(Ipp8u value, Ipp8u* pDst, Ipp32s dstStep,
                              IppiSize roiSize )
    {
        return ippiSet_8u_C1R(value, pDst, dstStep,
                              roiSize);
    }

    inline IppStatus CopyPlane(Ipp8u* pSrc, Ipp32s srcStep, Ipp8u* pDst, Ipp32s dstStep,
                              IppiSize roiSize )
    {
        return ippiCopy_8u_C1R(pSrc, srcStep, pDst, dstStep,
                              roiSize);
    }

    inline IppStatus CopyPlane_NV12(Ipp8u* pSrc, Ipp32s srcStep, Ipp8u* pDstU, Ipp8u* pDstV, Ipp32s dstStep,
        IppiSize roiSize )
    {
        Ipp32s i, j;
        for (i = 0; i < roiSize.height; i++) {
            for (j = 0; j < roiSize.width; j++) {
                pDstU[j] = pSrc[2*j];
                pDstV[j] = pSrc[2*j+1];
            }
            pSrc += srcStep;
            pDstU += dstStep;
            pDstV += dstStep;
        }
        return ippStsNoErr;
    }

    inline IppStatus CopyPlane_NV12(Ipp16u* pSrc, Ipp32s srcStep, Ipp16u* pDstU, Ipp16u* pDstV, Ipp32s dstStep,
        IppiSize roiSize )
    {
        Ipp32s i, j;
        for (i = 0; i < roiSize.height; i++) {
            for (j = 0; j < roiSize.width; j++) {
                pDstU[j] = pSrc[2*j];
                pDstV[j] = pSrc[2*j+1];
            }
            pSrc = (Ipp16u*)((Ipp8u*)pSrc + srcStep);
            pDstU = (Ipp16u*)((Ipp8u*)pDstU + dstStep);
            pDstV = (Ipp16u*)((Ipp8u*)pDstV + dstStep);
        }
        return ippStsNoErr;
    }

    inline IppStatus ExpandPlane(Ipp8u *StartPtr,
                                 Ipp32u uFrameWidth,
                                 Ipp32u uFrameHeight,
                                 Ipp32u uPitch,
                                 Ipp32u uPels,
                                 IppvcFrameFieldFlag uFrameFieldFlag)
    {
        return ippiExpandPlane_H264_8u_C1R(StartPtr,
                                           uFrameWidth,
                                           uFrameHeight,
                                           uPitch,
                                           uPels,
                                           uFrameFieldFlag);
    }

    inline IppStatus BiDirWeightBlock(Ipp8u * , IppVCBidir1_16u * info_,
                                      Ipp32u ulog2wd,
                                      Ipp32s iWeight1,
                                      Ipp32s iOffset1,
                                      Ipp32s iWeight2,
                                      Ipp32s iOffset2)
    {
        IppVCBidir1_8u * info = (IppVCBidir1_8u *)info_;
        return ippiBiDirWeightBlock_H264_8u_P3P1R(info->pSrc[0], info->pSrc[1], info->pDst,
            info->srcStep[0], info->srcStep[1], info->dstStep,
            ulog2wd, iWeight1, iOffset1, iWeight2, iOffset2, info->roiSize);
    }

    inline IppStatus BiDirWeightBlockImplicit(Ipp8u * , IppVCBidir1_16u * info_,
                                              Ipp32s iWeight1,
                                              Ipp32s iWeight2)
    {
        IppVCBidir1_8u * info = (IppVCBidir1_8u *)info_;
        return ippiBiDirWeightBlockImplicit_H264_8u_P3P1R(info->pSrc[0],
                                                          info->pSrc[1],
                                                          info->pDst,
                                                          info->srcStep[0],
                                                          info->srcStep[1],
                                                          info->dstStep,
                                                          iWeight1,
                                                          iWeight2,
                                                          info->roiSize);
    }

    inline IppStatus InterpolateBlock(Ipp8u * , IppVCBidir1_16u * info_)
    {
        IppVCBidir1_8u * info = (IppVCBidir1_8u *)info_;
        return ippiInterpolateBlock_H264_8u_P3P1R(info->pSrc[0], info->pSrc[1],
                                                  info->pDst,
                                                  info->roiSize.width,
                                                  info->roiSize.height,
                                                  info->srcStep[0],
                                                  info->srcStep[1],
                                                  info->dstStep);
    }

    inline IppStatus UniDirWeightBlock(Ipp8u *, IppVCBidir1_16u * info_,
                                       Ipp32u ulog2wd,
                                       Ipp32s iWeight,
                                       Ipp32s iOffset)
    {
        IppVCBidir1_8u * info = (IppVCBidir1_8u *)info_;
        return ippiUniDirWeightBlock_H264_8u_C1R(info->pDst, info->dstStep,
                                                 ulog2wd, iWeight,
                                                 iOffset, info->roiSize);
    }

    inline IppStatus ReconstructLumaIntraHalfMB(Ipp16s **ppSrcCoeff,
                                                Ipp8u *pSrcDstYPlane,
                                                Ipp32s srcdstYStep,
                                                IppIntra4x4PredMode_H264 *pMBIntraTypes,
                                                Ipp32u cbp4x2,
                                                Ipp32s QP,
                                                Ipp8u edgeType,
                                                Ipp32s bit_depth = 8)
    {
        return ippiReconstructLumaIntraHalfMB_H264_16s8u_C1R(ppSrcCoeff,
                                                             pSrcDstYPlane,
                                                             srcdstYStep,
                                                             pMBIntraTypes,
                                                             cbp4x2,
                                                             QP,
                                                             edgeType);
    }

    inline IppStatus ReconstructLumaInter8x8MB(Ipp16s **ppSrcDstCoeff,
                                               Ipp8u *pSrcDstYPlane,
                                               Ipp32u srcdstYStep,
                                               Ipp32u cbp8x8,
                                               Ipp32s QP,
                                               const Ipp16s *pQuantTable,
                                               Ipp8u bypass_flag,
                                               Ipp32s bit_depth = 8)
    {
        return ippiReconstructLumaInter8x8MB_H264_16s8u_C1R(ppSrcDstCoeff,
                                                            pSrcDstYPlane,
                                                            srcdstYStep,
                                                            cbp8x8,
                                                            QP,
                                                            pQuantTable,
                                                            bypass_flag);
    }

    inline IppStatus ReconstructLumaInter4x4MB(Ipp16s **ppSrcDstCoeff,
                                               Ipp8u *pSrcDstYPlane,
                                               Ipp32u srcdstYStep,
                                               Ipp32u cbp4x4,
                                               Ipp32s QP,
                                               const Ipp16s *pQuantTable,
                                               Ipp8u bypass_flag,
                                               Ipp32s bit_depth = 8)
    {
        return ippiReconstructLumaInter4x4MB_H264_16s8u_C1R(ppSrcDstCoeff,
                                                            pSrcDstYPlane,
                                                            srcdstYStep,
                                                            cbp4x4,
                                                            QP,
                                                            pQuantTable,
                                                            bypass_flag);
    }


    inline IppStatus ReconstructLumaIntra_16x16MB(Ipp16s **ppSrcDstCoeff,
                                                  Ipp8u *pSrcDstYPlane,
                                                  Ipp32u srcdstYStep,
                                                  IppIntra16x16PredMode_H264 intra_luma_mode,
                                                  Ipp32u cbp4x4,
                                                  Ipp32s QP,
                                                  Ipp8u edge_type,
                                                  const Ipp16s *pQuantTable,
                                                  Ipp8u bypass_flag,
                                                  Ipp32s bit_depth = 8)
    {
        return ippiReconstructLumaIntra_16x16MB_H264_16s8u_C1R(ppSrcDstCoeff,
                                                               pSrcDstYPlane,
                                                               srcdstYStep,
                                                               intra_luma_mode,
                                                               cbp4x4,
                                                               QP,
                                                               edge_type,
                                                               pQuantTable,
                                                               bypass_flag);
    }

    inline IppStatus ReconstructLumaIntraHalf8x8MB(Ipp16s **ppSrcDstCoeff,
                                                   Ipp8u *pSrcDstYPlane,
                                                   Ipp32s srcdstYStep,
                                                   IppIntra8x8PredMode_H264 *pMBIntraTypes,
                                                   Ipp32u cbp8x2,
                                                   Ipp32s QP,
                                                   Ipp8u edgeType,
                                                   const Ipp16s *pQuantTable,
                                                   Ipp8u bypass_flag,
                                                   Ipp32s bit_depth = 8)
    {
        return ippiReconstructLumaIntraHalf8x8MB_H264_16s8u_C1R(ppSrcDstCoeff,
                                                                pSrcDstYPlane,
                                                                srcdstYStep,
                                                                pMBIntraTypes,
                                                                cbp8x2,
                                                                QP,
                                                                edgeType,
                                                                pQuantTable,
                                                                bypass_flag);
    }


    inline IppStatus ReconstructLumaIntraHalf4x4MB(Ipp16s **ppSrcDstCoeff,
                                                   Ipp8u *pSrcDstYPlane,
                                                   Ipp32s srcdstYStep,
                                                   IppIntra4x4PredMode_H264 *pMBIntraTypes,
                                                   Ipp32u cbp4x2,
                                                   Ipp32s QP,
                                                   Ipp8u edgeType,
                                                   const Ipp16s *pQuantTable,
                                                   Ipp8u bypass_flag,
                                                   Ipp32s bit_depth = 8)
    {
        return ippiReconstructLumaIntraHalf4x4MB_H264_16s8u_C1R(ppSrcDstCoeff,
                                                                pSrcDstYPlane,
                                                                srcdstYStep,
                                                                pMBIntraTypes,
                                                                cbp4x2,
                                                                QP,
                                                                edgeType,
                                                                pQuantTable,
                                                                bypass_flag);
    }

    inline IppStatus ReconstructLumaIntra8x8MB(Ipp16s **ppSrcDstCoeff,
                                               Ipp8u *pSrcDstYPlane,
                                               Ipp32s srcdstYStep,
                                               IppIntra8x8PredMode_H264 *pMBIntraTypes,
                                               Ipp32u cbp8x8,
                                               Ipp32s QP,
                                               Ipp8u edgeType,
                                               const Ipp16s *pQuantTable,
                                               Ipp8u bypass_flag,
                                               Ipp32s bit_depth = 8)
    {
        return ippiReconstructLumaIntra8x8MB_H264_16s8u_C1R(ppSrcDstCoeff,
                                                            pSrcDstYPlane,
                                                            srcdstYStep,
                                                            pMBIntraTypes,
                                                            cbp8x8,
                                                            QP,
                                                            edgeType,
                                                            pQuantTable,
                                                            bypass_flag);
    }

    inline  IppStatus ReconstructLumaIntra4x4MB(Ipp16s **ppSrcDstCoeff,
                                                Ipp8u *pSrcDstYPlane,
                                                Ipp32s srcdstYStep,
                                                IppIntra4x4PredMode_H264 *pMBIntraTypes,
                                                Ipp32u cbp4x4,
                                                Ipp32s QP,
                                                Ipp8u edgeType,
                                                const Ipp16s *pQuantTable,
                                                Ipp8u bypass_flag,
                                                Ipp32s bit_depth = 8)
    {
        return ippiReconstructLumaIntra4x4MB_H264_16s8u_C1R(ppSrcDstCoeff,
                                                            pSrcDstYPlane,
                                                            srcdstYStep,
                                                            pMBIntraTypes,
                                                            cbp4x4,
                                                            QP,
                                                            edgeType,
                                                            pQuantTable,
                                                            bypass_flag);
    }

    inline IppStatus ReconstructLumaIntra16x16MB(Ipp16s **ppSrcCoeff,
                                                 Ipp8u *pSrcDstYPlane,
                                                 Ipp32u srcdstYStep,
                                                 const IppIntra16x16PredMode_H264 intra_luma_mode,
                                                 const Ipp32u cbp4x4,
                                                 const Ipp32s QP,
                                                 const Ipp8u edge_type,
                                                 Ipp32s bit_depth = 8)
    {
        return ippiReconstructLumaIntra16x16MB_H264_16s8u_C1R(ppSrcCoeff,
                                                              pSrcDstYPlane,
                                                              srcdstYStep,
                                                              intra_luma_mode,
                                                              cbp4x4,
                                                              QP,
                                                              edge_type);
    }

    inline IppStatus ReconstructLumaIntraMB(Ipp16s **ppSrcCoeff,
                                            Ipp8u *pSrcDstYPlane,
                                            Ipp32s srcdstYStep,
                                            const IppIntra4x4PredMode_H264 *pMBIntraTypes,
                                            const Ipp32u cbp4x4,
                                            const Ipp32s QP,
                                            const Ipp8u edgeType,
                                            Ipp32s bit_depth = 8)
    {
        return ippiReconstructLumaIntraMB_H264_16s8u_C1R(ppSrcCoeff,
                                                         pSrcDstYPlane,
                                                         srcdstYStep,
                                                         pMBIntraTypes,
                                                         cbp4x4,
                                                         QP,
                                                         edgeType);
    }

    inline IppStatus ReconstructLumaInterMB(Ipp16s **ppSrcCoeff,
                                            Ipp8u *pSrcDstYPlane,
                                            Ipp32u srcdstYStep,
                                            Ipp32u cbp4x4,
                                            Ipp32s QP,
                                            Ipp32s bit_depth = 8)
    {
        return ippiReconstructLumaInterMB_H264_16s8u_C1R(ppSrcCoeff,
                                                         pSrcDstYPlane,
                                                         srcdstYStep,
                                                         cbp4x4,
                                                         QP);
    }

    inline IppStatus ReconstructChromaInter4x4MB(Ipp16s **ppSrcDstCoeff,
                                                 Ipp8u *pSrcDstUPlane,
                                                 Ipp8u *pSrcDstVPlane,
                                                 Ipp32u srcdstUVStep,
                                                 Ipp32u cbpU,
                                                 Ipp32u cbpV,
                                                 Ipp32u chromaQPU,
                                                 Ipp32u chromaQPV,
                                                 const Ipp16s *pQuantTableU,
                                                 const Ipp16s *pQuantTableV,
                                                 Ipp8u bypass_flag,
                                                 Ipp32s bit_depth = 8)
    {
        return ippiReconstructChromaInter4x4MB_H264_16s8u_P2R(ppSrcDstCoeff,
                                                              pSrcDstUPlane,
                                                              pSrcDstVPlane,
                                                              srcdstUVStep,
                                                              CreateIPPCBPMask420(cbpU, cbpV),
                                                              chromaQPU,
                                                              chromaQPV,
                                                              pQuantTableU,
                                                              pQuantTableV,
                                                              bypass_flag);
    }

    inline IppStatus ReconstructChromaInterMB(Ipp16s **ppSrcCoeff,
                                              Ipp8u *pSrcDstUPlane,
                                              Ipp8u *pSrcDstVPlane,
                                              const Ipp32u srcdstStep,
                                              Ipp32u cbpU,
                                              Ipp32u cbpV,
                                              const Ipp32u ChromaQP,
                                              Ipp32s bit_depth = 8)
    {
        return ippiReconstructChromaInterMB_H264_16s8u_P2R(ppSrcCoeff,
                                                           pSrcDstUPlane,
                                                           pSrcDstVPlane,
                                                           srcdstStep,
                                                           CreateIPPCBPMask420(cbpU, cbpV),
                                                           ChromaQP);
    }

    inline IppStatus ReconstructChromaIntra4x4MB(Ipp16s **ppSrcDstCoeff,
                                                 Ipp8u *pSrcDstUPlane,
                                                 Ipp8u *pSrcDstVPlane,
                                                 Ipp32u srcdstUVStep,
                                                 IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                 Ipp32u cbpU,
                                                 Ipp32u cbpV,
                                                 Ipp32u chromaQPU,
                                                 Ipp32u chromaQPV,
                                                 Ipp8u edge_type,
                                                 const Ipp16s *pQuantTableU,
                                                 const Ipp16s *pQuantTableV,
                                                 Ipp8u bypass_flag,
                                                 Ipp32s bit_depth = 8)
    {
        return ippiReconstructChromaIntra4x4MB_H264_16s8u_P2R(ppSrcDstCoeff,
                                                              pSrcDstUPlane,
                                                              pSrcDstVPlane,
                                                              srcdstUVStep,
                                                              intra_chroma_mode,
                                                              CreateIPPCBPMask420(cbpU, cbpV),
                                                              chromaQPU,
                                                              chromaQPV,
                                                              edge_type,
                                                              pQuantTableU,
                                                              pQuantTableV,
                                                              bypass_flag);
    }

    inline IppStatus ReconstructChromaIntraMB(Ipp16s **ppSrcCoeff,
                                              Ipp8u *pSrcDstUPlane,
                                              Ipp8u *pSrcDstVPlane,
                                              const Ipp32u srcdstUVStep,
                                              const IppIntraChromaPredMode_H264 intra_chroma_mode,
                                              Ipp32u cbpU,
                                              Ipp32u cbpV,
                                              const Ipp32u ChromaQP,
                                              const Ipp8u edge_type,
                                              Ipp32s bit_depth = 8)
    {
        return ippiReconstructChromaIntraMB_H264_16s8u_P2R(ppSrcCoeff,
                                                           pSrcDstUPlane,
                                                           pSrcDstVPlane,
                                                           srcdstUVStep,
                                                           intra_chroma_mode,
                                                           CreateIPPCBPMask420(cbpU, cbpV),
                                                           ChromaQP,
                                                           edge_type);
    }

    inline IppStatus ReconstructChromaIntraHalfs4x4MB(Ipp16s **ppSrcDstCoeff,
                                                      Ipp8u *pSrcDstUPlane,
                                                      Ipp8u *pSrcDstVPlane,
                                                      Ipp32u srcdstUVStep,
                                                      IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                      Ipp32u cbpU,
                                                      Ipp32u cbpV,
                                                      Ipp32u chromaQPU,
                                                      Ipp32u chromaQPV,
                                                      Ipp8u edge_type_top,
                                                      Ipp8u edge_type_bottom,
                                                      const Ipp16s *pQuantTableU,
                                                      const Ipp16s *pQuantTableV,
                                                      Ipp8u bypass_flag,
                                                      Ipp32s bit_depth = 8)
    {
        return ippiReconstructChromaIntraHalfs4x4MB_H264_16s8u_P2R(ppSrcDstCoeff,
                                                                   pSrcDstUPlane,
                                                                   pSrcDstVPlane,
                                                                   srcdstUVStep,
                                                                   intra_chroma_mode,
                                                                   CreateIPPCBPMask420(cbpU, cbpV),
                                                                   chromaQPU,
                                                                   chromaQPV,
                                                                   edge_type_top,
                                                                   edge_type_bottom,
                                                                   pQuantTableU,
                                                                   pQuantTableV,
                                                                   bypass_flag);
    }

    inline IppStatus ReconstructChromaIntraHalfsMB(Ipp16s **ppSrcCoeff,
                                                   Ipp8u *pSrcDstUPlane,
                                                   Ipp8u *pSrcDstVPlane,
                                                   Ipp32u srcdstUVStep,
                                                   IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                   Ipp32u cbpU,
                                                   Ipp32u cbpV,
                                                   Ipp32u ChromaQP,
                                                   Ipp8u edge_type_top,
                                                   Ipp8u edge_type_bottom,
                                                   Ipp32s bit_depth = 8)
    {
        return ippiReconstructChromaIntraHalfsMB_H264_16s8u_P2R(ppSrcCoeff,
                                                                pSrcDstUPlane,
                                                                pSrcDstVPlane,
                                                                srcdstUVStep,
                                                                intra_chroma_mode,
                                                                CreateIPPCBPMask420(cbpU, cbpV),
                                                                ChromaQP,
                                                                edge_type_top,
                                                                edge_type_bottom);
    }

#define FILL_CHROMA_RECONSTRUCT_INFO_8U \
    IppiReconstructHighMB_16s8u info_temp[2];\
    const IppiReconstructHighMB_16s8u *info[2];\
    info[0] = &info_temp[0];\
    info[1] = &info_temp[1];\
    info_temp[0].ppSrcDstCoeff = ppSrcDstCoeff;   \
    info_temp[0].pSrcDstPlane = pSrcDstUPlane;  \
    info_temp[0].srcDstStep = srcdstUVStep;    \
    info_temp[0].cbp = (Ipp32u)cbpU;          \
    info_temp[0].qp = chromaQPU;               \
    info_temp[0].pQuantTable = (Ipp16s *) pQuantTableU;  \
    info_temp[0].bypassFlag = bypass_flag;      \
    info_temp[1].ppSrcDstCoeff = ppSrcDstCoeff;   \
    info_temp[1].pSrcDstPlane = pSrcDstVPlane;  \
    info_temp[1].srcDstStep = srcdstUVStep;    \
    info_temp[1].cbp = (Ipp32u)cbpV;          \
    info_temp[1].qp = chromaQPV;\
    info_temp[1].pQuantTable = (Ipp16s *) pQuantTableV;\
    info_temp[1].bypassFlag = bypass_flag;

    inline IppStatus ReconstructChromaIntraHalfs4x4MB422(Ipp16s **ppSrcDstCoeff,
                                                         Ipp8u *pSrcDstUPlane,
                                                         Ipp8u *pSrcDstVPlane,
                                                         Ipp32u srcdstUVStep,
                                                         IppIntraChromaPredMode_H264 intraChromaMode,
                                                         Ipp32u cbpU,
                                                         Ipp32u cbpV,
                                                         Ipp32u chromaQPU,
                                                         Ipp32u chromaQPV,
                                                         Ipp32u levelScaleDCU,
                                                         Ipp32u levelScaleDCV,
                                                         Ipp8u edgeTypeTop,
                                                         Ipp8u edgeTypeBottom,
                                                         const Ipp16s *pQuantTableU,
                                                         const Ipp16s *pQuantTableV,
                                                         Ipp8u bypass_flag,
                                                         Ipp32s bit_depth = 8)
    {
        FILL_CHROMA_RECONSTRUCT_INFO_8U;
        return ippiReconstructChroma422IntraHalf4x4_H264High_16s8u_IP2R(info,
                                                                        intraChromaMode,
                                                                        edgeTypeTop,
                                                                        edgeTypeBottom,
                                                                        levelScaleDCU,
                                                                        levelScaleDCV);
    }

    inline IppStatus ReconstructChromaIntraHalfs4x4MB444(Ipp16s **ppSrcDstCoeff,
                                                         Ipp8u *pSrcDstUPlane,
                                                         Ipp8u *pSrcDstVPlane,
                                                         Ipp32u srcdstUVStep,
                                                         IppIntraChromaPredMode_H264 intraChromaMode,
                                                         Ipp32u cbpU,
                                                         Ipp32u cbpV,
                                                         Ipp32u chromaQPU,
                                                         Ipp32u chromaQPV,
                                                         Ipp8u edgeTypeTop,
                                                         Ipp8u edgeTypeBottom,
                                                         const Ipp16s *pQuantTableU,
                                                         const Ipp16s *pQuantTableV,
                                                         Ipp8u bypassFlag,
                                                         Ipp32s bit_depth = 8)
    {
        VM_ASSERT(false);
        return ippStsNoErr;/*ippiReconstructChromaIntraHalfs4x4MB444_H264_16s8u_P2R_(ppSrcDstCoeff,
            pSrcDstUPlane,
            pSrcDstVPlane,
            srcdstUVStep,
            intraChromaMode,
            CreateIPPCBPMask444(cbpU, cbpV),
            chromaQPU,
            chromaQPV,
            edgeTypeTop,
            edgeTypeBottom,
            pQuantTableU,
            pQuantTableV,
            bypassFlag);*/
    }

   inline IppStatus ReconstructChromaInter4x4MB422(Ipp16s **ppSrcDstCoeff,
                                                   Ipp8u *pSrcDstUPlane,
                                                   Ipp8u *pSrcDstVPlane,
                                                   Ipp32u srcdstUVStep,
                                                   Ipp32u cbpU,
                                                   Ipp32u cbpV,
                                                   Ipp32u chromaQPU,
                                                   Ipp32u chromaQPV,
                                                   Ipp32u levelScaleDCU,
                                                   Ipp32u levelScaleDCV,
                                                   const Ipp16s *pQuantTableU,
                                                   const Ipp16s *pQuantTableV,
                                                   Ipp8u bypass_flag,
                                                   Ipp32s bit_depth = 8)
   {
       FILL_CHROMA_RECONSTRUCT_INFO_8U;
       return ippiReconstructChroma422Inter4x4_H264High_16s8u_IP2R(info,
                                                                   levelScaleDCU,
                                                                   levelScaleDCV);
   }

    inline IppStatus ReconstructChromaInter4x4MB444(Ipp16s **ppSrcDstCoeff,
                                                    Ipp8u *pSrcDstUPlane,
                                                    Ipp8u *pSrcDstVPlane,
                                                    Ipp32u srcdstUVStep,
                                                    Ipp32u cbpU,
                                                    Ipp32u cbpV,
                                                    Ipp32u chromaQPU,
                                                    Ipp32u chromaQPV,
                                                    const Ipp16s *pQuantTableU,
                                                    const Ipp16s *pQuantTableV,
                                                    Ipp8u bypassFlag,
                                                    Ipp32s bit_depth = 8)
   {
       VM_ASSERT(false);
        return ippStsNoErr;/*ippiReconstructChromaInter4x4MB444_H264_16s8u_P2R_(ppSrcDstCoeff,
            pSrcDstUPlane,
            pSrcDstVPlane,
            srcdstUVStep,
            CreateIPPCBPMask444(cbpU, cbpV),
            chromaQPU,
            chromaQPV,
            pQuantTableU,
            pQuantTableV,
            bypassFlag);*/
   }


    inline IppStatus ReconstructChromaIntra4x4MB422(Ipp16s **ppSrcDstCoeff,
                                                    Ipp8u *pSrcDstUPlane,
                                                    Ipp8u *pSrcDstVPlane,
                                                    Ipp32u srcdstUVStep,
                                                    IppIntraChromaPredMode_H264 intraChromaMode,
                                                    Ipp32u cbpU,
                                                    Ipp32u cbpV,
                                                    Ipp32u chromaQPU,
                                                    Ipp32u chromaQPV,
                                                    Ipp32u levelScaleDCU,
                                                    Ipp32u levelScaleDCV,
                                                    Ipp8u edgeType,
                                                    const Ipp16s *pQuantTableU,
                                                    const Ipp16s *pQuantTableV,
                                                    Ipp8u bypass_flag,
                                                    Ipp32s bit_depth = 8)
   {
       FILL_CHROMA_RECONSTRUCT_INFO_8U;
       return ippiReconstructChroma422Intra4x4_H264High_16s8u_IP2R(info,
                                                                   intraChromaMode,
                                                                   edgeType,
                                                                   levelScaleDCU,
                                                                   levelScaleDCV);
   }


    inline IppStatus ReconstructChromaIntra4x4MB444(Ipp16s **ppSrcDstCoeff,
                                                    Ipp8u *pSrcDstUPlane,
                                                    Ipp8u *pSrcDstVPlane,
                                                    Ipp32u srcdstUVStep,
                                                    IppIntraChromaPredMode_H264 intraChromaMode,
                                                    Ipp32u cbpU,
                                                    Ipp32u cbpV,
                                                    Ipp32u chromaQPU,
                                                    Ipp32u chromaQPV,
                                                    Ipp8u edgeType,
                                                    const Ipp16s *pQuantTableU,
                                                    const Ipp16s *pQuantTableV,
                                                    Ipp8u bypassFlag,
                                                    Ipp32s bit_depth = 8)
   {
       VM_ASSERT(false);
       return ippStsNoErr;/*ippiReconstructChromaIntra4x4MB444_H264_16s8u_P2R_(ppSrcDstCoeff,
            pSrcDstUPlane,
            pSrcDstVPlane,
            srcdstUVStep,
            intraChromaMode,
            CreateIPPCBPMask444(cbpU, cbpV),
            chromaQPU,
            chromaQPV,
            edgeType,
            pQuantTableU,
            pQuantTableV,
            bypassFlag);*/
   }

    IppStatus FilterDeblockingLuma_HorEdge(const IppiFilterDeblock_8u * pDeblockInfo);

    IppStatus FilterDeblockingLuma_VerEdge_MBAFF(const IppiFilterDeblock_8u * pDeblockInfo);

    IppStatus FilterDeblockingLuma_VerEdge(const IppiFilterDeblock_8u * pDeblockInfo);

    IppStatus FilterDeblockingChroma_VerEdge_MBAFF(const IppiFilterDeblock_8u * pDeblockInfo);

    IppStatus FilterDeblockingChroma_VerEdge(const IppiFilterDeblock_8u * pDeblockInfo);

    IppStatus FilterDeblockingChroma_HorEdge(const IppiFilterDeblock_8u * pDeblockInfo);

    IppStatus FilterDeblockingChroma422_VerEdge(const IppiFilterDeblock_8u * pDeblockInfo);

    IppStatus FilterDeblockingChroma422_HorEdge(const IppiFilterDeblock_8u * pDeblockInfo);

    IppStatus FilterDeblockingChroma444_VerEdge(const IppiFilterDeblock_8u * pDeblockInfo);

    IppStatus FilterDeblockingChroma444_HorEdge(const IppiFilterDeblock_8u * pDeblockInfo);


    ///****************************************************************************************/
    // 16 bits functions
    ///****************************************************************************************/
    inline void SetPlane(Ipp16u val, Ipp16u* pDst, Ipp32s len)
    {
        ippsSet_16s(val, (Ipp16s *)pDst, len);
    }

    inline void CopyPlane(const Ipp16u *pSrc, Ipp16u *pDst, Ipp32s len)
    {
        ippsCopy_16s((const Ipp16s *)pSrc, (Ipp16s *)pDst, len);
    }


    inline IppStatus SetPlane(Ipp16u value, Ipp16u* pDst, Ipp32s dstStep,
                              IppiSize roiSize )
    {
        if (!pDst)
            return ippStsNullPtrErr;

        return ippiSet_16s_C1R(value, (Ipp16s*)pDst, dstStep,
                        roiSize);
    }

    inline IppStatus CopyPlane(const Ipp16u* pSrc, Ipp32s srcStep, Ipp16u* pDst, Ipp32s dstStep,
                              IppiSize roiSize)
    {
        if (!pSrc || !pDst)
            return ippStsNullPtrErr;

        return ippiCopy_16s_C1R((const Ipp16s*)pSrc, srcStep, (Ipp16s*)pDst, dstStep,
                        roiSize);
    }

    inline  IppStatus ippiInterpolateLuma(const Ipp16u* , const IppVCInterpolateBlock_16u *info_)
    {
        IppVCInterpolate_16u info;

        info.pSrc = info_->pSrc[0];
        info.srcStep = info_->srcStep;
        info.pDst = info_->pDst[0];
        info.dstStep = info_->dstStep;
        info.dx = info_->pointVector.x;
        info.dy = info_->pointVector.y;
        info.roiSize = info_->sizeBlock;
        info.bitDepth = info_->bitDepth;

        return ippiInterpolateLuma_H264_16u_C1R(&info);
    }

    inline IppStatus ippiInterpolateLumaBlock(const Ipp16u *, const IppVCInterpolateBlock_16u *interpolateInfo)
    {
        return ippiInterpolateLumaBlock_H264_16u_P1R(interpolateInfo);
    }

    inline IppStatus ippiInterpolateChromaBlock(const Ipp16u *, const IppVCInterpolateBlock_16u *interpolateInfo)
    {
        return ippiInterpolateChromaBlock_H264_16u_P2R(interpolateInfo);
    }

    inline IppStatus ExpandPlane(Ipp16u *StartPtr,
                                 Ipp32u uFrameWidth,
                                 Ipp32u uFrameHeight,
                                 Ipp32u uPitch,
                                 Ipp32u uPels,
                                 IppvcFrameFieldFlag uFrameFieldFlag)
    {
        VM_ASSERT(false);
        return ippStsNoErr;/*ippiExpandPlane_H264_8u_C1R(StartPtr,
                                            uFrameWidth,
                                            uFrameHeight,
                                            uPitch,
                                            uPels,
                                            uFrameFieldFlag);*/
    }

    inline IppStatus UniDirWeightBlock(Ipp16u *, IppVCBidir1_16u * info,
                                       Ipp32u ulog2wd,
                                       Ipp32s iWeight,
                                       Ipp32s iOffset)
    {
        return ippiUnidirWeight_H264_16u_IP2P1R(info->pDst, info->dstStep,
                                                 ulog2wd, iWeight,
                                                 iOffset, info->roiSize, info->bitDepth);
    }


#define FILL_RECONSTRUCT_INFO   \
    IppiReconstructHighMB_32s16u info;\
    info.ppSrcDstCoeff = ppSrcDstCoeff;\
    info.pSrcDstPlane = pSrcDstYPlane;\
    info.srcDstStep = srcDstStep;\
    info.cbp = cbp;\
    info.qp = QP;\
    info.pQuantTable = (Ipp16s *) pQuantTable;\
    info.bypassFlag = bypass_flag;\
    info.bitDepth = bit_depth;

    inline IppStatus ReconstructLumaIntraHalfMB(Ipp32s **ppSrcCoeff,
                                                Ipp16u *pSrcDstYPlane,
                                                Ipp32s srcdstYStep,
                                                IppIntra4x4PredMode_H264 *pMBIntraTypes,
                                                Ipp32u cbp4x2,
                                                Ipp32s QP,
                                                Ipp8u  edgeType,
                                                Ipp32s bit_depth = 10)
    {
        VM_ASSERT(false);
        return ippStsNoErr;/*ippiReconstructLumaIntraHalfMB_H264_16s8u_C1R(ppSrcCoeff,
                                                                    pSrcDstYPlane,
                                                                    srcdstYStep,
                                                                    pMBIntraTypes,
                                                                    cbp4x2,
                                                                    QP,
                                                                    edgeType);*/
    }

    inline IppStatus ReconstructLumaInter8x8MB(Ipp32s **ppSrcDstCoeff,
                                               Ipp16u *pSrcDstYPlane,
                                               Ipp32u srcDstStep,
                                               Ipp32u cbp,
                                               Ipp32s QP,
                                               const Ipp16s *pQuantTable,
                                               Ipp8u  bypass_flag,
                                               Ipp32s bit_depth)
    {
        FILL_RECONSTRUCT_INFO;
        return ippiReconstructLumaInter8x8_H264High_32s16u_IP1R(&info);
    }

    inline IppStatus ReconstructLumaInter4x4MB(Ipp32s **ppSrcDstCoeff,
                                               Ipp16u *pSrcDstYPlane,
                                               Ipp32u srcDstStep,
                                               Ipp32u cbp,
                                               Ipp32s QP,
                                               const Ipp16s *pQuantTable,
                                               Ipp8u  bypass_flag,
                                               Ipp32s bit_depth)
    {
        FILL_RECONSTRUCT_INFO;
        return ippiReconstructLumaInter4x4_H264High_32s16u_IP1R(&info);
    }

    inline IppStatus ReconstructLumaIntra_16x16MB(Ipp32s **ppSrcDstCoeff,
                                                  Ipp16u *pSrcDstYPlane,
                                                  Ipp32u srcDstStep,
                                                  IppIntra16x16PredMode_H264 intra_luma_mode,
                                                  Ipp32u cbp,
                                                  Ipp32s QP,
                                                  Ipp8u  edge_type,
                                                  const Ipp16s *pQuantTable,
                                                  Ipp8u  bypass_flag,
                                                  Ipp32s bit_depth)
    {
        FILL_RECONSTRUCT_INFO;
        return ippiReconstructLumaIntra16x16_H264High_32s16u_IP1R(&info,
                                                                  intra_luma_mode,
                                                                  edge_type);
    }

    inline IppStatus ReconstructLumaIntraHalf8x8MB(Ipp32s **ppSrcDstCoeff,
                                                   Ipp16u *pSrcDstYPlane,
                                                   Ipp32s srcDstStep,
                                                   IppIntra8x8PredMode_H264 *pMBIntraTypes,
                                                   Ipp32u cbp,
                                                   Ipp32s QP,
                                                   Ipp8u  edgeType,
                                                   const Ipp16s *pQuantTable,
                                                   Ipp8u  bypass_flag,
                                                   Ipp32s bit_depth = 10)
    {
        FILL_RECONSTRUCT_INFO;
        return ippiReconstructLumaIntraHalf8x8_H264High_32s16u_IP1R(&info,
                                                                    pMBIntraTypes,
                                                                    edgeType);
    }

    inline IppStatus ReconstructLumaIntraHalf4x4MB(Ipp32s **ppSrcDstCoeff,
                                                   Ipp16u *pSrcDstYPlane,
                                                   Ipp32s srcDstStep,
                                                   IppIntra4x4PredMode_H264 *pMBIntraTypes,
                                                   Ipp32u cbp,
                                                   Ipp32s QP,
                                                   Ipp8u  edgeType,
                                                   const Ipp16s *pQuantTable,
                                                   Ipp8u  bypass_flag,
                                                   Ipp32s bit_depth = 10)
    {
        FILL_RECONSTRUCT_INFO;
        return ippiReconstructLumaIntraHalf4x4_H264High_32s16u_IP1R(&info,
                                                                    pMBIntraTypes,
                                                                    edgeType);
    }

    inline IppStatus ReconstructLumaIntra8x8MB(Ipp32s **ppSrcDstCoeff,
                                               Ipp16u *pSrcDstYPlane,
                                               Ipp32s srcDstStep,
                                               IppIntra8x8PredMode_H264 *pMBIntraTypes,
                                               Ipp32u cbp,
                                               Ipp32s QP,
                                               Ipp8u  edgeType,
                                               const Ipp16s *pQuantTable,
                                               Ipp8u  bypass_flag,
                                               Ipp32s bit_depth)
    {
        FILL_RECONSTRUCT_INFO;
        return ippiReconstructLumaIntra8x8_H264High_32s16u_IP1R(&info,
                                                                pMBIntraTypes,
                                                                edgeType);
    }

    inline  IppStatus ReconstructLumaIntra4x4MB(Ipp32s **ppSrcDstCoeff,
                                                Ipp16u *pSrcDstYPlane,
                                                Ipp32s srcDstStep,
                                                IppIntra4x4PredMode_H264 *pMBIntraTypes,
                                                Ipp32u cbp,
                                                Ipp32s QP,
                                                Ipp8u  edgeType,
                                                const Ipp16s *pQuantTable,
                                                Ipp8u  bypass_flag,
                                                Ipp32s bit_depth)
    {
        FILL_RECONSTRUCT_INFO;
        return ippiReconstructLumaIntra4x4_H264High_32s16u_IP1R(&info,
                                                                pMBIntraTypes,
                                                                edgeType);
    }

    inline IppStatus ReconstructLumaIntra16x16MB(Ipp32s **ppSrcCoeff,
                                                 Ipp16u *pSrcDstYPlane,
                                                 Ipp32u srcdstYStep,
                                                 const IppIntra16x16PredMode_H264 intra_luma_mode,
                                                 const Ipp32u cbp4x4,
                                                 const Ipp32s QP,
                                                 const Ipp8u edge_type,
                                                 Ipp32s bit_depth = 10)
    {
        VM_ASSERT(false);
        return ippStsNoErr;/*ippiReconstructLumaIntra16x16MB_H264_16s8u_C1R(ppSrcCoeff,
                                                                   pSrcDstYPlane,
                                                                   srcdstYStep,
                                                                   intra_luma_mode,
                                                                   cbp4x4,
                                                                   QP,
                                                                   edge_type);*/
    }

    inline IppStatus ReconstructLumaIntraMB(Ipp32s **ppSrcCoeff,
                                            Ipp16u *pSrcDstYPlane,
                                            Ipp32s srcdstYStep,
                                            const IppIntra4x4PredMode_H264 *pMBIntraTypes,
                                            const Ipp32u cbp4x4,
                                            const Ipp32s QP,
                                            const Ipp8u edgeType,
                                            Ipp32s bit_depth = 10)
    {
        VM_ASSERT(false);
        return ippStsNoErr;/*ippiReconstructLumaIntraMB_H264_16s8u_C1R(ppSrcCoeff,
                                                        pSrcDstYPlane,
                                                        srcdstYStep,
                                                        pMBIntraTypes,
                                                        cbp4x4,
                                                        QP,
                                                        edgeType);*/
    }

    inline IppStatus ReconstructLumaInterMB(Ipp32s **ppSrcCoeff,
                                            Ipp16u *pSrcDstYPlane,
                                            Ipp32u srcdstYStep,
                                            Ipp32u cbp4x4,
                                            Ipp32s QP,
                                            Ipp32s bit_depth = 10)
    {
        VM_ASSERT(false);
        return ippStsNoErr;/*ippiReconstructLumaInterMB_H264_16s8u_C1R(ppSrcCoeff,
                                                        pSrcDstYPlane,
                                                        srcdstYStep,
                                                        cbp4x4,
                                                        QP);*/
    }

#define FILL_CHROMA_RECONSTRUCT_INFO    \
    IppiReconstructHighMB_32s16u info_temp[2];\
    const IppiReconstructHighMB_32s16u *info[2];\
    info[0] = &info_temp[0];\
    info[1] = &info_temp[1];\
    info_temp[0].ppSrcDstCoeff = ppSrcDstCoeff;   \
    info_temp[0].pSrcDstPlane = pSrcDstUPlane;  \
    info_temp[0].srcDstStep = srcdstUVStep;    \
    info_temp[0].cbp = (Ipp32u)cbpU;          \
    info_temp[0].qp = chromaQPU;               \
    info_temp[0].pQuantTable = (Ipp16s *) pQuantTableU;  \
    info_temp[0].bypassFlag = bypass_flag;      \
    info_temp[0].bitDepth = bit_depth;            \
    info_temp[1].ppSrcDstCoeff = ppSrcDstCoeff;   \
    info_temp[1].pSrcDstPlane = pSrcDstVPlane;  \
    info_temp[1].srcDstStep = srcdstUVStep;    \
    info_temp[1].cbp = (Ipp32u)cbpV;          \
    info_temp[1].qp = chromaQPV;\
    info_temp[1].pQuantTable = (Ipp16s *) pQuantTableV;\
    info_temp[1].bypassFlag = bypass_flag;\
    info_temp[1].bitDepth = bit_depth;

    inline IppStatus ReconstructChromaInter4x4MB(Ipp32s **ppSrcDstCoeff,
                                                 Ipp16u *pSrcDstUPlane,
                                                 Ipp16u *pSrcDstVPlane,
                                                 Ipp32u srcdstUVStep,
                                                 Ipp32u cbpU,
                                                 Ipp32u cbpV,
                                                 Ipp32u chromaQPU,
                                                 Ipp32u chromaQPV,
                                                 const Ipp16s *pQuantTableU,
                                                 const Ipp16s *pQuantTableV,
                                                 Ipp8u  bypass_flag,
                                                 Ipp32s bit_depth)
    {
        FILL_CHROMA_RECONSTRUCT_INFO;
        return ippiReconstructChromaInter4x4_H264High_32s16u_IP2R(info);
    }

    inline IppStatus ReconstructChromaIntra4x4MB(Ipp32s **ppSrcDstCoeff,
                                                 Ipp16u *pSrcDstUPlane,
                                                 Ipp16u *pSrcDstVPlane,
                                                 Ipp32u srcdstUVStep,
                                                 IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                 Ipp32u cbpU,
                                                 Ipp32u cbpV,
                                                 Ipp32u chromaQPU,
                                                 Ipp32u chromaQPV,
                                                 Ipp8u  edge_type,
                                                 const Ipp16s *pQuantTableU,
                                                 const Ipp16s *pQuantTableV,
                                                 Ipp8u  bypass_flag,
                                                 Ipp32s bit_depth)
    {
        FILL_CHROMA_RECONSTRUCT_INFO;
        return ippiReconstructChromaIntra4x4_H264High_32s16u_IP2R(info,
            intra_chroma_mode,
            edge_type);
    }


    inline IppStatus ReconstructChromaIntraHalfs4x4MB(Ipp32s **ppSrcDstCoeff,
                                                      Ipp16u *pSrcDstUPlane,
                                                      Ipp16u *pSrcDstVPlane,
                                                      Ipp32u srcdstUVStep,
                                                      IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                      Ipp32u cbpU,
                                                      Ipp32u cbpV,
                                                      Ipp32u chromaQPU,
                                                      Ipp32u chromaQPV,
                                                      Ipp8u  edge_type_top,
                                                      Ipp8u  edge_type_bottom,
                                                      const Ipp16s *pQuantTableU,
                                                      const Ipp16s *pQuantTableV,
                                                      Ipp8u  bypass_flag,
                                                      Ipp32s bit_depth = 10)
    {
        FILL_CHROMA_RECONSTRUCT_INFO;
        return ippiReconstructChromaIntraHalf4x4_H264High_32s16u_IP2R(info,
                                                                      intra_chroma_mode,
                                                                      edge_type_top,
                                                                      edge_type_bottom);
    }

    inline IppStatus ReconstructChromaIntraHalfs4x4MB422(Ipp32s **ppSrcDstCoeff,
                                                         Ipp16u *pSrcDstUPlane,
                                                         Ipp16u *pSrcDstVPlane,
                                                         Ipp32u srcdstUVStep,
                                                         IppIntraChromaPredMode_H264 intraChromaMode,
                                                         Ipp32u cbpU,
                                                         Ipp32u cbpV,
                                                         Ipp32u chromaQPU,
                                                         Ipp32u chromaQPV,
                                                         Ipp32u levelScaleDCU,
                                                         Ipp32u levelScaleDCV,
                                                         Ipp8u  edgeTypeTop,
                                                         Ipp8u  edgeTypeBottom,
                                                         const Ipp16s *pQuantTableU,
                                                         const Ipp16s *pQuantTableV,
                                                         Ipp8u  bypass_flag,
                                                         Ipp32s bit_depth = 10)
    {
        FILL_CHROMA_RECONSTRUCT_INFO;
        return ippiReconstructChroma422IntraHalf4x4_H264High_32s16u_IP2R(info,
            intraChromaMode,
            edgeTypeTop,
            edgeTypeBottom,
            levelScaleDCU,
            levelScaleDCV);
    }

    inline IppStatus ReconstructChromaIntraHalfs4x4MB444(Ipp32s **ppSrcDstCoeff,
                                                         Ipp16u *pSrcDstUPlane,
                                                         Ipp16u *pSrcDstVPlane,
                                                         Ipp32u srcdstUVStep,
                                                         IppIntraChromaPredMode_H264 intraChromaMode,
                                                         Ipp32u cbpU,
                                                         Ipp32u cbpV,
                                                         Ipp32u chromaQPU,
                                                         Ipp32u chromaQPV,
                                                         Ipp8u  edgeTypeTop,
                                                         Ipp8u  edgeTypeBottom,
                                                         const Ipp16s *pQuantTableU,
                                                         const Ipp16s *pQuantTableV,
                                                         Ipp8u  bypassFlag,
                                                         Ipp32s bit_depth = 10)
    {
        VM_ASSERT(false);
        return ippStsNoErr;
    }

   inline IppStatus ReconstructChromaInter4x4MB422(Ipp32s **ppSrcDstCoeff,
                                                   Ipp16u *pSrcDstUPlane,
                                                   Ipp16u *pSrcDstVPlane,
                                                   Ipp32u srcdstUVStep,
                                                   Ipp32u cbpU,
                                                   Ipp32u cbpV,
                                                   Ipp32u chromaQPU,
                                                   Ipp32u chromaQPV,
                                                   Ipp32u levelScaleDCU,
                                                   Ipp32u levelScaleDCV,
                                                   const Ipp16s *pQuantTableU,
                                                   const Ipp16s *pQuantTableV,
                                                   Ipp8u  bypass_flag,
                                                   Ipp32s bit_depth)
   {
       FILL_CHROMA_RECONSTRUCT_INFO;
       return ippiReconstructChroma422Inter4x4_H264High_32s16u_IP2R(info,
                                                                    levelScaleDCU,
                                                                    levelScaleDCV);
   }

    inline IppStatus ReconstructChromaInter4x4MB444(Ipp32s **ppSrcDstCoeff,
                                                    Ipp16u *pSrcDstUPlane,
                                                    Ipp16u *pSrcDstVPlane,
                                                    Ipp32u srcdstUVStep,
                                                    Ipp32u cbpU,
                                                    Ipp32u cbpV,
                                                    Ipp32u chromaQPU,
                                                    Ipp32u chromaQPV,
                                                    const Ipp16s *pQuantTableU,
                                                    const Ipp16s *pQuantTableV,
                                                    Ipp8u  bypassFlag,
                                                    Ipp32s bit_depth = 10)
   {
       VM_ASSERT(false);
       return ippStsNoErr;
   }


    inline IppStatus ReconstructChromaInterMB(Ipp32s **ppSrcCoeff,
                                              Ipp16u *pSrcDstUPlane,
                                              Ipp16u *pSrcDstVPlane,
                                              const Ipp32u srcdstStep,
                                              Ipp32u cbpU,
                                              Ipp32u cbpV,
                                              const Ipp32u ChromaQP,
                                              Ipp32s bit_depth = 10)
    {
        VM_ASSERT(false);
        return ippStsNoErr;
    }

    inline IppStatus ReconstructChromaIntraMB(Ipp32s **ppSrcCoeff,
                                              Ipp16u *pSrcDstUPlane,
                                              Ipp16u *pSrcDstVPlane,
                                              const Ipp32u srcdstUVStep,
                                              const IppIntraChromaPredMode_H264 intra_chroma_mode,
                                              Ipp32u cbpU,
                                              Ipp32u cbpV,
                                              const Ipp32u ChromaQP,
                                              const Ipp8u edge_type,
                                              Ipp32s bit_depth = 10)
    {
        VM_ASSERT(false);
        return ippStsNoErr;
    }


    inline IppStatus ReconstructChromaIntraHalfsMB(Ipp32s **ppSrcCoeff,
                                                   Ipp16u *pSrcDstUPlane,
                                                   Ipp16u *pSrcDstVPlane,
                                                   Ipp32u srcdstUVStep,
                                                   IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                   Ipp32u cbpU,
                                                   Ipp32u cbpV,
                                                   Ipp32u ChromaQP,
                                                   Ipp8u  edge_type_top,
                                                   Ipp8u  edge_type_bottom,
                                                   Ipp32s bit_depth = 10)
    {
        VM_ASSERT(false);
        return ippStsNoErr;
    }

    inline IppStatus ReconstructChromaIntra4x4MB422(Ipp32s **ppSrcDstCoeff,
                                                    Ipp16u *pSrcDstUPlane,
                                                    Ipp16u *pSrcDstVPlane,
                                                    Ipp32u srcdstUVStep,
                                                    IppIntraChromaPredMode_H264 intraChromaMode,
                                                    Ipp32u cbpU,
                                                    Ipp32u cbpV,
                                                    Ipp32u chromaQPU,
                                                    Ipp32u chromaQPV,
                                                    Ipp32u levelScaleDCU,
                                                    Ipp32u levelScaleDCV,
                                                    Ipp8u  edgeType,
                                                    const Ipp16s *pQuantTableU,
                                                    const Ipp16s *pQuantTableV,
                                                    Ipp8u  bypass_flag,
                                                    Ipp32s bit_depth)
   {
       FILL_CHROMA_RECONSTRUCT_INFO;
       return ippiReconstructChroma422Intra4x4_H264High_32s16u_IP2R(info,
                                                                    intraChromaMode,
                                                                    edgeType,
                                                                    levelScaleDCU,
                                                                    levelScaleDCV);
   }

    inline IppStatus ReconstructChromaIntra4x4MB444(Ipp32s **ppSrcDstCoeff,
                                                    Ipp16u *pSrcDstUPlane,
                                                    Ipp16u *pSrcDstVPlane,
                                                    Ipp32u srcdstUVStep,
                                                    IppIntraChromaPredMode_H264 intraChromaMode,
                                                    Ipp32u cbpU,
                                                    Ipp32u cbpV,
                                                    Ipp32u chromaQPU,
                                                    Ipp32u chromaQPV,
                                                    Ipp8u  edgeType,
                                                    const Ipp16s *pQuantTableU,
                                                    const Ipp16s *pQuantTableV,
                                                    Ipp8u  bypassFlag,
                                                    Ipp32s bit_depth)
   {
       VM_ASSERT(false);
        return ippStsNoErr;
   }

    IppStatus FilterDeblockingLuma_HorEdge(Ipp16u* pSrcDst,
                                           Ipp32s  srcdstStep,
                                           Ipp8u*  pAlpha,
                                           Ipp8u*  pBeta,
                                           Ipp8u*  pThresholds,
                                           Ipp8u*  pBS,
                                           Ipp32s  bit_depth);

    IppStatus FilterDeblockingLuma_VerEdge_MBAFF(Ipp16u* pSrcDst,
                                                 Ipp32s  srcdstStep,
                                                 Ipp8u*  pAlpha,
                                                 Ipp8u*  pBeta,
                                                 Ipp8u*  pThresholds,
                                                 Ipp8u*  pBs,
                                                 Ipp32s  bit_depth);

    IppStatus FilterDeblockingLuma_VerEdge(Ipp16u* pSrcDst,
                                           Ipp32s  srcdstStep,
                                           Ipp8u*  pAlpha,
                                           Ipp8u*  pBeta,
                                           Ipp8u*  pThresholds,
                                           Ipp8u*  pBs,
                                           Ipp32s  bit_depth);

    IppStatus FilterDeblockingChroma_VerEdge_MBAFF(Ipp16u* pSrcDst,
                                                   Ipp32s  srcdstStep,
                                                   Ipp8u*  pAlpha,
                                                   Ipp8u*  pBeta,
                                                   Ipp8u*  pThresholds,
                                                   Ipp8u*  pBS,
                                                   Ipp32s  bit_depth);

    IppStatus FilterDeblockingChroma_VerEdge(Ipp16u* pSrcDst,
                                             Ipp32s  srcdstStep,
                                             Ipp8u*  pAlpha,
                                             Ipp8u*  pBeta,
                                             Ipp8u*  pThresholds,
                                             Ipp8u*  pBS,
                                             Ipp32s  bit_depth);

    IppStatus FilterDeblockingChroma_HorEdge(Ipp16u* pSrcDst,
                                             Ipp32s  srcdstStep,
                                             Ipp8u*  pAlpha,
                                             Ipp8u*  pBeta,
                                             Ipp8u*  pThresholds,
                                             Ipp8u*  pBS,
                                             Ipp32s  bit_depth);

    IppStatus FilterDeblockingChroma422_VerEdge(Ipp16u* pSrcDst,
                                                Ipp32s  srcdstStep,
                                                Ipp8u*  pAlpha,
                                                Ipp8u*  pBeta,
                                                Ipp8u*  pThresholds,
                                                Ipp8u*  pBS,
                                                Ipp32s  bit_depth);

    IppStatus FilterDeblockingChroma422_HorEdge(Ipp16u* pSrcDst,
                                                Ipp32s  srcdstStep,
                                                Ipp8u*  pAlpha,
                                                Ipp8u*  pBeta,
                                                Ipp8u*  pThresholds,
                                                Ipp8u*  pBS,
                                                Ipp32s  bit_depth);

    IppStatus FilterDeblockingChroma444_VerEdge(Ipp16u* pSrcDst,
                                                Ipp32s  srcdstStep,
                                                Ipp8u*  pAlpha,
                                                Ipp8u*  pBeta,
                                                Ipp8u*  pThresholds,
                                                Ipp8u*  pBS,
                                                Ipp32s  bit_depth);

    IppStatus FilterDeblockingChroma444_HorEdge(Ipp16u* pSrcDst,
                                                Ipp32s  srcdstStep,
                                                Ipp8u*  pAlpha,
                                                Ipp8u*  pBeta,
                                                Ipp8u*  pThresholds,
                                                Ipp8u*  pBS,
                                                Ipp32s  bit_depth);

#define FILL_VC_BIDIR_INFO  \
        IppVCBidir_16u info;\
        info.pSrc1 = info1->pSrc[0];\
        info.srcStep1 = info1->srcStep[0];\
        info.pSrc2 = info1->pSrc[1];\
        info.srcStep2 = info1->srcStep[1];\
        info.pDst = info1->pDst;\
        info.dstStep = info1->dstStep;\
        info.roiSize = info1->roiSize;\
        info.bitDepth = info1->bitDepth;

    inline IppStatus BiDirWeightBlock(Ipp16u * , IppVCBidir1_16u * info1,
                                      Ipp32u ulog2wd,
                                      Ipp32s iWeight1,
                                      Ipp32s iOffset1,
                                      Ipp32s iWeight2,
                                      Ipp32s iOffset2)
    {
        FILL_VC_BIDIR_INFO;
        return ippiBidirWeight_H264_16u_P2P1R(&info, ulog2wd, iWeight1, iOffset1, iWeight2, iOffset2);
    }

    inline IppStatus BiDirWeightBlockImplicit(Ipp16u * , IppVCBidir1_16u * info1,
                                              Ipp32s iWeight1,
                                              Ipp32s iWeight2)
    {
        FILL_VC_BIDIR_INFO;
        return ippiBidirWeightImplicit_H264_16u_P2P1R(&info,
                                                      iWeight1,
                                                      iWeight2);
    }

    inline IppStatus InterpolateBlock(Ipp16u * , IppVCBidir1_16u * info1)
    {
        FILL_VC_BIDIR_INFO
        return ippiBidir_H264_16u_P2P1R(&info);
    }

    inline IppStatus DecodeCAVLCChromaDcCoeffs422_H264(Ipp32u **ppBitStream,
                                                       Ipp32s *pOffset,
                                                       Ipp16s *pNumCoeff,
                                                       Ipp32s **ppDstCoeffs,
                                                       const Ipp32s *pTblCoeffToken,
                                                       const Ipp32s **ppTblTotalZerosCR,
                                                       const Ipp32s **ppTblRunBefore)
    {
        return ippiDecodeCAVLCChroma422DcCoeffs_H264_1u32s(ppBitStream,
                                                            pOffset,
                                                            pNumCoeff,
                                                            ppDstCoeffs,
                                                            pTblCoeffToken,
                                                            ppTblTotalZerosCR,
                                                            ppTblRunBefore);
    }

    inline IppStatus DecodeCAVLCCoeffs_H264(Ipp32u **ppBitStream,
                                            Ipp32s *pOffset,
                                            Ipp16s *pNumCoeff,
                                            Ipp32s **ppDstCoeffs,
                                            Ipp32u uVLCSelect,
                                            Ipp16s uMaxNumCoeff,
                                            const Ipp32s **ppTblCoeffToken,
                                            const Ipp32s **ppTblTotalZeros,
                                            const Ipp32s **ppTblRunBefore,
                                            const Ipp32s *pScanMatrix)
    {
        return ippiDecodeCAVLCCoeffs_H264_1u32s(ppBitStream,
                                                        pOffset,
                                                        pNumCoeff,
                                                        ppDstCoeffs,
                                                        uVLCSelect,
                                                        uMaxNumCoeff,
                                                        ppTblCoeffToken,
                                                        ppTblTotalZeros,
                                                        ppTblRunBefore,
                                                        pScanMatrix);
    }

    inline IppStatus MyDecodeCAVLCChromaDcCoeffs_H264(Ipp32u **ppBitStream,
                                                    Ipp32s *pOffset,
                                                    Ipp16s *pNumCoeff,
                                                    Ipp32s **ppDstCoeffs)
    {
        return MyippiDecodeCAVLCChromaDcCoeffs_H264_1u32s(ppBitStream,
                                                        pOffset,
                                                        pNumCoeff,
                                                        ppDstCoeffs);
    }

    inline IppStatus MyDecodeCAVLCCoeffs_H264(Ipp32u **ppBitStream,
                                            Ipp32s *pOffset,
                                            Ipp16s *pNumCoeff,
                                            Ipp32s **ppDstCoeffs,
                                            Ipp32u uVLCSelect,
                                            Ipp16s coeffLimit,
                                            Ipp16s uMaxNumCoeff,
                                            const Ipp32s *pScanMatrix,
                                            Ipp32s startIdx)
    {
        return MyippiDecodeCAVLCCoeffs_H264_1u32s(ppBitStream,
                                                pOffset,
                                                pNumCoeff,
                                                ppDstCoeffs,
                                                uVLCSelect,
                                                uMaxNumCoeff,
                                                pScanMatrix);
    }

// nv12 functions
    inline IppStatus ReconstructChromaIntraMB_NV12(Ipp16s **ppSrcCoeff,
                                              Ipp8u *pSrcDstUVPlane,
                                              const Ipp32u srcdstUVStep,
                                              const IppIntraChromaPredMode_H264 intra_chroma_mode,
                                              Ipp32u cbpU,
                                              Ipp32u cbpV,
                                              const Ipp32u ChromaQP,
                                              const Ipp8u edge_type,
                                              Ipp32s bit_depth = 8)
    {
        IppStatus sts = ippiReconstructChromaIntraMB_H264_16s8u_C2R(ppSrcCoeff,
                                                           pSrcDstUVPlane,
                                                           srcdstUVStep,
                                                           intra_chroma_mode,
                                                           CreateIPPCBPMask420(cbpU, cbpV),
                                                           ChromaQP,
                                                           edge_type);
        return sts;
    }

    inline IppStatus ReconstructChromaIntraMB_NV12(Ipp32s **ppSrcCoeff,
                                              Ipp16u *pSrcDstUPlane,
                                              const Ipp32u srcdstUVStep,
                                              const IppIntraChromaPredMode_H264 intra_chroma_mode,
                                              Ipp32u cbpU,
                                              Ipp32u cbpV,
                                              const Ipp32u ChromaQP,
                                              const Ipp8u edge_type,
                                              Ipp32s bit_depth = 10)
    {
        VM_ASSERT(false);
        return ippStsNoErr;
    }

    inline IppStatus ReconstructChromaInterMB_NV12(Ipp16s **ppSrcCoeff,
                                              Ipp8u *pSrcDstUVPlane,
                                              const Ipp32u srcdstStep,
                                              Ipp32u cbpU,
                                              Ipp32u cbpV,
                                              const Ipp32u ChromaQP,
                                              Ipp32s bit_depth = 8)
    {
        IppStatus sts = ippiReconstructChromaInterMB_H264_16s8u_C2R(ppSrcCoeff,
                                                           pSrcDstUVPlane,
                                                           srcdstStep,
                                                           CreateIPPCBPMask420(cbpU, cbpV),
                                                           ChromaQP);
        return sts;
    }

    inline IppStatus ReconstructChromaInterMB_NV12(Ipp32s **ppSrcCoeff,
                                              Ipp16u *pSrcDstUVPlane,
                                              const Ipp32u srcdstStep,
                                              Ipp32u cbpU,
                                              Ipp32u cbpV,
                                              const Ipp32u ChromaQP,
                                              Ipp32s bit_depth = 10)
    {
        VM_ASSERT(false);
        return ippStsNoErr;
    }

    inline IppStatus ippiInterpolateChromaBlockNV12(const Ipp8u *, const IppVCInterpolateBlock_16u *interpolateInfo)
    {
        return ippiInterpolateChromaBlock_H264_8u_C2C2R((IppVCInterpolateBlock_8u *)interpolateInfo);;
    }

    inline IppStatus ippiInterpolateChromaBlockNV12(const Ipp16u *, const IppVCInterpolateBlock_16u *interpolateInfo_)
    {
        IppVCInterpolateBlock_16u * interpolateInfo = (IppVCInterpolateBlock_16u *)interpolateInfo_;
        Ipp16u pSrcDstUPlane[64];
        Ipp16u pSrcDstVPlane[64];

        const Ipp16u * uvPlane = interpolateInfo->pSrc[0];
        Ipp32s uvStep = interpolateInfo->srcStep;

        ConvertNV12ToYV12(uvPlane, uvStep, pSrcDstUPlane, pSrcDstVPlane, 8, interpolateInfo->sizeBlock);

        interpolateInfo->pSrc[0] = pSrcDstUPlane;
        interpolateInfo->pSrc[1] = pSrcDstVPlane;
        interpolateInfo->srcStep = 8;

        IppStatus sts = ippiInterpolateChromaBlock_H264_16u_P2R(interpolateInfo);

        interpolateInfo->pSrc[0] = uvPlane;
        interpolateInfo->srcStep = uvStep;
        ConvertYV12ToNV12(pSrcDstUPlane, pSrcDstVPlane, 8, (Ipp16u*)interpolateInfo->pSrc[0], uvStep, interpolateInfo->sizeBlock);
        return sts;
    }


    IppStatus FilterDeblockingChroma_VerEdge_NV12(const IppiFilterDeblock_8u * pDeblockInfo);

    IppStatus FilterDeblockingChroma_HorEdge_NV12(const IppiFilterDeblock_8u * pDeblockInfo);

    IppStatus FilterDeblockingChroma_VerEdge_MBAFF_NV12(const IppiFilterDeblock_8u * pDeblockInfo);

    inline IppStatus InterpolateBlock_NV12(Ipp8u *pSrc1,
                                      IppVCBidir1_16u * info_)
    {
        IppVCBidir1_8u * info = (IppVCBidir1_8u *)info_;
        return ippiInterpolateBlock_H264_8u_P3P1R(info->pSrc[0], info->pSrc[1],
                                                  info->pDst,
                                                  info->roiSize.width * 2,
                                                  info->roiSize.height,
                                                  info->srcStep[0],
                                                  info->srcStep[1],
                                                  info->dstStep);
    }

    inline IppStatus InterpolateBlock_NV12(Ipp16u *pSrc1,
                                      IppVCBidir1_16u * info)
    {
        VM_ASSERT(false);
        return ippStsNoErr;
    }

    inline IppStatus UniDirWeightBlock_NV12(Ipp8u *,
                                       IppVCBidir1_16u * info_,
                                       Ipp32u ulog2wd,
                                       Ipp32s iWeightU,
                                       Ipp32s iOffsetU,
                                       Ipp32s iWeightV,
                                       Ipp32s iOffsetV)
    {
        IppVCBidir1_8u * info = (IppVCBidir1_8u *)info_;
        return ippiUniDirWeightBlock_NV12_H264_8u_C1IR(info->pDst, info->dstStep,
            ulog2wd, iWeightU, iOffsetU, iWeightV, iOffsetV, info->roiSize);
    }

    inline IppStatus UniDirWeightBlock_NV12(Ipp16u *pSrcDst,
                                       IppVCBidir1_16u * info,
                                       Ipp32u ulog2wd,
                                       Ipp32s iWeightU,
                                       Ipp32s iOffsetU,
                                       Ipp32s iWeightV,
                                       Ipp32s iOffsetV)
    {
        VM_ASSERT(false);
        return ippStsNoErr;
    }

    inline IppStatus BiDirWeightBlock_NV12(const Ipp8u *pSrc1,
                                      IppVCBidir1_16u * info_,
                                      Ipp32u ulog2wd,
                                      Ipp32s iWeightU1,
                                      Ipp32s iOffsetU1,
                                      Ipp32s iWeightU2,
                                      Ipp32s iOffsetU2,
                                      Ipp32s iWeightV1,
                                      Ipp32s iOffsetV1,
                                      Ipp32s iWeightV2,
                                      Ipp32s iOffsetV2)
    {
        IppVCBidir1_8u * info = (IppVCBidir1_8u *)info_;
        return ippiBiDirWeightBlock_NV12_H264_8u_P3P1R(info->pSrc[0], info->pSrc[1], info->pDst,
            info->srcStep[0], info->srcStep[1], info->dstStep,
            ulog2wd, iWeightU1, iOffsetU1, iWeightU2, iOffsetU2, iWeightV1, iOffsetV1, iWeightV2, iOffsetV2, info->roiSize);
    }

    inline IppStatus BiDirWeightBlock_NV12(Ipp16u *pSrc1,
                                      IppVCBidir1_16u * info,
                                      Ipp32u ulog2wd,
                                      Ipp32s iWeightU1,
                                      Ipp32s iOffsetU1,
                                      Ipp32s iWeightU2,
                                      Ipp32s iOffsetU2,
                                      Ipp32s iWeightV1,
                                      Ipp32s iOffsetV1,
                                      Ipp32s iWeightV2,
                                      Ipp32s iOffsetV2)
    {
        VM_ASSERT(false);
        return ippStsNoErr;
    }

    inline IppStatus ReconstructChromaIntraHalfsMB_NV12(Ipp16s **ppSrcCoeff,
                                                   Ipp8u *pSrcDstUVPlane,
                                                   Ipp32u srcdstUVStep,
                                                   IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                   Ipp32u cbpU,
                                                   Ipp32u cbpV,
                                                   Ipp32u ChromaQP,
                                                   Ipp8u edge_type_top,
                                                   Ipp8u edge_type_bottom,
                                                   Ipp32s bit_depth = 8)
    {
        return ippiReconstructChromaIntraHalvesMB_H264_16s8u_C2R(ppSrcCoeff,
                                                                pSrcDstUVPlane,
                                                                srcdstUVStep,
                                                                intra_chroma_mode,
                                                                CreateIPPCBPMask420(cbpU, cbpV),
                                                                ChromaQP,
                                                                edge_type_top,
                                                                edge_type_bottom);
    }

    inline IppStatus ReconstructChromaIntraHalfsMB_NV12(Ipp32s **ppSrcCoeff,
                                                   Ipp16u *pSrcDstUVPlane,
                                                   Ipp32u srcdstUVStep,
                                                   IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                   Ipp32u cbpU,
                                                   Ipp32u cbpV,
                                                   Ipp32u ChromaQP,
                                                   Ipp8u  edge_type_top,
                                                   Ipp8u  edge_type_bottom,
                                                   Ipp32s bit_depth = 10)
    {
        VM_ASSERT(false);
        return ippStsNoErr;
    }


    inline IppStatus ReconstructChromaIntra4x4MB_NV12(Ipp16s **ppSrcDstCoeff,
                                                 Ipp8u *pSrcDstUVPlane,
                                                 Ipp32u srcdstUVStep,
                                                 IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                 Ipp32u cbpU,
                                                 Ipp32u cbpV,
                                                 Ipp32u chromaQPU,
                                                 Ipp32u chromaQPV,
                                                 Ipp8u edge_type,
                                                 const Ipp16s *pQuantTableU,
                                                 const Ipp16s *pQuantTableV,
                                                 Ipp8u bypass_flag,
                                                 Ipp32s bit_depth = 8)
    {
        return ippiReconstructChromaIntra4x4MB_H264_16s8u_C2R(ppSrcDstCoeff,
                                                              pSrcDstUVPlane,
                                                              srcdstUVStep,
                                                              intra_chroma_mode,
                                                              CreateIPPCBPMask420(cbpU, cbpV),
                                                              chromaQPU,
                                                              chromaQPV,
                                                              edge_type,
                                                              pQuantTableU,
                                                              pQuantTableV,
                                                              bypass_flag);
    }

    inline IppStatus ReconstructChromaIntra4x4MB_NV12(Ipp32s **ppSrcDstCoeff,
                                                 Ipp16u *pSrcDstUVPlane,
                                                 Ipp32u srcdstUVStep,
                                                 IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                 Ipp32u cbpU,
                                                 Ipp32u cbpV,
                                                 Ipp32u chromaQPU,
                                                 Ipp32u chromaQPV,
                                                 Ipp8u  edge_type,
                                                 const Ipp16s *pQuantTableU,
                                                 const Ipp16s *pQuantTableV,
                                                 Ipp8u  bypass_flag,
                                                 Ipp32s bit_depth)
    {
        VM_ASSERT(false);
        return ippStsNoErr;
    }

    inline IppStatus ReconstructChromaInter4x4MB_NV12(Ipp16s **ppSrcDstCoeff,
                                                 Ipp8u *pSrcDstUVPlane,
                                                 Ipp32u srcdstUVStep,
                                                 Ipp32u cbpU,
                                                 Ipp32u cbpV,
                                                 Ipp32u chromaQPU,
                                                 Ipp32u chromaQPV,
                                                 const Ipp16s *pQuantTableU,
                                                 const Ipp16s *pQuantTableV,
                                                 Ipp8u bypass_flag,
                                                 Ipp32s bit_depth = 8)
    {
        return ippiReconstructChromaInter4x4MB_H264_16s8u_C2R(ppSrcDstCoeff,
                                                              pSrcDstUVPlane,
                                                              srcdstUVStep,
                                                              CreateIPPCBPMask420(cbpU, cbpV),
                                                              chromaQPU,
                                                              chromaQPV,
                                                              pQuantTableU,
                                                              pQuantTableV,
                                                              bypass_flag);
    }

    inline IppStatus ReconstructChromaInter4x4MB_NV12(Ipp32s **ppSrcDstCoeff,
                                                 Ipp16u *pSrcDstUVPlane,
                                                 Ipp32u srcdstUVStep,
                                                 Ipp32u cbpU,
                                                 Ipp32u cbpV,
                                                 Ipp32u chromaQPU,
                                                 Ipp32u chromaQPV,
                                                 const Ipp16s *pQuantTableU,
                                                 const Ipp16s *pQuantTableV,
                                                 Ipp8u  bypass_flag,
                                                 Ipp32s bit_depth)
    {
        VM_ASSERT(false);
        return ippStsNoErr;
    }

    inline IppStatus ReconstructChromaIntraHalfs4x4MB_NV12(Ipp16s **ppSrcDstCoeff,
                                                      Ipp8u *pSrcDstUVPlane,
                                                      Ipp32u srcdstUVStep,
                                                      IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                      Ipp32u cbpU,
                                                      Ipp32u cbpV,
                                                      Ipp32u chromaQPU,
                                                      Ipp32u chromaQPV,
                                                      Ipp8u edge_type_top,
                                                      Ipp8u edge_type_bottom,
                                                      const Ipp16s *pQuantTableU,
                                                      const Ipp16s *pQuantTableV,
                                                      Ipp8u bypass_flag,
                                                      Ipp32s bit_depth = 8)
    {
        return ippiReconstructChromaIntraHalfs4x4MB_NV12_H264_16s8u_P2R(ppSrcDstCoeff,
                                                                   pSrcDstUVPlane,
                                                                   srcdstUVStep,
                                                                   intra_chroma_mode,
                                                                   CreateIPPCBPMask420(cbpU, cbpV),
                                                                   chromaQPU,
                                                                   chromaQPV,
                                                                   edge_type_top,
                                                                   edge_type_bottom,
                                                                   pQuantTableU,
                                                                   pQuantTableV,
                                                                   bypass_flag);
    }

    inline IppStatus ReconstructChromaIntraHalfs4x4MB_NV12(Ipp32s **ppSrcDstCoeff,
                                                      Ipp16u *pSrcDstUVPlane,
                                                      Ipp32u srcdstUVStep,
                                                      IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                      Ipp32u cbpU,
                                                      Ipp32u cbpV,
                                                      Ipp32u chromaQPU,
                                                      Ipp32u chromaQPV,
                                                      Ipp8u  edge_type_top,
                                                      Ipp8u  edge_type_bottom,
                                                      const Ipp16s *pQuantTableU,
                                                      const Ipp16s *pQuantTableV,
                                                      Ipp8u  bypass_flag,
                                                      Ipp32s bit_depth = 10)
    {
        VM_ASSERT(false);
        return ippStsNoErr;
    }

#pragma warning(default: 4100)

extern Ipp32s resTable0[16];
extern Ipp32s resTable1[16];

extern Ipp32s shiftTable[];
extern Ipp32s bicubicFilter[16][4];
extern Ipp32s bilinearFilter[16][2];

inline Ipp32s countShift(Ipp32s src)
{
    Ipp32s shift;

    if (src == 0)
        return 31;

    shift = 0;
    src--;

    if (src >= 32768)
    {
        src >>= 16;
        shift = 16;
    }

    if (src >= 256)
    {
        src >>= 8;
        shift += 8;
    }

    if (src >= 16)
    {
        src >>= 4;
        shift += 4;
    }

    return (31 - (shift + shiftTable[src]));
}

} // namespace UMC

#if defined(LINUX) && defined(__GNUC__)
# pragma GCC diagnistic pop
#endif

#endif // __UMC_H264_DEC_IPP_WRAP_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
