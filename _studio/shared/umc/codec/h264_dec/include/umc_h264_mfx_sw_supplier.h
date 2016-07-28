/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_MFX_SW_SUPPLIER_H
#define __UMC_H264_MFX_SW_SUPPLIER_H

#include "umc_h264_mfx_supplier.h"

namespace UMC
{

class MFX_SW_TaskSupplier : public MFXTaskSupplier
{
public:
    MFX_SW_TaskSupplier();

    virtual void CreateTaskBroker();
    virtual H264Slice * CreateSlice();
    virtual void SetMBMap(const H264Slice * slice, H264DecoderFrame *frame, LocalResources * localRes);

    virtual bool ProcessNonPairedField(H264DecoderFrame * pFrame);

    virtual void AddFakeReferenceFrame(H264Slice * pSlice);

    virtual H264DecoderFrame * GetFreeFrame(const H264Slice *pSlice = NULL);

    virtual Status AllocateFrameData(H264DecoderFrame * pFrame, IppiSize dimensions, Ipp32s bit_depth, ColorFormat color_format);

    virtual void OnFullFrame(H264DecoderFrame * pFrame);

private:

};

} // namespace UMC

#endif // __UMC_H264_MFX_SW_SUPPLIER_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
