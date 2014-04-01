/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
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
    m_bIsNeedWADeblocking = m_pCurrentFrame->GetAU()->IsNeedWorkAroundForDeblocking();
    m_hasTiles = Task.m_pSlicesInfo->m_hasTiles;

    m_SD = CreateSegmentDecoder();

    m_DecodeDQPFlag = false;
    m_minCUDQPSize = m_pSeqParamSet->MaxCUSize >> m_pPicParamSet->diff_cu_qp_delta_depth;

    m_context = Task.m_context;
    if (!m_context && (Task.m_iTaskID == TASK_DEC_REC_H265 || Task.m_iTaskID == TASK_PROCESS_H265))
    {
        m_context = m_context_single_thread.get();
        m_context->Init(Task.m_pSlice);
        Task.m_pBuffer = (H265CoeffsPtrCommon)m_context->m_coeffBuffer.LockInputBuffer();
    }

    this->create((H265SeqParamSet*)m_pSeqParamSet);

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

    if (m_context)
    {
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

    Ipp32s sliceNum = m_pSlice->GetSliceNum();

    m_pRefPicList[0] = m_pCurrentFrame->GetRefPicList(sliceNum, REF_PIC_LIST_0)->m_refPicList;
    m_pRefPicList[1] = m_pCurrentFrame->GetRefPicList(sliceNum, REF_PIC_LIST_1)->m_refPicList;
}

void H265SegmentDecoderMultiThreaded::EndProcessingSegment(H265Task &Task)
{
    this->destroy();

    if (TASK_DEC_H265 == Task.m_iTaskID)
    {
        Ipp32s mvsDistortion = m_context->m_mvsDistortion;
        mvsDistortion = (mvsDistortion + 6) >> 2; // remove 1/2 pel
        Task.m_mvsDistortion = IPP_MAX(mvsDistortion, m_pSlice->m_mvsDistortion);

        Task.m_WrittenSize = (Ipp8u*)m_context->m_coeffsWrite - (Ipp8u*)Task.m_pBuffer;
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

UMC::Status H265SegmentDecoderMultiThreaded::ProcessSegment(void)
{
#ifdef __EXCEPTION_HANDLER_
    exceptionHandler.setTranslator();
#endif

    H265Task Task(m_iNumber);
    try
    {
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
                    umcRes = (this->*(Task.pFunction))(Task); //ProcessSlice function

                    if (UMC::UMC_ERR_END_OF_STREAM == umcRes)
                    {
                        Task.m_iMaxMB = Task.m_iFirstMB + Task.m_iMBToProcess;
                        //// if we decode less macroblocks if we need try to recovery:
                        RestoreErrorRect(Task.m_iFirstMB + Task.m_iMBToProcess, m_pSlice->GetMaxMB(), m_pSlice);
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
                if (m_pSlice) //restoreerror
                    RestoreErrorRect(Task.m_iFirstMB + Task.m_iMBToProcess, m_pSlice->GetMaxMB(), m_pSlice);
            }

            EndProcessingSegment(Task);
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

void H265SegmentDecoderMultiThreaded::RestoreErrorRect(Ipp32s startMb , Ipp32s endMb, H265Slice * pSlice)
{
    if (startMb >= endMb || !pSlice)
        return;

    m_pSlice = pSlice;
    m_pSlice->m_bError = true;

    try
    {
        H265DecoderFrame * pCurrentFrame = m_pSlice->GetCurrentFrame();

        if (!pCurrentFrame || !pCurrentFrame->m_pYPlane)
        {
            return;
        }

        //H264DecoderFrame * pRefFrame = pCurrentFrame->GetRefPicList(m_pSlice->GetSliceNum(), 0)->m_RefPicList[0];

        pCurrentFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);

        /*if (!pRefFrame || pRefFrame->IsSkipped())
        {
            pRefFrame = m_pTaskBroker->m_pTaskSupplier->GetDPBList(BASE_VIEW, 0)->FindClosest(pCurrentFrame);
        }

        if (!m_SD)
        {
            m_pCurrentFrame = pCurrentFrame;
            bit_depth_luma = m_pCurrentFrame->IsAuxiliaryFrame() ? m_pSlice->GetSeqParamEx()->bit_depth_aux :
                                                    m_pSeqParamSet->bit_depth_luma;
            bit_depth_chroma = m_pCurrentFrame->IsAuxiliaryFrame() ? 8 : m_pSeqParamSet->bit_depth_chroma;
            m_SD = CreateSegmentDecoder();
        }

        m_SD->RestoreErrorRect(startMb, endMb, pRefFrame, this);*/
    } catch (...)
    {
        // nothing to do
    }

    return;
}

UMC::Status H265SegmentDecoderMultiThreaded::DecodeSegment(H265Task & task)
{
    UMC::Status umcRes = UMC::UMC_OK;
    Ipp32s iMaxCUNumber = task.m_iFirstMB + task.m_iMBToProcess;

    if (m_pSlice->GetFirstMB() == task.m_iFirstMB)
    {
        m_pSlice->GetBitStream()->InitializeDecodingEngine_CABAC();
        m_pSlice->InitializeContexts();
    }

    try
    {
        umcRes = m_SD->DecodeSegment(task.m_iFirstMB, iMaxCUNumber, this);
        task.m_iMBToProcess = iMaxCUNumber - task.m_iFirstMB;
        
    } catch(...)
    {
        task.m_iMBToProcess = iMaxCUNumber - task.m_iFirstMB;
        throw;
    }
    
    return umcRes;
}

UMC::Status H265SegmentDecoderMultiThreaded::ReconstructSegment(H265Task & task)
{
    UMC::Status umcRes = UMC::UMC_OK;
    {
        Ipp32s iMaxCUNumber = task.m_iFirstMB + task.m_iMBToProcess;
        umcRes = m_SD->ReconstructSegment(task.m_iFirstMB, iMaxCUNumber, this);
        task.m_iMBToProcess = iMaxCUNumber - task.m_iFirstMB;
        return umcRes;
    }
}

UMC::Status H265SegmentDecoderMultiThreaded::DecRecSegment(H265Task & task)
{
    H265Bitstream  bitstream;
    UMC::Status umcRes = UMC::UMC_OK;

    Ipp32s iFirstPartition = IPP_MAX(m_pSliceHeader->SliceCurStartCUAddr, (Ipp32s)m_pSliceHeader->m_sliceSegmentCurStartCUAddr);
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

            if (usenessTiles != task.m_iFirstMB)
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
    size_t bsOffset = m_pSliceHeader->m_TileByteLocation[task.m_iFirstMB - 1];
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

    task.m_iMBToProcess = maxCUNumber - task.m_iFirstMB;

    return umcRes;

} // Status H265SegmentDecoderMultiThreaded::DecRecSegment(Ipp32s iCurMBNumber, Ipp32s &iMBToReconstruct)

UMC::Status H265SegmentDecoderMultiThreaded::SAOFrameTask(H265Task & task)
{
    START_TICK
    m_pCurrentFrame->getCD()->m_SAO.SAOProcess(m_pCurrentFrame, task.m_iFirstMB, task.m_iMBToProcess);
    END_TICK(sao_time)
    return UMC::UMC_OK;
}

UMC::Status H265SegmentDecoderMultiThreaded::DeblockSegmentTask(H265Task & task)
{
    START_TICK
    // when there is slice groups or threaded deblocking
    DeblockSegment(task);
    END_TICK(deblocking_time)
    return UMC::UMC_OK;

} // Status H265SegmentDecoderMultiThreaded::DeblockSegmentTask(Ipp32s iCurMBNumber, Ipp32s &iMBToDeblock)

UMC::Status H265SegmentDecoderMultiThreaded::ProcessSlice(H265Task & task)
{
    UMC::Status umcRes = UMC::UMC_OK;

    // Convert slice beginning and end to encode order
    Ipp32s iFirstPartition = IPP_MAX(m_pSliceHeader->SliceCurStartCUAddr, (Ipp32s)m_pSliceHeader->m_sliceSegmentCurStartCUAddr);
    Ipp32s iFirstCU = iFirstPartition / m_pCurrentFrame->m_CodingData->m_NumPartitions;
    Ipp32s iMaxCUNumber = m_pCurrentFrame->getNumCUsInFrame();

    m_pSlice->GetBitStream()->InitializeDecodingEngine_CABAC();
    m_pSlice->InitializeContexts();
    
    if (m_pSliceHeader->dependent_slice_segment_flag)
    {
        Ipp32s CUAddr = m_pCurrentFrame->m_CodingData->getCUOrderMap(iFirstCU);

        size_t tile = m_pCurrentFrame->m_CodingData->getTileIdxMap(CUAddr);
        if (m_pPicParamSet->tilesInfo[tile].firstCUAddr != CUAddr)
        {
            H265Slice * slice = m_pCurrentFrame->GetAU()->GetSliceByNumber(m_pSlice->m_iNumber - 1);
            VM_ASSERT(slice);
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

    try
    {
        umcRes = m_SD->DecodeSegmentCABAC_Single_H265(iFirstCU, iMaxCUNumber, this);

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


template<bool bitDepth, typename H265PlaneType>
class ReconstructorT : public ReconstructorBase
{
public:
    virtual void PredictIntra(Ipp32s predMode, H265PlaneYCommon* PredPel, H265PlaneYCommon* pels, Ipp32s pitch, Ipp32s width, Ipp32u bit_depth);

    virtual void GetPredPelsLuma(H265PlaneYCommon* pSrc, H265PlaneYCommon* PredPel, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf, Ipp32u bit_depth);

    virtual void PredictIntraChroma(Ipp32s predMode, H265PlaneYCommon* PredPel, H265PlaneYCommon* pels, Ipp32s pitch, Ipp32s width);

    virtual void GetPredPelsChromaNV12(H265PlaneYCommon* pSrc, H265PlaneYCommon* PredPel, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf, Ipp32u bit_depth);

    virtual void FilterPredictPels(DecodingContext* sd, H265CodingUnit* pCU, H265PlaneYCommon* PredPel, Ipp32s width, Ipp32u TrDepth, Ipp32u AbsPartIdx);

    virtual void FilterEdgeLuma(H265EdgeData *edge, H265PlaneYCommon *srcDst, size_t srcDstStride, Ipp32s x, Ipp32s y, Ipp32s dir, Ipp32u bit_depth);
    virtual void FilterEdgeChroma(H265EdgeData *edge, H265PlaneYCommon *srcDst, size_t srcDstStride, Ipp32s x, Ipp32s y, Ipp32s dir, Ipp32s chromaCbQpOffset, Ipp32s chromaCrQpOffset, Ipp32u bit_depth);
};

#pragma warning(disable: 4127)

template<bool bitDepth, typename H265PlaneType>
void ReconstructorT<bitDepth, H265PlaneType>::FilterPredictPels(DecodingContext* sd, H265CodingUnit* pCU, H265PlaneYCommon* pels, Ipp32s width, Ipp32u TrDepth, Ipp32u AbsPartIdx)
{
    H265PlaneType* PredPel = (H265PlaneType*)pels;

    if (sd->m_sps->sps_strong_intra_smoothing_enabled_flag)
    {
        Ipp32s CUSize = pCU->GetWidth(AbsPartIdx) >> TrDepth;

        Ipp32s blkSize = 32;
        Ipp32s threshold = 1 << (sd->m_sps->bit_depth_luma - 5);

        Ipp32s topLeft = PredPel[0];
        Ipp32s topRight = PredPel[2*width];
        Ipp32s midHor = PredPel[width];

        Ipp32s bottomLeft = PredPel[4*width];
        Ipp32s midVer = PredPel[3*width];

        bool bilinearLeft = IPP_ABS(topLeft + topRight - 2*midHor) < threshold; 
        bool bilinearAbove = IPP_ABS(topLeft + bottomLeft - 2*midVer) < threshold;

        if (CUSize >= blkSize && (bilinearLeft && bilinearAbove))
        {
            h265_FilterPredictPels_Bilinear(PredPel, width, topLeft, bottomLeft, topRight);
            return;
        }
    }
  
    h265_FilterPredictPels(PredPel, width);
}

template<bool bitDepth, typename H265PlaneType>
void ReconstructorT<bitDepth, H265PlaneType>::FilterEdgeLuma(H265EdgeData *edge, H265PlaneYCommon *srcDst, size_t srcDstStride, Ipp32s x, Ipp32s y, Ipp32s dir, Ipp32u bit_depth)
{
    if (bitDepth)
    {
        Ipp16u* srcDst_ = (Ipp16u*)srcDst;
        MFX_HEVC_PP::NAME(h265_FilterEdgeLuma_16u_I)(edge, srcDst_ + x + y*srcDstStride, (Ipp32s)srcDstStride, dir, bit_depth);
    }
    else
    {
        MFX_HEVC_PP::NAME(h265_FilterEdgeLuma_8u_I)(edge, srcDst + x + y*srcDstStride, (Ipp32s)srcDstStride, dir);
    }
}

#define   QpUV(iQpY)  ( ((iQpY) < 0) ? (iQpY) : (((iQpY) > 57) ? ((iQpY)-6) : g_ChromaScale[(iQpY)]) )

template<bool bitDepth, typename H265PlaneType>
void ReconstructorT<bitDepth, H265PlaneType>::FilterEdgeChroma(H265EdgeData *edge, H265PlaneYCommon *srcDst, size_t srcDstStride, Ipp32s x, Ipp32s y, Ipp32s dir,
                                                               Ipp32s chromaCbQpOffset, Ipp32s chromaCrQpOffset, Ipp32u bit_depth)
{
    if (bitDepth)
    {
        Ipp16u* srcDst_ = (Ipp16u*)srcDst;
        MFX_HEVC_PP::NAME(h265_FilterEdgeChroma_Interleaved_16u_I)(
            edge, 
            srcDst_ + 2*x + y*srcDstStride,
            (Ipp32s)srcDstStride,
            dir,
            QpUV(edge->qp + chromaCbQpOffset),
            QpUV(edge->qp + chromaCrQpOffset), bit_depth);
    }
    else
    {
        MFX_HEVC_PP::NAME(h265_FilterEdgeChroma_Interleaved_8u_I)(
            edge, 
            srcDst + 2*x + y*srcDstStride,
            (Ipp32s)srcDstStride,
            dir,
            QpUV(edge->qp + chromaCbQpOffset),
            QpUV(edge->qp + chromaCrQpOffset));
    }
}

template<bool bitDepth, typename H265PlaneType>
void ReconstructorT<bitDepth, H265PlaneType>::PredictIntra(Ipp32s predMode, H265PlaneYCommon* PredPel, H265PlaneYCommon* pRec, Ipp32s pitch, Ipp32s width, Ipp32u bit_depth)
{
    if (bitDepth)
    {
        Ipp16u* pels = (Ipp16u*)PredPel;
        Ipp16u* rec = (Ipp16u*)pRec;
        switch(predMode)
        {
        case INTRA_LUMA_PLANAR_IDX:
            MFX_HEVC_PP::h265_PredictIntra_Planar_16u(pels, rec, pitch, width);
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

template<bool bitDepth, typename H265PlaneType>
void ReconstructorT<bitDepth, H265PlaneType>::PredictIntraChroma(Ipp32s predMode, H265PlaneYCommon* PredPel, H265PlaneYCommon* pRec, Ipp32s pitch, Ipp32s Size)
{
    if (bitDepth)
    {
        Ipp16u* pels = (Ipp16u*)PredPel;
        Ipp16u* rec = (Ipp16u*)pRec;

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
            h265_PredictIntra_Planar_ChromaNV12_8u(PredPel, pRec, pitch, Size);
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

#pragma warning(default: 4127)

template<bool bitDepth, typename H265PlaneType>
void ReconstructorT<bitDepth, H265PlaneType>::GetPredPelsLuma(H265PlaneYCommon* pSrc, H265PlaneYCommon* PredPel, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf, Ipp32u bit_depth)
{
    H265PlaneType * src = (H265PlaneType *)pSrc;
    H265PlaneType * pels = (H265PlaneType *)PredPel;

    h265_GetPredPelsLuma(src, pels, blkSize, srcPitch, tpIf, lfIf, tlIf, bit_depth);
}

template<bool bitDepth, typename H265PlaneType>
void ReconstructorT<bitDepth, H265PlaneType>::GetPredPelsChromaNV12(H265PlaneYCommon* pSrc, H265PlaneYCommon* PredPel, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf, Ipp32u bit_depth)
{
    H265PlaneType * src = (H265PlaneType *)pSrc;
    H265PlaneType * pels = (H265PlaneType *)PredPel;

    h265_GetPredPelsChromaNV12(src, pels, blkSize, srcPitch, tpIf, lfIf, tlIf, bit_depth);
}

SegmentDecoderHPBase_H265* H265SegmentDecoderMultiThreaded::CreateSegmentDecoder()
{
    if (m_pSeqParamSet->bit_depth_luma > 8 || m_pSeqParamSet->bit_depth_chroma > 8)
    {
        m_reconstructor.reset(new ReconstructorT<true, Ipp16u>);
    }
    else
    {
        m_reconstructor.reset(new ReconstructorT<false, Ipp8u>);
    }

    return CreateSD_H265(m_pSeqParamSet->bit_depth_luma, m_pSeqParamSet->bit_depth_chroma);
}


} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
