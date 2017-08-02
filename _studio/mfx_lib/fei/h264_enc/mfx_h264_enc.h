//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2017 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_H264_FEI_ENC_H_
#define _MFX_H264_FEI_ENC_H_

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_H264_VIDEO_FEI_ENC)

#include "fei_common.h"
#include "mfx_enc_ext.h"

bool bEnc_ENC(mfxVideoParam *par);

class VideoENC_ENC :  public VideoENC_Ext
{
public:

     VideoENC_ENC(VideoCORE *core, mfxStatus *sts);

    // Destructor
    virtual ~VideoENC_ENC(void);

    virtual mfxStatus Init(mfxVideoParam *par) ;
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);
    virtual mfxTaskThreadingPolicy GetThreadingPolicy(void) {return MFX_TASK_THREADING_DEDICATED;}

    mfxStatus Submit(mfxEncodeInternalParams * iParams);

    mfxStatus QueryStatus(MfxHwH264Encode::DdiTask& task);

    static
    mfxStatus QueryIOSurf(VideoCORE*, mfxVideoParam *par, mfxFrameAllocRequest *request);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);

    virtual mfxStatus RunFrameVmeENCCheck(mfxENCInput *in,
                                    mfxENCOutput *out,
                                    MFX_ENTRY_POINT pEntryPoints[],
                                    mfxU32 &numEntryPoints);

    virtual mfxStatus RunFrameVmeENC(mfxENCInput *in, mfxENCOutput *out);

protected:
    mfxStatus ProcessAndCheckNewParameters(MfxHwH264Encode::MfxVideoParam & newPar,
                                           bool & isIdrRequired,
                                           mfxVideoParam const * newParIn = 0);

private:

    bool                                          m_bInit;
    VideoCORE*                                    m_core;

    std::auto_ptr<MfxHwH264Encode::DriverEncoder> m_ddi;
    std::vector<mfxU32>                           m_recFrameOrder; // !!! HACK !!!
    ENCODE_CAPS                                   m_caps;

    MfxHwH264Encode::MfxVideoParam                m_video;
    MfxHwH264Encode::MfxVideoParam                m_videoInit; // m_video may change by Reset, m_videoInit doesn't change
    MfxHwH264Encode::PreAllocatedVector           m_sei;

    MfxHwH264Encode::MfxFrameAllocResponse        m_rec;

    std::list<MfxHwH264Encode::DdiTask>           m_free;
    std::list<MfxHwH264Encode::DdiTask>           m_incoming;
    std::list<MfxHwH264Encode::DdiTask>           m_encoding;
    MfxHwH264Encode::DdiTask                      m_prevTask;
    UMC::Mutex                                    m_listMutex;

    mfxU32                                        m_inputFrameType;
    eMFXHWType                                    m_currentPlatform;
    eMFXVAType                                    m_currentVaType;
    bool                                          m_bSingleFieldMode;
    mfxU32                                        m_firstFieldDone;

#if MFX_VERSION < 1023
    std::map<mfxU32, mfxU32>                      m_frameOrder_frameNum;
#endif // MSDK_API < 1023
};

#endif
#endif

