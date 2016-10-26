//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_DEC_IPP_WRAP_H
#define __UMC_H264_DEC_IPP_WRAP_H

#include "ippi.h"
#include "umc_h264_dec_ipplevel.h"
#include "vm_debug.h"
#include "umc_h264_dec_defs_dec.h"
#include "mfx_h264_dispatcher.h"
#include "umc_h264_dec_structures.h"

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

namespace UMC
{
#pragma warning(disable: 4100)

    template<typename Plane>
    void ippiInterpolateLuma(const IppVCInterpolateBlock_16u *info_)
    {
        if (sizeof(Plane) == 1)
        {
            const IppVCInterpolateBlock_8u * interpolateInfo = (const IppVCInterpolateBlock_8u *) info_;
            ippiInterpolateLuma_H264_8u_C1R(
                interpolateInfo->pSrc[0], interpolateInfo->srcStep, interpolateInfo->pDst[0], interpolateInfo->dstStep,
                interpolateInfo->pointVector.x, interpolateInfo->pointVector.y, interpolateInfo->sizeBlock);
        }
        else
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

            ippiInterpolateLuma_H264_16u_C1R(&info);
        }
    }

    template<typename Plane>
    void ippiInterpolateLumaBlock(const IppVCInterpolateBlock_16u *interpolateInfo)
    {
        if (sizeof(Plane) == 1)
            ippiInterpolateLumaBlock_H264_8u_P1R((const IppVCInterpolateBlock_8u *)interpolateInfo);
        else
            ippiInterpolateLumaBlock_H264_16u_P1R(interpolateInfo);
    }


    template <typename Plane>
    inline IppStatus SetPlane(Plane value, Plane* pDst, Ipp32s dstStep,
                              IppiSize roiSize )
    {
        if (!pDst)
            return ippStsNullPtrErr;

        return sizeof(Plane) == 1 ? ippiSet_8u_C1R((Ipp8u)value, (Ipp8u*)pDst, dstStep, roiSize) : ippiSet_16s_C1R((Ipp16u)value, (Ipp16s*)pDst, dstStep, roiSize);
    }

    template <typename Plane>
    inline IppStatus CopyPlane(const Plane* pSrc, Ipp32s srcStep, Plane* pDst, Ipp32s dstStep,
                              IppiSize roiSize )
    {
        if (!pSrc || !pDst)
            return ippStsNullPtrErr;

        return sizeof(Plane) == 1 ? ippiCopy_8u_C1R((const Ipp8u*)pSrc, srcStep, (Ipp8u*)pDst, dstStep, roiSize) : ippiCopy_16s_C1R((const Ipp16s*)pSrc, srcStep, (Ipp16s*)pDst, dstStep,roiSize);
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

    ///****************************************************************************************/
    // 16 bits functions
    ///****************************************************************************************/

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
        return ippStsNoErr;
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
        return ippStsNoErr;
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
        return ippStsNoErr;
    }

    inline IppStatus ReconstructLumaInterMB(Ipp32s **ppSrcCoeff,
                                            Ipp16u *pSrcDstYPlane,
                                            Ipp32u srcdstYStep,
                                            Ipp32u cbp4x4,
                                            Ipp32s QP,
                                            Ipp32s bit_depth = 10)
    {
        VM_ASSERT(false);
        return ippStsNoErr;
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

    template <typename Plane>
    void ippiInterpolateChromaBlockNV12(const IppVCInterpolateBlock_16u *interpolateInfo)
    {
        if (sizeof(Plane) == 1)
            MFX_H264_PP::GetH264Dispatcher()->InterpolateChromaBlock_8u((IppVCInterpolateBlock_8u *)interpolateInfo);
            //ippiInterpolateChromaBlock_H264_8u_C2C2R((IppVCInterpolateBlock_8u *)interpolateInfo);
        else
            MFX_H264_PP::GetH264Dispatcher()->InterpolateChromaBlock_16u(interpolateInfo);
    }

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
                                      IppVCBidir1_16u * info1)
    {
        FILL_VC_BIDIR_INFO
        info.roiSize.width *= 2;
        return ippiBidir_H264_16u_P2P1R(&info);
    }

    inline void UniDirWeightBlock_NV12(Ipp8u *,
                                       IppVCBidir1_16u * info_,
                                       Ipp32u ulog2wd,
                                       Ipp32s iWeightU,
                                       Ipp32s iOffsetU,
                                       Ipp32s iWeightV,
                                       Ipp32s iOffsetV)
    {
        IppVCBidir1_8u * info = (IppVCBidir1_8u *)info_;
        MFX_H264_PP::GetH264Dispatcher()->UniDirWeightBlock_NV12(info->pDst, info->dstStep, ulog2wd, iWeightU, iOffsetU, iWeightV, iOffsetV, info->roiSize, info_->bitDepth);
    }

    inline void UniDirWeightBlock_NV12(Ipp16u *pSrcDst,
                                       IppVCBidir1_16u * info,
                                       Ipp32u ulog2wd,
                                       Ipp32s iWeightU,
                                       Ipp32s iOffsetU,
                                       Ipp32s iWeightV,
                                       Ipp32s iOffsetV)
    {
        MFX_H264_PP::GetH264Dispatcher()->UniDirWeightBlock_NV12(info->pDst, info->dstStep, ulog2wd, iWeightU, iOffsetU, iWeightV, iOffsetV, info->roiSize, info->bitDepth);
    }

    inline void BiDirWeightBlock_NV12(const Ipp8u *pSrc1,
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
        MFX_H264_PP::GetH264Dispatcher()->BiDirWeightBlock_NV12(info->pSrc[0], info->srcStep[0], info->pSrc[1], info->srcStep[1], info->pDst, info->dstStep,
            ulog2wd, iWeightU1, iOffsetU1, iWeightU2, iOffsetU2, iWeightV1, iOffsetV1, iWeightV2, iOffsetV2, info->roiSize, info_->bitDepth);
    }

    inline void BiDirWeightBlock_NV12(Ipp16u *pSrc1,
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
        MFX_H264_PP::GetH264Dispatcher()->BiDirWeightBlock_NV12(info->pSrc[0], info->srcStep[0], info->pSrc[1], info->srcStep[1], info->pDst, info->dstStep,
            ulog2wd, iWeightU1, iOffsetU1, iWeightU2, iOffsetU2, iWeightV1, iOffsetV1, iWeightV2, iOffsetV2, info->roiSize, info->bitDepth);
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


    inline void ReconstructChromaIntraHalfs4x4MB_NV12(Ipp16s **ppSrcDstCoeff,
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
                                                      Ipp32u  chroma_format_idc)
    {
        MFX_H264_PP::GetH264Dispatcher()->ReconstructChromaIntraHalfs4x4MB_NV12(ppSrcDstCoeff,
                                                      pSrcDstUVPlane, srcdstUVStep,
                                                      intra_chroma_mode,
                                                      cbpU, cbpV,
                                                      chromaQPU, chromaQPV,
                                                      edge_type_top, edge_type_bottom,
                                                      pQuantTableU, pQuantTableV,
                                                      levelScaleDCU, levelScaleDCV,
                                                      bypass_flag, bit_depth, chroma_format_idc);
    }

    inline void ReconstructChromaIntraHalfs4x4MB_NV12(Ipp32s **ppSrcDstCoeff,
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
                                                      Ipp32u  chroma_format_idc)
    {
        MFX_H264_PP::GetH264Dispatcher()->ReconstructChromaIntraHalfs4x4MB_NV12(ppSrcDstCoeff,
                                                      pSrcDstUVPlane, srcdstUVStep,
                                                      intra_chroma_mode,
                                                      cbpU, cbpV,
                                                      chromaQPU, chromaQPV,
                                                      edge_type_top, edge_type_bottom,
                                                      pQuantTableU, pQuantTableV,
                                                      levelScaleDCU, levelScaleDCV,
                                                      bypass_flag, bit_depth, chroma_format_idc);
    }


    template <typename Plane>
    void FilterDeblockingChromaEdge(Plane* pSrcDst,
                                                Ipp32s  srcdstStep,
                                                Ipp8u*  pAlpha,
                                                Ipp8u*  pBeta,
                                                Ipp8u*  pThresholds,
                                                Ipp8u*  pBS,
                                                Ipp32s  bit_depth,
                                                Ipp32u  chroma_format_idc,
                                                Ipp32u dir)
    {
        MFX_H264_PP::GetH264Dispatcher()->FilterDeblockingChromaEdge(pSrcDst, srcdstStep,
                                                pAlpha, pBeta,
                                                pThresholds, pBS,
                                                bit_depth, chroma_format_idc, dir);
    }

#pragma warning(default: 4100)

} // namespace UMC

#if defined(LINUX) && defined(__GNUC__)
# pragma GCC diagnistic pop
#endif

#endif // __UMC_H264_DEC_IPP_WRAP_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
