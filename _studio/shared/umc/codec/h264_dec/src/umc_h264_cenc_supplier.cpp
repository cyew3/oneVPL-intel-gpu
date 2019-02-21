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
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#include "umc_va_base.h"
#if defined (UMC_VA_LINUX)
#if defined (MFX_ENABLE_CPLIB)

#include "umc_h264_notify.h"
#include "umc_h264_nal_spl.h"
#include "umc_h264_cenc_decrypter.h"
#include "umc_h264_cenc_supplier.h"

#include "umc_va_linux_protected.h"

#include "mfx_common_int.h"

namespace UMC
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

Status CENCTaskSupplier::Init(VideoDecoderParams *pInit)
{
    Status umsRes = VATaskSupplier::Init(pInit);
    if (umsRes != UMC_OK)
        return umsRes;

#if !defined(OPEN_SOURCE)
    if (m_initializationParams.pVideoAccelerator->GetProtectedVA() &&
        (IS_PROTECTION_CENC(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected())))
    {
        m_pCENCDecrypter->Init();
        m_pCENCDecrypter->SetVideoHardwareAccelerator(((UMC::VideoDecoderParams*)pInit)->pVideoAccelerator);
    }
#endif

#if defined(ANDROID)
    m_DPBSizeEx += 2; // Fix for freeze issue
#endif

    return UMC_OK;
}

#if !defined(OPEN_SOURCE)
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

H264Slice *CENCTaskSupplier::ParseCENCSliceHeader(CENCParametersWrapper* pDecryptParams)
{
    if ((0 > m_Headers.m_SeqParams.GetCurrentID()) ||
        (0 > m_Headers.m_PicParams.GetCurrentID()))
    {
        return 0;
    }

    if (m_Headers.m_nalExtension.extension_present)
    {
        if (SVC_Extension::IsShouldSkipSlice(&m_Headers.m_nalExtension))
        {
            m_Headers.m_nalExtension.extension_present = 0;
            return 0;
        }
    }

    H264Slice * pSlice = CreateSlice();
    if (!pSlice)
    {
        return 0;
    }
    pSlice->SetHeap(&m_ObjHeap);
    pSlice->IncrementReference();

    notifier0<H264Slice> memory_leak_preventing_slice(pSlice, &H264Slice::DecrementReference);

    UMC_H264_DECODER::H264SEIPayLoad * spl = m_Headers.m_SEIParams.GetHeader(SEI_RECOVERY_POINT_TYPE);

    pSlice->m_pPicParamSet = m_Headers.m_PicParams.GetCurrentHeader();
    if (!pSlice->m_pPicParamSet)
    {
        return 0;
    }

    int32_t seq_parameter_set_id = pSlice->m_pPicParamSet->seq_parameter_set_id;

    pSlice->m_pSeqParamSetSvcEx = m_Headers.m_SeqParamsSvcExt.GetCurrentHeader();
    pSlice->m_pSeqParamSetMvcEx = m_Headers.m_SeqParamsMvcExt.GetCurrentHeader();
    pSlice->m_pSeqParamSet = m_Headers.m_SeqParams.GetHeader(seq_parameter_set_id);

    if (!pSlice->m_pSeqParamSet)
    {
        ErrorStatus::isSPSError = 0;
        return 0;
    }

    m_Headers.m_SeqParams.SetCurrentID(pSlice->m_pPicParamSet->seq_parameter_set_id);
    m_Headers.m_PicParams.SetCurrentID(pSlice->m_pPicParamSet->pic_parameter_set_id);

    if (pSlice->m_pSeqParamSet->errorFlags)
        ErrorStatus::isSPSError = 1;

    if (pSlice->m_pPicParamSet->errorFlags)
        ErrorStatus::isPPSError = 1;

    Status sts = InitializePictureParamSet(m_Headers.m_PicParams.GetCurrentHeader(), pSlice->m_pSeqParamSet, NAL_UT_CODED_SLICE_EXTENSION == pSlice->GetSliceHeader()->nal_unit_type);
    if (sts != UMC_OK)
    {
        return 0;
    }

    pSlice->m_pSeqParamSetEx = m_Headers.m_SeqExParams.GetHeader(seq_parameter_set_id);
    pSlice->m_pCurrentFrame = 0;

    pSlice->m_dTime = pDecryptParams->GetTime();

    int32_t iMBInFrame;
    int32_t iFieldIndex;

    // decode slice header
    {
        Status umcRes = UMC_OK;
        int32_t iSQUANT;

        try
        {
            memset(&pSlice->m_SliceHeader, 0, sizeof(pSlice->m_SliceHeader));

            pSlice->m_SliceHeader.pic_parameter_set_id = pSlice->m_pPicParamSet->pic_parameter_set_id;

            // decode first part of slice header
            umcRes = pDecryptParams->GetSliceHeaderPart1(&pSlice->m_SliceHeader);
            if (UMC_OK != umcRes)
                return 0;


            // decode second part of slice header
            umcRes = pDecryptParams->GetSliceHeaderPart2(&pSlice->m_SliceHeader,
                                                         pSlice->m_pPicParamSet,
                                                         pSlice->m_pSeqParamSet);
            if (UMC_OK != umcRes)
                return 0;

            // decode second part of slice header
            umcRes = pDecryptParams->GetSliceHeaderPart3(&pSlice->m_SliceHeader,
                                                         pSlice->m_PredWeight[0],
                                                         pSlice->m_PredWeight[1],
                                                         &pSlice->ReorderInfoL0,
                                                         &pSlice->ReorderInfoL1,
                                                         &pSlice->m_AdaptiveMarkingInfo,
                                                         &pSlice->m_BaseAdaptiveMarkingInfo,
                                                         pSlice->m_pPicParamSet,
                                                         pSlice->m_pSeqParamSet,
                                                         pSlice->m_pSeqParamSetSvcEx);
            if (UMC_OK != umcRes)
                return 0;

            //7.4.3 Slice header semantics
            //If the current picture is an IDR picture, frame_num shall be equal to 0
            //If this restrictions is violated, clear LT flag to avoid ref. pic. marking process corruption
            if (pSlice->m_SliceHeader.IdrPicFlag && pSlice->m_SliceHeader.frame_num != 0)
                pSlice->m_SliceHeader.long_term_reference_flag = 0;

            // decode 4 part of slice header
            umcRes = pDecryptParams->GetSliceHeaderPart4(&pSlice->m_SliceHeader, pSlice->m_pSeqParamSetSvcEx);
            if (UMC_OK != umcRes)
                return 0;

            pSlice->m_iMBWidth = pSlice->m_pSeqParamSet->frame_width_in_mbs;
            pSlice->m_iMBHeight = pSlice->m_pSeqParamSet->frame_height_in_mbs;

            pSlice->m_CENCStatusReportNumber = pDecryptParams->GetStatusReportNumber();

            // redundant slice, discard
            if (pSlice->m_SliceHeader.redundant_pic_cnt)
                return 0;

            // Set next MB.
            if (pSlice->m_SliceHeader.first_mb_in_slice >= (int32_t) (pSlice->m_iMBWidth * pSlice->m_iMBHeight))
            {
                return 0;
            }

            int32_t bit_depth_luma = pSlice->GetSeqParam()->bit_depth_luma;

            iSQUANT = pSlice->m_pPicParamSet->pic_init_qp +
                      pSlice->m_SliceHeader.slice_qp_delta;
            if (iSQUANT < QP_MIN - 6*(bit_depth_luma - 8)
                || iSQUANT > QP_MAX)
            {
                return 0;
            }

            if (pSlice->m_pPicParamSet->entropy_coding_mode)
                pSlice->m_BitStream.AlignPointerRight();
        }
        catch(const h264_exception & )
        {
            return 0;
        }
        catch(...)
        {
            return 0;
        }
    }

    pSlice->m_iMBWidth  = pSlice->GetSeqParam()->frame_width_in_mbs;
    pSlice->m_iMBHeight = pSlice->GetSeqParam()->frame_height_in_mbs;

    iMBInFrame = (pSlice->m_iMBWidth * pSlice->m_iMBHeight) / ((pSlice->m_SliceHeader.field_pic_flag) ? (2) : (1));
    iFieldIndex = (pSlice->m_SliceHeader.field_pic_flag && pSlice->m_SliceHeader.bottom_field_flag) ? (1) : (0);

    // set slice variables
    pSlice->m_iFirstMB = pSlice->m_SliceHeader.first_mb_in_slice;
    pSlice->m_iMaxMB = iMBInFrame;

    pSlice->m_iFirstMBFld = pSlice->m_SliceHeader.first_mb_in_slice + iMBInFrame * iFieldIndex;

    pSlice->m_iAvailableMB = iMBInFrame;

    if (pSlice->m_iFirstMB >= pSlice->m_iAvailableMB)
        return 0;

    // reset all internal variables
    pSlice->m_bFirstDebThreadedCall = true;
    pSlice->m_bError = false;

    // set conditional flags
    pSlice->m_bDecoded = false;
    pSlice->m_bPrevDeblocked = false;
    pSlice->m_bDeblocked = (pSlice->m_SliceHeader.disable_deblocking_filter_idc == DEBLOCK_FILTER_OFF);

    // frame is not associated yet
    pSlice->m_pCurrentFrame = NULL;

    pSlice->m_bInited = true;
    pSlice->m_pSeqParamSet->IncrementReference();
    pSlice->m_pPicParamSet->IncrementReference();
    if (pSlice->m_pSeqParamSetEx)
        pSlice->m_pSeqParamSetEx->IncrementReference();
    if (pSlice->m_pSeqParamSetMvcEx)
        pSlice->m_pSeqParamSetMvcEx->IncrementReference();
    if (pSlice->m_pSeqParamSetSvcEx)
        pSlice->m_pSeqParamSetSvcEx->IncrementReference();

    if (IsShouldSkipSlice(pSlice))
        return 0;

    if (!IsExtension())
    {
        memset(&pSlice->GetSliceHeader()->nal_ext, 0, sizeof(UMC_H264_DECODER::H264NalExtension));
    }

    if (spl)
    {
        if (m_WaitForIDR)
        {
            AllocateAndInitializeView(pSlice);
            ViewItem &view = GetView(pSlice->GetSliceHeader()->nal_ext.mvc.view_id);
            uint32_t recoveryFrameNum = (spl->SEI_messages.recovery_point.recovery_frame_cnt + pSlice->GetSliceHeader()->frame_num) % (1 << pSlice->m_pSeqParamSet->log2_max_frame_num);
            view.GetDPBList(0)->SetRecoveryFrameCnt(recoveryFrameNum);
        }

        if ((pSlice->GetSliceHeader()->slice_type != INTRASLICE))
        {
            UMC_H264_DECODER::H264SEIPayLoad * splToRemove = m_Headers.m_SEIParams.GetHeader(SEI_RECOVERY_POINT_TYPE);
            m_Headers.m_SEIParams.RemoveHeader(splToRemove);
        }
    }

    m_WaitForIDR = false;
    memory_leak_preventing_slice.ClearNotification();

    return pSlice;
}

