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

        memset(&m_vpp_fp, 0, sizeof(mfxExtVPPFieldProcessing));
        m_vpp_fp.Header.BufferId = MFX_EXTBUFF_VPP_FIELD_PROCESSING;
        m_vpp_fp.Header.BufferSz = sizeof(mfxExtVPPFieldProcessing);
    }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    bool attach2frame;

    tsNoiseFiller m_noise;
    mfxExtVPPFieldProcessing m_vpp_fp;
    mfxExtVPPDoUse vpp_du;

    enum
    {
        DO_USE = 1,
    };

    enum
    {
        MFX_PAR = 1,
        EXT_VPP,
        EXT_VPP_DENOISE,
        EXT_VPP_DETAIL,
        EXT_VPP_FRC,
        EXT_VPP_IS,
        EXT_VPP_PD,
        EXT_VPP_PROCAMP
    };

    struct tc_struct
    {
        mfxStatus sts;
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
    {/*00*/ MFX_ERR_NONE, 0, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FRAME},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD}}
    },
    {/*01*/ MFX_ERR_NONE, 0, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_TOPFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_TOPFIELD}}
    },
    {/*02*/ MFX_ERR_NONE, 0, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_TOPFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD}}
    },
    {/*03*/ MFX_ERR_NONE, 0, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_TOPFIELD}}
    },
    {/*04*/ MFX_ERR_NONE, 0, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_TOPFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_TOPFIELD},
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*05*/ MFX_ERR_NONE, 0, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_TOPFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_TOPFIELD},
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}}
    },
    {/*06*/ MFX_ERR_NONE, 0, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FRAME}}
    },
    {/*07*/ MFX_ERR_NONE, 0, {   // Mode=Frame, InField=BFF
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FRAME},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD}}
    },
    {/*08*/ MFX_ERR_NONE, 0, {   // Mode is set, but OutField is set to UNKNOWN
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FRAME},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_UNKNOWN}}
    },
    {/*09*/ MFX_ERR_UNSUPPORTED, 0, {   // feature is not supported
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_SWAP_FIELDS}}
    },
    {/*10*/ MFX_ERR_NONE, DO_USE, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_TOPFIELD}}
    },
    {/*11*/ MFX_ERR_NONE, DO_USE, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_TOPFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD}}
    },
    {/*12*/ MFX_ERR_NONE, DO_USE, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_TOPFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_TOPFIELD}}
    },
    {/*13*/ MFX_ERR_NONE, DO_USE, {
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {EXT_VPP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD}}
    },
    {/*14*/ MFX_ERR_NONE, DO_USE, {
        {EXT_VPP_DENOISE, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP_DENOISE, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {EXT_VPP_DENOISE, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD}}
    },
    {/*15*/ MFX_ERR_NONE, DO_USE, {
        {EXT_VPP_DETAIL, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP_DETAIL, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {EXT_VPP_DETAIL, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD}}
    },
    {/*16*/ MFX_ERR_NONE, DO_USE, {
        {EXT_VPP_FRC, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP_FRC, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {EXT_VPP_FRC, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD}}
    },
    {/*17*/ MFX_ERR_NONE, DO_USE, {
        {EXT_VPP_IS, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP_IS, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {EXT_VPP_IS, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD}}
    },
    {/*18*/ MFX_ERR_NONE, DO_USE, {
        {EXT_VPP_PD, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP_PD, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {EXT_VPP_PD, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD}}
    },
    {/*19*/ MFX_ERR_NONE, DO_USE, {
        {EXT_VPP_PROCAMP, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
        {EXT_VPP_PROCAMP, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_BOTTOMFIELD},
        {EXT_VPP_PROCAMP, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_BOTTOMFIELD}}
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    m_par.AsyncDepth = 1;

    SETPARS(m_pPar, MFX_PAR);
    SETPARS(&m_vpp_fp, EXT_VPP);

    if (tc.mode == DO_USE)
    {
        vpp_du.Header.BufferId = MFX_EXTBUFF_VPP_DOUSE;
        vpp_du.Header.BufferSz = sizeof(mfxExtVPPDoUse);

        if (tc.set_par[0].ext_type == EXT_VPP)
        {
            vpp_du.NumAlg = 1;
            vpp_du.AlgList = new mfxU32[1];
        }
        else
        {
            vpp_du.NumAlg = 2;
            vpp_du.AlgList = new mfxU32[2];
        }

        SETPARS(&m_vpp_fp, EXT_VPP);
        vpp_du.AlgList[0] = MFX_EXTBUFF_VPP_FIELD_PROCESSING;

        if (tc.set_par[0].ext_type == EXT_VPP_DENOISE)
            vpp_du.AlgList[1] = MFX_EXTBUFF_VPP_DENOISE;
        else if (tc.set_par[0].ext_type == EXT_VPP_DETAIL)
            vpp_du.AlgList[1] = MFX_EXTBUFF_VPP_DETAIL;
        else if (tc.set_par[0].ext_type == EXT_VPP_FRC)
            vpp_du.AlgList[1] = MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION;
        else if (tc.set_par[0].ext_type == EXT_VPP_IS)
            vpp_du.AlgList[1] = MFX_EXTBUFF_VPP_IMAGE_STABILIZATION;
        else if (tc.set_par[0].ext_type == EXT_VPP_PD)
            vpp_du.AlgList[1] = MFX_EXTBUFF_VPP_PICSTRUCT_DETECTION;
        else if (tc.set_par[0].ext_type == EXT_VPP_PROCAMP)
            vpp_du.AlgList[1] = MFX_EXTBUFF_VPP_PROCAMP;

        attach2frame = true;

        m_par.ExtParam = new mfxExtBuffer*[1];
        m_par.ExtParam[0] = (mfxExtBuffer*)&vpp_du;
        m_par.NumExtParam = 1;
    } else
    {
        m_par.ExtParam = new mfxExtBuffer*[1];
        m_par.ExtParam[0] = (mfxExtBuffer*)&m_vpp_fp;
        m_par.NumExtParam = 1;
    }

    CreateAllocators();

    SetFrameAllocator();
    SetHandle();

    AllocSurfaces();

    g_tsStatus.expect(tc.sts);

    if (tc.set_par[0].ext_type == EXT_VPP_IS)
        g_tsStatus.expect(MFX_WRN_FILTER_SKIPPED);
    if (tc.set_par[0].ext_type == EXT_VPP_PD && g_tsOSFamily != MFX_OS_FAMILY_WINDOWS)
        g_tsStatus.expect(MFX_WRN_FILTER_SKIPPED);

    mfxStatus sts = Init(m_session, m_pPar);

    if (tc.mode == DO_USE)
    {
        mfxVideoParam par2 = {0};
        mfxExtVPPDoUse vpp_du2;
        vpp_du2.Header.BufferId = MFX_EXTBUFF_VPP_DOUSE;
        vpp_du2.Header.BufferSz = sizeof(mfxExtVPPDoUse);

       if (tc.set_par[0].ext_type == EXT_VPP)
       {
           vpp_du2.NumAlg = 1;
           vpp_du2.AlgList = new mfxU32[1];
       }
       else
       {
           vpp_du2.NumAlg = 2;
           vpp_du2.AlgList = new mfxU32[2];
       }

        par2.NumExtParam = 1;
        par2.ExtParam = new mfxExtBuffer*[1];
        par2.ExtParam[0] = (mfxExtBuffer*)&vpp_du2;

        MFXVideoVPP_GetVideoParam(m_session, &par2);

        if (vpp_du2.NumAlg != vpp_du.NumAlg)
        {
            g_tsStatus.check(MFX_ERR_ABORTED);
            g_tsLog << "ERROR: Number of algorithms in specified DoUse buffer is not equal to number of algorithms in buffer returned by GetVideoParam() \n";
        }
        for (mfxU32 i = 0; i< vpp_du.NumAlg; i++)
        {
            sts = MFX_ERR_ABORTED;
            for (mfxU32 j = 0; j< vpp_du2.NumAlg; j++)
            {
                if (vpp_du.AlgList[i]==vpp_du2.AlgList[j])
                    sts = MFX_ERR_NONE;
            }
            if (sts == MFX_ERR_ABORTED)
                g_tsLog << "ERROR: List of algorithms in specified DoUse buffer is not equal to list of algorithms in buffer returned by GetVideoParam() \n";
            g_tsStatus.check(sts);
        }
    }

    /*if ((sts != tc.sts) && (sts = MFX_WRN_FILTER_SKIPPED))
    {
        g_tsLog << "ERROR: Init() returned MFX_WRN_FILTER_SKIPPED\n";
        g_tsStatus.check(MFX_ERR_ABORTED);
    }*/

    ///////////////////////////////////////////////////////////////////////////
    //g_tsStatus.expect(tc.sts);

    if ( sts == MFX_ERR_NONE )
        ProcessFrames(10);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_fieldcopy);

}
