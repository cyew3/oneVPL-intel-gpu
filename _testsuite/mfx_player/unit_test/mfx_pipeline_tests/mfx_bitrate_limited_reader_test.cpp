/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_bitrate_limited_reader.h"
#include "mock_mfx_bitstream_reader.h"
#include "mfx_file_generic.h"
#include "mock_time.h"

SUITE(BitrateLimitedReader)
{
    struct Init
    {
        MockTime mTime;
        MockBitstreamReader bsReader;
        BitrateLimitedReader blReader;

        TEST_METHOD_TYPE(MockTime::Current) ret_params_current;
        TEST_METHOD_TYPE(MockBitstreamReader::ReadNextFrame) ret_params;

        Init()
            : blReader(50, &mTime, 0, MakeUndeletable(bsReader))
        {
        }

    };
    TEST_FIXTURE(Init, EndofStream)
    {
        mfxTestBitstream bs(100);

        ret_params.ret_val = MFX_ERR_UNKNOWN;
        bsReader._ReadNextFrame.WillReturn(ret_params);

        CHECK_EQUAL(MFX_ERR_UNKNOWN, blReader.ReadNextFrame(bs));
    }

    TEST_FIXTURE(Init, limit_bitrate)
    {
        mfxTestBitstream bs(100);
        
        bs.DataLength = 25;
        ret_params.value0 = bs;
        ret_params.ret_val = MFX_ERR_NONE;
        bsReader._ReadNextFrame.WillReturn(ret_params);

        bs.DataLength = 25;
        ret_params.value0 = bs;
        ret_params.ret_val = MFX_ERR_NONE;
        bsReader._ReadNextFrame.WillReturn(ret_params);


        ret_params_current.ret_val = 1;
        mTime._Current.WillReturn(ret_params_current);

        //it means we read 50 bytes in 0.5 seconds, if we declare speed as 50 bytes sec, we need to wait about 500 milisec
        ret_params_current.ret_val = 1.5;
        mTime._Current.WillReturn(ret_params_current);

        mfxTestBitstream bs2(100);
        CHECK_EQUAL(MFX_ERR_NONE, blReader.ReadNextFrame(bs2));
        
        bs2.DataLength = 0;
        CHECK_EQUAL(MFX_ERR_NONE, blReader.ReadNextFrame(bs2));

        TEST_METHOD_TYPE(MockTime::Wait) wait_params;
        CHECK(mTime._Wait.WasCalled(&wait_params));
        
        //wait called only once with 50 ms parameter
        CHECK_EQUAL(500u, wait_params.value0);
        CHECK(!mTime._Wait.WasCalled(&wait_params));
    }

}