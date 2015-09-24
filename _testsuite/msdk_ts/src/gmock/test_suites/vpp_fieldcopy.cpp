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

    enum
    {
        DO_USE = 1,
    };

    enum
    {
        MFX_PAR = 1,
        EXT_VPP
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
    }
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    m_par.AsyncDepth = 1;

    SETPARS(&m_vpp_fp, EXT_VPP);
    SETPARS(m_pPar, MFX_PAR);

    if (tc.mode == DO_USE)
    {
        mfxExtVPPDoUse vpp_du = {0};
        vpp_du.NumAlg = 1;
        vpp_du.AlgList = new mfxU32[1];
        vpp_du.AlgList[0] = MFX_EXTBUFF_VPP_FIELD_PROCESSING;

        attach2frame = true;
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
    mfxStatus sts = Init(m_session, m_pPar);
    /*if ((sts != tc.sts) && (sts = MFX_WRN_FILTER_SKIPPED))
    {
        g_tsLog << "ERROR: Init() returned MFX_WRN_FILTER_SKIPPED\n";
        g_tsStatus.check(MFX_ERR_ABORTED);
    }*/

    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);

    ProcessFrames(10);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_fieldcopy);

}
