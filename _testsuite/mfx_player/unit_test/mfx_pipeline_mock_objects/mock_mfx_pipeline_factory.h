/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_ifactory.h"
#include "test_method.h"
#include "mfx_factory_default.h"
#include "mock_mfx_ivideo_session.h"

class MockPipelineFactory 
    : public IMFXPipelineFactory
{
public:
    virtual mfx_shared_ptr<IRandom> CreateRandomizer()
    {
        return mfx_make_shared<DefaultRandom>();
    }
    DECLARE_TEST_METHOD1(IVideoSession   *, CreateVideoSession,  MAKE_DYNAMIC_TRAIT(PipelineObjectDescBase, IPipelineObjectDesc*) )
    {
        TEST_METHOD_TYPE(CreateVideoSession) params_holder;
        if (_0)
        {
            PipelineObjectDescBase *dsc = dynamic_cast<PipelineObjectDescBase *>(_0);
            _CreateVideoSession.RegisterEvent(TEST_METHOD_TYPE(CreateVideoSession)((IVideoSession* )NULL, *dsc));
        }
        if (!_CreateVideoSession.GetReturn(params_holder))
        {
            return new MockVideoSession();
        }

        return params_holder.ret_val;
    }
    DECLARE_TEST_METHOD1(IVideoEncode   *, CreateVideoEncode,  MAKE_DYNAMIC_TRAIT(PipelineObjectDescBase, IPipelineObjectDesc*) )
    {
        TEST_METHOD_TYPE(CreateVideoEncode) params_holder;
        PipelineObjectDescBase *dsc = dynamic_cast<PipelineObjectDescBase *>(_0);
        _CreateVideoEncode.RegisterEvent(TEST_METHOD_TYPE(CreateVideoEncode)((IVideoEncode* )NULL, *dsc));
        _CreateVideoEncode.GetReturn(params_holder);

        return params_holder.ret_val;
    }

    DECLARE_TEST_METHOD1(IMFXVideoVPP*, CreateVPP, MAKE_DYNAMIC_TRAIT(PipelineObjectDescBase,const IPipelineObjectDesc& ) )
    {
        TEST_METHOD_TYPE(CreateVPP) params_holder;
        const PipelineObjectDescBase &dsc = dynamic_cast<const PipelineObjectDescBase &>(_0);
        _CreateVPP.RegisterEvent(TEST_METHOD_TYPE(CreateVPP)((IMFXVideoVPP* )NULL, dsc));
        _CreateVPP.GetReturn(params_holder);

        return params_holder.ret_val;
    }

    DECLARE_TEST_METHOD1(IYUVSource*, CreateDecode, IPipelineObjectDesc *)
    {
        UNREFERENCED_PARAMETER(_0);
        TEST_METHOD_TYPE(CreateDecode) params_holder;
        _CreateDecode.GetReturn(params_holder);

        return params_holder.ret_val;
    }

    DECLARE_TEST_METHOD1(IMFXVideoRender*, CreateRender, IPipelineObjectDesc *)
    {
        UNREFERENCED_PARAMETER(_0);
        TEST_METHOD_TYPE(CreateRender) params_holder;
        _CreateRender.GetReturn(params_holder);

        return params_holder.ret_val;
    }

    DECLARE_TEST_METHOD1(IAllocatorFactory*, CreateAllocatorFactory, IPipelineObjectDesc *)
    {
        UNREFERENCED_PARAMETER(_0);
        TEST_METHOD_TYPE(CreateAllocatorFactory) params_holder;
        _CreateAllocatorFactory.GetReturn(params_holder);

        return params_holder.ret_val;
    }

    DECLARE_TEST_METHOD1(IBitstreamConverterFactory *, CreateBitstreamCVTFactory, IPipelineObjectDesc * )
    {
        UNREFERENCED_PARAMETER(_0);
        TEST_METHOD_TYPE(CreateBitstreamCVTFactory) params_holder;
        _CreateBitstreamCVTFactory.GetReturn(params_holder);

        return params_holder.ret_val;
    }
    DECLARE_TEST_METHOD1(IFile *, CreatePARReader, IPipelineObjectDesc * )
    {
        UNREFERENCED_PARAMETER(_0);
        TEST_METHOD_TYPE(CreatePARReader) params_holder;
        _CreatePARReader.GetReturn(params_holder);

        return params_holder.ret_val;
    }
    DECLARE_TEST_METHOD1(IFile  *, CreateFileWriter, MAKE_DYNAMIC_TRAIT(PipelineObjectDescBase, IPipelineObjectDesc*) )
    {
        TEST_METHOD_TYPE(CreateFileWriter) params_holder;
        PipelineObjectDescBase *dsc = dynamic_cast<PipelineObjectDescBase *>(_0);
        _CreateFileWriter.RegisterEvent(TEST_METHOD_TYPE(CreateFileWriter)((IFile* )NULL, *dsc));
        _CreateFileWriter.GetReturn(params_holder);

        return params_holder.ret_val;
    }
};
