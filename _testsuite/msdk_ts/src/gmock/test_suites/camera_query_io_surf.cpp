#include "ts_vpp.h"

namespace camera_query_io_surf
{

typedef struct
{
    mfxSession            session;
    mfxVideoParam*        pPar;
    mfxFrameAllocRequest* pRequest;
}QIOSpar;

typedef void (*callback)(QIOSpar&);
void wrong_resol(QIOSpar& qiospar)
{
    qiospar.pPar->vpp.In.Width  = qiospar.pPar->vpp.Out.Width  = 721;
    qiospar.pPar->vpp.In.Height = qiospar.pPar->vpp.Out.Height = 481;
    qiospar.pPar->vpp.In.CropW  = qiospar.pPar->vpp.Out.CropW  = 741;
    qiospar.pPar->vpp.In.CropH  = qiospar.pPar->vpp.Out.CropH  = 582;
}
void wrong_ARs(QIOSpar& qiospar)
{
    qiospar.pPar->vpp.In.AspectRatioW  = qiospar.pPar->vpp.Out.AspectRatioW  = 666;
    qiospar.pPar->vpp.In.AspectRatioH = qiospar.pPar->vpp.Out.AspectRatioH = 0;
}

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
    {/*0 */ zero_session, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 0, MFX_ERR_INVALID_HANDLE },
    {/*1 */ zero_param,   MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 0, MFX_ERR_NULL_PTR },
    {/*2 */ zero_request, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 0, MFX_ERR_NULL_PTR },
    // negative (IN only)
    {/*3 */ 0, MFX_IOPATTERN_IN_SYSTEM_MEMORY, 0, MFX_ERR_INVALID_VIDEO_PARAM },
    {/*4 */ 0, MFX_IOPATTERN_IN_VIDEO_MEMORY,  0, MFX_ERR_INVALID_VIDEO_PARAM },
    {/*5 */ 0, MFX_IOPATTERN_IN_OPAQUE_MEMORY, 0, MFX_ERR_INVALID_VIDEO_PARAM },
    // negative (OUT only)
    {/*6 */ 0, MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 0, MFX_ERR_INVALID_VIDEO_PARAM },
    {/*7 */ 0, MFX_IOPATTERN_OUT_VIDEO_MEMORY,  0, MFX_ERR_INVALID_VIDEO_PARAM },
    {/*8 */ 0, MFX_IOPATTERN_OUT_OPAQUE_MEMORY, 0, MFX_ERR_INVALID_VIDEO_PARAM },
        //AsyncDepth = 0
    // IN = SYSTEM
    {/*9 */ 0, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 0, MFX_ERR_NONE },
    {/*10*/ 0, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY,  0, MFX_ERR_NONE },
    {/*11*/ 0, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY, 0, MFX_ERR_INVALID_VIDEO_PARAM },
    // IN = VIDEO
    {/*12*/ 0, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY,  0, /*MFX_ERR_NONE*/ MFX_ERR_INVALID_VIDEO_PARAM },
    {/*13*/ 0, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY,   0, /*MFX_ERR_NONE*/ MFX_ERR_INVALID_VIDEO_PARAM },
    {/*14*/ 0, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY,  0, /*MFX_ERR_NONE*/ MFX_ERR_INVALID_VIDEO_PARAM },
    // IN = OPAQ
    {/*15*/ 0, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 0, MFX_ERR_INVALID_VIDEO_PARAM },
    {/*16*/ 0, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY,  0, MFX_ERR_INVALID_VIDEO_PARAM },
    {/*17*/ 0, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY, 0, MFX_ERR_INVALID_VIDEO_PARAM },
        //AsyncDepth = 3
    // IN = SYSTEM
    {/*18*/ 0, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 3, MFX_ERR_NONE },
    {/*19*/ 0, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY,  3, MFX_ERR_NONE },
    {/*20*/ 0, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY, 3, MFX_ERR_INVALID_VIDEO_PARAM },
    // IN = VIDEO
    {/*21*/ 0, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY,  3, /*MFX_ERR_NONE*/ MFX_ERR_INVALID_VIDEO_PARAM },
    {/*22*/ 0, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY,   3, /*MFX_ERR_NONE*/ MFX_ERR_INVALID_VIDEO_PARAM },
    {/*23*/ 0, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY,  3, /*MFX_ERR_NONE*/ MFX_ERR_INVALID_VIDEO_PARAM },
    // IN = OPAQ
    {/*24*/ 0, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 3, MFX_ERR_INVALID_VIDEO_PARAM },
    {/*25*/ 0, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY,  3, MFX_ERR_INVALID_VIDEO_PARAM },
    {/*26*/ 0, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY, 3, MFX_ERR_INVALID_VIDEO_PARAM },
        //AsyncDepth = 5
    // IN = SYSTEM
    {/*27*/ 0, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 5, MFX_ERR_NONE },
    {/*28*/ 0, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY,  5, MFX_ERR_NONE },
    {/*29*/ 0, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY, 5, MFX_ERR_INVALID_VIDEO_PARAM },
    // IN = VIDEO
    {/*30*/ 0, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY,  5, /*MFX_ERR_NONE*/ MFX_ERR_INVALID_VIDEO_PARAM },
    {/*31*/ 0, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY,   5, /*MFX_ERR_NONE*/ MFX_ERR_INVALID_VIDEO_PARAM },
    {/*32*/ 0, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY,  5, /*MFX_ERR_NONE*/ MFX_ERR_INVALID_VIDEO_PARAM },
    // IN = OPAQ
    {/*33*/ 0, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 5, MFX_ERR_INVALID_VIDEO_PARAM },
    {/*34*/ 0, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY,  5, MFX_ERR_INVALID_VIDEO_PARAM },
    {/*35*/ 0, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY, 5, MFX_ERR_INVALID_VIDEO_PARAM },

    /* This function does not validate I/O parameters except those used in calculating the number of input and output surfaces.*/
    {/*36*/ wrong_resol, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 5, MFX_ERR_NONE },
    {/*37*/ wrong_resol, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY,  5, MFX_ERR_NONE },
    {/*38*/ wrong_resol, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY, 5, MFX_ERR_INVALID_VIDEO_PARAM },
    {/*39*/ wrong_ARs,   MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 5, MFX_ERR_NONE },
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
    if((tc.IOPattern == (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY)) ||
       (tc.IOPattern == (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY)) )
       tc.sts = MFX_ERR_NONE;
    else
        tc.sts = MFX_ERR_INVALID_VIDEO_PARAM;

    return tc;
}


int test(unsigned int id)
{
    TS_START;
    tsVideoVPP vpp(true, MFX_MAKEFOURCC('C', 'A', 'M', 'R'));
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

    if(par.session && par.pPar)
    {
        if((MFX_FOURCC_A2RGB10  == par.pPar->vpp.In.FourCC) ||
            (MFX_FOURCC_ARGB16  == par.pPar->vpp.In.FourCC) ||
            (MFX_FOURCC_R16     == par.pPar->vpp.In.FourCC))
            par.pPar->vpp.In.BitDepthLuma = 10;
        if((MFX_FOURCC_A2RGB10  == par.pPar->vpp.Out.FourCC) ||
            (MFX_FOURCC_ARGB16  == par.pPar->vpp.Out.FourCC) ||
            (MFX_FOURCC_R16     == par.pPar->vpp.Out.FourCC))
            par.pPar->vpp.Out.BitDepthLuma = 10;
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

        if(0 != tc.AsyncDepth)
        {
            mfxFrameAllocRequest request_for_async_depth_1[2];
            par.pPar->AsyncDepth = 1;
            mfxStatus sts = vpp.QueryIOSurf(par.session, par.pPar, request_for_async_depth_1);

            EXPECT_GE( vpp.m_request[0].NumFrameSuggested, request_for_async_depth_1[0].NumFrameSuggested );
            EXPECT_GE( vpp.m_request[1].NumFrameSuggested, request_for_async_depth_1[1].NumFrameSuggested );
            EXPECT_GE( vpp.m_request[0].NumFrameMin, request_for_async_depth_1[0].NumFrameMin );
            EXPECT_GE( vpp.m_request[1].NumFrameMin, request_for_async_depth_1[1].NumFrameMin );
        }

    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(camera_query_io_surf, test, sizeof(test_case)/sizeof(tc_struct) + IOP_NUM);
}