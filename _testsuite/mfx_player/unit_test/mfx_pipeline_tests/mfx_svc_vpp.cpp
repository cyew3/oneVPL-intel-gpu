/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_svc_vpp.h"
#include "mock_mfx_vpp.h"

SUITE(mfx_svc_vpp)
{
    TEST(DependencyidOrder)
    {
        MockVpp vpp;
        TEST_METHOD_TYPE(MockVpp::RunFrameVPPAsync) run;

        run.ret_val = MFX_ERR_MORE_SURFACE;
        vpp._RunFrameVPPAsync.WillReturn(run);
        vpp._RunFrameVPPAsync.WillReturn(run);
        run.ret_val = MFX_WRN_DEVICE_BUSY;
        vpp._RunFrameVPPAsync.WillReturn(run);
        run.ret_val = MFX_ERR_MORE_SURFACE;
        vpp._RunFrameVPPAsync.WillReturn(run);

        run.ret_val = MFX_ERR_NONE;
        vpp._RunFrameVPPAsync.WillReturn(run);

        run.ret_val = MFX_ERR_MORE_SURFACE;
        vpp._RunFrameVPPAsync.WillReturn(run);
        
        run.ret_val = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        vpp._RunFrameVPPAsync.WillReturn(run);
        
        run.ret_val = MFX_ERR_NONE;
        vpp._RunFrameVPPAsync.WillReturn(run);

        SVCedVpp svpp (MakeUndeletable(vpp));

        mfxFrameSurface1 srfin = {0};
        mfxFrameSurface1 srfout = {0};

        CHECK_EQUAL(MFX_ERR_MORE_SURFACE, svpp.RunFrameVPPAsync(&srfin, &srfout, 0, 0));
        CHECK(vpp._RunFrameVPPAsync.WasCalled(&run));
        CHECK_EQUAL(0, run.value1.Info.FrameId.DependencyId);
        
        CHECK_EQUAL(MFX_ERR_MORE_SURFACE, svpp.RunFrameVPPAsync(&srfin, &srfout, 0, 0));
        CHECK(vpp._RunFrameVPPAsync.WasCalled(&run));
        CHECK_EQUAL(1, run.value1.Info.FrameId.DependencyId);

        CHECK_EQUAL(MFX_WRN_DEVICE_BUSY, svpp.RunFrameVPPAsync(&srfin, &srfout, 0, 0));
        CHECK(vpp._RunFrameVPPAsync.WasCalled(&run));
        CHECK_EQUAL(2, run.value1.Info.FrameId.DependencyId);
        
        //device busy not resulted in dependency id incrementing
        CHECK_EQUAL(MFX_ERR_MORE_SURFACE, svpp.RunFrameVPPAsync(&srfin, &srfout, 0, 0));
        CHECK(vpp._RunFrameVPPAsync.WasCalled(&run));
        CHECK_EQUAL(2, run.value1.Info.FrameId.DependencyId);

        //err none  resulted in frame id dropped to input value
        CHECK_EQUAL(MFX_ERR_NONE, svpp.RunFrameVPPAsync(&srfin, &srfout, 0, 0));
        CHECK(vpp._RunFrameVPPAsync.WasCalled(&run));
        CHECK_EQUAL(3, run.value1.Info.FrameId.DependencyId);

        CHECK_EQUAL(MFX_ERR_MORE_SURFACE, svpp.RunFrameVPPAsync(&srfin, &srfout, 0, 0));
        CHECK(vpp._RunFrameVPPAsync.WasCalled(&run));
        CHECK_EQUAL(0, run.value1.Info.FrameId.DependencyId);

        //err none resulted in frame id dropped to input value
        CHECK_EQUAL(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, svpp.RunFrameVPPAsync(&srfin, &srfout, 0, 0));
        CHECK(vpp._RunFrameVPPAsync.WasCalled(&run));
        CHECK_EQUAL(1, run.value1.Info.FrameId.DependencyId);

        //warning resulted in frame id dropped to input value
        CHECK_EQUAL(MFX_ERR_NONE, svpp.RunFrameVPPAsync(&srfin, &srfout, 0, 0));
        CHECK(vpp._RunFrameVPPAsync.WasCalled(&run));
        CHECK_EQUAL(0, run.value1.Info.FrameId.DependencyId);

    }
}