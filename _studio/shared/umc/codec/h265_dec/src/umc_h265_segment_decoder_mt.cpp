/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_segment_decoder_mt.h"
#include "umc_h265_segment_decoder_templates.h"

#include "umc_h265_task_broker.h"
#include "umc_h265_frame_info.h"

#include "umc_h265_task_supplier.h"
#include "h265_tr_quant.h"

#include "umc_h265_timing.h"

#include "mfx_trace.h"
#include "umc_h265_dec_debug.h"

namespace UMC_HEVC_DECODER
{
H265SegmentDecoderMultiThreaded::H265SegmentDecoderMultiThreaded(TaskBroker_H265 * pTaskBroker)
    : H265SegmentDecoder(pTaskBroker)
    , m_SD(0)
{
} // H265SegmentDecoderMultiThreaded::H265SegmentDecoderMultiThreaded(H264SliceStore *Store)

H265SegmentDecoderMultiThreaded::~H265SegmentDecoderMultiThreaded(void)
{
    Release();

} // H265SegmentDecoderMultiThreaded::~H265SegmentDecoderMultiThreaded(void)

UMC::Status H265SegmentDecoderMultiThreaded::Init(Ipp32s iNumber)
{
    // release object before initialization
    Release();

    // call parent's Init
    return H265SegmentDecoder::Init(iNumber);
} // Status H265SegmentDecoderMultiThreaded::Init(Ipp32u nNumber)

void H265SegmentDecoderMultiThreaded::StartProcessingSegment(H265Task &Task)
{
    m_pSlice = Task.m_pSlice;
    m_pSliceHeader = m_pSlice->GetSliceHeader();
    m_pBitStream = m_pSlice->GetBitStream();

    m_pPicParamSet = m_pSlice->GetPicParam();
    m_pSeqParamSet = m_pSlice->GetSeqParam();
    m_pCurrentFrame = m_pSlice->GetCurrentFrame();
    m_SD = CreateSegmentDecoder();

    m_context = Task.m_context;
    if (!m_context)
    {
        m_context = m_context_single_thread.get();
        m_context->Init(Task.m_pSlice);
    }
    
    this->create((H265SeqParamSet*)m_pSeqParamSet);

    m_Prediction->InitTempBuff(m_context);

    Ipp32s sliceNum = m_pSlice->GetSliceNum();

    m_pRefPicList[0] = m_pCurrentFrame->GetRefPicList(sliceNum, REF_PIC_LIST_0)->m_refPicList;
    m_pRefPicList[1] = m_pCurrentFrame->GetRefPicList(sliceNum, REF_PIC_LIST_1)->m_refPicList;

    if (m_pSliceHeader->m_SeqParamSet->getScalingListFlag())
    {
        m_TrQuant->setScalingListDec(const_cast<H265ScalingList *>(m_pSliceHeader->m_SeqParamSet->getScalingList()));

        if (m_pSliceHeader->m_PicParamSet->getScalingListPresentFlag())
        {
            m_TrQuant->setScalingListDec(const_cast<H265ScalingList *>(m_pSliceHeader->m_PicParamSet->getScalingList()));
        }
        if (!m_pSliceHeader->m_PicParamSet->getScalingListPresentFlag() && !m_pSliceHeader->m_SeqParamSet->getScalingListPresentFlag())
        {
            m_TrQuant->setDefaultScalingList();
        }
        m_TrQuant->m_UseScalingList = true;
    }
    else
    {
        m_TrQuant->m_UseScalingList = false;
    }
}

void H265SegmentDecoderMultiThreaded::EndProcessingSegment(H265Task &Task)
{
    this->destroy();

    if (TASK_DEC_H265 == Task.m_iTaskID)
    {
        Ipp32s mvsDistortion = m_context->m_mvsDistortion;
        mvsDistortion = (mvsDistortion + 6) >> 2; // remove 1/2 pel
        Task.m_mvsDistortion = IPP_MAX(mvsDistortion, m_pSlice->m_mvsDistortion);
    }

    m_pTaskBroker->AddPerformedTask(&Task);
}

UMC::Status H265SegmentDecoderMultiThreaded::ProcessSegment(void)
{
#ifdef __EXCEPTION_HANDLER_
    exceptionHandler.setTranslator();
#endif

    H265Task Task(m_iNumber);

    if (m_pTaskBroker->GetNextTask(&Task))
    {
        UMC::Status umcRes = UMC::UMC_OK;

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "HEVCDec_work");
        VM_ASSERT(Task.pFunction);

        StartProcessingSegment(Task);

        if (!m_pCurrentFrame->IsSkipped() && (!m_pSlice || !m_pSlice->IsError()))
        {
            try // do decoding
            {
                Ipp32s firstMB = Task.m_iFirstMB;
                umcRes = (this->*(Task.pFunction))(firstMB, Task.m_iMBToProcess); //ProcessSlice function

                if (UMC::UMC_ERR_END_OF_STREAM == umcRes)
                {
                    //Task.m_iMaxMB = Task.m_iFirstMB + Task.m_iMBToProcess;
                    //VM_ASSERT(m_pSlice);
                    //// if we decode less macroblocks if we need try to recovery:
                    //RestoreErrorRect(Task.m_iFirstMB + Task.m_iMBToProcess, m_pSlice->GetMaxMB(), m_pSlice);
                    umcRes = UMC::UMC_OK;
                }
                else if (UMC::UMC_OK != umcRes)
                {
                    umcRes = UMC::UMC_ERR_INVALID_STREAM;
                }

                Task.m_bDone = true;

            } catch(const h265_exception & ex)
            {
                umcRes = ex.GetStatus();
            } catch(...)
            {
                umcRes = UMC::UMC_ERR_INVALID_STREAM;
            }
        }

        if (umcRes != UMC::UMC_OK)
        {
            Task.m_bError = true;
            Task.m_iMaxMB = Task.m_iFirstMB + Task.m_iMBToProcess;
//            if (m_pSlice) //restoreerror
//                RestoreErrorRect(Task.m_iFirstMB + Task.m_iMBToProcess, m_pSlice->GetMaxMB(), m_pSlice);
            // break; // DEBUG : ADB should we return if error occur??
        }

        EndProcessingSegment(Task);
        MFX_LTRACE_I(MFX_TRACE_LEVEL_INTERNAL, umcRes);
    }
    else
    {
        return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    }

    return UMC::UMC_OK;
} // Status H265SegmentDecoderMultiThreaded::ProcessSegment(void)

void H265SegmentDecoderMultiThreaded::RestoreErrorRect(Ipp32s , Ipp32s , H265Slice * )
{
    return;
}

UMC::Status H265SegmentDecoderMultiThreaded::DecodeSegment(Ipp32s iCurMBNumber, Ipp32s & iMBToProcess)
{
    UMC::Status umcRes = UMC::UMC_OK;
    Ipp32s iMaxCUNumber = iCurMBNumber + iMBToProcess;

    if (m_pSlice->GetFirstMB() == iCurMBNumber)
    {
        m_pSlice->GetBitStream()->InitializeDecodingEngine_CABAC();
        m_pSlice->InitializeContexts();
    }

    try
    {
        umcRes = m_SD->DecodeSegment(iCurMBNumber, iMaxCUNumber, this);

        iMBToProcess = iMaxCUNumber - iCurMBNumber;

        if ((UMC::UMC_OK == umcRes) ||
            (UMC::UMC_ERR_END_OF_STREAM == umcRes))
        {
            umcRes = UMC::UMC_ERR_END_OF_STREAM;
        }
        
    } catch(...)
    {
        iMBToProcess = iMaxCUNumber - iCurMBNumber;
        throw;
    }
    
    return umcRes;
}

UMC::Status H265SegmentDecoderMultiThreaded::ReconstructSegment(Ipp32s iCurMBNumber, Ipp32s &iMBToProcess)
{
    UMC::Status umcRes = UMC::UMC_OK;
    {
        Ipp32s iMaxCUNumber = iCurMBNumber + iMBToProcess;
        umcRes = m_SD->ReconstructSegment(iCurMBNumber, iMaxCUNumber, this);
        iMBToProcess = iMaxCUNumber - iCurMBNumber;
        return umcRes;
    }
}

UMC::Status H265SegmentDecoderMultiThreaded::DecRecSegment(Ipp32s iCurMBNumber, Ipp32s &)
{
    H265Bitstream  bitstream;
    UMC::Status umcRes = UMC::UMC_OK;

    Ipp32s iFirstPartition = IPP_MAX(m_pSliceHeader->SliceCurStartCUAddr, m_pSliceHeader->m_sliceSegmentCurStartCUAddr);
    Ipp32s iFirstCU = iFirstPartition / m_pCurrentFrame->m_CodingData->m_NumPartitions;
    Ipp32s iMaxCUNumber = m_pCurrentFrame->getNumCUsInFrame();

    Ipp32s firstCU = iFirstCU;
    Ipp32s maxCUNumber = iMaxCUNumber;

    if (m_pSlice->getTileLocationCount() > 1)
    {
        // perform decoding on current row(s)
        Ipp32u numberOfTiles = m_pPicParamSet->num_tile_columns * m_pPicParamSet->num_tile_rows;

        Ipp32s uselessTiles = 0;
        Ipp32s usenessTiles = 0;
        for (Ipp32u i = 0; i < numberOfTiles; i ++)
        {
            firstCU = m_pCurrentFrame->m_CodingData->GetInverseCUOrderMap(m_pPicParamSet->tilesInfo[i].firstCUAddr);
            maxCUNumber = m_pCurrentFrame->m_CodingData->GetInverseCUOrderMap(m_pPicParamSet->tilesInfo[i].endCUAddr) + 1;

            if (iFirstCU >= maxCUNumber)
            {
                uselessTiles++;
                continue;
            }

            usenessTiles++;

            if (usenessTiles != iCurMBNumber)
                continue;

            if (iFirstCU > firstCU)
                firstCU = iFirstCU;

            break;
        }
    }

    m_pBitStream = &bitstream;

    Ipp32u *ptr;
    Ipp32u size;
    m_pSlice->GetBitStream()->GetOrg(&ptr, &size);
    m_pBitStream->Reset((Ipp8u*)ptr, size);
    size_t bsOffset = m_pSlice->getTileLocation(iCurMBNumber - 1);
    m_pBitStream->SetDecodedBytes(bsOffset);

    m_pBitStream->InitializeDecodingEngine_CABAC();
    {
        SliceType slice_type = m_pSlice->GetSliceHeader()->slice_type;

        if (m_pSlice->GetPicParam()->cabac_init_present_flag && m_pSlice->GetSliceHeader()->m_CabacInitFlag)
        {
            switch (slice_type)
            {
                case P_SLICE:
                    slice_type = B_SLICE;
                    break;
                case B_SLICE:
                    slice_type = P_SLICE;
                    break;
                default:
                    VM_ASSERT(0);
            }
        }

        Ipp32s InitializationType;
        if (I_SLICE == slice_type)
        {
            InitializationType = 2;
        }
        else if (P_SLICE == slice_type)
        {
            InitializationType = 1;
        }
        else
        {
            InitializationType = 0;
        }

        m_pBitStream->InitializeContextVariablesHEVC_CABAC(InitializationType, m_pSlice->GetSliceHeader()->SliceQP + m_pSlice->GetSliceHeader()->slice_qp_delta);
    }

    umcRes = m_SD->DecodeSegmentCABAC_Single_H265(firstCU, maxCUNumber, this);

    if (UMC::UMC_OK != umcRes || UMC::UMC_ERR_END_OF_STREAM == umcRes)
    {
        umcRes = UMC::UMC_ERR_END_OF_STREAM;
    }

    return umcRes;

} // Status H265SegmentDecoderMultiThreaded::DecRecSegment(Ipp32s iCurMBNumber, Ipp32s &iMBToReconstruct)

UMC::Status H265SegmentDecoderMultiThreaded::SAOFrameTask(Ipp32s iCurMBNumber, Ipp32s &iMBToProcess)
{
    START_TICK

    m_SAO.SAOProcess(m_pCurrentFrame, iCurMBNumber, iMBToProcess);
    END_TICK(sao_time)
    return UMC::UMC_OK;
}

UMC::Status H265SegmentDecoderMultiThreaded::DeblockSegmentTask(Ipp32s iCurMBNumber, Ipp32s &iMBToProcess)
{
    START_TICK
    // when there is slice groups or threaded deblocking
    if (NULL == m_pSlice)
    {
        H265SegmentDecoder::DeblockFrame(iCurMBNumber, iMBToProcess);
        return UMC::UMC_OK;
    }

    Ipp32s iMaxMBNumber = iCurMBNumber + iMBToProcess;
    DeblockSegment(iCurMBNumber, iMaxMBNumber);
    END_TICK(deblocking_time)

    return UMC::UMC_OK;

} // Status H265SegmentDecoderMultiThreaded::DeblockSegmentTask(Ipp32s iCurMBNumber, Ipp32s &iMBToDeblock)

UMC::Status H265SegmentDecoderMultiThreaded::ProcessSlice(Ipp32s , Ipp32s &)
{
    UMC::Status umcRes = UMC::UMC_OK;

    // Convert slice beginning and end to encode order
    Ipp32s iFirstPartition = IPP_MAX(m_pSliceHeader->SliceCurStartCUAddr, m_pSliceHeader->m_sliceSegmentCurStartCUAddr);
    Ipp32s iFirstCU = iFirstPartition / m_pCurrentFrame->m_CodingData->m_NumPartitions;
    Ipp32s iMaxCUNumber = m_pCurrentFrame->getNumCUsInFrame();

    m_pSlice->GetBitStream()->InitializeDecodingEngine_CABAC();
    m_pSlice->InitializeContexts();
    
    if (m_pSlice->getDependentSliceSegmentFlag())
    {
        Ipp32s CUAddr = m_pCurrentFrame->m_CodingData->getCUOrderMap(iFirstCU);

        size_t tile = m_pCurrentFrame->m_CodingData->getTileIdxMap(CUAddr);
        if (m_pPicParamSet->tilesInfo[tile].firstCUAddr != CUAddr)
        {
            H265Slice * slice = m_pCurrentFrame->GetAU()->GetSliceByNumber(m_pSlice->m_iNumber - 1);
            VM_ASSERT(slice);
            memcpy(m_pSlice->GetBitStream()->context_hevc, slice->GetBitStream()->context_hevc, sizeof(m_pSlice->GetBitStream()->context_hevc));

            if (m_pPicParamSet->entropy_coding_sync_enabled_flag)
            {
                // Copy saved WPP CABAC state too
                memcpy(m_pBitStream->wpp_saved_cabac_context, slice->GetBitStream()->wpp_saved_cabac_context, sizeof(m_pBitStream->wpp_saved_cabac_context));

                // In case previous slice ended on the end of the row and WPP is enabled, CABAC state has to be corrected
                if (CUAddr % m_pSeqParamSet->WidthInCU == 0)
                {
                    // CABAC state is already reset, should only restore contexts
                    if (m_pSeqParamSet->WidthInCU > 1 &&
                        m_pCurrentFrame->m_CodingData->GetInverseCUOrderMap(CUAddr + 1 - m_pSeqParamSet->WidthInCU) >= m_pSliceHeader->SliceCurStartCUAddr / m_pCurrentFrame->m_CodingData->m_NumPartitions)
                    {
                        memcpy(m_pBitStream->context_hevc, m_pBitStream->wpp_saved_cabac_context, sizeof(m_pBitStream->context_hevc));
                    }
                    else
                    {
                        // Initialize here is necessary because otherwise CABAC state remains loaded from previous slice
                        m_pSlice->InitializeContexts();
                    }
                }
            }
            VM_ASSERT(slice->m_iMaxMB - 1);
            m_context->UpdateCurrCUContext(m_pCurrentFrame->m_CodingData->getCUOrderMap(slice->m_iMaxMB - 1), CUAddr);
        }
        else
        {
            m_context->ResetRowBuffer();
        }
    }

    {
        // perform decoding on current row(s)
        umcRes = m_SD->DecodeSegmentCABAC_Single_H265(iFirstCU, iMaxCUNumber, this);

        if ((UMC::UMC_OK == umcRes) ||
            (UMC::UMC_ERR_END_OF_STREAM == umcRes))
            umcRes = UMC::UMC_ERR_END_OF_STREAM;

//        iMBToProcess = m_CurMBAddr - iCurMBNumber;
        return umcRes;
    }
}


SegmentDecoderHPBase_H265* H265SegmentDecoderMultiThreaded::CreateSegmentDecoder()
{
    return CreateSD_H265(
        bit_depth_luma,
        bit_depth_chroma);
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
