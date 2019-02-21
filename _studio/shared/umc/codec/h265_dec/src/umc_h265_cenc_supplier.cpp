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

#include "umc_h265_cenc_supplier.h"
#include "umc_h265_cenc_slice_decoding.h"
#include "umc_h265_nal_spl.h"

#ifdef UMC_VA_DXVA
#include "umc_va_dxva2_protected.h"
#endif

#ifdef UMC_VA_LINUX
#include "umc_va_linux_protected.h"
#endif

#include "mfx_common_int.h"

namespace UMC_HEVC_DECODER
{

CENCTaskSupplier::CENCTaskSupplier()
    : VATaskSupplier()
    , m_status_report_index_feedback(-1)
#if !defined(OPEN_SOURCE)
    , m_pCENCDecrypter(new CENCDecrypter{})
#endif
{}

CENCTaskSupplier::~CENCTaskSupplier()
{}

#if !defined(OPEN_SOURCE)
UMC::Status CENCTaskSupplier::Init(UMC::VideoDecoderParams *pInit)
{
    UMC::Status umsRes = VATaskSupplier::Init(pInit);
    if (umsRes != UMC::UMC_OK)
        return umsRes;

    if (m_initializationParams.pVideoAccelerator->GetProtectedVA() &&
        (IS_PROTECTION_CENC(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected())))
    {
        m_pCENCDecrypter->Init();
        m_pCENCDecrypter->SetVideoHardwareAccelerator(((UMC::VideoDecoderParams*)pInit)->pVideoAccelerator);
    }

    return UMC::UMC_OK;
}

void CENCTaskSupplier::Reset()
{
    VATaskSupplier::Reset();

    m_status_report_index_feedback = -1;

    if (m_initializationParams.pVideoAccelerator->GetProtectedVA() &&
        (IS_PROTECTION_CENC(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected())))
    {
        m_pCENCDecrypter->Reset();
    }
}
#endif

H265Slice *CENCTaskSupplier::ParseCENCSliceHeader(CENCParametersWrapper* pDecryptParams)
{
    if ((0 > m_Headers.m_SeqParams.GetCurrentID()) ||
        (0 > m_Headers.m_PicParams.GetCurrentID()))
    {
        return 0;
    }

    H265CENCSlice * pSlice = m_ObjHeap.AllocateObject<H265CENCSlice>();
    pSlice->IncrementReference();

    notifier0<H265CENCSlice> memory_leak_preventing_slice(pSlice, &H265CENCSlice::DecrementReference);

    pSlice->SetDecryptParameters(pDecryptParams);
    pSlice->m_source.SetTime(pDecryptParams->GetTime());

    pSlice->SetPicParam(m_Headers.m_PicParams.GetCurrentHeader());
    H265PicParamSet const* pps = pSlice->GetPicParam();
    if (!pps)
    {
        return 0;
    }

    int32_t seq_parameter_set_id = pps->pps_seq_parameter_set_id;

    pSlice->SetSeqParam(m_Headers.m_SeqParams.GetHeader(seq_parameter_set_id));
    if (!pSlice->GetSeqParam())
    {
        return 0;
    }

    m_Headers.m_SeqParams.SetCurrentID(pps->pps_seq_parameter_set_id);
    m_Headers.m_PicParams.SetCurrentID(pps->pps_pic_parameter_set_id);

    pSlice->m_pCurrentFrame = NULL;

    bool ready = pSlice->Reset(&m_pocDecoding);
    if (!ready)
    {
        m_prevSliceBroken = pSlice->IsError();
        return 0;
    }

    H265SliceHeader * sliceHdr = pSlice->GetSliceHeader();
    VM_ASSERT(sliceHdr);

    if (m_prevSliceBroken && sliceHdr->dependent_slice_segment_flag)
    {
        //Prev. slice contains errors. There is no relayable way to infer parameters for dependent slice
        return 0;
    }

    m_prevSliceBroken = false;

    if (m_WaitForIDR)
    {
        if (pps->pps_curr_pic_ref_enabled_flag)
        {
            ReferencePictureSet const* rps = pSlice->getRPS();
            VM_ASSERT(rps);

            uint32_t const numPicTotalCurr = rps->getNumberOfUsedPictures();
            if (numPicTotalCurr)
                return 0;
        }
        else if (sliceHdr->slice_type != I_SLICE)
        {
            return 0;
        }
    }

    ActivateHeaders(const_cast<H265SeqParamSet *>(pSlice->GetSeqParam()), const_cast<H265PicParamSet *>(pps));

    m_WaitForIDR = false;
    memory_leak_preventing_slice.ClearNotification();

    pSlice->SetCENCStatusReportNumber(pDecryptParams->GetStatusReportNumber());

    //for SliceIdx m_SliceIdx m_iNumber
    pSlice->m_iNumber = m_SliceIdxInTaskSupplier;
    m_SliceIdxInTaskSupplier++;
    return pSlice;
}

UMC::Status CENCTaskSupplier::AddOneFrame(UMC::MediaData* src)
{
    if (!m_initializationParams.pVideoAccelerator->GetProtectedVA() ||
        !IS_PROTECTION_CENC(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected()))
    {
        return UMC::UMC_ERR_FAILED;
    }

    UMC::Status umsRes = UMC::UMC_OK;

    if (m_pLastSlice)
    {
        umsRes = AddSlice(m_pLastSlice, !src);
        if (umsRes == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || umsRes == UMC::UMC_OK)
            return umsRes;
    }

    uint32_t const flags = src ? src->GetFlags() : 0;
    if (flags & UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME)
        return UMC::UMC_ERR_FAILED;

    bool decrypted = false;
    int32_t src_size = src ? (int32_t)src->GetDataSize() : 0;
    VACencStatusBuf cencStatus{};

    auto aux = src ? src->GetAuxInfo(MFX_EXTBUFF_DECRYPTED_PARAM) : nullptr;
    auto dp_ext = aux ? reinterpret_cast<mfxExtDecryptedParam*>(aux->ptr) : nullptr;

    if (dp_ext)
    {
        if (!dp_ext->Data       ||
             dp_ext->DataLength != sizeof (VACencStatusBuf))
            return UMC::UMC_ERR_FAILED;

        cencStatus = *(reinterpret_cast<VACencStatusBuf*>(dp_ext->Data));
        decrypted = true;
    }
    else
    {
#if !defined(OPEN_SOURCE)
        void* bsDataPointer = src ? src->GetDataPointer() : 0;

        umsRes = m_pCENCDecrypter->DecryptFrame(src, &cencStatus);
        if (umsRes != UMC::UMC_OK)
            return umsRes;

        decrypted = src ? (bsDataPointer != src->GetDataPointer()) : false;
#endif
    }

    if (decrypted)
    {
        if ( cencStatus.status != VA_ENCRYPTION_STATUS_SUCCESSFUL ||
            !cencStatus.buf_size ||
            !cencStatus.buf ||
             cencStatus.slice_buf_type != VaCencSliceBufParamter ||
             cencStatus.slice_buf_size != sizeof(VACencSliceParameterBufferHEVC) ||
            !cencStatus.slice_buf)
            return UMC::UMC_ERR_FAILED;

        CENCParametersWrapper sliceParams{};
        sliceParams = *(reinterpret_cast<VACencSliceParameterBufferHEVC*>(cencStatus.slice_buf));
        sliceParams.SetStatusReportNumber(cencStatus.status_report_index_feedback);
        sliceParams.SetTime(src ? src->GetTime() : 0);

        if (m_status_report_index_feedback != cencStatus.status_report_index_feedback)
        {
            m_cencData.SetBufferPointer((uint8_t*)cencStatus.buf, cencStatus.buf_size);
            m_cencData.SetDataSize(cencStatus.buf_size);
            m_status_report_index_feedback = cencStatus.status_report_index_feedback;
        }

        if (m_checkCRAInsideResetProcess && !src)
            return UMC::UMC_ERR_FAILED;

        size_t moveToSpsOffset = m_checkCRAInsideResetProcess ? m_cencData.GetDataSize() : 0;

        do
        {
            UMC::MediaDataEx *nalUnit = m_pNALSplitter->GetNalUnits(&m_cencData);
            if (!nalUnit)
                break;

            UMC::MediaDataEx::_MediaDataEx* pMediaDataEx = nalUnit->GetExData();

            for (int32_t i = 0; i < (int32_t)pMediaDataEx->count; i++, pMediaDataEx->index ++)
            {
                if (m_checkCRAInsideResetProcess)
                {
                    switch ((NalUnitType)pMediaDataEx->values[i])
                    {

                    case NAL_UT_VPS:
                    case NAL_UT_SPS:
                    case NAL_UT_PPS:
                        DecodeHeaders(nalUnit);
                        break;

                    case NAL_UT_CODED_SLICE_RASL_N:
                    case NAL_UT_CODED_SLICE_RADL_N:
                    case NAL_UT_CODED_SLICE_TRAIL_R:
                    case NAL_UT_CODED_SLICE_TRAIL_N:
                    case NAL_UT_CODED_SLICE_TLA_R:
                    case NAL_UT_CODED_SLICE_TSA_N:
                    case NAL_UT_CODED_SLICE_STSA_R:
                    case NAL_UT_CODED_SLICE_STSA_N:
                    case NAL_UT_CODED_SLICE_BLA_W_LP:
                    case NAL_UT_CODED_SLICE_BLA_W_RADL:
                    case NAL_UT_CODED_SLICE_BLA_N_LP:
                    case NAL_UT_CODED_SLICE_IDR_W_RADL:
                    case NAL_UT_CODED_SLICE_IDR_N_LP:
                    case NAL_UT_CODED_SLICE_CRA:
                    case NAL_UT_CODED_SLICE_RADL_R:
                    case NAL_UT_CODED_SLICE_RASL_R:
                    default:
                        return UMC::UMC_ERR_FAILED;
                        break;
                    };

                    continue;
                }

                NalUnitType nut =
                    static_cast<NalUnitType>(pMediaDataEx->values[i]);
                switch (nut)
                {
                case NAL_UT_SEI:
                case NAL_UT_SEI_SUFFIX:
                    DecodeSEI(nalUnit);
                    break;

                case NAL_UT_VPS:
                case NAL_UT_SPS:
                case NAL_UT_PPS:
                    {
                        umsRes = DecodeHeaders(nalUnit);
                        if (umsRes == UMC::UMC_WRN_REPOSITION_INPROGRESS) umsRes = UMC::UMC_OK;
                        if (umsRes != UMC::UMC_OK)
                        {
                            if (umsRes == UMC::UMC_NTF_NEW_RESOLUTION ||
                                (nut == NAL_UT_SPS && umsRes == UMC::UMC_ERR_INVALID_STREAM))
                            {
                                int32_t nalIndex = pMediaDataEx->index;
                                int32_t size = pMediaDataEx->offsets[nalIndex + 1] - pMediaDataEx->offsets[nalIndex];

                                m_checkCRAInsideResetProcess = true;

                                if (AddSlice(0, !src) == UMC::UMC_OK)
                                {
                                    m_cencData.MoveDataPointer(- size - 3);

                                    if (!dp_ext)
                                        src->MoveDataPointer(-src_size);

                                    return UMC::UMC_OK;
                                }
                                moveToSpsOffset = m_cencData.GetDataSize() + size + 3;
                                continue;
                            }

                            return umsRes;
                        }
                    }
                    break;

                case NAL_UT_CODED_SLICE_RASL_N:
                case NAL_UT_CODED_SLICE_RADL_N:
                case NAL_UT_CODED_SLICE_TRAIL_R:
                case NAL_UT_CODED_SLICE_TRAIL_N:
                case NAL_UT_CODED_SLICE_TLA_R:
                case NAL_UT_CODED_SLICE_TSA_N:
                case NAL_UT_CODED_SLICE_STSA_R:
                case NAL_UT_CODED_SLICE_STSA_N:
                case NAL_UT_CODED_SLICE_BLA_W_LP:
                case NAL_UT_CODED_SLICE_BLA_W_RADL:
                case NAL_UT_CODED_SLICE_BLA_N_LP:
                case NAL_UT_CODED_SLICE_IDR_W_RADL:
                case NAL_UT_CODED_SLICE_IDR_N_LP:
                case NAL_UT_CODED_SLICE_CRA:
                case NAL_UT_CODED_SLICE_RADL_R:
                case NAL_UT_CODED_SLICE_RASL_R:
                case NAL_UT_AU_DELIMITER:
                case NAL_UT_EOS:
                case NAL_UT_EOB:
                default:
                    return UMC::UMC_ERR_FAILED;
                    break;
                };
            }

        } while ((MINIMAL_DATA_SIZE_H265 < m_cencData.GetDataSize()));

        if (m_checkCRAInsideResetProcess)
        {
            m_cencData.MoveDataPointer(int32_t(m_cencData.GetDataSize() - moveToSpsOffset));
            m_pNALSplitter->Reset();
        }

        H265Slice * pSlice = ParseCENCSliceHeader(&sliceParams);
        if (pSlice)
        {
            umsRes = AddSlice(pSlice, !src);
            if (umsRes == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || umsRes == UMC::UMC_OK)
                return umsRes;
        }
    }

    return AddSlice(0, !src);
}

#if !defined(OPEN_SOURCE)
void CENCTaskSupplier::CompleteFrame(H265DecoderFrame * pFrame)
{
    VATaskSupplier::CompleteFrame(pFrame);

    if (m_initializationParams.pVideoAccelerator->GetProtectedVA() &&
        (IS_PROTECTION_CENC(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected())))
    {
        m_pCENCDecrypter->ReleaseForNewBitstream();
    }

    return;
}
#endif

} // namespace UMC_HEVC_DECODER

#endif // #if defined (MFX_ENABLE_CPLIB)
#endif // #if defined (UMC_VA_LINUX)
#endif // MFX_ENABLE_H265_VIDEO_DECODE
