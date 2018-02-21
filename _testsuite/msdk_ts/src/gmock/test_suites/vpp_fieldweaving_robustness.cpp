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
    Test implements the following scenarios:
    1) VPP Init-Process-Close between FW/FS filter and other VPP filter. Expected that switching
       between VPP filters won't lead to crash
    2) Changing input picstruct for FW/FS filter in runtime. Only checks that there's no unexpected
       error from MSDK and no crash.
*/

namespace vpp_field_weaving_splitting_robustness
{

class TestSuite : tsVideoVPP
{
protected:
    enum MODE
    {
        WEAVING_FIRST = 0,
        SPLITTING_FIRST,
        WEAVING_RESET,
        SPLITTING_RESET,
        WEAVING_SKIP_TOP,
        WEAVING_SKIP_BOTTOM,
        SPLITTING_PS_CHANGE
    };

    enum
    {
        MFX_PAR = 1,
    };

    struct tc_struct
    {
        MODE mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

public:
    TestSuite()
        : tsVideoVPP()
    {}
    virtual ~TestSuite() {}

    int RunTest(tc_struct const& tc, mfxU16 IOP);

private:

    int RunCloseInitTest(tc_struct const& tc); // switching filters between vpp session
    int RunFWFieldSkipTest(MODE mode); // changing input picstructs in runtime
    int RunFSPicstructChangeTest(); // changing input picstruct in runtime

};

const mfxU16 IOPatterns[] = {MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY,
                             MFX_IOPATTERN_IN_VIDEO_MEMORY |MFX_IOPATTERN_OUT_VIDEO_MEMORY,
                             MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY};

const mfxU32 frameNumber = 4;

int TestSuite::RunTest(tc_struct const& tc, mfxU16 IOP)
{
    TS_START;

    MFXInit();

    // setting output parameters for allocating surfaces
    m_par.IOPattern  = IOP;
    switch (tc.mode)
    {
        case WEAVING_SKIP_TOP:
        case WEAVING_SKIP_BOTTOM:
        case WEAVING_FIRST:
        case WEAVING_RESET:
        {
            m_par.vpp.Out.Height = m_par.vpp.In.Height << 1;
            m_par.vpp.Out.CropH  = m_par.vpp.In.CropH << 1;
            SETPARS(m_par, MFX_PAR);
            break;
        }
        case SPLITTING_PS_CHANGE:
        case SPLITTING_FIRST:
        case SPLITTING_RESET:
        {
            m_par.vpp.Out.Height = m_par.vpp.In.Height >> 1;
            m_par.vpp.Out.CropH  = m_par.vpp.In.CropH >> 1;
            SETPARS(m_par, MFX_PAR);
            break;
        }
        default:
            ADD_FAILURE() << "Unknown test mode";
            throw tsFAIL;

    }

    CreateAllocators();
    SetFrameAllocator();
    SetHandle();

    QueryIOSurf();
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

    // calling pipeline
    switch (tc.mode)
    {
        case WEAVING_FIRST:
        case SPLITTING_FIRST:
        case WEAVING_RESET:
        case SPLITTING_RESET:
            return RunCloseInitTest(tc);
        case WEAVING_SKIP_TOP:
        case WEAVING_SKIP_BOTTOM:
            return RunFWFieldSkipTest(tc.mode);
        case SPLITTING_PS_CHANGE:
            return RunFSPicstructChangeTest();
        default:
            ADD_FAILURE() << "Unknown pipeline";
            throw tsFAIL;
    }

    TS_END;
    return 0;
}

int TestSuite::RunCloseInitTest(tc_struct const& tc)
{
    for (mfxI8 first_init = 1; first_init >= 0; first_init--)
    {
         // current run is not FW/FS
        if ((first_init && tc.mode != SPLITTING_FIRST && tc.mode != WEAVING_FIRST)
            || (!first_init && tc.mode != SPLITTING_RESET && tc.mode != WEAVING_RESET))
        {
            mfxExtVPPDeinterlacing& di = m_par;
            di.Mode                    = MFX_DEINTERLACING_BOB;
            m_par.vpp.In.PicStruct     = MFX_PICSTRUCT_FIELD_TFF;
            m_par.vpp.Out.PicStruct    = MFX_PICSTRUCT_PROGRESSIVE;
        }
        else if (tc.mode == WEAVING_RESET || tc.mode == WEAVING_FIRST)
        {
            m_par.vpp.Out.Height = m_par.vpp.In.Height << 1;
            m_par.vpp.Out.CropH  = m_par.vpp.In.CropH << 1;
            SETPARS(m_par, MFX_PAR);
        }
        else
        {
            m_par.vpp.Out.Height = m_par.vpp.In.Height >> 1;
            m_par.vpp.Out.CropH  = m_par.vpp.In.CropH >> 1;
            SETPARS(m_par, MFX_PAR);
        }

        Init(m_session, m_pPar);

        mfxStatus sts = MFX_ERR_NONE;
        mfxU32 nframes = 0;
        while (nframes < frameNumber)
        {
            if (sts != MFX_ERR_MORE_SURFACE) // FS should use the same input surface twice
                m_pSurfIn  = m_pSurfPoolIn->GetSurface();
            if (sts != MFX_ERR_MORE_DATA)    // FW should use the same output surface twice
                m_pSurfOut = m_pSurfPoolOut->GetSurface();

            sts = RunFrameVPPAsync(m_session, m_pSurfIn, m_pSurfOut, 0, m_pSyncPoint);
            if (sts >= MFX_ERR_NONE)
                nframes++;
            if (sts != MFX_ERR_MORE_DATA && sts != MFX_ERR_MORE_SURFACE && sts < MFX_ERR_NONE)
            {
                g_tsLog << "ERROR: unexpected status in RunFrameVPPAsync\n";
                g_tsStatus.check(sts);
            }
        }

        Close();

        m_par.RemoveExtBuffer(MFX_EXTBUFF_VPP_DEINTERLACING);
    }

    return 0;
}

int TestSuite::RunFWFieldSkipTest(MODE mode)
{
    Init(m_session, m_pPar);

    mfxStatus sts = MFX_ERR_NONE;
    for (mfxU32 nfield = 0; nfield < frameNumber; nfield++)
    {
        m_pSurfIn  = m_pSurfPoolIn->GetSurface();
        printf("FIELD #%d\n", nfield);

        if (sts != MFX_ERR_MORE_DATA)    // FW should use the same output surface twice
            m_pSurfOut = m_pSurfPoolOut->GetSurface();

        if (mode == WEAVING_SKIP_TOP)
        {
            // Expected output is TFF, but input picstruct is always bottom
            // in that case VPP uses bottom field (data in surface) as top
            m_pSurfIn->Info.PicStruct = MFX_PICSTRUCT_FIELD_BOTTOM;

            if (nfield % 2)
                g_tsStatus.expect(MFX_ERR_NONE);
            else
                g_tsStatus.expect(MFX_ERR_MORE_DATA);

            sts = RunFrameVPPAsync(m_session, m_pSurfIn, m_pSurfOut, 0, m_pSyncPoint);
            g_tsStatus.check(sts);
        }
        else if (mode == WEAVING_SKIP_BOTTOM)
        {
            // Expected output is TFF, first two fields set as top+bottom picstructs,
            // for second pair of fields use top+top picstructs
            if (nfield < 2)
            {
                if (nfield % 2)
                {
                    m_pSurfIn->Info.PicStruct  = MFX_PICSTRUCT_FIELD_BOTTOM;
                    g_tsStatus.expect(MFX_ERR_NONE);
                }
                else
                {
                    m_pSurfIn->Info.PicStruct  = MFX_PICSTRUCT_FIELD_TOP;
                    g_tsStatus.expect(MFX_ERR_MORE_DATA);
                }
            }
            else
            {
                m_pSurfIn->Info.PicStruct  = MFX_PICSTRUCT_FIELD_TOP;

                if (nfield % 2)
                    g_tsStatus.expect(MFX_ERR_NONE);
                else
                    g_tsStatus.expect(MFX_ERR_MORE_DATA);
            }

            sts = RunFrameVPPAsync(m_session, m_pSurfIn, m_pSurfOut, 0, m_pSyncPoint);
            g_tsStatus.check(sts);
        }
    }

    Close();
    return 0;
}

int TestSuite::RunFSPicstructChangeTest()
{
    // Init with input picstruct TFF, change to BFF in runtime
    Init(m_session, m_pPar);

    mfxStatus sts = MFX_ERR_NONE;
    for (mfxU32 nfield = 0; nfield < frameNumber; nfield++)
    {
        if (sts != MFX_ERR_MORE_SURFACE) // FS should use the same input surface
        {
            m_pSurfIn  = m_pSurfPoolIn->GetSurface();
            if (nfield % 2)
                m_pSurfIn->Info.PicStruct = MFX_PICSTRUCT_FIELD_BFF;
        }

        m_pSurfOut = m_pSurfPoolOut->GetSurface();

        if (nfield % 2)
            g_tsStatus.expect(MFX_ERR_NONE);
        else
            g_tsStatus.expect(MFX_ERR_MORE_SURFACE);

        sts = RunFrameVPPAsync(m_session, m_pSurfIn, m_pSurfOut, 0, m_pSyncPoint);
        g_tsStatus.check(sts);
    }

    Close();
    return 0;
}

enum TEST
{
    FW_TEST,
    FS_TEST
};

template<unsigned test_type>
class TestSuiteExt : public TestSuite
{
public:
    static int RunTest(unsigned int id);
    static tc_struct const test_cases[];
    static unsigned int const n_cases;
};

template<>
TestSuite::tc_struct const TestSuiteExt<FW_TEST>::test_cases[] =
{
    // First init with field weaving
    {/*00*/ WEAVING_FIRST, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct,  MFX_PICSTRUCT_FIELD_SINGLE},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_TFF}}
    },

    // Deinterlacing before field weaving
    {/*03*/ WEAVING_RESET, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct,  MFX_PICSTRUCT_FIELD_SINGLE},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_BFF}}
    },

    // Field weaving set input picstruct only bottom field
    {/*06*/ WEAVING_SKIP_TOP, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct,  MFX_PICSTRUCT_FIELD_SINGLE},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_TFF}}
    },

    // Field weaving, first frame processed from top+bottom input picstructs,
    // second frame from top+top
    {/*09*/ WEAVING_SKIP_BOTTOM, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct,  MFX_PICSTRUCT_FIELD_SINGLE},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_TFF}}
    },
};
template <>
unsigned int const TestSuiteExt<FW_TEST>::n_cases = sizeof(TestSuiteExt<FW_TEST>::test_cases)/sizeof(TestSuite::tc_struct) * (sizeof(IOPatterns)/sizeof(mfxU16));


template<>
TestSuite::tc_struct const TestSuiteExt<FS_TEST>::test_cases[] =
{
    // First init with field splitting
    {/*00*/ SPLITTING_FIRST, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct,  MFX_PICSTRUCT_FIELD_TFF},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE}}
    },

    // Deinterlacing before field splitting
    {/*03*/ SPLITTING_RESET, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct,  MFX_PICSTRUCT_FIELD_BFF},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE}}
    },

    // Field splitting init with tff frame, change in runtime to bff
    {/*06*/ SPLITTING_PS_CHANGE, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct,  MFX_PICSTRUCT_FIELD_TFF},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE}}
    },
};
template <>
unsigned int const TestSuiteExt<FS_TEST>::n_cases = sizeof(TestSuiteExt<FS_TEST>::test_cases)/sizeof(TestSuite::tc_struct) * (sizeof(IOPatterns)/sizeof(mfxU16));

template <unsigned test_type>
int TestSuiteExt<test_type>::RunTest(unsigned int id)
{
    auto tc = test_cases[id / (sizeof(IOPatterns)/sizeof(mfxU16))];
    mfxU16 iop = IOPatterns[id % (sizeof(IOPatterns)/sizeof(mfxU16))];

    TestSuite suite;
    if (test_type == FS_TEST)
        return suite.RunTest(tc, iop);
    else if (test_type == FW_TEST)
        return suite.RunTest(tc, iop);
    else
        ADD_FAILURE() << "Wrong test type!";
        throw tsFAIL;
}

TS_REG_TEST_SUITE(vpp_fieldweaving_robustness,   TestSuiteExt<FW_TEST>::RunTest, TestSuiteExt<FW_TEST>::n_cases);
TS_REG_TEST_SUITE(vpp_fieldsplitting_robustness, TestSuiteExt<FS_TEST>::RunTest, TestSuiteExt<FS_TEST>::n_cases);
}
