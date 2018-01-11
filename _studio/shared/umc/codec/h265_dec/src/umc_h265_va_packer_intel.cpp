//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef UMC_RESTRICTED_CODE_VA

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA

#include "umc_hevc_ddi.h"
#include "umc_h265_va_packer_dxva.h"

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    #if DDI_VERSION < 943
        #error "Gen11 should be compiled with DDI_VERSION >= 0.943"
    #endif
#endif

#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
    #if DDI_VERSION < 945
        #error "Gen12 should be compiled with DDI_VERSION >= 0.945"
    #endif
#endif

#include <numeric>

using namespace UMC;

namespace UMC_HEVC_DECODER
{
    class PackerDXVA2intel
        : public PackerDXVA2
    {

    public:

        PackerDXVA2intel(UMC::VideoAccelerator * va)
            : PackerDXVA2(va)
        {}

    private:

        void PackQmatrix(const H265Slice *pSlice);
        void PackPicParams(const H265DecoderFrame *pCurrentFrame, H265DecoderFrameInfo * pSliceInfo, TaskSupplier_H265 *supplier);
        bool PackSliceParams(H265Slice *pSlice, Ipp32u &, bool isLastSlice);
        void PackSubsets(const H265DecoderFrame *pCurrentFrame);
    };

    Packer * CreatePackerIntel(UMC::VideoAccelerator* va)
    { return new PackerDXVA2intel(va); }

    template <typename T>
    void GetPicParamsBuffer(UMC::VideoAccelerator* va, T** ppPicParams, Ipp32s size)
    {
        VM_ASSERT(va);
        VM_ASSERT(ppPicParams);

        UMCVACompBuffer *compBuf;
        *ppPicParams =
            reinterpret_cast<T*>(va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &compBuf));

        if (!*ppPicParams)
            throw h265_exception(UMC_ERR_FAILED);

        if (compBuf->GetBufferSize() < size)
            throw h265_exception(UMC_ERR_FAILED);

