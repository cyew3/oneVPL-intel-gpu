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

#include "vm_event.h"
#include "vm_semaphore.h"
#include "vm_thread.h"

#include "h265_tr_quant.h"

#include "umc_h265_bitstream_inlines.h"
#include "umc_h265_segment_decoder_templates.h"
#include "umc_h265_segment_decoder_mt.h"

#include "umc_h265_task_broker.h"
#include "umc_h265_frame_info.h"

#include "umc_h265_task_supplier.h"
#include "umc_h265_frame_list.h"

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
    //h265 segment processing
    m_pSlice = Task.m_pSlice; //from h264, the same use
    m_pSliceHeader = m_pSlice->GetSliceHeader(); //from h264, the same use
    m_pBitStream = m_pSlice->GetBitStream();

    m_pPicParamSet = m_pSlice->GetPicParam();
    m_pSeqParamSet = m_pSlice->GetSeqParam();
    m_pCurrentFrame = m_pSlice->GetCurrentFrame(); //from h264, the same use
    m_SD = CreateSegmentDecoder();

    m_context->Init(&Task);
    this->create((H265SeqParamSet*)m_pSeqParamSet);

    m_Prediction->InitTempBuff(m_context);

    // Pad reference frames that weren't padded yet
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

//void H265SegmentDecoderMultiThreaded::EndProcessingSegment(H265Task &Task, H265SampleAdaptiveOffset* pSAO)
void H265SegmentDecoderMultiThreaded::EndProcessingSegment(H265Task &Task)
{
    this->destroy();
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
                {
                    umcRes = (this->*(Task.pFunction))(firstMB, Task.m_iMBToProcess); //ProcessSlice function
                }

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

UMC::Status H265SegmentDecoderMultiThreaded::SAOFrameTask(Ipp32s , Ipp32s &)
{
    START_TICK

    H265Slice* pSlice = m_pCurrentFrame->GetAU()->GetSliceByNumber(1);

    SAOParams saoParam;
    saoParam.m_bSaoFlag[0] = pSlice->getSaoEnabledFlag();
    saoParam.m_bSaoFlag[1] = pSlice->getSaoEnabledFlagChroma();

    m_SAO.m_saoLcuBasedOptimization = true;
    m_SAO.m_Frame = m_pCurrentFrame;
    m_SAO.createNonDBFilterInfo();

    m_SAO.SAOProcess(m_pCurrentFrame, &saoParam);
    m_SAO.PCMRestoration();

    END_TICK(sao_time)
    return UMC::UMC_OK;

} // Status H265SegmentDecoderMultiThreaded::DeblockSegmentTask(Ipp32s iCurMBNumber, Ipp32s &iMBToDeblock)

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
    Ipp32s iMBRowSize = m_pSlice->GetMBRowWidth();
    Ipp32s iFirstMB = iCurMBNumber;

    // this is a cicle for rows of MBs
    for (; iCurMBNumber < iMaxMBNumber;)
    {
        Ipp32u nBorder;
        Ipp32s iPreCallMBNumber;

        // calculate the last MB in a row
        nBorder = IPP_MIN(iMaxMBNumber,
                      iCurMBNumber -
                      (iCurMBNumber % iMBRowSize) +
                      iMBRowSize);

        // perform the decoding on the current row
        iPreCallMBNumber = iCurMBNumber;

        try
        {
            DeblockSegment(iCurMBNumber, nBorder);
        } catch(...)
        {
            iMBToProcess = iCurMBNumber - iFirstMB;
            throw;
        }

        // check error(s)
        if ((iCurMBNumber >= iMaxMBNumber))
            break;

        // correct the MB number
        iCurMBNumber = iPreCallMBNumber -
                       (iPreCallMBNumber % iMBRowSize) +
                       iMBRowSize;
    }

    END_TICK(deblocking_time)

    return UMC::UMC_OK;

} // Status H265SegmentDecoderMultiThreaded::DeblockSegmentTask(Ipp32s iCurMBNumber, Ipp32s &iMBToDeblock)

UMC::Status H265SegmentDecoderMultiThreaded::ProcessSlice(Ipp32s iCurMBNumber, Ipp32s &)
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
            m_context->UpdateCurrCUContext(CUAddr - 1, CUAddr);
        }
        else
        {
            m_pSlice->InitializeContexts();
            m_context->ResetRowBuffer();
        }
    }

    Ipp32s iFirstMBToDeblock;
//    Ipp32s iAvailableMBToDeblock = 0;

     // set deblocking condition
    //bDoDeblocking = m_pSlice->GetDeblockingCondition();
    iFirstMBToDeblock = iCurMBNumber;

    //m_psBuffer = align_pointer<UMC::CoeffsPtrCommon> (m_pCoefficientsBuffer, DEFAULT_ALIGN_VALUE);

    // separate decoding and deblocking (tmp)
    {
        // perform decoding on current row(s)
        umcRes = m_SD->DecodeSegmentCABAC_Single_H265(iFirstCU, iMaxCUNumber, this);

        if ((UMC::UMC_OK == umcRes) ||
            (UMC::UMC_ERR_END_OF_STREAM == umcRes))
            umcRes = UMC::UMC_ERR_END_OF_STREAM;

//        iMBToProcess = m_CurMBAddr - iCurMBNumber;
        return umcRes;
    }

/*    // this is a cicle for rows of MBs
    for (; iCurMBNumber < iMaxMBNumber;)
    {
        Ipp32u nBorder;
        Ipp32s iPreCallMBNumber;

        // calculate the last MB in a row
        nBorder = IPP_MIN(iMaxMBNumber,
                      iCurMBNumber -
                      (iCurMBNumber % iMBRowSize) +
                      iMBRowSize);

        // perform the decoding on the current row
        iPreCallMBNumber = iCurMBNumber;

        try
        {
                umcRes = m_SD->DecodeSegmentCABAC_Single_H265(iCurMBNumber, nBorder, this);
        } catch(...)
        {
            iMBToProcess = m_CurMBAddr - iFirstMB;
            throw;
        }

        // sum count of MB to deblock
        iAvailableMBToDeblock += nBorder - iPreCallMBNumber;
        // check error(s)
        if ((UMC_OK != umcRes) ||
            (m_CurMBAddr >= iMaxMBNumber))
            break;

        // correct the MB number in a MBAFF case
        iCurMBNumber = iPreCallMBNumber -
                       (iPreCallMBNumber % iMBRowSize) +
                       iMBRowSize;

        // perform a deblocking on previous row(s)
        if (iCurMBNumber < iMaxMBNumber)
        {
            if ((bDoDeblocking) &&
                (iMBRowSize < iAvailableMBToDeblock))
            {
                Ipp32s iToDeblock = iAvailableMBToDeblock - iMBRowSize;

                DeblockSegmentTask(iFirstMBToDeblock, iToDeblock);
                iFirstMBToDeblock += iToDeblock;
                iAvailableMBToDeblock -= iToDeblock;
                m_cur_mb.isInited = false; // reset variables
            }
        }
    }

    if ((UMC_OK == umcRes) ||
        (UMC_ERR_END_OF_STREAM == umcRes))
    {
        iMBToProcess = m_CurMBAddr - iFirstMB;

        // perform deblocking of remain MBs
        if (bDoDeblocking)
            DeblockSegmentTask(iFirstMBToDeblock, iAvailableMBToDeblock);

        // in any case it is end of slice
        umcRes = UMC_ERR_END_OF_STREAM;
    }
*/
}


SegmentDecoderHPBase_H265* H265SegmentDecoderMultiThreaded::CreateSegmentDecoder()
{
    return CreateSD_H265(
        bit_depth_luma,
        bit_depth_chroma);
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
