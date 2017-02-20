//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_SEGMENT_DECODER_DXVA_H
#define __UMC_H264_SEGMENT_DECODER_DXVA_H

#include "umc_va_base.h"

#include <memory>
#include "umc_h264_segment_decoder_base.h"
#include "umc_h264_task_broker.h"
#include "umc_h264_va_packer.h"
#include "umc_h264_frame.h"

#define UMC_VA_MAX_FRAME_BUFFER 32 //max number of uncompressed buffers

namespace UMC
{
class TaskSupplier;

class H264_DXVA_SegmentDecoderCommon : public H264SegmentDecoderBase
{
public:
    H264_DXVA_SegmentDecoderCommon(TaskSupplier * pTaskSupplier);

    VideoAccelerator *m_va;

    void SetVideoAccelerator(VideoAccelerator *va);

protected:
    TaskSupplier * m_pTaskSupplier;

private:
    H264_DXVA_SegmentDecoderCommon & operator = (H264_DXVA_SegmentDecoderCommon &)
    {
        return *this;

    }
};

class H264_DXVA_SegmentDecoder : public H264_DXVA_SegmentDecoderCommon
{
public:
    H264_DXVA_SegmentDecoder(TaskSupplier * pTaskSupplier);

    ~H264_DXVA_SegmentDecoder();

    // Initialize object
    virtual Status Init(Ipp32s iNumber);

    void PackAllHeaders(H264DecoderFrame * pFrame, Ipp32s field);

    virtual Status ProcessSegment(void);

    Packer * GetPacker() { return m_Packer.get();}

    virtual void Reset();

protected:

    std::auto_ptr<Packer>  m_Packer;

private:
    H264_DXVA_SegmentDecoder & operator = (H264_DXVA_SegmentDecoder &)
    {
        return *this;
    }
};

/****************************************************************************************************/
// DXVASupport class implementation
/****************************************************************************************************/
template <class BaseClass>
class DXVASupport
{
public:

    DXVASupport()
        : m_va(0)
        , m_Base(0)
    {
    }

    ~DXVASupport()
    {
    }

    void Init()
    {
        m_Base = (BaseClass*)(this);
        if (!m_va)
            return;
    }

    void Reset()
    {
        H264_DXVA_SegmentDecoder * dxva_sd = (H264_DXVA_SegmentDecoder*)(m_Base->m_pSegmentDecoder[0]);
        if (dxva_sd)
            dxva_sd->Reset();
    }

    void DecodePicture(H264DecoderFrame * pFrame, Ipp32s field)
    {
        if (!m_va)
            return;

        Status sts = m_va->BeginFrame(pFrame->GetFrameData()->GetFrameMID());
        if (sts != UMC_OK)
            throw h264_exception(sts);

        H264_DXVA_SegmentDecoder * dxva_sd = (H264_DXVA_SegmentDecoder*)(m_Base->m_pSegmentDecoder[0]);
        VM_ASSERT(dxva_sd);

        for (Ipp32u i = 0; i < m_Base->m_iThreadNum; i++)
        {
            ((H264_DXVA_SegmentDecoder *)m_Base->m_pSegmentDecoder[i])->SetVideoAccelerator(m_va);
        }

        dxva_sd->PackAllHeaders(pFrame, field);

        sts = m_va->EndFrame();
        if (sts != UMC_OK)
            throw h264_exception(sts);
    }

    void SetVideoHardwareAccelerator(VideoAccelerator * va)
    {
        if (va)
            m_va = (VideoAccelerator*)va;
    }

protected:
    VideoAccelerator *m_va;
    BaseClass * m_Base;
};

class TaskBrokerSingleThreadDXVA : public TaskBroker
{
public:
    TaskBrokerSingleThreadDXVA(TaskSupplier * pTaskSupplier);

    virtual bool PrepareFrame(H264DecoderFrame * pFrame);

    // Get next working task
    virtual bool GetNextTaskInternal(H264Task *pTask);
    virtual void Start();

    virtual void Reset();

    void DXVAStatusReportingMode(bool mode)
    {
        m_useDXVAStatusReporting = mode;
    }

protected:
    virtual void AwakeThreads();

    class ReportItem
    {
    public:
        Ipp32u  m_index;
        Ipp32u  m_field;
        Ipp8u   m_status;

        ReportItem(Ipp32u index, Ipp32u field, Ipp8u status)
            : m_index(index)
            , m_field(field)
            , m_status(status)
        {
        }

        bool operator == (const ReportItem & item)
        {
            return (item.m_index == m_index) && (item.m_field == m_field);
        }

        bool operator != (const ReportItem & item)
        {
            return (item.m_index != m_index) || (item.m_field != m_field);
        }
    };

    typedef std::vector<ReportItem> Report;
    Report m_reports;
    Ipp64u m_lastCounter;
    Ipp64u m_counterFrequency;

    bool   m_useDXVAStatusReporting;
};

} // namespace UMC

#endif /* __UMC_H264_SEGMENT_DECODER_DXVA_H */
#endif // UMC_ENABLE_H264_VIDEO_DECODER