        compBuf->SetDataSize(size);
        memset(*ppPicParams, 0, size);
    }

    template <typename T>
    void GetSliceVABuffers(UMC::VideoAccelerator* va, T** ppSliceHeader, Ipp32s headerSize, void **ppSliceData, Ipp32s dataSize, Ipp32s dataAlignment)
    {
        VM_ASSERT(va);
        VM_ASSERT(ppSliceHeader);
        VM_ASSERT(ppSliceData);

        for (int phase = 0; phase < 2; phase++)
        {
            UMCVACompBuffer *headVABffr = 0;
            UMCVACompBuffer *dataVABffr = 0;
            void *headBffr = va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &headVABffr);
            void *dataBffr = va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &dataVABffr);

            Ipp32s const headBffrSize = headVABffr->GetBufferSize();
            Ipp32s const headBffrOffs = headVABffr->GetDataSize();

            Ipp32s const dataBffrSize = dataVABffr->GetBufferSize();
            Ipp32s       dataBffrOffs = dataVABffr->GetDataSize();

            dataBffrOffs = dataAlignment ?
                dataAlignment * ((dataBffrOffs + dataAlignment - 1) / dataAlignment) : dataBffrOffs;

            if (headBffrSize - headBffrOffs < headerSize ||
                dataBffrSize - dataBffrOffs < dataSize)
            {
                if (phase == 0)
                {
                    Status s = va->Execute();
                    if (s != UMC_OK)
                        throw h265_exception(s);
                    continue;
                }

                VM_ASSERT(false);
                return;
            }

            *ppSliceHeader = (T*)((char *)headBffr + headBffrOffs);
            *ppSliceData = (char *)dataBffr + dataBffrOffs;

            headVABffr->SetDataSize(headBffrOffs + headerSize);
            dataVABffr->SetDataSize(dataBffrOffs + dataSize);

            memset(*ppSliceHeader, 0, headerSize);
            (*ppSliceHeader)->BSNALunitDataLocation = dataBffrOffs;
            (*ppSliceHeader)->SliceBytesInBuffer = (UINT)dataSize;

            return;
        }
    }

    template <typename T>
    inline
    void PackPicHeaderCommon(H265DecoderFrame const* pCurrentFrame, H265DecoderFrameInfo const* pSliceInfo, H265DBPList const* dpb, T* pPicParam)
    {
        VM_ASSERT(pCurrentFrame);
        VM_ASSERT(pSliceInfo);
        VM_ASSERT(dpb);
        VM_ASSERT(pPicParam);

        H265Slice *pSlice = pSliceInfo->GetSlice(0);
        if (!pSlice)
            return;

        const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
        VM_ASSERT(pSeqParamSet);
        const H265PicParamSet *pPicParamSet = pSlice->GetPicParam();
        VM_ASSERT(pPicParamSet);

        H265SliceHeader const* sliceHeader = pSlice->GetSliceHeader();
        VM_ASSERT(sliceHeader);

        pPicParam->PicWidthInMinCbsY = (USHORT)LengthInMinCb(pSeqParamSet->pic_width_in_luma_samples, pSeqParamSet->log2_min_luma_coding_block_size);
        pPicParam->PicHeightInMinCbsY = (USHORT)LengthInMinCb(pSeqParamSet->pic_height_in_luma_samples, pSeqParamSet->log2_min_luma_coding_block_size);

        //
        //
        pPicParam->PicFlags.fields.chroma_format_idc = pSeqParamSet->chroma_format_idc;
        pPicParam->PicFlags.fields.separate_colour_plane_flag = pSeqParamSet->separate_colour_plane_flag;
        pPicParam->PicFlags.fields.bit_depth_luma_minus8 = (UCHAR)(pSeqParamSet->bit_depth_luma - 8);
        pPicParam->PicFlags.fields.bit_depth_chroma_minus8 = (UCHAR)(pSeqParamSet->bit_depth_chroma - 8);
        pPicParam->PicFlags.fields.log2_max_pic_order_cnt_lsb_minus4 = (UCHAR)(pSeqParamSet->log2_max_pic_order_cnt_lsb - 4);
        pPicParam->PicFlags.fields.NoPicReorderingFlag = pSeqParamSet->sps_max_num_reorder_pics == 0 ? 1 : 0;
        pPicParam->PicFlags.fields.NoBiPredFlag = sliceHeader->slice_type == B_SLICE ? 0 : 1;

        //
        //
        pPicParam->CurrPic.Index7bits = pCurrentFrame->m_index;
        pPicParam->CurrPicOrderCntVal = sliceHeader->slice_pic_order_cnt_lsb;

        int count = 0;
        int cntRefPicSetStCurrBefore = 0,
            cntRefPicSetStCurrAfter = 0,
            cntRefPicSetLtCurr = 0;
        ReferencePictureSet *rps = pSlice->getRPS();
        VM_ASSERT(rps);

        for (H265DecoderFrame const* frame = dpb->head(); frame && count < sizeof(pPicParam->RefFrameList) / sizeof(pPicParam->RefFrameList[0]); frame = frame->future())
        {
            if (frame != pCurrentFrame)
            {
                int refType = (frame->isShortTermRef() ? SHORT_TERM_REFERENCE : (frame->isLongTermRef() ? LONG_TERM_REFERENCE : NO_REFERENCE));

                if (refType != NO_REFERENCE)
                {
                    pPicParam->PicOrderCntValList[count] = frame->m_PicOrderCnt;

                    pPicParam->RefFrameList[count].Index7bits = frame->m_index;
                    pPicParam->RefFrameList[count].long_term_ref_flag = (refType == LONG_TERM_REFERENCE);

                    count++;
                }
            }
        }

        if (pPicParamSet->pps_curr_pic_ref_enabled_flag)
        {
            pPicParam->PicOrderCntValList[count] = pCurrentFrame->m_PicOrderCnt;

            pPicParam->RefFrameList[count].Index7bits = pCurrentFrame->m_index;
            pPicParam->RefFrameList[count].long_term_ref_flag = 1;

            count++;
        }

        Ipp32u index;
        int pocList[3 * 8];
        int numRefPicSetStCurrBefore = 0,
            numRefPicSetStCurrAfter = 0,
            numRefPicSetLtCurr = 0;
        for (index = 0; index < rps->getNumberOfNegativePictures(); index++)
            pocList[numRefPicSetStCurrBefore++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);
        for (; index < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures(); index++)
            pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);
        for (; index < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures() + rps->getNumberOfLongtermPictures(); index++)
        {
            Ipp32s poc = rps->getPOC(index);
            H265DecoderFrame *pFrm = dpb->findLongTermRefPic(pCurrentFrame, poc, pSeqParamSet->log2_max_pic_order_cnt_lsb, !rps->getCheckLTMSBPresent(index));

            if (pFrm)
            {
                pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr++] = pFrm->PicOrderCnt();
            }
            else
            {
                pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);
            }
        }

        for (int n = 0; n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr; n++)
        {
            if (!rps->getUsed(n))
                continue;

            for (int k = 0; k < count; k++)
            {
                if (pocList[n] == pPicParam->PicOrderCntValList[k])
                {
                    if (n < numRefPicSetStCurrBefore)
                        pPicParam->RefPicSetStCurrBefore[cntRefPicSetStCurrBefore++] = (UCHAR)k;
                    else if (n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter)
                        pPicParam->RefPicSetStCurrAfter[cntRefPicSetStCurrAfter++] = (UCHAR)k;
                    else if (n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr)
                        pPicParam->RefPicSetLtCurr[cntRefPicSetLtCurr++] = (UCHAR)k;
                }
            }
        }

        for (int n = count; n < sizeof(pPicParam->RefFrameList) / sizeof(pPicParam->RefFrameList[0]); n++)
        {
            pPicParam->RefFrameList[n].bPicEntry = (UCHAR)0xff;
            pPicParam->PicOrderCntValList[n] = -1;
        }

        for (int i = 0; i < 8; i++)
        {
            if (i >= cntRefPicSetStCurrBefore)
                pPicParam->RefPicSetStCurrBefore[i] = 0xff;
            if (i >= cntRefPicSetStCurrAfter)
                pPicParam->RefPicSetStCurrAfter[i] = 0xff;
            if (i >= cntRefPicSetLtCurr)
                pPicParam->RefPicSetLtCurr[i] = 0xff;
        }

        //
        //
        pPicParam->sps_max_dec_pic_buffering_minus1 = (UCHAR)(pSeqParamSet->sps_max_dec_pic_buffering[sliceHeader->nuh_temporal_id] - 1);
        pPicParam->log2_min_luma_coding_block_size_minus3 = (UCHAR)(pSeqParamSet->log2_min_luma_coding_block_size - 3);
        pPicParam->log2_diff_max_min_transform_block_size = (UCHAR)(pSeqParamSet->log2_max_transform_block_size - pSeqParamSet->log2_min_transform_block_size);
        pPicParam->log2_min_transform_block_size_minus2 = (UCHAR)(pSeqParamSet->log2_min_transform_block_size - 2);
        pPicParam->log2_diff_max_min_luma_coding_block_size = (UCHAR)(pSeqParamSet->log2_max_luma_coding_block_size - pSeqParamSet->log2_min_luma_coding_block_size);
        pPicParam->max_transform_hierarchy_depth_intra = (UCHAR)pSeqParamSet->max_transform_hierarchy_depth_intra;
        pPicParam->max_transform_hierarchy_depth_inter = (UCHAR)pSeqParamSet->max_transform_hierarchy_depth_inter;
        pPicParam->num_short_term_ref_pic_sets = (UCHAR)pSeqParamSet->getRPSList()->getNumberOfReferencePictureSets();
        pPicParam->num_long_term_ref_pics_sps = (UCHAR)pSeqParamSet->num_long_term_ref_pics_sps;
        pPicParam->num_ref_idx_l0_default_active_minus1 = (UCHAR)(pPicParamSet->num_ref_idx_l0_default_active - 1);
        pPicParam->num_ref_idx_l1_default_active_minus1 = (UCHAR)(pPicParamSet->num_ref_idx_l1_default_active - 1);
        pPicParam->init_qp_minus26 = (CHAR)pPicParamSet->init_qp - 26;

        pPicParam->wNumBitsForShortTermRPSInSlice = (USHORT)sliceHeader->wNumBitsForShortTermRPSInSlice;
        pPicParam->ucNumDeltaPocsOfRefRpsIdx = (UCHAR)pPicParam->wNumBitsForShortTermRPSInSlice;

        Ipp32u num_offsets = 0;
        for (Ipp32u i = 0; i < pSliceInfo->GetSliceCount(); ++i)
        {
            H265Slice const* slice = pSliceInfo->GetSlice(i);
            if (!slice)
                throw h265_exception(UMC_ERR_FAILED);

            H265SliceHeader const* sh = slice->GetSliceHeader();
            VM_ASSERT(sh);

            num_offsets += sh->num_entry_point_offsets;
        }

        pPicParam->TotalNumEntryPointOffsets = (USHORT)num_offsets;

        // dwCodingParamToolFlags
        //
        pPicParam->fields.scaling_list_enabled_flag = pSeqParamSet->scaling_list_enabled_flag;
        pPicParam->fields.amp_enabled_flag = pSeqParamSet->amp_enabled_flag;
        pPicParam->fields.sample_adaptive_offset_enabled_flag = pSeqParamSet->sample_adaptive_offset_enabled_flag;
        pPicParam->fields.pcm_enabled_flag = pSeqParamSet->pcm_enabled_flag;
        pPicParam->fields.pcm_sample_bit_depth_luma_minus1 = (UCHAR)(pSeqParamSet->pcm_sample_bit_depth_luma - 1);
        pPicParam->fields.pcm_sample_bit_depth_chroma_minus1 = (UCHAR)(pSeqParamSet->pcm_sample_bit_depth_chroma - 1);
        pPicParam->fields.log2_min_pcm_luma_coding_block_size_minus3 = (UCHAR)(pSeqParamSet->log2_min_pcm_luma_coding_block_size - 3);
        pPicParam->fields.log2_diff_max_min_pcm_luma_coding_block_size = (UCHAR)(pSeqParamSet->log2_max_pcm_luma_coding_block_size - pSeqParamSet->log2_min_pcm_luma_coding_block_size);
        pPicParam->fields.pcm_loop_filter_disabled_flag = pSeqParamSet->pcm_loop_filter_disabled_flag;
        pPicParam->fields.long_term_ref_pics_present_flag = pSeqParamSet->long_term_ref_pics_present_flag;
        pPicParam->fields.sps_temporal_mvp_enabled_flag = pSeqParamSet->sps_temporal_mvp_enabled_flag;
        pPicParam->fields.strong_intra_smoothing_enabled_flag = pSeqParamSet->sps_strong_intra_smoothing_enabled_flag;
        pPicParam->fields.dependent_slice_segments_enabled_flag = pPicParamSet->dependent_slice_segments_enabled_flag;
        pPicParam->fields.output_flag_present_flag = pPicParamSet->output_flag_present_flag;
        pPicParam->fields.num_extra_slice_header_bits = (UCHAR)pPicParamSet->num_extra_slice_header_bits;
        pPicParam->fields.sign_data_hiding_flag = pPicParamSet->sign_data_hiding_enabled_flag;
        pPicParam->fields.cabac_init_present_flag = pPicParamSet->cabac_init_present_flag;

        // PicShortFormatFlags
        //
        pPicParam->PicShortFormatFlags.fields.constrained_intra_pred_flag = pPicParamSet->constrained_intra_pred_flag;
        pPicParam->PicShortFormatFlags.fields.transform_skip_enabled_flag = pPicParamSet->transform_skip_enabled_flag;
        pPicParam->PicShortFormatFlags.fields.cu_qp_delta_enabled_flag = pPicParamSet->cu_qp_delta_enabled_flag;
        pPicParam->PicShortFormatFlags.fields.pps_slice_chroma_qp_offsets_present_flag = pPicParamSet->pps_slice_chroma_qp_offsets_present_flag;
        pPicParam->PicShortFormatFlags.fields.weighted_pred_flag = pPicParamSet->weighted_pred_flag;
        pPicParam->PicShortFormatFlags.fields.weighted_bipred_flag = pPicParamSet->weighted_bipred_flag;
        pPicParam->PicShortFormatFlags.fields.transquant_bypass_enabled_flag = pPicParamSet->transquant_bypass_enabled_flag;
        pPicParam->PicShortFormatFlags.fields.tiles_enabled_flag = pPicParamSet->tiles_enabled_flag;
        pPicParam->PicShortFormatFlags.fields.entropy_coding_sync_enabled_flag = pPicParamSet->entropy_coding_sync_enabled_flag;
        pPicParam->PicShortFormatFlags.fields.uniform_spacing_flag = pPicParamSet->uniform_spacing_flag;
        pPicParam->PicShortFormatFlags.fields.loop_filter_across_tiles_enabled_flag = pPicParamSet->loop_filter_across_tiles_enabled_flag;
        pPicParam->PicShortFormatFlags.fields.pps_loop_filter_across_slices_enabled_flag = pPicParamSet->pps_loop_filter_across_slices_enabled_flag;
        pPicParam->PicShortFormatFlags.fields.deblocking_filter_override_enabled_flag = pPicParamSet->deblocking_filter_override_enabled_flag;
        pPicParam->PicShortFormatFlags.fields.pps_deblocking_filter_disabled_flag = pPicParamSet->pps_deblocking_filter_disabled_flag;
        pPicParam->PicShortFormatFlags.fields.lists_modification_present_flag = pPicParamSet->lists_modification_present_flag;
        pPicParam->PicShortFormatFlags.fields.slice_segment_header_extension_present_flag = pPicParamSet->slice_segment_header_extension_present_flag;
        pPicParam->PicShortFormatFlags.fields.IrapPicFlag = (sliceHeader->nal_unit_type >= NAL_UT_CODED_SLICE_BLA_W_LP && sliceHeader->nal_unit_type <= NAL_UT_CODED_SLICE_CRA) ? 1 : 0;
        pPicParam->PicShortFormatFlags.fields.IdrPicFlag = sliceHeader->IdrPicFlag;
        pPicParam->PicShortFormatFlags.fields.IntraPicFlag = pSliceInfo->IsIntraAU() ? 1 : 0;

        pPicParam->pps_cb_qp_offset = (CHAR)pPicParamSet->pps_cb_qp_offset;
        pPicParam->pps_cr_qp_offset = (CHAR)pPicParamSet->pps_cr_qp_offset;
        if (pPicParam->PicShortFormatFlags.fields.tiles_enabled_flag)
        {
            pPicParam->num_tile_columns_minus1 = (UCHAR)pPicParamSet->num_tile_columns - 1;
            pPicParam->num_tile_rows_minus1 = (UCHAR)pPicParamSet->num_tile_rows - 1;

            pPicParam->num_tile_columns_minus1 = IPP_MIN(sizeof(pPicParam->column_width_minus1) / sizeof(pPicParam->column_width_minus1[0]) - 1, pPicParam->num_tile_columns_minus1);
            pPicParam->num_tile_rows_minus1 = IPP_MIN(sizeof(pPicParam->row_height_minus1) / sizeof(pPicParam->row_height_minus1[0]) - 1, pPicParam->num_tile_rows_minus1);

            for (Ipp32u i = 0; i <= pPicParam->num_tile_columns_minus1; i++)
                pPicParam->column_width_minus1[i] = (Ipp16u)(pPicParamSet->column_width[i] - 1);

            for (Ipp32u i = 0; i <= pPicParam->num_tile_rows_minus1; i++)
                pPicParam->row_height_minus1[i] = (Ipp16u)(pPicParamSet->row_height[i] - 1);
        }

        pPicParam->diff_cu_qp_delta_depth = (UCHAR)(pPicParamSet->diff_cu_qp_delta_depth);
        pPicParam->pps_beta_offset_div2 = (CHAR)(pPicParamSet->pps_beta_offset >> 1);
        pPicParam->pps_tc_offset_div2 = (CHAR)(pPicParamSet->pps_tc_offset >> 1);
        pPicParam->log2_parallel_merge_level_minus2 = (UCHAR)pPicParamSet->log2_parallel_merge_level - 2;
        pPicParam->CurrPicOrderCntVal = sliceHeader->slice_pic_order_cnt_lsb;


        //
        //
        pPicParam->RefFieldPicFlag = 0;
        pPicParam->RefBottomFieldFlag = 0;
    }

    inline
    void PackPicHeader(H265DecoderFrame const* pCurrentFrame, H265DecoderFrameInfo const* pSliceInfo, H265DBPList const* dpb, DXVA_Intel_PicParams_HEVC* pPicParam)
    { PackPicHeaderCommon(pCurrentFrame, pSliceInfo, dpb, pPicParam); }

    inline
    void PackPicHeader(H265DecoderFrame const* pCurrentFrame, H265DecoderFrameInfo const* pSliceInfo, H265DBPList const* dpb, DXVA_Intel_PicParams_HEVC_Rext* pPicParam)
    {
        PackPicHeaderCommon(pCurrentFrame, pSliceInfo, dpb, &pPicParam->PicParamsMain);

        const H265Slice *pSlice = pSliceInfo->GetSlice(0);
        VM_ASSERT(pSlice);

        const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
        VM_ASSERT(pSeqParamSet);
        const H265PicParamSet *pPicParamSet = pSlice->GetPicParam();
        VM_ASSERT(pPicParamSet);

        pPicParam->PicRangeExtensionFlags.fields.transform_skip_rotation_enabled_flag = pSeqParamSet->transform_skip_rotation_enabled_flag;
        pPicParam->PicRangeExtensionFlags.fields.transform_skip_context_enabled_flag = pSeqParamSet->transform_skip_context_enabled_flag;
        pPicParam->PicRangeExtensionFlags.fields.implicit_rdpcm_enabled_flag = pSeqParamSet->implicit_residual_dpcm_enabled_flag;
        pPicParam->PicRangeExtensionFlags.fields.explicit_rdpcm_enabled_flag = pSeqParamSet->explicit_residual_dpcm_enabled_flag;
        pPicParam->PicRangeExtensionFlags.fields.extended_precision_processing_flag = pSeqParamSet->extended_precision_processing_flag;
        pPicParam->PicRangeExtensionFlags.fields.intra_smoothing_disabled_flag = pSeqParamSet->intra_smoothing_disabled_flag;
        pPicParam->PicRangeExtensionFlags.fields.high_precision_offsets_enabled_flag = pSeqParamSet->high_precision_offsets_enabled_flag;
        pPicParam->PicRangeExtensionFlags.fields.persistent_rice_adaptation_enabled_flag = pSeqParamSet->fast_rice_adaptation_enabled_flag;
        pPicParam->PicRangeExtensionFlags.fields.cabac_bypass_alignment_enabled_flag = pSeqParamSet->cabac_bypass_alignment_enabled_flag;
        pPicParam->PicRangeExtensionFlags.fields.cross_component_prediction_enabled_flag = pPicParamSet->cross_component_prediction_enabled_flag;
        pPicParam->PicRangeExtensionFlags.fields.chroma_qp_offset_list_enabled_flag = pPicParamSet->chroma_qp_offset_list_enabled_flag;

        pPicParam->diff_cu_chroma_qp_offset_depth = (UCHAR)pPicParamSet->diff_cu_chroma_qp_offset_depth;
        pPicParam->chroma_qp_offset_list_len_minus1 = pPicParamSet->chroma_qp_offset_list_enabled_flag ? (UCHAR)pPicParamSet->chroma_qp_offset_list_len - 1 : 0;
        pPicParam->log2_sao_offset_scale_luma = (UCHAR)pPicParamSet->log2_sao_offset_scale_luma;
        pPicParam->log2_sao_offset_scale_chroma = (UCHAR)pPicParamSet->log2_sao_offset_scale_chroma;
        pPicParam->log2_max_transform_skip_block_size_minus2 = pPicParamSet->pps_range_extensions_flag && pPicParamSet->transform_skip_enabled_flag ? (UCHAR)pPicParamSet->log2_max_transform_skip_block_size_minus2 : 0;

        for (Ipp32u i = 0; i < pPicParamSet->chroma_qp_offset_list_len; i++)
        {
            pPicParam->cb_qp_offset_list[i] = (CHAR)pPicParamSet->cb_qp_offset_list[i + 1];
            pPicParam->cr_qp_offset_list[i] = (CHAR)pPicParamSet->cr_qp_offset_list[i + 1];
        }
    }

