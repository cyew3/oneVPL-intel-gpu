// Copyright (c) 2008-2018 Intel Corporation
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

#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_ENC)

#ifndef _MFX_H264_ENC_ENC_H_
#define _MFX_H264_ENC_ENC_H_

#include "mfxvideo++int.h"
#include "mfxvideo.h"
#include "mfx_umc_alloc_wrapper.h"
#include "umc_h264_video_encoder.h"
#include "umc_h264_core_enc.h"

#define H264MFX_UNREFERENCED_PARAMETER(X) X
#define MAX_FRAMES 16

class MFXVideoEncH264 : public VideoENC {
public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);

    MFXVideoEncH264(VideoCORE *core, mfxStatus *status);
    virtual ~MFXVideoEncH264();

    mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Close();

    virtual mfxStatus Reset(mfxVideoParam *par);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);
    virtual mfxStatus GetSliceParam(mfxSliceParam *par);
    virtual mfxStatus GetFrameResid(mfxFrameCUC *cuc)      {H264MFX_UNREFERENCED_PARAMETER(cuc); return MFX_ERR_UNSUPPORTED;}
    virtual mfxStatus GetSliceResid(mfxFrameCUC *cuc)      {H264MFX_UNREFERENCED_PARAMETER(cuc); return MFX_ERR_UNSUPPORTED;}

    virtual mfxStatus RunFrameFullENC(mfxFrameCUC *cuc)    {H264MFX_UNREFERENCED_PARAMETER(cuc); return MFX_ERR_UNSUPPORTED;}
    virtual mfxStatus RunSliceFullENC(mfxFrameCUC *cuc)    {H264MFX_UNREFERENCED_PARAMETER(cuc); return MFX_ERR_UNSUPPORTED;}
    virtual mfxStatus RunFrameVmeENC(mfxFrameCUC *cuc);
    virtual mfxStatus RunSliceVmeENC(mfxFrameCUC *cuc);
    virtual mfxStatus RunFramePredENC(mfxFrameCUC *cuc)    {H264MFX_UNREFERENCED_PARAMETER(cuc); return MFX_ERR_UNSUPPORTED;}
    virtual mfxStatus RunSlicePredENC(mfxFrameCUC *cuc)    {H264MFX_UNREFERENCED_PARAMETER(cuc); return MFX_ERR_UNSUPPORTED;}
    virtual mfxStatus RunFrameFTQ(mfxFrameCUC *cuc)        {H264MFX_UNREFERENCED_PARAMETER(cuc); return MFX_ERR_UNSUPPORTED;}
    virtual mfxStatus RunSliceFTQ(mfxFrameCUC *cuc)        {H264MFX_UNREFERENCED_PARAMETER(cuc); return MFX_ERR_UNSUPPORTED;}

protected:
    H264CoreEncoder_8u16s* enc; //Pointer to encoder instance

    H264EncoderFrame_8u16s* m_refs[MAX_FRAMES];
    mfxU8                  m_InitFlag;
    mfxVideoParam          m_VideoParam;
    mfxFrameParam          m_FrameParam;
    mfxSliceParam          m_SliceParam;
    mfx_UMC_MemAllocator   m_Allocator;
    VideoCORE*             m_pMFXCore;
    VideoBRC*              m_BRC;
    UMC::Status            m_StsUMC;

    mfxStatus              ENC_Slice(mfxFrameCUC *cuc, Ipp32s slice_num, H264Slice_8u16s *curr_slice, Ipp32s level);
    mfxStatus              ENC_Frame(mfxFrameCUC *cuc, Ipp32s level);

    mfxStatus AllocateFrames( mfxVideoParam* parMFX );
    void      DeallocateFrames();
    void      SetCurrentFrameParameters( mfxFrameCUC* cuc );
    void      ConvertCUC_ToInternalStructure( mfxFrameCUC *cuc );
};

MFXVideoEncH264* CreateMFXVideoEncH264(VideoCORE *core, mfxStatus *status);

#endif //_MFX_H264_ENC_ENC_H_

#endif //MFX_ENABLE_H264_VIDEO_ENCODER
