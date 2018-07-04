/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include <type_traits>
#include <iostream>

#include "avce_mfe_fei.h"

/*!

\file avce_mfe_fei_query_io_surf.cpp
\brief Gmock test avce_mfe_fei_query_io_surf

Description:

This test checks behavior of the encoding QueryIOSurf() both when FEI MFE is
enabled and not

Algorithm:
- Initializing Media SDK lib
- Initializing suite and case parameters
- Run QueryIOSurf() without FEI MFE parameters
- Run QueryIOSurf() with FEI MFE parameters
- Check that request results are the same for both QueryIOSurf() calls

*/

namespace avce_mfe_fei
{

int test_mfe_fei_query_io_surf(mfxU32 id)
{
    TS_START;
    tsVideoEncoder enc(MFX_CODEC_AVC);
    tc_struct& tc =  test_case[id];

    enc.MFXInit();

    g_tsStatus.expect(MFX_ERR_NONE);

    // Run QueryIOSurf() without MFE parameters
    set_params(enc, tc);
    enc.QueryIOSurf();

    // Save request result before next QueryIOSurf() call
    mfxFrameAllocRequest req = *enc.m_pRequest;

    // Run QueryIOSurf() with MFE parameters and enabled FEI
    set_params(enc, tc, /*fei_enabled*/false, /*mfe_enabled*/false);
    enc.QueryIOSurf();

    // Check request results
    EXPECT_EQ(0, (memcmp(&req, enc.m_pRequest, sizeof(mfxFrameAllocRequest))));

    TS_END;
    return 0;
}


TS_REG_TEST_SUITE(avce_mfe_fei_query_io_surf, test_mfe_fei_query_io_surf, sizeof(test_case)/sizeof(tc_struct));

};
