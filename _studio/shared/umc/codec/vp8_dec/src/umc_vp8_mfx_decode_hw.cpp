//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2014 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_VP8_VIDEO_DECODER)
#include <string.h>
#include "vm_debug.h"
#include "umc_video_data.h"

#include "umc_va_base.h"
#include "umc_vp8_mfx_decode_hw.h"

#ifdef UMC_VA_DXVA

#include "umc_va_dxva2.h"

VP8VideoDecoderHardware::VP8VideoDecoderHardware(void)
{

    m_pVideoAccelerator = NULL;

} // VP8VideoDecoderHardware::VP8VideoDecoderHardware(void)

VP8VideoDecoderHardware::~VP8VideoDecoderHardware(void)
{
    Close();

} // VP8VideoDecoderHardware::~VP8VideoDecoderHardware(void)

Status VP8VideoDecoderHardware::Close(void)
{
    return VP8VideoDecoderSoftware::Close();

} // Status VP8VideoDecoderHardware::Close(void)

Status VP8VideoDecoderHardware::Reset(void)
{
    return VP8VideoDecoderSoftware::Reset();

} // Status VP8VideoDecoderHardware::Reset(void)

Status VP8VideoDecoderHardware::Init(BaseCodecParams *pInit)
{

    VideoDecoderParams* pDecoderParams;
    pDecoderParams = DynamicCast<VideoDecoderParams>(pInit);

    if(!pDecoderParams) return UMC_ERR_FAILED;

    m_pVideoAccelerator = pDecoderParams->pVideoAccelerator;

    return VP8VideoDecoderSoftware::Init(pInit);

} // Status VP8VideoDecoderHardware::Init(BaseCodecParams *pInit)

Status VP8VideoDecoderHardware::GetFrame(MediaData* in, FrameData** out)
{
    Status sts = UMC_OK;

    sts = DecodeFrameHeader(in);
    UMC_CHECK_STATUS(sts);
    
    in; out;

    UMC::FrameMemID memId = m_currFrame->GetFrameMID();

    m_pFrameAllocator->IncreaseReference(memId);

    // begin frame
    sts = m_pVideoAccelerator->BeginFrame(memId);
    UMC_CHECK_STATUS(sts);

    // pack buffers
    sts = PackHeaders(in);
    UMC_CHECK_STATUS(sts);

    // execute
    sts = m_pVideoAccelerator->Execute();
    UMC_CHECK_STATUS(sts);

    // end frame
    sts = m_pVideoAccelerator->EndFrame();
    UMC_CHECK_STATUS(sts);

    // decrease reference
    //m_pFrameAllocator->DecreaseReference(memId);

    *out = m_currFrame;

    return sts;

} // Status VP8VideoDecoderHardware::GetFrame(MediaData* in, FrameData** out)

void VP8VideoDecoderHardware::SetFrameAllocator(FrameAllocator * frameAllocator)
{
    return VP8VideoDecoderSoftware::SetFrameAllocator(frameAllocator);

} // void VP8VideoDecoderHardware::SetFrameAllocator(FrameAllocator * frameAllocator)

