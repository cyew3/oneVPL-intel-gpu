/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2003-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_segment_decoder_mt.h"
#include "umc_h264_bitstream_inlines.h"
#include "umc_h264_segment_decoder_templates.h"

#include "umc_h264_task_broker.h"
#include "umc_h264_frame_info.h"

#include "umc_h264_task_supplier.h"
#include "umc_h264_frame_list.h"

#include "umc_h264_timing.h"

#include "mfx_trace.h"

namespace UMC
{
static
#ifdef __ICL
    __declspec(align(16))
#endif
const Ipp8u BlkOrder[16] =
{
    0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15
};

H264SegmentDecoderMultiThreaded::H264SegmentDecoderMultiThreaded(TaskBroker * pTaskBroker)
    : H264SegmentDecoder(pTaskBroker)
    , m_SD(0)
{
} // H264SegmentDecoderMultiThreaded::H264SegmentDecoderMultiThreaded(H264SliceStore *Store)

H264SegmentDecoderMultiThreaded::~H264SegmentDecoderMultiThreaded(void)
{
    Release();

} // H264SegmentDecoderMultiThreaded::~H264SegmentDecoderMultiThreaded(void)

Status H264SegmentDecoderMultiThreaded::Init(Ipp32s iNumber)
{
    // release object before initialization
    Release();

    // call parent's Init
    return H264SegmentDecoder::Init(iNumber);
} // Status H264SegmentDecoderMultiThreaded::Init(Ipp32u nNumber)

void H264SegmentDecoderMultiThreaded::StartProcessingSegment(H264Task &Task)
{
    m_pSlice = Task.m_pSlice;
    m_pCurrentFrame = m_pSlice->GetCurrentFrame();
    H264DecoderFrame *pCurrentFrame = m_pCurrentFrame;

    m_pSliceHeader = m_pSlice->GetSliceHeader();

    Ipp32s dId = m_pSliceHeader->nal_ext.svc.dependency_id;
    m_pCurrentFrame->m_pLayerFrames[dId]->m_iResourceNumber = m_pCurrentFrame->m_iResourceNumber;
    m_pCurrentFrame = m_pCurrentFrame->m_pLayerFrames[dId];

    m_field_index = m_pSliceHeader->bottom_field_flag;

    //if (m_pSlice->m_bIsFirstSliceInDepLayer)
    {
        m_currentLayer = m_pSliceHeader->nal_ext.svc.dependency_id;
        m_is_ref_base_pic = false;
    }

    // reset decoding variables
    m_pBitStream = m_pSlice->GetBitStream();
    mb_height = m_pSlice->GetMBHeight();
    mb_width = m_pSlice->GetMBWidth();
    m_pPicParamSet = m_pSlice->GetPicParam();
    m_pScalingPicParams = &m_pPicParamSet->scaling[NAL_UT_CODED_SLICE_EXTENSION == m_pSliceHeader->nal_unit_type ? 1 : 0];
    m_pPredWeight[0] = m_pSlice->GetPredWeigthTable(0);
    m_pPredWeight[1] = m_pSlice->GetPredWeigthTable(1);
    m_pSeqParamSet = m_pSlice->GetSeqParam();
    m_pCoeffBlocksRead = m_pCoeffBlocksWrite = m_psBuffer = Task.m_pBuffer;
    m_pMBIntraTypes = m_pSlice->GetMBIntraTypes();

    m_bNeedToCheckMBSliceEdges = m_pSlice->NeedToCheckSliceEdges();
    m_mbinfo = m_pSlice->GetMBInfo();

    bit_depth_luma = m_pCurrentFrame->IsAuxiliaryFrame() ? m_pSlice->GetSeqParamEx()->bit_depth_aux :
                                            m_pSeqParamSet->bit_depth_luma;
    bit_depth_chroma = m_pCurrentFrame->IsAuxiliaryFrame() ? 8 : m_pSeqParamSet->bit_depth_chroma;

    m_MVDistortion[0] = 0;
    m_MVDistortion[1] = 0;

    m_PairMBAddr = 0;
    m_CurMBAddr = 0;
    m_iSkipNextMacroblock = 0;

    m_gmbinfo = &(m_pCurrentFrame->m_mbinfo);
    if (m_pSlice->GetSliceHeader()->nal_ext.svc.quality_id)
    {
        m_gmbinfo = &(m_pSlice->GetCurrentFrame()->m_mbinfo);
    }

    m_isSliceGroups = m_pSlice->IsSliceGroups();
    is_profile_baseline_scalable = (m_pSlice->m_pSeqParamSet->profile_idc == H264VideoDecoderParams::H264_PROFILE_SCALABLE_BASELINE);

    if (Task.m_iTaskID == TASK_DEB_FRAME)
    {
        m_pSlice = 0;
    }

    m_SD = CreateSegmentDecoder();

    bool is_field = m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE;

    m_uPitchLuma = pCurrentFrame->pitch_luma();
    m_uPitchChroma = pCurrentFrame->pitch_chroma();

    m_uPitchLumaCur = m_pCurrentFrame->lumaSize().width;
    m_uPitchChromaCur = m_pCurrentFrame->chromaSize().width;

    m_pYPlane = pCurrentFrame->m_pYPlane;
    m_pUPlane = pCurrentFrame->m_pUPlane;
    m_pVPlane = pCurrentFrame->m_pVPlane;
    m_pUVPlane = pCurrentFrame->m_pUVPlane;

    m_cur_mb.isInited = false;

    if (m_pCurrentFrame->GetColorFormat() == NV12)
    {
        m_pUPlane = pCurrentFrame->m_pUVPlane;
        m_pVPlane = pCurrentFrame->m_pUVPlane;
    }

    if (is_field)
    {
        if(m_field_index)
        {
            Ipp32s pixel_luma_sz    = bit_depth_luma > 8 ? 2 : 1;
            Ipp32s pixel_chroma_sz  = bit_depth_chroma > 8 ? 2 : 1;

            m_pYPlane += m_uPitchLuma*pixel_luma_sz;
            m_pUPlane += m_uPitchChroma*pixel_chroma_sz;
            m_pVPlane += m_uPitchChroma*pixel_chroma_sz;
            m_pUVPlane += m_uPitchChroma*pixel_chroma_sz;
        }

        m_uPitchLuma *= 2;
        m_uPitchChroma *= 2;
        m_uPitchLumaCur *= 2;
        m_uPitchChromaCur *= 2;
    }

    deblocking_IL = 0;
    deblocking_stage2 = 0;
    m_spatial_resolution_change = 0;
    m_use_coeff_prediction = 0;

    if (!m_pSlice) // TASK_DEB_FRAME
        return;

    // InitContext
    m_iFirstSliceMb = m_pSlice->GetFirstMBNumber();
    m_iSliceNumber = m_pSlice->GetSliceNum();

    m_isMBAFF = 0 != m_pSliceHeader->MbaffFrameFlag;

    m_IsUseSpatialDirectMode = (m_pSliceHeader->direct_spatial_mv_pred_flag != 0);

    Ipp32s sliceNum = m_pSlice->GetSliceNum();
    m_pFields[0] = m_pCurrentFrame->GetRefPicList(sliceNum, 0)->m_Flags;
    m_pFields[1] = m_pCurrentFrame->GetRefPicList(sliceNum, 1)->m_Flags;
    m_pRefPicList[0] = m_pCurrentFrame->GetRefPicList(sliceNum, 0)->m_RefPicList;
    m_pRefPicList[1] = m_pCurrentFrame->GetRefPicList(sliceNum, 1)->m_RefPicList;

    if (TASK_PROCESS == Task.m_iTaskID || TASK_DEC == Task.m_iTaskID || TASK_DEC_REC == Task.m_iTaskID)
        m_pSlice->GetStateVariables(m_MBSkipCount, m_QuantPrev, m_prev_dquant);

    m_IsUseConstrainedIntra = (m_pPicParamSet->constrained_intra_pred_flag != 0);
    m_IsUseDirect8x8Inference = (m_pSeqParamSet->direct_8x8_inference_flag != 0);
    m_IsBSlice = (m_pSliceHeader->slice_type == BPREDSLICE);

    if (m_mbinfo.layerMbs) {
        m_pYResidual = m_mbinfo.layerMbs[m_currentLayer].m_pYResidual;
        m_pUResidual = m_mbinfo.layerMbs[m_currentLayer].m_pUResidual;
        m_pVResidual = m_mbinfo.layerMbs[m_currentLayer].m_pVResidual;
    } else {
        m_pYResidual = m_pUResidual = m_pVResidual = NULL;
    }

    // reset neighbouringing blocks numbers
    m_cur_mb.CurrentBlockNeighbours.m_bInited = 0;

    if (TASK_DEB_SLICE == Task.m_iTaskID && m_pSlice)
        CalculateResizeParametersSlice();
}

void H264SegmentDecoderMultiThreaded::EndProcessingSegment(H264Task &Task)
{
    // return performed task
    Task.m_WrittenSize = (Ipp8u*)m_pCoeffBlocksWrite - (Ipp8u*)Task.m_pBuffer;
    if (Task.m_bError)
    {
        Task.m_WrittenSize += COEFFICIENTS_BUFFER_SIZE;
    }

    if (TASK_DEC == Task.m_iTaskID)
    {
        m_MVDistortion[0] = (m_MVDistortion[0] + 6) >> INTERP_SHIFT;
        m_MVDistortion[1] = (m_MVDistortion[1] + 6) >> INTERP_SHIFT;

        if (m_pSlice->IsField() || m_pSlice->GetSliceHeader()->MbaffFrameFlag)
        {
            m_MVDistortion[0] <<= 1;
            m_MVDistortion[1] <<= 1;
        }

        m_MVDistortion[0] = IPP_MAX(m_MVDistortion[0], m_MVDistortion[1]);
        Task.m_mvsDistortion = IPP_MAX(m_MVDistortion[0], m_pSlice->m_MVsDistortion);
    }

    m_pTaskBroker->AddPerformedTask(&Task);
}

Status H264SegmentDecoderMultiThreaded::ProcessSegment(void)
{
    H264Task Task(m_iNumber);
    try
    {
        if (m_pTaskBroker->GetNextTask(&Task))
        {
            Status umcRes = UMC_OK;

            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "AVCDec_work");
            VM_ASSERT(Task.pFunction);

            StartProcessingSegment(Task);

            if (!m_pCurrentFrame->IsSkipped() && (!m_pSlice || !m_pSlice->IsError()))
            {
                try // do decoding
                {
                    Ipp32s firstMB = Task.m_iFirstMB;
                    if (m_field_index)
                    {
                        firstMB += mb_width*mb_height / 2;
                    }

                    umcRes = (this->*(Task.pFunction))(firstMB, Task.m_iMBToProcess);

                    if (UMC_ERR_END_OF_STREAM == umcRes)
                    {
                        Task.m_iMaxMB = Task.m_iFirstMB + Task.m_iMBToProcess;
                        VM_ASSERT(m_pSlice);
                        // if we decode less macroblocks if we need try to recovery:
                        RestoreErrorRect(Task.m_iFirstMB + Task.m_iMBToProcess, m_pSlice->GetMaxMB(), m_pSlice);
                        umcRes = UMC_OK;
                    }
                    else if (UMC_OK != umcRes)
                    {
                        umcRes = UMC_ERR_INVALID_STREAM;
                    }

                    Task.m_bDone = true;

                } catch(const h264_exception & ex)
                {
                    umcRes = ex.GetStatus();
                } catch(...)
                {
                    umcRes = UMC_ERR_INVALID_STREAM;
                }
            }

            if (umcRes != UMC_OK)
            {
                Task.m_bError = true;
                Task.m_iMaxMB = Task.m_iFirstMB + Task.m_iMBToProcess;
                if (m_pSlice)
                    RestoreErrorRect(Task.m_iFirstMB + Task.m_iMBToProcess, m_pSlice->GetMaxMB(), m_pSlice);
                // break; // DEBUG : ADB should we return if error occur??
            }

            EndProcessingSegment(Task);

            MFX_LTRACE_I(MFX_TRACE_LEVEL_INTERNAL, umcRes);
        }
        else
        {
            return UMC_ERR_NOT_ENOUGH_DATA;
        }
    }
    catch(h264_exception ex)
    {
        return ex.GetStatus();
    }
    catch(...)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    return UMC_OK;
} // Status H264SegmentDecoderMultiThreaded::ProcessSegment(void)

void H264SegmentDecoderMultiThreaded::RestoreErrorRect(Ipp32s startMb, Ipp32s endMb, H264Slice * pSlice)
{
    if (startMb >= endMb || !pSlice || pSlice->IsSliceGroups())
        return;

    m_pSlice = pSlice;
    m_isSliceGroups = m_pSlice->IsSliceGroups();
    m_pSeqParamSet = m_pSlice->GetSeqParam();

    try
    {
        H264DecoderFrame * pCurrentFrame = m_pSlice->GetCurrentFrame();

        if (!pCurrentFrame || !pCurrentFrame->m_pYPlane)
        {
            return;
        }

        H264DecoderFrame * pRefFrame = pCurrentFrame->GetRefPicList(m_pSlice->GetSliceNum(), 0)->m_RefPicList[0];

        pCurrentFrame->SetErrorFlagged(ERROR_FRAME_MAJOR);

        if (!pRefFrame || pRefFrame->IsSkipped())
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

        m_SD->RestoreErrorRect(startMb, endMb, pRefFrame, this);
    } catch (...)
    {
        // nothing to do
    }
}

Status H264SegmentDecoderMultiThreaded::DecodeSegment(Ipp32s iCurMBNumber, Ipp32s &iMBToProcess)
{
    Status umcRes = UMC_OK;
    Ipp32s iMaxMBNumber = iCurMBNumber + iMBToProcess;
    Ipp32s iMBRowSize = m_pSlice->GetMBRowWidth();
    Ipp32s iFirstMB = iCurMBNumber;
    ProcessLayerPreSlice();
    START_TICK

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
            if (m_pPicParamSet->entropy_coding_mode)
            {
                umcRes = DecodeMacroBlockCABAC(iCurMBNumber, nBorder);
            }
            else
            {
                umcRes = DecodeMacroBlockCAVLC(iCurMBNumber, nBorder);
            }
        } catch(...)
        {
            iMBToProcess = m_CurMBAddr - iFirstMB;
            throw;
        }

        // check error(s)
        if ((UMC_OK != umcRes) || (m_CurMBAddr >= iMaxMBNumber))
            break;

        // correct the MB number in a MBAFF case
        iCurMBNumber = iPreCallMBNumber -
                       (iPreCallMBNumber % iMBRowSize) +
                       iMBRowSize;
    }

    END_TICK(decode_time)

    if (UMC_ERR_END_OF_STREAM == umcRes)
    {
        iMBToProcess = m_CurMBAddr - iFirstMB;
    }

    return umcRes;

} // Status H264SegmentDecoderMultiThreaded::DecodeSegment(Ipp32s iCurMBNumber, Ipp32s &iMBToDecode)

Status H264SegmentDecoderMultiThreaded::ReconstructSegment(Ipp32s iCurMBNumber, Ipp32s &iMBToProcess)
{
    Status umcRes = UMC_OK;
    Ipp32s iMaxMBNumber = iCurMBNumber + iMBToProcess;
    Ipp32s iMBRowSize = m_pSlice->GetMBRowWidth();
    Ipp32s iFirstMB = iCurMBNumber;
    ProcessLayerPreSlice();
    START_TICK

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
            if (m_pPicParamSet->entropy_coding_mode)
            {
                umcRes = ReconstructMacroBlockCABAC(iCurMBNumber, nBorder);
            }
            else
            {
                umcRes = ReconstructMacroBlockCAVLC(iCurMBNumber, nBorder);
            }
        } catch(...)
        {
            iMBToProcess = m_CurMBAddr - iFirstMB;
            throw;
        }

        // check error(s)
        if ((UMC_OK != umcRes) || (m_CurMBAddr >= iMaxMBNumber))
            break;

        // correct the MB number in a MBAFF case
        iCurMBNumber = iPreCallMBNumber -
                       (iPreCallMBNumber % iMBRowSize) +
                       iMBRowSize;
    }

    END_TICK(decode_time)

    return umcRes;

} // Status H264SegmentDecoderMultiThreaded::ReconstructSegment(Ipp32s iCurMBNumber, Ipp32s &iMBToReconstruct)

Status H264SegmentDecoderMultiThreaded::DecRecSegment(Ipp32s iCurMBNumber, Ipp32s &iMBToProcess)
{
    Status umcRes = UMC_OK;
    Ipp32s iBorder = iCurMBNumber + iMBToProcess;

    ProcessLayerPreSlice();

    m_psBuffer = align_pointer<UMC::CoeffsPtrCommon> (m_pCoefficientsBuffer, DEFAULT_ALIGN_VALUE);

    try
    {
        if (m_pPicParamSet->entropy_coding_mode)
        {
            umcRes = m_SD->DecodeSegmentCABAC_Single(iCurMBNumber, iBorder, this);

        }
        else
        {
            umcRes = m_SD->DecodeSegmentCAVLC_Single(iCurMBNumber, iBorder, this);
        }
    }
    catch(...)
    {
        iMBToProcess = m_CurMBAddr - iCurMBNumber;
        throw;
    }

    iMBToProcess = m_CurMBAddr - iCurMBNumber;

    return umcRes;

} // Status H264SegmentDecoderMultiThreaded::DecRecSegment(Ipp32s iCurMBNumber, Ipp32s &iMBToReconstruct)

Status H264SegmentDecoderMultiThreaded::DeblockSegmentTask(Ipp32s iCurMBNumber, Ipp32s &iMBToProcess)
{
    START_TICK
    // when there is slice groups or threaded deblocking
    if (NULL == m_pSlice)
    {
        H264SegmentDecoder::DeblockFrame(iCurMBNumber, iMBToProcess);
        return UMC_OK;
    }

    Ipp32s iMaxMBNumber = iCurMBNumber + iMBToProcess;
    Ipp32s iMBRowSize = deblocking_IL ? mb_width : m_pSlice->GetMBRowWidth();
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
            iMBToProcess = m_CurMBAddr - iFirstMB;
            throw;
        }

        // check error(s)
        if ((m_CurMBAddr >= iMaxMBNumber))
            break;

        // correct the MB number in a MBAFF case
        iCurMBNumber = iPreCallMBNumber -
                       (iPreCallMBNumber % iMBRowSize) +
                       iMBRowSize;
    }

    END_TICK(deblocking_time)

    return UMC_OK;

} // Status H264SegmentDecoderMultiThreaded::DeblockSegmentTask(Ipp32s iCurMBNumber, Ipp32s &iMBToDeblock)

static const int block8x8_mask[4] = {
    (1 << 0) | (1 << 1) | (1 << 4) | (1 << 5),
    (1 << 2) | (1 << 3) | (1 << 6) | (1 << 7),
    (1 << 8) | (1 << 9) | (1 << 12) | (1 << 13),
    (1 << 10) | (1 << 11) | (1 << 14) | (1 << 15),
};

