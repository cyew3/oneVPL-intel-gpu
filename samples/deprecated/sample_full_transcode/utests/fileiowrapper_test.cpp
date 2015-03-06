//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "utest_pch.h"

#include "fileio_wrapper.h"
#include "pipeline_factory.h"

struct FileIOWrapperTest  : public ::testing::Test {
    FILE *f;
    msdk_string tmpFile;
    mfxBitstream bitstream;

       FileIOWrapperTest(){
        memset(&bitstream, 0, sizeof(bitstream));
        tmpFile = MSDK_STRING("TMP_13543.tmp");
        bitstream.Data = new mfxU8[1024];
    }

    ~FileIOWrapperTest(){
        remove(tmpFile.c_str());
        delete[] bitstream.Data;
    }
};

TEST_F(FileIOWrapperTest, putSample) {
    PipelineFactory fac;
    {
        std::auto_ptr<ISink> fio_wrapper ( fac.CreateFileIOWrapper(tmpFile,  MSDK_STRING("wb")));

        bitstream.DataOffset = 0;
        bitstream.MaxLength = 128;
        bitstream.DataLength = 128;
        SamplePtr sample(new SampleBitstream1(bitstream, 0));
        fio_wrapper->PutSample(sample);
    }

    f = fopen(tmpFile.c_str(), MSDK_STRING("rb"));
    EXPECT_EQ(128, fread(bitstream.Data, sizeof(mfxU8), 128, f));
    fclose(f);
}


