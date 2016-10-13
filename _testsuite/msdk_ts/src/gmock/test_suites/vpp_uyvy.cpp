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

namespace vpp_uyvy
{

class TestSuite : tsVideoVPP
{
public:
    TestSuite()
        : tsVideoVPP()
    {
        m_par.vpp.In.FourCC         = MFX_FOURCC_UYVY;
        m_par.vpp.Out.FourCC        = MFX_FOURCC_NV12;
        m_par.vpp.In.ChromaFormat   = MFX_CHROMAFORMAT_YUV422;
        m_par.vpp.Out.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
    }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:

    enum
    {
        MFX_PAR  = 1,
        COMP_PAR = 2
    };

    struct tc_struct
    {
        mfxStatus sts;

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
    {/*00*/ MFX_ERR_NONE, {
            {MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 50}}
    },
    {/*01*/ MFX_ERR_NONE, {
            {MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_BOB}}
    },
    {/*02*/ MFX_ERR_NONE, {
            {MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 50}}
    },
    {/*03*/ MFX_ERR_NONE, {
            {MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 0}}
    },
    {/*04*/ MFX_ERR_NONE, {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW,  576},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH,  384},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 720},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 480}}
    },
    {/*05*/ MFX_ERR_NONE, {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW,   576},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH,   384},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width,  576},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 384},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW,  576},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH,  384}}
    },
    {/*06*/ MFX_ERR_NONE, {
            {MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_PRESERVE_TIMESTAMP}}
    },
    {/*07*/ MFX_ERR_NONE, {
            {MFX_PAR, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_90}}
    },
    {/*08*/ MFX_ERR_INVALID_VIDEO_PARAM, {
            {MFX_PAR,  &tsStruct::mfxExtVPPComposite.NumInputStream,   1},
            {COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstW,        720},
            {COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstH,        480},
            {COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstX,          0},
            {COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstY,          0}}
    },
    {/*09*/ MFX_ERR_NONE, {
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Shift,  0},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Shift, 0}}
    },
    {/*10*/ MFX_ERR_NONE, {
            {MFX_PAR, &tsStruct::mfxExtVPPFieldProcessing.Mode,     MFX_VPP_COPY_FIELD},
            {MFX_PAR, &tsStruct::mfxExtVPPFieldProcessing.InField,  MFX_PICTYPE_TOPFIELD},
            {MFX_PAR, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_TOPFIELD}}
    },
    {/*11*/ MFX_ERR_NONE, {
            {MFX_PAR, &tsStruct::mfxExtVPPVideoSignalInfo.In.NominalRange, 1}}
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    mfxStatus sts = (g_tsOSFamily == MFX_OS_FAMILY_WINDOWS) ? MFX_ERR_INVALID_VIDEO_PARAM : tc.sts;

    MFXInit();

    SETPARS(&m_par, MFX_PAR);

    mfxExtVPPComposite* pExtComp = (mfxExtVPPComposite*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_COMPOSITE);

    if (pExtComp)
    {
        pExtComp->InputStream = new mfxVPPCompInputStream[pExtComp->NumInputStream];
        SETPARS(pExtComp, COMP_PAR);
    }

    CreateAllocators();
    SetFrameAllocator();
    SetHandle();

    g_tsStatus.expect(sts);
    Init(m_session, m_pPar);

    if (pExtComp && pExtComp->InputStream)
    {
        delete[] pExtComp->InputStream;
        pExtComp->InputStream = nullptr;
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_uyvy);

}
