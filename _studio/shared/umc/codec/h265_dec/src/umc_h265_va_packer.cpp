/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
*/

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef UMC_RESTRICTED_CODE_VA

#include "umc_h265_va_packer.h"

#include "umc_h265_slice_decoding.h"
#include "umc_h265_segment_decoder_templates.h"
#include "umc_h265_frame_list.h"

#include "umc_h265_slice_decoding.h"
#include "umc_h265_task_supplier.h"
#include "umc_h265_frame_info.h"

#ifdef UMC_VA_DXVA
#include "umc_mvc_ddi.h"
#include "umc_va_base.h"
#include "umc_va_dxva2_protected.h"
#endif

using namespace UMC;

namespace UMC_HEVC_DECODER
{

enum
{
    NO_REFERENCE = 0,
    SHORT_TERM_REFERENCE = 1,
    LONG_TERM_REFERENCE = 2,
    INTERVIEW_TERM_REFERENCE = 3
};


Packer::Packer(UMC::VideoAccelerator * va)
    : m_va(va)
{

}

Packer::~Packer()
{
}

#ifdef UMC_VA_DXVA

/****************************************************************************************************/
// DXVA Windows packer implementation
/****************************************************************************************************/
PackerDXVA2::PackerDXVA2(VideoAccelerator * va)
    : Packer(va)
    , m_statusReportFeedbackCounter(1)
{
}

Status PackerDXVA2::GetStatusReport(void * pStatusReport, size_t size)
{
    return m_va->ExecuteStatusReportBuffer(pStatusReport, (Ipp32u)size);
}

void PackerDXVA2::ExecuteBuffers()
{
    Status s = m_va->Execute();
#ifdef PRINT_TRACE
    printf(">>> VA.Execute: status %d\n", s);
#endif
    if(s != UMC_OK)
        throw h265_exception(s);
}

static inline int LengthInMinCb(int length, int cbsize)
{
    return length/(1 << cbsize);
}

void PackerDXVA2::GetPicParamVABuffer(DXVA_PicParams_HEVC **ppPicParam, size_t headerSize)
{
    VM_ASSERT(ppPicParam);

    for(int phase=0;phase < 2;phase++)
    {
        UMCVACompBuffer *compBuf;
        void *headBffr = m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &compBuf);
        size_t headBffrSize = compBuf->GetBufferSize(),
               headBffrOffs = compBuf->GetDataSize();

        if( headBffrSize - headBffrOffs < headerSize )
        {
            if(phase == 0)
            {
                Status s = m_va->Execute();
#ifdef PRINT_TRACE
                printf("   PicParam buffer overflow: VA.Execute: status %d\n", s);
#endif
                if(s != UMC_OK)
                    throw h265_exception(s);
                continue;
            }

            VM_ASSERT(false);
            return;
        }

        *ppPicParam = (DXVA_PicParams_HEVC *)((char *)headBffr + headBffrOffs);
        compBuf->SetDataSize(headBffrOffs + headerSize);

        memset(*ppPicParam, 0, headerSize);

        return;
    }
}

void PackerDXVA2::GetSliceVABuffers(
    DXVA_Slice_HEVC_Long **ppSliceHeader, size_t headerSize, 
    void **ppSliceData, size_t dataSize, 
    size_t dataAlignment)
{
    VM_ASSERT(ppSliceHeader);
    VM_ASSERT(ppSliceData);

    for(int phase=0;phase < 2;phase++)
    {
        UMCVACompBuffer *headVABffr = 0;
        UMCVACompBuffer *dataVABffr = 0;
        void *headBffr = m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &headVABffr);
        void *dataBffr = m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &dataVABffr);
        size_t headBffrSize = headVABffr->GetBufferSize(),
               headBffrOffs = headVABffr->GetDataSize();
        size_t dataBffrSize = dataVABffr->GetBufferSize(),
               dataBffrOffs = dataVABffr->GetDataSize();

        dataBffrOffs = dataAlignment ? dataAlignment*((dataBffrOffs + dataAlignment - 1)/dataAlignment) : dataBffrOffs;
#ifdef PRINT_TRACE
        printf("   DXVA_SLICE_CONTROL_BUFFER: buffer size: %u\n", headVABffr->GetBufferSize());
        printf("   DXVA_SLICE_CONTROL_BUFFER: sizeof slice hdr: %u\n", headerSize);
        printf("   DXVA_SLICE_CONTROL_BUFFER: offset of slice hdr in buffer: %u\n", headBffrOffs);
        printf("   DXVA_BITSTREAM_DATA_BUFFER: buffer size: %u\n", dataBffrSize);
        printf("   DXVA_BITSTREAM_DATA_BUFFER: sizeof slice data: %u\n", dataSize);
        printf("   DXVA_BITSTREAM_DATA_BUFFER: offset of slice data in buffer: %u\n", dataBffrOffs);
#endif

        if( headBffrSize - headBffrOffs < headerSize ||
            dataBffrSize - dataBffrOffs < dataSize )
        {
            if(phase == 0)
            {
                Status s = m_va->Execute();
#ifdef PRINT_TRACE
                printf("   Buffer overflow: VA.Execute: status %d\n", s);
#endif
                if(s != UMC_OK)
                    throw h265_exception(s);
                continue;
            }

            VM_ASSERT(false);
            return;
        }

        *ppSliceHeader = (DXVA_Slice_HEVC_Long *)((char *)headBffr + headBffrOffs);
        *ppSliceData = (char *)dataBffr + dataBffrOffs;

        headVABffr->SetDataSize(headBffrOffs + headerSize);
        dataVABffr->SetDataSize(dataBffrOffs + dataSize);

        memset(*ppSliceHeader, 0, headerSize);
        (*ppSliceHeader)->BSNALunitDataLocation = dataBffrOffs;
        (*ppSliceHeader)->SliceBytesInBuffer = dataSize;
#ifdef PRINT_TRACE
        printf("   DXVA_SLICE_CONTROL_BUFFER: SetDataSize(%u)\n", headBffrOffs + headerSize);
        printf("   DXVA_BITSTREAM_DATA_BUFFER: SetDataSize(%u)\n", dataBffrOffs + dataSize);
#endif

        return;
    }
}

void PackerDXVA2::GetIQMVABuffer(DXVA_Qmatrix_HEVC **ppQmatrix, size_t size)
{
    VM_ASSERT(ppQmatrix);

    for(int phase=0;phase < 2;phase++)
    {
        UMCVACompBuffer *compBuf;
        void *headBffr = m_va->GetCompBuffer(DXVA_INVERSE_QUANTIZATION_MATRIX_BUFFER, &compBuf);
        size_t bffrSize = compBuf->GetBufferSize(),
               bffrOffs = compBuf->GetDataSize();

        if( bffrSize - bffrOffs < size )
        {
            if(phase == 0)
            {
                Status s = m_va->Execute();
#ifdef PRINT_TRACE
                printf("   IQM buffer overflow: VA.Execute: status %d\n", s);
#endif
                if(s != UMC_OK)
                    throw h265_exception(s);
                continue;
            }

            VM_ASSERT(false);
            return;
        }

        *ppQmatrix = (DXVA_Qmatrix_HEVC *)((char *)headBffr + bffrOffs);
        compBuf->SetDataSize(bffrOffs + size);

        memset(*ppQmatrix, 0, size);

        return;
    }
}

void PackerDXVA2::BeginFrame()
{
    m_statusReportFeedbackCounter++;
}

void PackerDXVA2::EndFrame()
{
}

