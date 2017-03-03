/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

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
        remove(tmpFile.c_str());

        f = fopen(tmpFile.c_str(), MSDK_STRING("wb"));
        std::auto_ptr<mfxU8> buf(new mfxU8[size]);
        fwrite(buf.get(), sizeof(mfxU8), size, f);
        fclose(f);
    }

    ~FileIOTest(){
        fclose(f);
        remove(tmpFile.c_str());
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
    EXPECT_EQ(129, fio->Seek(129, MFX_SEEK_ORIGIN_CURRENT));
    fio.reset();
}

TEST_F(FileIOTest, test_file_write) {
    fio.reset(new FileIO(tmpFile.c_str(), MSDK_STRING("wb")));
    bitstream.MaxLength = 128;
    bitstream.DataLength = 100;
    bitstream.DataOffset = 0;
    fio->Write(&bitstream);
    fio.reset();

    f = fopen(tmpFile.c_str(), MSDK_STRING("rb"));
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
    f = fopen(tmpFile.c_str(), MSDK_STRING("rb"));
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
