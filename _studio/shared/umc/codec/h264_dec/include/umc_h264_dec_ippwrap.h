// Copyright (c) 2003-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#ifndef __UMC_H264_DEC_IPP_WRAP_H
#define __UMC_H264_DEC_IPP_WRAP_H

#include "ippi.h"
#include "umc_h264_dec_ipplevel.h"
#include "vm_debug.h"
#include "umc_h264_dec_defs_dec.h"
#include "mfx_h264_dispatcher.h"
#include "umc_h264_dec_structures.h"

// Aproximately 800 warnings "-Wunused-parameter"
#if defined(__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  #pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wunused-parameter"
#endif

typedef struct _IppiBidir1_8u
{
    const uint8_t * pSrc[2];
    int32_t   srcStep[2];
    uint8_t*   pDst;
    int32_t   dstStep;
    mfxSize roiSize;
} IppVCBidir1_8u;

typedef struct _IppiBidir1_16u
{
    const uint16_t * pSrc[2];
    int32_t   srcStep[2];
    uint16_t*   pDst;
    int32_t   dstStep;
    mfxSize roiSize;
    int32_t   bitDepth;
} IppVCBidir1_16u;

namespace UMC
{
// turn off the "unreferenced formal parameter" warning
#ifdef _MSVC_LANG
#pragma warning(disable: 4100)
#endif

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
    inline IppStatus SetPlane(Plane value, Plane* pDst, int32_t dstStep,
                              mfxSize roiSize )
    {
        if (!pDst)
            return ippStsNullPtrErr;

        return sizeof(Plane) == 1 ? ippiSet_8u_C1R((uint8_t)value, (uint8_t*)pDst, dstStep, roiSize) : ippiSet_16s_C1R((uint16_t)value, (int16_t*)pDst, dstStep, roiSize);
    }

    template <typename Plane>
    inline IppStatus CopyPlane(const Plane* pSrc, int32_t srcStep, Plane* pDst, int32_t dstStep,
                              mfxSize roiSize )
    {
        if (!pSrc || !pDst)
            return ippStsNullPtrErr;

        return sizeof(Plane) == 1 ? ippiCopy_8u_C1R((const uint8_t*)pSrc, srcStep, (uint8_t*)pDst, dstStep, roiSize) : ippiCopy_16s_C1R((const int16_t*)pSrc, srcStep, (int16_t*)pDst, dstStep,roiSize);
    }

    inline IppStatus BiDirWeightBlock(uint8_t * , IppVCBidir1_16u * info_,
                                      uint32_t ulog2wd,
                                      int32_t iWeight1,
                                      int32_t iOffset1,
                                      int32_t iWeight2,
                                      int32_t iOffset2)
    {
        IppVCBidir1_8u * info = (IppVCBidir1_8u *)info_;
        return ippiBiDirWeightBlock_H264_8u_P3P1R(info->pSrc[0], info->pSrc[1], info->pDst,
            info->srcStep[0], info->srcStep[1], info->dstStep,
            ulog2wd, iWeight1, iOffset1, iWeight2, iOffset2, info->roiSize);
    }

    inline IppStatus BiDirWeightBlockImplicit(uint8_t * , IppVCBidir1_16u * info_,
                                              int32_t iWeight1,
                                              int32_t iWeight2)
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

    inline IppStatus InterpolateBlock(uint8_t * , IppVCBidir1_16u * info_)
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

    inline IppStatus UniDirWeightBlock(uint8_t *, IppVCBidir1_16u * info_,
                                       uint32_t ulog2wd,
                                       int32_t iWeight,
                                       int32_t iOffset)
    {
        IppVCBidir1_8u * info = (IppVCBidir1_8u *)info_;
        return ippiUniDirWeightBlock_H264_8u_C1R(info->pDst, info->dstStep,
                                                 ulog2wd, iWeight,
                                                 iOffset, info->roiSize);
    }

