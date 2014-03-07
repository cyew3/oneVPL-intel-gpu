//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
//*/

#pragma once

#include <stdlib.h>
#include <memory>
#include <list>

#include "mfxplugin++.h"
#include "sample_defs.h"
#include "ffmpeg_port.h"
#include "ffmpeg_decoder_workload.h"

#define IS_PLUGIN_ASYNC() 0


class FFDecPlugin : public MFXDecoderPlugin
{
public:
    FFDecPlugin();
    ~FFDecPlugin();


    // methods to be called by Media SDK
    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus DecodeFrameSubmit(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out,  mfxThreadTask *task);
    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a);
    virtual mfxStatus FreeResources(mfxThreadTask task, mfxStatus sts);
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out);
    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close();
    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par);
    virtual mfxStatus SetAuxParams(void* , int ) {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus GetPayload(mfxU64 *, mfxPayload *) {
        return MFX_ERR_NONE;
    }
    virtual void Release() {
        delete this;
    }
    static MFXDecoderPlugin* Create() {
        return new FFDecPlugin();
    }
protected:
    bool m_bInited;



    mfxVideoParam   m_VideoParam;
    std::auto_ptr<MFXPluginParam>  m_PluginParam;

    std::list<FFDecWorkload> m_Tasks;
    mfxU32          m_MaxNumTasks;

    mfxStatus CheckParam(mfxVideoParam *mfxParam);
    mfxStatus CheckInOutFrameInfo(mfxFrameInfo *pIn, mfxFrameInfo *pOut);
    mfxU32 FindFreeTaskIdx();
    FFCodec m_ffDecoder;
    FFParser m_ffParser;
    //sharable context for caching swscaler contexts
    FFScaler m_scl;
    bool m_bIsOpaq;

    //core should be release after all active tasks
    MFXCoreInterface m_mfxCore;
};