Status CENCTaskSupplier::AddOneFrame(MediaData* src)
{
    if (!m_initializationParams.pVideoAccelerator->GetProtectedVA() ||
        !IS_PROTECTION_CENC(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected()))
    {
        return UMC_ERR_FAILED;
    }

    Status umsRes = UMC_OK;

    if (m_pLastSlice)
    {
        umsRes = AddSlice(m_pLastSlice, !src);
        if (umsRes == UMC_ERR_NOT_ENOUGH_BUFFER || umsRes == UMC_OK || umsRes == UMC_ERR_ALLOC)
            return umsRes;
    }

    uint32_t const flags = src ? src->GetFlags() : 0;
    if (flags & MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME)
        return UMC_ERR_FAILED;

    bool decrypted = false;
    int32_t src_size = src ? (int32_t)src->GetDataSize() : 0;
    VACencStatusBuf cencStatus{};

    auto aux = src ? src->GetAuxInfo(MFX_EXTBUFF_DECRYPTED_PARAM) : nullptr;
    auto dp_ext = aux ? reinterpret_cast<mfxExtDecryptedParam*>(aux->ptr) : nullptr;

    if (dp_ext)
    {
        if (!dp_ext->Data       ||
             dp_ext->DataLength != sizeof (VACencStatusBuf))
            return UMC_ERR_FAILED;

        cencStatus = *(reinterpret_cast<VACencStatusBuf*>(dp_ext->Data));
        decrypted = true;
    }
    else
    {
#if !defined(OPEN_SOURCE)
        void* bsDataPointer = src ? src->GetDataPointer() : 0;

        umsRes = m_pCENCDecrypter->DecryptFrame(src, &cencStatus);
        if (umsRes != UMC_OK)
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
             cencStatus.slice_buf_size != sizeof(VACencSliceParameterBufferH264) ||
            !cencStatus.slice_buf)
            return UMC_ERR_FAILED;

        CENCParametersWrapper sliceParams{};
        sliceParams = *(reinterpret_cast<VACencSliceParameterBufferH264*>(cencStatus.slice_buf));
        sliceParams.SetStatusReportNumber(cencStatus.status_report_index_feedback);
        sliceParams.SetTime(src ? src->GetTime() : 0);

        if (m_status_report_index_feedback != cencStatus.status_report_index_feedback)
        {
            m_cencData.SetBufferPointer((uint8_t*)cencStatus.buf, cencStatus.buf_size);
            m_cencData.SetDataSize(cencStatus.buf_size);
            m_status_report_index_feedback = cencStatus.status_report_index_feedback;
        }

        do
        {
            NalUnit *nalUnit = m_pNALSplitter->GetNalUnits(&m_cencData);

            if (!nalUnit) break;

            switch ((NAL_Unit_Type)nalUnit->GetNalUnitType())
            {
            case NAL_UT_SPS:
            case NAL_UT_PPS:
                umsRes = DecodeHeaders(nalUnit);
                if (umsRes == UMC_WRN_REPOSITION_INPROGRESS) umsRes = UMC_OK;
                if (umsRes != UMC_OK)
                {
                    if (umsRes == UMC_NTF_NEW_RESOLUTION)
                    {
                        int32_t size = (int32_t)nalUnit->GetDataSize();
                        m_cencData.MoveDataPointer(- size - 3);

                        if (!dp_ext)
                            src->MoveDataPointer(-src_size);
                    }
                    return umsRes;
                }

                if (nalUnit->GetNalUnitType() == NAL_UT_SPS || nalUnit->GetNalUnitType() == NAL_UT_PPS)
                {
                    m_accessUnit.CompleteLastLayer();
                }
                break;

            case NAL_UT_SEI:
                m_accessUnit.CompleteLastLayer();
                DecodeSEI(nalUnit);
                break;
            case NAL_UT_AUD:  //ignore it
            case NAL_UT_END_OF_STREAM:
            case NAL_UT_END_OF_SEQ:
            case NAL_UT_SPS_EX:
            case NAL_UT_SUBSET_SPS:
            case NAL_UT_PREFIX:
            case NAL_UT_IDR_SLICE:
            case NAL_UT_SLICE:
            case NAL_UT_AUXILIARY:
            case NAL_UT_CODED_SLICE_EXTENSION:
            case NAL_UT_DPA:
            case NAL_UT_DPB:
            case NAL_UT_DPC:
            case NAL_UT_FD:
            default:
                return UMC_ERR_FAILED;
                break;
            };

        } while ((MINIMAL_DATA_SIZE < m_cencData.GetDataSize()));

        H264Slice * pSlice = ParseCENCSliceHeader(&sliceParams);
        if (pSlice)
        {
            umsRes = AddSlice(pSlice, !src);
            if (umsRes == UMC_ERR_NOT_ENOUGH_BUFFER || umsRes == UMC_OK || umsRes == UMC_ERR_ALLOC)
                return umsRes;
        }
    }

    return AddSlice(0, !src);
}

#if !defined(OPEN_SOURCE)
Status CENCTaskSupplier::CompleteFrame(H264DecoderFrame * pFrame, int32_t field)
{
    Status sts = VATaskSupplier::CompleteFrame(pFrame, field);

    if (m_initializationParams.pVideoAccelerator->GetProtectedVA() &&
        (IS_PROTECTION_CENC(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected())))
    {
        m_pCENCDecrypter->ReleaseForNewBitstream();
    }

    return sts;
}
#endif

} // namespace UMC

#endif // #if defined (MFX_ENABLE_CPLIB)
#endif // #if defined (UMC_VA_LINUX)
#endif // MFX_ENABLE_H264_VIDEO_DECODE