    inline IppStatus ReconstructLumaIntraHalfMB(int16_t **ppSrcCoeff,
                                                uint8_t *pSrcDstYPlane,
                                                int32_t srcdstYStep,
                                                IppIntra4x4PredMode_H264 *pMBIntraTypes,
                                                uint32_t cbp4x2,
                                                int32_t QP,
                                                uint8_t edgeType,
                                                int32_t bit_depth = 8)
    {
        return ippiReconstructLumaIntraHalfMB_H264_16s8u_C1R(ppSrcCoeff,
                                                             pSrcDstYPlane,
                                                             srcdstYStep,
                                                             pMBIntraTypes,
                                                             cbp4x2,
                                                             QP,
                                                             edgeType);
    }

    inline IppStatus ReconstructLumaInter8x8MB(int16_t **ppSrcDstCoeff,
                                               uint8_t *pSrcDstYPlane,
                                               uint32_t srcdstYStep,
                                               uint32_t cbp8x8,
                                               int32_t QP,
                                               const int16_t *pQuantTable,
                                               uint8_t bypass_flag,
                                               int32_t bit_depth = 8)
    {
        return ippiReconstructLumaInter8x8MB_H264_16s8u_C1R(ppSrcDstCoeff,
                                                            pSrcDstYPlane,
                                                            srcdstYStep,
                                                            cbp8x8,
                                                            QP,
                                                            pQuantTable,
                                                            bypass_flag);
    }

    inline IppStatus ReconstructLumaInter4x4MB(int16_t **ppSrcDstCoeff,
                                               uint8_t *pSrcDstYPlane,
                                               uint32_t srcdstYStep,
                                               uint32_t cbp4x4,
                                               int32_t QP,
                                               const int16_t *pQuantTable,
                                               uint8_t bypass_flag,
                                               int32_t bit_depth = 8)
    {
        return ippiReconstructLumaInter4x4MB_H264_16s8u_C1R(ppSrcDstCoeff,
                                                            pSrcDstYPlane,
                                                            srcdstYStep,
                                                            cbp4x4,
                                                            QP,
                                                            pQuantTable,
                                                            bypass_flag);
    }


    inline IppStatus ReconstructLumaIntra_16x16MB(int16_t **ppSrcDstCoeff,
                                                  uint8_t *pSrcDstYPlane,
                                                  uint32_t srcdstYStep,
                                                  IppIntra16x16PredMode_H264 intra_luma_mode,
                                                  uint32_t cbp4x4,
                                                  int32_t QP,
                                                  uint8_t edge_type,
                                                  const int16_t *pQuantTable,
                                                  uint8_t bypass_flag,
                                                  int32_t bit_depth = 8)
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

    inline IppStatus ReconstructLumaIntraHalf8x8MB(int16_t **ppSrcDstCoeff,
                                                   uint8_t *pSrcDstYPlane,
                                                   int32_t srcdstYStep,
                                                   IppIntra8x8PredMode_H264 *pMBIntraTypes,
                                                   uint32_t cbp8x2,
                                                   int32_t QP,
                                                   uint8_t edgeType,
                                                   const int16_t *pQuantTable,
                                                   uint8_t bypass_flag,
                                                   int32_t bit_depth = 8)
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


    inline IppStatus ReconstructLumaIntraHalf4x4MB(int16_t **ppSrcDstCoeff,
                                                   uint8_t *pSrcDstYPlane,
                                                   int32_t srcdstYStep,
                                                   IppIntra4x4PredMode_H264 *pMBIntraTypes,
                                                   uint32_t cbp4x2,
                                                   int32_t QP,
                                                   uint8_t edgeType,
                                                   const int16_t *pQuantTable,
                                                   uint8_t bypass_flag,
                                                   int32_t bit_depth = 8)
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

    inline IppStatus ReconstructLumaIntra8x8MB(int16_t **ppSrcDstCoeff,
                                               uint8_t *pSrcDstYPlane,
                                               int32_t srcdstYStep,
                                               IppIntra8x8PredMode_H264 *pMBIntraTypes,
                                               uint32_t cbp8x8,
                                               int32_t QP,
                                               uint8_t edgeType,
                                               const int16_t *pQuantTable,
                                               uint8_t bypass_flag,
                                               int32_t bit_depth = 8)
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

    inline  IppStatus ReconstructLumaIntra4x4MB(int16_t **ppSrcDstCoeff,
                                                uint8_t *pSrcDstYPlane,
                                                int32_t srcdstYStep,
                                                IppIntra4x4PredMode_H264 *pMBIntraTypes,
                                                uint32_t cbp4x4,
                                                int32_t QP,
                                                uint8_t edgeType,
                                                const int16_t *pQuantTable,
                                                uint8_t bypass_flag,
                                                int32_t bit_depth = 8)
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

    inline IppStatus ReconstructLumaIntra16x16MB(int16_t **ppSrcCoeff,
                                                 uint8_t *pSrcDstYPlane,
                                                 uint32_t srcdstYStep,
                                                 const IppIntra16x16PredMode_H264 intra_luma_mode,
                                                 const uint32_t cbp4x4,
                                                 const int32_t QP,
                                                 const uint8_t edge_type,
                                                 int32_t bit_depth = 8)
    {
        return ippiReconstructLumaIntra16x16MB_H264_16s8u_C1R(ppSrcCoeff,
                                                              pSrcDstYPlane,
                                                              srcdstYStep,
                                                              intra_luma_mode,
                                                              cbp4x4,
                                                              QP,
                                                              edge_type);
    }

    inline IppStatus ReconstructLumaIntraMB(int16_t **ppSrcCoeff,
                                            uint8_t *pSrcDstYPlane,
                                            int32_t srcdstYStep,
                                            const IppIntra4x4PredMode_H264 *pMBIntraTypes,
                                            const uint32_t cbp4x4,
                                            const int32_t QP,
                                            const uint8_t edgeType,
                                            int32_t bit_depth = 8)
    {
        return ippiReconstructLumaIntraMB_H264_16s8u_C1R(ppSrcCoeff,
                                                         pSrcDstYPlane,
                                                         srcdstYStep,
                                                         pMBIntraTypes,
                                                         cbp4x4,
                                                         QP,
                                                         edgeType);
    }

    inline IppStatus ReconstructLumaInterMB(int16_t **ppSrcCoeff,
                                            uint8_t *pSrcDstYPlane,
                                            uint32_t srcdstYStep,
                                            uint32_t cbp4x4,
                                            int32_t QP,
                                            int32_t bit_depth = 8)
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
    info_temp[0].cbp = (uint32_t)cbpU;          \
    info_temp[0].qp = chromaQPU;               \
    info_temp[0].pQuantTable = (int16_t *) pQuantTableU;  \
    info_temp[0].bypassFlag = bypass_flag;      \
    info_temp[1].ppSrcDstCoeff = ppSrcDstCoeff;   \
    info_temp[1].pSrcDstPlane = pSrcDstVPlane;  \
    info_temp[1].srcDstStep = srcdstUVStep;    \
    info_temp[1].cbp = (uint32_t)cbpV;          \
    info_temp[1].qp = chromaQPV;\
    info_temp[1].pQuantTable = (int16_t *) pQuantTableV;\
    info_temp[1].bypassFlag = bypass_flag;

    ///****************************************************************************************/
    // 16 bits functions
    ///****************************************************************************************/

    inline IppStatus UniDirWeightBlock(uint16_t *, IppVCBidir1_16u * info,
                                       uint32_t ulog2wd,
                                       int32_t iWeight,
                                       int32_t iOffset)
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
    info.pQuantTable = (int16_t *) pQuantTable;\
    info.bypassFlag = bypass_flag;\
    info.bitDepth = bit_depth;

    inline IppStatus ReconstructLumaIntraHalfMB(int32_t **ppSrcCoeff,
                                                uint16_t *pSrcDstYPlane,
                                                int32_t srcdstYStep,
                                                IppIntra4x4PredMode_H264 *pMBIntraTypes,
                                                uint32_t cbp4x2,
                                                int32_t QP,
                                                uint8_t  edgeType,
                                                int32_t bit_depth = 10)
    {
        VM_ASSERT(false);
        return ippStsNoErr;
    }

    inline IppStatus ReconstructLumaInter8x8MB(int32_t **ppSrcDstCoeff,
                                               uint16_t *pSrcDstYPlane,
                                               uint32_t srcDstStep,
                                               uint32_t cbp,
                                               int32_t QP,
                                               const int16_t *pQuantTable,
                                               uint8_t  bypass_flag,
                                               int32_t bit_depth)
    {
        FILL_RECONSTRUCT_INFO;
        return ippiReconstructLumaInter8x8_H264High_32s16u_IP1R(&info);
    }

