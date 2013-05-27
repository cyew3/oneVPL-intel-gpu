/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mock_mfx_pipeline_factory.h"
#include "mock_bitstream_converter.h"


IMFXVideoVPP* MockPipelineFactory :: CreateVPP (IPipelineObjectDesc *)
{
    TEST_METHOD_TYPE(CreateVPP) params_holder;
    _CreateVPP.GetReturn(params_holder);

    return params_holder.ret_val;
}

IYUVSource*  MockPipelineFactory :: CreateDecode(IPipelineObjectDesc *)
{
    TEST_METHOD_TYPE(CreateDecode) params_holder;
    _CreateVPP.GetReturn(params_holder);

    return params_holder.ret_val;
}

IMFXVideoRender* MockPipelineFactory :: CreateRender(IPipelineObjectDesc *)
{
    TEST_METHOD_TYPE(CreateRender) params_holder;
    _CreateVPP.GetReturn(params_holder);

    return params_holder.ret_val;
}

IAllocatorFactory* MockPipelineFactory :: CreateAllocatorFactory(IPipelineObjectDesc *)
{
    TEST_METHOD_TYPE(CreateAllocatorFactory) params_holder;
    _CreateVPP.GetReturn(params_holder);

    return params_holder.ret_val;
}

IBitstreamConverterFactory * MockPipelineFactory :: CreateBitstreamCVTFactory(IPipelineObjectDesc * )
{
    TEST_METHOD_TYPE(CreateBitstreamCVTFactory) params_holder;
    _CreateVPP.GetReturn(params_holder);

    return params_holder.ret_val;
}
