//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_SEGMENT_DECODER_MT_H
#define __UMC_H265_SEGMENT_DECODER_MT_H

#include "umc_h265_segment_decoder.h"

namespace UMC_HEVC_DECODER
{

class SegmentDecoderRoutines;
class H265Task;

// Slice decoder capable of running in parallel with other decoders
class H265SegmentDecoderMultiThreaded : public H265SegmentDecoder
{
public:
    // Default constructor
    H265SegmentDecoderMultiThreaded(TaskBroker_H265 * pTaskBroker);
    // Destructor
    virtual ~H265SegmentDecoderMultiThreaded(void);

    // Initialize slice decoder
    virtual UMC::Status Init(Ipp32s iNumber);

    // Initialize decoder and call task processing function
    virtual UMC::Status ProcessSegment(void);

    virtual UMC::Status ProcessTask(H265Task &task);

    // Initialize CABAC context appropriately depending on where starting CTB is
    void InitializeDecoding(H265Task & task);
    // Combined decode and reconstruct task callback
    UMC::Status DecRecSegment(H265Task & task);
    // Decode task callback
    UMC::Status DecodeSegment(H265Task & task);
    // Reconstruct task callback
    UMC::Status ReconstructSegment(H265Task & task);
    // Deblocking filter task callback
    virtual UMC::Status DeblockSegmentTask(H265Task & task);
    // SAO filter task callback
    UMC::Status SAOFrameTask(H265Task & task);

    // Initialize state, decode, reconstruct and filter CTB range
    virtual UMC::Status ProcessSlice(H265Task & task);

    // Recover a region after error
    virtual void RestoreErrorRect(H265Task * task);

protected:

    // Initialize decoder with data of new slice
    virtual void StartProcessingSegment(H265Task &Task);
    // Finish work section
    virtual void EndProcessingSegment(H265Task &Task);

    // Decode one CTB
    bool DecodeCodingUnit_CABAC();

    // Decode CTB range
    UMC::Status DecodeSegment(Ipp32s curCUAddr, Ipp32s &nBorder);

    // Reconstruct CTB range
    UMC::Status ReconstructSegment(Ipp32s curCUAddr, Ipp32s nBorder);

    // Both decode and reconstruct a CTB range
    UMC::Status DecodeSegmentCABAC_Single_H265(Ipp32s curCUAddr, Ipp32s & nBorder);

    // Reconstructor depends on bitdepth_luma || bitdepth_chroma
    void CreateReconstructor();

    void RestoreErrorRect(Ipp32s startMb, Ipp32s endMb, H265DecoderFrame *pRefFrame);

private:

    // we lock the assignment operator to avoid any
    // accasional assignments
    H265SegmentDecoderMultiThreaded & operator = (H265SegmentDecoderMultiThreaded &)
    {
        return *this;

    } // H265SegmentDecoderMultiThreaded & operator = (H265SegmentDecoderMultiThreaded &)
};

} // namespace UMC_HEVC_DECODER

#endif /* __UMC_H265_SEGMENT_DECODER_MT_H */
#endif // UMC_ENABLE_H265_VIDEO_DECODER
