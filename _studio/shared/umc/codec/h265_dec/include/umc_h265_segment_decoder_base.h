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

#ifndef __UMC_H265_SEGMENT_DECODER_BASE_H
#define __UMC_H265_SEGMENT_DECODER_BASE_H

#include "umc_h265_dec_defs.h"

namespace UMC_HEVC_DECODER
{

class H265Task;
class TaskBroker_H265;

class H265SegmentDecoderBase
{
public:
    H265SegmentDecoderBase(TaskBroker_H265 * pTaskBroker)
        : m_iNumber(0)
        , m_pTaskBroker(pTaskBroker)
    {
    }

    virtual ~H265SegmentDecoderBase()
    {
    }

    virtual UMC::Status Init(Ipp32s iNumber)
    {
        m_iNumber = iNumber;
        return UMC::UMC_OK;
    }

    // Decode slice's segment
    virtual UMC::Status ProcessSegment(void) = 0;

    virtual void RestoreErrorRect(H265Task *)
    {
    }

protected:
    Ipp32s m_iNumber;                                           // (Ipp32s) ordinal number of decoder
    TaskBroker_H265 * m_pTaskBroker;
};

} // namespace UMC_HEVC_DECODER

#endif /* __UMC_H265_SEGMENT_DECODER_BASE_H */
#endif // UMC_ENABLE_H265_VIDEO_DECODER
