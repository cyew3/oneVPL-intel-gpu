/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

namespace vpp_mctf_douse
{

    class TestSuite : tsVideoVPP, public tsSurfaceProcessor
    {
    public:
        TestSuite()
            : tsVideoVPP()
        {
            m_surf_in_processor = this;
        }
        ~TestSuite() {}
        int RunTest(unsigned int id);
        static const unsigned int n_cases;

    private:

        tsNoiseFiller m_noise;
        mfxExtVPPDoUse* vpp_du;

        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 alg_num;
            mfxU32 alg_list[6 + 1];
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
        // MCTF
        {/*00*/ MFX_ERR_NONE, 1,
        { MFX_EXTBUFF_VPP_MCTF }
        },

        {/*01*/ MFX_ERR_INVALID_VIDEO_PARAM, 7,
        { MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_DETAIL, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DEINTERLACING, MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO, MFX_EXTBUFF_VPP_MCTF }
        },

        {/*02*/ MFX_ERR_NONE, 6,
        { MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_DETAIL, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DEINTERLACING, MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO, MFX_EXTBUFF_VPP_MCTF }
        },

        {/*03*/ MFX_ERR_NONE, 2,
        { MFX_EXTBUFF_VPP_MCTF, MFX_EXTBUFF_VPP_DENOISE }
        },

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];

        MFXInit();

        m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        m_par.AsyncDepth = 1;

        m_par.AddExtBuffer(MFX_EXTBUFF_VPP_DOUSE, sizeof(mfxExtVPPDoUse));
        vpp_du = (mfxExtVPPDoUse*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DOUSE);
        vpp_du->NumAlg = tc.alg_num;
        vpp_du->AlgList = new mfxU32[tc.alg_num];

        memset(vpp_du->AlgList, 0, sizeof(mfxU32)*vpp_du->NumAlg);

        for (mfxU32 i = 0; i< tc.alg_num; i++)
            vpp_du->AlgList[i] = tc.alg_list[i];

        CreateAllocators();

        SetFrameAllocator();
        SetHandle();

        AllocSurfaces();

        g_tsStatus.expect(tc.sts);
        mfxStatus sts = Init(m_session, m_pPar);

        if(MFX_ERR_NONE == tc.sts)
            g_tsStatus.expect(MFX_ERR_NONE);
        else
            g_tsStatus.expect(MFX_ERR_NOT_INITIALIZED);

        tsExtBufType<mfxVideoParam> par_init;
        mfxExtVPPDoUse* vpp_du2;

        par_init.AddExtBuffer(MFX_EXTBUFF_VPP_DOUSE, sizeof(mfxExtVPPDoUse));
        vpp_du2 = (mfxExtVPPDoUse*)par_init.GetExtBuffer(MFX_EXTBUFF_VPP_DOUSE);
        vpp_du2->NumAlg = tc.alg_num;
        vpp_du2->AlgList = new mfxU32[tc.alg_num];

        memset(vpp_du2->AlgList, 0, sizeof(mfxU32)*vpp_du2->NumAlg);

        GetVideoParam(m_session, &par_init);
        g_tsStatus.expect(MFX_ERR_NONE);

        //if ((vpp_du2->NumAlg != vpp_du->NumAlg) && (MFX_ERR_NONE ==sts))
        //{
        //    g_tsLog << "ERROR: Number of algorithms in specified DoUse buffer is not equal to number of algorithms in buffer returned by GetVideoParam() \n";
        //    g_tsStatus.check(MFX_ERR_ABORTED);
        //}
        //else
        //if ((vpp_du2->NumAlg == vpp_du->NumAlg) && (MFX_ERR_NONE != sts))
        //{
        //    g_tsLog << "ERROR: Number of algorithms in specified DoUse buffer must not be equal to number of algorithms in buffer returned by GetVideoParam() \n";
        //    g_tsStatus.check(MFX_ERR_ABORTED);
        //}


        if ((vpp_du2->NumAlg == vpp_du->NumAlg) && (MFX_ERR_NONE == sts))
        {
            for (mfxU32 i = 0; i < vpp_du->NumAlg; i++)
            {
                sts = MFX_ERR_ABORTED;
                for (mfxU32 j = 0; j < vpp_du2->NumAlg; j++)
                {
                    if (vpp_du->AlgList[i] == vpp_du2->AlgList[j])
                        sts = MFX_ERR_NONE;
                }
                if (sts == MFX_ERR_ABORTED)
                    g_tsLog << "ERROR: List of algorithms in specified DoUse buffer is not equal to list of algorithms in buffer returned by GetVideoParam() \n";
                g_tsStatus.check(sts);
            }
            ProcessFrames(2);
        }

        delete[] vpp_du->AlgList;
        vpp_du->AlgList = 0;
        delete[] vpp_du2->AlgList;
        vpp_du2->AlgList = 0;

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(vpp_mctf_douse);

}
