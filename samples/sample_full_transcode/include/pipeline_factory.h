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

#include "sample_utils.h"
#include "mfxvideo++.h"
#include "mfxaudio++.h"
#include "base_allocator.h"
#include "cmdline_parser.h"
#include "transform_storage.h"
#include "transform_vdec.h"
#include "transform_venc.h"
#include "transform_adec.h"
#include "samples_pool.h"
#include "pipeline_profile.h"
#include "mfxsplmux++.h"
#include "mfxplugin++.h"
#include "session_storage.h"
#include "isink.h"
#include "splitter_wrapper.h"
#include "circular_splitter.h"
#include "muxer_wrapper.h"
#include "hw_device.h"

class PipelineManager;

class PipelineFactory
{
public:
    PipelineFactory(void);
    virtual ~PipelineFactory(void);

    virtual MFXVideoSessionExt*      CreateVideoSession();
    virtual MFXAudioSession*         CreateAudioSession();

    //MFX native objects to be wrapped into transforms
    //TODO: deprecate this
    virtual MFXVideoDECODE*          CreateVideoDecoder(MFXVideoSessionExt&);
    virtual ITransform*              CreateVideoDecoderTransform(MFXVideoSessionExt&, int timeout);

    //TODO: deprecate this
    virtual MFXVideoENCODE*          CreateVideoEncoder(MFXVideoSessionExt&);
    virtual ITransform*              CreateVideoEncoderTransform(MFXVideoSessionExt&, int timeout);

    virtual MFXVideoVPP*             CreateVideoVPP(MFXVideoSessionExt&);
    virtual ITransform*              CreateVideoVPPTransform(MFXVideoSessionExt&, int timeout);

    //TODO: deprecate this
    virtual MFXAudioDECODE*          CreateAudioDecoder(MFXAudioSession&);
    virtual ITransform*              CreateAudioDecoderTransform(MFXAudioSession&, int timeout);
    virtual ITransform*              CreateAudioDecoderNullTransform(MFXAudioSession&, int timeout);

    virtual MFXVideoUSER*            CreateVideoUserModule(MFXVideoSessionExt&);
    //virtual MFXPlugin*               CreatePlugin(mfxPluginType , MFXVideoUSER *, const msdk_string & pluginFullPath);
    virtual ITransform*              CreatePluginTransform(MFXVideoSessionExt&, mfxPluginType , const msdk_string & pluginFullPath, int);

    virtual MFXMuxer*                CreateMuxer();
    virtual MuxerWrapper*            CreateMuxerWrapper(MFXStreamParamsExt &, std::auto_ptr<MFXDataIO>& );

    virtual MFXAudioENCODE*          CreateAudioEncoder(MFXAudioSession&);
    virtual ITransform*              CreateAudioEncoderTransform(MFXAudioSession&, int timeout);
    virtual ITransform*              CreateAudioEncoderNullTransform(MFXAudioSession&, int timeout);

    //TODO: deprecate this
    virtual MFXSplitter*             CreateSplitter();
    virtual ISplitterWrapper*        CreateSplitterWrapper(std::auto_ptr<MFXDataIO>& );
    virtual ISplitterWrapper*        CreateCircularSplitterWrapper(std::auto_ptr<MFXDataIO>& , mfxU64 nLimit);

    virtual BaseFrameAllocator*      CreateFrameAllocator(AllocatorImpl );

    //TODO: deprecate this
    virtual MFXDataIO*               CreateFileIO(const msdk_string&, const msdk_string&);
    virtual ISink*                   CreateFileIOWrapper(const msdk_string&, const msdk_string&);

    virtual CmdLineParser*           CreateCmdLineParser();
    virtual SamplePool*              CreateSamplePool(int);
    virtual TransformStorage*        CreateTransformStorage();

    virtual IPipelineProfile*        CreatePipelineProfile(const MFXStreamParams &splInfo, CmdLineParser & parser);
    virtual SessionStorage *         CreateSessionStorage(const MFXSessionInfo &vSession, const MFXSessionInfo &aSession);

    virtual CHWDevice*               CreateHardwareDevice(AllocatorImpl );
    virtual mfxAllocatorParams*      CreateAllocatorParam(CHWDevice*, AllocatorImpl);
};
