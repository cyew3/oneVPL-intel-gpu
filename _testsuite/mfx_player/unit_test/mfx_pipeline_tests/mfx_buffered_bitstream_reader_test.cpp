/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_buffered_bs_reader.h"
#include "mock_mfx_bitstream_reader.h"
#include "mfx_file_generic.h"

SUITE(BufferedBitstreamReader)
{
    struct BufferPrepare
    {
        MockFile mock_file;
        MockBitstreamReader bsReader;
        BufferedBitstreamReader buffer;
        TEST_METHOD_TYPE(MockFile::GetInfo) ret_params_file;
        TEST_METHOD_TYPE(MockBitstreamReader::ReadNextFrame) ret_params;
        
        BufferPrepare() : buffer(MakeUndeletable(bsReader)
            , BufferedBitstreamReader::SpecificChunkSize
            , 10
            , BufferedBitstreamReader::SpecificChunkSize
            , 10
            , mfx_shared_ptr<IFile>(MakeUndeletable(mock_file)))
        {
            ret_params_file.value1 = 100;
            mock_file._GetInfo.WillReturn(ret_params_file);
        }
    };
    TEST_FIXTURE(BufferPrepare, downstream_readnext_frame_called_with_right_buffer)
    {
        ret_params.ret_val = MFX_ERR_UNKNOWN;
        bsReader._ReadNextFrame.WillReturn(ret_params);

        CHECK_EQUAL( MFX_ERR_NONE, buffer.Init(VM_STRING("tmpFileName")));

        //enough size
        mfxTestBitstream bs2(100);
        
        CHECK_EQUAL(MFX_ERR_UNKNOWN, buffer.ReadNextFrame(bs2));
        CHECK(bsReader._ReadNextFrame.WasCalled(&ret_params));
        CHECK(ret_params.value0.Data != 0);
        CHECK(ret_params.value0.MaxLength != 0);
    }
    TEST_FIXTURE(BufferPrepare, FrameSplitted_among_chunks)
    {
        mfxTestBitstream bs(100, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        bs.DataLength = 5;
        bs.FrameType = MFX_FRAMETYPE_I;
        bs.Data = bs.Buffer();

        ret_params.value0 = bs;
        bsReader._ReadNextFrame.WillReturn(ret_params);

        bs.DataLength = 11;
        bs.FrameType = MFX_FRAMETYPE_P;
        bs.Data = bs.Buffer() + 5;
        ret_params.value0 = bs;
        bsReader._ReadNextFrame.WillReturn(ret_params);

        ret_params_file.value1 = 100;
        mock_file._GetInfo.WillReturn(ret_params_file);

        bs.DataLength = 0;
        bs.FrameType = 0;
        bs.Data = 0;
        ret_params.value0 = bs;
        ret_params.ret_val = MFX_ERR_UNKNOWN;

        bsReader._ReadNextFrame.WillReturn(ret_params);

        CHECK_EQUAL( MFX_ERR_NONE, buffer.Init(VM_STRING("tmpFileName")));

        //enough size 
        mfxTestBitstream bs2(100);
        CHECK_EQUAL(MFX_ERR_NONE, buffer.ReadNextFrame(bs2));

        //check first frame
        CHECK_EQUAL(5u, bs2.DataLength);
        CHECK_EQUAL(MFX_FRAMETYPE_I, bs2.FrameType);
        CHECK_EQUAL(0, memcmp(bs2.Data, bs.Buffer(), 5 ));
        
        bs2.DataLength = 0;

        //second frame has storage layer border, so it will be copied twice
        CHECK_EQUAL(MFX_ERR_NONE, buffer.ReadNextFrame(bs2));
        CHECK_EQUAL(11u, bs2.DataLength);
        CHECK_EQUAL(MFX_FRAMETYPE_P, bs2.FrameType);
        CHECK_EQUAL(0, memcmp(bs2.Data, bs.Buffer() + 5, 11 ));

        bs2.DataLength = 0;

        //second frame has storage layer border, so it will be copied twice
        CHECK_EQUAL(MFX_ERR_UNKNOWN, buffer.ReadNextFrame(bs2));
    }

    TEST_FIXTURE(BufferPrepare, GetNextFrame_NotEnoughBuffer)
    {
        mfxTestBitstream bs(100, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
        bs.DataLength = 5;
        bs.FrameType = MFX_FRAMETYPE_I;
        bs.Data = bs.Buffer();

        ret_params.value0 = bs;
        bsReader._ReadNextFrame.WillReturn(ret_params);

        ret_params.ret_val = MFX_ERR_UNKNOWN;
        bsReader._ReadNextFrame.WillReturn(ret_params);

        CHECK_EQUAL( MFX_ERR_NONE, buffer.Init(VM_STRING("tmpFileName")));

        //not enough size so frame will be splited and buffered bitstream reader will return an warning
        //TODO: add configurability to return an error probably
        mfxTestBitstream bs2(4);
        CHECK_EQUAL(MFX_ERR_NONE, buffer.ReadNextFrame(bs2));
    }
}