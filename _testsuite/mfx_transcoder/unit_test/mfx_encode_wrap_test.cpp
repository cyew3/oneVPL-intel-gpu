/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_encoder_wrap.h"
#include "mfx_field_output_encode.h"
#include "mock_mfx_pipeline_factory.h"

SUITE(EncodeWrap)
{
    struct Init : public CleanTestEnvironment
    {
        MockPipelineFactory mkfac;
    };
    TEST_FIXTURE(Init, RenderFrame_ViewOutput_SyncMode)
    {
        TEST_METHOD_TYPE(MockVideoEncode::EncodeFrameAsync) arg;

        MockFile *mock_file = new MockFile();
        TEST_METHOD_TYPE(MockFile::isOpen) default_true(true);
        mock_file->_isOpen.WillDefaultReturn(&default_true);

        ComponentParams cParams(VM_STRING(""), VM_STRING(""), &mkfac);
        MockVideoEncode *mock_encode = new MockVideoEncode();
        std::auto_ptr<IVideoEncode> tmp(mock_encode);
        MFXEncodeWRAPPER encWrapper(cParams, NULL, tmp);

        //only setdownstream required
        encWrapper.SetDownStream(mock_file);

        mfxFrameSurface1 Surf = {0};
        mfxSyncPoint syncp = reinterpret_cast<mfxSyncPoint>(1);
        mfxSyncPoint syncp2 = reinterpret_cast<mfxSyncPoint>(2);
        Surf.Data.TimeStamp = 3;

        arg.ret_val= MFX_ERR_MORE_BITSTREAM;
        arg.value0 = NULL;  
        arg.value1 = NULL;  
        arg.value2 = NULL;  
        arg.value3 = &syncp;  

        mock_encode->_EncodeFrameAsync.WillReturn(arg);

        arg.ret_val = MFX_ERR_NONE;
        arg.value3 = &syncp2;  
        mock_encode->_EncodeFrameAsync.WillReturn(arg);

        CHECK_EQUAL(MFX_ERR_NONE, encWrapper.RenderFrame(&Surf, NULL));

        TEST_METHOD_TYPE(MockVideoEncode::EncodeFrameAsync) args_called, args_called_second;

        //EFA was called only once by generis encoder wrapper in case of field output
        CHECK_EQUAL(true, mock_encode->_EncodeFrameAsync.WasCalled(&args_called));
        CHECK_EQUAL(false, mock_encode->_EncodeFrameAsync.WasCalled(NULL));
    }

    TEST_FIXTURE(Init, RenderFrame_FieldOutput_SyncMode)
    {
        TEST_METHOD_TYPE(MockVideoEncode::EncodeFrameAsync) arg;
        
        
        MockFile *mock_file = new MockFile();
        TEST_METHOD_TYPE(MockFile::isOpen) default_true(true);
        mock_file->_isOpen.WillDefaultReturn(&default_true);

        ComponentParams cParams(VM_STRING(""), VM_STRING(""), &mkfac);
        MockVideoEncode *mock_encode = new MockVideoEncode();
        std::auto_ptr<IVideoEncode> tmp(mock_encode);
        tmp.reset(new FieldOutputEncoder(tmp));
        MFXEncodeWRAPPER encWrapper(cParams, NULL, tmp);

        //only setdownstream required
        encWrapper.SetDownStream(mock_file);

        mfxFrameSurface1 Surf = {0};
        mfxSyncPoint syncp = reinterpret_cast<mfxSyncPoint>(1);
        mfxSyncPoint syncp2 = reinterpret_cast<mfxSyncPoint>(2);
        Surf.Data.TimeStamp = 3;

        arg.ret_val= MFX_ERR_MORE_BITSTREAM;
        arg.value0 = NULL;  
        arg.value1 = NULL;  
        arg.value2 = NULL;  
        arg.value3 = &syncp;  

        mock_encode->_EncodeFrameAsync.WillReturn(arg);

        arg.ret_val = MFX_ERR_NONE;
        arg.value3 = &syncp2;  
        mock_encode->_EncodeFrameAsync.WillReturn(arg);

        CHECK_EQUAL(MFX_ERR_NONE, encWrapper.RenderFrame(&Surf, NULL));

        TEST_METHOD_TYPE(MockVideoEncode::EncodeFrameAsync) args_called, args_called_second;

        //EFA was called twice in case of field output
        CHECK_EQUAL(true, mock_encode->_EncodeFrameAsync.WasCalled(&args_called));
        CHECK_EQUAL(true, mock_encode->_EncodeFrameAsync.WasCalled(&args_called_second));
        CHECK_EQUAL(false, mock_encode->_EncodeFrameAsync.WasCalled(NULL));

        //syncoperation was called 2 times due to implementation
        TEST_METHOD_TYPE(MockVideoEncode::SyncOperation) args_syncpoint;
        CHECK_EQUAL(true, mock_encode->_SyncOperation.WasCalled(&args_syncpoint));
        CHECK_EQUAL(reinterpret_cast<void*>(1), reinterpret_cast<void*>(args_syncpoint.value0));
        CHECK_EQUAL(TimeoutVal<>::val(), args_syncpoint.value1); //real whole time waiting

        //CHECK_EQUAL(true, mock_encode->_SyncOperation.WasCalled(&args_syncpoint));
        //CHECK_EQUAL(reinterpret_cast<void*>(1), reinterpret_cast<void*>(args_syncpoint.value0));
        //CHECK_EQUAL(TimeoutVal<>::val(), args_syncpoint.value1); //real whole time waiting since no bitstreams ready

        //CHECK_EQUAL(true, mock_encode->_SyncOperation.WasCalled(&args_syncpoint));
        //CHECK_EQUAL(reinterpret_cast<void*>(1), reinterpret_cast<void*>(args_syncpoint.value0));
        //CHECK_EQUAL(0u, args_syncpoint.value1); //just checking prior filewriting
        
        CHECK_EQUAL(true, mock_encode->_SyncOperation.WasCalled(&args_syncpoint));
        CHECK_EQUAL(reinterpret_cast<void*>(2), reinterpret_cast<void*>(args_syncpoint.value0));
        CHECK_EQUAL(TimeoutVal<>::val(), args_syncpoint.value1); //wait for completion

        CHECK_EQUAL(false, mock_encode->_SyncOperation.WasCalled());

        
        //same surface passed twice
        CHECK_EQUAL(3u, args_called.value1->Data.TimeStamp );
        CHECK_EQUAL(3u, args_called_second.value1->Data.TimeStamp );

        //same bitstream data pointer, since it is a sync mode
        CHECK_EQUAL(args_called.value2->Data, args_called_second.value2->Data);
    }

    TEST_FIXTURE(Init, RenderFrame_FieldOutput_ASYNC_Mode)
    {
        TEST_METHOD_TYPE(MockVideoEncode::EncodeFrameAsync) arg;


        MockFile *mock_file = new MockFile();
        TEST_METHOD_TYPE(MockFile::isOpen) default_true(true);
        mock_file->_isOpen.WillDefaultReturn(&default_true);

        ComponentParams cParams(VM_STRING(""), VM_STRING(""), &mkfac);
        cParams.m_nMaxAsync = 2;
        MockVideoEncode *mock_encode = new MockVideoEncode();
        std::auto_ptr<IVideoEncode> tmp(mock_encode);
        tmp.reset(new FieldOutputEncoder(tmp));
        MFXEncodeWRAPPER encWrapper(cParams, NULL, tmp);

        //only setdownstream required
        encWrapper.SetDownStream(mock_file);

        mfxFrameSurface1 Surf = {0};
        mfxSyncPoint syncp = reinterpret_cast<mfxSyncPoint>(1);
        mfxSyncPoint syncp2 = reinterpret_cast<mfxSyncPoint>(2);
        Surf.Data.TimeStamp = 3;

        arg.ret_val= MFX_ERR_MORE_BITSTREAM;
        arg.value0 = NULL;  
        arg.value1 = NULL;  
        arg.value2 = NULL;  
        arg.value3 = &syncp;  

        mock_encode->_EncodeFrameAsync.WillReturn(arg);

        arg.ret_val = MFX_ERR_NONE;
        arg.value3 = &syncp2;  
        mock_encode->_EncodeFrameAsync.WillReturn(arg);

        arg.ret_val = MFX_ERR_MORE_DATA;
        mock_encode->_EncodeFrameAsync.WillDefaultReturn(&arg);


        CHECK_EQUAL(MFX_ERR_NONE, encWrapper.RenderFrame(&Surf, NULL));

        TEST_METHOD_TYPE(MockVideoEncode::EncodeFrameAsync) args_called, args_called_second;

        //EFA was called twice
        CHECK_EQUAL(true, mock_encode->_EncodeFrameAsync.WasCalled(&args_called));
        CHECK_EQUAL(true, mock_encode->_EncodeFrameAsync.WasCalled(&args_called_second));
        CHECK_EQUAL(false, mock_encode->_EncodeFrameAsync.WasCalled(NULL));
        
        //syncoperation was not called since async depth is 2 
        TEST_METHOD_TYPE(MockVideoEncode::SyncOperation) args_syncpoint;
        CHECK_EQUAL(false, mock_encode->_SyncOperation.WasCalled(&args_syncpoint));

        CHECK_EQUAL(MFX_ERR_NONE, encWrapper.RenderFrame(NULL, NULL));

        //syncoperation was called 2 times now since EOS met
        CHECK_EQUAL(true, mock_encode->_SyncOperation.WasCalled(&args_syncpoint));
        CHECK_EQUAL(reinterpret_cast<void*>(1), reinterpret_cast<void*>(args_syncpoint.value0));
        CHECK_EQUAL(0u, args_syncpoint.value1);

        CHECK_EQUAL(true, mock_encode->_SyncOperation.WasCalled(&args_syncpoint));
        CHECK_EQUAL(reinterpret_cast<void*>(2), reinterpret_cast<void*>(args_syncpoint.value0));
        CHECK_EQUAL(0u, args_syncpoint.value1);

        CHECK_EQUAL(false, mock_encode->_SyncOperation.WasCalled(&args_syncpoint));


        //same surface passed twice
        CHECK_EQUAL(3u, args_called.value1->Data.TimeStamp );
        CHECK_EQUAL(3u, args_called_second.value1->Data.TimeStamp );

        //same bitstream data pointer, since it is a sync mode
        CHECK_EQUAL((int*)args_called.value2->Data, (int*)args_called_second.value2->Data);
    }
    
    TEST_FIXTURE(Init, RenderFrame_syncop_returns_deviceFailed_after_devicebusy_from_encodeframeasync)
    {
        TEST_METHOD_TYPE(MockVideoEncode::EncodeFrameAsync) arg;
        TEST_METHOD_TYPE(MockVideoEncode::SyncOperation) arg_so;

        mfxFrameSurface1 Surf = {0};
        mfxSyncPoint syncp = reinterpret_cast<mfxSyncPoint>(1);
        Surf.Data.TimeStamp = 3;

        arg.ret_val= MFX_WRN_DEVICE_BUSY;
        arg.value0 = NULL;  
        arg.value1 = NULL;  
        arg.value2 = NULL;  
        arg.value3 = &syncp;

        arg_so.ret_val = MFX_ERR_DEVICE_FAILED;

        ComponentParams cParams(VM_STRING(""), VM_STRING(""), &mkfac);
        MockVideoEncode *mock_encode = new MockVideoEncode();
        std::auto_ptr<IVideoEncode> tmp(mock_encode);
        MFXEncodeWRAPPER encWrapper(cParams, NULL, tmp);

        mock_encode->_EncodeFrameAsync.WillDefaultReturn(&arg);
        mock_encode->_SyncOperation.WillDefaultReturn(&arg_so);

        TEST_METHOD_TYPE(MockVideoEncode::EncodeFrameAsync) args_called;
        
        mfxFrameSurface1 surface = {};
        CHECK_EQUAL(MFX_ERR_DEVICE_FAILED, encWrapper.RenderFrame(&surface, NULL));

        CHECK_EQUAL(true, mock_encode->_EncodeFrameAsync.WasCalled(&args_called));
        //encodeframeasync should be called just once since error happened
        CHECK_EQUAL(false, mock_encode->_EncodeFrameAsync.WasCalled(&args_called));
    }
}
