/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include <type_traits>
#include <iostream>

#include "ts_encoder.h"
#include "avce_mfe.h"

/*!

\file avce_mfe_init.cpp
\brief Gmock test avce_mfe_get_video_param.cpp

Description:

This test checks behavior of encoding GetVideoParam() for various values of the
MFE parameters (fields MFMode and MaxNumFrames) combined with other parameters
that affect the MFE functionality

Algorithm:
- Initializing Media SDK lib
- Initializing suite and case parameters
- Run Init()
- Check Init() status
- Run GetVideoParam()
- Check GetVideoParam() status
- Check output MFE parameters

*/

namespace avce_mfe
{

int test_get_video_param(unsigned int id)
{
    TS_START;
    tsVideoEncoder enc(MFX_CODEC_AVC);
    tc_struct& tc =  test_case[id];

    enc.MFXInit();

    tsExtBufType<mfxVideoParam> pout;
    mfxExtMultiFrameParam& mfe_out = pout;

    set_params(enc, tc, pout);

    // Update some test specific parameters
    if ((tc.inMFMode == MFX_MF_AUTO || tc.inMFMode == MFX_MF_MANUAL) &&
            tc.inMaxNumFrames == 0)
    {
        if (g_tsHWtype == MFX_HW_SKL) {
            tc.expMaxNumFrames = 3;
        }
        else {
            tc.expMaxNumFrames = 1;
        }
    }
    if (tc.cOpt & COptMAD) {
        tc.sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    g_tsStatus.expect(tc.sts);
    enc.Init(enc.m_session, &enc.m_par);

    g_tsStatus.expect(MFX_ERR_NONE);
    enc.GetVideoParam(enc.m_session, &pout);

    EXPECT_EQ(tc.expMaxNumFrames, mfe_out.MaxNumFrames);
    EXPECT_EQ(tc.expMFMode, mfe_out.MFMode);

    TS_END;
    return 0;
}


TS_REG_TEST_SUITE(avce_mfe_get_video_param, test_get_video_param, test_case_num);

};
