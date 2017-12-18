/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2018 Intel Corporation. All Rights Reserved.
//
//
*/

#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

/*
    Test implements 2 stages:
    1) Init VPP with CM implemented filter, Process frames, Close VPP
    2) Init VPP with MediaSDK filter, Process frames, Close VPP
    The order of accomplishment of stages depends on test_case.
    Expected that switching between VPP filters won't lead to crash
*/

namespace vpp_initclose_fieldcopy_robustness
{

class TestSuite : tsVideoVPP
{

public:

    TestSuite()
    {
        m_par.AsyncDepth = 1;
    }

    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:

    enum
    {
        FIELDCOPY_FIRST = 0,
        FIELDCOPY_RESET,
    };

    enum
    {
        MFX_PAR = 1,
        FCOPY
    };

    struct tc_struct
    {
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];

};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ FIELDCOPY_FIRST, {
        {FCOPY, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FRAME},
        {FCOPY, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {FCOPY, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD},
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*01*/ FIELDCOPY_FIRST, {
        {FCOPY, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FRAME},
        {FCOPY, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {FCOPY, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD},
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*02*/ FIELDCOPY_FIRST, {
        {FCOPY, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FRAME},
        {FCOPY, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {FCOPY, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD},
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}
    },

    {/*03*/ FIELDCOPY_RESET, {
        {FCOPY, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FRAME},
        {FCOPY, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {FCOPY, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD},
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}
    },
    {/*04*/ FIELDCOPY_RESET, {
        {FCOPY, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FRAME},
        {FCOPY, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {FCOPY, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD},
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*05*/ FIELDCOPY_RESET, {
        {FCOPY, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FRAME},
        {FCOPY, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {FCOPY, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD},
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    mfxU8 frame_number = 2;

    MFXInit();

    SETPARS(m_par, MFX_PAR);

    CreateAllocators();
    SetHandle();

    for (mfxU8 counter = 0; counter < 2; counter++)
    {
        if (counter == tc.mode)
        {
            SETPARS(m_par, FCOPY);
        }
        else
        {
            mfxExtVPPDenoise& dn = m_par;
            dn.DenoiseFactor = 67;
        }

        Init();

        if (m_par.IOPattern == (MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        {
            mfxExtOpaqueSurfaceAlloc& osa = m_par;
            m_pSurfPoolIn->AllocOpaque(m_request[0], osa);
            m_pSurfPoolOut->AllocOpaque(m_request[1], osa);
        }
        else
        {
            AllocSurfaces();
        }

        for(mfxU8 i = 0; i < frame_number; i++)
        {
            m_pSurfIn  = m_pSurfPoolIn->GetSurface();
            m_pSurfOut = m_pSurfPoolOut->GetSurface();
            mfxStatus sts = RunFrameVPPAsync(m_session, m_pSurfIn, m_pSurfOut, 0, m_pSyncPoint);
            g_tsStatus.check(sts);
        }

        Close();

        m_par.RemoveExtBuffer(MFX_EXTBUFF_VPP_FIELD_PROCESSING);
        m_par.RemoveExtBuffer(MFX_EXTBUFF_VPP_DENOISE);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_fieldcopy_robustness);

}
