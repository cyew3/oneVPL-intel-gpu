/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */


#include "utest_common.h"
#include "mfx_pipeline_dec.h"
#include "mfx_decode_pipeline_config.h"
#include "mock_mfx_vpp.h"
#include "mock_mfx_pipeline_config.h"
#include "mock_mfx_pipeline_factory.h"
#include "mock_mfx_ivideo_session.h"
#include "mfx_test_utils.h"

SUITE(PipelineOptions)
{
    class MFXDecPipeline2 : public MFXDecPipeline
    {
    public:
        MFXDecPipeline2(IMFXPipelineFactory *pFactory)
            : MFXDecPipeline(pFactory)
        {
        }
        mfxStatus CreateVPP()
        {
            return MFXDecPipeline::CreateVPP();
        }
        mfxStatus InitVPP()
        {
            return MFXDecPipeline::InitVPP();
        }
        mfxStatus CreateCore()
        {
            return MFXDecPipeline::CreateCore();
        }
    };

    TEST(d3d11_single_texture)
    {
        PipelineRunner<MFXPipelineConfigDecode>::CmdLineParams params;
        params[VM_STRING("-d3d11:single_texture")] = std::vector<tstring>();
        params[VM_STRING("-dec:d3d11:single_texture")] = std::vector<tstring>();
        params[VM_STRING("-enc:d3d11:single_texture")] = std::vector<tstring>();

        PipelineRunner<MFXPipelineConfigDecode> pipeline;
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params));
    }

    TEST(seek_frame_num)
    {
        PipelineRunner<MFXPipelineConfigDecode>::CmdLineParams params;
        params[VM_STRING("-seekframe")].push_back(VM_STRING("0"));
        params[VM_STRING("-seekframe")].push_back(VM_STRING("10"));

        PipelineRunner<MFXPipelineConfigDecode> pipeline;
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params));

        params.clear();
        params[VM_STRING("-seekframe")].push_back(VM_STRING("1"));
        CHECK(MFX_ERR_NONE != pipeline.ProcessCommand(params));
    }
    template <class PipelineConfigType>
    class VPPCreateHelper
    {
    protected:
       typedef typename PipelineRunner<MockPipelineConfig>::MultiCmdLineParams MultiCmdLineParams;
       MultiCmdLineParams params;
       PipelineRunner<PipelineConfigType> pipeline;
       TEST_METHOD_TYPE(MockVpp::Init) vpp_init_params;

       //run vpp initialisation routines a
       bool create_vpp()
       {
           MockVpp mock_vpp;
           MockPipelineFactory mock_factory;

           TEST_METHOD_TYPE(MockPipelineFactory::CreateVPP) create_vpp_params_factory;
           create_vpp_params_factory.ret_val = MakeUndeletable(mock_vpp);
           mock_factory._CreateVPP.WillReturn(create_vpp_params_factory);
           //
           TEST_METHOD_TYPE(MockPipelineConfig::CreatePipeline) ret_params_pipeline_cfg;
           MockPipelineConfig mock_cfg;
           MFXDecPipeline2 *pPipeline2 = new MFXDecPipeline2(MakeUndeletable(mock_factory));
           ret_params_pipeline_cfg.ret_val = pPipeline2;
           mock_cfg._CreatePipeline.WillReturn(ret_params_pipeline_cfg);


           CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params, &mock_cfg));

           //try build pipeline
           CHECK_EQUAL(MFX_ERR_NONE, pPipeline2->CreateVPP());
           CHECK_EQUAL(MFX_ERR_NONE, pPipeline2->InitVPP());

           //check that factory was called with valid type
           TEST_METHOD_TYPE(MockPipelineFactory::CreateVPP) create_vpp_params;
           CHECK_EQUAL(true, mock_factory._CreateVPP.WasCalled(&create_vpp_params));
           CHECK_EQUAL(VPP_MFX_NATIVE, create_vpp_params.value0.Type);

           
           CHECK_EQUAL(true, mock_vpp._Init.WasCalled(&vpp_init_params));

          
           return create_vpp_params.value0.Type== VPP_MFX_NATIVE;
       }
        template <class VppExBuffer>
       bool get_buffer(VppExBuffer &buffer)
       {
           mfxVideoParam & vp = *vpp_init_params.value0;
            bool bFound = false;
           for (size_t j  = 0; j < vp.NumExtParam; j++)
           {
               if (vp.ExtParam[j]->BufferId == BufferIdOf<VppExBuffer>::id)
               {
                   buffer = *(reinterpret_cast<VppExBuffer*>(vp.ExtParam[j]));
                   bFound = true;
                   break;
               }
           }
           return bFound;
       }
    };

    typedef VPPCreateHelper<MFXPipelineConfigDecode> VppCreateHelperDecode;
//    typedef VPPCreateHelper<MFXPipelineConfigEncode> VppCreateHelperEncode;

    TEST_FIXTURE(VppCreateHelperDecode, stabilization)
    {
        std::vector<tstring> opt_detail;
        opt_detail.push_back(VM_STRING("1"));
        params.insert(make_pair(VM_STRING("-stabilize"), opt_detail));
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params));

        mfxExtVPPImageStab vstab = {};
        CHECK(create_vpp());
        CHECK(get_buffer(vstab));
        CHECK_EQUAL(MFX_IMAGESTAB_MODE_UPSCALE, vstab.Mode);

        //also check boxing mode
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params));

        opt_detail[0] = VM_STRING("2");
        params.clear();
        params.insert(make_pair(VM_STRING("-stabilize"), opt_detail));

        CHECK(create_vpp());
        CHECK(get_buffer(vstab));
        CHECK_EQUAL(MFX_IMAGESTAB_MODE_BOXING, vstab.Mode);

        params.clear();
        params.insert(make_pair(VM_STRING("-stabilize"), std::vector<tstring>()));
        CHECK(MFX_ERR_NONE != pipeline.ProcessCommand(params));
    }

    TEST(denoise)
    {
        PipelineRunner<MFXPipelineConfigDecode>::MultiCmdLineParams params;
        std::vector<tstring> opt_detail;
        opt_detail.push_back(VM_STRING("33"));
        params.insert(make_pair(VM_STRING("-denoise"), opt_detail));

        PipelineRunner<MFXPipelineConfigDecode> pipeline;
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params));

        params.clear();
        params.insert(make_pair(VM_STRING("-denoise"), std::vector<tstring>()));
        CHECK(MFX_ERR_NONE != pipeline.ProcessCommand(params));
    }

    TEST(detail)
    {
        PipelineRunner<MFXPipelineConfigDecode>::MultiCmdLineParams params;
        std::vector<tstring> opt_detail;
        opt_detail.push_back(VM_STRING("33"));
        params.insert(make_pair(VM_STRING("-detail"), opt_detail));

        PipelineRunner<MFXPipelineConfigDecode> pipeline;
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params));

        params.clear();
        params.insert(make_pair(VM_STRING("-detail"), std::vector<tstring>()));
        CHECK(MFX_ERR_NONE != pipeline.ProcessCommand(params));
    }

    TEST(procamp)
    {
        typedef PipelineRunner<MFXPipelineConfigDecode>::MultiCmdLineParams MultiCmdLineParams;
        
        MultiCmdLineParams params;
        MultiCmdLineParams::mapped_type opt_detail;
        opt_detail.push_back(VM_STRING("1.33"));
        PipelineRunner<MFXPipelineConfigDecode> pipeline;

        // options receive one parameter
        params.clear();
        params.insert(make_pair(VM_STRING("-brightness"), opt_detail));
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params));

        params.clear();
        params.insert(make_pair(VM_STRING("-contrast"), opt_detail));
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params));

        params.clear();
        params.insert(make_pair(VM_STRING("-hue"), opt_detail));
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params));

        params.clear();
        params.insert(make_pair(VM_STRING("-saturation"), opt_detail));
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params));

        // options fails with no parameters
        params.clear();
        params.insert(make_pair(VM_STRING("-brightness"), MultiCmdLineParams::mapped_type()));
        CHECK(MFX_ERR_NONE != pipeline.ProcessCommand(params));

        params.clear();
        params.insert(make_pair(VM_STRING("-contrast"), MultiCmdLineParams::mapped_type()));
        CHECK(MFX_ERR_NONE != pipeline.ProcessCommand(params));

        params.clear();
        params.insert(make_pair(VM_STRING("-hue"), MultiCmdLineParams::mapped_type()));
        CHECK(MFX_ERR_NONE != pipeline.ProcessCommand(params));
        params.clear();

        params.insert(make_pair(VM_STRING("-saturation"), MultiCmdLineParams::mapped_type()));
        CHECK(MFX_ERR_NONE != pipeline.ProcessCommand(params));
    }

    TEST_FIXTURE(VppCreateHelperDecode, DETECT_PAFF)
    {
        //typedef PipelineRunner<MockPipelineConfig>::CmdLineParams CmdLineParams;
        //CmdLineParams params;
        //PipelineRunner<MFXPipelineConfigDecode> pipeline;

        // options receive no parameter
           params.insert(make_pair(VM_STRING("-paff"), MultiCmdLineParams::mapped_type()));

        mfxExtVPPDoUse DoUse = {};
        CHECK(create_vpp());
        CHECK(get_buffer(DoUse));
        
        bool bFoundPaffBuffer = false;
        for (size_t i  = 0; i < DoUse.NumAlg; i++)
        {
            if (DoUse.AlgList[i] == MFX_EXTBUFF_VPP_PICSTRUCT_DETECTION)
            {
                bFoundPaffBuffer = true;
                break;
            }
        }

        CHECK(bFoundPaffBuffer);
    }

    struct TestHWLIB
    {
        static void run_test(const tstring & key, mfxIMPL result)
        {
            MockVideoSession  mock_session ;

            typedef PipelineRunner<MockPipelineConfig>::CmdLineParams CmdLineParams;
            CmdLineParams params;
            PipelineRunner<MFXPipelineConfigDecode> pipeline;

            // options receive no parameter
            params[key].empty();

            //=new MockVideoSession();
            MockPipelineFactory mock_factory;

            TEST_METHOD_TYPE(MockPipelineFactory::CreateVideoSession) create_session_params_factory;
            create_session_params_factory.ret_val =  MakeUndeletable(mock_session);

            mock_factory._CreateVideoSession.WillReturn(create_session_params_factory);
            //
            TEST_METHOD_TYPE(MockPipelineConfig::CreatePipeline) ret_params_pipeline_cfg;
            MockPipelineConfig mock_cfg;
            MFXDecPipeline2 *pPipeline2 = new MFXDecPipeline2(MakeUndeletable(mock_factory));
            ret_params_pipeline_cfg.ret_val = pPipeline2;
            mock_cfg._CreatePipeline.WillReturn(ret_params_pipeline_cfg);


            CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params, &mock_cfg));
            //test medhod
            pPipeline2->CreateCore();

            CHECK_EQUAL(true, mock_factory._CreateVideoSession.WasCalled(&create_session_params_factory));
            CHECK_EQUAL((int)VIDEO_SESSION_NATIVE, create_session_params_factory.value0.Type);

            TEST_METHOD_TYPE(MockVideoSession::Init) session_init_params;
            CHECK_EQUAL(true, mock_session._Init.WasCalled(&session_init_params));
            CHECK_EQUAL((int)result, session_init_params.value0);
        }
    };

    TEST(platform_lib_commands)
    {
        TestHWLIB::run_test(VM_STRING("-hw"), MFX_IMPL_HARDWARE_ANY);
        TestHWLIB::run_test(VM_STRING("-hw:1"), MFX_IMPL_HARDWARE);
        TestHWLIB::run_test(VM_STRING("-hw:2"), MFX_IMPL_HARDWARE2);
        TestHWLIB::run_test(VM_STRING("-hw:3"), MFX_IMPL_HARDWARE3);
        TestHWLIB::run_test(VM_STRING("-hw:4"), MFX_IMPL_HARDWARE4);
        TestHWLIB::run_test(VM_STRING("-hw:default"), MFX_IMPL_HARDWARE);
    }
}