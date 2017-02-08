//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2017 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_H264_PAK_H_
#define _MFX_H264_PAK_H_

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_H264_VIDEO_FEI_PAK)

#include "fei_common.h"
#include "mfx_pak_ext.h"
//#include "mfxpak.h"
//#include "mfx_h264_encode_hw_utils.h"


//using namespace MfxHwH264Encode;
/*
namespace MfxEncPAK
{

}; // namespace

class mfxFeiPAKEncodeInternalParams : public mfxEncodeInternalParams
{
public:
     mfxPAKInput  *in;
     mfxPAKOutput *out;
};
*/
bool bEnc_PAK(mfxVideoParam *par);

class VideoPAK_PAK:  public VideoPAK_Ext
{
public:

     VideoPAK_PAK(VideoCORE *core, mfxStatus *sts);

    // Destructor
    virtual ~VideoPAK_PAK(void);

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);
    virtual mfxTaskThreadingPolicy GetThreadingPolicy(void) { return MFX_TASK_THREADING_DEDICATED_WAIT; }

    mfxStatus Submit(mfxEncodeInternalParams * iParams);

    mfxStatus Query(MfxHwH264Encode::DdiTask& task);

    static mfxStatus QueryIOSurf(VideoCORE*, mfxVideoParam *par, mfxFrameAllocRequest *request);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);

    virtual mfxStatus RunFramePAKCheck(mfxPAKInput  *in,
                                       mfxPAKOutput *out,
                                       MFX_ENTRY_POINT pEntryPoints[],
                                       mfxU32 &numEntryPoints);

    virtual mfxStatus RunFramePAK(mfxPAKInput *in, mfxPAKOutput *out);

private:

    bool                                          m_bInit;
    VideoCORE*                                    m_core;

    std::list <mfxFrameSurface1*>                 m_SurfacesForOutput;

    std::auto_ptr<MfxHwH264Encode::DriverEncoder> m_ddi;
    std::vector<mfxU32>                           m_recFrameOrder; // !!! HACK !!!
    ENCODE_CAPS                                   m_caps;

    MfxHwH264Encode::MfxVideoParam                m_video;
    MfxHwH264Encode::PreAllocatedVector           m_sei;

    MfxHwH264Encode::MfxFrameAllocResponse        m_rec;
    MfxHwH264Encode::MfxFrameAllocResponse        m_opaqHren;

    std::list<MfxHwH264Encode::DdiTask>           m_free;
    std::list<MfxHwH264Encode::DdiTask>           m_incoming;
    std::list<MfxHwH264Encode::DdiTask>           m_encoding;
    MfxHwH264Encode::DdiTask                      m_prevTask;
    UMC::Mutex                                    m_listMutex;

    mfxU32                                        m_inputFrameType;
    eMFXHWType                                    m_currentPlatform;
    eMFXVAType                                    m_currentVaType;
    mfxU32                                        m_singleFieldProcessingMode;
    mfxU32                                        m_firstFieldDone;
};

#endif
#endif

