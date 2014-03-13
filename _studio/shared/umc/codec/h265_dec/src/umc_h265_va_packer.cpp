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

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef UMC_RESTRICTED_CODE_VA

#include "umc_h265_va_packer.h"

#ifdef UMC_VA
#include "umc_h265_task_supplier.h"
#endif

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

void PackerDXVA2::GetPicParamVABuffer(DXVA_Intel_PicParams_HEVC **ppPicParam, size_t headerSize)
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

        *ppPicParam = (DXVA_Intel_PicParams_HEVC *)((char *)headBffr + headBffrOffs);
        compBuf->SetDataSize(headBffrOffs + headerSize);

        memset(*ppPicParam, 0, headerSize);

        return;
    }
}

void PackerDXVA2::GetSliceVABuffers(
    DXVA_Intel_Slice_HEVC_Long **ppSliceHeader, size_t headerSize, 
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

        *ppSliceHeader = (DXVA_Intel_Slice_HEVC_Long *)((char *)headBffr + headBffrOffs);
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

void PackerDXVA2::GetIQMVABuffer(DXVA_Intel_Qmatrix_HEVC **ppQmatrix, size_t size)
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

        *ppQmatrix = (DXVA_Intel_Qmatrix_HEVC *)((char *)headBffr + bffrOffs);
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

#if HEVC_SPEC_VER == MK_HEVCVER(0, 84) || HEVC_SPEC_VER == MK_HEVCVER(0, 83)
void PackerDXVA2::PackPicParams(const H265DecoderFrame *pCurrentFrame,
                        H265DecoderFrameInfo * pSliceInfo,
                        TaskSupplier_H265 *supplier)
{
    DXVA_Intel_PicParams_HEVC* pPicParam = 0;
    GetPicParamVABuffer(&pPicParam, sizeof(DXVA_Intel_PicParams_HEVC));

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
    pPicParam->PicFlags.fields.scaling_list_enabled_flag                    = pSeqParamSet->scaling_list_enabled_flag ? 1 : 0 ;
    pPicParam->PicFlags.fields.transform_skip_enabled_flag                  = pPicParamSet->transform_skip_enabled_flag ? 1 : 0 ;
    pPicParam->PicFlags.fields.amp_enabled_flag                             = pSeqParamSet->getUseAMP() ? 1 : 0 ;
    pPicParam->PicFlags.fields.strong_intra_smoothing_enabled_flag          = pSeqParamSet->getUseStrongIntraSmoothing() ? 1 : 0 ;
    pPicParam->PicFlags.fields.sign_data_hiding_flag                        = pPicParamSet->sign_data_hiding_enabled_flag ? 1 : 0 ;
    pPicParam->PicFlags.fields.constrained_intra_pred_flag                  = pPicParamSet->constrained_intra_pred_flag ? 1 : 0 ;
    pPicParam->PicFlags.fields.cu_qp_delta_enabled_flag                     = pPicParamSet->cu_qp_delta_enabled_flag;
    pPicParam->PicFlags.fields.weighted_pred_flag                           = pPicParamSet->weighted_pred_flag ? 1 : 0 ;
    pPicParam->PicFlags.fields.weighted_bipred_flag                         = pPicParamSet->weighted_bipred_flag ? 1 : 0 ;
    pPicParam->PicFlags.fields.transquant_bypass_enabled_flag               = pPicParamSet->transquant_bypass_enabled_flag ? 1 : 0 ;    
    pPicParam->PicFlags.fields.tiles_enabled_flag                           = pPicParamSet->tiles_enabled_flag;
    pPicParam->PicFlags.fields.entropy_coding_sync_enabled_flag             = pPicParamSet->entropy_coding_sync_enabled_flag;
    pPicParam->PicFlags.fields.pps_loop_filter_across_slices_enabled_flag   = pPicParamSet->pps_loop_filter_across_slices_enabled_flag;
    pPicParam->PicFlags.fields.loop_filter_across_tiles_enabled_flag        = pPicParamSet->loop_filter_across_tiles_enabled_flag;
    pPicParam->PicFlags.fields.pcm_loop_filter_disabled_flag                = pSeqParamSet->getPCMFilterDisableFlag() ? 1 : 0 ;
    pPicParam->PicFlags.fields.field_pic_flag                               = 0;
    pPicParam->PicFlags.fields.bottom_field_flag                            = 0;
    pPicParam->PicFlags.fields.NoPicReorderingFlag                          = 0;
    pPicParam->PicFlags.fields.NoBiPredFlag                                 = 0;
    
    //
    //
    pPicParam->CurrPic.Index7bits   = pCurrentFrame->m_index;    // ?
    pPicParam->CurrPicOrderCntVal   = pSlice->GetSliceHeader()->slice_pic_order_cnt_lsb;

    int numRefPicSetStCurrBefore = 0,
        numRefPicSetStCurrAfter  = 0,
        numRefPicSetLtCurr       = 0,
        index, pocList[3*8];
    ReferencePictureSet *rps = pSlice->getRPS();

    for(index = 0; index < rps->getNumberOfNegativePictures(); index++)
        if(rps->getUsed(index))
            pocList[numRefPicSetStCurrBefore++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);

    for(; index < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures(); index++)
        if(rps->getUsed(index))
            pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);

    int num_ref_frames = pSeqParamSet->sps_max_dec_pic_buffering[pSlice->GetSliceHeader()->nuh_temporal_id];

    pPicParam->sps_max_dec_pic_buffering_minus1 = (UCHAR)(num_ref_frames - 1);

    int count = 0;
    int cntRefPicSetStCurrBefore = 0,
        cntRefPicSetStCurrAfter  = 0,
        cntRefPicSetLtCurr = 0;
    H265DBPList *dpb = supplier->GetDPBList();
    for(H265DecoderFrame* frame = dpb->head() ; frame && count < sizeof(pPicParam->RefFrameList)/sizeof(pPicParam->RefFrameList[0]) ; frame = frame->future())
    {
        if(frame != pCurrentFrame)
        {
            int refType = frame->isShortTermRef() ? SHORT_TERM_REFERENCE : (frame->isLongTermRef() ? LONG_TERM_REFERENCE : NO_REFERENCE);

            if(refType != NO_REFERENCE)
            {
                pPicParam->PicOrderCntValList[count] = frame->m_PicOrderCnt;

                pPicParam->RefFrameList[count].Index7bits = frame->m_index;
                pPicParam->RefFrameList[count].long_term_ref_flag = (refType == LONG_TERM_REFERENCE);

                for(int n=0 ; n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter ; n++)
                {
                    if(frame->m_PicOrderCnt == pocList[n])
                    {
                        if(n < numRefPicSetStCurrBefore)
                        {
                            if(cntRefPicSetStCurrBefore < sizeof(pPicParam->RefPicSetStCurrBefore)/sizeof(pPicParam->RefPicSetStCurrBefore[0]))
                                pPicParam->RefPicSetStCurrBefore[cntRefPicSetStCurrBefore++] = count;
                        }
                        else if(n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter)
                        {
                            if(cntRefPicSetStCurrAfter < sizeof(pPicParam->RefPicSetStCurrAfter)/sizeof(pPicParam->RefPicSetStCurrAfter[0]))
                                pPicParam->RefPicSetStCurrAfter[cntRefPicSetStCurrAfter++] = count;
                        }
                        else if(n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr)
                        {
                            if(cntRefPicSetLtCurr < sizeof(pPicParam->RefPicSetLtCurr)/sizeof(pPicParam->RefPicSetLtCurr[0]))
                                pPicParam->RefPicSetLtCurr[cntRefPicSetLtCurr++] = count;
                        }
                    }
                }

                m_refFrameListCache[count].bPicEntry = pPicParam->RefFrameList[count].bPicEntry;    // save to use in slice param packer
                count++;
            }
        }
    }
    m_refFrameListCacheSize = count;
    for(int n=count;n < sizeof(pPicParam->RefFrameList)/sizeof(pPicParam->RefFrameList[0]);n++)
    {
        pPicParam->RefFrameList[n].bPicEntry = (UCHAR)0xff;
        pPicParam->PicOrderCntValList[count] = -1;
    }

    for(int i=0;i < 8;i++)
    {
        if(i >= cntRefPicSetStCurrBefore)
            pPicParam->RefPicSetStCurrBefore[i] = 0xff;
        if(i >= cntRefPicSetStCurrAfter)
            pPicParam->RefPicSetStCurrAfter[i] = 0xff;
        if(i >= cntRefPicSetLtCurr)
            pPicParam->RefPicSetLtCurr[i] = 0xff;
    }


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
    pPicParam->log2_min_transform_block_size_minus2         = (UCHAR)(pSeqParamSet->log2_min_transform_block_size - 2);
    pPicParam->log2_diff_max_min_transform_block_size       = (UCHAR)(pSeqParamSet->log2_max_transform_block_size - pSeqParamSet->log2_min_transform_block_size);
    pPicParam->log2_min_pcm_luma_coding_block_size_minus3   = (UCHAR)(pSeqParamSet->log2_min_pcm_luma_coding_block_size - 3);
    pPicParam->log2_diff_max_min_pcm_luma_coding_block_size = (UCHAR)(pSeqParamSet->log2_max_pcm_luma_coding_block_size - pSeqParamSet->log2_min_pcm_luma_coding_block_size);
    pPicParam->max_transform_hierarchy_depth_intra          = (UCHAR)pSeqParamSet->max_transform_hierarchy_depth_intra;
    pPicParam->max_transform_hierarchy_depth_inter          = (UCHAR)pSeqParamSet->max_transform_hierarchy_depth_inter;
    pPicParam->init_qp_minus26                              = (CHAR)pPicParamSet->init_qp - 26;
    pPicParam->diff_cu_qp_delta_depth                       = (UCHAR)(pPicParamSet->diff_cu_qp_delta_depth);
    pPicParam->pps_cb_qp_offset                             = (CHAR)pPicParamSet->pps_cb_qp_offset;
    pPicParam->pps_cr_qp_offset                             = (CHAR)pPicParamSet->pps_cr_qp_offset;
    if(pPicParam->PicFlags.fields.tiles_enabled_flag)
    {
        pPicParam->num_tile_columns_minus1          = (UCHAR)pPicParamSet->num_tile_columns - 1;
        pPicParam->num_tile_rows_minus1             = (UCHAR)pPicParamSet->num_tile_rows - 1;
        for (Ipp32s i = 0; i < pPicParamSet->num_tile_columns; i++)
            pPicParam->column_width_minus1[i] = (Ipp16u)(pPicParamSet->column_width[i] - 1);

        for (Ipp32s i = 0; i < pPicParamSet->num_tile_rows; i++)
            pPicParam->row_height_minus1[i] = (Ipp16u)(pPicParamSet->row_height[i] - 1);
    }
    pPicParam->log2_parallel_merge_level_minus2     = (UCHAR)pPicParamSet->log2_parallel_merge_level - 2;
    pPicParam->StatusReportFeedbackNumber = m_statusReportFeedbackCounter;

    pPicParam->continuation_flag = 1;

    // PicShortFormatFlags
    //
    pPicParam->PicShortFormatFlags.fields.lists_modification_present_flag               = pPicParamSet->lists_modification_present_flag ? 1 : 0 ;
    pPicParam->PicShortFormatFlags.fields.long_term_ref_pics_present_flag               = pSeqParamSet->getLongTermRefsPresent() ? 1 : 0 ;
    pPicParam->PicShortFormatFlags.fields.sps_temporal_mvp_enabled_flag                 = pSeqParamSet->getTMVPFlagsPresent() ? 1 : 0 ;
    pPicParam->PicShortFormatFlags.fields.cabac_init_present_flag                       = pPicParamSet->cabac_init_present_flag ? 1 : 0 ;
    pPicParam->PicShortFormatFlags.fields.output_flag_present_flag                      = pPicParamSet->output_flag_present_flag ? 1 : 0 ;
    pPicParam->PicShortFormatFlags.fields.dependent_slice_segments_enabled_flag         = pPicParamSet->dependent_slice_segments_enabled_flag ? 1 : 0 ;
    pPicParam->PicShortFormatFlags.fields.pps_slice_chroma_qp_offsets_present_flag      = pPicParamSet->pps_slice_chroma_qp_offsets_present_flag ? 1 : 0 ;
    pPicParam->PicShortFormatFlags.fields.sample_adaptive_offset_enabled_flag           = pSeqParamSet->getUseSAO() ? 1 : 0 ;
    pPicParam->PicShortFormatFlags.fields.deblocking_filter_override_enabled_flag       = pPicParamSet->deblocking_filter_override_enabled_flag ? 1 : 0 ;
    pPicParam->PicShortFormatFlags.fields.pps_disable_deblocking_filter_flag            = pPicParamSet->pps_deblocking_filter_disabled_flag ? 1 : 0 ;
    pPicParam->PicShortFormatFlags.fields.IrapPicFlag                                   = 0;
    pPicParam->PicShortFormatFlags.fields.IdrPicFlag                                    = 0;
    pPicParam->PicShortFormatFlags.fields.IntraPicFlag                                  = 0;
    pPicParam->PicShortFormatFlags.fields.slice_segment_header_extension_present_flag   = pPicParamSet->slice_segment_header_extension_present_flag ? 1 : 0 ;

    //
    //
    pPicParam->log2_max_pic_order_cnt_lsb_minus4    = (UCHAR)(pSeqParamSet->log2_max_pic_order_cnt_lsb - 4);
    pPicParam->num_short_term_ref_pic_sets          = (UCHAR)pSeqParamSet->getRPSList()->getNumberOfReferencePictureSets();
    pPicParam->num_long_term_ref_pics_sps           = (UCHAR)pSeqParamSet->getNumLongTermRefPicSPS();
    pPicParam->num_ref_idx_l0_default_active_minus1 = (UCHAR)(pPicParamSet->num_ref_idx_l0_default_active - 1);
    pPicParam->num_ref_idx_l1_default_active_minus1 = (UCHAR)(pPicParamSet->num_ref_idx_l1_default_active - 1);
    pPicParam->pps_beta_offset_div2                 = (CHAR)(pPicParamSet->pps_beta_offset >> 1);
    pPicParam->pps_tc_offset_div2                   = (CHAR)(pPicParamSet->pps_tc_offset >> 1);    
    pPicParam->StRPSBits                            = 0;    // TODO
    pPicParam->num_extra_slice_header_bits          = (UCHAR)pPicParamSet->num_extra_slice_header_bits;

    //pPicParam->CollocatedRefIdx.Index7bits          = (UCHAR)(pSlice->getSliceType() != I_SLICE ? pSlice->GetSliceHeader()->collocated_ref_idx : -1 );
    //pPicParam->PicShortFormatFlags.fields.deblocking_filter_control_present_flag     = pPicParamSet->getDeblockingFilterControlPresentFlag() ? 1 : 0 ;
    //pPicParam->PicShortFormatFlags.fields.slice_temporal_mvp_enabled_flag            = pSlice->GetSliceHeader()->slice_temporal_mvp_enabled_flag ? 1 : 0 ;
    //for(unsigned k=0;k < 32 && k < pSeqParamSet->getNumLongTermRefPicSPS();k++)
    //    pPicParam->lt_ref_pic_poc_lsb_sps[k] = (USHORT)pSeqParamSet->getLtRefPicPocLsbSps(k);
    //pPicParam->used_by_curr_pic_lt_sps_flags        = (UINT)0;

}

