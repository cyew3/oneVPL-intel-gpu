/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//
//
*/

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef UMC_RESTRICTED_CODE_VA

#include <algorithm>
#include "umc_structures.h"

#include "umc_h265_segment_decoder_dxva.h"
#include "umc_h265_frame_list.h"

#include "umc_h265_slice_decoding.h"
#include "umc_h265_task_supplier.h"
#include "umc_h265_frame_info.h"

#include "umc_h265_va_packer.h"

#ifdef UMC_VA_DXVA
#include "umc_va_base.h"
#include "umc_va_dxva2_protected.h"
#endif


namespace UMC_HEVC_DECODER
{
H265_DXVA_SegmentDecoderCommon::H265_DXVA_SegmentDecoderCommon(TaskSupplier_H265 * pTaskSupplier)
    : H265SegmentDecoderBase(pTaskSupplier->GetTaskBroker())
    , m_va(0)
    , m_pTaskSupplier(pTaskSupplier)
{
}

void H265_DXVA_SegmentDecoderCommon::SetVideoAccelerator(UMC::VideoAccelerator *va)
{
    VM_ASSERT(va);
    m_va = (UMC::VideoAccelerator*)va;
}

H265_DXVA_SegmentDecoder::H265_DXVA_SegmentDecoder(TaskSupplier_H265 * pTaskSupplier)
    : H265_DXVA_SegmentDecoderCommon(pTaskSupplier)
    , m_CurrentSliceID(0)
{
}

H265_DXVA_SegmentDecoder::~H265_DXVA_SegmentDecoder()
{
}

UMC::Status H265_DXVA_SegmentDecoder::Init(Ipp32s iNumber)
{
    return H265SegmentDecoderBase::Init(iNumber);
}

void H265_DXVA_SegmentDecoder::PackAllHeaders(H265DecoderFrame * pFrame)
{
    H265DecoderFrameInfo * sliceInfo = pFrame->m_pSlicesInfo;
    int sliceCount = sliceInfo->GetSliceCount();

    if (!sliceCount)
        return;

#ifdef UMC_VA_DXVA
    if (!m_Packer.get())
    {
        if (m_va->IsIntelCustomGUID()) // intel MVC profile
            m_Packer.reset(new PackerDXVA2(m_va));
        else
            m_Packer.reset(new MSPackerDXVA2(m_va));
    }
#endif

    H265Slice *pSlice = sliceInfo->GetSlice(0);
    const H265SeqParamSet *pSeqParamSet = pSlice->GetSeqParam();
    H265DecoderFrame *pCurrentFrame = pSlice->GetCurrentFrame();

    m_Packer->BeginFrame();
    m_Packer->PackPicParams(pCurrentFrame, sliceInfo, m_pTaskSupplier);
    if (pSeqParamSet->scaling_list_enabled_flag)
    {
        m_Packer->PackQmatrix(pSlice);
    }

    bool isLongFormat = m_va->IsLongSliceControl();

    Ipp32u sliceNum = 0;
    for (Ipp32s n = 0; n < sliceCount; n++)
    {
        m_Packer->PackSliceParams(sliceInfo->GetSlice(n), sliceNum, isLongFormat, n == sliceCount - 1);
        sliceNum++;
    }

    m_Packer->ExecuteBuffers();
    m_Packer->EndFrame();
}

UMC::Status H265_DXVA_SegmentDecoder::ProcessSegment(void)
{
    H265Task Task(m_iNumber);

    if (m_pTaskBroker->GetNextTask(&Task))
    {
    }
    else
    {
        return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    }

    return UMC::UMC_OK;
}


TaskBrokerSingleThreadDXVA::TaskBrokerSingleThreadDXVA(TaskSupplier_H265 * pTaskSupplier)
    : TaskBroker_H265(pTaskSupplier)
    , m_lastCounter(0)
    , m_useDXVAStatusReporting(true)
{
    m_counterFrequency = vm_time_get_frequency();
}

bool TaskBrokerSingleThreadDXVA::PrepareFrame(H265DecoderFrame * pFrame)
{
    if (!pFrame || pFrame->prepared)
    {
        return true;
    }

    if (!pFrame->prepared &&
        (pFrame->GetAU()->GetStatus() == H265DecoderFrameInfo::STATUS_FILLED || pFrame->GetAU()->GetStatus() == H265DecoderFrameInfo::STATUS_STARTED))
    {
        pFrame->prepared = true;
    }

    return true;
}

void TaskBrokerSingleThreadDXVA::Reset()
{
    m_lastCounter = 0;
    TaskBroker_H265::Reset();
}

void TaskBrokerSingleThreadDXVA::Start()
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    TaskBroker_H265::Start();
    m_completedQueue.clear();

    if (m_useDXVAStatusReporting)
        return;