void H264SegmentDecoderMultiThreaded::DecodeMotionVectors_CABAC(void)
{
    Ipp32s curmb_fdf = pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
    // initialize blocks coding pattern maps

    Ipp32s type = m_cur_mb.GlobalMacroblockInfo->mbtype - MBTYPE_FORWARD;

    Ipp32s dir, dir_num;

    m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[0] = 0;
    m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[1] = 0;

    dir_num = m_pSliceHeader->slice_type == BPREDSLICE ? 2 : 1;

    memset(motion_pred_flag[0], 0, 4);
    memset(motion_pred_flag[1], 0, 4);

    Ipp32s is_motion_pred_flag_exist = 0;

    if (m_pSliceHeader->nal_ext.svc.no_inter_layer_pred_flag == 0 && inCropWindow(this->m_cur_mb))
    {
        memset(motion_pred_flag[0], m_pSliceHeader->default_motion_prediction_flag, 4);
        memset(motion_pred_flag[1], m_pSliceHeader->default_motion_prediction_flag, 4);

        if (m_pSliceHeader->adaptive_motion_prediction_flag)
        {
            is_motion_pred_flag_exist = 1;
            memset(motion_pred_flag[0], 0, 4);
            memset(motion_pred_flag[1], 0, 4);
        }
    }

    for (dir = 0; dir < dir_num; dir++)
    switch (m_cur_mb.GlobalMacroblockInfo->mbtype) {
        case MBTYPE_BIDIR:
            if (is_motion_pred_flag_exist)
            {
                motion_pred_flag[dir][0] = this->DecodeMBMotionPredictionFlag_CABAC(dir);
            }
            if (motion_pred_flag[dir][0])
                m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[dir] = 0xffff;//1;
            break;
        case MBTYPE_BACKWARD:
            if (is_motion_pred_flag_exist && dir == 1)
            {
                motion_pred_flag[dir][0] = this->DecodeMBMotionPredictionFlag_CABAC(dir);
            }
            if (motion_pred_flag[dir][0])
                m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[dir] = 0xffff;//1;
            break;
        case MBTYPE_FORWARD:
            if (is_motion_pred_flag_exist && dir == 0)
            {
                motion_pred_flag[dir][0] = this->DecodeMBMotionPredictionFlag_CABAC(dir);
            }
            if (motion_pred_flag[dir][0])
                m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[dir] = 0xffff;//1;
            break;
        case MBTYPE_INTER_16x8:
            {
                if (m_cur_mb.LocalMacroblockInfo->sbdir[0] == dir ||
                    m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BIDIR) {
                    if (is_motion_pred_flag_exist)
                        motion_pred_flag[dir][0] = this->DecodeMBMotionPredictionFlag_CABAC(dir);
                    if (motion_pred_flag[dir][0])
                        m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[dir] |=
                            block8x8_mask[0] | block8x8_mask[1];
                        //1;
                }
                if (m_cur_mb.LocalMacroblockInfo->sbdir[1] == dir ||
                    m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BIDIR) {
                    if (is_motion_pred_flag_exist)
                        motion_pred_flag[dir][1] = this->DecodeMBMotionPredictionFlag_CABAC(dir);
                    if (motion_pred_flag[dir][1])
                        m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[dir] |=
                            block8x8_mask[2] | block8x8_mask[3];
                        //(1 << 8);
                }
            }
            break;
        case MBTYPE_INTER_8x16:
            {
                if (m_cur_mb.LocalMacroblockInfo->sbdir[0] == dir ||
                    m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BIDIR) {
                    if (is_motion_pred_flag_exist)
                        motion_pred_flag[dir][0] = this->DecodeMBMotionPredictionFlag_CABAC(dir);
                    if (motion_pred_flag[dir][0])
                        m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[dir] |=
                            block8x8_mask[0] | block8x8_mask[2];
                        //1;
                }
                if (m_cur_mb.LocalMacroblockInfo->sbdir[1] == dir ||
                    m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BIDIR) {
                    if (is_motion_pred_flag_exist)
                        motion_pred_flag[dir][1] = this->DecodeMBMotionPredictionFlag_CABAC(dir);
                    if (motion_pred_flag[dir][1])
                        m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[dir] |=
                            block8x8_mask[1] | block8x8_mask[3];
                            //(1 << 2);
                }
            }
            break;
        case MBTYPE_INTER_8x8:
        case MBTYPE_INTER_8x8_REF0:
            {
                int j;
                for (j = 0; j < 4; j ++) {
                    if (m_cur_mb.LocalMacroblockInfo->sbdir[j] == dir ||
                        m_cur_mb.LocalMacroblockInfo->sbdir[j] == D_DIR_BIDIR) {
                        if (is_motion_pred_flag_exist)
                            motion_pred_flag[dir][j] = this->DecodeMBMotionPredictionFlag_CABAC(dir);
                        if (motion_pred_flag[dir][j]) {
                            m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[dir] |= 1 << (subblock_block_mapping[j]);
                            m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[dir] |= 1 << (subblock_block_mapping[j]+1);
                            m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[dir] |= 1 << (subblock_block_mapping[j]+4);
                            m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[dir] |= 1 << (subblock_block_mapping[j]+5);
                        }
                    }
                }
            }
            break;
        default:
            return;
    }

    if(type >= 0)
    {
        if (m_cur_mb.LocalMacroblockInfo->sbdir[0] > D_DIR_BIDIR) // direct or skip MB
        {
            memset(m_cur_mb.MVDelta[LIST_0], 0, sizeof(H264DecoderMacroblockMVs));
            memset(m_cur_mb.MVDelta[LIST_1], 0, sizeof(H264DecoderMacroblockMVs));
            return;
        }

        // get all ref_idx_L0
        GetRefIdx4x4_CABAC(
                    m_pSliceHeader->num_ref_idx_l0_active<<curmb_fdf,
                    pCodFBD[type][0],
                    0);
        // get all ref_idx_L1
        if (m_pSliceHeader->slice_type == BPREDSLICE)
        GetRefIdx4x4_CABAC(
                    m_pSliceHeader->num_ref_idx_l1_active<<curmb_fdf,
                    pCodFBD[type][1],
                    1);
        // get all mvd_L0
        GetMVD4x4_CABAC(
                    pCodFBD[type][2],
                    0);
        // get all mvd_L1
        if (m_pSliceHeader->slice_type == BPREDSLICE)
        GetMVD4x4_CABAC(
                    pCodFBD[type][3],
                    1);
    }
    else
    {
#ifdef __ICL
        __declspec(align(16))Ipp8u pCodMVdL0[16];
        __declspec(align(16))Ipp8u pCodMVdL1[16];
#else
        Ipp8u pCodMVdL0[16];
        Ipp8u pCodMVdL1[16];
#endif
        Ipp8u *pMVdL0 = pCodMVdL0;
        Ipp8u *pMVdL1 = pCodMVdL1;
        Ipp8u cL0;
        Ipp8u cL1;
        switch (m_cur_mb.GlobalMacroblockInfo->mbtype)
        {
        case MBTYPE_INTER_16x8:
                cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BWD) ? CodNone : CodInBS;
                cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_FWD) ? CodNone : CodInBS;
                pMVdL0[0] = cL0;
                pMVdL1[0] = cL1;
                cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BWD) ? CodNone : CodInBS;
                cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_FWD) ? CodNone : CodInBS;
                pMVdL0[8] = cL0;
                pMVdL1[8] = cL1;

                // get all ref_idx_L0
                GetRefIdx4x4_16x8_CABAC(
                            m_pSliceHeader->num_ref_idx_l0_active<<curmb_fdf,
                            pMVdL0,
                            0);

                // get all ref_idx_L1
                if (m_pSliceHeader->slice_type == BPREDSLICE)
                GetRefIdx4x4_16x8_CABAC(
                            m_pSliceHeader->num_ref_idx_l1_active<<curmb_fdf,
                            pMVdL1,
                            1);

                // get all mvd_L0
                GetMVD4x4_16x8_CABAC(
                            pMVdL0,
                            0);

                // get all mvd_L1
                if (m_pSliceHeader->slice_type == BPREDSLICE)
                GetMVD4x4_16x8_CABAC(
                            pMVdL1,
                            1);
                break;
            case MBTYPE_INTER_8x16:
                cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BWD) ? CodNone : CodInBS;
                cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_FWD) ? CodNone : CodInBS;
                pMVdL0[0] = cL0;
                pMVdL1[0] = cL1;
                cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BWD) ? CodNone : CodInBS;
                cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_FWD) ? CodNone : CodInBS;
                pMVdL0[2] = cL0;
                pMVdL1[2] = cL1;

                GetRefIdx4x4_8x16_CABAC(
                            m_pSliceHeader->num_ref_idx_l0_active<<curmb_fdf,
                            pMVdL0,
                            0);

                // get all ref_idx_L1
                if (m_pSliceHeader->slice_type == BPREDSLICE)
                GetRefIdx4x4_8x16_CABAC(
                            m_pSliceHeader->num_ref_idx_l1_active<<curmb_fdf,
                            pMVdL1,
                            1);

                // get all mvd_L0
                GetMVD4x4_8x16_CABAC(
                            pMVdL0,
                            0);

                // get all mvd_L1
                if (m_pSliceHeader->slice_type == BPREDSLICE)
                GetMVD4x4_8x16_CABAC(
                            pMVdL1,
                            1);
                break;
            case MBTYPE_INTER_8x8:
            case MBTYPE_INTER_8x8_REF0:
                Ipp8u pCodRIxL0[16];
                Ipp8u pCodRIxL1[16];
                const Ipp8u *pRIxL0, *pRIxL1;
                pRIxL0 = pCodRIxL0;
                pRIxL1 = pCodRIxL1;

                MFX_INTERNAL_CPY(pCodRIxL0, pCodTemplate, sizeof(pCodTemplate[0])*16);
                MFX_INTERNAL_CPY(pCodRIxL1, pCodTemplate, sizeof(pCodTemplate[0])*16);
                MFX_INTERNAL_CPY(pCodMVdL0, pCodTemplate, sizeof(pCodTemplate[0])*16);
                MFX_INTERNAL_CPY(pCodMVdL1, pCodTemplate, sizeof(pCodTemplate[0])*16);
                for (Ipp32s i = 0; i < 4; i ++)
                {
                    Ipp32s j = subblock_block_mapping[i];

                    if (m_cur_mb.LocalMacroblockInfo->sbdir[i] > D_DIR_BIDIR)
                    {
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_DIRECT_SPATIAL_BWD) ? CodNone : CodSkip;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_DIRECT_SPATIAL_FWD) ? CodNone : CodSkip;
                        pCodRIxL0[j] = pCodRIxL0[j + 1] = pCodRIxL0[j + 4] = pCodRIxL0[j + 5] = cL0;
                        pCodRIxL1[j] = pCodRIxL1[j + 1] = pCodRIxL1[j + 4] = pCodRIxL1[j + 5] = cL1;
                        pCodMVdL0[j] = pCodMVdL0[j + 1] = pCodMVdL0[j + 4] = pCodMVdL0[j + 5] = cL0;
                        pCodMVdL1[j] = pCodMVdL1[j + 1] = pCodMVdL1[j + 4] = pCodMVdL1[j + 5] = cL1;
                        continue;
                    }

                    switch (m_cur_mb.GlobalMacroblockInfo->sbtype[i])
                    {
                    case SBTYPE_8x8:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_BWD) ? CodNone : CodInBS;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_FWD) ? CodNone : CodInBS;

                        if (motion_pred_flag[0][i] && cL0 == CodInBS)
                            pCodRIxL0[j] = CodSkip;
                        else
                            pCodRIxL0[j] = cL0;

                        if (motion_pred_flag[1][i] && cL1 == CodInBS)
                            pCodRIxL1[j] = CodSkip;
                        else
                            pCodRIxL1[j] = cL1;

                        pCodMVdL0[j] = cL0;
                        pCodMVdL1[j] = cL1;
                        pCodRIxL0[j + 4] = CodAbov;
                        pCodRIxL1[j + 4] = CodAbov;
                        pCodMVdL0[j + 4] = CodAbov;
                        pCodMVdL1[j + 4] = CodAbov;
                        break;
                    case SBTYPE_8x4:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_BWD) ? CodNone : CodInBS;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_FWD) ? CodNone : CodInBS;

                        if (motion_pred_flag[0][i] && cL0 == CodInBS)
                            pCodRIxL0[j] = CodSkip;
                        else
                            pCodRIxL0[j] = cL0;

                        if (motion_pred_flag[1][i] && cL1 == CodInBS)
                            pCodRIxL1[j] = CodSkip;
                        else
                            pCodRIxL1[j] = cL1;

                        pCodMVdL0[j] = cL0;
                        pCodMVdL1[j] = cL1;
                        pCodRIxL0[j + 4] = CodAbov;
                        pCodRIxL1[j + 4] = CodAbov;
                        pCodMVdL0[j + 4] = cL0;
                        pCodMVdL1[j + 4] = cL1;
                        break;
                    case SBTYPE_4x8:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_BWD) ? CodNone : CodInBS;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_FWD) ? CodNone : CodInBS;

                        if (motion_pred_flag[0][i] && cL0 == CodInBS)
                            pCodRIxL0[j] = CodSkip;
                        else
                            pCodRIxL0[j] = cL0;

                        if (motion_pred_flag[1][i] && cL1 == CodInBS)
                            pCodRIxL1[j] = CodSkip;
                        else
                            pCodRIxL1[j] = cL1;

                        pCodMVdL0[j] = cL0;
                        pCodMVdL1[j] = cL1;
                        pCodMVdL0[j + 1] = cL0;
                        pCodMVdL1[j + 1] = cL1;
                        pCodRIxL0[j + 4] = CodAbov;
                        pCodRIxL1[j + 4] = CodAbov;
                        pCodMVdL0[j + 4] = pCodMVdL0[j + 5] = CodAbov;
                        pCodMVdL1[j + 4] = pCodMVdL1[j + 5] = CodAbov;
                        break;
                    case SBTYPE_4x4:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_BWD) ? CodNone : CodInBS;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_FWD) ? CodNone : CodInBS;

                        if (motion_pred_flag[0][i] && cL0 == CodInBS)
                            pCodRIxL0[j] = CodSkip;
                        else
                            pCodRIxL0[j] = cL0;

                        if (motion_pred_flag[1][i] && cL1 == CodInBS)
                            pCodRIxL1[j] = CodSkip;
                        else
                            pCodRIxL1[j] = cL1;

                        pCodRIxL0[j + 4] = CodAbov;
                        pCodRIxL1[j + 4] = CodAbov;
                        pCodMVdL0[j] = pCodMVdL0[j + 1] = pCodMVdL0[j + 4] = pCodMVdL0[j + 5] = cL0;
                        pCodMVdL1[j] = pCodMVdL1[j + 1] = pCodMVdL1[j + 4] = pCodMVdL1[j + 5] = cL1;
                        break;
                    case SBTYPE_DIRECT:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_DIRECT_SPATIAL_BWD) ? CodNone : CodSkip;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_DIRECT_SPATIAL_FWD) ? CodNone : CodSkip;
                        pCodRIxL0[j] = pCodRIxL0[j + 1] = pCodRIxL0[j + 4] = pCodRIxL0[j + 5] = cL0;
                        pCodRIxL1[j] = pCodRIxL1[j + 1] = pCodRIxL1[j + 4] = pCodRIxL1[j + 5] = cL1;
                        pCodMVdL0[j] = pCodMVdL0[j + 1] = pCodMVdL0[j + 4] = pCodMVdL0[j + 5] = cL0;
                        pCodMVdL1[j] = pCodMVdL1[j + 1] = pCodMVdL1[j + 4] = pCodMVdL1[j + 5] = cL1;
                        break;
                    default:
                        throw h264_exception(UMC_ERR_INVALID_STREAM);
                    }
                }
                // get all ref_idx_L0
                GetRefIdx4x4_CABAC(
                            m_pSliceHeader->num_ref_idx_l0_active<<curmb_fdf,
                            BlkOrder,
                            pRIxL0,
                            0);

                // get all ref_idx_L1
                if (m_pSliceHeader->slice_type == BPREDSLICE)
                GetRefIdx4x4_CABAC(
                            m_pSliceHeader->num_ref_idx_l1_active<<curmb_fdf,
                            BlkOrder,
                            pRIxL1,
                            1);

                // get all mvd_L0
                GetMVD4x4_CABAC(
                            BlkOrder,
                            pMVdL0,
                            0);

                // get all mvd_L1
                if (m_pSliceHeader->slice_type == BPREDSLICE)
                GetMVD4x4_CABAC(
                            BlkOrder,
                            pMVdL1,
                            1);

                if (m_pSliceHeader->nal_unit_type == NAL_UT_CODED_SLICE_EXTENSION && m_pSliceHeader->nal_ext.svc_extension_flag)
                {
                    Ipp32s i, j;
                    for (i = 0; i < 4; i ++)
                    {
                        RefIndexType RefIndexL0, RefIndexL1;
                        Ipp32s uPredDir;
                        if (m_cur_mb.LocalMacroblockInfo->sbdir[i] <= D_DIR_BIDIR)
                            continue;
                        j = subblock_block_mapping[i];
                        ComputeDirectSpatialRefIdx8x8(&RefIndexL0, &RefIndexL1, i);

                        if ((RefIndexL0 < 0) && (RefIndexL1 < 0))
                            RefIndexL0 = RefIndexL1 = 0;

                        if ((0 <= RefIndexL0) && (0 > RefIndexL1))
                            uPredDir = D_DIR_DIRECT_SPATIAL_FWD;
                        else if ((0 > RefIndexL0) && (0 <= RefIndexL1))
                            uPredDir = D_DIR_DIRECT_SPATIAL_BWD;
                        else
                            uPredDir = D_DIR_DIRECT_SPATIAL_BIDIR;

                        m_cur_mb.RefIdxs[0]->refIndexs[i] = (RefIndexType)RefIndexL0;
                        m_cur_mb.RefIdxs[1]->refIndexs[i] = (RefIndexType)RefIndexL1;

                        m_cur_mb.LocalMacroblockInfo->sbdir[i]=(Ipp8s)uPredDir;
                        m_cur_mb.GlobalMacroblockInfo->sbtype[i]=SBTYPE_8x8;
                    }   // for sb
                }

                break;
            default:
                throw h264_exception(UMC_ERR_INVALID_STREAM);
        }    // switch

    }
    Ipp32s sbdir_store[4];
    if (m_pSliceHeader->nal_unit_type == NAL_UT_CODED_SLICE_EXTENSION && m_pSliceHeader->nal_ext.svc_extension_flag)
    {
        for (Ipp32s block = 0; block < 4; block++)
        {
            sbdir_store[block] = m_cur_mb.LocalMacroblockInfo->sbdir[block];
            if      (m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_DIRECT_SPATIAL_FWD)
                m_cur_mb.LocalMacroblockInfo->sbdir[block] = D_DIR_FWD;
            else if (m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_DIRECT_SPATIAL_BWD)
                m_cur_mb.LocalMacroblockInfo->sbdir[block] = D_DIR_BWD;
            else if (m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_DIRECT_SPATIAL_BIDIR)
                m_cur_mb.LocalMacroblockInfo->sbdir[block] = D_DIR_BIDIR;
        }
    }

    ReconstructMotionVectors();
    if (m_pSliceHeader->nal_unit_type == NAL_UT_CODED_SLICE_EXTENSION && m_pSliceHeader->nal_ext.svc_extension_flag)
    {
        for (Ipp32s block = 0; block < 4; block++)
        {
            m_cur_mb.LocalMacroblockInfo->sbdir[block] = (Ipp8s)sbdir_store[block];
        }
    }

} // void H264SegmentDecoderMultiThreaded::DecodeMotionVectors_CABAC(void)

#define RECONSTRUCT_MOTION_VECTORS(sub_block_num, vector_offset, position) \
    switch (pSBDir[sub_block_num]) \
    { \
    case D_DIR_FWD: \
        if (m_cur_mb.GetRefIdx(0, sub_block_num) == -1)\
        {\
            ResetMVs8x8(0, vector_offset);\
        }\
        else\
            ReconstructMVs##position(0); \
        ResetMVs8x8(1, vector_offset); \
        break; \
    case D_DIR_BIDIR: \
        if (m_cur_mb.GetRefIdx(0, sub_block_num) == -1)\
        {\
            ResetMVs8x8(0, vector_offset);\
        }\
        else\
            ReconstructMVs##position(0); \
        \
        if (m_cur_mb.GetRefIdx(1, sub_block_num) == -1)\
        {\
            ResetMVs8x8(1, vector_offset);\
        }\
        else\
            ReconstructMVs##position(1); \
        break; \
    case D_DIR_BWD: \
        ResetMVs8x8(0, vector_offset); \
        if (m_cur_mb.GetRefIdx(1, sub_block_num) == -1)\
        {\
            ResetMVs8x8(1, vector_offset);\
        }\
        else\
            ReconstructMVs##position(1); \
        break; \
    case D_DIR_DIRECT_SPATIAL_FWD: \
        ResetMVs8x8(1, vector_offset); \
        break; \
    case D_DIR_DIRECT_SPATIAL_BWD: \
        ResetMVs8x8(0, vector_offset); \
        break; \
    default: \
        break; \
    }

