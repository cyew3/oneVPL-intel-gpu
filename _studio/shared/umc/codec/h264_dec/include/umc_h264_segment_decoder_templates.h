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

#ifndef __UMC_H264_SEGMENT_DECODER_TEMPLATES_H
#define __UMC_H264_SEGMENT_DECODER_TEMPLATES_H

#include "umc_h264_dec_internal_cabac.h"
#include "umc_h264_reconstruct_templates.h"
#include "umc_h264_redual_decoder_templates.h"

#include "umc_h264_timing.h"

#include "umc_h264_resampling_templates.h"

namespace UMC
{

#pragma warning(disable: 4127)

template <Ipp32s color_format, typename Coeffs, typename PlaneY, typename PlaneUV, Ipp32s is_field>
class ColorSpecific
{
public:

    static inline void UpdateNeighbouringBlocks(H264SegmentDecoderMultiThreaded * sd)
    {
        switch(color_format)
        {
        case 3:
            sd->UpdateNeighbouringBlocksH4();
            break;
        case 2:
            sd->UpdateNeighbouringBlocksH2();
            break;
        default:
            // field of frame case
            if (!sd->m_isMBAFF)
            {
                H264DecoderBlockNeighboursInfo *p = &(sd->m_cur_mb.CurrentBlockNeighbours);
                H264DecoderMacroblockNeighboursInfo *pMBs = &(sd->m_cur_mb.CurrentMacroblockNeighbours);

                p->mbs_left[0].mb_num =
                p->mbs_left[1].mb_num =
                p->mbs_left[2].mb_num =
                p->mbs_left[3].mb_num = pMBs->mb_A;
                p->mb_above.mb_num = pMBs->mb_B;
                p->mb_above_right.mb_num = pMBs->mb_C;
                p->mb_above_left.mb_num = pMBs->mb_D;
                p->mbs_left_chroma[0][0].mb_num =
                p->mbs_left_chroma[0][1].mb_num =
                p->mbs_left_chroma[1][0].mb_num =
                p->mbs_left_chroma[1][1].mb_num = pMBs->mb_A;
                p->mb_above_chroma[0].mb_num =
                p->mb_above_chroma[1].mb_num = pMBs->mb_B;

                if (0 == p->m_bInited)
                {
                    p->mbs_left[0].block_num = 3;
                    p->mbs_left[1].block_num = 7;
                    p->mbs_left[2].block_num = 11;
                    p->mbs_left[3].block_num = 15;
                    p->mb_above.block_num = 12;
                    p->mb_above_right.block_num = 12;
                    p->mb_above_left.block_num = 15;
                    p->mbs_left_chroma[0][0].block_num = 17;
                    p->mbs_left_chroma[0][1].block_num = 19;
                    p->mbs_left_chroma[1][0].block_num = 21;
                    p->mbs_left_chroma[1][1].block_num = 23;
                    p->mb_above_chroma[0].block_num = 18;
                    p->mb_above_chroma[1].block_num = 22;

                    // set the init flag
                    p->m_bInited = 1;
                }
            }
            else
            {
                sd->UpdateNeighbouringBlocksBMEH();
            }
            break;
        }
    }

    static inline Ipp32u GetChromaAC()
    {
        switch(color_format)
        {
        case 3:
            return D_CBP_CHROMA_AC_444;
        case 2:
            return D_CBP_CHROMA_AC_422;
        default:
            return D_CBP_CHROMA_AC_420;
        }
    }

    static inline IppStatus ReconstructChromaIntra4x4MB_Swit(Coeffs **ppSrcDstCoeff,
                                                             PlaneUV *pSrcDstUPlane,
                                                             PlaneUV *pSrcDstVPlane,
                                                             Ipp32u srcdstUVStep,
                                                             IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                             Ipp32u cbpU,
                                                             Ipp32u cbpV,
                                                             Ipp32u chromaQPU,
                                                             Ipp32u chromaQPV,
                                                             Ipp32u levelScaleDCU,
                                                             Ipp32u levelScaleDCV,
                                                             Ipp8u  edge_type,
                                                             const Ipp16s *pQuantTableU,
                                                             const Ipp16s *pQuantTableV,
                                                             Ipp8u  bypass_flag,
                                                             Ipp32s bit_depth = 8)
    {
        switch(color_format)
        {
        case 3:
            return ReconstructChromaIntra4x4MB444(ppSrcDstCoeff,
                                                  pSrcDstUPlane,
                                                  pSrcDstVPlane,
                                                  srcdstUVStep,
                                                  intra_chroma_mode,
                                                  cbpU,
                                                  cbpV,
                                                  chromaQPU,
                                                  chromaQPV,
                                                  edge_type,
                                                  pQuantTableU,
                                                  pQuantTableV,
                                                  bypass_flag,
                                                  bit_depth);
        case 2:
            return ReconstructChromaIntra4x4MB422(ppSrcDstCoeff,
                                                  pSrcDstUPlane,
                                                  pSrcDstVPlane,
                                                  srcdstUVStep,
                                                  intra_chroma_mode,
                                                  cbpU,
                                                  cbpV,
                                                  chromaQPU,
                                                  chromaQPV,
                                                  levelScaleDCU,
                                                  levelScaleDCV,
                                                  edge_type,
                                                  pQuantTableU,
                                                  pQuantTableV,
                                                  bypass_flag,
                                                  bit_depth);
        default:
            return ReconstructChromaIntra4x4MB(ppSrcDstCoeff,
                                               pSrcDstUPlane,
                                               pSrcDstVPlane,
                                               srcdstUVStep,
                                               intra_chroma_mode,
                                               cbpU,
                                               cbpV,
                                               chromaQPU,
                                               chromaQPV,
                                               edge_type,
                                               pQuantTableU,
                                               pQuantTableV,
                                               bypass_flag,
                                               bit_depth);
        }
    }

    static inline IppStatus ReconstructChromaInter4x4MB_Swit(Coeffs **ppSrcDstCoeff,
                                                             PlaneUV *pSrcDstUPlane,
                                                             PlaneUV *pSrcDstVPlane,
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
                                                             Ipp32s bit_depth = 8)
    {
        switch(color_format)
        {
        case 3:
            return ReconstructChromaInter4x4MB444(ppSrcDstCoeff,
                                                  pSrcDstUPlane,
                                                  pSrcDstVPlane,
                                                  srcdstUVStep,
                                                  cbpU,
                                                  cbpV,
                                                  chromaQPU,
                                                  chromaQPV,
                                                  pQuantTableU,
                                                  pQuantTableV,
                                                  bypass_flag,
                                                  bit_depth);
        case 2:
            return ReconstructChromaInter4x4MB422(ppSrcDstCoeff,
                                                  pSrcDstUPlane,
                                                  pSrcDstVPlane,
                                                  srcdstUVStep,
                                                  cbpU,
                                                  cbpV,
                                                  chromaQPU,
                                                  chromaQPV,
                                                  levelScaleDCU,
                                                  levelScaleDCV,
                                                  pQuantTableU,
                                                  pQuantTableV,
                                                  bypass_flag,
                                                  bit_depth);
        default:
            return ReconstructChromaInter4x4MB(ppSrcDstCoeff,
                                               pSrcDstUPlane,
                                               pSrcDstVPlane,
                                               srcdstUVStep,
                                               cbpU,
                                               cbpV,
                                               chromaQPU,
                                               chromaQPV,
                                               pQuantTableU,
                                               pQuantTableV,
                                               bypass_flag,
                                               bit_depth);
        }
    }
};

class SegmentDecoderHPBase
{
public:

    virtual ~SegmentDecoderHPBase() {}

    virtual Status DecodeSegmentCAVLC(Ipp32u curMB, Ipp32u nMaxMBNumber,
        H264SegmentDecoderMultiThreaded * sd) = 0;

    virtual Status DecodeSegmentCAVLC_Single(Ipp32s curMB, Ipp32s nMacroBlocksToDecode,
        H264SegmentDecoderMultiThreaded * sd) = 0;

    virtual Status DecodeSegmentCABAC(Ipp32u curMB, Ipp32u nMaxMBNumber, H264SegmentDecoderMultiThreaded * sd) = 0;

    virtual Status DecodeSegmentCABAC_Single(Ipp32s curMB, Ipp32s nMacroBlocksToDecode,
        H264SegmentDecoderMultiThreaded * sd) = 0;

    virtual Status ReconstructSegment(Ipp32u curMB,Ipp32u nMaxMBNumber,
        H264SegmentDecoderMultiThreaded * sd) = 0;

    virtual Status Reconstruct_DXVA_Single(Ipp32s curMB, Ipp32s nMacroBlocksToDecode,
                                            H264SegmentDecoderMultiThreaded * sd) = 0;

    virtual void RestoreErrorRect(Ipp32s startMb, Ipp32s endMb, H264DecoderFrame *pRefFrame,
        H264SegmentDecoderMultiThreaded * sd) = 0;
};

template <typename Coeffs, typename PlaneY, typename PlaneUV, Ipp32s color_format, Ipp32s is_field, bool is_high_profile>
class MBDecoder :
    public ResidualDecoderCABAC<Coeffs, color_format, is_field>,
    public ResidualDecoderCAVLC<Coeffs, color_format, is_field>,
    public ResidualDecoderPCM<Coeffs, PlaneY, PlaneUV, color_format, is_field>
{
public:

#define inCropWindow(x) (pGetMBCropFlag(x.GlobalMacroblockInfo))

    void DecodeMacroblock_ISlice_CABAC(H264SegmentDecoderMultiThreaded *sd)
    {
        Ipp8s baseQP = sd->m_cur_mb.LocalMacroblockInfo->QP;

        sd->m_cur_mb.LocalMacroblockInfo->baseQP = baseQP;
        sd->m_cur_mb.LocalMacroblockInfo->baseMbType = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        // reset macroblock info
        if (sd->m_pSliceHeader->ref_layer_dq_id < 0)
        {
            memset(sd->m_cur_mb.LocalMacroblockInfo, 0, sizeof(H264DecoderMacroblockLocalInfo));
            memset(sd->m_cur_mb.GlobalMacroblockInfo->sbtype, 0, sizeof(sd->m_cur_mb.GlobalMacroblockInfo->sbtype));
        }

        memset(sd->m_cur_mb.GetNumCoeffs()->numCoeffs, 0, sizeof(H264DecoderMacroblockCoeffsInfo));

        sd->m_cur_mb.LocalMacroblockInfo->QP = (Ipp8s) sd->m_QuantPrev;
        sd->m_cur_mb.GlobalMacroblockInfo->slice_id = (Ipp16s) sd->m_iSliceNumber;
        memset((void *) sd->m_cur_mb.RefIdxs[0], -1, sizeof(H264DecoderMacroblockRefIdxs));
        memset((void *) sd->m_cur_mb.RefIdxs[1], -1, sizeof(H264DecoderMacroblockRefIdxs));

        if (sd->m_pSliceHeader->slice_skip_flag)
        {
            DecodeMacroblock_SkipSlice(sd, baseQP);
            sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP;
            return;
        }

        // decode macroblock field flag
        if (sd->m_isMBAFF)
        {
            if (0 == (sd->m_CurMBAddr & 1))
            {
                sd->DecodeMBFieldDecodingFlag_CABAC();
            }
        }
        else
        {
            pSetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
        }
        umc_h264_mv_resampling_mb(sd, sd->m_CurMB_X, sd->m_CurMB_Y, pGetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo));
        sd->m_cur_mb.LocalMacroblockInfo->baseMbType = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        // update neighbouring addresses
        sd->UpdateNeighbouringAddresses();
        // update neighbouring block positions
        ColorSpecific<color_format, Coeffs, PlaneY, PlaneUV, is_field>::UpdateNeighbouringBlocks(sd);

        if (sd->m_pSliceHeader->nal_ext.svc.no_inter_layer_pred_flag == 0 && inCropWindow(sd->m_cur_mb))
        {
            if (sd->m_pSliceHeader->adaptive_prediction_flag)
            {
                sd->DecodeMBBaseModeFlag_CABAC();
            }
            else
            {
                pSetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo,
                    sd->m_pSliceHeader->default_base_mode_flag);
            }
        }
        else {
            pSetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
        }