    for (H265DecoderFrameInfo *pTemp = m_FirstAU; pTemp; pTemp = pTemp->GetNextAU())
    {
        pTemp->SetStatus(H265DecoderFrameInfo::STATUS_COMPLETED);
        CompleteFrame(pTemp->m_pFrame);
    }

    m_FirstAU = 0;
}

enum
{
    NUMBER_OF_STATUS = 512,
};

bool TaskBrokerSingleThreadDXVA::GetNextTaskInternal(H265Task *)
{
    if (!m_useDXVAStatusReporting)
        return false;

    H265_DXVA_SegmentDecoder * dxva_sd = static_cast<H265_DXVA_SegmentDecoder *>(m_pTaskSupplier->m_pSegmentDecoder[0]);

    if (!dxva_sd->GetPacker())
        return false;

#ifdef UMC_VA_DXVA
    DXVA_Intel_Status_HEVC pStatusReport[NUMBER_OF_STATUS];

    bool wasCompleted = false;

    if (m_reports.size() && m_FirstAU)
    {
        Ipp32u i = 0;
        H265DecoderFrameInfo * au = m_FirstAU;
        bool wasFound = false;
        while (au)
        {
            for (; i < m_reports.size(); i++)
            {
                if ((m_reports[i].m_index == (Ipp32u)au->m_pFrame->m_index) /* && (au->IsBottom() == (m_reports[i].m_field != 0)) */)
                {
                    switch (m_reports[i].m_status)
                    {
                    case 1:
                        au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MINOR);
                        break;
                    case 2:
                        au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                        break;
                    case 3:
                        au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                        break;
                    case 4:
                        au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                        break;
                    }

                    au->SetStatus(H265DecoderFrameInfo::STATUS_COMPLETED);
                    CompleteFrame(au->m_pFrame);
                    wasFound = true;
                    wasCompleted = true;

                    m_reports.erase(m_reports.begin() + i);//, (i + 1 == m_reports.size()) ? m_reports.end() : m_reports.begin() + i + i);
                    break;
                }
            }

            if (!wasFound)
                break;

            au = au->GetNextAU();
        }

        SwitchCurrentAU();

        //m_reports.erase(m_reports.begin(), m_reports.begin() + i);
    }

    for (;;)
    {
        memset (&pStatusReport, 0, sizeof(pStatusReport));
        dxva_sd->GetPacker()->GetStatusReport(&pStatusReport[0], sizeof(DXVA_Intel_Status_HEVC)* NUMBER_OF_STATUS);

        for (Ipp32u i = 0; i < NUMBER_OF_STATUS; i++)
        {
            if (!pStatusReport[i].StatusReportFeedbackNumber)
                continue;

            bool wasFound = false;
            H265DecoderFrameInfo * au = m_FirstAU;
            for (; au; au = au->GetNextAU())
            {
                if (pStatusReport[i].current_picture.Index7Bits == au->m_pFrame->m_index)
                {
                    switch (pStatusReport[i].bStatus)
                    {
                    case 1:
                        au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MINOR);
                        break;
                    case 2:
                        au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                        break;
                    case 3:
                        au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                        break;
                    case 4:
                        au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                        break;
                    }

                    au->SetStatus(H265DecoderFrameInfo::STATUS_COMPLETED);
                    CompleteFrame(au->m_pFrame);
                    wasFound = true;
                    wasCompleted = true;
                    break;
                }

                break;
            }

            if (!wasFound)
            {
                if (std::find(m_reports.begin(), m_reports.end(), ReportItem(pStatusReport[i].current_picture.Index7Bits, 0/*field*/, 0)) == m_reports.end())
                {
                    m_reports.push_back(ReportItem(pStatusReport[i].current_picture.Index7Bits, 0/*field*/, pStatusReport[i].bStatus));
                    wasCompleted = true;
                }
            }
        }

        SwitchCurrentAU();
        if (!m_FirstAU)
            break;

        break;
    }

    if (!wasCompleted && m_FirstAU)
    {
        Ipp64u currentCounter = (Ipp64u) vm_time_get_tick();

        if (m_lastCounter == 0)
            m_lastCounter = currentCounter;

        Ipp64u diff = (currentCounter - m_lastCounter);
        if (diff >= m_counterFrequency)
        {
            Report::iterator iter = std::find(m_reports.begin(), m_reports.end(), ReportItem(m_FirstAU->m_pFrame->m_index, false, 0));
            if (iter != m_reports.end())
            {
                m_reports.erase(iter);
            }

            m_FirstAU->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
            m_FirstAU->SetStatus(H265DecoderFrameInfo::STATUS_COMPLETED);
            CompleteFrame(m_FirstAU->m_pFrame);

            SwitchCurrentAU();
            m_lastCounter = 0;
        }
    }
    else
    {
        m_lastCounter = 0;
    }

#endif

    return false;
}

void TaskBrokerSingleThreadDXVA::AwakeThreads()
{
}

} // namespace UMC_HEVC_DECODER

#endif // UMC_RESTRICTED_CODE_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER
