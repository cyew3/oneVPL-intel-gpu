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
#include "mfxstructures.h"

namespace vpp_reset
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
    {/*00*/ MFX_ERR_NONE,
        {
            {INIT, &tsStruct::mfxExtVPPDenoise.DenoiseFactor,   50},
            {INIT, &tsStruct::mfxExtVPPDetail.DetailFactor,     40},
            {INIT, &tsStruct::mfxExtVPPProcAmp.Brightness,      30},
            {INIT, &tsStruct::mfxExtVPPDeinterlacing.Mode,      MFX_DEINTERLACING_BOB},

            {RESET, &tsStruct::mfxExtVPPDetail.DetailFactor,    80},
            {RESET, &tsStruct::mfxExtVPPDenoise.DenoiseFactor,  60},
        },
        {}
    },
    {/*01*/ MFX_ERR_NONE,
        {
            {INIT, &tsStruct::mfxExtVPPDenoise.DenoiseFactor,   50},
            {INIT, &tsStruct::mfxExtVPPDetail.DetailFactor,     40},
            {INIT, &tsStruct::mfxExtVPPProcAmp.Brightness,      30},
            {INIT, &tsStruct::mfxExtVPPDeinterlacing.Mode,      MFX_DEINTERLACING_BOB},
        },
        {3, {MFX_EXTBUFF_VPP_DETAIL, MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DENOISE}}
    },
    {/*02*/ MFX_ERR_NONE,
        {
            {INIT, &tsStruct::mfxExtVPPDenoise.DenoiseFactor,   50},
            {INIT, &tsStruct::mfxExtVPPDetail.DetailFactor,     40},
            {INIT, &tsStruct::mfxExtVPPProcAmp.Brightness,      30},
            {INIT, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_BOB},

            {RESET, &tsStruct::mfxExtVPPDetail.DetailFactor,    80},
            {RESET, &tsStruct::mfxExtVPPDenoise.DenoiseFactor,  60},
        },
        {1, {MFX_EXTBUFF_VPP_PROCAMP}}
    },
    {/*03*/ MFX_ERR_NONE,
        {
            {RESET, &tsStruct::mfxExtVPPDenoise.DenoiseFactor,  50},
            {RESET, &tsStruct::mfxExtVPPDetail.DetailFactor,    40},
            {RESET, &tsStruct::mfxExtVPPProcAmp.Brightness,     30},
            {RESET, &tsStruct::mfxExtVPPDeinterlacing.Mode,     MFX_DEINTERLACING_BOB},
        }
    },
    {/*04*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Height,     496},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Width,      736},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Height,    496},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Width,     736},
        },
        {}
    },
    {/*05*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Height,     496},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Width,      736},
        },
        {}
    },
    {/*06*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Height,    496},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Width,     736},
        },
        {}
    },
    {/*07*/ MFX_ERR_NONE,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Height,     464},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Width,      704},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Height,    464},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Width,     704},
        },
        {}
    },
    {/*08*/ MFX_ERR_NONE,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Height,     464},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Width,      704},
        },
        {}
    },
    {/*09*/ MFX_ERR_NONE,
        {
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Height,    464},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Width,     704},
        },
        {}
    },
#if !defined(_WIN32)
    {/*10*/ MFX_ERR_NONE,
        {
            {INIT,  &tsStruct::mfxExtVPPFieldProcessing.Mode,   MFX_VPP_COPY_FRAME},

            {RESET, &tsStruct::mfxExtVPPFieldProcessing.Mode,   MFX_VPP_COPY_FIELD},
        },
    },
#endif
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

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

void CreateEmptyBuffers(tsExtBufType<mfxVideoParam>& par, tsExtBufType<mfxVideoParam>& pattern)
{
    for (mfxU32 i = 0; i < pattern.NumExtParam; i++)
    {
        if (pattern.ExtParam[i]->BufferId != MFX_EXTBUFF_VPP_DONOTUSE)
            par.AddExtBuffer(pattern.ExtParam[i]->BufferId, pattern.ExtParam[i]->BufferSz);
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

    SETPARS(&m_par, INIT);

    CreateAllocators();
    SetFrameAllocator();
    SetHandle();
    AllocSurfaces();

    g_tsStatus.expect(MFX_ERR_NONE);
    Init(m_session, &m_par);

    tsExtBufType<mfxVideoParam> par_init (def);
    CreateEmptyBuffers(*&par_init, *&m_par);

    g_tsStatus.expect(MFX_ERR_NONE);
    GetVideoParam(m_session, &par_init);

    EXPECT_EQ(m_par, par_init) << "ERROR: Init parameters and parameters from GetVideoParams are not equal\n";

    tsExtBufType<mfxVideoParam> par_reset (def);
    SETPARS(&par_reset, RESET);
    SetParamsDoNotUse(*&par_reset, tc.dnu_struct);

    g_tsStatus.expect(tc.sts);
    Reset(m_session, &par_reset);

    tsExtBufType<mfxVideoParam> par_after_reset (def);
    CreateEmptyBuffers(*&par_after_reset, *&par_init);
    CreateEmptyBuffers(*&par_after_reset, *&par_reset);

    g_tsStatus.expect(MFX_ERR_NONE);
    GetVideoParam(m_session, &par_after_reset);

    mfxExtVPPDoNotUse* dnu = (mfxExtVPPDoNotUse*)par_reset.GetExtBuffer(MFX_EXTBUFF_VPP_DONOTUSE);

    for (mfxU32 i = 0; i< par_init.NumExtParam; i++)
    {
        mfxExtBuffer* reset = 0;
        mfxExtBuffer* after_reset = 0;

        reset       = par_reset.GetExtBuffer(      par_init.ExtParam[i]->BufferId);
        after_reset = par_after_reset.GetExtBuffer(par_init.ExtParam[i]->BufferId);

        // Creating empty buffer
        def.AddExtBuffer(par_init.ExtParam[i]->BufferId, par_init.ExtParam[i]->BufferSz);
        mfxExtBuffer* empty = def.GetExtBuffer(par_init.ExtParam[i]->BufferId);

        bool in_dnu = false;
        if (dnu != 0)
        {
            for (mfxU32 j = 0; j < dnu->NumAlg; j++)
            {
                if (par_init.ExtParam[i]->BufferId == dnu->AlgList[j])
                {
                    in_dnu = true;
                    break;
                }
            }
        }

        EXPECT_FALSE(in_dnu && (0 != memcmp(empty, after_reset, std::max(empty->BufferSz, after_reset->BufferSz))))
            << "ERROR: Filter from Init was disabled in Reset but exists after Reset \n";

        EXPECT_FALSE(reset != 0 && (0 != memcmp(reset, after_reset, std::max(reset->BufferSz, after_reset->BufferSz))) )
            << "ERROR: Filter from Init was changed in Reset but parameters from Reset and after Reset are not equal \n";

        EXPECT_FALSE(!in_dnu && reset == 0 && (0 != memcmp(par_init.ExtParam[i], after_reset, std::max(par_init.ExtParam[i]->BufferSz, after_reset->BufferSz))))
            << "ERROR: Filter's parameters from Init and after Reset are not equal \n";
    }

    for (mfxU32 i = 0; i< par_reset.NumExtParam; i++)
    {
        if (par_reset.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_DONOTUSE) continue; // VPP should not configure mfxExtVPPDoNotUse buffer!

        mfxExtBuffer* after_reset = 0;
        after_reset = par_after_reset.GetExtBuffer(par_reset.ExtParam[i]->BufferId);
 
        EXPECT_FALSE (after_reset == 0) << "ERROR: Filter specified in Reset does not exists after Reset \n";

        EXPECT_FALSE(0 != memcmp(par_reset.ExtParam[i], after_reset, std::max(par_reset.ExtParam[i]->BufferSz, after_reset->BufferSz)))
            << "ERROR: Filter's parameters from Reset and after Reset are not equal \n";
    }

    if (dnu != 0)
    {
        delete[] dnu->AlgList;
        dnu->AlgList = 0;

    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_reset);

}
