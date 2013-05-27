/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_frames_drop.h"

SUITE(FrameDrop)
{
    TEST(NoDropRequired)
    {
        MockRender *render = new MockRender();
        FramesDrop<IMFXVideoRender> drop_render(0, 1, render);
        
        mfxFrameSurface1 surf[3] = {0};

        surf[0].Data.TimeStamp = 1;
        surf[1].Data.TimeStamp = 2;
        surf[2].Data.TimeStamp = 3;
        
        CHECK_EQUAL(MFX_ERR_NONE, drop_render.RenderFrame(surf, NULL));
        CHECK_EQUAL(MFX_ERR_NONE, drop_render.RenderFrame(surf + 1, NULL));
        CHECK_EQUAL(MFX_ERR_NONE, drop_render.RenderFrame(surf + 2, NULL));

        TEST_METHOD_TYPE(MockRender::RenderFrame) params;
        CHECK(render->_RenderFrame.WasCalled(&params));
        CHECK_EQUAL(1u, params.value0->Data.TimeStamp);
        CHECK(render->_RenderFrame.WasCalled(&params));
        CHECK_EQUAL(2u, params.value0->Data.TimeStamp);
        CHECK(render->_RenderFrame.WasCalled(&params));
        CHECK_EQUAL(3u, params.value0->Data.TimeStamp);
    }

    TEST(Drop_1_of_2)
    {
        MockRender *render = new MockRender();
        FramesDrop<IMFXVideoRender> drop_render(1, 2, render);

        mfxFrameSurface1 surf[5] = {0};

        surf[0].Data.TimeStamp = 1;
        surf[1].Data.TimeStamp = 2;
        surf[2].Data.TimeStamp = 3;
        surf[3].Data.TimeStamp = 4;
        surf[4].Data.TimeStamp = 5;

        CHECK_EQUAL(MFX_ERR_NONE, drop_render.RenderFrame(surf, NULL));
        CHECK_EQUAL(MFX_ERR_NONE, drop_render.RenderFrame(surf + 1, NULL));
        CHECK_EQUAL(MFX_ERR_NONE, drop_render.RenderFrame(surf + 2, NULL));
        CHECK_EQUAL(MFX_ERR_NONE, drop_render.RenderFrame(surf + 3, NULL));
        CHECK_EQUAL(MFX_ERR_NONE, drop_render.RenderFrame(surf + 4, NULL));

        TEST_METHOD_TYPE(MockRender::RenderFrame) params;
        CHECK(render->_RenderFrame.WasCalled(&params));
        CHECK_EQUAL(1u, params.value0->Data.TimeStamp);
        CHECK(render->_RenderFrame.WasCalled(&params));
        CHECK_EQUAL(3u, params.value0->Data.TimeStamp);
        CHECK(render->_RenderFrame.WasCalled(&params));
        CHECK_EQUAL(5u, params.value0->Data.TimeStamp);
    }
    
    TEST(Drop_1_of_3)
    {
        MockRender *render = new MockRender();
        FramesDrop<IMFXVideoRender> drop_render(1, 3, render);

        mfxFrameSurface1 surf[6] = {0};

        surf[0].Data.TimeStamp = 1;
        surf[1].Data.TimeStamp = 2;
        surf[2].Data.TimeStamp = 3;
        surf[3].Data.TimeStamp = 4;
        surf[4].Data.TimeStamp = 5;
        surf[5].Data.TimeStamp = 6;

        CHECK_EQUAL(MFX_ERR_NONE, drop_render.RenderFrame(surf, NULL));
        CHECK_EQUAL(MFX_ERR_NONE, drop_render.RenderFrame(surf + 1, NULL));
        CHECK_EQUAL(MFX_ERR_NONE, drop_render.RenderFrame(surf + 2, NULL));
        CHECK_EQUAL(MFX_ERR_NONE, drop_render.RenderFrame(surf + 3, NULL));
        CHECK_EQUAL(MFX_ERR_NONE, drop_render.RenderFrame(surf + 4, NULL));
        CHECK_EQUAL(MFX_ERR_NONE, drop_render.RenderFrame(surf + 5, NULL));

        TEST_METHOD_TYPE(MockRender::RenderFrame) params;
        CHECK(render->_RenderFrame.WasCalled(&params));
        CHECK_EQUAL(1u, params.value0->Data.TimeStamp);
        CHECK(render->_RenderFrame.WasCalled(&params));
        CHECK_EQUAL(2u, params.value0->Data.TimeStamp);
        CHECK(render->_RenderFrame.WasCalled(&params));
        CHECK_EQUAL(4u, params.value0->Data.TimeStamp);
        CHECK(render->_RenderFrame.WasCalled(&params));
        CHECK_EQUAL(5u, params.value0->Data.TimeStamp);

        //no more calls
        CHECK(!render->_RenderFrame.WasCalled());
    }

}