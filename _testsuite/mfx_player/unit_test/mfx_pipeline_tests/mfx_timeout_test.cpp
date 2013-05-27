/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */
#include "utest_common.h"
#include "mfx_timeout.h"
#include "mfx_pipeline_dec.h"
#include "mfx_decode_pipeline_config.h"

#if defined(DEBUG) || defined (_DEBUG)
#define MAX_SYNC_WAIT      300000u // 5 min
#else
#define MAX_SYNC_WAIT      15000u // 15 sec
#endif


SUITE(TIMEOUT)
{
    TEST(generic)
    {
        CHECK_EQUAL(MAX_SYNC_WAIT, TimeoutVal<>::val());
        CHECK_EQUAL(MAX_SYNC_WAIT, TimeoutVal<PIPELINE_TIMEOUT_FFS>::val());

        SetTimeout(PIPELINE_TIMEOUT_GENERIC, 30);

        CHECK_EQUAL(30u, TimeoutVal<>::val());
        CHECK_EQUAL(MAX_SYNC_WAIT, TimeoutVal<PIPELINE_TIMEOUT_FFS>::val());

        SetTimeout(PIPELINE_TIMEOUT_FFS, 33);

        CHECK_EQUAL(30u, TimeoutVal<>::val());
        CHECK_EQUAL(33u, TimeoutVal<PIPELINE_TIMEOUT_FFS>::val());
    }

    TEST(remained_value)
    {
        CHECK_EQUAL(30u, TimeoutVal<>::val());
        CHECK_EQUAL(33u, TimeoutVal<PIPELINE_TIMEOUT_FFS>::val());
    }
    
    TEST(within_pipline_cmd_line)
    {
        PipelineRunner<MFXPipelineConfigDecode>::CmdLineParams params;
        std::vector<tstring> opt_detail;
        opt_detail.push_back(VM_STRING("32"));

        params.insert(std::make_pair(VM_STRING("-syncop_timeout"), opt_detail));

        PipelineRunner<MFXPipelineConfigDecode> pipeline;
        CHECK_EQUAL(MFX_ERR_NONE, pipeline.ProcessCommand(params));

        CHECK_EQUAL(32u, TimeoutVal<>::val());
    }
}