        if (pGetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo) == 0)
        {
            // First decode the "macroblock header", e.g., macroblock type,
            // intra-coding types.
            sd->DecodeMBTypeISlice_CABAC();

            // decode macroblock having I type
            if (MBTYPE_PCM != sd->m_cur_mb.GlobalMacroblockInfo->mbtype)
                DecodeMacroblock_I_CABAC(sd);
            // macroblock has PCM type
            else {
                sd->m_cur_mb.LocalMacroblockInfo->IntraTypes.intra_chroma_mode_dec = 0;
                DecodeMacroblock_PCM(sd);
            }
        }
        else
        {
            sd->m_cur_mb.LocalMacroblockInfo->IntraTypes.intra_chroma_mode_dec = 0;
            DecodeMacroblock_BASE_MODE_CABAC(sd);

            if (sd->m_use_coeff_prediction && !sd->m_cur_mb.LocalMacroblockInfo->cbp) 
            {
                sd->m_cur_mb.LocalMacroblockInfo->QP = (Ipp8s)baseQP;
            }
        }


        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs = sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual = sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
        sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP;

    } // void DecodeMacroblock_ISlice_CABAC(H264SegmentDecoderMultiThreaded *sd)

    void DecodeMacroblock_PSlice_CABAC(H264SegmentDecoderMultiThreaded *sd)
    {
        Ipp8s baseQP = sd->m_cur_mb.LocalMacroblockInfo->QP;
        Ipp32s iSkip;

        sd->m_cur_mb.LocalMacroblockInfo->baseQP = baseQP;
        sd->m_cur_mb.LocalMacroblockInfo->baseMbType = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        // reset macroblock info
        if (sd->m_pSliceHeader->ref_layer_dq_id < 0)
        {
            memset(sd->m_cur_mb.LocalMacroblockInfo, 0, sizeof(H264DecoderMacroblockLocalInfo));
            memset(sd->m_cur_mb.GlobalMacroblockInfo->sbtype, 0, sizeof(sd->m_cur_mb.GlobalMacroblockInfo->sbtype));

        }

        memset(sd->m_cur_mb.GetNumCoeffs()->numCoeffs, 0, sizeof(H264DecoderMacroblockCoeffsInfo));
        sd->m_cur_mb.GlobalMacroblockInfo->mbflags.isDirect = 0;

        sd->m_cur_mb.LocalMacroblockInfo->QP = (Ipp8s) sd->m_QuantPrev;
        sd->m_cur_mb.GlobalMacroblockInfo->slice_id = (Ipp16s) sd->m_iSliceNumber;
        memset((void *) sd->m_cur_mb.RefIdxs[1], -1, sizeof(H264DecoderMacroblockRefIdxs));
        memset((void *) sd->m_cur_mb.MVs[1], 0, sizeof(H264DecoderMacroblockMVs));
        memset(sd->m_cur_mb.MVDelta[1], 0, sizeof(H264DecoderMacroblockMVs));
        if (sd->m_pSliceHeader->slice_skip_flag)
        {
            DecodeMacroblock_SkipSlice(sd, baseQP);
            sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP;
            return;
        }
        // decode skip flag
        // sometimes we have decoded the flag,
        // when we were doing decoding of the previous macroblock
        if ((!sd->m_isMBAFF) ||
            (0 == (sd->m_CurMBAddr & 1)) ||
            (!pGetMBSkippedFlag(sd->m_cur_mb.GlobalMacroblockPairInfo)))
            iSkip = sd->DecodeMBSkipFlag_CABAC(MB_SKIP_FLAG_P_SP);
        else
            iSkip = sd->m_iSkipNextMacroblock;

        if (iSkip)
        {
            Ipp32s BaseMB8x8TSFlag = pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo);

            // reset macroblock variables
            memset((void *) sd->m_cur_mb.RefIdxs[0], 0, sizeof(H264DecoderMacroblockRefIdxs));
            memset(sd->m_cur_mb.MVDelta[0], 0, sizeof(H264DecoderMacroblockMVs));
            memset(sd->m_cur_mb.LocalMacroblockInfo->sbdir, 0, 4);

            sd->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_SKIPPED;
            pSetMBSkippedFlag(sd->m_cur_mb.GlobalMacroblockInfo);
            pSetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
            if (!sd->m_isMBAFF)
                pSetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
            sd->m_prev_dquant = 0;

            sd->m_iSkipNextMacroblock = 0;
            if (inCropWindow(sd->m_cur_mb)) {
                pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo,
                    sd->m_pSliceHeader->default_residual_prediction_flag);
            }
            else {
                pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
            }

            if (sd->m_isMBAFF)
            {
                // but we don't know the spatial structure of the current macroblock.
                // we need to decode next macroblock for obtaining the structure.
                if (0 == (sd->m_CurMBAddr & 1))
                {
                    sd->m_CurMBAddr += 1;
                    sd->m_CurMB_Y += 1;
                    // the next macroblock isn't skipped, obtain the structure
                    if (0 == sd->DecodeMBSkipFlag_CABAC(MB_SKIP_FLAG_P_SP))
                        sd->DecodeMBFieldDecodingFlag_CABAC();
                    // the next macroblock is skipped too
                    else
                        sd->m_iSkipNextMacroblock = 1;
                    sd->m_CurMBAddr -= 1;
                    sd->m_CurMB_Y -= 1;
                }
            }

            sd->UpdateNeighbouringAddresses();
            ColorSpecific<color_format, Coeffs, PlaneY, PlaneUV, is_field>::UpdateNeighbouringBlocks(sd);

            sd->ReconstructSkipMotionVectors();

            sd->m_cur_mb.LocalMacroblockInfo->cbp = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0] = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1] = 0;

            pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,0);

            if (sd->m_use_coeff_prediction) 
            {
                if (pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo))
                {
                    sd->m_cur_mb.LocalMacroblockInfo->QP = (Ipp8s)baseQP;

                    if (sd->m_cur_mb.LocalMacroblockInfo->baseMbType > MBTYPE_PCM)
                    {
                        pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,BaseMB8x8TSFlag);
                    }
                }
            }


            sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP;

            return;
        }
        else {
            sd->m_cur_mb.GlobalMacroblockInfo->mbflags.isSkipped = 0;
            sd->m_iSkipNextMacroblock = 0;
        }

        // decode macroblock field flag
        if (sd->m_isMBAFF)
        {
            if (0 == (sd->m_CurMBAddr & 1))
                sd->DecodeMBFieldDecodingFlag_CABAC();
        }
        else
            pSetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);

        umc_h264_mv_resampling_mb(sd, sd->m_CurMB_X, sd->m_CurMB_Y, pGetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo));
        sd->m_cur_mb.LocalMacroblockInfo->baseMbType = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        // update neighbouring addresses
        sd->UpdateNeighbouringAddresses();
        // update neighbouring block positions
        ColorSpecific<color_format, Coeffs, PlaneY, PlaneUV, is_field>::UpdateNeighbouringBlocks(sd);

        if (sd->m_pSliceHeader->nal_ext.svc.no_inter_layer_pred_flag == 0 && inCropWindow(sd->m_cur_mb))
        {
            if (sd->m_pSliceHeader->adaptive_prediction_flag)
            {
                sd->DecodeMBBaseModeFlag_CABAC();
            }
            else
            {
                pSetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo,
                    sd->m_pSliceHeader->default_base_mode_flag);
            }
        }
        else {
            pSetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
        }

        if (pGetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo) == 0)
        {
            // First decode the "macroblock header", e.g., macroblock type,
            // intra-coding types.
            sd->DecodeMBTypePSlice_CABAC();

            // decode macroblock having P type
            if (MBTYPE_PCM < sd->m_cur_mb.GlobalMacroblockInfo->mbtype)
            {
                DecodeMacroblock_P_CABAC(sd);
            }
            else
            {
                memset((void *) sd->m_cur_mb.RefIdxs[0], -1, sizeof(H264DecoderMacroblockRefIdxs));
                memset((void *) sd->m_cur_mb.MVs[0], 0, sizeof(H264DecoderMacroblockMVs));
                memset(sd->m_cur_mb.MVDelta[0], 0, sizeof(H264DecoderMacroblockMVs));

                // decode macroblock having I type
                if (MBTYPE_PCM > sd->m_cur_mb.GlobalMacroblockInfo->mbtype)
                    DecodeMacroblock_I_CABAC(sd);
                // macroblock has PCM type
                else {
                    sd->m_cur_mb.LocalMacroblockInfo->IntraTypes.intra_chroma_mode_dec = 0;
                    DecodeMacroblock_PCM(sd);
                }
            }
        }
        else
        {
            sd->m_cur_mb.LocalMacroblockInfo->IntraTypes.intra_chroma_mode_dec = 0;
            DecodeMacroblock_BASE_MODE_CABAC(sd);
        }

        if (sd->m_use_coeff_prediction && !sd->m_cur_mb.LocalMacroblockInfo->cbp) 
        {
            if ((pGetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo) &&
                IS_INTRA_MBTYPE(sd->m_cur_mb.GlobalMacroblockInfo->mbtype)) ||
                (pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo) && 
                IS_INTER_MBTYPE(sd->m_cur_mb.GlobalMacroblockInfo->mbtype)))
            {
                sd->m_cur_mb.LocalMacroblockInfo->QP = (Ipp8s)baseQP;
            }
        }


        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs = sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual = sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
        sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP;

    } // void DecodeMacroblock_PSlice_CABAC(H264SegmentDecoderMultiThreaded *sd)

    void DecodeMacroblock_BSlice_CABAC(H264SegmentDecoderMultiThreaded * sd)
    {
        Ipp8s baseQP = sd->m_cur_mb.LocalMacroblockInfo->QP;
        Ipp32s iSkip;

        sd->m_cur_mb.LocalMacroblockInfo->baseQP = baseQP;
        sd->m_cur_mb.LocalMacroblockInfo->baseMbType = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;


        // reset macroblock info
        sd->m_cur_mb.GlobalMacroblockInfo->slice_id = (Ipp16s) sd->m_iSliceNumber;
        // reset macroblock info
        if (sd->m_pSliceHeader->ref_layer_dq_id < 0)
        {
            memset(sd->m_cur_mb.LocalMacroblockInfo, 0, sizeof(H264DecoderMacroblockLocalInfo));
            memset(sd->m_cur_mb.GlobalMacroblockInfo->sbtype, 0, sizeof(sd->m_cur_mb.GlobalMacroblockInfo->sbtype));
        }

        memset(sd->m_cur_mb.GetNumCoeffs()->numCoeffs, 0, sizeof(H264DecoderMacroblockCoeffsInfo));
        sd->m_cur_mb.GlobalMacroblockInfo->mbflags.isDirect = 0;
        sd->m_cur_mb.LocalMacroblockInfo->QP = (Ipp8s) sd->m_QuantPrev;
        sd->m_cur_mb.GlobalMacroblockInfo->slice_id = (Ipp16s) sd->m_iSliceNumber;

        if (sd->m_pSliceHeader->slice_skip_flag)
        {
            DecodeMacroblock_SkipSlice(sd, baseQP);
            sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP;
            return;
        }
        // decode skip flag
        // sometimes we have decoded the flag,
        // when we were doing decoding of the previous macroblock
        if ((!sd->m_isMBAFF) ||
            (0 == (sd->m_CurMBAddr & 1)) ||
            (!pGetMBSkippedFlag(sd->m_cur_mb.GlobalMacroblockPairInfo)))
            iSkip = sd->DecodeMBSkipFlag_CABAC(MB_SKIP_FLAG_B);
        else
            iSkip = sd->m_iSkipNextMacroblock;

        if (iSkip)
        {
            Ipp32s BaseMB8x8TSFlag = pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo);

            // reset macroblock variables
            memset(sd->m_cur_mb.MVDelta[0], 0, sizeof(H264DecoderMacroblockMVs));
            memset(sd->m_cur_mb.MVDelta[1], 0, sizeof(H264DecoderMacroblockMVs));
            memset(sd->m_cur_mb.LocalMacroblockInfo->sbdir, 0, 4);

            // reset macroblock variables
            sd->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_SKIPPED;
            pSetMBSkippedFlag(sd->m_cur_mb.GlobalMacroblockInfo);
            pSetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
            if (!sd->m_isMBAFF)
                pSetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
            sd->m_prev_dquant = 0;
            if (inCropWindow(sd->m_cur_mb)) {
                pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo,
                    sd->m_pSliceHeader->default_residual_prediction_flag);
            }
            else {
                pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
            }

            sd->m_iSkipNextMacroblock = 0;
            if (sd->m_isMBAFF)
            {
                // but we don't know the spatial structure of the current macroblock.
                // we need to decode next macroblock for obtaining the structure.
                if (0 == (sd->m_CurMBAddr & 1))
                {
                    sd->m_CurMBAddr += 1;
                    sd->m_CurMB_Y += 1;
                    // the next macroblock isn't skipped, obtain the structure
                    if (0 == sd->DecodeMBSkipFlag_CABAC(MB_SKIP_FLAG_B))
                        sd->DecodeMBFieldDecodingFlag_CABAC();
                    // the next macroblock is skipped too
                    else
                        sd->m_iSkipNextMacroblock = 1;
                    sd->m_CurMBAddr -= 1;
                    sd->m_CurMB_Y -= 1;

                    sd->UpdateNeighbouringAddresses();
                }
            }
            else
            {
                if (sd->m_IsUseSpatialDirectMode)
                {
                    sd->UpdateNeighbouringAddresses();
                }
            }

            if (sd->m_IsUseSpatialDirectMode)
            {
                ColorSpecific<color_format, Coeffs, PlaneY, PlaneUV, is_field>::UpdateNeighbouringBlocks(sd);
            }

            sd->DecodeDirectMotionVectors(true);

            sd->m_cur_mb.LocalMacroblockInfo->cbp = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0] = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1] = 0;

            pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,0);

            if (sd->m_use_coeff_prediction) 
            {
                if (pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo))
                {
                    sd->m_cur_mb.LocalMacroblockInfo->QP = (Ipp8s)baseQP;

                    if (sd->m_cur_mb.LocalMacroblockInfo->baseMbType > MBTYPE_PCM)
                    {
                        pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,BaseMB8x8TSFlag);
                    }
                }
            }

            sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP;

            return;
        }
        else {
            sd->m_cur_mb.GlobalMacroblockInfo->mbflags.isSkipped = 0;
            sd->m_iSkipNextMacroblock = 0;
        }

        // decode macroblock field flag
        if (sd->m_isMBAFF)
        {
            if (0 == (sd->m_CurMBAddr & 1))
                sd->DecodeMBFieldDecodingFlag_CABAC();
        }
        else
            pSetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);

        umc_h264_mv_resampling_mb(sd, sd->m_CurMB_X, sd->m_CurMB_Y, pGetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo));
        sd->m_cur_mb.LocalMacroblockInfo->baseMbType = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        sd->UpdateNeighbouringAddresses();
        ColorSpecific<color_format, Coeffs, PlaneY, PlaneUV, is_field>::UpdateNeighbouringBlocks(sd);

        if (sd->m_pSliceHeader->nal_ext.svc.no_inter_layer_pred_flag == 0 && inCropWindow(sd->m_cur_mb))
        {
            if (sd->m_pSliceHeader->adaptive_prediction_flag)
            {
                sd->DecodeMBBaseModeFlag_CABAC();
            }
            else
            {
                pSetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo,
                    sd->m_pSliceHeader->default_base_mode_flag);
            }
        }
        else {
            pSetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
        }

        if (pGetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo) == 0)
        {
            // First decode the "macroblock header", e.g., macroblock type,
            // intra-coding types.
            sd->DecodeMBTypeBSlice_CABAC();

            // decode macroblock having P type
            if (MBTYPE_PCM < sd->m_cur_mb.GlobalMacroblockInfo->mbtype)
            {
                DecodeMacroblock_B_CABAC(sd);
            }
            else
            {
                memset((void *) sd->m_cur_mb.RefIdxs[0], -1, sizeof(H264DecoderMacroblockRefIdxs));
                memset((void *) sd->m_cur_mb.RefIdxs[1], -1, sizeof(H264DecoderMacroblockRefIdxs));
                memset((void *) sd->m_cur_mb.MVs[0], 0, sizeof(H264DecoderMacroblockMVs));
                memset((void *) sd->m_cur_mb.MVs[1], 0, sizeof(H264DecoderMacroblockMVs));
                memset(sd->m_cur_mb.MVDelta[0], 0, sizeof(H264DecoderMacroblockMVs));
                memset(sd->m_cur_mb.MVDelta[1], 0, sizeof(H264DecoderMacroblockMVs));

                // decode macroblock having I type
                if (MBTYPE_PCM > sd->m_cur_mb.GlobalMacroblockInfo->mbtype)
                    DecodeMacroblock_I_CABAC(sd);
                // macroblock has PCM type
                else {
                    sd->m_cur_mb.LocalMacroblockInfo->IntraTypes.intra_chroma_mode_dec = 0;
                    DecodeMacroblock_PCM(sd);
                }
            }
        }
        else
        {
            sd->m_cur_mb.LocalMacroblockInfo->IntraTypes.intra_chroma_mode_dec = 0;
            DecodeMacroblock_BASE_MODE_CABAC(sd);
        }

        if (sd->m_use_coeff_prediction && !sd->m_cur_mb.LocalMacroblockInfo->cbp) 
        {
            if ((pGetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo) &&
                IS_INTRA_MBTYPE(sd->m_cur_mb.GlobalMacroblockInfo->mbtype)) ||
                (pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo) && 
                IS_INTER_MBTYPE(sd->m_cur_mb.GlobalMacroblockInfo->mbtype)))
            {
                sd->m_cur_mb.LocalMacroblockInfo->QP = (Ipp8s)baseQP;
            }
        }


        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs = sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual = sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
        sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP;

    } // void DecodeMacroblock_BSlice_CABAC(H264SegmentDecoderMultiThreaded * sd)

    void DecodeMacroblock_ISlice_CAVLC(H264SegmentDecoderMultiThreaded *sd)
    {
        Ipp8s baseQP = sd->m_cur_mb.LocalMacroblockInfo->QP;

        sd->m_cur_mb.LocalMacroblockInfo->baseQP = baseQP;
        sd->m_cur_mb.LocalMacroblockInfo->baseMbType = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        if (sd->m_pSliceHeader->ref_layer_dq_id < 0)
        {
            memset(sd->m_cur_mb.LocalMacroblockInfo, 0, sizeof(H264DecoderMacroblockLocalInfo));
            memset(sd->m_cur_mb.GlobalMacroblockInfo->sbtype, 0, sizeof(sd->m_cur_mb.GlobalMacroblockInfo->sbtype));
        }

        memset(sd->m_cur_mb.GetNumCoeffs()->numCoeffs, 0, sizeof(H264DecoderMacroblockCoeffsInfo));

        sd->m_cur_mb.LocalMacroblockInfo->QP = (Ipp8s) sd->m_QuantPrev;
        sd->m_cur_mb.GlobalMacroblockInfo->slice_id = (Ipp16s) sd->m_iSliceNumber;
        memset((void *) sd->m_cur_mb.RefIdxs[0], -1, sizeof(H264DecoderMacroblockRefIdxs));
        memset((void *) sd->m_cur_mb.RefIdxs[1], -1, sizeof(H264DecoderMacroblockRefIdxs));

        if (sd->m_pSliceHeader->slice_skip_flag)
        {
            DecodeMacroblock_SkipSlice(sd, baseQP);
            sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP;
            return;
        }

        // decode macroblock field flag
        if (sd->m_isMBAFF)
        {
            if (0 == (sd->m_CurMBAddr & 1))
            {
                sd->DecodeMBFieldDecodingFlag_CAVLC();
            }
        }
        else
        {
            pSetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
        }

        umc_h264_mv_resampling_mb(sd, sd->m_CurMB_X, sd->m_CurMB_Y, pGetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo));
        sd->m_cur_mb.LocalMacroblockInfo->baseMbType = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        // update neighbouring addresses
        sd->UpdateNeighbouringAddresses();
        // update neighbouring block positions
        ColorSpecific<color_format, Coeffs, PlaneY, PlaneUV, is_field>::UpdateNeighbouringBlocks(sd);

        if (sd->m_pSliceHeader->nal_ext.svc.no_inter_layer_pred_flag == 0 && inCropWindow(sd->m_cur_mb))
        {
            if (sd->m_pSliceHeader->adaptive_prediction_flag)
            {
                sd->m_cur_mb.GlobalMacroblockInfo->mbflags.isBaseMode =
                    (Ipp8u)sd->m_pBitStream->Get1Bit();
            }
            else
            {
                pSetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo,
                                   sd->m_pSliceHeader->default_base_mode_flag);
            }
        }
        else {
            pSetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
        }

        if (pGetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo) == 0)
        {
            // First decode the "macroblock header", e.g., macroblock type,
            // intra-coding types.
            sd->DecodeMBTypeISlice_CAVLC();

            // decode macroblock having I type
            if (MBTYPE_PCM != sd->m_cur_mb.GlobalMacroblockInfo->mbtype)
                DecodeMacroblock_I_CAVLC(sd);
            // macroblock has PCM type
            else
                DecodeMacroblock_PCM(sd);
        }
        else
        {
            DecodeMacroblock_BASE_MODE_CAVLC(sd);

            if (sd->m_use_coeff_prediction && !sd->m_cur_mb.LocalMacroblockInfo->cbp) 
            {
                sd->m_cur_mb.LocalMacroblockInfo->QP = (Ipp8s)baseQP;
            }
        }


        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs = sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual = sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
        sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP;

    } // void DecodeMacroblock_ISlice_CAVLC(H264SegmentDecoderMultiThreaded *sd)

    void DecodeMacroblock_PSlice_CAVLC(H264SegmentDecoderMultiThreaded *sd)
    {
        sd->m_cur_mb.GlobalMacroblockInfo->slice_id = (Ipp16s) sd->m_iSliceNumber;
        Ipp8s baseQP = sd->m_cur_mb.LocalMacroblockInfo->QP;

        sd->m_cur_mb.LocalMacroblockInfo->baseQP = baseQP;
        sd->m_cur_mb.LocalMacroblockInfo->baseMbType = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        // reset macroblock info
        if (sd->m_pSliceHeader->ref_layer_dq_id < 0)
        {
            memset(sd->m_cur_mb.LocalMacroblockInfo, 0, sizeof(H264DecoderMacroblockLocalInfo));
            memset(sd->m_cur_mb.GlobalMacroblockInfo->sbtype, 0, sizeof(sd->m_cur_mb.GlobalMacroblockInfo->sbtype));
        }

        memset(sd->m_cur_mb.GetNumCoeffs()->numCoeffs, 0, sizeof(H264DecoderMacroblockCoeffsInfo));

        sd->m_cur_mb.GlobalMacroblockInfo->mbflags.isDirect = 0;
        sd->m_cur_mb.LocalMacroblockInfo->QP = (Ipp8s) sd->m_QuantPrev;
        sd->m_cur_mb.GlobalMacroblockInfo->slice_id = (Ipp16s) sd->m_iSliceNumber;
        memset((void *) sd->m_cur_mb.RefIdxs[1], -1, sizeof(H264DecoderMacroblockRefIdxs));
        memset((void *) sd->m_cur_mb.MVs[1], 0, sizeof(H264DecoderMacroblockMVs));
        memset(sd->m_cur_mb.MVDelta[1], 0, sizeof(H264DecoderMacroblockMVs));

        if (sd->m_pSliceHeader->slice_skip_flag)
        {
            DecodeMacroblock_SkipSlice(sd, baseQP);
            sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP;
            return;
        }

        // reset macroblock info
        if (0 == sd->m_MBSkipCount)
            sd->m_MBSkipCount = sd->DecodeMBSkipRun_CAVLC();
        else
            sd->m_MBSkipCount -= 1;

        if (0 < sd->m_MBSkipCount)
        {
            Ipp32s BaseMB8x8TSFlag = pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo);

            Ipp8u *pNumCoeffsArray = sd->m_cur_mb.GetNumCoeffs()->numCoeffs;
            memset(pNumCoeffsArray, 0, sizeof(H264DecoderMacroblockCoeffsInfo));

            // reset macroblock variables
            memset((void *) sd->m_cur_mb.RefIdxs[0], 0, sizeof(H264DecoderMacroblockRefIdxs));
            memset(sd->m_cur_mb.LocalMacroblockInfo->sbdir, 0, 4);
            sd->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_SKIPPED;
            pSetMBSkippedFlag(sd->m_cur_mb.GlobalMacroblockInfo);
            pSetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
            sd->m_cur_mb.LocalMacroblockInfo->cbp = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0] = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1] = 0;
            sd->m_prev_dquant = 0;

            if (inCropWindow(sd->m_cur_mb)) {
                pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo,
                    sd->m_pSliceHeader->default_residual_prediction_flag);
            }
            else {
                pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
            }

            if ((sd->m_isMBAFF) &&
                (0 == (sd->m_CurMBAddr & 1)))
            {
                // but we don't know the spatial structure of the current macroblock.
                // we need to decode next macroblock for obtaining the structure.
                if (1 == sd->m_MBSkipCount)
                {
                    sd->m_CurMBAddr += 1;
                    sd->m_CurMB_Y += 1;
                    // the next macroblock isn't skipped, obtain the structure
                    sd->DecodeMBFieldDecodingFlag_CAVLC();
                    // the next macroblock is skipped too
                    sd->m_CurMBAddr -= 1;
                    sd->m_CurMB_Y -= 1;
                }
                else
                    sd->DecodeMBFieldDecodingFlag();
            }

            sd->UpdateNeighbouringAddresses();
            ColorSpecific<color_format, Coeffs, PlaneY, PlaneUV, is_field>::UpdateNeighbouringBlocks(sd);

            sd->ReconstructSkipMotionVectors();

            pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,0);

            if (sd->m_use_coeff_prediction) 
            {
                if (pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo))
                {
                    sd->m_cur_mb.LocalMacroblockInfo->QP = (Ipp8s)baseQP;

                    if (sd->m_cur_mb.LocalMacroblockInfo->baseMbType > MBTYPE_PCM)
                    {
                        pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,BaseMB8x8TSFlag);
                    }
                }
            }

            sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP;

            return;
        } else {
            sd->m_cur_mb.GlobalMacroblockInfo->mbflags.isSkipped = 0;
        }

        // decode macroblock field flag
        if (sd->m_isMBAFF)
        {
            if (0 == (sd->m_CurMBAddr & 1))
                sd->DecodeMBFieldDecodingFlag_CAVLC();
        }
        else
            pSetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);

        umc_h264_mv_resampling_mb(sd, sd->m_CurMB_X, sd->m_CurMB_Y, pGetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo));
        sd->m_cur_mb.LocalMacroblockInfo->baseMbType = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        sd->UpdateNeighbouringAddresses();
        ColorSpecific<color_format, Coeffs, PlaneY, PlaneUV, is_field>::UpdateNeighbouringBlocks(sd);

        if (sd->m_pSliceHeader->nal_ext.svc.no_inter_layer_pred_flag == 0 && inCropWindow(sd->m_cur_mb))
        {
            if (sd->m_pSliceHeader->adaptive_prediction_flag)
            {
                sd->m_cur_mb.GlobalMacroblockInfo->mbflags.isBaseMode =
                    (Ipp8u)sd->m_pBitStream->Get1Bit();
            }
            else
            {
                pSetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo,
                    sd->m_pSliceHeader->default_base_mode_flag);
            }
        }
        else {
            pSetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
        }

        if (pGetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo) == 0)
        {

            // First decode the "macroblock header", e.g., macroblock type,
            // intra-coding types.
            sd->DecodeMBTypePSlice_CAVLC();

            // decode macroblock having P type
            if (MBTYPE_PCM < sd->m_cur_mb.GlobalMacroblockInfo->mbtype)
            {
                DecodeMacroblock_P_CAVLC(sd);
            }
            else
            {
                memset((void *) sd->m_cur_mb.RefIdxs[0], -1, sizeof(H264DecoderMacroblockRefIdxs));
                memset((void *) sd->m_cur_mb.MVs[0], 0, sizeof(H264DecoderMacroblockMVs));
                memset(sd->m_cur_mb.MVDelta[0], 0, sizeof(H264DecoderMacroblockMVs));

                // decode macroblock having I type
                if (MBTYPE_PCM > sd->m_cur_mb.GlobalMacroblockInfo->mbtype)
                    DecodeMacroblock_I_CAVLC(sd);
                // macroblock has PCM type
                else
                    DecodeMacroblock_PCM(sd);
            }
        }
        else
        {
            DecodeMacroblock_BASE_MODE_CAVLC(sd);
        }

        if (sd->m_use_coeff_prediction && !sd->m_cur_mb.LocalMacroblockInfo->cbp) 
        {
            if ((pGetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo) &&
                IS_INTRA_MBTYPE(sd->m_cur_mb.GlobalMacroblockInfo->mbtype)) ||
                (pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo) && 
                IS_INTER_MBTYPE(sd->m_cur_mb.GlobalMacroblockInfo->mbtype)))
            {
                sd->m_cur_mb.LocalMacroblockInfo->QP = (Ipp8s)baseQP;
            }
        }

        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs = sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual = sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
        sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP;

    } // void DecodeMacroblock_PSlice_CAVLC(H264SegmentDecoderMultiThreaded *sd)

    void DecodeMacroblock_BSlice_CAVLC(H264SegmentDecoderMultiThreaded *sd)
    {
        // reset macroblock info
        Ipp8s baseQP = sd->m_cur_mb.LocalMacroblockInfo->QP;
        sd->m_cur_mb.GlobalMacroblockInfo->slice_id = (Ipp16s) sd->m_iSliceNumber;

        sd->m_cur_mb.LocalMacroblockInfo->baseQP = baseQP;
        sd->m_cur_mb.LocalMacroblockInfo->baseMbType = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        // reset macroblock info
        if (sd->m_pSliceHeader->ref_layer_dq_id < 0)
        {
            memset(sd->m_cur_mb.LocalMacroblockInfo, 0, sizeof(H264DecoderMacroblockLocalInfo));
            memset(sd->m_cur_mb.GlobalMacroblockInfo->sbtype, 0, sizeof(sd->m_cur_mb.GlobalMacroblockInfo->sbtype));
        }

        memset(sd->m_cur_mb.GetNumCoeffs()->numCoeffs, 0, sizeof(H264DecoderMacroblockCoeffsInfo));

        sd->m_cur_mb.GlobalMacroblockInfo->mbflags.isDirect = 0;
        sd->m_cur_mb.LocalMacroblockInfo->QP = (Ipp8s) sd->m_QuantPrev;
        sd->m_cur_mb.GlobalMacroblockInfo->slice_id = (Ipp16s) sd->m_iSliceNumber;

        if (sd->m_pSliceHeader->slice_skip_flag)
        {
            DecodeMacroblock_SkipSlice(sd, baseQP);
            sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP;
            return;
        }

        if (0 == sd->m_MBSkipCount)
            sd->m_MBSkipCount = sd->DecodeMBSkipRun_CAVLC();
        else
            sd->m_MBSkipCount -= 1;

        if (0 < sd->m_MBSkipCount)
        {
            Ipp32s BaseMB8x8TSFlag = pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo);
            memset(sd->m_cur_mb.LocalMacroblockInfo->sbdir, 0, 4);

            // reset macroblock variables
            sd->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_SKIPPED;
            pSetMBSkippedFlag(sd->m_cur_mb.GlobalMacroblockInfo);
            pSetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
            sd->m_cur_mb.LocalMacroblockInfo->cbp = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0] = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1] = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual = 0;
            sd->m_prev_dquant = 0;

            if (inCropWindow(sd->m_cur_mb)) {
                pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo,
                    sd->m_pSliceHeader->default_residual_prediction_flag);
            }
            else {
                pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
            }


            Ipp8u *pNumCoeffsArray = sd->m_cur_mb.GetNumCoeffs()->numCoeffs;
            memset(pNumCoeffsArray, 0, sizeof(H264DecoderMacroblockCoeffsInfo));
            if ((sd->m_isMBAFF) &&
                (0 == (sd->m_CurMBAddr & 1)))
            {
                // but we don't know the spatial structure of the current macroblock.
                // we need to decode next macroblock for obtaining the structure.
                if (1 == sd->m_MBSkipCount)
                {
                    sd->m_CurMBAddr += 1;
                    sd->m_CurMB_Y += 1;
                    // the next macroblock isn't skipped, obtain the structure
                    sd->DecodeMBFieldDecodingFlag_CAVLC();
                    // the next macroblock is skipped too
                    sd->m_CurMBAddr -= 1;
                    sd->m_CurMB_Y -= 1;
                }
                else
                    sd->DecodeMBFieldDecodingFlag();

                sd->UpdateNeighbouringAddresses();
            }
            else
            {
                if (sd->m_IsUseSpatialDirectMode)
                {
                    sd->UpdateNeighbouringAddresses();
                }
            }

            if (sd->m_IsUseSpatialDirectMode)
            {
                ColorSpecific<color_format, Coeffs, PlaneY, PlaneUV, is_field>::UpdateNeighbouringBlocks(sd);
            }

            sd->DecodeDirectMotionVectors(true);

            pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,0);

            if (sd->m_use_coeff_prediction) 
            {
                if (pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo))
                {
                    sd->m_cur_mb.LocalMacroblockInfo->QP = (Ipp8s)baseQP;

                    if (sd->m_cur_mb.LocalMacroblockInfo->baseMbType > MBTYPE_PCM)
                    {
                        pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,BaseMB8x8TSFlag);
                    }
                }
            }

            sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP;

            return;
        } else {
            sd->m_cur_mb.GlobalMacroblockInfo->mbflags.isSkipped = 0;
        }

        // decode macroblock field flag
        if (sd->m_isMBAFF)
        {
            if (0 == (sd->m_CurMBAddr & 1))
                sd->DecodeMBFieldDecodingFlag_CAVLC();
        }
        else
            pSetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);

        umc_h264_mv_resampling_mb(sd, sd->m_CurMB_X, sd->m_CurMB_Y, pGetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo));
        sd->m_cur_mb.LocalMacroblockInfo->baseMbType = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        sd->UpdateNeighbouringAddresses();
        ColorSpecific<color_format, Coeffs, PlaneY, PlaneUV, is_field>::UpdateNeighbouringBlocks(sd);

        if (sd->m_pSliceHeader->nal_ext.svc.no_inter_layer_pred_flag == 0 && inCropWindow(sd->m_cur_mb))
        {
            if (sd->m_pSliceHeader->adaptive_prediction_flag)
            {
                sd->m_cur_mb.GlobalMacroblockInfo->mbflags.isBaseMode =
                    (Ipp8u)sd->m_pBitStream->Get1Bit();
            }
            else
            {
                pSetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo,
                    sd->m_pSliceHeader->default_base_mode_flag);
            }
        }
        else {
            pSetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
        }

        if (pGetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo) == 0)
        {

            // First decode the "macroblock header", e.g., macroblock type,
            // intra-coding types.
            sd->DecodeMBTypeBSlice_CAVLC();

            // decode macroblock having P type
            if (MBTYPE_PCM < sd->m_cur_mb.GlobalMacroblockInfo->mbtype)
            {
                DecodeMacroblock_B_CAVLC(sd);
            }
            else
            {
                memset((void *) sd->m_cur_mb.RefIdxs[0], -1, sizeof(H264DecoderMacroblockRefIdxs));
                memset((void *) sd->m_cur_mb.RefIdxs[1], -1, sizeof(H264DecoderMacroblockRefIdxs));
                memset((void *) sd->m_cur_mb.MVs[0], 0, sizeof(H264DecoderMacroblockMVs));
                memset((void *) sd->m_cur_mb.MVs[1], 0, sizeof(H264DecoderMacroblockMVs));
                memset(sd->m_cur_mb.MVDelta[0], 0, sizeof(H264DecoderMacroblockMVs));
                memset(sd->m_cur_mb.MVDelta[1], 0, sizeof(H264DecoderMacroblockMVs));

                // decode macroblock having I type
                if (MBTYPE_PCM > sd->m_cur_mb.GlobalMacroblockInfo->mbtype)
                    DecodeMacroblock_I_CAVLC(sd);
                // macroblock has PCM type
                else
                    DecodeMacroblock_PCM(sd);
            }
        }
        else
        {
            DecodeMacroblock_BASE_MODE_CAVLC(sd);
        }

        if (sd->m_use_coeff_prediction && !sd->m_cur_mb.LocalMacroblockInfo->cbp) 
        {
            if ((pGetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo) &&
                IS_INTRA_MBTYPE(sd->m_cur_mb.GlobalMacroblockInfo->mbtype)) ||
                (pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo) && 
                IS_INTER_MBTYPE(sd->m_cur_mb.GlobalMacroblockInfo->mbtype)))
            {
                sd->m_cur_mb.LocalMacroblockInfo->QP = (Ipp8s)baseQP;
            }
        }


        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs = sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual = sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
        sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP;

    } // void DecodeMacroblock_BSlice_CAVLC(H264SegmentDecoderMultiThreaded *sd)

    void DecodeMacroblock_SkipSlice(H264SegmentDecoderMultiThreaded *sd,
                                    Ipp32s baseQP)
    {
        baseQP = baseQP;

        sd->m_cur_mb.LocalMacroblockInfo->cbp = 0;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma = 0;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs = 0;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual = 0;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0] = 0;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1] = 0;
        sd->m_prev_dquant = 0;

        pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo, 1);
        pSetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo, 1);
        if (!sd->m_isMBAFF)
            pSetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);

        umc_h264_mv_resampling_mb(sd, sd->m_CurMB_X, sd->m_CurMB_Y, pGetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo));
        sd->m_cur_mb.LocalMacroblockInfo->baseMbType = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        sd->m_cur_mb.GlobalMacroblockInfo->mbflags.isSkipped = 0;

        if (!sd->m_use_coeff_prediction)
        {
            pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,0);
        }

        sd->m_refinementFlag = 0;

        if (sd->m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTRA_BL)
        {
            sd->m_refinementFlag = 1;
        }
        else if (sd->m_cur_mb.GlobalMacroblockInfo->mbtype <= MBTYPE_PCM)
        {
            sd->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTRA_BL;
        }

        if (sd->m_use_coeff_prediction)
        {
            sd->m_cur_mb.LocalMacroblockInfo->QP = (Ipp8s)baseQP;
        }

    } // void DecodeMacroblock_SkipSlice(H264SegmentDecoderMultiThreaded *sd)

    void DecodeMacroblock_I_CABAC(H264SegmentDecoderMultiThreaded *sd)
    {
        bool noSubMbPartSizeLessThan8x8Flag;

        // Reset buffer pointers to start
        // This works only as long as "batch size" for VLD and reconstruct
        // is the same. When/if want to make them different, need to change this.
        IntraType *pMBIntraTypes = sd->m_pMBIntraTypes + sd->m_CurMBAddr*NUM_INTRA_TYPE_ELEMENTS;

        // First decode the "macroblock header", e.g., macroblock type,
        // intra-coding types, motion vectors and CBP.

        // Get MB type, possibly change MBSKipCount to non-zero
        if (is_high_profile)
            noSubMbPartSizeLessThan8x8Flag = false;

        Ipp8u mbtype = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,0);
        pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);

        if (mbtype == MBTYPE_INTRA)
        {
            Ipp8u transform_size_8x8_mode_flag = 0;

            if ((is_high_profile) &&
                (sd->m_pPicParamSet->transform_8x8_mode_flag))
            {
                Ipp32s left_inc = sd->m_cur_mb.CurrentBlockNeighbours.mbs_left[0].mb_num>=0?
                    GetMB8x8TSDecFlag(sd->m_gmbinfo->mbs[sd->m_cur_mb.CurrentBlockNeighbours.mbs_left[0].mb_num]):0;
                Ipp32s top_inc = sd->m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num>=0?
                    GetMB8x8TSDecFlag(sd->m_gmbinfo->mbs[sd->m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num]):0;
                Ipp32u ctxIdxInc = top_inc + left_inc;
                transform_size_8x8_mode_flag  = (Ipp8u) sd->m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffset[TRANSFORM_SIZE_8X8_FLAG] + ctxIdxInc);
                pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,transform_size_8x8_mode_flag);
                pSetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo,transform_size_8x8_mode_flag);
            }

            if (transform_size_8x8_mode_flag)
                sd->DecodeIntraTypes8x8_CABAC(pMBIntraTypes, sd->m_IsUseConstrainedIntra);
            else
                sd->DecodeIntraTypes4x4_CABAC(pMBIntraTypes, sd->m_IsUseConstrainedIntra);
        }

        // decode chroma intra prediction mode
        if (color_format)
            sd->DecodeIntraPredChromaMode_CABAC();

        sd->DecodeEdgeType();

        // decode CBP
        if (mbtype != MBTYPE_INTRA_16x16)
        {
            sd->m_cur_mb.LocalMacroblockInfo->cbp = (Ipp8u) sd->DecodeCBP_CABAC(color_format);
        }

        // decode delta QP
        if ((sd->m_cur_mb.LocalMacroblockInfo->cbp || mbtype == MBTYPE_INTRA_16x16) && (sd->m_pSliceHeader->scan_idx_end >= sd->m_pSliceHeader->scan_idx_start))
        {
            // decode delta for quant value
            {
                sd->DecodeMBQPDelta_CABAC();
                sd->m_QuantPrev = sd->m_cur_mb.LocalMacroblockInfo->QP;
            }

            sd->m_pBitStream->SetIdx(sd->m_pSliceHeader->scan_idx_start,
                                     sd->m_pSliceHeader->scan_idx_end);

            // Now, decode the coefficients
            if (MBTYPE_INTRA_16x16 != mbtype)
            {
                if (is_high_profile && pGetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo))
                    this->DecodeCoefficients8x8_CABAC(sd);
                else
                    this->DecodeCoefficients4x4_CABAC(sd);
            }
            else
                this->DecodeCoefficients16x16_CABAC(sd);
        }

        sd->m_cur_mb.LocalMacroblockInfo->IntraTypes.intra_chroma_mode = sd->m_cur_mb.LocalMacroblockInfo->IntraTypes.intra_chroma_mode_dec;
    } // void DecodeMacroblock_I_CABAC(H264SegmentDecoderMultiThreaded *sd)

    void DecodeMacroblock_BASE_MODE_CABAC(H264SegmentDecoderMultiThreaded *sd)
    {
        bool noSubMbPartSizeLessThan8x8Flag;
        Ipp32s BaseMB8x8TSFlag = pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo);

        if (is_high_profile)
            noSubMbPartSizeLessThan8x8Flag = false;

        pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,0);

        if ((sd->m_pSliceHeader->slice_type != INTRASLICE) &&
            (sd->m_pSliceHeader->slice_type != S_INTRASLICE)
            && inCropWindow(sd->m_cur_mb)) {
            if (sd->m_pSliceHeader->adaptive_residual_prediction_flag) {
                sd->DecodeMBResidualPredictionFlag_CABAC();
            } else {
                pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo,
                    sd->m_pSliceHeader->default_residual_prediction_flag);
            }
        }
        else {
            pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
        }

        sd->m_cur_mb.LocalMacroblockInfo->cbp = (Ipp8u) sd->DecodeCBP_CABAC(color_format);

        if ((is_high_profile) &&
            (sd->m_cur_mb.LocalMacroblockInfo->cbp&15) &&
            (sd->m_pPicParamSet->transform_8x8_mode_flag))
        {
            Ipp32s left_inc = sd->m_cur_mb.CurrentBlockNeighbours.mbs_left[0].mb_num>=0?
                GetMB8x8TSDecFlag(sd->m_gmbinfo->mbs[sd->m_cur_mb.CurrentBlockNeighbours.mbs_left[0].mb_num]):0;
            Ipp32s top_inc = sd->m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num>=0?
                GetMB8x8TSDecFlag(sd->m_gmbinfo->mbs[sd->m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num]):0;
            Ipp32u ctxIdxInc = top_inc + left_inc;
            Ipp8u transform_size_8x8_mode_flag  = (Ipp8u) sd->m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffset[TRANSFORM_SIZE_8X8_FLAG] + ctxIdxInc);
            pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,transform_size_8x8_mode_flag);
            pSetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo,transform_size_8x8_mode_flag);
        } else if ((sd->m_use_coeff_prediction) && ((sd->m_cur_mb.LocalMacroblockInfo->cbp & 15) == 0))
        {
            if (IS_INTRA_MBTYPE(sd->m_cur_mb.GlobalMacroblockInfo->mbtype))
            {
                pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,BaseMB8x8TSFlag);
            }
            else if (pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo))
            {
                pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,BaseMB8x8TSFlag);
            }
        }

        if (sd->m_cur_mb.LocalMacroblockInfo->cbp)
        {
            // decode delta for quant value
            if (1/*!sd->m_pBitStream->NextBit()*/)
            {
                sd->DecodeMBQPDelta_CABAC();
                sd->m_QuantPrev = sd->m_cur_mb.LocalMacroblockInfo->QP;
            }
        }


        if (sd->m_cur_mb.LocalMacroblockInfo->cbp)
        {

            sd->m_pBitStream->SetIdx(sd->m_pSliceHeader->scan_idx_start,
                                     sd->m_pSliceHeader->scan_idx_end);

            if (is_high_profile && pGetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo))
                this->DecodeCoefficients8x8_CABAC(sd);
            else
                this->DecodeCoefficients4x4_CABAC(sd);
        }

        sd->m_refinementFlag = 0;

        if (sd->m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTRA_BL)
        {
            sd->m_refinementFlag = 1;
        }
        else if (sd->m_cur_mb.GlobalMacroblockInfo->mbtype <= MBTYPE_PCM)
        {
            if (!sd->m_pSliceHeader->tcoeff_level_prediction_flag || sd->m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_PCM)
                sd->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTRA_BL;
            else
            {
                sd->m_cur_mb.GlobalMacroblockInfo->mbtype = (Ipp8s)sd->m_cur_mb.LocalMacroblockInfo->baseMbType;
                sd->DecodeEdgeType();
            }
        }
    } // void DecodeMacroblock_BASE_MODE_CABAC(H264SegmentDecoderMultiThreaded *sd)

    void DecodeMacroblock_P_CABAC(H264SegmentDecoderMultiThreaded *sd)
    {
        bool noSubMbPartSizeLessThan8x8Flag = true;
        Ipp32s BaseMB8x8TSFlag = pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo);

        if (is_high_profile)
            noSubMbPartSizeLessThan8x8Flag = false;

        Ipp8u mbtype = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,0);

        if (is_high_profile)
        {
            if (mbtype==MBTYPE_INTER_8x8 || mbtype==MBTYPE_INTER_8x8_REF0)
            {
                Ipp32s sum_partnum =
                    SbPartNumMinus1[sd->m_IsUseDirect8x8Inference][sd->m_cur_mb.GlobalMacroblockInfo->sbtype[0]]+
                    SbPartNumMinus1[sd->m_IsUseDirect8x8Inference][sd->m_cur_mb.GlobalMacroblockInfo->sbtype[1]]+
                    SbPartNumMinus1[sd->m_IsUseDirect8x8Inference][sd->m_cur_mb.GlobalMacroblockInfo->sbtype[2]]+
                    SbPartNumMinus1[sd->m_IsUseDirect8x8Inference][sd->m_cur_mb.GlobalMacroblockInfo->sbtype[3]];

                if (sum_partnum == 0)
                    noSubMbPartSizeLessThan8x8Flag = true;
            }
            else
                noSubMbPartSizeLessThan8x8Flag = true;
        }

        // Motion Vector Computation
        sd->DecodeMotionVectors_CABAC();

        if ((sd->m_pSliceHeader->slice_type != INTRASLICE) &&
            (sd->m_pSliceHeader->slice_type != S_INTRASLICE)
            && inCropWindow(sd->m_cur_mb)) {
            if (sd->m_pSliceHeader->adaptive_residual_prediction_flag) {
                sd->DecodeMBResidualPredictionFlag_CABAC();
            } else {
                pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo,
                    sd->m_pSliceHeader->default_residual_prediction_flag);
            }
        }
        else {
            pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
        }
        // cbp
        sd->m_cur_mb.LocalMacroblockInfo->cbp = (Ipp8u) sd->DecodeCBP_CABAC(color_format);

        if (0 == sd->m_cur_mb.LocalMacroblockInfo->cbp)
        {
            if  ((sd->m_use_coeff_prediction) && (sd->m_cur_mb.LocalMacroblockInfo->baseMbType > MBTYPE_PCM) &&
                (pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo)))
            {
                pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,BaseMB8x8TSFlag);
            }
        }
        else
        { // delta QP
            if (is_high_profile && noSubMbPartSizeLessThan8x8Flag && sd->m_cur_mb.LocalMacroblockInfo->cbp&15 &&
                sd->m_pPicParamSet->transform_8x8_mode_flag)
            {
                Ipp8u transform_size_8x8_mode_flag = 0;
                Ipp32u ctxIdxInc;
                Ipp32s left_inc = sd->m_cur_mb.CurrentBlockNeighbours.mbs_left[0].mb_num>=0?
                    GetMB8x8TSDecFlag(sd->m_gmbinfo->mbs[sd->m_cur_mb.CurrentBlockNeighbours.mbs_left[0].mb_num]):0;
                Ipp32s top_inc = sd->m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num>=0?
                    GetMB8x8TSDecFlag(sd->m_gmbinfo->mbs[sd->m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num]):0;
                ctxIdxInc = top_inc+left_inc;
                transform_size_8x8_mode_flag  = (Ipp8u) sd->m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffset[TRANSFORM_SIZE_8X8_FLAG] + ctxIdxInc);
                pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,transform_size_8x8_mode_flag);
                pSetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo,transform_size_8x8_mode_flag);
            } else if ((sd->m_use_coeff_prediction) && (sd->m_cur_mb.LocalMacroblockInfo->baseMbType > MBTYPE_PCM) &&
                (pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo)) &&
                (((sd->m_cur_mb.LocalMacroblockInfo->cbp & 15) == 0)/* || !noSubMbPartSizeLessThan8x8Flag*/))
            {
                pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,BaseMB8x8TSFlag);
            }


            // decode delta for quant value
            sd->DecodeMBQPDelta_CABAC();
            sd->m_QuantPrev = sd->m_cur_mb.LocalMacroblockInfo->QP;

            // Now, decode the coefficients

            sd->m_pBitStream->SetIdx(sd->m_pSliceHeader->scan_idx_start,
                                     sd->m_pSliceHeader->scan_idx_end);

            if (is_high_profile && pGetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo))
                this->DecodeCoefficients8x8_CABAC(sd);
            else
                this->DecodeCoefficients4x4_CABAC(sd);
        }
    } // void DecodeMacroblock_P_CABAC(H264SegmentDecoderMultiThreaded *sd)

    void DecodeMacroblock_B_CABAC(H264SegmentDecoderMultiThreaded *sd)
    {
        bool noSubMbPartSizeLessThan8x8Flag = true;
        Ipp32s BaseMB8x8TSFlag = pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo);

        if (is_high_profile)
            noSubMbPartSizeLessThan8x8Flag = false;

        Ipp8u mbtype = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,0);

        if (is_high_profile)
        {
            if (MBTYPE_DIRECT == mbtype) { // PK
                if (sd->m_IsUseDirect8x8Inference)
                    noSubMbPartSizeLessThan8x8Flag = true;
            } else if ((MBTYPE_INTER_8x8 == mbtype)/* || ((MBTYPE_DIRECT == mbtype)) */) {
                Ipp32s sum_partnum =
                    SbPartNumMinus1[sd->m_IsUseDirect8x8Inference][sd->m_cur_mb.GlobalMacroblockInfo->sbtype[0]]+
                    SbPartNumMinus1[sd->m_IsUseDirect8x8Inference][sd->m_cur_mb.GlobalMacroblockInfo->sbtype[1]]+
                    SbPartNumMinus1[sd->m_IsUseDirect8x8Inference][sd->m_cur_mb.GlobalMacroblockInfo->sbtype[2]]+
                    SbPartNumMinus1[sd->m_IsUseDirect8x8Inference][sd->m_cur_mb.GlobalMacroblockInfo->sbtype[3]];

                if (sum_partnum==0)
                    noSubMbPartSizeLessThan8x8Flag = true;
            }
            else
                noSubMbPartSizeLessThan8x8Flag = true;
        }

        if (mbtype != MBTYPE_DIRECT)
        {
            // Motion Vector Computation
            if (mbtype == MBTYPE_INTER_8x8)
            {
                // First, if B slice and MB is 8x8, set the MV for any DIRECT
                // 8x8 partitions. The MV for the 8x8 DIRECT partition need to
                // be properly set before the MV for subsequent 8x8 partitions
                // can be computed, due to prediction. The DIRECT MV are computed
                // by a separate function and do not depend upon block neighbors for
                // predictors, so it is done here first.
                if (sd->m_cur_mb.GlobalMacroblockInfo->sbtype[0] == SBTYPE_DIRECT ||
                    sd->m_cur_mb.GlobalMacroblockInfo->sbtype[1] == SBTYPE_DIRECT ||
                    sd->m_cur_mb.GlobalMacroblockInfo->sbtype[2] == SBTYPE_DIRECT ||
                    sd->m_cur_mb.GlobalMacroblockInfo->sbtype[3] == SBTYPE_DIRECT)
                {
                    sd->DecodeDirectMotionVectors(false);
                }
            }

            sd->DecodeMotionVectors_CABAC();
        }
        else
        {
            memset(sd->m_cur_mb.MVDelta[0], 0, sizeof(H264DecoderMacroblockMVs));
            memset(sd->m_cur_mb.MVDelta[1], 0, sizeof(H264DecoderMacroblockMVs));

            sd->DecodeDirectMotionVectors(true);
        }

        if ((sd->m_pSliceHeader->slice_type != INTRASLICE) &&
            (sd->m_pSliceHeader->slice_type != S_INTRASLICE)
            && inCropWindow(sd->m_cur_mb)) {
            if (sd->m_pSliceHeader->adaptive_residual_prediction_flag) {
                sd->DecodeMBResidualPredictionFlag_CABAC();
            } else {
                pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo,
                    sd->m_pSliceHeader->default_residual_prediction_flag);
            }
        }
        else {
            pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
        }

        // cbp
        sd->m_cur_mb.LocalMacroblockInfo->cbp = (Ipp8u)sd->DecodeCBP_CABAC(color_format);

        if (0 == sd->m_cur_mb.LocalMacroblockInfo->cbp)
        {
            if  ((sd->m_use_coeff_prediction) && (sd->m_cur_mb.LocalMacroblockInfo->baseMbType > MBTYPE_PCM) &&
                (pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo)))
            {
                pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,BaseMB8x8TSFlag);
            }
        }
        else
        { // // delta QP

            if (is_high_profile && noSubMbPartSizeLessThan8x8Flag && sd->m_cur_mb.LocalMacroblockInfo->cbp&15 &&
                sd->m_pPicParamSet->transform_8x8_mode_flag)
            {
                Ipp8u transform_size_8x8_mode_flag = 0;
                Ipp32u ctxIdxInc;
                Ipp32s left_inc = sd->m_cur_mb.CurrentBlockNeighbours.mbs_left[0].mb_num>=0?
                    GetMB8x8TSDecFlag(sd->m_gmbinfo->mbs[sd->m_cur_mb.CurrentBlockNeighbours.mbs_left[0].mb_num]):0;
                Ipp32s top_inc = sd->m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num>=0?
                    GetMB8x8TSDecFlag(sd->m_gmbinfo->mbs[sd->m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num]):0;
                ctxIdxInc = top_inc+left_inc;
                transform_size_8x8_mode_flag  = (Ipp8u) sd->m_pBitStream->DecodeSingleBin_CABAC(ctxIdxOffset[TRANSFORM_SIZE_8X8_FLAG] + ctxIdxInc);
                pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,transform_size_8x8_mode_flag);
                pSetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo,transform_size_8x8_mode_flag);
            } else if ((sd->m_use_coeff_prediction) && (sd->m_cur_mb.LocalMacroblockInfo->baseMbType > MBTYPE_PCM) &&
                (pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo)) &&
                (((sd->m_cur_mb.LocalMacroblockInfo->cbp & 15) == 0) /*|| !noSubMbPartSizeLessThan8x8Flag*/))
            {
                pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,BaseMB8x8TSFlag);
            }

            // decode delta for quant value
            {
                sd->DecodeMBQPDelta_CABAC();
                sd->m_QuantPrev = sd->m_cur_mb.LocalMacroblockInfo->QP;
            }

            // Now, decode the coefficients

            sd->m_pBitStream->SetIdx(sd->m_pSliceHeader->scan_idx_start,
                                     sd->m_pSliceHeader->scan_idx_end);

            if (is_high_profile && pGetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo))
                this->DecodeCoefficients8x8_CABAC(sd);
            else
                this->DecodeCoefficients4x4_CABAC(sd);
        }
    } // void DecodeMacroblock_B_CABAC(H264SegmentDecoderMultiThreaded *sd)

    void DecodeMacroblock_I_CAVLC(H264SegmentDecoderMultiThreaded *sd)
    {
        bool noSubMbPartSizeLessThan8x8Flag;

        // Reset buffer pointers to start
        // This works only as long as "batch size" for VLD and reconstruct
        // is th same. When/if want to make them different, need to change this.
        IntraType * pMBIntraTypes = sd->m_pMBIntraTypes + sd->m_CurMBAddr*NUM_INTRA_TYPE_ELEMENTS;

        // First decode the "macroblock header", e.g., macroblock type,
        // intra-coding types, motion vectors and CBP.

        // Get MB type, possibly change MBSKipCount to non-zero
        if (is_high_profile)
            noSubMbPartSizeLessThan8x8Flag = false;

        Ipp8u mbtype = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,0);
        pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);

        if (mbtype == MBTYPE_INTRA)
        {
            Ipp8u transform_size_8x8_mode_flag = 0;
            if (is_high_profile && sd->m_pPicParamSet->transform_8x8_mode_flag)
            {
                transform_size_8x8_mode_flag  = (Ipp8u) sd->m_pBitStream->Get1Bit();
                pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,transform_size_8x8_mode_flag);
                pSetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo,transform_size_8x8_mode_flag);
            }

            if (transform_size_8x8_mode_flag)
                sd->DecodeIntraTypes8x8_CAVLC(pMBIntraTypes, sd->m_IsUseConstrainedIntra);
            else
                sd->DecodeIntraTypes4x4_CAVLC(pMBIntraTypes, sd->m_IsUseConstrainedIntra);
        }

        if (color_format)
        {
            // Get chroma prediction mode
            sd->m_cur_mb.LocalMacroblockInfo->IntraTypes.intra_chroma_mode = (Ipp8u) sd->m_pBitStream->GetVLCElement(false);
            if (sd->m_cur_mb.LocalMacroblockInfo->IntraTypes.intra_chroma_mode > 3)
                throw h264_exception(UMC_ERR_INVALID_STREAM);
        }

        sd->DecodeEdgeType();

        // cbp
        if (mbtype != MBTYPE_INTRA_16x16)
        {
            sd->m_cur_mb.LocalMacroblockInfo->cbp = (Ipp8u) sd->DecodeCBP_CAVLC(color_format);
        }

        if ((sd->m_cur_mb.LocalMacroblockInfo->cbp || mbtype == MBTYPE_INTRA_16x16) && (sd->m_pSliceHeader->scan_idx_end >= sd->m_pSliceHeader->scan_idx_start))
        {
            Ipp8u transform_size_8x8_mode_flag = 0;
            if (is_high_profile && noSubMbPartSizeLessThan8x8Flag && sd->m_cur_mb.LocalMacroblockInfo->cbp&15
                && sd->m_pPicParamSet->transform_8x8_mode_flag) {
                transform_size_8x8_mode_flag  = (Ipp8u) sd->m_pBitStream->Get1Bit();
                pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo, transform_size_8x8_mode_flag);
                pSetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo, transform_size_8x8_mode_flag);
            }

            // decode delta for quant value
            if (!sd->m_pBitStream->NextBit())
            {
                sd->DecodeMBQPDelta_CAVLC();
                sd->m_QuantPrev = sd->m_cur_mb.LocalMacroblockInfo->QP;
            }

            sd->m_pBitStream->SetIdx(sd->m_pSliceHeader->scan_idx_start,
                                     sd->m_pSliceHeader->scan_idx_end);

            // Now, decode the coefficients
            if (MBTYPE_INTRA_16x16 != mbtype)
            {
                if (is_high_profile && pGetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo))
                    this->DecodeCoefficients8x8_CAVLC(sd);
                else
                    this->DecodeCoefficients4x4_CAVLC(sd);
            }
            else
                this->DecodeCoefficients16x16_CAVLC(sd);
        }
        else
        {
            Ipp8u *pNumCoeffsArray = sd->m_cur_mb.GetNumCoeffs()->numCoeffs;
            memset(pNumCoeffsArray, 0, sizeof(H264DecoderMacroblockCoeffsInfo));
        }
    } // void DecodeMacroblock_I_CAVLC(H264SegmentDecoderMultiThreaded *sd)

    void DecodeMacroblock_BASE_MODE_CAVLC(H264SegmentDecoderMultiThreaded *sd)
    {
        bool noSubMbPartSizeLessThan8x8Flag;
        Ipp32s residual_prediction_flag;
        Ipp32s BaseMB8x8TSFlag = pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo);

        if (is_high_profile)
            noSubMbPartSizeLessThan8x8Flag = false;

        pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,0);

        if ((sd->m_pSliceHeader->slice_type != INTRASLICE) &&
            (sd->m_pSliceHeader->slice_type != S_INTRASLICE)
            && inCropWindow(sd->m_cur_mb)) {
            if (sd->m_pSliceHeader->adaptive_residual_prediction_flag) {
                residual_prediction_flag = (Ipp8u)sd->m_pBitStream->Get1Bit();
                pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo, residual_prediction_flag);
            } else {
                pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo,
                    sd->m_pSliceHeader->default_residual_prediction_flag);
            }
        }
        else {
            pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
        }

        sd->m_cur_mb.LocalMacroblockInfo->cbp = (Ipp8u) sd->DecodeCBP_CAVLC(color_format);

        if (is_high_profile && sd->m_cur_mb.LocalMacroblockInfo->cbp&15 &&
            sd->m_pPicParamSet->transform_8x8_mode_flag)
        {
            Ipp8u transform_size_8x8_mode_flag = 0;
            transform_size_8x8_mode_flag  = (Ipp8u) sd->m_pBitStream->Get1Bit();
            pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo, transform_size_8x8_mode_flag);
            pSetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo, transform_size_8x8_mode_flag);
        } else if ((sd->m_use_coeff_prediction) && ((sd->m_cur_mb.LocalMacroblockInfo->cbp & 15) == 0))
        {
            if (IS_INTRA_MBTYPE(sd->m_cur_mb.GlobalMacroblockInfo->mbtype))
            {
                pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,BaseMB8x8TSFlag);
            }
            else if (pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo))
            {
                pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,BaseMB8x8TSFlag);
            }
        }

        if (sd->m_cur_mb.LocalMacroblockInfo->cbp)
        {
            // decode delta for quant value
            if (!sd->m_pBitStream->NextBit())
            {
                sd->DecodeMBQPDelta_CAVLC();
                sd->m_QuantPrev = sd->m_cur_mb.LocalMacroblockInfo->QP;
            }
        }

        if (sd->m_cur_mb.LocalMacroblockInfo->cbp)
        {
            sd->m_pBitStream->SetIdx(sd->m_pSliceHeader->scan_idx_start,
                                    sd->m_pSliceHeader->scan_idx_end);

            if (is_high_profile && pGetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo))
                this->DecodeCoefficients8x8_CAVLC(sd);
            else
                this->DecodeCoefficients4x4_CAVLC(sd);
        }

        sd->m_refinementFlag = 0;

        if (sd->m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTRA_BL)
        {
            sd->m_refinementFlag = 1;
        }
        else if (sd->m_cur_mb.GlobalMacroblockInfo->mbtype <= MBTYPE_PCM)
        {
            if (!sd->m_pSliceHeader->tcoeff_level_prediction_flag || sd->m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_PCM)
            {
                sd->m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTRA_BL;
            }
            else
            {
                sd->m_cur_mb.GlobalMacroblockInfo->mbtype = (Ipp8s)sd->m_cur_mb.LocalMacroblockInfo->baseMbType;
                sd->DecodeEdgeType();
            }
        }
    } // void DecodeMacroblock_BASE_MODE_CAVLC(H264SegmentDecoderMultiThreaded *sd)

    void DecodeMacroblock_P_CAVLC(H264SegmentDecoderMultiThreaded *sd)
    {
        Ipp32s residual_prediction_flag;
        bool noSubMbPartSizeLessThan8x8Flag = true;
        Ipp32s BaseMB8x8TSFlag = pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo);

        // Get MB type, possibly change MBSKipCount to non-zero
        if (is_high_profile)
            noSubMbPartSizeLessThan8x8Flag = false;

        Ipp8u mbtype = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,0);

        if (is_high_profile)
        {
            if (mbtype==MBTYPE_INTER_8x8 || mbtype==MBTYPE_INTER_8x8_REF0)
            {
                Ipp32s sum_partnum =
                    SbPartNumMinus1[sd->m_IsUseDirect8x8Inference][sd->m_cur_mb.GlobalMacroblockInfo->sbtype[0]]+
                    SbPartNumMinus1[sd->m_IsUseDirect8x8Inference][sd->m_cur_mb.GlobalMacroblockInfo->sbtype[1]]+
                    SbPartNumMinus1[sd->m_IsUseDirect8x8Inference][sd->m_cur_mb.GlobalMacroblockInfo->sbtype[2]]+
                    SbPartNumMinus1[sd->m_IsUseDirect8x8Inference][sd->m_cur_mb.GlobalMacroblockInfo->sbtype[3]];

                if (sum_partnum == 0)
                    noSubMbPartSizeLessThan8x8Flag = true;
            }
            else
                noSubMbPartSizeLessThan8x8Flag = true;
        }

        // MV and Ref Index
        sd->DecodeMotionVectorsPSlice_CAVLC();
        //sd->ReconstructMotionVectors();

        if ((sd->m_pSliceHeader->slice_type != INTRASLICE) &&
            (sd->m_pSliceHeader->slice_type != S_INTRASLICE)
            && inCropWindow(sd->m_cur_mb)) {
            if (sd->m_pSliceHeader->adaptive_residual_prediction_flag) {
                residual_prediction_flag = (Ipp8u)sd->m_pBitStream->Get1Bit();
                pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo, residual_prediction_flag);
            } else {
                pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo,
                    sd->m_pSliceHeader->default_residual_prediction_flag);
            }
        }
        else {
            pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
        }

        // cbp
        sd->m_cur_mb.LocalMacroblockInfo->cbp = (Ipp8u) sd->DecodeCBP_CAVLC(color_format);

        if (0 == sd->m_cur_mb.LocalMacroblockInfo->cbp)
        {
            if  ((sd->m_use_coeff_prediction) && (sd->m_cur_mb.LocalMacroblockInfo->baseMbType > MBTYPE_PCM) &&
                (pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo)))
            {
                pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,BaseMB8x8TSFlag);
            }
            Ipp8u *pNumCoeffsArray = sd->m_cur_mb.GetNumCoeffs()->numCoeffs;
            memset(pNumCoeffsArray, 0, sizeof(H264DecoderMacroblockCoeffsInfo));
        }
        else
        {
            Ipp8u transform_size_8x8_mode_flag = 0;
            if (is_high_profile && noSubMbPartSizeLessThan8x8Flag && sd->m_cur_mb.LocalMacroblockInfo->cbp&15 &&
                sd->m_pPicParamSet->transform_8x8_mode_flag) {
                transform_size_8x8_mode_flag  = (Ipp8u) sd->m_pBitStream->Get1Bit();
                pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo, transform_size_8x8_mode_flag);
                pSetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo, transform_size_8x8_mode_flag);
            } else if ((sd->m_use_coeff_prediction) && (sd->m_cur_mb.LocalMacroblockInfo->baseMbType > MBTYPE_PCM) &&
                (pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo)) &&
                (((sd->m_cur_mb.LocalMacroblockInfo->cbp & 15) == 0) /*|| !noSubMbPartSizeLessThan8x8Flag*/))
            {
                pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,BaseMB8x8TSFlag);
            }

            // check for usual case of zero QP delta
            if (!sd->m_pBitStream->NextBit())
            {
                sd->DecodeMBQPDelta_CAVLC();
                sd->m_QuantPrev = sd->m_cur_mb.LocalMacroblockInfo->QP;
            }

            sd->m_pBitStream->SetIdx(sd->m_pSliceHeader->scan_idx_start,
                                     sd->m_pSliceHeader->scan_idx_end);

            // Now, decode the coefficients
            if (is_high_profile && pGetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo))
                this->DecodeCoefficients8x8_CAVLC(sd);
            else
                this->DecodeCoefficients4x4_CAVLC(sd);
        }
    } // void DecodeMacroblock_P_CAVLC(H264SegmentDecoderMultiThreaded *sd)

    void DecodeMacroblock_B_CAVLC(H264SegmentDecoderMultiThreaded *sd)
    {
        Ipp32s residual_prediction_flag;
        bool noSubMbPartSizeLessThan8x8Flag = true;
        Ipp32s BaseMB8x8TSFlag = pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo);

        // Get MB type, possibly change MBSKipCount to non-zero
        if (is_high_profile)
            noSubMbPartSizeLessThan8x8Flag = false;

        Ipp8u mbtype = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,0);

        if (is_high_profile)
        {
            if (MBTYPE_DIRECT == mbtype) { // PK
                if (sd->m_IsUseDirect8x8Inference)
                    noSubMbPartSizeLessThan8x8Flag = true;
            } else if ((MBTYPE_INTER_8x8 == mbtype)/* || ((MBTYPE_DIRECT == mbtype))*/) {
                Ipp32s sum_partnum =
                    SbPartNumMinus1[sd->m_IsUseDirect8x8Inference][sd->m_cur_mb.GlobalMacroblockInfo->sbtype[0]]+
                    SbPartNumMinus1[sd->m_IsUseDirect8x8Inference][sd->m_cur_mb.GlobalMacroblockInfo->sbtype[1]]+
                    SbPartNumMinus1[sd->m_IsUseDirect8x8Inference][sd->m_cur_mb.GlobalMacroblockInfo->sbtype[2]]+
                    SbPartNumMinus1[sd->m_IsUseDirect8x8Inference][sd->m_cur_mb.GlobalMacroblockInfo->sbtype[3]];

                if (sum_partnum == 0)
                    noSubMbPartSizeLessThan8x8Flag = true;
            }
            else
                noSubMbPartSizeLessThan8x8Flag = true;
        }

        if (mbtype != MBTYPE_DIRECT)
        {
            // Motion Vector Computation

            if (mbtype == MBTYPE_INTER_8x8)
            {
                // First, if B slice and MB is 8x8, set the MV for any DIRECT
                // 8x8 partitions. The MV for the 8x8 DIRECT partition need to
                // be properly set before the MV for subsequent 8x8 partitions
                // can be computed, due to prediction. The DIRECT MV are computed
                // by a separate function and do not depend upon block neighbors for
                // predictors, so it is done here first.
                if (sd->m_cur_mb.GlobalMacroblockInfo->sbtype[0] == SBTYPE_DIRECT ||
                    sd->m_cur_mb.GlobalMacroblockInfo->sbtype[1] == SBTYPE_DIRECT ||
                    sd->m_cur_mb.GlobalMacroblockInfo->sbtype[2] == SBTYPE_DIRECT ||
                    sd->m_cur_mb.GlobalMacroblockInfo->sbtype[3] == SBTYPE_DIRECT)
                {
                    sd->DecodeDirectMotionVectors(false);
                }
            }

            // MV and Ref Index
            sd->DecodeMotionVectors_CAVLC(true);
            //sd->ReconstructMotionVectors();
        }
        else
        {
            sd->DecodeDirectMotionVectors(true);
        }

        if ((sd->m_pSliceHeader->slice_type != INTRASLICE) &&
            (sd->m_pSliceHeader->slice_type != S_INTRASLICE)
            && inCropWindow(sd->m_cur_mb)) {
            if (sd->m_pSliceHeader->adaptive_residual_prediction_flag) {
                residual_prediction_flag = (Ipp8u)sd->m_pBitStream->Get1Bit();
                pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo, residual_prediction_flag);
            } else {
                pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo,
                    sd->m_pSliceHeader->default_residual_prediction_flag);
            }
        }
        else {
            pSetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);
        }

        sd->m_cur_mb.LocalMacroblockInfo->cbp = (Ipp8u) sd->DecodeCBP_CAVLC(color_format);

        if (0 == sd->m_cur_mb.LocalMacroblockInfo->cbp)
        {
            if  ((sd->m_use_coeff_prediction) && (sd->m_cur_mb.LocalMacroblockInfo->baseMbType > MBTYPE_PCM) &&
                (pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo)))
            {
                pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,BaseMB8x8TSFlag);
            }
            Ipp8u *pNumCoeffsArray = sd->m_cur_mb.GetNumCoeffs()->numCoeffs;
            memset(pNumCoeffsArray, 0, sizeof(H264DecoderMacroblockCoeffsInfo));
        }
        else
        {
            Ipp8u transform_size_8x8_mode_flag = 0;
            if (is_high_profile && noSubMbPartSizeLessThan8x8Flag && sd->m_cur_mb.LocalMacroblockInfo->cbp&15 &&
                sd->m_pPicParamSet->transform_8x8_mode_flag) {
                transform_size_8x8_mode_flag  = (Ipp8u) sd->m_pBitStream->Get1Bit();
                pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo, transform_size_8x8_mode_flag);
                pSetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo, transform_size_8x8_mode_flag);
            }
            else if ((sd->m_use_coeff_prediction) && (sd->m_cur_mb.LocalMacroblockInfo->baseMbType > MBTYPE_PCM) &&
                (pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo)) &&
                (((sd->m_cur_mb.LocalMacroblockInfo->cbp & 15) == 0) /*|| !noSubMbPartSizeLessThan8x8Flag*/))
            {
                pSetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo,BaseMB8x8TSFlag);
            }
            // check for usual case of zero QP delta
            if (!sd->m_pBitStream->NextBit())
            {
                sd->DecodeMBQPDelta_CAVLC();
                sd->m_QuantPrev = sd->m_cur_mb.LocalMacroblockInfo->QP;
            }


            sd->m_pBitStream->SetIdx(sd->m_pSliceHeader->scan_idx_start,
                                     sd->m_pSliceHeader->scan_idx_end);

            // Now, decode the coefficients
            if (is_high_profile && pGetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo))
                this->DecodeCoefficients8x8_CAVLC(sd);
            else
                this->DecodeCoefficients4x4_CAVLC(sd);
        }
    } // void DecodeMacroblock_B_CAVLC(H264SegmentDecoderMultiThreaded *sd)

    void DecodeMacroblock_PCM(H264SegmentDecoderMultiThreaded *sd)
    {
        // Reset buffer pointers to start
        // This works only as long as "batch size" for VLD and reconstruct
        // is the same. When/if want to make them different, need to change this.

        if (pGetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo) != 0)
            return;

        this->DecodeCoefficients_PCM(sd);
        // For PCM type MB, num coeffs are set by above call, cbp is
        // set to all blocks coded (for deblock filter), MV are set to zero,
        // QP is unchanged.
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma = D_CBP_LUMA_DC | D_CBP_LUMA_AC;
        if (color_format)
        {
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0] =
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1] = D_CBP_CHROMA_DC | ColorSpecific<color_format, Coeffs, PlaneY, PlaneUV, is_field>::GetChromaAC();
        }

        sd->m_prev_dquant = 0;
    } // void DecodeMacroblock_PCM(H264SegmentDecoderMultiThreaded *sd)
};

