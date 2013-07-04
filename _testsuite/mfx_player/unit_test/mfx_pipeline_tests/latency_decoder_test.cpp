/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_latency_decoder.h"

SUITE(LatencyDecoder)
{
    struct Init : public CleanTestEnvironment
    {
        MockYUVSource mockSource; 
        MockTime * pTime; 
        MockPrinter * pPrinter; 
        LatencyDecoder * pLatencyDec;

        mfxFrameSurface1 surface_actual, *pSurface, *pSurface2;
        mfxSyncPoint syncp, syncp2, syncp3, syncp4;
        int s1,s2;
        mfxBitstream2 bs;
        mfxBitstream2 zero;
        //
        TEST_METHOD_TYPE(MockYUVSource::DecodeFrameAsync) dfa_returns;

        //
        TEST_METHOD_TYPE(MockYUVSource::SyncOperation) so_returns;

        //mock timer params
        TEST_METHOD_TYPE(MockTime::GetTick) ticks;
        Init()
            : ticks(0x10)
            , syncp((mfxSyncPoint)&s1)
            , syncp3((mfxSyncPoint)&s2)
            , surface_actual()
            , bs()
            , zero()
        {
            pTime       = new MockTime();
            pPrinter    = new MockPrinter();
            

            //initializing timer with time = 1
            pTime->_GetTick.WillDefaultReturn(&ticks);

            surface_actual.Data.TimeStamp = 0x10;
            pSurface = &surface_actual;
            //initialize mockdecoder::decodeframeasync with standard behavior
            dfa_returns.ret_val = MFX_ERR_NONE;
            dfa_returns.value0 = 0;
            dfa_returns.value1 = 0;
            dfa_returns.value2 = &pSurface;
            dfa_returns.value3 = &syncp;

            //initialize so with default behavior
            so_returns.ret_val = MFX_ERR_NONE;

            //bitstream to check that pts of it 
            zero.isNull = true;
        }
    };

    TEST_FIXTURE(Init, DecodeFrameAsync_DEVICE_BUSY)
    {
        std::auto_ptr<IYUVSource> ptr(MakeUndeletable(mockSource));

        pLatencyDec = new LatencyDecoder(false, pPrinter, pTime, VM_STRING("Decode"), ptr);
        mockSource._DecodeFrameAsync.WillReturn(dfa_returns);
        

        //time = 1
        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->DecodeFrameAsync(zero, NULL, &pSurface2, &syncp2));

        //increment timer value to 2
        ticks.ret_val += 0x10;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        dfa_returns.ret_val = MFX_WRN_DEVICE_BUSY;
        mockSource._DecodeFrameAsync.WillReturn(dfa_returns);
        surface_actual.Data.TimeStamp = 0x30;//invalid timestamp that shouldn't be used
        CHECK_EQUAL(MFX_WRN_DEVICE_BUSY, pLatencyDec->DecodeFrameAsync(bs, NULL, &pSurface2, &syncp2));
        CHECK_EQUAL(0x20, bs.TimeStamp);

        //increment timer value to 3
        ticks.ret_val += 0x10;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        dfa_returns.ret_val = MFX_ERR_NONE;
        mockSource._DecodeFrameAsync.WillReturn(dfa_returns);
        surface_actual.Data.TimeStamp = 0x20;//correct timestamp
        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->DecodeFrameAsync(bs, NULL, &pSurface2, &syncp2));
        
        //timestamp of bitstream shouldn't change it is first time bs entered intodecodeframeasync
        CHECK_EQUAL(0x20, bs.TimeStamp);

        //increment timer value to 40
        ticks.ret_val += 0x10;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->SyncOperation(syncp,0));
        
        //increment timer value to 50
        ticks.ret_val += 0x10;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->SyncOperation(syncp,0));

        //print commands invoked on close
        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->Close());

        CHECK_EQUAL("0:?: 48000.00  (16000.00 64000.00)", convert_w2s(pPrinter->Lines[1]));
        CHECK_EQUAL("1:?: 48000.00  (32000.00 80000.00)", convert_w2s(pPrinter->Lines[2]));
        CHECK_EQUAL("Decode average latency(ms) = 48000.00", convert_w2s(pPrinter->Lines[3]));
        CHECK_EQUAL("Decode max latency(ms) = 48000.00", convert_w2s(pPrinter->Lines[4]));
        CHECK_EQUAL(5u, pPrinter->Lines.size());
    }

    TEST_FIXTURE(Init, SyncOperation_IN_EXECUTION)
    {
        std::auto_ptr<IYUVSource> ptr(MakeUndeletable(mockSource));
        pLatencyDec = new LatencyDecoder(false, pPrinter, pTime, VM_STRING("Decode"), ptr);
        mockSource._DecodeFrameAsync.WillReturn(dfa_returns);
        //time = 1
        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->DecodeFrameAsync(zero, NULL, &pSurface2, &syncp2));

        //increment timer value to 2
        ticks.ret_val += 0x10;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        //simulating task not ready
        so_returns.ret_val = MFX_WRN_IN_EXECUTION;
        mockSource._SyncOperation.WillReturn(so_returns);

        pLatencyDec->SyncOperation(syncp,0);

        //increment timer value to 3
        ticks.ret_val += 0x10;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        so_returns.ret_val = MFX_ERR_NONE;
        pLatencyDec->SyncOperation(syncp,0);

        //print commands invoked on close
        pLatencyDec->Close();

        CHECK_EQUAL("0:?: 32000.00  (16000.00 48000.00)", convert_w2s(pPrinter->Lines[1]));
        CHECK_EQUAL("Decode average latency(ms) = 32000.00", convert_w2s(pPrinter->Lines[2]));
        CHECK_EQUAL("Decode max latency(ms) = 32000.00", convert_w2s(pPrinter->Lines[3]));
        CHECK_EQUAL(4u, pPrinter->Lines.size());


        //doesnt print on second close
        pPrinter->Lines.clear();
        pLatencyDec->Close();
        CHECK_EQUAL(0u, pPrinter->Lines.size());
    }

    TEST_FIXTURE(Init, SyncOperation_AgregatedStat)
    {
        std::auto_ptr<IYUVSource> ptr(MakeUndeletable(mockSource));
        pLatencyDec = new LatencyDecoder(true, pPrinter, pTime, VM_STRING(""), ptr);

        mockSource._DecodeFrameAsync.WillReturn(dfa_returns);
        //time = 1
        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->DecodeFrameAsync(zero, NULL, &pSurface2, &syncp2));
        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->SyncOperation(syncp2,0));

        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->Close());

        CHECK_EQUAL(0u, pPrinter->Lines.size());
    }

    TEST_FIXTURE(Init, Multiple_SyncOperations)
    {
        std::auto_ptr<IYUVSource> ptr(MakeUndeletable(mockSource));
        pLatencyDec = new LatencyDecoder(false, pPrinter, pTime, VM_STRING("Decode"), ptr);

        mockSource._DecodeFrameAsync.WillReturn(dfa_returns);
        //time = 1
        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->DecodeFrameAsync(zero, NULL, &pSurface2, &syncp2));

        //time = 2
        ticks.ret_val += 0x10;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        dfa_returns.value3 = &syncp3;
        surface_actual.Data.TimeStamp = 0x20;//invalid timestamp that shouldn be used
        mockSource._DecodeFrameAsync.WillReturn(dfa_returns);

        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->DecodeFrameAsync(zero, NULL, &pSurface2, &syncp4));


        //time = 3
        ticks.ret_val += 0x10;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->SyncOperation(syncp2,0));

        //time = 4
        ticks.ret_val += 0x10;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->SyncOperation(syncp2,0));

        //time = 5
        ticks.ret_val += 0x10;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->SyncOperation(syncp4,0));

        //time = 6
        ticks.ret_val += 0x10;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->SyncOperation(syncp4,0));

        //print commands invoked on close
        pLatencyDec->Close();

        CHECK_EQUAL(5u, pPrinter->Lines.size());
        
        CHECK_EQUAL("0:?: 32000.00  (16000.00 48000.00)", convert_w2s(pPrinter->Lines[1]));
        CHECK_EQUAL("1:?: 48000.00  (32000.00 80000.00)", convert_w2s(pPrinter->Lines[2]));
        CHECK_EQUAL("Decode average latency(ms) = 40000.00", convert_w2s(pPrinter->Lines[3]));
        CHECK_EQUAL("Decode max latency(ms) = 48000.00", convert_w2s(pPrinter->Lines[4]));
    }


    TEST_FIXTURE(Init, Closing_by_destructor)
    {
        std::auto_ptr<IYUVSource> ptr(MakeUndeletable(mockSource));
        pLatencyDec = new LatencyDecoder(true, pPrinter, pTime, VM_STRING(""), ptr);

        mockSource._DecodeFrameAsync.WillReturn(dfa_returns);
        //time = 1
        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->DecodeFrameAsync(zero, NULL, &pSurface2, &syncp2));
        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->SyncOperation(syncp2,0));

        delete pLatencyDec;

        //since mock object not deleted only remained close is it's close
        CHECK(!mockSource._Close.WasCalled());
    }

    TEST_FIXTURE(Init, Latency_Should_Start_FromDecodingHeader)
    {
        std::auto_ptr<IYUVSource> ptr(MakeUndeletable(mockSource));
        pLatencyDec = new LatencyDecoder(false, pPrinter, pTime, VM_STRING("Decode"), ptr);
        mockSource._DecodeFrameAsync.WillReturn(dfa_returns);
        
        //time = 1 - this is starttime for bitstream decoding if any
        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->DecodeHeader(NULL, NULL));

        //time = 2
        ticks.ret_val += 0x10;
        pTime->_GetTick.WillDefaultReturn(&ticks);
        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->DecodeHeader(NULL, NULL));


        //time = 3
        ticks.ret_val += 0x10;
        pTime->_GetTick.WillDefaultReturn(&ticks);
        CHECK_EQUAL(MFX_ERR_NONE, pLatencyDec->DecodeFrameAsync(zero, NULL, &pSurface2, &syncp2));

        //time = 4
        ticks.ret_val += 0x10;
        pTime->_GetTick.WillDefaultReturn(&ticks);

        pLatencyDec->SyncOperation(syncp,0);

        //print commands invoked on close
        pLatencyDec->Close();

        CHECK_EQUAL("0:?: 48000.00  (16000.00 64000.00)", convert_w2s(pPrinter->Lines[1]));
        CHECK_EQUAL("Decode average latency(ms) = 48000.00", convert_w2s(pPrinter->Lines[2]));
        CHECK_EQUAL("Decode max latency(ms) = 48000.00", convert_w2s(pPrinter->Lines[3]));
        CHECK_EQUAL(4u, pPrinter->Lines.size());
    }
}