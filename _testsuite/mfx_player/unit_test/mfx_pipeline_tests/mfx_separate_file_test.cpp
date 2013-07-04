/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_separate_file.h"

SUITE(SeparateFilesWriter)
{
    struct Init : public CleanTestEnvironment
    {
        MockFile *mFile;
        std::auto_ptr<IFile> tmp;
        SeparateFilesWriter wrt;
        
        TEST_METHOD_TYPE(MockFile::isOpen) open_state;
        Init() : mFile(new MockFile())
               , tmp(mFile)
               , wrt(tmp)
        {
            open_state = true;
            mFile->_isOpen.WillDefaultReturn(&open_state);
        }
    };

    TEST_FIXTURE(Init, bypass_open_and_close)
    {
        CHECK_EQUAL(0, wrt.Open(VM_STRING("aaa.f"), VM_STRING("a")));

        TEST_METHOD_TYPE(MockFile::Open) open_params;// (0,VM_STRING(""),VM_STRING(""));

        CHECK(mFile->_Open.WasCalled(&open_params));
        CHECK_EQUAL("aaa.f", convert_w2s(open_params.value0.c_str()));
        CHECK_EQUAL("a", convert_w2s(open_params.value1.c_str()));

        CHECK_EQUAL(0, wrt.Close());
        CHECK(mFile->_Close.WasCalled());
    }

    TEST_FIXTURE(Init, new_file_on_reopen)
    {
        CHECK_EQUAL(0, wrt.Open(VM_STRING("aaa.f.l"), VM_STRING("a")));

        TEST_METHOD_TYPE(MockFile::Open) open_params;
        CHECK(mFile->_Open.WasCalled(&open_params));

        //call to reopen
        CHECK_EQUAL(0, wrt.Reopen());

        //close then init with different name
        CHECK(mFile->_Close.WasCalled());
        CHECK(mFile->_Open.WasCalled(&open_params));

        CHECK_EQUAL("aaa.f_1.l", convert_w2s(open_params.value0.c_str()));
        CHECK_EQUAL("a", convert_w2s(open_params.value1.c_str()));

        //call to reopen
        CHECK_EQUAL(0, wrt.Reopen());

        //close then init with different name
        CHECK(mFile->_Close.WasCalled());
        CHECK(mFile->_Open.WasCalled(&open_params));

        CHECK_EQUAL("aaa.f_2.l", convert_w2s(open_params.value0.c_str()));
        CHECK_EQUAL("a", convert_w2s(open_params.value1.c_str()));
    }

    TEST_FIXTURE(Init, no_new_file_if_close_then_reopen_open)
    {
        CHECK_EQUAL(0, wrt.Open(VM_STRING("aaa.f.l"), VM_STRING("a")));

        TEST_METHOD_TYPE(MockFile::Open) open_params;
        CHECK(mFile->_Open.WasCalled(&open_params));
        
        CHECK_EQUAL(0, wrt.Reopen());
        CHECK(mFile->_Open.WasCalled());
        CHECK_EQUAL(0, wrt.Reopen());
        CHECK(mFile->_Open.WasCalled());
        CHECK_EQUAL(0, wrt.Close());

        //not opened
        open_state = false;
        mFile->_isOpen.WillReturn(open_state);

        CHECK_EQUAL(0, wrt.Reopen());
        CHECK(!mFile->_Open.WasCalled());

        CHECK_EQUAL(0, wrt.Open(VM_STRING("aaa.f2.l"), VM_STRING("a")));
        CHECK(mFile->_Open.WasCalled(&open_params));

        open_state.ret_val = true;
        mFile->_isOpen.WillReturn(open_state);

        CHECK_EQUAL("aaa.f2.l", convert_w2s(open_params.value0.c_str()));
        CHECK_EQUAL("a", convert_w2s(open_params.value1.c_str()));

        CHECK_EQUAL(0, wrt.Reopen());
        CHECK(mFile->_Open.WasCalled(&open_params));

        CHECK_EQUAL("aaa.f2_1.l", convert_w2s(open_params.value0.c_str()));
        CHECK_EQUAL("a", convert_w2s(open_params.value1.c_str()));
    }
};