#if HEVC_SPEC_VER == MK_HEVCVER(0, 84)
void PackerDXVA2::PackPicParams(const H265DecoderFrame *pCurrentFrame,
                        H265DecoderFrameInfo * pSliceInfo,
                        TaskSupplier_H265 *supplier)
{
    DXVA_PicParams_HEVC* pPicParam = 0;
    GetPicParamVABuffer(&pPicParam, sizeof(DXVA_PicParams_HEVC));

    H265Slice *pSlice = pSliceInfo->GetSlice(0);
    const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
    const H265PicParamSet *pPicParamSet = pSlice->GetPicParam();
    
    //
    //
    pPicParam->PicWidthInMinCbsY   = (USHORT)LengthInMinCb(pSeqParamSet->getPicWidthInLumaSamples(), pSeqParamSet->getLog2MinCUSize());
    pPicParam->PicHeightInMinCbsY  = (USHORT)LengthInMinCb(pSeqParamSet->getPicHeightInLumaSamples(), pSeqParamSet->getLog2MinCUSize());

    //
    //
    pPicParam->PicFlags.fields.chroma_format_idc                            = pSeqParamSet->getChromaFormatIdc();
    pPicParam->PicFlags.fields.separate_colour_plane_flag                   = 0;    // 0 in HEVC spec HM10 by design
    pPicParam->PicFlags.fields.pcm_enabled_flag                             = pSeqParamSet->getUsePCM() ? 1 : 0 ;
    pPicParam->PicFlags.fields.scaling_list_enabled_flag                    = pSeqParamSet->getScalingListFlag() ? 1 : 0 ;
    pPicParam->PicFlags.fields.transform_skip_enabled_flag                  = pPicParamSet->getUseTransformSkip() ? 1 : 0 ;
    pPicParam->PicFlags.fields.amp_enabled_flag                             = pSeqParamSet->getUseAMP() ? 1 : 0 ;
    pPicParam->PicFlags.fields.strong_intra_smoothing_enabled_flag          = pSeqParamSet->getUseStrongIntraSmoothing() ? 1 : 0 ;
    pPicParam->PicFlags.fields.sign_data_hiding_flag                        = pPicParamSet->getSignHideFlag() ? 1 : 0 ;
    pPicParam->PicFlags.fields.constrained_intra_pred_flag                  = pPicParamSet->getConstrainedIntraPred() ? 1 : 0 ;
    pPicParam->PicFlags.fields.cu_qp_delta_enabled_flag                     = pPicParamSet->getUseDQP();
    pPicParam->PicFlags.fields.weighted_pred_flag                           = pPicParamSet->getUseWP() ? 1 : 0 ;
    pPicParam->PicFlags.fields.weighted_bipred_flag                         = pPicParamSet->getWPBiPred() ? 1 : 0 ;
    pPicParam->PicFlags.fields.transquant_bypass_enabled_flag               = pPicParamSet->getTransquantBypassEnableFlag() ? 1 : 0 ;    
    pPicParam->PicFlags.fields.tiles_enabled_flag                           = pPicParamSet->getTilesEnabledFlag();
    pPicParam->PicFlags.fields.entropy_coding_sync_enabled_flag             = pPicParamSet->getEntropyCodingSyncEnabledFlag();
    pPicParam->PicFlags.fields.pps_loop_filter_across_slices_enabled_flag   = pPicParamSet->getLoopFilterAcrossSlicesEnabledFlag();
    pPicParam->PicFlags.fields.loop_filter_across_tiles_enabled_flag        = pPicParamSet->getLoopFilterAcrossTilesEnabledFlag();
    pPicParam->PicFlags.fields.pcm_loop_filter_disabled_flag                = pSeqParamSet->getPCMFilterDisableFlag() ? 1 : 0 ;
    pPicParam->PicFlags.fields.field_pic_flag                               = 0;
    pPicParam->PicFlags.fields.bottom_field_flag                            = 0;
    pPicParam->PicFlags.fields.NoPicReorderingFlag                          = 0;
    pPicParam->PicFlags.fields.NoBiPredFlag                                 = 0;
    //pPicParam->PicFlags.fields.tiles_fixed_structure_flag                   = pSeqParamSet->getTilesFixedStructureFlag();
    
    //
    //
    pPicParam->CurrPic.Index7bits   = pCurrentFrame->m_index;    // ?
    pPicParam->CurrPicOrderCntVal   = pSlice->getPOC();

    
    int num_ref_frames = pSeqParamSet->getMaxDecPicBuffering(pSlice->getTLayer());

    pPicParam->sps_max_dec_pic_buffering_minus1 = (UCHAR)(num_ref_frames - 1);


    // RefFrameList
    //
    int count = 0;
    H265DBPList *dpb = supplier->GetDPBList();
    for(H265DecoderFrame* frame = dpb->head() ; frame && count < 16 ; frame = frame->future())
    {
        if(frame != pCurrentFrame)
        {
            int refType = frame->isShortTermRef() ? SHORT_TERM_REFERENCE : (frame->isLongTermRef() ? LONG_TERM_REFERENCE : NO_REFERENCE);

            if(refType != NO_REFERENCE)
            {
                pPicParam->RefFrameList[count].Index7bits = frame->m_index;
                pPicParam->RefFrameList[count].long_term_ref_flag = (refType == LONG_TERM_REFERENCE);
                m_refFrameListCache[count].bPicEntry = pPicParam->RefFrameList[count].bPicEntry;    // save to use in slice param packer
                count++;
            }
        }
    }
    m_refFrameListCacheSize = count;
    for(int n=count;n < 16;n++)
        pPicParam->RefFrameList[n].bPicEntry = (UCHAR)0xff;

    // PicOrderCntValList
    //
    const ReferencePictureSet *rps = pSlice->getRPS();
    for(int index=0;index < num_ref_frames;index++)
        pPicParam->PicOrderCntValList[index] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);
    for(int index=num_ref_frames;index < 16;index++)
        pPicParam->PicOrderCntValList[index] = -1;

    //
    //
    pPicParam->RefFieldPicFlag                              = 0;
    pPicParam->RefBottomFieldFlag                           = 0;
    pPicParam->bit_depth_luma_minus8                        = (UCHAR)(pSeqParamSet->getBitDepthY() - 8);
    pPicParam->bit_depth_chroma_minus8                      = (UCHAR)(pSeqParamSet->getBitDepthC() - 8);
    pPicParam->pcm_sample_bit_depth_luma_minus1             = (UCHAR)(pSeqParamSet->getPCMBitDepthLuma() - 1);
    pPicParam->pcm_sample_bit_depth_chroma_minus1           = (UCHAR)(pSeqParamSet->getPCMBitDepthChroma() - 1);
    pPicParam->log2_min_luma_coding_block_size_minus3       = (UCHAR)(pSeqParamSet->getLog2MinCUSize() - 3);
    pPicParam->log2_diff_max_min_luma_coding_block_size     = (UCHAR)(pSeqParamSet->getLog2CtbSize() - pSeqParamSet->getLog2MinCUSize());
    pPicParam->log2_min_transform_block_size_minus2         = (UCHAR)(pSeqParamSet->getQuadtreeTULog2MinSize() - 2);
    pPicParam->log2_diff_max_min_transform_block_size       = (UCHAR)(pSeqParamSet->getQuadtreeTULog2MaxSize() - pSeqParamSet->getQuadtreeTULog2MinSize());
    pPicParam->log2_min_pcm_luma_coding_block_size_minus3   = (UCHAR)(pSeqParamSet->getPCMLog2MinSize() - 3);
    pPicParam->log2_diff_max_min_pcm_luma_coding_block_size = (UCHAR)(pSeqParamSet->getPCMLog2MaxSize() - pSeqParamSet->getPCMLog2MinSize());
    pPicParam->max_transform_hierarchy_depth_intra          = (UCHAR)pSeqParamSet->getQuadtreeTUMaxDepthIntra() - 1;
    pPicParam->max_transform_hierarchy_depth_inter          = (UCHAR)pSeqParamSet->getQuadtreeTUMaxDepthInter() - 1;
    pPicParam->init_qp_minus26                              = (CHAR)pPicParamSet->getPicInitQP() - 26;
    pPicParam->diff_cu_qp_delta_depth                       = (UCHAR)(pPicParamSet->getMaxCuDQPDepth());
    pPicParam->pps_cb_qp_offset                             = (CHAR)pPicParamSet->getChromaCbQpOffset();
    pPicParam->pps_cr_qp_offset                             = (CHAR)pPicParamSet->getChromaCrQpOffset();
    if(pPicParam->PicFlags.fields.tiles_enabled_flag)
    {
        pPicParam->num_tile_columns_minus1          = (UCHAR)pPicParamSet->getNumColumns() - 1;
        pPicParam->num_tile_rows_minus1             = (UCHAR)pPicParamSet->getNumRows() - 1;
        pPicParamSet->getColumnWidthMinus1(pPicParam->column_width_minus1);
        pPicParamSet->getRowHeightMinus1(pPicParam->row_height_minus1);
    }
    pPicParam->log2_parallel_merge_level_minus2     = (UCHAR)pPicParamSet->getLog2ParallelMergeLevel() - 2;
    pPicParam->StatusReportFeedbackNumber = m_statusReportFeedbackCounter;

    pPicParam->continuation_flag = 1;

    // pic_short_format_flags
    //
    pPicParam->pic_short_format_flags.fields.lists_modification_present_flag            = pPicParamSet->getListsModificationPresentFlag() ? 1 : 0 ;
    pPicParam->pic_short_format_flags.fields.long_term_ref_pics_present_flag            = pSeqParamSet->getLongTermRefsPresent() ? 1 : 0 ;
    pPicParam->pic_short_format_flags.fields.sps_temporal_mvp_enabled_flag              = pSeqParamSet->getTMVPFlagsPresent() ? 1 : 0 ;
    pPicParam->pic_short_format_flags.fields.cabac_init_present_flag                    = pPicParamSet->getCabacInitPresentFlag() ? 1 : 0 ;
    pPicParam->pic_short_format_flags.fields.output_flag_present_flag                   = pPicParamSet->getOutputFlagPresentFlag() ? 1 : 0 ;
    pPicParam->pic_short_format_flags.fields.dependent_slice_segments_enabled_flag      = pPicParamSet->getDependentSliceSegmentEnabledFlag() ? 1 : 0 ;
    pPicParam->pic_short_format_flags.fields.pps_slice_chroma_qp_offsets_present_flag   = pPicParamSet->getSliceChromaQpFlag() ? 1 : 0 ;
    pPicParam->pic_short_format_flags.fields.slice_temporal_mvp_enabled_flag            = pSlice->getEnableTMVPFlag() ? 1 : 0 ;
    pPicParam->pic_short_format_flags.fields.sample_adaptive_offset_enabled_flag        = pSeqParamSet->getUseSAO() ? 1 : 0 ;
    pPicParam->pic_short_format_flags.fields.deblocking_filter_control_present_flag     = pPicParamSet->getDeblockingFilterControlPresentFlag() ? 1 : 0 ;
    pPicParam->pic_short_format_flags.fields.deblocking_filter_override_enabled_flag    = pPicParamSet->getDeblockingFilterOverrideEnabledFlag() ? 1 : 0 ;
    pPicParam->pic_short_format_flags.fields.pps_disable_deblocking_filter_flag         = pPicParamSet->getPicDisableDeblockingFilterFlag() ? 1 : 0 ;
    pPicParam->pic_short_format_flags.fields.IrapPicFlag                                = 0;
    pPicParam->pic_short_format_flags.fields.IdrPicFlag                                 = 0;
    pPicParam->pic_short_format_flags.fields.IntraPicFlag                               = 0;

    //
    //
    pPicParam->CollocatedRefIdx.Index7bits          = (UCHAR)(pSlice->getSliceType() != I_SLICE ? pSlice->getColRefIdx() : -1 );
    pPicParam->log2_max_pic_order_cnt_lsb_minus4    = (UCHAR)(pSeqParamSet->log2_max_pic_order_cnt_lsb - 4);
    pPicParam->num_short_term_ref_pic_sets          = (UCHAR)pSeqParamSet->getRPSList()->getNumberOfReferencePictureSets();
    pPicParam->num_long_term_ref_pics_sps            = (UCHAR)pSeqParamSet->getNumLongTermRefPicSPS();
    //for(unsigned k=0;k < 32 && k < pSeqParamSet->getNumLongTermRefPicSPS();k++)
    //    pPicParam->lt_ref_pic_poc_lsb_sps[k] = (USHORT)pSeqParamSet->getLtRefPicPocLsbSps(k);
    //pPicParam->used_by_curr_pic_lt_sps_flags        = (UINT)0;
    pPicParam->num_ref_idx_l0_default_active_minus1 = (UCHAR)(pPicParamSet->getNumRefIdxL0DefaultActive() - 1);
    pPicParam->num_ref_idx_l1_default_active_minus1 = (UCHAR)(pPicParamSet->getNumRefIdxL1DefaultActive() - 1);
    pPicParam->pps_beta_offset_div2                     = (CHAR)pPicParamSet->getDeblockingFilterBetaOffsetDiv2();
    pPicParam->pps_tc_offset_div2                       = (CHAR)pPicParamSet->getDeblockingFilterTcOffsetDiv2();    
}
#endif

#if HEVC_SPEC_VER == MK_HEVCVER(0, 81)
void PackerDXVA2::PackPicParams(const H265DecoderFrame *pCurrentFrame,
                        H265DecoderFrameInfo * pSliceInfo,
                        TaskSupplier_H265 *supplier)
{
    DXVA_PicParams_HEVC* pPicParam = 0;
    GetPicParamVABuffer(&pPicParam, sizeof(DXVA_PicParams_HEVC));

    H265Slice *pSlice = pSliceInfo->GetSlice(0);
    const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
    const H265PicParamSet *pPicParamSet = pSlice->GetPicParam();
    
    // picture size
    pPicParam->wFrameWidthInMinCbMinus1   = (USHORT)LengthInMinCb(pSeqParamSet->getPicWidthInLumaSamples(), pSeqParamSet->getLog2MinCUSize()) - 1;
    pPicParam->wFrameHeightInMinCbMinus1  = (USHORT)LengthInMinCb(pSeqParamSet->getPicHeightInLumaSamples(), pSeqParamSet->getLog2MinCUSize()) - 1;


    // picFlags
    pPicParam->chroma_format_idc                    = pSeqParamSet->getChromaFormatIdc();
    pPicParam->separate_colour_plane_flag           = 0;    // 0 in HEVC spec v9.1
    pPicParam->pcm_enabled_flag                     = pSeqParamSet->getUsePCM() ? 1 : 0 ;
    pPicParam->scaling_list_enable_flag             = pSeqParamSet->getScalingListFlag() ? 1 : 0 ;
    pPicParam->transform_skip_enabled_flag          = pPicParamSet->getUseTransformSkip() ? 1 : 0 ;
    pPicParam->amp_enabled_flag                     = pSeqParamSet->getUseAMP() ? 1 : 0 ;
    pPicParam->strong_intra_smoothing_enable_flag   = pSeqParamSet->getUseStrongIntraSmoothing() ? 1 : 0 ;
    pPicParam->sign_data_hiding_flag                = pPicParamSet->getSignHideFlag() ? 1 : 0 ;
    pPicParam->constrained_intra_pred_flag          = pPicParamSet->getConstrainedIntraPred() ? 1 : 0 ;
    pPicParam->cu_qp_delta_enabled_flag             = pPicParamSet->getUseDQP();
    pPicParam->weighted_pred_flag                   = pPicParamSet->getUseWP() ? 1 : 0 ;
    pPicParam->weighted_bipred_flag                 = pPicParamSet->getWPBiPred() ? 1 : 0 ;
    pPicParam->transquant_bypass_enable_flag        = pPicParamSet->getTransquantBypassEnableFlag() ? 1 : 0 ;    
    pPicParam->tiles_enabled_flag                   = pPicParamSet->getTilesEnabledFlag();
    pPicParam->entropy_coding_sync_enabled_flag     = pPicParamSet->getEntropyCodingSyncEnabledFlag();
    pPicParam->loop_filter_across_slices_flag       = pPicParamSet->getLoopFilterAcrossSlicesEnabledFlag();
    pPicParam->loop_filter_across_tiles_flag        = pPicParamSet->getLoopFilterAcrossTilesEnabledFlag();
    pPicParam->pcm_loop_filter_disable_flag         = pSeqParamSet->getPCMFilterDisableFlag() ? 1 : 0 ;
    pPicParam->field_pic_flag                       = 0;
    pPicParam->bottom_field_flag                    = 0;    
    pPicParam->tiles_fixed_structure_flag           = pSeqParamSet->getTilesFixedStructureFlag();
    
    //
    //
    pPicParam->CurrPic.Index7bits   = pCurrentFrame->m_index;
    pPicParam->poc_curr_pic         = pSlice->getPOC();

    // RefFrameList
    //
    int count = 0;
    H265DBPList *dpb = supplier->GetDPBList();
    for(H265DecoderFrame* frame = dpb->head() ; frame && count < 16 ; frame = frame->future())
    {
        if(frame != pCurrentFrame)
        {
            int refType = frame->isShortTermRef() ? SHORT_TERM_REFERENCE : (frame->isLongTermRef() ? LONG_TERM_REFERENCE : NO_REFERENCE);

            if(refType != NO_REFERENCE)
            {
                pPicParam->RefFrameList[count].Index7bits = frame->m_index;
                pPicParam->RefFrameList[count].long_term_ref_flag = (refType == LONG_TERM_REFERENCE);
                m_refFrameListCache[count].bPicEntry = pPicParam->RefFrameList[count].bPicEntry;    // save to use in slice param packer
                count++;
            }
        }
    }
    m_refFrameListCacheSize = count;
    for(int n=count;n < 16;n++)
        pPicParam->RefFrameList[n].bPicEntry = (UCHAR)0xff;
    pPicParam->num_ref_frames = (UCHAR)count;

    
    // PicOrderCntValList
    //

    const ReferencePictureSet *rps = pSlice->getRPS();
    pPicParam->num_ref_frames = (UCHAR)rps->getNumberOfPictures();
    for(int index=0;index < pPicParam->num_ref_frames && index < 16;index++)
        pPicParam->poc_list[index] = pPicParam->poc_curr_pic + rps->getDeltaPOC(index);
    for(int index=pPicParam->num_ref_frames;index < 16;index++)
        pPicParam->poc_list[index] = -1;

    //
    //
    pPicParam->ref_field_pic_flag                   = (USHORT)0;
    pPicParam->ref_bottom_field_flag                = (USHORT)0;
    pPicParam->bit_depth_luma_minus8                = (UCHAR)pSeqParamSet->getBitDepthY() - 8;
    pPicParam->bit_depth_chroma_minus8              = (UCHAR)pSeqParamSet->getBitDepthC() - 8;
    pPicParam->pcm_bit_depth_luma                   = (UCHAR)pSeqParamSet->getPCMBitDepthLuma();
    pPicParam->pcm_bit_depth_chroma                 = (UCHAR)pSeqParamSet->getPCMBitDepthChroma();
    pPicParam->log2_min_coding_block_size           = (UCHAR)pSeqParamSet->getLog2MinCUSize();
    pPicParam->log2_max_coding_block_size           = (UCHAR)pSeqParamSet->getLog2CtbSize();
    pPicParam->log2_min_transform_block_size        = (UCHAR)pSeqParamSet->getQuadtreeTULog2MinSize();
    pPicParam->log2_max_transform_block_size        = (UCHAR)pSeqParamSet->getQuadtreeTULog2MaxSize();
    pPicParam->log2_min_PCM_cb_size                 = (UCHAR)pSeqParamSet->getPCMLog2MinSize();
    pPicParam->log2_max_PCM_cb_size                 = (UCHAR)pSeqParamSet->getPCMLog2MaxSize();
    pPicParam->max_transform_hierarchy_depth_intra  = (UCHAR)pSeqParamSet->getQuadtreeTUMaxDepthIntra() - 1;
    pPicParam->max_transform_hierarchy_depth_inter  = (UCHAR)pSeqParamSet->getQuadtreeTUMaxDepthInter() - 1;
    pPicParam->pic_init_qp_minus26                  = (UCHAR)(pPicParamSet->pic_init_qp - 26);
    pPicParam->diff_cu_qp_delta_depth               = (UCHAR)(pPicParamSet->getMaxCuDQPDepth());
    pPicParam->pps_cb_qp_offset                     = (CHAR)pPicParamSet->getChromaCbQpOffset();
    pPicParam->pps_cr_qp_offset                     = (CHAR)pPicParamSet->getChromaCrQpOffset();
    if(pPicParam->tiles_enabled_flag)
    {
        pPicParam->num_tile_columns_minus1          = (UCHAR)(pPicParamSet->getNumColumns() - 1);
        pPicParam->num_tile_rows_minus1             = (UCHAR)(pPicParamSet->getNumRows() - 1);
        pPicParamSet->getColumnWidth(pPicParam->tile_column_width);
        pPicParamSet->getRowHeight(pPicParam->tile_row_height);
    }
    pPicParam->log2_parallel_merge_level_minus2     = (UCHAR)pPicParamSet->getLog2ParallelMergeLevel() - 2;
    pPicParam->StatusReportFeedbackNumber           = (UINT)0;    // ???

    pPicParam->ContinuationFlag = 1;

    // picShortFormatFlags
    pPicParam->lists_modification_present_flag          = pPicParamSet->getListsModificationPresentFlag() ? 1 : 0 ;
    pPicParam->long_term_ref_pics_present_flag          = pSeqParamSet->getLongTermRefsPresent() ? 1 : 0 ;
    pPicParam->sps_temporal_mvp_enable_flag             = pSeqParamSet->getTMVPFlagsPresent() ? 1 : 0 ;
    pPicParam->cabac_init_present_flag                  = pPicParamSet->getCabacInitPresentFlag() ? 1 : 0 ;
    pPicParam->output_flag_present_flag                 = pPicParamSet->getOutputFlagPresentFlag() ? 1 : 0 ;
    pPicParam->dependent_slice_segments_enabled_flag    = pPicParamSet->getDependentSliceSegmentEnabledFlag() ? 1 : 0 ;
    pPicParam->slice_chroma_qp_offsets_present_flag     = pPicParamSet->getSliceChromaQpFlag() ? 1 : 0 ;
    pPicParam->SAO_enabled_flag                         = pSeqParamSet->getUseSAO() ? 1 : 0 ;
    pPicParam->deblocking_filter_control_present_flag   = pPicParamSet->getDeblockingFilterControlPresentFlag() ? 1 : 0 ;
    pPicParam->deblocking_filter_override_enabled_flag  = pPicParamSet->getDeblockingFilterOverrideEnabledFlag() ? 1 : 0 ;
    pPicParam->pps_disable_deblocking_filter_flag       = pPicParamSet->getPicDisableDeblockingFilterFlag() ? 1 : 0 ;

    pPicParam->CollocatedRefIdx.Index7bits          = (UCHAR)(pSlice->getSliceType() != I_SLICE ? pSlice->getColRefIdx() : -1 );
    pPicParam->log2_max_pic_order_cnt_lsb_minus4    = (UCHAR)(pSeqParamSet->log2_max_pic_order_cnt_lsb - 4);
    pPicParam->num_short_term_ref_pic_sets          = (UCHAR)pSeqParamSet->getRPSList()->getNumberOfReferencePictureSets();
    pPicParam->num_long_term_ref_pic_sps            = (UCHAR)pSeqParamSet->getNumLongTermRefPicSPS();
    for(unsigned k=0;k < 32 && k < pSeqParamSet->getNumLongTermRefPicSPS();k++)
        pPicParam->lt_ref_pic_poc_lsb_sps[k] = (USHORT)pSeqParamSet->getLtRefPicPocLsbSps(k);
    pPicParam->used_by_curr_pic_lt_sps_flags        = (UINT)0;
    pPicParam->num_ref_idx_l0_default_active_minus1 = (UCHAR)(pPicParamSet->getNumRefIdxL0DefaultActive() - 1);
    pPicParam->num_ref_idx_l1_default_active_minus1 = (UCHAR)(pPicParamSet->getNumRefIdxL1DefaultActive() - 1);
    pPicParam->beta_offset_div2                     = (CHAR)pPicParamSet->getDeblockingFilterBetaOffsetDiv2();
    pPicParam->tc_offset_div2                       = (CHAR)pPicParamSet->getDeblockingFilterTcOffsetDiv2();    
}
#endif

void PackerDXVA2::PackSliceParams(H265Slice *pSlice, bool isLong, bool isLastSlice)
{
    size_t headerSize = sizeof(DXVA_Slice_HEVC_Short);

    if(isLong)
        headerSize = sizeof(DXVA_Slice_HEVC_Long);

    DXVA_Slice_HEVC_Long* pDXVASlice = 0;
    void *pSliceData = 0;
    size_t rawDataSize = pSlice->m_BitStream.BytesLeft();

    GetSliceVABuffers(&pDXVASlice, headerSize, &pSliceData, rawDataSize, 64);

    if(isLong)
    {
//      const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
        const H265PicParamSet *pPicParamSet = pSlice->GetPicParam();
        const H265DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();
        int sliceNum = pSlice->GetSliceNum();

        pDXVASlice->ByteOffsetToSliceData = 0;  // ???
        pDXVASlice->slice_segment_address = pSlice->getSliceAddr();
        pDXVASlice->num_LCUs_for_slice = 0; 

        for(int iDir = 0; iDir < 2; iDir++)
        {
            int index = 0;
            const H265DecoderRefPicList::ReferenceInformation* pRefPicList = pCurrentFrame->GetRefPicList(sliceNum, iDir)->m_refPicList;

            for(int i=0;i < 16;i++)
            {
                const H265DecoderRefPicList::ReferenceInformation &frameInfo = pRefPicList[i];
                if(frameInfo.refFrame)
                {
                    for(int j=0 ; j < m_refFrameListCacheSize ; j++)
                    {
                        if(m_refFrameListCache[j].Index7bits == (UCHAR)frameInfo.refFrame->m_index)
                        {
                            pDXVASlice->RefPicList[iDir][index].Index7bits = j;
                            index++;

                            break;
                        }
                    }
                }
            }

            for(;index < 16;index++)
                pDXVASlice->RefPicList[iDir][index].bPicEntry = (UCHAR)-1;
        }
        // picFlags
        pDXVASlice->last_slice_of_pic               = isLastSlice;
        pDXVASlice->dependent_slice_segment_flag    = (UINT)pPicParamSet->getDependentSliceSegmentEnabledFlag();   // dependent_slices_enabled_flag
        pDXVASlice->slice_type                      = (UINT)pSlice->getSliceType();
        pDXVASlice->color_plane_id                  = 0; // field is left for future expansion
        pDXVASlice->slice_sao_luma_flag             = (UINT)pSlice->getSaoEnabledFlag(); 
        pDXVASlice->slice_sao_chroma_flag           = (UINT)pSlice->getSaoEnabledFlagChroma();
        pDXVASlice->mvd_l1_zero_flag                = (UINT)pSlice->getMvdL1ZeroFlag();
        pDXVASlice->cabac_init_flag                 = (UINT)pSlice->getCabacInitFlag();
        pDXVASlice->slice_disable_lf_flag           = 0;//pSlice->getLoopFilterDisable() ? 1 : 0 ;        
        pDXVASlice->collocated_from_l0_flag         = (UINT)pSlice->getColFromL0Flag();
        pDXVASlice->lf_across_slices_enabled_flag   = (UINT)pPicParamSet->getLoopFilterAcrossSlicesEnabledFlag();

        pDXVASlice->num_ref_idx_l0_active_minus1    = (UCHAR)pSlice->getNumRefIdx(REF_PIC_LIST_0) - 1;
        pDXVASlice->num_ref_idx_l1_active_minus1    = (UCHAR)pSlice->getNumRefIdx(REF_PIC_LIST_1) - 1;
        pDXVASlice->slice_qp_delta                  = (CHAR)(pSlice->getSliceQp() - (pPicParamSet->getPicInitQP()));
        pDXVASlice->slice_cb_qp_offset              = (CHAR)pSlice->getSliceQpDeltaCb();
        pDXVASlice->slice_cr_qp_offset              = (CHAR)pSlice->getSliceQpDeltaCr();
        pDXVASlice->beta_offset_div2                = (CHAR)pPicParamSet->getDeblockingFilterBetaOffsetDiv2();
        pDXVASlice->tc_offset_div2                  = (CHAR)pPicParamSet->getDeblockingFilterTcOffsetDiv2();
        pDXVASlice->luma_log2_weight_denom          = (UCHAR)pSlice->getLog2WeightDenomLuma();
        pDXVASlice->chroma_log2_weight_denom        = (UCHAR)pSlice->getLog2WeightDenomChroma();

        pDXVASlice->max_num_merge_candidates        = (UCHAR)pSlice->getMaxNumMergeCand();
        pDXVASlice->num_entry_point_offsets         = pSlice->getNumEntryPointOffsets();

        for(int l=0;l < 2;l++)
        {
            wpScalingParam *wp;
            EnumRefPicList eRefPicList = ( l ? REF_PIC_LIST_1 : REF_PIC_LIST_0 );
            for (int iRefIdx=0;iRefIdx < pSlice->getNumRefIdx(eRefPicList);iRefIdx++)
            {
                pSlice->getWpScaling(eRefPicList, iRefIdx, wp);

                pDXVASlice->luma_offset[l][iRefIdx]         = (CHAR)wp[0].iOffset;
                pDXVASlice->delta_luma_weight[l][iRefIdx]   = (CHAR)wp[0].iDeltaWeight;

                for(int chroma=0;chroma < 2;chroma++)
                {
                    pDXVASlice->chroma_offset[l][iRefIdx][chroma] = (CHAR)wp[1 + chroma].iOffset;
                    pDXVASlice->chroma_offset[l][iRefIdx][chroma] = (CHAR)wp[1 + chroma].iDeltaWeight;
                }
            }
        }
    }

    // copy slice data to slice data buffer
    memcpy(pSliceData, pSlice->m_BitStream.GetRawDataPtr(), rawDataSize);
}

template<int COUNT> static inline
void initQMatrix(const H265ScalingList *scalingList, int sizeId, UCHAR qm[6][COUNT])
{/*         n*m    listId
        --------------------
        Intra   Y       0
        Intra   Cb      1
        Intra   Cr      2
        Inter   Y       3
        Inter   Cb      4
        Inter   Cr      5           */

    for(int n=0;n < 6;n++)
    {
        const int *src = scalingList->getScalingListAddress(sizeId, n);
        for(int i=0;i < COUNT;i++)  // coef.
            qm[n][i] = (UCHAR)src[i];
    }
}


template<int COUNT> static inline
void initQMatrix(const H265ScalingList *scalingList, int sizeId, UCHAR qm[3][2][COUNT])
{
    for(int comp=0 ; comp <= 2 ; comp++)    // Y Cb Cr
    {
        for(int n=0; n <= 1;n++)
        {
            int listId = comp + 3*n;
            const int *src = scalingList->getScalingListAddress(sizeId, listId);
            for(int i=0;i < COUNT;i++)  // coef.
                qm[comp][n][i] = (UCHAR)src[i];
        }
    }
}

static inline
void initQMatrix(const H265ScalingList *scalingList, int sizeId, UCHAR qm[2][64])
{/*      n      m     listId
        --------------------
        Intra   Y       0
        Inter   Y       1           */

    for(int n=0;n < 2;n++)  // Intra, Inter
    {
        const int *src = scalingList->getScalingListAddress(sizeId, n);

        for(int i=0;i < 64;i++)  // coef.
            qm[n][i] = (UCHAR)src[i];
    }
}

/*
void H265TrQuant::setDefaultScalingList(bool use_ts)
{
    H265ScalingList sl;

    for(Ipp32u sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
    {
        for(Ipp32u listId = 0; listId < g_scalingListNum[sizeId]; listId++)
        {
            ::memcpy(sl.getScalingListAddress(sizeId, listId),
                sl.getScalingListDefaultAddress(sizeId, listId, use_ts),
                sizeof(Ipp32s) * min(MAX_MATRIX_COEF_NUM, (Ipp32s)g_scalingListSize[sizeId]));
            sl.setScalingListDC(sizeId, listId, SCALING_LIST_DC);
        }
    }

    setScalingListDec(&sl);
}
*/

static int s_quantTSDefault4x4[16] =
{
  16,16,16,16,
  16,16,16,16,
  16,16,16,16,
  16,16,16,16
};

static int s_quantIntraDefault8x8[64] =
{
  16,16,16,16,17,18,21,24,  // 10 10 10 10 11 12 15 18
  16,16,16,16,17,19,22,25,
  16,16,17,18,20,22,25,29,
  16,16,18,21,24,27,31,36,
  17,17,20,24,30,35,41,47,
  18,19,22,27,35,44,54,65,
  21,22,25,31,41,54,70,88,
  24,25,29,36,47,65,88,115
};

static int s_quantInterDefault8x8[64] =
{
  16,16,16,16,17,18,20,24,
  16,16,16,17,18,20,24,25,
  16,16,17,18,20,24,25,28,
  16,17,18,20,24,25,28,33,
  17,18,20,24,25,28,33,41,
  18,20,24,25,28,33,41,54,
  20,24,25,28,33,41,54,71,
  24,25,28,33,41,54,71,91
};

static const int *getDefaultScalingList(unsigned sizeId, unsigned listId)
{
    int *src = 0;
    switch(sizeId)
    {
    case SCALING_LIST_4x4:
        src = g_quantTSDefault4x4;
        break;
    case SCALING_LIST_8x8:
        src = (listId<3) ? s_quantIntraDefault8x8 : s_quantInterDefault8x8;
        break;
    case SCALING_LIST_16x16:
        src = (listId<3) ? s_quantIntraDefault8x8 : s_quantInterDefault8x8;
        break;
    case SCALING_LIST_32x32:
        src = (listId<1) ? s_quantIntraDefault8x8 : s_quantInterDefault8x8;
        break;
    default:
        VM_ASSERT(0);
        src = NULL;
        break;
    }
    return src;
}

void PackerDXVA2::PackQmatrix(const H265Slice *pSlice/*const H265PicParamSet *pPicParamSet*/)
{
    DXVA_Qmatrix_HEVC* pQmatrix = 0;
    GetIQMVABuffer(&pQmatrix, sizeof(DXVA_Qmatrix_HEVC));
    const H265ScalingList *scalingList = 0;

    if(!pSlice->GetSeqParam()->sps_scaling_list_data_present_flag)
    {
        // TODO: build default scaling list in target buffer location
        static bool doInit = true;
        static H265ScalingList sl;

        if(doInit)
        {
            for(Ipp32u sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
            {
                for(Ipp32u listId = 0; listId < g_scalingListNum[sizeId]; listId++)
                {
                    const int *src = getDefaultScalingList(sizeId, listId);
                          int *dst = sl.getScalingListAddress(sizeId, listId);
                    int count = min(MAX_MATRIX_COEF_NUM, (Ipp32s)g_scalingListSize[sizeId]);
                    ::memcpy(dst, src, sizeof(Ipp32s) * count);
                    sl.setScalingListDC(sizeId, listId, SCALING_LIST_DC);
                }
            }
            doInit = false;
        }

        scalingList = &sl;
        memset(&pQmatrix->bScalingListDC, SCALING_LIST_DC, sizeof(pQmatrix->bScalingListDC));
    }
    else
        scalingList = pSlice->GetPicParam()->getScalingList();

    initQMatrix(scalingList, SCALING_LIST_4x4,   pQmatrix->ucScalingLists0);    // 4x4
    initQMatrix(scalingList, SCALING_LIST_8x8,   pQmatrix->ucScalingLists1);    // 8x8
    initQMatrix(scalingList, SCALING_LIST_16x16, pQmatrix->ucScalingLists2);    // 16x16
    initQMatrix(scalingList, SCALING_LIST_32x32, pQmatrix->ucScalingLists3);    // 32x32

    // TODO
    //for(int k=SCALING_LIST_16x16;k <= SCALING_LIST_32x32;k++)
    //{
    //    for(int m=0;m < 3;m++)  // Y, Cb, Cr
    //    {
    //        for(int n=0;n < 2;n++)  // Intra, Inter
    //        {
    //            pQmatrix->bScalingListDC[k][m][n] = (UCHAR)scalingList->getScalingListDC(k, k == SCALING_LIST_16x16 ? 3*n + m : n );
    //        }
    //    }
    //}
}

#endif // UMC_VA_DXVA


#if 0 //def UMC_VA_LINUX
/****************************************************************************************************/
// VA linux packer implementation
/****************************************************************************************************/
PackerVA::PackerVA(VideoAccelerator * va)
    : Packer(va)
{
}

void PackerVA::FillFrame(VAPictureH264 * pic, const H265DecoderFrame *pFrame,
                         Ipp32s field, Ipp32s reference, Ipp32s defaultIndex)
{
    Ipp32s index = pFrame->m_index;

    if (index == -1)
        index = defaultIndex;

    pic->picture_id = m_va->GetSurfaceID(index);
    pic->TopFieldOrderCnt = pFrame->m_PicOrderCnt[pFrame->GetNumberByParity(0)];
    pic->BottomFieldOrderCnt = pFrame->m_PicOrderCnt[pFrame->GetNumberByParity(1)];

    pic->flags = 0;

/*    if (pFrame->m_PictureStructureForDec == 0)
    {
        pic->flags |= field ? VA_PICTURE_H264_BOTTOM_FIELD : 0;//VA_PICTURE_H264_TOP_FIELD;
    }

    if (reference == 1)
        pic->flags |= VA_PICTURE_H264_SHORT_TERM_REFERENCE;

    if (reference == 2)
        pic->flags |= VA_PICTURE_H264_LONG_TERM_REFERENCE;
*/
    if (reference == 2)
        pic->flags = VA_PICTURE_H264_BOTTOM_FIELD;

    if (pic->picture_id == -1)
    {
        pic->TopFieldOrderCnt = 0;
        pic->BottomFieldOrderCnt = 0;
        pic->flags = VA_PICTURE_H264_INVALID;
    }
}

Ipp32s PackerVA::FillRefFrame(VAPictureH264 * pic, const H265DecoderFrame *pFrame,
                            ReferenceFlags_H265 flags, bool isField, Ipp32s defaultIndex)
{
    Ipp32s index = pFrame->m_index;

    if (index == -1)
        index = defaultIndex;

    pic->picture_id = m_va->GetSurfaceID(index);
    pic->TopFieldOrderCnt = pFrame->m_PicOrderCnt[pFrame->GetNumberByParity(0)];
    pic->BottomFieldOrderCnt = pFrame->m_PicOrderCnt[pFrame->GetNumberByParity(1)];

    pic->flags = 0;

    if (isField)
    {
        pic->flags |= flags.field ? VA_PICTURE_H264_BOTTOM_FIELD : VA_PICTURE_H264_TOP_FIELD;
    }

    pic->flags |= flags.isLongReference ? VA_PICTURE_H264_LONG_TERM_REFERENCE : VA_PICTURE_H264_SHORT_TERM_REFERENCE;

    if (pic->picture_id == -1)
    {
        pic->TopFieldOrderCnt = 0;
        pic->BottomFieldOrderCnt = 0;
        pic->flags = VA_PICTURE_H264_INVALID;
    }

    return pic->picture_id;
}

void PackerVA::FillFrameAsInvalid(VAPictureH264 * pic)
{
    pic->picture_id = VA_INVALID_SURFACE;
    pic->TopFieldOrderCnt = 0;
    pic->BottomFieldOrderCnt = 0;
    pic->flags = VA_PICTURE_H264_INVALID;
}

void PackerVA::PackPicParams(const H265DecoderFrame *pCurrentFrame,
                        H265DecoderFrameInfo * pSliceInfo,
                        H265DBPList * pDPBList)
{
    H265Slice  * pSlice = pSliceInfo->GetSlice(0);
    const H265SliceHeader *pSliceHeader = pSlice->GetSliceHeader();
    const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
    const H265PicParamSet *pPicParamSet = pSlice->GetPicParam();

    UMCVACompBuffer *picParamBuf;
    VAPictureParameterBufferH264* pPicParams_H264 =
        (VAPictureParameterBufferH264*)m_va->GetCompBuffer(VAPictureParameterBufferType, &picParamBuf, sizeof(VAPictureParameterBufferH264));
    memset(pPicParams_H264, 0, sizeof(VAPictureParameterBufferH264));

    Ipp32s reference = pCurrentFrame->isShortTermRef() ? 1 : (pCurrentFrame->isLongTermRef() ? 2 : 0);
    FillFrame(&(pPicParams_H264->CurrPic), pCurrentFrame, pSliceHeader->bottom_field_flag, reference, 0);

    pPicParams_H264->CurrPic.flags = 0;
    if (reference == 1)
        pPicParams_H264->CurrPic.flags |= VA_PICTURE_H264_SHORT_TERM_REFERENCE;

    if (reference == 2)
        pPicParams_H264->CurrPic.flags |= VA_PICTURE_H264_LONG_TERM_REFERENCE;

    if (pSliceHeader->field_pic_flag)
    {
        if (!pSliceHeader->bottom_field_flag)
                pPicParams_H264->CurrPic.BottomFieldOrderCnt = 0;
            else
                pPicParams_H264->CurrPic.TopFieldOrderCnt = 0;
    }

    //packing
    pPicParams_H264->picture_width_in_mbs_minus1 = (unsigned short)(pSeqParamSet->frame_width_in_mbs - 1);
    pPicParams_H264->picture_height_in_mbs_minus1 = (unsigned short)(pSeqParamSet->frame_height_in_mbs - 1);

    pPicParams_H264->bit_depth_luma_minus8 = (unsigned char)(pSeqParamSet->bit_depth_luma - 8);
    pPicParams_H264->bit_depth_chroma_minus8 = (unsigned char)(pSeqParamSet->bit_depth_chroma - 8);

    pPicParams_H264->num_ref_frames = (unsigned char)pSeqParamSet->num_ref_frames;

    pPicParams_H264->chroma_format_idc = pSeqParamSet->chroma_format_idc;
    pPicParams_H264->residual_colour_transform_flag = pSeqParamSet->residual_colour_transform_flag;
    pPicParams_H264->frame_mbs_only_flag = pSeqParamSet->frame_mbs_only_flag;
    pPicParams_H264->mb_adaptive_frame_field_flag = pSliceHeader->MbaffFrameFlag;
    pPicParams_H264->direct_8x8_inference_flag = pSeqParamSet->direct_8x8_inference_flag;
    pPicParams_H264->MinLumaBiPredSize8x8 = pSeqParamSet->level_idc > 30 ? 1 : 0;

    pPicParams_H264->num_slice_groups_minus1 = (unsigned char)(pPicParamSet->num_slice_groups - 1);
    pPicParams_H264->slice_group_map_type = (unsigned char)pPicParamSet->SliceGroupInfo.slice_group_map_type;
    pPicParams_H264->pic_init_qp_minus26 = (unsigned char)(pPicParamSet->pic_init_qp - 26);
    pPicParams_H264->chroma_qp_index_offset = (unsigned char)pPicParamSet->chroma_qp_index_offset[0];
    pPicParams_H264->second_chroma_qp_index_offset = (unsigned char)pPicParamSet->chroma_qp_index_offset[1];

    pPicParams_H264->entropy_coding_mode_flag = pPicParamSet->entropy_coding_mode;
    pPicParams_H264->weighted_pred_flag = pPicParamSet->weighted_pred_flag;
    pPicParams_H264->weighted_bipred_idc = pPicParamSet->weighted_bipred_idc;
    pPicParams_H264->transform_8x8_mode_flag = pPicParamSet->transform_8x8_mode_flag;
    pPicParams_H264->constrained_intra_pred_flag = pPicParamSet->constrained_intra_pred_flag;
    pPicParams_H264->field_pic_flag = pSliceHeader->field_pic_flag;

    pPicParams_H264->frame_num = (unsigned short)pSliceHeader->frame_num;

    //create reference picture list
    for (Ipp32s i = 0; i < 16; i++)
    {
        FillFrameAsInvalid(&(pPicParams_H264->ReferenceFrames[i]));
    }

    Ipp32s referenceCount = 0;
    Ipp32s j = 0;

    bool isSkipFirst = true;

    Ipp32s dpbSize = pDPBList->GetDPBSize();
    for (H265DecoderFrame * pFrm = pDPBList->head(); pFrm && (j < dpbSize); pFrm = pFrm->future())
    {
        VM_ASSERT(j < dpbSize);

        Ipp32s defaultIndex = 0;

        if ((0 == pCurrentFrame->m_index) && !pFrm->IsFrameExist())
        {
            defaultIndex = 1;
        }

        if (pFrm == pCurrentFrame)
        {
            if (pCurrentFrame->m_pSlicesInfo != pSliceInfo)
            {
                reference = pFrm->isShortTermRef() ? 1 : (pFrm->isLongTermRef() ? 2 : 0);
                referenceCount ++;
                Ipp32s field = pFrm->m_bottom_field_flag[0];
                FillFrame(&(pPicParams_H264->ReferenceFrames[j]), pFrm,
                    field, reference, defaultIndex);

                if (field)
                {
                    pPicParams_H264->ReferenceFrames[j].TopFieldOrderCnt = 0;
                    pPicParams_H264->ReferenceFrames[j].BottomFieldOrderCnt = pFrm->m_PicOrderCnt[pFrm->GetNumberByParity(field)];
                }
                else
                {
                    pPicParams_H264->ReferenceFrames[j].TopFieldOrderCnt = pFrm->m_PicOrderCnt[pFrm->GetNumberByParity(field)];
                    pPicParams_H264->ReferenceFrames[j].BottomFieldOrderCnt = 0;
                }

                j++;
            }
            continue;
        }

        reference = pFrm->isShortTermRef() ? 1 : (pFrm->isLongTermRef() ? 2 : 0);
        referenceCount ++;
        FillFrame(&(pPicParams_H264->ReferenceFrames[j]), pFrm, 0, reference, defaultIndex);

        j++;
    }

    pPicParams_H264->num_ref_frames = referenceCount;
    picParamBuf->SetDataSize(sizeof(VAPictureParameterBufferH264));

    //pack quantization matrix
    PackQmatrix(pSlice->GetPicParam());

    Ipp32s count = pSliceInfo->GetSliceCount();
    // set data size
    m_va->GetCompBuffer(VASliceParameterBufferType, &picParamBuf, sizeof(VASliceParameterBufferH264)*(count));
    picParamBuf->SetNumOfItem(count);

    Ipp32s size = 0;

    for (Ipp32s i = 0; i < count; i++)
    {
        H265Slice  * pSlice = pSliceInfo->GetSlice(i);

        Ipp8u *pNalUnit; //ptr to first byte of start code
        Ipp32u NalUnitSize; // size of NAL unit in byte
        H265Bitstream *pBitstream = pSlice->GetBitStreamDXVA();

        pBitstream->GetOrg((Ipp32u**)&pNalUnit, &NalUnitSize);
        Ipp32s AlignedNalUnitSize = NalUnitSize;

        size += AlignedNalUnitSize;
    }

    Ipp32s AlignedNalUnitSize = ALIGN_VALUE_H265<Ipp32s>(size, 128);

    UMCVACompBuffer* compBuf;
    m_va->GetCompBuffer(VASliceDataBufferType, &compBuf, AlignedNalUnitSize);
    if (compBuf->GetBufferSize() )
    {

    }

    memset((Ipp8u*)compBuf->GetPtr() + size, 0, AlignedNalUnitSize - size);

    compBuf->SetDataSize(0);
}

bool PackerVA::PackSliceParams(H265Slice *pSlice, Ipp32s sliceNum, bool all_data)
{
    bool partial_data = false;
    H265DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();
    const H265SliceHeader *pSliceHeader = pSlice->GetSliceHeader();

    VAPictureParameterBufferH264* pPicParams_H264 = (VAPictureParameterBufferH264*)m_va->GetCompBuffer(VAPictureParameterBufferType);

    UMCVACompBuffer* compBuf;
    VASliceParameterBufferH264* pSlice_H264 = (VASliceParameterBufferH264*)m_va->GetCompBuffer(VASliceParameterBufferType, &compBuf);
    pSlice_H264 += sliceNum;
    memset(pSlice_H264, 0, sizeof(VASliceParameterBufferH264));

    Ipp8u *pNalUnit; //ptr to first byte of start code
    Ipp32u NalUnitSize; // size of NAL unit in byte
    H265Bitstream *pBitstream = pSlice->GetBitStreamDXVA();

    pBitstream->GetOrg((Ipp32u**)&pNalUnit, &NalUnitSize);

    if (!all_data)
    {
        NalUnitSize = pBitstream->BytesLeft();
        pNalUnit += pBitstream->BytesDecoded();
    }

    UMCVACompBuffer* CompBuf;
    Ipp8u *pBitStreamBuffer = (Ipp8u*)m_va->GetCompBuffer(VASliceDataBufferType, &CompBuf);
    Ipp32s AlignedNalUnitSize = NalUnitSize;

    pSlice_H264->slice_data_flag = all_data ? VA_SLICE_DATA_FLAG_ALL : VA_SLICE_DATA_FLAG_END;

    if (CompBuf->GetBufferSize() - CompBuf->GetDataSize() < AlignedNalUnitSize)
    {
        AlignedNalUnitSize = NalUnitSize = CompBuf->GetBufferSize() - CompBuf->GetDataSize();
        pBitstream->SetDecodedBytes(pBitstream->BytesDecoded() + NalUnitSize);
        pSlice_H264->slice_data_flag = all_data ? VA_SLICE_DATA_FLAG_BEGIN : VA_SLICE_DATA_FLAG_MIDDLE;
        partial_data = true;
    }

    pSlice_H264->slice_data_size = NalUnitSize;

    pSlice_H264->slice_data_offset = CompBuf->GetDataSize();
    CompBuf->SetDataSize(pSlice_H264->slice_data_offset + AlignedNalUnitSize);

    VM_ASSERT (CompBuf->GetBufferSize() >= pSlice_H264->slice_data_offset + AlignedNalUnitSize);

    pBitStreamBuffer += pSlice_H264->slice_data_offset;
    memcpy(pBitStreamBuffer, pNalUnit, NalUnitSize);
    memset(pBitStreamBuffer + NalUnitSize, 0, AlignedNalUnitSize - NalUnitSize);

    Ipp8u *pSliceData; //ptr to slice data
    Ipp32u SliceDataOffset; //offset to first bit of slice data from first bit of slice header minus all 0x03
    pBitstream->GetState((Ipp32u**)&pSliceData, &SliceDataOffset); //it is supposed that slice header was already parsed

    SliceDataOffset = 31 - SliceDataOffset;
    pSliceData += SliceDataOffset/8;
    SliceDataOffset = (Ipp32u)(SliceDataOffset%8 + 8 * (pSliceData - pNalUnit)); //from start code to slice data

    Ipp32s k = 0;
    for(Ipp8u *ptr = pNalUnit; ptr < pSliceData; ptr++)
        if(ptr[0]==0 && ptr[1]==0 && ptr[2]==3)
            k++;

    //k = 0;
    pSlice_H264->slice_data_bit_offset = (unsigned short)(SliceDataOffset + 8*k);

    pSlice_H264->first_mb_in_slice = (unsigned short)(pSlice->GetSliceHeader()->first_mb_in_slice >> pSlice->GetSliceHeader()->MbaffFrameFlag);
    pSlice_H264->slice_type = (unsigned char)pSliceHeader->slice_type;
    pSlice_H264->direct_spatial_mv_pred_flag = (unsigned char)pSliceHeader->direct_spatial_mv_pred_flag;
    pSlice_H264->cabac_init_idc = (unsigned char)(pSliceHeader->cabac_init_idc);
    pSlice_H264->slice_qp_delta = (char)pSliceHeader->slice_qp_delta;
    pSlice_H264->disable_deblocking_filter_idc = (unsigned char)pSliceHeader->disable_deblocking_filter_idc;
    pSlice_H264->luma_log2_weight_denom = (unsigned char)pSliceHeader->luma_log2_weight_denom;
    pSlice_H264->chroma_log2_weight_denom = (unsigned char)pSliceHeader->chroma_log2_weight_denom;

    if (pSliceHeader->num_ref_idx_active_override_flag != 0)
    {
        pSlice_H264->num_ref_idx_l0_active_minus1 = (unsigned char)(pSliceHeader->num_ref_idx_l0_active-1);
        if (BPREDSLICE == pSliceHeader->slice_type)
            pSlice_H264->num_ref_idx_l1_active_minus1 = (unsigned char)(pSliceHeader->num_ref_idx_l1_active-1);
    }
    else
    {
        pSlice_H264->num_ref_idx_l0_active_minus1 = pSlice->GetPicParam()->num_ref_idx_l0_active - 1;
        pSlice_H264->num_ref_idx_l1_active_minus1 = pSlice->GetPicParam()->num_ref_idx_l1_active - 1;
    }

    if (pSliceHeader->disable_deblocking_filter_idc != 1)
    {
        pSlice_H264->slice_alpha_c0_offset_div2 = (char)(pSliceHeader->slice_alpha_c0_offset / 2);
        pSlice_H264->slice_beta_offset_div2 = (char)(pSliceHeader->slice_beta_offset / 2);
    }

    if ( (pPicParams_H264->weighted_pred_flag &&
          ((PREDSLICE == pSliceHeader->slice_type) || (S_PREDSLICE == pSliceHeader->slice_type))) ||
         ((pPicParams_H264->weighted_bipred_idc == 1) && (BPREDSLICE == pSliceHeader->slice_type)))
    {
        //Weights
        const PredWeightTable_H265 *pPredWeight[2];
        pPredWeight[0] = pSlice->GetPredWeigthTable(0);
        pPredWeight[1] = pSlice->GetPredWeigthTable(1);

        Ipp32s  i;
        for(i=0; i < 32; i++)
        {
            pSlice_H264->luma_weight_l0[i] = pPredWeight[0][i].luma_weight;
            pSlice_H264->luma_offset_l0[i] = pPredWeight[0][i].luma_offset;
            pSlice_H264->chroma_weight_l0[i][0] = pPredWeight[0][i].chroma_weight[0];
            pSlice_H264->chroma_offset_l0[i][0] = pPredWeight[0][i].chroma_offset[0];
            pSlice_H264->chroma_weight_l0[i][1] = pPredWeight[0][i].chroma_weight[1];
            pSlice_H264->chroma_offset_l0[i][1] = pPredWeight[0][i].chroma_offset[1];
        }

        for(i=0; i < 32; i++)
        {
            pSlice_H264->luma_weight_l1[i] = pPredWeight[1][i].luma_weight;
            pSlice_H264->luma_offset_l1[i] = pPredWeight[1][i].luma_offset;
            pSlice_H264->chroma_weight_l1[i][0] = pPredWeight[1][i].chroma_weight[0];
            pSlice_H264->chroma_offset_l1[i][0] = pPredWeight[1][i].chroma_offset[0];
            pSlice_H264->chroma_weight_l1[i][1] = pPredWeight[1][i].chroma_weight[1];
            pSlice_H264->chroma_offset_l1[i][1] = pPredWeight[1][i].chroma_offset[1];
        }
    }
    else
    {
        pSlice_H264->luma_log2_weight_denom = (unsigned char)5;
        pSlice_H264->chroma_log2_weight_denom = (unsigned char)5;
    }

    Ipp32s realSliceNum = pSlice->GetSliceNum();
    H265DecoderFrame **pRefPicList0 = pCurrentFrame->GetRefPicList(realSliceNum, 0)->m_RefPicList;
    H265DecoderFrame **pRefPicList1 = pCurrentFrame->GetRefPicList(realSliceNum, 1)->m_RefPicList;
    ReferenceFlags_H265 *pFields0 = pCurrentFrame->GetRefPicList(realSliceNum, 0)->m_Flags;
    ReferenceFlags_H265 *pFields1 = pCurrentFrame->GetRefPicList(realSliceNum, 1)->m_Flags;

    Ipp32s i,j;
    for(i = 0; i < 32; i++)
    {
        if (pRefPicList0[i] != NULL && i < pSliceHeader->num_ref_idx_l0_active)
        {
            Ipp32s defaultIndex = ((0 == pCurrentFrame->m_index) && !pRefPicList0[i]->IsFrameExist()) ? 1 : 0;

            Ipp32s idx = FillRefFrame(&(pSlice_H264->RefPicList0[i]), pRefPicList0[i],
                pFields0[i], pSliceHeader->field_pic_flag, defaultIndex);

            if (pSlice_H264->RefPicList0[i].picture_id == pPicParams_H264->CurrPic.picture_id &&
                pRefPicList0[i]->IsFrameExist())
            {
                pSlice_H264->RefPicList0[i].BottomFieldOrderCnt = 0;
            }

            /*bool isFound = false;
            for(j = 0; j < 16; j++) //index in RefFrameList array
            {
                if (idx == pPicParams_H264->ReferenceFrames[j].picture_id)
                {
                    isFound = true;
                    break;
                }

                if (pPicParams_H264->ReferenceFrames[j].flags == VA_PICTURE_H264_INVALID)
                    break;
            }

            if (!isFound) // add to ref list of picParams
            {
                //if pRefPicList0[i]
                VM_ASSERT(false);
                FillFrameAsInvalid(&(pSlice_H264->RefPicList0[i]));
            }*/
        }
        else
        {
            FillFrameAsInvalid(&(pSlice_H264->RefPicList0[i]));
        }

        if (pRefPicList1[i] != NULL && i < pSliceHeader->num_ref_idx_l1_active)
        {
            Ipp32s defaultIndex = ((0 == pCurrentFrame->m_index) && !pRefPicList1[i]->IsFrameExist()) ? 1 : 0;

            Ipp32s idx = FillRefFrame(&(pSlice_H264->RefPicList1[i]), pRefPicList1[i],
                pFields1[i], pSliceHeader->field_pic_flag, defaultIndex);

            if (pSlice_H264->RefPicList1[i].picture_id == pPicParams_H264->CurrPic.picture_id && pRefPicList1[i]->IsFrameExist())
            {
                pSlice_H264->RefPicList1[i].BottomFieldOrderCnt = 0;
            }

            /*bool isFound = false;
            for(j = 0; j < 16; j++) //index in RefFrameList array
            {
                if (idx == pPicParams_H264->ReferenceFrames[j].picture_id)
                {
                    isFound = true;
                    break;
                }

                if (pPicParams_H264->ReferenceFrames[j].flags == VA_PICTURE_H264_INVALID)
                    break;
            }

            if (!isFound) // add to ref list of picParams
            {
                VM_ASSERT(false);
                FillFrameAsInvalid(&(pSlice_H264->RefPicList1[i]));
            }*/
        }
        else
        {
            FillFrameAsInvalid(&(pSlice_H264->RefPicList1[i]));
        }
    }

    return !partial_data;
}

void PackerVA::PackQmatrix(const H265PicParamSet *pPicParamSet)
{
    UMCVACompBuffer *quantBuf;
    VAIQMatrixBufferH264* pQmatrix_H264 = (VAIQMatrixBufferH264*)m_va->GetCompBuffer(VAIQMatrixBufferType, &quantBuf, sizeof(VAIQMatrixBufferH264));
    quantBuf->SetDataSize(sizeof(VAIQMatrixBufferH264));

    Ipp32s i, j;
    //may be just use memcpy???
    for(j = 0; j < 6; j++)
    {
        for(i = 0; i < 16; i++)
        {
             pQmatrix_H264->ScalingList4x4[j][i] = pPicParamSet->ScalingLists4x4[j].ScalingListCoeffs[i];
        }
    }

    for(j = 0; j < 2; j++)
    {
        for(i = 0; i < 64; i++)
        {
             pQmatrix_H264->ScalingList8x8[j][i] = pPicParamSet->ScalingLists8x8[j].ScalingListCoeffs[i];
        }
    }
}

void PackerVA::PackDeblockingParameters(
    const DeblockingParametersMBAFF *,
    const H265SliceHeader *,
    const H265DecoderFrame *,
    const H265DecoderLocalMacroblockDescriptor *,
    const H265PicParamSet *)
{
}
#endif // UMC_VA_LINUX

} // namespace UMC_HEVC_DECODER

#endif // UMC_RESTRICTED_CODE_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER
