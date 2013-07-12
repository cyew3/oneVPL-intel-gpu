/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_pipeline_transcode.h"
#include "mock_mfx_pipeline_factory.h"
#include "mfx_encode_pipeline_config.h"
#include "mock_mfx_pipeline_config.h"
#include "mock_mfx_vpp.h"
#include "mock_icurrent_frame_control.h"
#include "mfx_encoded_frameInfo_encoder.h"


SUITE(mfxExtAvcFrameInfoSuite)
{
    struct MFXTranscodingPipeline2 : public MFXTranscodingPipeline
    {
        IVideoEncode *m_pEncode;
        MFXTranscodingPipeline2(IMFXPipelineFactory * pFac, IVideoEncode *pEncode)
            : MFXTranscodingPipeline(pFac)
            , m_pEncode (pEncode)
        {
        }
        
        mfxStatus GetEncoder(IVideoEncode **pEncode) {
            *pEncode = m_pEncode;
            return MFX_ERR_NONE;
        }
        mfxStatus ProcessTrickCommands()
        {
            return MFXTranscodingPipeline::ProcessTrickCommands();
        }
        std::list<ICommandActivator*>& GetCommands() {
            return m_commands;
        }
        
        DECLARE_TEST_METHOD0(mfxU32, GetNumDecodedFrames) {
            TEST_METHOD_TYPE(GetNumDecodedFrames) result_val;
            if (!_GetNumDecodedFrames.GetReturn(result_val)) {
                return 0;
            }
            return result_val.ret_val;
        }
    };

    struct mfxExtAVCEncodedFrameInfoTest {
        MockPipelineFactory mock_factory;
        PipelineRunner<MockPipelineConfig>::CmdLineParams params;
        MockPipelineConfig mock_cfg;
        MockVideoEncode mock_encode;
        MFXTranscodingPipeline2 *pPipeline2;
        PipelineRunner<MockPipelineConfig> pipeline;

        TEST_METHOD_TYPE(MockVideoEncode::QueryInterface) vParam;
        MockCurrentFrameControl mock_control;
        TEST_METHOD_TYPE(MockCurrentFrameControl::AddExtBuffer) buffer_to_add;
        TEST_METHOD_TYPE(MFXTranscodingPipeline2::GetNumDecodedFrames) current_frame;

        mfxExtAVCEncodedFrameInfoTest () {
            TEST_METHOD_TYPE(MockPipelineConfig::CreatePipeline) ret_params_pipeline_cfg;

            pPipeline2 = new MFXTranscodingPipeline2(MakeUndeletable(mock_factory), &mock_encode);
            ret_params_pipeline_cfg.ret_val = pPipeline2;

            mock_cfg._CreatePipeline.WillReturn(ret_params_pipeline_cfg);

            vParam.value1 = (void*)&mock_control;
            mock_encode._QueryInterface.WillDefaultReturn(&vParam);
        }
        void VerifyAttached() {
            CHECK_EQUAL(MFX_ERR_NONE, pPipeline2->ProcessTrickCommands());
            CHECK(mock_control._AddExtBuffer.WasCalled(&buffer_to_add));
            CHECK_EQUAL((mfxU32)MFX_EXTBUFF_ENCODED_FRAME_INFO, buffer_to_add.value0.BufferId);
        }

        void VerifyNOTAttached() {
            CHECK_EQUAL(MFX_ERR_NONE, pPipeline2->ProcessTrickCommands());
            CHECK_EQUAL(false, mock_control._AddExtBuffer.WasCalled(&buffer_to_add));
        }
        void VerifyDetached() {
            TEST_METHOD_TYPE(MockCurrentFrameControl::RemoveExtBuffer) buffer_to_remove;

            CHECK_EQUAL(MFX_ERR_NONE, pPipeline2->ProcessTrickCommands());
            CHECK(mock_control._RemoveExtBuffer.WasCalled(&buffer_to_remove));
            CHECK_EQUAL((mfxU32)MFX_EXTBUFF_ENCODED_FRAME_INFO, buffer_to_remove.value0);
        }
    };

    TEST_FIXTURE(mfxExtAVCEncodedFrameInfoTest, no_params)
    {
        params[VM_STRING("-enc_frame_info")] =  std::vector<tstring>();

        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params, &mock_cfg));

        std::list<ICommandActivator*> &cmds = pPipeline2->GetCommands();
        CHECK_EQUAL(1u, cmds.size());
    }

    TEST_FIXTURE(mfxExtAVCEncodedFrameInfoTest, no_params_not_last_parameter)
    {
        params[VM_STRING("-enc_frame_info")] =  std::vector<tstring>();
        params[VM_STRING("-h264")] =  std::vector<tstring>();

        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params, &mock_cfg));

        std::list<ICommandActivator*> &cmds = pPipeline2->GetCommands();
        CHECK_EQUAL(1u, cmds.size());
    }

    TEST_FIXTURE(mfxExtAVCEncodedFrameInfoTest, 1_param)
    {
        CMD_PARAM("-enc_frame_info", "1");

        PipelineRunner<MockPipelineConfig> pipeline;

        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params, &mock_cfg));

        std::list<ICommandActivator*> &cmds = pPipeline2->GetCommands();
        CHECK_EQUAL(2u, cmds.size());
    }

    TEST_FIXTURE(mfxExtAVCEncodedFrameInfoTest, 1_param_no_eos)
    {
        CMD_PARAM("-enc_frame_info", "1");
        params[VM_STRING("-h264")] =  std::vector<tstring>();

        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params, &mock_cfg));

        std::list<ICommandActivator*> &cmds = pPipeline2->GetCommands();
        CHECK_EQUAL(2u, cmds.size());
    }

    TEST_FIXTURE(mfxExtAVCEncodedFrameInfoTest, 2_params)
    {
        CMD_PARAM("-enc_frame_info", "1");
        CMD_PARAM("-enc_frame_info", "2");

        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params, &mock_cfg));

        std::list<ICommandActivator*> &cmds = pPipeline2->GetCommands();
        CHECK_EQUAL(2u, cmds.size());
    }
    TEST_FIXTURE(mfxExtAVCEncodedFrameInfoTest, 2_params_no_eos)
    {
        CMD_PARAM("-enc_frame_info", "1");
        CMD_PARAM("-enc_frame_info", "2");
        params[VM_STRING("-h264")] =  std::vector<tstring>();

        PipelineRunner<MockPipelineConfig> pipeline;

        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params, &mock_cfg));

        std::list<ICommandActivator*> &cmds = pPipeline2->GetCommands();
        CHECK_EQUAL(2u, cmds.size());
    }

    TEST_FIXTURE(mfxExtAVCEncodedFrameInfoTest, no_params_should_apply_on_1st_frame_and_further) {
        params[VM_STRING("-enc_frame_info")] =  std::vector<tstring>();
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params, &mock_cfg));
        
        VerifyAttached();

        //next frame also should have buffer enabled
        current_frame.ret_val = 2;
        pPipeline2->_GetNumDecodedFrames.WillReturn(current_frame);

        VerifyNOTAttached();
    }

    TEST_FIXTURE(mfxExtAVCEncodedFrameInfoTest, 1_params_should_apply_only_2nd)
    {
        CMD_PARAM("-enc_frame_info", "1");
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params, &mock_cfg));

        //no buffers for 1st frame
        VerifyNOTAttached();

        //buffer set for 2nd frame 
        current_frame.ret_val = 1;
        pPipeline2->_GetNumDecodedFrames.WillDefaultReturn(&current_frame);

        VerifyAttached();
        
        //buffer not set for 3rd frame
        current_frame.ret_val = 2;
        pPipeline2->_GetNumDecodedFrames.WillDefaultReturn(&current_frame);

        VerifyDetached();
    }

    TEST_FIXTURE(mfxExtAVCEncodedFrameInfoTest, 2_params_should_apply_on_2nd_frame_3rd_frame_andNot_on_4th)
    {
        CMD_PARAM("-enc_frame_info", "1");
        CMD_PARAM("-enc_frame_info", "2");
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params, &mock_cfg));

        //no buffers for 1st frame
        VerifyNOTAttached();

        //buffer set for 2nd frame 
        current_frame.ret_val = 1;
        pPipeline2->_GetNumDecodedFrames.WillDefaultReturn(&current_frame);

        VerifyAttached();

        //buffer set for 3rd frame
        current_frame.ret_val = 2;
        pPipeline2->_GetNumDecodedFrames.WillDefaultReturn(&current_frame);

        VerifyNOTAttached();

        //buffer detached 4rd frame
        current_frame.ret_val = 3;
        pPipeline2->_GetNumDecodedFrames.WillDefaultReturn(&current_frame);

        VerifyDetached();
    }

    TEST_FIXTURE(mfxExtAVCEncodedFrameInfoTest, assign_buffer_in_encode_frame_async_call) {
        
        EncodedFrameInfoEncoder enc(make_instant_auto_ptr(MakeUndeletable(mock_encode)));
        mfxExtAVCEncodedFrameInfo buf = {};
        mfx_init_ext_buffer(buf);
        enc.AddExtBuffer((mfxExtBuffer & )buf);
        mfxBitstream bs = {};
        mfxSyncPoint sp;
        CHECK_EQUAL(MFX_ERR_NONE, enc.EncodeFrameAsync(NULL, NULL, &bs, &sp));
        TEST_METHOD_TYPE(MockVideoEncode::EncodeFrameAsync) efa_params;
        CHECK(mock_encode._EncodeFrameAsync.WasCalled(&efa_params));
        CHECK_EQUAL(1, efa_params.value2.NumExtParam);
        CHECK_EQUAL((mfxU32)MFX_EXTBUFF_ENCODED_FRAME_INFO, efa_params.value2.ExtParam[0]->BufferId);
    }
}