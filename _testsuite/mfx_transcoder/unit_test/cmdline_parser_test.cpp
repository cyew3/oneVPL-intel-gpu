/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_pipeline_transcode.h"
#include "mock_mfx_pipeline_factory.h"
#include "mfx_encode_pipeline_config.h"
#include "mock_mfx_pipeline_config.h"
#include "mock_mfx_vpp.h"


SUITE(CmdLineTransPipeline)
{
    TEST(CBR_MODE)
    {
        MockPipelineFactory *fac = new MockPipelineFactory();
        
        MFXTranscodingPipeline pipeline(fac);
        vm_char argv[][128] = {
            VM_STRING("-b"), VM_STRING("10"),
            VM_STRING("-h264"),
        };
        std::vector<vm_char*> options;
        for (int i = 0; i < MFX_ARRAY_SIZE(argv);i++)
        {
            options.push_back(argv[i]);
        }
        vm_char **p = &options.front();

        CHECK(MFX_ERR_NONE == pipeline.ProcessCommand(p, MFX_ARRAY_SIZE(argv)));
    }

    TEST(extcodingoptions2)
    {
        PipelineRunner<MFXPipelineConfigEncode>::CmdLineParams params;
        PipelineRunner<MFXPipelineConfigEncode> pipeline;

        params[VM_STRING("-IntRefType")].push_back(VM_STRING("1"));
        params[VM_STRING("-IntRefCycleSize")].push_back(VM_STRING("2"));
        params[VM_STRING("-IntRefQPDelta")].push_back(VM_STRING("3"));
        params[VM_STRING("-MaxFrameSize")].push_back(VM_STRING("4"));
        params[VM_STRING("-BitrateLimit")].push_back(VM_STRING("off"));
        params[VM_STRING("-MBBRC")].push_back(VM_STRING("on"));
        params[VM_STRING("-ExtBRC")].push_back(VM_STRING("off"));
        
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params));

        params.clear();
        params[VM_STRING("-IntRefType")].push_back(VM_STRING("66000"));
        CHECK(MFX_ERR_NONE != pipeline.ProcessCommand(params));
        params.clear();
        params[VM_STRING("-IntRefCycleSize")].push_back(VM_STRING("66000"));
        CHECK(MFX_ERR_NONE != pipeline.ProcessCommand(params));
        params.clear();
        params[VM_STRING("-IntRefQPDelta")].push_back(VM_STRING("33000"));
        CHECK(MFX_ERR_NONE != pipeline.ProcessCommand(params));
        params.clear();
        params[VM_STRING("-BitrateLimit")].push_back(VM_STRING("66"));
        CHECK(MFX_ERR_NONE != pipeline.ProcessCommand(params));
        params.clear();
        params[VM_STRING("-MBBRC")].push_back(VM_STRING("66"));
        CHECK(MFX_ERR_NONE != pipeline.ProcessCommand(params));
        params.clear();
        params[VM_STRING("-ExtBRC")].push_back(VM_STRING("66"));
        CHECK(MFX_ERR_NONE != pipeline.ProcessCommand(params));
    }

    TEST(extCodingOptionsDDI)
    {
        PipelineRunner<MFXPipelineConfigEncode>::CmdLineParams params;
        PipelineRunner<MFXPipelineConfigEncode> pipeline;

        params[VM_STRING("-WeightedBiPredIdc")].push_back(VM_STRING("1"));
        params[VM_STRING("-CabacInitIdcPlus1")].push_back(VM_STRING("1"));
        //params[VM_STRING("-MergeLists")].push_back(VM_STRING("on"));
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params));
        params.clear();
    }

    TEST(ParFile)
    {
        PipelineRunner<MockPipelineConfig>::CmdLineParams params;
        params[VM_STRING("-h264")] =  std::vector<tstring>();
        params[VM_STRING("-i")].push_back(VM_STRING("D:\\mediasdk_streams\\YUV\\foster_720x576.yuv"));
        params[VM_STRING("-o")].push_back(VM_STRING("build\\win_x64\\output\\br_h264_encode_suite\\0001.h264"));
        params[VM_STRING("-h")].push_back(VM_STRING("576"));
        params[VM_STRING("-w")].push_back(VM_STRING("720"));
        params[VM_STRING("-par")].push_back(VM_STRING("super_parfile"));

        std::vector<tstring> par_file_lines;
        par_file_lines.push_back(VM_STRING("VuiNalHrdParameters = on"));
        par_file_lines.push_back(VM_STRING("AUDelimiter = on"));
        par_file_lines.push_back(VM_STRING("PicTimingSEI = on"));
        par_file_lines.push_back(VM_STRING("EndOfSequence = on"));

        TEST_METHOD_TYPE(MockFile::ReadLine) ret_params;
        TEST_METHOD_TYPE(MockFile::isEOF) ret_params_eof;
        
        MockFile mFile;

        for (size_t i = 0;i < par_file_lines.size(); i++)
        {
            ret_params.value0 = const_cast<vm_char*>(par_file_lines[i].c_str());
            mFile._ReadLine.WillReturn(ret_params);

            //assume that algorithm checks for eof after each readfile call
            ret_params_eof.ret_val = false;
            mFile._isEOF.WillReturn(ret_params_eof);
        }

        MockPipelineFactory *mock_factory = new MockPipelineFactory;

        TEST_METHOD_TYPE(MockPipelineFactory::CreatePARReader) ret_params_factory;
        ret_params_factory.ret_val = MakeUndeletable(mFile);

        mock_factory->_CreatePARReader.WillReturn(ret_params_factory);

        TEST_METHOD_TYPE(MockPipelineConfig::CreatePipeline) ret_params_pipeline_cfg;
        MockPipelineConfig mock_cfg;
        ret_params_pipeline_cfg.ret_val = new MFXTranscodingPipeline(mock_factory);

        mock_cfg._CreatePipeline.WillReturn(ret_params_pipeline_cfg);

        PipelineRunner<MockPipelineConfig> pipeline;
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params, &mock_cfg));

        TEST_METHOD_TYPE(MockFile::Open) ret_params_open;

        CHECK(mFile._Open.WasCalled(&ret_params_open));

        CHECK_EQUAL(convert_w2s(ret_params_open.value0), "super_parfile");
    }

    class MFXTranscodingPipeline2 : public MFXTranscodingPipeline
    {
    public:
        MFXTranscodingPipeline2(IMFXPipelineFactory * pFac)
            : MFXTranscodingPipeline(pFac)
        {
        }
        mfxStatus InitRender()
        {
            return MFXTranscodingPipeline::InitRender();
        }
        mfxStatus CheckParams()
        {
            return MFXTranscodingPipeline::CheckParams();
        }
        mfxStatus ReleasePipeline()
        {
            return MFXTranscodingPipeline::ReleasePipeline();
        }
        mfxStatus CreateRender()
        {
            return MFXTranscodingPipeline::CreateRender();
        }
        mfxStatus CreateVPP()
        {
            return MFXTranscodingPipeline::CreateVPP();
        }
        std::list<ICommandActivator*>& GetCommands() {
            return m_commands;
        }

    };

    TEST(RepeatMode_ExtBuffers)
    {
        PipelineRunner<MockPipelineConfig>::CmdLineParams params;
        params[VM_STRING("-h264")] =  std::vector<tstring>();
        params[VM_STRING("-i")].push_back(VM_STRING("D:\\mediasdk_streams\\YUV\\foster_720x576.yuv"));
        params[VM_STRING("-o")].push_back(VM_STRING("build\\win_x64\\output\\br_h264_encode_suite\\0001.h264"));
        params[VM_STRING("-h")].push_back(VM_STRING("576"));
        params[VM_STRING("-w")].push_back(VM_STRING("720"));
        params[VM_STRING("-repeat")].push_back(VM_STRING("2"));
        params[VM_STRING("-PicTimingSEI")].push_back(VM_STRING("on"));

        MockPipelineFactory mock_factory;
        TEST_METHOD_TYPE(MockPipelineFactory::CreateVideoEncode) ret_params_factory;

        MockVideoEncode vencode;

        //configuring resolution that is used by filewriter base class - TODO: possible that this logic is rudiment
        mfxVideoParam videoparams_ret = {0};
        videoparams_ret.mfx.FrameInfo.Width = 640;
        videoparams_ret.mfx.FrameInfo.Height = 480;
        videoparams_ret.mfx.FrameInfo.FourCC = MFX_FOURCC_YV12;

        TEST_METHOD_TYPE(MockVideoEncode::GetVideoParam) get_video_params_ret;
        get_video_params_ret.ret_val = MFX_ERR_NONE;
        get_video_params_ret.value0 = &videoparams_ret;

        vencode._GetVideoParam.WillDefaultReturn(&get_video_params_ret);

        //configuring mock file writer that will be created by encoder wraper
        MockFile mock_file;
        TEST_METHOD_TYPE(MockFile::isOpen) default_true(true);
        mock_file._isOpen.WillDefaultReturn(&default_true);

        TEST_METHOD_TYPE(MockPipelineFactory::CreateFileWriter) create_file_ret;
        create_file_ret.ret_val = MakeUndeletable(mock_file);
        mock_factory._CreateFileWriter.WillReturn(create_file_ret);
        create_file_ret.ret_val = MakeUndeletable(mock_file);
         mock_factory._CreateFileWriter.WillReturn(create_file_ret);


        //configuring returns for main factory 
        ret_params_factory.ret_val = MakeUndeletable(vencode);
        mock_factory._CreateVideoEncode.WillReturn(ret_params_factory);
        ret_params_factory.ret_val = MakeUndeletable(vencode);
        mock_factory._CreateVideoEncode.WillReturn(ret_params_factory);

        TEST_METHOD_TYPE(MockPipelineConfig::CreatePipeline) ret_params_pipeline_cfg;

        MockPipelineConfig mock_cfg;
        MFXTranscodingPipeline2 *pPipeline2 = new MFXTranscodingPipeline2(MakeUndeletable(mock_factory));
        ret_params_pipeline_cfg.ret_val = pPipeline2;

        mock_cfg._CreatePipeline.WillReturn(ret_params_pipeline_cfg);

        PipelineRunner<MockPipelineConfig> pipeline;
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params, &mock_cfg));

        //build pipeline twice aim to simulate pipeline behavior in case or repeat option
        CHECK_EQUAL(MFX_ERR_NONE, pPipeline2->CheckParams());
        CHECK_EQUAL(MFX_ERR_NONE, pPipeline2->CreateRender());
        CHECK_EQUAL(MFX_ERR_NONE, pPipeline2->InitRender());

        TEST_METHOD_TYPE(MockVideoEncode::Init) ret_params_init, ret_params_init2;
        CHECK(vencode._Init.WasCalled(&ret_params_init));

        mfxVideoParam & ep = ret_params_init.value0;
        mfxExtCodingOption *pExtCO  = NULL;

        for (size_t j  = 0; j < ep.NumExtParam; j++)
        {
            if (ep.ExtParam[j]->BufferId == BufferIdOf<mfxExtCodingOption>::id)
            {
                pExtCO = reinterpret_cast<mfxExtCodingOption*>(ep.ExtParam[j]);
            }
        }
        CHECK(pExtCO != NULL);
        //check that pictimingsei enabled
        CHECK_EQUAL(MFX_CODINGOPTION_ON, pExtCO->PicTimingSEI);

        //same should happen after repeat started
        CHECK_EQUAL(MFX_ERR_NONE, pPipeline2->ReleasePipeline());
        CHECK_EQUAL(MFX_ERR_NONE, pPipeline2->CheckParams());
        CHECK_EQUAL(MFX_ERR_NONE, pPipeline2->CreateRender());
        CHECK_EQUAL(MFX_ERR_NONE, pPipeline2->InitRender());

        CHECK(vencode._Init.WasCalled(&ret_params_init));
        mfxVideoParam & ep2 = ret_params_init.value0;
        pExtCO = NULL;
        for (size_t j  = 0; j < ep2.NumExtParam; j++)
        {
            if (ep2.ExtParam[j]->BufferId == BufferIdOf<mfxExtCodingOption>::id)
            {
                pExtCO = reinterpret_cast<mfxExtCodingOption*>(ep2.ExtParam[j]);
            }
        }
        CHECK(pExtCO != NULL);
        //check that pictimingsei enabled
        CHECK_EQUAL(MFX_CODINGOPTION_ON, pExtCO->PicTimingSEI);
    }


    struct ParFileForSvc_VPP
    {
        PipelineRunner<MockPipelineConfig>::CmdLineParams params;
        MockPipelineFactory mock_factory;
        TEST_METHOD_TYPE(MockPipelineFactory::CreateVPP) ret_params_factory;

        ParFileForSvc_VPP()
        {
            params[VM_STRING("-h264")] =  std::vector<tstring>();
            CMD_PARAM("-i", "D:\\mediasdk_streams\\YUV\\foster_720x576.yuv");
            CMD_PARAM("-o", "build\\win_x64\\output\\br_h264_encode_suite\\0001.h264");
            //svc sample layers description
            CMD_PARAM("-svc_rate_ctrl.Layer[0].QualityId", "0");
            CMD_PARAM("-svc_rate_ctrl.Layer[0].TemporalId", "0");
            CMD_PARAM("-svc_rate_ctrl.Layer[0].DependencyId", "0");
            CMD_PARAM("-svc_rate_ctrl.Layer[0].CbrVbr.MaxKbps", "630");
            CMD_PARAM("-svc_rate_ctrl.Layer[0].CbrVbr.BufferSizeInKB", "157");
            CMD_PARAM("-svc_rate_ctrl.Layer[0].CbrVbr.InitialDelayInKB", "78");
            CMD_PARAM("-svc_rate_ctrl.Layer[0].CbrVbr.TargetKbps", "630");
            CMD_PARAM("-svc_rate_ctrl.Layer[1].QualityId", "0");
            CMD_PARAM("-svc_rate_ctrl.Layer[1].TemporalId", "0");
            CMD_PARAM("-svc_rate_ctrl.Layer[1].DependencyId", "1");
            CMD_PARAM("-svc_rate_ctrl.Layer[1].CbrVbr.MaxKbps", "1500");
            CMD_PARAM("-svc_rate_ctrl.Layer[1].CbrVbr.BufferSizeInKB", "375");
            CMD_PARAM("-svc_rate_ctrl.Layer[1].CbrVbr.InitialDelayInKB", "187");
            CMD_PARAM("-svc_rate_ctrl.Layer[1].CbrVbr.TargetKbps", "1500");
            CMD_PARAM("-svc_rate_ctrl.RateControlMethod", "1");
            CMD_PARAM("-svc_seq.DependencyLayer[0].TemporalNum", "1");
            CMD_PARAM("-svc_seq.DependencyLayer[0].QualityLayer[0].TcoeffPredictionFlag", "0");
            CMD_PARAM("-svc_seq.DependencyLayer[0].GopPicSize", "24");
            CMD_PARAM("-svc_seq.DependencyLayer[0].GopRefDist", "5");
            CMD_PARAM("-svc_seq.DependencyLayer[0].Width", "176");
            CMD_PARAM("-svc_seq.DependencyLayer[0].Height", "144");
            CMD_PARAM("-svc_seq.DependencyLayer[0].TemporalId[0]", "0");
            CMD_PARAM("-svc_seq.DependencyLayer[0].RefLayerDid", "0");
            CMD_PARAM("-svc_seq.DependencyLayer[0].QualityNum", "1");
            CMD_PARAM("-svc_seq.DependencyLayer[0].Active", "16");
            CMD_PARAM("-svc_seq.DependencyLayer[1].TemporalNum", "1");
            CMD_PARAM("-svc_seq.DependencyLayer[1].QualityLayer[0].TcoeffPredictionFlag", "0");
            CMD_PARAM("-svc_seq.DependencyLayer[1].GopPicSize", "24");
            CMD_PARAM("-svc_seq.DependencyLayer[1].GopRefDist", "5");
            //CMD_PARAM("-svc_seq.DependencyLayer[1].Width", "352");
            //CMD_PARAM("-svc_seq.DependencyLayer[1].Height", "288");
            CMD_PARAM("-svc_seq.DependencyLayer[1].TemporalId[0]", "0");
            CMD_PARAM("-svc_seq.DependencyLayer[1].RefLayerDid", "0");
            CMD_PARAM("-svc_seq.DependencyLayer[1].QualityNum", "1");
            CMD_PARAM("-svc_seq.DependencyLayer[1].Active", "16");
            CMD_PARAM("-svc_seq.TemporalScale[0]", "1");
        }
        void prepare() 
        {
            MockVpp mock_vpp;
            ret_params_factory.ret_val = MakeUndeletable(mock_vpp);
            mock_factory._CreateVPP.WillReturn(ret_params_factory);

            //create vpp gets called twice
            ret_params_factory.ret_val = MakeUndeletable(mock_vpp);
            mock_factory._CreateVPP.WillReturn(ret_params_factory);

            TEST_METHOD_TYPE(MockPipelineConfig::CreatePipeline) ret_params_pipeline_cfg;
            MockPipelineConfig mock_cfg;
            MFXTranscodingPipeline2 *pPipeline2 = new MFXTranscodingPipeline2(MakeUndeletable(mock_factory));
            ret_params_pipeline_cfg.ret_val = pPipeline2;

            mock_cfg._CreatePipeline.WillReturn(ret_params_pipeline_cfg);

            PipelineRunner<MockPipelineConfig> pipeline;
            CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params, &mock_cfg));

            //build pipeline twice aim to simulate pipeline behavior in case or repeat option
            CHECK_EQUAL(MFX_ERR_NONE, pPipeline2->CheckParams());
            CHECK_EQUAL(MFX_ERR_NONE, pPipeline2->CreateVPP());
        }
    };


    TEST_FIXTURE(ParFileForSvc_VPP, SVCedVpp_ON)
    {
        CMD_PARAM("-h", "352");
        CMD_PARAM("-w", "288");
        CMD_PARAM("-svc_seq.DependencyLayer[1].Height", "288");
        CMD_PARAM("-svc_seq.DependencyLayer[1].Width", "352");

        prepare();

        //vpp called twice 1st-for native
        CHECK(mock_factory._CreateVPP.WasCalled(&ret_params_factory));
        PipelineObjectDescBase & base = dynamic_cast<PipelineObjectDescBase &>(ret_params_factory.value0);
        CHECK_EQUAL(VPP_MFX_NATIVE, base.Type);

        //2nd-for wrapper
        CHECK(mock_factory._CreateVPP.WasCalled(&ret_params_factory));
        PipelineObjectDescBase & base2 = dynamic_cast<PipelineObjectDescBase &>(ret_params_factory.value0);
        CHECK_EQUAL(VPP_SVC, base2.Type);
    }

    TEST_FIXTURE(ParFileForSvc_VPP, SVCedVpp_OFF)
    {
        //declaring both input files for each dependency layer force mfx_transcoder to work without vpp
        CMD_PARAM("-svc_seq.DependencyLayer[0].InputFile", "m:\\mediasdk_streams\\SVC-YUV\\matrix_176x144_300.yuv");
        CMD_PARAM("-svc_seq.DependencyLayer[1].InputFile", "m:\\mediasdk_streams\\SVC-YUV\\matrix_352x288_300.yuv");
        CMD_PARAM("-svc_seq.DependencyLayer[1].Height", "288");
        CMD_PARAM("-svc_seq.DependencyLayer[1].Width", "352");

        prepare();

        //vpp not called at all 
        CHECK(!mock_factory._CreateVPP.WasCalled(&ret_params_factory));
    }

    TEST_FIXTURE(ParFileForSvc_VPP, SVCedVpp_OFF_dueto_resolution)
    {
        //resolution of second layer is same as in input resolution
        CMD_PARAM("-w", "176");
        CMD_PARAM("-h", "144");
        CMD_PARAM("-svc_seq.DependencyLayer[1].Width", "176");
        CMD_PARAM("-svc_seq.DependencyLayer[1].Height", "144");
        

        prepare();

        //vpp not called at all 
        CHECK(!mock_factory._CreateVPP.WasCalled(&ret_params_factory));
    }

    TEST_FIXTURE(ParFileForSvc_VPP, SVCedVpp_OFF_dueto_no_input_resolution)
    {
        //input resolution not specified - cannot use vpp, also if there is no input files in every layer - processing should stop
        //, later, but create vpp - succedded
        CMD_PARAM("-svc_seq.DependencyLayer[0].InputFile", "m:\\mediasdk_streams\\SVC-YUV\\matrix_176x144_300.yuv");
        CMD_PARAM("-svc_seq.DependencyLayer[1].InputFile", "m:\\mediasdk_streams\\SVC-YUV\\matrix_352x288_300.yuv");
        CMD_PARAM("-svc_seq.DependencyLayer[1].Width", "352");
        CMD_PARAM("-svc_seq.DependencyLayer[1].Height", "288");

        prepare();

        //vpp not called at all 
        CHECK(!mock_factory._CreateVPP.WasCalled(&ret_params_factory));
    }

}