#elif HEVC_SPEC_VER == MK_HEVCVER(0, 85)

void PackerDXVA2::PackPicParams(const H265DecoderFrame *pCurrentFrame,
                        H265DecoderFrameInfo * pSliceInfo,
                        TaskSupplier_H265 *supplier)
{
    DXVA_Intel_PicParams_HEVC* pPicParam = 0;
    GetPicParamVABuffer(&pPicParam, sizeof(DXVA_Intel_PicParams_HEVC));

    H265Slice *pSlice = pSliceInfo->GetSlice(0);
    H265SliceHeader * sliceHeader = pSlice->GetSliceHeader();
    const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
    const H265PicParamSet *pPicParamSet = pSlice->GetPicParam();
    
    //
    //
    pPicParam->PicWidthInMinCbsY   = (USHORT)LengthInMinCb(pSeqParamSet->pic_width_in_luma_samples, pSeqParamSet->log2_min_luma_coding_block_size);
    pPicParam->PicHeightInMinCbsY  = (USHORT)LengthInMinCb(pSeqParamSet->pic_height_in_luma_samples, pSeqParamSet->log2_min_luma_coding_block_size);

    //
    //
    pPicParam->PicFlags.fields.chroma_format_idc                            = pSeqParamSet->chroma_format_idc;
    pPicParam->PicFlags.fields.separate_colour_plane_flag                   = 0;    // 0 in HEVC spec HM10 by design
    pPicParam->PicFlags.fields.bit_depth_luma_minus8                        = (UCHAR)(pSeqParamSet->bit_depth_luma - 8);
    pPicParam->PicFlags.fields.bit_depth_chroma_minus8                      = (UCHAR)(pSeqParamSet->bit_depth_chroma - 8);
    pPicParam->PicFlags.fields.log2_max_pic_order_cnt_lsb_minus4            = (UCHAR)(pSeqParamSet->log2_max_pic_order_cnt_lsb - 4);
    pPicParam->PicFlags.fields.NoPicReorderingFlag                          = pSeqParamSet->sps_max_num_reorder_pics == 0 ? 1 : 0;
    pPicParam->PicFlags.fields.NoBiPredFlag                                 = 0;
    
    //
    //
    pPicParam->CurrPic.Index7bits   = pCurrentFrame->m_index;    // ?
    pPicParam->CurrPicOrderCntVal   = sliceHeader->slice_pic_order_cnt_lsb;

    int count = 0;
    int cntRefPicSetStCurrBefore = 0,
        cntRefPicSetStCurrAfter  = 0,
        cntRefPicSetLtCurr = 0;
    H265DBPList *dpb = supplier->GetDPBList();
    ReferencePictureSet *rps = pSlice->getRPS();
    for(H265DecoderFrame* frame = dpb->head() ; frame && count < sizeof(pPicParam->RefFrameList)/sizeof(pPicParam->RefFrameList[0]) ; frame = frame->future())
    {
        if(frame != pCurrentFrame)
        {
            int refType = frame->isShortTermRef() ? SHORT_TERM_REFERENCE : (frame->isLongTermRef() ? LONG_TERM_REFERENCE : NO_REFERENCE);

            if(refType != NO_REFERENCE)
            {
                pPicParam->PicOrderCntValList[count] = frame->m_PicOrderCnt;

                pPicParam->RefFrameList[count].Index7bits = frame->m_index;
                pPicParam->RefFrameList[count].long_term_ref_flag = (refType == LONG_TERM_REFERENCE);

                m_refFrameListCache[count].bPicEntry = pPicParam->RefFrameList[count].bPicEntry;    // save to use in slice param packer
                count++;
            }
        }
    }

    int index, pocList[3*8];
    int numRefPicSetStCurrBefore = 0,
        numRefPicSetStCurrAfter  = 0,
        numRefPicSetLtCurr       = 0;
    for(index = 0; index < rps->getNumberOfNegativePictures(); index++)
            pocList[numRefPicSetStCurrBefore++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);
    for(; index < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures(); index++)
            pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);
    for(; index < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures() + rps->getNumberOfLongtermPictures(); index++)
    {
        {
            //pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);

            Ipp32s poc = rps->getPOC(index);
            H265DecoderFrame *pFrm = supplier->GetDPBList()->findLongTermRefPic(pCurrentFrame, poc, pSeqParamSet->log2_max_pic_order_cnt_lsb, !rps->getCheckLTMSBPresent(index));

            if (pFrm)
            {
                pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr++] = pFrm->PicOrderCnt();
            }
            else
            {
                pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);
            }
        }
    }

    for(int n=0 ; n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr ; n++)
    {
        if (!rps->getUsed(n))
            continue;

        for(int k=0;k < count;k++)
        {
            if(pocList[n] == pPicParam->PicOrderCntValList[k])
            {
                if(n < numRefPicSetStCurrBefore)
                    pPicParam->RefPicSetStCurrBefore[cntRefPicSetStCurrBefore++] = (UCHAR)k;
                else if(n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter)
                    pPicParam->RefPicSetStCurrAfter[cntRefPicSetStCurrAfter++] = (UCHAR)k;
                else if(n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr)
                    pPicParam->RefPicSetLtCurr[cntRefPicSetLtCurr++] = (UCHAR)k;
            }
        }
    }

    m_refFrameListCacheSize = count;
    for(int n=count;n < sizeof(pPicParam->RefFrameList)/sizeof(pPicParam->RefFrameList[0]);n++)
    {
        pPicParam->RefFrameList[n].bPicEntry = (UCHAR)0xff;
        pPicParam->PicOrderCntValList[n] = -1;
    }

    for(int i=0;i < 8;i++)
    {
        if(i >= cntRefPicSetStCurrBefore)
            pPicParam->RefPicSetStCurrBefore[i] = 0xff;
        if(i >= cntRefPicSetStCurrAfter)
            pPicParam->RefPicSetStCurrAfter[i] = 0xff;
        if(i >= cntRefPicSetLtCurr)
            pPicParam->RefPicSetLtCurr[i] = 0xff;
    }

    //
    //
    pPicParam->sps_max_dec_pic_buffering_minus1             = (UCHAR)(pSeqParamSet->sps_max_dec_pic_buffering[sliceHeader->nuh_temporal_id] - 1);
    pPicParam->log2_min_luma_coding_block_size_minus3       = (UCHAR)(pSeqParamSet->log2_min_luma_coding_block_size- 3);
    pPicParam->log2_diff_max_min_transform_block_size       = (UCHAR)(pSeqParamSet->log2_max_transform_block_size - pSeqParamSet->log2_min_transform_block_size);
    pPicParam->log2_min_transform_block_size_minus2         = (UCHAR)(pSeqParamSet->log2_min_transform_block_size - 2);
    pPicParam->log2_diff_max_min_luma_coding_block_size     = (UCHAR)(pSeqParamSet->log2_max_luma_coding_block_size - pSeqParamSet->log2_min_luma_coding_block_size);
    pPicParam->max_transform_hierarchy_depth_intra          = (UCHAR)pSeqParamSet->max_transform_hierarchy_depth_intra;
    pPicParam->max_transform_hierarchy_depth_inter          = (UCHAR)pSeqParamSet->max_transform_hierarchy_depth_inter;
    pPicParam->num_short_term_ref_pic_sets                  = (UCHAR)pSeqParamSet->getRPSList()->getNumberOfReferencePictureSets();
    pPicParam->num_long_term_ref_pics_sps                   = (UCHAR)pSeqParamSet->num_long_term_ref_pics_sps;
    pPicParam->num_ref_idx_l0_default_active_minus1         = (UCHAR)(pPicParamSet->num_ref_idx_l0_default_active - 1);
    pPicParam->num_ref_idx_l1_default_active_minus1         = (UCHAR)(pPicParamSet->num_ref_idx_l1_default_active - 1);
    pPicParam->init_qp_minus26                              = (CHAR)pPicParamSet->init_qp - 26;
    
    pPicParam->wNumBitsForShortTermRPSInSlice               = (USHORT)sliceHeader->wNumBitsForShortTermRPSInSlice;
    pPicParam->ucNumDeltaPocsOfRefRpsIdx                    = (UCHAR)pPicParam->wNumBitsForShortTermRPSInSlice;

    // dwCodingParamToolFlags
    //
    pPicParam->fields.scaling_list_enabled_flag                      = pSeqParamSet->scaling_list_enabled_flag ? 1 : 0 ;    
    pPicParam->fields.amp_enabled_flag                               = pSeqParamSet->amp_enabled_flag ? 1 : 0 ;
    pPicParam->fields.sample_adaptive_offset_enabled_flag            = pSeqParamSet->sample_adaptive_offset_enabled_flag ? 1 : 0 ;
    pPicParam->fields.pcm_enabled_flag                               = pSeqParamSet->pcm_enabled_flag ? 1 : 0 ;
    pPicParam->fields.pcm_sample_bit_depth_luma_minus1               = (UCHAR)(pSeqParamSet->pcm_sample_bit_depth_luma - 1);
    pPicParam->fields.pcm_sample_bit_depth_chroma_minus1             = (UCHAR)(pSeqParamSet->pcm_sample_bit_depth_chroma - 1);
    pPicParam->fields.log2_min_pcm_luma_coding_block_size_minus3     = (UCHAR)(pSeqParamSet->log2_min_pcm_luma_coding_block_size - 3);
    pPicParam->fields.log2_diff_max_min_pcm_luma_coding_block_size   = (UCHAR)(pSeqParamSet->log2_max_pcm_luma_coding_block_size - pSeqParamSet->log2_min_pcm_luma_coding_block_size);
    pPicParam->fields.pcm_loop_filter_disabled_flag                  = pSeqParamSet->pcm_loop_filter_disabled_flag ? 1 : 0 ;
    pPicParam->fields.long_term_ref_pics_present_flag                = pSeqParamSet->long_term_ref_pics_present_flag ? 1 : 0 ;
    pPicParam->fields.sps_temporal_mvp_enabled_flag                  = pSeqParamSet->sps_temporal_mvp_enabled_flag ? 1 : 0 ;
    pPicParam->fields.strong_intra_smoothing_enabled_flag            = pSeqParamSet->sps_strong_intra_smoothing_enabled_flag ? 1 : 0 ;
    pPicParam->fields.dependent_slice_segments_enabled_flag          = pPicParamSet->dependent_slice_segments_enabled_flag ? 1 : 0 ;
    pPicParam->fields.output_flag_present_flag                       = pPicParamSet->output_flag_present_flag ? 1 : 0 ;
    pPicParam->fields.num_extra_slice_header_bits                    = (UCHAR)pPicParamSet->num_extra_slice_header_bits;
    pPicParam->fields.sign_data_hiding_flag                          = pPicParamSet->sign_data_hiding_enabled_flag ? 1 : 0 ;
    pPicParam->fields.cabac_init_present_flag                        = pPicParamSet->cabac_init_present_flag ? 1 : 0 ;

    // PicShortFormatFlags
    //
    pPicParam->PicShortFormatFlags.fields.constrained_intra_pred_flag                   = pPicParamSet->constrained_intra_pred_flag ? 1 : 0 ;
    pPicParam->PicShortFormatFlags.fields.transform_skip_enabled_flag                   = pPicParamSet->transform_skip_enabled_flag ? 1 : 0 ;
    pPicParam->PicShortFormatFlags.fields.cu_qp_delta_enabled_flag                      = pPicParamSet->cu_qp_delta_enabled_flag;
    pPicParam->PicShortFormatFlags.fields.pps_slice_chroma_qp_offsets_present_flag      = pPicParamSet->pps_slice_chroma_qp_offsets_present_flag ? 1 : 0 ;
    pPicParam->PicShortFormatFlags.fields.weighted_pred_flag                            = pPicParamSet->weighted_pred_flag ? 1 : 0 ;
    pPicParam->PicShortFormatFlags.fields.weighted_bipred_flag                          = pPicParamSet->weighted_bipred_flag ? 1 : 0 ;
    pPicParam->PicShortFormatFlags.fields.transquant_bypass_enabled_flag                = pPicParamSet->transquant_bypass_enabled_flag ? 1 : 0 ;    
    pPicParam->PicShortFormatFlags.fields.tiles_enabled_flag                            = pPicParamSet->tiles_enabled_flag;
    pPicParam->PicShortFormatFlags.fields.entropy_coding_sync_enabled_flag              = pPicParamSet->entropy_coding_sync_enabled_flag;
    pPicParam->PicShortFormatFlags.fields.uniform_spacing_flag                          = 0 ;
    pPicParam->PicShortFormatFlags.fields.loop_filter_across_tiles_enabled_flag         = pPicParamSet->loop_filter_across_tiles_enabled_flag;
    pPicParam->PicShortFormatFlags.fields.pps_loop_filter_across_slices_enabled_flag    = pPicParamSet->pps_loop_filter_across_slices_enabled_flag;
    pPicParam->PicShortFormatFlags.fields.deblocking_filter_override_enabled_flag       = pPicParamSet->deblocking_filter_override_enabled_flag ? 1 : 0 ;
    pPicParam->PicShortFormatFlags.fields.pps_deblocking_filter_disabled_flag           = pPicParamSet->pps_deblocking_filter_disabled_flag ? 1 : 0 ;
    pPicParam->PicShortFormatFlags.fields.lists_modification_present_flag               = pPicParamSet->lists_modification_present_flag ? 1 : 0 ;
    pPicParam->PicShortFormatFlags.fields.slice_segment_header_extension_present_flag   = pPicParamSet->slice_segment_header_extension_present_flag ? 1 : 0 ;
    pPicParam->PicShortFormatFlags.fields.IrapPicFlag                                   = (sliceHeader->nal_unit_type >= NAL_UT_CODED_SLICE_BLA_W_LP && sliceHeader->nal_unit_type <= NAL_UT_CODED_SLICE_CRA) ? 1 : 0;
    pPicParam->PicShortFormatFlags.fields.IdrPicFlag                                    = sliceHeader->IdrPicFlag;
    pPicParam->PicShortFormatFlags.fields.IntraPicFlag                                  = pSliceInfo->IsIntraAU() ? 1 : 0;

    pPicParam->pps_cb_qp_offset                                     = (CHAR)pPicParamSet->pps_cb_qp_offset;
    pPicParam->pps_cr_qp_offset                                     = (CHAR)pPicParamSet->pps_cr_qp_offset;
    if(pPicParam->PicShortFormatFlags.fields.tiles_enabled_flag)
    {
        pPicParam->num_tile_columns_minus1                          = (UCHAR)pPicParamSet->num_tile_columns - 1;
        pPicParam->num_tile_rows_minus1                             = (UCHAR)pPicParamSet->num_tile_rows - 1;
        for (Ipp32u i = 0; i < pPicParamSet->num_tile_columns; i++)
            pPicParam->column_width_minus1[i] = (Ipp16u)(pPicParamSet->column_width[i] - 1);

        for (Ipp32u i = 0; i < pPicParamSet->num_tile_rows; i++)
            pPicParam->row_height_minus1[i] = (Ipp16u)(pPicParamSet->row_height[i] - 1);
    }
    pPicParam->diff_cu_qp_delta_depth                               = (UCHAR)(pPicParamSet->diff_cu_qp_delta_depth);
    pPicParam->pps_beta_offset_div2                                 = (CHAR)(pPicParamSet->pps_beta_offset >> 1);
    pPicParam->pps_tc_offset_div2                                   = (CHAR)(pPicParamSet->pps_tc_offset >> 1);  
    pPicParam->log2_parallel_merge_level_minus2                     = (UCHAR)pPicParamSet->log2_parallel_merge_level - 2;
    pPicParam->CurrPicOrderCntVal                                   = sliceHeader->slice_pic_order_cnt_lsb;


    //
    //
    pPicParam->RefFieldPicFlag                              = 0;
    pPicParam->RefBottomFieldFlag                           = 0;
    
    pPicParam->StatusReportFeedbackNumber = m_statusReportFeedbackCounter;
}

#endif


#if HEVC_SPEC_VER == MK_HEVCVER(0, 84) || HEVC_SPEC_VER == MK_HEVCVER(0, 83)
void PackerDXVA2::PackSliceParams(H265Slice *pSlice, bool isLong, bool isLastSlice)
{
    size_t headerSize = sizeof(DXVA_Intel_Slice_HEVC_Short);

    if(isLong)
        headerSize = sizeof(DXVA_Intel_Slice_HEVC_Long);

    DXVA_Intel_Slice_HEVC_Long* pDXVASlice = 0;
    void *pSliceData = 0;
    size_t rawDataSize = pSlice->m_BitStream.BytesLeft();

    GetSliceVABuffers(&pDXVASlice, headerSize, &pSliceData, rawDataSize, 64);

    if(isLong)
    {
        const H265PicParamSet *pPicParamSet = pSlice->GetPicParam();
        const H265DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();
        int sliceNum = pSlice->GetSliceNum();

        pDXVASlice->ByteOffsetToSliceData = 0;  // ???
        pDXVASlice->slice_segment_address = pSlice->GetSliceHeader()->slice_segment_address;
        pDXVASlice->NumCTUsInSlice = 0; 

        for(int iDir = 0; iDir < 2; iDir++)
        {
            int index = 0;
            const H265DecoderRefPicList::ReferenceInformation* pRefPicList = pCurrentFrame->GetRefPicList(sliceNum, iDir)->m_refPicList;

            for(int i=0;i < 15;i++)
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

            for(;index < 15;index++)
                pDXVASlice->RefPicList[iDir][index].bPicEntry = (UCHAR)-1;
        }


        // picFlags
        //

        pDXVASlice->LongSliceFlags.fields.LastSliceOfPic                                = isLastSlice;
        pDXVASlice->LongSliceFlags.fields.dependent_slice_segment_flag                  = (UINT)pPicParamSet->dependent_slice_segments_enabled_flag;   // dependent_slices_enabled_flag
        pDXVASlice->LongSliceFlags.fields.slice_type                                    = (UINT)pSlice->GetSliceHeader()->slice_type;
        pDXVASlice->LongSliceFlags.fields.color_plane_id                                = 0; // field is left for future expansion
        pDXVASlice->LongSliceFlags.fields.slice_sao_luma_flag                           = (UINT)pSlice->GetSliceHeader()->slice_sao_luma_flag; 
        pDXVASlice->LongSliceFlags.fields.slice_sao_chroma_flag                         = (UINT)pSlice->GetSliceHeader()->slice_sao_chroma_flag;
        pDXVASlice->LongSliceFlags.fields.mvd_l1_zero_flag                              = (UINT)pSlice->GetSliceHeader()->mvd_l1_zero_flag;
        pDXVASlice->LongSliceFlags.fields.cabac_init_flag                               = (UINT)pSlice->GetSliceHeader()->cabac_init_flag;
        pDXVASlice->LongSliceFlags.fields.slice_temporal_mvp_enabled_flag               = (UINT)pSlice->GetSliceHeader()->slice_temporal_mvp_enabled_flag;
        pDXVASlice->LongSliceFlags.fields.slice_deblocking_filter_disabled_flag         = (UINT)pSlice->GetSliceHeader()->m_deblockingFilterDisable;
        pDXVASlice->LongSliceFlags.fields.collocated_from_l0_flag                       = (UINT)pSlice->GetSliceHeader()->collocated_from_l0_flag;
        pDXVASlice->LongSliceFlags.fields.slice_loop_filter_across_slices_enabled_flag  = (UINT)pPicParamSet->pps_loop_filter_across_slices_enabled_flag;

#if HEVC_SPEC_VER == MK_HEVCVER(0, 84)
        pDXVASlice->collocated_ref_idx              = (UCHAR)(pSlice->GetSliceHeader()->slice_type != I_SLICE ? pSlice->GetSliceHeader()->collocated_ref_idx : -1 );
#else
        pDXVASlice->CollocatedRefIdx.Index7bits     = (UCHAR)(pSlice->GetSliceHeader()->slice_type != I_SLICE ? pSlice->GetSliceHeader()->collocated_ref_idx : -1 );
#endif
        pDXVASlice->num_ref_idx_l0_active_minus1    = (UCHAR)pSlice->getNumRefIdx(REF_PIC_LIST_0) - 1;
        pDXVASlice->num_ref_idx_l1_active_minus1    = (UCHAR)pSlice->getNumRefIdx(REF_PIC_LIST_1) - 1;
        pDXVASlice->slice_qp_delta                  = (CHAR)(pSlice->GetSliceHeader()->SliceQP - (pPicParamSet->init_qp));
        pDXVASlice->slice_cb_qp_offset              = (CHAR)pSlice->GetSliceHeader()->slice_cb_qp_offset;
        pDXVASlice->slice_cr_qp_offset              = (CHAR)pSlice->GetSliceHeader()->slice_cr_qp_offset;
        pDXVASlice->slice_beta_offset_div2          = (CHAR)(pPicParamSet->pps_beta_offset >> 1);
        pDXVASlice->slice_tc_offset_div2            = (CHAR)(pPicParamSet->pps_tc_offset >> 1);
        pDXVASlice->luma_log2_weight_denom          = (UCHAR)pSlice->getLog2WeightDenomLuma();
        pDXVASlice->delta_chroma_log2_weight_denom  = (UCHAR)(pSlice->getLog2WeightDenomChroma() - pSlice->getLog2WeightDenomLuma());

        for(int l=0;l < 2;l++)
        {
            wpScalingParam *wp;
            EnumRefPicList eRefPicList = ( l == 1 ? REF_PIC_LIST_1 : REF_PIC_LIST_0 );
            for (int iRefIdx=0;iRefIdx < pSlice->getNumRefIdx(eRefPicList);iRefIdx++)
            {
                 wp = pSlice->GetSliceHeader()->pred_weight_table[eRefPicList][iRefIdx];

                if(eRefPicList == REF_PIC_LIST_0)
                {
                    pDXVASlice->luma_offset_l0[iRefIdx]       = (CHAR)wp[0].iOffset;
                    pDXVASlice->delta_luma_weight_l0[iRefIdx] = (CHAR)wp[0].iDeltaWeight;
                    for(int chroma=0;chroma < 2;chroma++)
                    {
                        pDXVASlice->delta_chroma_weight_l0[iRefIdx][chroma] = (CHAR)wp[1 + chroma].iDeltaWeight;
                        pDXVASlice->ChromaOffsetL0        [iRefIdx][chroma] = (CHAR)wp[1 + chroma].iOffset;
                    }
                }
                else
                {
                    pDXVASlice->luma_offset_l1[iRefIdx]       = (CHAR)wp[0].iOffset;
                    pDXVASlice->delta_luma_weight_l1[iRefIdx] = (CHAR)wp[0].iDeltaWeight;
                    for(int chroma=0;chroma < 2;chroma++)
                    {
                        pDXVASlice->delta_chroma_weight_l1[iRefIdx][chroma] = (CHAR)wp[1 + chroma].iDeltaWeight;
                        pDXVASlice->ChromaOffsetL1        [iRefIdx][chroma] = (CHAR)wp[1 + chroma].iOffset;
                    }
                }
            }
        }

        pDXVASlice->five_minus_max_num_merge_cand = (UCHAR)(5 - pSlice->GetSliceHeader()->max_num_merge_cand);
        pDXVASlice->num_entry_point_offsets       = pSlice->GetSliceHeader()->num_entry_point_offsets;
    }

    // copy slice data to slice data buffer
    MFX_INTERNAL_CPY(pSliceData, pSlice->m_BitStream.GetRawDataPtr(), rawDataSize);
}

#elif HEVC_SPEC_VER == MK_HEVCVER(0, 85)

void PackerDXVA2::PackSliceParams(H265Slice *pSlice, Ipp32u &, bool isLong, bool isLastSlice)
{
    static Ipp8u start_code_prefix[] = {0, 0, 1};

    DXVA_Intel_Slice_HEVC_Long* pDXVASlice = 0;

    void*   pSliceData = 0;
    Ipp32u  rawDataSize = 0;
    const void*   rawDataPtr = 0;
    size_t  headerSize = sizeof(DXVA_Slice_HEVC_Short);

    if(isLong)
        headerSize = sizeof(DXVA_Intel_Slice_HEVC_Long);

    if(isLong)
    {
        //rawDataSize = pSlice->m_BitStream.BytesLeft();
        //rawDataPtr = pSlice->m_BitStream.GetRawDataPtr();

        pSlice->m_BitStream.GetOrg((Ipp32u**)&rawDataPtr, &rawDataSize);

        GetSliceVABuffers(&pDXVASlice, headerSize, &pSliceData, rawDataSize + sizeof(start_code_prefix), 64);

        const H265PicParamSet *pPicParamSet = pSlice->GetPicParam();
        const H265DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();
        int sliceNum = pSlice->GetSliceNum();

        pDXVASlice->ByteOffsetToSliceData = pSlice->m_BitStream.BytesDecoded() + sizeof(start_code_prefix);  // ???
        pDXVASlice->slice_segment_address = pSlice->GetSliceHeader()->slice_segment_address;

        Ipp32u k = 0;
        for(Ipp8u *ptr = (Ipp8u *)rawDataPtr; ptr < (Ipp8u *)rawDataPtr + pSlice->m_BitStream.BytesDecoded() - 2; ptr++)
        {
            if(ptr[0]==0 && ptr[1]==0 && ptr[2]==3)
            {
                k++;
                //size_t offset = ptr - startPtr + 2;
                //if (offset <= (SliceDataOffset >> 3) + 2)
                {
                    //memmove(startPtr + offset, startPtr + offset + 1, NalUnitSize - offset);
                    //sliceParams->SliceBytesInBuffer--;
                }
            }
        }

        pDXVASlice->ByteOffsetToSliceData += k;

        for(int iDir = 0; iDir < 2; iDir++)
        {
            int index = 0;
            const H265DecoderRefPicList::ReferenceInformation* pRefPicList = pCurrentFrame->GetRefPicList(sliceNum, iDir)->m_refPicList;

            EnumRefPicList eRefPicList = ( iDir == 1 ? REF_PIC_LIST_1 : REF_PIC_LIST_0 );
            for(int i=0;i < pSlice->getNumRefIdx(eRefPicList);i++)
            {
                const H265DecoderRefPicList::ReferenceInformation &frameInfo = pRefPicList[i];
                if(frameInfo.refFrame)
                {
                    bool isFound = false;
                    for(int j=0 ; j < m_refFrameListCacheSize ; j++)
                    {
                        if(m_refFrameListCache[j].Index7bits == (UCHAR)frameInfo.refFrame->m_index)
                        {
                            pDXVASlice->RefPicList[iDir][index].Index7bits = j;
                            index++;
                            isFound = true;
                            break;
                        }
                    }
                    VM_ASSERT(isFound);
                }
                else
                    break;
            }

            for(;index < 15;index++)
                pDXVASlice->RefPicList[iDir][index].bPicEntry = (UCHAR)-1;
        }


        // LongSliceFlags
        //

        pDXVASlice->LongSliceFlags.fields.LastSliceOfPic                                = isLastSlice;
        pDXVASlice->LongSliceFlags.fields.dependent_slice_segment_flag                  = (UINT)pSlice->GetSliceHeader()->dependent_slice_segment_flag;   // dependent_slices_enabled_flag
        pDXVASlice->LongSliceFlags.fields.slice_type                                    = (UINT)pSlice->GetSliceHeader()->slice_type;
        pDXVASlice->LongSliceFlags.fields.color_plane_id                                = 0; // field is left for future expansion
        pDXVASlice->LongSliceFlags.fields.slice_sao_luma_flag                           = (UINT)pSlice->GetSliceHeader()->slice_sao_luma_flag; 
        pDXVASlice->LongSliceFlags.fields.slice_sao_chroma_flag                         = (UINT)pSlice->GetSliceHeader()->slice_sao_chroma_flag;
        pDXVASlice->LongSliceFlags.fields.mvd_l1_zero_flag                              = (UINT)pSlice->GetSliceHeader()->mvd_l1_zero_flag;
        pDXVASlice->LongSliceFlags.fields.cabac_init_flag                               = (UINT)pSlice->GetSliceHeader()->cabac_init_flag;
        pDXVASlice->LongSliceFlags.fields.slice_temporal_mvp_enabled_flag               = (UINT)pSlice->GetSliceHeader()->slice_temporal_mvp_enabled_flag;
        pDXVASlice->LongSliceFlags.fields.slice_deblocking_filter_disabled_flag         = (UINT)pSlice->GetSliceHeader()->slice_deblocking_filter_disabled_flag;
        pDXVASlice->LongSliceFlags.fields.collocated_from_l0_flag                       = (UINT)pSlice->GetSliceHeader()->collocated_from_l0_flag;
        pDXVASlice->LongSliceFlags.fields.slice_loop_filter_across_slices_enabled_flag  = (UINT)pSlice->GetSliceHeader()->slice_loop_filter_across_slices_enabled_flag;

        
        //
        //

        pDXVASlice->collocated_ref_idx              = (UCHAR)(pSlice->GetSliceHeader()->slice_type != I_SLICE ? pSlice->GetSliceHeader()->collocated_ref_idx : -1 );
        pDXVASlice->num_ref_idx_l0_active_minus1    = (UCHAR)pSlice->getNumRefIdx(REF_PIC_LIST_0) - 1;
        pDXVASlice->num_ref_idx_l1_active_minus1    = (UCHAR)pSlice->getNumRefIdx(REF_PIC_LIST_1) - 1;
        //pDXVASlice->slice_qp_delta                  = (CHAR)(pSlice->getSliceQp() - (pPicParamSet->init_qp));
        pDXVASlice->slice_qp_delta                  = (CHAR)(pSlice->GetSliceHeader()->SliceQP - (pPicParamSet->init_qp));
        //pDXVASlice->slice_cb_qp_offset              = (CHAR)pSlice->getSliceQpDeltaCb();
        pDXVASlice->slice_cb_qp_offset              = (CHAR)pSlice->GetSliceHeader()->slice_cb_qp_offset;
        //pDXVASlice->slice_cr_qp_offset              = (CHAR)pSlice->getSliceQpDeltaCr();
        pDXVASlice->slice_cr_qp_offset              = (CHAR)pSlice->GetSliceHeader()->slice_cr_qp_offset;
        pDXVASlice->slice_beta_offset_div2          = (CHAR)(pSlice->GetSliceHeader()->slice_beta_offset >> 1);
        pDXVASlice->slice_tc_offset_div2            = (CHAR)(pSlice->GetSliceHeader()->slice_tc_offset >> 1);
        pDXVASlice->luma_log2_weight_denom          = (UCHAR)pSlice->GetSliceHeader()->luma_log2_weight_denom;
        pDXVASlice->delta_chroma_log2_weight_denom  = (UCHAR)(pSlice->GetSliceHeader()->chroma_log2_weight_denom - pSlice->GetSliceHeader()->luma_log2_weight_denom);

        for(int l=0;l < 2;l++)
        {
            wpScalingParam *wp;
            EnumRefPicList eRefPicList = ( l == 1 ? REF_PIC_LIST_1 : REF_PIC_LIST_0 );
            for (int iRefIdx=0;iRefIdx < pSlice->getNumRefIdx(eRefPicList);iRefIdx++)
            {
                wp = pSlice->GetSliceHeader()->pred_weight_table[eRefPicList][iRefIdx];

                if(eRefPicList == REF_PIC_LIST_0)
                {
                    pDXVASlice->luma_offset_l0[iRefIdx]       = (CHAR)wp[0].offset;
                    pDXVASlice->delta_luma_weight_l0[iRefIdx] = (CHAR)wp[0].delta_weight;
                    for(int chroma=0;chroma < 2;chroma++)
                    {
                        pDXVASlice->delta_chroma_weight_l0[iRefIdx][chroma] = (CHAR)wp[1 + chroma].delta_weight;
                        pDXVASlice->ChromaOffsetL0        [iRefIdx][chroma] = (CHAR)wp[1 + chroma].offset;
                    }
                }
                else
                {
                    pDXVASlice->luma_offset_l1[iRefIdx]       = (CHAR)wp[0].offset;
                    pDXVASlice->delta_luma_weight_l1[iRefIdx] = (CHAR)wp[0].delta_weight;
                    for(int chroma=0;chroma < 2;chroma++)
                    {
                        pDXVASlice->delta_chroma_weight_l1[iRefIdx][chroma] = (CHAR)wp[1 + chroma].delta_weight;
                        pDXVASlice->ChromaOffsetL1        [iRefIdx][chroma] = (CHAR)wp[1 + chroma].offset;
                    }
                }
            }
        }

        pDXVASlice->five_minus_max_num_merge_cand = (UCHAR)(5 - pSlice->GetSliceHeader()->max_num_merge_cand);
    }
    else
    {
        H265Bitstream *pBitstream = pSlice->GetBitStream();

        pBitstream->GetOrg((Ipp32u**)&rawDataPtr, &rawDataSize);
        GetSliceVABuffers(&pDXVASlice, headerSize, &pSliceData, rawDataSize + 3, 64);
    }

    // copy slice data to slice data buffer
    MFX_INTERNAL_CPY(pSliceData, start_code_prefix, sizeof(start_code_prefix));
    MFX_INTERNAL_CPY((Ipp8u*)pSliceData + sizeof(start_code_prefix), rawDataPtr, rawDataSize);
}

