/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_field_output_encode.h"
#include "mock_mfx_pipeline_factory.h"

SUITE(FieldOutputEncoder)
{
    struct SInit {
        MockVideoEncode mock_encode;
        std::auto_ptr<IVideoEncode> ptr;
        FieldOutputEncoder enc;
        
        SInit() : ptr(MakeUndeletable(mock_encode)), enc(ptr) {
        }
    };
    TEST_FIXTURE(SInit, MORE_BITSTREAM)
    {
        TEST_METHOD_TYPE(MockVideoEncode::EncodeFrameAsync) arg;

        mfxFrameSurface1 Surf = {0};
        mfxSyncPoint syncp = reinterpret_cast<mfxSyncPoint>(1);
        mfxSyncPoint syncp2 = reinterpret_cast<mfxSyncPoint>(2);
    //    Surf.Data.TimeStamp = 3;
        arg.ret_val = MFX_ERR_MORE_BITSTREAM;
        arg.value0  = NULL;  
        arg.value1  = NULL;  
        arg.value2  = NULL;  
        arg.value3  = &syncp;  

        mock_encode._EncodeFrameAsync.WillReturn(arg);

        arg.ret_val = MFX_ERR_NONE;
        arg.value3 = &syncp2;  
        mock_encode._EncodeFrameAsync.WillReturn(arg);

        mfxBitstream bs = {0};
        mfxSyncPoint final;
        CHECK_EQUAL(MFX_ERR_NONE, enc.EncodeFrameAsync(NULL, &Surf, &bs, &final));

        CHECK(mock_encode._EncodeFrameAsync.WasCalled(&arg));
        CHECK(mock_encode._EncodeFrameAsync.WasCalled(&arg));
        CHECK(!mock_encode._EncodeFrameAsync.WasCalled(&arg));
        CHECK_EQUAL(reinterpret_cast<mfxSyncPoint>(1), final);
    }
    
    //in async_depth scenarios encoder might buffer frames 
    TEST_FIXTURE(SInit, ASYNC)
    {
        TEST_METHOD_TYPE(MockVideoEncode::EncodeFrameAsync) arg;

        mfxFrameSurface1 Surf = {0};
        mfxSyncPoint syncp = reinterpret_cast<mfxSyncPoint>(1);
        mfxSyncPoint syncp2 = reinterpret_cast<mfxSyncPoint>(2);
        //    Surf.Data.TimeStamp = 3;
        arg.ret_val = MFX_ERR_MORE_DATA;
        arg.value0  = 0;
        arg.value1  = 0;
        arg.value2  = 0;
        arg.value3  = 0;  

        mock_encode._EncodeFrameAsync.WillReturn(arg);

        arg.ret_val = MFX_ERR_MORE_BITSTREAM;
        arg.value3 = &syncp;  
        mock_encode._EncodeFrameAsync.WillReturn(arg);

        arg.ret_val = MFX_ERR_NONE;
        arg.value3 = &syncp2;  
        mock_encode._EncodeFrameAsync.WillReturn(arg);

        mfxBitstream bs = {0};
        mfxSyncPoint final;
        CHECK_EQUAL(MFX_ERR_MORE_DATA, enc.EncodeFrameAsync(NULL, &Surf, &bs, &final));

        //only called one dueto more data
        CHECK(mock_encode._EncodeFrameAsync.WasCalled(&arg));
        CHECK(!mock_encode._EncodeFrameAsync.WasCalled(&arg));

        CHECK_EQUAL(MFX_ERR_NONE, enc.EncodeFrameAsync(NULL, &Surf, &bs, &final));

        //not efa called twice due to more bitstream
        CHECK(mock_encode._EncodeFrameAsync.WasCalled(&arg));
        CHECK(mock_encode._EncodeFrameAsync.WasCalled(&arg));
        CHECK(!mock_encode._EncodeFrameAsync.WasCalled(&arg));
    }

    //check that syncop actually works with returned syncpoint and resulted in sync call to both sync points
    /*This tested in more complicated test in ENCODE_WRAPPER*/

}
