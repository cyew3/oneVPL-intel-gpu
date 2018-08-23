//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_RECONSTRUCT_TEMPLATES_H
#define __UMC_H264_RECONSTRUCT_TEMPLATES_H

#include "umc_h264_dec_ippwrap.h"

namespace UMC
{

#define NO_PADDING

// turn off the "conditional expression is constant" warning
#ifdef _MSVC_LANG
#pragma warning(disable: 4127)
#endif

enum
{
    BI_DIR                      = 0,
    UNI_DIR                     = 1
};

template <typename PlaneY,
          typename PlaneUV,
          int32_t color_format,
          int32_t is_field,
          int32_t is_weight>
class ReconstructMB
{
public:

    enum
    {
        width_chroma_div = 0,  // for plane
        height_chroma_div = (color_format < 2),  // for plane

        point_width_chroma_div = (color_format < 3),  // for plane
        point_height_chroma_div = (color_format < 2),  // for plane

        roi_width_chroma_div = (color_format < 3),  // for roi
        roi_height_chroma_div = (color_format < 2),  // for roi
    };

    typedef PlaneY * PlanePtrY;
    typedef PlaneUV * PlanePtrUV;

    class ReconstructParams
    {
    public:
        // prediction pointers
        H264DecoderMotionVector *(m_pMV[2]);                    // (H264DecoderMotionVector * []) array of pointers to motion vectors
        int32_t m_iRefIndex[2];                                  // (int32_t) current reference index
        int8_t *(m_pRefIndex[2]);                                // (int8_t * []) pointer to array of reference indexes

        IppVCInterpolateBlock_16u m_lumaInterpolateInfo;
        IppVCInterpolateBlock_16u m_chromaInterpolateInfo;

        IppVCBidir1_16u   m_bidirLuma;
        IppVCBidir1_16u   m_bidirChroma[2];

        // current macroblock parameters
        int32_t m_iOffsetLuma;                                   // (int32_t) luminance offset
        int32_t m_iOffsetChroma;                                 // (int32_t) chrominance offset
        int32_t m_iIntraMBLumaOffset;
        int32_t m_iIntraMBChromaOffset;
        int32_t m_iIntraMBLumaOffsetTmp;
        int32_t m_iIntraMBChromaOffsetTmp;

        H264SegmentDecoder *m_pSegDec;                          // (H264SegmentDecoder *) pointer to segment decoder

        // weighting parameters
        int32_t luma_log2_weight_denom;                          // (int32_t)
        int32_t chroma_log2_weight_denom;                        // (int32_t)
        int32_t weighted_bipred_idc;                             // (int32_t)
        bool m_bBidirWeightMB;                                  // (bool) flag of bidirectional weighting
        bool m_bUnidirWeightMB;                                 // (bool) flag of unidirectional weighting

        // current frame parameters

        bool is_mbaff;
        bool is_bottom_mb;

        PlanePtrY m_pDstY;                                      // (PlanePtrY) pointer to an Y plane
        PlanePtrUV m_pDstUV;                                    // (PlanePtrUV) pointer to an U plane
    };

    inline
    int32_t ChromaOffset(int32_t x, int32_t y, int32_t pitch)
    {
        return (x >> width_chroma_div) + (y >> height_chroma_div) * pitch;

    } // int32_t ChromaOffset(int32_t x, int32_t y, int32_t pitch)

    // we can't use template instead implicit parameter's passing
    // couse we use slightly different types
    void CompensateMotionLumaBlock(ReconstructParams *pParams,
                                   int32_t iDir,
                                   int32_t iBlockNumber,
                                   int32_t iUniDir)
    {
        IppVCInterpolateBlock_16u * interpolateInfo = &pParams->m_lumaInterpolateInfo;
        int32_t iRefIndex;
        int32_t iRefFieldTop;

        // get reference index
        iRefIndex = (pParams->m_iRefIndex[iDir] = GetReferenceIndex(pParams->m_pRefIndex[iDir], iBlockNumber));
        if (is_field && pParams->is_mbaff)
            iRefIndex >>= 1;

        if (iRefIndex < 0 || !pParams->m_pSegDec->m_pRefPicList[iDir][iRefIndex])
        {
            pParams->m_bidirLuma.pSrc[iDir] = interpolateInfo->pDst[0];
            pParams->m_bidirLuma.srcStep[iDir] = interpolateInfo->dstStep;

            if (sizeof(PlaneY) == 1)
            {
                ippiSet_8u_C1R(0, (uint8_t*)interpolateInfo->pDst[0], interpolateInfo->dstStep, interpolateInfo->sizeBlock);
            }
            else
            {
                ippiSet_16u_C1R(0, interpolateInfo->pDst[0], interpolateInfo->dstStep, interpolateInfo->sizeBlock);
            }
            return;
        }

        // get reference frame & pitch
        interpolateInfo->pSrc[0] = (uint16_t*) pParams->m_pSegDec->m_pRefPicList[iDir][iRefIndex]->m_pYPlane;

        VM_ASSERT(interpolateInfo->pSrc[0]);

        if (is_field)
        {
            if (pParams->is_mbaff)
            {
                iRefFieldTop = pParams->is_bottom_mb ^ (pParams->m_iRefIndex[iDir] & 1);
                pParams->m_iRefIndex[iDir] = iRefIndex;
            }
            else
                iRefFieldTop = GetReferenceField(pParams->m_pSegDec->m_pFields[iDir], iRefIndex);

            if (iRefFieldTop)
                interpolateInfo->pSrc[0] = (uint16_t*)((PlanePtrY)(interpolateInfo->pSrc[0]) + (interpolateInfo->srcStep >> 1 ));
        }

        // create vectors & factors
        interpolateInfo->pointVector.x = pParams->m_pMV[iDir][iBlockNumber].mvx;
        interpolateInfo->pointVector.y = pParams->m_pMV[iDir][iBlockNumber].mvy;

        // we should do interpolation if vertical or horizontal vector isn't zero
        if (interpolateInfo->pointVector.x | interpolateInfo->pointVector.y)
        {
            // fill parameters
            ippiInterpolateLumaBlock<PlaneY>(interpolateInfo);

            // save pointers for optimized interpolation
            pParams->m_bidirLuma.pSrc[iDir] = interpolateInfo->pDst[0];
            pParams->m_bidirLuma.srcStep[iDir] = interpolateInfo->dstStep;
        }
        else
        {
            int32_t iOffset = pParams->m_iOffsetLuma +
                             pParams->m_iIntraMBLumaOffset;

            // save pointers for optimized interpolation
            if (0 == iUniDir)
            {
                pParams->m_bidirLuma.pSrc[iDir] = (uint16_t*)((PlanePtrY) (interpolateInfo->pSrc[0]) + iOffset);
                pParams->m_bidirLuma.srcStep[iDir] = interpolateInfo->srcStep;
            }
            // we have to do interpolation for uni-direction motion
            else
            {
                interpolateInfo->pSrc[0] = (uint16_t*) ((PlanePtrY)(interpolateInfo->pSrc[0]) + iOffset);
                ippiInterpolateLuma<PlaneY>(interpolateInfo);

                // save pointers for optimized interpolation
                pParams->m_bidirLuma.pSrc[iDir] = interpolateInfo->pDst[0];
                pParams->m_bidirLuma.srcStep[iDir] = interpolateInfo->srcStep;
            }
        }
    } // void CompensateMotionLumaBlock(ReconstructParams *pParams,

    void CompensateMotionChromaBlock(ReconstructParams *pParams,
                                     int32_t iDir,
                                     int32_t iBlockNumber,
                                     int32_t iUniDir)
    {
        IppVCInterpolateBlock_16u * interpolateInfo = &pParams->m_chromaInterpolateInfo;
        int32_t iRefIndex;
        int32_t iRefFieldTop;

        // get reference index
        iRefIndex = (pParams->m_iRefIndex[iDir] = GetReferenceIndex(pParams->m_pRefIndex[iDir], iBlockNumber));
        if (is_field && pParams->is_mbaff)
            iRefIndex >>= 1;

        if (iRefIndex < 0 || !pParams->m_pSegDec->m_pRefPicList[iDir][iRefIndex])
        {
            // save pointers for optimized interpolation
            pParams->m_bidirChroma[0].pSrc[iDir] = interpolateInfo->pDst[0];

            pParams->m_bidirChroma[0].srcStep[iDir] = interpolateInfo->dstStep;

            if (sizeof(PlaneUV) == 1)
            {
                ippiSet_16u_C1R(0, (uint16_t*)interpolateInfo->pDst[0], interpolateInfo->dstStep, interpolateInfo->sizeBlock);
            }
            else
            {
                ippiSet_32s_C1R(0, (int32_t*)interpolateInfo->pDst[0], interpolateInfo->dstStep, interpolateInfo->sizeBlock);
            }
            return;
        }

        // get reference frame & pitch
        interpolateInfo->pSrc[0] = (uint16_t*)pParams->m_pSegDec->m_pRefPicList[iDir][iRefIndex]->m_pUVPlane;
        VM_ASSERT(interpolateInfo->pSrc[0]);

        if (is_field)
        {
            if (pParams->is_mbaff)
            {
                iRefFieldTop = pParams->is_bottom_mb ^ (pParams->m_iRefIndex[iDir] & 1);
                pParams->m_iRefIndex[iDir] = iRefIndex;
            }
            else
                iRefFieldTop = GetReferenceField(pParams->m_pSegDec->m_pFields[iDir], iRefIndex);

            if (iRefFieldTop)
            {
                interpolateInfo->pSrc[0] = (uint16_t*)((PlanePtrUV)(interpolateInfo->pSrc[0]) + (interpolateInfo->srcStep >> 1));
            }
        }

        // save vector
        interpolateInfo->pointVector.x = pParams->m_pMV[iDir][iBlockNumber].mvx;
        interpolateInfo->pointVector.y = pParams->m_pMV[iDir][iBlockNumber].mvy;

        // adjust vectors when we decode a field
        if ((is_field) && (1 == color_format))
        {
            if (pParams->is_mbaff)
            {
                interpolateInfo->pointVector.y += (pParams->is_bottom_mb - iRefFieldTop) * 2;
            }
            else
            {
                if (!pParams->m_pSegDec->m_field_index && iRefFieldTop)
                    interpolateInfo->pointVector.y -= 2;
                else if(pParams->m_pSegDec->m_field_index && !iRefFieldTop)
                    interpolateInfo->pointVector.y += 2;
            }
        }

        // we should do interpolation if vertical or horizontal vector isn't zero
        if ((interpolateInfo->pointVector.x | interpolateInfo->pointVector.y) ||
            (iUniDir))
        {
            // scale motion vector
            interpolateInfo->pointVector.x <<= ((int32_t) (3 <= color_format));
            interpolateInfo->pointVector.y <<= ((int32_t) (2 <= color_format));

            ippiInterpolateChromaBlockNV12<PlaneUV>(interpolateInfo);

            // save pointers for optimized interpolation
            pParams->m_bidirChroma[0].pSrc[iDir] = interpolateInfo->pDst[0];
            pParams->m_bidirChroma[0].srcStep[iDir] = interpolateInfo->dstStep;
        }
        else
        {
            int32_t iOffset = pParams->m_iOffsetChroma +
                             pParams->m_iIntraMBChromaOffset;

            // save pointers for optimized interpolation
            pParams->m_bidirChroma[0].pSrc[iDir] = (uint16_t*)((PlanePtrUV)(interpolateInfo->pSrc[0]) + iOffset);
            pParams->m_bidirChroma[0].srcStep[iDir] = interpolateInfo->srcStep;
        }

    } // void CompensateMotionChromaBlock(ReconstructParams *pParams,

    inline
    void InterpolateMacroblock(ReconstructParams *pParams)
    {
        // combine bidir predictions into one,
        // no weighting

        InterpolateBlock(pParams->m_pDstY, &pParams->m_bidirLuma);

        if (color_format)
        {
            InterpolateBlock_NV12(pParams->m_pDstUV, &pParams->m_bidirChroma[0]);
        }

    } // void InterpolateMacroblock(ReconstructParams *pParams)

    inline
    void BiDirWeightMacroblock(ReconstructParams *pParams)
    {
        // combine bidir predictions into one,
        // explicit weighting
        BiDirWeightBlock(pParams->m_pDstY, &pParams->m_bidirLuma,
                         pParams->luma_log2_weight_denom,
                         pParams->m_pSegDec->m_pPredWeight[D_DIR_FWD][pParams->m_iRefIndex[D_DIR_FWD]].luma_weight,
                         pParams->m_pSegDec->m_pPredWeight[D_DIR_FWD][pParams->m_iRefIndex[D_DIR_FWD]].luma_offset,
                         pParams->m_pSegDec->m_pPredWeight[D_DIR_BWD][pParams->m_iRefIndex[D_DIR_BWD]].luma_weight,
                         pParams->m_pSegDec->m_pPredWeight[D_DIR_BWD][pParams->m_iRefIndex[D_DIR_BWD]].luma_offset);

        if (color_format)
        {
            BiDirWeightBlock_NV12(pParams->m_pDstUV,
                            &pParams->m_bidirChroma[0],
                                pParams->chroma_log2_weight_denom,
                                pParams->m_pSegDec->m_pPredWeight[D_DIR_FWD][pParams->m_iRefIndex[D_DIR_FWD]].chroma_weight[0],
                                pParams->m_pSegDec->m_pPredWeight[D_DIR_FWD][pParams->m_iRefIndex[D_DIR_FWD]].chroma_offset[0],
                                pParams->m_pSegDec->m_pPredWeight[D_DIR_BWD][pParams->m_iRefIndex[D_DIR_BWD]].chroma_weight[0],
                                pParams->m_pSegDec->m_pPredWeight[D_DIR_BWD][pParams->m_iRefIndex[D_DIR_BWD]].chroma_offset[0],
                                pParams->m_pSegDec->m_pPredWeight[D_DIR_FWD][pParams->m_iRefIndex[D_DIR_FWD]].chroma_weight[1],
                                pParams->m_pSegDec->m_pPredWeight[D_DIR_FWD][pParams->m_iRefIndex[D_DIR_FWD]].chroma_offset[1],
                                pParams->m_pSegDec->m_pPredWeight[D_DIR_BWD][pParams->m_iRefIndex[D_DIR_BWD]].chroma_weight[1],
                                pParams->m_pSegDec->m_pPredWeight[D_DIR_BWD][pParams->m_iRefIndex[D_DIR_BWD]].chroma_offset[1]);
        }
    } // void BiDirWeightMacroblock(ReconstructParams *pParams)

