/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2016 Intel Corporation. All Rights Reserved.
//
//
*/

#include "ts_vpp.h"
#include "ts_struct.h"

//A special test that checks current release limitations

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace camera_unsupport
{

typedef void (*callback)(tsExtBufType<mfxVideoParam>&, mfxU32, mfxU32);

void ext_buf(tsExtBufType<mfxVideoParam>& par, mfxU32 id, mfxU32 size)
{
    par.AddExtBuffer(id, size); 
}
void del_buf(tsExtBufType<mfxVideoParam>& par, mfxU32 id, mfxU32 /*size*/)
{
    par.RemoveExtBuffer(id); 
}

class TestSuite : tsVideoVPP
{
public:
    TestSuite() : tsVideoVPP(true, MFX_MAKEFOURCC('C','A','M','R')) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 n_par = 8;

    enum ALLOC_CTRL
    {
        SET_ALLOCATOR     =  0x0100,
        ALLOC_OPAQUE      =  0x0200,
    };

    enum STAGE
    {
        QUERY       = 0,
        QUERYIOSURF = 1,
        INIT        = 2,
        RESET       = 3,
    };

    struct tc_struct
    {
        mfxI32 exp_sts[4];
        mfxU32 alloc_ctrl;
        struct f_pair 
        {
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[n_par];
        struct 
        {
            callback func;
            mfxU32 buf;
            mfxU32 size;
        } set_buf;
    };

    static const tc_struct test_case[];
};

enum SHORT_STS
{
    UNSUP   = MFX_ERR_UNSUPPORTED,
    INVALID = MFX_ERR_INVALID_VIDEO_PARAM,
    ERR_INI = MFX_ERR_NOT_INITIALIZED,
    ERR_INC = MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
    OK      = MFX_ERR_NONE,
};

const TestSuite::tc_struct TestSuite::test_case[] = 
{
           //Query,  QueryIO,  Init,    Reset
    {/*00*/ { UNSUP, INVALID, INVALID, ERR_INI }, SET_ALLOCATOR, { &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY } },
    {/*01*/ { UNSUP, INVALID, INVALID, ERR_INI }, SET_ALLOCATOR, { &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY } },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();
    Load();

    m_spool_in.UseDefaultAllocator(!!(m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY));
    SetFrameAllocator(m_session, m_spool_in.GetAllocator());
    m_spool_out.SetAllocator(m_spool_in.GetAllocator(), true);

    if (tc.alloc_ctrl & ALLOC_OPAQUE)
    {
        m_par.vpp.In.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
        mfxExtOpaqueSurfaceAlloc& osa = m_par;
        QueryIOSurf();
        if (m_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
            m_spool_in.AllocOpaque(m_request[0], osa);
        if (m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
            m_spool_out.AllocOpaque(m_request[1], osa);
    }

    tsExtBufType<mfxVideoParam> par_out;
    mfxExtCamPipeControl& cam_ctrl = par_out;
    m_pParOut = &par_out;

    for(mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par[i].f)
        {
            tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
            std::cout << "  Set field " << tc.set_par[i].f->name << " to " << tc.set_par[i].v << "\n";
        }
    }
    if(tc.set_buf.func) 
    {
        (*tc.set_buf.func)(m_par, tc.set_buf.buf, tc.set_buf.size);
        (*tc.set_buf.func)(par_out, tc.set_buf.buf, tc.set_buf.size);
    }

    mfxVideoParam par_copy;
    memcpy(&par_copy, m_pPar, sizeof(mfxVideoParam));

    //Query mode 2, Different (in,out)
    g_tsStatus.expect((mfxStatus) tc.exp_sts[QUERY]);
    Query(m_session, m_pPar, m_pParOut);

    for(mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par[i].f)
        {
            if(MFX_ERR_UNSUPPORTED == tc.exp_sts[QUERY])
                tsStruct::check_eq(&par_out, *tc.set_par[i].f, 0);
            EXPECT_EQ(0, memcmp(m_pPar, &par_copy,sizeof(mfxVideoParam))) << "Failed.: Input structure must not be changed";
        }
    }

    //Query mode 2, inplace (in,in)
    g_tsStatus.expect((mfxStatus) tc.exp_sts[QUERY]);
    Query(m_session, m_pPar, m_pPar);

    for(mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par[i].f)
        {
            if(MFX_ERR_UNSUPPORTED == tc.exp_sts[QUERY])
                tsStruct::check_eq(m_pPar, *tc.set_par[i].f, 0);
        }
    }

    memcpy(m_pPar, &par_copy, sizeof(mfxVideoParam));
    g_tsStatus.expect((mfxStatus) tc.exp_sts[QUERYIOSURF]);
    QueryIOSurf(m_session, m_pPar, m_pRequest);
    EXPECT_EQ(0, memcmp(m_pPar, &par_copy,sizeof(mfxVideoParam))) << "Failed.: Input structure must not be changed";

    g_tsStatus.expect((mfxStatus) tc.exp_sts[INIT]);
    Init(m_session, m_pPar);
    EXPECT_EQ(0, memcmp(m_pPar, &par_copy,sizeof(mfxVideoParam))) << "Failed.: Input structure must not be changed";

    g_tsStatus.expect((mfxStatus)tc.exp_sts[RESET]);
    Reset();

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(camera_unsupport);

}