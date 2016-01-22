/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2013-2016 Intel Corporation. All Rights Reserved.
//
//
*/

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef UMC_RESTRICTED_CODE_VA

#include "umc_h265_va_packer.h"

#ifdef UMC_VA
#include "umc_h265_task_supplier.h"
#include "huc_based_drm_common.h"
#endif

#ifdef UMC_VA_DXVA
#include "umc_va_base.h"
#include "umc_va_dxva2_protected.h"
#endif

#ifdef UMC_VA_LINUX
#include "umc_va_linux_protected.h"
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


Packer * Packer::CreatePacker(UMC::VideoAccelerator * va)
{
    Packer * packer = 0;

#ifdef UMC_VA_DXVA
    if (va->GetProtectedVA() && IS_PROTECTION_WIDEVINE(va->GetProtectedVA()->GetProtected()))
        packer = new PackerDXVA2_Widevine(va);
    else if (va->IsIntelCustomGUID())
        packer = new PackerDXVA2(va);
    else
        packer = new MSPackerDXVA2(va);
#elif defined(UMC_VA_LINUX)
    if (va->GetProtectedVA() && IS_PROTECTION_WIDEVINE(va->GetProtectedVA()->GetProtected()))
        packer = new PackerVA_Widevine(va);
    else
        packer = new PackerVA(va);
#endif

    return packer;
}

Packer::Packer(UMC::VideoAccelerator * va)
    : m_va(va)
{
}

Packer::~Packer()
{
}

#ifdef UMC_VA

//the tables used to restore original scan order of scaling lists (req. by drivers since ci-main-49045)
Ipp16u const* SL_tab_up_right[] =
{
    ScanTableDiag4x4,
    g_sigLastScanCG32x32,
    g_sigLastScanCG32x32,
    g_sigLastScanCG32x32
};

template <int COUNT>
inline
void initQMatrix(const H265ScalingList *scalingList, int sizeId, unsigned char qm[6][COUNT], bool force_upright_scan = false)
{
    /*        n*m    listId
        --------------------
        Intra   Y       0
        Intra   Cb      1
        Intra   Cr      2
        Inter   Y       3
        Inter   Cb      4
        Inter   Cr      5         */

    Ipp16u const* scan = 0;
    if (force_upright_scan)
        scan = SL_tab_up_right[sizeId];

    for (int n = 0; n < 6; n++)
    {
        const int *src = scalingList->getScalingListAddress(sizeId, n);
        for (int i = 0; i < COUNT; i++)  // coef.
        {
            int const idx = scan ? scan[i] : i;
            qm[n][i] = (unsigned char)src[idx];
        }
    }
}

template<int COUNT>
inline
void initQMatrix(const H265ScalingList *scalingList, int sizeId, unsigned char qm[3][2][COUNT])
{
    for(int comp=0 ; comp <= 2 ; comp++)    // Y Cb Cr
    {
        for(int n=0; n <= 1;n++)
        {
            int listId = comp + 3*n;
            const int *src = scalingList->getScalingListAddress(sizeId, listId);
            for(int i=0;i < COUNT;i++)  // coef.
                qm[comp][n][i] = (unsigned char)src[i];
        }
    }
}

