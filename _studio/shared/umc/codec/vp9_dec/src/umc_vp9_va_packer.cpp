/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//
//
*/

#include "umc_vp9_dec_defs.h"

#ifdef UMC_ENABLE_VP9_VIDEO_DECODER

#ifndef UMC_RESTRICTED_CODE_VA

#include "umc_vp9_va_packer.h"
#include "umc_vp9_bitstream.h"
#include "umc_vp9_frame.h"
#include "umc_vp9_utils.h"

#ifdef UMC_VA_DXVA
#include "umc_va_base.h"
#include "umc_va_dxva2_protected.h"
#endif

using namespace UMC;

namespace UMC_VP9_DECODER
{

inline
void mfx_memcpy(void * dst, size_t dstLen, void * src, size_t len)
{
    memcpy_s(dst, dstLen, src, len);
}

Packer * Packer::CreatePacker(UMC::VideoAccelerator * va)
{
    Packer * packer = 0;

#ifdef UMC_VA_DXVA
    if (va->IsIntelCustomGUID()) // intel profile
        packer = new PackerIntel(va);
#if defined(NTDDI_WIN10_TH2)
    else
        packer = new PackerMS(va);
#endif
#elif defined(UMC_VA_LINUX)
    packer = new PackerVA(va);
#endif

    return packer;
}

Packer::Packer(UMC::VideoAccelerator * va)
    : m_va(va)
{}

Packer::~Packer()
{}

#ifdef UMC_VA_DXVA

/****************************************************************************************************/
// DXVA Windows packer implementation
/****************************************************************************************************/
PackerDXVA::PackerDXVA(VideoAccelerator * va)
    : Packer(va)
    , m_report_counter(1)
{}

Status PackerDXVA::GetStatusReport(void * pStatusReport, size_t size)
{
    return
        m_va->ExecuteStatusReportBuffer(pStatusReport, (Ipp32u)size);
}

void PackerDXVA::BeginFrame()
{
    m_report_counter++;
}

void PackerDXVA::EndFrame()
{
}

PackerIntel::PackerIntel(VideoAccelerator * va)
    : PackerDXVA(va)
{}

void PackerIntel::PackAU(VP9Bitstream* bs, VP9DecoderFrame const* info)
{
    UMC::UMCVACompBuffer* compBufPic = NULL;
    DXVA_Intel_PicParams_VP9 *picParam = (DXVA_Intel_PicParams_VP9*)m_va->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS_VP9, &compBufPic);
    if (!picParam || !compBufPic || (compBufPic->GetBufferSize() < sizeof(DXVA_Intel_PicParams_VP9)))
        throw vp9_exception(MFX_ERR_MEMORY_ALLOC);

    compBufPic->SetDataSize(sizeof(DXVA_Intel_PicParams_VP9));
    memset(picParam, 0, sizeof(DXVA_Intel_PicParams_VP9));

    PackPicParams(picParam, info);

    UMCVACompBuffer* compBufSeg = NULL;
    DXVA_Intel_Segment_VP9 *segParam = (DXVA_Intel_Segment_VP9*)m_va->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX_VP9, &compBufSeg);
    if (!segParam || !compBufSeg || (compBufSeg->GetBufferSize() < sizeof(DXVA_Intel_Segment_VP9)))
        throw vp9_exception(MFX_ERR_MEMORY_ALLOC);

    compBufSeg->SetDataSize(sizeof(DXVA_Intel_Segment_VP9));
    memset(segParam, 0, sizeof(DXVA_Intel_Segment_VP9));

    PackSegmentParams(segParam, info);

    Ipp8u* data;
    Ipp32u length;
    bs->GetOrg(&data, &length);
    Ipp32u offset = (Ipp32u)bs->BytesDecoded();
    length -= offset;

    do
    {
        UMC::UMCVACompBuffer* compBufBs = NULL;
        mfxU8 *bistreamData = (mfxU8 *)m_va->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_BITSTREAM_DATA_VP9, &compBufBs);
        if (!bistreamData || !compBufBs)
            throw vp9_exception(MFX_ERR_MEMORY_ALLOC);

        mfxU32 lenght2 = length;
        if (compBufBs->GetBufferSize() < (mfxI32)length)
            lenght2 = compBufBs->GetBufferSize();

        mfx_memcpy(bistreamData, lenght2, data + offset, lenght2);
        compBufBs->SetDataSize(lenght2);

        length -= lenght2;
        offset += lenght2;

        if (length)
        {
            Status sts = m_va->Execute();
            if (sts != UMC::UMC_OK)
                throw vp9_exception(sts);
        }
    } while (length);
}

void PackerIntel::PackPicParams(DXVA_Intel_PicParams_VP9* picParam, VP9DecoderFrame const* info)
{
    picParam->FrameHeightMinus1 = (USHORT)info->height - 1;
    picParam->FrameWidthMinus1 = (USHORT)info->width - 1;

    picParam->PicFlags.fields.frame_type = info->frameType;
    picParam->PicFlags.fields.show_frame = info->showFrame;
    picParam->PicFlags.fields.error_resilient_mode = info->errorResilientMode;
    picParam->PicFlags.fields.intra_only = info->intraOnly;

    if (KEY_FRAME == info->frameType || info->intraOnly)
    {
        picParam->PicFlags.fields.LastRefIdx = info->activeRefIdx[0];
        picParam->PicFlags.fields.GoldenRefIdx = info->activeRefIdx[1];
        picParam->PicFlags.fields.AltRefIdx = info->activeRefIdx[2];
    }
    else
    {
        picParam->PicFlags.fields.LastRefIdx = info->activeRefIdx[0];
        picParam->PicFlags.fields.LastRefSignBias = info->refFrameSignBias[LAST_FRAME];
        picParam->PicFlags.fields.GoldenRefIdx = info->activeRefIdx[1];
        picParam->PicFlags.fields.GoldenRefSignBias = info->refFrameSignBias[GOLDEN_FRAME];
        picParam->PicFlags.fields.AltRefIdx = info->activeRefIdx[2];
        picParam->PicFlags.fields.AltRefSignBias = info->refFrameSignBias[ALTREF_FRAME];
    }

    picParam->PicFlags.fields.allow_high_precision_mv = info->allowHighPrecisionMv;
    picParam->PicFlags.fields.mcomp_filter_type = info->interpFilter;
    picParam->PicFlags.fields.frame_parallel_decoding_mode = info->frameParallelDecodingMode;
    picParam->PicFlags.fields.segmentation_enabled = info->segmentation.enabled;;
    picParam->PicFlags.fields.segmentation_temporal_update = info->segmentation.temporalUpdate;
    picParam->PicFlags.fields.segmentation_update_map = info->segmentation.updateMap;
    picParam->PicFlags.fields.reset_frame_context = info->resetFrameContext;
    picParam->PicFlags.fields.refresh_frame_context = info->refreshFrameContext;
    picParam->PicFlags.fields.frame_context_idx = info->frameContextIdx;
    picParam->PicFlags.fields.LosslessFlag = info->lossless;

    if (KEY_FRAME == info->frameType)
        for (int i = 0; i < NUM_REF_FRAMES; i++)
        {
            picParam->RefFrameList[i] = UCHAR_MAX;
        }
    else
    {
        for (mfxU8 ref = 0; ref < NUM_REF_FRAMES; ++ref)
            picParam->RefFrameList[ref] = (UCHAR)info->ref_frame_map[ref];
    }

    picParam->CurrPic = (UCHAR)info->currFrame;
    picParam->filter_level = (UCHAR)info->lf.filterLevel;
    picParam->sharpness_level = (UCHAR)info->lf.sharpnessLevel;
    picParam->log2_tile_rows = (UCHAR)info->log2TileRows;
    picParam->log2_tile_columns = (UCHAR)info->log2TileColumns;
    picParam->UncompressedHeaderLengthInBytes = (UCHAR)info->frameHeaderLength;
    picParam->FirstPartitionSize = (USHORT)info->firstPartitionSize;

    for (mfxU8 i = 0; i < VP9_NUM_OF_SEGMENT_TREE_PROBS; ++i)
        picParam->mb_segment_tree_probs[i] = info->segmentation.treeProbs[i];

    for (mfxU8 i = 0; i < VP9_NUM_OF_PREDICTION_PROBS; ++i)
        picParam->segment_pred_probs[i] = info->segmentation.predProbs[i];

    picParam->BSBytesInBuffer = info->frameDataSize;
    picParam->StatusReportFeedbackNumber = ++m_report_counter;

    picParam->profile = (UCHAR)info->profile;
    picParam->BitDepthMinus8 = (UCHAR)(info->bit_depth - 8);
    picParam->subsampling_x = (UCHAR)info->subsamplingX;
    picParam->subsampling_y = (UCHAR)info->subsamplingY;
}

void PackerIntel::PackSegmentParams(DXVA_Intel_Segment_VP9* segParam, VP9DecoderFrame const* info)
{
    for (mfxU8 segmentId = 0; segmentId < VP9_MAX_NUM_OF_SEGMENTS; ++segmentId)
    {
        segParam->SegData[segmentId].SegmentFlags.fields.SegmentReferenceEnabled = !!(info->segmentation.featureMask[segmentId] & (1 << SEG_LVL_REF_FRAME));

        segParam->SegData[segmentId].SegmentFlags.fields.SegmentReference =
            GetSegData(info->segmentation, segmentId, SEG_LVL_REF_FRAME);

        segParam->SegData[segmentId].SegmentFlags.fields.SegmentReferenceSkipped = !!(info->segmentation.featureMask[segmentId] & (1 << SEG_LVL_SKIP));

        for (mfxU8 ref = INTRA_FRAME; ref < MAX_REF_FRAMES; ++ref)
            for (mfxU8 mode = 0; mode < MAX_MODE_LF_DELTAS; ++mode)
                segParam->SegData[segmentId].FilterLevel[ref][mode] = info->lf_info.level[segmentId][ref][mode];

        const mfxI32 qIndex = GetQIndex(info->segmentation, segmentId, info->baseQIndex);
        if (qIndex < 0)
            throw vp9_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

        segParam->SegData[segmentId].LumaACQuantScale = info->yDequant[qIndex][1];
        segParam->SegData[segmentId].LumaDCQuantScale = info->yDequant[qIndex][0];
        segParam->SegData[segmentId].ChromaACQuantScale = info->uvDequant[qIndex][1];
        segParam->SegData[segmentId].ChromaDCQuantScale = info->uvDequant[qIndex][0];
    }
}

#if defined(NTDDI_WIN10_TH2)
PackerMS::PackerMS(UMC::VideoAccelerator * va)
    : PackerDXVA(va)
{
}

