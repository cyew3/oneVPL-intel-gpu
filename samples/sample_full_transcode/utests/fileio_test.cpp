//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "utest_pch.h"

#include "fileio.h"

struct FileIOTest  : public ::testing::Test {
    std::auto_ptr<FileIO> fio;
    FILE *f;
    msdk_string tmpFile;
    mfxBitstream bitstream;

    FileIOTest(){
        tmpFile = MSDK_STRING("TMP_13542.tmp");
        bitstream.Data = new mfxU8[1024];
    }

    void CreateNewTmpFile(mfxU32 size){
        _tremove(tmpFile.c_str());
        f = _tfopen(tmpFile.c_str(), MSDK_STRING("wb"));
        std::auto_ptr<mfxU8> buf(new mfxU8[size]);
        fwrite(buf.get(), sizeof(mfxU8), size, f);
        fclose(f);
    }

    ~FileIOTest(){
        fclose(f);
        _tremove(tmpFile.c_str());
        delete[] bitstream.Data;
    }        
};

TEST_F(FileIOTest, test_file_read) {
    CreateNewTmpFile(128);
    fio.reset(new FileIO(tmpFile.c_str(),  MSDK_STRING("rb")));

    bitstream.MaxLength = 128;
    bitstream.DataOffset = 0;
    bitstream.DataLength = 0;
    EXPECT_EQ(128, fio->Read(&bitstream));
    fio.reset();
}

TEST_F(FileIOTest, test_file_read_not_enough_buffer) {
    CreateNewTmpFile(128);
    fio.reset(new FileIO(tmpFile.c_str(), MSDK_STRING("rb")));

    bitstream.MaxLength = 128;
    bitstream.DataOffset = 10;
    bitstream.DataLength = 0;
    EXPECT_EQ(118, fio->Read(&bitstream));
    fio.reset();
}

TEST_F(FileIOTest, test_file_read_eof) {
    CreateNewTmpFile(128);
    fio.reset(new FileIO(tmpFile.c_str(),  MSDK_STRING("rb")));

    bitstream.MaxLength = 128;
    bitstream.DataOffset = 0;
    bitstream.DataLength = 0;
    fio->Read(&bitstream);
    EXPECT_EQ(0, fio->Read(&bitstream));
    fio.reset();
}

TEST_F(FileIOTest, test_file_seek) {
    CreateNewTmpFile(128);
    fio.reset(new FileIO(tmpFile.c_str(), MSDK_STRING("rb")));

    fio->Seek(100, MFX_SEEK_ORIGIN_BEGIN);
    EXPECT_EQ(28, fread(bitstream.Data, sizeof(mfxU8), 128, f));
    fio.reset();
}

TEST_F(FileIOTest, test_file_seek_overboard) {
    CreateNewTmpFile(128);
    fio.reset(new FileIO(tmpFile.c_str(), MSDK_STRING("rb")));

    //TODO: MFX API not defined seek api well
    EXPECT_EQ(0, fio->Seek(129, MFX_SEEK_ORIGIN_CURRENT));
    fio.reset();
}

TEST_F(FileIOTest, test_file_write) {
    fio.reset(new FileIO(tmpFile.c_str(), MSDK_STRING("wb")));
    bitstream.MaxLength = 128;
    bitstream.DataLength = 100;
    bitstream.DataOffset = 0;
    fio->Write(&bitstream);
    fio.reset();
    
    f = _tfopen(tmpFile.c_str(), MSDK_STRING("rb"));
    EXPECT_EQ(100, fread(bitstream.Data, sizeof(mfxU8), 100, f));
    fclose(f);
}

TEST_F(FileIOTest, test_file_write_incorrect_length) {
    fio.reset(new FileIO(tmpFile.c_str(), MSDK_STRING("wb")));
    bitstream.MaxLength = 128;
    bitstream.DataLength = 100;
    bitstream.DataOffset = 100;
    fio->Write(&bitstream);
    fio.reset();
    f = _tfopen(tmpFile.c_str(), MSDK_STRING("rb"));
    EXPECT_EQ(128, fread(bitstream.Data, sizeof(mfxU8), 130, f));
    fclose(f);
}

#if 0
TEST_F(FileIOTest, test_file_seek_overboard) {
    CreateNewTmpFile(128);
    fio.reset(new FileIO(tmpFile.c_str(), MSDK_STRING("rb")));

    EXPECT_EQ(-1, fio->Seek(129, MFX_SEEK_ORIGIN_CURRENT));
    fio.reset();
}
#endif