#if DDI_VERSION >= 945
    inline
    void FillPaletteEntries(DXVA_Intel_PicParams_HEVC_SCC* pPicParam, Ipp8u numComps, Ipp32u const* entries, Ipp32u count)
    {
        VM_ASSERT(!(count > 128));

        pPicParam->PredictorPaletteSize = static_cast<UCHAR>(count);
        for (Ipp8u i = 0; i < numComps; ++i, entries += count)
            std::copy(entries, entries + count, pPicParam->PredictorPaletteEntries[i]);
    }

    inline
    void PackPicHeader(H265DecoderFrame const* pCurrentFrame, H265DecoderFrameInfo const* pSliceInfo, H265DBPList const* dpb, DXVA_Intel_PicParams_HEVC_SCC* pPicParam)
    {
        PackPicHeader(pCurrentFrame, pSliceInfo, dpb, &pPicParam->PicParamsRext);

        const H265Slice *pSlice = pSliceInfo->GetSlice(0);
        VM_ASSERT(pSlice);

        const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
        VM_ASSERT(pSeqParamSet);
        const H265PicParamSet *pPicParamSet = pSlice->GetPicParam();
        VM_ASSERT(pPicParamSet);

        pPicParam->PicSCCExtensionFlags.fields.pps_curr_pic_ref_enabled_flag = pPicParamSet->pps_curr_pic_ref_enabled_flag;
        pPicParam->PicSCCExtensionFlags.fields.palette_mode_enabled_flag = pSeqParamSet->palette_mode_enabled_flag;
        pPicParam->PicSCCExtensionFlags.fields.motion_vector_resolution_control_idc = pSeqParamSet->motion_vector_resolution_control_idc;
        pPicParam->PicSCCExtensionFlags.fields.intra_boundary_filtering_disabled_flag = pSeqParamSet->intra_boundary_filtering_disabled_flag;
        pPicParam->PicSCCExtensionFlags.fields.residual_adaptive_colour_transform_enabled_flag = pPicParamSet->residual_adaptive_colour_transform_enabled_flag;
        pPicParam->PicSCCExtensionFlags.fields.pps_slice_act_qp_offsets_present_flag = pPicParamSet->pps_slice_act_qp_offsets_present_flag;

        pPicParam->pps_act_y_qp_offset_plus5  = static_cast<CHAR>(pPicParamSet->pps_act_y_qp_offset  + 5);
        pPicParam->pps_act_cb_qp_offset_plus5 = static_cast<CHAR>(pPicParamSet->pps_act_cb_qp_offset + 5);
        pPicParam->pps_act_cr_qp_offset_plus3 = static_cast<CHAR>(pPicParamSet->pps_act_cr_qp_offset + 3);

        pPicParam->palette_max_size = static_cast<UCHAR>(pSeqParamSet->palette_max_size);
        pPicParam->delta_palette_max_predictor_size = static_cast<UCHAR>(pSeqParamSet->delta_palette_max_predictor_size);

        Ipp8u const numComps = pSeqParamSet->chroma_format_idc ? 3 : 1;
        if (pPicParamSet->pps_palette_predictor_initializer_present_flag)
        {
            VM_ASSERT(!pPicParamSet->m_paletteInitializers.empty());
            VM_ASSERT(pPicParamSet->pps_num_palette_predictor_initializer == pPicParamSet->m_paletteInitializers.size());

            FillPaletteEntries(pPicParam, numComps, &pPicParamSet->m_paletteInitializers[0], pPicParamSet->pps_num_palette_predictor_initializer);
        }
        else if (pSeqParamSet->sps_palette_predictor_initializer_present_flag)
        {
            VM_ASSERT(!pSeqParamSet->m_paletteInitializers.empty());
            VM_ASSERT(pSeqParamSet->sps_num_palette_predictor_initializer == pSeqParamSet->m_paletteInitializers.size());

            FillPaletteEntries(pPicParam, numComps, &pSeqParamSet->m_paletteInitializers[0], pSeqParamSet->sps_num_palette_predictor_initializer);
        }
    }
