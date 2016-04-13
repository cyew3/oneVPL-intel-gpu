/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

namespace vpp_donotuse
{

class TestSuite : tsVideoVPP
{
public:
    TestSuite()
        : tsVideoVPP() {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    enum
    {
        MFX_PAR = 1
    };

    enum
    {
        INIT  = 1,
        RESET = 2,
    };

    struct do_not_use
    {
        mfxU32 alg_num;
        mfxU32 alg_list[6];
    };

private:

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxF32 v;
        } set_par[MAX_NPARS];
        do_not_use dnu_struct;
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, INIT, {},
        {2, {MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_DETAIL}}
    },
    {/*01*/ MFX_ERR_NONE, INIT, {},
        {2, {MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION}}
    },
    {/*02*/ MFX_ERR_NONE, INIT, {},
        {2, {MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_PROCAMP}}
    },
    {/*03*/ MFX_ERR_NONE, INIT, {},
        {2, {MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_DEINTERLACING}}
    },
    {/*04*/ MFX_ERR_NONE, INIT, {},
        {2, {MFX_EXTBUFF_VPP_DETAIL, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION}}
    },
    {/*05*/ MFX_ERR_NONE, INIT, {},
        {2, {MFX_EXTBUFF_VPP_DETAIL, MFX_EXTBUFF_VPP_PROCAMP}}
    },
    {/*06*/ MFX_ERR_NONE, INIT, {},
        {2, {MFX_EXTBUFF_VPP_DETAIL, MFX_EXTBUFF_VPP_DEINTERLACING}}
    },
    {/*07*/ MFX_ERR_NONE, INIT, {},
        {2, {MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION, MFX_EXTBUFF_VPP_PROCAMP}}
    },
    {/*08*/ MFX_ERR_NONE, INIT, {},
        {2, {MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION, MFX_EXTBUFF_VPP_DEINTERLACING}}
    },
    {/*09*/ MFX_ERR_NONE, INIT, {},
        {2, {MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DEINTERLACING}}
    },
    {/*10*/ MFX_ERR_NONE, INIT, {},
        {6, {MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_DETAIL, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DEINTERLACING, MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO}}
    },
    {/*11*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFX_PAR,  &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 50},
        {1, {MFX_EXTBUFF_VPP_DENOISE}}
    },
    {/*12*/ MFX_ERR_NONE, RESET, {MFX_PAR,  &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 50},
        {1, {MFX_EXTBUFF_VPP_DENOISE}}
    },
    {/*13*/ MFX_ERR_NONE, RESET,
        {
            {MFX_PAR,  &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 50},
            {MFX_PAR,  &tsStruct::mfxExtVPPDetail.DetailFactor, 50},
        },
        {1, {MFX_EXTBUFF_VPP_DENOISE}}
    }
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

void CreateEmptyBuffers(tsExtBufType<mfxVideoParam>& par, tsExtBufType<mfxVideoParam>& pattern)
{
    for (mfxU32 i = 0; i < pattern.NumExtParam; i++)
    {
        if (pattern.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_DONOTUSE)
        {
            par.AddExtBuffer(MFX_EXTBUFF_VPP_DONOTUSE, sizeof(mfxExtVPPDoNotUse));
            mfxExtVPPDoNotUse* vpp_dnu = (mfxExtVPPDoNotUse*)par.GetExtBuffer(MFX_EXTBUFF_VPP_DONOTUSE);
            vpp_dnu->NumAlg = 8;
            vpp_dnu->AlgList = new mfxU32[8];

            memset(vpp_dnu->AlgList, 0, sizeof(mfxU32)*vpp_dnu->NumAlg);
        }
        else
            par.AddExtBuffer(pattern.ExtParam[i]->BufferId, pattern.ExtParam[i]->BufferSz);
    }
}

void SetParamsDoNotUse(tsExtBufType<mfxVideoParam>& par, const TestSuite::do_not_use dnu)
{
    if(dnu.alg_num != 0)
    {
        mfxExtVPPDoNotUse* vpp_dnu;
        par.AddExtBuffer(MFX_EXTBUFF_VPP_DONOTUSE, sizeof(mfxExtVPPDoNotUse));
        vpp_dnu = (mfxExtVPPDoNotUse*)par.GetExtBuffer(MFX_EXTBUFF_VPP_DONOTUSE);
        vpp_dnu->NumAlg = dnu.alg_num;
        vpp_dnu->AlgList = new mfxU32[dnu.alg_num];

        memset(vpp_dnu->AlgList, 0, sizeof(mfxU32)*vpp_dnu->NumAlg);

        for(mfxU32 i = 0; i < dnu.alg_num; i++)
            vpp_dnu->AlgList[i] = dnu.alg_list[i];
    }
}
int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    m_par.AsyncDepth = 1;

    tsExtBufType<mfxVideoParam> def (m_par);

    SETPARS(&m_par, MFX_PAR);

    if (tc.mode == INIT)
    {
        SetParamsDoNotUse(*&m_par, tc.dnu_struct);

        g_tsStatus.expect(tc.sts);
        mfxStatus sts = Init(m_session, m_pPar);
        g_tsStatus.expect(MFX_ERR_NONE);
    }
    else if (tc.mode == RESET)
    {
        g_tsStatus.expect(tc.sts);
        mfxStatus sts = Init(m_session, m_pPar);
        g_tsStatus.expect(MFX_ERR_NONE);

        tsExtBufType<mfxVideoParam> par_reset(def);
        SetParamsDoNotUse(*&par_reset, tc.dnu_struct);

        Reset(m_session, &par_reset);

        tsExtBufType<mfxVideoParam> par_after_reset(def);
        CreateEmptyBuffers(*&par_after_reset, *&m_par);
        CreateEmptyBuffers(*&par_after_reset, *&par_reset);

        g_tsStatus.expect(MFX_ERR_NONE);
        GetVideoParam(m_session, &par_after_reset);

        mfxExtVPPDoNotUse* dnu = (mfxExtVPPDoNotUse*)par_reset.GetExtBuffer(MFX_EXTBUFF_VPP_DONOTUSE);

        for (mfxU32 i = 0; i< m_par.NumExtParam; i++)
        {
            mfxExtBuffer* after_reset = 0;
            after_reset = par_after_reset.GetExtBuffer(m_par.ExtParam[i]->BufferId);

            // Creating empty buffer
            def.AddExtBuffer(m_par.ExtParam[i]->BufferId, m_par.ExtParam[i]->BufferSz);
            mfxExtBuffer* empty = def.GetExtBuffer(m_par.ExtParam[i]->BufferId);

            bool in_dnu=false;

            for (mfxU32 j = 0; j < dnu->NumAlg; j++)
            {
                if (m_par.ExtParam[i]->BufferId == dnu->AlgList[j])
                {
                    in_dnu = true;
                }
            }

            EXPECT_FALSE(in_dnu && (0 != memcmp(empty, after_reset, std::max(empty->BufferSz, after_reset->BufferSz))))
                << "ERROR: Filter from Init was disabled in Reset but exists after Reset \n";

            EXPECT_FALSE(!in_dnu && (0 != memcmp(m_par.ExtParam[i], after_reset, std::max(m_par.ExtParam[i]->BufferSz, after_reset->BufferSz))))
                << "ERROR: Filter's parameters from Init and after Reset are not equal \n";
        }

        delete[] dnu->AlgList;
        dnu->AlgList = 0;
    }
    else
    {
        EXPECT_FALSE(1) << "ERROR: Unsupported test mode \n";
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_donotuse);

}
