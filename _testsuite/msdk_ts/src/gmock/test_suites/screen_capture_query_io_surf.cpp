/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"

namespace screen_capture_query_io_surf
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
    mfxU32      FourCC;
    mfxStatus   sts;
} tc_struct;

tc_struct test_case[] =
{
    {/*00*/zero_session, MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 0, MFX_FOURCC_NV12, MFX_ERR_INVALID_HANDLE},
    {/*01*/zero_param,   MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 0, MFX_FOURCC_NV12, MFX_ERR_NULL_PTR      },
    {/*02*/zero_request, MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 0, MFX_FOURCC_NV12, MFX_ERR_NULL_PTR      },
    {/*03*/0,            MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 5, MFX_FOURCC_NV12, MFX_ERR_NONE          },
    {/*04*/0,            MFX_IOPATTERN_OUT_VIDEO_MEMORY,  5, MFX_FOURCC_NV12, MFX_ERR_NONE          },

    {/*05*/zero_session, MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 0, MFX_FOURCC_RGB4, MFX_ERR_INVALID_HANDLE},
    {/*06*/zero_param,   MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 0, MFX_FOURCC_RGB4, MFX_ERR_NULL_PTR      },
    {/*07*/zero_request, MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 0, MFX_FOURCC_RGB4, MFX_ERR_NULL_PTR      },
    {/*08*/0,            MFX_IOPATTERN_OUT_SYSTEM_MEMORY, 5, MFX_FOURCC_RGB4, MFX_ERR_NONE          },
    {/*09*/0,            MFX_IOPATTERN_OUT_VIDEO_MEMORY,  5, MFX_FOURCC_RGB4, MFX_ERR_NONE          },
};

enum
{
    IOP_IN  = (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_IN_OPAQUE_MEMORY),
    IOP_OUT = (MFX_IOPATTERN_OUT_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY),
    IOP_NUM = (1<<(3+3)),
};

tc_struct GetTestCase(unsigned int id)
{
    if (id < sizeof(test_case)/sizeof(tc_struct))
        return test_case[id];

    //all possible combinations of IOPattern
    tc_struct tc = {};
    id -= sizeof(test_case)/sizeof(tc_struct); // exclude pre-defined cases

    // inherit values from base set
    unsigned int base_id = id % (sizeof(test_case)/sizeof(tc_struct));
    tc.AsyncDepth = test_case[base_id].AsyncDepth;
    tc.FourCC = test_case[base_id].FourCC;

    // get IOPattern IN from test-case id
    tc.IOPattern = (id & IOP_IN);

    // get IOPattern OUT from rest of test-case id
    id &= ~IOP_IN;
    while (id && (id & (~IOP_OUT)))
        id <<= 1;
    tc.IOPattern |= id;

    // only one IOPattern OUT is valid
    tc.sts = (!id || (id & (id - 1))) ? MFX_ERR_INVALID_VIDEO_PARAM : MFX_ERR_NONE; 

    return tc;
}

int test(unsigned int id)
{
    TS_START;
    tsVideoDecoder dec(MFX_CODEC_CAPTURE, true, MFX_MAKEFOURCC('C','A','P','T'));
    tc_struct tc = GetTestCase(id);

    dec.MFXInit();

    if(dec.m_uid)
        dec.Load();

    QIOSpar par = {dec.m_session, &dec.m_par, &dec.m_request};
    dec.m_par.IOPattern  = tc.IOPattern;
    dec.m_par.AsyncDepth = 0;
    dec.m_par.mfx.FrameInfo.FourCC = tc.FourCC;
    if (tc.FourCC == MFX_FOURCC_RGB4)
    {
        dec.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
    }

    if (tc.set_par)
    {
        (*tc.set_par)(par);
    }

    g_tsStatus.expect(tc.sts);
    dec.QueryIOSurf(par.session, par.pPar, par.pRequest);

    if (g_tsStatus.get() >= MFX_ERR_NONE)
    {
        mfxU32 expectedType = MFX_MEMTYPE_FROM_DECODE|MFX_MEMTYPE_EXTERNAL_FRAME;

        if(dec.m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
            expectedType |= MFX_MEMTYPE_SYSTEM_MEMORY;
        else
            expectedType |= MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;

        EXPECT_EQ( expectedType, dec.m_request.Type );
        EXPECT_EQ( dec.m_par.mfx.FrameInfo, dec.m_request.Info );
        EXPECT_GT( dec.m_request.NumFrameMin, 0 );
        EXPECT_GE( dec.m_request.NumFrameSuggested, dec.m_request.NumFrameMin );

        if (tc.AsyncDepth)
        {
            mfxFrameAllocRequest& request1 = dec.m_request;
            mfxFrameAllocRequest  request2 = {};
            dec.m_par.AsyncDepth = tc.AsyncDepth;

            dec.QueryIOSurf(dec.m_session, &dec.m_par, &request2);

            EXPECT_EQ( request1.NumFrameMin + dec.m_par.AsyncDepth - 1, request2.NumFrameMin );
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(screen_capture_query_io_surf, test, sizeof(test_case)/sizeof(tc_struct) + IOP_NUM);
}