#endif


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
            ::MFX_INTERNAL_CPY(sl.getScalingListAddress(sizeId, listId),
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
    DXVA_Intel_Qmatrix_HEVC* pQmatrix = 0;
    GetIQMVABuffer(&pQmatrix, sizeof(DXVA_Intel_Qmatrix_HEVC));
    const H265ScalingList *scalingList = 0;

#if HEVC_SPEC_VER == MK_HEVCVER(0, 85)

    if (pSlice->GetPicParam()->pps_scaling_list_data_present_flag)
    {
        scalingList = pSlice->GetPicParam()->getScalingList();
    }
    else if (pSlice->GetSeqParam()->sps_scaling_list_data_present_flag)
    {
        scalingList = pSlice->GetSeqParam()->getScalingList();
    }
    else
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
                    ::MFX_INTERNAL_CPY(dst, src, sizeof(Ipp32s) * count);
                    sl.setScalingListDC(sizeId, listId, SCALING_LIST_DC);
                }
            }
            doInit = false;
        }

        scalingList = &sl;
    }

    initQMatrix<16>(scalingList, SCALING_LIST_4x4,   pQmatrix->ucScalingLists0);    // 4x4
    initQMatrix<64>(scalingList, SCALING_LIST_8x8,   pQmatrix->ucScalingLists1);    // 8x8
    initQMatrix<64>(scalingList, SCALING_LIST_16x16, pQmatrix->ucScalingLists2);    // 16x16
    initQMatrix(scalingList, SCALING_LIST_32x32, pQmatrix->ucScalingLists3);    // 32x32

    for(int sizeId = SCALING_LIST_16x16; sizeId <= SCALING_LIST_32x32; sizeId++)
    {
        for(unsigned listId = 0; listId <  g_scalingListNum[sizeId]; listId++)
        {
            if(sizeId == SCALING_LIST_16x16)
                pQmatrix->ucScalingListDCCoefSizeID2[listId] = (UCHAR)scalingList->getScalingListDC(sizeId, listId);
            else if(sizeId == SCALING_LIST_32x32)
                pQmatrix->ucScalingListDCCoefSizeID3[listId] = (UCHAR)scalingList->getScalingListDC(sizeId, listId);
        }
    }

