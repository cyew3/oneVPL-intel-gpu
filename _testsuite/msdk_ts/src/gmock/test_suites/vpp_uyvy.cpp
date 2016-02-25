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
    { }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:

    enum
    {
        MFX_PAR = 1,
        DENOISE,
        DEINTER,
        DETAIL,
        PROC_AMP,
        CROP,
        CROP_AND_RESIZE,
        FR_CONV,
        ROTATE,
        IMAGE_STAB,
        COMPOSITION,
        COMPOSITION_STREAM,
        SHIFT,
        FIELD_COPY,
        SIGNAL_INFO
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

};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, DENOISE, {
           {DENOISE, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 50}}
    },
    {/*01*/ MFX_ERR_NONE, DEINTER, {
           {DEINTER, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_BOB}}
    },
    {/*02*/ MFX_ERR_NONE, DETAIL, {
           {DETAIL, &tsStruct::mfxExtVPPDetail.DetailFactor, 50}}
    },
    {/*03*/ MFX_ERR_NONE, PROC_AMP, {
           {PROC_AMP, &tsStruct::mfxExtVPPProcAmp.Brightness, 0}}
    },
    {/*04*/ MFX_ERR_NONE, CROP, { } },
    {/*05*/ MFX_ERR_NONE, CROP_AND_RESIZE, { } },
    {/*06*/ MFX_ERR_NONE, FR_CONV, {
        {FR_CONV, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_PRESERVE_TIMESTAMP}}
    },
    {/*07*/ MFX_ERR_NONE, ROTATE, {
        {ROTATE, &tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_90}}
    },
    {/*08*/ MFX_ERR_NONE, IMAGE_STAB, {
        {IMAGE_STAB, &tsStruct::mfxExtVPPImageStab.Mode, MFX_IMAGESTAB_MODE_BOXING}}
    },
    {/*09*/ MFX_ERR_INVALID_VIDEO_PARAM, COMPOSITION, {
        {COMPOSITION, &tsStruct::mfxExtVPPComposite.NumInputStream, 0},
        {COMPOSITION_STREAM, &tsStruct::mfxVPPCompInputStream.DstW, 720},
        {COMPOSITION_STREAM, &tsStruct::mfxVPPCompInputStream.DstH, 480},
        {COMPOSITION_STREAM, &tsStruct::mfxVPPCompInputStream.DstX, 0},
        {COMPOSITION_STREAM, &tsStruct::mfxVPPCompInputStream.DstY, 0},}
    },
    {/*10*/ MFX_ERR_NONE, SHIFT, {
        {MFX_PAR, &tsStruct::mfxInfoVPP.In.Shift, 0},
        {MFX_PAR, &tsStruct::mfxInfoVPP.Out.Shift, 0}}
    },
    {/*11*/ MFX_ERR_NONE, FIELD_COPY, {
            {FIELD_COPY, &tsStruct::mfxExtVPPFieldProcessing.Mode, MFX_VPP_COPY_FIELD},
            {FIELD_COPY, &tsStruct::mfxExtVPPFieldProcessing.InField, MFX_PICTYPE_TOPFIELD},
            {FIELD_COPY, &tsStruct::mfxExtVPPFieldProcessing.OutField, MFX_PICTYPE_TOPFIELD}}
    },
    {/*12*/ MFX_ERR_NONE, SIGNAL_INFO, {
        {SIGNAL_INFO, &tsStruct::mfxExtVPPVideoSignalInfo.In.NominalRange, 1}}
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    std::vector<mfxExtBuffer*> buffs;
    mfxExtVPPDenoise                ext_buf_denoise;
    mfxExtVPPDeinterlacing          ext_buf_deinterlacing;
    mfxExtVPPDetail                 ext_buf_detail;
    mfxExtVPPProcAmp                ext_buf_procamp;
    mfxExtVPPFrameRateConversion    ext_buf_fr_conv;
    mfxExtVPPRotation               ext_buf_rotation;
    mfxExtVPPImageStab              ext_buf_im_stab;
    mfxExtVPPComposite              ext_buf_composition;
    mfxVPPCompInputStream           comp_input_stream;
    mfxExtVPPFieldProcessing        ext_buf_field_copy;
    mfxExtVPPVideoSignalInfo        ext_buf_signal_info;

    MFXInit();

    SETPARS(m_pPar, MFX_PAR);

    m_par.vpp.In.FourCC  = MFX_FOURCC_UYVY;
    m_par.vpp.Out.FourCC = MFX_FOURCC_NV12;

    m_par.vpp.In.ChromaFormat  = MFX_CHROMAFORMAT_YUV422;
    m_par.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

    if (tc.mode == CROP)
    {
        m_par.vpp.In.CropW = m_par.vpp.In.Width  * 4 / 5;
        m_par.vpp.In.CropH = m_par.vpp.In.Height * 4 / 5;

        m_par.vpp.Out.CropW = m_par.vpp.In.Width;
        m_par.vpp.Out.CropH = m_par.vpp.In.Height;
    }
    else if (tc.mode == CROP_AND_RESIZE)
    {
        m_par.vpp.In.CropW = m_par.vpp.In.Width  * 4 / 5;
        m_par.vpp.In.CropH = m_par.vpp.In.Height * 4 / 5;

        m_par.vpp.Out.CropW = m_par.vpp.In.CropW;
        m_par.vpp.Out.CropH = m_par.vpp.In.CropH;

        m_par.vpp.Out.Width = m_par.vpp.Out.CropW;
        m_par.vpp.Out.Height = m_par.vpp.Out.CropH;
    }
    else if (tc.mode == DENOISE)
    {
        memset(&ext_buf_denoise, 0, sizeof(mfxExtVPPDenoise));
        ext_buf_denoise.Header.BufferId = MFX_EXTBUFF_VPP_DENOISE;
        ext_buf_denoise.Header.BufferSz = sizeof(mfxExtVPPDenoise);

        SETPARS(&ext_buf_denoise, DENOISE);

        buffs.push_back((mfxExtBuffer*)&ext_buf_denoise);
    }
    else if (tc.mode == DEINTER)
    {
        memset(&ext_buf_deinterlacing, 0, sizeof(mfxExtVPPDeinterlacing));
        ext_buf_deinterlacing.Header.BufferId = MFX_EXTBUFF_VPP_DEINTERLACING;
        ext_buf_deinterlacing.Header.BufferSz = sizeof(mfxExtVPPDeinterlacing);

        SETPARS(&ext_buf_deinterlacing, DEINTER);

        buffs.push_back((mfxExtBuffer*)&ext_buf_deinterlacing);
    }
    else if (tc.mode == DETAIL)
    {
        memset(&ext_buf_detail, 0, sizeof(mfxExtVPPDetail));
        ext_buf_detail.Header.BufferId = MFX_EXTBUFF_VPP_DETAIL;
        ext_buf_detail.Header.BufferSz = sizeof(mfxExtVPPDetail);

        SETPARS(&ext_buf_detail, DETAIL);

        buffs.push_back((mfxExtBuffer*)&ext_buf_detail);
    }
    else if (tc.mode == PROC_AMP)
    {
        memset(&ext_buf_procamp, 0, sizeof(mfxExtVPPProcAmp));
        ext_buf_procamp.Header.BufferId = MFX_EXTBUFF_VPP_PROCAMP;
        ext_buf_procamp.Header.BufferSz = sizeof(mfxExtVPPProcAmp);

        SETPARS(&ext_buf_procamp, PROC_AMP);

        buffs.push_back((mfxExtBuffer*)&ext_buf_procamp);
    }
    else if (tc.mode == FR_CONV)
    {
        memset(&ext_buf_fr_conv, 0, sizeof(mfxExtVPPFrameRateConversion));
        ext_buf_fr_conv.Header.BufferId = MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION;
        ext_buf_fr_conv.Header.BufferSz = sizeof(mfxExtVPPFrameRateConversion);

        SETPARS(&ext_buf_fr_conv, FR_CONV);

        buffs.push_back((mfxExtBuffer*)&ext_buf_fr_conv);
    }
    else if (tc.mode == ROTATE)
    {
        memset(&ext_buf_rotation, 0, sizeof(mfxExtVPPRotation));
        ext_buf_rotation.Header.BufferId = MFX_EXTBUFF_VPP_ROTATION;
        ext_buf_rotation.Header.BufferSz = sizeof(mfxExtVPPRotation);

        SETPARS(&ext_buf_rotation, ROTATE);

        buffs.push_back((mfxExtBuffer*)&ext_buf_rotation);
    }
    else if (tc.mode == IMAGE_STAB)
    {
        memset(&ext_buf_im_stab, 0, sizeof(mfxExtVPPImageStab));
        ext_buf_im_stab.Header.BufferId = MFX_EXTBUFF_VPP_IMAGE_STABILIZATION;
        ext_buf_im_stab.Header.BufferSz = sizeof(mfxExtVPPImageStab);

        SETPARS(&ext_buf_im_stab, IMAGE_STAB);

        buffs.push_back((mfxExtBuffer*)&ext_buf_im_stab);
    }
    else if (tc.mode == COMPOSITION)
    {
        memset(&comp_input_stream, 0, sizeof(mfxVPPCompInputStream));

        SETPARS(&comp_input_stream, COMPOSITION_STREAM);

        memset(&ext_buf_composition, 0, sizeof(mfxExtVPPComposite));
        ext_buf_composition.Header.BufferId = MFX_EXTBUFF_VPP_COMPOSITE;
        ext_buf_composition.Header.BufferSz = sizeof(mfxExtVPPComposite);

        SETPARS(&ext_buf_composition, COMPOSITION);

        ext_buf_composition.InputStream = &comp_input_stream;

        buffs.push_back((mfxExtBuffer*)&ext_buf_composition);
    }
    else if (tc.mode == FIELD_COPY)
    {
        memset(&ext_buf_field_copy, 0, sizeof(mfxExtVPPFieldProcessing));
        ext_buf_field_copy.Header.BufferId = MFX_EXTBUFF_VPP_FIELD_PROCESSING;
        ext_buf_field_copy.Header.BufferSz = sizeof(mfxExtVPPFieldProcessing);

        SETPARS(&ext_buf_field_copy, FIELD_COPY);

        buffs.push_back((mfxExtBuffer*)&ext_buf_field_copy);
    }
    else if (tc.mode == SIGNAL_INFO)
    {
        memset(&ext_buf_signal_info, 0, sizeof(mfxExtVPPVideoSignalInfo));
        ext_buf_signal_info.Header.BufferId = MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO;
        ext_buf_signal_info.Header.BufferSz = sizeof(mfxExtVPPVideoSignalInfo);

        SETPARS(&ext_buf_signal_info, SIGNAL_INFO);

        buffs.push_back((mfxExtBuffer*)&ext_buf_signal_info);
    }

    m_par.NumExtParam = (mfxU16)buffs.size();
    m_par.ExtParam = m_par.NumExtParam ? &buffs[0] : 0;

    CreateAllocators();
    SetFrameAllocator();
    SetHandle();

    g_tsStatus.expect((g_tsOSFamily == MFX_OS_FAMILY_WINDOWS) ? MFX_ERR_INVALID_VIDEO_PARAM : tc.sts);
    if (tc.mode == IMAGE_STAB) g_tsStatus.expect(MFX_ERR_INVALID_VIDEO_PARAM);

    Init(m_session, m_pPar);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_uyvy);

}