    inline
    void BiDirWeightMacroblockImplicit(ReconstructParams *pParams, int32_t iBlockNumber)
    {
        FactorArrayValue iDistScaleFactor;

        if (is_field && pParams->is_mbaff)
        {
            int32_t curfield = pParams->is_bottom_mb;
            int32_t ref0field = curfield ^ (GetReferenceIndex(pParams->m_pRefIndex[D_DIR_FWD], iBlockNumber) & 1);
            int32_t ref1field = curfield ^ (GetReferenceIndex(pParams->m_pRefIndex[D_DIR_BWD], iBlockNumber) & 1);

            const FactorArrayValue *pDistScaleFactors = pParams->m_pSegDec->m_pSlice->GetDistScaleFactorAFF()->values[pParams->m_iRefIndex[D_DIR_BWD]][curfield][ref0field][ref1field];
            iDistScaleFactor = (FactorArrayValue) (pDistScaleFactors[pParams->m_iRefIndex[D_DIR_FWD]] >> 2);
        }
        else
        {
            iDistScaleFactor = (FactorArrayValue) (pParams->m_pSegDec->m_pSlice->GetDistScaleFactor()->values[pParams->m_iRefIndex[D_DIR_BWD]][pParams->m_iRefIndex[D_DIR_FWD]] >> 2);
        }

        // combine bidir predictions into one,
        // implicit weighting

        BiDirWeightBlockImplicit(pParams->m_pDstY, &pParams->m_bidirLuma,
                                 64 - iDistScaleFactor,
                                 iDistScaleFactor);

        if (color_format)
        {
            pParams->m_bidirChroma[0].roiSize.width <<= 1;

            BiDirWeightBlockImplicit(pParams->m_pDstUV,
                                        &pParams->m_bidirChroma[0],
                                        64 - iDistScaleFactor,
                                        iDistScaleFactor);

            pParams->m_bidirChroma[0].roiSize.width >>= 1;
        }
    } // void BiDirWeightMacroblockImplicit(ReconstructParams *pParams)

    inline
    void UniDirWeightLuma(ReconstructParams *pParams, int32_t iDir)
    {
        UniDirWeightBlock(pParams->m_pDstY,
                          &pParams->m_bidirLuma,
                          pParams->luma_log2_weight_denom,
                          pParams->m_pSegDec->m_pPredWeight[iDir][pParams->m_iRefIndex[iDir]].luma_weight,
                          pParams->m_pSegDec->m_pPredWeight[iDir][pParams->m_iRefIndex[iDir]].luma_offset);

    } // void UniDirWeightLuma(ReconstructParams *params, int32_t iDir)

    inline
    void UniDirWeightChroma(ReconstructParams *pParams, int32_t iDir)
    {
        UniDirWeightBlock_NV12(pParams->m_pDstUV,
                            &pParams->m_bidirChroma[0],
                            pParams->chroma_log2_weight_denom,
                            pParams->m_pSegDec->m_pPredWeight[iDir][pParams->m_iRefIndex[iDir]].chroma_weight[0],
                            pParams->m_pSegDec->m_pPredWeight[iDir][pParams->m_iRefIndex[iDir]].chroma_offset[0],
                            pParams->m_pSegDec->m_pPredWeight[iDir][pParams->m_iRefIndex[iDir]].chroma_weight[1],
                            pParams->m_pSegDec->m_pPredWeight[iDir][pParams->m_iRefIndex[iDir]].chroma_offset[1]);
    } // void UniDirWeightLuma(ReconstructParams *params, int32_t iDir)

    void CompensateBiDirBlock(ReconstructParams &params,
                              PlanePtrY pDstY,
                              PlanePtrUV pDstUV,
                              int32_t iPitchLuma,
                              int32_t iPitchChroma,
                              int32_t iBlockNumber)
    {
        if (GetReferenceIndex(params.m_pRefIndex[D_DIR_FWD], iBlockNumber) == -1 ||
            GetReferenceIndex(params.m_pRefIndex[D_DIR_BWD], iBlockNumber) == -1)
        {
            int32_t iDir = GetReferenceIndex(params.m_pRefIndex[D_DIR_FWD], iBlockNumber) == -1 ? D_DIR_BWD : D_DIR_FWD;
            CompensateUniDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma, iDir, iBlockNumber);
            return;
        }

        // set the destination
        params.m_lumaInterpolateInfo.pDst[0] = (uint16_t*)(((PlanePtrY) params.m_pSegDec->m_pPredictionBuffer) +
                         params.m_iIntraMBLumaOffsetTmp);
        params.m_lumaInterpolateInfo.dstStep = 16;
        // do forward prediction
        CompensateMotionLumaBlock(&params, D_DIR_FWD, iBlockNumber, BI_DIR);

        if (color_format)
        {
            params.m_chromaInterpolateInfo.pDst[0] = (uint16_t*)((PlanePtrUV) ((((PlanePtrY) params.m_pSegDec->m_pPredictionBuffer) + 16 * 16) +
                                          params.m_iIntraMBChromaOffsetTmp));
            params.m_chromaInterpolateInfo.dstStep = 16;
            // do forward prediction
            CompensateMotionChromaBlock(&params, D_DIR_FWD, iBlockNumber, BI_DIR);
        }

        // set the destination
        params.m_lumaInterpolateInfo.pDst[0] = (uint16_t*)(pDstY + params.m_iIntraMBLumaOffset);
        params.m_lumaInterpolateInfo.dstStep = iPitchLuma;

