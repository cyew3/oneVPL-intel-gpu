//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_MPEG2_ENCODE_HW_H__
#define __MFX_MPEG2_ENCODE_HW_H__

#include "mfx_common.h"


#if defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE)

#include "mfxvideo++int.h"
#include "mfx_mpeg2_encode_full_hw.h"
#include "mfx_mpeg2_encode_utils_hw.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_PAK) && defined (MFX_ENABLE_MPEG2_VIDEO_ENC)
#include "mfx_mpeg2_encode_hybrid_hw.h"
#endif

class MFXVideoENCODEMPEG2_HW : public VideoENCODE {
protected:

public:
    static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out)
    {   
        return MPEG2EncoderHW::ControllerBase::Query(core,in,out);
    }
    static mfxStatus QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)
    {
        return MPEG2EncoderHW::ControllerBase::QueryIOSurf(core,par,request);
    }
    MFXVideoENCODEMPEG2_HW(VideoCORE *core, mfxStatus *sts)
    {
        m_pCore = core;
        pEncoder = 0;
        *sts = MFX_ERR_NONE;
    }
    virtual ~MFXVideoENCODEMPEG2_HW() {Close();}
    virtual mfxStatus Init(mfxVideoParam *par)
    {
        mfxStatus     sts  = MFX_ERR_NONE;
        ENCODE_CAPS   Caps = {};
        MPEG2EncoderHW::HW_MODE  mode = MPEG2EncoderHW::UNSUPPORTED;
        if (pEncoder)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;        
        }
        sts = MPEG2EncoderHW::CheckHwCaps(m_pCore, par, 0, &Caps);
        MFX_CHECK_STS(sts);

        mode = MPEG2EncoderHW::GetHwEncodeMode(Caps);
        if (mode == MPEG2EncoderHW::FULL_ENCODE)
        {
            pEncoder = new MPEG2EncoderHW::FullEncode(m_pCore,&sts);        
        }
#if defined (MFX_ENABLE_MPEG2_VIDEO_PAK) && defined (MFX_ENABLE_MPEG2_VIDEO_ENC)
        else if (mode == MPEG2EncoderHW::HYBRID_ENCODE)
        {
            pEncoder = new MPEG2EncoderHW::HybridEncode(m_pCore,&sts);        
        }
#endif
        else
        {
            return MFX_WRN_PARTIAL_ACCELERATION;
        }
        sts = pEncoder->Init(par);
        if (sts == MFX_WRN_PARTIAL_ACCELERATION || sts < MFX_ERR_NONE)
        {
            Close();
            return sts;
        }
        return sts;
    }
    virtual mfxStatus Reset(mfxVideoParam *par)
    {
        if (!pEncoder)
        {
            return MFX_ERR_NOT_INITIALIZED;        
        }
        return pEncoder->Reset(par);
    }
    virtual mfxStatus Close(void)
    {
        mfxStatus sts = MFX_ERR_NONE;
        if (pEncoder)
        {
            sts = pEncoder->Close();
            delete pEncoder;
            pEncoder = 0;        
        }
        return sts;   
    }
    virtual mfxStatus GetVideoParam(mfxVideoParam *par)
    {
        if (!pEncoder)
        {
            return MFX_ERR_NOT_INITIALIZED;        
        }
        return pEncoder->GetVideoParam(par);    
    }
    virtual mfxStatus GetFrameParam(mfxFrameParam *)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat)
    {
        if (!pEncoder)
        {
            return MFX_ERR_NOT_INITIALIZED;        
        }
        return pEncoder->GetEncodeStat(stat);       
    }
    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, 
        mfxFrameSurface1 *surface, 
        mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, 
        mfxEncodeInternalParams *pInternalParams)
    {
        if (!pEncoder)
        {
            return MFX_ERR_NOT_INITIALIZED;        
        }
        return pEncoder->EncodeFrameCheck(ctrl,surface,bs,reordered_surface,pInternalParams);     
    }
    virtual mfxStatus EncodeFrame(mfxEncodeCtrl *ctrl, 
        mfxEncodeInternalParams *pInternalParams, 
        mfxFrameSurface1 *surface, 
        mfxBitstream *bs)
    {
        if (!pEncoder)
        {
            return MFX_ERR_NOT_INITIALIZED;        
        }
        return pEncoder->EncodeFrame(ctrl,pInternalParams,surface,bs);     
    }

    virtual mfxStatus CancelFrame(mfxEncodeCtrl *ctrl, 
        mfxEncodeInternalParams *pInternalParams, 
        mfxFrameSurface1 *surface, 
        mfxBitstream *bs)
    {
        if (!pEncoder)
        {
            return MFX_ERR_NOT_INITIALIZED;        
        }
        return pEncoder->CancelFrame(ctrl,pInternalParams,surface,bs);     
    }
    virtual
    mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl,
                               mfxFrameSurface1 *surface,
                               mfxBitstream *bs,
                               mfxFrameSurface1 **reordered_surface,
                               mfxEncodeInternalParams *pInternalParams,
                               MFX_ENTRY_POINT pEntryPoints[],
                               mfxU32 &numEntryPoints)
    {
        if (!pEncoder)
        {
            return MFX_ERR_NOT_INITIALIZED;        
        }
        return pEncoder->EncodeFrameCheck(ctrl,surface,bs,reordered_surface,pInternalParams,pEntryPoints,numEntryPoints); 
    }
    virtual
    mfxTaskThreadingPolicy GetThreadingPolicy(void) 
    {
        if (!pEncoder)
        {
            return MFX_TASK_THREADING_DEFAULT;        
        }
        return pEncoder->GetThreadingPolicy();
    }
protected:


private:
    VideoCORE*                      m_pCore;
    MPEG2EncoderHW::EncoderBase*    pEncoder;

};

#endif // MFX_ENABLE_MPEG2_VIDEO_ENCODE
#endif
