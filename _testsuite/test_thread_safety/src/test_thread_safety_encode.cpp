/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "test_thread_safety.h"
#if (defined ENABLE_TEST_THREAD_SAFETY_FOR_H264_ENCODE) || (defined ENABLE_TEST_THREAD_SAFETY_FOR_MPEG2_ENCODE) || (defined ENABLE_TEST_THREAD_SAFETY_FOR_VC1_ENCODE)

#include "vm_types.h"
#include "test_thread_safety_encode.h"
#include "mfx_default_pipeline_mgr.h"
#include "mfx_encode.h"
#include "mfx_file_generic.h"

IMFXPipeline * ConfigForTestThreadSafetyEncode::CreatePipeline()
{
    return new EncodePipelineForTestThreadSafety(new MFXPipelineFactory());
}

EncodePipelineForTestThreadSafety::EncodePipelineForTestThreadSafety(IMFXPipelineFactory *pFactory)
    : MFXTranscodingPipeline(pFactory)
{
}

mfxStatus EncodePipelineForTestThreadSafety::CreateRender()
{
    mfxStatus sts = MFX_ERR_NONE;
    std::auto_ptr<IVideoEncode> pEncoder = CreateEncoder();

    m_pRender = new OutputBitstreamTester(m_components[eREN], &sts, pEncoder);
    MFX_CHECK_STS(sts);
    
    return DecorateRender();
}

OutputBitstreamTester::OutputBitstreamTester(ComponentParams &refParams, mfxStatus *status, std::auto_ptr<IVideoEncode> &pEncode)
: MFXEncodeWRAPPER(refParams, status, pEncode)
, m_handle(0)
{
    if (status)
    {
        *status = MFX_ERR_NONE;
    }
}

mfxStatus OutputBitstreamTester::Init(mfxVideoParam *par, const vm_char *)
{
    MFX_CHECK_STS(MFXEncodeWRAPPER::Init(par, VM_STRING("")));

    m_video = *par;
    m_handle = outReg->Register();
    return MFX_ERR_NONE;
}

mfxStatus OutputBitstreamTester::PutBsData(mfxBitstream *pBS)
{
    if (NULL == pBS)
        return MFX_ERR_NONE;

    if (outReg->CommitData(m_handle, pBS->Data + pBS->DataOffset, pBS->DataLength) < 0)
        return MFX_ERR_UNKNOWN;

    return MFX_ERR_NONE;
}

mfxStatus OutputBitstreamTester::Close()
{
    if (m_handle != 0)
    {
        outReg->UnRegister(m_handle);
        m_handle = 0;
    }

    return MFXEncodeWRAPPER::Close();
}

mfxI32 RunEncode(mfxI32 argc, vm_char** argv, IPipelineSynhro *pExternalSync)
{
    std::auto_ptr<IMFXPipelineConfig> cfg(new ConfigForTestThreadSafetyEncode(argc, argv, pExternalSync));
    MFXPipelineManager defMgr;

    return defMgr.Execute(cfg.get());
}

#else //ENABLE_...

int RunDecode(int, vm_char**) { return 1; }

#endif //ENABLE_...
