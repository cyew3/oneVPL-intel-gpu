#include "ts_encoder.h"

namespace
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
    {// 0
        zero_session, 
        MFX_IOPATTERN_IN_SYSTEM_MEMORY, 
        0, 
        MFX_ERR_INVALID_HANDLE
    },
    {// 1
        zero_param, 
        MFX_IOPATTERN_IN_SYSTEM_MEMORY, 
        0, 
        MFX_ERR_NULL_PTR
    },
    {// 2
        zero_request, 
        MFX_IOPATTERN_IN_SYSTEM_MEMORY, 
        0, 
        MFX_ERR_NULL_PTR
    },
    {// 3
        0, 
        MFX_IOPATTERN_IN_SYSTEM_MEMORY, 
        5, 
        MFX_ERR_NONE
    },
    {// 4
        0, 
        MFX_IOPATTERN_IN_VIDEO_MEMORY, 
        5, 
        MFX_ERR_NONE
    },
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

    // only one IOPattern IN is valid
    tc.sts = (!tc.IOPattern || (tc.IOPattern & (tc.IOPattern - 1))) ? MFX_ERR_INVALID_VIDEO_PARAM : MFX_ERR_NONE; 

    // get IOPattern OUT from rest of test-case id
    id &= ~IOP_IN;
    while(id && (id & (~IOP_OUT)))
        id <<= 1;
    tc.IOPattern |= id;

    // only one OUT is valid either, but encoder shouldn't check this

    return tc;
}


int test(unsigned int id)
{
    TS_START;
    tsVideoEncoder enc(MFX_CODEC_VP8);
    tc_struct tc = GetTestCase(id);

    enc.MFXInit();

    QIOSpar par = {enc.m_session, &enc.m_par, &enc.m_request};
    enc.m_par.IOPattern  = tc.IOPattern;
    enc.m_par.AsyncDepth = 1;

    if(tc.set_par)
    {
        (*tc.set_par)(par);
    }

    g_tsStatus.expect(tc.sts);
    enc.QueryIOSurf(par.session, par.pPar, par.pRequest);

    if(g_tsStatus.get() >= MFX_ERR_NONE)
    {
        mfxU32 expectedType = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME;

        if(enc.m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
            expectedType |= MFX_MEMTYPE_SYSTEM_MEMORY;
        else 
            expectedType |= MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;

        EXPECT_EQ( expectedType, enc.m_request.Type );
        EXPECT_EQ( enc.m_par.mfx.FrameInfo, enc.m_request.Info );
        EXPECT_GT( enc.m_request.NumFrameMin, 0 );
        EXPECT_GE( enc.m_request.NumFrameSuggested, enc.m_request.NumFrameMin );

        if(tc.AsyncDepth)
        {
            mfxFrameAllocRequest& request1 = enc.m_request;
            mfxFrameAllocRequest  request2 = {};
            enc.m_par.AsyncDepth = tc.AsyncDepth;

            enc.QueryIOSurf(enc.m_session, &enc.m_par, &request2);

            EXPECT_EQ( request1.NumFrameMin + enc.m_par.AsyncDepth - 1, request2.NumFrameMin );
        }

    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(vp8e_query_io_surf, test, sizeof(test_case)/sizeof(tc_struct) + IOP_NUM);

};