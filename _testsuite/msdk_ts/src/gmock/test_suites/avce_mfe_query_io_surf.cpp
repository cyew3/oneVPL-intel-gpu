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

\file avce_mfe_query_io_surf.cpp
\brief Gmock test avce_mfe_query_io_surf

Description:

This test checks behavior of the encoding QueryIOSurf() when video parameters
are passed with and without MFE parameters

Algorithm:
- Initializing Media SDK lib
- Initializing suite and case parameters
- Run QueryIOSurf() without MFE parameters
- Run QueryIOSurf() with MFE parameters
- Check that request results are the same for both QueryIOSurf() calls

*/

namespace avce_mfe
{

void set_query_io_params(tsVideoEncoder& enc, tc_struct& tc)
{
    enc.m_par.mfx.FrameInfo.Width = MSDK_ALIGN16(tc.width);
    enc.m_par.mfx.FrameInfo.CropW = MSDK_ALIGN16(tc.cropW);
    enc.m_par.mfx.FrameInfo.CropH = MSDK_ALIGN16(tc.cropH);

    if (tc.width > 1920 && tc.height > 1088) {
        enc.m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        enc.m_par.mfx.FrameInfo.Height = MSDK_ALIGN32(tc.height);
    }
    else {
        enc.m_par.mfx.FrameInfo.Height = MSDK_ALIGN16(tc.height);
    }

    enc.m_par.mfx.NumSlice = tc.numSlice;
}

int test_query_io_surf(unsigned int id)
{
    TS_START;
    tsVideoEncoder enc(MFX_CODEC_AVC);
    tc_struct& tc =  test_case[id];

    enc.MFXInit();

    g_tsStatus.expect(MFX_ERR_NONE);

    // Run QueryIOSurf() without MFE parameters
    set_query_io_params(enc, tc);
    enc.QueryIOSurf();

    // Save request result before next QueryIOSurf() call
    mfxFrameAllocRequest req;
    memcpy(&req, enc.m_pRequest, sizeof(mfxFrameAllocRequest));
    memset(enc.m_pRequest, 0, sizeof(mfxFrameAllocRequest));

    // Run QueryIOSurf() with MFE parameters
    tsExtBufType<mfxVideoParam> pout;
    mfxExtMultiFrameParam& mfe_out = pout;
    set_params(enc, tc, pout);
    enc.QueryIOSurf();

    // Check request results
    EXPECT_EQ(0, (memcmp(&req, enc.m_pRequest, sizeof(mfxFrameAllocRequest))));

    TS_END;
    return 0;
}


TS_REG_TEST_SUITE(avce_mfe_query_io_surf, test_query_io_surf, test_case_num);

};