void PackerMS::PackAU(VP9Bitstream* bs, VP9DecoderFrame const* info)
{
    UMC::UMCVACompBuffer* compBufPic = NULL;
    DXVA_PicParams_VP9 *picParam =
        reinterpret_cast<DXVA_PicParams_VP9*>(m_va->GetCompBuffer(D3D9_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS_VP9, &compBufPic));
    if (!compBufPic || (compBufPic->GetBufferSize() < sizeof(DXVA_PicParams_VP9)))
        throw vp9_exception(MFX_ERR_MEMORY_ALLOC);

    compBufPic->SetDataSize(sizeof(DXVA_PicParams_VP9));
    memset(picParam, 0, sizeof(DXVA_PicParams_VP9));

    PackPicParams(picParam, info);

    Ipp8u* data;
    Ipp32u length;
    bs->GetOrg(&data, &length);

    Ipp32u const offset =
        static_cast<Ipp32u>(bs->BytesDecoded());
    data += offset;
    length -= offset;

    USHORT chopping = 0;
    do
    {
        UMC::UMCVACompBuffer* compBufSlice = NULL;
        DXVA_Slice_VPx_Short* slice =
            reinterpret_cast<DXVA_Slice_VPx_Short*>(m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &compBufSlice));
        if (!compBufSlice || compBufSlice->GetBufferSize() < sizeof(DXVA_Slice_VPx_Short))
            throw vp9_exception(MFX_ERR_MEMORY_ALLOC);

        compBufSlice->SetDataSize(sizeof(DXVA_Slice_VPx_Short));
        memset(slice, 0, sizeof(DXVA_Slice_VPx_Short));

        UMC::UMCVACompBuffer* compBufBs = NULL;
        mfxU8* bistreamData = 
            reinterpret_cast<mfxU8 *>(m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &compBufBs));
        if (!compBufBs)
            throw vp9_exception(MFX_ERR_MEMORY_ALLOC);

        mfxU32 lenght2 = length;
        if (compBufBs->GetBufferSize() < (mfxI32)length)
            lenght2 = compBufBs->GetBufferSize();
        
        mfxU32 const padding = 
            align_value<Ipp32s>(lenght2, 128) - lenght2;

        slice->BSNALunitDataLocation = 0;
        slice->SliceBytesInBuffer = lenght2 + padding;
        slice->wBadSliceChopping = chopping | (lenght2 < length);

        mfx_memcpy(bistreamData, lenght2, data, lenght2);

        bistreamData += lenght2;
        memset(bistreamData, 0, padding);

        compBufBs->SetDataSize(slice->SliceBytesInBuffer);

        length -= lenght2;
        data += lenght2;
        if (!chopping)
            chopping = 2;

        if (length)
        {
            Status sts = m_va->Execute();
            if (sts != UMC::UMC_OK)
                throw vp9_exception(sts);
        }
    } while (length);
}

void PackerMS::PackPicParams(DXVA_PicParams_VP9* pp, VP9DecoderFrame const* info)
{
    UCHAR const idx = 
        (UCHAR)info->currFrame;
    VM_ASSERT((idx & 0x7f) == idx);

    pp->CurrPic.Index7Bits = idx;
    pp->profile = (UCHAR)info->profile;

    //wFormatAndPictureInfoFlags
    pp->frame_type                   = info->frameType;
    pp->show_frame                   = info->showFrame;
    pp->error_resilient_mode         = info->errorResilientMode;
    pp->subsampling_x                = info->subsamplingX;
    pp->subsampling_y                = info->subsamplingY;
    pp->refresh_frame_context        = info->refreshFrameContext;
    pp->frame_parallel_decoding_mode = info->frameParallelDecodingMode;
    pp->intra_only                   = info->intraOnly;
    pp->frame_context_idx            = info->frameContextIdx;
    pp->reset_frame_context          = info->resetFrameContext;
    pp->allow_high_precision_mv      = pp->frame_type == KEY_FRAME ? 0 : info->allowHighPrecisionMv;

    pp->width = info->width;
    pp->height = info->height;

    pp->BitDepthMinus8Luma   = (UCHAR)(info->bit_depth - 8);
    pp->BitDepthMinus8Chroma = (UCHAR)(info->bit_depth - 8);

    pp->interp_filter = (UCHAR)info->interpFilter;

    if (KEY_FRAME == info->frameType)
        for (int i = 0; i < NUM_REF_FRAMES; i++)
            pp->ref_frame_map[i].AssociatedFlag = 1;
    else
        for (int i = 0; i < NUM_REF_FRAMES; ++i)
        {
            UCHAR const r_idx = 
                (UCHAR)info->ref_frame_map[i];
            VM_ASSERT((r_idx & 0x7f) == r_idx);

            pp->ref_frame_map[i].Index7Bits = r_idx;

            pp->ref_frame_coded_width[i]  = info->sizesOfRefFrame[i].width;
            pp->ref_frame_coded_height[i] = info->sizesOfRefFrame[i].height;
        }

    for (int i = 0; i < REFS_PER_FRAME; i++)
    {
        if (KEY_FRAME == info->frameType || info->intraOnly)
            pp->frame_refs[i].AssociatedFlag = 1;
        else
            pp->frame_refs[i].Index7Bits = (UCHAR)info->activeRefIdx[0];

        pp->ref_frame_sign_bias[i + 1] = (UCHAR)info->refFrameSignBias[i + 1];
    }
    
    pp->filter_level    = (UCHAR)info->lf.filterLevel;
    pp->sharpness_level = (UCHAR)info->lf.sharpnessLevel;

    //pp->wControlInfoFlags
    pp->mode_ref_delta_enabled   = info->lf.modeRefDeltaEnabled;
    pp->mode_ref_delta_update    = info->lf.modeRefDeltaUpdate;
    pp->use_prev_in_find_mv_refs = !info->errorResilientMode && !info->lastShowFrame;
       
    for (Ipp8u i = 0; i < MAX_REF_LF_DELTAS; i++)
        pp->ref_deltas[i]  = info->lf.refDeltas[i];

    for (Ipp8u i = 0; i < MAX_MODE_LF_DELTAS; i++)
        pp->mode_deltas[i]  = info->lf.modeDeltas[i];

    pp->base_qindex   = (SHORT)info->baseQIndex;
    pp->y_dc_delta_q  = (CHAR)info->y_dc_delta_q;
    pp->uv_dc_delta_q = (CHAR)info->uv_dc_delta_q;
    pp->uv_ac_delta_q = (CHAR)info->uv_ac_delta_q;

    //pp->stVP9Segments.wSegmentInfoFlags = ;
    for (Ipp8u i = 0; i < VP9_NUM_OF_SEGMENT_TREE_PROBS; i++)
        pp->stVP9Segments.tree_probs[i] = info->segmentation.treeProbs[i];

    //segmentation.predProbs[] is already filled with VP9_MAX_PROB when !segmentation.temporalUpdate (see VideoDECODEVP9_HW::DecodeFrameHeader)
    for (Ipp8u i = 0; i < VP9_NUM_OF_PREDICTION_PROBS; ++i)
        pp->stVP9Segments.pred_probs[i] = info->segmentation.predProbs[i];

    for (Ipp8u i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; i++)
    {
        pp->stVP9Segments.feature_mask[i] = (UCHAR)info->segmentation.featureMask[i];

        pp->stVP9Segments.feature_data[i][0] = (SHORT)GetSegData(info->segmentation, i, SEG_LVL_ALT_Q);
        pp->stVP9Segments.feature_data[i][1] = (SHORT)GetSegData(info->segmentation, i, SEG_LVL_ALT_LF);
        pp->stVP9Segments.feature_data[i][2] = (SHORT)GetSegData(info->segmentation, i, SEG_LVL_REF_FRAME);
        pp->stVP9Segments.feature_data[i][3] = (SHORT)GetSegData(info->segmentation, i, SEG_LVL_SKIP);
    }

    pp->log2_tile_cols = (UCHAR)info->log2TileColumns;
    pp->log2_tile_rows = (UCHAR)info->log2TileRows;
    
    pp->uncompressed_header_size_byte_aligned =
        (USHORT)info->frameHeaderLength;
    
    pp->first_partition_size =
        (USHORT)info->firstPartitionSize;

    pp->StatusReportFeedbackNumber = m_report_counter;
}

#endif

#endif // UMC_VA_DXVA

#if defined(UMC_VA_LINUX)
/****************************************************************************************************/
// VA linux packer implementation
/****************************************************************************************************/
PackerVA::PackerVA(VideoAccelerator * va)
    : Packer(va)
{}

Status PackerVA::GetStatusReport(void * , size_t )
{
    return UMC_OK;
}

void PackerVA::BeginFrame()
{ /* nothing to do here*/ }

void PackerVA::EndFrame()
{ /* nothing to do here*/ }

void PackerVA::PackAU(VP9Bitstream* bs, VP9DecoderFrame const* info)
{
    if (!bs || !info)
        throw vp9_exception(MFX_ERR_NULL_PTR);

    UMC::UMCVACompBuffer* pCompBuf = NULL;
    VADecPictureParameterBufferVP9 *picParam =
        (VADecPictureParameterBufferVP9*)m_va->GetCompBuffer(VAPictureParameterBufferType, &pCompBuf, sizeof(VADecPictureParameterBufferVP9));

    if (!picParam)
        throw vp9_exception(MFX_ERR_MEMORY_ALLOC);

    memset(picParam, 0, sizeof(VADecPictureParameterBufferVP9));
    PackPicParams(picParam, info);

    pCompBuf = NULL;
    VASliceParameterBufferVP9 *sliceParam =
        (VASliceParameterBufferVP9*)m_va->GetCompBuffer(VASliceParameterBufferType, &pCompBuf, sizeof(VASliceParameterBufferVP9));
    if (!sliceParam)
        throw vp9_exception(MFX_ERR_MEMORY_ALLOC);

    memset(sliceParam, 0, sizeof(VASliceParameterBufferVP9));
    PackSliceParams(sliceParam, info);

    Ipp8u* data;
    Ipp32u length;
    bs->GetOrg(&data, &length);
    Ipp32u const offset = bs->BytesDecoded();
    length -= offset;

    pCompBuf = NULL;
    mfxU8 *bistreamData = (mfxU8*)m_va->GetCompBuffer(VASliceDataBufferType, &pCompBuf, length);
    if (!bistreamData)
        throw vp9_exception(MFX_ERR_MEMORY_ALLOC);
    
    mfx_memcpy(bistreamData, length, data + offset, length);
    pCompBuf->SetDataSize(length);
}

void PackerVA::PackPicParams(VADecPictureParameterBufferVP9* picParam, VP9DecoderFrame const* info)
{
    VM_ASSERT(picParam);
    VM_ASSERT(info);

    picParam->frame_width = info->width;
    picParam->frame_height = info->height;

    if (KEY_FRAME == info->frameType)
        memset(picParam->reference_frames, VA_INVALID_SURFACE, sizeof(picParam->reference_frames));
    else
    {
        for (mfxU8 ref = 0; ref < NUM_REF_FRAMES; ++ref)
            picParam->reference_frames[ref] = m_va->GetSurfaceID(info->ref_frame_map[ref]);
    }

    picParam->pic_fields.bits.subsampling_x = info->subsamplingX;
    picParam->pic_fields.bits.subsampling_y = info->subsamplingY;
    picParam->pic_fields.bits.frame_type = info->frameType;
    picParam->pic_fields.bits.show_frame = info->showFrame;
    picParam->pic_fields.bits.error_resilient_mode = info->errorResilientMode;
    picParam->pic_fields.bits.intra_only = info->intraOnly;
    picParam->pic_fields.bits.allow_high_precision_mv = info->allowHighPrecisionMv;
    picParam->pic_fields.bits.mcomp_filter_type = info->interpFilter;
    picParam->pic_fields.bits.frame_parallel_decoding_mode = info->frameParallelDecodingMode;
    picParam->pic_fields.bits.reset_frame_context = info->resetFrameContext;
    picParam->pic_fields.bits.refresh_frame_context = info->refreshFrameContext;
    picParam->pic_fields.bits.frame_context_idx = info->frameContextIdx;
    picParam->pic_fields.bits.segmentation_enabled = info->segmentation.enabled;
    picParam->pic_fields.bits.segmentation_temporal_update = info->segmentation.temporalUpdate;
    picParam->pic_fields.bits.segmentation_update_map = info->segmentation.updateMap;

    {
        picParam->pic_fields.bits.last_ref_frame = info->activeRefIdx[0];
        picParam->pic_fields.bits.last_ref_frame_sign_bias = info->refFrameSignBias[LAST_FRAME];
        picParam->pic_fields.bits.golden_ref_frame = info->activeRefIdx[1];
        picParam->pic_fields.bits.golden_ref_frame_sign_bias = info->refFrameSignBias[GOLDEN_FRAME];
        picParam->pic_fields.bits.alt_ref_frame = info->activeRefIdx[2];
        picParam->pic_fields.bits.alt_ref_frame_sign_bias = info->refFrameSignBias[ALTREF_FRAME];
    }
    picParam->pic_fields.bits.lossless_flag = info->lossless;

    picParam->filter_level = info->lf.filterLevel;
    picParam->sharpness_level = info->lf.sharpnessLevel;
    picParam->log2_tile_rows = info->log2TileRows;
    picParam->log2_tile_columns = info->log2TileColumns;
    picParam->frame_header_length_in_bytes = info->frameHeaderLength;
    picParam->first_partition_size = info->firstPartitionSize;

    for (mfxU8 i = 0; i < VP9_NUM_OF_SEGMENT_TREE_PROBS; ++i)
        picParam->mb_segment_tree_probs[i] = info->segmentation.treeProbs[i];

    for (mfxU8 i = 0; i < VP9_NUM_OF_PREDICTION_PROBS; ++i)
        picParam->segment_pred_probs[i] = info->segmentation.predProbs[i];

    picParam->profile = info->profile;
    picParam->bit_depth = info->bit_depth - 8;
}

void PackerVA::PackSliceParams(VASliceParameterBufferVP9* sliceParam, VP9DecoderFrame const* info)
{
    VM_ASSERT(sliceParam);
    VM_ASSERT(info);

    sliceParam->slice_data_size = info->frameDataSize;
    sliceParam->slice_data_offset = 0;
    sliceParam->slice_data_flag = VA_SLICE_DATA_FLAG_ALL;

    for (mfxU8 segmentId = 0; segmentId < VP9_MAX_NUM_OF_SEGMENTS; ++segmentId)
    {
        sliceParam->seg_param[segmentId].segment_flags.fields.segment_reference_enabled = !!(info->segmentation.featureMask[segmentId] & (1 << SEG_LVL_REF_FRAME));

        sliceParam->seg_param[segmentId].segment_flags.fields.segment_reference =
            GetSegData(info->segmentation, segmentId, SEG_LVL_REF_FRAME);

        sliceParam->seg_param[segmentId].segment_flags.fields.segment_reference_skipped = !!(info->segmentation.featureMask[segmentId] & (1 << SEG_LVL_SKIP));

        for (mfxU8 ref = INTRA_FRAME; ref < MAX_REF_FRAMES; ++ref)
            for (mfxU8 mode = 0; mode < MAX_MODE_LF_DELTAS; ++mode)
                sliceParam->seg_param[segmentId].filter_level[ref][mode] = info->lf_info.level[segmentId][ref][mode];

        const mfxI32 qIndex = GetQIndex(info->segmentation, segmentId, info->baseQIndex);
        if (qIndex < 0)
            throw vp9_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

        sliceParam->seg_param[segmentId].luma_ac_quant_scale = info->yDequant[qIndex][1];
        sliceParam->seg_param[segmentId].luma_dc_quant_scale = info->yDequant[qIndex][0];
        sliceParam->seg_param[segmentId].chroma_ac_quant_scale = info->uvDequant[qIndex][1];
        sliceParam->seg_param[segmentId].chroma_dc_quant_scale = info->uvDequant[qIndex][0];
    }
}

#endif // #if defined(UMC_VA_LINUX)

} // namespace UMC_HEVC_DECODER

#endif // UMC_RESTRICTED_CODE_VA
#endif // UMC_ENABLE_VP9_VIDEO_DECODER
