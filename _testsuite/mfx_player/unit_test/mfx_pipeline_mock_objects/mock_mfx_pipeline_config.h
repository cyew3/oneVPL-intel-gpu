/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_ipipeline_config.h"
#include "test_method.h"

class MockPipelineConfig
    : public IMFXPipelineConfig
{
public:
    MockPipelineConfig()
        : m_argc()
        , m_argv()
    {
    }

    MockPipelineConfig(int argc, vm_char ** argv)
        : m_argc(argc)
        , m_argv(argv)
    {
    }

    DECLARE_TEST_METHOD0(IMFXPipeline *, CreatePipeline)
    {
        TEST_METHOD_TYPE(CreatePipeline) ret_params;
        if (!_CreatePipeline.GetReturn(ret_params))
        {
            return NULL;
        }
        return ret_params.ret_val;
    }
    virtual std::mutex * GetExternalSync() override
    {
        return nullptr;
    }
    DECLARE_TEST_METHOD0(vm_char**,  GetArgv)
    {
        TEST_METHOD_TYPE(GetArgv) ret_params;
        if (!_GetArgv.GetReturn(ret_params))
        {
            return m_argv;
        }
        return ret_params.ret_val;
    }
    DECLARE_TEST_METHOD0(int, GetArgc)
    {
        TEST_METHOD_TYPE(GetArgc) ret_params;
        if (!_GetArgc.GetReturn(ret_params))
        {
            return m_argc;
        }
        return ret_params.ret_val;
    }

protected:
    int        m_argc;
    vm_char ** m_argv;
};
