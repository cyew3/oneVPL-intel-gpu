// Copyright (c) 2012-2019 Intel Corporation
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
#ifdef MFX_ENABLE_H265_VIDEO_DECODE
#ifndef MFX_VA

#include "umc_h265_segment_decoder_mt.h"

#include "umc_h265_task_broker.h"
#include "umc_h265_frame_info.h"

#include "umc_h265_task_supplier.h"
#include "umc_h265_tr_quant.h"

#include "mfx_trace.h"
#include "umc_h265_debug.h"
#include "umc_h265_ipplevel.h"

#include "mfx_h265_dispatcher.h"

namespace UMC_HEVC_DECODER
{
H265SegmentDecoderMultiThreaded::H265SegmentDecoderMultiThreaded(TaskBroker_H265 * pTaskBroker)
    : H265SegmentDecoder(pTaskBroker)
{
} // H265SegmentDecoderMultiThreaded::H265SegmentDecoderMultiThreaded(TaskBroker_H265 * pTaskBroker)

H265SegmentDecoderMultiThreaded::~H265SegmentDecoderMultiThreaded(void)
{
    Release();

} // H265SegmentDecoderMultiThreaded::~H265SegmentDecoderMultiThreaded(void)

// Initialize slice decoder
UMC::Status H265SegmentDecoderMultiThreaded::Init(int32_t iNumber)
{
    // release object before initialization
    Release();

    // call parent's Init
    return H265SegmentDecoder::Init(iNumber);
} // Status H265SegmentDecoderMultiThreaded::Init(uint32_t nNumber)

// Initialize decoder with data of new slice
void H265SegmentDecoderMultiThreaded::StartProcessingSegment(H265Task &Task)
{
    m_pSlice = Task.m_pSlice;
    m_pSliceHeader = m_pSlice->GetSliceHeader();
    m_pBitStream = m_pSlice->GetBitStream();

    m_pPicParamSet = m_pSlice->GetPicParam();
    m_pSeqParamSet = m_pSlice->GetSeqParam();
    m_pCurrentFrame = m_pSlice->GetCurrentFrame();
    m_bIsNeedWADeblocking = m_pCurrentFrame->GetAU()->IsNeedWorkAroundForDeblocking();
    m_hasTiles = Task.m_pSlicesInfo->m_hasTiles;

    m_ChromaArrayType = m_pSeqParamSet->ChromaArrayType;

    CreateReconstructor();

    m_DecodeDQPFlag = false;
    m_IsCuChromaQpOffsetCoded = false;
    m_minCUDQPSize = m_pSeqParamSet->MaxCUSize >> m_pPicParamSet->diff_cu_qp_delta_depth;

    m_context = Task.m_context;
    if (!m_context && (Task.m_iTaskID == TASK_DEC_REC_H265 || Task.m_iTaskID == TASK_PROCESS_H265))
    {
        m_context = m_context_single_thread.get();
        m_context->Init(&Task);
        Task.m_pBuffer = (CoeffsPtr)m_context->m_coeffBuffer.LockInputBuffer();
    }

    this->create((H265SeqParamSet*)m_pSeqParamSet);

    if (m_context)
    {
        switch (Task.m_iTaskID)
        {
        case TASK_DEC_H265:
        case TASK_DEC_REC_H265:
        case TASK_PROCESS_H265:
            VM_ASSERT(!m_context->m_coeffsWrite);
            m_context->m_coeffsWrite = Task.m_pBuffer;
            break;
        case TASK_REC_H265:
            VM_ASSERT(!m_context->m_coeffsRead);
            m_context->m_coeffsRead = Task.m_pBuffer;
            break;
        }

        m_TrQuant->m_context = m_context;
        m_Prediction->InitTempBuff(m_context);

        if (m_pSeqParamSet->scaling_list_enabled_flag)
        {
            if (m_pPicParamSet->pps_scaling_list_data_present_flag || !m_pSeqParamSet->sps_scaling_list_data_present_flag)
            {
                m_context->m_dequantCoef = &m_pPicParamSet->getScalingList()->m_dequantCoef;
            }
            else
            {
                m_context->m_dequantCoef = &m_pSeqParamSet->getScalingList()->m_dequantCoef;
            }
        }
    }

    int32_t sliceNum = m_pSlice->GetSliceNum();

    m_pRefPicList[0] = m_pCurrentFrame->GetRefPicList(sliceNum, REF_PIC_LIST_0)->m_refPicList;
    m_pRefPicList[1] = m_pCurrentFrame->GetRefPicList(sliceNum, REF_PIC_LIST_1)->m_refPicList;
}

// Finish work section
void H265SegmentDecoderMultiThreaded::EndProcessingSegment(H265Task &Task)
{
    this->destroy();

    if (TASK_DEC_H265 == Task.m_iTaskID)
    {
        int32_t mvsDistortion = m_context->m_mvsDistortionTemp;
        mvsDistortion = (mvsDistortion + 6) >> 2; // remove 1/2 pel
        m_context->m_mvsDistortionTemp = MFX_MAX(mvsDistortion, m_context->m_mvsDistortion);

        Task.m_WrittenSize = (uint8_t*)m_context->m_coeffsWrite - (uint8_t*)Task.m_pBuffer;
    }

    switch (Task.m_iTaskID)
    {
    case TASK_DEC_H265:
    case TASK_DEC_REC_H265:
    case TASK_PROCESS_H265:
        m_context->m_coeffsWrite = 0;
        break;
    case TASK_REC_H265:
        m_context->m_coeffsRead = 0;
        break;
    }

    m_pTaskBroker->AddPerformedTask(&Task);
}

UMC::Status H265SegmentDecoderMultiThreaded::ProcessTask(H265Task &task)
{
    switch(task.m_iTaskID)
    {
    case TASK_PROCESS_H265:
        return ProcessSlice(task);
    case TASK_DEB_H265:
        return DeblockSegmentTask(task);
    case TASK_SAO_H265:
        return SAOFrameTask(task);
    case TASK_DEC_REC_H265:
        return DecRecSegment(task);
    case TASK_REC_H265:
        return ReconstructSegment(task);
    case TASK_DEC_H265:
        return DecodeSegment(task);
    }

    return UMC::UMC_ERR_FAILED;
}

// Initialize decoder and call task processing function
UMC::Status H265SegmentDecoderMultiThreaded::ProcessSegment(void)
{
#ifdef __EXCEPTION_HANDLER_
    exceptionHandler.setTranslator();
#endif

    H265Task task(m_iNumber);
    try
    {
        if (m_pTaskBroker->GetNextTask(&task))
        {
            UMC::Status umcRes = UMC::UMC_OK;

            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "HEVCDec_work");

            StartProcessingSegment(task);

            try // do decoding
            {
                umcRes = ProcessTask(task);

                if (UMC::UMC_ERR_END_OF_STREAM == umcRes)
                {
                    task.m_iMaxMB = task.m_iFirstMB + task.m_iMBToProcess;
                    //// if we decode less macroblocks if we need try to recovery:
                    RestoreErrorRect(&task);
                    umcRes = UMC::UMC_OK;
                }
                else if (UMC::UMC_OK != umcRes)
                {
                    umcRes = UMC::UMC_ERR_INVALID_STREAM;
                }
            } catch(const h265_exception & ex)
            {
                umcRes = ex.GetStatus();
            } catch(...)
            {
                umcRes = UMC::UMC_ERR_INVALID_STREAM;
            }

            if (umcRes != UMC::UMC_OK)
            {
                task.m_bError = true;
                task.m_iMaxMB = task.m_iFirstMB + task.m_iMBToProcess;
                if (m_pSlice) //restoreerror
                    RestoreErrorRect(&task);
            }

            EndProcessingSegment(task);
            MFX_LTRACE_I(MFX_TRACE_LEVEL_INTERNAL, umcRes);
        }
        else
        {
            return UMC::UMC_ERR_NOT_ENOUGH_DATA;
        }
    }
    catch (h265_exception ex)
    {
        return ex.GetStatus();
    }
    catch(...)
    {
        return UMC::UMC_ERR_INVALID_STREAM;
    }

    return UMC::UMC_OK;
} // Status H265SegmentDecoderMultiThreaded::ProcessSegment(void)

// Recover a region after error
void H265SegmentDecoderMultiThreaded::RestoreErrorRect(H265Task * task)
{
    H265Slice * pSlice = task->m_pSlice;
    int32_t startMb = task->m_iFirstMB + task->m_iMBToProcess;
    if (!pSlice || startMb >= pSlice->GetMaxMB())
        return;

    int32_t endMb = pSlice->GetMaxMB();
    m_pSlice = pSlice;
    task->m_bError = true;

    try
    {
        H265DecoderFrame * pCurrentFrame = m_pSlice->GetCurrentFrame();

        if (!pCurrentFrame || !pCurrentFrame->m_pYPlane)
        {
            return;
        }

        if (startMb > task->m_iFirstMB)
            startMb--;

        for (int32_t i = startMb; i < endMb; i++)
        {
            int32_t rsCUAddr = pCurrentFrame->m_CodingData->getCUOrderMap(i);
            H265CodingUnit* cu = pCurrentFrame->getCU(rsCUAddr);
            cu->initCU(this, rsCUAddr);
            cu->setCbfSubParts(0, 0, 0, 0, 0);
            cu->m_Frame = 0;

            int32_t CUX = (cu->m_CUPelX >> m_pSeqParamSet->log2_min_transform_block_size);
            int32_t CUY = (cu->m_CUPelY >> m_pSeqParamSet->log2_min_transform_block_size);

            H265MVInfo *colInfo = pCurrentFrame->m_CodingData->m_colocatedInfo + CUY * m_pSeqParamSet->NumPartitionsInFrameWidth + CUX;

            for (uint32_t k = 0; k < m_pSeqParamSet->NumPartitionsInCUSize; k++)
            {
                for (uint32_t j = 0; j < m_pSeqParamSet->NumPartitionsInCUSize; j++)
                {
                    colInfo[j].m_refIdx[REF_PIC_LIST_0] = -1;
                    colInfo[j].m_refIdx[REF_PIC_LIST_1] = -1;
                }
                colInfo += m_pSeqParamSet->NumPartitionsInFrameWidth;
            }
        }

        H265DecoderFrame * refFrame = pCurrentFrame->GetRefPicList(m_pSlice->GetSliceNum(), 0)->m_refPicList[0].refFrame;

        pCurrentFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);

        if (!refFrame)
        {
            H265DBPList *dbpList = m_pTaskBroker->m_pTaskSupplier->GetDPBList();
            if (dbpList)
                refFrame = dbpList->FindClosest(pCurrentFrame);
        }

        CreateReconstructor();
        RestoreErrorRect(startMb, endMb, refFrame);
    } catch (...)
    {
        // nothing to do
    }

    return;
}

void H265SegmentDecoderMultiThreaded::RestoreErrorRect(int32_t startMb, int32_t endMb, H265DecoderFrame *refFrame)
{
    if (startMb > 0)
        startMb--;

    H265DecoderFrame * pCurrentFrame = m_pSlice->GetCurrentFrame();
    int32_t mb_width = m_pSlice->GetSeqParam()->WidthInCU;

    mfxSize cuSize;
    cuSize.width = cuSize.height = m_pSlice->GetSeqParam()->MaxCUSize;
    mfxSize picSize = {static_cast<int>(m_pSlice->GetSeqParam()->pic_width_in_luma_samples), static_cast<int>(m_pSlice->GetSeqParam()->pic_height_in_luma_samples)};

    int32_t offsetX, offsetY;
    offsetX = (startMb % mb_width) * cuSize.height;
    offsetY = (startMb / mb_width) * cuSize.height;

    int32_t offsetXL = ((endMb - 1) % mb_width) * cuSize.height;
    int32_t offsetYL = ((endMb - 1) / mb_width) * cuSize.height;

    if (refFrame && refFrame->m_pYPlane)
    {
        m_reconstructor->CopyPartOfFrameFromRef((PlanePtrY)refFrame->m_pYPlane, (PlanePtrY)pCurrentFrame->m_pYPlane, pCurrentFrame->pitch_luma(),
                offsetX, offsetY, offsetXL, offsetYL,
                cuSize, picSize);
    }
    else
    {
        m_reconstructor->CopyPartOfFrameFromRef(0, (PlanePtrY)pCurrentFrame->m_pYPlane, pCurrentFrame->pitch_luma(),
                offsetX, offsetY, offsetXL, offsetYL,
                cuSize, picSize);
    }

    if (!pCurrentFrame->m_pVPlane)
    {
        switch (pCurrentFrame->m_chroma_format)
        {
        case CHROMA_FORMAT_420: // YUV420
            offsetY >>= 1;
            offsetYL >>= 1;

            cuSize.width >>= 1;
            cuSize.height >>= 1;

            picSize.height >>= 1;
            break;
        case CHROMA_FORMAT_422: // YUV422
            //cuSize.width >>= 1;
            break;
        case CHROMA_FORMAT_444: // YUV444
            break;

        case CHROMA_FORMAT_400: // YUV400
            return;
        default:
            VM_ASSERT(false);
            return;
        }

        if (refFrame && refFrame->m_pUVPlane)
        {
            m_reconstructor->CopyPartOfFrameFromRef((PlanePtrUV)refFrame->m_pUVPlane, (PlanePtrUV)pCurrentFrame->m_pUVPlane, pCurrentFrame->pitch_chroma(),
                    offsetX, offsetY, offsetXL, offsetYL,
                    cuSize, picSize);
        }
        else
        {
            m_reconstructor->CopyPartOfFrameFromRef(0, (PlanePtrUV)pCurrentFrame->m_pUVPlane, pCurrentFrame->pitch_chroma(),
                    offsetX, offsetY, offsetXL, offsetYL,
                    cuSize, picSize);
        }
    }
    else
    {
        switch (pCurrentFrame->m_chroma_format)
        {
        case CHROMA_FORMAT_420: // YUV420
            offsetX >>= 1;
            offsetY >>= 1;
            offsetXL >>= 1;
            offsetYL >>= 1;

            cuSize.width >>= 1;
            cuSize.height >>= 1;

            picSize.width >>= 1;
            picSize.height >>= 1;
            break;
        case CHROMA_FORMAT_422: // YUV422
            offsetX >>= 1;
            offsetXL >>= 1;

            cuSize.width >>= 1;

            picSize.width >>= 1;
            break;
        case CHROMA_FORMAT_444: // YUV444
            break;

        case CHROMA_FORMAT_400: // YUV400
            return;
        default:
            VM_ASSERT(false);
            return;
        }

        if (refFrame && refFrame->m_pUPlane && refFrame->m_pVPlane)
        {
            m_reconstructor->CopyPartOfFrameFromRef((PlanePtrUV)refFrame->m_pUPlane, (PlanePtrUV)pCurrentFrame->m_pUPlane, pCurrentFrame->pitch_chroma(),
                    offsetX, offsetY, offsetXL, offsetYL,
                    cuSize, picSize);
            m_reconstructor->CopyPartOfFrameFromRef((PlanePtrUV)refFrame->m_pVPlane, (PlanePtrUV)pCurrentFrame->m_pVPlane, pCurrentFrame->pitch_chroma(),
                    offsetX, offsetY, offsetXL, offsetYL,
                    cuSize, picSize);
        }
        else
        {
            m_reconstructor->CopyPartOfFrameFromRef(0, (PlanePtrUV)pCurrentFrame->m_pUPlane, pCurrentFrame->pitch_chroma(),
                    offsetX, offsetY, offsetXL, offsetYL,
                    cuSize, picSize);
            m_reconstructor->CopyPartOfFrameFromRef(0, (PlanePtrUV)pCurrentFrame->m_pVPlane, pCurrentFrame->pitch_chroma(),
                    offsetX, offsetY, offsetXL, offsetYL,
                    cuSize, picSize);
        }
    }
}

