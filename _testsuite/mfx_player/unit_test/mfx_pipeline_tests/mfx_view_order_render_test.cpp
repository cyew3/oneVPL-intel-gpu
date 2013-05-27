/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_view_ordered_render.h"
#include "mfxmvc.h"


SUITE(ViewOrderedRender)
{
    struct InitSetup
        : CleanTestEnvironment
    {
        mfxVideoParam vParam;
        mfxExtBuffer * buffers[1];
        mfxExtMVCSeqDesc extDesc;
        mfxMVCViewDependency dependencies[5];

        mfxFrameSurface1 surfaces[5];
        
        InitSetup()
            : extDesc()
            , vParam()
        {
            extDesc.Header.BufferId = MFX_EXTBUFF_MVC_SEQ_DESC;
            extDesc.Header.BufferSz = sizeof(mfxExtMVCSeqDesc);
            extDesc.NumView = 5;
            extDesc.NumViewAlloc = 5;
            extDesc.View = dependencies;
            buffers[0] = (mfxExtBuffer * )&extDesc;

            MFX_ZERO_MEM(dependencies);
            dependencies[0].ViewId = 0;
            dependencies[1].ViewId = 2;
            dependencies[2].ViewId = 4;
            dependencies[3].ViewId = 3;
            dependencies[4].ViewId = 1;

            vParam.NumExtParam = 1;
            vParam.ExtParam = buffers;

            MFX_ZERO_MEM(surfaces);
            surfaces[0].Info.FrameId.ViewId  = 0;
            surfaces[1].Info.FrameId.ViewId  = 1;
            surfaces[2].Info.FrameId.ViewId  = 2;
            surfaces[3].Info.FrameId.ViewId  = 3;
            surfaces[4].Info.FrameId.ViewId  = 4;
        }
    };
    
    TEST_FIXTURE(InitSetup, OrderOfViewRenderingTest)
    {
        MockRender *rMock = new MockRender();
        MFXViewOrderedRender vRender(rMock );

        CHECK_EQUAL(MFX_ERR_NONE, vRender.SetAutoView(false));
        CHECK(rMock->_SetAutoView.WasCalled());

        CHECK_EQUAL(MFX_ERR_NONE, vRender.Init(&vParam, NULL));
        CHECK(rMock->_Init.WasCalled());
    
        for (int i = 0; i< MFX_ARRAY_SIZE(surfaces); i++)
        {
            CHECK_EQUAL(MFX_ERR_NONE, vRender.RenderFrame(&surfaces[i],NULL));
            if (i + 1 < MFX_ARRAY_SIZE(surfaces))
            {
                //all surfaces should be locked
                CHECK_EQUAL(1 , surfaces[i].Data.Locked);
            }
        }

        //CHECK(rMock->_RenderFrame.WasCalled());

        //emulating end of stream
        //CHECK_EQUAL(MFX_ERR_NONE, vRender.RenderFrame(NULL,NULL));

        for (int i = 0; i< MFX_ARRAY_SIZE(surfaces);i++)
        {
            TEST_METHOD_TYPE(MockRender::RenderFrame) params;
            CHECK(rMock->_RenderFrame.WasCalled(&params));

            //if (params)
            {
                CHECK_EQUAL(MFX_ERR_NONE, params.ret_val);
                CHECK_EQUAL(dependencies[i].ViewId, params.value0->Info.FrameId.ViewId); 
                CHECK_EQUAL(0, params.value0->Data.Locked); 
            }
        }
    }
    
    TEST_FIXTURE(InitSetup, PartialNumberOfViews)
    {
        MockRender *rMock = new MockRender();
        MFXViewOrderedRender vRender(rMock );

        vRender.SetAutoView(false);
        vRender.Init(&vParam, NULL);
        
        vRender.RenderFrame(&surfaces[1],NULL);
        vRender.RenderFrame(&surfaces[2],NULL);

        //first view is appeared out of initial order
        //according to spec mvcDecoder only can do this if whole stream contains less views than in header

        surfaces[0].Info.FrameId.ViewId = surfaces[1].Info.FrameId.ViewId;

        CHECK_EQUAL(MFX_ERR_NONE,  vRender.RenderFrame(&surfaces[1],NULL));

        {
            TEST_METHOD_TYPE(MockRender::RenderFrame) params;
            CHECK(rMock->_RenderFrame.WasCalled(&params));
            //if (params)
            {
                CHECK_EQUAL(MFX_ERR_NONE, params.ret_val);
                //2 is number of view followed in highest order according to our dependencies structure
                CHECK_EQUAL(2, params.value0->Info.FrameId.ViewId); 
                CHECK_EQUAL(0, params.value0->Data.Locked); 
            }
        }

        {
            TEST_METHOD_TYPE(MockRender::RenderFrame) params;
            CHECK(rMock->_RenderFrame.WasCalled(&params));
            // if (params)
            {
                CHECK_EQUAL(MFX_ERR_NONE, params.ret_val);
                CHECK_EQUAL(1, params.value0->Info.FrameId.ViewId); 

            //    CHECK_EQUAL(0, params->value0->Data.Locked); 
            }
        }

        CHECK(!rMock->_RenderFrame.WasCalled());
        //VERIFY_NOT_NCALL(rMock, RenderFrame, 2);

        //then number of view shouldn't be changed during whole decoding
        CHECK_EQUAL(MFX_ERR_UNDEFINED_BEHAVIOR,  vRender.RenderFrame(&surfaces[1],NULL));
        CHECK(!rMock->_RenderFrame.WasCalled());
        //VERIFY_NOT_NCALL(rMock, RenderFrame, 2);
    }

    TEST_FIXTURE(InitSetup, CorrectCompletionOfViewRender)
    {
        MockRender *rMock = new MockRender();
        MFXViewOrderedRender vRender(rMock );

        vRender.SetAutoView(false);
        vRender.Init(&vParam, NULL);


        vRender.RenderFrame(&surfaces[0],NULL);
        vRender.RenderFrame(&surfaces[1],NULL);

        //VERIFY_NOT_CALL(rMock, RenderFrame);
        CHECK(!rMock->_RenderFrame.WasCalled());

        //emulating end of stream
        CHECK_EQUAL(MFX_ERR_NONE, vRender.RenderFrame(NULL,NULL));

        //render frame should happen for existed views
        CHECK(rMock->_RenderFrame.WasCalled());
        CHECK(rMock->_RenderFrame.WasCalled());
        CHECK(rMock->_RenderFrame.WasCalled());//last time render frame with 0
        //VERIFY_NCALL(rMock, RenderFrame, 1);
    }

    TEST_FIXTURE(InitSetup, CorrectCompletionOfViewRender2)
    {
        MockRender *rMock = new MockRender();
        MFXViewOrderedRender vRender(rMock );

        vRender.SetAutoView(false);
        vRender.Init(&vParam, NULL);

        //rendering all views
        for (int i = 0; i < MFX_ARRAY_SIZE(surfaces); i++)
        {
            vRender.RenderFrame(&surfaces[i],NULL);
        }

        //all surfaces delivered to target render
        //VERIFY_NCALL(rMock, RenderFrame, MFX_ARRAY_SIZE(surfaces) - 1);
        for (int i = 0; i < MFX_ARRAY_SIZE(surfaces); i++)
        {
            CHECK(rMock->_RenderFrame.WasCalled());
        }
        
        CHECK(!rMock->_RenderFrame.WasCalled());
        //VERIFY_NOT_NCALL(rMock, RenderFrame, MFX_ARRAY_SIZE(surfaces));

        //emulating end of stream
        CHECK_EQUAL(MFX_ERR_NONE, vRender.RenderFrame(NULL,NULL));

        //render frame should happen with 0 surface only
        CHECK(rMock->_RenderFrame.WasCalled());
        //VERIFY_NCALL(rMock, RenderFrame, MFX_ARRAY_SIZE(surfaces) - 1);

        //no more render frames
        CHECK(!rMock->_RenderFrame.WasCalled());
        //VERIFY_NOT_NCALL(rMock, RenderFrame, MFX_ARRAY_SIZE(surfaces) + 1);
    }

    TEST_FIXTURE(InitSetup, IncorrectCompletionOfViewRender)
    {
        MockRender *rMock = new MockRender();
        MFXViewOrderedRender vRender(rMock );

        vRender.SetAutoView(false);
        vRender.Init(&vParam, NULL);

        //render needs to know how many views would be
        for (int i = 0; i < MFX_ARRAY_SIZE(surfaces); i++)
        {
            vRender.RenderFrame(&surfaces[i],NULL);
        }

        //rendering some of views
        for (int i = 0; i < MFX_ARRAY_SIZE(surfaces) - 1; i++)
        {
            vRender.RenderFrame(&surfaces[i],NULL);
        }
        
        //only first 5 surfaces are rendered
        for (int i = 0; i < MFX_ARRAY_SIZE(surfaces); i++)
        {
            CHECK(rMock->_RenderFrame.WasCalled());
        }
        CHECK(!rMock->_RenderFrame.WasCalled());

        //emulating end of stream
        CHECK_EQUAL(MFX_ERR_UNDEFINED_BEHAVIOR, vRender.RenderFrame(NULL,NULL));

        //nothing rendered
        CHECK(!rMock->_RenderFrame.WasCalled());
    }

    TEST_FIXTURE(InitSetup, QueryIOSurfacesInViewRender)
    {
        MockRender *rMock = new MockRender();
        MFXViewOrderedRender vRender(rMock );

        vRender.SetAutoView(false);
        //vRender.Init(&vParam, NULL);

        mfxFrameAllocRequest request_returned;
        request_returned.NumFrameMin = 5;
        request_returned.NumFrameSuggested = 6;


        rMock->_QueryIOSurf.WillReturn(TEST_METHOD_TYPE(MockRender::QueryIOSurf) (MFX_ERR_NONE, NULL, &request_returned));

        mfxFrameAllocRequest request_result;
        CHECK_EQUAL(MFX_ERR_NONE, vRender.QueryIOSurf(&vParam, &request_result));
        CHECK_EQUAL(10, request_result.NumFrameMin);
        CHECK_EQUAL(11, request_result.NumFrameSuggested);
    }

}
