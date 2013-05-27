/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2009 Intel Corporation. All Rights Reserved.


\* ****************************************************************************** */
#ifndef __MFX_H264_DEC_H__
#define __MFX_H264_DEC_H__

#include "mfx_common.h"
#if defined (MFX_ENABLE_H264_VIDEO_DEC)

#include "mfxvideo++int.h"

#include "umc_video_decoder.h"
#include "mfx_umc_alloc_wrapper.h"

namespace UMC
{
   class MFXProTaskSupplier;
};

typedef UMC::MFXProTaskSupplier MFX_AVC_Decoder;

class MFXVideoDECH264 : public VideoDEC
{
public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);

    MFXVideoDECH264(VideoCORE *core, mfxStatus * sts);
    virtual ~MFXVideoDECH264(void);

    virtual mfxStatus Init(mfxVideoParam* par);
    virtual mfxStatus Reset(mfxVideoParam* par);
    virtual mfxStatus Close(void);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);
    virtual mfxStatus GetSliceParam(mfxSliceParam *par);

    virtual mfxStatus RunFrameFullDEC(mfxFrameCUC *cuc);
    virtual mfxStatus RunFramePredDEC(mfxFrameCUC *cuc);
    virtual mfxStatus RunFrameIQT(mfxFrameCUC *cuc, mfxU8 scan);
    virtual mfxStatus RunFrameILDB(mfxFrameCUC *cuc);
    virtual mfxStatus GetFrameRecon(mfxFrameCUC *cuc);

    virtual mfxStatus RunSliceFullDEC(mfxFrameCUC *cuc);
    virtual mfxStatus RunSlicePredDEC(mfxFrameCUC *cuc);
    virtual mfxStatus RunSliceIQT(mfxFrameCUC *cuc, mfxU8 scan);
    virtual mfxStatus RunSliceILDB(mfxFrameCUC *cuc);
    virtual mfxStatus GetSliceRecon(mfxFrameCUC *cuc);

protected:
    mfxStatus InternalReset(mfxVideoParam* par);

    VideoCORE *m_core;
    mfxVideoParam m_vPar;
    mfxFrameParam m_fPar;
    mfxSliceParam m_sPar;

    MFX_AVC_Decoder           *m_pH264VideoDecoder;
    mfx_UMC_MemAllocator       m_MemoryAllocator;

    UMC::BaseCodec            *m_pPostProcessing;
    UMC::VideoData             m_InternMediaDataOut;

};

#endif // MFX_ENABLE_H264_VIDEO_DEC
#endif // __MFX_H264_DEC_H__