void H264SegmentDecoderMultiThreaded::ReconstructMotionVectors(void)
{
    /* DEBUG : temporary put off MBAFF */
    if (false == m_isMBAFF)
    {
        switch (m_cur_mb.GlobalMacroblockInfo->mbtype)
        {
            // forward predicted macroblock
        case MBTYPE_FORWARD:
            //if (m_cur_mb.LocalMacroblockInfo->sbdir[0] <= D_DIR_BIDIR) // DEBUG : what check did?
            {
                // set forward vectors
                ReconstructMVs16x16(0);

                // set backward vectors
                ResetMVs16x16(1);
            }
            break;

            // backward predicted macroblock
        case MBTYPE_BACKWARD:
            //if (m_cur_mb.LocalMacroblockInfo->sbdir[0] <= D_DIR_BIDIR)
            {
                // set forward vectors
                ResetMVs16x16(0);

                // set backward vectors
                ReconstructMVs16x16(1);
            }
            break;

            // bi-directional predicted macroblock
        case MBTYPE_BIDIR:
            //if (m_cur_mb.LocalMacroblockInfo->sbdir[0] <= D_DIR_BIDIR)
            {
                // set forward vectors
                ReconstructMVs16x16(0);

                // set backward vectors
                ReconstructMVs16x16(1);
            }
            break;

        case MBTYPE_INTER_16x8:
            {
                // set forward vectors for the first sub-block
                if (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BWD)
                    ResetMVs16x8(0, 0);
                else
                    ReconstructMVs16x8(0, 0);

                // set backward vectors for the first sub-block
                if (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_FWD)
                    ResetMVs16x8(1, 0);
                else
                    ReconstructMVs16x8(1, 0);

                // set forward vectors for the second sub-block
                if (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BWD)
                    ResetMVs16x8(0, 8);
                else
                    ReconstructMVs16x8(0, 1);

                // set backward vectors for the second sub-block
                if (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_FWD)
                    ResetMVs16x8(1, 8);
                else
                    ReconstructMVs16x8(1, 1);
            }
            break;

        case MBTYPE_INTER_8x16:
            {
                // set forward vectors for the first sub-block
                if (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BWD)
                    ResetMVs8x16(0, 0);
                else
                    ReconstructMVs8x16(0, 0);

                // set backward vectors for the first sub-block
                if (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_FWD)
                    ResetMVs8x16(1, 0);
                else
                    ReconstructMVs8x16(1, 0);

                // set forward vectors for the second sub-block
                if (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BWD)
                    ResetMVs8x16(0, 2);
                else
                    ReconstructMVs8x16(0, 1);

                // set backward vectors for the second sub-block
                if (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_FWD)
                    ResetMVs8x16(1, 2);
                else
                    ReconstructMVs8x16(1, 1);

                CopyMVs8x16(0);
                CopyMVs8x16(1);
            }
            break;

        case MBTYPE_INTER_8x8:
        case MBTYPE_INTER_8x8_REF0:
            {
                Ipp8s *pSBDir = m_cur_mb.LocalMacroblockInfo->sbdir;

                RECONSTRUCT_MOTION_VECTORS(0, 0, External);
                RECONSTRUCT_MOTION_VECTORS(1, 2, Top);
                RECONSTRUCT_MOTION_VECTORS(2, 8, Left);
                RECONSTRUCT_MOTION_VECTORS(3, 10, Internal);
            }
            break;

            // all other
        default:
            return;
            break;
        }

        return;
    }


    // initialize blocks coding pattern maps

    Ipp32s type = m_cur_mb.GlobalMacroblockInfo->mbtype - MBTYPE_FORWARD;
    if(type >= 0)
    {
        if (m_cur_mb.LocalMacroblockInfo->sbdir[0] > D_DIR_BIDIR)
            return;

        // get all mvd_L0
        ReconstructMotionVectors4x4(pCodFBD[type][2], 0);
        // get all mvd_L1
        ReconstructMotionVectors4x4(pCodFBD[type][3], 1);
    }
    else
    {
        Ipp32u i, j;
        const Ipp8u *pRIxL0, *pRIxL1, *pMVdL0, *pMVdL1;
#ifdef __ICL
        __declspec(align(16))Ipp8u pCodRIxL0[16];
        __declspec(align(16))Ipp8u pCodRIxL1[16];
        __declspec(align(16))Ipp8u pCodMVdL0[16];
        __declspec(align(16))Ipp8u pCodMVdL1[16];
#else
        Ipp8u pCodRIxL0[16];
        Ipp8u pCodRIxL1[16];
        Ipp8u pCodMVdL0[16];
        Ipp8u pCodMVdL1[16];
#endif
        pRIxL0 = pCodRIxL0;
        pRIxL1 = pCodRIxL1;
        pMVdL0 = pCodMVdL0;
        pMVdL1 = pCodMVdL1;

        Ipp8u cL0;
        Ipp8u cL1;
        switch (m_cur_mb.GlobalMacroblockInfo->mbtype)
        {
        case MBTYPE_INTER_16x8:
                cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BWD) ? CodNone : CodInBS;
                cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_FWD) ? CodNone : CodInBS;
                pCodRIxL0[0] = cL0;
                pCodRIxL1[0] = cL1;
                cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BWD) ? CodNone : CodInBS;
                cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_FWD) ? CodNone : CodInBS;
                pCodRIxL0[8] = cL0;
                pCodRIxL1[8] = cL1;

                // get all mvd_L0
                ReconstructMotionVectors16x8(pRIxL0, 0);

                // get all mvd_L1
                ReconstructMotionVectors16x8(pRIxL1, 1);
                break;
            case MBTYPE_INTER_8x16:
                cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BWD) ? CodNone : CodInBS;
                cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_FWD) ? CodNone : CodInBS;
                pCodRIxL0[0] = cL0;
                pCodRIxL1[0] = cL1;
                cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BWD) ? CodNone : CodInBS;
                cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_FWD) ? CodNone : CodInBS;
                pCodRIxL0[2] = cL0;
                pCodRIxL1[2] = cL1;

                // get all mvd_L0
                ReconstructMotionVectors8x16(pRIxL0, 0);

                // get all mvd_L1
                ReconstructMotionVectors8x16(pRIxL1, 1);
                break;
            case MBTYPE_INTER_8x8:
            case MBTYPE_INTER_8x8_REF0:
                MFX_INTERNAL_CPY(pCodRIxL0, pCodTemplate, sizeof(pCodTemplate[0])*16);
                MFX_INTERNAL_CPY(pCodRIxL1, pCodTemplate, sizeof(pCodTemplate[0])*16);
                MFX_INTERNAL_CPY(pCodMVdL0, pCodTemplate, sizeof(pCodTemplate[0])*16);
                MFX_INTERNAL_CPY(pCodMVdL1, pCodTemplate, sizeof(pCodTemplate[0])*16);
                {
                for (i = 0; i < 4; i ++)
                {
                    j = subblock_block_mapping[i];

                    Ipp32s sbtype = (m_cur_mb.LocalMacroblockInfo->sbdir[i] < D_DIR_DIRECT) ?
                        m_cur_mb.GlobalMacroblockInfo->sbtype[i] : SBTYPE_DIRECT;

                    switch (sbtype)
                    {
                    case SBTYPE_8x8:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_BWD) ? CodNone : CodInBS;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_FWD) ? CodNone : CodInBS;
                        pCodRIxL0[j] = cL0;
                        pCodRIxL1[j] = cL1;
                        pCodMVdL0[j] = cL0;
                        pCodMVdL1[j] = cL1;
                        pCodRIxL0[j + 4] = CodAbov;
                        pCodRIxL1[j + 4] = CodAbov;
                        pCodMVdL0[j + 4] = CodAbov;
                        pCodMVdL1[j + 4] = CodAbov;
                        break;
                    case SBTYPE_8x4:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_BWD) ? CodNone : CodInBS;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_FWD) ? CodNone : CodInBS;
                        pCodRIxL0[j] = cL0;
                        pCodRIxL1[j] = cL1;
                        pCodMVdL0[j] = cL0;
                        pCodMVdL1[j] = cL1;
                        pCodRIxL0[j + 4] = CodAbov;
                        pCodRIxL1[j + 4] = CodAbov;
                        pCodMVdL0[j + 4] = cL0;
                        pCodMVdL1[j + 4] = cL1;
                        break;
                    case SBTYPE_4x8:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_BWD) ? CodNone : CodInBS;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_FWD) ? CodNone : CodInBS;
                        pCodRIxL0[j] = cL0;
                        pCodRIxL1[j] = cL1;
                        pCodMVdL0[j] = cL0;
                        pCodMVdL1[j] = cL1;
                        pCodMVdL0[j + 1] = cL0;
                        pCodMVdL1[j + 1] = cL1;
                        pCodRIxL0[j + 4] = CodAbov;
                        pCodRIxL1[j + 4] = CodAbov;
                        pCodMVdL0[j + 4] = pCodMVdL0[j + 5] = CodAbov;
                        pCodMVdL1[j + 4] = pCodMVdL1[j + 5] = CodAbov;
                        break;
                    case SBTYPE_4x4:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_BWD) ? CodNone : CodInBS;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_FWD) ? CodNone : CodInBS;
                        pCodRIxL0[j] = cL0;
                        pCodRIxL1[j] = cL1;
                        pCodRIxL0[j + 4] = CodAbov;
                        pCodRIxL1[j + 4] = CodAbov;
                        pCodMVdL0[j] = pCodMVdL0[j + 1] = pCodMVdL0[j + 4] = pCodMVdL0[j + 5] = cL0;
                        pCodMVdL1[j] = pCodMVdL1[j + 1] = pCodMVdL1[j + 4] = pCodMVdL1[j + 5] = cL1;
                        break;
                    case SBTYPE_DIRECT:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_DIRECT_SPATIAL_BWD) ? CodNone : CodSkip;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_DIRECT_SPATIAL_FWD) ? CodNone : CodSkip;
                        pCodRIxL0[j] = pCodRIxL0[j + 1] = pCodRIxL0[j + 4] = pCodRIxL0[j + 5] = cL0;
                        pCodRIxL1[j] = pCodRIxL1[j + 1] = pCodRIxL1[j + 4] = pCodRIxL1[j + 5] = cL1;
                        pCodMVdL0[j] = pCodMVdL0[j + 1] = pCodMVdL0[j + 4] = pCodMVdL0[j + 5] = cL0;
                        pCodMVdL1[j] = pCodMVdL1[j + 1] = pCodMVdL1[j + 4] = pCodMVdL1[j + 5] = cL1;
                        break;
                    default:
                        throw h264_exception(UMC_ERR_INVALID_STREAM);
                    }
                }
                // get all mvd_L0
                ReconstructMotionVectors4x4(BlkOrder,
                                            pMVdL0,
                                            0);

                // get all mvd_L1
                ReconstructMotionVectors4x4(BlkOrder,
                                            pMVdL1,
                                            1);
                break;
                }
            default:
                VM_ASSERT(false);
                throw h264_exception(UMC_ERR_INVALID_STREAM);
                break;
        }    // switch

    }
} // void H264SegmentDecoderMultiThreaded::ReconstructMotionVectors(void)

bool IsColocatedSame(Ipp32u cur_pic_struct, Ipp32u ref_pic_struct)
{
    if (cur_pic_struct == ref_pic_struct && cur_pic_struct != AFRM_STRUCTURE)
        return true;

    return false;
}

void H264SegmentDecoderMultiThreaded::ReconstructDirectMotionVectorsSpatial(bool isDirectMB)
{
    H264DecoderMotionVector  mvL0, mvL1;
    VM_ASSERT(m_pRefPicList[1][0]);

    Ipp32s field = m_pFields[1][0].field;
    bool bL1RefPicisShortTerm = m_pFields[1][0].isShortReference;

    // set up pointers to where MV and RefIndex will be stored
    H264DecoderMotionVector *pFwdMV = m_cur_mb.MVs[0]->MotionVectors;
    H264DecoderMotionVector *pBwdMV = m_cur_mb.MVs[1]->MotionVectors;

    Ipp32s bAll4x4AreSame;
    Ipp32s bAll8x8AreSame = 0;

    Ipp32s refIdxL0, refIdxL1;

    // find the reference indexes
    ComputeDirectSpatialRefIdx(&refIdxL0, &refIdxL1);
    RefIndexType RefIndexL0 = (RefIndexType)refIdxL0;
    RefIndexType RefIndexL1 = (RefIndexType)refIdxL1;

    // set up reference index array
    {
        RefIndexType *pFwdRefInd = m_cur_mb.GetReferenceIndexStruct(0)->refIndexs;
        RefIndexType *pBwdRefInd = m_cur_mb.GetReferenceIndexStruct(1)->refIndexs;

        if (0 <= (RefIndexL0 & RefIndexL1))
        {
            memset(pFwdRefInd, RefIndexL0, 4);
            memset(pBwdRefInd, RefIndexL1, 4);
        }
        else
        {
            memset(pFwdRefInd, 0, 4);
            memset(pBwdRefInd, 0, 4);
        }
    }

    // Because predicted MV is computed using 16x16 block it is likely
    // that all 4x4 blocks will use the same MV and reference frame.
    // It is possible, however, for the MV for any 4x4 block to be set
    // to 0,0 instead of the computed MV. This possibility by default
    // forces motion compensation to be performed for each 4x4, the slowest
    // possible option. These booleans are used to detect when all of the
    // 4x4 blocks in an 8x8 can be combined for motion comp, and even better,
    // when all of the 8x8 blocks in the macroblock can be combined.

    // Change mbtype to any INTER 16x16 type, for computeMV function,
    // required for the 8x8 DIRECT case to force computeMV to get MV
    // using 16x16 type instead.
    m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_FORWARD;

    if (RefIndexL0 != -1) // Forward MV (L0)
    {
        // L0 ref idx exists, use to obtain predicted L0 motion vector
        // for the macroblock
        if (false == m_isMBAFF)
            ReconstructMVPredictorExternalBlock(0, m_cur_mb.CurrentBlockNeighbours.mb_above_right, &mvL0);
        else
            ReconstructMVPredictorExternalBlockMBAFF(0, m_cur_mb.CurrentBlockNeighbours.mb_above_right, &mvL0);
    }
    else
    {
        mvL0 = zeroVector;
    }

    if (RefIndexL1 != -1)    // Backward MV (L1)
    {
        // L0 ref idx exists, use to obtain predicted L0 motion vector
        // for the macroblock
        if (false == m_isMBAFF)
            ReconstructMVPredictorExternalBlock(1, m_cur_mb.CurrentBlockNeighbours.mb_above_right, &mvL1);
        else
            ReconstructMVPredictorExternalBlockMBAFF(1, m_cur_mb.CurrentBlockNeighbours.mb_above_right, &mvL1);
    }
    else
    {
        mvL1 = zeroVector;
    }

    if (mvL0.mvy > m_MVDistortion[0])
        m_MVDistortion[0] = mvL0.mvy;

    if (mvL1.mvy > m_MVDistortion[1])
        m_MVDistortion[1] = mvL1.mvy;

    // set direction for MB
    Ipp32u uPredDir;    // prediction direction of macroblock
    if ((0 <= RefIndexL0) && (0 > RefIndexL1))
    {
        uPredDir = D_DIR_DIRECT_SPATIAL_FWD;
        m_cur_mb.GlobalMacroblockInfo->mbtype= MBTYPE_FORWARD;
    }
    else if ((0 > RefIndexL0) && (0 <= RefIndexL1))
    {
        uPredDir = D_DIR_DIRECT_SPATIAL_BWD;
        m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_BACKWARD;
    }
    else
    {
        uPredDir = D_DIR_DIRECT_SPATIAL_BIDIR;
        m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_BIDIR;
    }

    if (isDirectMB && !(*(Ipp32u*)&mvL0) && !(*(Ipp32u*)&mvL1))
    {
        fill_n<H264DecoderMotionVector>(pFwdMV, 16, mvL0);
        fill_n<H264DecoderMotionVector>(pBwdMV, 16, mvL1);
        return;
    }

    bool allColocatedSame = IsColocatedSame(m_pCurrentFrame->m_PictureStructureForDec, m_pRefPicList[1][0]->m_PictureStructureForDec);

    // In loops below, set MV and RefIdx for all subblocks. Conditionally
    // change MV to 0,0 and RefIndex to 0 (doing so called UseZeroPred here).
    // To select UseZeroPred for a part:
    //  (RefIndexLx < 0) ||
    //  (bL1RefPicisShortTerm && RefIndexLx==0 && ColocatedRefIndex==0 &&
    //    (colocated motion vectors in range -1..+1)
    // When both RefIndexLx are -1, ZeroPred is used and both RefIndexLx
    // are changed to zero.

    // It is desirable to avoid checking the colocated motion vectors and
    // colocated ref index, bools and orders of conditional testing are
    // set up to do so.

    // At the MB level, the colocated do not need to be checked if:
    //  - both RefIndexLx < 0
    //  - colocated is INTRA (know all colocated RefIndex are -1)
    //  - L1 Ref Pic is not short term
    // set bMaybeUseZeroPred to true if any of the above are false

    // Set MV for all DIRECT 4x4 subblocks
    Ipp32s sb, sbcolpartoffset;
    Ipp8s colocatedRefIndex;
    H264DecoderMotionVector *pRefMV; // will be colocated L0 or L1
    Ipp32s MBsCol[4];
    Ipp32s sbColPartOffset[4];

    if (m_IsUseDirect8x8Inference)
    {
        sbColPartOffset[0] = 0;
        sbColPartOffset[2] = 12;
        MBsCol[0] = GetColocatedLocation(m_pRefPicList[1][0], field, sbColPartOffset[0]);
        sbColPartOffset[1] = sbColPartOffset[0] + 3;
        sbColPartOffset[3] = sbColPartOffset[2] + 3;
    }
    else
    {
        sbColPartOffset[0] = 0;
        sbColPartOffset[2] = 8;
        MBsCol[0] = GetColocatedLocation(m_pRefPicList[1][0], field, sbColPartOffset[0]);
        sbColPartOffset[1] = sbColPartOffset[0] + 2;
        sbColPartOffset[3] = sbColPartOffset[2] + 2;
    }

    Ipp32u MBCol = MBsCol[0];
    H264DecoderMotionVector *pRefMVL0 = m_pRefPicList[1][0]->m_mbinfo.MV[0][MBCol].MotionVectors;
    H264DecoderMotionVector *pRefMVL1 = m_pRefPicList[1][0]->m_mbinfo.MV[1][MBCol].MotionVectors;
    bool isInter = IS_INTER_MBTYPE(m_pRefPicList[1][0]->m_mbinfo.mbs[MBCol].mbtype);
    RefIndexType * refIndexes = GetRefIdxs(&m_pRefPicList[1][0]->m_mbinfo, 0, MBCol);
    bool isNeedToCheckMVSBase = isInter && bL1RefPicisShortTerm && ((RefIndexL0 != -1) || (RefIndexL1 != -1));

    bool isAll4x4RealSame1[4];
    if (allColocatedSame)
    {
        switch(m_pRefPicList[1][0]->m_mbinfo.mbs[MBCol].mbtype)
        {
        case MBTYPE_INTER_16x8:
        case MBTYPE_INTER_8x16:
            isAll4x4RealSame1[0] = isAll4x4RealSame1[1] = isAll4x4RealSame1[2] = isAll4x4RealSame1[3] = true;
            break;
        case MBTYPE_INTER_8x8:
        case MBTYPE_INTER_8x8_REF0:
            isAll4x4RealSame1[0] = isAll4x4RealSame1[1] = isAll4x4RealSame1[2] = isAll4x4RealSame1[3] = false;
            break;

        case MBTYPE_INTRA:
        case MBTYPE_INTRA_16x16:
        case MBTYPE_PCM:
            if (isDirectMB)
            {
                fill_n<H264DecoderMotionVector>(pFwdMV, 16, mvL0);
                fill_n<H264DecoderMotionVector>(pBwdMV, 16, mvL1);
                return;
            }
            else
            {
                isAll4x4RealSame1[0] = isAll4x4RealSame1[1] = isAll4x4RealSame1[2] = isAll4x4RealSame1[3] = true;
            }
            break;

        default: // 16x16
            if (isDirectMB)
            {
                colocatedRefIndex = GetReferenceIndex(refIndexes, 0);
                if (colocatedRefIndex >= 0)
                {
                    // use L0 colocated
                    pRefMV = pRefMVL0;
                }
                else
                {
                    // use L1 colocated
                    colocatedRefIndex = GetReferenceIndex(&m_pRefPicList[1][0]->m_mbinfo, 1, MBCol, 0);
                    pRefMV = pRefMVL1;
                }

                bool isNeedToCheckMVS = isNeedToCheckMVSBase && (colocatedRefIndex == 0);

                bool bUseZeroPredL0 = false;
                bool bUseZeroPredL1 = false;

                if (isNeedToCheckMVS)
                {
                    VM_ASSERT(pRefMV);
                    if (pRefMV[0].mvx >= -1 &&
                        pRefMV[0].mvx <= 1 &&
                        pRefMV[0].mvy >= -1 &&
                        pRefMV[0].mvy <= 1)
                    {
                        // All subpart conditions for forcing zero pred are met,
                        // use final RefIndexLx==0 condition for L0,L1.
                        bUseZeroPredL0 = (RefIndexL0 == 0);
                        bUseZeroPredL1 = (RefIndexL1 == 0);
                    }
                }

                fill_n<H264DecoderMotionVector>(pFwdMV, 16, bUseZeroPredL0 ? zeroVector : mvL0);
                fill_n<H264DecoderMotionVector>(pBwdMV, 16, bUseZeroPredL1 ? zeroVector : mvL1);
                return;
            }
            else
            {
                isAll4x4RealSame1[0] = isAll4x4RealSame1[1] = isAll4x4RealSame1[2] = isAll4x4RealSame1[3] = true;
            }
            break;
        };
    }
    else
    {
        MBsCol[1] = MBsCol[0];
        MBsCol[2] = GetColocatedLocation(m_pRefPicList[1][0], field, sbColPartOffset[2]);
        MBsCol[3] = MBsCol[2];
        sbColPartOffset[3] = sbColPartOffset[2] + (m_IsUseDirect8x8Inference ? 3 : 2);
        isAll4x4RealSame1[0] = isAll4x4RealSame1[1] = isAll4x4RealSame1[2] = isAll4x4RealSame1[3] = true;
    }

    for (sb = 0; sb < 4; sb++)
    {
        if (m_cur_mb.GlobalMacroblockInfo->sbtype[sb] != SBTYPE_DIRECT)
        {
            bAll8x8AreSame = 17;
            continue;
        }

        // DIRECT 8x8 block
        bAll4x4AreSame = 0;

        if (!allColocatedSame)
        {
            MBCol = MBsCol[sb];
            pRefMVL0 = m_pRefPicList[1][0]->m_mbinfo.MV[0][MBCol].MotionVectors;
            pRefMVL1 = m_pRefPicList[1][0]->m_mbinfo.MV[1][MBCol].MotionVectors;
            isInter = IS_INTER_MBTYPE(m_pRefPicList[1][0]->m_mbinfo.mbs[MBCol].mbtype);
            isNeedToCheckMVSBase = isInter && bL1RefPicisShortTerm && ((RefIndexL0 != -1) || (RefIndexL1 != -1));
            refIndexes = GetRefIdxs(&m_pRefPicList[1][0]->m_mbinfo, 0, MBCol);
        }

        sbcolpartoffset = sbColPartOffset[sb];

        colocatedRefIndex = GetReferenceIndex(refIndexes, sbcolpartoffset);
        if (colocatedRefIndex >= 0)
        {
            // use L0 colocated
            pRefMV = pRefMVL0;
        }
        else
        {
            // use L1 colocated
            colocatedRefIndex = GetReferenceIndex(&m_pRefPicList[1][0]->m_mbinfo, 1, MBCol, sbcolpartoffset);
            pRefMV = pRefMVL1;
        }

        bool isNeedToCheckMVS = isNeedToCheckMVSBase && (colocatedRefIndex == 0);

        if (isNeedToCheckMVS)
        {
            bool isAll4x4Same = isAll4x4RealSame1[sb];

            if (!allColocatedSame)
            {
                Ipp32s sbtype;
                switch(m_pRefPicList[1][0]->m_mbinfo.mbs[MBCol].mbtype)
                {
                case MBTYPE_INTER_8x8:
                case MBTYPE_INTER_8x8_REF0:
                    sbtype = m_pRefPicList[1][0]->m_mbinfo.mbs[MBCol].sbtype[sb];
                    isAll4x4Same = (sbtype == SBTYPE_8x8);
                    break;
                };
            }

            if (isAll4x4Same)
            {
                sbcolpartoffset = sbColPartOffset[sb];
                bool bUseZeroPredL0 = false;
                bool bUseZeroPredL1 = false;
                Ipp32s sbpartoffset = ((sb&2))*4+(sb&1)*2;

                VM_ASSERT(pRefMV);
                if (pRefMV[sbcolpartoffset].mvx >= -1 &&
                    pRefMV[sbcolpartoffset].mvx <= 1 &&
                    pRefMV[sbcolpartoffset].mvy >= -1 &&
                    pRefMV[sbcolpartoffset].mvy <= 1)
                {
                    // All subpart conditions for forcing zero pred are met,
                    // use final RefIndexLx==0 condition for L0,L1.
                    bUseZeroPredL0 = (RefIndexL0 == 0);
                    bUseZeroPredL1 = (RefIndexL1 == 0);
                    bAll4x4AreSame = 4;
                }

                storeStructInformationInto8x8<H264DecoderMotionVector>(&pFwdMV[sbpartoffset],
                    bUseZeroPredL0 ? zeroVector : mvL0);

                storeStructInformationInto8x8<H264DecoderMotionVector>(&pBwdMV[sbpartoffset],
                    bUseZeroPredL1 ? zeroVector : mvL1);
            }
            else
            {
                for (Ipp32u ypos = 0; ypos < 2; ypos++)
                {
                    for (Ipp32u xpos = 0; xpos < 2; xpos++)
                    {
                        sbcolpartoffset = m_IsUseDirect8x8Inference ? sbColPartOffset[sb] : sbColPartOffset[sb] + ypos*4 + xpos;
                        bool bUseZeroPredL0 = false;
                        bool bUseZeroPredL1 = false;
                        Ipp32s sbpartoffset = (ypos+(sb&2))*4+(sb&1)*2;

                        VM_ASSERT(pRefMV);
                        if (pRefMV[sbcolpartoffset].mvx >= -1 &&
                            pRefMV[sbcolpartoffset].mvx <= 1 &&
                            pRefMV[sbcolpartoffset].mvy >= -1 &&
                            pRefMV[sbcolpartoffset].mvy <= 1)
                        {
                            // All subpart conditions for forcing zero pred are met,
                            // use final RefIndexLx==0 condition for L0,L1.
                            bUseZeroPredL0 = (RefIndexL0 == 0);
                            bUseZeroPredL1 = (RefIndexL1 == 0);
                            bAll4x4AreSame ++;
                        }

                        pFwdMV[xpos + sbpartoffset] = bUseZeroPredL0 ? zeroVector : mvL0;
                        pBwdMV[xpos + sbpartoffset] = bUseZeroPredL1 ? zeroVector : mvL1;
                    }   // xpos
                }   // ypos // next row of subblocks
            }
        }
        else
        {
            storeStructInformationInto8x8<H264DecoderMotionVector>(&pFwdMV[subblock_block_mapping[sb]], mvL0);
            storeStructInformationInto8x8<H264DecoderMotionVector>(&pBwdMV[subblock_block_mapping[sb]], mvL1);
        }

        // set direction for 8x8 block
        m_cur_mb.LocalMacroblockInfo->sbdir[sb] = (Ipp8u)uPredDir;

        // set type for 8x8 block
        if (!bAll4x4AreSame || bAll4x4AreSame == 4)
            m_cur_mb.GlobalMacroblockInfo->sbtype[sb] = SBTYPE_8x8;
        else
            m_cur_mb.GlobalMacroblockInfo->sbtype[sb] = SBTYPE_4x4;

        bAll8x8AreSame += bAll4x4AreSame;
    }   // for sb

    // set mbtype to 8x8 if it was not; use larger type if possible
    if (bAll8x8AreSame && bAll8x8AreSame != 16)
    {
        m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER_8x8;

        if (allColocatedSame && isDirectMB &&
            (m_pRefPicList[1][0]->m_mbinfo.mbs[MBCol].mbtype == MBTYPE_INTER_16x8 ||
            m_pRefPicList[1][0]->m_mbinfo.mbs[MBCol].mbtype == MBTYPE_INTER_8x16))
        {
            m_cur_mb.GlobalMacroblockInfo->mbtype = m_pRefPicList[1][0]->m_mbinfo.mbs[MBCol].mbtype;
            m_cur_mb.LocalMacroblockInfo->sbdir[1] = m_cur_mb.LocalMacroblockInfo->sbdir[3];
        }
    }
} // void H264SegmentDecoderMultiThreaded::ReconstructDirectMotionVectorsSpatial(H264DecoderFrame **

void H264SegmentDecoderMultiThreaded::GetMVD4x4_CABAC(const Ipp8u *pBlkIdx,
                                                        const Ipp8u *pCodMVd,
                                                        Ipp32u ListNum)
{
    Ipp32u i, j, m = 0;

    // new
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[ListNum]->MotionVectors;

    for (i = 0; i < 16; i ++)
    {
        j = pBlkIdx[i];

        if ((m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x8 ||
             m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x8_REF0) &&
            !(i & 3))
            m = i;

        // new
        H264DecoderMotionVector mvd;

        switch (pCodMVd[j])
        {
        case CodNone:
            pMVd[j] = zeroVector;
            m++;
            break;

        case CodInBS:
            mvd = GetSE_MVD_CABAC(ListNum, j);
            pMVd[j] = mvd;
            break;

        case CodLeft:
            pMVd[j] = pMVd[j - 1];
            break;

        case CodAbov:
            pMVd[j]  = pMVd[j - 4];
            break;

        case CodSkip:
            pMVd[j] = zeroVector;
            break;
        }
    } // for i
} // void H264SegmentDecoderMultiThreaded::GetMVD4x4_CABAC(const Ipp8u *pBlkIdx,

void H264SegmentDecoderMultiThreaded::ReconstructMotionVectors4x4(const Ipp8u *pBlkIdx,
                                                                    const Ipp8u *pCodMVd,
                                                                    Ipp32u ListNum)
{
    Ipp32u i, j, m = 0;

    // new
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[ListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[ListNum]->MotionVectors;
    // old

    for (i = 0; i < 16; i ++)
    {
        j = pBlkIdx[i];

        if ((m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x8 ||
             m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x8_REF0) &&
            !(i & 3))
            m = i;

        // new
        H264DecoderMotionVector mv;
        H264DecoderMotionVector mvd;

        switch (pCodMVd[j])
        {
        case CodNone:
            pMV[j] = zeroVector;
            m++;
            break;

        case CodInBS:
            // TBD optimization - modify to compute predictors here instead of calling this method
            if (((m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[ListNum] >> j) & 1) == 0)
            {
                ComputeMotionVectorPredictors((Ipp8u) ListNum,
                                              m_cur_mb.GetReferenceIndex(ListNum, j),
                                              m++,
                                              &mv);
            } else {
                mv.mvx = pMV[j].mvx;
                mv.mvy = pMV[j].mvy;
                m++;
            }
            mvd = pMVd[j];
            // new & old
            mv.mvx = (Ipp16s) (mv.mvx + (Ipp32s) mvd.mvx);
            mv.mvy = (Ipp16s) (mv.mvy + (Ipp32s) mvd.mvy);

            if (mv.mvy > m_MVDistortion[ListNum])
                m_MVDistortion[ListNum] = mv.mvy;

            // new
            pMV[j] = mv;
            break;

        case CodLeft:
            pMV[j] = pMV[j - 1];
            break;

        case CodAbov:
            pMV[j]  = pMV[j - 4];
            break;

        case CodSkip:
            break;
        }
    } // for i
} // void H264SegmentDecoderMultiThreaded::ReconstructMotionVectors4x4(const Ipp8u *pBlkIdx,

void H264SegmentDecoderMultiThreaded::GetMVD4x4_CABAC(const Ipp8u pCodMVd,
                                                        Ipp32u ListNum)
{
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[ListNum]->MotionVectors;

#ifdef __ICL
    __assume_aligned(pMVd, 16);
#endif
    if(pCodMVd == CodNone)
    {
        memset(&pMVd[0], 0, 16*sizeof(H264DecoderMotionVector));
    }
    else
    {
        H264DecoderMotionVector mvd = GetSE_MVD_CABAC(ListNum, 0);

        for (Ipp32s k = 0; k < 16; k ++)
        {
            pMVd[k] = mvd;
        } // for k
    }
} // void H264SegmentDecoderMultiThreaded::GetMVD4x4_CABAC(const Ipp8u pCodMVd,

void H264SegmentDecoderMultiThreaded::ReconstructMotionVectors4x4(const Ipp8u pCodMVd,
                                                                    Ipp32u ListNum)
{
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[ListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[ListNum]->MotionVectors;
    RefIndexType *pRIx = m_cur_mb.GetReferenceIndexStruct(ListNum)->refIndexs;

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif
    if(pCodMVd == CodNone)
    {
        memset(&pMV[0],0,16*sizeof(H264DecoderMotionVector));
    }
    else
    {
        H264DecoderMotionVector mv;
        H264DecoderMotionVector *predPtr = m_cur_mb.MVs[ListNum]->MotionVectors;

        if (m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[ListNum] == 0)
            ComputeMotionVectorPredictors( (Ipp8u) ListNum,
                                            pRIx[0],
                                            0, &mv);
        else {
            mv.mvx = predPtr->mvx;
            mv.mvy = predPtr->mvy;
        }

        H264DecoderMotionVector mvd = pMVd[0];
        mv.mvx = (Ipp16s) (mv.mvx + (Ipp32s) mvd.mvx);
        mv.mvy = (Ipp16s) (mv.mvy + (Ipp32s) mvd.mvy);

        if (mv.mvy > m_MVDistortion[ListNum])
            m_MVDistortion[ListNum] = mv.mvy;

        for (Ipp32s k = 0; k < 16; k ++)
        {
            pMV[k] = mv;
        } // for k
    }
} // void H264SegmentDecoderMultiThreaded::ReconstructMotionVectors4x4(const Ipp8u pCodMVd,

void H264SegmentDecoderMultiThreaded::GetMVD4x4_16x8_CABAC(const Ipp8u *pCodMVd,
                                                             Ipp32u ListNum)
{
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[ListNum]->MotionVectors;

#ifdef __ICL
    __assume_aligned(pMVd, 16);
#endif
    if(pCodMVd[0] == CodNone)
    {
        memset(&pMVd[0],0,8*sizeof(H264DecoderMotionVector));
    }
    else
    {
        H264DecoderMotionVector mvd = GetSE_MVD_CABAC(ListNum, 0);

        for (Ipp32s k = 0; k < 8; k ++)
        {
            pMVd[k] = mvd;
        } // for k
    }

    if(pCodMVd[8] == CodNone)
    {
        memset(&pMVd[8],0,8*sizeof(H264DecoderMotionVector));
    }
    else
    {
        H264DecoderMotionVector mvd = GetSE_MVD_CABAC(ListNum, 8);

        for (Ipp32s k = 8; k < 16; k ++)
        {
            pMVd[k] = mvd;
        } // for k
    }
} // void H264SegmentDecoderMultiThreaded::GetMVD4x4_16x8_CABAC(const Ipp8u *pCodMVd,

void H264SegmentDecoderMultiThreaded::ReconstructMotionVectors16x8(const Ipp8u *pCodMVd,
                                                                     Ipp32u ListNum)
{
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[ListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[ListNum]->MotionVectors;
    RefIndexType *pRIx = m_cur_mb.GetReferenceIndexStruct(ListNum)->refIndexs;

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif
    if(pCodMVd[0] == CodNone)
    {
        memset(&pMV[0],0,8*sizeof(H264DecoderMotionVector));
    }
    else
    {
        H264DecoderMotionVector mv;
        H264DecoderMotionVector *predPtr = m_cur_mb.MVs[ListNum]->MotionVectors;

        if ((m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[ListNum] & 1) == 0) {
            ComputeMotionVectorPredictors( (Ipp8u) ListNum,
                                            pRIx[0],
                                            0, &mv);
        } else {
            mv.mvx = predPtr[0].mvx;
            mv.mvy = predPtr[0].mvy;
        }

        H264DecoderMotionVector mvd = pMVd[0];
        mv.mvx  = (Ipp16s) (mv.mvx + (Ipp32s) mvd.mvx);
        mv.mvy  = (Ipp16s) (mv.mvy + (Ipp32s) mvd.mvy);

        if (mv.mvy > m_MVDistortion[ListNum])
            m_MVDistortion[ListNum] = mv.mvy;

        for (Ipp32s k = 0; k < 8; k ++)
        {
            pMV[k] = mv;
        } // for k
    }

    if(pCodMVd[8] == CodNone)
    {
        memset(&pMV[8],0,8*sizeof(H264DecoderMotionVector));
    }
    else
    {
        H264DecoderMotionVector *predPtr = m_cur_mb.MVs[ListNum]->MotionVectors;
        H264DecoderMotionVector mv;
        if (((m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[ListNum] >> 8) & 1) == 0) {
            ComputeMotionVectorPredictors( (Ipp8u) ListNum,
                                            pRIx[2],
                                            1, &mv);
        } else {
            mv.mvx = predPtr[8].mvx;
            mv.mvy = predPtr[8].mvy;
        }

        H264DecoderMotionVector mvd = pMVd[8];
        mv.mvx  = (Ipp16s) (mv.mvx + (Ipp32s) mvd.mvx);
        mv.mvy  = (Ipp16s) (mv.mvy + (Ipp32s) mvd.mvy);

        if (mv.mvy > m_MVDistortion[ListNum])
            m_MVDistortion[ListNum] = mv.mvy;

        for (Ipp32s k = 8; k < 16; k ++)
        {
            pMV[k] = mv;
        } // for k
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs16x8(const Ipp8u *pCodMVd,

void H264SegmentDecoderMultiThreaded::GetMVD4x4_8x16_CABAC(const Ipp8u *pCodMVd,
                                                             Ipp32u ListNum)
{
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[ListNum]->MotionVectors;

#ifdef __ICL
    __assume_aligned(pMVd, 16);
#endif
    if(pCodMVd[0] == CodNone)
    {
        pMVd[0] = pMVd[1] = zeroVector;
    }
    else
    {
        H264DecoderMotionVector mvd = GetSE_MVD_CABAC(ListNum, 0);

        for (Ipp32s k = 0; k < 2; k ++)
        {
            pMVd[k] = mvd;
        } // for k
    }

    if(pCodMVd[2] == CodNone)
    {
        pMVd[3] = pMVd[2] = zeroVector;
    }
    else
    {
        H264DecoderMotionVector mvd = GetSE_MVD_CABAC(ListNum, 2);

        for (Ipp32s k = 2; k < 4; k ++)
        {
            pMVd[k] = mvd;
        } // for k
    }
    MFX_INTERNAL_CPY(&pMVd[4], &pMVd[0], 4*sizeof(H264DecoderMotionVector));
    MFX_INTERNAL_CPY(&pMVd[8], &pMVd[0], 4*sizeof(H264DecoderMotionVector));
    MFX_INTERNAL_CPY(&pMVd[12], &pMVd[0], 4*sizeof(H264DecoderMotionVector));

} // void H264SegmentDecoderMultiThreaded::GetMVD4x4_8x16_CABAC(const Ipp8u *pCodMVd,

void H264SegmentDecoderMultiThreaded::ReconstructMotionVectors8x16(const Ipp8u *pCodMVd,
                                                                     Ipp32u ListNum)
{
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[ListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[ListNum]->MotionVectors;
    RefIndexType *pRIx = m_cur_mb.GetReferenceIndexStruct(ListNum)->refIndexs;

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif
    if(pCodMVd[0] == CodNone)
    {
        pMV[0] = pMV[1] = zeroVector;
    }
    else
    {
        H264DecoderMotionVector mv;
        H264DecoderMotionVector *predPtr = m_cur_mb.MVs[ListNum]->MotionVectors;

        if (((m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[ListNum] >> 0) & 1) == 0) {
            ComputeMotionVectorPredictors( (Ipp8u) ListNum,
                                            pRIx[0],
                                            0, &mv);
        } else {
            mv.mvx = predPtr[0].mvx;
            mv.mvy = predPtr[0].mvy;
        }

        H264DecoderMotionVector mvd = pMVd[0];
        mv.mvx  = (Ipp16s) (mv.mvx + (Ipp32s) mvd.mvx);
        mv.mvy  = (Ipp16s) (mv.mvy + (Ipp32s) mvd.mvy);

        if (mv.mvy > m_MVDistortion[ListNum])
            m_MVDistortion[ListNum] = mv.mvy;

        for (Ipp32s k = 0; k < 2; k ++)
        {
            pMV[k] = mv;
        } // for k
    }

    if(pCodMVd[2] == CodNone)
    {
        pMV[2] = pMV[3] = zeroVector;
    }
    else
    {
        H264DecoderMotionVector mv;
        H264DecoderMotionVector *predPtr = m_cur_mb.MVs[ListNum]->MotionVectors;

        // get the prediction
        if (((m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[ListNum] >> 2) & 1) == 0) {
            ComputeMotionVectorPredictors( (Ipp8u) ListNum,
                                            pRIx[1],
                                            1, &mv);
        } else {
            mv.mvx = predPtr[2].mvx;
            mv.mvy = predPtr[2].mvy;
        }

        H264DecoderMotionVector mvd = pMVd[2];
        mv.mvx  = (Ipp16s) (mv.mvx + (Ipp32s) mvd.mvx);
        mv.mvy  = (Ipp16s) (mv.mvy + (Ipp32s) mvd.mvy);

        if (mv.mvy > m_MVDistortion[ListNum])
            m_MVDistortion[ListNum] = mv.mvy;

        for (Ipp32s k = 2; k < 4; k ++)
        {
            pMV[k] = mv;
        } // for k
    }
    MFX_INTERNAL_CPY(&pMV[4], &pMV[0], 4*sizeof(H264DecoderMotionVector));
    MFX_INTERNAL_CPY(&pMV[8], &pMV[0], 4*sizeof(H264DecoderMotionVector));
    MFX_INTERNAL_CPY(&pMV[12], &pMV[0], 4*sizeof(H264DecoderMotionVector));

} // void Status H264SegmentDecoderMultiThreaded::ReconstructMotionVectors8x16(const Ipp8u* pCodMVd,

void H264SegmentDecoderMultiThreaded::ReconstructSkipMotionVectors(void)
{
    // check "zero motion vectors" condition
    H264DecoderBlockLocation mbAddrA, mbAddrB;

    // select neighbours
    mbAddrA = m_cur_mb.CurrentBlockNeighbours.mbs_left[0];
    mbAddrB = m_cur_mb.CurrentBlockNeighbours.mb_above;

    if ((0 <= mbAddrA.mb_num) &&
        (0 <= mbAddrB.mb_num))
    {
        Ipp32s refIdxA, refIdxB;

        // select motion vectors & reference indexes
        H264DecoderMotionVector mvA = GetMV(m_gmbinfo, 0, mbAddrA.mb_num, mbAddrA.block_num);
        H264DecoderMotionVector mvB = GetMV(m_gmbinfo, 0, mbAddrB.mb_num, mbAddrB.block_num);
        refIdxA = GetReferenceIndex(m_gmbinfo, 0, mbAddrA.mb_num, mbAddrA.block_num);
        refIdxB = GetReferenceIndex(m_gmbinfo, 0, mbAddrB.mb_num, mbAddrB.block_num);

        // adjust vectors & reference in the MBAFF case
        if (m_isMBAFF)
        {
            Ipp32s iCurStruct = pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
            Ipp32s iNeighbourStruct;

            iNeighbourStruct = GetMBFieldDecodingFlag(m_gmbinfo->mbs[mbAddrA.mb_num]);
            mvA.mvy <<= iNeighbourStruct;
            refIdxA <<= (1 - iNeighbourStruct);

            iNeighbourStruct = GetMBFieldDecodingFlag(m_gmbinfo->mbs[mbAddrB.mb_num]);
            mvB.mvy <<= iNeighbourStruct;
            refIdxB <<= (1 - iNeighbourStruct);

            if (iCurStruct)
            {
                mvA.mvy /= 2;
                mvB.mvy /= 2;
            }
            else
            {
                refIdxA >>= 1;
                refIdxB >>= 1;
            }
        }

        // check condition
        if ((mvA.mvx | mvA.mvy | refIdxA) &&
            (mvB.mvx | mvB.mvy | refIdxB))
        {
            H264DecoderMotionVector mvPred;

            if (false == m_isMBAFF)
                ReconstructMVPredictorExternalBlock(0, m_cur_mb.CurrentBlockNeighbours.mb_above_right, &mvPred);
            else
                ReconstructMVPredictorExternalBlockMBAFF(0, m_cur_mb.CurrentBlockNeighbours.mb_above_right, &mvPred);

            if (mvPred.mvy > m_MVDistortion[0])
                m_MVDistortion[0] = mvPred.mvy;

            // fill motion vectors
            for (Ipp32s i = 0; i < 16; i += 1)
                m_cur_mb.MVs[0]->MotionVectors[i] = mvPred;

            return;
        }
    }

    // in oposite case we clean motion vectors
    memset((void *) m_cur_mb.MVs[0], 0, sizeof(H264DecoderMacroblockMVs));
} // void H264SegmentDecoderMultiThreaded::ReconstructSkipMotionVectors(void)

inline
Ipp32s CreateMVFromCode(Ipp32s code)
{
    Ipp32s val, sign;

    val = (code + 1) >> 1;
    sign = (code & 1) - 1;
    val ^= sign;
    val -= sign;

    return val;

} // Ipp32s CreateMVFromCode(Ipp32s code)

#define DecodeMVDelta_CAVLC(list_num, block_num) \
{ \
    H264DecoderMotionVector mvD; \
    mvD.mvx = (Ipp16s) CreateMVFromCode(m_pBitStream->GetVLCElement(false)); \
    mvD.mvy = (Ipp16s) CreateMVFromCode(m_pBitStream->GetVLCElement(false)); \
    m_cur_mb.MVDelta[list_num]->MotionVectors[block_num] = mvD; \
}

void H264SegmentDecoderMultiThreaded::DecodeMotionVectorsPSlice_CAVLC(void)
{
    m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[0] = 0;
    m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[1] = 0;

    if ((m_cur_mb.LocalMacroblockInfo->sbdir[0] > D_DIR_BIDIR)
        && (m_cur_mb.GlobalMacroblockInfo->mbtype - MBTYPE_FORWARD >= 0))
    {
        return;
    }

    Ipp32s mbtype = m_cur_mb.GlobalMacroblockInfo->mbtype;
    Ipp32s is_motion_pred_flag_exist = 0;
    Ipp8u  motion_pred_flag[4] = {0, 0, 0, 0};
    Ipp32s i;

    if (m_pSliceHeader->nal_ext.svc.no_inter_layer_pred_flag == 0 && inCropWindow(this->m_cur_mb))
    {
        motion_pred_flag[0] = m_pSliceHeader->default_motion_prediction_flag;
        motion_pred_flag[1] = m_pSliceHeader->default_motion_prediction_flag;
        motion_pred_flag[2] = m_pSliceHeader->default_motion_prediction_flag;
        motion_pred_flag[3] = m_pSliceHeader->default_motion_prediction_flag;

        if (m_pSliceHeader->adaptive_motion_prediction_flag)
        {
            is_motion_pred_flag_exist = 1;
            motion_pred_flag[0] = 0;
            motion_pred_flag[1] = 0;
            motion_pred_flag[2] = 0;
            motion_pred_flag[3] = 0;
        }
    }

    switch (mbtype)
    {
    case MBTYPE_FORWARD:
        {
            // decode reference indexes
            {
                H264DecoderMacroblockRefIdxs * refs = m_cur_mb.GetReferenceIndexStruct(0);
                Ipp32s refIdx = 0;

                if (is_motion_pred_flag_exist)
                {
                    motion_pred_flag[0] = (Ipp8u)m_pBitStream->Get1Bit();
                }

                if (!motion_pred_flag[0])
                {
                    Ipp32s num_ref_idx_l0_active = m_pSliceHeader->num_ref_idx_l0_active <<
                        pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);

                    if (2 == num_ref_idx_l0_active)

                        refIdx = m_pBitStream->Get1Bit() ^ 1;
                    else if (2 < num_ref_idx_l0_active)
                        refIdx = m_pBitStream->GetVLCElement(false);
                }
                else
                {
                    refIdx = refs->refIndexs[0];
                    m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[0] = 0xffff;// 1;
                }
                memset(refs->refIndexs, refIdx, sizeof(H264DecoderMacroblockRefIdxs));
            }

            // decode motion vector deltas
            DecodeMVDelta_CAVLC(0, 0);
        }
        break;

    case MBTYPE_INTER_16x8:
        {
            // decode reference indexes
            {
                H264DecoderMacroblockRefIdxs * refs = m_cur_mb.GetReferenceIndexStruct(0);
                Ipp32s num_ref_idx_l0_active = m_pSliceHeader->num_ref_idx_l0_active <<
                                               pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
                RefIndexType refIdx[2] = {0, 0};

                if (is_motion_pred_flag_exist)
                {
                    motion_pred_flag[0] = (Ipp8u)m_pBitStream->Get1Bit();
                    motion_pred_flag[1] = (Ipp8u)m_pBitStream->Get1Bit();
                }

                for (i = 0; i < 2; i++)
                {
                    if (!motion_pred_flag[i])
                    {
                        if (2 == num_ref_idx_l0_active)
                            refIdx[i] = (RefIndexType)(m_pBitStream->Get1Bit() ^ 1);
                        else if (2 < num_ref_idx_l0_active)
                            refIdx[i] = (RefIndexType)m_pBitStream->GetVLCElement(false);

                    }
                    else
                    {
                        refIdx[i] = refs->refIndexs[2*i];

                        m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[0] |=
                            block8x8_mask[2*i] | block8x8_mask[2*i+1];
                    }
                }
                refs->refIndexs[0] = refs->refIndexs[1] = refIdx[0];
                refs->refIndexs[2] = refs->refIndexs[3] = refIdx[1];
            }

            // decode motion vector deltas
            DecodeMVDelta_CAVLC(0, 0);
            DecodeMVDelta_CAVLC(0, 8);
        }
        break;

    case MBTYPE_INTER_8x16:
        {
            // decode reference indexes
            {
                H264DecoderMacroblockRefIdxs * refs = m_cur_mb.GetReferenceIndexStruct(0);
                Ipp32s num_ref_idx_l0_active = m_pSliceHeader->num_ref_idx_l0_active <<
                    pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
                RefIndexType refIdx[2] = {0, 0};

                if (is_motion_pred_flag_exist)
                {
                    motion_pred_flag[0] = (Ipp8u)m_pBitStream->Get1Bit();
                    motion_pred_flag[1] = (Ipp8u)m_pBitStream->Get1Bit();
                }

                for (i = 0; i < 2; i++)
                {
                    if (!motion_pred_flag[i])
                    {
                        if (2 == num_ref_idx_l0_active)
                            refIdx[i] = (RefIndexType)(m_pBitStream->Get1Bit() ^ 1);
                        else if (2 < num_ref_idx_l0_active)
                            refIdx[i] = (RefIndexType)m_pBitStream->GetVLCElement(false);

                    }
                    else
                    {
                        refIdx[i] = refs->refIndexs[i];

                        m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[0] |=
                            block8x8_mask[i] | block8x8_mask[i+2];
                    }
                }
                refs->refIndexs[0] = refs->refIndexs[2] = refIdx[0];
                refs->refIndexs[1] = refs->refIndexs[3] = refIdx[1];
            }

            // decode motion vector deltas
            DecodeMVDelta_CAVLC(0, 0);
            DecodeMVDelta_CAVLC(0, 2);
        }
        break;

        // 8x8 devision and smaller
    default:
        {
            Ipp8s *pSubBlockType = m_cur_mb.GlobalMacroblockInfo->sbtype;
            Ipp8s *pSubDirType = m_cur_mb.LocalMacroblockInfo->sbdir;

            if ((SBTYPE_8x8 == pSubBlockType[0]) &&
                (SBTYPE_8x8 == pSubBlockType[1]) &&
                (SBTYPE_8x8 == pSubBlockType[2]) &&
                (SBTYPE_8x8 == pSubBlockType[3]) &&

                (pSubDirType[0] < D_DIR_DIRECT) &&
                (pSubDirType[1] < D_DIR_DIRECT) &&
                (pSubDirType[2] < D_DIR_DIRECT) &&
                (pSubDirType[3] < D_DIR_DIRECT) )
            {
                H264DecoderMacroblockRefIdxs * refs = m_cur_mb.GetReferenceIndexStruct(0);

                // decode reference indexes
                {
                    Ipp32s num_ref_idx_l0_active = m_pSliceHeader->num_ref_idx_l0_active <<
                        pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);

                    if (is_motion_pred_flag_exist)
                    {
                        motion_pred_flag[0] = (Ipp8u)m_pBitStream->Get1Bit();
                        motion_pred_flag[1] = (Ipp8u)m_pBitStream->Get1Bit();
                        motion_pred_flag[2] = (Ipp8u)m_pBitStream->Get1Bit();
                        motion_pred_flag[3] = (Ipp8u)m_pBitStream->Get1Bit();
                    }

                    for (i = 0; i < 4; i++)
                    {
                        if (motion_pred_flag[i])
                        {
                            m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[0] |=
                                block8x8_mask[i];
                        }
                    }

                    if ((MBTYPE_INTER_8x8 == mbtype) &&
                        (1 < num_ref_idx_l0_active))
                    {
                        for (i = 0; i < 4; i++)
                        {
                            if (!motion_pred_flag[i])
                            {
                                if (2 == num_ref_idx_l0_active)
                                {
                                    refs->refIndexs[i] = (RefIndexType)(m_pBitStream->Get1Bit() ^ 1);
                                }
                                else
                                {
                                    refs->refIndexs[i] = (RefIndexType)m_pBitStream->GetVLCElement(false);
                                }
                            }
                        }
                    }
                    else if (mbtype == MBTYPE_INTER_8x8_REF0) {
                        for (i = 0; i < 4; i++)
                        {
                            if (!motion_pred_flag[i])
                            {
                                refs->refIndexs[i] = 0;
                            }
                        }
                    } else {
                        memset(refs, 0, sizeof(H264DecoderMacroblockRefIdxs));
                    }
                }

                // decode motion vector deltas
                DecodeMVDelta_CAVLC(0, 0);
                DecodeMVDelta_CAVLC(0, 2);
                DecodeMVDelta_CAVLC(0, 8);
                DecodeMVDelta_CAVLC(0, 10);

            }
            else
            {
                DecodeMotionVectors_CAVLC(false);
                return;
            }
        }
        break;
    };

    ReconstructMotionVectors();
    AdujstMvsAndType();
} // void H264SegmentDecoderMultiThreaded::DecodeMotionVectorsPSlice_CAVLC(void)

void H264SegmentDecoderMultiThreaded::DecodeMotionVectors_CAVLC(bool bIsBSlice)
{
    m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[0] = 0;
    m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[1] = 0;

    if ((m_cur_mb.LocalMacroblockInfo->sbdir[0] > D_DIR_BIDIR)
        && (m_cur_mb.GlobalMacroblockInfo->mbtype - MBTYPE_FORWARD >= 0))
    {
        return;
    }

    Ipp32s num_vectorsL0, block, subblock, i;
    Ipp32s mvdx_l0, mvdy_l0, mvdx_l1 = 0, mvdy_l1 = 0;
    Ipp32s num_vectorsL1;
    Ipp32s dec_vectorsL0;
    Ipp32s dec_vectorsL1;
    Ipp32s num_refIxL0;
    Ipp32s num_refIxL1;
    Ipp32s dec_refIxL0;
    Ipp32s dec_refIxL1;
    Ipp32u uVLCCodes[64+MAX_NUM_REF_FRAMES*2];  // sized for max possible:
                                                //  16 MV * 2 codes per MV * 2 directions
                                                //  plus L0 and L1 ref indexes
    Ipp32u *pVLCCodes;        // pointer to pass to VLC function
    Ipp32u *pVLCCodesL0MV;
    Ipp32u *pVLCCodesL1MV;
    Ipp32u *pVLCCodesL0RefIndex;
    Ipp32u *pVLCCodesL1RefIndex;

    Ipp32u dirPart;    // prediction direction of current MB partition
    Ipp32s numParts;
    Ipp32s partCtr;
    Ipp8s RefIxL0 = 0;
    Ipp8s RefIxL1 = 0;
    RefIndexType *pRefIndexL0 = 0;
    RefIndexType *pRefIndexL1 = 0;

    H264DecoderMotionVector *pMVDeltaL0 = 0;
    H264DecoderMotionVector *pMVDeltaL1 = 0;

    Ipp32s uNumRefIdxL0Active;
    Ipp32s uNumRefIdxL1Active;
    Ipp32s uNumVLCCodes;

    Ipp32s is_motion_pred_flag_exist = 0;
    Ipp8u  motion_pred_flag_l0[16];
    Ipp8u  motion_pred_flag_l1[16];

    memset(motion_pred_flag_l0, 0, 16);
    memset(motion_pred_flag_l1, 0, 16);

    if (m_pSliceHeader->nal_ext.svc.no_inter_layer_pred_flag == 0 && inCropWindow(this->m_cur_mb))
    {
        memset(motion_pred_flag_l0, m_pSliceHeader->default_motion_prediction_flag, 16);
        memset(motion_pred_flag_l1, m_pSliceHeader->default_motion_prediction_flag, 16);

        if (m_pSliceHeader->adaptive_motion_prediction_flag)
        {
            is_motion_pred_flag_exist = 1;
            memset(motion_pred_flag_l0, 0, 16);
            memset(motion_pred_flag_l1, 0, 16);
        }
    }

    switch (m_cur_mb.GlobalMacroblockInfo->mbtype)
    {
    case MBTYPE_FORWARD:
        num_vectorsL0 = 1;
        num_refIxL0 = 1;
        num_vectorsL1 = 0;
        num_refIxL1 = 0;
        dirPart = D_DIR_FWD;
        numParts = 1;
        break;
    case MBTYPE_BACKWARD:
        num_vectorsL0 = 0;
        num_refIxL0 = 0;
        num_vectorsL1 = 1;
        num_refIxL1 = 1;
        dirPart = D_DIR_BWD;
        numParts = 1;
        break;
    case MBTYPE_BIDIR:
        num_vectorsL0 = 1;
        num_refIxL0 = 1;
        num_vectorsL1 = 1;
        num_refIxL1 = 1;
        dirPart = D_DIR_BIDIR;
        numParts = 1;
        break;
    case MBTYPE_INTER_8x8:
    case MBTYPE_INTER_8x8_REF0:
        num_vectorsL0 = 0;
        num_refIxL0 = 0;
        num_vectorsL1 = 0;
        num_refIxL1 = 0;
        numParts = 0;
        for (block = 0; block<4; block++)
        {
            Ipp32s sbtype = (m_cur_mb.LocalMacroblockInfo->sbdir[block] < D_DIR_DIRECT) ?
                m_cur_mb.GlobalMacroblockInfo->sbtype[block] : SBTYPE_DIRECT;
            switch (sbtype)
            {
            case SBTYPE_8x8:
                if ((m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_FWD) ||
                    (m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_BIDIR))
                {
                    num_vectorsL0++;
                    num_refIxL0++;
                }
                if ((m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_BWD) ||
                    (m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_BIDIR))
                {
                    num_vectorsL1++;
                    num_refIxL1++;
                }
                numParts++;
                break;
            case SBTYPE_8x4:
            case SBTYPE_4x8:
                if ((m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_FWD) ||
                    (m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_BIDIR))
                {
                    num_vectorsL0 += 2;
                    num_refIxL0++;        // only one per refIx per 8x8
                }
                if ((m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_BWD) ||
                    (m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_BIDIR))
                {
                    num_vectorsL1 += 2;
                    num_refIxL1++;
                }
                numParts += 2;
                break;
            case SBTYPE_4x4:
                if ((m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_FWD) ||
                    (m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_BIDIR))
                {
                    num_vectorsL0 += 4;
                    num_refIxL0++;
                }
                if ((m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_BWD) ||
                    (m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_BIDIR))
                {
                    num_vectorsL1 += 4;
                    num_refIxL1++;
                }
                numParts += 4;
                break;
            case SBTYPE_DIRECT:
                numParts++;
                break;
            default:
                throw h264_exception(UMC_ERR_INVALID_STREAM);
                break;
            }
        }
        dirPart = m_cur_mb.LocalMacroblockInfo->sbdir[0];
        break;
    case MBTYPE_INTER_16x8:
    case MBTYPE_INTER_8x16:
        num_vectorsL0 = 0;
        num_refIxL0 = 0;
        num_vectorsL1 = 0;
        num_refIxL1 = 0;
        if ((m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_FWD) || (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BIDIR))
        {
            num_vectorsL0++;
            num_refIxL0++;
        }
        if ((m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BWD) || (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BIDIR))
        {
            num_vectorsL1++;
            num_refIxL1++;
        }
        if ((m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_FWD) || (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BIDIR))
        {
            num_vectorsL0++;
            num_refIxL0++;
        }
        if ((m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BWD) || (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BIDIR))
        {
            num_vectorsL1++;
            num_refIxL1++;
        }
        numParts = 2;
        dirPart = m_cur_mb.LocalMacroblockInfo->sbdir[0];
        break;
    default:
        VM_ASSERT(false);
        return; // DEBUG: may be throw ???
    }

    if (is_motion_pred_flag_exist)
    {
        for (i=0; i<num_refIxL0; i++)
        {
            motion_pred_flag_l0[i] = (Ipp8u)m_pBitStream->Get1Bit();
        }

        for (i=0; i<num_refIxL1; i++)
        {
            motion_pred_flag_l1[i] = (Ipp8u)m_pBitStream->Get1Bit();
        }
    }
    // Get all of the reference index and MV VLC codes from the bitstream
    // The bitstream contains them in the following order, which is the
    // order they are returned in uVLCCodes:
    //   L0 reference index for all MB parts using L0 prediction
    //   L1 reference index for all MB parts using L1 prediction
    //   L0 MV delta for all MB parts using L0 prediction
    //   L1 MV delta for all MB parts using L1 prediction
    // Reference index data is present only when the number of active
    // reference frames is greater than one. Also, MBTYPE_INTER_8x8_REF0
    // type MB has no ref index info in the bitstream (use 0 for all).
    uNumRefIdxL0Active = m_pSliceHeader->num_ref_idx_l0_active<<pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
    uNumRefIdxL1Active = m_pSliceHeader->num_ref_idx_l1_active<<pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
    if (uNumRefIdxL0Active == 1 || MBTYPE_INTER_8x8_REF0 == m_cur_mb.GlobalMacroblockInfo->mbtype)
        num_refIxL0 = 0;
    if (uNumRefIdxL1Active == 1 || MBTYPE_INTER_8x8_REF0 == m_cur_mb.GlobalMacroblockInfo->mbtype)
        num_refIxL1 = 0;

    // set pointers into VLC codes array
    pVLCCodes = uVLCCodes;
    pVLCCodesL0RefIndex = uVLCCodes;
    pVLCCodesL1RefIndex = pVLCCodesL0RefIndex + num_refIxL0;
    pVLCCodesL0MV = pVLCCodesL1RefIndex + num_refIxL1;
    pVLCCodesL1MV = pVLCCodesL0MV + num_vectorsL0*2;

    uNumVLCCodes = (num_vectorsL0+num_vectorsL1)*2 + num_refIxL0 + num_refIxL1;

    // When possible ref index range is 0..1, the reference index is coded
    // as 1 bit. When that occurs, get the ref index codes separate from
    // the motion vectors. Otherwise get all of the codes at once.
    if (uNumRefIdxL0Active >= 2 || uNumRefIdxL1Active >= 2)
    {
        // 1 bit codes for at least one set of ref index codes
        if (uNumRefIdxL0Active == 2)
        {
            for (i=0; i<num_refIxL0; i++)
            {
                if (!motion_pred_flag_l0[i])
                    pVLCCodesL0RefIndex[i] = !m_pBitStream->Get1Bit();
            }
        }
        else if (uNumRefIdxL0Active > 2)
        {
            for (i=0;i<num_refIxL0;i++)
            {
                if (!motion_pred_flag_l0[i])
                    pVLCCodesL0RefIndex[i]=m_pBitStream->GetVLCElement(false);
            }
        }

        if (uNumRefIdxL1Active == 2)
        {
            for (i=0; i<num_refIxL1; i++)
            {
                if (!motion_pred_flag_l1[i])
                    pVLCCodesL1RefIndex[i] = !m_pBitStream->Get1Bit();
            }
        }
        else if (uNumRefIdxL1Active > 2)
        {
            for (i=0;i<num_refIxL1;i++)
            {
                if (!motion_pred_flag_l1[i])
                    pVLCCodesL1RefIndex[i]=m_pBitStream->GetVLCElement(false);
            }
        }

        // to get the MV for the MB
        uNumVLCCodes -= num_refIxL0 + num_refIxL1;
        pVLCCodes = pVLCCodesL0MV;
    }

    // get all MV and possibly Ref Index codes for the MB
    for (i=0;i<uNumVLCCodes;i++,pVLCCodes++)
    {
        *pVLCCodes = m_pBitStream->GetVLCElement(false);
    }

    block = 0;
    subblock = 0;
    dec_vectorsL0 = 0;
    dec_vectorsL1 = 0;
    dec_refIxL0 = 0;
    dec_refIxL1 = 0;
    Ipp32u sboffset;
    for (partCtr = 0,sboffset=0; partCtr < numParts; partCtr++)
    {
        pMVDeltaL0 = &m_cur_mb.MVDelta[0]->MotionVectors[sboffset];
        pRefIndexL0 = &m_cur_mb.GetReferenceIndexStruct(0)->refIndexs[subblock_block_membership[sboffset]]; // DEBUG: ADB

        if (dirPart == D_DIR_FWD || dirPart == D_DIR_BIDIR)
        {    // L0

            if (motion_pred_flag_l0[dec_refIxL0])
                m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[0] |= (1 << sboffset);

            if (num_refIxL0)
            {
                // dec_refIxL0 is incremented in mbtype-specific code below
                // instead of here because there is only one refIx for each
                // 8x8 block, so number of refIx values is not the same as
                // numParts.
                if (motion_pred_flag_l0[dec_refIxL0])
                {
                    RefIxL0 = pRefIndexL0[0];
                }
                else
                {
                    RefIxL0 = (Ipp8s)pVLCCodesL0RefIndex[dec_refIxL0];

                    if (RefIxL0 >= (Ipp8s)uNumRefIdxL0Active || RefIxL0 < 0)
                    {
                        throw h264_exception(UMC_ERR_INVALID_STREAM);
                    }
                }
            }
            else
            {
                if (motion_pred_flag_l0[dec_refIxL0] &&
                    (MBTYPE_INTER_8x8_REF0 == m_cur_mb.GlobalMacroblockInfo->mbtype/* || uNumRefIdxL0Active == 0*/))
                {
                    RefIxL0 = pRefIndexL0[0];
                } else {
                    RefIxL0 = 0;
                }
            }

            mvdx_l0 = (pVLCCodesL0MV[dec_vectorsL0*2]+1) >> 1;
            if (!(pVLCCodesL0MV[dec_vectorsL0*2]&1))
                mvdx_l0 = -mvdx_l0;

            mvdy_l0 = (pVLCCodesL0MV[dec_vectorsL0*2+1]+1) >> 1;
            if (!(pVLCCodesL0MV[dec_vectorsL0*2+1]&1))
                mvdy_l0 = -mvdy_l0;
            dec_vectorsL0++;
        }    // L0
        else
        {
            mvdx_l0 = mvdy_l0 = 0;
            RefIxL0 = -1;
        }

        if (bIsBSlice)
        {
            pMVDeltaL1 = &m_cur_mb.MVDelta[1]->MotionVectors[sboffset];
            pRefIndexL1 = &m_cur_mb.GetReferenceIndexStruct(1)->refIndexs[subblock_block_membership[sboffset]];  // DEBUG: ADB

            if (dirPart == D_DIR_BWD || dirPart == D_DIR_BIDIR)
            {
                if (motion_pred_flag_l1[dec_refIxL1])
                    m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[1] |= (1 << sboffset);

                if (num_refIxL1)
                {
                    RefIxL1 = (Ipp8s)pVLCCodesL1RefIndex[dec_refIxL1];
                    // dec_refIxL1 is incremented in mbtype-specific code below
                    // instead of here because there is only one refIx for each
                    // 8x8 block, so number of refIx values is not the same as
                    // numParts.
                    if (motion_pred_flag_l1[dec_refIxL1])
                    {
                        RefIxL1 = pRefIndexL1[0];
                    }
                    else
                    {
                        RefIxL1 = (Ipp8s)pVLCCodesL1RefIndex[dec_refIxL1];

                        if (RefIxL1 >= (Ipp8s)uNumRefIdxL1Active || RefIxL1 < 0)
                        {
                            // something is wrong
                        }
                    }
                }
                else
                {
                    if (motion_pred_flag_l0[dec_refIxL1] &&
                        (MBTYPE_INTER_8x8_REF0 == m_cur_mb.GlobalMacroblockInfo->mbtype/* || uNumRefIdxL1Active == 0*/))
                    {
                        RefIxL1 = pRefIndexL1[0];
                    } else {
                        RefIxL1 = 0;
                    }
                }

                mvdx_l1 = (pVLCCodesL1MV[dec_vectorsL1*2]+1)>>1;
                if (!(pVLCCodesL1MV[dec_vectorsL1*2]&1))
                    mvdx_l1 = -mvdx_l1;

                mvdy_l1 = (pVLCCodesL1MV[dec_vectorsL1*2+1]+1)>>1;
                if (!(pVLCCodesL1MV[dec_vectorsL1*2+1]&1))
                    mvdy_l1 = -mvdy_l1;
                dec_vectorsL1++;
            }    // L1
            else
            {
                mvdx_l1 = mvdy_l1 = 0;
                RefIxL1 = -1;
            }
        }    // B slice

        // Store motion vectors and reference indexes into this frame's buffers
        switch (m_cur_mb.GlobalMacroblockInfo->mbtype)
        {
        case MBTYPE_FORWARD:
        case MBTYPE_BACKWARD:
        case MBTYPE_BIDIR:

            pMVDeltaL0[0].mvx = (Ipp16s)mvdx_l0;
            pMVDeltaL0[0].mvy = (Ipp16s)mvdy_l0;

            fill_n(pRefIndexL0, 4, RefIxL0);

            if (bIsBSlice)
            {
                pMVDeltaL1[0].mvx = (Ipp16s)mvdx_l1;
                pMVDeltaL1[0].mvy = (Ipp16s)mvdy_l1;

                fill_n(pRefIndexL1, 4, RefIxL1);
            }
            break;
        case MBTYPE_INTER_16x8:
            pMVDeltaL0[0].mvx = (Ipp16s)mvdx_l0;
            pMVDeltaL0[0].mvy = (Ipp16s)mvdy_l0;

            // store in 8 top or bottom half blocks
            fill_n(pRefIndexL0, 2, RefIxL0);

            if (bIsBSlice)
            {
                pMVDeltaL1[0].mvx = (Ipp16s)mvdx_l1;
                pMVDeltaL1[0].mvy = (Ipp16s)mvdy_l1;

                fill_n(pRefIndexL1, 2, RefIxL1);

                if (dirPart == D_DIR_BWD || dirPart == D_DIR_BIDIR)
                    dec_refIxL1++;
            }
            block++;
            sboffset=8;
            if (dirPart == D_DIR_FWD || dirPart == D_DIR_BIDIR)
                dec_refIxL0++;
            dirPart = m_cur_mb.LocalMacroblockInfo->sbdir[block];        // next partition
            break;
        case MBTYPE_INTER_8x16:
            pMVDeltaL0[0].mvx = (Ipp16s)mvdx_l0;
            pMVDeltaL0[0].mvy = (Ipp16s)mvdy_l0;

            // store in 8 left or right half blocks
            pRefIndexL0[0] = RefIxL0;
            pRefIndexL0[2] = RefIxL0;

            if (bIsBSlice)
            {
                pMVDeltaL1[0].mvx = (Ipp16s)mvdx_l1;
                pMVDeltaL1[0].mvy = (Ipp16s)mvdy_l1;

                // store in 8 left or right half blocks
                pRefIndexL1[0] = RefIxL1;
                pRefIndexL1[2] = RefIxL1;

                if (dirPart == D_DIR_BWD || dirPart == D_DIR_BIDIR)
                    dec_refIxL1++;
            }
            block++;
            sboffset=2;
            if (dirPart == D_DIR_FWD || dirPart == D_DIR_BIDIR)
                dec_refIxL0++;
            dirPart = m_cur_mb.LocalMacroblockInfo->sbdir[block];        // next partition
            break;
        case MBTYPE_INTER_8x8:
        case MBTYPE_INTER_8x8_REF0:
            {
                Ipp32s sbtype = (m_cur_mb.LocalMacroblockInfo->sbdir[block>>2] < D_DIR_DIRECT) ?
                    m_cur_mb.GlobalMacroblockInfo->sbtype[block>>2] : SBTYPE_DIRECT;

            switch (sbtype)
            {
            case SBTYPE_8x8:
                pMVDeltaL0[0].mvx = (Ipp16s)mvdx_l0;
                pMVDeltaL0[0].mvy = (Ipp16s)mvdy_l0;

                pRefIndexL0[0] = RefIxL0;

                if (bIsBSlice)
                {
                    pMVDeltaL1[0].mvx = (Ipp16s)mvdx_l1;
                    pMVDeltaL1[0].mvy = (Ipp16s)mvdy_l1;

                    pRefIndexL1[0] = RefIxL1;
                    if (dirPart == D_DIR_BWD || dirPart == D_DIR_BIDIR)
                        dec_refIxL1++;
                }
                if (block == 4)
                {
                    sboffset += 8 - 2;
                }
                else
                {
                    sboffset += 2;
                }
                block += 4;
                if (dirPart == D_DIR_FWD || dirPart == D_DIR_BIDIR)
                    dec_refIxL0++;
                break;
            case SBTYPE_8x4:
                pMVDeltaL0[0].mvx = (Ipp16s)mvdx_l0;
                pMVDeltaL0[0].mvy = (Ipp16s)mvdy_l0;

                pRefIndexL0[0] = RefIxL0;
                if (bIsBSlice)
                {
                    pMVDeltaL1[0].mvx = (Ipp16s)mvdx_l1;
                    pMVDeltaL1[0].mvy = (Ipp16s)mvdy_l1;

                    pRefIndexL1[0] = RefIxL1;
                }

                if (subblock == 1)
                {
                    if (block == 4)
                    {
                        sboffset+=2;
                    }
                    else
                    {
                        sboffset-=2;
                    }
                    block += 4;
                    subblock = 0;
                    if (dirPart == D_DIR_FWD || dirPart == D_DIR_BIDIR)
                        dec_refIxL0++;
                    if (dirPart == D_DIR_BWD || dirPart == D_DIR_BIDIR)
                        dec_refIxL1++;
                }
                else
                {
                    subblock++;
                    sboffset  += 4;
                }
                break;
            case SBTYPE_4x8:
                pMVDeltaL0[0].mvx = (Ipp16s)mvdx_l0;
                pMVDeltaL0[0].mvy = (Ipp16s)mvdy_l0;

                pRefIndexL0[0] = RefIxL0;
                if (bIsBSlice)
                {
                    pMVDeltaL1[0].mvx = (Ipp16s)mvdx_l1;
                    pMVDeltaL1[0].mvy = (Ipp16s)mvdy_l1;

                    pRefIndexL1[0] = RefIxL1;
                }
                if (subblock == 1)
                {
                    if (block == 4)
                    {
                        sboffset += 8 - 3;
                    }
                    else
                    {
                        sboffset++;
                    }
                    block += 4;
                    subblock = 0;
                    if (dirPart == D_DIR_FWD || dirPart == D_DIR_BIDIR)
                        dec_refIxL0++;
                    if (dirPart == D_DIR_BWD || dirPart == D_DIR_BIDIR)
                        dec_refIxL1++;
                }
                else
                {
                    subblock++;
                    sboffset++;
                }
                break;
            case SBTYPE_4x4:
                pMVDeltaL0[0].mvx = (Ipp16s)mvdx_l0;
                pMVDeltaL0[0].mvy = (Ipp16s)mvdy_l0;

                pRefIndexL0[0] = RefIxL0;
                if (bIsBSlice)
                {
                    pMVDeltaL1[0].mvx = (Ipp16s)mvdx_l1;
                    pMVDeltaL1[0].mvy = (Ipp16s)mvdy_l1;

                    pRefIndexL1[0] = RefIxL1;
                }
                sboffset += (xyoff[block+subblock][0]>>2) +
                    ((xyoff[block+subblock][1])>>2)*4;
                if (subblock == 3)
                {
                    block += 4;
                    subblock = 0;
                    if (dirPart == D_DIR_FWD || dirPart == D_DIR_BIDIR)
                        dec_refIxL0++;
                    if (dirPart == D_DIR_BWD || dirPart == D_DIR_BIDIR)
                        dec_refIxL1++;
                }
                else
                {
                    subblock++;
                }
                break;
            case SBTYPE_DIRECT:
                // nothing to do except advance to next 8x8 partition
                if (block == 4)
                {
                    sboffset += 8 - 2;
                }
                else
                {
                    sboffset += 2;
                }
                block += 4;
                break;
            }    // 8x8 switch sbtype
            }

            if (block<16)
                dirPart = m_cur_mb.LocalMacroblockInfo->sbdir[block>>2];        // next partition
            break;
        }    // switch mbtype
    }    // for partCtr

    if (m_pSliceHeader->nal_unit_type == NAL_UT_CODED_SLICE_EXTENSION && m_pSliceHeader->nal_ext.svc_extension_flag &&
        (m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x8 ||
        m_cur_mb.GlobalMacroblockInfo->mbtype ==  MBTYPE_INTER_8x8_REF0)) {
        for (i = 0; i < 4; i ++)
        {
            RefIndexType RefIndexL0, RefIndexL1;
            Ipp32s uPredDir;
            if (m_cur_mb.LocalMacroblockInfo->sbdir[i] <= D_DIR_BIDIR)
                continue;
            ComputeDirectSpatialRefIdx8x8(&RefIndexL0, &RefIndexL1, i);

            if ((RefIndexL0 < 0) && (RefIndexL1 < 0))
                RefIndexL0 = RefIndexL1 = 0;

            if ((0 <= RefIndexL0) && (0 > RefIndexL1))
                uPredDir = D_DIR_DIRECT_SPATIAL_FWD;
            else if ((0 > RefIndexL0) && (0 <= RefIndexL1))
                uPredDir = D_DIR_DIRECT_SPATIAL_BWD;
            else
                uPredDir = D_DIR_DIRECT_SPATIAL_BIDIR;

            m_cur_mb.GetReferenceIndexStruct(0)->refIndexs[i] = RefIndexL0;
            m_cur_mb.GetReferenceIndexStruct(1)->refIndexs[i] = RefIndexL1;

            m_cur_mb.LocalMacroblockInfo->sbdir[i]=(Ipp8s)uPredDir;
            m_cur_mb.GlobalMacroblockInfo->sbtype[i]=SBTYPE_8x8;
        }   // for sb
    }


    Ipp32s sbdir_store[4];
    if (m_pSliceHeader->nal_unit_type == NAL_UT_CODED_SLICE_EXTENSION && m_pSliceHeader->nal_ext.svc_extension_flag)
    {
        for (Ipp32s block = 0; block < 4; block++)
        {
            sbdir_store[block] = m_cur_mb.LocalMacroblockInfo->sbdir[block];
            if      (m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_DIRECT_SPATIAL_FWD)
                m_cur_mb.LocalMacroblockInfo->sbdir[block] = D_DIR_FWD;
            else if (m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_DIRECT_SPATIAL_BWD)
                m_cur_mb.LocalMacroblockInfo->sbdir[block] = D_DIR_BWD;
            else if (m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_DIRECT_SPATIAL_BIDIR)
                m_cur_mb.LocalMacroblockInfo->sbdir[block] = D_DIR_BIDIR;
        }
    }

    ReconstructMotionVectors();
    if (m_pSliceHeader->nal_unit_type == NAL_UT_CODED_SLICE_EXTENSION && m_pSliceHeader->nal_ext.svc_extension_flag)
    {
        for (Ipp32s block = 0; block < 4; block++)
        {
            m_cur_mb.LocalMacroblockInfo->sbdir[block] = (Ipp8s)sbdir_store[block];
        }
    }
    return;
}  // void H264SegmentDecoderMultiThreaded::DecodeMotionVectors_CAVLC(bool bIsBSlice)

void H264SegmentDecoderMultiThreaded::DecodeDirectMotionVectors(bool isDirectMB)
{
    if (m_pSliceHeader->nal_unit_type == NAL_UT_CODED_SLICE_EXTENSION && m_pSliceHeader->nal_ext.svc_extension_flag)
    {
        Ipp32s RefIndexL0, RefIndexL1;

        if ((m_cur_mb.GlobalMacroblockInfo->mbtype != MBTYPE_INTER_8x8)/* &&
                                                                       (m_cur_mb.GlobalMacroblockInfo->mbtype != MBTYPE_DIRECT)*/)
        {

            // find the reference indexes
            ComputeDirectSpatialRefIdx(&RefIndexL0, &RefIndexL1);

            if ((RefIndexL0 < 0) && (RefIndexL1 < 0))
                RefIndexL0 = RefIndexL1 = 0;
        }
        else
        {
            /* ???????????????? */
            RefIndexL0 = RefIndexL1 = 0;
        }

        //        RefIndexL0 = RefIndexL1 = 0;

        if (m_cur_mb.GlobalMacroblockInfo->mbtype != MBTYPE_INTER_8x8)
        {
            // set up reference index array
            memset(m_cur_mb.RefIdxs[0]->refIndexs, RefIndexL0, 4);
            memset(m_cur_mb.RefIdxs[1]->refIndexs, RefIndexL1, 4);

            // set direction for MB
            if ((0 <= RefIndexL0) && (0 > RefIndexL1))
                m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_FORWARD;
            else if ((0 > RefIndexL0) && (0 <= RefIndexL1))
                m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_BACKWARD;
            else
                m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_BIDIR;

            m_cur_mb.MVDelta[0]->MotionVectors[0].mvx = 0;
            m_cur_mb.MVDelta[0]->MotionVectors[0].mvy = 0;
            m_cur_mb.MVDelta[1]->MotionVectors[0].mvx = 0;
            m_cur_mb.MVDelta[1]->MotionVectors[0].mvy = 0;

            m_cur_mb.LocalMacroblockInfo->sbdir[0] = 0;
            m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[0] = 0;
            m_cur_mb.LocalMacroblockInfo->m_mv_prediction_flag[1] = 0;

            ReconstructMotionVectors();
        }
        else
        {
            bool isAll8x8Same = true;
            Ipp32s sb, sboffset;
            Ipp32u uPredDir;

            for (sb = 0; sb<4; sb++)
            {
                if (m_cur_mb.GlobalMacroblockInfo->sbtype[sb] != SBTYPE_DIRECT)
                {
                    isAll8x8Same = false;
                    continue;
                }
                uPredDir = D_DIR_DIRECT_SPATIAL_BIDIR;

                sboffset = 0;
                switch (sb)
                {
                case 0:
                    sboffset = 0;
                    break;
                case 1:
                    sboffset = 2;
                    break;
                case 2:
                    sboffset = 8;
                    break;
                case 3:
                    sboffset = 2 + 8;
                    break;
                }

                m_cur_mb.MVDelta[0]->MotionVectors[sboffset].mvx = 0;
                m_cur_mb.MVDelta[0]->MotionVectors[sboffset].mvy = 0;
                m_cur_mb.MVDelta[1]->MotionVectors[sboffset].mvx = 0;
                m_cur_mb.MVDelta[1]->MotionVectors[sboffset].mvy = 0;

                m_cur_mb.RefIdxs[0]->refIndexs[sb] = (RefIndexType)RefIndexL0;
                m_cur_mb.RefIdxs[1]->refIndexs[sb] = (RefIndexType)RefIndexL1;

                m_cur_mb.LocalMacroblockInfo->sbdir[sb]=(Ipp8s)uPredDir;
            }   // for sb
        }
    }
    else
    {
        if (m_IsUseSpatialDirectMode)
        {
            ReconstructDirectMotionVectorsSpatial(isDirectMB);
        }   // temporal DIRECT
        else
        {
            // temporal DIRECT prediction
            if (!m_IsUseDirect8x8Inference)
            {
                DecodeDirectMotionVectorsTemporal(isDirectMB);
            }
            else
            {
                DecodeDirectMotionVectorsTemporal_8x8Inference();
            }
        }
    }
}

Status H264SegmentDecoderMultiThreaded::ProcessLayerPre()
{
    /* This is temporal solution for 1 thread mode. For AVC layer SetMBMap was already called */
    if (m_pSlice->m_bIsFirstSliceInLayer &&
        ((m_pSlice->m_iNumber - 1) > m_pSlice->GetCurrentFrame()->m_lastSliceInAVC))
    {
        m_pTaskBroker->m_pTaskSupplier->SetMBMap(m_pSlice, m_pCurrentFrame, m_pTaskBroker->GetLocalResource());
        m_mbinfo.active_next_mb_table = m_pSlice->GetMBInfo().active_next_mb_table;
    }

    CalculateResizeParameters();
    SVCDeblocking();
    LoadMBInfoFromRefLayer();

    if (m_isSliceGroups)
    {
        if (m_pSlice->m_bIsFirstSliceInLayer && (m_pSlice->m_iNumber - 1 > m_pCurrentFrame->m_lastSliceInAVC))
        {
            Ipp32s nMBCount = m_pCurrentFrame->totalMBs;
            for (Ipp32s i = 0; i < nMBCount; i++)
            {
                m_gmbinfo->mbs[i].slice_id = -1;
            }
        }
    }

    return UMC_OK;
}

void H264SegmentDecoderMultiThreaded::SVCDeblocking()
{
    Ipp32s iMBToProcess;
    Ipp32s m_spatial_resolution_change_save = m_spatial_resolution_change;

    H264Slice *pSlice = m_pSlice;
    H264DecoderFrame *pFrame = pSlice->GetCurrentFrame();
    H264SliceHeader *pSliceHeader_save = pSlice->GetSliceHeader();
    H264SliceHeader *pSliceHeader = pSlice->m_firstILPredSliceInDepLayer->GetSliceHeader();

    ReferenceFlags *(pFields[2]);
    H264DecoderFrame **(pRefPicList[2]);

    if (DEBLOCK_FILTER_OFF == pSliceHeader->disable_inter_layer_deblocking_filter_idc)
        return;

    if (pSlice->m_bIsFirstSliceInDepLayer && (m_currentLayer > 0) &&
        (pSliceHeader->ref_layer_dq_id >= 0) && m_spatial_resolution_change)
    {
        H264DecoderLayerDescriptor *layerMb, *curlayerMb;

        Ipp32s refLayer = pSliceHeader->ref_layer_dq_id >> 4;
        VM_ASSERT(refLayer < m_currentLayer);

        layerMb = &m_mbinfo.layerMbs[refLayer];
        curlayerMb = &m_mbinfo.layerMbs[m_currentLayer];

        iMBToProcess = pFrame->m_pLayerFrames[refLayer]->totalMBs;

        deblocking_IL = 1;
        deblocking_stage2 = 0;
        m_spatial_resolution_change = (pSliceHeader->ref_layer_dq_id & 15) ? 0 : 
            layerMb->m_pResizeParameter->spatial_resolution_change;
        m_pSlice = pSlice->m_firstILPredSliceInDepLayer;
        m_pSliceHeader = pSliceHeader;
        m_isMBAFF = 0 != m_mbinfo.layerMbs[m_pSliceHeader->ref_layer_dq_id >> 4].is_MbAff;

        pFields[0] = m_pFields[0];
        pFields[1] = m_pFields[1];
        pRefPicList[0] = m_pRefPicList[0];
        pRefPicList[1] = m_pRefPicList[1];

        H264DecoderFrame *pCurrentFrame = m_pSlice->GetCurrentFrame()->m_pLayerFrames[refLayer];
        m_gmbinfo = &(pCurrentFrame->m_mbinfo);
        iMBToProcess = pCurrentFrame->totalMBs;
        m_pCurrentFrame = pCurrentFrame;
        mb_width = m_pCurrentFrame->lumaSize().width >> 4;

        if (m_pSliceHeader->ref_layer_dq_id & 0x0f) // quality_id
            m_gmbinfo = &(m_pSlice->GetCurrentFrame()->m_mbinfo);

        Ipp32s firstMB = pCurrentFrame->GetNumberByParity(0) == 0 ? 0 : pCurrentFrame->totalMBs;
        m_pPicParamSet = m_pSlice->GetCurrentFrame()->GetAU(m_pSlice->m_pCurrentFrame->GetNumberByParity(m_field_index))->GetSliceByNumber(m_gmbinfo->mbs[firstMB].slice_id)->GetPicParam();
        is_profile_baseline_scalable = m_mbinfo.layerMbs[m_pSliceHeader->ref_layer_dq_id >> 4].is_profile_baseline_scalable;

        DeblockSegmentTask(firstMB, iMBToProcess);

        if ((DEBLOCK_FILTER_ON_2_PASS ==
            m_pSliceHeader->disable_inter_layer_deblocking_filter_idc_from_stream) ||
            (DEBLOCK_FILTER_ON_2_PASS_NO_CHROMA ==
            m_pSliceHeader->disable_inter_layer_deblocking_filter_idc_from_stream))
        {
            deblocking_stage2 = 1;
            DeblockSegmentTask(0, iMBToProcess);
        }

        m_pFields[0] = pFields[0];
        m_pFields[1] = pFields[1];
        m_pRefPicList[0] = pRefPicList[0];
        m_pRefPicList[1] = pRefPicList[1];

        m_pSlice = pSlice;
        m_pSliceHeader = pSliceHeader_save;
        deblocking_IL = 0;
        deblocking_stage2 = 0;
        m_spatial_resolution_change = m_spatial_resolution_change_save;
        refLayer = m_pSliceHeader->nal_ext.svc.dependency_id;
        m_pCurrentFrame = m_pSlice->GetCurrentFrame()->m_pLayerFrames[refLayer];
        m_gmbinfo = &(m_pCurrentFrame->m_mbinfo);
        mb_width = m_pSlice->GetMBWidth();
        m_pPicParamSet = m_pSlice->GetPicParam();
        m_isMBAFF = 0 != m_pSliceHeader->MbaffFrameFlag;
        is_profile_baseline_scalable = (m_pSlice->m_pSeqParamSet->profile_idc == H264VideoDecoderParams::H264_PROFILE_SCALABLE_BASELINE);
    }
}

void H264SegmentDecoderMultiThreaded::CopyMacroblockInfoFromBaseLayer(H264DecoderLayerDescriptor *layerMb, Ipp32s mbAddr, Ipp32s refMbAddr)
{
    static const Ipp32u num_coeffs[4] = {256,384,512,768};
    Ipp32s numCoeff = num_coeffs[m_pCurrentFrame->m_chroma_format];

    if (layerMb->mbs[refMbAddr].mbtype < MBTYPE_PCM)
    {
        IntraType *pMBIntraTypes = m_pMBIntraTypes + mbAddr*NUM_INTRA_TYPE_ELEMENTS;
        IntraType *pRefMBIntraTypes = m_pMBIntraTypes + refMbAddr*NUM_INTRA_TYPE_ELEMENTS;

        for (Ipp32u k = 0; k < NUM_INTRA_TYPE_ELEMENTS; k++)
        {
            pMBIntraTypes[k] = pRefMBIntraTypes[k];
        }
    }

    Ipp16s *pCoeffPtr = m_mbinfo.SavedMacroblockTCoeffs + mbAddr*COEFFICIENTS_BUFFER_SIZE;
    Ipp16s *pRefCoeffPtr = m_mbinfo.SavedMacroblockTCoeffs + refMbAddr*COEFFICIENTS_BUFFER_SIZE;

    for (Ipp32s k = 0; k < numCoeff; k++)
    {
        pCoeffPtr[k] = pRefCoeffPtr[k];
    }

    pCoeffPtr = m_mbinfo.SavedMacroblockCoeffs + mbAddr*COEFFICIENTS_BUFFER_SIZE;
    pRefCoeffPtr = m_mbinfo.SavedMacroblockCoeffs + refMbAddr*COEFFICIENTS_BUFFER_SIZE;

    for (Ipp32s k = 0; k < numCoeff; k++)
    {
        pCoeffPtr[k] = pRefCoeffPtr[k];
    }

    m_gmbinfo->mbs[mbAddr].mbflags = m_gmbinfo->mbs[refMbAddr].mbflags;
    m_mbinfo.mbs[mbAddr].QP = m_mbinfo.mbs[refMbAddr].QP;
}

void H264SegmentDecoderMultiThreaded::LoadMBInfoFromRefLayer()
{
    H264Slice *pSlice = m_pSlice;
    H264DecoderFrame *pFrame = pSlice->GetCurrentFrame();
    H264SliceHeader *pSliceHeader = pSlice->m_firstILPredSliceInDepLayer->GetSliceHeader();
    Ipp32s pitch_luma = pFrame->pitch_luma();
    Ipp32s pitch_chroma = pFrame->pitch_chroma();
    Ipp32s pixel_sz = (pFrame->m_bpp > 8) + 1;
    Ipp32s chromaShift = (3 > pFrame->m_chroma_format) ? 1 : 0;
    H264DecoderLayerDescriptor *layerMb, *curlayerMb;
    const H264SeqParamSet *curSps = pSlice->GetSeqParam();
    Ipp32s nv12 = (pFrame->GetColorFormat() == NV12);

    if (pSlice->m_bIsFirstSliceInDepLayer && (m_currentLayer > 0) &&
       (pSliceHeader->ref_layer_dq_id >= 0))
    {
        Ipp32s refLayer = pSliceHeader->ref_layer_dq_id >> 4;
        H264DecoderFrame *pRefFrame = pSlice->GetCurrentFrame()->m_pLayerFrames[refLayer];
        Ipp32s refWidth = pRefFrame->lumaSize().width;
        Ipp32s refHeight = pRefFrame->lumaSize().height;
        Ipp32s refChromaWidth = refWidth >> ((Ipp32s) (3 > pFrame->m_chroma_format));

        H264DecoderGlobalMacroblocksDescriptor *RefGlobalMBinfo = &(pRefFrame->m_mbinfo);
        if (pSliceHeader->ref_layer_dq_id & 0x0f) // quality_id
            RefGlobalMBinfo = &(pSlice->GetCurrentFrame()->m_mbinfo);

        if (refLayer == m_currentLayer)
            return;

        if (!m_spatial_resolution_change && !pSlice->GetSliceHeader()->nal_ext.svc.quality_id)
        {
            size_t nMBCount = m_pCurrentFrame->totalMBs;
            MFX_INTERNAL_CPY(m_pCurrentFrame->m_mbinfo.mbs, pRefFrame->m_mbinfo.mbs, (Ipp32u)(sizeof(H264DecoderMacroblockGlobalInfo) * nMBCount)); // for mbtype as baseMBType 
        }

        VM_ASSERT(refLayer < m_currentLayer);

        layerMb = &m_mbinfo.layerMbs[refLayer];
        curlayerMb = &m_mbinfo.layerMbs[m_currentLayer];

        H264DecoderLayerResizeParameter *resizeParams = curlayerMb->m_pResizeParameter;
        bool cropping = resizeParams->extended_spatial_scalability == 2 || resizeParams->leftOffset || resizeParams->topOffset ||
            resizeParams->cur_layer_width != resizeParams->scaled_ref_layer_width ||
            resizeParams->cur_layer_height != resizeParams->scaled_ref_layer_height;

        if (!cropping && !m_spatial_resolution_change)
        {
        }
        else
        {
            if (m_spatial_resolution_change && pSliceHeader->constrained_intra_resampling_flag)
            {
                constrainedIntraResamplingCheckMBs(pSlice->GetSeqParam(),
                                                    curlayerMb,
                                                    layerMb,
                                                    RefGlobalMBinfo->mbs,
                                                    m_mbinfo.m_tmpBuf2,
                                                    mb_width,
                                                    mb_height);

                SVCResampling<>::umc_h264_constrainedIntraUpsampling(pSlice,
                                                    curlayerMb,
                                                    RefGlobalMBinfo->mbs,
                                                    layerMb,
                                                    pFrame,
                                                    m_mbinfo,
                                                    pitch_luma,
                                                    pitch_chroma,
                                                    pixel_sz,
                                                    (refWidth + 32) * pixel_sz,
                                                    ((refWidth + 32) * pixel_sz) >> chromaShift,
                                                    nv12);
            }
            else
            {
                if (m_spatial_resolution_change)
                    SVCResampling<>::interpolateInterMBs(layerMb->m_pYPlane, (refWidth + 32) * pixel_sz,
                                        layerMb->m_pUPlane, (refChromaWidth + 16) * pixel_sz,
                                        layerMb->m_pVPlane, (refChromaWidth + 16) * pixel_sz,
                                        0, refHeight >> 4,
                                        refWidth >> 4, refHeight >> 4,
                                        RefGlobalMBinfo->mbs,
                                        layerMb,
                                        -1);

                SVCResampling<>::umc_h264_upsampling(pSlice,
                                    layerMb,
                                    curlayerMb,
                                    pFrame,
                                    m_mbinfo.m_tmpBuf0,
                                    m_mbinfo.m_tmpBuf1,
                                    pitch_luma,
                                    pitch_chroma,
                                    (refWidth + 32) * pixel_sz,
                                    ((refWidth + 32) * pixel_sz) >> chromaShift,
                                    nv12);
            }
        }

        umc_h264_mv_resampling(this,
                               pSlice,
                               layerMb, curlayerMb, m_gmbinfo,
                               &m_mbinfo);

        upsamplingResidual(pSlice, layerMb, /*(Coeffs*)*/curlayerMb,
                                   m_mbinfo.m_tmpBuf0,m_mbinfo.m_tmpBuf1, m_mbinfo.m_tmpBuf2,
                                    pRefFrame->lumaSize());

        pFrame->m_use_base_residual = 1;

        if (!m_spatial_resolution_change &&
            (curlayerMb->m_pResizeParameter->leftOffset ||
             curlayerMb->m_pResizeParameter->topOffset ||
             curlayerMb->m_pResizeParameter->ref_layer_width != ((Ipp32s)curSps->frame_width_in_mbs << 4)))
        {
            H264DecoderLayerResizeParameter *pResizeParameter = curlayerMb->m_pResizeParameter;
            Ipp32s MbW = (Ipp32s)curSps->frame_width_in_mbs;
            Ipp32s MbH = (Ipp32s)curSps->frame_height_in_mbs;
            Ipp32s leftOffset = pResizeParameter->leftOffset >> 4;
            Ipp32s rightOffset = pResizeParameter->rightOffset >> 4;
            Ipp32s topOffset = pResizeParameter->topOffset >> 4;
            Ipp32s bottomOffset = pResizeParameter->bottomOffset >> 4;

            Ipp32s startH = (topOffset < 0) ? 0 : topOffset;
            Ipp32s endH = (bottomOffset < 0) ? MbH : MbH - bottomOffset;
            Ipp32s startW = (leftOffset < 0) ? 0 : leftOffset;
            Ipp32s endW = (rightOffset < 0) ? MbW : MbW - rightOffset;

            if (curlayerMb->m_pResizeParameter->ref_layer_width != ((Ipp32s)curSps->frame_width_in_mbs << 4) && !curlayerMb->m_pResizeParameter->topOffset && 
                !curlayerMb->m_pResizeParameter->leftOffset)
            {
                Ipp32s refMbW = curlayerMb->m_pResizeParameter->ref_layer_width >> 4;
                for (Ipp32s i = MbH-bottomOffset; i > 0; i--)
                {
                    for (Ipp32s j = MbW-rightOffset; j >= 0; j--)
                    {
                        Ipp32s mbAddr = i*MbW + j;
                        Ipp32s refMbAddr = i*refMbW + j;
                        CopyMacroblockInfoFromBaseLayer(layerMb, mbAddr, refMbAddr);
                    }
                }
                return;
            }

            Ipp32s breakPoint;
            if ((leftOffset + rightOffset) == 0)
            {
                if ((leftOffset + topOffset * MbW) < 0)
                    breakPoint = endH;
                else
                    breakPoint = startH;
            }
            else
            {
                breakPoint = topOffset - ((leftOffset + topOffset * MbW)/(leftOffset + rightOffset));
            }

            if (breakPoint < startH)
                breakPoint = startH;

            if (breakPoint > endH)
                breakPoint = endH;

            // shift < 0
            for (Ipp32s j = startH; j < breakPoint; j++)
            {
                Ipp32s MbAddr = j * MbW;
                Ipp32s shift = (j - topOffset) * (leftOffset + rightOffset) + leftOffset + topOffset * MbW;

                for (Ipp32s i = startW; i < endW; i++)
                {
                    CopyMacroblockInfoFromBaseLayer(layerMb, MbAddr+i, MbAddr+i-shift);
                }
            }

            // shift > 0
            for (Ipp32s j = endH-1; j >= breakPoint; j--)
            {
                Ipp32s MbAddr = j * MbW;
                Ipp32s shift = (j - topOffset) * (leftOffset + rightOffset) + leftOffset + topOffset * MbW;

                for (Ipp32s i = endW - 1; i >= startW; i--)
                {
                    CopyMacroblockInfoFromBaseLayer(layerMb, MbAddr+i, MbAddr+i-shift);
                }
            }
        }
    }
    else if (pSlice->m_bIsFirstSliceInDepLayer)
    {
        H264DecoderFrame *pFrame = pSlice->GetCurrentFrame();
        IppiSize lumaSize, chromaSize;

        lumaSize = m_pCurrentFrame->lumaSize();

        chromaSize.width = lumaSize.width >> ((Ipp32s) (3 > pFrame->m_chroma_format));
        chromaSize.height = lumaSize.height >> ((Ipp32s) (2 > pFrame->m_chroma_format));

        if (m_pYResidual) {
            memset(m_pYResidual, 0, lumaSize.width * lumaSize.height*sizeof(Ipp16s));
            memset(m_pUResidual, 0, chromaSize.width * chromaSize.height*sizeof(Ipp16s));
            memset(m_pVResidual, 0, chromaSize.width * chromaSize.height*sizeof(Ipp16s));
        }

        pFrame->m_use_base_residual = 0;
    }

    if (pSlice->m_bIsFirstSliceInDepLayer && m_spatial_resolution_change && pSliceHeader->ref_layer_dq_id >= 0)
    {
        Ipp32u refLayer = pSliceHeader->ref_layer_dq_id >> 4;
        Ipp32u size = pFrame->m_pLayerFrames[refLayer]->lumaSize().width *
                   pFrame->m_pLayerFrames[refLayer]->lumaSize().height;

        memset(m_mbinfo.SavedMacroblockCoeffs, 0, size * sizeof(Ipp16s));

        size = (pFrame->m_pLayerFrames[m_currentLayer]->lumaSize().width * pFrame->m_pLayerFrames[m_currentLayer]->lumaSize().height) >> 8;
        memset(m_mbinfo.SavedMacroblockTCoeffs, 0, size * COEFFICIENTS_BUFFER_SIZE*sizeof(Ipp16s));
    }
}

void H264SegmentDecoderMultiThreaded::StoreRefBasePicture()
{
    H264Slice *pSlice = m_pSlice;
    H264DecoderFrame *pFrame = pSlice->GetCurrentFrame();

    if (pSlice->GetSliceNum() - 1 != pFrame->m_lastSliceInBaseRefLayer || !m_is_ref_base_pic)
        return;

    DeblockFrame(0, m_pSlice->GetCurrentFrame()->totalMBs);

    if (pFrame->m_storeRefBasePic != 1 || !pSlice->m_bIsMaxQId || !pSlice->m_bIsMaxDId)
        return;

    Ipp32s pitch_luma = pFrame->pitch_luma();
    Ipp32s pitch_chroma = pFrame->pitch_chroma();

    IppiSize lumaSize = pFrame->lumaSize();
    IppiSize chromaSize = pFrame->chromaSize();
    Ipp32s nv12 = (pFrame->GetColorFormat() == NV12);

    VM_ASSERT(pSlice->m_bIsMaxQId && pSlice->m_bIsMaxDId);

    CopyPlane(pFrame->m_pYPlane_base, pitch_luma,
              pFrame->m_pYPlane, pitch_luma, lumaSize);

    if (nv12) {
        chromaSize.width <<= 1;
        CopyPlane(pFrame->m_pUVPlane_base, pitch_chroma,
            pFrame->m_pUVPlane, pitch_chroma, chromaSize);
    } else {
        CopyPlane(pFrame->m_pUPlane_base, pitch_chroma,
                  pFrame->m_pUPlane, pitch_chroma, chromaSize);

        CopyPlane(pFrame->m_pVPlane_base, pitch_chroma,
                  pFrame->m_pVPlane, pitch_chroma, chromaSize);
    }
}

void H264SegmentDecoderMultiThreaded::StoreLayersMBInfo()
{
    H264Slice *pSlice = m_pSlice;
    H264DecoderFrame *pFrame = m_pCurrentFrame;
    Ipp32s nv12_flag = pFrame->GetColorFormat() == NV12;

    if (pSlice->m_bIsLastSliceInDepLayer && (m_currentLayer < pSlice->GetCurrentFrame()->m_maxDId))
    {
        Ipp32s nMBCount = pFrame->totalMBs << (pFrame->m_PictureStructureForDec < FRM_STRUCTURE ? 1 : 0);
        H264DecoderLayerDescriptor *layerMb = &m_mbinfo.layerMbs[m_currentLayer];
        MFX_INTERNAL_CPY(layerMb->MV[0], m_gmbinfo->MV[0], sizeof(H264DecoderMacroblockMVs) * nMBCount);
        MFX_INTERNAL_CPY(layerMb->MV[1], m_gmbinfo->MV[1], sizeof(H264DecoderMacroblockMVs) * nMBCount);

        for (Ipp32s i = 0; i < nMBCount; i++)
        {
            layerMb->mbs[i].mbtype = m_gmbinfo->mbs[i].mbtype;
            layerMb->mbs[i].sbtype[0] = m_gmbinfo->mbs[i].sbtype[0];
            layerMb->mbs[i].sbtype[1] = m_gmbinfo->mbs[i].sbtype[1];
            layerMb->mbs[i].sbtype[2] = m_gmbinfo->mbs[i].sbtype[2];
            layerMb->mbs[i].sbtype[3] = m_gmbinfo->mbs[i].sbtype[3];
            layerMb->mbs[i].mbflags = m_gmbinfo->mbs[i].mbflags;
            layerMb->mbs[i].sbdir[0] = m_mbinfo.mbs[i].sbdir[0];
            layerMb->mbs[i].sbdir[1] = m_mbinfo.mbs[i].sbdir[1];
            layerMb->mbs[i].sbdir[2] = m_mbinfo.mbs[i].sbdir[2];
            layerMb->mbs[i].sbdir[3] = m_mbinfo.mbs[i].sbdir[3];
            layerMb->mbs[i].refIdxs[0] = m_gmbinfo->mbs[i].refIdxs[0];
            layerMb->mbs[i].refIdxs[1] = m_gmbinfo->mbs[i].refIdxs[1];
        }

        IppiSize lumaSize = pFrame->lumaSize();

        IppiSize chromaSize = pFrame->chromaSize();

        Ipp32s pitch_luma = pSlice->GetCurrentFrame()->pitch_luma();
        Ipp32s pitch_chroma = pSlice->GetCurrentFrame()->pitch_chroma();

        // don't zero padded blocks now, will be filled in interpolateMB

        CopyPlane(m_pYPlane, pitch_luma,
                  layerMb->m_pYPlane, lumaSize.width + 32, lumaSize);

        if (nv12_flag) {
            CopyPlane_NV12(m_pUVPlane, pitch_chroma,
                layerMb->m_pUPlane, layerMb->m_pVPlane,
                chromaSize.width + 16, chromaSize);
        } else {
            CopyPlane(m_pUPlane, pitch_chroma,
                layerMb->m_pUPlane, chromaSize.width + 16, chromaSize);

            CopyPlane(m_pVPlane, pitch_chroma,
                layerMb->m_pVPlane, chromaSize.width + 16, chromaSize);
        }

    }
}

Status H264SegmentDecoderMultiThreaded::ProcessLayerPreSlice()
{
    m_store_ref_base_pic_flag = 0;
    if ((m_pSliceHeader->nal_ext.svc.store_ref_base_pic_flag== 1) &&
        (m_pSliceHeader->nal_ext.svc.quality_id == 0) &&
        m_pSlice->GetCurrentFrame()->m_storeRefBasePic)
    {
        m_store_ref_base_pic_flag = 1;
    }

    m_bIsMaxQId = m_pSlice->m_bIsMaxQId;
    m_bIsMaxDId = m_pSlice->m_bIsMaxDId;

    m_reconstruct_all = 0;
    if ((m_bIsMaxQId || m_store_ref_base_pic_flag == 1) && m_bIsMaxDId)
    {
        m_reconstruct_all = 1;
    }

    if ((m_reconstruct_all != 1) /* don't do deblocking at all*/ ||
        (m_store_ref_base_pic_flag && m_bIsMaxDId) /* do deblocking later */)
    {
        bDoDeblocking = false;
        m_pSlice->m_bDeblocked = true;
    }

    CalculateResizeParametersSlice();

    m_use_base_residual = m_pSlice->GetCurrentFrame()->m_use_base_residual;

    m_coeff_prediction_flag = !m_pSliceHeader->nal_ext.svc.no_inter_layer_pred_flag &&
        !m_spatial_resolution_change && !m_pSliceHeader->tcoeff_level_prediction_flag;

    m_use_coeff_prediction = m_pSliceHeader->tcoeff_level_prediction_flag || m_coeff_prediction_flag;
    return UMC_OK;
}


Status H264SegmentDecoderMultiThreaded::ProcessLayerPost()
{
    m_pSlice->StoreBaseLayerFrameInfo();
    StoreLayersMBInfo();
    StoreRefBasePicture();
    return UMC_OK;
}

void H264SegmentDecoderMultiThreaded::CalculateResizeParameters()
{
    CalculateResizeParametersSlice();

    H264Slice *pSlice = m_pSlice;
    H264DecoderFrame *pFrame = pSlice->GetCurrentFrame();
    H264SliceHeader *pSliceHeader = pSlice->GetSliceHeader();

    Ipp32s d_id = m_currentLayer;

    if (m_mbinfo.layerMbs == NULL)
        return;

    if (pSlice->m_bIsFirstSliceInDepLayer)
    {
        pFrame->m_offsets[d_id][0] = 0;
        pFrame->m_offsets[d_id][1] = 0;
        pFrame->m_offsets[d_id][2] = 0;
        pFrame->m_offsets[d_id][3] = 0;

        H264DecoderLayerDescriptor *curlayerMb = &m_mbinfo.layerMbs[m_currentLayer];

        if (pSliceHeader->ref_layer_dq_id >= 0)
        {
            pFrame->m_offsets[d_id][0] = curlayerMb->m_pResizeParameter->leftOffset;
            pFrame->m_offsets[d_id][1] = curlayerMb->m_pResizeParameter->topOffset;
            pFrame->m_offsets[d_id][2] = curlayerMb->m_pResizeParameter->rightOffset;
            pFrame->m_offsets[d_id][3] = curlayerMb->m_pResizeParameter->bottomOffset;
        }
    }
}

void H264SegmentDecoderMultiThreaded::CalculateResizeParametersSlice()
{
    H264Slice *pSlice = m_pSlice;
    H264DecoderFrame *pFrame = pSlice->GetCurrentFrame();
    H264SliceHeader *pSliceHeader = pSlice->GetSliceHeader();

    pSliceHeader = pSlice->m_firstILPredSliceInDepLayer ? pSlice->m_firstILPredSliceInDepLayer->GetSliceHeader() : pSliceHeader;

    m_spatial_resolution_change = 0;
    m_next_spatial_resolution_change = 1;

    if (m_mbinfo.layerMbs == NULL)
        return;

    if (pSlice->m_bIsFirstSliceInDepLayer)
    {
        const H264SeqParamSet *curSps = pSlice->GetSeqParam();
        H264DecoderLayerDescriptor *curlayerMb = &m_mbinfo.layerMbs[m_currentLayer];
        curlayerMb->frame_mbs_only_flag = curSps->frame_mbs_only_flag;
        curlayerMb->is_MbAff = pSliceHeader->MbaffFrameFlag;
        curlayerMb->field_pic_flag = pSliceHeader->field_pic_flag;
        curlayerMb->bottom_field_flag = pSliceHeader->bottom_field_flag;
        curlayerMb->is_profile_baseline_scalable = (pSlice->m_pSeqParamSet->profile_idc == H264VideoDecoderParams::H264_PROFILE_SCALABLE_BASELINE);

        curlayerMb->m_pResizeParameter = &curlayerMb->lrp[pFrame->GetNumberByParity(pSliceHeader->bottom_field_flag)];

        m_next_spatial_resolution_change = curlayerMb->m_pResizeParameter->next_spatial_resolution_change;

        if (!pSliceHeader->nal_ext.svc.quality_id)
        {
            m_spatial_resolution_change = curlayerMb->m_pResizeParameter->spatial_resolution_change;
        }
    }
    else
    {
        if (!pSliceHeader->nal_ext.svc.quality_id)
        {
            m_spatial_resolution_change = m_mbinfo.layerMbs[m_currentLayer].m_pResizeParameter->spatial_resolution_change;
        }
        m_next_spatial_resolution_change = m_mbinfo.layerMbs[m_currentLayer].m_pResizeParameter->next_spatial_resolution_change;
    }
}

Status H264SegmentDecoderMultiThreaded::ProcessSlice(Ipp32s iCurMBNumber, Ipp32s &iMBToProcess)
{
    Status umcRes = UMC_OK;
    Ipp32s iFirstMB = iCurMBNumber;
    Ipp32s iMBRowSize = m_pSlice->GetMBRowWidth();
    Ipp32s iMaxMBNumber = iCurMBNumber + iMBToProcess;

    Ipp32s iFirstMBToDeblock;
    Ipp32s iAvailableMBToDeblock = 0;

     // set deblocking condition
    bDoDeblocking = m_pSlice->GetDeblockingCondition();
    iFirstMBToDeblock = iCurMBNumber;

    if (m_pSlice->m_pSeqParamSetSvcEx)
    {
        bDoDeblocking = !m_pSlice->IsSliceGroups() && (DEBLOCK_FILTER_OFF != m_pSlice->GetSliceHeader()->disable_deblocking_filter_idc);
    }

    m_psBuffer = align_pointer<UMC::CoeffsPtrCommon> (m_pCoefficientsBuffer, DEFAULT_ALIGN_VALUE);

    ProcessLayerPre();
    ProcessLayerPreSlice();

    m_is_ref_base_pic = ((m_pSlice->GetSliceNum() - 1) <= m_pSlice->GetCurrentFrame()->m_lastSliceInBaseRefLayer) &&
        ((m_pSlice->GetSliceNum() - 1) >= m_pSlice->GetCurrentFrame()->m_firstSliceInBaseRefLayer) && (m_pSlice->GetCurrentFrame()->m_storeRefBasePic == 1);

    if (m_is_ref_base_pic)
    {
        H264DecoderFrame *pFrame = m_pSlice->GetCurrentFrame();

        Ipp32s pitch_luma = pFrame->pitch_luma();
        Ipp32s pitch_chroma = pFrame->pitch_chroma();

        IppiSize lumaSize = pFrame->lumaSize();
        IppiSize chromaSize = pFrame->chromaSize();
        Ipp32s nv12 = (pFrame->GetColorFormat() == NV12);

        if ((m_pSlice->GetSliceNum() - 1) == m_pSlice->GetCurrentFrame()->m_firstSliceInBaseRefLayer)
        {
            VM_ASSERT(pFrame->m_pYPlane_base);

            CopyPlane(pFrame->m_pYPlane, pitch_luma,
                      pFrame->m_pYPlane_base, pitch_luma, lumaSize);

            if (nv12) {
                chromaSize.width <<= 1;
                CopyPlane(pFrame->m_pUVPlane, pitch_chroma,
                    pFrame->m_pUVPlane_base, pitch_chroma, chromaSize);
            } else {
                CopyPlane(pFrame->m_pUPlane, pitch_chroma,
                          pFrame->m_pUPlane_base, pitch_chroma, chromaSize);

                CopyPlane(pFrame->m_pVPlane, pitch_chroma,
                          pFrame->m_pVPlane_base, pitch_chroma, chromaSize);
            }
        }

        m_pYPlane_base = pFrame->m_pYPlane;
        m_pUVPlane_base = pFrame->m_pUVPlane;
        m_pYPlane = pFrame->m_pYPlane_base;
        m_pUVPlane = pFrame->m_pUVPlane_base;

        if (pFrame->GetColorFormat() == NV12)
        {
            m_pUPlane = m_pUVPlane;
            m_pVPlane = m_pUVPlane;
            m_pUPlane_base = m_pUVPlane_base;
            m_pVPlane_base = m_pUVPlane_base;
        }
    }

    // handle a slice group case
    if (m_isSliceGroups)
    {
        // perform decoding on current row(s)
        if (0 == m_pPicParamSet->entropy_coding_mode)
        {
            umcRes = m_SD->DecodeSegmentCAVLC_Single(iFirstMB, iMaxMBNumber, this);
        }
        else
        {
            umcRes = m_SD->DecodeSegmentCABAC_Single(iFirstMB, iMaxMBNumber, this);
        }

        if ((UMC_OK == umcRes) ||
            (UMC_ERR_END_OF_STREAM == umcRes))
            umcRes = UMC_ERR_END_OF_STREAM;

        iMBToProcess = m_CurMBAddr - iCurMBNumber;
        return umcRes;
    }

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
            if (0 == m_pPicParamSet->entropy_coding_mode)
            {
                umcRes = m_SD->DecodeSegmentCAVLC_Single(iCurMBNumber, nBorder, this);
            }
            else
            {
                umcRes = m_SD->DecodeSegmentCABAC_Single(iCurMBNumber, nBorder, this);
            }
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

    ProcessLayerPost();

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

    return umcRes;
}

SegmentDecoderHPBase* H264SegmentDecoderMultiThreaded::CreateSegmentDecoder()
{
    // It is against baseline/main constraints, but just for robustness
    bool faked_high_profile = m_pPicParamSet->transform_8x8_mode_flag || m_pPicParamSet->pic_scaling_matrix_present_flag ||
        m_pPicParamSet->chroma_qp_index_offset[1] != m_pPicParamSet->chroma_qp_index_offset[0];

    return CreateSD(
        bit_depth_luma,
        bit_depth_chroma,
        m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE,
        m_pCurrentFrame->m_chroma_format,
        (m_pSeqParamSet->profile_idc >= H264VideoDecoderParams::H264_PROFILE_HIGH ||
        m_pSeqParamSet->profile_idc == H264VideoDecoderParams::H264_PROFILE_CAVLC444_INTRA ||
        m_pSeqParamSet->profile_idc == H264VideoDecoderParams::H264_PROFILE_SCALABLE_BASELINE ||
        m_pSeqParamSet->profile_idc == H264VideoDecoderParams::H264_PROFILE_SCALABLE_HIGH) ||
        faked_high_profile);
}

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
