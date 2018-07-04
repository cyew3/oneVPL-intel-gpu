/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

/*!

\file avce_mfe_fei_close.cpp
\brief Gmock test avce_mfe_fei_close

Description:

This test checks behavior of encoding Close() after encoding Init() and Query()
when FEI MFE is enabled

Algorithm:
- Initializing Media SDK lib
- Get test mode and corresponding parameters depending on input test case id.
  If id belongs to range [0..test_case_num-1] then corresponding test case is
  run in Init mode (Close() is called after Init()). Otherwise it runs in Query
  mode (Close() is called after Query())
- Set video parameters
- Run Init() or Query() depending on test mode
- Run Close
- Check close status
*/

#include "avce_mfe_fei.h"

namespace avce_mfe_fei
{

typedef enum {
    TestModeInit = 0,
    TestModeQuery = 1
} testMode_e;

size_t test_case_num = sizeof(test_case) / sizeof(tc_struct);

int test_mfe_fei_close(mfxU32 id)
{
    TS_START;
    tsVideoEncoder enc(MFX_CODEC_AVC);
    testMode_e mode = (id < test_case_num) ? TestModeInit : TestModeQuery;
    size_t test_id = (mode == TestModeInit) ? id : id - test_case_num;
    tc_struct& tc =  test_case[test_id];

    enc.MFXInit();

    tsExtBufType<mfxVideoParam> out_par;
    mfxExtMultiFrameParam& mfe_out = out_par;
    mfxExtFeiParam& fei_out = out_par;
    out_par.mfx.CodecId = enc.m_par.mfx.CodecId;
    set_params(enc, tc);

    g_tsStatus.disable_next_check();
    if (mode == TestModeInit) {
        enc.Init(enc.m_session, &enc.m_par);
    }
    else {
        enc.Query(enc.m_session, &enc.m_par, &out_par);
        g_tsStatus.expect(MFX_ERR_NOT_INITIALIZED);
    }

    enc.Close();

    TS_END;
    return 0;
}


TS_REG_TEST_SUITE(avce_mfe_fei_close, test_mfe_fei_close, 2 * test_case_num);

};


