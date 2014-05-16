#include "ts_vpp.h"

namespace ptir_query_io_surf
{

typedef struct
{
    mfxSession            session;
    mfxVideoParam*        pPar;
    mfxFrameAllocRequest* pRequest;
}QIOSpar;

typedef void (*callback)(QIOSpar&);

void zero_session   (QIOSpar& p) { p.session = 0; }
void zero_param     (QIOSpar& p) { p.pPar = 0; }
void zero_request   (QIOSpar& p) { p.pRequest = 0; }

typedef struct
{
    callback    set_par;
    mfxU32      IOPattern;
    mfxU32      AsyncDepth;
    mfxStatus   sts;
} tc_struct;

tc_struct test_case[] =
{
    // zero values
    {/*0*/ zero_session, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 0, MFX_ERR_INVALID_HANDLE },
    {/*1*/ zero_param, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 0, MFX_ERR_NULL_PTR },
    {/*2*/ zero_request, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 0, MFX_ERR_NULL_PTR },

    // negative (IN only)
    {/*3*/ 0, MFX_IOPATTERN_IN_SYSTEM_MEMORY, 0, MFX_ERR_INVALID_VIDEO_PARAM },
    {/*3*/ 0, MFX_IOPATTERN_IN_VIDEO_MEMORY, 0, MFX_ERR_INVALID_VIDEO_PARAM },
    {/*3*/ 0, MFX_IOPATTERN_IN_OPAQUE_MEMORY, 0, MFX_ERR_INVALID_VIDEO_PARAM },

    // negative (OUT only)
    {/*3*/ 0, MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 0, MFX_ERR_INVALID_VIDEO_PARAM },
    {/*3*/ 0, MFX_IOPATTERN_OUT_VIDEO_MEMORY, 0, MFX_ERR_INVALID_VIDEO_PARAM },
    {/*3*/ 0, MFX_IOPATTERN_OUT_OPAQUE_MEMORY, 0, MFX_ERR_INVALID_VIDEO_PARAM },

    // IN = SYSTEM
    {/*3*/ 0, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 0, MFX_ERR_NONE },
    {/*4*/ 0, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY, 0, MFX_ERR_NONE },
    {/*4*/ 0, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY, 0, MFX_ERR_NONE },

    // IN = VIDEO
    {/*3*/ 0, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 0, MFX_ERR_NONE },
    {/*4*/ 0, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY, 0, MFX_ERR_NONE },
    {/*4*/ 0, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY, 0, MFX_ERR_NONE },

    // IN = OPAQ
    {/*3*/ 0, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 0, MFX_ERR_NONE },
    {/*4*/ 0, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY, 0, MFX_ERR_NONE },
    {/*4*/ 0, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY, 0, MFX_ERR_NONE },

    // Async
    // {/*4*/ 0, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 5, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
    // {/*4*/ 0, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY, 5, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
    // {/*4*/ 0, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY, 5, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM },
};

enum
{
    IOP_IN  = (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_IN_OPAQUE_MEMORY),
    IOP_OUT = (MFX_IOPATTERN_OUT_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY),
    IOP_NUM = (1<<(3+3)),
};


tc_struct GetTestCase(unsigned int id)
{
    if(id < sizeof(test_case)/sizeof(tc_struct))
        return test_case[id];

    //all possible combinations of IOPattern
    tc_struct tc = {};
    id -= sizeof(test_case)/sizeof(tc_struct); // exclude pre-defined cases

    // get IOPattern IN from test-case id
    tc.IOPattern = (id & IOP_IN);

    // get IOPattern OUT from rest of test-case id
    id &= ~IOP_IN;
    while(id && (id & (~IOP_OUT)))
        id <<= 1;
    tc.IOPattern |= id;

    // pure IOPatterns only
    if((tc.IOPattern == (MFX_IOPATTERN_IN_VIDEO_MEMORY |MFX_IOPATTERN_OUT_VIDEO_MEMORY )) ||
       (tc.IOPattern == (MFX_IOPATTERN_IN_VIDEO_MEMORY |MFX_IOPATTERN_OUT_SYSTEM_MEMORY)) ||
       (tc.IOPattern == (MFX_IOPATTERN_IN_VIDEO_MEMORY |MFX_IOPATTERN_OUT_OPAQUE_MEMORY)) ||
       (tc.IOPattern == (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY )) ||
       (tc.IOPattern == (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY)) ||
       (tc.IOPattern == (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY)) ||
       (tc.IOPattern == (MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY )) ||
       (tc.IOPattern == (MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY)) ||
       (tc.IOPattern == (MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY)))
       tc.sts = MFX_ERR_NONE;
    else
        tc.sts = MFX_ERR_INVALID_VIDEO_PARAM;

    return tc;
}


int test(unsigned int id)
{
    TS_START;
    tsVideoVPP vpp(true, MFX_MAKEFOURCC('P', 'T', 'I', 'R'));
    tc_struct tc = GetTestCase(id);

    vpp.MFXInit();
    vpp.Load();

    QIOSpar par = {vpp.m_session, &vpp.m_par, vpp.m_request};
    vpp.m_par.IOPattern  = tc.IOPattern;
    vpp.m_par.AsyncDepth = 1;

    if(tc.set_par)
    {
        (*tc.set_par)(par);
    }

    g_tsStatus.expect(tc.sts);
    mfxStatus sts = vpp.QueryIOSurf(par.session, par.pPar, par.pRequest);

    if(sts >= MFX_ERR_NONE)
    {
        mfxU32 expectedType[] = {MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_EXTERNAL_FRAME,
                                 MFX_MEMTYPE_FROM_VPPOUT|MFX_MEMTYPE_EXTERNAL_FRAME};

        if (vpp.m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
            expectedType[0] |= MFX_MEMTYPE_SYSTEM_MEMORY;
        else if (vpp.m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
            expectedType[0] |= MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
        else
            expectedType[0] |= MFX_MEMTYPE_OPAQUE_FRAME;

        if (vpp.m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
            expectedType[1] |= MFX_MEMTYPE_SYSTEM_MEMORY;
        else if (vpp.m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
            expectedType[1] |= MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
        else
            expectedType[1] |= MFX_MEMTYPE_OPAQUE_FRAME;

        if (vpp.m_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
            EXPECT_EQ( vpp.m_request[0].Type & (MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_FROM_VPPIN),
                                               (MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_FROM_VPPIN) );
        else
            EXPECT_EQ( expectedType[0], vpp.m_request[0].Type );
        EXPECT_EQ( vpp.m_par.vpp.In, vpp.m_request[0].Info );
        EXPECT_GT( vpp.m_request[0].NumFrameMin, 0 );
        EXPECT_GE( vpp.m_request[0].NumFrameSuggested, vpp.m_request[0].NumFrameMin );

        if (vpp.m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
            EXPECT_EQ( vpp.m_request[1].Type & (MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_FROM_VPPOUT),
                                               (MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_FROM_VPPOUT) );
        else
            EXPECT_EQ( expectedType[1], vpp.m_request[1].Type );
        EXPECT_EQ( vpp.m_par.vpp.Out, vpp.m_request[1].Info );
        EXPECT_GT( vpp.m_request[1].NumFrameMin, 0 );
        EXPECT_GE( vpp.m_request[1].NumFrameSuggested, vpp.m_request[1].NumFrameMin );
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(ptir_query_io_surf, test, sizeof(test_case)/sizeof(tc_struct) + IOP_NUM);
}