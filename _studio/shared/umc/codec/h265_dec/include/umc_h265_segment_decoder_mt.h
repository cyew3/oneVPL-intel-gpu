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

#ifndef __UMC_H265_SEGMENT_DECODER_MT_H
#define __UMC_H265_SEGMENT_DECODER_MT_H

#include "umc_h265_segment_decoder.h"

namespace UMC_HEVC_DECODER
{

class SegmentDecoderHPBase_H265;
class H265Task;

class H265SegmentDecoderMultiThreaded : public H265SegmentDecoder
{
public:
    // Default constructor
    H265SegmentDecoderMultiThreaded(TaskBroker_H265 * pTaskBroker);
    // Destructor
    virtual
    ~H265SegmentDecoderMultiThreaded(void);

    // Initialize object
    virtual
    UMC::Status Init(Ipp32s iNumber);

    // Decode slice's segment
    virtual
    UMC::Status ProcessSegment(void);

    // asynchronous called functions
    UMC::Status DecRecSegment(H265Task & task);
    UMC::Status DecodeSegment(H265Task & task);
    UMC::Status ReconstructSegment(H265Task & task);
    virtual UMC::Status DeblockSegmentTask(H265Task & task);
    UMC::Status SAOFrameTask(H265Task & task);

    virtual UMC::Status ProcessSlice(H265Task & task);

    void RestoreErrorRect(Ipp32s startMb, Ipp32s endMb, H265Slice * pSlice);

    SegmentDecoderHPBase_H265* m_SD;

protected:

    virtual SegmentDecoderHPBase_H265* CreateSegmentDecoder();

    virtual void StartProcessingSegment(H265Task &Task);
//    virtual void EndProcessingSegment(H265Task &Task, H265SampleAdaptiveOffset* pSAO);
    virtual void EndProcessingSegment(H265Task &Task);

private:

    // we lock the assignment operator to avoid any
    // accasional assignments
    H265SegmentDecoderMultiThreaded & operator = (H265SegmentDecoderMultiThreaded &)
    {
        return *this;

    } // H265SegmentDecoderMultiThreaded & operator = (H265SegmentDecoderMultiThreaded &)
};

} // namespace UMC_HEVC_DECODER

#endif /* __UMC_H264_SEGMENT_DECODER_MT_H */
#endif // UMC_ENABLE_H264_VIDEO_DECODER
