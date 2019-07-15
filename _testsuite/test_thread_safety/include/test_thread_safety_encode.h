/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2019 Intel Corporation. All Rights Reserved.

File Name: test_thread_safety_encode.h

\* ****************************************************************************** */

#ifndef __TEST_THREAD_SAFETY_ENCODE_H__
#define __TEST_THREAD_SAFETY_ENCODE_H__

#include "shared_utils.h"
#include "mfx_pipeline_transcode.h"
#include "mfx_encode_pipeline_config.h"
#include "mfx_factory_default.h"
#include "test_thread_safety.h"
#include "test_thread_safety_output.h"

class EncodePipelineForTestThreadSafety : public MFXTranscodingPipeline
{
public:
    EncodePipelineForTestThreadSafety(IMFXPipelineFactory *pFactory);
protected:
    // override function to build-in instrumented render
    virtual mfxStatus CreateRender();
};

class ConfigForTestThreadSafetyEncode
    : public MFXPipelineConfigEncode
{
public:
    ConfigForTestThreadSafetyEncode(int argc, vm_char ** argv, std::mutex *pSync)
        : MFXPipelineConfigEncode(argc, argv)
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
class FactoryForThreadSafetyEncode
    : public MFXPipelineFactory
{
public:
    virtual IMFXVideoRender * CreateRender( IPipelineObjectDesc * pParams);
};


class OutputBitstreamTester : public MFXEncodeWRAPPER
{
public:
    OutputBitstreamTester(ComponentParams &refParams, mfxStatus *status, std::unique_ptr<IVideoEncode> &&pEncode);
    virtual ~OutputBitstreamTester() { Close(); }
    virtual mfxStatus Init(mfxVideoParam *par, const vm_char *);
    virtual mfxStatus Close();

protected:
    virtual mfxStatus PutBsData(mfxBitstream *pBS);

private:
    mfxVideoParam m_video;
    mfxHDL m_handle;
};

#endif //__TEST_THREAD_SAFETY_ENCODE_H__
