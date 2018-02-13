/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2016-2018 Intel Corporation. All Rights Reserved.
//
//
*/

#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

namespace vpp_mctf_reset
{

    class TestSuite : protected tsVideoVPP, public tsSurfaceProcessor
    {
    public:
        TestSuite()
            : tsVideoVPP()
        {
            m_surf_in_processor = this;
            m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
            m_par.AsyncDepth = 1;
        }
        ~TestSuite() {}
        int RunTest(unsigned int id);
        static const unsigned int n_cases;

        struct do_not_use
        {
            mfxU32 alg_num;
            mfxU32 alg_list[6];
        };

        enum
        {
            RESET = 1,
            INIT,
        };

    private:

        tsNoiseFiller m_noise;

        struct tc_struct
        {
            mfxStatus sts;
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
            do_not_use dnu_struct;
            mfxU32 platforms_n;
            // suppose 19 platforms can be mentioned
            HWType platform[19];
        };

        static const tc_struct test_case[];

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            mfxFrameAllocator* pfa = (mfxFrameAllocator*)m_spool_in.GetAllocator();
            m_noise.tsSurfaceProcessor::ProcessSurface(&s, pfa);

            return MFX_ERR_NONE;
        }

    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_INVALID_VIDEO_PARAM,
        {
            { RESET, &tsStruct::mfxExtVppMctf.FilterStrength,       0 },
        },
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    
    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];

        tsExtBufType<mfxVideoParam> def(m_par);

        SETPARS(&m_par, INIT);

        MFXInit();

        CreateAllocators();
        SetFrameAllocator();
        SetHandle();

        mfxExtOpaqueSurfaceAlloc* pOSA = (mfxExtOpaqueSurfaceAlloc*)m_par.GetExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

        if (m_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY || m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
        {
            QueryIOSurf();

            m_request[0].NumFrameMin = m_request[0].NumFrameSuggested = pOSA->In.NumSurface;
            m_request[1].NumFrameMin = m_request[1].NumFrameSuggested = pOSA->Out.NumSurface;
        }

        if (m_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
            m_pSurfPoolIn->AllocOpaque(m_request[0], *pOSA);
        if (m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
            m_pSurfPoolOut->AllocOpaque(m_request[1], *pOSA);

        g_tsStatus.expect(MFX_ERR_NONE);
        Init(m_session, &m_par);

 
        SETPARS(&m_par, RESET);
        g_tsStatus.expect(tc.sts);
        Reset(m_session, &m_par);


        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(vpp_mctf_reset);
}