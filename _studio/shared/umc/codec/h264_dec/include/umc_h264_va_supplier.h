//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_VA_SUPPLIER_H
#define __UMC_H264_VA_SUPPLIER_H

#ifndef UMC_RESTRICTED_CODE_VA

#include "umc_h264_dec_mfx.h"
#include "umc_h264_mfx_supplier.h"
#include "umc_h264_segment_decoder_dxva.h"

namespace UMC
{

class MFXVideoDECODEH264;

/****************************************************************************************************/
// TaskSupplier
/****************************************************************************************************/
class VATaskSupplier :
          public MFXTaskSupplier
        , public DXVASupport<VATaskSupplier>
{
    friend class TaskBroker;
    friend class DXVASupport<VATaskSupplier>;
    friend class VideoDECODEH264;

public:

    VATaskSupplier();

    virtual Status Init(VideoDecoderParams *pInit);

    virtual void Reset();

    void SetBufferedFramesNumber(Ipp32u buffered);

    virtual Status DecodeHeaders(MediaDataEx *nalUnit);

protected:

    virtual void CreateTaskBroker();

    Status AddSource(MediaData*);

    virtual Status AllocateFrameData(H264DecoderFrame * pFrame, IppiSize dimensions, Ipp32s bit_depth, ColorFormat chroma_format_idc);

    virtual void InitFrameCounter(H264DecoderFrame * pFrame, const H264Slice *pSlice);

    virtual Status CompleteFrame(H264DecoderFrame * pFrame, Ipp32s field);

    virtual void AfterErrorRestore();

    virtual H264DecoderFrame * GetFreeFrame(const H264Slice *pSlice = NULL);

    virtual H264Slice * DecodeSliceHeader(MediaDataEx *nalUnit);

    virtual H264DecoderFrame *GetFrameToDisplayInternal(bool force);

protected:

    virtual Ipp32s GetFreeFrameIndex();

    Ipp32u m_bufferedFrameNumber;

private:
    VATaskSupplier & operator = (VATaskSupplier &)
    {
        return *this;
    }
};

// this template class added to apply big surface pool workaround depends on platform
// because platform check can't be added inside VATaskSupplier
// for WidevineTaskSupplier the also can be applied
template <class BaseClass>
class VATaskSupplierBigSurfacePool :
    public BaseClass
{
public:
    VATaskSupplierBigSurfacePool()
    {};
    virtual ~VATaskSupplierBigSurfacePool()
    {};

protected:

    virtual Status AllocateFrameData(H264DecoderFrame * pFrame, IppiSize dimensions, Ipp32s bit_depth, ColorFormat chroma_format_idc)
    {
        Status ret = BaseClass::AllocateFrameData(pFrame, dimensions, bit_depth, chroma_format_idc);
        if (ret == UMC_OK)
        {
            pFrame->m_index = BaseClass::GetFreeFrameIndex();
        }

        return ret;
    }
};
} // namespace UMC

#endif // UMC_RESTRICTED_CODE_VA

#endif // __UMC_H264_VA_SUPPLIER_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
