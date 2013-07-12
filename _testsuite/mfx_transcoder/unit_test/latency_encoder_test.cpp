/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_latency_encode.h"

SUITE(LatencyEncoder)
{
    struct Init : public CleanTestEnvironment
    {
        mfxFrameSurface1 actual_srf;
        mfxFrameSurface1 * pSurface;

        MockTime * pTime;
        //mock timer params
        TEST_METHOD_TYPE(MockTime::GetTick) ticks;

        MockVideoEncode *pMockEncode;
        TEST_METHOD_TYPE(MockVideoEncode::EncodeFrameAsync) efa_returns;
        //
        TEST_METHOD_TYPE(MockVideoEncode::SyncOperation) so_returns;

        mfxBitstream bitstream;
        mfxSyncPoint syncp, syncp2;
        int s1, s2;

        std::auto_ptr<IVideoEncode> mock_encode_ptr;
        MockPrinter * pMockPrinter;

        LatencyEncode * pTestEncode;

        mfxBitstream bs;

        Init()
            : ticks(3)
            , syncp((mfxSyncPoint)&s1)
            , syncp2((mfxSyncPoint)&s2)
            , bitstream()
            , actual_srf()
            , bs()
        {
            bitstream.FrameType = MFX_FRAMETYPE_I;

            pSurface = &actual_srf;
            
            pTime       = new MockTime();
            mock_encode_ptr.reset(pMockEncode = new MockVideoEncode());
            pMockPrinter= new MockPrinter();


            efa_returns.ret_val = MFX_ERR_NONE;
            efa_returns.value0 = NULL;
            efa_returns.value1 = NULL;
            efa_returns.value2 = bitstream;
            efa_returns.value3 = &syncp;
        }
    };

    TEST_FIXTURE(Init, EncodeFrameAsync_Decode_plus_encode_no_between_sync)
    {
        pTestEncode = new LatencyEncode(true, pMockPrinter, pTime, mock_encode_ptr);

        actual_srf.Data.TimeStamp = 2;

        bitstream.TimeStamp = 2;
        efa_returns.value2 = bitstream;
        pMockEncode->_EncodeFrameAsync.WillReturn(efa_returns);

        pTime->_GetTick.WillReturn(ticks);
        
        mfxSyncPoint sp = 0;
        //test function call
        CHECK_EQUAL(MFX_ERR_NONE, pTestEncode->EncodeFrameAsync(NULL, &actual_srf, &bs, &sp));
        CHECK_EQUAL(MFX_ERR_NONE, pTestEncode->SyncOperation(sp, 0));
        CHECK_EQUAL(MFX_ERR_NONE, pTestEncode->Close());

        CHECK_EQUAL(4u, pMockPrinter->Lines.size());
        //1000ms
        CHECK_EQUAL("0:I: 1000.00  (2000.00 3000.00)", convert_w2s(pMockPrinter->Lines[1]));
        CHECK_EQUAL("Encode average latency(ms) = 1000.00", convert_w2s(pMockPrinter->Lines[2]));
        CHECK_EQUAL("Encode max latency(ms) = 1000.00", convert_w2s(pMockPrinter->Lines[3]));
    }

    
    TEST_FIXTURE(Init, EncodeFrameAsync_DeviceBusy)
    {
        pTestEncode = new LatencyEncode(false, pMockPrinter, pTime, mock_encode_ptr);

        //time = 3
        pTime->_GetTick.WillDefaultReturn(&ticks);

        efa_returns.ret_val = MFX_WRN_DEVICE_BUSY;
        pMockEncode->_EncodeFrameAsync.WillReturn(efa_returns);

        mfxSyncPoint sp = 0;
        //test function call
        CHECK_EQUAL(MFX_WRN_DEVICE_BUSY, pTestEncode->EncodeFrameAsync(NULL, &actual_srf, &bs, &sp));
        CHECK_EQUAL(3, actual_srf.Data.TimeStamp);

        //time = 4
        ticks.ret_val++;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        efa_returns.ret_val = MFX_ERR_NONE;
        

        //we dont emulate buffering for now pts are equal to input pts
        bitstream.TimeStamp = 3;
        efa_returns.value2 = bitstream;

        pMockEncode->_EncodeFrameAsync.WillReturn(efa_returns);

        CHECK_EQUAL(MFX_ERR_NONE, pTestEncode->EncodeFrameAsync(NULL, &actual_srf, &bs, &sp));
        //pts of surface not changed to 4
        CHECK_EQUAL(3, actual_srf.Data.TimeStamp);


        //time = 5
        ticks.ret_val++;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        CHECK_EQUAL(MFX_ERR_NONE, pTestEncode->SyncOperation(sp, 0));

        //lets encode one more frame to check average calculation
        bitstream.TimeStamp = 5;
        efa_returns.value2 = bitstream;
        pMockEncode->_EncodeFrameAsync.WillReturn(efa_returns);

        CHECK_EQUAL(MFX_ERR_NONE, pTestEncode->EncodeFrameAsync(NULL, &actual_srf, &bs, &sp));
        //pts of surface changed to 5
        CHECK_EQUAL(5, actual_srf.Data.TimeStamp);
        CHECK_EQUAL(5, bs.TimeStamp);

        //time = 6
        ticks.ret_val++;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        CHECK_EQUAL(MFX_ERR_NONE, pTestEncode->SyncOperation(sp, 0));
        CHECK_EQUAL(MFX_ERR_NONE, pTestEncode->Close());

        CHECK_EQUAL(5u, pMockPrinter->Lines.size());
        //1000ms
        CHECK_EQUAL("0:I: 2000.00  (3000.00 5000.00)", convert_w2s(pMockPrinter->Lines[1]));
        CHECK_EQUAL("1:I: 1000.00  (5000.00 6000.00)", convert_w2s(pMockPrinter->Lines[2]));
        CHECK_EQUAL("Encode average latency(ms) = 1500.00", convert_w2s(pMockPrinter->Lines[3]));        
        CHECK_EQUAL("Encode max latency(ms) = 2000.00", convert_w2s(pMockPrinter->Lines[4]));  
    }

    TEST_FIXTURE(Init, SyncOperation_WrnInExecution)
    {
        pTestEncode = new LatencyEncode(false, pMockPrinter, pTime, mock_encode_ptr);

        //time = 3
        bitstream.TimeStamp = 3;
        efa_returns.value2 = bitstream;
        pMockEncode->_EncodeFrameAsync.WillReturn(efa_returns);

        mfxSyncPoint sp = 0;
        //test function call
        CHECK_EQUAL(MFX_ERR_NONE, pTestEncode->EncodeFrameAsync(NULL, &actual_srf, &bs, &sp));

        so_returns.ret_val = MFX_WRN_IN_EXECUTION;
        pMockEncode->_SyncOperation.WillReturn(so_returns);
        
        //time = 4
        ticks.ret_val++;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        CHECK_EQUAL(MFX_WRN_IN_EXECUTION, pTestEncode->SyncOperation(sp, 0));


        so_returns.ret_val = MFX_ERR_NONE;
        pMockEncode->_SyncOperation.WillReturn(so_returns);

        //time = 5
        ticks.ret_val++;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        CHECK_EQUAL(MFX_ERR_NONE, pTestEncode->SyncOperation(sp, 0));

        CHECK_EQUAL(MFX_ERR_NONE, pTestEncode->Close());

        CHECK_EQUAL(4u, pMockPrinter->Lines.size());
        //1000ms
        CHECK_EQUAL("0:I: 2000.00  (3000.00 5000.00)", convert_w2s(pMockPrinter->Lines[1]));
        
        //second call to close shouldn't print statistics again
        pMockPrinter->Lines.clear();
        CHECK_EQUAL(MFX_ERR_NONE, pTestEncode->Close());

        CHECK_EQUAL(0u, pMockPrinter->Lines.size());
    }

    TEST_FIXTURE(Init, Multiple_SyncOperations)
    {
        pTestEncode = new LatencyEncode(false, pMockPrinter, pTime, mock_encode_ptr);

        //time = 3
        bitstream.TimeStamp = 3;
        efa_returns.value2 = bitstream;
        pMockEncode->_EncodeFrameAsync.WillReturn(efa_returns);

        mfxBitstream bs2 = {};
        mfxSyncPoint sp = 0;
        mfxSyncPoint sp2 = 0;
        //test function call
        CHECK_EQUAL(MFX_ERR_NONE, pTestEncode->EncodeFrameAsync(NULL, &actual_srf, &bs, &sp)); 

        //time = 4
        ticks.ret_val++;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        bitstream.TimeStamp = 4;
        efa_returns.value3 = &syncp2;
        efa_returns.value2 = bitstream;
        pMockEncode->_EncodeFrameAsync.WillReturn(efa_returns);

        //test function call
        CHECK_EQUAL(MFX_ERR_NONE, pTestEncode->EncodeFrameAsync(NULL, &actual_srf, &bs2, &sp2));

        //time = 5
        ticks.ret_val++;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        CHECK_EQUAL(MFX_ERR_NONE, pTestEncode->SyncOperation(sp, 0));
        //time = 6
        ticks.ret_val++;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        CHECK_EQUAL(MFX_ERR_NONE, pTestEncode->SyncOperation(sp, 0));

        //time = 7
        ticks.ret_val++;
        pTime->_GetTick.WillDefaultReturn(&ticks);


        CHECK_EQUAL(MFX_ERR_NONE, pTestEncode->SyncOperation(sp2, 0));

        //time = 8
        ticks.ret_val++;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        CHECK_EQUAL(MFX_ERR_NONE, pTestEncode->SyncOperation(sp2, 0));

        CHECK_EQUAL(MFX_ERR_NONE, pTestEncode->Close());

        CHECK_EQUAL(5u, pMockPrinter->Lines.size());
        
        CHECK_EQUAL("0:I: 2000.00  (3000.00 5000.00)", convert_w2s(pMockPrinter->Lines[1]));
        CHECK_EQUAL("1:I: 3000.00  (4000.00 7000.00)", convert_w2s(pMockPrinter->Lines[2]));

    }

}