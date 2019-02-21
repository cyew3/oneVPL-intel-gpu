// Copyright (c) 2003-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

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

    virtual Status AllocateFrameData(H264DecoderFrame * pFrame);

    virtual void OnFullFrame(H264DecoderFrame * pFrame);

private:

};

} // namespace UMC

#endif // __UMC_H264_MFX_SW_SUPPLIER_H
#endif // MFX_ENABLE_H264_VIDEO_DECODE
