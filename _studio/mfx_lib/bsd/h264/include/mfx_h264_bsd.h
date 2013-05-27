/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2009 Intel Corporation. All Rights Reserved.
\* ****************************************************************************** */
#ifndef __MFX_H264_BSD_H__
#define __MFX_H264_BSD_H__

#include "mfx_common.h"
#if defined (MFX_ENABLE_H264_VIDEO_BSD)

#include "mfxvideo++int.h"

#include "umc_video_decoder.h"
#include "mfx_umc_alloc_wrapper.h"

namespace UMC
{
   class MFXProTaskSupplier;
};

typedef UMC::MFXProTaskSupplier MFX_AVC_Decoder;

class VideoBSDH264 : public VideoBSD
{
public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);

    VideoBSDH264(VideoCORE *core, mfxStatus *sts);
    virtual ~VideoBSDH264(void);

    virtual mfxStatus Init(mfxVideoParam* par);
    virtual mfxStatus Reset(mfxVideoParam* par);
    virtual mfxStatus Close(void);

    virtual mfxStatus GetVideoParam(mfxVideoParam*);
    virtual mfxStatus GetFrameParam(mfxFrameParam*);
    virtual mfxStatus GetSliceParam(mfxSliceParam*);

    virtual mfxStatus RunVideoParam(mfxBitstream *bs, mfxVideoParam *par);
    virtual mfxStatus RunFrameParam(mfxBitstream *bs, mfxFrameParam *par);
    virtual mfxStatus RunSliceParam(mfxBitstream *bs, mfxSliceParam *par);
    virtual mfxStatus ExtractUserData(mfxBitstream *bs, mfxU8 *ud, mfxU32 *sz, mfxU64 *ts);

    virtual mfxStatus RunSliceBSD(mfxFrameCUC *cuc);
    virtual mfxStatus RunSliceMFX(mfxFrameCUC *cuc);
    virtual mfxStatus RunFrameBSD(mfxFrameCUC *cuc);
    virtual mfxStatus RunFrameMFX(mfxFrameCUC *cuc);

protected:
    mfxStatus InternalReset(mfxVideoParam* par);

    VideoCORE *m_core;
    mfxVideoParam m_vPar;
    mfxFrameParam m_fPar;
    mfxSliceParam m_sPar;
    bool m_qMatrixChanged;

    MFX_AVC_Decoder           *m_pH264VideoDecoder;
    mfx_UMC_MemAllocator       m_MemoryAllocator;

    UMC::BaseCodec            *m_pPostProcessing;
    UMC::VideoData             m_InternMediaDataOut;
};

#endif // MFX_ENABLE_H264_VIDEO_BSD
#endif // __MFX_H264_BSD_H__
