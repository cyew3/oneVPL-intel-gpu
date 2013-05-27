/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2009 Intel Corporation. All Rights Reserved.

File Name: mfx_mpeg2_bsd.h

\* ****************************************************************************** */
#ifndef __MFX_MPEG2_BSD_H__
#define __MFX_MPEG2_BSD_H__

#include "mfx_common.h"
#if defined (MFX_ENABLE_MPEG2_VIDEO_BSD)

#include "mfxvideo++int.h"
#include "umc_mpeg2_dec_base.h"

class VideoBSDMPEG2 : public VideoBSD
{
public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);

    VideoBSDMPEG2(VideoCORE *core, mfxStatus *sts);
    virtual ~VideoBSDMPEG2(void);

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
    mfxStatus FindAndParseSlice(UMC::Mpeg2Bitstream& bs, UMC::Mpeg2Slice& slice, mfxU32& headOffset, mfxU32& headSize);
    mfxStatus UpdateQMatrices(mfxFrameCUC& cuc);
    void PopulateMfxVideoParam(mfxVideoParam& par);
    void PopulateMfxFrameParam(mfxFrameParam& par);
    void PopulateMfxMbCode(const UMC::Mpeg2Mb& umcMb, mfxMbCode& mfxMb);
    void PopulateMfxMbCodeSkipped(const mfxMbCode& prevMb, mfxMbCode& mfxMb);

    VideoCORE *m_core;
    mfxVideoParam m_vPar;
    mfxFrameParam m_fPar;
    mfxSliceParam m_sPar;

    UMC::MPEG2VideoDecoderBase m_implUMC;
    UMC::Mpeg2SeqHead m_seqHead;
    UMC::Mpeg2GroupHead m_groupHead;
    UMC::Mpeg2PicHead m_picHead;
    UMC::Mpeg2Slice m_sliceHead;
    bool m_qMatrixChanged;
    bool m_bVlcTableInited;
};

#endif
#endif
