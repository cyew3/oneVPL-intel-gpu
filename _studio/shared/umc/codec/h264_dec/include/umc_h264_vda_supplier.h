/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012 Intel Corporation. All Rights Reserved.
//
//
*/

#ifdef __APPLE__
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_VDA_SUPPLIER_H
#define __UMC_H264_VDA_SUPPLIER_H

#include "umc_h264_dec_mfx.h"
#include "umc_h264_mfx_supplier.h"
#include "umc_h264_segment_decoder_dxva.h"

#include <VDADecoder.h>
#include <CVImageBuffer.h>

namespace UMC
{

class MFXVideoDECODEH264;

/****************************************************************************************************/
// TaskSupplier
/****************************************************************************************************/
    class VDATaskSupplier : public MFXTaskSupplier, public DXVASupport<VDATaskSupplier>

{
    friend class TaskBroker;
    friend class DXVASupport<VDATaskSupplier>;    
    friend class VideoDECODEH264;

public:

    VDATaskSupplier();
    ~VDATaskSupplier();

    virtual Status Init(BaseCodecParams *pInit);
    virtual void Close();
    virtual bool GetFrameToDisplay(MediaData *dst, bool force);
    void SetBufferedFramesNumber(Ipp32u buffered);

protected:
    VDADecoder m_VDADecoder;
    bool m_isVDAInstantiated; 
    bool m_isHaveSPS;           //flag indicating whether or not the raw SPS bytes are available
    bool m_isHavePPS;           //flag indicating whether or not the raw PPS bytes are available
    OSStatus CreateDecoder(SInt32 inHeight, SInt32 inWidth, OSType inSourceFormat, CFDataRef inAVCCData, VDADecoder *decoderOut, void * userContextInfo);
    
    virtual Status RunDecoding(bool force, H264DecoderFrame ** decoded = 0);

    virtual Status AllocateFrameData(H264DecoderFrame * pFrame, IppiSize dimensions, Ipp32s bit_depth, ColorFormat chroma_format_idc);

    virtual Status CompleteFrame(H264DecoderFrame * pFrame, Ipp32s field);

    virtual H264DecoderFrame * GetFreeFrame(const H264Slice *pSlice = NULL);

    virtual H264Slice * DecodeSliceHeader(MediaDataEx *nalUnit);

    virtual Status DecodeHeaders(MediaDataEx *nalUnit);
//    virtual H264DecoderFrame *GetFrameToDisplayInternal(bool force);   
//    virtual H264DecoderFrame *GetAnyFrameToDisplay(bool force);
private:
    Ipp32u m_bufferedFrameNumber;

    H264DecoderFrame * m_pLastDecodedFrame;   //needed for cut and paste from VATaskSupplier::CompleteFrame
};

class TaskBrokerSingleThreadVDA : public TaskBrokerSingleThread
{
public:
    TaskBrokerSingleThreadVDA(TaskSupplier * pTaskSupplier);

    virtual void WaitFrameCompletion();

    virtual bool PrepareFrame(H264DecoderFrame * pFrame);

    // Get next working task
    virtual bool GetNextTaskInternal(H264Task *pTask);
    virtual void Start();

    virtual void Reset();
    void decoderOutputCallback(CFDictionaryRef frameInfo, OSStatus status, uint32_t infoFlags, CVImageBufferRef imageBuffer);
    static void decoderOutputCallback2(void *decompressionOutputRefCon, CFDictionaryRef frameInfo, OSStatus status, uint32_t infoFlags, CVImageBufferRef imageBuffer);

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
};

class H264_VDA_SegmentDecoder : public H264SegmentDecoderMultiThreaded
{
public:
    H264_VDA_SegmentDecoder(TaskSupplier * pTaskSupplier);

    virtual Status ProcessSegment(void);
};

} // namespace UMC


#endif // __UMC_H264_VDA_SUPPLIER_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
#endif // __APPLE__
