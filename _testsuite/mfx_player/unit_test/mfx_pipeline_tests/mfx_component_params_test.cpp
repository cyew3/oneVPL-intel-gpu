/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_component_params.h"
#include "mock_mfx_pipeline_factory.h"

SUITE(ComponentParams1)
{
    struct init
    {
        MockPipelineFactory mcfac;
        std::auto_ptr<ComponentParams> cparams2;
        mfxFrameSurface1 surfaces[10];
        SrfEncCtl pSurfaceEncCtrl;
        MockVideoSession mockSession;
        

        init()
        {
            mcfac._CreateVideoSession.WillReturn(TEST_METHOD_TYPE(MockPipelineFactory::CreateVideoSession)(MakeUndeletable(mockSession)));
            cparams2.reset(new ComponentParams(VM_STRING(""), VM_STRING(""), &mcfac));
            mfxFrameAllocRequest request = {0};
            request.Info.Width = 1;
            request.Info.Height = 1;

            ComponentParams::SurfacesAllocated &srfAllocated =  cparams2->RegisterAlloc(request);

            MFX_ZERO_MEM(surfaces);
            for(size_t i = 0; i < MFX_ARRAY_SIZE(surfaces); i++)
            {
                srfAllocated.surfaces.push_back(SrfEncCtl(&surfaces[i]));
            }
        }
    };

    TEST_FIXTURE(init, FindFreeSurface_predefined_random)
    {
        MockRandom * myRand = new MockRandom();
        myRand->_rand.WillReturn(Hold(5u));
        myRand->_rand.WillReturn(Hold(4u));
        myRand->_rand.WillReturn(Hold(3u));

        //changing rand max to proper fit to 
        myRand->_rand_max.WillReturn(Hold(9u));
        myRand->_rand_max.WillReturn(Hold(8u));
        myRand->_rand_max.WillReturn(Hold(7u));

        cparams2->m_nSelectAlgo = USE_RANDOM;
        cparams2->m_pRandom=myRand;

        //prepare pool
        for(size_t i = 0; i < MFX_ARRAY_SIZE(surfaces); i++)
        {
            surfaces[i].Data.MemId = (mfxMemId)i;
        }

        //marking surfaces as busy 
        surfaces[0].Data.Locked = 1;
        //surfaces[1].Data.Locked = 1; -- this surfaces will be selected after 2 attempts
        surfaces[5].Data.Locked = 1;
        surfaces[3].Data.Locked = 1;
        

        cparams2->FindFreeSurface(0, &pSurfaceEncCtrl, NULL);

        CHECK_EQUAL((mfxMemId)1, pSurfaceEncCtrl.pSurface->Data.MemId);
    }

    TEST_FIXTURE(init, FindFreeSurface_generic_random)
    {
        cparams2->m_pRandom = mfx_make_shared<DefaultRandom>();
        cparams2->m_nSelectAlgo = USE_RANDOM;

        //prepare pool
        for(size_t i = 0; i < MFX_ARRAY_SIZE(surfaces); i++)
        {
            surfaces[i].Data.MemId  = (mfxMemId)i;
            surfaces[i].Data.Locked = 0;
        }
        //checking for actual random
        bool bRandom = false;

        //at least one selection is random
        for (int j = 0; j < MFX_ARRAY_SIZE(surfaces); j++)
        {
            cparams2->FindFreeSurface(0, &pSurfaceEncCtrl, NULL);
            if (pSurfaceEncCtrl.pSurface->Data.MemId != (mfxMemId)j)
            {
                bRandom = true;
                break;
            }
            pSurfaceEncCtrl.pSurface->Data.Locked = 1;
        }
        CHECK(bRandom);

    }

    TEST_FIXTURE(init, FindFreeSurface_random_performance)
    {
        MockRandom2 * mRandom = new MockRandom2();
        MockRender render;
        cparams2->m_pRandom = mRandom;
        cparams2->m_nSelectAlgo = USE_RANDOM;

        //prepare pool
        size_t i;
        for( i = 0; i < MFX_ARRAY_SIZE(surfaces); i++)
        {
            surfaces[i].Data.MemId  = (mfxMemId)i;
            surfaces[i].Data.Locked = 1;
        }

        //render required in order to exit from infinite waiting, it requires only once
        ParameterHolder1<mfxStatus, mfxU32> hld (MFX_ERR_DEVICE_FAILED, (mfxU32) 0);
        render._WaitTasks.WillDefaultReturn(&hld);

        //surface not found and only 9 times random gets executed
        CHECK(MFX_ERR_NONE != cparams2->FindFreeSurface(0, &pSurfaceEncCtrl, &render));
        CHECK(mRandom->Calls() == MFX_ARRAY_SIZE(surfaces) - 1);

        
        //any surface can be found, cannot test actual performance, only by counting number of rand() queries
        for (i = 0; i < MFX_ARRAY_SIZE(surfaces); i++)
        {
            //unlocking each surface and testing for it
            surfaces[i].Data.Locked = 0;
            mRandom->Calls() = 0;
            CHECK_EQUAL(MFX_ERR_NONE, cparams2->FindFreeSurface(0, &pSurfaceEncCtrl, &render));
            CHECK_EQUAL(0, pSurfaceEncCtrl.pSurface->Data.Locked);

            //checking number of random calls it should be less then array len for every element
            CHECK(mRandom->Calls() < MFX_ARRAY_SIZE(surfaces));
     
            //unlock this surfaces
            surfaces[i].Data.Locked = 1;
        }
    }
    TEST_FIXTURE(init, FindFreeSurface_SelectOldest)
    {
        cparams2->m_nSelectAlgo = USE_OLDEST_DIRECT;
        //prepare pool
        size_t i;
        for( i = 0; i < MFX_ARRAY_SIZE(surfaces); i++)
        {
            surfaces[i].Data.MemId  = (mfxMemId)i;
            surfaces[i].Data.Locked = 0;
        }

        //lets lock something
        surfaces[4].Data.Locked = 5;
        surfaces[8].Data.Locked = 5;

        int j = 0;
        //taking new surface in sequential order
        for (i = 0; i < 2 * MFX_ARRAY_SIZE(surfaces); i++, j = (j+1) % MFX_ARRAY_SIZE(surfaces))
        {
            CHECK_EQUAL(MFX_ERR_NONE, cparams2->FindFreeSurface(0, &pSurfaceEncCtrl, NULL));
            if (j == 4 || j==8)
            {
                j = (j+1) % MFX_ARRAY_SIZE(surfaces);
            }
            CHECK_EQUAL((mfxMemId)j, pSurfaceEncCtrl.pSurface->Data.MemId);
        }
    }

    TEST_FIXTURE(init, FindFreeSurface_SelectOldest_inverse)
    {
        cparams2->m_nSelectAlgo = USE_OLDEST_REVERSE;
        //prepare pool
        size_t i;
        for( i = 0; i < MFX_ARRAY_SIZE(surfaces); i++)
        {
            surfaces[i].Data.MemId  = (mfxMemId)i;
            surfaces[i].Data.Locked = 0;
        }

        //lets lock something
        surfaces[4].Data.Locked = 5;
        surfaces[8].Data.Locked = 5;

        int j = MFX_ARRAY_SIZE(surfaces)-1;
        //taking new surface in sequential order
        for (i = 0; i < 2 * MFX_ARRAY_SIZE(surfaces); i++, j = (0 == j) ? MFX_ARRAY_SIZE(surfaces)-1 : j - 1)
        {
            CHECK_EQUAL(MFX_ERR_NONE, cparams2->FindFreeSurface(0, &pSurfaceEncCtrl, NULL));
            if (j == 4 || j==8)
            {
                j = (0 == j) ? MFX_ARRAY_SIZE(surfaces) - 1 : j-1 ;
            }
            CHECK_EQUAL((mfxMemId)j, pSurfaceEncCtrl.pSurface->Data.MemId);
        }
    }

    TEST_FIXTURE(init, FindFreeSurface_generic)
    {
        //prepare pool
        size_t i;
        for( i = 0; i < MFX_ARRAY_SIZE(surfaces); i++)
        {
            surfaces[i].Data.MemId  = (mfxMemId)i;
            surfaces[i].Data.Locked = 0;
        }

        //taking first new surface 
        for (i = 0; i < 3; i++)
        {
            CHECK_EQUAL(MFX_ERR_NONE, cparams2->FindFreeSurface(0, &pSurfaceEncCtrl, NULL));
            CHECK_EQUAL((mfxMemId)0, pSurfaceEncCtrl.pSurface->Data.MemId);
        }
        //lets lock something
        surfaces[0].Data.Locked = 5;
        surfaces[1].Data.Locked = 5;

        //taking first new surface 
        for (i = 0; i < 3; i++)
        {
            CHECK_EQUAL(MFX_ERR_NONE, cparams2->FindFreeSurface(0, &pSurfaceEncCtrl, NULL));
            CHECK_EQUAL((mfxMemId)2u, pSurfaceEncCtrl.pSurface->Data.MemId);
        }
        surfaces[2].Data.Locked = 5;
        //taking first new surface 
        for (i = 0; i < 3; i++)
        {
            CHECK_EQUAL(MFX_ERR_NONE, cparams2->FindFreeSurface(0, &pSurfaceEncCtrl, NULL));
            CHECK_EQUAL((mfxMemId)3u, pSurfaceEncCtrl.pSurface->Data.MemId);
        }
        surfaces[3].Data.Locked = 5;
        //taking first new surface 
        for (i = 0; i < 3; i++)
        {
            CHECK_EQUAL(MFX_ERR_NONE, cparams2->FindFreeSurface(0, &pSurfaceEncCtrl, NULL));
            CHECK_EQUAL((mfxMemId)4u, pSurfaceEncCtrl.pSurface->Data.MemId);
        }
    }

}