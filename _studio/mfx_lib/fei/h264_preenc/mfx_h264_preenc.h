//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#ifndef _MFX_H264_PREENC_H_
#define _MFX_H264_PREENC_H_

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)

#include "mfx_enc_ext.h"
#include "mfx_h264_encode_hw_utils.h"
#include "mfxfei.h"

#include <memory>
#include <list>
#include <vector>

bool bEnc_PREENC(mfxVideoParam *par);

class VideoENC_PREENC:  public VideoENC_Ext
{
public:

     VideoENC_PREENC(VideoCORE *core, mfxStatus *sts);

    // Destructor
    virtual ~VideoENC_PREENC(void);

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);
    virtual mfxTaskThreadingPolicy GetThreadingPolicy(void) {return MFX_TASK_THREADING_DEDICATED_WAIT;}

    mfxStatus Submit(mfxEncodeInternalParams * iParams);

    mfxStatus QueryStatus(MfxHwH264Encode::DdiTask& task);

    static mfxStatus Query(VideoCORE*, mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(VideoCORE*, mfxVideoParam *par, mfxFrameAllocRequest *request);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);

    virtual mfxStatus RunFrameVmeENCCheck(mfxENCInput  *in,
                                          mfxENCOutput *out,
                                          MFX_ENTRY_POINT pEntryPoints[],
                                          mfxU32 &numEntryPoints);

    virtual mfxStatus RunFrameVmeENC(mfxENCInput *in, mfxENCOutput *out);

private:

    bool                                          m_bInit;
    VideoCORE*                                    m_core;

    std::list <mfxFrameSurface1*>                 m_SurfacesForOutput;

    std::auto_ptr<MfxHwH264Encode::DriverEncoder> m_ddi;
    ENCODE_CAPS                                   m_caps;

    MfxHwH264Encode::MfxVideoParam                m_video;
    MfxHwH264Encode::PreAllocatedVector           m_sei;

    MfxHwH264Encode::MfxFrameAllocResponse        m_raw;
    MfxHwH264Encode::MfxFrameAllocResponse        m_opaqHren;

    std::list<MfxHwH264Encode::DdiTask>           m_free;
    std::list<MfxHwH264Encode::DdiTask>           m_incoming;
    std::list<MfxHwH264Encode::DdiTask>           m_encoding;
    UMC::Mutex                                    m_listMutex;

    mfxU32                                        m_inputFrameType;
    eMFXHWType                                    m_currentPlatform;
    eMFXVAType                                    m_currentVaType;
    mfxU32                                        m_singleFieldProcessingMode;
    mfxU32                                        m_firstFieldDone;
};

#endif
#endif

