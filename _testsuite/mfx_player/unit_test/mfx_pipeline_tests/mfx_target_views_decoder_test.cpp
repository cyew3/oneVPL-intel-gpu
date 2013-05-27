/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_mvc_target_views_decoder.h"
#include "mfxmvc.h"

SUITE(TargetViewsDecoder)
{
    struct DecodeHeaderSetup : CleanTestEnvironment
    {
        mfxVideoParam vParamRet, vParam;
        std::vector<std::pair<mfxU16, mfxU16>> viewMap;

        //data that will be returned by mock decoder
        mfxExtBuffer    * pBuffers[1];
        
        mfxExtMVCSeqDesc  sequence;
        TEST_METHOD_TYPE(MockYUVSource::DecodeHeader) args_dec_header;

        //video params that application passed to DecodeFrameAsync
        mfxVideoParam     vParamsFromLocal;
        mfxExtBuffer    * pBuffersLocal[1];
        mfxExtMVCSeqDesc  sequenceLocal;

        mfxU16            viewIds[3];
        mfxMVCViewDependency dependency[3];
        mfxMVCOperationPoint operation_points;

        DecodeHeaderSetup()
            : sequence()
            , operation_points()
            , sequenceLocal()
            , vParamsFromLocal()
        {
            viewIds[0] = 0;
            viewIds[0] = 1;
            viewIds[0] = 3;

            sequence.Header.BufferId = MFX_EXTBUFF_MVC_SEQ_DESC;
            sequence.Header.BufferSz = sizeof(mfxExtMVCSeqDesc);
            sequence.NumViewId = 3;
            sequence.NumViewIdAlloc = 0;
            sequence.ViewId = viewIds;
            sequence.View = dependency;
            sequence.NumView = 3;
            sequence.NumViewAlloc = 0;
            sequence.NumOP = 1;
            sequence.NumOPAlloc = 0;
            sequence.OP = &operation_points;

            MFX_ZERO_MEM(dependency[0]);
            MFX_ZERO_MEM(dependency[1]);
            MFX_ZERO_MEM(dependency[2]);

            dependency[0].ViewId = 12;
            dependency[1].ViewId = 5;
            dependency[2].ViewId = 7;

            operation_points.NumTargetViews = 3;
            operation_points.TargetViewId = viewIds;


            pBuffers[0] = (mfxExtBuffer*)&sequence;

            vParamRet.NumExtParam = 1;
            vParamRet.ExtParam    = pBuffers;


            //generic setup for mockdecoder return
            args_dec_header.ret_val = MFX_ERR_NONE;
            args_dec_header.value0  = NULL;
            args_dec_header.value1  = &vParamRet;

            sequenceLocal.Header.BufferId = MFX_EXTBUFF_MVC_SEQ_DESC;
            sequenceLocal.Header.BufferSz = sizeof(mfxExtMVCSeqDesc);
            pBuffersLocal[0] = (mfxExtBuffer*)&sequenceLocal;
        }
        void SetupAllPossibleDependencies()
        {
            std::vector<std::pair<mfxU16, mfxU16> > initializer;
            initializer.push_back(std::make_pair(1, 2));
            initializer.push_back(std::make_pair(0, 2));
            initializer.push_back(std::make_pair(0, 1));

            for (size_t i = 0; i< initializer.size(); i++)
            {
                //create whole box of deoendencies
                dependency[i].NumNonAnchorRefsL0 = 2;
                dependency[i].NumNonAnchorRefsL1 = 2;
                dependency[i].NumAnchorRefsL0 = 2;
                dependency[i].NumAnchorRefsL1 = 2;

                dependency[i].NonAnchorRefL0[0] = dependency[initializer[i].first].ViewId;
                dependency[i].NonAnchorRefL0[1] = dependency[initializer[i].second].ViewId;

                dependency[i].NonAnchorRefL1[0] = dependency[initializer[i].first].ViewId;
                dependency[i].NonAnchorRefL1[1] = dependency[initializer[i].second].ViewId;

                dependency[i].AnchorRefL0[0] = dependency[initializer[i].first].ViewId;
                dependency[i].AnchorRefL0[1] = dependency[initializer[i].second].ViewId;

                dependency[i].AnchorRefL1[0] = dependency[initializer[i].first].ViewId;
                dependency[i].AnchorRefL1[1] = dependency[initializer[i].second].ViewId;
            }
        }
    };

    TEST_FIXTURE(DecodeHeaderSetup, passing_null_bs_to_YUV_source)
    {
        //setting up init parameters
        viewMap.push_back(std::pair<mfxU16, mfxU16>(0, 5));
        viewMap.push_back(std::pair<mfxU16, mfxU16>(1, 6));

        MockYUVSource *pMockSource = new MockYUVSource();

        args_dec_header.ret_val = MFX_ERR_MORE_DATA;
        pMockSource->_DecodeHeader.WillReturn(args_dec_header);

        TargetViewsDecoder * pDecoder = new TargetViewsDecoder(viewMap, 0, pMockSource);

        CHECK_EQUAL(MFX_ERR_MORE_DATA, pDecoder->DecodeHeader(NULL, &vParamsFromLocal));
    }

    TEST_FIXTURE(DecodeHeaderSetup, warning_status_returned_from_mock)
    {
        //setting up init parameters
        viewMap.push_back(std::pair<mfxU16, mfxU16>(0, 5));

        MockYUVSource *pMockSource = new MockYUVSource();

        args_dec_header.ret_val = MFX_WRN_PARTIAL_ACCELERATION;
        sequence.NumViewIdAlloc = 3; //return value matches to requested = completely whitebox
        sequence.NumViewAlloc = 3;
        sequence.NumOPAlloc = 1; 
        pMockSource->_DecodeHeader.WillReturn(args_dec_header);

        TargetViewsDecoder * pDecoder = new TargetViewsDecoder(viewMap, 0, pMockSource);

        vParamsFromLocal.mfx.CodecProfile = 100;

        //decode header should clear profile since it is AVC
        CHECK_EQUAL(MFX_WRN_PARTIAL_ACCELERATION, pDecoder->DecodeHeader(NULL, &vParamsFromLocal));
        CHECK_EQUAL(0u, vParamsFromLocal.mfx.CodecProfile);
    }


    TEST_FIXTURE(DecodeHeaderSetup, passing_valid_ext_buffers_to_target_decode_header)
    {
        //setting up init parameters
        viewMap.push_back(std::pair<mfxU16, mfxU16>(0, 5));
        viewMap.push_back(std::pair<mfxU16, mfxU16>(1, 6));

        MockYUVSource *pMockSource = new MockYUVSource();

        args_dec_header.ret_val = MFX_ERR_NOT_ENOUGH_BUFFER;
        pMockSource->_DecodeHeader.WillReturn(args_dec_header);

        args_dec_header.ret_val = MFX_ERR_NONE; 
        sequence.NumViewIdAlloc = 3; //return value matches to requested = completely whitebox
        sequence.NumViewAlloc = 3;
        sequence.NumOPAlloc = 1; 
        pMockSource->_DecodeHeader.WillReturn(args_dec_header);

        TargetViewsDecoder * pDecoder = new TargetViewsDecoder(viewMap, 0, pMockSource);

        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->DecodeHeader(NULL, &vParamsFromLocal));
        
        TEST_METHOD_TYPE(MockYUVSource::DecodeHeader) args_passed_to_mock;
        CHECK_EQUAL(true, pMockSource->_DecodeHeader.WasCalled(&args_passed_to_mock));
        CHECK_EQUAL(1, args_passed_to_mock.value1->NumExtParam);
        
        MFXExtBufferPtr<mfxExtMVCSeqDesc> sequence(*args_passed_to_mock.value1);
        CHECK_EQUAL(0u, sequence.get()->NumViewIdAlloc);
        
        pMockSource->_DecodeHeader.WasCalled(&args_passed_to_mock);
        MFXExtBufferPtr<mfxExtMVCSeqDesc> sequence2(*args_passed_to_mock.value1);
        CHECK_EQUAL(3u, sequence2.get()->NumViewIdAlloc);

    }

    TEST_FIXTURE(DecodeHeaderSetup, passing_valid_ext_buffers_to_target_Init)
    {
        //data from command line
        viewMap.push_back(std::pair<mfxU16, mfxU16>(0, 5));
        viewMap.push_back(std::pair<mfxU16, mfxU16>(2, 6));

        MockYUVSource *pMockSource = new MockYUVSource();

        args_dec_header.ret_val = MFX_ERR_NOT_ENOUGH_BUFFER;
        pMockSource->_DecodeHeader.WillReturn(args_dec_header);

        args_dec_header.ret_val = MFX_ERR_NONE;
        sequence.NumViewIdAlloc = 3; //return value matches to requested = completely whitebox
        sequence.NumView = 2;
        sequence.NumViewAlloc = 2;
        pMockSource->_DecodeHeader.WillReturn(args_dec_header);

        TargetViewsDecoder * pDecoder = new TargetViewsDecoder(viewMap, 7, pMockSource);

        //checking for target view overflows
        CHECK_EQUAL(MFX_ERR_UNKNOWN, pDecoder->DecodeHeader(NULL, &vParamsFromLocal));
        
        //correcting mock yuv source behavior to return 3 views
        sequence.NumView = 3;
        sequence.NumViewAlloc = 3;
        pMockSource->_DecodeHeader.WillReturn(args_dec_header);

        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->DecodeHeader(NULL, &vParamsFromLocal));
        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->Init(&vParamsFromLocal));

        TEST_METHOD_TYPE(MockYUVSource::Init) args_passed_to_mock;
        CHECK_EQUAL(true, pMockSource->_Init.WasCalled(&args_passed_to_mock));
        CHECK_EQUAL(2, args_passed_to_mock.value0->NumExtParam);

        MFXExtBufferPtr<mfxExtMVCSeqDesc> sequence(*args_passed_to_mock.value0);
        CHECK_EQUAL(3u, sequence.get()->NumViewIdAlloc);
        CHECK_EQUAL(3u, sequence.get()->NumViewId);
        CHECK_EQUAL(12, sequence.get()->View[0].ViewId);
        CHECK_EQUAL(5, sequence.get()->View[1].ViewId);
        CHECK_EQUAL(7, sequence.get()->View[2].ViewId);

        //checking for targetview buffers attached as well
        MFXExtBufferPtr<mfxExtMVCTargetViews> targetViews(*args_passed_to_mock.value0);
        CHECK_EQUAL(2u, targetViews->NumView);
        CHECK_EQUAL(12, targetViews->ViewId[0]);
        CHECK_EQUAL(7, targetViews->ViewId[1]);
        CHECK_EQUAL(7, targetViews->TemporalId);

    }

    TEST_FIXTURE(DecodeHeaderSetup, DecodeHeader_MVC_to_MVC_returned_ext_buffers_with_dependencies)
    {
        //setting up init parameters
        viewMap.push_back(std::pair<mfxU16, mfxU16>(0, 5));
        viewMap.push_back(std::pair<mfxU16, mfxU16>(1, 6));

        MockYUVSource *pMockSource = new MockYUVSource();

        args_dec_header.ret_val = MFX_ERR_NOT_ENOUGH_BUFFER;
        pMockSource->_DecodeHeader.WillReturn(args_dec_header);

        args_dec_header.ret_val = MFX_ERR_NONE; 
        sequence.NumViewIdAlloc = 3; //return value matches to requested = completely whitebox
        sequence.NumViewAlloc = 3;
        sequence.NumOPAlloc = 1; 
        pMockSource->_DecodeHeader.WillReturn(args_dec_header);

        TargetViewsDecoder * pDecoder = new TargetViewsDecoder(viewMap, 0, pMockSource);

        mfxU16               _viewIds[3];
        mfxMVCViewDependency _dependency[3];
        mfxMVCOperationPoint _operation_points[1];

        mfxExtMVCSeqDesc  _sequence = {0};

        _sequence.Header.BufferSz = sizeof(mfxExtMVCSeqDesc);
        _sequence.Header.BufferId = BufferIdOf<mfxExtMVCSeqDesc>::id;

        mfxExtBuffer* buf[] = {(mfxExtBuffer*)&_sequence};

        vParamsFromLocal.NumExtParam = MFX_ARRAY_SIZE(buf) ;
        vParamsFromLocal.ExtParam = buf;

        SetupAllPossibleDependencies();

        CHECK_EQUAL(MFX_ERR_NOT_ENOUGH_BUFFER, pDecoder->DecodeHeader(NULL, &vParamsFromLocal));

        _sequence.ViewId = _viewIds;
        _sequence.View = _dependency;
        _sequence.OP = _operation_points;

        _sequence.NumViewId = 2;
        _sequence.NumView = 2;
        _sequence.NumOP = 1;
        
        _sequence.NumViewIdAlloc = 2;
        _sequence.NumViewAlloc = 2;
        _sequence.NumOPAlloc = 1;

        //again two call to mock object will be performed
        args_dec_header.ret_val = MFX_ERR_NOT_ENOUGH_BUFFER;
        pMockSource->_DecodeHeader.WillReturn(args_dec_header);

        args_dec_header.ret_val = MFX_ERR_NONE; 
        pMockSource->_DecodeHeader.WillReturn(args_dec_header);

        
        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->DecodeHeader(NULL, &vParamsFromLocal));

        //////////////////////////////////////////////////////////////////////////
        //checking that obtained structure doesn't have invalid dependencies
        
        CHECK_EQUAL(1, vParamsFromLocal.NumExtParam);
        mfxExtMVCSeqDesc * seqDescription = (mfxExtMVCSeqDesc *)vParamsFromLocal.ExtParam[0];
        CHECK_EQUAL(seqDescription , &_sequence);
        
        CHECK_EQUAL(2u, seqDescription->NumView);
        CHECK_EQUAL(2u, seqDescription->NumViewId);
        CHECK_EQUAL(1u, seqDescription->NumOP);

        CHECK_EQUAL(5u, seqDescription->View[0].ViewId);
        mfxU16 dependency[16] = {6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

        CHECK_EQUAL(1u, seqDescription->View[0].NumAnchorRefsL0);
        CHECK_EQUAL(1u, seqDescription->View[0].NumAnchorRefsL1);
        CHECK_EQUAL(1u, seqDescription->View[0].NumNonAnchorRefsL0);
        CHECK_EQUAL(1u, seqDescription->View[0].NumNonAnchorRefsL1);

        CHECK_EQUAL(0, memcmp(seqDescription->View[0].AnchorRefL0, dependency, 16));
        CHECK_EQUAL(0, memcmp(seqDescription->View[0].AnchorRefL1, dependency, 16));
        CHECK_EQUAL(0, memcmp(seqDescription->View[0].NonAnchorRefL0, dependency, 16));
        CHECK_EQUAL(0, memcmp(seqDescription->View[0].NonAnchorRefL1, dependency, 16));


        CHECK_EQUAL(6u, seqDescription->View[1].ViewId);

        CHECK_EQUAL(1u, seqDescription->View[1].NumAnchorRefsL0);
        CHECK_EQUAL(1u, seqDescription->View[1].NumAnchorRefsL1);
        CHECK_EQUAL(1u, seqDescription->View[1].NumNonAnchorRefsL0);
        CHECK_EQUAL(1u, seqDescription->View[1].NumNonAnchorRefsL1);

        dependency[0] = 5;
        CHECK_EQUAL(0, memcmp(seqDescription->View[1].AnchorRefL0, dependency, 16));
        CHECK_EQUAL(0, memcmp(seqDescription->View[1].AnchorRefL1, dependency, 16));
        CHECK_EQUAL(0, memcmp(seqDescription->View[1].NonAnchorRefL0, dependency, 16));
        CHECK_EQUAL(0, memcmp(seqDescription->View[1].NonAnchorRefL1, dependency, 16));

        CHECK_EQUAL(5u, seqDescription->ViewId[0]);
        CHECK_EQUAL(6u, seqDescription->ViewId[1]);

        CHECK_EQUAL(2u, seqDescription->OP[0].NumViews);
        CHECK_EQUAL(5u, seqDescription->OP[0].TargetViewId[0]);
        CHECK_EQUAL(6u, seqDescription->OP[0].TargetViewId[1]);
    }

    struct DecodeFrameAsyncSetup : DecodeHeaderSetup
    {
        mfxBitstream2 zero;

        TEST_METHOD_TYPE(MockYUVSource::DecodeFrameAsync) args_passed_from_dfa;
        mfxVideoParam *pvParamRet;

        //decodeframeasync uses pointer to pointer
        mfxFrameSurface1 surface1, surface2, surface3;
        mfxFrameSurface1 *pSurface1, *pSurface2, *pSurface3;
        TEST_METHOD_TYPE(MockYUVSource::DecodeFrameAsync) args1, args2, args3;

        DecodeFrameAsyncSetup()
            : pSurface1(&surface1)
            , pSurface2(&surface2)
            , pSurface3(&surface3)
            , zero()
            , surface1()
            , surface2()
            , surface3()
        {
            zero.isNull = true;

            surface1.Info.FrameId.ViewId = 12;
            surface2.Info.FrameId.ViewId = 5;
            surface3.Info.FrameId.ViewId = 7;

            args1.ret_val = MFX_ERR_NONE;
            args1.value0  = NULL;
            args1.value1  = &surface1;
            args1.value2  = &pSurface1;
            args1.value3  = NULL;

            args2.ret_val = MFX_ERR_NONE;
            args2.value0  = NULL;
            args2.value1  = &surface2;
            args2.value2  = &pSurface2;
            args2.value3  = NULL;

            args3.ret_val = MFX_ERR_NONE;
            args3.value0  = NULL;
            args3.value1  = &surface3;
            args3.value2  = &pSurface3;
            args3.value3  = NULL;
        }
    };

    TEST_FIXTURE(DecodeFrameAsyncSetup, DecodeFrameAsync_CorrectBehavior)
    {
        viewMap.push_back(std::pair<mfxU16, mfxU16>(0, 5));
        viewMap.push_back(std::pair<mfxU16, mfxU16>(1, 6));

        //setting up mock decoder
        MockYUVSource *pMockSource = new MockYUVSource();

        
        //we need to configure decode header as well
        args_dec_header.ret_val = MFX_ERR_NOT_ENOUGH_BUFFER;
        pMockSource->_DecodeHeader.WillReturn(args_dec_header);

        args_dec_header.ret_val = MFX_ERR_NONE;
        sequence.NumViewIdAlloc = 3; //return value matches to requested = completely whitebox
        sequence.NumViewAlloc = 3;
        sequence.NumOPAlloc = 1;
        pMockSource->_DecodeHeader.WillReturn(args_dec_header);

        //creating test unit
        TargetViewsDecoder * pDecoder = new TargetViewsDecoder(viewMap, 0, pMockSource);

        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->DecodeHeader(NULL, &vParamsFromLocal));

        pMockSource->_DecodeFrameAsync.WillReturn(args1);
        pMockSource->_DecodeFrameAsync.WillReturn(args2);
        
        //init not required by design since we only care of proper views id mapping
        //CHECK_EQUAL(MFX_ERR_NONE, pDecoder->Init(&vParamsFromLocal));

        mfxFrameSurface1 *ret_surface = NULL;
        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->DecodeFrameAsync(zero, NULL, &ret_surface, NULL));
        CHECK_EQUAL(5, ret_surface->Info.FrameId.ViewId );

        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->DecodeFrameAsync(zero, NULL, &ret_surface, NULL));
        CHECK_EQUAL(6, ret_surface->Info.FrameId.ViewId );
    }

    TEST_FIXTURE(DecodeFrameAsyncSetup, DecodeFrameAsync_ViewIdNotInMap)
    {
        //setting up init parameters
        viewMap.push_back(std::pair<mfxU16, mfxU16>(0, 5));
        viewMap.push_back(std::pair<mfxU16, mfxU16>(1, 6));

        //setting up mock decoder
        MockYUVSource *pMockSource = new MockYUVSource();

        pMockSource->_DecodeFrameAsync.WillReturn(args3);

        //creating test unit
        TargetViewsDecoder * pDecoder = new TargetViewsDecoder(viewMap, 0, pMockSource);
        mfxFrameSurface1 *ret_surface;
        CHECK(MFX_ERR_NONE != pDecoder->DecodeFrameAsync(zero, NULL, &ret_surface, NULL));
    }

#if 0
    TEST_FIXTURE(DecodeHeaderSetup, IncorrectMapProvided)
    {
        args_dec_header.ret_val = MFX_ERR_NOT_ENOUGH_BUFFER;

        MockYUVSource *pMockSource = new MockYUVSource();
        pMockSource->_DecodeHeader.WillReturn(args_dec_header);

        viewMap.push_back(std::pair<mfxU16, mfxU16>(8, 6));

        TargetViewsDecoder * pDecoder = new TargetViewsDecoder(viewMap, pMockSource);

        CHECK_EQUAL(MFX_ERR_UNKNOWN, pDecoder->DecodeHeader(NULL, &vParamsFromLocal));
    }

    TEST_FIXTURE(DecodeHeaderSetup, buffers_smaller_than_target_buffers)
    {
        args_dec_header.ret_val = MFX_ERR_NOT_ENOUGH_BUFFER;

        MockYUVSource *pMockSource = new MockYUVSource();
        pMockSource->_DecodeHeader.WillReturn(args_dec_header);
        
        //test object
        TargetViewsDecoder * pDecoder = new TargetViewsDecoder(viewMap, pMockSource);

        //allocating smaller buffer than target view decoder/also smaller then mock decoder
        sequenceLocal.NumViewId        = 0;
        sequenceLocal.NumViewIdAlloc   = 1;
        vParamsFromLocal.NumExtParam   = 1;
        vParamsFromLocal.ExtParam      = pBuffersLocal;
        
        CHECK_EQUAL(MFX_ERR_NOT_ENOUGH_BUFFER, pDecoder->DecodeHeader(NULL, &vParamsFromLocal));
    }
    
    TEST_FIXTURE(DecodeHeaderSetup, buffers_smaller_than_mock_buffers)
    {
        args_dec_header.ret_val = MFX_ERR_NOT_ENOUGH_BUFFER;

        MockYUVSource *pMockSource = new MockYUVSource();
        pMockSource->_DecodeHeader.WillReturn(args_dec_header);

        //test object
        TargetViewsDecoder * pDecoder = new TargetViewsDecoder(viewMap, pMockSource);

        //allocating smaller buffer that mock decoder , however enough for target view decoder
        sequenceLocal.NumViewId        = 0;
        sequenceLocal.NumViewIdAlloc   = 2;
        vParamsFromLocal.NumExtParam   = 1;
        vParamsFromLocal.ExtParam      = pBuffersLocal;
        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->DecodeHeader(NULL, &vParamsFromLocal));
    }

    TEST_FIXTURE(DecodeHeaderSetup, buffers_enough)
    {
        MockYUVSource *pMockSource = new MockYUVSource();
        pMockSource->_DecodeHeader.WillReturn(args_dec_header);

        //test object
        TargetViewsDecoder * pDecoder = new TargetViewsDecoder(viewMap, pMockSource);

        //allocating buffers enough for mock decoder
        sequenceLocal.NumViewId        = 0;
        sequenceLocal.NumViewIdAlloc   = 3;
        vParamsFromLocal.NumExtParam   = 1;
        vParamsFromLocal.ExtParam      = pBuffersLocal;

        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->DecodeHeader(NULL, &vParamsFromLocal));

        //Check stage
        //1 - we need to check arguments passed to mock decoder
        TEST_METHOD_TYPE(MockYUVSource::DecodeHeader) args_passed_to_mock;
        CHECK(pMockSource->_DecodeHeader.WasCalled(&args_passed_to_mock));

        CHECK(NULL == args_passed_to_mock.value0);
        CHECK(&vParamsFromLocal == args_passed_to_mock.value1);
        
        //taregtviedecoder.decoheader will also change returned values to application
        CHECK_EQUAL(1, vParamsFromLocal.NumExtParam);

        mfxExtMVCSeqDesc &seq_ret = (mfxExtMVCSeqDesc &)*(vParamsFromLocal.ExtParam[0]);

        CHECK(MFX_EXTBUFF_MVC_SEQ_DESC == seq_ret.Header.BufferId);
        CHECK_EQUAL(sizeof(mfxExtMVCSeqDesc), seq_ret.Header.BufferSz);
        CHECK_EQUAL(2u, seq_ret.NumViewId);
        CHECK_EQUAL(seq_ret.ViewId[0], 0);
        CHECK_EQUAL(seq_ret.ViewId[1], 3);
    }
#endif
}
