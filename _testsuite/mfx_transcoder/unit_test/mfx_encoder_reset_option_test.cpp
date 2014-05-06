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


SUITE(mfxExtEncoderResetOption)
{
    struct MFXTranscodingPipeline2 : public MFXTranscodingPipeline
    {
        IVideoEncode *m_pEncode;
        MFXTranscodingPipeline2(IMFXPipelineFactory * pFac, IVideoEncode *pEncode)
            : MFXTranscodingPipeline(pFac)
            , m_pEncode (pEncode)
        {
        }

        mfxStatus CheckParams() {
            return MFXDecPipeline::CheckParams();
        }
        mfxStatus ProcessTrickCommands()
        {
            return MFXTranscodingPipeline::ProcessTrickCommands();
        }
        DECLARE_TEST_METHOD1(mfxStatus, GetRender, MAKE_DYNAMIC_TRAIT(IMFXVideoRender *, IMFXVideoRender **)) {
            TEST_METHOD_TYPE(GetRender) ret_val;
            if (!_GetRender.GetReturn(ret_val) && !_0) {
                return MFX_ERR_NONE;
            }
            *_0 = ret_val.value0;
            return ret_val.ret_val;
        }

        DECLARE_TEST_METHOD0(mfxU32, GetNumDecodedFrames) {
            TEST_METHOD_TYPE(GetNumDecodedFrames) result_val;
            if (!_GetNumDecodedFrames.GetReturn(result_val)) {
                return 0;
            }
            return result_val.ret_val;
        }
    };

    struct mfxExtEncoderResetOptionTest
    {
        MockPipelineFactory mock_factory;
        PipelineRunner<MockPipelineConfig>::CmdLineParams params;
        MockPipelineConfig mock_cfg;
        MockVideoEncode mock_encode;
        MFXTranscodingPipeline2 *pPipeline2;
        PipelineRunner<MockPipelineConfig> pipeline;
        TEST_METHOD_TYPE(MFXTranscodingPipeline2::GetNumDecodedFrames) current_frame;
        MockRender mock_rnd;

        mfxExtEncoderResetOptionTest() {
            //start with frame 0
            current_frame.ret_val = 0;

            TEST_METHOD_TYPE(MockPipelineConfig::CreatePipeline) ret_params_pipeline_cfg;
            pPipeline2 = new MFXTranscodingPipeline2(MakeUndeletable(mock_factory), &mock_encode);
            
            TEST_METHOD_TYPE(MFXTranscodingPipeline2::GetRender) ret_render;
            ret_render.ret_val = MFX_ERR_NONE;
            ret_render.value0 = &mock_rnd;
            pPipeline2->_GetRender.WillDefaultReturn(&ret_render);

            ret_params_pipeline_cfg.ret_val = pPipeline2;

            mock_cfg._CreatePipeline.WillReturn(ret_params_pipeline_cfg);
        }
        void MoveToNexFrame() {
            //buffer set for 2nd frame 
            current_frame.ret_val++;
            pPipeline2->_GetNumDecodedFrames.WillDefaultReturn(&current_frame);
        }
    };

    TEST_FIXTURE(mfxExtEncoderResetOptionTest, attached_on_reset) {

        params[VM_STRING("-reset_start")].push_back(VM_STRING("1"));
        params[VM_STRING("-StartNewSequence")].push_back(VM_STRING("on"));
        params[VM_STRING("-reset_end")].empty();
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params, &mock_cfg));
        //creating commands
        pPipeline2->CheckParams();

        TEST_METHOD_TYPE(MockVideoEncode::Reset) reset_args;

        //
        TEST_METHOD_TYPE(MockRender::RenderFrame) render_frame_args;
        render_frame_args.ret_val = MFX_ERR_MORE_DATA;
        mock_rnd._RenderFrame.WillDefaultReturn(&render_frame_args);
        //here reset should called
        CHECK_EQUAL(MFX_ERR_NONE, pPipeline2->ProcessTrickCommands());
        CHECK_EQUAL(false, mock_rnd._Reset.WasCalled());

        MoveToNexFrame();
        CHECK_EQUAL(MFX_ERR_NONE, pPipeline2->ProcessTrickCommands());

        CHECK_EQUAL(true, mock_rnd._Reset.WasCalled(&reset_args));
        CHECK_EQUAL(1, reset_args.value0.NumExtParam);
        CHECK_EQUAL((mfxU32)MFX_EXTBUFF_ENCODER_RESET_OPTION, reset_args.value0.ExtParam[0]->BufferId);
    }
}