template <typename Coeffs, typename PlaneY, typename PlaneUV, Ipp32s color_format, Ipp32s is_field, bool is_high_profile>
class MBNullDecoder //:
    /*public ResidualDecoderCABAC<Coeffs, color_format, is_field>,
    public ResidualDecoderCAVLC<Coeffs, color_format, is_field>,
    public ResidualDecoderPCM<Coeffs, PlaneY, PlaneUV, color_format, is_field>*/
{
public:
    virtual
    ~MBNullDecoder(void)
    {
    } // MBNullDecoder(void)

    void DecodeMacroblock_ISlice_CABAC(H264SegmentDecoderMultiThreaded *)
    {
    } // void DecodeMacroblock_ISlice_CABAC(

    void DecodeMacroblock_PSlice_CABAC(H264SegmentDecoderMultiThreaded *)
    {
    } // void DecodeMacroblock_PSlice_CABAC(

    void DecodeMacroblock_BSlice_CABAC(H264SegmentDecoderMultiThreaded * )
    {
    } // void DecodeMacroblock_BSlice_CABAC(

    void DecodeMacroblock_ISlice_CAVLC(H264SegmentDecoderMultiThreaded *)
    {
    } // void DecodeMacroblock_ISlice_CAVLC(

    void DecodeMacroblock_PSlice_CAVLC(H264SegmentDecoderMultiThreaded *)
    {
    } // void DecodeMacroblock_PSlice_CAVLC(

    void DecodeMacroblock_BSlice_CAVLC(H264SegmentDecoderMultiThreaded *)
    {
    } // void DecodeMacroblock_BSlice_CAVLC(
};