#else

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
                    ::MFX_INTERNAL_CPY(dst, src, sizeof(Ipp32s) * count);
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

    // DDI 0.81
    for(int k=SCALING_LIST_16x16;k <= SCALING_LIST_32x32;k++)
    {
        for(int m=0;m < 3;m++)  // Y, Cb, Cr
        {
            for(int n=0;n < 2;n++)  // Intra, Inter
            {
                pQmatrix->bScalingListDC[k][m][n] = (UCHAR)scalingList->getScalingListDC(k, k == SCALING_LIST_16x16 ? 3*n + m : n );
            }
        }
    }

#endif
}


MSPackerDXVA2::MSPackerDXVA2(UMC::VideoAccelerator * va)
    : PackerDXVA2(va)
{
}

void MSPackerDXVA2::PackQmatrix(const H265Slice *pSlice)
{
    UMCVACompBuffer *compBuf;
    DXVA_Qmatrix_HEVC* pQmatrix = (DXVA_Qmatrix_HEVC*)m_va->GetCompBuffer(DXVA_INVERSE_QUANTIZATION_MATRIX_BUFFER, &compBuf);;
    compBuf->SetDataSize(sizeof(DXVA_Qmatrix_HEVC));

    const H265ScalingList *scalingList = 0;

    if (pSlice->GetPicParam()->pps_scaling_list_data_present_flag)
    {
        scalingList = pSlice->GetPicParam()->getScalingList();
    }
    else if (pSlice->GetSeqParam()->sps_scaling_list_data_present_flag)
    {
        scalingList = pSlice->GetSeqParam()->getScalingList();
    }
    else
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
                    MFX_INTERNAL_CPY(dst, src, sizeof(Ipp32s) * count);
                    sl.setScalingListDC(sizeId, listId, SCALING_LIST_DC);
                }
            }
            doInit = false;
        }

        scalingList = &sl;
    }

    initQMatrix<16>(scalingList, SCALING_LIST_4x4,   pQmatrix->ucScalingLists0);    // 4x4
    initQMatrix<64>(scalingList, SCALING_LIST_8x8,   pQmatrix->ucScalingLists1);    // 8x8
    initQMatrix<64>(scalingList, SCALING_LIST_16x16, pQmatrix->ucScalingLists2);    // 16x16
    initQMatrix(scalingList, SCALING_LIST_32x32, pQmatrix->ucScalingLists3);    // 32x32

    for(Ipp32u sizeId = SCALING_LIST_16x16; sizeId <= SCALING_LIST_32x32; sizeId++)
    {
        for(Ipp32u listId = 0; listId <  g_scalingListNum[sizeId]; listId++)
        {
            if(sizeId == SCALING_LIST_16x16)
                pQmatrix->ucScalingListDCCoefSizeID2[listId] = (UCHAR)scalingList->getScalingListDC(sizeId, listId);
            else if(sizeId == SCALING_LIST_32x32)
                pQmatrix->ucScalingListDCCoefSizeID3[listId] = (UCHAR)scalingList->getScalingListDC(sizeId, listId);
        }
    }
}

void MSPackerDXVA2::PackPicParams(const H265DecoderFrame *pCurrentFrame,
                    H265DecoderFrameInfo * sliceInfo,
                    TaskSupplier_H265 * supplier)
{
    UMCVACompBuffer *compBuf;
    DXVA_PicParams_HEVC* pPicParam = (DXVA_PicParams_HEVC*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &compBuf);
    memset(pPicParam, 0, sizeof(DXVA_PicParams_HEVC));

    compBuf->SetDataSize(sizeof(DXVA_PicParams_HEVC));

    H265Slice *pSlice = sliceInfo->GetSlice(0);
    H265SliceHeader *sliceHeader = pSlice->GetSliceHeader();
    const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
    const H265PicParamSet *pPicParamSet = pSlice->GetPicParam();
    
    //
    //
    pPicParam->PicWidthInMinCbsY   = (USHORT)LengthInMinCb(pSeqParamSet->pic_width_in_luma_samples, pSeqParamSet->log2_min_luma_coding_block_size);
    pPicParam->PicHeightInMinCbsY  = (USHORT)LengthInMinCb(pSeqParamSet->pic_height_in_luma_samples, pSeqParamSet->log2_min_luma_coding_block_size);

    //
    //
    pPicParam->chroma_format_idc                            = pSeqParamSet->chroma_format_idc;
    pPicParam->separate_colour_plane_flag                   = 0;    // 0 in HEVC spec HM10 by design
    pPicParam->bit_depth_luma_minus8                        = (UCHAR)(pSeqParamSet->bit_depth_luma - 8);
    pPicParam->bit_depth_chroma_minus8                      = (UCHAR)(pSeqParamSet->bit_depth_chroma - 8);
    pPicParam->log2_max_pic_order_cnt_lsb_minus4            = (UCHAR)(pSeqParamSet->log2_max_pic_order_cnt_lsb - 4);
    pPicParam->NoPicReorderingFlag                          = pSeqParamSet->sps_max_num_reorder_pics == 0 ? 1 : 0;
    pPicParam->NoBiPredFlag                                 = 0;
    
    //
    //
    pPicParam->CurrPic.Index7Bits   = pCurrentFrame->m_index;    // ?

    pPicParam->sps_max_dec_pic_buffering_minus1             = (UCHAR)(pSeqParamSet->sps_max_dec_pic_buffering[sliceHeader->nuh_temporal_id] - 1);
    pPicParam->log2_min_luma_coding_block_size_minus3       = (UCHAR)(pSeqParamSet->log2_min_luma_coding_block_size- 3);
    pPicParam->log2_diff_max_min_transform_block_size       = (UCHAR)(pSeqParamSet->log2_max_transform_block_size - pSeqParamSet->log2_min_transform_block_size);
    pPicParam->log2_min_transform_block_size_minus2         = (UCHAR)(pSeqParamSet->log2_min_transform_block_size - 2);
    pPicParam->log2_diff_max_min_luma_coding_block_size     = (UCHAR)(pSeqParamSet->log2_max_luma_coding_block_size - pSeqParamSet->log2_min_luma_coding_block_size);
    pPicParam->max_transform_hierarchy_depth_intra          = (UCHAR)pSeqParamSet->max_transform_hierarchy_depth_intra;
    pPicParam->max_transform_hierarchy_depth_inter          = (UCHAR)pSeqParamSet->max_transform_hierarchy_depth_inter;
    pPicParam->num_short_term_ref_pic_sets                  = (UCHAR)pSeqParamSet->getRPSList()->getNumberOfReferencePictureSets();
    pPicParam->num_long_term_ref_pics_sps                   = (UCHAR)pSeqParamSet->num_long_term_ref_pics_sps;
    pPicParam->num_ref_idx_l0_default_active_minus1         = (UCHAR)(pPicParamSet->num_ref_idx_l0_default_active - 1);
    pPicParam->num_ref_idx_l1_default_active_minus1         = (UCHAR)(pPicParamSet->num_ref_idx_l1_default_active - 1);
    pPicParam->init_qp_minus26                              = (CHAR)pPicParamSet->init_qp - 26;

    if (!sliceHeader->short_term_ref_pic_set_sps_flag)
    {
        pPicParam->ucNumDeltaPocsOfRefRpsIdx                    = (UCHAR)pSeqParamSet->getRPSList()->getNumberOfReferencePictureSets();
        pPicParam->wNumBitsForShortTermRPSInSlice               = (USHORT)sliceHeader->wNumBitsForShortTermRPSInSlice;
    }
    
    // dwCodingParamToolFlags
    //
    pPicParam->scaling_list_enabled_flag                      = pSeqParamSet->scaling_list_enabled_flag ? 1 : 0 ;    
    pPicParam->amp_enabled_flag                               = pSeqParamSet->amp_enabled_flag ? 1 : 0 ;
    pPicParam->sample_adaptive_offset_enabled_flag            = pSeqParamSet->sample_adaptive_offset_enabled_flag ? 1 : 0 ;
    pPicParam->pcm_enabled_flag                               = pSeqParamSet->pcm_enabled_flag ? 1 : 0 ;
    pPicParam->pcm_sample_bit_depth_luma_minus1               = (UCHAR)(pSeqParamSet->pcm_sample_bit_depth_luma - 1);
    pPicParam->pcm_sample_bit_depth_chroma_minus1             = (UCHAR)(pSeqParamSet->pcm_sample_bit_depth_chroma - 1);
    pPicParam->log2_min_pcm_luma_coding_block_size_minus3     = (UCHAR)(pSeqParamSet->log2_min_pcm_luma_coding_block_size - 3);
    pPicParam->log2_diff_max_min_pcm_luma_coding_block_size   = (UCHAR)(pSeqParamSet->log2_max_pcm_luma_coding_block_size - pSeqParamSet->log2_min_pcm_luma_coding_block_size);
    pPicParam->pcm_loop_filter_disabled_flag                  = pSeqParamSet->pcm_loop_filter_disabled_flag ? 1 : 0 ;
    pPicParam->long_term_ref_pics_present_flag                = pSeqParamSet->long_term_ref_pics_present_flag ? 1 : 0 ;
    pPicParam->sps_temporal_mvp_enabled_flag                  = pSeqParamSet->sps_temporal_mvp_enabled_flag ? 1 : 0 ;
    pPicParam->strong_intra_smoothing_enabled_flag            = pSeqParamSet->sps_strong_intra_smoothing_enabled_flag ? 1 : 0 ;
    pPicParam->dependent_slice_segments_enabled_flag          = pPicParamSet->dependent_slice_segments_enabled_flag ? 1 : 0 ;
    pPicParam->output_flag_present_flag                       = pPicParamSet->output_flag_present_flag ? 1 : 0 ;
    pPicParam->num_extra_slice_header_bits                    = (UCHAR)pPicParamSet->num_extra_slice_header_bits;
    pPicParam->sign_data_hiding_enabled_flag                  = pPicParamSet->sign_data_hiding_enabled_flag ? 1 : 0 ;
    pPicParam->cabac_init_present_flag                        = pPicParamSet->cabac_init_present_flag ? 1 : 0 ;

    pPicParam->constrained_intra_pred_flag                   = pPicParamSet->constrained_intra_pred_flag ? 1 : 0 ;
    pPicParam->transform_skip_enabled_flag                   = pPicParamSet->transform_skip_enabled_flag ? 1 : 0 ;
    pPicParam->cu_qp_delta_enabled_flag                      = pPicParamSet->cu_qp_delta_enabled_flag;
    pPicParam->pps_slice_chroma_qp_offsets_present_flag      = pPicParamSet->pps_slice_chroma_qp_offsets_present_flag ? 1 : 0 ;
    pPicParam->weighted_pred_flag                            = pPicParamSet->weighted_pred_flag ? 1 : 0 ;
    pPicParam->weighted_bipred_flag                          = pPicParamSet->weighted_bipred_flag ? 1 : 0 ;
    pPicParam->transquant_bypass_enabled_flag                = pPicParamSet->transquant_bypass_enabled_flag ? 1 : 0 ;    
    pPicParam->tiles_enabled_flag                            = pPicParamSet->tiles_enabled_flag;
    pPicParam->entropy_coding_sync_enabled_flag              = pPicParamSet->entropy_coding_sync_enabled_flag;
    pPicParam->uniform_spacing_flag                          = 0 ;
    pPicParam->loop_filter_across_tiles_enabled_flag         = pPicParamSet->loop_filter_across_tiles_enabled_flag;
    pPicParam->pps_loop_filter_across_slices_enabled_flag    = pPicParamSet->pps_loop_filter_across_slices_enabled_flag;
    pPicParam->deblocking_filter_override_enabled_flag       = pPicParamSet->deblocking_filter_override_enabled_flag ? 1 : 0 ;
    pPicParam->pps_deblocking_filter_disabled_flag          = pPicParamSet->pps_deblocking_filter_disabled_flag ? 1 : 0 ;
    pPicParam->lists_modification_present_flag              = pPicParamSet->lists_modification_present_flag ? 1 : 0 ;
    pPicParam->slice_segment_header_extension_present_flag  = pPicParamSet->slice_segment_header_extension_present_flag;
    pPicParam->IrapPicFlag                                  = (sliceHeader->nal_unit_type >= NAL_UT_CODED_SLICE_BLA_W_LP && sliceHeader->nal_unit_type <= NAL_UT_CODED_SLICE_CRA) ? 1 : 0;
    pPicParam->IdrPicFlag                                   = sliceHeader->IdrPicFlag;
    pPicParam->IntraPicFlag                                 = sliceInfo->IsIntraAU() ? 1 : 0;
    
    pPicParam->pps_cb_qp_offset                                     = (CHAR)pPicParamSet->pps_cb_qp_offset;
    pPicParam->pps_cr_qp_offset                                     = (CHAR)pPicParamSet->pps_cr_qp_offset;
    if(pPicParam->tiles_enabled_flag)
    {
        pPicParam->num_tile_columns_minus1                          = (UCHAR)pPicParamSet->num_tile_columns - 1;
        pPicParam->num_tile_rows_minus1                             = (UCHAR)pPicParamSet->num_tile_rows - 1;

        //if (!pPicParamSet->uniform_spacing_flag)
        {
            for (Ipp32u i = 0; i < pPicParamSet->num_tile_columns; i++)
                pPicParam->column_width_minus1[i] = (Ipp16u)(pPicParamSet->column_width[i] - 1);

            for (Ipp32u i = 0; i < pPicParamSet->num_tile_rows; i++)
                pPicParam->row_height_minus1[i] = (Ipp16u)(pPicParamSet->row_height[i] - 1);
        }
    }

    pPicParam->diff_cu_qp_delta_depth                               = (UCHAR)(pPicParamSet->diff_cu_qp_delta_depth);
    pPicParam->pps_beta_offset_div2                                 = (CHAR)(pPicParamSet->pps_beta_offset >> 1);
    pPicParam->pps_tc_offset_div2                                   = (CHAR)(pPicParamSet->pps_tc_offset >> 1);  
    pPicParam->log2_parallel_merge_level_minus2                     = (UCHAR)pPicParamSet->log2_parallel_merge_level - 2;
    pPicParam->CurrPicOrderCntVal                                   = sliceHeader->slice_pic_order_cnt_lsb;  
    

    Ipp32s count = 0;
    H265DBPList *dpb = supplier->GetDPBList();
    ReferencePictureSet *rps = pSlice->getRPS();
    for(H265DecoderFrame* frame = dpb->head() ; frame && count < sizeof(pPicParam->RefPicList)/sizeof(pPicParam->RefPicList[0]) ; frame = frame->future())
    {
        if(frame != pCurrentFrame)
        {
            int refType = frame->isShortTermRef() ? SHORT_TERM_REFERENCE : (frame->isLongTermRef() ? LONG_TERM_REFERENCE : NO_REFERENCE);

            if(refType != NO_REFERENCE)
            {
                pPicParam->PicOrderCntValList[count] = frame->m_PicOrderCnt;

                pPicParam->RefPicList[count].Index7Bits = frame->m_index;
                pPicParam->RefPicList[count].AssociatedFlag = (refType == LONG_TERM_REFERENCE);

                m_refFrameListCache[count].bPicEntry = pPicParam->RefPicList[count].bPicEntry;    // save to use in slice param packer
                count++;
            }
        }
    }

    Ipp32s index, pocList[3*8];
    Ipp32s numRefPicSetStCurrBefore = 0,
        numRefPicSetStCurrAfter  = 0,
        numRefPicSetLtCurr       = 0;
    for(index = 0; index < rps->getNumberOfNegativePictures(); index++)
            pocList[numRefPicSetStCurrBefore++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);
    for(; index < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures(); index++)
            pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);
    for(; index < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures() + rps->getNumberOfLongtermPictures(); index++)
        {
            Ipp32s poc = rps->getPOC(index);
            H265DecoderFrame *pFrm = supplier->GetDPBList()->findLongTermRefPic(pCurrentFrame, poc, pSeqParamSet->log2_max_pic_order_cnt_lsb, !rps->getCheckLTMSBPresent(index));

            if (pFrm)
            {
                pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr++] = pFrm->PicOrderCnt();
            }
            else
            {
                pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);
            }
        }

    Ipp32s cntRefPicSetStCurrBefore = 0,
        cntRefPicSetStCurrAfter  = 0,
        cntRefPicSetLtCurr = 0;

    for(Ipp32s n=0 ; n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr ; n++)
    {
        if (!rps->getUsed(n))
            continue;

        for(Ipp32s k=0;k < count;k++)
        {
            if(pocList[n] == pPicParam->PicOrderCntValList[k])
            {
                if(n < numRefPicSetStCurrBefore)
                    pPicParam->RefPicSetStCurrBefore[cntRefPicSetStCurrBefore++] = (UCHAR)k;
                else if(n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter)
                    pPicParam->RefPicSetStCurrAfter[cntRefPicSetStCurrAfter++] = (UCHAR)k;
                else if(n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr)
                    pPicParam->RefPicSetLtCurr[cntRefPicSetLtCurr++] = (UCHAR)k;
            }
        }
    }

    m_refFrameListCacheSize = count;
    for(int n=count;n < sizeof(pPicParam->RefPicList)/sizeof(pPicParam->RefPicList[0]);n++)
    {
        pPicParam->RefPicList[n].bPicEntry = (UCHAR)0xff;
        pPicParam->PicOrderCntValList[n] = -1;
    }

    for(Ipp32s i = 0;i < 8; i++)
    {
        if (i >= cntRefPicSetStCurrBefore)
            pPicParam->RefPicSetStCurrBefore[i] = 0xff;
        if (i >= cntRefPicSetStCurrAfter)
            pPicParam->RefPicSetStCurrAfter[i] = 0xff;
        if (i >= cntRefPicSetLtCurr)
            pPicParam->RefPicSetLtCurr[i] = 0xff;
    }
    
    pPicParam->StatusReportFeedbackNumber = m_statusReportFeedbackCounter;
}

void MSPackerDXVA2::PackSliceParams(H265Slice *pSlice, Ipp32u &sliceNum, bool , bool )
{
    static Ipp8u start_code_prefix[] = {0, 0, 1};

    Ipp32u  rawDataSize = 0;
    const void*  rawDataPtr = 0;

    H265Bitstream *pBitstream = pSlice->GetBitStream();
    pBitstream->GetOrg((Ipp32u**)&rawDataPtr, &rawDataSize);

    UMCVACompBuffer *headVABffr = 0;
    UMCVACompBuffer *dataVABffr = 0;
    DXVA_Slice_HEVC_Short* dxvaSlice = (DXVA_Slice_HEVC_Short*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &headVABffr);
    dxvaSlice += sliceNum;
    Ipp8u *dataBffr = (Ipp8u *)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &dataVABffr);

    dxvaSlice->BSNALunitDataLocation = dataVABffr->GetDataSize();

    Ipp32s storedSize = rawDataSize + sizeof(start_code_prefix);
    dxvaSlice->SliceBytesInBuffer = storedSize;
    dxvaSlice->wBadSliceChopping = 0;

    if (storedSize >= dataVABffr->GetBufferSize() - dataVABffr->GetDataSize())
        return;

    dataBffr += dataVABffr->GetDataSize();
    memcpy(dataBffr, start_code_prefix, sizeof(start_code_prefix));
    memcpy(dataBffr + sizeof(start_code_prefix), rawDataPtr, rawDataSize);

    size_t alignedSize = dataVABffr->GetDataSize() + storedSize;
    alignedSize = align_value<size_t>(alignedSize, 128);

    memset(dataBffr + storedSize, 0, alignedSize - storedSize);
    dataVABffr->SetDataSize(alignedSize);
    headVABffr->SetDataSize(headVABffr->GetDataSize() + sizeof(DXVA_Slice_HEVC_Short));
}

#endif // UMC_VA_DXVA

} // namespace UMC_HEVC_DECODER

#endif // UMC_RESTRICTED_CODE_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER
