/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

#define TEST_NAME avce_input_rgb_colorspace

namespace TEST_NAME
{

    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_AVC){}
        ~TestSuite() { }
        int RunTest(unsigned int id);
        static const unsigned int n_cases;

    private:

        struct tc_struct
        {
            mfxStatus QuerySts;
            mfxU16    ColourDescriptionPresent;
            mfxU16    MatrixCoefficients;

        };
        void SetParams(tsExtBufType<mfxVideoParam>& par, const tc_struct& tc);
        static const tc_struct test_case[];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/MFX_ERR_NONE, 0, MFX_TRANSFERMATRIX_UNKNOWN },
        {/*01*/MFX_ERR_NONE, 0, MFX_TRANSFERMATRIX_BT709 },
        {/*02*/MFX_ERR_NONE, 0, MFX_TRANSFERMATRIX_BT601 },
        {/*03*/MFX_ERR_NONE, 1, MFX_TRANSFERMATRIX_UNKNOWN },
        {/*04*/MFX_ERR_NONE, 1, MFX_TRANSFERMATRIX_BT709 },
        {/*05*/MFX_ERR_NONE, 1, MFX_TRANSFERMATRIX_BT601 },
        {/*06*/MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 2, MFX_TRANSFERMATRIX_UNKNOWN },
        {/*07*/MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, 256 },
        
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);


    class Verifier : public tsBitstreamProcessor, private tsParserH264AU
    {
    public:
        Verifier(tsExtBufType<mfxVideoParam>& par)
            :vsi(*((mfxExtVideoSignalInfo*)par.GetExtBuffer(MFX_EXTBUFF_VIDEO_SIGNAL_INFO)))
        {
            tsParserH264AU::set_trace_level(0);

            exp_colour_description_present_flag = vsi.ColourDescriptionPresent;
            exp_matrix_coefficients = vsi.MatrixCoefficients;
        }
        ~Verifier() {}

        bool IsOn(const mfxU16& option)
        {
            return MFX_CODINGOPTION_ON == option;
        }

        void SetExpected(const mfxU16& option, Bs8u& expected, Bs8u def = 1)
        {
            if (MFX_CODINGOPTION_ON == option)
                expected = 1;
            else if (MFX_CODINGOPTION_OFF == option)
                expected = 0;
            else
                expected = def;
        }

        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
        {
            tsParserH264AU::SetBuffer(bs);
            mfxU32 checked = 0;

            while (checked++ < nFrames)
            {
                UnitType& hdr = ParseOrDie();
                for (UnitType::nalu_param* nalu = hdr.NALU; nalu < hdr.NALU + hdr.NumUnits; nalu++)
                {
                    if (0x07 == nalu->nal_unit_type) //SPS
                    {
                        if (nalu->SPS->vui_parameters_present_flag && nalu->SPS->vui /*&& MFX_CODINGOPTION_ON != cod2.DisableVUI*/)
                        {
                            const vui_params *vui = nalu->SPS->vui;

                            EXPECT_EQ(exp_colour_description_present_flag, vui->colour_description_present_flag);
                            if (vui->colour_description_present_flag)
                                EXPECT_EQ(exp_matrix_coefficients, vui->matrix_coefficients);
                        }
                    }
                }
            }

            return MFX_ERR_NONE;
        }

    private:
        const mfxExtVideoSignalInfo  vsi;
        
        Bs8u exp_colour_description_present_flag;
        Bs8u exp_matrix_coefficients;
    };

    void TestSuite::SetParams(tsExtBufType<mfxVideoParam>& par, const tc_struct& tc)
    {
        mfxExtVideoSignalInfo&  vsi = par;
        
        vsi.ColourDescriptionPresent = tc.ColourDescriptionPresent;
        vsi.MatrixCoefficients = tc.MatrixCoefficients;
    }

   
    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];

        MFXInit();

        SetParams(m_par, tc);
        g_tsStatus.expect(tc.QuerySts);
        Query();

        if (tc.QuerySts == MFX_ERR_NONE)
        {
            Verifier verif(m_par);
            m_bs_processor = &verif;
            EncodeFrames(5);
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(TEST_NAME);
#undef TEST_NAME

}