#endif

    void PackerDXVA2intel::PackPicParams(const H265DecoderFrame *pCurrentFrame, H265DecoderFrameInfo * pSliceInfo, TaskSupplier_H265 *supplier)
    {
        H265DBPList const* dpb = supplier->GetDPBList();
        if (!dpb)
            throw h265_exception(UMC_ERR_FAILED);

#if !defined(PRE_SI_TARGET_PLATFORM_GEN11)
        {
            DXVA_Intel_PicParams_HEVC* pp = 0;
            GetPicParamsBuffer(m_va, &pp, sizeof(DXVA_Intel_PicParams_HEVC));
            PackPicHeader(pCurrentFrame, pSliceInfo, dpb, pp);
            pp->StatusReportFeedbackNumber = m_statusReportFeedbackCounter;
        }
#else
#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
        if (m_va->m_Profile & VA_PROFILE_SCC)
        {
            DXVA_Intel_PicParams_HEVC_SCC* pp = 0;
            GetPicParamsBuffer(m_va, &pp, sizeof(DXVA_Intel_PicParams_HEVC_SCC));
            PackPicHeader(pCurrentFrame, pSliceInfo, dpb, pp);
            pp->PicParamsRext.PicParamsMain.StatusReportFeedbackNumber = m_statusReportFeedbackCounter;
        }
        else
#endif
        if (m_va->m_Profile & VA_PROFILE_REXT)
        {
            DXVA_Intel_PicParams_HEVC_Rext* pp = 0;
            GetPicParamsBuffer(m_va, &pp, sizeof(DXVA_Intel_PicParams_HEVC_Rext));
            PackPicHeader(pCurrentFrame, pSliceInfo, dpb, pp);
            pp->PicParamsMain.StatusReportFeedbackNumber = m_statusReportFeedbackCounter;
        }
        else
        {
            DXVA_Intel_PicParams_HEVC* pp = 0;
            GetPicParamsBuffer(m_va, &pp, sizeof(DXVA_Intel_PicParams_HEVC));
            PackPicHeader(pCurrentFrame, pSliceInfo, dpb, pp);
            pp->StatusReportFeedbackNumber = m_statusReportFeedbackCounter;
        }
#endif
    }

    template <typename T>
    inline
    void PackSliceHeaderCommon(H265Slice const* pSlice, DXVA_Intel_PicParams_HEVC const* pp, size_t prefix_size, T* header)
    {
        VM_ASSERT(pSlice);
        VM_ASSERT(pp);

        H265SliceHeader const* sh = pSlice->GetSliceHeader();
        VM_ASSERT(sh);

        header->ByteOffsetToSliceData = (UINT)(pSlice->m_BitStream.BytesDecoded() + prefix_size);
        header->slice_segment_address = sh->slice_segment_address;

        const H265DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();
        VM_ASSERT(pCurrentFrame);
        int const sliceNum = pSlice->GetSliceNum();

        for (int iDir = 0; iDir < 2; iDir++)
        {
            int index = 0;
            const UMC_HEVC_DECODER::H265DecoderRefPicList *pStartRefPicList = pCurrentFrame->GetRefPicList(sliceNum, iDir);
            if (!pStartRefPicList)
                return;
            const H265DecoderRefPicList::ReferenceInformation* pRefPicList = pStartRefPicList->m_refPicList;

            EnumRefPicList eRefPicList = (iDir == 1 ? REF_PIC_LIST_1 : REF_PIC_LIST_0);
            for (int i = 0; i < pSlice->getNumRefIdx(eRefPicList); i++)
            {
                const H265DecoderRefPicList::ReferenceInformation &frameInfo = pRefPicList[i];
                if (frameInfo.refFrame)
                {
                    bool isFound = false;
                    for (int j = 0; j < sizeof(pp->RefFrameList); j++)
                    {
                        if (pp->RefFrameList[j].Index7bits == (UCHAR)frameInfo.refFrame->m_index)
                        {
                            header->RefPicList[iDir][index].Index7bits = j;
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

            for (; index < 15; index++)
                header->RefPicList[iDir][index].bPicEntry = (UCHAR)-1;
        }


        // LongSliceFlags
        //        header->LongSliceFlags.fields.LastSliceOfPic = isLastSlice;
        header->LongSliceFlags.fields.dependent_slice_segment_flag = (UINT)sh->dependent_slice_segment_flag;   // dependent_slices_enabled_flag
        header->LongSliceFlags.fields.slice_type = (UINT)sh->slice_type;
        header->LongSliceFlags.fields.color_plane_id = 0; // field is left for future expansion
        header->LongSliceFlags.fields.slice_sao_luma_flag = (UINT)sh->slice_sao_luma_flag;
        header->LongSliceFlags.fields.slice_sao_chroma_flag = (UINT)sh->slice_sao_chroma_flag;
        header->LongSliceFlags.fields.mvd_l1_zero_flag = (UINT)sh->mvd_l1_zero_flag;
        header->LongSliceFlags.fields.cabac_init_flag = (UINT)sh->cabac_init_flag;
        header->LongSliceFlags.fields.slice_temporal_mvp_enabled_flag = (UINT)sh->slice_temporal_mvp_enabled_flag;
        header->LongSliceFlags.fields.slice_deblocking_filter_disabled_flag = (UINT)sh->slice_deblocking_filter_disabled_flag;
        header->LongSliceFlags.fields.collocated_from_l0_flag = (UINT)sh->collocated_from_l0_flag;
        header->LongSliceFlags.fields.slice_loop_filter_across_slices_enabled_flag = (UINT)sh->slice_loop_filter_across_slices_enabled_flag;

        //
        H265PicParamSet const* pps = pSlice->GetPicParam();
        VM_ASSERT(pps);

        header->collocated_ref_idx = (UCHAR)(sh->slice_type != I_SLICE ? sh->collocated_ref_idx : -1);
        header->num_ref_idx_l0_active_minus1 = (UCHAR)pSlice->getNumRefIdx(REF_PIC_LIST_0) - 1;
        header->num_ref_idx_l1_active_minus1 = (UCHAR)pSlice->getNumRefIdx(REF_PIC_LIST_1) - 1;
        header->slice_qp_delta = (CHAR)(sh->SliceQP - pps->init_qp);
        header->slice_cb_qp_offset = (CHAR)sh->slice_cb_qp_offset;
        header->slice_cr_qp_offset = (CHAR)sh->slice_cr_qp_offset;
        header->slice_beta_offset_div2 = (CHAR)(sh->slice_beta_offset >> 1);
        header->slice_tc_offset_div2 = (CHAR)(sh->slice_tc_offset >> 1);
        header->luma_log2_weight_denom = (UCHAR)sh->luma_log2_weight_denom;
        header->delta_chroma_log2_weight_denom = (UCHAR)(sh->chroma_log2_weight_denom - sh->luma_log2_weight_denom);

        header->five_minus_max_num_merge_cand = (UCHAR)(5 - sh->max_num_merge_cand);

        //WPP/tiles
        header->num_entry_point_offsets = static_cast<USHORT>(sh->num_entry_point_offsets);
        if (header->num_entry_point_offsets)
        {
            H265DecoderFrame* frame = pSlice->GetCurrentFrame();
            if (!frame)
                throw h265_exception(UMC_ERR_FAILED);

            H265DecoderFrameInfo* fi = frame->GetAU();
            if (!fi)
                throw h265_exception(UMC_ERR_FAILED);

            Ipp32u offset = 0;
            Ipp32u const count = fi->GetSliceCount();
            for (Ipp32u i = 0; i < count; ++i)
            {
                H265Slice const* slice = fi->GetSlice(i);
                VM_ASSERT(slice);

                if (slice == pSlice)
                    break;

                H265SliceHeader const* h = slice->GetSliceHeader();
                VM_ASSERT(h);
                offset += h->num_entry_point_offsets;
            }

            header->EntryOffsetToSubsetArray = static_cast<USHORT>(offset);
        }
    }

    inline
    void PackSliceHeader(H265Slice const* pSlice, DXVA_Intel_PicParams_HEVC const* pp, size_t prefix_size, DXVA_Intel_Slice_HEVC_Long* header)
    {
        PackSliceHeaderCommon(pSlice, pp, prefix_size, header);

        H265SliceHeader const* sh = pSlice->GetSliceHeader();
        VM_ASSERT(sh);

        for (int l = 0; l < 2; l++)
        {
            EnumRefPicList eRefPicList = (l == 1 ? REF_PIC_LIST_1 : REF_PIC_LIST_0);
            for (int iRefIdx = 0; iRefIdx < pSlice->getNumRefIdx(eRefPicList); iRefIdx++)
            {
                wpScalingParam const* wp = sh->pred_weight_table[eRefPicList][iRefIdx];

                if (eRefPicList == REF_PIC_LIST_0)
                {
                    header->luma_offset_l0[iRefIdx] = (CHAR)wp[0].offset;
                    header->delta_luma_weight_l0[iRefIdx] = (CHAR)wp[0].delta_weight;

                    for (int chroma = 0; chroma < 2; chroma++)
                    {
                        header->delta_chroma_weight_l0[iRefIdx][chroma] = (CHAR)wp[1 + chroma].delta_weight;
                        header->ChromaOffsetL0[iRefIdx][chroma] = (CHAR)wp[1 + chroma].offset;
                    }
                }
                else
                {
                    header->luma_offset_l1[iRefIdx] = (CHAR)wp[0].offset;
                    header->delta_luma_weight_l1[iRefIdx] = (CHAR)wp[0].delta_weight;

                    for (int chroma = 0; chroma < 2; chroma++)
                    {
                        header->delta_chroma_weight_l1[iRefIdx][chroma] = (CHAR)wp[1 + chroma].delta_weight;
                        header->ChromaOffsetL1[iRefIdx][chroma] = (CHAR)wp[1 + chroma].offset;
                    }
                }
            }
        }
    }

#if DDI_VERSION >= 943
    inline
    void PackSliceHeader(H265Slice const* pSlice, DXVA_Intel_PicParams_HEVC const* pp, size_t prefix_size, DXVA_Intel_Slice_HEVC_EXT_Long* header)
    {
        PackSliceHeaderCommon(pSlice, pp, prefix_size, header);

        H265SliceHeader const* sh = pSlice->GetSliceHeader();
        VM_ASSERT(sh);

        for (int l = 0; l < 2; l++)
        {
            EnumRefPicList eRefPicList = (l == 1 ? REF_PIC_LIST_1 : REF_PIC_LIST_0);
            for (int iRefIdx = 0; iRefIdx < pSlice->getNumRefIdx(eRefPicList); iRefIdx++)
            {
                wpScalingParam const* wp = sh->pred_weight_table[eRefPicList][iRefIdx];

                if (eRefPicList == REF_PIC_LIST_0)
                {
                    header->luma_offset_l0[iRefIdx] = (SHORT)wp[0].offset;
                    header->delta_luma_weight_l0[iRefIdx] = (CHAR)wp[0].delta_weight;

                    for (int chroma = 0; chroma < 2; chroma++)
                    {
                        header->delta_chroma_weight_l0[iRefIdx][chroma] = (CHAR)wp[1 + chroma].delta_weight;
                        header->ChromaOffsetL0[iRefIdx][chroma] = (SHORT)wp[1 + chroma].offset;
                    }
                }
                else
                {
                    header->luma_offset_l1[iRefIdx] = (SHORT)wp[0].offset;
                    header->delta_luma_weight_l1[iRefIdx] = (CHAR)wp[0].delta_weight;

                    for (int chroma = 0; chroma < 2; chroma++)
                    {
                        header->delta_chroma_weight_l1[iRefIdx][chroma] = (CHAR)wp[1 + chroma].delta_weight;
                        header->ChromaOffsetL1[iRefIdx][chroma] = (SHORT)wp[1 + chroma].offset;
                    }
                }
            }
        }

        header->SliceRextFlags.fields.cu_chroma_qp_offset_enabled_flag = sh->cu_chroma_qp_offset_enabled_flag;
#if DDI_VERSION >= 947
        header->slice_act_y_qp_offset  = (CHAR)sh->slice_act_y_qp_offset;
        header->slice_act_cb_qp_offset = (CHAR)sh->slice_act_cb_qp_offset;
        header->slice_act_cr_qp_offset = (CHAR)sh->slice_act_cr_qp_offset;
#endif
#if DDI_VERSION >= 949
        header->use_integer_mv_flag    = (UCHAR)sh->use_integer_mv_flag;
#endif
    }
#endif

    bool PackerDXVA2intel::PackSliceParams(H265Slice *pSlice, Ipp32u &, bool isLastSlice)
    {
        Ipp8u const start_code_prefix[] = { 0, 0, 1 };
        size_t const prefix_size = 0;//sizeof(start_code_prefix);

        H265HeadersBitstream* pBitstream = pSlice->GetBitStream();
        VM_ASSERT(pBitstream);

        Ipp32u      rawDataSize = 0;
        const void* rawDataPtr = 0;
        pBitstream->GetOrg((Ipp32u**)&rawDataPtr, &rawDataSize);

        void*       pSliceData = 0;
        bool isLong = m_va->IsLongSliceControl();
        if (!isLong)
        {
            DXVA_Intel_Slice_HEVC_Short* header;
            GetSliceVABuffers(m_va, &header, sizeof(DXVA_Intel_Slice_HEVC_Short), &pSliceData, rawDataSize + prefix_size, isLastSlice ? 128 : 0);
        }
        else
        {
            UMCVACompBuffer *compBuf;
            DXVA_Intel_PicParams_HEVC const* pp = (DXVA_Intel_PicParams_HEVC*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &compBuf);

#if !defined(PRE_SI_TARGET_PLATFORM_GEN11)
            {
                DXVA_Intel_Slice_HEVC_Long* header = 0;
                GetSliceVABuffers(m_va, &header, sizeof(DXVA_Intel_Slice_HEVC_Long), &pSliceData, rawDataSize + prefix_size, isLastSlice ? 128 : 0);
                PackSliceHeader(pSlice, pp, prefix_size, header);
            }
#else
            if (   (m_va->m_Profile & VA_PROFILE_REXT)
#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
                || (m_va->m_Profile & VA_PROFILE_SCC)
#endif
                )
            {
                DXVA_Intel_Slice_HEVC_EXT_Long* header = 0;
                GetSliceVABuffers(m_va, &header, sizeof(DXVA_Intel_Slice_HEVC_EXT_Long), &pSliceData, rawDataSize + prefix_size, isLastSlice ? 128 : 0);
                PackSliceHeader(pSlice, pp, prefix_size, header);
            }
            else
            {
                DXVA_Intel_Slice_HEVC_Long* header = 0;
                GetSliceVABuffers(m_va, &header, sizeof(DXVA_Intel_Slice_HEVC_Long), &pSliceData, rawDataSize + prefix_size, isLastSlice ? 128 : 0);
                PackSliceHeader(pSlice, pp, prefix_size, header);
            }
#endif
        }

        // copy slice data to slice data buffer
        VM_ASSERT(pSliceData);
        MFX_INTERNAL_CPY(pSliceData, start_code_prefix, prefix_size);
        MFX_INTERNAL_CPY((Ipp8u*)pSliceData + prefix_size, rawDataPtr, rawDataSize);

        return true;
    }

    void PackerDXVA2intel::PackQmatrix(const H265Slice *pSlice)
    {
        UMCVACompBuffer *compBuf;
        DXVA_Intel_Qmatrix_HEVC *pQmatrix = (DXVA_Intel_Qmatrix_HEVC *)m_va->GetCompBuffer(DXVA_INVERSE_QUANTIZATION_MATRIX_BUFFER, &compBuf);
        compBuf->SetDataSize(sizeof(DXVA_Intel_Qmatrix_HEVC));
        memset(pQmatrix, 0, sizeof(DXVA_Intel_Qmatrix_HEVC));

        PackerDXVA2::PackQmatrix(pSlice, pQmatrix);
    }

    void PackerDXVA2intel::PackSubsets(H265DecoderFrame const* frame)
    {
#if defined(MFX_ENABLE_HEVCD_WPP)
        if (m_va->m_HWPlatform < MFX_HW_TGL_LP)
            return;

        UMCVACompBuffer *compBuf;
        DXVA_Intel_PicParams_HEVC const* pp = (DXVA_Intel_PicParams_HEVC*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &compBuf);
        if (!pp)
            throw h265_exception(UMC_ERR_FAILED);

        if (!pp->TotalNumEntryPointOffsets)
            return;

        SUBSET_HEVC* subset =
            reinterpret_cast<SUBSET_HEVC*>(m_va->GetCompBuffer(D3DDDIFMT_INTEL_HEVC_SUBSET, &compBuf));
        if (!subset)
            throw h265_exception(UMC_ERR_FAILED);

        compBuf->SetDataSize(sizeof(SUBSET_HEVC));
        memset(subset, 0, compBuf->GetDataSize());

        H265DecoderFrameInfo* fi = frame->GetAU();
        if (!fi)
            throw h265_exception(UMC_ERR_FAILED);

        auto offset = &subset->entry_point_offset_minus1[0];
        Ipp32u const count = fi->GetSliceCount();
        for (Ipp32u i = 0; i < count; ++i)
        {
            H265Slice const* slice = fi->GetSlice(i);
            if (!slice)
                throw h265_exception(UMC_ERR_FAILED);

            H265SliceHeader const* sh = slice->GetSliceHeader();
            VM_ASSERT(sh);
            VM_ASSERT(sh->num_entry_point_offsets + 1 == slice->getTileLocationCount());

            auto location = offset;
            std::accumulate(slice->m_tileByteLocation + 1, slice->m_tileByteLocation + slice->getTileLocationCount(), slice->m_tileByteLocation[0],
                [&location](Ipp32u position, Ipp32u entry)
                { return *location++ = entry - (position  + 1); }
            );

            offset += sh->num_entry_point_offsets;
        }
#endif //MFX_ENABLE_HEVCD_WPP
    }
}

#endif //UMC_VA_DXVA

#endif // UMC_RESTRICTED_CODE_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER
