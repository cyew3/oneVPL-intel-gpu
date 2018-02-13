/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright (c) 2018 Intel Corporation. All Rights Reserved.

File Name: vpp_mctf_init.cpp

\* ****************************************************************************** */

/*!
\file vpp_mctf_init.cpp
\brief Gmock test vpp_init

Description:
This suite tests VPP - MCTF initialization\n
1 test case for check unsupported on Windows
*/

#include "ts_vpp.h"
#include "ts_struct.h"

//!Namespace of VPP Init test
namespace vpp_mctf_init
{

    //!\brief Main test class
    class TestSuite : protected tsVideoVPP, public tsSurfaceProcessor
    {
    public:
        //! \brief A constructor (VPP)
        TestSuite() : tsVideoVPP(true)
        {
            m_surf_in_processor = this;
            m_par.vpp.In.FourCC = MFX_FOURCC_YUY2;
            m_par.vpp.Out.FourCC = MFX_FOURCC_NV12;
            m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        }
        //! \brief A destructor
        ~TestSuite() { }
        //! \brief Main method. Runs test case
        //! \param id - test case number
        virtual int RunTest(unsigned int id);
        //! The number of test cases
        static const unsigned int n_cases;
    protected:

        enum
        {
            MFX_PAR
        };

        typedef enum
        {
            STANDARD,
            NULL_PTR_SESSION,
            NULL_PTR_PARAM,
            DOUBLE_VPP_INIT,
        } TestMode;

        struct tc_struct
        {
            mfxStatus sts;
            TestMode  mode;

            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU64 v;
            } set_par[MAX_NPARS];
        };

        //! \brief Set of test cases
        static const tc_struct test_case[];

        //! \brief Stream generator
        tsNoiseFiller m_noise;

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            mfxFrameAllocator* pfa = (mfxFrameAllocator*)m_spool_in.GetAllocator();
            m_noise.tsSurfaceProcessor::ProcessSurface(&s, pfa);

            return MFX_ERR_NONE;
        }

        int RunTest(const tc_struct& tc);
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        // mctf related tests
        {/*00*/ MFX_ERR_INVALID_VIDEO_PARAM, STANDARD,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Header, MFX_EXTBUFF_VPP_MCTF },
            },
        }
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        return  RunTest(test_case[id]);
    }

    int TestSuite::RunTest(const TestSuite::tc_struct& tc)
    {
        TS_START;
        mfxStatus sts = tc.sts;
        SETPARS(&m_par, MFX_PAR);

        if (MFX_FOURCC_YV12 == m_par.vpp.In.FourCC && MFX_ERR_NONE == tc.sts
            && MFX_OS_FAMILY_WINDOWS == g_tsOSFamily && g_tsImpl & MFX_IMPL_VIA_D3D11)
            sts = MFX_WRN_PARTIAL_ACCELERATION;
        if (NULL_PTR_SESSION != tc.mode)
        {
            MFXInit();
        }
        if (NULL_PTR_PARAM == tc.mode)
        {
            m_pPar = nullptr;
        }
        if (DOUBLE_VPP_INIT == tc.mode)
        {
            g_tsStatus.expect(tc.sts);
            Init(m_session, m_pPar);
            Close();
        }

        CreateAllocators();

        SetFrameAllocator();
        SetHandle();

        g_tsStatus.expect(sts);
        Init(m_session, m_pPar);

        if (sts == MFX_ERR_NONE)
        {
            AllocSurfaces();
            ProcessFrames(g_tsConfig.sim ? 3 : 10);
        }

        TS_END;
        return 0;
    }

    //!\brief Reg the test suite into test system
    TS_REG_TEST_SUITE_CLASS(vpp_mctf_init);
}
