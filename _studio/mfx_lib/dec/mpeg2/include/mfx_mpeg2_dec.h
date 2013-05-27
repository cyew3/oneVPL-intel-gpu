/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2009 Intel Corporation. All Rights Reserved.

File Name: mfx_mpeg2_dec.h

\* ****************************************************************************** */
#ifndef __MFX_MPEG2_DEC_H__
#define __MFX_MPEG2_DEC_H__

#include "mfx_common.h"
#if defined (MFX_ENABLE_MPEG2_VIDEO_DEC)

#include "mfxvideo++int.h"
#include "umc_mpeg2_dec_base.h"
#include "mfx_mpeg2_dec_utils.h"

class MFXVideoDECMPEG2 : public VideoDEC
{
public:
    static mfxStatus Query(mfxVideoParam* in, mfxVideoParam* out);

    MFXVideoDECMPEG2(VideoCORE *core, mfxStatus *sts);
    virtual ~MFXVideoDECMPEG2(void);

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
    void PopulateUmcMb(UMC::Mpeg2Mb& mb, const mfxMbCode& mfxMb);

    VideoCORE *m_core;
    mfxVideoParam m_vPar;
    mfxFrameParam m_fPar;
    mfxSliceParam m_sPar;

    UMC::MPEG2VideoDecoderBase m_implUMC;
    MFXMPEG2_DECLARE_COUNTERS;
};

#endif
#endif
