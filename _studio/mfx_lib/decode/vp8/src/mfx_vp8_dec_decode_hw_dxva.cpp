//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined(MFX_ENABLE_VP8_VIDEO_DECODE_HW) && (defined(MFX_VA_WIN) || defined(MFX_VA))

#include "mfx_session.h"
#include "mfx_common_decode_int.h"
#include "mfx_vp8_dec_decode_hw.h"
#include "mfx_enc_common.h"

#include "umc_va_base.h"
#include "umc_va_dxva2.h"

#include "vm_sys_info.h"

#ifndef MFX_VA_WIN

#include <va/va.h>
#include <va/va_dec_vp8.h>

#endif

#include <iostream>
#include <sstream>
#include <fstream>

#include "mfx_vp8_dec_decode_common.h"

#ifdef MFX_VA_WIN

mfxStatus VideoDECODEVP8_HW::PackHeaders(mfxBitstream *p_bistream)
{

    using namespace VP8Defs;

    UMCVACompBuffer* compBufPic;
    VP8_DECODE_PICTURE_PARAMETERS *picParams = (VP8_DECODE_PICTURE_PARAMETERS*)m_p_video_accelerator->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS, &compBufPic);
    memset(picParams, 0, sizeof(VP8_DECODE_PICTURE_PARAMETERS));

    picParams->wFrameWidthInMbsMinus1 = (USHORT)(((m_frame_info.frameSize.width + 15) / 16) - 1);
    picParams->wFrameHeightInMbsMinus1 = (USHORT)(((m_frame_info.frameSize.height + 15) / 16) - 1);

    sFrameInfo info = m_frames.back();

    picParams->CurrPicIndex = (UCHAR)info.currIndex;

    if(m_frame_info.frameType == I_PICTURE)
    {
        picParams->key_frame = 1;

        picParams->LastRefPicIndex = (UCHAR)0xff;
        picParams->GoldenRefPicIndex = (UCHAR)0xff;
        picParams->AltRefPicIndex = (UCHAR)0xff;
        picParams->DeblockedPicIndex = (UCHAR)0xff;
    }
    else // inter frame
    {
        picParams->key_frame = 0;

        picParams->LastRefPicIndex = (UCHAR)info.lastrefIndex;
        picParams->GoldenRefPicIndex = (UCHAR)info.goldIndex;
        picParams->AltRefPicIndex = (UCHAR)info.altrefIndex;
        picParams->DeblockedPicIndex = (UCHAR)0xff;
    }

    picParams->version = m_frame_info.version;
    picParams->segmentation_enabled = m_frame_info.segmentationEnabled;
    picParams->update_mb_segmentation_map = m_frame_info.updateSegmentMap;
    picParams->update_segment_feature_data = m_frame_info.updateSegmentData;
    picParams->filter_type = m_frame_info.loopFilterType;
    picParams->sharpness_level = m_frame_info.sharpnessLevel;
    picParams->loop_filter_adj_enable = m_frame_info.mbLoopFilterAdjust;
    picParams->mode_ref_lf_delta_update = m_frame_info.modeRefLoopFilterDeltaUpdate;

    if (m_frame_info.frameType != I_PICTURE)
    {
        picParams->sign_bias_golden = m_refresh_info.refFrameBiasTable[3];
        picParams->sign_bias_alternate = m_refresh_info.refFrameBiasTable[2];
    }

    picParams->mb_no_coeff_skip = m_frame_info.mbSkipEnabled;
    picParams->LoopFilterDisable = 0;
    if (0 == m_frame_info.loopFilterLevel || (2 == m_frame_info.version || 3 == m_frame_info.version))
    {
        picParams->LoopFilterDisable = 1;
    }

    picParams->mb_segment_tree_probs[0] = m_frame_info.segmentTreeProbabilities[0];
    picParams->mb_segment_tree_probs[1] = m_frame_info.segmentTreeProbabilities[1];
    picParams->mb_segment_tree_probs[2] = m_frame_info.segmentTreeProbabilities[2];

    /*

    // partition 0 is always MB header this partition always exists. there is no need for a flag to indicate this.
    // if CodedCoeffTokenPartition == 0, it means Partition #1 also exists. That is, there is a total of 2 partitions.
    // if CodedCoeffTokenPartition == 1, it means Partition #1-2 also exists. That is, there is a total of 3 partitions.
    // if CodedCoeffTokenPartition == 2, it means Partition #1-4 also exists. That is, there is a total of 5 partitions.
    // if CodedCoeffTokenPartition == 3, it means Partition #1-8 also exists. That is, there is a total of 9 partitions.

    //picParams->CodedCoeffTokenPartition = m_frame_info.numPartitions - 1; // or m_frame_info.numTokenPartitions

    */

    picParams->CodedCoeffTokenPartition = m_CodedCoeffTokenPartition;

    if (m_frame_info.segmentationEnabled)
    {

        for (int i = 0; i < 4; i++)
        {
            if (m_frame_info.segmentAbsMode)
                picParams->loop_filter_level[i] = m_frame_info.segmentFeatureData[VP8_ALT_LOOP_FILTER][i];
            else
            {
                picParams->loop_filter_level[i] = m_frame_info.loopFilterLevel + m_frame_info.segmentFeatureData[VP8_ALT_LOOP_FILTER][i];
                picParams->loop_filter_level[i] = (picParams->loop_filter_level[i] >= 0) ?
                    ((picParams->loop_filter_level[i] <= 63) ? picParams->loop_filter_level[i] : 63) : 0;
            }
        }

    }
    else
    {
        picParams->loop_filter_level[0] = m_frame_info.loopFilterLevel;
        picParams->loop_filter_level[1] = m_frame_info.loopFilterLevel;
        picParams->loop_filter_level[2] = m_frame_info.loopFilterLevel;
        picParams->loop_filter_level[3] = m_frame_info.loopFilterLevel;
    }

    for (Ipp32u i = 0; i < VP8_NUM_OF_REF_FRAMES; i += 1)
    {
        picParams->ref_lf_delta[i] = m_frame_info.refLoopFilterDeltas[i];
    }

    for (Ipp32u i = 0; i < VP8_NUM_OF_MODE_LF_DELTAS; i += 1)
    {
        picParams->mode_lf_delta[i] = m_frame_info.modeLoopFilterDeltas[i];
    }

    picParams->prob_skip_false = m_frame_info.skipFalseProb;
    picParams->prob_intra = m_frame_info.intraProb;
    picParams->prob_last = m_frame_info.lastProb;
    picParams->prob_golden = m_frame_info.goldProb;

    const mfxU8 *prob_y_table;
    const mfxU8 *prob_uv_table;

    if (I_PICTURE == m_frame_info.frameType)
    {
        prob_y_table = vp8_kf_mb_mode_y_probs;
        prob_uv_table = vp8_kf_mb_mode_uv_probs;
    }
    else
    {
        prob_y_table = m_frameProbs.mbModeProbY;
        prob_uv_table = m_frameProbs.mbModeProbUV;
    }

    for (Ipp32u i = 0; i < VP8_NUM_MB_MODES_Y - 1; i += 1)
    {
        picParams->y_mode_probs[i] = prob_y_table[i];
    }

    for (Ipp32u i = 0; i < VP8_NUM_MB_MODES_UV - 1; i += 1)
    {
        picParams->uv_mode_probs[i] = prob_uv_table[i];
    }

    for (Ipp32u i = 0; i < VP8_NUM_MV_PROBS; i += 1)
    {
        picParams->mv_update_prob[0][i] = m_frameProbs.mvContexts[0][i];
        picParams->mv_update_prob[1][i] = m_frameProbs.mvContexts[1][i];
    }

    picParams->FirstMbBitOffset = m_frame_info.entropyDecSize;
    picParams->P0EntropyCount = (UCHAR) (8 - (m_boolDecoder[0].bitcount() & 0x07));
    picParams->P0EntropyRange = (UINT)m_boolDecoder[0].range();
    picParams->P0EntropyValue = (UCHAR)((m_boolDecoder[0].value() >> 24) & 0xff);

    picParams->PartitionSize[0] = m_frame_info.firstPartitionSize;

    for (Ipp32s i = 1; i < m_frame_info.numPartitions + 1; i += 1)
    {
        picParams->PartitionSize[i] = m_frame_info.partitionSize[i - 1];
    }

    compBufPic->SetDataSize(sizeof(VP8_DECODE_PICTURE_PARAMETERS));

    ////////////////////////////////////////////////////////////////
    UMCVACompBuffer* compBufCp;
    Ipp8u*coeffProbs = (Ipp8u*)m_p_video_accelerator->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_VP8_COEFFICIENT_PROBABILITIES, &compBufCp);

    // [4][8][3][11]
    memcpy_s(coeffProbs, sizeof(m_frameProbs.coeff_probs), m_frameProbs.coeff_probs, sizeof(m_frameProbs.coeff_probs));
    compBufCp->SetDataSize(sizeof(m_frameProbs.coeff_probs));

    ////////////////////////////////////////////////////////////////

    UMCVACompBuffer* compBufQm;
    VP8_DECODE_QM_TABLE *qmTable = (VP8_DECODE_QM_TABLE*)m_p_video_accelerator->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX, &compBufQm);

    if (m_frame_info.segmentationEnabled == 0)
    {
        // when segmentation is disabled, use the first entry 0 for the quantization values
        qmTable->Qvalue[0][0] = (USHORT)m_quantInfo.ydcQ[0];
        qmTable->Qvalue[0][1] = (USHORT)m_quantInfo.yacQ[0];
        qmTable->Qvalue[0][2] = (USHORT)m_quantInfo.uvdcQ[0];
        qmTable->Qvalue[0][3] = (USHORT)m_quantInfo.uvacQ[0];
        qmTable->Qvalue[0][4] = (USHORT)m_quantInfo.y2dcQ[0];
        qmTable->Qvalue[0][5] = (USHORT)m_quantInfo.y2acQ[0];
    }
    else
    {
        for (Ipp32u i = 0; i < 4; i += 1)
        {
            qmTable->Qvalue[i][0] = (USHORT)m_quantInfo.ydcQ[i];
            qmTable->Qvalue[i][1] = (USHORT)m_quantInfo.yacQ[i];
            qmTable->Qvalue[i][2] = (USHORT)m_quantInfo.uvdcQ[i];
            qmTable->Qvalue[i][3] = (USHORT)m_quantInfo.uvacQ[i];
            qmTable->Qvalue[i][4] = (USHORT)m_quantInfo.y2dcQ[i];
            qmTable->Qvalue[i][5] = (USHORT)m_quantInfo.y2acQ[i];
        }
    }

    compBufQm->SetDataSize(sizeof(VP8_DECODE_QM_TABLE));

    //////////////////////////////////////////////////////////////////

    UMCVACompBuffer* compBufBs;
    Ipp8u *bistreamData = (Ipp8u *)m_p_video_accelerator->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_BITSTREAM_DATA, &compBufBs);
    Ipp8u *pBuffer = (Ipp8u*)p_bistream->Data;
    Ipp32s size = p_bistream->DataLength;
    Ipp32u offset = 0;

    if (m_frame_info.frameType == I_PICTURE)
    {
        offset = 10;
    }
    else
    {
        offset = 3;
    }

    memcpy_s(bistreamData, size - offset, pBuffer + offset, size - offset);
    compBufBs->SetDataSize((Ipp32s)size - offset);

    return MFX_ERR_NONE;

} // Status VP8VideoDecoderHardware::PackHeaders(MediaData* src)

#endif

#endif