/*//////////////////////////////////////////////////////////////////////////////
////                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"

#ifndef _MFX_H264_PAK_H_
#define _MFX_H264_PAK_H_

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_H264_VIDEO_FEI_PAK)

#include "mfx_pak_ext.h"
#include "mfx_h264_encode_hw_utils.h"
#include "mfxfei.h"
#include "mfxpak.h"

#include <memory>
#include <list>
#include <vector>

using namespace MfxHwH264Encode;

namespace MfxEncPAK
{

}; // namespace

class mfxFeiPAKEncodeInternalParams : public mfxEncodeInternalParams
{
public:
     mfxPAKInput *in;
     mfxPAKOutput *out;
};

bool bEnc_PAK(mfxVideoParam *par);

class VideoPAK_PAK:  public VideoPAK_Ext
{
public:

     VideoPAK_PAK(VideoCORE *core, mfxStatus *sts);

    // Destructor
    virtual
    ~VideoPAK_PAK(void);

    virtual
    mfxStatus Init(mfxVideoParam *par) ;
    virtual
    mfxStatus Reset(mfxVideoParam *par);
    virtual
    mfxStatus Close(void);
    virtual
    mfxTaskThreadingPolicy GetThreadingPolicy(void) {return MFX_TASK_THREADING_DEDICATED_WAIT;}

    mfxStatus Submit(mfxEncodeInternalParams * iParams);    
    
    mfxStatus Query(DdiTask& task);

    static 
    mfxStatus QueryIOSurf(VideoCORE*, mfxVideoParam *par, mfxFrameAllocRequest *request);

    virtual
    mfxStatus GetVideoParam(mfxVideoParam *par);
    
    virtual
    mfxStatus RunFramePAKCheck(  mfxPAKInput *in,
                                    mfxPAKOutput *out,
                                    MFX_ENTRY_POINT pEntryPoints[],
                                    mfxU32 &numEntryPoints);
    virtual
    mfxStatus RunFramePAK(mfxPAKInput *in, mfxPAKOutput *out);

    virtual
    mfxStatus RunSeqHeader(mfxFrameCUC *cuc);
private:

    bool                    m_bInit;
    VideoCORE*              m_core;

private:
    std::list <mfxFrameSurface1*>               m_SurfacesForOutput;


private:
    std::auto_ptr<DriverEncoder> m_ddi;
    std::vector<mfxU32>     m_recFrameOrder; // !!! HACK !!!
    ENCODE_CAPS m_caps;

    MfxHwH264Encode::MfxVideoParam                  m_video;
    PreAllocatedVector m_sei;
        
    MfxHwH264Encode::MfxFrameAllocResponse          m_rec;
    MfxHwH264Encode::MfxFrameAllocResponse          m_opaqHren;
    
    std::list<DdiTask> m_free;
    std::list<DdiTask> m_incoming;
    std::list<DdiTask> m_encoding;
    DdiTask m_prevTask;
    UMC::Mutex m_listMutex; 
    
    mfxU32 m_inputFrameType;
    eMFXHWType m_currentPlatform;
    eMFXVAType m_currentVaType;
    mfxU32 m_singleFieldProcessingMode;
    mfxU32 m_firstFieldDone;
};

#endif
#endif