inline
void initQMatrix(const H265ScalingList *scalingList, int sizeId, unsigned char qm[2][64], bool force_upright_scan = false)
{
    /*      n      m     listId
        --------------------
        Intra   Y       0
        Inter   Y       1         */

    Ipp16u const* scan = 0;
    if (force_upright_scan)
        scan = SL_tab_up_right[sizeId];

    for(int n=0;n < 2;n++)  // Intra, Inter
    {
        const int *src = scalingList->getScalingListAddress(sizeId, n);

        for (int i = 0; i < 64; i++)  // coef.
        {
            int const idx = scan ? scan[i] : i;
            qm[n][i] = (unsigned char)src[idx];
        }
    }
}

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
    const int *src = 0;
    switch(sizeId)
    {
    case SCALING_LIST_4x4:
        src = (int*)g_quantTSDefault4x4;
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
#endif // #ifdef UMC_VA

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

static inline int LengthInMinCb(int length, int cbsize)
{
    return length/(1 << cbsize);
}

void PackerDXVA2::GetSliceVABuffers(
    DXVA_Intel_Slice_HEVC_Long **ppSliceHeader, Ipp32s headerSize,
    void **ppSliceData, Ipp32s dataSize,
    Ipp32s dataAlignment)
{
    VM_ASSERT(ppSliceHeader);
    VM_ASSERT(ppSliceData);

    for(int phase=0;phase < 2;phase++)
    {
        UMCVACompBuffer *headVABffr = 0;
        UMCVACompBuffer *dataVABffr = 0;
        void *headBffr = m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &headVABffr);
        void *dataBffr = m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &dataVABffr);
        Ipp32s headBffrSize = headVABffr->GetBufferSize(),
               headBffrOffs = headVABffr->GetDataSize();
        Ipp32s dataBffrSize = dataVABffr->GetBufferSize(),
               dataBffrOffs = dataVABffr->GetDataSize();

        dataBffrOffs = dataAlignment ? dataAlignment*((dataBffrOffs + dataAlignment - 1)/dataAlignment) : dataBffrOffs;

        if( headBffrSize - headBffrOffs < headerSize ||
            dataBffrSize - dataBffrOffs < dataSize )
        {
            if(phase == 0)
            {
                Status s = m_va->Execute();
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
        (*ppSliceHeader)->SliceBytesInBuffer = (UINT)dataSize;
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

void PackerDXVA2::PackAU(const H265DecoderFrame *frame, TaskSupplier_H265 * supplier)
{
    H265DecoderFrameInfo * sliceInfo = frame->m_pSlicesInfo;
    int sliceCount = sliceInfo->GetSliceCount();

    if (!sliceCount)
        return;

    H265Slice *pSlice = sliceInfo->GetSlice(0);
    const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
    H265DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();

    Ipp32s first_slice = 0;

    for (;;)
    {
        bool notchopping = true;
        PackPicParams(pCurrentFrame, sliceInfo, supplier);
        if (pSeqParamSet->scaling_list_enabled_flag)
        {
            PackQmatrix(pSlice);
        }

        Ipp32u sliceNum = 0;
        for (Ipp32s n = first_slice; n < sliceCount; n++)
        {
            notchopping = PackSliceParams(sliceInfo->GetSlice(n), sliceNum, n == sliceCount - 1);
            if (!notchopping)
            {
                //dependent slices should be with first independent slice
                for (Ipp32s i = n; i >= first_slice; i--)
                {
                    if (!sliceInfo->GetSlice(i)->GetSliceHeader()->dependent_slice_segment_flag)
                        break;

                    UMCVACompBuffer *headVABffr = 0;

                    m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &headVABffr);
                    size_t headerSize = m_va->IsLongSliceControl() ? sizeof(DXVA_Intel_Slice_HEVC_Long) : sizeof(DXVA_Slice_HEVC_Short);
                    headVABffr->SetDataSize(headVABffr->GetDataSize() - headerSize); //remove one slice

                    n--;
                }

                if (n <= first_slice) // avoid splitting of slice
                {
                    m_va->Execute(); //for free picParam and Quant buffers
                    return;
                }

                first_slice = n;
                break;
            }

            sliceNum++;
        }

        Status s = m_va->Execute();
        if(s != UMC_OK)
            throw h265_exception(s);

        if (!notchopping)
            continue;

        break;
    }
}

void PackerDXVA2::PackPicParams(const H265DecoderFrame *pCurrentFrame,
                        H265DecoderFrameInfo * pSliceInfo,
                        TaskSupplier_H265 *supplier)
{
    UMCVACompBuffer *compBuf;
    DXVA_Intel_PicParams_HEVC *pPicParam = (DXVA_Intel_PicParams_HEVC*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &compBuf);
    compBuf->SetDataSize(sizeof(DXVA_Intel_PicParams_HEVC));

    memset(pPicParam, 0, sizeof(DXVA_Intel_PicParams_HEVC));

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
    pPicParam->PicFlags.fields.separate_colour_plane_flag                   = pSeqParamSet->separate_colour_plane_flag;
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

    Ipp32u index;
    int pocList[3*8];
    int numRefPicSetStCurrBefore = 0,
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
    pPicParam->fields.scaling_list_enabled_flag                      = pSeqParamSet->scaling_list_enabled_flag;
    pPicParam->fields.amp_enabled_flag                               = pSeqParamSet->amp_enabled_flag;
    pPicParam->fields.sample_adaptive_offset_enabled_flag            = pSeqParamSet->sample_adaptive_offset_enabled_flag;
    pPicParam->fields.pcm_enabled_flag                               = pSeqParamSet->pcm_enabled_flag;
    pPicParam->fields.pcm_sample_bit_depth_luma_minus1               = (UCHAR)(pSeqParamSet->pcm_sample_bit_depth_luma - 1);
    pPicParam->fields.pcm_sample_bit_depth_chroma_minus1             = (UCHAR)(pSeqParamSet->pcm_sample_bit_depth_chroma - 1);
    pPicParam->fields.log2_min_pcm_luma_coding_block_size_minus3     = (UCHAR)(pSeqParamSet->log2_min_pcm_luma_coding_block_size - 3);
    pPicParam->fields.log2_diff_max_min_pcm_luma_coding_block_size   = (UCHAR)(pSeqParamSet->log2_max_pcm_luma_coding_block_size - pSeqParamSet->log2_min_pcm_luma_coding_block_size);
    pPicParam->fields.pcm_loop_filter_disabled_flag                  = pSeqParamSet->pcm_loop_filter_disabled_flag;
    pPicParam->fields.long_term_ref_pics_present_flag                = pSeqParamSet->long_term_ref_pics_present_flag;
    pPicParam->fields.sps_temporal_mvp_enabled_flag                  = pSeqParamSet->sps_temporal_mvp_enabled_flag;
    pPicParam->fields.strong_intra_smoothing_enabled_flag            = pSeqParamSet->sps_strong_intra_smoothing_enabled_flag;
    pPicParam->fields.dependent_slice_segments_enabled_flag          = pPicParamSet->dependent_slice_segments_enabled_flag;
    pPicParam->fields.output_flag_present_flag                       = pPicParamSet->output_flag_present_flag;
    pPicParam->fields.num_extra_slice_header_bits                    = (UCHAR)pPicParamSet->num_extra_slice_header_bits;
    pPicParam->fields.sign_data_hiding_flag                          = pPicParamSet->sign_data_hiding_enabled_flag;
    pPicParam->fields.cabac_init_present_flag                        = pPicParamSet->cabac_init_present_flag;

    // PicShortFormatFlags
    //
    pPicParam->PicShortFormatFlags.fields.constrained_intra_pred_flag                   = pPicParamSet->constrained_intra_pred_flag;
    pPicParam->PicShortFormatFlags.fields.transform_skip_enabled_flag                   = pPicParamSet->transform_skip_enabled_flag;
    pPicParam->PicShortFormatFlags.fields.cu_qp_delta_enabled_flag                      = pPicParamSet->cu_qp_delta_enabled_flag;
    pPicParam->PicShortFormatFlags.fields.pps_slice_chroma_qp_offsets_present_flag      = pPicParamSet->pps_slice_chroma_qp_offsets_present_flag;
    pPicParam->PicShortFormatFlags.fields.weighted_pred_flag                            = pPicParamSet->weighted_pred_flag;
    pPicParam->PicShortFormatFlags.fields.weighted_bipred_flag                          = pPicParamSet->weighted_bipred_flag;
    pPicParam->PicShortFormatFlags.fields.transquant_bypass_enabled_flag                = pPicParamSet->transquant_bypass_enabled_flag;
    pPicParam->PicShortFormatFlags.fields.tiles_enabled_flag                            = pPicParamSet->tiles_enabled_flag;
    pPicParam->PicShortFormatFlags.fields.entropy_coding_sync_enabled_flag              = pPicParamSet->entropy_coding_sync_enabled_flag;
    pPicParam->PicShortFormatFlags.fields.uniform_spacing_flag                          = pPicParamSet->uniform_spacing_flag;
    pPicParam->PicShortFormatFlags.fields.loop_filter_across_tiles_enabled_flag         = pPicParamSet->loop_filter_across_tiles_enabled_flag;
    pPicParam->PicShortFormatFlags.fields.pps_loop_filter_across_slices_enabled_flag    = pPicParamSet->pps_loop_filter_across_slices_enabled_flag;
    pPicParam->PicShortFormatFlags.fields.deblocking_filter_override_enabled_flag       = pPicParamSet->deblocking_filter_override_enabled_flag;
    pPicParam->PicShortFormatFlags.fields.pps_deblocking_filter_disabled_flag           = pPicParamSet->pps_deblocking_filter_disabled_flag;
    pPicParam->PicShortFormatFlags.fields.lists_modification_present_flag               = pPicParamSet->lists_modification_present_flag;
    pPicParam->PicShortFormatFlags.fields.slice_segment_header_extension_present_flag   = pPicParamSet->slice_segment_header_extension_present_flag;
    pPicParam->PicShortFormatFlags.fields.IrapPicFlag                                   = (sliceHeader->nal_unit_type >= NAL_UT_CODED_SLICE_BLA_W_LP && sliceHeader->nal_unit_type <= NAL_UT_CODED_SLICE_CRA) ? 1 : 0;
    pPicParam->PicShortFormatFlags.fields.IdrPicFlag                                    = sliceHeader->IdrPicFlag;
    pPicParam->PicShortFormatFlags.fields.IntraPicFlag                                  = pSliceInfo->IsIntraAU() ? 1 : 0;

    pPicParam->pps_cb_qp_offset                                     = (CHAR)pPicParamSet->pps_cb_qp_offset;
    pPicParam->pps_cr_qp_offset                                     = (CHAR)pPicParamSet->pps_cr_qp_offset;
    if(pPicParam->PicShortFormatFlags.fields.tiles_enabled_flag)
    {
        pPicParam->num_tile_columns_minus1                          = (UCHAR)pPicParamSet->num_tile_columns - 1;
        pPicParam->num_tile_rows_minus1                             = (UCHAR)pPicParamSet->num_tile_rows - 1;

        pPicParam->num_tile_columns_minus1 = IPP_MIN(sizeof(pPicParam->column_width_minus1)/sizeof(pPicParam->column_width_minus1[0]) - 1, pPicParam->num_tile_columns_minus1);
        pPicParam->num_tile_rows_minus1 = IPP_MIN(sizeof(pPicParam->row_height_minus1)/sizeof(pPicParam->row_height_minus1[0]) - 1, pPicParam->num_tile_rows_minus1);

        for (Ipp32u i = 0; i <= pPicParam->num_tile_columns_minus1; i++)
            pPicParam->column_width_minus1[i] = (Ipp16u)(pPicParamSet->column_width[i] - 1);

        for (Ipp32u i = 0; i <= pPicParam->num_tile_rows_minus1; i++)
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

bool PackerDXVA2::PackSliceParams(H265Slice *pSlice, Ipp32u &, bool isLastSlice)
{
    static Ipp8u start_code_prefix[] = {0, 0, 1};

    DXVA_Intel_Slice_HEVC_Long* pDXVASlice = 0;

    void*   pSliceData = 0;
    Ipp32u  rawDataSize = 0;
    const void*   rawDataPtr = 0;
    Ipp32s  headerSize = sizeof(DXVA_Slice_HEVC_Short);

    bool isLong = m_va->IsLongSliceControl();
    if (isLong)
    {
        headerSize = sizeof(DXVA_Intel_Slice_HEVC_Long);

        pSlice->m_BitStream.GetOrg((Ipp32u**)&rawDataPtr, &rawDataSize);

        GetSliceVABuffers(&pDXVASlice, headerSize, &pSliceData, rawDataSize + sizeof(start_code_prefix),  isLastSlice ? 128 : 0);

        const H265PicParamSet *pPicParamSet = pSlice->GetPicParam();
        const H265DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();
        int sliceNum = pSlice->GetSliceNum();

        pDXVASlice->ByteOffsetToSliceData = (UINT)(pSlice->m_BitStream.BytesDecoded() + sizeof(start_code_prefix));  // ???
        pDXVASlice->slice_segment_address = pSlice->GetSliceHeader()->slice_segment_address;

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
        pDXVASlice->slice_qp_delta                  = (CHAR)(pSlice->GetSliceHeader()->SliceQP - pPicParamSet->init_qp);
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
        GetSliceVABuffers(&pDXVASlice, headerSize, &pSliceData, rawDataSize + 3, isLastSlice ? 128 : 0);
    }

    // copy slice data to slice data buffer
    MFX_INTERNAL_CPY(pSliceData, start_code_prefix, sizeof(start_code_prefix));
    MFX_INTERNAL_CPY((Ipp8u*)pSliceData + sizeof(start_code_prefix), rawDataPtr, rawDataSize);
    return true;
}

void PackerDXVA2::PackQmatrix(const H265Slice *pSlice)
{
    UMCVACompBuffer *compBuf;
    DXVA_Intel_Qmatrix_HEVC *pQmatrix = (DXVA_Intel_Qmatrix_HEVC *)m_va->GetCompBuffer(DXVA_INVERSE_QUANTIZATION_MATRIX_BUFFER, &compBuf);
    compBuf->SetDataSize(sizeof(DXVA_Intel_Qmatrix_HEVC));
    memset(pQmatrix, 0, sizeof(DXVA_Intel_Qmatrix_HEVC));

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
    initQMatrix    (scalingList, SCALING_LIST_32x32, pQmatrix->ucScalingLists3);    // 32x32

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
    memset(pQmatrix, 0, sizeof(DXVA_Qmatrix_HEVC));

    const H265ScalingList *scalingList = 0;

    //new driver want list to be in raster scan, but we made it flatten during [H265HeadersBitstream::xDecodeScalingList]
    //so we should restore it now
    bool force_upright_scan =
        m_va->ScalingListScanOrder() == 0;

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

        if (doInit)
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

    initQMatrix<16>(scalingList, SCALING_LIST_4x4,   pQmatrix->ucScalingLists0, force_upright_scan);    // 4x4
    initQMatrix<64>(scalingList, SCALING_LIST_8x8,   pQmatrix->ucScalingLists1, force_upright_scan);    // 8x8
    initQMatrix<64>(scalingList, SCALING_LIST_16x16, pQmatrix->ucScalingLists2, force_upright_scan);    // 16x16
    initQMatrix    (scalingList, SCALING_LIST_32x32, pQmatrix->ucScalingLists3, force_upright_scan);    // 32x32

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

    pPicParam->CurrPic.Index7Bits   = pCurrentFrame->m_index;

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

        pPicParam->num_tile_columns_minus1 = IPP_MIN(sizeof(pPicParam->column_width_minus1)/sizeof(pPicParam->column_width_minus1[0]) - 1, pPicParam->num_tile_columns_minus1);
        pPicParam->num_tile_rows_minus1 = IPP_MIN(sizeof(pPicParam->row_height_minus1)/sizeof(pPicParam->row_height_minus1[0]) - 1, pPicParam->num_tile_rows_minus1);

        //if (!pPicParamSet->uniform_spacing_flag)
        {
            for (Ipp32u i = 0; i <= pPicParam->num_tile_columns_minus1; i++)
                pPicParam->column_width_minus1[i] = (Ipp16u)(pPicParamSet->column_width[i] - 1);

            for (Ipp32u i = 0; i <= pPicParam->num_tile_rows_minus1; i++)
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

    Ipp32u index;
    Ipp32s pocList[3*8];
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

bool MSPackerDXVA2::PackSliceParams(H265Slice *pSlice, Ipp32u &sliceNum, bool isLastSlice)
{
    static Ipp8u start_code_prefix[] = {0, 0, 1};

    Ipp32u  rawDataSize = 0;
    const void*  rawDataPtr = 0;

    H265Bitstream *pBitstream = pSlice->GetBitStream();
    pBitstream->GetOrg((Ipp32u**)&rawDataPtr, &rawDataSize);

    UMCVACompBuffer *headVABffr = 0;
    UMCVACompBuffer *dataVABffr = 0;
    DXVA_Slice_HEVC_Short* dxvaSlice = (DXVA_Slice_HEVC_Short*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &headVABffr);
    if (headVABffr->GetBufferSize() - headVABffr->GetDataSize() < sizeof(DXVA_Slice_HEVC_Short))
        return false;

    dxvaSlice += sliceNum;
    Ipp8u *dataBffr = (Ipp8u *)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &dataVABffr);

    dxvaSlice->BSNALunitDataLocation = dataVABffr->GetDataSize();

    Ipp32s storedSize = rawDataSize + sizeof(start_code_prefix);
    dxvaSlice->SliceBytesInBuffer = storedSize;
    dxvaSlice->wBadSliceChopping = 0;

    if (storedSize + 127 >= dataVABffr->GetBufferSize() - dataVABffr->GetDataSize())
        return false;

    dataBffr += dataVABffr->GetDataSize();
    MFX_INTERNAL_CPY(dataBffr, start_code_prefix, sizeof(start_code_prefix));
    MFX_INTERNAL_CPY(dataBffr + sizeof(start_code_prefix), rawDataPtr, rawDataSize);

    Ipp32s fullSize = dataVABffr->GetDataSize() + storedSize;
    if (isLastSlice)
    {
        Ipp32s alignedSize = align_value<Ipp32s>(fullSize, 128);
        VM_ASSERT(alignedSize < dataVABffr->GetBufferSize());
        memset(dataBffr + storedSize, 0, alignedSize - fullSize);
        fullSize = alignedSize;
    }

    dataVABffr->SetDataSize(fullSize);
    headVABffr->SetDataSize(headVABffr->GetDataSize() + sizeof(DXVA_Slice_HEVC_Short));
    return true;
}


PackerDXVA2_Widevine::PackerDXVA2_Widevine(VideoAccelerator * va)
    : PackerDXVA2(va)
{
}

void PackerDXVA2_Widevine::PackAU(const H265DecoderFrame *frame, TaskSupplier_H265 * supplier)
{
    H265DecoderFrameInfo * sliceInfo = frame->m_pSlicesInfo;
    int sliceCount = sliceInfo->GetSliceCount();

    if (!sliceCount)
        return;

    H265Slice *pSlice = sliceInfo->GetSlice(0);
    //const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
    H265DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();

    //Ipp32s first_slice = 0;

    //for (;;)
    //{
        //bool notchopping = true;
        PackPicParams(pCurrentFrame, sliceInfo, supplier);
    //    if (pSeqParamSet->scaling_list_enabled_flag)
    //    {
    //        PackQmatrix(pSlice);
    //    }

    //    Ipp32u sliceNum = 0;
    //    for (Ipp32s n = first_slice; n < sliceCount; n++)
    //    {
    //        notchopping = PackSliceParams(sliceInfo->GetSlice(n), sliceNum, n == sliceCount - 1);
    //        if (!notchopping)
    //        {
    //            //dependent slices should be with first independent slice
    //            for (Ipp32s i = n; i >= first_slice; i--)
    //            {
    //                if (!sliceInfo->GetSlice(i)->GetSliceHeader()->dependent_slice_segment_flag)
    //                    break;

    //                UMCVACompBuffer *headVABffr = 0;

    //                m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &headVABffr);
    //                size_t headerSize = m_va->IsLongSliceControl() ? sizeof(DXVA_Intel_Slice_HEVC_Long) : sizeof(DXVA_Slice_HEVC_Short);
    //                headVABffr->SetDataSize(headVABffr->GetDataSize() - headerSize); //remove one slice

    //                n--;
    //            }

    //            if (n <= first_slice) // avoid splitting of slice
    //            {
    //                m_va->Execute(); //for free picParam and Quant buffers
    //                return;
    //            }

    //            first_slice = n;
    //            break;
    //        }

    //        sliceNum++;
    //    }

        Status s = m_va->Execute();
        if(s != UMC_OK)
            throw h265_exception(s);

    //    if (!notchopping)
    //        continue;

    //    break;
    //}

    if (m_va && m_va->GetProtectedVA())
    {
        mfxBitstream * bs = m_va->GetProtectedVA()->GetBitstream();

        if (bs && bs->EncryptedData)
        {
            Ipp32s count = sliceInfo->GetSliceCount();
            m_va->GetProtectedVA()->MoveBSCurrentEncrypt(count);
        }
    }
}

void PackerDXVA2_Widevine::PackPicParams(const H265DecoderFrame *pCurrentFrame,
                        H265DecoderFrameInfo * pSliceInfo,
                        TaskSupplier_H265 *supplier)
{
    UMCVACompBuffer *compBuf;
    DXVA_Intel_PicParams_HEVC *pPicParam = (DXVA_Intel_PicParams_HEVC*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &compBuf);
    compBuf->SetDataSize(sizeof(DXVA_Intel_PicParams_HEVC));

    memset(pPicParam, 0, sizeof(DXVA_Intel_PicParams_HEVC));

    H265Slice *pSlice = pSliceInfo->GetSlice(0);
    H265SliceHeader * sliceHeader = pSlice->GetSliceHeader();
    const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
    //const H265PicParamSet *pPicParamSet = pSlice->GetPicParam();

    ////
    ////
    //pPicParam->PicWidthInMinCbsY   = (USHORT)LengthInMinCb(pSeqParamSet->pic_width_in_luma_samples, pSeqParamSet->log2_min_luma_coding_block_size);
    //pPicParam->PicHeightInMinCbsY  = (USHORT)LengthInMinCb(pSeqParamSet->pic_height_in_luma_samples, pSeqParamSet->log2_min_luma_coding_block_size);

    ////
    ////
    //pPicParam->PicFlags.fields.chroma_format_idc                            = pSeqParamSet->chroma_format_idc;
    //pPicParam->PicFlags.fields.separate_colour_plane_flag                   = pSeqParamSet->separate_colour_plane_flag;
    //pPicParam->PicFlags.fields.bit_depth_luma_minus8                        = (UCHAR)(pSeqParamSet->bit_depth_luma - 8);
    //pPicParam->PicFlags.fields.bit_depth_chroma_minus8                      = (UCHAR)(pSeqParamSet->bit_depth_chroma - 8);
    //pPicParam->PicFlags.fields.log2_max_pic_order_cnt_lsb_minus4            = (UCHAR)(pSeqParamSet->log2_max_pic_order_cnt_lsb - 4);
    //pPicParam->PicFlags.fields.NoPicReorderingFlag                          = pSeqParamSet->sps_max_num_reorder_pics == 0 ? 1 : 0;
    //pPicParam->PicFlags.fields.NoBiPredFlag                                 = 0;

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

    Ipp32u index;
    int pocList[3*8];
    int numRefPicSetStCurrBefore = 0,
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

    ////
    ////
    //pPicParam->sps_max_dec_pic_buffering_minus1             = (UCHAR)(pSeqParamSet->sps_max_dec_pic_buffering[sliceHeader->nuh_temporal_id] - 1);
    //pPicParam->log2_min_luma_coding_block_size_minus3       = (UCHAR)(pSeqParamSet->log2_min_luma_coding_block_size- 3);
    //pPicParam->log2_diff_max_min_transform_block_size       = (UCHAR)(pSeqParamSet->log2_max_transform_block_size - pSeqParamSet->log2_min_transform_block_size);
    //pPicParam->log2_min_transform_block_size_minus2         = (UCHAR)(pSeqParamSet->log2_min_transform_block_size - 2);
    //pPicParam->log2_diff_max_min_luma_coding_block_size     = (UCHAR)(pSeqParamSet->log2_max_luma_coding_block_size - pSeqParamSet->log2_min_luma_coding_block_size);
    //pPicParam->max_transform_hierarchy_depth_intra          = (UCHAR)pSeqParamSet->max_transform_hierarchy_depth_intra;
    //pPicParam->max_transform_hierarchy_depth_inter          = (UCHAR)pSeqParamSet->max_transform_hierarchy_depth_inter;
    //pPicParam->num_short_term_ref_pic_sets                  = (UCHAR)pSeqParamSet->getRPSList()->getNumberOfReferencePictureSets();
    //pPicParam->num_long_term_ref_pics_sps                   = (UCHAR)pSeqParamSet->num_long_term_ref_pics_sps;
    //pPicParam->num_ref_idx_l0_default_active_minus1         = (UCHAR)(pPicParamSet->num_ref_idx_l0_default_active - 1);
    //pPicParam->num_ref_idx_l1_default_active_minus1         = (UCHAR)(pPicParamSet->num_ref_idx_l1_default_active - 1);
    //pPicParam->init_qp_minus26                              = (CHAR)pPicParamSet->init_qp - 26;

    //pPicParam->wNumBitsForShortTermRPSInSlice               = (USHORT)sliceHeader->wNumBitsForShortTermRPSInSlice;
    //pPicParam->ucNumDeltaPocsOfRefRpsIdx                    = (UCHAR)pPicParam->wNumBitsForShortTermRPSInSlice;

    //// dwCodingParamToolFlags
    ////
    //pPicParam->fields.scaling_list_enabled_flag                      = pSeqParamSet->scaling_list_enabled_flag;
    //pPicParam->fields.amp_enabled_flag                               = pSeqParamSet->amp_enabled_flag;
    //pPicParam->fields.sample_adaptive_offset_enabled_flag            = pSeqParamSet->sample_adaptive_offset_enabled_flag;
    //pPicParam->fields.pcm_enabled_flag                               = pSeqParamSet->pcm_enabled_flag;
    //pPicParam->fields.pcm_sample_bit_depth_luma_minus1               = (UCHAR)(pSeqParamSet->pcm_sample_bit_depth_luma - 1);
    //pPicParam->fields.pcm_sample_bit_depth_chroma_minus1             = (UCHAR)(pSeqParamSet->pcm_sample_bit_depth_chroma - 1);
    //pPicParam->fields.log2_min_pcm_luma_coding_block_size_minus3     = (UCHAR)(pSeqParamSet->log2_min_pcm_luma_coding_block_size - 3);
    //pPicParam->fields.log2_diff_max_min_pcm_luma_coding_block_size   = (UCHAR)(pSeqParamSet->log2_max_pcm_luma_coding_block_size - pSeqParamSet->log2_min_pcm_luma_coding_block_size);
    //pPicParam->fields.pcm_loop_filter_disabled_flag                  = pSeqParamSet->pcm_loop_filter_disabled_flag;
    //pPicParam->fields.long_term_ref_pics_present_flag                = pSeqParamSet->long_term_ref_pics_present_flag;
    //pPicParam->fields.sps_temporal_mvp_enabled_flag                  = pSeqParamSet->sps_temporal_mvp_enabled_flag;
    //pPicParam->fields.strong_intra_smoothing_enabled_flag            = pSeqParamSet->sps_strong_intra_smoothing_enabled_flag;
    //pPicParam->fields.dependent_slice_segments_enabled_flag          = pPicParamSet->dependent_slice_segments_enabled_flag;
    //pPicParam->fields.output_flag_present_flag                       = pPicParamSet->output_flag_present_flag;
    //pPicParam->fields.num_extra_slice_header_bits                    = (UCHAR)pPicParamSet->num_extra_slice_header_bits;
    //pPicParam->fields.sign_data_hiding_flag                          = pPicParamSet->sign_data_hiding_enabled_flag;
    //pPicParam->fields.cabac_init_present_flag                        = pPicParamSet->cabac_init_present_flag;

    //// PicShortFormatFlags
    ////
    //pPicParam->PicShortFormatFlags.fields.constrained_intra_pred_flag                   = pPicParamSet->constrained_intra_pred_flag;
    //pPicParam->PicShortFormatFlags.fields.transform_skip_enabled_flag                   = pPicParamSet->transform_skip_enabled_flag;
    //pPicParam->PicShortFormatFlags.fields.cu_qp_delta_enabled_flag                      = pPicParamSet->cu_qp_delta_enabled_flag;
    //pPicParam->PicShortFormatFlags.fields.pps_slice_chroma_qp_offsets_present_flag      = pPicParamSet->pps_slice_chroma_qp_offsets_present_flag;
    //pPicParam->PicShortFormatFlags.fields.weighted_pred_flag                            = pPicParamSet->weighted_pred_flag;
    //pPicParam->PicShortFormatFlags.fields.weighted_bipred_flag                          = pPicParamSet->weighted_bipred_flag;
    //pPicParam->PicShortFormatFlags.fields.transquant_bypass_enabled_flag                = pPicParamSet->transquant_bypass_enabled_flag;
    //pPicParam->PicShortFormatFlags.fields.tiles_enabled_flag                            = pPicParamSet->tiles_enabled_flag;
    //pPicParam->PicShortFormatFlags.fields.entropy_coding_sync_enabled_flag              = pPicParamSet->entropy_coding_sync_enabled_flag;
    //pPicParam->PicShortFormatFlags.fields.uniform_spacing_flag                          = pPicParamSet->uniform_spacing_flag;
    //pPicParam->PicShortFormatFlags.fields.loop_filter_across_tiles_enabled_flag         = pPicParamSet->loop_filter_across_tiles_enabled_flag;
    //pPicParam->PicShortFormatFlags.fields.pps_loop_filter_across_slices_enabled_flag    = pPicParamSet->pps_loop_filter_across_slices_enabled_flag;
    //pPicParam->PicShortFormatFlags.fields.deblocking_filter_override_enabled_flag       = pPicParamSet->deblocking_filter_override_enabled_flag;
    //pPicParam->PicShortFormatFlags.fields.pps_deblocking_filter_disabled_flag           = pPicParamSet->pps_deblocking_filter_disabled_flag;
    //pPicParam->PicShortFormatFlags.fields.lists_modification_present_flag               = pPicParamSet->lists_modification_present_flag;
    //pPicParam->PicShortFormatFlags.fields.slice_segment_header_extension_present_flag   = pPicParamSet->slice_segment_header_extension_present_flag;
    //pPicParam->PicShortFormatFlags.fields.IrapPicFlag                                   = (sliceHeader->nal_unit_type >= NAL_UT_CODED_SLICE_BLA_W_LP && sliceHeader->nal_unit_type <= NAL_UT_CODED_SLICE_CRA) ? 1 : 0;
    //pPicParam->PicShortFormatFlags.fields.IdrPicFlag                                    = sliceHeader->IdrPicFlag;
    //pPicParam->PicShortFormatFlags.fields.IntraPicFlag                                  = pSliceInfo->IsIntraAU() ? 1 : 0;

    //pPicParam->pps_cb_qp_offset                                     = (CHAR)pPicParamSet->pps_cb_qp_offset;
    //pPicParam->pps_cr_qp_offset                                     = (CHAR)pPicParamSet->pps_cr_qp_offset;
    //if(pPicParam->PicShortFormatFlags.fields.tiles_enabled_flag)
    //{
    //    pPicParam->num_tile_columns_minus1                          = (UCHAR)pPicParamSet->num_tile_columns - 1;
    //    pPicParam->num_tile_rows_minus1                             = (UCHAR)pPicParamSet->num_tile_rows - 1;

    //    pPicParam->num_tile_columns_minus1 = IPP_MIN(sizeof(pPicParam->column_width_minus1)/sizeof(pPicParam->column_width_minus1[0]) - 1, pPicParam->num_tile_columns_minus1);
    //    pPicParam->num_tile_rows_minus1 = IPP_MIN(sizeof(pPicParam->row_height_minus1)/sizeof(pPicParam->row_height_minus1[0]) - 1, pPicParam->num_tile_rows_minus1);

    //    for (Ipp32u i = 0; i <= pPicParam->num_tile_columns_minus1; i++)
    //        pPicParam->column_width_minus1[i] = (Ipp16u)(pPicParamSet->column_width[i] - 1);

    //    for (Ipp32u i = 0; i <= pPicParam->num_tile_rows_minus1; i++)
    //        pPicParam->row_height_minus1[i] = (Ipp16u)(pPicParamSet->row_height[i] - 1);
    //}

    //pPicParam->diff_cu_qp_delta_depth                               = (UCHAR)(pPicParamSet->diff_cu_qp_delta_depth);
    //pPicParam->pps_beta_offset_div2                                 = (CHAR)(pPicParamSet->pps_beta_offset >> 1);
    //pPicParam->pps_tc_offset_div2                                   = (CHAR)(pPicParamSet->pps_tc_offset >> 1);
    //pPicParam->log2_parallel_merge_level_minus2                     = (UCHAR)pPicParamSet->log2_parallel_merge_level - 2;
    //pPicParam->CurrPicOrderCntVal                                   = sliceHeader->slice_pic_order_cnt_lsb;


    ////
    ////
    //pPicParam->RefFieldPicFlag                              = 0;
    //pPicParam->RefBottomFieldFlag                           = 0;

    pPicParam->StatusReportFeedbackNumber = m_statusReportFeedbackCounter;
    pPicParam->TotalNumEntryPointOffsets = pSlice->m_WidevineStatusReportNumber;
}

#endif // UMC_VA_DXVA

#if defined(UMC_VA_LINUX)
/****************************************************************************************************/
// VA linux packer implementation
/****************************************************************************************************/
PackerVA::PackerVA(VideoAccelerator * va)
    : Packer(va)
{
}

Status PackerVA::GetStatusReport(void * , size_t )
{
    return UMC_OK;
}

void PackerVA::PackPicParams(const H265DecoderFrame *pCurrentFrame, H265DecoderFrameInfo * pSliceInfo, TaskSupplier_H265 *supplier)
{
    H265Slice * pSlice = pSliceInfo->GetSlice(0);
    const H265SliceHeader* sliceHeader = pSlice->GetSliceHeader();
    const H265SeqParamSet* pSeqParamSet = pSlice->GetSeqParam();
    const H265PicParamSet* pPicParamSet = pSlice->GetPicParam();

    UMCVACompBuffer *picParamBuf;
    VAPictureParameterBufferHEVC* picParam = (VAPictureParameterBufferHEVC*)m_va->GetCompBuffer(VAPictureParameterBufferType, &picParamBuf, sizeof(VAPictureParameterBufferHEVC));
    if (!picParam)
        throw h265_exception(UMC_ERR_FAILED);

    picParamBuf->SetDataSize(sizeof(VAPictureParameterBufferHEVC));
    memset(picParam, 0, sizeof(VAPictureParameterBufferHEVC));

    picParam->CurrPic.picture_id = m_va->GetSurfaceID(pCurrentFrame->m_index);
    picParam->CurrPic.pic_order_cnt = pCurrentFrame->m_PicOrderCnt;
    picParam->CurrPic.flags = 0;

    int count = 0;
    H265DBPList *dpb = supplier->GetDPBList();
    for(H265DecoderFrame* frame = dpb->head() ; frame && count < sizeof(picParam->ReferenceFrames)/sizeof(picParam->ReferenceFrames[0]) ; frame = frame->future())
    {
        if (frame == pCurrentFrame)
            continue;

        int refType = frame->isShortTermRef() ? SHORT_TERM_REFERENCE : (frame->isLongTermRef() ? LONG_TERM_REFERENCE : NO_REFERENCE);

        if (refType != NO_REFERENCE)
        {
            picParam->ReferenceFrames[count].pic_order_cnt = frame->m_PicOrderCnt;
            picParam->ReferenceFrames[count].picture_id = m_va->GetSurfaceID(frame->m_index);;
            picParam->ReferenceFrames[count].flags = refType == LONG_TERM_REFERENCE ? VA_PICTURE_HEVC_LONG_TERM_REFERENCE : 0;
            count++;
        }
    }

    for (Ipp32u n = count;n < sizeof(picParam->ReferenceFrames)/sizeof(picParam->ReferenceFrames[0]); n++)
    {
        picParam->ReferenceFrames[n].picture_id = VA_INVALID_SURFACE;
        picParam->ReferenceFrames[n].flags = VA_PICTURE_HEVC_INVALID;
    }

    ReferencePictureSet *rps = pSlice->getRPS();
    Ipp32s index, pocList[3*8];
    Ipp32s numRefPicSetStCurrBefore = 0,
        numRefPicSetStCurrAfter  = 0,
        numRefPicSetLtCurr       = 0;
    for(index = 0; index < rps->getNumberOfNegativePictures(); index++)
            pocList[numRefPicSetStCurrBefore++] = picParam->CurrPic.pic_order_cnt + rps->getDeltaPOC(index);

    for(; index < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures(); index++)
            pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter++] = picParam->CurrPic.pic_order_cnt + rps->getDeltaPOC(index);

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
            pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr++] = picParam->CurrPic.pic_order_cnt + rps->getDeltaPOC(index);
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
            if(pocList[n] == picParam->ReferenceFrames[k].pic_order_cnt)
            {
                if(n < numRefPicSetStCurrBefore)
                    picParam->ReferenceFrames[k].flags |= VA_PICTURE_HEVC_RPS_ST_CURR_BEFORE;
                else if(n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter)
                    picParam->ReferenceFrames[k].flags |= VA_PICTURE_HEVC_RPS_ST_CURR_AFTER;
                else if(n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr)
                    picParam->ReferenceFrames[k].flags |= VA_PICTURE_HEVC_RPS_LT_CURR;
            }
        }
    }

    picParam->pic_width_in_luma_samples = (uint16_t)pSeqParamSet->pic_width_in_luma_samples;
    picParam->pic_height_in_luma_samples = (uint16_t)pSeqParamSet->pic_height_in_luma_samples;

    picParam->pic_fields.bits.chroma_format_idc = pSeqParamSet->chroma_format_idc;
    picParam->pic_fields.bits.separate_colour_plane_flag = pSeqParamSet->separate_colour_plane_flag;

    picParam->pic_fields.bits.pcm_enabled_flag = pSeqParamSet->pcm_enabled_flag;
    picParam->pic_fields.bits.scaling_list_enabled_flag = pSeqParamSet->scaling_list_enabled_flag;
    picParam->pic_fields.bits.transform_skip_enabled_flag = pPicParamSet->transform_skip_enabled_flag;
    picParam->pic_fields.bits.amp_enabled_flag = pSeqParamSet->amp_enabled_flag;
    picParam->pic_fields.bits.strong_intra_smoothing_enabled_flag = pSeqParamSet->sps_strong_intra_smoothing_enabled_flag;

    picParam->pic_fields.bits.sign_data_hiding_enabled_flag = pPicParamSet->sign_data_hiding_enabled_flag;
    picParam->pic_fields.bits.constrained_intra_pred_flag = pPicParamSet->constrained_intra_pred_flag;
    picParam->pic_fields.bits.cu_qp_delta_enabled_flag = pPicParamSet->cu_qp_delta_enabled_flag;
    picParam->pic_fields.bits.weighted_pred_flag = pPicParamSet->weighted_pred_flag;
    picParam->pic_fields.bits.weighted_bipred_flag = pPicParamSet->weighted_bipred_flag;

    picParam->pic_fields.bits.transquant_bypass_enabled_flag = pPicParamSet->transquant_bypass_enabled_flag;
    picParam->pic_fields.bits.tiles_enabled_flag = pPicParamSet->tiles_enabled_flag;
    picParam->pic_fields.bits.entropy_coding_sync_enabled_flag = pPicParamSet->entropy_coding_sync_enabled_flag;
    picParam->pic_fields.bits.pps_loop_filter_across_slices_enabled_flag = pPicParamSet->pps_loop_filter_across_slices_enabled_flag;
    picParam->pic_fields.bits.loop_filter_across_tiles_enabled_flag = pPicParamSet->loop_filter_across_tiles_enabled_flag;

    picParam->pic_fields.bits.pcm_loop_filter_disabled_flag = pSeqParamSet->pcm_loop_filter_disabled_flag;
    picParam->pic_fields.bits.NoPicReorderingFlag = pSeqParamSet->sps_max_num_reorder_pics == 0 ? 1 : 0;
    picParam->pic_fields.bits.NoBiPredFlag = 0;

    picParam->sps_max_dec_pic_buffering_minus1 = (uint8_t)(pSeqParamSet->sps_max_dec_pic_buffering[pSlice->GetSliceHeader()->nuh_temporal_id] - 1);
    picParam->bit_depth_luma_minus8 = (uint8_t)(pSeqParamSet->bit_depth_luma - 8);
    picParam->bit_depth_chroma_minus8 = (uint8_t)(pSeqParamSet->bit_depth_chroma - 8);
    picParam->pcm_sample_bit_depth_luma_minus1 = (uint8_t)(pSeqParamSet->pcm_sample_bit_depth_luma - 1);
    picParam->pcm_sample_bit_depth_chroma_minus1 = (uint8_t)(pSeqParamSet->pcm_sample_bit_depth_chroma - 1);
    picParam->log2_min_luma_coding_block_size_minus3 = (uint8_t)(pSeqParamSet->log2_min_luma_coding_block_size- 3);
    picParam->log2_diff_max_min_luma_coding_block_size = (uint8_t)(pSeqParamSet->log2_max_luma_coding_block_size - pSeqParamSet->log2_min_luma_coding_block_size);
    picParam->log2_min_transform_block_size_minus2 = (uint8_t)(pSeqParamSet->log2_min_transform_block_size - 2);
    picParam->log2_diff_max_min_transform_block_size = (uint8_t)(pSeqParamSet->log2_max_transform_block_size - pSeqParamSet->log2_min_transform_block_size);
    picParam->log2_min_pcm_luma_coding_block_size_minus3 = (uint8_t)(pSeqParamSet->log2_min_pcm_luma_coding_block_size - 3);
    picParam->log2_diff_max_min_pcm_luma_coding_block_size = (uint8_t)(pSeqParamSet->log2_max_pcm_luma_coding_block_size - pSeqParamSet->log2_min_pcm_luma_coding_block_size);
    picParam->max_transform_hierarchy_depth_intra = (uint8_t)pSeqParamSet->max_transform_hierarchy_depth_intra;
    picParam->max_transform_hierarchy_depth_inter = (uint8_t)pSeqParamSet->max_transform_hierarchy_depth_inter;
    picParam->init_qp_minus26 = pPicParamSet->init_qp - 26;
    picParam->diff_cu_qp_delta_depth = (uint8_t)pPicParamSet->diff_cu_qp_delta_depth;
    picParam->pps_cb_qp_offset = (uint8_t)pPicParamSet->pps_cb_qp_offset;
    picParam->pps_cr_qp_offset = (uint8_t)pPicParamSet->pps_cr_qp_offset;
    picParam->log2_parallel_merge_level_minus2 = (uint8_t)(pPicParamSet->log2_parallel_merge_level - 2);

    if (pPicParamSet->tiles_enabled_flag)
    {
        picParam->num_tile_columns_minus1 = (uint8_t)(pPicParamSet->num_tile_columns - 1);
        picParam->num_tile_rows_minus1 = (uint8_t)(pPicParamSet->num_tile_rows - 1);

        picParam->num_tile_columns_minus1 = IPP_MIN(sizeof(picParam->column_width_minus1)/sizeof(picParam->column_width_minus1[0]) - 1, picParam->num_tile_columns_minus1);
        picParam->num_tile_rows_minus1 = IPP_MIN(sizeof(picParam->row_height_minus1)/sizeof(picParam->row_height_minus1[0]) - 1, picParam->num_tile_rows_minus1);

        for (Ipp32u i = 0; i <= picParam->num_tile_columns_minus1; i++)
            picParam->column_width_minus1[i] = (Ipp16u)(pPicParamSet->column_width[i] - 1);

        for (Ipp32u i = 0; i <= picParam->num_tile_rows_minus1; i++)
            picParam->row_height_minus1[i] = (Ipp16u)(pPicParamSet->row_height[i] - 1);
    }

    picParam->slice_parsing_fields.bits.lists_modification_present_flag = pPicParamSet->lists_modification_present_flag;
    picParam->slice_parsing_fields.bits.long_term_ref_pics_present_flag = pSeqParamSet->long_term_ref_pics_present_flag;
    picParam->slice_parsing_fields.bits.sps_temporal_mvp_enabled_flag = pSeqParamSet->sps_temporal_mvp_enabled_flag;
    picParam->slice_parsing_fields.bits.cabac_init_present_flag = pPicParamSet->cabac_init_present_flag;
    picParam->slice_parsing_fields.bits.output_flag_present_flag = pPicParamSet->output_flag_present_flag;
    picParam->slice_parsing_fields.bits.dependent_slice_segments_enabled_flag = pPicParamSet->dependent_slice_segments_enabled_flag;
    picParam->slice_parsing_fields.bits.pps_slice_chroma_qp_offsets_present_flag = pPicParamSet->pps_slice_chroma_qp_offsets_present_flag;
    picParam->slice_parsing_fields.bits.sample_adaptive_offset_enabled_flag = pSeqParamSet->sample_adaptive_offset_enabled_flag;
    picParam->slice_parsing_fields.bits.deblocking_filter_override_enabled_flag = pPicParamSet->deblocking_filter_override_enabled_flag;
    picParam->slice_parsing_fields.bits.pps_disable_deblocking_filter_flag = pPicParamSet->pps_deblocking_filter_disabled_flag;
    picParam->slice_parsing_fields.bits.slice_segment_header_extension_present_flag = pPicParamSet->slice_segment_header_extension_present_flag;

    picParam->slice_parsing_fields.bits.RapPicFlag = (sliceHeader->nal_unit_type >= NAL_UT_CODED_SLICE_BLA_W_LP && sliceHeader->nal_unit_type <= NAL_UT_CODED_SLICE_CRA) ? 1 : 0;
    picParam->slice_parsing_fields.bits.IdrPicFlag = sliceHeader->IdrPicFlag;
    picParam->slice_parsing_fields.bits.IntraPicFlag = pSliceInfo->IsIntraAU() ? 1 : 0;

    picParam->log2_max_pic_order_cnt_lsb_minus4 = (uint8_t)(pSeqParamSet->log2_max_pic_order_cnt_lsb - 4);
    picParam->num_short_term_ref_pic_sets = (uint8_t)pSeqParamSet->getRPSList()->getNumberOfReferencePictureSets();
    picParam->num_long_term_ref_pic_sps = (uint8_t)pSeqParamSet->num_long_term_ref_pics_sps;
    picParam->num_ref_idx_l0_default_active_minus1 = (uint8_t)(pPicParamSet->num_ref_idx_l0_default_active - 1);
    picParam->num_ref_idx_l1_default_active_minus1 = (uint8_t)(pPicParamSet->num_ref_idx_l1_default_active - 1);
    picParam->pps_beta_offset_div2 = (int8_t)(pPicParamSet->pps_beta_offset >> 1);
    picParam->pps_tc_offset_div2 = (int8_t)(pPicParamSet->pps_tc_offset >> 1);
    picParam->num_extra_slice_header_bits = (uint8_t)pPicParamSet->num_extra_slice_header_bits;

    picParam->st_rps_bits = pSlice->GetSliceHeader()->wNumBitsForShortTermRPSInSlice;
}

void PackerVA::CreateSliceParamBuffer(H265DecoderFrameInfo * sliceInfo)
{
    Ipp32s count = sliceInfo->GetSliceCount();

    UMCVACompBuffer *pSliceParamBuf;
    size_t sizeOfStruct = m_va->IsLongSliceControl() ? sizeof(VASliceParameterBufferHEVC) : sizeof(VASliceParameterBufferBaseHEVC);
    m_va->GetCompBuffer(VASliceParameterBufferType, &pSliceParamBuf, sizeOfStruct*(count));
    if (!pSliceParamBuf)
        throw h265_exception(UMC_ERR_FAILED);

    pSliceParamBuf->SetNumOfItem(count);
}

void PackerVA::CreateSliceDataBuffer(H265DecoderFrameInfo * sliceInfo)
{
    Ipp32s count = sliceInfo->GetSliceCount();

    Ipp32s size = 0;
    Ipp32s AlignedNalUnitSize = 0;

    for (Ipp32s i = 0; i < count; i++)
    {
        H265Slice  * pSlice = sliceInfo->GetSlice(i);

        Ipp8u *pNalUnit; //ptr to first byte of start code
        Ipp32u NalUnitSize; // size of NAL unit in byte
        H265Bitstream *pBitstream = pSlice->GetBitStream();

        pBitstream->GetOrg((Ipp32u**)&pNalUnit, &NalUnitSize);
        size += NalUnitSize + 3;
    }

    AlignedNalUnitSize = align_value<Ipp32s>(size, 128);

    UMCVACompBuffer* compBuf;
    m_va->GetCompBuffer(VASliceDataBufferType, &compBuf, AlignedNalUnitSize);
    if (!compBuf)
        throw h265_exception(UMC_ERR_FAILED);

    memset((Ipp8u*)compBuf->GetPtr() + size, 0, AlignedNalUnitSize - size);

    compBuf->SetDataSize(0);
}

bool PackerVA::PackSliceParams(H265Slice *pSlice, Ipp32u &sliceNum, bool isLastSlice)
{
    static Ipp8u start_code_prefix[] = {0, 0, 1};

    H265DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();
    const H265SliceHeader *sliceHeader = pSlice->GetSliceHeader();
    const H265PicParamSet *pPicParamSet = pSlice->GetPicParam();

    VAPictureParameterBufferHEVC* picParams = (VAPictureParameterBufferHEVC*)m_va->GetCompBuffer(VAPictureParameterBufferType);
    if (!picParams)
        throw h265_exception(UMC_ERR_FAILED);

    UMCVACompBuffer* compBuf;
    VASliceParameterBufferHEVC* sliceParams = (VASliceParameterBufferHEVC*)m_va->GetCompBuffer(VASliceParameterBufferType, &compBuf);
    if (!sliceParams)
        throw h265_exception(UMC_ERR_FAILED);

    if (m_va->IsLongSliceControl())
    {
        sliceParams += sliceNum;
        memset(sliceParams, 0, sizeof(VASliceParameterBufferHEVC));
    }
    else
    {
        sliceParams = (VASliceParameterBufferHEVC*)((VASliceParameterBufferBaseHEVC*)sliceParams + sliceNum);
        memset(sliceParams, 0, sizeof(VASliceParameterBufferBaseHEVC));
    }

    Ipp32u  rawDataSize = 0;
    const void*   rawDataPtr = 0;

    pSlice->m_BitStream.GetOrg((Ipp32u**)&rawDataPtr, &rawDataSize);

    sliceParams->slice_data_size = rawDataSize + sizeof(start_code_prefix);
    sliceParams->slice_data_offset = compBuf->GetDataSize();
    sliceParams->slice_data_flag = VA_SLICE_DATA_FLAG_ALL;//chopping == CHOPPING_NONE ? VA_SLICE_DATA_FLAG_ALL : VA_SLICE_DATA_FLAG_END;;

    compBuf->SetDataSize(sliceParams->slice_data_offset + sliceParams->slice_data_size);

    Ipp8u *sliceDataBuf = (Ipp8u*)m_va->GetCompBuffer(VASliceDataBufferType, &compBuf);
    if (!sliceDataBuf)
        throw h265_exception(UMC_ERR_FAILED);

    sliceDataBuf += sliceParams->slice_data_offset;
    MFX_INTERNAL_CPY(sliceDataBuf, start_code_prefix, sizeof(start_code_prefix));
    MFX_INTERNAL_CPY(sliceDataBuf + sizeof(start_code_prefix), rawDataPtr, rawDataSize);

    if (!m_va->IsLongSliceControl())
        return true;

    sliceParams->slice_data_byte_offset = pSlice->m_BitStream.BytesDecoded() + sizeof(start_code_prefix);
    sliceParams->slice_segment_address = sliceHeader->slice_segment_address;

    for(Ipp32s iDir = 0; iDir < 2; iDir++)
    {
        Ipp32s index = 0;
        const H265DecoderRefPicList::ReferenceInformation* pRefPicList = pCurrentFrame->GetRefPicList(pSlice->GetSliceNum(), iDir)->m_refPicList;

        EnumRefPicList eRefPicList = ( iDir == 1 ? REF_PIC_LIST_1 : REF_PIC_LIST_0 );
        for (Ipp32s i = 0; i < pSlice->getNumRefIdx(eRefPicList); i++)
        {
            const H265DecoderRefPicList::ReferenceInformation &frameInfo = pRefPicList[i];
            if (frameInfo.refFrame)
            {
                bool isFound = false;
                for (uint8_t j = 0; j < sizeof(picParams->ReferenceFrames)/sizeof(picParams->ReferenceFrames[0]); j++)
                {
                    if (picParams->ReferenceFrames[j].picture_id == m_va->GetSurfaceID(frameInfo.refFrame->m_index))
                    {
                        sliceParams->RefPicList[iDir][index] = j;
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

        for(;index < sizeof(sliceParams->RefPicList[0])/sizeof(sliceParams->RefPicList[0][0]); index++)
            sliceParams->RefPicList[iDir][index] = 0xff;
    }

    sliceParams->LongSliceFlags.fields.LastSliceOfPic = isLastSlice ? 1 : 0;
    sliceParams->LongSliceFlags.fields.dependent_slice_segment_flag = sliceHeader->dependent_slice_segment_flag;
    sliceParams->LongSliceFlags.fields.slice_type = sliceHeader->slice_type;
    sliceParams->LongSliceFlags.fields.color_plane_id = sliceHeader->colour_plane_id;
    sliceParams->LongSliceFlags.fields.slice_sao_luma_flag = sliceHeader->slice_sao_luma_flag;
    sliceParams->LongSliceFlags.fields.slice_sao_chroma_flag = sliceHeader->slice_sao_chroma_flag;
    sliceParams->LongSliceFlags.fields.mvd_l1_zero_flag = sliceHeader->mvd_l1_zero_flag;
    sliceParams->LongSliceFlags.fields.cabac_init_flag = sliceHeader->cabac_init_flag;
    sliceParams->LongSliceFlags.fields.slice_temporal_mvp_enabled_flag = sliceHeader->slice_temporal_mvp_enabled_flag;
    sliceParams->LongSliceFlags.fields.slice_deblocking_filter_disabled_flag = sliceHeader->slice_deblocking_filter_disabled_flag;
    sliceParams->LongSliceFlags.fields.collocated_from_l0_flag = sliceHeader->collocated_from_l0_flag;
    sliceParams->LongSliceFlags.fields.slice_loop_filter_across_slices_enabled_flag = sliceHeader->slice_loop_filter_across_slices_enabled_flag;

    sliceParams->collocated_ref_idx = (uint8_t)((sliceHeader->slice_type != I_SLICE) ?  sliceHeader->collocated_ref_idx : -1);
    sliceParams->num_ref_idx_l0_active_minus1 = (uint8_t)(pSlice->getNumRefIdx(REF_PIC_LIST_0) - 1);
    sliceParams->num_ref_idx_l1_active_minus1 = (uint8_t)(pSlice->getNumRefIdx(REF_PIC_LIST_1) - 1);
    sliceParams->slice_qp_delta = (int8_t)(sliceHeader->SliceQP - pPicParamSet->init_qp);
    sliceParams->slice_cb_qp_offset = (int8_t)sliceHeader->slice_cb_qp_offset;
    sliceParams->slice_cr_qp_offset = (int8_t)sliceHeader->slice_cr_qp_offset;
    sliceParams->slice_beta_offset_div2 = (int8_t)(sliceHeader->slice_beta_offset >> 1);
    sliceParams->slice_tc_offset_div2 = (int8_t)(sliceHeader->slice_tc_offset >> 1);

    sliceParams->luma_log2_weight_denom = (uint8_t)sliceHeader->luma_log2_weight_denom;
    sliceParams->delta_chroma_log2_weight_denom = (uint8_t)(sliceHeader->chroma_log2_weight_denom - sliceHeader->luma_log2_weight_denom);

    for (Ipp32s l = 0; l < 2; l++)
    {
        const wpScalingParam *wp;
        EnumRefPicList eRefPicList = ( l == 1 ? REF_PIC_LIST_1 : REF_PIC_LIST_0 );
        for (Ipp32s iRefIdx = 0; iRefIdx < pSlice->getNumRefIdx(eRefPicList); iRefIdx++)
        {
            wp = sliceHeader->pred_weight_table[eRefPicList][iRefIdx];

            if (eRefPicList == REF_PIC_LIST_0)
            {
                sliceParams->luma_offset_l0[iRefIdx]       = (int8_t)wp[0].offset;
                sliceParams->delta_luma_weight_l0[iRefIdx] = (int8_t)wp[0].delta_weight;
                for(int chroma=0;chroma < 2;chroma++)
                {
                    sliceParams->delta_chroma_weight_l0[iRefIdx][chroma] = (int8_t)wp[1 + chroma].delta_weight;
                    sliceParams->ChromaOffsetL0        [iRefIdx][chroma] = (int8_t)wp[1 + chroma].offset;
                }
            }
            else
            {
                sliceParams->luma_offset_l1[iRefIdx]       = (int8_t)wp[0].offset;
                sliceParams->delta_luma_weight_l1[iRefIdx] = (int8_t)wp[0].delta_weight;
                for(int chroma=0;chroma < 2;chroma++)
                {
                    sliceParams->delta_chroma_weight_l1[iRefIdx][chroma] = (int8_t)wp[1 + chroma].delta_weight;
                    sliceParams->ChromaOffsetL1        [iRefIdx][chroma] = (int8_t)wp[1 + chroma].offset;
                }
            }
        }
    }

    sliceParams->five_minus_max_num_merge_cand = (uint8_t)(5 - sliceHeader->max_num_merge_cand);
    return true;
}

void PackerVA::PackQmatrix(const H265Slice *pSlice)
{
    UMCVACompBuffer *quantBuf;
    VAIQMatrixBufferHEVC* qmatrix = (VAIQMatrixBufferHEVC*)m_va->GetCompBuffer(VAIQMatrixBufferType, &quantBuf, sizeof(VAIQMatrixBufferHEVC));
    if (!qmatrix)
        throw h265_exception(UMC_ERR_FAILED);
    quantBuf->SetDataSize(sizeof(VAIQMatrixBufferHEVC));

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
                    int count = IPP_MIN(MAX_MATRIX_COEF_NUM, (Ipp32s)g_scalingListSize[sizeId]);
                    ::MFX_INTERNAL_CPY(dst, src, sizeof(Ipp32s) * count);
                    sl.setScalingListDC(sizeId, listId, SCALING_LIST_DC);
                }
            }
            doInit = false;
        }

        scalingList = &sl;
    }

    initQMatrix<16>(scalingList, SCALING_LIST_4x4,   qmatrix->ScalingList4x4);    // 4x4
    initQMatrix<64>(scalingList, SCALING_LIST_8x8,   qmatrix->ScalingList8x8);    // 8x8
    initQMatrix<64>(scalingList, SCALING_LIST_16x16, qmatrix->ScalingList16x16);    // 16x16
    initQMatrix(scalingList, SCALING_LIST_32x32, qmatrix->ScalingList32x32);    // 32x32

    for(int sizeId = SCALING_LIST_16x16; sizeId <= SCALING_LIST_32x32; sizeId++)
    {
        for(unsigned listId = 0; listId <  g_scalingListNum[sizeId]; listId++)
        {
            if(sizeId == SCALING_LIST_16x16)
                qmatrix->ScalingListDC16x16[listId] = (uint8_t)scalingList->getScalingListDC(sizeId, listId);
            else if(sizeId == SCALING_LIST_32x32)
                qmatrix->ScalingListDC32x32[listId] = (uint8_t)scalingList->getScalingListDC(sizeId, listId);
        }
    }
}

void PackerVA::PackAU(const H265DecoderFrame *frame, TaskSupplier_H265 * supplier)
{
    H265DecoderFrameInfo * sliceInfo = frame->m_pSlicesInfo;
    int sliceCount = sliceInfo->GetSliceCount();

    if (!sliceCount)
        return;

    H265Slice *pSlice = sliceInfo->GetSlice(0);
    const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
    H265DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();

    PackPicParams(pCurrentFrame, sliceInfo, supplier);

    if (pSeqParamSet->scaling_list_enabled_flag)
    {
        PackQmatrix(pSlice);
    }

    CreateSliceParamBuffer(sliceInfo);
    CreateSliceDataBuffer(sliceInfo);

    Ipp32u sliceNum = 0;
    for (Ipp32s n = 0; n < sliceCount; n++)
    {
        PackSliceParams(sliceInfo->GetSlice(n), sliceNum, n == sliceCount - 1);
        sliceNum++;
    }

    Status s = m_va->Execute();
    if(s != UMC_OK)
        throw h265_exception(s);
}

void PackerVA::BeginFrame()
{
}

void PackerVA::EndFrame()
{
}

/****************************************************************************************************/
// PAVP Widevine HuC-based implementation
/****************************************************************************************************/

PackerVA_Widevine::PackerVA_Widevine(VideoAccelerator * va)
    : PackerVA(va)
{
}

void PackerVA_Widevine::PackPicParams(const H265DecoderFrame *pCurrentFrame, H265DecoderFrameInfo * pSliceInfo, TaskSupplier_H265 *supplier)
{
    H265Slice * pSlice = pSliceInfo->GetSlice(0);
    const H265SliceHeader* sliceHeader = pSlice->GetSliceHeader();
    const H265SeqParamSet* pSeqParamSet = pSlice->GetSeqParam();
    const H265PicParamSet* pPicParamSet = pSlice->GetPicParam();

    UMCVACompBuffer *picParamBuf;
    VAPictureParameterBufferHEVC* picParam = (VAPictureParameterBufferHEVC*)m_va->GetCompBuffer(VAPictureParameterBufferType, &picParamBuf, sizeof(VAPictureParameterBufferHEVC));
    if (!picParam)
        throw h265_exception(UMC_ERR_FAILED);

    picParamBuf->SetDataSize(sizeof(VAPictureParameterBufferHEVC));
    memset(picParam, 0, sizeof(VAPictureParameterBufferHEVC));

    picParam->CurrPic.picture_id = m_va->GetSurfaceID(pCurrentFrame->m_index);
    picParam->CurrPic.pic_order_cnt = pCurrentFrame->m_PicOrderCnt;
    picParam->CurrPic.flags = 0;

    int count = 0;
    H265DBPList *dpb = supplier->GetDPBList();
    for(H265DecoderFrame* frame = dpb->head() ; frame && count < sizeof(picParam->ReferenceFrames)/sizeof(picParam->ReferenceFrames[0]) ; frame = frame->future())
    {
        if (frame == pCurrentFrame)
            continue;

        int refType = frame->isShortTermRef() ? SHORT_TERM_REFERENCE : (frame->isLongTermRef() ? LONG_TERM_REFERENCE : NO_REFERENCE);

        if (refType != NO_REFERENCE)
        {
            picParam->ReferenceFrames[count].pic_order_cnt = frame->m_PicOrderCnt;
            picParam->ReferenceFrames[count].picture_id = m_va->GetSurfaceID(frame->m_index);;
            picParam->ReferenceFrames[count].flags = refType == LONG_TERM_REFERENCE ? VA_PICTURE_HEVC_LONG_TERM_REFERENCE : 0;
            count++;
        }
    }

    for (Ipp32u n = count;n < sizeof(picParam->ReferenceFrames)/sizeof(picParam->ReferenceFrames[0]); n++)
    {
        picParam->ReferenceFrames[n].picture_id = VA_INVALID_SURFACE;
        picParam->ReferenceFrames[n].flags = VA_PICTURE_HEVC_INVALID;
    }

    ReferencePictureSet *rps = pSlice->getRPS();
    Ipp32s index, pocList[3*8];
    Ipp32s numRefPicSetStCurrBefore = 0,
        numRefPicSetStCurrAfter  = 0,
        numRefPicSetLtCurr       = 0;
    for(index = 0; index < rps->getNumberOfNegativePictures(); index++)
            pocList[numRefPicSetStCurrBefore++] = picParam->CurrPic.pic_order_cnt + rps->getDeltaPOC(index);

    for(; index < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures(); index++)
            pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter++] = picParam->CurrPic.pic_order_cnt + rps->getDeltaPOC(index);

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
            pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr++] = picParam->CurrPic.pic_order_cnt + rps->getDeltaPOC(index);
        }
    }

    Ipp32s i = 0;
    VAPictureHEVC sortedReferenceFrames[15];
    memset(sortedReferenceFrames, 0, sizeof(sortedReferenceFrames));

    for(Ipp32s n=0 ; n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr ; n++)
    {
        if (!rps->getUsed(n))
            continue;

        for(Ipp32s k=0;k < count;k++)
        {
            if(pocList[n] == picParam->ReferenceFrames[k].pic_order_cnt)
            {
                if(n < numRefPicSetStCurrBefore)
                {
                    picParam->ReferenceFrames[k].flags |= VA_PICTURE_HEVC_RPS_ST_CURR_BEFORE;
                    sortedReferenceFrames[i++] = picParam->ReferenceFrames[k];
                }
                else if(n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter)
                {
                    picParam->ReferenceFrames[k].flags |= VA_PICTURE_HEVC_RPS_ST_CURR_AFTER;
                    sortedReferenceFrames[i++] = picParam->ReferenceFrames[k];
                }
                else if(n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr)
                {
                    picParam->ReferenceFrames[k].flags |= VA_PICTURE_HEVC_RPS_LT_CURR;
                    sortedReferenceFrames[i++] = picParam->ReferenceFrames[k];
                }
            }
        }
    }

    for (Ipp32u n = count;n < sizeof(sortedReferenceFrames)/sizeof(sortedReferenceFrames[0]); n++)
    {
        sortedReferenceFrames[n].picture_id = VA_INVALID_SURFACE;
        sortedReferenceFrames[n].flags = VA_PICTURE_HEVC_INVALID;
    }

    MFX_INTERNAL_CPY(picParam->ReferenceFrames, sortedReferenceFrames, sizeof(sortedReferenceFrames));

    //picParam->pic_width_in_luma_samples = (uint16_t)pSeqParamSet->pic_width_in_luma_samples;
    //picParam->pic_height_in_luma_samples = (uint16_t)pSeqParamSet->pic_height_in_luma_samples;

    //picParam->pic_fields.bits.chroma_format_idc = pSeqParamSet->chroma_format_idc;
    //picParam->pic_fields.bits.separate_colour_plane_flag = pSeqParamSet->separate_colour_plane_flag;

    //picParam->pic_fields.bits.pcm_enabled_flag = pSeqParamSet->pcm_enabled_flag;
    //picParam->pic_fields.bits.scaling_list_enabled_flag = pSeqParamSet->scaling_list_enabled_flag;
    //picParam->pic_fields.bits.transform_skip_enabled_flag = pPicParamSet->transform_skip_enabled_flag;
    //picParam->pic_fields.bits.amp_enabled_flag = pSeqParamSet->amp_enabled_flag;
    //picParam->pic_fields.bits.strong_intra_smoothing_enabled_flag = pSeqParamSet->sps_strong_intra_smoothing_enabled_flag;

    //picParam->pic_fields.bits.sign_data_hiding_enabled_flag = pPicParamSet->sign_data_hiding_enabled_flag;
    //picParam->pic_fields.bits.constrained_intra_pred_flag = pPicParamSet->constrained_intra_pred_flag;
    //picParam->pic_fields.bits.cu_qp_delta_enabled_flag = pPicParamSet->cu_qp_delta_enabled_flag;
    //picParam->pic_fields.bits.weighted_pred_flag = pPicParamSet->weighted_pred_flag;
    //picParam->pic_fields.bits.weighted_bipred_flag = pPicParamSet->weighted_bipred_flag;

    //picParam->pic_fields.bits.transquant_bypass_enabled_flag = pPicParamSet->transquant_bypass_enabled_flag;
    //picParam->pic_fields.bits.tiles_enabled_flag = pPicParamSet->tiles_enabled_flag;
    //picParam->pic_fields.bits.entropy_coding_sync_enabled_flag = pPicParamSet->entropy_coding_sync_enabled_flag;
    //picParam->pic_fields.bits.pps_loop_filter_across_slices_enabled_flag = pPicParamSet->pps_loop_filter_across_slices_enabled_flag;
    //picParam->pic_fields.bits.loop_filter_across_tiles_enabled_flag = pPicParamSet->loop_filter_across_tiles_enabled_flag;

    //picParam->pic_fields.bits.pcm_loop_filter_disabled_flag = pSeqParamSet->pcm_loop_filter_disabled_flag;
    //picParam->pic_fields.bits.NoPicReorderingFlag = pSeqParamSet->sps_max_num_reorder_pics == 0 ? 1 : 0;
    //picParam->pic_fields.bits.NoBiPredFlag = 0;

    //picParam->sps_max_dec_pic_buffering_minus1 = (uint8_t)(pSeqParamSet->sps_max_dec_pic_buffering[pSlice->GetSliceHeader()->nuh_temporal_id] - 1);
    //picParam->bit_depth_luma_minus8 = (uint8_t)(pSeqParamSet->bit_depth_luma - 8);
    //picParam->bit_depth_chroma_minus8 = (uint8_t)(pSeqParamSet->bit_depth_chroma - 8);
    //picParam->pcm_sample_bit_depth_luma_minus1 = (uint8_t)(pSeqParamSet->pcm_sample_bit_depth_luma - 1);
    //picParam->pcm_sample_bit_depth_chroma_minus1 = (uint8_t)(pSeqParamSet->pcm_sample_bit_depth_chroma - 1);
    //picParam->log2_min_luma_coding_block_size_minus3 = (uint8_t)(pSeqParamSet->log2_min_luma_coding_block_size- 3);
    //picParam->log2_diff_max_min_luma_coding_block_size = (uint8_t)(pSeqParamSet->log2_max_luma_coding_block_size - pSeqParamSet->log2_min_luma_coding_block_size);
    //picParam->log2_min_transform_block_size_minus2 = (uint8_t)(pSeqParamSet->log2_min_transform_block_size - 2);
    //picParam->log2_diff_max_min_transform_block_size = (uint8_t)(pSeqParamSet->log2_max_transform_block_size - pSeqParamSet->log2_min_transform_block_size);
    //picParam->log2_min_pcm_luma_coding_block_size_minus3 = (uint8_t)(pSeqParamSet->log2_min_pcm_luma_coding_block_size - 3);
    ////picParam->log2_diff_max_min_pcm_luma_coding_block_size = (uint8_t)(pSeqParamSet->log2_max_pcm_luma_coding_block_size - pSeqParamSet->log2_min_pcm_luma_coding_block_size);
    //picParam->max_transform_hierarchy_depth_intra = (uint8_t)pSeqParamSet->max_transform_hierarchy_depth_intra;
    //picParam->max_transform_hierarchy_depth_inter = (uint8_t)pSeqParamSet->max_transform_hierarchy_depth_inter;
    //picParam->init_qp_minus26 = pPicParamSet->init_qp - 26;
    //picParam->diff_cu_qp_delta_depth = (uint8_t)pPicParamSet->diff_cu_qp_delta_depth;
    //picParam->pps_cb_qp_offset = (uint8_t)pPicParamSet->pps_cb_qp_offset;
    //picParam->pps_cr_qp_offset = (uint8_t)pPicParamSet->pps_cr_qp_offset;
    //picParam->log2_parallel_merge_level_minus2 = (uint8_t)(pPicParamSet->log2_parallel_merge_level - 2);

    //if (pPicParamSet->tiles_enabled_flag)
    //{
    //    picParam->num_tile_columns_minus1 = (uint8_t)(pPicParamSet->num_tile_columns - 1);
    //    picParam->num_tile_rows_minus1 = (uint8_t)(pPicParamSet->num_tile_rows - 1);

    //    picParam->num_tile_columns_minus1 = IPP_MIN(sizeof(picParam->column_width_minus1)/sizeof(picParam->column_width_minus1[0]) - 1, picParam->num_tile_columns_minus1);
    //    picParam->num_tile_rows_minus1 = IPP_MIN(sizeof(picParam->row_height_minus1)/sizeof(picParam->row_height_minus1[0]) - 1, picParam->num_tile_rows_minus1);

    //    for (Ipp32u i = 0; i <= picParam->num_tile_columns_minus1; i++)
    //        picParam->column_width_minus1[i] = (Ipp16u)(pPicParamSet->column_width[i] - 1);

    //    for (Ipp32u i = 0; i <= picParam->num_tile_rows_minus1; i++)
    //        picParam->row_height_minus1[i] = (Ipp16u)(pPicParamSet->row_height[i] - 1);
    //}

    //picParam->slice_parsing_fields.bits.lists_modification_present_flag = pPicParamSet->lists_modification_present_flag;
    //picParam->slice_parsing_fields.bits.long_term_ref_pics_present_flag = pSeqParamSet->long_term_ref_pics_present_flag;
    //picParam->slice_parsing_fields.bits.sps_temporal_mvp_enabled_flag = pSeqParamSet->sps_temporal_mvp_enabled_flag;
    //picParam->slice_parsing_fields.bits.cabac_init_present_flag = pPicParamSet->cabac_init_present_flag;
    //picParam->slice_parsing_fields.bits.output_flag_present_flag = pPicParamSet->output_flag_present_flag;
    //picParam->slice_parsing_fields.bits.dependent_slice_segments_enabled_flag = pPicParamSet->dependent_slice_segments_enabled_flag;
    //picParam->slice_parsing_fields.bits.pps_slice_chroma_qp_offsets_present_flag = pPicParamSet->pps_slice_chroma_qp_offsets_present_flag;
    //picParam->slice_parsing_fields.bits.sample_adaptive_offset_enabled_flag = pSeqParamSet->sample_adaptive_offset_enabled_flag;
    //picParam->slice_parsing_fields.bits.deblocking_filter_override_enabled_flag = pPicParamSet->deblocking_filter_override_enabled_flag;
    //picParam->slice_parsing_fields.bits.pps_disable_deblocking_filter_flag = pPicParamSet->pps_deblocking_filter_disabled_flag;
    //picParam->slice_parsing_fields.bits.slice_segment_header_extension_present_flag = pPicParamSet->slice_segment_header_extension_present_flag;

    //picParam->slice_parsing_fields.bits.RapPicFlag = (sliceHeader->nal_unit_type >= NAL_UT_CODED_SLICE_BLA_W_LP && sliceHeader->nal_unit_type <= NAL_UT_CODED_SLICE_CRA) ? 1 : 0;
    //picParam->slice_parsing_fields.bits.IdrPicFlag = sliceHeader->IdrPicFlag;
    //picParam->slice_parsing_fields.bits.IntraPicFlag = pSliceInfo->IsIntraAU() ? 1 : 0;

    //picParam->log2_max_pic_order_cnt_lsb_minus4 = (uint8_t)(pSeqParamSet->log2_max_pic_order_cnt_lsb - 4);
    //picParam->num_short_term_ref_pic_sets = (uint8_t)pSeqParamSet->getRPSList()->getNumberOfReferencePictureSets();
    //picParam->num_long_term_ref_pic_sps = (uint8_t)pSeqParamSet->num_long_term_ref_pics_sps;
    //picParam->num_ref_idx_l0_default_active_minus1 = (uint8_t)(pPicParamSet->num_ref_idx_l0_default_active - 1);
    //picParam->num_ref_idx_l1_default_active_minus1 = (uint8_t)(pPicParamSet->num_ref_idx_l1_default_active - 1);
    //picParam->pps_beta_offset_div2 = (int8_t)(pPicParamSet->pps_beta_offset >> 1);
    //picParam->pps_tc_offset_div2 = (int8_t)(pPicParamSet->pps_tc_offset >> 1);
    //picParam->num_extra_slice_header_bits = (uint8_t)pPicParamSet->num_extra_slice_header_bits;

    //picParam->st_rps_bits = pSlice->GetSliceHeader()->wNumBitsForShortTermRPSInSlice;

    UMCVACompBuffer *pParamBuf;
    unsigned int* pSliceID = (unsigned int*)m_va->GetCompBuffer(VAStatusParameterBufferType, &pParamBuf, sizeof(unsigned int));
    if (!pSliceID)
        throw h265_exception(UMC_ERR_FAILED);

    *pSliceID = pSlice->m_WidevineStatusReportNumber;

    pParamBuf->SetDataSize(sizeof(unsigned int));
}

void PackerVA_Widevine::PackAU(const H265DecoderFrame *frame, TaskSupplier_H265 * supplier)
{
    H265DecoderFrameInfo * sliceInfo = frame->m_pSlicesInfo;
    int sliceCount = sliceInfo->GetSliceCount();

    if (!sliceCount)
        return;

    H265Slice *pSlice = sliceInfo->GetSlice(0);
    //const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
    H265DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();

    PackPicParams(pCurrentFrame, sliceInfo, supplier);

    //if (pSeqParamSet->scaling_list_enabled_flag)
    //{
    //    PackQmatrix(pSlice);
    //}

    //CreateSliceParamBuffer(sliceInfo);
    //CreateSliceDataBuffer(sliceInfo);

    //Ipp32u sliceNum = 0;
    //for (Ipp32s n = 0; n < sliceCount; n++)
    //{
    //    PackSliceParams(sliceInfo->GetSlice(n), sliceNum, n == sliceCount - 1);
    //    sliceNum++;
    //}

    Status s = m_va->Execute();
    if(s != UMC_OK)
        throw h265_exception(s);
}

#endif // #if defined(UMC_VA_LINUX)

} // namespace UMC_HEVC_DECODER

#endif // UMC_RESTRICTED_CODE_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER
