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
        #error "Gen11/Gen12 should be compiled with DDI_VERSION >= 0.943"
    #endif
#endif

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
    };

    Packer * CreatePackerIntel(UMC::VideoAccelerator* va)
    { return new PackerDXVA2intel(va); }

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

    void PackerDXVA2intel::PackPicParams(const H265DecoderFrame *pCurrentFrame, H265DecoderFrameInfo * pSliceInfo, TaskSupplier_H265 *supplier)
    {
        H265Slice *pSlice = pSliceInfo->GetSlice(0);
        if (!pSlice)
            return;
        H265SliceHeader * sliceHeader = pSlice->GetSliceHeader();
        const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
        const H265PicParamSet *pPicParamSet = pSlice->GetPicParam();

        UMCVACompBuffer *compBuf;
        DXVA_Intel_PicParams_HEVC *pPicParam = (DXVA_Intel_PicParams_HEVC*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &compBuf);
        Ipp32s const size =
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            !(m_va->m_Profile & VA_PROFILE_REXT) ? sizeof(DXVA_Intel_PicParams_HEVC) : sizeof(DXVA_Intel_PicParams_HEVC_Rext);
#else
            sizeof(DXVA_Intel_PicParams_HEVC);
#endif

        if (compBuf->GetBufferSize() < size)
            throw h265_exception(UMC_ERR_FAILED);

        compBuf->SetDataSize(size);
        memset(pPicParam, 0, size);

        //
        //
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
        pPicParam->CurrPic.Index7bits = pCurrentFrame->m_index;    // ?
        pPicParam->CurrPicOrderCntVal = sliceHeader->slice_pic_order_cnt_lsb;

        int count = 0;
        int cntRefPicSetStCurrBefore = 0,
            cntRefPicSetStCurrAfter = 0,
            cntRefPicSetLtCurr = 0;
        H265DBPList *dpb = supplier->GetDPBList();
        ReferencePictureSet *rps = pSlice->getRPS();
        for (H265DecoderFrame* frame = dpb->head(); frame && count < sizeof(pPicParam->RefFrameList) / sizeof(pPicParam->RefFrameList[0]); frame = frame->future())
        {
            if (frame != pCurrentFrame)
            {
                int refType = frame->isShortTermRef() ? SHORT_TERM_REFERENCE : (frame->isLongTermRef() ? LONG_TERM_REFERENCE : NO_REFERENCE);

                if (refType != NO_REFERENCE)
                {
                    pPicParam->PicOrderCntValList[count] = frame->m_PicOrderCnt;

                    pPicParam->RefFrameList[count].Index7bits = frame->m_index;
                    pPicParam->RefFrameList[count].long_term_ref_flag = (refType == LONG_TERM_REFERENCE);

                    count++;
                }
            }
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

        m_refFrameListCacheSize = count;
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

        pPicParam->StatusReportFeedbackNumber = m_statusReportFeedbackCounter;

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
        if (m_va->m_Profile & VA_PROFILE_REXT)
        {
            DXVA_Intel_PicParams_HEVC_Rext* pExtPicParam = (DXVA_Intel_PicParams_HEVC_Rext*)pPicParam;
            pExtPicParam->PicRangeExtensionFlags.fields.transform_skip_rotation_enabled_flag = pSeqParamSet->transform_skip_rotation_enabled_flag;
            pExtPicParam->PicRangeExtensionFlags.fields.transform_skip_context_enabled_flag = pSeqParamSet->transform_skip_context_enabled_flag;
            pExtPicParam->PicRangeExtensionFlags.fields.implicit_rdpcm_enabled_flag = pSeqParamSet->implicit_residual_dpcm_enabled_flag;
            pExtPicParam->PicRangeExtensionFlags.fields.explicit_rdpcm_enabled_flag = pSeqParamSet->explicit_residual_dpcm_enabled_flag;
            pExtPicParam->PicRangeExtensionFlags.fields.extended_precision_processing_flag = pSeqParamSet->extended_precision_processing_flag;
            pExtPicParam->PicRangeExtensionFlags.fields.intra_smoothing_disabled_flag = pSeqParamSet->intra_smoothing_disabled_flag;
            pExtPicParam->PicRangeExtensionFlags.fields.high_precision_offsets_enabled_flag = pSeqParamSet->high_precision_offsets_enabled_flag;
            pExtPicParam->PicRangeExtensionFlags.fields.persistent_rice_adaptation_enabled_flag = pSeqParamSet->fast_rice_adaptation_enabled_flag;
            pExtPicParam->PicRangeExtensionFlags.fields.cabac_bypass_alignment_enabled_flag = pSeqParamSet->cabac_bypass_alignment_enabled_flag;
            pExtPicParam->PicRangeExtensionFlags.fields.cross_component_prediction_enabled_flag = pPicParamSet->cross_component_prediction_enabled_flag;
            pExtPicParam->PicRangeExtensionFlags.fields.chroma_qp_offset_list_enabled_flag      = pPicParamSet->chroma_qp_offset_list_enabled_flag;

            pExtPicParam->diff_cu_chroma_qp_offset_depth            = (UCHAR)pPicParamSet->diff_cu_chroma_qp_offset_depth;
            pExtPicParam->chroma_qp_offset_list_len_minus1          = pPicParamSet->chroma_qp_offset_list_enabled_flag ?(UCHAR)pPicParamSet->chroma_qp_offset_list_len - 1 : 0;
            pExtPicParam->log2_sao_offset_scale_luma                = (UCHAR)pPicParamSet->log2_sao_offset_scale_luma;
            pExtPicParam->log2_sao_offset_scale_chroma              = (UCHAR)pPicParamSet->log2_sao_offset_scale_chroma;
            pExtPicParam->log2_max_transform_skip_block_size_minus2 = pPicParamSet->pps_range_extensions_flag && pPicParamSet->transform_skip_enabled_flag ? (UCHAR)pPicParamSet->log2_max_transform_skip_block_size - 2 : 0;

            for (Ipp32u i = 0; i < pPicParamSet->chroma_qp_offset_list_len; i++)
            {
                pExtPicParam->cb_qp_offset_list[i] = (CHAR)pPicParamSet->cb_qp_offset_list[i + 1];
                pExtPicParam->cr_qp_offset_list[i] = (CHAR)pPicParamSet->cr_qp_offset_list[i + 1];
            }
        }
#endif
    }

    template <typename T>
    inline
    void PackSliceHeaderCommon(H265Slice const* pSlice, DXVA_Intel_PicParams_HEVC const* pp, size_t prefix_size, T* header)
    {
        VM_ASSERT(pSlice);
        VM_ASSERT(pp);

        H265SliceHeader const* ssh = pSlice->GetSliceHeader();
        VM_ASSERT(ssh);

        header->ByteOffsetToSliceData = (UINT)(pSlice->m_BitStream.BytesDecoded() + prefix_size);
        header->slice_segment_address = ssh->slice_segment_address;

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
        header->LongSliceFlags.fields.dependent_slice_segment_flag = (UINT)ssh->dependent_slice_segment_flag;   // dependent_slices_enabled_flag
        header->LongSliceFlags.fields.slice_type = (UINT)ssh->slice_type;
        header->LongSliceFlags.fields.color_plane_id = 0; // field is left for future expansion
        header->LongSliceFlags.fields.slice_sao_luma_flag = (UINT)ssh->slice_sao_luma_flag;
        header->LongSliceFlags.fields.slice_sao_chroma_flag = (UINT)ssh->slice_sao_chroma_flag;
        header->LongSliceFlags.fields.mvd_l1_zero_flag = (UINT)ssh->mvd_l1_zero_flag;
        header->LongSliceFlags.fields.cabac_init_flag = (UINT)ssh->cabac_init_flag;
        header->LongSliceFlags.fields.slice_temporal_mvp_enabled_flag = (UINT)ssh->slice_temporal_mvp_enabled_flag;
        header->LongSliceFlags.fields.slice_deblocking_filter_disabled_flag = (UINT)ssh->slice_deblocking_filter_disabled_flag;
        header->LongSliceFlags.fields.collocated_from_l0_flag = (UINT)ssh->collocated_from_l0_flag;
        header->LongSliceFlags.fields.slice_loop_filter_across_slices_enabled_flag = (UINT)ssh->slice_loop_filter_across_slices_enabled_flag;

        //
        H265PicParamSet const* pps = pSlice->GetPicParam();
        VM_ASSERT(pps);

        header->collocated_ref_idx = (UCHAR)(ssh->slice_type != I_SLICE ? ssh->collocated_ref_idx : -1);
        header->num_ref_idx_l0_active_minus1 = (UCHAR)pSlice->getNumRefIdx(REF_PIC_LIST_0) - 1;
        header->num_ref_idx_l1_active_minus1 = (UCHAR)pSlice->getNumRefIdx(REF_PIC_LIST_1) - 1;
        header->slice_qp_delta = (CHAR)(ssh->SliceQP - pps->init_qp);
        header->slice_cb_qp_offset = (CHAR)ssh->slice_cb_qp_offset;
        header->slice_cr_qp_offset = (CHAR)ssh->slice_cr_qp_offset;
        header->slice_beta_offset_div2 = (CHAR)(ssh->slice_beta_offset >> 1);
        header->slice_tc_offset_div2 = (CHAR)(ssh->slice_tc_offset >> 1);
        header->luma_log2_weight_denom = (UCHAR)ssh->luma_log2_weight_denom;
        header->delta_chroma_log2_weight_denom = (UCHAR)(ssh->chroma_log2_weight_denom - ssh->luma_log2_weight_denom);

        header->five_minus_max_num_merge_cand = (UCHAR)(5 - ssh->max_num_merge_cand);
    }

    inline
    void PackSliceHeader(H265Slice const* pSlice, DXVA_Intel_PicParams_HEVC const* pp, size_t prefix_size, DXVA_Intel_Slice_HEVC_Long* header)
    {
        PackSliceHeaderCommon(pSlice, pp, prefix_size, header);

        H265SliceHeader const* ssh = pSlice->GetSliceHeader();
        VM_ASSERT(ssh);

        for (int l = 0; l < 2; l++)
        {
            EnumRefPicList eRefPicList = (l == 1 ? REF_PIC_LIST_1 : REF_PIC_LIST_0);
            for (int iRefIdx = 0; iRefIdx < pSlice->getNumRefIdx(eRefPicList); iRefIdx++)
            {
                wpScalingParam const* wp = ssh->pred_weight_table[eRefPicList][iRefIdx];

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
    void PackSliceHeader(H265Slice const* pSlice, DXVA_Intel_PicParams_HEVC const* pp, size_t prefix_size, DXVA_Intel_Slice_HEVC_Rext_Long* header)
    {
        PackSliceHeaderCommon(pSlice, pp, prefix_size, header);

        H265SliceHeader const* ssh = pSlice->GetSliceHeader();
        VM_ASSERT(ssh);

        for (int l = 0; l < 2; l++)
        {
            EnumRefPicList eRefPicList = (l == 1 ? REF_PIC_LIST_1 : REF_PIC_LIST_0);
            for (int iRefIdx = 0; iRefIdx < pSlice->getNumRefIdx(eRefPicList); iRefIdx++)
            {
                wpScalingParam const* wp = ssh->pred_weight_table[eRefPicList][iRefIdx];

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

        header->SliceRextFlags.fields.cu_chroma_qp_offset_enabled_flag = ssh->cu_chroma_qp_offset_enabled_flag;
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

            const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();

#if !defined(PRE_SI_TARGET_PLATFORM_GEN11)
            {
                DXVA_Intel_Slice_HEVC_Long* header = 0;
                GetSliceVABuffers(m_va, &header, sizeof(DXVA_Intel_Slice_HEVC_Long), &pSliceData, rawDataSize + prefix_size, isLastSlice ? 128 : 0);
                PackSliceHeader(pSlice, pp, prefix_size, header);
            }
#else
            if (!(m_va->m_Profile & VA_PROFILE_REXT))
            {
                DXVA_Intel_Slice_HEVC_Long* header = 0;
                GetSliceVABuffers(m_va, &header, sizeof(DXVA_Intel_Slice_HEVC_Long), &pSliceData, rawDataSize + prefix_size, isLastSlice ? 128 : 0);
                PackSliceHeader(pSlice, pp, prefix_size, header);
            }
            else
            {
                DXVA_Intel_Slice_HEVC_Rext_Long* header = 0;
                GetSliceVABuffers(m_va, &header, sizeof(DXVA_Intel_Slice_HEVC_Rext_Long), &pSliceData, rawDataSize + prefix_size, isLastSlice ? 128 : 0);
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
}

#endif //UMC_VA_DXVA

#endif // UMC_RESTRICTED_CODE_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER
