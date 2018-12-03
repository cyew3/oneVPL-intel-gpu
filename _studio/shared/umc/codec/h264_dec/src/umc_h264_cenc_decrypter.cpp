// Copyright (c) 2003-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_va_base.h"
#if defined (UMC_VA_LINUX)
#if defined (MFX_ENABLE_CPLIB)

#if !defined(OPEN_SOURCE)
#include "umc_va_linux.h"
#endif

#include "umc_h264_cenc_decrypter.h"

#include <limits.h>

namespace UMC
{

using namespace UMC_H264_DECODER;

CENCParametersWrapper::CENCParametersWrapper(void)
{
    nal_ref_idc = 0;
    idr_pic_flag = 0;
    slice_type = 0;
    field_frame_flag = 0;
    frame_number = 0;
    idr_pic_id = 0;
    pic_order_cnt_lsb = 0;
    delta_pic_order_cnt_bottom = 0;
    memset(delta_pic_order_cnt, 0, sizeof(int32_t)*2);

    ref_pic_fields.value = 0;

    memset(memory_management_control_operation, 0, sizeof(uint8_t)*32);
    memset(difference_of_pic_nums_minus1, 0, sizeof(int32_t)*32);
    memset(long_term_pic_num, 0, sizeof(int32_t)*32);
    memset(max_long_term_frame_idx_plus1, 0, sizeof(int32_t)*32);
    memset(long_term_frame_idx, 0, sizeof(int32_t)*32);

    next = nullptr;

    m_pts = 0;
    m_CENCStatusReportNumber = 0;
}

CENCParametersWrapper::~CENCParametersWrapper()
{
    ;
}

CENCParametersWrapper & CENCParametersWrapper::operator = (const VACencSliceParameterBufferH264 & pDecryptParameters)
{
    CopyDecryptParams(pDecryptParameters);
    m_pts = 0;
    m_CENCStatusReportNumber = 0;
    return *this;
}

void CENCParametersWrapper::CopyDecryptParams(const VACencSliceParameterBufferH264 & pDecryptParameters)
{
    VACencSliceParameterBufferH264* temp = this;
    *temp = pDecryptParameters;
}

Status CENCParametersWrapper::GetSliceHeaderPart1(H264SliceHeader *hdr)
{
    uint32_t val;

    hdr->nal_ref_idc = nal_ref_idc;
    hdr->IdrPicFlag = idr_pic_flag;

    // slice type
    val = slice_type;
    if (val > S_INTRASLICE)
    {
        if (val > S_INTRASLICE + S_INTRASLICE + 1)
        {
            return UMC_ERR_INVALID_STREAM;
        }
        else
        {
            // Slice type is specifying type of not only this but all remaining
            // slices in the picture. Since slice type is always present, this bit
            // of info is not used in our implementation. Adjust (just shift range)
            // and return type without this extra info.
            val -= (S_INTRASLICE + 1);
        }
    }

    if (val > INTRASLICE) // all other doesn't support
        return UMC_ERR_INVALID_STREAM;

    hdr->slice_type = (EnumSliceCodType)val;
    if (NAL_UT_IDR_SLICE == hdr->nal_unit_type && hdr->slice_type != INTRASLICE)
        return UMC_ERR_INVALID_STREAM;

    return UMC_OK;
} // Status GetSliceHeaderPart1(H264SliceHeader *pSliceHeader)

Status CENCParametersWrapper::GetSliceHeaderPart2(H264SliceHeader *hdr,
                                                  const H264PicParamSet *pps,
                                                  const H264SeqParamSet *sps)
{
    hdr->frame_num = frame_number;

    hdr->bottom_field_flag = 0;
    if (sps->frame_mbs_only_flag == 0)
    {
        hdr->field_pic_flag = (field_frame_flag == VA_TOP_FIELD) || (field_frame_flag == VA_BOTTOM_FIELD);
        hdr->MbaffFrameFlag = !hdr->field_pic_flag && sps->mb_adaptive_frame_field_flag;
        if (hdr->field_pic_flag != 0)
        {
            hdr->bottom_field_flag = (field_frame_flag == VA_BOTTOM_FIELD);
        }
    }

    if (hdr->IdrPicFlag)
    {
        int32_t pic_id = hdr->idr_pic_id = idr_pic_id;
        if (pic_id < 0 || pic_id > 65535)
            return UMC_ERR_INVALID_STREAM;
    }

    if (sps->pic_order_cnt_type == 0)
    {
        hdr->pic_order_cnt_lsb = pic_order_cnt_lsb;
        if (pps->bottom_field_pic_order_in_frame_present_flag && (!hdr->field_pic_flag))
            hdr->delta_pic_order_cnt_bottom = delta_pic_order_cnt_bottom;
    }

    if ((sps->pic_order_cnt_type == 1) && (sps->delta_pic_order_always_zero_flag == 0))
    {
        hdr->delta_pic_order_cnt[0] = delta_pic_order_cnt[0];  // WA to be sure that value is not 0, to correctly process FrameNumGap
        if (pps->bottom_field_pic_order_in_frame_present_flag && (!hdr->field_pic_flag))
            hdr->delta_pic_order_cnt[1] = delta_pic_order_cnt[1];  // WA to be sure that value is not 0, to correctly process FrameNumGap
    }

    return UMC_OK;
}

Status CENCParametersWrapper::GetSliceHeaderPart3(
    H264SliceHeader *hdr,        // slice header read goes here
    PredWeightTable * /*pPredWeight_L0*/, // L0 weight table goes here
    PredWeightTable * /*pPredWeight_L1*/, // L1 weight table goes here
    RefPicListReorderInfo *pReorderInfo_L0,
    RefPicListReorderInfo *pReorderInfo_L1,
    AdaptiveMarkingInfo *pAdaptiveMarkingInfo,
    AdaptiveMarkingInfo *pBaseAdaptiveMarkingInfo,
    const H264PicParamSet *pps,
    const H264SeqParamSet * /*sps*/,
    const H264SeqParamSetSVCExtension * /*spsSvcExt*/)
{
    pReorderInfo_L0->num_entries = 0;
    pReorderInfo_L1->num_entries = 0;
    hdr->num_ref_idx_l0_active = pps->num_ref_idx_l0_active;
    hdr->num_ref_idx_l1_active = pps->num_ref_idx_l1_active;

    hdr->direct_spatial_mv_pred_flag = 1;

    if (!hdr->nal_ext.svc_extension_flag || hdr->nal_ext.svc.quality_id == 0)
    {
        // dec_ref_pic_marking
        pAdaptiveMarkingInfo->num_entries = 0;
        pBaseAdaptiveMarkingInfo->num_entries = 0;

        if (hdr->nal_ref_idc)
        {
            if (hdr->IdrPicFlag)
            {
                hdr->no_output_of_prior_pics_flag = ref_pic_fields.bits.no_output_of_prior_pics_flag;
                hdr->long_term_reference_flag = ref_pic_fields.bits.long_term_reference_flag;
            }
            else
            {
                uint8_t mmco;
                uint32_t num_entries = 0;

                hdr->adaptive_ref_pic_marking_mode_flag = ref_pic_fields.bits.adaptive_ref_pic_marking_mode_flag;

                while (hdr->adaptive_ref_pic_marking_mode_flag != 0)
                {
                    mmco = memory_management_control_operation[num_entries];
                    if (mmco == 0)
                        break;

                    if (mmco > 6)
                        return UMC_ERR_INVALID_STREAM;

                    pAdaptiveMarkingInfo->mmco[num_entries] = mmco;

                    switch (pAdaptiveMarkingInfo->mmco[num_entries])
                    {
                    case 1:
                        // mark a short-term picture as unused for reference
                        // Value is difference_of_pic_nums_minus1
                        pAdaptiveMarkingInfo->value[num_entries*2] = difference_of_pic_nums_minus1[num_entries];
                        break;
                    case 2:
                        // mark a long-term picture as unused for reference
                        // value is long_term_pic_num
                        pAdaptiveMarkingInfo->value[num_entries*2] = long_term_pic_num[num_entries];
                        break;
                    case 3:
                        // Assign a long-term frame idx to a short-term picture
                        // Value is difference_of_pic_nums_minus1 followed by
                        // long_term_frame_idx. Only this case uses 2 value entries.
                        pAdaptiveMarkingInfo->value[num_entries*2] = difference_of_pic_nums_minus1[num_entries];
                        pAdaptiveMarkingInfo->value[num_entries*2+1] = long_term_frame_idx[num_entries];
                        break;
                    case 4:
                        // Specify max long term frame idx
                        // Value is max_long_term_frame_idx_plus1
                        // Set to "no long-term frame indices" (-1) when value == 0.
                        pAdaptiveMarkingInfo->value[num_entries*2] = max_long_term_frame_idx_plus1[num_entries];
                        break;
                    case 5:
                        // Mark all as unused for reference
                        // no value
                        break;
                    case 6:
                        // Assign long term frame idx to current picture
                        // Value is long_term_frame_idx
                        pAdaptiveMarkingInfo->value[num_entries*2] = long_term_frame_idx[num_entries];
                        break;
                    case 0:
                    default:
                        // invalid mmco command in bitstream
                        return UMC_ERR_INVALID_STREAM;
                    }  // switch

                    num_entries++;

                    if (num_entries >= MAX_NUM_REF_FRAMES)
                    {
                        return UMC_ERR_INVALID_STREAM;
                    }
                } // while

                if (ref_pic_fields.bits.dec_ref_pic_marking_count != num_entries)
                {
                    return UMC_ERR_INVALID_STREAM;
                }

                pAdaptiveMarkingInfo->num_entries = num_entries;
            }
        }    // def_ref_pic_marking
    }

    return UMC_OK;
} // GetSliceHeaderPart3()

Status CENCParametersWrapper::GetSliceHeaderPart4(H264SliceHeader *hdr,
                                                  const H264SeqParamSetSVCExtension *)
{
    hdr->scan_idx_start = 0;
    hdr->scan_idx_end = 15;

    return UMC_OK;
} // GetSliceHeaderPart4()

#if !defined(OPEN_SOURCE)
struct VADriverVTableTpiPrivate {
    VAStatus (*vaEndCenc) (VADriverContextP ctx, VAContextID context);
    VAStatus (*vaQueryCenc) (VADriverContextP ctx, VABufferID buf_id, uint32_t size, VACencStatusBuf *info/* out */);
    unsigned long reserved[VA_PADDING_MEDIUM];
};

Status CENCDecrypter::DecryptFrame(MediaData *pSource, VACencStatusBuf* pCencStatus)
{
    if (!pSource)
        return UMC_OK;

    if (!m_dpy)
        m_va->GetVideoDecoder((void**)&m_dpy);

    VADisplayContextP pDisplayContext = (VADisplayContextP)m_dpy;
    if (pDisplayContext)
        return UMC_ERR_FAILED;

    VADriverContextP pDriverContext = pDisplayContext->pDriverContext;
    if (pDriverContext)
        return UMC_ERR_FAILED;

    if (!m_vaEndCenc || !m_vaQueryCenc)
    {
        void* vtable_tpi = pDriverContext->vtable_tpi;
        if (vtable_tpi)
        {
            VADriverVTableTpiPrivate* vtable_tpi_private = (VADriverVTableTpiPrivate*)vtable_tpi;
            m_vaEndCenc = vtable_tpi_private->vaEndCenc;
            m_vaQueryCenc = vtable_tpi_private->vaQueryCenc;
        }
        else
            return UMC_ERR_FAILED;
    }

    size_t dataSize = pSource->GetDataSize();
    if (!m_bitstreamSubmitted && (dataSize != 0))
    {
        VAStatus va_res = VA_STATUS_SUCCESS;

        VAEncryptionParameters PESInputParams = {};
        VAEncryptionSegmentInfo SegmentInfo = {};
        PESInputParams.segment_info = &SegmentInfo;
        PESInputParams.status_report_index = m_PESPacketCounter++;
        PESInputParams.num_segments = 0;
        PESInputParams.size_of_length = 0;
        PESInputParams.session_id = 0;
        PESInputParams.encryption_type = VA_ENCRYPTION_TYPE_CENC_CBC; //VA_ENCRYPTION_TYPE_CENC_CTR_LENGTH

        VACompBuffer* protectedSliceData = nullptr;
        VACompBuffer* protectedParams = nullptr;

        m_va->GetCompBuffer(VAProtectedSliceDataBufferType, (UMCVACompBuffer**)&protectedSliceData, dataSize);
        if (!protectedSliceData)
            throw h264_exception(UMC_ERR_FAILED);

        memcpy(protectedSliceData->GetPtr(), pSource->GetDataPointer(), dataSize);
        protectedSliceData->SetDataSize(dataSize);

        int32_t encryptionParameterBufferType = -2;
        m_va->GetCompBuffer(encryptionParameterBufferType, (UMCVACompBuffer**)&protectedParams, sizeof(VAEncryptionParameters));
        if (!protectedParams)
            throw h264_exception(UMC_ERR_FAILED);

        memcpy(protectedParams->GetPtr(), &PESInputParams, sizeof(VAEncryptionParameters));
        protectedParams->SetDataSize(sizeof(VAEncryptionParameters));

        Status sts = m_va->Execute();
        if (sts != UMC_OK)
            throw h264_exception(sts);

        va_res = m_vaEndCenc(pDriverContext, 0 /*contextId*/);
        if (VA_STATUS_SUCCESS != va_res)
            throw h264_exception(UMC_ERR_FAILED);

        m_paramsSet.buf_size = 1024; // increase if needed
        m_paramsSet.slice_buf_type = VaCencSliceBufParamter;
        m_paramsSet.slice_buf_size = sizeof(VACencSliceParameterBufferH264);
        if (!m_paramsSet.buf) m_paramsSet.buf = (void*) calloc(1, m_paramsSet.buf_size);
        if (!m_paramsSet.slice_buf) m_paramsSet.slice_buf = (void*) calloc(1, m_paramsSet.slice_buf_size);

        do
        {
            va_res = m_vaQueryCenc(pDriverContext, protectedSliceData->GetID(), sizeof(VACencStatusBuf), &m_paramsSet);
            if (VA_STATUS_SUCCESS != va_res)
                throw h264_exception(UMC_ERR_FAILED);
        }
        while (m_paramsSet.status == VA_ENCRYPTION_STATUS_INCOMPLETE);

        if (m_paramsSet.status != VA_ENCRYPTION_STATUS_SUCCESSFUL)
            return UMC_ERR_DEVICE_FAILED;

        if (m_paramsSet.status_report_index_feedback != PESInputParams.status_report_index)
            return UMC_ERR_DEVICE_FAILED;

        *pCencStatus = m_paramsSet;

        pSource->MoveDataPointer(dataSize);

        m_bitstreamSubmitted = true;
    }

    return UMC_OK;
}
#endif // #if !defined(OPEN_SOURCE)

} // namespace UMC

#endif // #if defined (MFX_ENABLE_CPLIB)
#endif // #if defined (UMC_VA_LINUX)
#endif // UMC_ENABLE_H264_VIDEO_DECODER
