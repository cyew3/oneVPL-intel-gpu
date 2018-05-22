/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

/*!

\file avce_mfe_close.cpp
\brief Gmock test avce_mfe_close

Description:

This test checks behavior of the encoding Close() when MFE is enabled

Algorithm:
- Initializing Media SDK lib
- Get test parameters depending on input test case id.
  Note: total number of test cases is 2 * test_case_num for test_case_num
  defined in avce_mfe_query.cpp. If id is less than test_case_num the test
  will be run in TestModeInit mode. Otherwise it will be run in TestModeQuery
- Set video parameters
- Run Init() or Query() depending on test mode
- Run Close
- Check close status
*/

#include "ts_encoder.h"
#include "avce_mfe.h"

namespace avce_mfe
{

typedef enum {
    TestModeInit = 0,
    TestModeQuery = 1
} testMode_e;

int test_close(unsigned int id)
{
    TS_START;
    tsVideoEncoder enc(MFX_CODEC_AVC);
    testMode_e mode = (id < test_case_num) ? TestModeInit : TestModeQuery;
    size_t test_id = (mode == TestModeInit) ? id : id - test_case_num;
    tc_struct& tc =  test_case[test_id];

    enc.MFXInit();

    tsExtBufType<mfxVideoParam> pout;
    mfxExtMultiFrameParam& mfe_out = pout;
    set_params(enc, tc, pout);

    g_tsStatus.disable_next_check();
    if (mode == TestModeInit) {
        enc.Init(enc.m_session, &enc.m_par);
    }
    else {
        enc.Query(enc.m_session, &enc.m_par, &pout);
        g_tsStatus.expect(MFX_ERR_NOT_INITIALIZED);
    }

    enc.Close();

    TS_END;
    return 0;
}


TS_REG_TEST_SUITE(avce_mfe_close, test_close, 2 * test_case_num);

};