// Initialize CABAC context appropriately depending on where starting CTB is
void H265SegmentDecoderMultiThreaded::InitializeDecoding(H265Task & task)
{
    if (task.m_pSlicesInfo->m_hasTiles)
    {
        m_pBitStream = &m_context->m_BitStream;

        if (task.m_threadingInfo->processInfo.firstCU == task.m_iFirstMB || m_pSlice->GetFirstMB() == task.m_iFirstMB) // need to initialize bitstream
        {
            int32_t sliceTileNumber = 0;

            if (m_pSlice->getTileLocationCount() > 1)
            { // need to find tile of slice and initialize bitstream
                uint32_t numberOfTiles = m_pPicParamSet->getNumTiles();
                int32_t iFirstPartition = MFX_MAX(m_pSliceHeader->SliceCurStartCUAddr, (int32_t)m_pSliceHeader->m_sliceSegmentCurStartCUAddr);
                int32_t firstSliceCU = iFirstPartition / m_pCurrentFrame->m_CodingData->m_NumPartitions;

                int32_t uselessTiles = 0;
                int32_t usenessTiles = 0;
                for (uint32_t i = 0; i < numberOfTiles; i ++)
                {
                    int32_t maxCUNumber = task.m_pSlicesInfo->m_tilesThreadingInfo[i].processInfo.maxCU;

                    if (firstSliceCU > maxCUNumber)
                    {
                        uselessTiles++;
                        continue;
                    }

                    if (task.m_iFirstMB > maxCUNumber)
                    {
                        usenessTiles++;
                        continue;
                    }

                    break;
                }

                sliceTileNumber = usenessTiles;
            }

            uint32_t *ptr;
            uint32_t size;
            m_pSlice->GetBitStream()->GetOrg(&ptr, &size);
            m_pBitStream->Reset((uint8_t*)ptr, size);
            size_t bsOffset = m_pSlice->m_tileByteLocation[sliceTileNumber];
            m_pBitStream->SetDecodedBytes(bsOffset);

            m_pBitStream->InitializeDecodingEngine_CABAC();
            {
                SliceType slice_type = m_pSlice->GetSliceHeader()->slice_type;

                if (m_pSlice->GetPicParam()->cabac_init_present_flag && m_pSlice->GetSliceHeader()->cabac_init_flag)
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

                int32_t InitializationType;
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

            if (!m_pSliceHeader->dependent_slice_segment_flag)
            {
                m_context->ResetRowBuffer();
                m_context->SetNewQP(m_pSliceHeader->SliceQP);
                m_context->ResetRecRowBuffer();
            }
        }
    }
    else
    {
        if (m_pSlice->GetFirstMB() == task.m_iFirstMB)
        {
            m_pSlice->GetBitStream()->InitializeDecodingEngine_CABAC();
            m_pSlice->InitializeContexts();
            m_context->ResetRecRowBuffer();
        }
    }
}

// Decode task callback
UMC::Status H265SegmentDecoderMultiThreaded::DecodeSegment(H265Task & task)
{
    UMC::Status umcRes = UMC::UMC_OK;
    int32_t iMaxCUNumber = task.m_iFirstMB + task.m_iMBToProcess;

    InitializeDecoding(task);

    try
    {
        umcRes = DecodeSegment(task.m_iFirstMB, iMaxCUNumber);
        task.m_iMBToProcess = iMaxCUNumber - task.m_iFirstMB;
    } catch(...)
    {
        task.m_iMBToProcess = iMaxCUNumber - task.m_iFirstMB;
        throw;
    }

    return umcRes;
}

// Reconstruct task callback
UMC::Status H265SegmentDecoderMultiThreaded::ReconstructSegment(H265Task & task)
{
    UMC::Status umcRes = UMC::UMC_OK;

    {
        int32_t iMaxCUNumber = task.m_iFirstMB + task.m_iMBToProcess;
        umcRes = ReconstructSegment(task.m_iFirstMB, iMaxCUNumber);
        task.m_iMBToProcess = iMaxCUNumber - task.m_iFirstMB;
        return umcRes;
    }
}

// Combined decode and reconstruct task callback
UMC::Status H265SegmentDecoderMultiThreaded::DecRecSegment(H265Task & task)
{
    InitializeDecoding(task);

    UMC::Status umcRes = UMC::UMC_OK;
    int32_t iMaxCUNumber = task.m_iFirstMB + task.m_iMBToProcess;

    try
    {
        umcRes = DecodeSegmentCABAC_Single_H265(task.m_iFirstMB, iMaxCUNumber);
        task.m_iMBToProcess = iMaxCUNumber - task.m_iFirstMB;
    } catch(...)
    {
        task.m_iMBToProcess = iMaxCUNumber - task.m_iFirstMB;
        throw;
    }

    return umcRes;
}

// SAO filter task callback
UMC::Status H265SegmentDecoderMultiThreaded::SAOFrameTask(H265Task & task)
{
#ifndef MFX_VA
    m_pCurrentFrame->getCD()->m_SAO.SAOProcess(m_pCurrentFrame, task.m_iFirstMB, task.m_iMBToProcess);
#endif
    return UMC::UMC_OK;
}

// Deblocking filter task callback
UMC::Status H265SegmentDecoderMultiThreaded::DeblockSegmentTask(H265Task & task)
{
    // when there is slice groups or threaded deblocking
    DeblockSegment(task);
    return UMC::UMC_OK;

} // Status H265SegmentDecoderMultiThreaded::DeblockSegmentTask(int32_t iCurMBNumber, int32_t &iMBToDeblock)

// Initialize state, decode, reconstruct and filter CTB range
UMC::Status H265SegmentDecoderMultiThreaded::ProcessSlice(H265Task & task)
{
    UMC::Status umcRes = UMC::UMC_OK;

    // Convert slice beginning and end to encode order
    int32_t iFirstPartition = MFX_MAX(m_pSliceHeader->SliceCurStartCUAddr, (int32_t)m_pSliceHeader->m_sliceSegmentCurStartCUAddr);
    int32_t iFirstCU = iFirstPartition / m_pCurrentFrame->m_CodingData->m_NumPartitions;
    int32_t iMaxCUNumber = m_pCurrentFrame->getNumCUsInFrame();

    m_pSlice->GetBitStream()->InitializeDecodingEngine_CABAC();
    m_pSlice->InitializeContexts();

    if (m_pSliceHeader->dependent_slice_segment_flag)
    {
        int32_t CUAddr = m_pCurrentFrame->m_CodingData->getCUOrderMap(iFirstCU);

        size_t tile = m_pCurrentFrame->m_CodingData->getTileIdxMap(CUAddr);
        if (m_pPicParamSet->tilesInfo[tile].firstCUAddr != CUAddr)
        {
            H265Slice * slice = m_pCurrentFrame->GetAU()->GetSliceByNumber(m_pSlice->m_iNumber - 1);
            if (!slice)
                throw UMC::UMC_ERR_INVALID_STREAM;

            MFX_INTERNAL_CPY(m_pSlice->GetBitStream()->context_hevc, slice->GetBitStream()->context_hevc, sizeof(m_pSlice->GetBitStream()->context_hevc));

            if (m_pPicParamSet->entropy_coding_sync_enabled_flag)
            {
                // Copy saved WPP CABAC state too
                MFX_INTERNAL_CPY(m_pBitStream->wpp_saved_cabac_context, slice->GetBitStream()->wpp_saved_cabac_context, sizeof(m_pBitStream->wpp_saved_cabac_context));

                // In case previous slice ended on the end of the row and WPP is enabled, CABAC state has to be corrected
                if (CUAddr % m_pSeqParamSet->WidthInCU == 0)
                {
                    m_context->SetNewQP(m_pSliceHeader->SliceQP);
                    // CABAC state is already reset, should only restore contexts
                    if (m_pSeqParamSet->WidthInCU > 1 &&
                        m_pCurrentFrame->m_CodingData->GetInverseCUOrderMap(CUAddr + 1 - m_pSeqParamSet->WidthInCU) >= m_pSliceHeader->SliceCurStartCUAddr / m_pCurrentFrame->m_CodingData->m_NumPartitions)
                    {
                        MFX_INTERNAL_CPY(m_pBitStream->context_hevc, m_pBitStream->wpp_saved_cabac_context, sizeof(m_pBitStream->context_hevc));
                    }
                    else
                    {
                        // Initialize here is necessary because otherwise CABAC state remains loaded from previous slice
                        m_pSlice->InitializeContexts();
                    }
                }
            }

            VM_ASSERT(slice->m_iMaxMB >= 1);
            m_context->UpdateCurrCUContext(m_pCurrentFrame->m_CodingData->getCUOrderMap(slice->m_iMaxMB - 1), CUAddr);
            m_context->UpdateRecCurrCTBContext(m_pCurrentFrame->m_CodingData->getCUOrderMap(slice->m_iMaxMB - 1), CUAddr);
        }
        else
        {
            m_context->ResetRowBuffer();
            m_context->SetNewQP(m_pSliceHeader->SliceQP);
            m_context->ResetRecRowBuffer();
        }
    }

    if (!m_pSliceHeader->dependent_slice_segment_flag)
    {
        m_context->ResetRowBuffer();
        m_context->SetNewQP(m_pSliceHeader->SliceQP);
        m_context->ResetRecRowBuffer();
    }

    try
    {
        umcRes = DecodeSegmentCABAC_Single_H265(iFirstCU, iMaxCUNumber);

        task.m_iMBToProcess = iMaxCUNumber - task.m_iFirstMB;

        if (UMC::UMC_OK == umcRes || UMC::UMC_ERR_END_OF_STREAM == umcRes)
        {
            umcRes = UMC::UMC_ERR_END_OF_STREAM;
        }

    } catch(...)
    {
        task.m_iMBToProcess = iMaxCUNumber - task.m_iFirstMB;
        throw;
    }

    return umcRes;
}

// Decode one CTB
bool H265SegmentDecoderMultiThreaded::DecodeCodingUnit_CABAC()
{
    if (m_pPicParamSet->cu_qp_delta_enabled_flag)
    {
        m_DecodeDQPFlag = true;
    }

    uint32_t IsLast = 0;
    DecodeCUCABAC(0, 0, IsLast);
    return IsLast > 0;
} // void DecodeCodingUnit_CABAC(H265SegmentDecoderMultiThreaded *sd)

// Decode CTB range
UMC::Status H265SegmentDecoderMultiThreaded::DecodeSegment(int32_t curCUAddr, int32_t &nBorder)
{
    UMC::Status umcRes = UMC::UMC_OK;
    int32_t rsCUAddr = m_pCurrentFrame->m_CodingData->getCUOrderMap(curCUAddr);

    for (;;)
    {
        m_cu = m_pCurrentFrame->getCU(rsCUAddr);
        m_cu->initCU(this, rsCUAddr);

        DecodeSAOOneLCU();
        bool is_last = DecodeCodingUnit_CABAC(); //decode CU

        if (is_last)
        {
            umcRes = UMC::UMC_ERR_END_OF_STREAM;
            nBorder = curCUAddr + 1;
            if (m_pPicParamSet->entropy_coding_sync_enabled_flag && rsCUAddr % m_pSeqParamSet->WidthInCU == 1)
            {
                // Save CABAC context after 2nd CTB
                MFX_INTERNAL_CPY(m_pBitStream->wpp_saved_cabac_context, m_pBitStream->context_hevc, sizeof(m_pBitStream->context_hevc));
            }
            break;
        }

        int32_t newCUAddr = curCUAddr + 1;
        int32_t newRSCUAddr = m_pCurrentFrame->m_CodingData->getCUOrderMap(newCUAddr);

        if (newRSCUAddr >= m_pCurrentFrame->m_CodingData->m_NumCUsInFrame ||
            m_pCurrentFrame->m_CodingData->getTileIdxMap(rsCUAddr) != m_pCurrentFrame->m_CodingData->getTileIdxMap(newRSCUAddr))
            break;

        if (m_pPicParamSet->entropy_coding_sync_enabled_flag)
        {
            uint32_t CUX = rsCUAddr % m_pSeqParamSet->WidthInCU;
            bool end_of_row = (CUX == m_pSeqParamSet->WidthInCU - 1);

            if (end_of_row)
            {
                uint32_t uVal = m_pBitStream->DecodeTerminatingBit_CABAC();
                VM_ASSERT(uVal);
                (void)uVal;
            }

            if (CUX == 1)
            {
                // Save CABAC context after 2nd CTB
                MFX_INTERNAL_CPY(m_pBitStream->wpp_saved_cabac_context, m_pBitStream->context_hevc, sizeof(m_pBitStream->context_hevc));
            }

            if (end_of_row)
            {
                // Reset CABAC state
                m_pBitStream->InitializeDecodingEngine_CABAC();
                m_context->SetNewQP(m_pSliceHeader->SliceQP);

                // Should load CABAC context from saved buffer
                if (m_pSeqParamSet->WidthInCU > 1 &&
                    m_pCurrentFrame->m_CodingData->GetInverseCUOrderMap(rsCUAddr + 2 - m_pSeqParamSet->WidthInCU) >= m_pSliceHeader->SliceCurStartCUAddr / m_pCurrentFrame->m_CodingData->m_NumPartitions)
                {
                    // Restore saved CABAC context
                    MFX_INTERNAL_CPY(m_pBitStream->context_hevc, m_pBitStream->wpp_saved_cabac_context, sizeof(m_pBitStream->context_hevc));
                }
                else
                {
                    // Reset CABAC contexts
                    m_pSlice->InitializeContexts();
                }
            }
        }

        m_context->UpdateCurrCUContext(rsCUAddr, newRSCUAddr);

        if (newCUAddr >= nBorder)
        {
            break;
        }

        curCUAddr = newCUAddr;
        rsCUAddr = newRSCUAddr;
    }

    return umcRes;
}

// Reconstruct CTB range
UMC::Status H265SegmentDecoderMultiThreaded::ReconstructSegment(int32_t curCUAddr, int32_t nBorder)
{
    UMC::Status umcRes = UMC::UMC_OK;
    int32_t rsCUAddr = m_pCurrentFrame->m_CodingData->getCUOrderMap(curCUAddr);

    m_cu = m_pCurrentFrame->getCU(rsCUAddr);

    for (;;)
    {
        ReconstructCU(0, 0);

        curCUAddr++;
        int32_t newRSCUAddr = m_pCurrentFrame->m_CodingData->getCUOrderMap(curCUAddr);

        if (newRSCUAddr >= m_pCurrentFrame->m_CodingData->m_NumCUsInFrame)
            break;

        m_context->UpdateRecCurrCTBContext(rsCUAddr, newRSCUAddr);

        if (curCUAddr >= nBorder)
        {
            break;
        }

        rsCUAddr = newRSCUAddr;
        m_cu = m_pCurrentFrame->getCU(rsCUAddr);
    }

    return umcRes;
}

// Both decode and reconstruct a CTB range
UMC::Status H265SegmentDecoderMultiThreaded::DecodeSegmentCABAC_Single_H265(int32_t curCUAddr, int32_t & nBorder)
{
    UMC::Status umcRes = UMC::UMC_OK;
    int32_t rsCUAddr = m_pCurrentFrame->m_CodingData->getCUOrderMap(curCUAddr);

    CoeffsPtr saveBuffer = m_context->m_coeffsWrite;

    for (;;)
    {
        m_cu = m_pCurrentFrame->getCU(rsCUAddr);
        m_cu->initCU(this, rsCUAddr);

        m_context->m_coeffsRead = saveBuffer;
        m_context->m_coeffsWrite = saveBuffer;

        DecodeSAOOneLCU();
        bool is_last = DecodeCodingUnit_CABAC(); //decode CU

        ReconstructCU(0, 0);

        if (is_last)
        {
            umcRes = UMC::UMC_ERR_END_OF_STREAM;
            nBorder = curCUAddr + 1;

            if (m_pPicParamSet->entropy_coding_sync_enabled_flag && rsCUAddr % m_pSeqParamSet->WidthInCU == 1)
            {
                // Save CABAC context after 2nd CTB
                MFX_INTERNAL_CPY(m_pBitStream->wpp_saved_cabac_context, m_pBitStream->context_hevc, sizeof(m_pBitStream->context_hevc));
            }
            break;
        }

        int32_t newCUAddr = curCUAddr + 1;
        int32_t newRSCUAddr = m_pCurrentFrame->m_CodingData->getCUOrderMap(newCUAddr);

        if (newRSCUAddr >= m_pCurrentFrame->m_CodingData->m_NumCUsInFrame)
            break;

        if (m_pCurrentFrame->m_CodingData->getTileIdxMap(rsCUAddr) !=
            m_pCurrentFrame->m_CodingData->getTileIdxMap(newRSCUAddr))
        {
            m_context->ResetRowBuffer();
            m_context->SetNewQP(m_pSliceHeader->SliceQP);
            m_context->ResetRecRowBuffer();
            m_pBitStream->DecodeTerminatingBit_CABAC();

            // reset CABAC engine
            m_pBitStream->InitializeDecodingEngine_CABAC();
            m_pSlice->InitializeContexts();
        }
        else
        {
            if (m_pPicParamSet->entropy_coding_sync_enabled_flag)
            {
                uint32_t CUX = rsCUAddr % m_pSeqParamSet->WidthInCU;
                bool end_of_row = (CUX == m_pSeqParamSet->WidthInCU - 1);

                if (end_of_row)
                {
                    uint32_t uVal = m_pBitStream->DecodeTerminatingBit_CABAC();
                    if (!uVal)
                    {
                        m_pSlice->m_bError = true;
                    }
                }

                if (CUX == 1)
                {
                    // Save CABAC context after 2nd CTB
                    MFX_INTERNAL_CPY(m_pBitStream->wpp_saved_cabac_context, m_pBitStream->context_hevc, sizeof(m_pBitStream->context_hevc));
                }

                if (end_of_row)
                {
                    // Reset CABAC state
                    m_pBitStream->InitializeDecodingEngine_CABAC();
                    m_context->SetNewQP(m_pSliceHeader->SliceQP);

                    // Should load CABAC context from saved buffer
                    if (m_pSeqParamSet->WidthInCU > 1 &&
                        m_pCurrentFrame->m_CodingData->GetInverseCUOrderMap(rsCUAddr + 2 - m_pSeqParamSet->WidthInCU) >= m_pSliceHeader->SliceCurStartCUAddr / m_pCurrentFrame->m_CodingData->m_NumPartitions)
                    {
                        // Restore saved CABAC context
                        MFX_INTERNAL_CPY(m_pBitStream->context_hevc, m_pBitStream->wpp_saved_cabac_context, sizeof(m_pBitStream->context_hevc));
                    }
                    else
                    {
                        // Reset CABAC contexts
                        m_pSlice->InitializeContexts();
                    }
                }
            }
            m_context->UpdateCurrCUContext(rsCUAddr, newRSCUAddr);
            m_context->UpdateRecCurrCTBContext(rsCUAddr, newRSCUAddr);
        }

        if (newCUAddr >= nBorder)
        {
            break;
        }

        curCUAddr = newCUAddr;
        rsCUAddr = newRSCUAddr;
    }

    return umcRes;
}

// Reconstructor template
template<bool bitDepth, typename H265PlaneType>
class ReconstructorT : public ReconstructorBase
{
public:

    ReconstructorT(int32_t chromaFormat)
        : m_chromaFormat(chromaFormat)
    {
    }

    virtual bool Is8BitsReconstructor() const
    {
        return !bitDepth;
    }

    virtual int32_t GetChromaFormat() const
    {
        return m_chromaFormat;
    }

    // Do luma intra prediction
    virtual void PredictIntra(int32_t predMode, PlaneY* PredPel, PlaneY* pels, int32_t pitch, int32_t width, uint32_t bit_depth);

    // Create a buffer of neighbour luma samples for intra prediction
    virtual void GetPredPelsLuma(PlaneY* pSrc, PlaneY* PredPel, int32_t blkSize, int32_t srcPitch, uint32_t tpIf, uint32_t lfIf, uint32_t tlIf, uint32_t bit_depth);

    // Do chroma intra prediction
    virtual void PredictIntraChroma(int32_t predMode, PlaneY* PredPel, PlaneY* pels, int32_t pitch, int32_t width);

    // Create a buffer of neighbour NV12 chroma samples for intra prediction
    virtual void GetPredPelsChromaNV12(PlaneY* pSrc, PlaneY* PredPel, int32_t blkSize, int32_t srcPitch, uint32_t tpIf, uint32_t lfIf, uint32_t tlIf, uint32_t bit_depth);

    // Strong intra smoothing luma filter
    virtual void FilterPredictPels(DecodingContext* sd, H265CodingUnit* pCU, PlaneY* PredPel, int32_t width, uint32_t TrDepth, uint32_t AbsPartIdx);

    // Luma deblocking edge filter
    virtual void FilterEdgeLuma(H265EdgeData *edge, PlaneY *srcDst, size_t srcDstStride, int32_t x, int32_t y, int32_t dir, uint32_t bit_depth);
    // Chroma deblocking edge filter
    virtual void FilterEdgeChroma(H265EdgeData *edge, PlaneY *srcDst, size_t srcDstStride, int32_t x, int32_t y, int32_t dir, int32_t chromaCbQpOffset, int32_t chromaCrQpOffset, uint32_t bit_depth);

    virtual void CopyPartOfFrameFromRef(PlanePtrY pRefPlane, PlanePtrY pCurrentPlane, int32_t pitch,
        int32_t offsetX, int32_t offsetY, int32_t offsetXL, int32_t offsetYL,
        mfxSize cuSize, mfxSize frameSize);

    virtual void ReconstructPCMBlock(PlanePtrY luma, int32_t pitchLuma, uint32_t PcmLeftShiftBitLuma, PlanePtrY chroma, int32_t pitchChroma, uint32_t PcmLeftShiftBitChroma, CoeffsPtr *pcm, uint32_t size);

    virtual void DecodePCMBlock(H265Bitstream *bitStream, CoeffsPtr *pcm, uint32_t size, uint32_t sampleBits);

private:
    int32_t m_chromaFormat;
};

// turn off the "conditional expression is constant" warning
#ifdef _MSVC_LANG
#pragma warning(disable: 4127)
#endif

template<bool bitDepth, typename H265PlaneType>
void ReconstructorT<bitDepth, H265PlaneType>::ReconstructPCMBlock(PlanePtrY luma, int32_t pitchLuma, uint32_t PcmLeftShiftBitLuma,
                                                                  PlanePtrY chroma, int32_t pitchChroma, uint32_t PcmLeftShiftBitChroma, CoeffsPtr *pcm, uint32_t size)
{
    H265PlaneType * plane = (H265PlaneType *)luma;
    H265PlaneType * coeffs = (H265PlaneType *)*pcm;
    size_t const offset_luma =
        size * size * sizeof(H265PlaneType) / sizeof(Coeffs);
    *pcm += offset_luma;

    for (uint32_t Y = 0; Y < size; Y++)
    {
        for (uint32_t X = 0; X < size; X++)
        {
            plane[X] = coeffs[X] << PcmLeftShiftBitLuma;
        }

        coeffs += size;
        plane += pitchLuma;
    }

    uint32_t isChroma422 = GetChromaFormat() == CHROMA_FORMAT_422 ? 1 : 0;

    size >>= 1;
    size_t const offset_chroma =
        size * size * sizeof(H265PlaneType) / sizeof(Coeffs) << isChroma422;

    H265PlaneType * coeffsU = (H265PlaneType *)*pcm;
    *pcm += offset_chroma;
    H265PlaneType * coeffsV = (H265PlaneType *)*pcm;
    *pcm += offset_chroma;

    plane = (H265PlaneType *)chroma;

    for (uint32_t Y = 0; Y < (size << isChroma422); Y++)
    {
        for (uint32_t X = 0; X < size; X++)
        {
            plane[X * 2] = coeffsU[X] << PcmLeftShiftBitChroma;
            plane[X * 2 + 1] = coeffsV[X] << PcmLeftShiftBitChroma;
        }

        coeffsU += size;
        coeffsV += size;
        plane += pitchChroma;
    }
}

template<bool bitDepth, typename H265PlaneType>
void ReconstructorT<bitDepth, H265PlaneType>::DecodePCMBlock(H265Bitstream *bitStream, CoeffsPtr *pcm, uint32_t size, uint32_t sampleBits)
{
    H265PlaneType *pcmSamples = (H265PlaneType*)*pcm;

    size_t const offset =
        size * size * sizeof(H265PlaneType) / sizeof(Coeffs);
    *pcm += offset;

    for (uint32_t Y = 0; Y < size; Y++)
    {
        for (uint32_t X = 0; X < size; X++)
        {
            H265PlaneType sample = (H265PlaneType)bitStream->GetBits(sampleBits);
            pcmSamples[X] = sample;
        }
        pcmSamples += size;
    }
}

// Strong intra smoothing luma filter
template<bool bitDepth, typename H265PlaneType>
void ReconstructorT<bitDepth, H265PlaneType>::FilterPredictPels(DecodingContext* sd, H265CodingUnit* pCU, PlaneY* pels, int32_t width, uint32_t TrDepth, uint32_t AbsPartIdx)
{
    H265PlaneType* PredPel = (H265PlaneType*)pels;

    if (sd->m_sps->sps_strong_intra_smoothing_enabled_flag)
    {
        int32_t CUSize = pCU->GetWidth(AbsPartIdx) >> TrDepth;

        int32_t blkSize = 32;
        int32_t threshold = 1 << (sd->m_sps->bit_depth_luma - 5);

        int32_t topLeft = PredPel[0];
        int32_t topRight = PredPel[2*width];
        int32_t midHor = PredPel[width];

        int32_t bottomLeft = PredPel[4*width];
        int32_t midVer = PredPel[3*width];

        bool bilinearLeft = IPP_ABS(topLeft + topRight - 2*midHor) < threshold;
        bool bilinearAbove = IPP_ABS(topLeft + bottomLeft - 2*midVer) < threshold;

        if (CUSize >= blkSize && (bilinearLeft && bilinearAbove))
        {
#if !defined(ANDROID)
            if (sd->m_sps->bit_depth_luma > 10)
            {
                MFX_HEVC_PP::h265_FilterPredictPels_Bilinear_16s_px((int16_t *)PredPel, width, topLeft, bottomLeft, topRight);
            }
            else
#endif
            {
                h265_FilterPredictPels_Bilinear(PredPel, width, topLeft, bottomLeft, topRight);
            }
            return;
        }
    }

    h265_FilterPredictPels(PredPel, width);
}

// Luma deblocking edge filter
template<bool bitDepth, typename H265PlaneType>
void ReconstructorT<bitDepth, H265PlaneType>::FilterEdgeLuma(H265EdgeData *edge, PlaneY *srcDst, size_t srcDstStride, int32_t x, int32_t y, int32_t dir, uint32_t bit_depth)
{
    if (bitDepth)
    {
        uint16_t* srcDst_ = (uint16_t*)srcDst;
        MFX_HEVC_PP::NAME(h265_FilterEdgeLuma_16u_I)(edge, srcDst_ + x + y*srcDstStride, (int32_t)srcDstStride, dir, bit_depth);
    }
    else
    {
        MFX_HEVC_PP::NAME(h265_FilterEdgeLuma_8u_I)(edge, srcDst + x + y*srcDstStride, (int32_t)srcDstStride, dir);
    }
}

inline
int32_t CalcQpUV(int32_t iQpY, int32_t chroma_fmt)
{
    if (iQpY < 0)
        return iQpY;
    else if (iQpY > 57)
        //if ChromaArrayType is greater than 1, the variable QpC is set equal to Min( qPi, 51 )
        return chroma_fmt != CHROMA_FORMAT_422 ? iQpY - 6 : 51;
    else
    {
        int32_t const chromaScaleIndex =
            chroma_fmt != CHROMA_FORMAT_420 ? 1 : 0;
        return g_ChromaScale[chromaScaleIndex][(iQpY)];
    }
}

// Chroma deblocking edge filter
template<bool bitDepth, typename H265PlaneType>
void ReconstructorT<bitDepth, H265PlaneType>::FilterEdgeChroma(H265EdgeData *edge, PlaneY *srcDst, size_t srcDstStride, int32_t x, int32_t y, int32_t dir,
                                                               int32_t chromaCbQpOffset, int32_t chromaCrQpOffset, uint32_t bit_depth)
{
    int32_t const chroma_fmt = GetChromaFormat();

    if (bitDepth)
    {
        uint16_t* srcDst_ = (uint16_t*)srcDst;
        MFX_HEVC_PP::NAME(h265_FilterEdgeChroma_Interleaved_16u_I)(
            edge,
            srcDst_ + 2*x + y*srcDstStride,
            (int32_t)srcDstStride,
            dir,
            CalcQpUV(edge->qp + chromaCbQpOffset, chroma_fmt),
            CalcQpUV(edge->qp + chromaCrQpOffset, chroma_fmt), bit_depth);
    }
    else
    {
        MFX_HEVC_PP::NAME(h265_FilterEdgeChroma_Interleaved_8u_I)(
            edge,
            srcDst + 2*x + y*srcDstStride,
            (int32_t)srcDstStride,
            dir,
            CalcQpUV(edge->qp + chromaCbQpOffset, chroma_fmt),
            CalcQpUV(edge->qp + chromaCrQpOffset, chroma_fmt));
    }
}

// Do luma intra prediction
template<bool bitDepth, typename H265PlaneType>
void ReconstructorT<bitDepth, H265PlaneType>::PredictIntra(int32_t predMode, PlaneY* PredPel, PlaneY* pRec, int32_t pitch, int32_t width, uint32_t bit_depth)
{
    if (bitDepth)
    {
        uint16_t* pels = (uint16_t*)PredPel;
        uint16_t* rec = (uint16_t*)pRec;

#if !defined(ANDROID)
        if (bit_depth > 10)
        {
            switch(predMode)
            {
            case INTRA_LUMA_PLANAR_IDX:
                MFX_HEVC_PP::h265_PredictIntra_Planar_16s_px((int16_t *)pels, (int16_t *)rec, pitch, width);
                break;
            case INTRA_LUMA_DC_IDX:
                MFX_HEVC_PP::h265_PredictIntra_DC_16u(pels, rec, pitch, width, 1);
                break;
            case INTRA_LUMA_VER_IDX:
                MFX_HEVC_PP::h265_PredictIntra_Ver_16u(pels, rec, pitch, width, bit_depth, 1);
                break;
            case INTRA_LUMA_HOR_IDX:
                MFX_HEVC_PP::h265_PredictIntra_Hor_16u(pels, rec, pitch, width, bit_depth, 1);
                break;
            default:
                MFX_HEVC_PP::h265_PredictIntra_Ang_16u_px(predMode, pels, rec, pitch, width);
            }
        }
        else
#endif
        {
            switch(predMode)
            {
            case INTRA_LUMA_PLANAR_IDX:
                MFX_HEVC_PP::h265_PredictIntra_Planar(pels, rec, pitch, width);
                break;
            case INTRA_LUMA_DC_IDX:
                MFX_HEVC_PP::h265_PredictIntra_DC_16u(pels, rec, pitch, width, 1);
                break;
            case INTRA_LUMA_VER_IDX:
                MFX_HEVC_PP::h265_PredictIntra_Ver_16u(pels, rec, pitch, width, bit_depth, 1);
                break;
            case INTRA_LUMA_HOR_IDX:
                MFX_HEVC_PP::h265_PredictIntra_Hor_16u(pels, rec, pitch, width, bit_depth, 1);
                break;
            default:
                MFX_HEVC_PP::NAME(h265_PredictIntra_Ang_16u)(predMode, pels, rec, pitch, width);
            }
        }
    }
    else
    {
        switch(predMode)
        {
        case INTRA_LUMA_PLANAR_IDX:
            MFX_HEVC_PP::NAME(h265_PredictIntra_Planar_8u)(PredPel, pRec, pitch, width);
            break;
        case INTRA_LUMA_DC_IDX:
            MFX_HEVC_PP::h265_PredictIntra_DC_8u(PredPel, pRec, pitch, width, 1);
            break;
        case INTRA_LUMA_VER_IDX:
            MFX_HEVC_PP::h265_PredictIntra_Ver_8u(PredPel, pRec, pitch, width, 8, 1);
            break;
        case INTRA_LUMA_HOR_IDX:
            MFX_HEVC_PP::h265_PredictIntra_Hor_8u(PredPel, pRec, pitch, width, 8, 1);
            break;
        default:
            MFX_HEVC_PP::NAME(h265_PredictIntra_Ang_8u)(predMode, PredPel, pRec, pitch, width);
        }
    }
}

// Do chroma intra prediction
template<bool bitDepth, typename H265PlaneType>
void ReconstructorT<bitDepth, H265PlaneType>::PredictIntraChroma(int32_t predMode, PlaneY* PredPel, PlaneY* pRec, int32_t pitch, int32_t Size)
{
    if (bitDepth)
    {
        uint16_t* pels = (uint16_t*)PredPel;
        uint16_t* rec = (uint16_t*)pRec;

        switch(predMode)
        {
        case INTRA_LUMA_PLANAR_IDX:
            h265_PredictIntra_Planar_ChromaNV12_16u(pels, rec, pitch, Size);
            break;
        case INTRA_LUMA_DC_IDX:
            h265_PredictIntra_DC_ChromaNV12_16u(pels, rec, pitch, Size);
            break;
        case INTRA_LUMA_VER_IDX:
            h265_PredictIntra_Ver_ChromaNV12_16u(pels, rec, pitch, Size);
            break;
        case INTRA_LUMA_HOR_IDX:
            h265_PredictIntra_Hor_ChromaNV12_16u(pels, rec, pitch, Size);
            break;
        default:
            h265_PredictIntra_Ang_ChromaNV12_16u(pels, rec, pitch, Size, predMode);
            break;
        }
    }
    else
    {
        switch(predMode)
        {
        case INTRA_LUMA_PLANAR_IDX:
            h265_PredictIntra_Planar_ChromaNV12(PredPel, pRec, pitch, Size);
            break;
        case INTRA_LUMA_DC_IDX:
            h265_PredictIntra_DC_ChromaNV12_8u(PredPel, pRec, pitch, Size);
            break;
        case INTRA_LUMA_VER_IDX:
            h265_PredictIntra_Ver_ChromaNV12_8u(PredPel, pRec, pitch, Size);
            break;
        case INTRA_LUMA_HOR_IDX:
            h265_PredictIntra_Hor_ChromaNV12_8u(PredPel, pRec, pitch, Size);
            break;
        default:
            h265_PredictIntra_Ang_ChromaNV12_8u(PredPel, pRec, pitch, Size, predMode);
            break;
        }
    }
}

// restore the "conditional expression is constant" warning
#ifdef _MSVC_LANG
#pragma warning(default: 4127)
#endif

// Create a buffer of neighbour luma samples for intra prediction
template<bool bitDepth, typename H265PlaneType>
void ReconstructorT<bitDepth, H265PlaneType>::GetPredPelsLuma(PlaneY* pSrc, PlaneY* PredPel, int32_t blkSize, int32_t srcPitch, uint32_t tpIf, uint32_t lfIf, uint32_t tlIf, uint32_t bit_depth)
{
    H265PlaneType * src = (H265PlaneType *)pSrc;
    H265PlaneType * pels = (H265PlaneType *)PredPel;

    h265_GetPredPelsLuma(src, pels, blkSize, srcPitch, tpIf, lfIf, tlIf, bit_depth);
}

// Create a buffer of neighbour NV12 chroma samples for intra prediction
template<bool bitDepth, typename H265PlaneType>
void ReconstructorT<bitDepth, H265PlaneType>::GetPredPelsChromaNV12(PlaneY* pSrc, PlaneY* PredPel, int32_t blkSize, int32_t srcPitch, uint32_t tpIf, uint32_t lfIf, uint32_t tlIf, uint32_t bit_depth)
{
    H265PlaneType * src = (H265PlaneType *)pSrc;
    H265PlaneType * pels = (H265PlaneType *)PredPel;

    h265_GetPredPelsChromaNV12(src, pels, this->m_chromaFormat == CHROMA_FORMAT_422 ? 1 : 0, blkSize, srcPitch, tpIf, lfIf, tlIf, bit_depth);
}

template<bool bitDepth, typename H265PlaneType>
void ReconstructorT<bitDepth, H265PlaneType>::CopyPartOfFrameFromRef(PlanePtrY refPlane, PlanePtrY currentPlane, int32_t pitch,
    int32_t offsetX, int32_t offsetY, int32_t offsetXL, int32_t offsetYL,
    mfxSize cuSize, mfxSize picSize)
{
    H265PlaneType *pRefPlane = (H265PlaneType*)refPlane;
    H265PlaneType *pCurrentPlane = (H265PlaneType*)currentPlane;
    mfxSize roiSize;

    roiSize.height = cuSize.height;
    roiSize.width = picSize.width - offsetX;
    int32_t offset = offsetX + offsetY*pitch;

    offsetXL += cuSize.width;
    if (offsetXL > picSize.width)
        offsetXL = picSize.width;

    if (offsetY + cuSize.height > picSize.height)
        roiSize.height = picSize.height - offsetY;

    if (offsetYL == offsetY)
    {
        roiSize.width = offsetXL - offsetX;
    }

    if (pRefPlane)
    {
        CopyPlane(pRefPlane + offset,
                    pitch,
                    pCurrentPlane + offset,
                    pitch,
                    roiSize);
    }
    else
    {
        SetPlane(128, pCurrentPlane + offset, pitch, roiSize);
    }

    if (offsetYL > offsetY)
    {
        if (offsetYL + cuSize.height > picSize.height)
            roiSize.height = picSize.height - offsetYL;

        roiSize.width = offsetXL;
        offset = offsetYL*pitch;

        if (pRefPlane)
        {
            CopyPlane(pRefPlane + offset,
                        pitch,
                        pCurrentPlane + offset,
                        pitch,
                        roiSize);
        }
        else
        {
            SetPlane(128, pCurrentPlane + offset, pitch, roiSize);
        }
    }

    if (offsetYL - offsetY > cuSize.height)
    {
        roiSize.height = offsetYL - offsetY - cuSize.height;
        roiSize.width = picSize.width;
        offset = (offsetY + cuSize.height)*pitch;

        if (pRefPlane)
        {
            CopyPlane(pRefPlane + offset,
                        pitch,
                        pCurrentPlane + offset,
                        pitch,
                        roiSize);
        }
        else
        {
            SetPlane(128, pCurrentPlane + offset, pitch, roiSize);
        }
    }
}


void H265SegmentDecoderMultiThreaded::CreateReconstructor()
{
    if (m_reconstructor.get() && m_reconstructor->Is8BitsReconstructor() == !m_pSeqParamSet->need16bitOutput)
        return;

    if (m_pSeqParamSet->need16bitOutput)
    {
        m_reconstructor.reset(new ReconstructorT<true, uint16_t>(m_pSeqParamSet->ChromaArrayType));
    }
    else
    {
        m_reconstructor.reset(new ReconstructorT<false, uint8_t>(m_pSeqParamSet->ChromaArrayType));
    }
}

} // namespace UMC_HEVC_DECODER
#endif // #ifndef MFX_VA
#endif // MFX_ENABLE_H265_VIDEO_DECODE
