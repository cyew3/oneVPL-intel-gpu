// Copyright (c) 2003-2019 Intel Corporation
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
#if defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "umc_va_base.h"
#if defined (UMC_VA_LINUX)
#if defined (MFX_ENABLE_CPLIB)

#if !defined(OPEN_SOURCE)
#include "umc_va_linux.h"
#endif

#include "umc_h265_tables.h"
#include "umc_h265_cenc_decrypter.h"
#include "umc_h265_cenc_slice_decoding.h"

namespace UMC_HEVC_DECODER
{
CENCParametersWrapper::CENCParametersWrapper(void)
{
    nal_unit_type = NAL_UT_INVALID;
    nuh_temporal_id = 0;
    slice_type = (SliceType)0;
    slice_pic_order_cnt_lsb = 0;
    has_eos_or_eob = 0;

    slice_fields.value = 0;

    num_of_curr_before = 0;
    num_of_curr_after = 0;
    num_of_curr_total = 0;
    num_of_foll_st = 0;
    num_of_curr_lt = 0;
    num_of_foll_lt = 0;

    memset(delta_poc_curr_before, 0, sizeof(int32_t)*8);
    memset(delta_poc_curr_after, 0, sizeof(int32_t)*8);
    memset(delta_poc_curr_total, 0, sizeof(int32_t)*8);
    memset(delta_poc_foll_st, 0, sizeof(int32_t)*16);
    memset(delta_poc_curr_lt, 0, sizeof(int32_t)*8);
    memset(delta_poc_foll_lt, 0, sizeof(int32_t)*16);
    memset(delta_poc_msb_present_flag, 0, sizeof(uint8_t)*16);
    memset(is_lt_curr_total, 0, sizeof(uint8_t)*8);
    memset(ref_list_idx, 0, sizeof(uint8_t)*2*16);

    next = nullptr;

    m_pts = 0;
    m_CENCStatusReportNumber = 0;
}

CENCParametersWrapper::~CENCParametersWrapper()
{
    ;
}

CENCParametersWrapper & CENCParametersWrapper::operator = (const VACencSliceParameterBufferHEVC & pDecryptParameters)
{
    CopyDecryptParams(pDecryptParameters);
    m_pts = 0;
    m_CENCStatusReportNumber = 0;
    return *this;
}

void CENCParametersWrapper::CopyDecryptParams(const VACencSliceParameterBufferHEVC & pDecryptParameters)
{
    VACencSliceParameterBufferHEVC* temp = this;
    *temp = pDecryptParameters;
}

UMC::Status CENCParametersWrapper::GetSliceHeaderPart1(H265SliceHeader * sliceHdr)
{
    if (!sliceHdr)
        throw h265_exception(UMC::UMC_ERR_NULL_PTR);

    sliceHdr->IdrPicFlag = (sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_IDR_W_RADL || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_IDR_N_LP) ? 1 : 0;

    if ( sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_IDR_W_RADL
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_IDR_N_LP
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_N_LP
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_RADL
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_LP
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_CRA )
    {
        sliceHdr->no_output_of_prior_pics_flag = slice_fields.bits.no_output_of_prior_pics_flag;
    }

    return UMC::UMC_OK;
}

// Parse remaining of slice header after GetSliceHeaderPart1
void CENCParametersWrapper::decodeSlice(H265CENCSlice *pSlice, const H265SeqParamSet *sps, const H265PicParamSet *pps)
{
    if (!pSlice || !pps || !sps)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    H265SliceHeader * sliceHdr = pSlice->GetSliceHeader();

    sliceHdr->slice_type = (SliceType)slice_type;

    if (sliceHdr->slice_type > I_SLICE || sliceHdr->slice_type < B_SLICE)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    if (pps->output_flag_present_flag)
    {
        sliceHdr->pic_output_flag = slice_fields.bits.pic_output_flag;
    }
    else
    {
        sliceHdr->pic_output_flag = 1;
    }

    if (sps->separate_colour_plane_flag  ==  1)
    {
        sliceHdr->colour_plane_id = slice_fields.bits.colour_plane_id;
    }

    if (sliceHdr->IdrPicFlag)
    {
        sliceHdr->slice_pic_order_cnt_lsb = 0;
        ReferencePictureSet* rps = pSlice->getRPS();
        rps->num_negative_pics = 0;
        rps->num_positive_pics = 0;
        rps->setNumberOfLongtermPictures(0);
        rps->num_pics = 0;
    }
    else
    {
        sliceHdr->slice_pic_order_cnt_lsb = slice_pic_order_cnt_lsb;

        {
            ReferencePictureSet* rps = pSlice->getRPS();
            parseRefPicSet(rps);

            if (((uint32_t)rps->getNumberOfNegativePictures() > sps->sps_max_dec_pic_buffering[sps->sps_max_sub_layers - 1]) ||
                ((uint32_t)rps->getNumberOfPositivePictures() > sps->sps_max_dec_pic_buffering[sps->sps_max_sub_layers - 1] - (uint32_t)rps->getNumberOfNegativePictures()))
            {
                pSlice->m_bError = true;
                if (sliceHdr->slice_type != I_SLICE)
                    throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

                rps->num_negative_pics = 0;
                rps->num_positive_pics = 0;
                rps->num_pics = 0;
            }
        }

        if (sps->long_term_ref_pics_present_flag)
        {
            ReferencePictureSet* rps = pSlice->getRPS();
            int offset = rps->getNumberOfNegativePictures()+rps->getNumberOfPositivePictures();

            for(uint32_t j = offset, k = 0; k < rps->num_lt_pics; j++, k++)
            {
                rps->delta_poc_msb_present_flag[j] = delta_poc_msb_present_flag[k];
            }
        }

        if ( sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_LP
            || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_RADL
            || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_N_LP )
        {
            ReferencePictureSet* rps = pSlice->getRPS();
            rps->num_negative_pics = 0;
            rps->num_positive_pics = 0;
            rps->setNumberOfLongtermPictures(0);
            rps->num_pics = 0;
        }
    }

    if (!pps->tiles_enabled_flag)
    {
        pSlice->allocateTileLocation(1);
        pSlice->m_tileByteLocation[0] = 0;
    }
}

UMC::Status CENCParametersWrapper::GetSliceHeaderFull(H265CENCSlice *rpcSlice, const H265SeqParamSet *sps, const H265PicParamSet *pps)
{
    if (!rpcSlice)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    rpcSlice->GetSliceHeader()->slice_pic_parameter_set_id = pps->pps_pic_parameter_set_id;

    if (rpcSlice->GetSliceHeader()->slice_pic_parameter_set_id > 63)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    UMC::Status sts = GetSliceHeaderPart1(rpcSlice->GetSliceHeader());
    if (UMC::UMC_OK != sts)
        return sts;
    decodeSlice(rpcSlice, sps, pps);

    return UMC::UMC_OK;
}

// Parse RPS part in slice header
void CENCParametersWrapper::parseRefPicSet(ReferencePictureSet* rps)
{
    rps->num_negative_pics = 0;
    rps->num_positive_pics = 0;
    rps->num_pics = 0;
    rps->num_lt_pics = 0;

    int32_t index = 0, j;
    for(j = 0; j < num_of_curr_before; index++, j++)
    {
        rps->used_by_curr_pic_flag[index] = 1;
        rps->m_DeltaPOC[index] = delta_poc_curr_before[j];
        rps->num_negative_pics++;
        rps->num_pics++;
    }
    for(j = 0; j < num_of_curr_after; index++, j++)
    {
        rps->used_by_curr_pic_flag[index] = 1;
        rps->m_DeltaPOC[index] = delta_poc_curr_after[j];
        rps->num_positive_pics++;
        rps->num_pics++;
    }
    for(j = 0; j < num_of_foll_st; index++, j++)
    {
        rps->used_by_curr_pic_flag[index] = 0;
        rps->m_DeltaPOC[index] = delta_poc_foll_st[j];
        if (rps->m_DeltaPOC[index] < 0)
            rps->num_negative_pics++;
        else
            rps->num_positive_pics++;
        rps->num_pics++;
    }

    rps->sortDeltaPOC();

    for(j = 0; j < num_of_curr_lt; index++, j++)
    {
        rps->used_by_curr_pic_flag[index] = 1;
        rps->m_DeltaPOC[index] = delta_poc_curr_lt[j];
        rps->num_lt_pics++;
        rps->num_pics++;
    }
    for(j = 0; j < num_of_foll_lt; index++, j++)
    {
        rps->used_by_curr_pic_flag[index] = 0;
        rps->m_DeltaPOC[index] = delta_poc_foll_lt[j];
        rps->num_lt_pics++;
        rps->num_pics++;
    }
}

#if !defined(OPEN_SOURCE)
struct VADriverVTableTpiPrivate {
    VAStatus (*vaEndCenc) (VADriverContextP ctx, VAContextID context);
    VAStatus (*vaQueryCenc) (VADriverContextP ctx, VABufferID buf_id, uint32_t size, VACencStatusBuf *info/* out */);
    unsigned long reserved[VA_PADDING_MEDIUM];
};

UMC::Status CENCDecrypter::DecryptFrame(UMC::MediaData *pSource, VACencStatusBuf* pCencStatus)
{
    if (!pSource)
        return UMC::UMC_OK;

    if (!m_dpy)
        m_va->GetVideoDecoder((void**)&m_dpy);

    VADisplayContextP pDisplayContext = (VADisplayContextP)m_dpy;
    if (pDisplayContext)
        return UMC::UMC_ERR_FAILED;

    VADriverContextP pDriverContext = pDisplayContext->pDriverContext;
    if (pDriverContext)
        return UMC::UMC_ERR_FAILED;

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
            return UMC::UMC_ERR_FAILED;
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

        UMC::VACompBuffer* protectedSliceData = nullptr;
        UMC::VACompBuffer* protectedParams = nullptr;

        m_va->GetCompBuffer(VAProtectedSliceDataBufferType, (UMC::UMCVACompBuffer**)&protectedSliceData, dataSize);
        if (!protectedSliceData)
            throw h265_exception(UMC::UMC_ERR_FAILED);

        memcpy(protectedSliceData->GetPtr(), pSource->GetDataPointer(), dataSize);
        protectedSliceData->SetDataSize(dataSize);

        int32_t encryptionParameterBufferType = -2;
        m_va->GetCompBuffer(encryptionParameterBufferType, (UMC::UMCVACompBuffer**)&protectedParams, sizeof(VAEncryptionParameters));
        if (!protectedParams)
            throw h265_exception(UMC::UMC_ERR_FAILED);

        memcpy(protectedParams->GetPtr(), &PESInputParams, sizeof(VAEncryptionParameters));
        protectedParams->SetDataSize(sizeof(VAEncryptionParameters));

        UMC::Status sts = m_va->Execute();
        if (sts != UMC::UMC_OK)
            throw h265_exception(sts);

        va_res = m_vaEndCenc(pDriverContext, 0 /*contextId*/);
        if (VA_STATUS_SUCCESS != va_res)
            throw h265_exception(UMC::UMC_ERR_FAILED);

        m_paramsSet.buf_size = 1024; // increase if needed
        m_paramsSet.slice_buf_type = VaCencSliceBufParamter;
        m_paramsSet.slice_buf_size = sizeof(VACencSliceParameterBufferHEVC);
        if (!m_paramsSet.buf) m_paramsSet.buf = (void*) calloc(1, m_paramsSet.buf_size);
        if (!m_paramsSet.slice_buf) m_paramsSet.slice_buf = (void*) calloc(1, m_paramsSet.slice_buf_size);

        do
        {
            va_res = m_vaQueryCenc(pDriverContext, protectedSliceData->GetID(), sizeof(VACencStatusBuf), &m_paramsSet);
            if (VA_STATUS_SUCCESS != va_res)
                throw h265_exception(UMC::UMC_ERR_FAILED);
        }
        while (m_paramsSet.status == VA_ENCRYPTION_STATUS_INCOMPLETE);

        if (m_paramsSet.status != VA_ENCRYPTION_STATUS_SUCCESSFUL)
            return UMC::UMC_ERR_DEVICE_FAILED;

        if (m_paramsSet.status_report_index_feedback != PESInputParams.status_report_index)
            return UMC::UMC_ERR_DEVICE_FAILED;

        *pCencStatus = m_paramsSet;

        pSource->MoveDataPointer(dataSize);

        m_bitstreamSubmitted = true;
    }

    return UMC::UMC_OK;
}
#endif // #if !defined(OPEN_SOURCE)

} // namespace UMC_HEVC_DECODER

#endif // #if defined (MFX_ENABLE_CPLIB)
#endif // #if defined (UMC_VA_LINUX)
#endif // MFX_ENABLE_H265_VIDEO_DECODE
