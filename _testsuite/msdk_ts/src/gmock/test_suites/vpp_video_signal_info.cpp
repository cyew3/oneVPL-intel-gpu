#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

namespace vpp_video_signal_info
{

class TestSuite : tsVideoVPP
{
public:
    TestSuite()
        : tsVideoVPP()
    { }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:

    mfxExtVPPDoUse* vpp_du;
    mfxExtVPPDoUse* vpp_du2;

    enum
    {
        MFX_PAR = 1,
        DOUSE
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
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, MFX_ERR_NONE, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.In.TransferMatrix, MFX_TRANSFERMATRIX_BT601},
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.Out.TransferMatrix, MFX_TRANSFERMATRIX_BT601},
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.In.NominalRange, MFX_NOMINALRANGE_0_255},
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.Out.NominalRange, MFX_NOMINALRANGE_0_255},
        }
    },
    {/*01*/ MFX_ERR_NONE, MFX_ERR_NONE, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.In.TransferMatrix, MFX_TRANSFERMATRIX_BT601},
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.Out.TransferMatrix, MFX_TRANSFERMATRIX_BT601},
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.In.NominalRange, MFX_NOMINALRANGE_UNKNOWN},
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.Out.NominalRange, MFX_NOMINALRANGE_UNKNOWN},
        }
    },
    {/*02*/ MFX_ERR_NONE, MFX_ERR_NONE, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.In.TransferMatrix, MFX_NOMINALRANGE_UNKNOWN},
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.Out.TransferMatrix, MFX_NOMINALRANGE_UNKNOWN},
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.In.NominalRange, MFX_NOMINALRANGE_0_255},
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.Out.NominalRange, MFX_NOMINALRANGE_0_255},
        }
    },
    {/*03*/ MFX_ERR_NONE, MFX_ERR_NONE, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.In.TransferMatrix, MFX_TRANSFERMATRIX_BT709},
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.Out.TransferMatrix, MFX_TRANSFERMATRIX_BT709},
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.In.NominalRange, MFX_NOMINALRANGE_0_255},
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.Out.NominalRange, MFX_NOMINALRANGE_0_255},
        }
    },
    {/*04*/ MFX_ERR_NONE, MFX_ERR_NONE, 0,
        {
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.In.TransferMatrix, MFX_TRANSFERMATRIX_BT601},
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.Out.TransferMatrix, MFX_TRANSFERMATRIX_BT601},
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.In.NominalRange, MFX_NOMINALRANGE_16_235},
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.Out.NominalRange, MFX_NOMINALRANGE_16_235},
        }
    },
    {/*05*/ MFX_ERR_NONE, MFX_ERR_NONE, DOUSE, { } },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    m_par.AsyncDepth = 1;
    m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

    SETPARS(&m_par, MFX_PAR);

    tsExtBufType<mfxVideoParam> par_out;

    if (tc.mode == DOUSE)
    {
        m_par.AddExtBuffer(MFX_EXTBUFF_VPP_DOUSE, sizeof(mfxExtVPPDoUse));
        vpp_du = (mfxExtVPPDoUse*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DOUSE);
        vpp_du->NumAlg = 1;
        vpp_du->AlgList = new mfxU32[1];

        vpp_du->AlgList[0] = MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO;

        //for Query
        par_out.AddExtBuffer(MFX_EXTBUFF_VPP_DOUSE, sizeof(mfxExtVPPDoUse));
        vpp_du2 = (mfxExtVPPDoUse*)par_out.GetExtBuffer(MFX_EXTBUFF_VPP_DOUSE);
        vpp_du2->NumAlg = 1;
        vpp_du2->AlgList = new mfxU32[1];

        vpp_du2->AlgList[0] = 0;
    }
    else
        par_out.AddExtBuffer(MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO, sizeof(mfxExtVPPVideoSignalInfo));

    SetHandle();

    g_tsStatus.expect(tc.sts_query);
    Query(m_session, m_pPar, &par_out);

    g_tsStatus.expect(tc.sts_init);
    Init(m_session, m_pPar);

    if (tc.mode == DOUSE)
    {
        delete[] vpp_du->AlgList;
        vpp_du->AlgList = 0;
        delete[] vpp_du2->AlgList;
        vpp_du2->AlgList = 0;
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_video_signal_info);

}
