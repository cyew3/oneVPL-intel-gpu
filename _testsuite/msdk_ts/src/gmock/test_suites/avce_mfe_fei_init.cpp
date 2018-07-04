/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "avce_mfe_fei.h"

/*!

\file avce_mfe_fei_init.cpp
\brief Gmock test avce_mfe_fei_init

Description:

This test checks behavior of the encoding Init() for various values of the MFE
parameters with enabled FEI function (ENCODE|ENC|PREENC|PACK)

Algorithm:
- Initializing Media SDK lib
- Initializing suite and case parameters
- Run Init()
- Check Init() status
- Check that input MFE parameters are not changed

*/

namespace avce_mfe_fei
{

int mfe_fei_init(mfxU32 id)
{
    TS_START;
    tsVideoEncoder enc(MFX_CODEC_AVC);
    tc_struct& tc =  test_case[id];

    enc.MFXInit();
    set_params(enc, tc);

    if (tc.feiFunc != MFX_FEI_FUNCTION_ENCODE) {
        // Update default status that is valid for Query() but it is
        // not valid for Init()
        tc.sts = MFX_ERR_INVALID_VIDEO_PARAM;
    }

    g_tsStatus.expect(tc.sts);
    enc.Init(enc.m_session, &enc.m_par);

    mfxExtMultiFrameParam& mfe_in = enc.m_par;
    EXPECT_EQ(mfe_in.MaxNumFrames, tc.inMaxNumFrames);
    EXPECT_EQ(mfe_in.MFMode, tc.inMFMode);

    TS_END;
    return 0;
}


TS_REG_TEST_SUITE(avce_mfe_fei_init, mfe_fei_init, sizeof(test_case)/sizeof(tc_struct));

};