static const Ipp32s tcoeffs_scale_factors[6]  = { 8, 9, 10, 11, 13, 14 };

template <typename Coeffs, typename PlaneY, typename PlaneUV, Ipp32s color_format, Ipp32s is_field, bool is_high_profile, bool nv12_support = true>
class MBReconstructor
{
public:

    enum
    {
        width_chroma_div = nv12_support ? 0 : (color_format < 3),  // for plane
        height_chroma_div = (color_format < 2),  // for plane
    };

    typedef PlaneY * PlanePtrY;
    typedef PlaneUV * PlanePtrUV;
    typedef Coeffs *  CoeffsPtr;

    virtual ~MBReconstructor() {}

    void ScaleTCoeffs_raw(Ipp16s *srcdst, Ipp32s len, Ipp32s scale, Ipp32s shift) {
        Ipp32s i;

        for( i = 0; i < len; i++ )
        {
            srcdst[i] = (Ipp16s)(( ( scale * srcdst[i] ) << shift ) >> 12);
        }
    }

#define COEFF_ADD_S16_I(srcdst, src) srcdst = (Ipp16s)((srcdst) + (src))

    void UpdateTCoeffs_4x4(H264SegmentDecoderMultiThreaded * sd,
        Ipp32s QPLuma,
        Ipp32s QPChromaU,
        Ipp32s QPChromaV,
        Ipp32s addtcoeff_flag)
    {
        Ipp32s blockNum = 1;
        Ipp16s* tmpCoeffPtr0, *tmpTCoeffPtr;
        int i, s, iii;
        int a = 0;

        Ipp32s qp_delta;
        Ipp32s qp_delta_chroma[2];
        Ipp32s scale;
        Ipp32s shift;
        Ipp32s scale_chroma[2], shift_chroma[2];
        Ipp32s ChromaPlane;
        Ipp32s baseQPChromaU, baseQPChromaV;
        Ipp32s QPChromaIndexU, QPChromaIndexV;
        Ipp32s first_mb_in_slice_group;
        H264Slice *pSlice = sd->m_pSlice;

        tmpCoeffPtr0 = (Ipp16s*)sd->m_pCoeffBlocksRead;
        tmpTCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockTCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];

        if (addtcoeff_flag) {
            qp_delta = sd->m_cur_mb.LocalMacroblockInfo->baseQP - QPLuma + 54;
            scale = tcoeffs_scale_factors[ qp_delta % 6 ];
            shift = qp_delta / 6;
            QPChromaIndexU = sd->m_cur_mb.LocalMacroblockInfo->baseQP + sd->m_pPicParamSet->chroma_qp_index_offset[0];

            QPChromaIndexU = IPP_MIN(QPChromaIndexU, (Ipp32s)QP_MAX);
            QPChromaIndexU = IPP_MAX(0, QPChromaIndexU);
            baseQPChromaU = QPtoChromaQP[QPChromaIndexU];

            if (is_high_profile)
            {
                QPChromaIndexV = sd->m_cur_mb.LocalMacroblockInfo->baseQP + sd->m_pPicParamSet->chroma_qp_index_offset[1];
                QPChromaIndexV = IPP_MIN(QPChromaIndexV, (Ipp32s)QP_MAX);
                QPChromaIndexV = IPP_MAX(0, QPChromaIndexV);
                baseQPChromaV = QPtoChromaQP[QPChromaIndexV];
            } else {
                baseQPChromaV = baseQPChromaU;
            }

            qp_delta_chroma[0] =  baseQPChromaU - QPChromaU + 54;
            qp_delta_chroma[1] =  baseQPChromaV - QPChromaV + 54;

            scale_chroma[0] = tcoeffs_scale_factors[ qp_delta_chroma[0] % 6 ];
            shift_chroma[0] = qp_delta_chroma[0] / 6;
            scale_chroma[1] = tcoeffs_scale_factors[ qp_delta_chroma[1] % 6 ];
            shift_chroma[1] = qp_delta_chroma[1] / 6;

            ScaleTCoeffs_raw(tmpTCoeffPtr, 256, scale, shift);
        }

        if (sd->m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTRA_16x16) {
            if (addtcoeff_flag) {
                if (sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma & 1) {
                    for (i = 0; i < 16; i++) {
                           COEFF_ADD_S16_I(tmpTCoeffPtr[block_subblock_mapping[i] * 16], tmpCoeffPtr0[i]);
                     }
                    tmpCoeffPtr0 += 16;
                }
            } else {
                if (sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma & 1) {
                    for (i = 0; i < 16; i++) {
                        tmpTCoeffPtr[block_subblock_mapping[i] * 16] = tmpCoeffPtr0[i];
                    }
                    tmpCoeffPtr0 += 16;
                } else {
                    for (i = 0; i < 16; i++) {
                        tmpTCoeffPtr[block_subblock_mapping[i] * 16] = 0;
                    }
                }
            }
            a = 1;
        }

        sd->cbp4x4_luma_dequant = 0;

        for (iii = 0; iii < 16; iii++)
        {
            if ((sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma & (1 << blockNum))) {
                if (addtcoeff_flag) {
                    ippsAdd_16s_I(tmpCoeffPtr0, tmpTCoeffPtr, 16);
                } else {
                    ippsCopy_16s(tmpCoeffPtr0 + a, tmpTCoeffPtr + a, 16 - a);
                }
                tmpCoeffPtr0 += 16;
            } else if (!addtcoeff_flag) {
                memset(tmpTCoeffPtr + a, 0, (16 - a) * sizeof(Ipp16s));
            }

            s = 0;
            if (sd->m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTRA_16x16
                && *tmpTCoeffPtr) {
                s = 1;
                sd->cbp4x4_luma_dequant |= 1;
            }

            for (i = s; i < 16; i++) {
                if (tmpTCoeffPtr[i]) {
                    sd->cbp4x4_luma_dequant |= (1 << blockNum);
                    break;
                }
            }

            tmpTCoeffPtr += 16;
            blockNum++;
        }

        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs = sd->cbp4x4_luma_dequant;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual =
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs;

        for (ChromaPlane = 0; ChromaPlane < 2; ChromaPlane++) {
            if (addtcoeff_flag) {
                ScaleTCoeffs_raw(tmpTCoeffPtr, 64, scale_chroma[ChromaPlane], shift_chroma[ChromaPlane]);
                if (sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[ChromaPlane] & 1) {
                    for (i = 0; i < 4; i++) {
                        COEFF_ADD_S16_I(tmpTCoeffPtr[i * 16], tmpCoeffPtr0[i]);
                     }
                    tmpCoeffPtr0 += 4;
                }
            } else {
                if (sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[ChromaPlane] & 1) {
                    for (i = 0; i < 4; i++) {
                        tmpTCoeffPtr[i * 16] = tmpCoeffPtr0[i];
                    }
                    tmpCoeffPtr0 += 4;
                } else {
                    for (i = 0; i < 4; i++) {
                        tmpTCoeffPtr[i * 16] = 0;
                    }
                }
            }
            tmpTCoeffPtr += 64;
        }

        tmpTCoeffPtr -= 128;

        for (ChromaPlane = 0; ChromaPlane < 2; ChromaPlane++) {
            a = 1;
            blockNum = 1;
            sd->cbp4x4_chroma_dequant[ChromaPlane] = 0;
            for (iii = 0; iii < 4; iii++)
            {
                if ((sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[ChromaPlane] & (1 << blockNum))) {
                    if (addtcoeff_flag) {
                        ippsAdd_16s_I(tmpCoeffPtr0, tmpTCoeffPtr, 16);
                    } else {
                        ippsCopy_16s(tmpCoeffPtr0 + a, tmpTCoeffPtr + a, 16 - a);
                    }
                    tmpCoeffPtr0 += 16;
                } else if (!addtcoeff_flag) {
                    memset(tmpTCoeffPtr + a, 0, (16 - a) * sizeof(Ipp16s));
                }

                if (*tmpTCoeffPtr) {
                    sd->cbp4x4_chroma_dequant[ChromaPlane] |= 1;
                }

                for (i = 1; i < 16; i++) {
                    if (tmpTCoeffPtr[i]) {
                        sd->cbp4x4_chroma_dequant[ChromaPlane] |= (1 << blockNum);
                        break;
                    }
                }

                tmpTCoeffPtr += 16;
                blockNum++;
            }
        }

        sd->m_pCoeffBlocksRead = tmpCoeffPtr0;

        // fixme: QP_deblock for first_mb_in_slice

        if (sd->m_isSliceGroups)
        {
            first_mb_in_slice_group = sd->m_pSliceHeader->first_mb_in_slice;
        }
        else
        {
            first_mb_in_slice_group = 0;
        }

        if (!sd->m_pSliceHeader->tcoeff_level_prediction_flag ||
            (sd->cbp4x4_luma_dequant != 0) ||
            (sd->cbp4x4_chroma_dequant[0] != 0) ||
            (sd->cbp4x4_chroma_dequant[1] != 0) ||
            (sd->m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTRA_16x16) ||
            (first_mb_in_slice_group == sd->m_CurMBAddr))
        {
            sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP;
        }
        else if (sd->m_pSliceHeader->first_mb_in_slice == sd->m_CurMBAddr) {
            sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_mbinfo.mbs[sd->m_CurMBAddr-1].QP_deblock;
        } else {
            sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = (Ipp8s)pSlice->last_QP_deblock;
        }
    }

    void UpdateTCoeffs_8x8(H264SegmentDecoderMultiThreaded * sd,
        Ipp32s QPLuma,
        Ipp32s QPChromaU,
        Ipp32s QPChromaV,
        Ipp32s addtcoeff_flag) {

        Ipp32s blockNum = 0x1e;
        Ipp16s* tmpCoeffPtr0, *tmpTCoeffPtr;
        int a, i, iii;

        Ipp32s scale = 0;
        Ipp32s shift = 0;
        Ipp32s scale_chroma[2], shift_chroma[2];
        Ipp32s ChromaPlane;
        Ipp32s qp_delta;
        Ipp32s qp_delta_chroma[2];
        Ipp32s baseQPChromaU, baseQPChromaV;
        Ipp32s QPChromaIndexU, QPChromaIndexV;
        Ipp32s first_mb_in_slice_group;
        H264Slice *pSlice = sd->m_pSlice;

        if (addtcoeff_flag) {
            qp_delta = sd->m_cur_mb.LocalMacroblockInfo->baseQP - QPLuma + 54;
            scale = tcoeffs_scale_factors[ qp_delta % 6 ];
            shift = qp_delta / 6;
            QPChromaIndexU = sd->m_cur_mb.LocalMacroblockInfo->baseQP + sd->m_pPicParamSet->chroma_qp_index_offset[0];

            QPChromaIndexU = IPP_MIN(QPChromaIndexU, (Ipp32s)QP_MAX);
            QPChromaIndexU = IPP_MAX(0, QPChromaIndexU);
            baseQPChromaU = QPtoChromaQP[QPChromaIndexU];

            if (is_high_profile)
            {
                QPChromaIndexV = sd->m_cur_mb.LocalMacroblockInfo->baseQP + sd->m_pPicParamSet->chroma_qp_index_offset[1];
                QPChromaIndexV = IPP_MIN(QPChromaIndexV, (Ipp32s)QP_MAX);
                QPChromaIndexV = IPP_MAX(0, QPChromaIndexV);
                baseQPChromaV = QPtoChromaQP[QPChromaIndexV];
            } else {
                baseQPChromaV = baseQPChromaU;
            }

            qp_delta_chroma[0] =  baseQPChromaU - QPChromaU + 54;
            qp_delta_chroma[1] =  baseQPChromaV - QPChromaV + 54;

            scale_chroma[0] = tcoeffs_scale_factors[ qp_delta_chroma[0] % 6 ];
            shift_chroma[0] = qp_delta_chroma[0] / 6;
            scale_chroma[1] = tcoeffs_scale_factors[ qp_delta_chroma[1] % 6 ];
            shift_chroma[1] = qp_delta_chroma[1] / 6;
        }

        tmpCoeffPtr0 = (Ipp16s*)sd->m_pCoeffBlocksRead;
        tmpTCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockTCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];

        sd->cbp4x4_luma_dequant = 0;

        for (iii = 0; iii < 4; iii++)
        {
            if ((sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma & (blockNum))) {
                if (addtcoeff_flag) {
                    ScaleTCoeffs_raw(tmpTCoeffPtr, 64, scale, shift);
                    ippsAdd_16s_I(tmpCoeffPtr0, tmpTCoeffPtr, 64);
                } else {
                    ippsCopy_16s(tmpCoeffPtr0, tmpTCoeffPtr, 64);
                }
                tmpCoeffPtr0 += 64;
            } else if (!addtcoeff_flag) {
                memset(tmpTCoeffPtr, 0, 64 * sizeof(Ipp16s));
            } else {
                ScaleTCoeffs_raw(tmpTCoeffPtr, 64, scale, shift);
            }

            for (i = 0; i < 64; i++) {
                if (tmpTCoeffPtr[i]) {
                    sd->cbp4x4_luma_dequant |= blockNum;
                    break;
                }
            }

            tmpTCoeffPtr += 64;
            blockNum <<= 4;
        }

        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs = sd->cbp4x4_luma_dequant;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual =
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs;

        for (ChromaPlane = 0; ChromaPlane < 2; ChromaPlane++) {
            if (addtcoeff_flag) {
                ScaleTCoeffs_raw(tmpTCoeffPtr, 64, scale_chroma[ChromaPlane], shift_chroma[ChromaPlane]);
                if (sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[ChromaPlane] & 1) {
                    for (i = 0; i < 4; i++) {
                        COEFF_ADD_S16_I(tmpTCoeffPtr[i * 16], tmpCoeffPtr0[i]);
                     }
                    tmpCoeffPtr0 += 4;
                }
            } else {
                if (sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[ChromaPlane] & 1) {
                    for (i = 0; i < 4; i++) {
                        tmpTCoeffPtr[i * 16] = tmpCoeffPtr0[i];
                    }
                    tmpCoeffPtr0 += 4;
                } else {
                    for (i = 0; i < 4; i++) {
                        tmpTCoeffPtr[i * 16] = 0;
                    }
                }
            }
            tmpTCoeffPtr += 64;
        }

        tmpTCoeffPtr -= 128;

        for (ChromaPlane = 0; ChromaPlane < 2; ChromaPlane++) {
            a = 1;
            blockNum = 1;
            sd->cbp4x4_chroma_dequant[ChromaPlane] = 0;
            for (iii = 0; iii < 4; iii++)
            {
                if ((sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[ChromaPlane] & (1 << blockNum))) {
                    if (addtcoeff_flag) {
                        ippsAdd_16s_I(tmpCoeffPtr0, tmpTCoeffPtr, 16);
                    } else {
                        ippsCopy_16s(tmpCoeffPtr0 + a, tmpTCoeffPtr + a, 16 - a);
                    }
                    tmpCoeffPtr0 += 16;
                } else if (!addtcoeff_flag) {
                    memset(tmpTCoeffPtr + a, 0, (16 - a) * sizeof(Ipp16s));
                }

                if (*tmpTCoeffPtr) {
                    sd->cbp4x4_chroma_dequant[ChromaPlane] |= 1;
                }

                for (i = 1; i < 16; i++) {
                    if (tmpTCoeffPtr[i]) {
                        sd->cbp4x4_chroma_dequant[ChromaPlane] |= (1 << blockNum);
                        break;
                    }
                }

                tmpTCoeffPtr += 16;
                blockNum++;
            }
        }

        sd->m_pCoeffBlocksRead = tmpCoeffPtr0;

// fixme: QP_deblock for first_mb_in_slice
        if (sd->m_isSliceGroups)
        {
            first_mb_in_slice_group = sd->m_pSliceHeader->first_mb_in_slice;
        }
        else
        {
            first_mb_in_slice_group = 0;
        }

        if (!sd->m_pSliceHeader->tcoeff_level_prediction_flag ||
            (sd->cbp4x4_luma_dequant != 0) ||
            (sd->cbp4x4_chroma_dequant[0] != 0) ||
            (sd->cbp4x4_chroma_dequant[1] != 0) ||
            (sd->m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTRA_16x16) ||
            (first_mb_in_slice_group == sd->m_CurMBAddr))
        {
            sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP;
        }
        else if (sd->m_pSliceHeader->first_mb_in_slice == sd->m_CurMBAddr) {
            sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = sd->m_mbinfo.mbs[sd->m_CurMBAddr-1].QP_deblock;
        } else {
            sd->m_cur_mb.LocalMacroblockInfo->QP_deblock = (Ipp8s)pSlice->last_QP_deblock;
        }
    }


    void DequantSaveCoeffs(H264SegmentDecoderMultiThreaded * sd,
        Ipp32s lumaQP,
        Ipp32s QPChromaU,
        Ipp8u ibl = 0)
    {
        Ipp32s blockNum = 1;
        Ipp16s* tmpCoeffPtr;
        Ipp16s* tmpCoeffPtr0;
        int iii, jjj;

        if (ibl)
            tmpCoeffPtr0 = (Ipp16s*)sd->m_pCoeffBlocksRead;
        else
            tmpCoeffPtr0 = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockTCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];
        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];

        for (iii = 0; iii < 4; iii++)
        {
            for (jjj = 0; jjj < 4; jjj++)
            {
                if (sd->cbp4x4_luma_dequant & (1 << blockNum))
                {
                    ippiDequantResidual_H264_16s_C1(tmpCoeffPtr0, tmpCoeffPtr, lumaQP);
                }
                else
                {
                    memset(tmpCoeffPtr, 0, 16 * sizeof(Ipp16s));
                }
                if (!ibl || sd->cbp4x4_luma_dequant & (1 << blockNum))
                    tmpCoeffPtr0 += 16;
                tmpCoeffPtr += 16;
                blockNum++;
            }
        }

        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE + 256];

        ippiDequantChroma_H264_16s_C1((Ipp16s**)&tmpCoeffPtr0, tmpCoeffPtr,
            CreateIPPCBPMask420(sd->cbp4x4_chroma_dequant[0],
            sd->cbp4x4_chroma_dequant[1]),
            QPChromaU, ibl);

        if (ibl) sd->m_pCoeffBlocksRead = tmpCoeffPtr0;
    }

    void DequantSaveCoeffs_4x4(H264SegmentDecoderMultiThreaded * sd,
        Ipp32s lumaQP,
        Ipp32s QPChromaU,
        Ipp32s QPChromaV,
        Ipp32u levelScaleDCU,
        Ipp32u levelScaleDCV,
        const Ipp16s *pQuantTable,
        const Ipp16s *pQuantTableU,
        const Ipp16s *pQuantTableV,
        Ipp8u bypass_flag,
        Ipp8u ibl = 0)
    {
        Ipp32s blockNum = 1;
        Ipp16s* tmpCoeffPtr;
        Ipp16s* tmpCoeffPtr0;
        int iii, jjj;
        Ipp32u levelScale[2];
        const Ipp16s *pQuantTableChroma[2];
        Ipp32s QPChroma[2];

        levelScale[0] = levelScaleDCU;
        levelScale[1] = levelScaleDCV;
        pQuantTableChroma[0] = pQuantTableU;
        pQuantTableChroma[1] = pQuantTableV;
        QPChroma[0] = QPChromaU;
        QPChroma[1] = QPChromaV;

        if (ibl)
            tmpCoeffPtr0 = (Ipp16s*)sd->m_pCoeffBlocksRead;
        else
            tmpCoeffPtr0 = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockTCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];
        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];

        for (iii = 0; iii < 4; iii++)
        {
            for (jjj = 0; jjj < 4; jjj++)
            {
                if (sd->cbp4x4_luma_dequant & (1 << blockNum))
                {
                    ippiDequantResidual4x4_H264_16s_C1(tmpCoeffPtr0, tmpCoeffPtr, lumaQP,
                        pQuantTable, bypass_flag);
                }
                else
                {
                    memset(tmpCoeffPtr, 0, 16 * sizeof(Ipp16s));
                }
                if (!ibl || sd->cbp4x4_luma_dequant & (1 << blockNum))
                    tmpCoeffPtr0 += 16;
                tmpCoeffPtr += 16;
                blockNum++;
            }
        }

        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE + 256];

        ippiDequantChromaHigh_H264_16s_C1((Ipp16s**)&tmpCoeffPtr0, tmpCoeffPtr,
            CreateIPPCBPMask420(sd->cbp4x4_chroma_dequant[0],
            sd->cbp4x4_chroma_dequant[1]),
            QPChromaU, QPChromaV, pQuantTableU, pQuantTableV, ibl);

        if (ibl) sd->m_pCoeffBlocksRead = tmpCoeffPtr0;
    }

    void DequantSaveCoeffs_8x8(H264SegmentDecoderMultiThreaded * sd,
        Ipp32s lumaQP,
        Ipp32s QPChromaU,
        Ipp32s QPChromaV,
        Ipp32u levelScaleDCU,
        Ipp32u levelScaleDCV,
        const Ipp16s *pQuantTable,
        const Ipp16s *pQuantTableU,
        const Ipp16s *pQuantTableV,
        Ipp8u bypass_flag,
        Ipp8u ibl = 0)
    {
        Ipp32s blockNum = 0x1e;
        Ipp16s* tmpCoeffPtr;
        Ipp16s* tmpCoeffPtr0;
        int iii;
        Ipp32u levelScale[2];
        const Ipp16s *pQuantTableChroma[2];
        Ipp32s QPChroma[2];

        levelScale[0] = levelScaleDCU;
        levelScale[1] = levelScaleDCV;
        pQuantTableChroma[0] = pQuantTableU;
        pQuantTableChroma[1] = pQuantTableV;
        QPChroma[0] = QPChromaU;
        QPChroma[1] = QPChromaV;

        if (ibl)
            tmpCoeffPtr0 = (Ipp16s*)sd->m_pCoeffBlocksRead;
        else
            tmpCoeffPtr0 = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockTCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];
        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];

        for (iii = 0; iii < 4; iii++)
        {
            if (sd->cbp4x4_luma_dequant & (blockNum))
            {
                ippiDequantResidual8x8_H264_16s_C1(tmpCoeffPtr0,tmpCoeffPtr, lumaQP,
                    pQuantTable, bypass_flag);
            }
            else
            {
                memset(tmpCoeffPtr, 0, 64 * sizeof(Ipp16s));
            }
            if (!ibl || sd->cbp4x4_luma_dequant & (blockNum))
                tmpCoeffPtr0 += 64;
            tmpCoeffPtr += 64;
            blockNum<<=4;
        }

        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE + 256];

        ippiDequantChromaHigh_H264_16s_C1((Ipp16s**)&tmpCoeffPtr0, tmpCoeffPtr,
            CreateIPPCBPMask420(sd->cbp4x4_chroma_dequant[0],
            sd->cbp4x4_chroma_dequant[1]),
            QPChromaU, QPChromaV, pQuantTableU, pQuantTableV,
            ibl);

        if (ibl) sd->m_pCoeffBlocksRead = tmpCoeffPtr0;
    }

    Ipp32u CBPcalculate(Ipp16s *coeffPtr)
    {
        Ipp64s *tmpCoeffPtr = (Ipp64s *)coeffPtr;
        Ipp32s blockNum = 1;
        Ipp32u cbp4x4;
        int iii, jjj;

        cbp4x4 = 0;

        for (iii = 0; iii < 4; iii++)
        {
            for (jjj = 0; jjj < 4; jjj++)
            {
                if ((tmpCoeffPtr[0] | tmpCoeffPtr[1] | tmpCoeffPtr[2] | tmpCoeffPtr[3]) != 0)
                {
                    cbp4x4 |= (1 << blockNum);
                }
                tmpCoeffPtr += 4;
                blockNum++;
            }
        }

        return cbp4x4;
    }

    Ipp32u CBPcalculate_8x8(Ipp16s *coeffPtr)
    {
        Ipp64s *tmpCoeffPtr = (Ipp64s *)coeffPtr;
        Ipp32s blockNum = 1;
        Ipp32u cbp4x4;
        int iii, jjj;

        cbp4x4 = 0;

        for (iii = 0; iii < 4; iii++)
        {
            Ipp32s tmp = 0;
            for (jjj = 0; jjj < 4; jjj++)
            {
                if ((tmpCoeffPtr[0] | tmpCoeffPtr[1] | tmpCoeffPtr[2] | tmpCoeffPtr[3]) != 0)
                {
                    tmp = 0xf;

                }
                tmpCoeffPtr += 4;
            }
            cbp4x4 |= (tmp << blockNum);
            blockNum += 4;
        }

        return cbp4x4;
    }

    Ipp32u CBPcalculateResidual(Ipp16s *coeffPtr,
                                Ipp32s pitch)
    {
        Ipp32s blockNum = 1;
        Ipp32u cbp4x4;
        int iii, jjj;

        cbp4x4 = 0;

        for (iii = 0; iii < 4; iii++)
        {
            for (jjj = 0; jjj < 4; jjj++)
            {
                Ipp32s index  = (iii >> 1) * 8 * pitch + (iii & 1) * 8 +
                                (jjj >> 1) * 4 * pitch + (jjj & 1) * 4;
                Ipp64s tmp0 = *(Ipp64s *)(coeffPtr + index);
                Ipp64s tmp1 = *(Ipp64s *)(coeffPtr + index + pitch);
                Ipp64s tmp2 = *(Ipp64s *)(coeffPtr + index + 2 * pitch);
                Ipp64s tmp3 = *(Ipp64s *)(coeffPtr + index + 3 * pitch);

                if ((tmp0 | tmp1 | tmp2 | tmp3) != 0)
                {
                    cbp4x4 |= (1 << blockNum);
                }
                blockNum++;
            }
        }

        return cbp4x4;
    }

    Ipp32u CBPcalculateResidual_8x8(Ipp16s *coeffPtr,
        Ipp32s pitch)
    {
        Ipp32s cbp = CBPcalculateResidual(coeffPtr, pitch);
        Ipp32s i, m = 0x1e;

        for (i = 0; i < 4; i++) {
            if (cbp & m) cbp |= m;
            m <<= 4;
        }

        return cbp;
    }

    void AddCoeffs(H264SegmentDecoderMultiThreaded * sd,
        Ipp32s lumaQP,
        Ipp32s QPChromaU,
        Ipp8u ibl = 0)
    {
        Ipp16s tmpBuf[16*8];
        Ipp32s blockNum = 1;
        Ipp16s* tmpCoeffPtr, *tmpCoeffPtr0;
        int iii, jjj;

        if (ibl)
            tmpCoeffPtr0 = (Ipp16s*)sd->m_pCoeffBlocksRead;
        else
            tmpCoeffPtr0 = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockTCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];

        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];

        for (iii = 0; iii < 4; iii++)
        {
            for (jjj = 0; jjj < 4; jjj++)
            {
                if (sd->cbp4x4_luma_dequant & (1 << blockNum))
                {
                    ippiDequantResidual_H264_16s_C1(tmpCoeffPtr0, tmpBuf, lumaQP);
                    ippsAdd_16s_I(tmpBuf, tmpCoeffPtr, 16);
                }
                if (!ibl || sd->cbp4x4_luma_dequant & (1 << blockNum))
                    tmpCoeffPtr0 += 16;
                tmpCoeffPtr0 += 16;
                tmpCoeffPtr += 16;
                blockNum++;
            }
        }

        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs = CBPcalculate(tmpCoeffPtr);
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual =
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs;

        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE + 256];

        ippiDequantChroma_H264_16s_C1((Ipp16s**)&tmpCoeffPtr0, tmpBuf,
            CreateIPPCBPMask420(sd->cbp4x4_chroma_dequant[0],
            sd->cbp4x4_chroma_dequant[1]),
            QPChromaU, ibl);

        ippsAdd_16s_I(tmpBuf, tmpCoeffPtr, 16*8);

        if (ibl) sd->m_pCoeffBlocksRead = tmpCoeffPtr0;
    }

    void AddCoeffs_4x4(H264SegmentDecoderMultiThreaded * sd,
        Ipp32s lumaQP,
        Ipp32s QPChromaU,
        Ipp32s QPChromaV,
        Ipp32u levelScaleDCU,
        Ipp32u levelScaleDCV,
        const Ipp16s *pQuantTable,
        const Ipp16s *pQuantTableU,
        const Ipp16s *pQuantTableV,
        Ipp8u bypass_flag,
        Ipp8u ibl = 0)
    {
        Ipp16s tmpBuf[16*8];
        Ipp32s blockNum = 1;
        Ipp16s* tmpCoeffPtr, *tmpCoeffPtr0;
        int iii, jjj;
        Ipp32u levelScale[2];
        const Ipp16s *pQuantTableChroma[2];
        Ipp32s QPChroma[2];

        levelScale[0] = levelScaleDCU;
        levelScale[1] = levelScaleDCV;
        pQuantTableChroma[0] = pQuantTableU;
        pQuantTableChroma[1] = pQuantTableV;
        QPChroma[0] = QPChromaU;
        QPChroma[1] = QPChromaV;

        if (ibl)
            tmpCoeffPtr0 = (Ipp16s*)sd->m_pCoeffBlocksRead;
        else
            tmpCoeffPtr0 = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockTCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];
        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];

        for (iii = 0; iii < 4; iii++)
        {
            for (jjj = 0; jjj < 4; jjj++)
            {
                if (sd->cbp4x4_luma_dequant & (1 << blockNum))
                {
                    ippiDequantResidual4x4_H264_16s_C1(tmpCoeffPtr0, tmpBuf, lumaQP,
                        pQuantTable, bypass_flag);
                    ippsAdd_16s_I(tmpBuf, tmpCoeffPtr, 16);
                }
                if (!ibl || sd->cbp4x4_luma_dequant & (1 << blockNum))
                    tmpCoeffPtr0 += 16;
                tmpCoeffPtr += 16;
                blockNum++;
            }
        }

        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs = CBPcalculate(tmpCoeffPtr);
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual =
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs;

        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE + 256];

        ippiDequantChromaHigh_H264_16s_C1((Ipp16s**)&tmpCoeffPtr0, tmpBuf,
            CreateIPPCBPMask420(sd->cbp4x4_chroma_dequant[0],
            sd->cbp4x4_chroma_dequant[1]),
            QPChromaU, QPChromaV, pQuantTableU, pQuantTableV, ibl);

        ippsAdd_16s_I(tmpBuf, tmpCoeffPtr, 16*8);

        if (ibl) sd->m_pCoeffBlocksRead = tmpCoeffPtr0;
    }

    void AddCoeffs_8x8(H264SegmentDecoderMultiThreaded * sd,
        Ipp32s lumaQP,
        Ipp32s QPChromaU,
        Ipp32s QPChromaV,
        Ipp32u levelScaleDCU,
        Ipp32u levelScaleDCV,
        const Ipp16s *pQuantTable,
        const Ipp16s *pQuantTableU,
        const Ipp16s *pQuantTableV,
        Ipp8u bypass_flag,
        Ipp8u ibl = 0)
    {
        Ipp16s tmpBuf[16*8];
        Ipp32s blockNum = 0x1e;
        Ipp16s* tmpCoeffPtr, *tmpCoeffPtr0;
        int iii;
        Ipp32u levelScale[2];
        const Ipp16s *pQuantTableChroma[2];
        Ipp32s QPChroma[2];

        levelScale[0] = levelScaleDCU;
        levelScale[1] = levelScaleDCV;
        pQuantTableChroma[0] = pQuantTableU;
        pQuantTableChroma[1] = pQuantTableV;
        QPChroma[0] = QPChromaU;
        QPChroma[1] = QPChromaV;

        if (ibl)
            tmpCoeffPtr0 = (Ipp16s*)sd->m_pCoeffBlocksRead;
        else
            tmpCoeffPtr0 = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockTCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];
        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];

        for (iii = 0; iii < 4; iii++)
        {
            if (sd->cbp4x4_luma_dequant & (blockNum))
            {
                ippiDequantResidual8x8_H264_16s_C1(tmpCoeffPtr0, tmpBuf, lumaQP,
                    pQuantTable, bypass_flag);
                ippsAdd_16s_I(tmpBuf, tmpCoeffPtr, 64);
            }
            if (!ibl || sd->cbp4x4_luma_dequant & (blockNum))
                tmpCoeffPtr0 += 64;
            tmpCoeffPtr += 64;
            blockNum<<=4;
        }

        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs = CBPcalculate_8x8(tmpCoeffPtr);
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual =
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs;

        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE + 256];

        ippiDequantChromaHigh_H264_16s_C1((Ipp16s**)&tmpCoeffPtr0, tmpBuf,
            CreateIPPCBPMask420(sd->cbp4x4_chroma_dequant[0],
            sd->cbp4x4_chroma_dequant[1]),
            QPChromaU, QPChromaV, pQuantTableU, pQuantTableV, ibl);

        ippsAdd_16s_I(tmpBuf, tmpCoeffPtr, 16*8);

        if (ibl) sd->m_pCoeffBlocksRead = tmpCoeffPtr0;
    }

    void ReconstructLumaBaseMode(H264SegmentDecoderMultiThreaded * sd,
                                 Ipp32u offsetY,
                                 Ipp32u rec_pitch_luma)
    {
        Ipp32s blockNum = 1;
        Ipp16s* tmpCoeffPtr;
        int iii, jjj;

        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];

        Ipp8u* pYPlane = (Ipp8u*)sd->m_pYPlane + offsetY;

        for (iii = 0; iii < 4; iii++)
        {
            for (jjj = 0; jjj < 4; jjj++)
            {
                Ipp32s index  = (iii >> 1) * 8 * rec_pitch_luma + (iii & 1) * 8 +
                                (jjj >> 1) * 4 * rec_pitch_luma + (jjj & 1) * 4;

                ippiTransformResidualAndAdd_H264_16s8u_C1I(pYPlane + index,
                                                         tmpCoeffPtr,
                                                         pYPlane + index, rec_pitch_luma,
                                                         rec_pitch_luma);
                tmpCoeffPtr += 16;
                blockNum++;
            }
        }
    }

    void ReconstructLumaBaseMode_8x8(H264SegmentDecoderMultiThreaded * sd,
        Ipp32u offsetY,
        Ipp32u rec_pitch_luma)
    {
        Ipp32s blockNum = 1;
        Ipp16s* tmpCoeffPtr;
        int iii;

        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];

        for (iii = 0; iii < 4; iii++)
        {
            Ipp32s index  = (iii >> 1) * 8 * rec_pitch_luma + (iii & 1) * 8;
            Ipp8u* pYPlane = (Ipp8u*)((PlanePtrY)sd->m_pYPlane + offsetY + index);

            ippiTransformResidualAndAdd8x8_H264_16s8u_C1I(pYPlane,
                tmpCoeffPtr,
                pYPlane, rec_pitch_luma,
                rec_pitch_luma);

            tmpCoeffPtr += 64;
            blockNum++;
        }
    }

    void ReconstructLumaBaseModeResidual(H264SegmentDecoderMultiThreaded * sd,
                                         Ipp32u offsetY,
                                         Ipp32u rec_pitch_luma,
                                         Ipp32s is_pred_used)
    {
        Ipp32s blockNum = 1;
        Ipp16s* tmpCoeffPtr;
        int iii, jjj, k, l;

        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];

        if (is_pred_used)
        {
            for (iii = 0; iii < 4; iii++)
            {
                for (jjj = 0; jjj < 4; jjj++)
                {
                    Ipp32s index  = (iii >> 1) * 8 * rec_pitch_luma + (iii & 1) * 8 +
                        (jjj >> 1) * 4 * rec_pitch_luma + (jjj & 1) * 4;
                    Ipp16s* pYPlane = sd->m_pYResidual + offsetY + index;

                    ippiTransformResidualAndAdd_H264_16s16s_C1I(pYPlane,
                        tmpCoeffPtr,
                        pYPlane, rec_pitch_luma << 1,
                        rec_pitch_luma << 1);

                    for (k = 0; k < 4; k++) {
                        for (l = 0; l < 4; l++) {
                            pYPlane[l] = IPP_MIN(pYPlane[l], 255);
                            pYPlane[l] = IPP_MAX(pYPlane[l], -255);
                        }
                        pYPlane += rec_pitch_luma;
                    }

                    tmpCoeffPtr += 16;
                    blockNum++;
                }
            }
        }
        else
        {
            for (iii = 0; iii < 4; iii++)
            {
                for (jjj = 0; jjj < 4; jjj++)
                {
                    Ipp32s index  = (iii >> 1) * 8 * rec_pitch_luma + (iii & 1) * 8 +
                        (jjj >> 1) * 4 * rec_pitch_luma + (jjj & 1) * 4;
                    Ipp16s* pYPlane = sd->m_pYResidual + offsetY + index;

                    ippiTransformResidual_H264_16s16s_C1I(tmpCoeffPtr,
                        pYPlane, rec_pitch_luma << 1);

                    for (k = 0; k < 4; k++) {
                        for (l = 0; l < 4; l++) {
                            pYPlane[l] = IPP_MIN(pYPlane[l], 255);
                            pYPlane[l] = IPP_MAX(pYPlane[l], -255);
                        }
                        pYPlane += rec_pitch_luma;
                    }

                    tmpCoeffPtr += 16;
                    blockNum++;
                }
            }
        }

        tmpCoeffPtr = sd->m_pYResidual + offsetY;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual =
            CBPcalculateResidual(tmpCoeffPtr, rec_pitch_luma);
    }

    void ReconstructLumaBaseModeResidual_8x8(H264SegmentDecoderMultiThreaded * sd,
                                             Ipp32u offsetY,
                                             Ipp32u rec_pitch_luma,
                                             Ipp32s is_pred_used)
    {
        Ipp32s blockNum = 1;
        Ipp16s* tmpCoeffPtr;
        int iii, k, l;

        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];

        if (is_pred_used)
        {
            for (iii = 0; iii < 4; iii++)
            {
                Ipp32s index  = (iii >> 1) * 8 * rec_pitch_luma + (iii & 1) * 8;
                Ipp16s* pYPlane = sd->m_pYResidual + offsetY + index;

                ippiTransformResidualAndAdd8x8_H264_16s16s_C1I(pYPlane,
                    tmpCoeffPtr,
                    pYPlane, rec_pitch_luma << 1,
                    rec_pitch_luma << 1);

                for (k = 0; k < 8; k++) {
                    for (l = 0; l < 8; l++) {
                        pYPlane[l] = IPP_MIN(pYPlane[l], 255);
                        pYPlane[l] = IPP_MAX(pYPlane[l], -255);
                    }
                    pYPlane += rec_pitch_luma;
                }

                tmpCoeffPtr += 64;
                blockNum++;
            }
        }
        else
        {
            for (iii = 0; iii < 4; iii++)
            {
                Ipp32s index  = (iii >> 1) * 8 * rec_pitch_luma + (iii & 1) * 8;
                Ipp16s* pYPlane = sd->m_pYResidual + offsetY + index;

                ippiTransformResidual8x8_H264_16s16s_C1I(tmpCoeffPtr,
                    pYPlane, rec_pitch_luma << 1);

                for (k = 0; k < 8; k++) {
                    for (l = 0; l < 8; l++) {
                        pYPlane[l] = IPP_MIN(pYPlane[l], 255);
                        pYPlane[l] = IPP_MAX(pYPlane[l], -255);
                    }
                    pYPlane += rec_pitch_luma;
                }

                tmpCoeffPtr += 64;
                blockNum++;
            }
        }


        tmpCoeffPtr = sd->m_pYResidual + offsetY;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual =
            CBPcalculateResidual_8x8(tmpCoeffPtr, rec_pitch_luma);
    }

    void ReconstructChromaBaseMode(H264SegmentDecoderMultiThreaded * sd,
                                   Ipp32u offsetC,
                                   Ipp32u rec_pitch_chroma)
    {
        Ipp8u *pCPlane;
        Ipp16s* tmpCoeffPtr;
        int iii, jjj;

        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE + 256];

        pCPlane = (Ipp8u*)((PlanePtrUV)sd->m_pUPlane + offsetC);
        for (iii = 0; iii < 2; iii++)
        {
            for (jjj = 0; jjj < 4; jjj++)
            {
                ippiTransformResidualAndAdd_H264_16s8u_C1I(pCPlane,  tmpCoeffPtr,
                                                         pCPlane, rec_pitch_chroma,
                                                         rec_pitch_chroma);

                pCPlane += 4;
                if (jjj == 1)
                {
                    pCPlane += (rec_pitch_chroma << 2) - 8;
                }

                tmpCoeffPtr += 16;

            }

            pCPlane = (Ipp8u*)((PlanePtrUV)sd->m_pVPlane + offsetC);
        }
    }

    void ReconstructChromaBaseMode_NV12(H264SegmentDecoderMultiThreaded * sd,
        Ipp32u offsetC,
        Ipp32u rec_pitch_chroma)
    {
        Ipp16s* tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE + 256];

        Ipp8u* pCPlane = (Ipp8u*)sd->m_pUVPlane + offsetC;
        for (Ipp32s iii = 0; iii < 2; iii++)
        {
            for (Ipp32s jjj = 0; jjj < 4; jjj++)
            {
                ippiTransformResidualAndAdd_H264_16s8u_C1I_NV12(pCPlane,  tmpCoeffPtr,
                    pCPlane, rec_pitch_chroma,
                    rec_pitch_chroma);

                pCPlane += 8;
                if (jjj == 1)
                {
                    pCPlane += (rec_pitch_chroma << 2) - 16;
                }

                tmpCoeffPtr += 16;

            }

            pCPlane = (Ipp8u*)sd->m_pUVPlane + offsetC + 1;
        }
    }

    void ReconstructChromaBaseModeResidual(H264SegmentDecoderMultiThreaded * sd,
                                           Ipp32u offsetC,
                                           Ipp32u rec_pitch_chroma,
                                           Ipp32s is_pred_used)
    {
        Ipp16s *pCPlane;
        Ipp16s* tmpCoeffPtr;
        int iii, jjj, k, l;

        tmpCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE + 256];

        pCPlane = sd->m_pUResidual + offsetC;

        if (is_pred_used)
        {
            for (iii = 0; iii < 2; iii++)
            {
                for (jjj = 0; jjj < 4; jjj++)
                {
                    Ipp16s *tmpPtr;
                    ippiTransformResidualAndAdd_H264_16s16s_C1I(pCPlane,  tmpCoeffPtr,
                        pCPlane, rec_pitch_chroma << 1,
                        rec_pitch_chroma << 1);

                    tmpPtr = pCPlane;
                    for (k = 0; k < 4; k++) {
                        for (l = 0; l < 4; l++) {
                            tmpPtr[l] = IPP_MIN(tmpPtr[l], 255);
                            tmpPtr[l] = IPP_MAX(tmpPtr[l], -255);
                        }
                        tmpPtr += rec_pitch_chroma;
                    }

                    pCPlane += 4;
                    if (jjj == 1)
                    {
                        pCPlane += (rec_pitch_chroma << 2) - 8;
                    }

                    tmpCoeffPtr += 16;

                }

                pCPlane = sd->m_pVResidual + offsetC;
            }
        }
        else
        {
            for (iii = 0; iii < 2; iii++)
            {
                for (jjj = 0; jjj < 4; jjj++)
                {
                    Ipp16s *tmpPtr;
                    ippiTransformResidual_H264_16s16s_C1I(tmpCoeffPtr,
                        pCPlane, rec_pitch_chroma << 1);

                    tmpPtr = pCPlane;
                    for (k = 0; k < 4; k++) {
                        for (l = 0; l < 4; l++) {
                            tmpPtr[l] = IPP_MIN(tmpPtr[l], 255);
                            tmpPtr[l] = IPP_MAX(tmpPtr[l], -255);
                        }
                        tmpPtr += rec_pitch_chroma;
                    }

                    pCPlane += 4;
                    if (jjj == 1)
                    {
                        pCPlane += (rec_pitch_chroma << 2) - 8;
                    }

                    tmpCoeffPtr += 16;

                }

                pCPlane = sd->m_pVResidual + offsetC;
            }
        }
    }

    void ReconstructMacroblock_IBLType(H264SegmentDecoderMultiThreaded * sd)
    {
        // per-macroblock variables

        Ipp32s fdf = pGetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo);

        Ipp32s mbXOffset = sd->m_CurMB_X * 16;
        Ipp32s mbYOffset = sd->m_CurMB_Y * 16;
        VM_ASSERT(mbXOffset < sd->m_pCurrentFrame->lumaSize().width);
        VM_ASSERT(mbYOffset < sd->m_pCurrentFrame->lumaSize().height);

        // reconstruct starts here
        // Perform motion compensation to reconstruct the YUV data
        //
        Ipp32u offsetY = mbXOffset + (mbYOffset * sd->m_uPitchLuma);
        Ipp32u offsetC = (mbXOffset >> width_chroma_div) +  ((mbYOffset >> height_chroma_div) * sd->m_uPitchChroma);

        Ipp32s bitdepth_luma_qp_scale = 6*(sd->bit_depth_luma - 8);
        Ipp32s bitdepth_chroma_qp_scale = 6*(sd->bit_depth_chroma - 8
            + sd->m_pSeqParamSet->residual_colour_transform_flag);

        Ipp32s lumaQP = sd->m_cur_mb.LocalMacroblockInfo->QP + bitdepth_luma_qp_scale;
        Ipp32s QPChromaU, QPChromaV;
        Ipp32s QPChromaIndexU, QPChromaIndexV;
        QPChromaIndexU = sd->m_cur_mb.LocalMacroblockInfo->QP + sd->m_pPicParamSet->chroma_qp_index_offset[0];

        QPChromaIndexU = IPP_MIN(QPChromaIndexU, (Ipp32s)QP_MAX);
        QPChromaIndexU = IPP_MAX(-bitdepth_chroma_qp_scale, QPChromaIndexU);
        QPChromaU = QPChromaIndexU < 0 ? QPChromaIndexU : QPtoChromaQP[QPChromaIndexU];
        QPChromaU += bitdepth_chroma_qp_scale;

        if (is_high_profile)
        {
            QPChromaIndexV = sd->m_cur_mb.LocalMacroblockInfo->QP + sd->m_pPicParamSet->chroma_qp_index_offset[1];
            QPChromaIndexV = IPP_MIN(QPChromaIndexV, (Ipp32s)QP_MAX);
            QPChromaIndexV = IPP_MAX(-bitdepth_chroma_qp_scale, QPChromaIndexV);
            QPChromaV = QPChromaIndexV < 0 ? QPChromaIndexV : QPtoChromaQP[QPChromaIndexV];
            QPChromaV += bitdepth_chroma_qp_scale;
        } else {
            QPChromaV = QPChromaU;
        }

        const Ipp16s *ScalingMatrix4x4Y;
        const Ipp16s *ScalingMatrix4x4U;
        const Ipp16s *ScalingMatrix4x4V;
        const Ipp16s *ScalingMatrix8x8;
        Ipp16s ScalingDCU = 0;
        Ipp16s ScalingDCV = 0;

        if (is_high_profile)
        {
            ScalingMatrix4x4Y = sd->m_pScalingPicParams->m_LevelScale4x4[0].LevelScaleCoeffs[lumaQP];
            ScalingMatrix4x4U = sd->m_pScalingPicParams->m_LevelScale4x4[1].LevelScaleCoeffs[QPChromaU];
            ScalingMatrix4x4V = sd->m_pScalingPicParams->m_LevelScale4x4[2].LevelScaleCoeffs[QPChromaV];
            ScalingMatrix8x8 = sd->m_pScalingPicParams->m_LevelScale8x8[0].LevelScaleCoeffs[lumaQP];

            ScalingDCU = sd->m_pScalingPicParams->m_LevelScale4x4[1].LevelScaleCoeffs[QPChromaU+3][0];
            ScalingDCV = sd->m_pScalingPicParams->m_LevelScale4x4[2].LevelScaleCoeffs[QPChromaV+3][0];
        }

        Ipp32u rec_pitch_luma = sd->m_uPitchLuma; // !!! adjust rec_pitch to MBAFF
        Ipp32u rec_pitch_chroma = sd->m_uPitchChroma; // !!! adjust rec_pitch to MBAFF

        if (sd->m_isMBAFF)
        {
            Ipp32s currmb_fdf = fdf;
            Ipp32s currmb_bf = (sd->m_CurMBAddr & 1);

            if (currmb_fdf) //current mb coded as field MB
            {
                if (currmb_bf)
                {
                    offsetY -= 15 * rec_pitch_luma;
                    offsetC -= (color_format == 1 ? 7 : 15) * rec_pitch_chroma;
                }
                rec_pitch_luma *= 2;
                rec_pitch_chroma *= 2;
            }
        }

        /* TCOEFF condition for intra */

        sd->cbp4x4_luma_dequant =
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
        sd->cbp4x4_chroma_dequant[0] =
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0];
        sd->cbp4x4_chroma_dequant[1] =
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1];

        if (is_high_profile) {
            if (pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo)) {
                if (sd->m_coeff_prediction_flag && sd->m_refinementFlag)
                {
                    AddCoeffs_8x8(sd, lumaQP, QPChromaU, QPChromaV,
                        ScalingDCU, ScalingDCV,
                        ScalingMatrix8x8, ScalingMatrix4x4U, ScalingMatrix4x4V,
                        sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag, 1);
                }
                else
                {
                    DequantSaveCoeffs_8x8(sd, lumaQP, QPChromaU, QPChromaV,
                        ScalingDCU, ScalingDCV,
                        ScalingMatrix8x8, ScalingMatrix4x4U, ScalingMatrix4x4V,
                        sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag, 1);
                }
            } else {
                if (sd->m_coeff_prediction_flag && sd->m_refinementFlag)
                {
                    AddCoeffs_4x4(sd, lumaQP, QPChromaU, QPChromaV,
                        ScalingDCU, ScalingDCV,
                        ScalingMatrix4x4Y, ScalingMatrix4x4U, ScalingMatrix4x4V,
                        sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag, 1);
                }
                else
                {
                    DequantSaveCoeffs_4x4(sd, lumaQP, QPChromaU, QPChromaV,
                        ScalingDCU, ScalingDCV,
                        ScalingMatrix4x4Y, ScalingMatrix4x4U, ScalingMatrix4x4V,
                        sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag, 1);
                }
            }
        } else {
        /* Dequant coeffs and add it with base ones if needed */
            if (sd->m_coeff_prediction_flag && sd->m_refinementFlag)
            {
                AddCoeffs(sd, lumaQP, QPChromaU, 1);
            }
            else
            {
                DequantSaveCoeffs(sd, lumaQP, QPChromaU, 1);
            }
        }

        if ((sd->m_bIsMaxQId && sd->m_next_spatial_resolution_change) || sd->m_is_ref_base_pic || sd->m_reconstruct_all)
        {
            if (is_high_profile)
            {
                if (pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo)) {
                    ReconstructLumaBaseMode_8x8(sd, offsetY, rec_pitch_luma);
                } else {
                    ReconstructLumaBaseMode(sd, offsetY, rec_pitch_luma);
                }
            }
            else
            {
                ReconstructLumaBaseMode(sd, offsetY, rec_pitch_luma);
            }

            if (color_format)
            {
                // reconstruct chroma block(s)
                if (is_high_profile) // color_format
                {
                    if (nv12_support) {
                        ReconstructChromaBaseMode_NV12(sd, offsetC, rec_pitch_chroma);
                    } else {
                        ReconstructChromaBaseMode(sd, offsetC, rec_pitch_chroma);
                    }
                }
                else // if (is_high_profile)
                {
                    if (nv12_support) {
                        ReconstructChromaBaseMode_NV12(sd, offsetC, rec_pitch_chroma);
                    } else {
                        ReconstructChromaBaseMode(sd, offsetC, rec_pitch_chroma);
                    }
                } // if (is_high_profile)
            }
        }


        sd->m_pSlice->last_QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP_deblock;

        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs =
            (Ipp16u)(sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs >> 1);
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual =
            (Ipp16u)(sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual >> 1);

    } // void ReconstructMacroblock_ISlice(H264SegmentDecoderMultiThreaded * sd)

    void RestoreIntraPrediction(H264SegmentDecoderMultiThreaded * sd,
        Ipp32s offsetYrec,
        Ipp32s offsetCrec,
        Ipp32s offsetY,
        Ipp32s offsetC,
        Ipp32s fdf,
        Ipp32s nv12)
    {
        Ipp8u* intra_y = (Ipp8u*)sd->m_mbinfo.m_tmpBuf0;
        Ipp8u* intra_u = intra_y + 256;
        Ipp8u* intra_v = intra_u + 64;
        int i, j;
        Ipp32s iCX = sd->ipred_cx;
        Ipp32s iCY = sd->ipred_cy;

        for (i = 0 ; i < 2; i++) {
            for (j = 0; j < 2; j++) {
                if (sd->ipred_flag[i][j]) {
                    int sx, sy, ex, ey;

                    switch ((i << 1)+ j) {
                                default:
                                case 0:
                                    sx = sy = 0;
                                    ex = iCX;
                                    ey = iCY;
                                    break;
                                case 1:
                                    if (iCX >= 16)
                                        continue;
                                    sx = iCX;
                                    sy = 0;
                                    ex = 16;
                                    ey = iCY;
                                    break;
                                case 2:
                                    if (iCY >= 16)
                                        continue;
                                    sx = 0;
                                    sy = iCY;
                                    ex = iCX;
                                    ey = 16;
                                    break;
                                case 3:
                                    if (iCX >= 16 || iCY >= 16)
                                        continue;
                                    sx = iCX;
                                    sy = iCY;
                                    ex = 16;
                                    ey = 16;
                                    break;
                    }

                    CopyMacroblock(intra_y + sx + 16*sy,
                        sd->m_pYPlane + offsetYrec + sx + (sd->m_uPitchLuma * sy),
                        16, sd->m_uPitchLuma, ex-sx, ey-sy);
                    ClearResidual(sd->m_pYResidual + offsetY + sx + (sd->m_uPitchLumaCur<<fdf)*sy, (sd->m_uPitchLumaCur<<fdf), ex-sx, ey-sy);
                    sx = ( sx + 1 ) >> 1;
                    sy = ( sy + 1 ) >> 1;
                    ex = ( ex + 1 ) >> 1;
                    ey = ( ey + 1 ) >> 1;
                    if (nv12) {
                        CopyMacroblock(intra_u + sx*2 + 16*sy,
                            sd->m_pUVPlane + offsetCrec + sx*2 + (sd->m_uPitchChroma * sy),
                            16, sd->m_uPitchChroma, (ex-sx)*2, ey-sy);
                    } else {
                        CopyMacroblock(intra_u + sx + 8*sy,
                            sd->m_pUPlane + offsetCrec + sx + (sd->m_uPitchChroma * sy),
                            8, sd->m_uPitchChroma, ex-sx, ey-sy);
                        CopyMacroblock(intra_v + sx + 8*sy,
                            sd->m_pVPlane + offsetCrec + sx + (sd->m_uPitchChroma * sy),
                            8, sd->m_uPitchChroma, ex-sx, ey-sy);
                    }

                    ClearResidual(sd->m_pUResidual + offsetC + sx + (sd->m_uPitchChromaCur<<fdf)*sy, (sd->m_uPitchChromaCur<<fdf), ex-sx, ey-sy);
                    ClearResidual(sd->m_pVResidual + offsetC + sx + (sd->m_uPitchChromaCur<<fdf)*sy, (sd->m_uPitchChromaCur<<fdf), ex-sx, ey-sy);
                }
            }
        }
    }

    void AddMacroblockSat(Ipp16s *src, Ipp8u *srcdst, int len, int stride1, int stride2)
    {
        int ii, jj;
        for (ii = 0; ii < len; ii++) {
            for (jj = 0; jj < len; jj++) {
                Ipp16s tmp = src[jj] + srcdst[jj];
                srcdst[jj] = (Ipp8u)(tmp > 255 ? 255 : (tmp < 0 ? 0 : tmp));
            }
            src += stride1;
            srcdst += stride2;
        }
    }
    void AddMacroblockSat_NV12(Ipp16s *src, Ipp8u *srcdst, int len, int stride1, int stride2)
    {
        int ii, jj;
        for (ii = 0; ii < len; ii++) {
            for (jj = 0; jj < len; jj++) {
                Ipp16s tmp = src[jj] + srcdst[jj*2];
                srcdst[jj*2] = (Ipp8u)(tmp > 255 ? 255 : (tmp < 0 ? 0 : tmp));
            }
            src += stride1;
            srcdst += stride2;
        }
    }
    void CopyMacroblock(Ipp8u* src, Ipp8u* dst, int stride1, int stride2, int lenx, int leny)
    {
        int ii, jj;
        for (ii = 0; ii < leny; ii++) {
            for (jj = 0; jj < lenx; jj++) {
                dst[jj] = src[jj];
            }
            src += stride1;
            dst += stride2;
        }
    }

    void ClearResidual(Ipp16s* dst, int stride, int lenx, int leny) 
    {
        int ii, jj;
        for (ii = 0; ii < leny; ii++) {
            for (jj = 0; jj < lenx; jj++) {
                dst[jj] = 0;
            }
            dst += stride;
        }
    }

    void StoreIntraPrediction(H264SegmentDecoderMultiThreaded * sd,
        Ipp32s offsetYrec,
        Ipp32s offsetCrec, Ipp32s nv12)
    {
        H264Slice *pSlice = sd->m_pSlice;
        H264DecoderLayerDescriptor *cur_layer = &sd->m_mbinfo.layerMbs[sd->m_currentLayer];
        H264DecoderLayerDescriptor *ref_layer = &sd->m_mbinfo.layerMbs[pSlice->GetSliceHeader()->ref_layer_dq_id >> 4];
        H264DecoderLayerResizeParameter *pResizeParameter = cur_layer->m_pResizeParameter;
        Ipp32s x, y;
        Ipp32s iCX       = 0;
        Ipp32s iCY       = 0;
        Ipp32s shiftX = pResizeParameter->shiftX;
        Ipp32s shiftY = pResizeParameter->shiftY;
        Ipp32s scaleX = pResizeParameter->scaleX;
        Ipp32s scaleY = pResizeParameter->scaleY;
        Ipp32s addX = pResizeParameter->addX;
        Ipp32s addY = pResizeParameter->addY;
        Ipp32s ipred = 0;
        Ipp8u* intra_y = (Ipp8u*)sd->m_mbinfo.m_tmpBuf0;
        Ipp8u* intra_u = intra_y + 256;
        Ipp8u* intra_v = intra_u + 64;

        Ipp32s ref_layer_height = pResizeParameter->ref_layer_height;

        if (pResizeParameter->RefLayerFieldPicFlag)
        {
            ref_layer_height >>= 1;
        }

        Ipp32s iBaseMbX0 = (( ( sd->m_CurMB_X * 16) * scaleX + addX) >> shiftX );
        Ipp32s iBaseMbY0 = (( ( sd->m_CurMB_Y * 16) * scaleY + addY) >> shiftY );

        Ipp32s halfMBs = ((pResizeParameter->ref_layer_height * pResizeParameter->ref_layer_width) / 512);
        Ipp32s fieldOffset = ref_layer->field_pic_flag && ref_layer->bottom_field_flag ? halfMBs : 0;

        if (iBaseMbX0 > pResizeParameter->ref_layer_width - 1)
            iBaseMbX0 = pResizeParameter->ref_layer_width - 1;
        if (iBaseMbY0 > ref_layer_height - 1)
            iBaseMbY0 = ref_layer_height - 1;

        iBaseMbX0 >>= 4;
        iBaseMbY0 >>= 4;

        sd->ipred_flag[0][0] = 0;
        sd->ipred_flag[0][1] = 0;
        sd->ipred_flag[1][0] = 0;
        sd->ipred_flag[1][1] = 0;
        sd->ipred_cx = 0;
        sd->ipred_cy = 0;

        //----- determine first location that points to a different macroblock (not efficient implementation!) -----
        for( ; iCX < 16; iCX++ )
        {
            Ipp32s iBaseMbX = (Ipp32s)((Ipp32u)((sd->m_CurMB_X * 16 + iCX) * scaleX + addX) >> shiftX);
            if (iBaseMbX > pResizeParameter->ref_layer_width - 1)
                iBaseMbX = pResizeParameter->ref_layer_width - 1;
            iBaseMbX >>= 4;

            if( iBaseMbX > iBaseMbX0 )
            {
                break;
            }
        }
        for( ; iCY < 16; iCY++ )
        {
            Ipp32s iBaseMbY = (Ipp32s)((Ipp32u)((sd->m_CurMB_Y * 16 + iCY) * scaleY + addY) >> shiftY );
            if (iBaseMbY > ref_layer_height - 1)
                iBaseMbY = ref_layer_height - 1;
            iBaseMbY >>= 4;

            if( iBaseMbY > iBaseMbY0 )
            {
                break;
            }
        }
        sd->ipred_cx = iCX;
        sd->ipred_cy = iCY;

        for (y = 0; y < 2; y++) {
            Ipp32s yRef = sd->m_CurMB_Y * 16 + y * 15;
            Ipp32s refMbPartIdxY, refMbAddrY;

            /* Only FRAME MB is implemented */
            //yRef = (curMB_Y * 16 + (4 * y + 1)) * (1 + fieldMbFlag - field_pic_flag) * (1 + field_pic_flag);

            yRef = (yRef * scaleY + addY) >> shiftY;
            if (yRef < 0 || yRef >= ref_layer_height)
                continue;
            refMbAddrY = (yRef >> 4) * (pResizeParameter->ref_layer_width >> 4);
            refMbPartIdxY = yRef & 15;

            for (x = 0; x < 2; x++)
            {
                Ipp32s xRef = ((sd->m_CurMB_X * 16 + x * 15) * scaleX +
                    addX) >> shiftX;
                Ipp32s refMbAddr = refMbAddrY + (xRef >> 4) + fieldOffset;

                if (xRef < 0 || xRef >= pResizeParameter->ref_layer_width)
                    continue;

                if (ref_layer->mbs[refMbAddr].mbtype <= MBTYPE_PCM)
                {
                    sd->ipred_flag[y][x] = 1;
                    ipred = 1;
                    continue;
                }
            }
        }


        if (ipred) {
            CopyMacroblock(sd->m_pYPlane + offsetYrec,
                intra_y, sd->m_uPitchLuma, 16, 16, 16);
            if (nv12) {
                CopyMacroblock(sd->m_pUVPlane + offsetCrec,
                    intra_u, sd->m_uPitchChroma, 16, 16, 8);
            } else {
                CopyMacroblock(sd->m_pUPlane + offsetCrec,
                    intra_u, sd->m_uPitchChroma, 8, 8, 8);
                CopyMacroblock(sd->m_pVPlane + offsetCrec,
                    intra_v, sd->m_uPitchChroma, 8, 8, 8);
            }
        }
    }

    void ReconstructMacroblock_ISlice(H264SegmentDecoderMultiThreaded * sd)
    {
        Ipp8u mbtype = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;
        H264Slice *pSlice = sd->m_pSlice;
        H264DecoderFrame *pFrame = pSlice->GetCurrentFrame();
        CoeffsPtrCommon pCoeffBlocksRead_save = sd->m_pCoeffBlocksRead;
        Ipp16s tbuf[1024];

        if (pFrame->m_maxDId > 0)
        {
            IppiSize roiSize16 = {16, 16};
            IppiSize roiSize8 = {8, 8};

            Ipp32s fdf = pGetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo);

            Ipp32s mbXOffset = sd->m_CurMB_X * 16;
            Ipp32s mbYOffset = sd->m_CurMB_Y * 16;

            VM_ASSERT(mbXOffset < sd->m_pCurrentFrame->lumaSize().width);
            VM_ASSERT(mbYOffset < sd->m_pCurrentFrame->lumaSize().height);

            Ipp32u uPitchLuma = sd->m_uPitchLumaCur;
            Ipp32u uPitchChroma = sd->m_uPitchChromaCur;

            Ipp32u offsetY = mbXOffset + (mbYOffset * uPitchLuma);
            Ipp32u offsetC = (mbXOffset >> (Ipp32s)(color_format<3)) +  ((mbYOffset >> (Ipp32s)(color_format <= 1)) * uPitchChroma);

            bool need_adjust = (sd->m_CurMBAddr & 1) && fdf;

            if (need_adjust)
            {
                offsetY -= 15 * uPitchLuma;
                offsetC -= (color_format == 1 ? 7 : 15) * uPitchChroma;
            }
            ippiSet_16s_C1R(0, sd->m_pYResidual + offsetY, uPitchLuma*2 << fdf, roiSize16);
            ippiSet_16s_C1R(0, sd->m_pUResidual + offsetC, uPitchChroma*2 << fdf, roiSize8);
            ippiSet_16s_C1R(0, sd->m_pVResidual + offsetC, uPitchChroma*2 << fdf, roiSize8);
        }

        if (MBTYPE_INTRA_BL == mbtype)
        {
            ReconstructMacroblock_IBLType(sd);
            return;
        }

        // per-macroblock variables
        Ipp32s fdf = pGetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo);
        // reconstruct Data
        IntraType *pMBIntraTypes = sd->m_pMBIntraTypes + sd->m_CurMBAddr*NUM_INTRA_TYPE_ELEMENTS;

        Ipp32s mbXOffset = sd->m_CurMB_X * 16;
        Ipp32s mbYOffset = sd->m_CurMB_Y * 16;
        VM_ASSERT(mbXOffset < sd->m_pCurrentFrame->lumaSize().width);
        VM_ASSERT(mbYOffset < sd->m_pCurrentFrame->lumaSize().height);

        // reconstruct starts here
        // Perform motion compensation to reconstruct the YUV data
        //
        Ipp32u offsetY = mbXOffset + (mbYOffset * sd->m_uPitchLuma);
        Ipp32u offsetC = (mbXOffset >> width_chroma_div) +  ((mbYOffset >> height_chroma_div) * sd->m_uPitchChroma);

        Ipp32s bitdepth_luma_qp_scale = 6*(sd->bit_depth_luma - 8);
        Ipp32s bitdepth_chroma_qp_scale = 6*(sd->bit_depth_chroma - 8
            + sd->m_pSeqParamSet->residual_colour_transform_flag);

        Ipp32s lumaQP = sd->m_cur_mb.LocalMacroblockInfo->QP + bitdepth_luma_qp_scale;
        Ipp32s QPChromaU, QPChromaV;
        Ipp32s QPChromaIndexU, QPChromaIndexV;
        QPChromaIndexU = sd->m_cur_mb.LocalMacroblockInfo->QP + sd->m_pPicParamSet->chroma_qp_index_offset[0];

        QPChromaIndexU = IPP_MIN(QPChromaIndexU, (Ipp32s)QP_MAX);
        QPChromaIndexU = IPP_MAX(-bitdepth_chroma_qp_scale, QPChromaIndexU);
        QPChromaU = QPChromaIndexU < 0 ? QPChromaIndexU : QPtoChromaQP[QPChromaIndexU];
        QPChromaU += bitdepth_chroma_qp_scale;

        if (is_high_profile)
        {
            QPChromaIndexV = sd->m_cur_mb.LocalMacroblockInfo->QP + sd->m_pPicParamSet->chroma_qp_index_offset[1];
            QPChromaIndexV = IPP_MIN(QPChromaIndexV, (Ipp32s)QP_MAX);
            QPChromaIndexV = IPP_MAX(-bitdepth_chroma_qp_scale, QPChromaIndexV);
            QPChromaV = QPChromaIndexV < 0 ? QPChromaIndexV : QPtoChromaQP[QPChromaIndexV];
            QPChromaV += bitdepth_chroma_qp_scale;
        } else {
            QPChromaV = QPChromaU;
        }

        const Ipp16s *ScalingMatrix4x4Y;
        const Ipp16s *ScalingMatrix4x4U;
        const Ipp16s *ScalingMatrix4x4V;
        const Ipp16s *ScalingMatrix8x8;
        Ipp16s ScalingDCU = 0;
        Ipp16s ScalingDCV = 0;

        if (is_high_profile)
        {
            ScalingMatrix4x4Y = sd->m_pScalingPicParams->m_LevelScale4x4[0].LevelScaleCoeffs[lumaQP];
            ScalingMatrix4x4U = sd->m_pScalingPicParams->m_LevelScale4x4[1].LevelScaleCoeffs[QPChromaU];
            ScalingMatrix4x4V = sd->m_pScalingPicParams->m_LevelScale4x4[2].LevelScaleCoeffs[QPChromaV];
            ScalingMatrix8x8 = sd->m_pScalingPicParams->m_LevelScale8x8[0].LevelScaleCoeffs[lumaQP];

            ScalingDCU = sd->m_pScalingPicParams->m_LevelScale4x4[1].LevelScaleCoeffs[QPChromaU+3][0];
            ScalingDCV = sd->m_pScalingPicParams->m_LevelScale4x4[2].LevelScaleCoeffs[QPChromaV+3][0];
        }

        Ipp32u rec_pitch_luma = sd->m_uPitchLuma; // !!! adjust rec_pitch to MBAFF
        Ipp32u rec_pitch_chroma = sd->m_uPitchChroma; // !!! adjust rec_pitch to MBAFF

        if (sd->m_isMBAFF)
        {
            Ipp32s currmb_fdf = fdf;
            Ipp32s currmb_bf = (sd->m_CurMBAddr & 1);

            if (currmb_fdf) //current mb coded as field MB
            {
                if (currmb_bf)
                {
                    offsetY -= 15 * rec_pitch_luma;
                    offsetC -= (color_format == 1 ? 7 : 15) * rec_pitch_chroma;
                }
                rec_pitch_luma *= 2;
                rec_pitch_chroma *= 2;
            }
        }

        if (mbtype != MBTYPE_PCM)
        {
            Ipp32s special_MBAFF_case = 0; // !!!
            Ipp8u edge_type = 0;
            Ipp8u edge_type_2t = 0;
            Ipp8u edge_type_2b = 0;

            sd->cbp4x4_luma_dequant =
                sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
            sd->cbp4x4_chroma_dequant[0] =
                sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0];
            sd->cbp4x4_chroma_dequant[1] =
                sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1];

            if (pFrame->m_maxDId || pFrame->m_maxQId)
            {
                Ipp32s start = 0;
                Ipp32s tcoeff_pred_flag = sd->m_pSliceHeader->tcoeff_level_prediction_flag && pGetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo);

                if (pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo)) {
                    UpdateTCoeffs_8x8(sd,
                        lumaQP,
                        QPChromaU,
                        QPChromaV,
                        tcoeff_pred_flag);
                } else {
                    UpdateTCoeffs_4x4(sd,
                        lumaQP,
                        QPChromaU,
                        QPChromaV,
                        tcoeff_pred_flag);
                }

                Ipp16s *tmpTCoeffPtr = (Ipp16s*) &sd->m_mbinfo.SavedMacroblockTCoeffs[sd->m_CurMBAddr*COEFFICIENTS_BUFFER_SIZE];
                Ipp16s *coeffBlocksRead = tbuf;

                if (sd->m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTRA_16x16 &&
                    (sd->cbp4x4_luma_dequant & 1)) {
                        for (Ipp32s i = 0; i < 16; i++)
                          coeffBlocksRead[i] = tmpTCoeffPtr[block_subblock_mapping[i] * 16];
                        coeffBlocksRead += 16;
                        start = 1;
                }

                if (pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo)) {
                    Ipp32s block = 0x1e;
                    for (Ipp32s j = 0; j < 4; j++) {
                        if (sd->cbp4x4_luma_dequant & block) {
                            ippsCopy_16s(tmpTCoeffPtr, coeffBlocksRead, 64);
                            coeffBlocksRead += 64;
                        }
                        block <<= 4;
                        tmpTCoeffPtr += 64;
                    }
                } else {
                    Ipp32s block = 2;
                    for (Ipp32s j = 0; j < 16; j++) {
                        if (sd->cbp4x4_luma_dequant & block) {
                            ippsCopy_16s(tmpTCoeffPtr + start, coeffBlocksRead + start, 16 - start);
                            if (start)
                                *coeffBlocksRead = 0;
                            coeffBlocksRead += 16;
                        }
                        block <<= 1;
                        tmpTCoeffPtr += 16;
                    }
                }

                for (Ipp32s i = 0; i < 2; i++) {
                    if (sd->cbp4x4_chroma_dequant[i] & 1) {
                            for (Ipp32s j = 0; j < 4; j++)
                              coeffBlocksRead[j] = tmpTCoeffPtr[16*j];
                            coeffBlocksRead += 4;
                            start = 1;
                    }
                    tmpTCoeffPtr += 64;
                }
                tmpTCoeffPtr -= 128;

                for (Ipp32s i = 0; i < 2; i++) {
                    Ipp32s block = 2;
                    for (Ipp32s j = 0; j < 4; j++) {
                        if (sd->cbp4x4_chroma_dequant[i] & block) {
                            ippsCopy_16s(tmpTCoeffPtr, coeffBlocksRead, 16);
                            coeffBlocksRead += 16;
                        }
                        block <<= 1;
                        tmpTCoeffPtr += 16;
                    }
                }
            }

            if (sd->m_isMBAFF)
            {
                special_MBAFF_case = 0; // adjust to MBAFF

                sd->ReconstructEdgeType(edge_type_2t, edge_type_2b, special_MBAFF_case);
                edge_type = (Ipp8u) (edge_type_2t | edge_type_2b);
            }
            else
            {
                edge_type = (Ipp8u)sd->m_mbinfo.mbs[sd->m_CurMBAddr].IntraTypes.edge_type;
            }

            if (pFrame->m_maxDId || pFrame->m_maxQId)
            {
                sd->m_pCoeffBlocksRead = tbuf;
            }

            // reconstruct luma block(s)
            if (mbtype == MBTYPE_INTRA_16x16)
            {
                if (is_high_profile)
                {
                    ReconstructLumaIntra_16x16MB(
                        (CoeffsPtr*)(&sd->m_pCoeffBlocksRead),
                        (PlanePtrY)sd->m_pYPlane + offsetY,
                        rec_pitch_luma,
                        (IppIntra16x16PredMode_H264) pMBIntraTypes[0],
                        sd->cbp4x4_luma_dequant,
                        lumaQP,
                        edge_type,
                        ScalingMatrix4x4Y,
                        sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag,
                        sd->bit_depth_luma);
                }
                else
                {
                    ReconstructLumaIntra16x16MB(
                        (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                        (PlanePtrY)sd->m_pYPlane + offsetY,
                        rec_pitch_luma,
                        (IppIntra16x16PredMode_H264) pMBIntraTypes[0],
                        (Ipp32u) sd->cbp4x4_luma_dequant,
                        lumaQP,
                        edge_type,
                        sd->bit_depth_luma);
                }
            }
            else // if (intra16x16)
            {
                if (is_high_profile)
                {
                    switch (special_MBAFF_case)
                    {
                    default:
                        if (pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo))
                        {
                            ReconstructLumaIntraHalf8x8MB(
                                (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                                (PlanePtrY)sd->m_pYPlane + offsetY,
                                rec_pitch_luma,
                                (IppIntra8x8PredMode_H264 *) pMBIntraTypes,
                                sd->m_cur_mb.LocalMacroblockInfo->cbp,
                                lumaQP,
                                edge_type_2t,
                                ScalingMatrix8x8,
                                sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag);
                            ReconstructLumaIntraHalf8x8MB(
                                (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                                (PlanePtrY)sd->m_pYPlane + offsetY+8*rec_pitch_luma,
                                rec_pitch_luma,
                                (IppIntra8x8PredMode_H264 *) pMBIntraTypes+2,
                                sd->m_cur_mb.LocalMacroblockInfo->cbp>>2,
                                lumaQP,
                                edge_type_2b,
                                ScalingMatrix8x8,
                                sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag);
                        }
                        else
                        {
                            ReconstructLumaIntraHalf4x4MB(
                                (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                                (PlanePtrY)sd->m_pYPlane + offsetY,
                                rec_pitch_luma,
                                (IppIntra4x4PredMode_H264 *) pMBIntraTypes,
                                sd->cbp4x4_luma_dequant>>1,
                                lumaQP,
                                edge_type_2t,
                                ScalingMatrix4x4Y,
                                sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag);
                            ReconstructLumaIntraHalf4x4MB(
                                (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                                (PlanePtrY)sd->m_pYPlane + offsetY+8*rec_pitch_luma,
                                rec_pitch_luma,
                                (IppIntra4x4PredMode_H264 *) pMBIntraTypes+8,
                                sd->cbp4x4_luma_dequant>>9,
                                lumaQP,
                                edge_type_2b,
                                ScalingMatrix4x4Y,
                                sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag);
                        }
                        break;
                    case 0:
                        if (pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo))
                        {
                            Ipp32u cbp8x8 = ((sd->cbp4x4_luma_dequant >> 1) & 0x1) + 
                            ((sd->cbp4x4_luma_dequant >> 4) & 0x2) + 
                            ((sd->cbp4x4_luma_dequant >> 7) & 0x4) + 
                            ((sd->cbp4x4_luma_dequant >> 10) & 0x8);

                            ReconstructLumaIntra8x8MB (
                                (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                                (PlanePtrY)sd->m_pYPlane + offsetY,
                                rec_pitch_luma,
                                (IppIntra8x8PredMode_H264 *) pMBIntraTypes,
                                cbp8x8,
                                lumaQP,
                                edge_type,
                                ScalingMatrix8x8,
                                sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag,
                                sd->bit_depth_luma);
                        }
                        else
                            ReconstructLumaIntra4x4MB (
                                (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                                (PlanePtrY)sd->m_pYPlane + offsetY,
                                rec_pitch_luma,
                                (IppIntra4x4PredMode_H264 *) pMBIntraTypes,
                                sd->cbp4x4_luma_dequant,
                                lumaQP,
                                edge_type,
                                ScalingMatrix4x4Y,
                                sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag,
                                sd->bit_depth_luma);
                        break;
                    }
                }
                else
                {
                    switch (special_MBAFF_case)
                    {
                    default:
                        ReconstructLumaIntraHalfMB(
                            (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                            (PlanePtrY)sd->m_pYPlane + offsetY,
                            rec_pitch_luma,
                            (IppIntra4x4PredMode_H264 *) pMBIntraTypes,
                            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma>>1,
                            lumaQP,
                            edge_type_2t,
                            sd->bit_depth_luma);
                        ReconstructLumaIntraHalfMB(
                            (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                            (PlanePtrY)sd->m_pYPlane + offsetY+8*rec_pitch_luma,
                            rec_pitch_luma,
                            (IppIntra4x4PredMode_H264 *) pMBIntraTypes+8,
                            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma>>9,
                            lumaQP,
                            edge_type_2b,
                            sd->bit_depth_luma);
                        break;
                    case 0:
                        ReconstructLumaIntraMB(
                            (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                            (PlanePtrY)sd->m_pYPlane + offsetY,
                            rec_pitch_luma,
                            (IppIntra4x4PredMode_H264 *) pMBIntraTypes,
                            sd->cbp4x4_luma_dequant,
                            lumaQP,
                            edge_type,
                            sd->bit_depth_luma);
                        break;
                    }
                }
            }

            if (color_format)
            {
                Ipp32s bitdepth_chroma_qp_scale = 6*(sd->bit_depth_chroma - 8
                    + sd->m_pSeqParamSet->residual_colour_transform_flag);

                Ipp32s QPChromaU, QPChromaV;
                Ipp32s QPChromaIndexU, QPChromaIndexV;
                QPChromaIndexU = sd->m_cur_mb.LocalMacroblockInfo->QP + sd->m_pPicParamSet->chroma_qp_index_offset[0];

                QPChromaIndexU = IPP_MIN(QPChromaIndexU, (Ipp32s)QP_MAX);
                QPChromaIndexU = IPP_MAX(-bitdepth_chroma_qp_scale, QPChromaIndexU);
                QPChromaU = QPChromaIndexU < 0 ? QPChromaIndexU : QPtoChromaQP[QPChromaIndexU];
                QPChromaU += bitdepth_chroma_qp_scale;

                if (is_high_profile)
                {
                    QPChromaIndexV = sd->m_cur_mb.LocalMacroblockInfo->QP + sd->m_pPicParamSet->chroma_qp_index_offset[1];
                    QPChromaIndexV = IPP_MIN(QPChromaIndexV, (Ipp32s)QP_MAX);
                    QPChromaIndexV = IPP_MAX(-bitdepth_chroma_qp_scale, QPChromaIndexV);
                    QPChromaV = QPChromaIndexV < 0 ? QPChromaIndexV : QPtoChromaQP[QPChromaIndexV];
                    QPChromaV += bitdepth_chroma_qp_scale;
                }

                if (nv12_support)
                {
                    // reconstruct chroma block(s)
                    if (is_high_profile) // color_format
                    {
                        switch (special_MBAFF_case)
                        {
                        default:
                            ReconstructChromaIntraHalfs4x4MB_NV12 (
                                (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                                (PlanePtrUV)sd->m_pUVPlane + offsetC,
                                rec_pitch_chroma,
                                (IppIntraChromaPredMode_H264) sd->m_cur_mb.LocalMacroblockInfo->IntraTypes.intra_chroma_mode,
                                sd->cbp4x4_chroma_dequant[0],
                                sd->cbp4x4_chroma_dequant[1],
                                QPChromaU,
                                QPChromaV,
                                edge_type_2t,
                                edge_type_2b,
                                ScalingMatrix4x4U,
                                ScalingMatrix4x4V,
                                sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag,
                                sd->bit_depth_chroma);
                                break;
                        case 0:
                            ReconstructChromaIntra4x4MB_NV12(
                                (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                                (PlanePtrUV)sd->m_pUVPlane + offsetC,
                                rec_pitch_chroma,
                                (IppIntraChromaPredMode_H264) sd->m_cur_mb.LocalMacroblockInfo->IntraTypes.intra_chroma_mode,
                                sd->cbp4x4_chroma_dequant[0],
                                sd->cbp4x4_chroma_dequant[1],
                                QPChromaU,
                                QPChromaV,
                                edge_type,
                                ScalingMatrix4x4U,
                                ScalingMatrix4x4V,
                                sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag,
                                sd->bit_depth_chroma);
                                break;
                        }
                    }
                    else // if (is_high_profile)
                    {
                        switch (special_MBAFF_case)
                        {
                        default:
                            ReconstructChromaIntraHalfsMB_NV12 (
                                (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                                (PlanePtrUV)sd->m_pUVPlane + offsetC,
                                rec_pitch_chroma,
                                (IppIntraChromaPredMode_H264) sd->m_cur_mb.LocalMacroblockInfo->IntraTypes.intra_chroma_mode,
                                sd->cbp4x4_chroma_dequant[0],
                                sd->cbp4x4_chroma_dequant[1],
                                QPChromaU,
                                edge_type_2t,
                                edge_type_2b,
                                sd->bit_depth_chroma);
                            break;
                        case 0:
                            ReconstructChromaIntraMB_NV12 (
                                (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                                (PlanePtrUV)sd->m_pUVPlane + offsetC,
                                rec_pitch_chroma,
                                (IppIntraChromaPredMode_H264) sd->m_cur_mb.LocalMacroblockInfo->IntraTypes.intra_chroma_mode,
                                sd->cbp4x4_chroma_dequant[0],
                                sd->cbp4x4_chroma_dequant[1],
                                QPChromaU,
                                edge_type,
                                sd->bit_depth_chroma);
                            break;
                        }
                    } // if (is_high_profile)
                }
                else  // nv12_support
                {
                    // reconstruct chroma block(s)
                    if (is_high_profile) // color_format
                    {
                        switch (special_MBAFF_case)
                        {
                        default:
                            ReconstructChromaIntraHalfs4x4MB (
                                (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                                (PlanePtrUV)sd->m_pUPlane + offsetC,
                                (PlanePtrUV)sd->m_pVPlane + offsetC,
                                rec_pitch_chroma,
                                (IppIntraChromaPredMode_H264) sd->m_cur_mb.LocalMacroblockInfo->IntraTypes.intra_chroma_mode,
                                sd->cbp4x4_chroma_dequant[0],
                                sd->cbp4x4_chroma_dequant[1],
                                QPChromaU,
                                QPChromaV,
                                edge_type_2t,
                                edge_type_2b,
                                ScalingMatrix4x4U,
                                ScalingMatrix4x4V,
                                sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag);
                                break;
                        case 0:
                            ColorSpecific<color_format, Coeffs, PlaneY, PlaneUV, is_field>::ReconstructChromaIntra4x4MB_Swit(
                                (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                                (PlanePtrUV)sd->m_pUPlane + offsetC,
                                (PlanePtrUV)sd->m_pVPlane + offsetC,
                                rec_pitch_chroma,
                                (IppIntraChromaPredMode_H264) sd->m_cur_mb.LocalMacroblockInfo->IntraTypes.intra_chroma_mode,
                                sd->cbp4x4_chroma_dequant[0],
                                sd->cbp4x4_chroma_dequant[1],
                                QPChromaU,
                                QPChromaV,
                                ScalingDCU,
                                ScalingDCV,
                                edge_type,
                                ScalingMatrix4x4U,
                                ScalingMatrix4x4V,
                                sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag,
                                sd->bit_depth_chroma);
                                break;
                        }
                    }
                    else // if (is_high_profile)
                    {
                        switch (special_MBAFF_case)
                        {
                        default:
                            ReconstructChromaIntraHalfsMB (
                                (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                                (PlanePtrUV)sd->m_pUPlane + offsetC,
                                (PlanePtrUV)sd->m_pVPlane + offsetC,
                                rec_pitch_chroma,
                                (IppIntraChromaPredMode_H264) sd->m_cur_mb.LocalMacroblockInfo->IntraTypes.intra_chroma_mode,
                                sd->cbp4x4_chroma_dequant[0],
                                sd->cbp4x4_chroma_dequant[1],
                                QPChromaU,
                                edge_type_2t,
                                edge_type_2b,
                                sd->bit_depth_chroma);
                            break;
                        case 0:
                            ReconstructChromaIntraMB (
                                (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                                (PlanePtrUV)sd->m_pUPlane + offsetC,
                                (PlanePtrUV)sd->m_pVPlane + offsetC,
                                rec_pitch_chroma,
                                (IppIntraChromaPredMode_H264) sd->m_cur_mb.LocalMacroblockInfo->IntraTypes.intra_chroma_mode,
                                sd->cbp4x4_chroma_dequant[0],
                                sd->cbp4x4_chroma_dequant[1],
                                QPChromaU,
                                edge_type,
                                sd->bit_depth_chroma);
                            break;
                        }
                    } // if (is_high_profile)
                }   // nv12_support
            }

            if (pFrame->m_maxDId || pFrame->m_maxQId) {
                sd->m_pCoeffBlocksRead = pCoeffBlocksRead_save;
            }

            if (sd->m_is_ref_base_pic)
            {
                CopyMacroblock(sd->m_pYPlane + offsetY,
                    sd->m_pYPlane_base + offsetY, sd->m_uPitchLuma, sd->m_uPitchLuma, 16, 16);
                if (nv12_support)
                {
                    CopyMacroblock(sd->m_pUVPlane + offsetC,
                        sd->m_pUVPlane_base + offsetC, sd->m_uPitchChroma, sd->m_uPitchChroma, 16, 8);
                }
                else
                {
                    CopyMacroblock(sd->m_pUPlane + offsetC,
                        sd->m_pUPlane_base + offsetC, sd->m_uPitchChroma, sd->m_uPitchChroma, 8, 8);
                    CopyMacroblock(sd->m_pVPlane + offsetC,
                        sd->m_pVPlane_base + offsetC, sd->m_uPitchChroma, sd->m_uPitchChroma, 8, 8);
                }
            }
        }
        else
        {
            // reconstruct PCM block(s)
            ReconstructMB<PlaneY, PlaneUV, color_format, is_field, 1>  mb;
            mb.ReconstructPCMMB(offsetY, offsetC, rec_pitch_luma, rec_pitch_chroma, sd);
        }

        sd->m_pSlice->last_QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP_deblock;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs =
            (Ipp16u)(sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs >> 1);
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual =
            (Ipp16u)(sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual >> 1);
    } // void ReconstructMacroblock_ISlice(H264SegmentDecoderMultiThreaded * sd)

    void ReconstructMacroblock_PSlice(H264SegmentDecoderMultiThreaded * sd)
    {
        H264Slice *pSlice = sd->m_pSlice;
        H264DecoderFrame *pFrame = pSlice->GetCurrentFrame();
        H264DecoderLayerDescriptor *reflayerMb;
        Ipp32s refMbAff = 0;

        // per-macroblock variables
        if (sd->m_mbinfo.layerMbs) {
            reflayerMb = &sd->m_mbinfo.layerMbs[(pSlice->GetSliceHeader()->ref_layer_dq_id >> 4)];
            refMbAff = reflayerMb->is_MbAff;
        }

        // reconstruct Data
        Ipp8u mbtype = sd->m_cur_mb.GlobalMacroblockInfo->mbtype;

        if ((MBTYPE_PCM >= mbtype) || (MBTYPE_INTRA_BL == mbtype))
        {
            ReconstructMacroblock_ISlice(sd);
            return;
        }

        Ipp32s fdf = pGetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo);

        Ipp32s mbXOffset = sd->m_CurMB_X * 16;
        Ipp32s mbYOffset = sd->m_CurMB_Y * 16;

        VM_ASSERT(mbXOffset < sd->m_pCurrentFrame->lumaSize().width);
        VM_ASSERT(mbYOffset < sd->m_pCurrentFrame->lumaSize().height);

        // reconstruct starts here
        // Perform motion compensation to reconstruct the YUV data
        //

        Ipp32u uPitchLuma = sd->m_uPitchLumaCur;
        Ipp32u uPitchChroma = sd->m_uPitchChromaCur;
        Ipp32s pitch_luma = sd->m_uPitchLuma;
        Ipp32s pitch_chroma = sd->m_uPitchChroma;

        Ipp32u offsetY = mbXOffset + (mbYOffset * uPitchLuma);
        Ipp32u offsetC = (mbXOffset >> (width_chroma_div + nv12_support)) +  ((mbYOffset >> height_chroma_div) * uPitchChroma);

        Ipp32u offsetYrec = mbXOffset + (mbYOffset * sd->m_uPitchLuma);
        Ipp32u offsetCrec = (mbXOffset >> width_chroma_div) +  ((mbYOffset >> height_chroma_div) * sd->m_uPitchChroma);

        Ipp32s bitdepth_luma_qp_scale = 6*(sd->bit_depth_luma - 8);
        Ipp32s bitdepth_chroma_qp_scale = 6*(sd->bit_depth_chroma - 8
            + sd->m_pSeqParamSet->residual_colour_transform_flag);

        Ipp32s lumaQP = sd->m_cur_mb.LocalMacroblockInfo->QP + bitdepth_luma_qp_scale;
        Ipp32s QPChromaU, QPChromaV;
        Ipp32s QPChromaIndexU, QPChromaIndexV;
        QPChromaIndexU = sd->m_cur_mb.LocalMacroblockInfo->QP + sd->m_pPicParamSet->chroma_qp_index_offset[0];

        QPChromaIndexU = IPP_MIN(QPChromaIndexU, (Ipp32s)QP_MAX);
        QPChromaIndexU = IPP_MAX(-bitdepth_chroma_qp_scale, QPChromaIndexU);
        QPChromaU = QPChromaIndexU < 0 ? QPChromaIndexU : QPtoChromaQP[QPChromaIndexU];
        QPChromaU += bitdepth_chroma_qp_scale;

        if (is_high_profile)
        {
            QPChromaIndexV = sd->m_cur_mb.LocalMacroblockInfo->QP + sd->m_pPicParamSet->chroma_qp_index_offset[1];
            QPChromaIndexV = IPP_MIN(QPChromaIndexV, (Ipp32s)QP_MAX);
            QPChromaIndexV = IPP_MAX(-bitdepth_chroma_qp_scale, QPChromaIndexV);
            QPChromaV = QPChromaIndexV < 0 ? QPChromaIndexV : QPtoChromaQP[QPChromaIndexV];
            QPChromaV += bitdepth_chroma_qp_scale;
        } else {
            QPChromaV = QPChromaU;
        }

        const Ipp16s *ScalingMatrix4x4Y;
        const Ipp16s *ScalingMatrix4x4U;
        const Ipp16s *ScalingMatrix4x4V;
        const Ipp16s *ScalingMatrix8x8;
        Ipp16s ScalingDCU = 0;
        Ipp16s ScalingDCV = 0;

        //PredictMotionVectors(sd);
        //SaveMotionVectors(sd);

        if (is_high_profile)
        {
            ScalingMatrix4x4Y = sd->m_pScalingPicParams->m_LevelScale4x4[3].LevelScaleCoeffs[lumaQP];
            ScalingMatrix4x4U = sd->m_pScalingPicParams->m_LevelScale4x4[4].LevelScaleCoeffs[QPChromaU];
            ScalingMatrix4x4V = sd->m_pScalingPicParams->m_LevelScale4x4[5].LevelScaleCoeffs[QPChromaV];
            ScalingMatrix8x8 = sd->m_pScalingPicParams->m_LevelScale8x8[1].LevelScaleCoeffs[lumaQP];

            ScalingDCU = sd->m_pScalingPicParams->m_LevelScale4x4[4].LevelScaleCoeffs[QPChromaU+3][0];
            ScalingDCV = sd->m_pScalingPicParams->m_LevelScale4x4[5].LevelScaleCoeffs[QPChromaV+3][0];
        }

        if (sd->m_isMBAFF && (sd->m_CurMBAddr & 1) && fdf) {
            offsetY -= 15 * uPitchLuma;
            offsetC -= (color_format == 1 ? 7 : 15) * uPitchChroma;
        }

        if (sd->m_reconstruct_all)
        {
            if (!refMbAff && !sd->m_isMBAFF && pGetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo) &&
                !sd->m_mbinfo.layerMbs[sd->m_currentLayer].restricted_spatial_resolution_change_flag
                && sd->m_spatial_resolution_change ) {
                    StoreIntraPrediction(sd, offsetYrec, offsetCrec, nv12_support);
            }

            if (sd->m_isMBAFF)
            {
                Ipp32s fdf = pGetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo);
                Ipp32u offsetY_1 = offsetY;
                Ipp32u offsetC_1 = offsetC;
                Ipp32u offsetYrec_1 = offsetYrec;
                Ipp32u offsetCrec_1 = offsetCrec;
                bool need_adjust = (sd->m_CurMBAddr & 1) && fdf;
                if (need_adjust)
                {
                    mbYOffset -= 16;

                    offsetY_1 = offsetY - uPitchLuma;
                    offsetC_1 = offsetC - uPitchChroma;


                    offsetYrec -= 15 * sd->m_uPitchLuma;
                    offsetCrec -= (color_format == 1 ? 7 : 15) * sd->m_uPitchChroma;

                    offsetYrec_1 = offsetYrec - sd->m_uPitchLuma;
                    offsetCrec_1 = offsetCrec - sd->m_uPitchChroma;

                }

                pitch_luma <<= fdf;
                pitch_chroma <<= fdf;

                if(sd->m_pPicParamSet->weighted_bipred_idc!= 0 ||
                    sd->m_pPicParamSet->weighted_pred_flag!= 0)
                {
                    if(fdf)
                    {
                        ReconstructMB<PlaneY, PlaneUV, color_format, true, 1>  mb;
                        mb.CompensateMotionMacroBlock((PlanePtrY)sd->m_pYPlane + offsetYrec,
                            (PlanePtrUV)sd->m_pVPlane + offsetCrec,
                            (PlanePtrUV)sd->m_pUPlane + offsetCrec,
                            mbXOffset,
                            mbYOffset >> fdf,
                            offsetYrec_1,
                            offsetCrec_1,
                            pitch_luma,
                            pitch_chroma,
                            sd);
                    } else {
                        ReconstructMB<PlaneY, PlaneUV, color_format, false, 1>  mb;
                        mb.CompensateMotionMacroBlock((PlanePtrY)sd->m_pYPlane + offsetYrec,
                            (PlanePtrUV)sd->m_pVPlane + offsetCrec,
                            (PlanePtrUV)sd->m_pUPlane + offsetCrec,
                            mbXOffset,
                            mbYOffset >> fdf,
                            offsetYrec_1,
                            offsetCrec_1,
                            pitch_luma,
                            pitch_chroma,
                            sd);
                    }
                } else {
                    if(fdf)
                    {
                        ReconstructMB<PlaneY, PlaneUV, color_format, true, 0>  mb;
                        mb.CompensateMotionMacroBlock((PlanePtrY)sd->m_pYPlane + offsetYrec,
                            (PlanePtrUV)sd->m_pVPlane + offsetCrec,
                            (PlanePtrUV)sd->m_pUPlane + offsetCrec,
                            mbXOffset,
                            mbYOffset >> fdf,
                            offsetYrec_1,
                            offsetCrec_1,
                            pitch_luma,
                            pitch_chroma,
                            sd);
                    }
                    else
                    {
                        ReconstructMB<PlaneY, PlaneUV, color_format, false, 0>  mb;
                        mb.CompensateMotionMacroBlock((PlanePtrY)sd->m_pYPlane + offsetYrec,
                            (PlanePtrUV)sd->m_pVPlane + offsetCrec,
                            (PlanePtrUV)sd->m_pUPlane + offsetCrec,
                            mbXOffset,
                            mbYOffset >> fdf,
                            offsetYrec_1,
                            offsetCrec_1,
                            pitch_luma,
                            pitch_chroma,
                            sd);
                    }
                }
            }
            else
            {
                ReconstructMB<PlaneY, PlaneUV, color_format, is_field, 1>  mb;
                mb.CompensateMotionMacroBlock((PlanePtrY)sd->m_pYPlane + offsetYrec,
                    (PlanePtrUV)sd->m_pVPlane + offsetCrec,
                    (PlanePtrUV)sd->m_pUPlane + offsetCrec,
                    mbXOffset,
                    mbYOffset,
                    offsetYrec,
                    offsetCrec,
                    pitch_luma,
                    pitch_chroma,
                    sd);

            }

            if (!refMbAff && !sd->m_isMBAFF && pGetMBBaseModeFlag(sd->m_cur_mb.GlobalMacroblockInfo) &&
                !sd->m_mbinfo.layerMbs[sd->m_currentLayer].restricted_spatial_resolution_change_flag
                && sd->m_spatial_resolution_change) {
                    RestoreIntraPrediction(sd, offsetYrec, offsetCrec, offsetY, offsetC, fdf, nv12_support);
            }
        }

        if (pFrame->m_maxDId == 0 && pFrame->m_maxQId == 0)
        {
            if (sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma & D_CBP_LUMA_AC)
            {
                if (is_high_profile)
                {
                    if (pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo))
                    {
                        ReconstructLumaInter8x8MB(
                            (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                            (PlanePtrY)sd->m_pYPlane + offsetYrec,
                            pitch_luma,
                            sd->m_cur_mb.LocalMacroblockInfo->cbp,
                            lumaQP,
                            ScalingMatrix8x8,
                            sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag,
                            sd->bit_depth_luma);
                    }
                    else
                    {
                        ReconstructLumaInter4x4MB(
                            (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                            (PlanePtrY)sd->m_pYPlane + offsetYrec,
                            pitch_luma,
                            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma,
                            lumaQP,
                            ScalingMatrix4x4Y,
                            sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag,
                            sd->bit_depth_luma);
                    }
                }
                else
                {
                    ReconstructLumaInterMB(
                        (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                        (PlanePtrY)sd->m_pYPlane + offsetYrec,
                        pitch_luma,
                        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma,
                        lumaQP,
                        sd->bit_depth_luma);
                }
            }

            if (color_format && (sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0]
            || sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1]))
            {
                if (nv12_support)
                {
                    if (is_high_profile)
                    {
                        ReconstructChromaInter4x4MB_NV12(
                            (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                            (PlanePtrUV)sd->m_pUVPlane + offsetCrec,
                            pitch_chroma,
                            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0],
                            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1],
                            QPChromaU,
                            QPChromaV,
                            ScalingMatrix4x4U,
                            ScalingMatrix4x4V,
                            sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag,
                            sd->bit_depth_chroma);
                    }
                    else
                    {
                        ReconstructChromaInterMB_NV12(
                            (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                            (PlanePtrUV)sd->m_pUVPlane + offsetCrec,
                            pitch_chroma,
                            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0],
                            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1],
                            QPChromaU,
                            sd->bit_depth_chroma);
                    }
                }
                else // if (nv12_support)
                {
                    if (is_high_profile)
                    {
                        ColorSpecific<color_format, Coeffs, PlaneY, PlaneUV, is_field>::ReconstructChromaInter4x4MB_Swit(
                            (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                            (PlanePtrUV)sd->m_pUPlane + offsetCrec,
                            (PlanePtrUV)sd->m_pVPlane + offsetCrec,
                            pitch_chroma,
                            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0],
                            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1],
                            QPChromaU,
                            QPChromaV,
                            ScalingDCU,
                            ScalingDCV,
                            ScalingMatrix4x4U,
                            ScalingMatrix4x4V,
                            sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag,
                            sd->bit_depth_chroma);
                    }
                    else
                    {
                        ReconstructChromaInterMB(
                            (CoeffsPtr*)&sd->m_pCoeffBlocksRead,
                            (PlanePtrUV)sd->m_pUPlane + offsetCrec,
                            (PlanePtrUV)sd->m_pVPlane + offsetCrec,
                            pitch_chroma,
                            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0],
                            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1],
                            QPChromaU,
                            sd->bit_depth_chroma);
                    }
                } // if (nv12_support)
            }
        } else {
            Ipp32s scoeff_pred_flag = 0;
            Ipp32s tcoeff_pred_flag = 0;

            if ((sd->m_cur_mb.LocalMacroblockInfo->baseMbType > MBTYPE_PCM) &&
                pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo))
            {
                tcoeff_pred_flag = sd->m_pSliceHeader->tcoeff_level_prediction_flag;
                scoeff_pred_flag = sd->m_coeff_prediction_flag;
            }

            if (pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo)) {
                UpdateTCoeffs_8x8(sd,
                    lumaQP,
                    QPChromaU,
                    QPChromaV,
                    tcoeff_pred_flag);
            } else {
                UpdateTCoeffs_4x4(sd,
                    lumaQP,
                    QPChromaU,
                    QPChromaV,
                    tcoeff_pred_flag);
            }

            if (is_high_profile)
            {
                if (pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo))
                {
                    if (scoeff_pred_flag)
                    {
                        AddCoeffs_8x8(sd, lumaQP, QPChromaU, QPChromaV,
                            ScalingDCU, ScalingDCV,
                            ScalingMatrix8x8, ScalingMatrix4x4U, ScalingMatrix4x4V,
                            sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag);
                    }
                    else
                    {
                        DequantSaveCoeffs_8x8(sd, lumaQP, QPChromaU, QPChromaV,
                            ScalingDCU, ScalingDCV,
                            ScalingMatrix8x8, ScalingMatrix4x4U, ScalingMatrix4x4V,
                            sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag);
                    }

                }
                else
                {
                    if (scoeff_pred_flag)
                    {
                        AddCoeffs_4x4(sd, lumaQP, QPChromaU, QPChromaV,
                            ScalingDCU, ScalingDCV,
                            ScalingMatrix4x4Y, ScalingMatrix4x4U, ScalingMatrix4x4V,
                            sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag);
                    }
                    else
                    {
                        DequantSaveCoeffs_4x4(sd, lumaQP, QPChromaU, QPChromaV,
                            ScalingDCU, ScalingDCV,
                            ScalingMatrix4x4Y, ScalingMatrix4x4U, ScalingMatrix4x4V,
                            sd->m_pSeqParamSet->qpprime_y_zero_transform_bypass_flag);
                    }
                }
            }
            else
            {
                if (scoeff_pred_flag)
                {
                    AddCoeffs(sd, lumaQP, QPChromaU);
                }
                else
                {
                    DequantSaveCoeffs(sd, lumaQP, QPChromaU);
                }
            }

            if ((!pGetMBResidualPredictionFlag(sd->m_cur_mb.GlobalMacroblockInfo) && sd->m_use_base_residual))
            {
                IppiSize roiSize16 = {16, 16};
                IppiSize roiSize8 = {8, 8};

                ippiSet_16s_C1R(0, sd->m_pYResidual + offsetY, uPitchLuma*2 << fdf, roiSize16);
                ippiSet_16s_C1R(0, sd->m_pUResidual + offsetC, uPitchChroma*2 << fdf, roiSize8);
                ippiSet_16s_C1R(0, sd->m_pVResidual + offsetC, uPitchChroma*2 << fdf, roiSize8);
            }

            if (sd->m_reconstruct_all || (sd->m_bIsMaxQId && sd->m_next_spatial_resolution_change))
            {
                if (is_high_profile)
                {
                    if (pGetMB8x8TSFlag(sd->m_cur_mb.GlobalMacroblockInfo))
                    {
                        ReconstructLumaBaseModeResidual_8x8(sd, offsetY, uPitchLuma << fdf,
                            sd->m_use_base_residual);
                    }
                    else
                    {
                        ReconstructLumaBaseModeResidual(sd, offsetY, uPitchLuma << fdf,
                            sd->m_use_base_residual);
                    }
                }
                else
                {
                    ReconstructLumaBaseModeResidual(sd, offsetY, uPitchLuma << fdf,
                        sd->m_use_base_residual);
                }
                if (color_format)
                {
                    ReconstructChromaBaseModeResidual(sd, offsetC, uPitchChroma << fdf,
                        sd->m_use_base_residual);
                }
            }

            if (sd->m_reconstruct_all)
            {
                AddMacroblockSat(sd->m_pYResidual + offsetY,
                    sd->m_pYPlane + offsetYrec,
                    16, uPitchLuma << fdf, pitch_luma);
                if (nv12_support) {
                    AddMacroblockSat_NV12(sd->m_pUResidual + offsetC,
                        sd->m_pUVPlane + offsetCrec,
                        8, uPitchChroma << fdf, pitch_chroma);
                    AddMacroblockSat_NV12(sd->m_pVResidual + offsetC,
                        sd->m_pUVPlane + offsetCrec + 1,
                        8, uPitchChroma << fdf, pitch_chroma);
                } else {
                    AddMacroblockSat(sd->m_pUResidual + offsetC,
                        sd->m_pUPlane + offsetCrec,
                        8, uPitchChroma << fdf, pitch_chroma);
                    AddMacroblockSat(sd->m_pVResidual + offsetC,
                        sd->m_pVPlane + offsetCrec,
                        8, uPitchChroma << fdf, pitch_chroma);
                }

                if (sd->m_is_ref_base_pic)
                {
                    IppiSize roiSize16 = {16, 16};
                    IppiSize roiSize8 = {8, 8};

                    ippiSet_16s_C1R(0, sd->m_pYResidual + offsetY, uPitchLuma*2 << fdf, roiSize16);
                    ippiSet_16s_C1R(0, sd->m_pUResidual + offsetC, uPitchChroma*2 << fdf, roiSize8);
                    ippiSet_16s_C1R(0, sd->m_pVResidual + offsetC, uPitchChroma*2 << fdf, roiSize8);
                }
            }
        }

        sd->m_pSlice->last_QP_deblock = sd->m_cur_mb.LocalMacroblockInfo->QP_deblock;

        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs =
            (Ipp16u)(sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_coeffs >> 1);
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual =
            (Ipp16u)(sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma_residual >> 1);
    }

    void ReconstructMacroblock_BSlice(H264SegmentDecoderMultiThreaded * sd)
    {
        ReconstructMacroblock_PSlice(sd);
    }
};

template <class Decoder, class Reconstructor, typename Coeffs, typename PlaneY, typename PlaneUV, Ipp32s color_format, Ipp32s is_field, bool is_high_profile>
class SegmentDecoderHP : public SegmentDecoderHPBase,
    public Decoder,
    public Reconstructor
{
public:
    typedef PlaneY * PlanePtrY;
    typedef PlaneUV * PlanePtrUV;
    typedef Coeffs *  CoeffsPtr;

    typedef UMC::SegmentDecoderHP<Decoder, Reconstructor, Coeffs,
        PlaneY, PlaneUV, color_format, is_field, is_high_profile> ThisClassType;

    Status DecodeSegmentCAVLC(Ipp32u curMB,
                              Ipp32u nBorder,
                              H264SegmentDecoderMultiThreaded * sd)
    {
        Status umcRes = UMC_OK;
        void (ThisClassType::*pDecFunc)(H264SegmentDecoderMultiThreaded *sd);

        Ipp32s MBYAdjust = 0;
        if (is_field && sd->m_field_index)
            MBYAdjust  = sd->mb_height/2;

        sd->m_CurMBAddr = curMB;

        // select decoding function
        switch (sd->m_pSliceHeader->slice_type)
        {
            // intra coded slice
        case INTRASLICE:
        case S_INTRASLICE:
            pDecFunc = &ThisClassType::DecodeMacroblock_ISlice_CAVLC;
            break;

            // predicted slice
        case PREDSLICE:
        case S_PREDSLICE:
            pDecFunc = &ThisClassType::DecodeMacroblock_PSlice_CAVLC;
            break;

            // bidirectional predicted slice
        default:
            pDecFunc = &ThisClassType::DecodeMacroblock_BSlice_CAVLC;
            break;
        };

        // reset buffer pointers to start
        // in anyways we write this buffer consistently
        sd->m_pCoeffBlocksWrite = sd->GetCoefficientsBuffer();

        // set initial macroblock coordinates
        sd->m_CurMB_X = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) % sd->mb_width);
        sd->m_CurMB_Y = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) / sd->mb_width) - MBYAdjust;
        sd->m_CurMB_Y <<= (Ipp32s) sd->m_isMBAFF;

        for (; curMB < nBorder; curMB += 1)
        {
            // align pointer to improve performance
            sd->m_pCoeffBlocksWrite = (UMC::CoeffsPtrCommon) (align_pointer<CoeffsPtr> (sd->m_pCoeffBlocksWrite, 16));

            sd->UpdateCurrentMBInfo();
            setInCropWindow(sd);

            pSetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);

            (this->*pDecFunc)(sd);

            // search for end of stream in case
            // of unknown slice sizes
            if ((sd->m_MBSkipCount <= 1) &&
                (true == sd->m_isSliceGroups))
            {
                // Check for new slice header, which can change the next MB
                // to be decoded
                if (false == sd->m_pBitStream->More_RBSP_Data())
                {
                    // align bit stream to start code
                    sd->m_CurMBAddr += 1;
                    umcRes = UMC_ERR_END_OF_STREAM;
                    break;
                }

            }   // check for new slice header

            if (false == sd->m_isSliceGroups)
            {
                // set next MB coordinates
                if (0 == sd->m_isMBAFF)
                {
                    sd->m_CurMB_X += 1;
                }
                else
                {
                    sd->m_CurMB_X += sd->m_CurMBAddr & 1;
                    sd->m_CurMB_Y ^= 1;
                }
                // set next MB addres
                sd->m_CurMBAddr += 1;
            }
            else
            {
                sd->m_CurMBAddr = sd->m_mbinfo.active_next_mb_table[sd->m_CurMBAddr];
                if (sd->m_isMBAFF) {
                    sd->m_CurMB_X = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) % sd->mb_width);
                    sd->m_CurMB_Y = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) / sd->mb_width) - MBYAdjust;
                    sd->m_CurMB_Y <<= (Ipp32s) sd->m_isMBAFF;
                    sd->m_CurMB_Y += (sd->m_CurMBAddr&1);
                } else {
                    sd->m_CurMB_X = (sd->m_CurMBAddr % sd->mb_width);
                    sd->m_CurMB_Y = (sd->m_CurMBAddr / sd->mb_width) - MBYAdjust;
                }
            }
        }

        // save bitstream variables
        sd->m_pSlice->SetStateVariables(sd->m_MBSkipCount, sd->m_QuantPrev, sd->m_prev_dquant);
        return umcRes;
    }

    virtual Status setInCropWindow(H264SegmentDecoderMultiThreaded * sd)
    {
        H264Slice *pSlice = sd->m_pSlice;
        H264DecoderLayerResizeParameter *pResizeParameter;

        if (sd->m_currentLayer == 0) {
            pSetMBCropFlag(sd->m_cur_mb.GlobalMacroblockInfo, 1);
            return UMC_OK;
        }

        pResizeParameter = sd->m_mbinfo.layerMbs[sd->m_currentLayer].m_pResizeParameter;

        if (sd->m_pSliceHeader->nal_ext.svc.no_inter_layer_pred_flag ||
            sd->m_pSliceHeader->nal_ext.svc.quality_id) {
                pSetMBCropFlag(sd->m_cur_mb.GlobalMacroblockInfo, 1);
                return UMC_OK;
        }

        pSetMBCropFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);

        if (!sd->m_isMBAFF && !pSlice->GetSliceHeader()->field_pic_flag) {
            if( ( sd->m_CurMB_X >= ( pResizeParameter->leftOffset + 15 ) / 16 ) && ( sd->m_CurMB_X < ( pResizeParameter->leftOffset + pResizeParameter->scaled_ref_layer_width  ) / 16 ) &&
                ( sd->m_CurMB_Y >= ( pResizeParameter->topOffset + 15 ) / 16 ) && ( sd->m_CurMB_Y < ( pResizeParameter->topOffset + pResizeParameter->scaled_ref_layer_height ) / 16 )   )
            {
                pSetMBCropFlag(sd->m_cur_mb.GlobalMacroblockInfo, 1);
            }
        }else {
            Ipp32s y0, y1;

            if (sd->m_isMBAFF) {
                y0 = sd->m_CurMB_Y & ~1;
            } else {
                y0 = sd->m_CurMB_Y << 1;
            }
            y1 = y0 + 1;

            if( ( sd->m_CurMB_X >= ( pResizeParameter->leftOffset + 15 ) / 16 ) && ( sd->m_CurMB_X < ( pResizeParameter->leftOffset + pResizeParameter->scaled_ref_layer_width  ) / 16 ) &&
                ( y0 >= ( pResizeParameter->topOffset + 15 ) / 16 ) && ( y1 < ( pResizeParameter->topOffset + pResizeParameter->scaled_ref_layer_height ) / 16 )   )
            {
                pSetMBCropFlag(sd->m_cur_mb.GlobalMacroblockInfo, 1);
            }
        }
        return UMC_OK;
    }

    virtual Status DecodeSegmentCAVLC_Single(Ipp32s curMB, Ipp32s nBorder,
                                             H264SegmentDecoderMultiThreaded * sd)
    {
        Status umcRes = UMC_OK;
        void (ThisClassType::*pDecFunc)(H264SegmentDecoderMultiThreaded *sd);
        void (ThisClassType::*pRecFunc)(H264SegmentDecoderMultiThreaded *sd);

        Ipp32s MBYAdjust = 0;
        if (is_field && sd->m_field_index)
            MBYAdjust  = sd->mb_height/2;

        sd->m_CurMBAddr = curMB;

        // select decoding function
        switch (sd->m_pSliceHeader->slice_type)
        {
            // intra coded slice
        case INTRASLICE:
        case S_INTRASLICE:
            pDecFunc = &ThisClassType::DecodeMacroblock_ISlice_CAVLC;
            pRecFunc = &ThisClassType::ReconstructMacroblock_ISlice;
            break;

            // predicted slice
        case PREDSLICE:
        case S_PREDSLICE:
            pDecFunc = &ThisClassType::DecodeMacroblock_PSlice_CAVLC;
            pRecFunc = &ThisClassType::ReconstructMacroblock_PSlice;
            break;

            // bidirectional predicted slice
        default:
            pDecFunc = &ThisClassType::DecodeMacroblock_BSlice_CAVLC;
            pRecFunc = &ThisClassType::ReconstructMacroblock_BSlice;
            break;
        };

        // set initial macroblock coordinates
        sd->m_CurMB_X = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) % sd->mb_width);
        sd->m_CurMB_Y = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) / sd->mb_width) - MBYAdjust;
        sd->m_CurMB_Y <<= (Ipp32s) sd->m_isMBAFF;

        for (; curMB < nBorder; curMB += 1)
        {
            // align pointer to improve performance
            sd->m_pCoeffBlocksWrite = (UMC::CoeffsPtrCommon) sd->GetCoefficientsBuffer();
            sd->m_pCoeffBlocksRead = (UMC::CoeffsPtrCommon) sd->GetCoefficientsBuffer();

            sd->UpdateCurrentMBInfo();

            setInCropWindow(sd);

            pSetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);

            START_TICK
            (this->*pDecFunc)(sd);
            END_TICK(decode_time)

            START_TICK1
            (this->*pRecFunc)(sd);
            END_TICK1(reconstruction_time)

            // search for end of stream in case
            // of unknown slice sizes
            if ((sd->m_MBSkipCount <= 1) &&
                (true == sd->m_isSliceGroups))
            {
                // Check for new slice header, which can change the next MB
                // to be decoded
                if (false == sd->m_pBitStream->More_RBSP_Data())
                {
                    // align bit stream to start code
                    sd->m_CurMBAddr += 1;
                    umcRes = UMC_ERR_END_OF_STREAM;
                    break;
                }

            }   // check for new slice header

            if (false == sd->m_isSliceGroups)
            {
                // set next MB coordinates
                if (0 == sd->m_isMBAFF)
                {
                    sd->m_CurMB_X += 1;
                }
                else
                {
                    sd->m_CurMB_X += sd->m_CurMBAddr & 1;
                    sd->m_CurMB_Y ^= 1;
                }
                // set next MB addres
                sd->m_CurMBAddr += 1;
            }
            else
            {
                sd->m_CurMBAddr = sd->m_mbinfo.active_next_mb_table[sd->m_CurMBAddr];
                if (sd->m_isMBAFF) {
                    sd->m_CurMB_X = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) % sd->mb_width);
                    sd->m_CurMB_Y = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) / sd->mb_width) - MBYAdjust;
                    sd->m_CurMB_Y <<= (Ipp32s) sd->m_isMBAFF;
                    sd->m_CurMB_Y += (sd->m_CurMBAddr&1);
                } else {
                    sd->m_CurMB_X = (sd->m_CurMBAddr % sd->mb_width);
                    sd->m_CurMB_Y = (sd->m_CurMBAddr / sd->mb_width) - MBYAdjust;
                }
            }

        }

        // save bitstream variables
        sd->m_pSlice->SetStateVariables(sd->m_MBSkipCount, sd->m_QuantPrev, sd->m_prev_dquant);
        return umcRes;
    }

    virtual Status Reconstruct_DXVA_Single(Ipp32s curMB, Ipp32s nBorder,
                                             H264SegmentDecoderMultiThreaded * sd)
    {
        void (ThisClassType::*pRecFunc)(H264SegmentDecoderMultiThreaded *sd);

        Ipp32s MBYAdjust = 0;
        if (is_field && sd->m_field_index)
            MBYAdjust  = sd->mb_height/2;

        sd->m_CurMBAddr = curMB;

        // select decoding function
        switch (sd->m_pSliceHeader->slice_type)
        {
            // intra coded slice
        case INTRASLICE:
        case S_INTRASLICE:
            pRecFunc = &ThisClassType::ReconstructMacroblock_ISlice;
            break;

            // predicted slice
        case PREDSLICE:
        case S_PREDSLICE:
            pRecFunc = &ThisClassType::ReconstructMacroblock_PSlice;
            break;

            // bidirectional predicted slice
        default:
            pRecFunc = &ThisClassType::ReconstructMacroblock_BSlice;
            break;
        };

        // set initial macroblock coordinates
        sd->m_CurMB_X = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) % sd->mb_width);
        sd->m_CurMB_Y = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) / sd->mb_width) - MBYAdjust;
        sd->m_CurMB_Y <<= (Ipp32s) sd->m_isMBAFF;

        for (; curMB < nBorder; curMB += 1)
        {
            sd->UpdateCurrentMBInfo();
            //sd->UpdateNeighbouringAddresses();
            //ColorSpecific<color_format, Coeffs, PlaneY, PlaneUV, is_field>::UpdateNeighbouringBlocks(sd);

            (this->*pRecFunc)(sd);

            if (false == sd->m_isSliceGroups)
            {
                // set next MB coordinates
                if (0 == sd->m_isMBAFF)
                    sd->m_CurMB_X += 1;
                else
                {
                    sd->m_CurMB_X += sd->m_CurMBAddr & 1;
                    sd->m_CurMB_Y ^= 1;
                }
                // set next MB addres
                sd->m_CurMBAddr += 1;
            }
            else
            {
                sd->m_CurMBAddr = sd->m_mbinfo.active_next_mb_table[sd->m_CurMBAddr];
                sd->m_CurMB_X = (sd->m_CurMBAddr % sd->mb_width);
                sd->m_CurMB_Y = (sd->m_CurMBAddr / sd->mb_width) - MBYAdjust;
            }
        }

        return UMC_OK;
    }

    Status DecodeSegmentCABAC(Ipp32u curMB,
                              Ipp32u nBorder,
                              H264SegmentDecoderMultiThreaded *sd)
    {
        Status umcRes = UMC_OK;
        void (ThisClassType::*pDecFunc)(H264SegmentDecoderMultiThreaded *sd);

        Ipp32s MBYAdjust = 0;
        if (is_field && sd->m_field_index)
            MBYAdjust  = sd->mb_height / 2;

        sd->m_CurMBAddr = curMB;

        // select decoding function
        switch (sd->m_pSliceHeader->slice_type)
        {
            // intra coded slice
        case INTRASLICE:
        case S_INTRASLICE:
            pDecFunc = &ThisClassType::DecodeMacroblock_ISlice_CABAC;
            break;

            // predicted slice
        case PREDSLICE:
        case S_PREDSLICE:
            pDecFunc = &ThisClassType::DecodeMacroblock_PSlice_CABAC;
            break;

            // bidirectional predicted slice
        default:
            pDecFunc = &ThisClassType::DecodeMacroblock_BSlice_CABAC;
            break;
        };

        // reset buffer pointers to start
        // in anyways we write this buffer consistently
        sd->m_pCoeffBlocksWrite = sd->GetCoefficientsBuffer();

        // set initial macroblock coordinates
        sd->m_CurMB_X = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) % sd->mb_width);
        sd->m_CurMB_Y = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) / sd->mb_width) - MBYAdjust;
        sd->m_CurMB_Y <<= (Ipp32s) sd->m_isMBAFF;

        for (; curMB < nBorder; curMB += 1)
        {
            // align pointer to improve performance
            sd->m_pCoeffBlocksWrite = (UMC::CoeffsPtrCommon) (align_pointer<CoeffsPtr> (sd->m_pCoeffBlocksWrite, 16));

            sd->UpdateCurrentMBInfo();
            setInCropWindow(sd);

            pSetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);

            (this->*pDecFunc)(sd);

            // decode end of slice
            {
                Ipp32s end_of_slice;

                if ((0 == sd->m_isMBAFF) ||
                    (sd->m_CurMBAddr & 1))
                    end_of_slice = sd->m_pBitStream->DecodeSymbolEnd_CABAC();
                else
                    end_of_slice = 0;

                if (end_of_slice)
                {
                    sd->m_pBitStream->TerminateDecode_CABAC();

                    sd->m_CurMBAddr += 1;
                    umcRes = UMC_ERR_END_OF_STREAM;
                    break;
                }
            }

            if (false == sd->m_isSliceGroups)
            {
                // set next MB coordinates
                if (0 == sd->m_isMBAFF)
                    sd->m_CurMB_X += 1;
                else
                {
                    sd->m_CurMB_X += sd->m_CurMBAddr & 1;
                    sd->m_CurMB_Y ^= 1;
                }
                // set next MB addres
                sd->m_CurMBAddr += 1;
            }
            else
            {
                sd->m_CurMBAddr = sd->m_mbinfo.active_next_mb_table[sd->m_CurMBAddr];

                if (sd->m_isMBAFF) {
                    sd->m_CurMB_X = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) % sd->mb_width);
                    sd->m_CurMB_Y = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) / sd->mb_width) - MBYAdjust;
                    sd->m_CurMB_Y <<= (Ipp32s) sd->m_isMBAFF;
                    sd->m_CurMB_Y += (sd->m_CurMBAddr&1);
                } else {
                    sd->m_CurMB_X = (sd->m_CurMBAddr % sd->mb_width);
                    sd->m_CurMB_Y = (sd->m_CurMBAddr / sd->mb_width) - MBYAdjust;
                }
            }
        }

        // save bitstream variables
        sd->m_pSlice->SetStateVariables(sd->m_MBSkipCount, sd->m_QuantPrev, sd->m_prev_dquant);
        return umcRes;
    }

    virtual Status DecodeSegmentCABAC_Single(Ipp32s curMB, Ipp32s nBorder,
                                             H264SegmentDecoderMultiThreaded * sd)
    {
        Status umcRes = UMC_OK;
        void (ThisClassType::*pDecFunc)(H264SegmentDecoderMultiThreaded *sd);
        void (ThisClassType::*pRecFunc)(H264SegmentDecoderMultiThreaded *sd);

        Ipp32s MBYAdjust = 0;
        if (is_field && sd->m_field_index)
            MBYAdjust  = sd->mb_height / 2;

        sd->m_CurMBAddr = curMB;

        // select decoding function
        switch (sd->m_pSliceHeader->slice_type)
        {
            // intra coded slice
        case INTRASLICE:
        case S_INTRASLICE:
            pDecFunc = &ThisClassType::DecodeMacroblock_ISlice_CABAC;
            pRecFunc = &ThisClassType::ReconstructMacroblock_ISlice;
            break;

            // predicted slice
        case PREDSLICE:
        case S_PREDSLICE:
            pDecFunc = &ThisClassType::DecodeMacroblock_PSlice_CABAC;
            pRecFunc = &ThisClassType::ReconstructMacroblock_PSlice;
            break;

            // bidirectional predicted slice
        default:
            pDecFunc = &ThisClassType::DecodeMacroblock_BSlice_CABAC;
            pRecFunc = &ThisClassType::ReconstructMacroblock_BSlice;
            break;
        };

        // set initial macroblock coordinates
        sd->m_CurMB_X = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) % sd->mb_width);
        sd->m_CurMB_Y = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) / sd->mb_width) - MBYAdjust;
        sd->m_CurMB_Y <<= (Ipp32s) sd->m_isMBAFF;

        for (; curMB < nBorder; curMB += 1)
        {
            // reset buffer pointers to start
            // in anyways we write this buffer consistently
            // align pointer to improve performance
            sd->m_pCoeffBlocksRead  = sd->m_pCoeffBlocksWrite = (UMC::CoeffsPtrCommon)sd->GetCoefficientsBuffer();
            sd->UpdateCurrentMBInfo();

            setInCropWindow(sd);

            pSetMB8x8TSDecFlag(sd->m_cur_mb.GlobalMacroblockInfo, 0);

            START_TICK
            (this->*pDecFunc)(sd);
            END_TICK(decode_time)

            START_TICK1
            (this->*pRecFunc)(sd);
            END_TICK1(reconstruction_time)

            // decode end of slice
            if (!sd->m_pSliceHeader->slice_skip_flag)
            {
                Ipp32s end_of_slice;

                if ((0 == sd->m_isMBAFF) ||
                    (sd->m_CurMBAddr & 1))
                    end_of_slice = sd->m_pBitStream->DecodeSymbolEnd_CABAC();
                else
                    end_of_slice = 0;

                if (end_of_slice)
                {
                    sd->m_pBitStream->TerminateDecode_CABAC();

                    sd->m_CurMBAddr += 1;
                    umcRes = UMC_ERR_END_OF_STREAM;
                    break;
                }
            }

            if (false == sd->m_isSliceGroups)
            {
                // set next MB coordinates
                if (0 == sd->m_isMBAFF)
                    sd->m_CurMB_X += 1;
                else
                {
                    sd->m_CurMB_X += sd->m_CurMBAddr & 1;
                    sd->m_CurMB_Y ^= 1;
                }
                // set next MB addres
                sd->m_CurMBAddr += 1;
            }
            else
            {
                sd->m_CurMBAddr = sd->m_mbinfo.active_next_mb_table[sd->m_CurMBAddr];

                if (sd->m_isMBAFF) {
                    sd->m_CurMB_X = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) % sd->mb_width);
                    sd->m_CurMB_Y = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) / sd->mb_width) - MBYAdjust;
                    sd->m_CurMB_Y <<= (Ipp32s) sd->m_isMBAFF;
                    sd->m_CurMB_Y += (sd->m_CurMBAddr&1);
                } else {
                    sd->m_CurMB_X = (sd->m_CurMBAddr % sd->mb_width);
                    sd->m_CurMB_Y = (sd->m_CurMBAddr / sd->mb_width) - MBYAdjust;
                }
            }
        }

        // save bitstream variables
        sd->m_pSlice->SetStateVariables(sd->m_MBSkipCount, sd->m_QuantPrev, sd->m_prev_dquant);

        return umcRes;
    }

    virtual Status ReconstructSegment(Ipp32u curMB, Ipp32u nBorder,
                                      H264SegmentDecoderMultiThreaded * sd)
    {
        Status umcRes = UMC_OK;
        void (ThisClassType::*pRecFunc)(H264SegmentDecoderMultiThreaded *sd);

        Ipp32s MBYAdjust = 0;
        if (is_field && sd->m_field_index)
            MBYAdjust  = sd->mb_height/2;

        sd->m_CurMBAddr = curMB;

        // reset buffer pointers to start
        // in anyways we read this buffer consistently
        sd->m_pCoeffBlocksRead = sd->GetCoefficientsBuffer();

        // select reconstruct function
        switch (sd->m_pSliceHeader->slice_type)
        {
            // intra coded slice
        case INTRASLICE:
        case S_INTRASLICE:
            pRecFunc = &ThisClassType::ReconstructMacroblock_ISlice;
            break;

            // predicted slice
        case PREDSLICE:
        case S_PREDSLICE:
            pRecFunc = &ThisClassType::ReconstructMacroblock_PSlice;
            break;

            // bidirectional predicted slice
        default:
            pRecFunc = &ThisClassType::ReconstructMacroblock_BSlice;
            break;
        };

        // set initial macroblock coordinates
        sd->m_CurMB_X = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) % sd->mb_width);
        sd->m_CurMB_Y = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) / sd->mb_width) - MBYAdjust;
        sd->m_CurMB_Y <<= (Ipp32s) sd->m_isMBAFF;

        // reconstruct starts here
        for (;curMB < nBorder; curMB += 1)
        {
            // align pointer to improve performance
            sd->m_pCoeffBlocksRead = (UMC::CoeffsPtrCommon) (align_pointer<CoeffsPtr> (sd->m_pCoeffBlocksRead, 16));

            sd->UpdateCurrentMBInfo();

            (this->*pRecFunc)(sd);

            if (false == sd->m_isSliceGroups)
            {
                // set next MB coordinates
                if (0 == sd->m_isMBAFF)
                    sd->m_CurMB_X += 1;
                else
                {
                    sd->m_CurMB_X += sd->m_CurMBAddr & 1;
                    sd->m_CurMB_Y ^= 1;
                }
                // set next MB addres
                sd->m_CurMBAddr += 1;
            }
            else
            {
                sd->m_CurMBAddr = sd->m_mbinfo.active_next_mb_table[sd->m_CurMBAddr];

                if (sd->m_isMBAFF) {
                    sd->m_CurMB_X = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) % sd->mb_width);
                    sd->m_CurMB_Y = ((sd->m_CurMBAddr >> (Ipp32s) sd->m_isMBAFF) / sd->mb_width) - MBYAdjust;
                    sd->m_CurMB_Y <<= (Ipp32s) sd->m_isMBAFF;
                    sd->m_CurMB_Y += (sd->m_CurMBAddr&1);
                } else {
                    sd->m_CurMB_X = (sd->m_CurMBAddr % sd->mb_width);
                    sd->m_CurMB_Y = (sd->m_CurMBAddr / sd->mb_width) - MBYAdjust;
                }
            }
        }

        return umcRes;
    }

    virtual void RestoreErrorPlane(PlanePtrY pRefPlane, PlanePtrY pCurrentPlane, Ipp32s pitch,
        Ipp32s offsetX, Ipp32s offsetY, Ipp32s offsetXL, Ipp32s offsetYL,
        Ipp32s mb_width, Ipp32s fieldOffset, IppiSize mbSize)
    {
            IppiSize roiSize;

            roiSize.height = mbSize.height;
            roiSize.width = mb_width * mbSize.width - offsetX;
            Ipp32s offset = offsetX + offsetY*pitch;

            if (offsetYL == offsetY)
            {
                roiSize.width = offsetXL - offsetX + mbSize.width;
            }

            if (pRefPlane)
            {
                CopyPlane(pRefPlane + offset + (fieldOffset*pitch >> 1),
                            pitch,
                            pCurrentPlane + offset + (fieldOffset*pitch >> 1),
                            pitch,
                            roiSize);
            }
            else
            {
                SetPlane(128, pCurrentPlane + offset + (fieldOffset*pitch >> 1), pitch, roiSize);
            }

            if (offsetYL > offsetY)
            {
                roiSize.height = mbSize.height;
                roiSize.width = offsetXL + mbSize.width;
                offset = offsetYL*pitch;

                if (pRefPlane)
                {
                    CopyPlane(pRefPlane + offset + (fieldOffset*pitch >> 1),
                                pitch,
                                pCurrentPlane + offset + (fieldOffset*pitch >> 1),
                                pitch,
                                roiSize);
                }
                else
                {
                    SetPlane(128, pCurrentPlane + offset + (fieldOffset*pitch >> 1), pitch, roiSize);
                }
            }

            if (offsetYL - offsetY > mbSize.height)
            {
                roiSize.height = offsetYL - offsetY - mbSize.height;
                roiSize.width = mb_width * mbSize.width;
                offset = (offsetY + mbSize.height)*pitch;

                if (pRefPlane)
                {
                    CopyPlane(pRefPlane + offset + (fieldOffset*pitch >> 1),
                                pitch,
                                pCurrentPlane + offset + (fieldOffset*pitch >> 1),
                                pitch,
                                roiSize);
                }
                else
                {
                    SetPlane(128, pCurrentPlane + offset + (fieldOffset*pitch >> 1), pitch, roiSize);
                }
            }
    }

    virtual void RestoreErrorRect(Ipp32s startMb, Ipp32s endMb, H264DecoderFrame *pRefFrame,
        H264SegmentDecoderMultiThreaded * sd)
    {
        if (startMb > 0)
            startMb--;

        if (startMb >= endMb || sd->m_isSliceGroups)
            return;

        H264DecoderFrame * pCurrentFrame = sd->m_pSlice->GetCurrentFrame();
        sd->mb_height = sd->m_pSlice->GetMBHeight();
        sd->mb_width = sd->m_pSlice->GetMBWidth();

        Ipp32s pitch_luma = pCurrentFrame->pitch_luma();
        Ipp32s pitch_chroma = pCurrentFrame->pitch_chroma();

        Ipp32s fieldOffset = 0;

        Ipp32s MBYAdjust = 0;

        if (FRM_STRUCTURE > pCurrentFrame->m_PictureStructureForDec)
        {
            if (sd->m_pSlice->IsBottomField())
            {
                fieldOffset = 1;
                startMb += sd->mb_width*sd->mb_height / 2;
                endMb += sd->mb_width*sd->mb_height / 2;
                MBYAdjust = sd->mb_height / 2;
            }

            pitch_luma *= 2;
            pitch_chroma *= 2;
        }

        Ipp32s offsetX, offsetY;
        offsetX = (startMb % sd->mb_width) * 16;
        offsetY = ((startMb / sd->mb_width) - MBYAdjust) * 16;

        Ipp32s offsetXL = ((endMb - 1) % sd->mb_width) * 16;
        Ipp32s offsetYL = (((endMb - 1) / sd->mb_width) - MBYAdjust) * 16;

        IppiSize mbSize;

        mbSize.width = 16;
        mbSize.height = 16;

        if (pRefFrame && pRefFrame->m_pYPlane)
        {
            RestoreErrorPlane((PlanePtrY)pRefFrame->m_pYPlane, (PlanePtrY)pCurrentFrame->m_pYPlane, pitch_luma,
                    offsetX, offsetY, offsetXL, offsetYL,
                    sd->mb_width, fieldOffset, mbSize);
        }
        else
        {
            RestoreErrorPlane(0, (PlanePtrY)pCurrentFrame->m_pYPlane, pitch_luma,
                    offsetX, offsetY, offsetXL, offsetYL,
                    sd->mb_width, fieldOffset, mbSize);
        }

        bool nv12_support = (pCurrentFrame->GetColorFormat() == NV12);
        if (nv12_support)
        {
            offsetY >>= 1;
            offsetYL >>= 1;

            mbSize.height >>= 1;

            if (pRefFrame && pRefFrame->m_pUVPlane)
            {
                RestoreErrorPlane((PlanePtrUV)pRefFrame->m_pUVPlane, (PlanePtrUV)pCurrentFrame->m_pUVPlane, pitch_chroma,
                        offsetX, offsetY, offsetXL, offsetYL,
                        sd->mb_width, fieldOffset, mbSize);
            }
            else
            {
                RestoreErrorPlane(0, (PlanePtrUV)pCurrentFrame->m_pUVPlane, pitch_chroma,
                        offsetX, offsetY, offsetXL, offsetYL,
                        sd->mb_width, fieldOffset, mbSize);
            }
        }
        else
        {
            switch (pCurrentFrame->m_chroma_format)
            {
            case CHROMA_FORMAT_420: // YUV420
                offsetX >>= 1;
                offsetY >>= 1;
                offsetXL >>= 1;
                offsetYL >>= 1;

                mbSize.width >>= 1;
                mbSize.height >>= 1;

                break;
            case CHROMA_FORMAT_422: // YUV422
                offsetX >>= 1;
                offsetXL >>= 1;

                mbSize.width >>= 1;
                break;
            case CHROMA_FORMAT_444: // YUV444
                break;

            case CHROMA_FORMAT_400: // YUV400
                return;
            default:
                VM_ASSERT(false);
                return;
            }

            if (pRefFrame && pRefFrame->m_pUPlane && pRefFrame->m_pVPlane)
            {
                RestoreErrorPlane((PlanePtrUV)pRefFrame->m_pUPlane, (PlanePtrUV)pCurrentFrame->m_pUPlane, pitch_chroma,
                        offsetX, offsetY, offsetXL, offsetYL,
                        sd->mb_width, fieldOffset, mbSize);
                RestoreErrorPlane((PlanePtrUV)pRefFrame->m_pVPlane, (PlanePtrUV)pCurrentFrame->m_pVPlane, pitch_chroma,
                        offsetX, offsetY, offsetXL, offsetYL,
                        sd->mb_width, fieldOffset, mbSize);
            }
            else
            {
                RestoreErrorPlane(0, (PlanePtrUV)pCurrentFrame->m_pUPlane, pitch_chroma,
                        offsetX, offsetY, offsetXL, offsetYL,
                        sd->mb_width, fieldOffset, mbSize);
                RestoreErrorPlane(0, (PlanePtrUV)pCurrentFrame->m_pVPlane, pitch_chroma,
                        offsetX, offsetY, offsetXL, offsetYL,
                        sd->mb_width, fieldOffset, mbSize);
            }
        }
    }
};

