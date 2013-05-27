/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2012 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_RA_SUPPLIER_H
#define __UMC_H264_RA_SUPPLIER_H

#define UMC_RESTRICTED_CODE_RA

#ifndef UMC_RESTRICTED_CODE_RA

#include "umc_h264_task_supplier.h"

namespace UMC
{

/****************************************************************************************************/
// TaskSupplier
/****************************************************************************************************/
class RATaskSupplier :
          public TaskSupplier
{
    friend class TaskBroker;

public:

    RATaskSupplier(VideoData &m_LastDecodedFrame, BaseCodec *&m_PostProcessing);

    Status GetRandomFrame(MediaData *pSource, MediaData *pDst, ReferenceList* /*reflist*/);

protected:
    virtual void CompleteFrame(H264DecoderFrame * pFrame, Ipp32s field);
    virtual bool GetFrameToDisplay(MediaData *dst, bool force);
    virtual void InitFrameCounter(H264DecoderFrame * pFrame, const H264Slice *pSlice);

    virtual Status AllocateFrameData(H264DecoderFrame * pFrame, IppiSize dimensions, Ipp32s bit_depth, Ipp32s chroma_format_idc);

private:
    RATaskSupplier & operator = (RATaskSupplier &)
    {
        return *this;

    } // RATaskSupplier & operator = (RATaskSupplier &)

    H264DecoderFrame * m_pLastDecodedFrame;

    void LoadDPB(ReferenceList* reflist);
    ReferenceList* g_reflist;
};

} // namespace UMC

#endif // UMC_RESTRICTED_CODE_RA

#endif // __UMC_H264_RA_SUPPLIER_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