    inline IppStatus ReconstructLumaInter4x4MB(int32_t **ppSrcDstCoeff,
                                               uint16_t *pSrcDstYPlane,
                                               uint32_t srcDstStep,
                                               uint32_t cbp,
                                               int32_t QP,
                                               const int16_t *pQuantTable,
                                               uint8_t  bypass_flag,
                                               int32_t bit_depth)
    {
        FILL_RECONSTRUCT_INFO;
        return ippiReconstructLumaInter4x4_H264High_32s16u_IP1R(&info);
    }

    inline IppStatus ReconstructLumaIntra_16x16MB(int32_t **ppSrcDstCoeff,
                                                  uint16_t *pSrcDstYPlane,
                                                  uint32_t srcDstStep,
                                                  IppIntra16x16PredMode_H264 intra_luma_mode,
                                                  uint32_t cbp,
                                                  int32_t QP,
                                                  uint8_t  edge_type,
                                                  const int16_t *pQuantTable,
                                                  uint8_t  bypass_flag,
                                                  int32_t bit_depth)
    {
        FILL_RECONSTRUCT_INFO;
        return ippiReconstructLumaIntra16x16_H264High_32s16u_IP1R(&info,
                                                                  intra_luma_mode,
                                                                  edge_type);
    }

    inline IppStatus ReconstructLumaIntraHalf8x8MB(int32_t **ppSrcDstCoeff,
                                                   uint16_t *pSrcDstYPlane,
                                                   int32_t srcDstStep,
                                                   IppIntra8x8PredMode_H264 *pMBIntraTypes,
                                                   uint32_t cbp,
                                                   int32_t QP,
                                                   uint8_t  edgeType,
                                                   const int16_t *pQuantTable,
                                                   uint8_t  bypass_flag,
                                                   int32_t bit_depth = 10)
    {
        FILL_RECONSTRUCT_INFO;
        return ippiReconstructLumaIntraHalf8x8_H264High_32s16u_IP1R(&info,
                                                                    pMBIntraTypes,
                                                                    edgeType);
    }

    inline IppStatus ReconstructLumaIntraHalf4x4MB(int32_t **ppSrcDstCoeff,
                                                   uint16_t *pSrcDstYPlane,
                                                   int32_t srcDstStep,
                                                   IppIntra4x4PredMode_H264 *pMBIntraTypes,
                                                   uint32_t cbp,
                                                   int32_t QP,
                                                   uint8_t  edgeType,
                                                   const int16_t *pQuantTable,
                                                   uint8_t  bypass_flag,
                                                   int32_t bit_depth = 10)
    {
        FILL_RECONSTRUCT_INFO;
        return ippiReconstructLumaIntraHalf4x4_H264High_32s16u_IP1R(&info,
                                                                    pMBIntraTypes,
                                                                    edgeType);
    }

    inline IppStatus ReconstructLumaIntra8x8MB(int32_t **ppSrcDstCoeff,
                                               uint16_t *pSrcDstYPlane,
                                               int32_t srcDstStep,
                                               IppIntra8x8PredMode_H264 *pMBIntraTypes,
                                               uint32_t cbp,
                                               int32_t QP,
                                               uint8_t  edgeType,
                                               const int16_t *pQuantTable,
                                               uint8_t  bypass_flag,
                                               int32_t bit_depth)
    {
        FILL_RECONSTRUCT_INFO;
        return ippiReconstructLumaIntra8x8_H264High_32s16u_IP1R(&info,
                                                                pMBIntraTypes,
                                                                edgeType);
    }

    inline  IppStatus ReconstructLumaIntra4x4MB(int32_t **ppSrcDstCoeff,
                                                uint16_t *pSrcDstYPlane,
                                                int32_t srcDstStep,
                                                IppIntra4x4PredMode_H264 *pMBIntraTypes,
                                                uint32_t cbp,
                                                int32_t QP,
                                                uint8_t  edgeType,
                                                const int16_t *pQuantTable,
                                                uint8_t  bypass_flag,
                                                int32_t bit_depth)
    {
        FILL_RECONSTRUCT_INFO;
        return ippiReconstructLumaIntra4x4_H264High_32s16u_IP1R(&info,
                                                                pMBIntraTypes,
                                                                edgeType);
    }

    inline IppStatus ReconstructLumaIntra16x16MB(int32_t **ppSrcCoeff,
                                                 uint16_t *pSrcDstYPlane,
                                                 uint32_t srcdstYStep,
                                                 const IppIntra16x16PredMode_H264 intra_luma_mode,
                                                 const uint32_t cbp4x4,
                                                 const int32_t QP,
                                                 const uint8_t edge_type,
                                                 int32_t bit_depth = 10)
    {
        VM_ASSERT(false);
        return ippStsNoErr;
    }

    inline IppStatus ReconstructLumaIntraMB(int32_t **ppSrcCoeff,
                                            uint16_t *pSrcDstYPlane,
                                            int32_t srcdstYStep,
                                            const IppIntra4x4PredMode_H264 *pMBIntraTypes,
                                            const uint32_t cbp4x4,
                                            const int32_t QP,
                                            const uint8_t edgeType,
                                            int32_t bit_depth = 10)
    {
        VM_ASSERT(false);
        return ippStsNoErr;
    }

    inline IppStatus ReconstructLumaInterMB(int32_t **ppSrcCoeff,
                                            uint16_t *pSrcDstYPlane,
                                            uint32_t srcdstYStep,
                                            uint32_t cbp4x4,
                                            int32_t QP,
                                            int32_t bit_depth = 10)
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
    info_temp[0].cbp = (uint32_t)cbpU;          \
    info_temp[0].qp = chromaQPU;               \
    info_temp[0].pQuantTable = (int16_t *) pQuantTableU;  \
    info_temp[0].bypassFlag = bypass_flag;      \
    info_temp[0].bitDepth = bit_depth;            \
    info_temp[1].ppSrcDstCoeff = ppSrcDstCoeff;   \
    info_temp[1].pSrcDstPlane = pSrcDstVPlane;  \
    info_temp[1].srcDstStep = srcdstUVStep;    \
    info_temp[1].cbp = (uint32_t)cbpV;          \
    info_temp[1].qp = chromaQPV;\
    info_temp[1].pQuantTable = (int16_t *) pQuantTableV;\
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

    inline IppStatus BiDirWeightBlock(uint16_t * , IppVCBidir1_16u * info1,
                                      uint32_t ulog2wd,
                                      int32_t iWeight1,
                                      int32_t iOffset1,
                                      int32_t iWeight2,
                                      int32_t iOffset2)
    {
        FILL_VC_BIDIR_INFO;
        return ippiBidirWeight_H264_16u_P2P1R(&info, ulog2wd, iWeight1, iOffset1, iWeight2, iOffset2);
    }

    inline IppStatus BiDirWeightBlockImplicit(uint16_t * , IppVCBidir1_16u * info1,
                                              int32_t iWeight1,
                                              int32_t iWeight2)
    {
        FILL_VC_BIDIR_INFO;
        return ippiBidirWeightImplicit_H264_16u_P2P1R(&info,
                                                      iWeight1,
                                                      iWeight2);
    }

    inline IppStatus InterpolateBlock(uint16_t * , IppVCBidir1_16u * info1)
    {
        FILL_VC_BIDIR_INFO
        return ippiBidir_H264_16u_P2P1R(&info);
    }


// nv12 functions
    inline IppStatus ReconstructChromaIntraMB_NV12(int16_t **ppSrcCoeff,
                                              uint8_t *pSrcDstUVPlane,
                                              const uint32_t srcdstUVStep,
                                              const IppIntraChromaPredMode_H264 intra_chroma_mode,
                                              uint32_t cbpU,
                                              uint32_t cbpV,
                                              const uint32_t ChromaQP,
                                              const uint8_t edge_type,
                                              int32_t bit_depth = 8)
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

    inline IppStatus ReconstructChromaIntraMB_NV12(int32_t **ppSrcCoeff,
                                              uint16_t *pSrcDstUPlane,
                                              const uint32_t srcdstUVStep,
                                              const IppIntraChromaPredMode_H264 intra_chroma_mode,
                                              uint32_t cbpU,
                                              uint32_t cbpV,
                                              const uint32_t ChromaQP,
                                              const uint8_t edge_type,
                                              int32_t bit_depth = 10)
    {
        VM_ASSERT(false);
        return ippStsNoErr;
    }

    inline IppStatus ReconstructChromaInterMB_NV12(int16_t **ppSrcCoeff,
                                              uint8_t *pSrcDstUVPlane,
                                              const uint32_t srcdstStep,
                                              uint32_t cbpU,
                                              uint32_t cbpV,
                                              const uint32_t ChromaQP,
                                              int32_t bit_depth = 8)
    {
        IppStatus sts = ippiReconstructChromaInterMB_H264_16s8u_C2R(ppSrcCoeff,
                                                           pSrcDstUVPlane,
                                                           srcdstStep,
                                                           CreateIPPCBPMask420(cbpU, cbpV),
                                                           ChromaQP);
        return sts;
    }

    inline IppStatus ReconstructChromaInterMB_NV12(int32_t **ppSrcCoeff,
                                              uint16_t *pSrcDstUVPlane,
                                              const uint32_t srcdstStep,
                                              uint32_t cbpU,
                                              uint32_t cbpV,
                                              const uint32_t ChromaQP,
                                              int32_t bit_depth = 10)
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

    inline IppStatus InterpolateBlock_NV12(uint8_t *pSrc1,
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

    inline IppStatus InterpolateBlock_NV12(uint16_t *pSrc1,
                                      IppVCBidir1_16u * info1)
    {
        FILL_VC_BIDIR_INFO
        info.roiSize.width *= 2;
        return ippiBidir_H264_16u_P2P1R(&info);
    }

    inline void UniDirWeightBlock_NV12(uint8_t *,
                                       IppVCBidir1_16u * info_,
                                       uint32_t ulog2wd,
                                       int32_t iWeightU,
                                       int32_t iOffsetU,
                                       int32_t iWeightV,
                                       int32_t iOffsetV)
    {
        IppVCBidir1_8u * info = (IppVCBidir1_8u *)info_;
        MFX_H264_PP::GetH264Dispatcher()->UniDirWeightBlock_NV12(info->pDst, info->dstStep, ulog2wd, iWeightU, iOffsetU, iWeightV, iOffsetV, info->roiSize, info_->bitDepth);
    }

    inline void UniDirWeightBlock_NV12(uint16_t *pSrcDst,
                                       IppVCBidir1_16u * info,
                                       uint32_t ulog2wd,
                                       int32_t iWeightU,
                                       int32_t iOffsetU,
                                       int32_t iWeightV,
                                       int32_t iOffsetV)
    {
        MFX_H264_PP::GetH264Dispatcher()->UniDirWeightBlock_NV12(info->pDst, info->dstStep, ulog2wd, iWeightU, iOffsetU, iWeightV, iOffsetV, info->roiSize, info->bitDepth);
    }

    inline void BiDirWeightBlock_NV12(const uint8_t *pSrc1,
                                      IppVCBidir1_16u * info_,
                                      uint32_t ulog2wd,
                                      int32_t iWeightU1,
                                      int32_t iOffsetU1,
                                      int32_t iWeightU2,
                                      int32_t iOffsetU2,
                                      int32_t iWeightV1,
                                      int32_t iOffsetV1,
                                      int32_t iWeightV2,
                                      int32_t iOffsetV2)
    {
        IppVCBidir1_8u * info = (IppVCBidir1_8u *)info_;
        MFX_H264_PP::GetH264Dispatcher()->BiDirWeightBlock_NV12(info->pSrc[0], info->srcStep[0], info->pSrc[1], info->srcStep[1], info->pDst, info->dstStep,
            ulog2wd, iWeightU1, iOffsetU1, iWeightU2, iOffsetU2, iWeightV1, iOffsetV1, iWeightV2, iOffsetV2, info->roiSize, info_->bitDepth);
    }

    inline void BiDirWeightBlock_NV12(uint16_t *pSrc1,
                                      IppVCBidir1_16u * info,
                                      uint32_t ulog2wd,
                                      int32_t iWeightU1,
                                      int32_t iOffsetU1,
                                      int32_t iWeightU2,
                                      int32_t iOffsetU2,
                                      int32_t iWeightV1,
                                      int32_t iOffsetV1,
                                      int32_t iWeightV2,
                                      int32_t iOffsetV2)
    {
        MFX_H264_PP::GetH264Dispatcher()->BiDirWeightBlock_NV12(info->pSrc[0], info->srcStep[0], info->pSrc[1], info->srcStep[1], info->pDst, info->dstStep,
            ulog2wd, iWeightU1, iOffsetU1, iWeightU2, iOffsetU2, iWeightV1, iOffsetV1, iWeightV2, iOffsetV2, info->roiSize, info->bitDepth);
    }

    inline IppStatus ReconstructChromaIntraHalfsMB_NV12(int16_t **ppSrcCoeff,
                                                   uint8_t *pSrcDstUVPlane,
                                                   uint32_t srcdstUVStep,
                                                   IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                   uint32_t cbpU,
                                                   uint32_t cbpV,
                                                   uint32_t ChromaQP,
                                                   uint8_t edge_type_top,
                                                   uint8_t edge_type_bottom,
                                                   int32_t bit_depth = 8)
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

    inline IppStatus ReconstructChromaIntraHalfsMB_NV12(int32_t **ppSrcCoeff,
                                                   uint16_t *pSrcDstUVPlane,
                                                   uint32_t srcdstUVStep,
                                                   IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                   uint32_t cbpU,
                                                   uint32_t cbpV,
                                                   uint32_t ChromaQP,
                                                   uint8_t  edge_type_top,
                                                   uint8_t  edge_type_bottom,
                                                   int32_t bit_depth = 10)
    {
        VM_ASSERT(false);
        return ippStsNoErr;
    }


    inline void ReconstructChromaIntraHalfs4x4MB_NV12(int16_t **ppSrcDstCoeff,
                                                      uint8_t *pSrcDstUVPlane,
                                                      uint32_t srcdstUVStep,
                                                      IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                      uint32_t cbpU, uint32_t cbpV,
                                                      uint32_t chromaQPU, uint32_t chromaQPV,
                                                      uint8_t  edge_type_top, uint8_t  edge_type_bottom,
                                                      const int16_t *pQuantTableU, const int16_t *pQuantTableV,
                                                      int16_t levelScaleDCU, int16_t levelScaleDCV,
                                                      uint8_t  bypass_flag,
                                                      int32_t bit_depth,
                                                      uint32_t  chroma_format_idc)
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

    inline void ReconstructChromaIntraHalfs4x4MB_NV12(int32_t **ppSrcDstCoeff,
                                                      uint16_t *pSrcDstUVPlane,
                                                      uint32_t srcdstUVStep,
                                                      IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                      uint32_t cbpU, uint32_t cbpV,
                                                      uint32_t chromaQPU, uint32_t chromaQPV,
                                                      uint8_t  edge_type_top, uint8_t  edge_type_bottom,
                                                      const int16_t *pQuantTableU, const int16_t *pQuantTableV,
                                                      int16_t levelScaleDCU, int16_t levelScaleDCV,
                                                      uint8_t  bypass_flag,
                                                      int32_t bit_depth,
                                                      uint32_t  chroma_format_idc)
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
                                                int32_t  srcdstStep,
                                                uint8_t*  pAlpha,
                                                uint8_t*  pBeta,
                                                uint8_t*  pThresholds,
                                                uint8_t*  pBS,
                                                int32_t  bit_depth,
                                                uint32_t  chroma_format_idc,
                                                uint32_t dir)
    {
        MFX_H264_PP::GetH264Dispatcher()->FilterDeblockingChromaEdge(pSrcDst, srcdstStep,
                                                pAlpha, pBeta,
                                                pThresholds, pBS,
                                                bit_depth, chroma_format_idc, dir);
    }

// restore the "unreferenced formal parameter" warning
#ifdef _MSVC_LANG
#pragma warning(default: 4100)
#endif

} // namespace UMC

#if defined(__GNUC__)
  #pragma GCC diagnostic pop // "-Wunused-parameter", "-Wdeprecated-declarations"
#elif defined(__clang__)
  #pragma clang diagnostic pop // "-Wunused-parameter"
#endif

#endif // __UMC_H264_DEC_IPP_WRAP_H
#endif // MFX_ENABLE_H264_VIDEO_DECODE
