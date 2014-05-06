/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_encode_pipeline_config.h"
#include "mfx_ip_field_pair_disable_encoder.h"

SUITE(ip_field_disabler)
{
    TEST(within_pipline_cmd_line)
    {
        PipelineRunner<MFXPipelineConfigEncode>::CmdLineParams params;
        params[VM_STRING("-disable_ip_field_pair")].empty();

        PipelineRunner<MFXPipelineConfigEncode> pipeline;
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params));

        //TODO: add more checks
        //CHECK_EQUAL(32u, TimeoutVal<>::val());
    }

    TEST(frame_type)
    {
        MockVideoEncode *pMock = new MockVideoEncode();
        std::auto_ptr<IVideoEncode> pEnc (pMock);
        IPFieldPairDisableEncode ipfield_pair_enc(pEnc);

        mfxVideoParam vParamMfx = {0};
        vParamMfx.mfx.GopPicSize = 3;

        TEST_METHOD_TYPE(MockVideoEncode::GetVideoParam) vParam;
        vParam.ret_val = MFX_ERR_NONE;
        vParam.value0  = &vParamMfx;
        pMock->TEST_METHOD_NAME(GetVideoParam).WillReturn(vParam);

        CHECK_EQUAL(MFX_ERR_NONE, ipfield_pair_enc.Init(&vParamMfx));
        mfxFrameSurface1 srf = {0};
        TEST_METHOD_TYPE(MockVideoEncode::EncodeFrameAsync) efaParams;

        //0 = I/xI
        CHECK_EQUAL(MFX_ERR_NONE, ipfield_pair_enc.EncodeFrameAsync(NULL, &srf, NULL, NULL));
        CHECK(pMock->TEST_METHOD_NAME(EncodeFrameAsync).WasCalled(&efaParams));
        CHECK_EQUAL(MFX_FRAMETYPE_xI | MFX_FRAMETYPE_I, efaParams.value0->FrameType);
        //1 = unknown
        CHECK_EQUAL(MFX_ERR_NONE, ipfield_pair_enc.EncodeFrameAsync(NULL, &srf, NULL, NULL));
        CHECK(pMock->TEST_METHOD_NAME(EncodeFrameAsync).WasCalled(&efaParams));
        CHECK_EQUAL((mfxEncodeCtrl*)NULL, efaParams.value0);
        //2 = unknown
        CHECK_EQUAL(MFX_ERR_NONE, ipfield_pair_enc.EncodeFrameAsync(NULL, &srf, NULL, NULL));
        CHECK(pMock->TEST_METHOD_NAME(EncodeFrameAsync).WasCalled(&efaParams));
        CHECK_EQUAL((mfxEncodeCtrl*)NULL, efaParams.value0);
        //3 = I/xI
        CHECK_EQUAL(MFX_ERR_NONE, ipfield_pair_enc.EncodeFrameAsync(NULL, &srf, NULL, NULL));
        CHECK(pMock->TEST_METHOD_NAME(EncodeFrameAsync).WasCalled(&efaParams));
        CHECK_EQUAL(MFX_FRAMETYPE_xI | MFX_FRAMETYPE_I, efaParams.value0->FrameType);
    }
}