        params.m_bidirLuma.pDst = (uint16_t*)((PlanePtrY)pDstY + params.m_iIntraMBLumaOffset);
        params.m_bidirLuma.dstStep = iPitchLuma;
        params.m_bidirLuma.roiSize = params.m_lumaInterpolateInfo.sizeBlock;

        // do backward prediction
        CompensateMotionLumaBlock(&params, D_DIR_BWD, iBlockNumber, BI_DIR);

        if (color_format)
        {
            params.m_bidirChroma[0].pDst = params.m_chromaInterpolateInfo.pDst[0] = (uint16_t*)(pDstUV + params.m_iIntraMBChromaOffset);
            params.m_bidirChroma[0].dstStep = params.m_chromaInterpolateInfo.dstStep = iPitchChroma;
            params.m_bidirChroma[0].roiSize = params.m_chromaInterpolateInfo.sizeBlock;

            // do backward prediction
            CompensateMotionChromaBlock(&params, D_DIR_BWD, iBlockNumber, BI_DIR);
        }

        // do waighting
        if ((is_weight) &&
            (params.m_bBidirWeightMB))
        {
            if (1 == params.weighted_bipred_idc)
                BiDirWeightMacroblock(&params);
            else if (2 == params.weighted_bipred_idc)
                BiDirWeightMacroblockImplicit(&params, iBlockNumber);
            else
                VM_ASSERT(0);
        }
        else
        {
            InterpolateMacroblock(&params);
        }

    } // void CompensateBiDirBlock(ReconstructParams &params,

    void CompensateUniDirBlock(ReconstructParams &params,
                               PlanePtrY pDstY,
                               PlanePtrUV pDstUV,
                               int32_t iPitchLuma,
                               int32_t iPitchChroma,
                               int32_t iDir,
                               int32_t iBlockNumber)
    {
        // set the destination
        params.m_lumaInterpolateInfo.pDst[0] = (uint16_t*)(pDstY + params.m_iIntraMBLumaOffset);
        params.m_lumaInterpolateInfo.dstStep = iPitchLuma;

        params.m_bidirLuma.pDst = (uint16_t*)((PlanePtrY)pDstY + params.m_iIntraMBLumaOffset);
        params.m_bidirLuma.dstStep = iPitchLuma;
        params.m_bidirLuma.roiSize = params.m_lumaInterpolateInfo.sizeBlock;

        // do forward prediction
        CompensateMotionLumaBlock(&params, iDir, iBlockNumber, UNI_DIR);

        if (color_format)
        {
            params.m_bidirChroma[0].pDst = params.m_chromaInterpolateInfo.pDst[0] = (uint16_t*)(pDstUV + params.m_iIntraMBChromaOffset);
            params.m_bidirChroma[0].dstStep = params.m_chromaInterpolateInfo.dstStep = iPitchChroma;
            params.m_bidirChroma[0].roiSize = params.m_chromaInterpolateInfo.sizeBlock;

            // do forward prediction
            CompensateMotionChromaBlock(&params, iDir, iBlockNumber, UNI_DIR);
        }

        // optional prediction weighting
        if ((is_weight) &&
            (params.m_bUnidirWeightMB))
        {
            const PredWeightTable &pTab = params.m_pSegDec->m_pPredWeight[iDir][params.m_iRefIndex[iDir]];

            if (pTab.luma_weight_flag)
                UniDirWeightLuma(&params, iDir);

            if ((color_format) &&
                (pTab.chroma_weight_flag))
                UniDirWeightChroma(&params, iDir);
        }

    } // void CompensateUniDirBlock(ReconstructParams &params,

    void CompensateBlock8x8(PlanePtrY pDstY,
                            PlanePtrUV pDstUV,
                            int32_t iPitchLuma,
                            int32_t iPitchChroma,
                            ReconstructParams &params,
                            int32_t iSubBlockType,
                            int32_t iSubBlockDir,
                            int32_t iSubBlockNumber)
    {
        switch (iSubBlockType)
        {
        case SBTYPE_8x8:
            {
                params.m_lumaInterpolateInfo.sizeBlock.width = 8;
                params.m_lumaInterpolateInfo.sizeBlock.height = 8;
                params.m_chromaInterpolateInfo.sizeBlock.width = 8 >> roi_width_chroma_div;
                params.m_chromaInterpolateInfo.sizeBlock.height = 8 >> roi_height_chroma_div;
                params.m_iIntraMBLumaOffset = 0;
                params.m_iIntraMBChromaOffset = 0;

                if ((D_DIR_BIDIR == iSubBlockDir) ||
                    (D_DIR_DIRECT == iSubBlockDir) ||
                    (D_DIR_DIRECT_SPATIAL_BIDIR == iSubBlockDir))
                {
                    params.m_iIntraMBLumaOffsetTmp = 0;
                    params.m_iIntraMBChromaOffsetTmp = 0;

                    CompensateBiDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma,
                                         iSubBlockNumber);
                }
                else
                {
                    int32_t iDir = ((D_DIR_BWD == iSubBlockDir) || (D_DIR_DIRECT_SPATIAL_BWD == iSubBlockDir)) ?
                                   (D_DIR_BWD) :
                                   (D_DIR_FWD);

                    CompensateUniDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma, iDir,
                                          iSubBlockNumber);
                }
            }
            break;

        case SBTYPE_8x4:
            {
                params.m_lumaInterpolateInfo.sizeBlock.width = 8;
                params.m_lumaInterpolateInfo.sizeBlock.height = 4;
                params.m_chromaInterpolateInfo.sizeBlock.width = 8 >> roi_width_chroma_div;
                params.m_chromaInterpolateInfo.sizeBlock.height = 4 >> roi_height_chroma_div;
                params.m_iIntraMBLumaOffset = 0;
                params.m_iIntraMBChromaOffset = 0;

                if ((D_DIR_BIDIR == iSubBlockDir) ||
                    (D_DIR_DIRECT == iSubBlockDir) ||
                    (D_DIR_DIRECT_SPATIAL_BIDIR == iSubBlockDir))
                {
                    params.m_iIntraMBLumaOffsetTmp = 0;
                    params.m_iIntraMBChromaOffsetTmp = 0;

                    CompensateBiDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma,
                                         iSubBlockNumber);

                    // set sub-block offset for second half of MB
                    params.m_lumaInterpolateInfo.pointBlockPos.y += 4;
                    params.m_chromaInterpolateInfo.pointBlockPos.y += 4 >> point_height_chroma_div;
                    params.m_iIntraMBLumaOffset = 4 * iPitchLuma;
                    params.m_iIntraMBChromaOffset = (4 >> height_chroma_div) * iPitchChroma;

                    params.m_iIntraMBLumaOffsetTmp = 4 * 16;
                    params.m_iIntraMBChromaOffsetTmp = (4 >> height_chroma_div) * 16;

                    CompensateBiDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma,
                                         iSubBlockNumber + 4);
                }
                else
                {
                    int32_t iDir = ((D_DIR_BWD == iSubBlockDir) || (D_DIR_DIRECT_SPATIAL_BWD == iSubBlockDir)) ?
                                   (D_DIR_BWD) :
                                   (D_DIR_FWD);

                    CompensateUniDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma, iDir,
                                          iSubBlockNumber);

                    // set sub-block offset for second half of MB
                    params.m_lumaInterpolateInfo.pointBlockPos.y += 4;
                    params.m_chromaInterpolateInfo.pointBlockPos.y += 4 >> point_height_chroma_div;
                    params.m_iIntraMBLumaOffset = 4 * iPitchLuma;
                    params.m_iIntraMBChromaOffset = (4 >> height_chroma_div) * iPitchChroma;

                    CompensateUniDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma, iDir,
                                          iSubBlockNumber + 4);
                }
            }
            break;

        case SBTYPE_4x8:
            {
                params.m_lumaInterpolateInfo.sizeBlock.width = 4;
                params.m_lumaInterpolateInfo.sizeBlock.height = 8;
                params.m_chromaInterpolateInfo.sizeBlock.width = 4 >> roi_width_chroma_div;
                params.m_chromaInterpolateInfo.sizeBlock.height = 8 >> roi_height_chroma_div;
                params.m_iIntraMBLumaOffset = 0;
                params.m_iIntraMBChromaOffset = 0;

                if ((D_DIR_BIDIR == iSubBlockDir) ||
                    (D_DIR_DIRECT == iSubBlockDir) ||
                    (D_DIR_DIRECT_SPATIAL_BIDIR == iSubBlockDir))
                {
                    params.m_iIntraMBLumaOffsetTmp = 0;
                    params.m_iIntraMBChromaOffsetTmp = 0;

                    CompensateBiDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma,
                                         iSubBlockNumber);

                    // set sub-block offset for second half of MB
                    params.m_lumaInterpolateInfo.pointBlockPos.x += 4;
                    params.m_chromaInterpolateInfo.pointBlockPos.x += 4 >> point_width_chroma_div;
                    params.m_iIntraMBLumaOffset = 4;
                    params.m_iIntraMBChromaOffset = (4 >> width_chroma_div);

                    params.m_iIntraMBLumaOffsetTmp = 4;
                    params.m_iIntraMBChromaOffsetTmp = (4 >> width_chroma_div);

                    CompensateBiDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma,
                                         iSubBlockNumber + 1);
                }
                else
                {
                    int32_t iDir = ((D_DIR_BWD == iSubBlockDir) || (D_DIR_DIRECT_SPATIAL_BWD == iSubBlockDir)) ?
                                   (D_DIR_BWD) :
                                   (D_DIR_FWD);

                    CompensateUniDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma, iDir,
                                          iSubBlockNumber);

                    // set sub-block offset for second half of MB
                    params.m_lumaInterpolateInfo.pointBlockPos.x += 4;
                    params.m_chromaInterpolateInfo.pointBlockPos.x += 4 >> point_width_chroma_div;
                    params.m_iIntraMBLumaOffset = 4;
                    params.m_iIntraMBChromaOffset = (4 >> width_chroma_div);

                    CompensateUniDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma, iDir,
                                          iSubBlockNumber + 1);
                }
            }
            break;

        default:
            // 4x4 sub division
            {
                params.m_lumaInterpolateInfo.sizeBlock.width = 4;
                params.m_lumaInterpolateInfo.sizeBlock.height = 4;
                params.m_chromaInterpolateInfo.sizeBlock.width = 4 >> roi_width_chroma_div;
                params.m_chromaInterpolateInfo.sizeBlock.height = 4 >> roi_height_chroma_div;
                params.m_iIntraMBLumaOffset = 0;
                params.m_iIntraMBChromaOffset = 0;

                if ((D_DIR_BIDIR == iSubBlockDir) ||
                    (D_DIR_DIRECT == iSubBlockDir) ||
                    (D_DIR_DIRECT_SPATIAL_BIDIR == iSubBlockDir))
                {
                    params.m_iIntraMBLumaOffsetTmp = 0;
                    params.m_iIntraMBChromaOffsetTmp = 0;

                    CompensateBiDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma,
                                         iSubBlockNumber);

                    // set sub-block offset for second quarter of MB
                    params.m_lumaInterpolateInfo.pointBlockPos.x += 4;
                    params.m_chromaInterpolateInfo.pointBlockPos.x += 4 >> point_width_chroma_div;
                    params.m_iIntraMBLumaOffset = 4;
                    params.m_iIntraMBChromaOffset = (4 >> width_chroma_div);

                    params.m_iIntraMBLumaOffsetTmp = 4;
                    params.m_iIntraMBChromaOffsetTmp = (4 >> width_chroma_div);

                    CompensateBiDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma,
                                         iSubBlockNumber + 1);

                    // set sub-block offset for third quarter of MB
                    params.m_lumaInterpolateInfo.pointBlockPos.x -= 4;
                    params.m_lumaInterpolateInfo.pointBlockPos.y += 4;
                    params.m_chromaInterpolateInfo.pointBlockPos.x -= 4 >> point_width_chroma_div;
                    params.m_chromaInterpolateInfo.pointBlockPos.y += 4 >> point_height_chroma_div;
                    params.m_iIntraMBLumaOffset = 4 * iPitchLuma;
                    params.m_iIntraMBChromaOffset = (4 >> height_chroma_div) * iPitchChroma;

                    params.m_iIntraMBLumaOffsetTmp = 4 * 16;
                    params.m_iIntraMBChromaOffsetTmp = (4 >> height_chroma_div) * 16;

                    CompensateBiDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma,
                                         iSubBlockNumber + 4);

                    // set sub-block offset for fourth quarter of MB
                    params.m_lumaInterpolateInfo.pointBlockPos.x += 4;
                    params.m_chromaInterpolateInfo.pointBlockPos.x += 4 >> point_width_chroma_div;
                    params.m_iIntraMBLumaOffset = 4 + 4 * iPitchLuma;
                    params.m_iIntraMBChromaOffset = (4 >> width_chroma_div) +
                                                    (4 >> height_chroma_div) * iPitchChroma;

                    params.m_iIntraMBLumaOffsetTmp = 4 + 4 * 16;
                    params.m_iIntraMBChromaOffsetTmp = (4 >> width_chroma_div) +
                                                       (4 >> height_chroma_div) * 16;

                    CompensateBiDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma,
                                         iSubBlockNumber + 5);
                }
                else
                {
                    int32_t iDir = ((D_DIR_BWD == iSubBlockDir) || (D_DIR_DIRECT_SPATIAL_BWD == iSubBlockDir)) ?
                                   (D_DIR_BWD) :
                                   (D_DIR_FWD);

                    CompensateUniDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma, iDir,
                                          iSubBlockNumber);

                    // set sub-block offset for second quarter of MB
                    params.m_lumaInterpolateInfo.pointBlockPos.x += 4;
                    params.m_chromaInterpolateInfo.pointBlockPos.x += 4 >> point_width_chroma_div;
                    params.m_iIntraMBLumaOffset = 4;
                    params.m_iIntraMBChromaOffset = (4 >> width_chroma_div);

                    CompensateUniDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma, iDir,
                                          iSubBlockNumber + 1);

                    // set sub-block offset for third quarter of MB
                    params.m_lumaInterpolateInfo.pointBlockPos.x -= 4;
                    params.m_lumaInterpolateInfo.pointBlockPos.y += 4;
                    params.m_chromaInterpolateInfo.pointBlockPos.x -= 4 >> point_width_chroma_div;
                    params.m_chromaInterpolateInfo.pointBlockPos.y += 4 >> point_height_chroma_div;
                    params.m_iIntraMBLumaOffset = 4 * iPitchLuma;
                    params.m_iIntraMBChromaOffset = (4 >> height_chroma_div) * iPitchChroma;

                    CompensateUniDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma, iDir,
                                          iSubBlockNumber + 4);

                    // set sub-block offset for fourth quarter of MB
                    params.m_lumaInterpolateInfo.pointBlockPos.x += 4;
                    params.m_chromaInterpolateInfo.pointBlockPos.x += 4 >> point_width_chroma_div;
                    params.m_iIntraMBLumaOffset = 4 + 4 * iPitchLuma;
                    params.m_iIntraMBChromaOffset = (4 >> width_chroma_div) +
                                                    (4 >> height_chroma_div) * iPitchChroma;

                    CompensateUniDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma, iDir,
                                          iSubBlockNumber + 5);
                }
            }
            break;
        }

    } // void CompensateBlock8x8(PlanePtrY pDstY,

    void CompensateMotionMacroBlock(PlanePtrY pDstY,
                                    PlanePtrUV pDstUV,
                                    uint32_t mbXOffset, // for edge clipping
                                    uint32_t mbYOffset,
                                    int32_t offsetY,
                                    int32_t offsetC,
                                    int32_t pitch_luma,
                                    int32_t pitch_chroma,
                                    H264SegmentDecoder *sd)
    {
        int32_t mbtype = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        int8_t *psbdir = sd->m_cur_mb.LocalMacroblockInfo->sbdir;

        bool bBidirWeightMB = false;    // is bidir weighting in effect for the MB?
        bool bUnidirWeightMB = false;    // is explicit L0 weighting in effect for the MB?

        // Optional weighting vars
        uint32_t weighted_bipred_idc = 0;
        uint32_t luma_log2_weight_denom = 0;
        uint32_t chroma_log2_weight_denom = 0;

        RefIndexType *pRefIndexL0 = 0;
        RefIndexType *pRefIndexL1 = 0;

        VM_ASSERT(IS_INTER_MBTYPE(sd->m_cur_mb.GlobalMacroblockInfo->mbtype));

        ReconstructParams params;
        params.is_mbaff = sd->m_isMBAFF;
        params.is_bottom_mb = (params.is_mbaff && (sd->m_CurMBAddr & 1)) ? 1 : 0;

        pRefIndexL0 = sd->m_cur_mb.GetReferenceIndexStruct(0)->refIndexs;

        if (((PREDSLICE == sd->m_pSliceHeader->slice_type) ||
            (S_PREDSLICE == sd->m_pSliceHeader->slice_type)) &&
            (sd->m_pPicParamSet->weighted_pred_flag != 0))
        {
            // L0 weighting specified in pic param set. Get weighting params
            // for the slice.
            luma_log2_weight_denom = sd->m_pSliceHeader->luma_log2_weight_denom;
            chroma_log2_weight_denom = sd->m_pSliceHeader->chroma_log2_weight_denom;
            bUnidirWeightMB = true;
        }

        // get luma interp func pointer table in cache
        if (sd->m_pSliceHeader->slice_type == BPREDSLICE)
        {
            pRefIndexL1 = sd->m_cur_mb.GetReferenceIndexStruct(1)->refIndexs;
            // DIRECT MB have the same subblock partition structure as the
            // colocated MB. Take advantage of that to perform motion comp
            // for the direct MB using the largest partitions possible.
            if (mbtype == MBTYPE_DIRECT || mbtype == MBTYPE_SKIPPED)
            {
                mbtype = MBTYPE_INTER_8x8;
            }

            // Bi-dir weighting?
            weighted_bipred_idc = sd->m_pPicParamSet->weighted_bipred_idc;
            if (weighted_bipred_idc == 1)
            {
                // explicit bidir weighting
                luma_log2_weight_denom = sd->m_pSliceHeader->luma_log2_weight_denom;
                chroma_log2_weight_denom = sd->m_pSliceHeader->chroma_log2_weight_denom;
                bUnidirWeightMB = true;
                bBidirWeightMB = true;
            }
            if (weighted_bipred_idc == 2)
            {
                bBidirWeightMB = true;
            }
        }

        params.m_lumaInterpolateInfo.bitDepth = sd->bit_depth_luma;
        params.m_lumaInterpolateInfo.sizeFrame = sd->m_pCurrentFrame->lumaSize();
        params.m_lumaInterpolateInfo.sizeFrame.height >>= is_field;

        params.m_lumaInterpolateInfo.srcStep = sd->m_pCurrentFrame->pitch_luma();

        params.m_chromaInterpolateInfo.bitDepth = sd->bit_depth_chroma;
        params.m_chromaInterpolateInfo.sizeFrame = sd->m_pCurrentFrame->chromaSize();
        params.m_chromaInterpolateInfo.sizeFrame.height >>= is_field;

        params.m_chromaInterpolateInfo.srcStep = sd->m_pCurrentFrame->pitch_chroma();

        if (is_field)
        {
            params.m_lumaInterpolateInfo.srcStep *= 2;
            params.m_chromaInterpolateInfo.srcStep *= 2;
        }

        params.m_bidirLuma.bitDepth = sd->bit_depth_luma;
        params.m_bidirChroma[0].bitDepth = sd->bit_depth_chroma;
        params.m_bidirChroma[1].bitDepth = sd->bit_depth_chroma;

        int32_t iPitchLuma = pitch_luma;
        int32_t iPitchChroma = pitch_chroma;

        params.m_pMV[0] = sd->m_cur_mb.MVs[0]->MotionVectors;
        params.m_pMV[1] = (BPREDSLICE == sd->m_pSliceHeader->slice_type) ? (sd->m_cur_mb.MVs[1]->MotionVectors) : (NULL);
        params.m_pRefIndex[0] = pRefIndexL0;
        params.m_pRefIndex[1] = pRefIndexL1;
        params.m_iOffsetLuma = offsetY;
        params.m_iOffsetChroma = offsetC;
        params.m_lumaInterpolateInfo.pointBlockPos.x = mbXOffset;
        params.m_lumaInterpolateInfo.pointBlockPos.y = mbYOffset;
        params.m_chromaInterpolateInfo.pointBlockPos.x = mbXOffset >> point_width_chroma_div;
        params.m_chromaInterpolateInfo.pointBlockPos.y = mbYOffset >> point_height_chroma_div;
        if (is_weight)
        {
            params.luma_log2_weight_denom = luma_log2_weight_denom;
            params.chroma_log2_weight_denom = chroma_log2_weight_denom;
            params.weighted_bipred_idc = weighted_bipred_idc;
            params.m_bBidirWeightMB = bBidirWeightMB;
            params.m_bUnidirWeightMB = bUnidirWeightMB;
        }

        params.m_pSegDec = sd;

        if (mbtype != MBTYPE_INTER_8x8 && mbtype != MBTYPE_INTER_8x8_REF0)
        {
            // reconstruct macro block
            switch (mbtype)
            {
            case MBTYPE_INTER_16x8:
                {
                    params.m_lumaInterpolateInfo.sizeBlock.width = 16;
                    params.m_lumaInterpolateInfo.sizeBlock.height = 8;
                    params.m_chromaInterpolateInfo.sizeBlock.width = 16 >> roi_width_chroma_div;
                    params.m_chromaInterpolateInfo.sizeBlock.height = 8 >> roi_height_chroma_div;
                    params.m_iIntraMBLumaOffset = 0;
                    params.m_iIntraMBChromaOffset = 0;

                    if (D_DIR_BIDIR == psbdir[0] || D_DIR_DIRECT_SPATIAL_BIDIR == psbdir[0])
                    {
                        params.m_iIntraMBLumaOffsetTmp = 0;
                        params.m_iIntraMBChromaOffsetTmp = 0;

                        CompensateBiDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma,
                                             0);
                    }
                    else
                    {
                        int32_t iDir = ((D_DIR_BWD == psbdir[0]) || (D_DIR_DIRECT_SPATIAL_BWD == psbdir[0])) ?
                                       (D_DIR_BWD) :
                                       (D_DIR_FWD);

                        CompensateUniDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma, iDir,
                                              0);
                    }

                    // set sub-block offset for second half of MB
                    params.m_lumaInterpolateInfo.pointBlockPos.y += 8;
                    params.m_chromaInterpolateInfo.pointBlockPos.y += 8 >> point_height_chroma_div;
                    params.m_iIntraMBLumaOffset = 8 * iPitchLuma;
                    params.m_iIntraMBChromaOffset = (8 >> height_chroma_div) * iPitchChroma;

                    if (D_DIR_BIDIR == psbdir[1] || D_DIR_DIRECT_SPATIAL_BIDIR == psbdir[1])
                    {
                        params.m_iIntraMBLumaOffsetTmp = 8 * 16;
                        params.m_iIntraMBChromaOffsetTmp = (8 >> height_chroma_div) * 16;

                        CompensateBiDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma,
                                             8);
                    }
                    else
                    {
                        int32_t iDir = ((D_DIR_BWD == psbdir[1]) || (D_DIR_DIRECT_SPATIAL_BWD == psbdir[1])) ?
                                       (D_DIR_BWD) :
                                       (D_DIR_FWD);

                        CompensateUniDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma, iDir,
                                              8);
                    }
                }
                break;

            case MBTYPE_INTER_8x16:
                {
                    params.m_lumaInterpolateInfo.sizeBlock.width = 8;
                    params.m_lumaInterpolateInfo.sizeBlock.height = 16;
                    params.m_chromaInterpolateInfo.sizeBlock.width = 8 >> roi_width_chroma_div;
                    params.m_chromaInterpolateInfo.sizeBlock.height = 16 >> roi_height_chroma_div;
                    params.m_iIntraMBLumaOffset = 0;
                    params.m_iIntraMBChromaOffset = 0;

                    if (D_DIR_BIDIR == psbdir[0] || D_DIR_DIRECT_SPATIAL_BIDIR == psbdir[0])
                    {
                        params.m_iIntraMBLumaOffsetTmp = 0;
                        params.m_iIntraMBChromaOffsetTmp = 0;

                        CompensateBiDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma,
                                             0);
                    }
                    else
                    {
                        int32_t iDir = ((D_DIR_BWD == psbdir[0]) || (D_DIR_DIRECT_SPATIAL_BWD == psbdir[0])) ?
                                       (D_DIR_BWD) :
                                       (D_DIR_FWD);

                        CompensateUniDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma, iDir,
                                              0);
                    }

                    // set sub-block offset for second half of MB
                    params.m_lumaInterpolateInfo.pointBlockPos.x += 8;
                    params.m_chromaInterpolateInfo.pointBlockPos.x += 8 >> point_width_chroma_div;
                    params.m_iIntraMBLumaOffset = 8;
                    params.m_iIntraMBChromaOffset = 8 >> width_chroma_div;

                    if (D_DIR_BIDIR == psbdir[1] || D_DIR_DIRECT_SPATIAL_BIDIR == psbdir[1])
                    {
                        params.m_iIntraMBLumaOffsetTmp = 8;
                        params.m_iIntraMBChromaOffsetTmp = 8 >> width_chroma_div;

                        CompensateBiDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma,
                                             2);
                    }
                    else
                    {
                        int32_t iDir = ((D_DIR_BWD == psbdir[1]) || (D_DIR_DIRECT_SPATIAL_BWD == psbdir[1])) ?
                                       (D_DIR_BWD) :
                                       (D_DIR_FWD);

                        CompensateUniDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma, iDir,
                                              2);
                    }
                }
                break;

            default:
                {
                    params.m_lumaInterpolateInfo.sizeBlock.width = 16;
                    params.m_lumaInterpolateInfo.sizeBlock.height = 16;
                    params.m_chromaInterpolateInfo.sizeBlock.width = 16 >> roi_width_chroma_div;
                    params.m_chromaInterpolateInfo.sizeBlock.height = 16 >> roi_height_chroma_div;
                    params.m_iIntraMBLumaOffset = 0;
                    params.m_iIntraMBChromaOffset = 0;

                    if (MBTYPE_BIDIR == mbtype)
                    {
                        params.m_iIntraMBLumaOffsetTmp = 0;
                        params.m_iIntraMBChromaOffsetTmp = 0;

                        CompensateBiDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma,
                                             0);
                    }
                    else
                    {
                        int32_t iDir = (MBTYPE_BACKWARD == mbtype) ? (D_DIR_BWD) : (D_DIR_FWD);

                        CompensateUniDirBlock(params, pDstY, pDstUV, iPitchLuma, iPitchChroma, iDir,
                                              0);
                    }
                }
                break;
            }
        }
        else
        {
            int8_t *pSubBlockType = sd->m_cur_mb.GlobalMacroblockInfo->sbtype;
            int8_t *pSubBlockDir = sd->m_cur_mb.LocalMacroblockInfo->sbdir;

            // sub block 0
            CompensateBlock8x8(pDstY, pDstUV, iPitchLuma, iPitchChroma,
                params, pSubBlockType[0], pSubBlockDir[0], 0);

            // sub block 1
            pDstY += 8;
            pDstUV += (8 >> width_chroma_div);
            params.m_iOffsetLuma = offsetY + 8;
            params.m_iOffsetChroma = offsetC + (8 >> width_chroma_div);
            params.m_lumaInterpolateInfo.pointBlockPos.x = mbXOffset + 8;
            params.m_lumaInterpolateInfo.pointBlockPos.y = mbYOffset;
            params.m_chromaInterpolateInfo.pointBlockPos.x = (mbXOffset + 8) >> point_width_chroma_div;
            params.m_chromaInterpolateInfo.pointBlockPos.y = mbYOffset >> point_height_chroma_div;
            CompensateBlock8x8(pDstY, pDstUV, iPitchLuma, iPitchChroma,
                params, pSubBlockType[1], pSubBlockDir[1], 2);

            // sub block 2
            pDstY += - 8 + 8 * iPitchLuma;
            pDstUV += - (8 >> width_chroma_div)  +
                     (8 >> height_chroma_div) * iPitchChroma;
            params.m_iOffsetLuma = offsetY + 8 * iPitchLuma;
            params.m_iOffsetChroma = offsetC + (8 >> height_chroma_div) * iPitchChroma;
            params.m_lumaInterpolateInfo.pointBlockPos.x = mbXOffset;
            params.m_lumaInterpolateInfo.pointBlockPos.y = mbYOffset + 8;
            params.m_chromaInterpolateInfo.pointBlockPos.x = mbXOffset >> point_width_chroma_div;
            params.m_chromaInterpolateInfo.pointBlockPos.y = (mbYOffset + 8) >> point_height_chroma_div;
            CompensateBlock8x8(pDstY, pDstUV, iPitchLuma, iPitchChroma,
                params, pSubBlockType[2], pSubBlockDir[2], 8);

            // sub block 3
            pDstY += 8;
            pDstUV += (8 >> width_chroma_div);
            params.m_iOffsetLuma = offsetY + 8 + 8 * iPitchLuma;
            params.m_iOffsetChroma = offsetC + (8 >> width_chroma_div) +
                                     (8 >> height_chroma_div) * iPitchChroma;
            params.m_lumaInterpolateInfo.pointBlockPos.x = mbXOffset + 8;
            params.m_lumaInterpolateInfo.pointBlockPos.y = mbYOffset + 8;
            params.m_chromaInterpolateInfo.pointBlockPos.x = (mbXOffset + 8) >> point_width_chroma_div;
            params.m_chromaInterpolateInfo.pointBlockPos.y = (mbYOffset + 8) >> point_height_chroma_div;
            CompensateBlock8x8(pDstY, pDstUV, iPitchLuma, iPitchChroma,
                params, pSubBlockType[3], pSubBlockDir[3], 10);
        }
    } // void CompensateMotionMacroBlock(PlanePtrY pDstY,

    ////////////////////////////////////////////////////////////////////////////////
    // Copy raw pixel values from the bitstream to the reconstructed frame for
    // all luma and chroma blocks of one macroblock.
    ////////////////////////////////////////////////////////////////////////////////
    void ReconstructPCMMB(uint32_t offsetY, uint32_t offsetC, int32_t pitch_luma, int32_t pitch_chroma,
                          H264SegmentDecoder * sd)
    {
        // to retrieve non-aligned pointer from m_pCoeffBlocksRead
        PlanePtrY  pDstY = (PlanePtrY)sd->m_pYPlane + offsetY;
        PlanePtrUV pDstUV = (PlanePtrUV)sd->m_pUVPlane + offsetC;

        PlanePtrY pCoeffBlocksRead_Y = reinterpret_cast<PlanePtrY> (sd->m_pCoeffBlocksRead);
        // get pointer to raw bytes from m_pCoeffBlocksRead
        for (int32_t i = 0; i<16; i++)
            MFX_INTERNAL_CPY(pDstY + i * pitch_luma, pCoeffBlocksRead_Y + i * 16, 16*sizeof(PlaneY));

        sd->m_pCoeffBlocksRead = (UMC::CoeffsPtrCommon)((uint8_t*)sd->m_pCoeffBlocksRead +
                                 256*sizeof(PlaneY));

        if (color_format)
        {
            Ipp32s const iWidth = (color_format == 3) ? 16 : 8;
            Ipp32s const iHeight = 8 + 8 * (color_format >> 1);
            size_t const pitch = iWidth;

            PlaneUV pSrcDstUPlane[iHeight * pitch];
            PlaneUV pSrcDstVPlane[iHeight * pitch];

            PlanePtrUV pCoeffBlocksRead_UV = (PlanePtrUV) (sd->m_pCoeffBlocksRead);
            for (int32_t i = 0; i < iHeight; i += 1)
                MFX_INTERNAL_CPY(pSrcDstUPlane + i * pitch, pCoeffBlocksRead_UV + i * iWidth, iWidth*sizeof(PlaneUV));

            pCoeffBlocksRead_UV += iWidth * iHeight * sizeof(PlaneUV);

            for (int32_t i = 0; i < iHeight; i += 1)
                MFX_INTERNAL_CPY(pSrcDstVPlane + i * pitch, pCoeffBlocksRead_UV + i * iWidth, iWidth*sizeof(PlaneUV));

            sd->m_pCoeffBlocksRead = (UMC::CoeffsPtrCommon)((uint8_t*)sd->m_pCoeffBlocksRead +
                2* iWidth * iHeight * sizeof(PlaneUV));

            mfxSize roi = {8,8};
            ConvertYV12ToNV12(pSrcDstUPlane, pSrcDstVPlane, 8, pDstUV, pitch_chroma, roi);
        }
    } // void ReconstructPCMMB(
};

// restore the "conditional expression is constant" warning
#ifdef _MSVC_LANG
#pragma warning(default: 4127)
#endif

} // namespace UMC

#endif // __UMC_H264_RECONSTRUCT_TEMPLATES_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
