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

#ifndef __UMC_H264_VA_SUPPLIER_H
#define __UMC_H264_VA_SUPPLIER_H

#include "umc_h264_dec_mfx.h"
#include "umc_h264_mfx_supplier.h"
#include "umc_h264_segment_decoder_dxva.h"

namespace UMC
{

class MFXVideoDECODEH264;

class LazyCopier
{
public:
    void Reset();

    void Add(H264Slice * slice);
    void Remove(H264DecoderFrameInfo * info);
    void Remove(H264Slice * slice);
    void CopyAll();

private:
    typedef std::list<H264Slice *> SlicesList;
    SlicesList m_slices;
};

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

    virtual void Close();
    virtual void Reset();

    void SetBufferedFramesNumber(Ipp32u buffered);

    virtual Status DecodeHeaders(NalUnit *nalUnit);

    virtual Status AddSource(MediaData * pSource);

protected:

    virtual void CreateTaskBroker();

    virtual Status AllocateFrameData(H264DecoderFrame * pFrame);

    virtual void InitFrameCounter(H264DecoderFrame * pFrame, const H264Slice *pSlice);

    virtual Status CompleteFrame(H264DecoderFrame * pFrame, Ipp32s field);

    virtual void AfterErrorRestore();

    virtual H264DecoderFrame * GetFreeFrame(const H264Slice *pSlice = NULL);

    virtual H264Slice * DecodeSliceHeader(NalUnit *nalUnit);

    virtual H264DecoderFrame *GetFrameToDisplayInternal(bool force);

protected:

    virtual Ipp32s GetFreeFrameIndex();

    Ipp32u m_bufferedFrameNumber;
    LazyCopier m_lazyCopier;

private:
    VATaskSupplier & operator = (VATaskSupplier &)
    {
        return *this;
    }
};

// this template class added to apply big surface pool workaround depends on platform
// because platform check can't be added inside VATaskSupplier
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

    virtual Status AllocateFrameData(H264DecoderFrame * pFrame)
    {
        Status ret = BaseClass::AllocateFrameData(pFrame);
        if (ret == UMC_OK)
        {
            pFrame->m_index = BaseClass::GetFreeFrameIndex();
        }

        return ret;
    }
};
} // namespace UMC

#endif // __UMC_H264_VA_SUPPLIER_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
