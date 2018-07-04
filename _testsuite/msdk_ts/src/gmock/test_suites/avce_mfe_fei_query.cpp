/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

/*!

\file avce_mfe_fei_query.cpp
\brief Gmock test avce_mfe_fei_query

Description:

This test checks behavior of the encoding query for various values of the MFE
parameters with enabled FEI function (ENCODE|ENC|PREENC|PACK)

Algorithm:
- Initializing Media SDK lib
- Initializing suite and case parameters
- Run Query()
- Check Query() status
- Check output MFE parameters

*/

#include "avce_mfe_fei.h"

namespace avce_mfe_fei
{

int mfe_fei_query(mfxU32 id)
{
    TS_START;
    tsVideoEncoder enc(MFX_CODEC_AVC);
    tc_struct& tc =  test_case[id];

    enc.MFXInit();

    tsExtBufType<mfxVideoParam> out_par;
    mfxExtMultiFrameParam& mfe_out = out_par;
    mfxExtFeiParam& fei_out = out_par;
    out_par.mfx.CodecId = enc.m_par.mfx.CodecId;
    set_params(enc, tc);

    g_tsStatus.expect(tc.sts);
    enc.Query(enc.m_session, &enc.m_par, &out_par);
    EXPECT_EQ(tc.expMaxNumFrames, mfe_out.MaxNumFrames);
    EXPECT_EQ(tc.expMFMode, mfe_out.MFMode);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(avce_mfe_fei_query, mfe_fei_query, sizeof(test_case)/sizeof(tc_struct));

};
