/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2019 Intel Corporation. All Rights Reserved.

File Name: test_thread_safety_decode.h

\* ****************************************************************************** */

#ifndef __TEST_THREAD_SAFETY_DECODE_H__
#define __TEST_THREAD_SAFETY_DECODE_H__

#include "shared_utils.h"
#include "mfx_pipeline_dec.h"
#include "mfx_decode_pipeline_config.h"
#include "mfx_factory_default.h"
#include "test_thread_safety.h"
#include "test_thread_safety_output.h"

//TODO: remove this
class DecodePipelineForTestThreadSafety
    : public MFXDecPipeline
{
public:
    DecodePipelineForTestThreadSafety(IMFXPipelineFactory *pFactory);
    // override function to build-in instrumented render
    virtual mfxStatus CreateRender();
};

class ConfigForTestThreadSafetyDecode
    : public MFXPipelineConfigDecode
{
public:
    ConfigForTestThreadSafetyDecode(int argc, vm_char ** argv, std::mutex *pSync)
        : MFXPipelineConfigDecode(argc, argv)
        , m_pSync(pSync)
    {
    }

    // TODO:override function to build-in instrumented render
    //virtual IMFXPipelineFactory * CreateFactory();
    virtual IMFXPipeline * CreatePipeline();
    virtual std::mutex * GetExternalSync() override {return m_pSync;}

protected:
    std::mutex *m_pSync;
};

//TODO: implement this
class FactoryForThreadSafetyDecode
    : public MFXPipelineFactory
{
public:
    virtual IMFXVideoRender * CreateRender( IPipelineObjectDesc * pParams);
};

class OutputYuvTester : public MFXVideoRender
{
public:
    OutputYuvTester(IVideoSession *core, mfxStatus *status);
    virtual ~OutputYuvTester() { Close(); }
    virtual mfxStatus Close();
    virtual mfxStatus Init(mfxVideoParam *par, const vm_char *);
    virtual mfxStatus RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl *ctrl = 0);

private:
    friend class UnlockOnExit;

    mfxVideoParam m_video;
    mfxHDL m_handle;
};

#endif //__TEST_THREAD_SAFETY_DECODE_H__