Status VP8VideoDecoderHardware::PackHeaders(MediaData* src)
{
    /////////////////////////////////////////////////////////////////////////////////////////
    
    UMCVACompBuffer* compBufPic;
    VP8_DECODE_PICTURE_PARAMETERS *picParams = (VP8_DECODE_PICTURE_PARAMETERS*)m_pVideoAccelerator->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS, &compBufPic);
    //VP8_DECODE_PICTURE_PARAMETERS *picParams = (VP8_DECODE_PICTURE_PARAMETERS*)m_pVideoAccelerator->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &compBufPic);

    picParams->wFrameWidthInMbsMinus1 = USHORT((m_frameInfo.frameSize.width / 16) - 1);
    picParams->wFrameHeightInMbsMinus1 = USHORT((m_frameInfo.frameSize.height / 16) - 1);

    picParams->CurrPicIndex = 0;

    picParams->LastRefPicIndex = 0xff;
    picParams->GoldenRefPicIndex = 0xff;
    picParams->AltRefPicIndex = 0xff;
    picParams->DeblockedPicIndex = 0xff;

    if (I_PICTURE == m_frameInfo.frameType)
    {
        picParams->key_frame = 1;

        picParams->LastRefPicIndex = 0xff;
        picParams->GoldenRefPicIndex = 0xff;
        picParams->AltRefPicIndex = 0xff;
        picParams->DeblockedPicIndex = 0xff;
    }

    picParams->version = 0;
    picParams->segmentation_enabled = m_frameInfo.segmentationEnabled;
    picParams->update_mb_segmentation_map = m_frameInfo.updateSegmentMap;
    picParams->update_segment_feature_data = m_frameInfo.updateSegmentData;

    picParams->filter_type = m_frameInfo.loopFilterType;
    picParams->sign_bias_golden = 0; // ?
    picParams->sign_bias_alternate = 0; // ?
    picParams->mb_no_coeff_skip = 0; // ?

    picParams->CodedCoeffTokenPartition = m_frameInfo.numPartitions; // or m_frameInfo.numTokenPartitions

    // TO DO
    if (0 == m_frameInfo.segmentationEnabled)
    {
        picParams->loop_filter_level[0] = 0;
        picParams->loop_filter_level[1] = 0;
        picParams->loop_filter_level[2] = 0;
        picParams->loop_filter_level[3] = 0;
    }
    else
    {
        picParams->loop_filter_level[0] = 0;
        picParams->loop_filter_level[1] = 0;
        picParams->loop_filter_level[2] = 0;
        picParams->loop_filter_level[3] = 0;
    }

    picParams->sharpness_level = m_frameInfo.sharpnessLevel;

    picParams->mb_segment_tree_probs[0] = m_frameInfo.segmentTreeProbabilities[0];
    picParams->mb_segment_tree_probs[1] = m_frameInfo.segmentTreeProbabilities[1];
    picParams->mb_segment_tree_probs[2] = m_frameInfo.segmentTreeProbabilities[2];

    picParams->prob_skip_false = m_frameInfo.skipFalseProb;
    picParams->prob_intra = m_frameInfo.intraProb;
    picParams->prob_last = m_frameInfo.lastProb;
    picParams->prob_golden = m_frameInfo.goldProb;

    for (Ipp32u i = 0; i < VP8_NUM_MB_MODES_Y - 1; i += 1)
    {
        // memcpy?
        picParams->y_mode_probs[i] = m_frameProbs.mbModeProbY[i];
    }

    for (Ipp32u i = 0; i < VP8_NUM_MB_MODES_UV - 1; i += 1)
    {
        picParams->uv_mode_probs[i] = m_frameProbs.mbModeProbUV[i];
    }

    for (Ipp32u i = 0; i < VP8_NUM_MV_PROBS; i += 1)
    {
        picParams->mv_update_prob[0][i] = m_frameProbs.mvContexts[0][i];
        picParams->mv_update_prob[1][i] = m_frameProbs.mvContexts[1][i];
    }

    memset(picParams->PartitionSize, 0, sizeof(Ipp32u) * 9);

    picParams->PartitionSize[0] = m_frameInfo.partitionSize[0];

    for (Ipp32s i = 1; i < m_frameInfo.numPartitions; i += 1)
    {
        picParams->PartitionSize[i] = m_frameInfo.partitionSize[i];
    }

    picParams->FirstMbBitOffset = 0;
    picParams->P0EntropyCount = 0;
    picParams->P0EntropyRange = 0;
    picParams->P0EntropyValue = 0;

    compBufPic->SetDataSize(sizeof(VP8_DECODE_PICTURE_PARAMETERS));

    ////////////////////////////////////////////////////////////////

    UMCVACompBuffer* compBufQm;
    VP8_DECODE_QM_TABLE *qmTable = (VP8_DECODE_QM_TABLE*)m_pVideoAccelerator->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX, &compBufQm);

    if (0 == m_frameInfo.segmentationEnabled)
    {
        // when segmentation is disabled, use the first entry 0 for the quantization values
        // ippsCopy_8u
        qmTable->Qvalue[0][0] = USHORT(m_quantInfo.ydcQ[0]);
        qmTable->Qvalue[0][1] = USHORT(m_quantInfo.yacQ[0]);
        qmTable->Qvalue[0][2] = USHORT(m_quantInfo.uvdcQ[0]);
        qmTable->Qvalue[0][3] = USHORT(m_quantInfo.uvacQ[0]);
        qmTable->Qvalue[0][4] = USHORT(m_quantInfo.y2dcQ[0]);
        qmTable->Qvalue[0][5] = USHORT(m_quantInfo.y2acQ[0]);
    }
    else
    {
        for (Ipp32u i = 0; i < 4; i += 1)
        {
            qmTable->Qvalue[i][0] = USHORT(m_quantInfo.ydcQ[i]);
            qmTable->Qvalue[i][1] = USHORT(m_quantInfo.yacQ[i]);
            qmTable->Qvalue[i][2] = USHORT(m_quantInfo.uvdcQ[i]);
            qmTable->Qvalue[i][3] = USHORT(m_quantInfo.uvacQ[i]);
            qmTable->Qvalue[i][4] = USHORT(m_quantInfo.y2dcQ[i]);
            qmTable->Qvalue[i][5] = USHORT(m_quantInfo.y2acQ[i]);
        }
    }

    compBufQm->SetDataSize(sizeof(VP8_DECODE_QM_TABLE));

    ////////////////////////////////////////////////////////////////

    UMCVACompBuffer* compBufBs;
    Ipp8u *bistreamData = (Ipp8u *)m_pVideoAccelerator->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_BITSTREAM_DATA, &compBufBs);

    MFX_INTERNAL_CPY(bistreamData, src->GetDataPointer(), int(src->GetDataSize()));
    compBufBs->SetDataSize((Ipp32s)src->GetDataSize());

    UMCVACompBuffer* compBufCp;
    Ipp8u (*coeffProbs)[8][3][11] = (Ipp8u (*)[8][3][11])m_pVideoAccelerator->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_VP8_COEFFICIENT_PROBABILITIES, &compBufCp);

    //[4][8][3][11]
    MFX_INTERNAL_CPY(coeffProbs, m_frameProbs.coeff_probs, sizeof(Ipp8u) * 4 * 8 * 3 * 11);

    compBufCp->SetDataSize(sizeof(Ipp8u) * 4 * 8 * 3 * 11);

    return UMC_OK;

} // Status VP8VideoDecoderHardware::PackHeaders(MediaData* src)


#endif // #ifdef UMC_VA_DXVA
#endif