#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

namespace vpp_fieldcopy
{

class TestSuite : tsVideoVPP, public tsSurfaceProcessor
{
public:
    TestSuite()
        : tsVideoVPP()
        , attach2frame(false)
    {
        m_surf_in_processor = this;

        m_par.AddExtBuffer(MFX_EXTBUFF_VPP_FIELD_PROCESSING, sizeof(mfxExtVPPFieldProcessing));
        m_vpp_fp = (mfxExtVPPFieldProcessing*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_FIELD_PROCESSING);
    }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    bool attach2frame;

    tsNoiseFiller m_noise;
    mfxExtVPPFieldProcessing* m_vpp_fp;
    mfxExtVPPDoUse* vpp_du;

    enum
    {
        MFX_PAR = 1,
        EXT_VPP,
    };

    struct tc_struct
    {
        mfxStatus sts_query;
        mfxStatus sts_init;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        mfxFrameAllocator* pfa = (mfxFrameAllocator*)m_spool_in.GetAllocator();
        m_noise.tsSurfaceProcessor::ProcessSurface(&s, pfa);

        // provide FieldCopy buffer for each RunVPPFrameAsync. Or not.
        if (attach2frame)
        {
            s.Data.ExtParam = m_par.ExtParam;
            s.Data.NumExtParam = m_par.NumExtParam;
        } else
        {
            s.Data.ExtParam = 0;
            s.Data.NumExtParam = 0;
        }

        return MFX_ERR_NONE;
    }
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FRAME},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD}}
    },
    {/*01*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_TOPFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_TOPFIELD}}
    },
    {/*02*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_TOPFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD}}
    },
    {/*03*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_TOPFIELD}}
    },
    {/*04*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_TOPFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_TOPFIELD},
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*05*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_TOPFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_TOPFIELD},
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*06*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FRAME}}
    },
    {/*07*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {   // Mode=Frame, InField=BFF
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FRAME},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD}}
    },
    {/*08*/ MFX_ERR_NONE, MFX_ERR_NONE, 0, {   // Mode is set, but OutField is set to UNKNOWN
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FRAME},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_UNKNOWN}}
    },
    {/*09*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, 0, {   // feature is not supported
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_SWAP_FIELDS}}
    },
    {/*10*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_EXTBUFF_VPP_DENOISE, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD}}
    },
    {/*11*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_EXTBUFF_VPP_DETAIL, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD}}
    },
    {/*12*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD}}
    },
    {/*13*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_EXTBUFF_VPP_IMAGE_STABILIZATION, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD}}
    },
    {/*14*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_EXTBUFF_VPP_PROCAMP, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD}}
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    mfxStatus sts_query = tc.sts_query,
              sts_init  = tc.sts_init;

    if (tc.set_par[0].ext_type == MFX_EXTBUFF_VPP_IMAGE_STABILIZATION)
    {
        sts_query = MFX_ERR_UNSUPPORTED;
        sts_init  = MFX_ERR_INVALID_VIDEO_PARAM;
    }
    if (tc.set_par[0].ext_type == MFX_EXTBUFF_VPP_PICSTRUCT_DETECTION && g_tsOSFamily != MFX_OS_FAMILY_WINDOWS)
    {
        sts_query = MFX_ERR_UNSUPPORTED;
        sts_init  = MFX_ERR_INVALID_VIDEO_PARAM;
    }
    if (g_tsOSFamily == MFX_OS_FAMILY_WINDOWS)
    {
        sts_query = MFX_ERR_UNSUPPORTED;
        sts_init  = MFX_ERR_INVALID_VIDEO_PARAM;
    }

    MFXInit();

    m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    m_par.AsyncDepth = 1;

    SETPARS(m_pPar, MFX_PAR);
    SETPARS(m_vpp_fp, EXT_VPP);

    if (tc.mode != 0)
    {
        m_par.AddExtBuffer(MFX_EXTBUFF_VPP_DOUSE, sizeof(mfxExtVPPDoUse));
        vpp_du = (mfxExtVPPDoUse*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DOUSE);
        vpp_du->NumAlg = 1;
        vpp_du->AlgList = new mfxU32[1];

        memset(vpp_du->AlgList, 0, sizeof(mfxU32)*vpp_du->NumAlg);

        vpp_du->AlgList[0] = tc.mode;

        attach2frame = true;
    }

    CreateAllocators();

    SetFrameAllocator();
    SetHandle();

    AllocSurfaces();

    tsExtBufType<mfxVideoParam> par_out;
    par_out=m_par;

    g_tsStatus.expect(sts_query);
    Query(m_session, m_pPar, &par_out);

    g_tsStatus.expect(sts_init);
    Init(m_session, m_pPar);

    g_tsStatus.expect(MFX_ERR_NONE);

    if (tc.mode != 0 && MFX_ERR_NONE == sts_init)
    {
        tsExtBufType<mfxVideoParam> par2;
        mfxExtVPPDoUse* vpp_du2;

        par2.AddExtBuffer(MFX_EXTBUFF_VPP_DOUSE, sizeof(mfxExtVPPDoUse));
        vpp_du2 = (mfxExtVPPDoUse*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DOUSE);
        vpp_du2->NumAlg = 1;
        vpp_du2->AlgList = new mfxU32[1];

        memset(vpp_du2->AlgList, 0, sizeof(mfxU32)*vpp_du2->NumAlg);

        GetVideoParam(m_session, &par2);
        g_tsStatus.expect(MFX_ERR_NONE);

        if (vpp_du2->NumAlg != vpp_du->NumAlg)
        {
            g_tsLog << "ERROR: Number of algorithms in specified DoUse buffer is not equal to number of algorithms in buffer returned by GetVideoParam() \n";
            g_tsStatus.check(MFX_ERR_ABORTED);
        }
        for (mfxU32 i = 0; i< vpp_du->NumAlg; i++)
        {
            mfxStatus sts = MFX_ERR_ABORTED;
            for (mfxU32 j = 0; j< vpp_du2->NumAlg; j++)
            {
                if (vpp_du->AlgList[i]==vpp_du2->AlgList[j])
                    sts = MFX_ERR_NONE;
            }
            if (sts == MFX_ERR_ABORTED)
                g_tsLog << "ERROR: List of algorithms in specified DoUse buffer is not equal to list of algorithms in buffer returned by GetVideoParam() \n";
            g_tsStatus.check(sts);
        }

        delete[] vpp_du->AlgList;
        vpp_du->AlgList = 0;
        delete[] m_par.ExtParam;
        m_par.ExtParam = 0;
        delete[] vpp_du2->AlgList;
        vpp_du2->AlgList = 0;
        delete[] par2.ExtParam;
        par2.ExtParam = 0;
    }

    if ( MFX_ERR_NONE == sts_init )
        ProcessFrames(10);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_fieldcopy);

}
