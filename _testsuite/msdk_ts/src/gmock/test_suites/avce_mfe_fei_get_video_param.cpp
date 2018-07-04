/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */


#include "avce_mfe_fei.h"

/*!

\file avce_mfe_fei_get_video_param.cpp
\brief Gmock test avce_mfe_fei_get_video_param

Description:

This test checks behavior of encoding GetVideoParam() for various values of the
MFE parameters with enabled FEI ENCODE function

Algorithm:
- Initializing Media SDK lib
- Initializing suite and case parameters
- Run Init()
- Check Init() status
- Run GetVideoParam()
- Check GetVideoParam() status
- Check output MFE parameters

*/

namespace avce_mfe_fei
{

int test_mfe_fei_get_video_param(mfxU32 id)
{
    TS_START;
    tsVideoEncoder enc(MFX_CODEC_AVC);
    tc_struct& tc =  test_case[id];

    enc.MFXInit();

    set_params(enc, tc);

    // Update some test specific parameters
    if ((tc.inMFMode == MFX_MF_AUTO || tc.inMFMode == MFX_MF_MANUAL) &&
            tc.inMaxNumFrames == 0)
    {
        if (g_tsHWtype == MFX_HW_SKL) {
            tc.expMaxNumFrames = 2;
        }
        else {
            tc.expMaxNumFrames = 1;
        }
    }
    if ((tc.inMFMode == MFX_MF_DEFAULT || tc.inMFMode > MFX_MF_MANUAL) &&
            tc.expMaxNumFrames == 1)
    {
        tc.expMFMode = MFX_MF_AUTO;
    }

    if (tc.feiFunc != MFX_FEI_FUNCTION_ENCODE) {
        // Update default status that is valid for Query() but it is
        // not valid for Init()
        tc.sts = MFX_ERR_INVALID_VIDEO_PARAM;
    }

    g_tsStatus.expect(tc.sts);
    enc.Init(enc.m_session, &enc.m_par);

    if (tc.feiFunc == MFX_FEI_FUNCTION_ENCODE) {
        tsExtBufType<mfxVideoParam> out_par;
        mfxExtMultiFrameParam& mfe_out = out_par;
        mfxExtFeiParam& fei_out = out_par;

        g_tsStatus.expect(MFX_ERR_NONE);
        enc.GetVideoParam(enc.m_session, &out_par);

        EXPECT_EQ(tc.expMaxNumFrames, mfe_out.MaxNumFrames);
        EXPECT_EQ(tc.expMFMode, mfe_out.MFMode);
    }

    TS_END;
    return 0;
}


TS_REG_TEST_SUITE(avce_mfe_fei_get_video_param, test_mfe_fei_get_video_param, sizeof(test_case)/sizeof(tc_struct));

};