template <typename Coeffs, typename PlaneY, typename PlaneUV, bool is_field, Ipp32s color_format, bool is_high_profile>
class CreateSoftSegmentDecoderWrapper
{
public:

    static SegmentDecoderHPBase* CreateSegmentDecoder()
    {
        static SegmentDecoderHP<
            MBDecoder<Coeffs, PlaneY, PlaneUV, color_format, is_field, is_high_profile>,
            MBReconstructor<Coeffs, PlaneY, PlaneUV, color_format, is_field, is_high_profile>,
            Coeffs, PlaneY, PlaneUV, color_format, is_field, is_high_profile> k;
        return &k;
    }
};

template <typename Coeffs, typename PlaneY, typename PlaneUV, bool is_field>
class CreateSegmentDecoderWrapper
{
public:

    static SegmentDecoderHPBase* CreateSoftSegmentDecoder(Ipp32s color_format, bool is_high_profile)
    {
        static SegmentDecoderHPBase* global_sds_array[4][2] = {0};

        if (global_sds_array[0][0] == 0)
        {
#undef INIT
#define INIT(cf, hp) global_sds_array[cf][hp] = CreateSoftSegmentDecoderWrapper<Coeffs, PlaneY, PlaneUV, is_field, cf, hp>::CreateSegmentDecoder();

            INIT(3, true);
            INIT(2, true);
            INIT(1, true);
            INIT(0, true);
            INIT(3, false);
            INIT(2, false);
            INIT(1, false);
            INIT(0, false);
        }

        return global_sds_array[color_format][is_high_profile];
    }
};

#pragma warning(default: 4127)

// declare functions for creating proper decoders
extern
SegmentDecoderHPBase* CreateSD(Ipp32s bit_depth_luma,
                               Ipp32s bit_depth_chroma,
                               bool is_field,
                               Ipp32s color_format,
                               bool is_high_profile);
extern
SegmentDecoderHPBase* CreateSD_ManyBits(Ipp32s bit_depth_luma,
                                        Ipp32s bit_depth_chroma,
                                        bool is_field,
                                        Ipp32s color_format,
                                        bool is_high_profile);

} // end namespace UMC

#endif // __UMC_H264_SEGMENT_DECODER_TEMPLATES_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
