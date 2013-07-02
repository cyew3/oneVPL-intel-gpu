//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008-2013 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_ENCODE_H__
#define __MFX_H265_ENCODE_H__

#include "ippdefs.h"
#include "ippi.h"
#include "ippvc.h"
#include "float.h"
#include "math.h"

#include "mfxdefs.h"
#include "mfxvideo.h"
#include "mfxvideo++int.h"
#include "mfxsvc.h"
#include "mfx_ext_buffers.h"
//#include "umc_svc_brc.h"

#include "mfx_umc_alloc_wrapper.h"

class H265Encoder;
struct H265NALUnit;

class mfxVideoH265InternalParam : public mfxVideoParam
{
public:
    mfxVideoH265InternalParam();
    mfxVideoH265InternalParam(mfxVideoParam const &);

    mfxVideoH265InternalParam & operator = (mfxVideoParam const &);

    void SetCalcParams( mfxVideoParam *parMFX);

    void GetCalcParams( mfxVideoParam *parMFX);

    struct CalculableParam
    {
        mfxU32 BufferSizeInKB;
        mfxU32 InitialDelayInKB;
        mfxU32 TargetKbps;
        mfxU32 MaxKbps;
    } calcParam;
};

class MFXVideoENCODEH265 : public VideoENCODE {
public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);

    MFXVideoENCODEH265(VideoCORE *core, mfxStatus *status);
    virtual ~MFXVideoENCODEH265();
    virtual mfxStatus Init(mfxVideoParam *par);

    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);

    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat);
    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams);
    virtual mfxStatus EncodeFrame(mfxEncodeCtrl *ctrl, mfxEncodeInternalParams *pInternalParams, mfxFrameSurface1 *surface, mfxBitstream *bs);
    virtual mfxStatus CancelFrame(mfxEncodeCtrl * /*ctrl*/, mfxEncodeInternalParams * /*pInternalParams*/, mfxFrameSurface1 * /*surface*/, mfxBitstream * /*bs*/) {return MFX_ERR_UNSUPPORTED;}

protected:
// ------ mfx level
    VideoCORE *m_core;
    mfxVideoH265InternalParam            m_mfxVideoParam;
    mfxExtCodingOption                   m_mfxExtOpts;
    mfxExtCodingOptionHEVC               m_mfxHEVCOpts;
    H265Encoder *m_enc;

    mfxU32                 m_frameCountSync; // counter for sync. part
    mfxU32                 m_frameCount;     // counter for Async. part
    mfxU32                 m_frameCountBufferedSync;
    mfxU32                 m_frameCountBuffered;

//    mfxU32  m_totalBits;
    mfxU32  m_encodedFrames;
    bool    m_isInitialized;

// ------ init
    mfxStatus InitEnc();
    mfxStatus ResetEnc();
    mfxStatus CloseEnc();

//  Encoding
    mfxStatus Encode(mfxFrameSurface1 *surface, mfxBitstream *mfxBS);
};

#endif // __MFX_H265_ENCODE_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
