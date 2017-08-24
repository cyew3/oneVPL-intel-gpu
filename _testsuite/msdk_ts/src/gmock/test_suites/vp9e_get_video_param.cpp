/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"

namespace vp9e_get_video_param
{
#define DEFAULT_ASPECT_RATIO_W 1
#define DEFAULT_ASPECT_RATIO_H 1
#define DEFAULT_GOP_SIZE 0xffff
#define DEFAULT_GOP_REF_DIST 1
#define DEFAULT_QPI 128
#define DEFAULT_QPP 133
#define DEFAULT_TARGET_USAGE MFX_TARGETUSAGE_BALANCED
#define DEFAULT_NUMREFFRAME 1
#define DEFAULT_LOWPOWER MFX_CODINGOPTION_ON

    class TestSuite : tsVideoEncoder
    {
    public:
        static const unsigned int n_cases;

        TestSuite() : tsVideoEncoder(MFX_CODEC_VP9)
        {

        }
        ~TestSuite() {}

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 type;
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        int RunTest(const tc_struct& tc, unsigned int fourcc_id);

    private:
        enum
        {
            MFX_PAR,
            NULL_SESSION,
            NOT_INIT,
            FAILED_INIT,
            NULL_PAR,
            NONE,
            DEFAULTS,
            DEFAULTS_BRC,
            SPSPPS_EXTBUF,
            AFTER_ENCODING_CHECK,
            AFTER_RESET_CHECK
        };

        static const tc_struct test_case[];

        static const mfxU32 sps_buf_sz = 100;
        static const mfxU32 pps_buf_sz = 50;
        mfxU8 m_sp_buf[sps_buf_sz + pps_buf_sz];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, NONE },
        {/*01*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION },
        {/*02*/ MFX_ERR_NOT_INITIALIZED, NOT_INIT },
        {/*03*/ MFX_ERR_NOT_INITIALIZED, FAILED_INIT },
        {/*04*/ MFX_ERR_NULL_PTR, NULL_PAR },
        {/*05*/ MFX_ERR_NONE, NONE,
            { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY },
        },
        {/*06 Check nils map to defaults correctly*/ MFX_ERR_NONE, DEFAULTS,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW, 0},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH, 0},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 0},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.IdrInterval, 0},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 0 },
            }
        },
        {/*07 RateControl=0 should map to CBR when TargetKbps is set*/ MFX_ERR_NONE, DEFAULTS_BRC,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps/*mandatory value*/, 500 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 0 },
            }
        },
        {/*08 RateControl=0 should map to CBR when TargetKbps=MaxKbps*/ MFX_ERR_NONE, DEFAULTS_BRC,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps/*mandatory value*/, 500 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 500 },
            }
        },
        {/*09 RateControl=0 should map to VBR when TargetKbps<MaxKbps*/ MFX_ERR_NONE, DEFAULTS_BRC,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps/*mandatory value*/, 500 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 600 },
            }
        },
        {/*10 QPI and QPP should set to defaults when RateControl=CQP*/ MFX_ERR_NONE, DEFAULTS_BRC,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 0 },
        }
        },
        {/*11 Check this specific extbuf because it is mentioned in the doc*/ MFX_ERR_UNSUPPORTED, SPSPPS_EXTBUF },
        {/*12 Check the same params return after encoding*/ MFX_ERR_NONE, AFTER_ENCODING_CHECK },
        {/*13 Check the params are updated after reset*/ MFX_ERR_NONE, AFTER_RESET_CHECK },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(const tc_struct& tc, unsigned int fourcc_id)
    {
        TS_START;

        mfxVideoParam requested_par = {};

        MFXInit();
        Load();

        //set default params
        m_par.mfx.FrameInfo.PicStruct = 1;
        m_par.mfx.GopPicSize = 15;
        m_par.mfx.GopRefDist = 1;
        m_par.mfx.FrameInfo.AspectRatioW = 1;
        m_par.mfx.FrameInfo.AspectRatioH = 1;

        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_P010)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else if (fourcc_id == MFX_FOURCC_AYUV)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_Y410)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else
        {
            g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        SETPARS(m_pPar, MFX_PAR);

        InitAndSetAllocator();

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_VP9E_HW.Data, sizeof(MFX_PLUGINID_VP9E_HW.Data)))
        {
            // MFX_PLUGIN_VP9E_HW unsupported on platform less CNL(NV12) and ICL(P010, AYUV, Y410)
            if ((fourcc_id == MFX_FOURCC_NV12 && g_tsHWtype < MFX_HW_CNL)
                || ((fourcc_id == MFX_FOURCC_P010 || fourcc_id == MFX_FOURCC_AYUV
                    || fourcc_id == MFX_FOURCC_Y410) && g_tsHWtype < MFX_HW_ICL))
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query();
                return 0;
            }
        }
        else {
            g_tsLog << "WARNING: loading encoder from plugin failed!\n";
            return 0;
        }

        //init
        if (tc.type == FAILED_INIT)
        {
            g_tsStatus.expect(MFX_ERR_NULL_PTR);
            Init(m_session, NULL);
        }
        else if (tc.type != NOT_INIT)
        {
            Query();
            if (tc.type == DEFAULTS_BRC && m_pPar->mfx.RateControlMethod == MFX_RATECONTROL_CQP)
            {
                g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            }
            Init();
        }

        //call test function
        g_tsStatus.expect(tc.sts);
        if (tc.type == SPSPPS_EXTBUF)
        {
            mfxExtCodingOptionSPSPPS& sp = m_par;
            sp.SPSBuffer = m_sp_buf;
            sp.PPSBuffer = m_sp_buf + sps_buf_sz;
            sp.SPSBufSize = sps_buf_sz;
            sp.PPSBufSize = pps_buf_sz;

            tsExtBufType<mfxVideoParam> getvpar_out_par = m_par;
            mfxStatus getvparam_result_status = GetVideoParam(m_session, &getvpar_out_par);
            mfxExtCodingOptionSPSPPS *spspps_params = reinterpret_cast <mfxExtCodingOptionSPSPPS*>(getvpar_out_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION_SPSPPS));
        }
        else
        {
            if (tc.type == NULL_SESSION)
                GetVideoParam(NULL, &requested_par);
            else if (tc.type == NULL_PAR)
                GetVideoParam(m_session, NULL);
            else
                GetVideoParam(m_session, &requested_par);
        }

        //check requested parameters
        if (tc.sts == MFX_ERR_NONE)
        {
            if(tc.type == DEFAULTS)
            {
                EXPECT_EQ(requested_par.mfx.FrameInfo.AspectRatioW, DEFAULT_ASPECT_RATIO_W)
                    << "ERROR: Default AspectRatioW expected to be " << DEFAULT_ASPECT_RATIO_W << ", but it is " << requested_par.mfx.FrameInfo.AspectRatioW;
                EXPECT_EQ(requested_par.mfx.FrameInfo.AspectRatioH, DEFAULT_ASPECT_RATIO_H)
                    << "ERROR: Default AspectRatioH expected to be " << DEFAULT_ASPECT_RATIO_H << ", but it is " << requested_par.mfx.FrameInfo.AspectRatioH;
                EXPECT_EQ(requested_par.mfx.GopPicSize, DEFAULT_GOP_SIZE)
                    << "ERROR: Default GopPicSize expected to be " << DEFAULT_GOP_SIZE << ", but it is " << requested_par.mfx.GopPicSize;
                EXPECT_EQ(requested_par.mfx.GopRefDist, DEFAULT_GOP_REF_DIST)
                    << "ERROR: Default GopRefDist expected to be " << DEFAULT_GOP_REF_DIST << ", but it is " << requested_par.mfx.GopRefDist;
                EXPECT_EQ(requested_par.mfx.TargetUsage, DEFAULT_TARGET_USAGE)
                    << "ERROR: Default TargetUsage expected to be " << DEFAULT_TARGET_USAGE << ", but it is " << requested_par.mfx.TargetUsage;
                EXPECT_EQ(requested_par.mfx.NumRefFrame, DEFAULT_NUMREFFRAME)
                    << "ERROR: Default NumRefFrame expected to be " << DEFAULT_NUMREFFRAME << ", but it is " << requested_par.mfx.NumRefFrame;
                EXPECT_EQ(requested_par.mfx.LowPower, DEFAULT_LOWPOWER)
                    << "ERROR: Default LowPower expected to be " << DEFAULT_LOWPOWER << ", but it is " << requested_par.mfx.LowPower;

                if (m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_NV12 || m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_AYUV)
                {
                    EXPECT_EQ(requested_par.mfx.FrameInfo.BitDepthLuma, 8)
                        << "ERROR: Default BitDepthLuma isn't 8";
                    EXPECT_EQ(requested_par.mfx.FrameInfo.BitDepthChroma, 8)
                        << "ERROR: Default BitDepthChroma isn't 8";
                }
                else if (m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_P010 || m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_Y410)
                {
                    EXPECT_EQ(requested_par.mfx.FrameInfo.BitDepthLuma, 10)
                        << "ERROR: Default BitDepthLuma isn't 10";
                    EXPECT_EQ(requested_par.mfx.FrameInfo.BitDepthChroma, 10)
                        << "ERROR: Default BitDepthChroma isn't 10";
                }
            }
            else if (tc.type == DEFAULTS_BRC)
            {
                if (m_pPar->mfx.RateControlMethod == MFX_RATECONTROL_CQP && m_pPar->mfx.QPI == 0 && m_pPar->mfx.QPP == 0)
                {
                    EXPECT_EQ(requested_par.mfx.QPI, DEFAULT_QPI)
                        << "ERROR: Default QPI expected to be " << DEFAULT_QPI << ", but it is " << requested_par.mfx.QPI;
                    EXPECT_EQ(requested_par.mfx.QPP, DEFAULT_QPP)
                        << "ERROR: Default QPP expected to be " << DEFAULT_QPP << ", but it is " << requested_par.mfx.QPP;
                }
                else if (m_pPar->mfx.RateControlMethod == 0)
                {
                    if ((m_pPar->mfx.TargetKbps && !m_pPar->mfx.MaxKbps) || (m_pPar->mfx.TargetKbps && (m_pPar->mfx.TargetKbps == m_pPar->mfx.MaxKbps)))
                    {
                        EXPECT_EQ(requested_par.mfx.RateControlMethod, MFX_RATECONTROL_CBR)
                            << "ERROR: Default RateControlMethod expected to be " << MFX_RATECONTROL_CBR << ", but it is " << requested_par.mfx.RateControlMethod;
                    }
                    else if (m_pPar->mfx.TargetKbps && (m_pPar->mfx.TargetKbps < m_pPar->mfx.MaxKbps))
                    {
                        EXPECT_EQ(requested_par.mfx.RateControlMethod, MFX_RATECONTROL_VBR)
                            << "ERROR: Default RateControlMethod expected to be " << MFX_RATECONTROL_VBR << ", but it is " << requested_par.mfx.RateControlMethod;
                    }
                }
            }
            else
            {
                EXPECT_EQ(0, memcmp(&(requested_par.mfx.FrameInfo.FourCC), &(m_pPar->mfx.FrameInfo.FourCC), sizeof(mfxU32)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: FourCC";
                EXPECT_EQ(0, memcmp(&(requested_par.mfx.FrameInfo.Width), &(m_pPar->mfx.FrameInfo.Width), sizeof(mfxU16)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: Width";
                EXPECT_EQ(0, memcmp(&(requested_par.mfx.FrameInfo.Height), &(m_pPar->mfx.FrameInfo.Height), sizeof(mfxU16)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: Height";
                EXPECT_EQ(0, memcmp(&(requested_par.mfx.FrameInfo.CropW), &(m_pPar->mfx.FrameInfo.CropW), sizeof(mfxU16)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: CropW";
                EXPECT_EQ(0, memcmp(&(requested_par.mfx.FrameInfo.CropH), &(m_pPar->mfx.FrameInfo.CropH), sizeof(mfxU16)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: CropH";
                EXPECT_EQ(0, memcmp(&(requested_par.mfx.FrameInfo.CropX), &(m_pPar->mfx.FrameInfo.CropX), sizeof(mfxU16)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: CropX";
                EXPECT_EQ(0, memcmp(&(requested_par.mfx.FrameInfo.CropY), &(m_pPar->mfx.FrameInfo.CropY), sizeof(mfxU16)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: CropY";
                EXPECT_EQ(0, memcmp(&(requested_par.mfx.FrameInfo.FrameRateExtN), &(m_pPar->mfx.FrameInfo.FrameRateExtN), sizeof(mfxU32)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: FrameRateExtN";
                EXPECT_EQ(0, memcmp(&(requested_par.mfx.FrameInfo.FrameRateExtD), &(m_pPar->mfx.FrameInfo.FrameRateExtD), sizeof(mfxU32)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: FrameRateExtD";
                EXPECT_EQ(0, memcmp(&(requested_par.mfx.FrameInfo.AspectRatioW), &(m_pPar->mfx.FrameInfo.AspectRatioW), sizeof(mfxU16)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: AspectRatioW";
                EXPECT_EQ(0, memcmp(&(requested_par.mfx.FrameInfo.AspectRatioH), &(m_pPar->mfx.FrameInfo.AspectRatioH), sizeof(mfxU16)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: AspectRatioH";
                EXPECT_EQ(0, memcmp(&(requested_par.mfx.FrameInfo.ChromaFormat), &(m_pPar->mfx.FrameInfo.ChromaFormat), sizeof(mfxU16)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: ChromaFormat";
                EXPECT_EQ(0, memcmp(&(requested_par.mfx.GopPicSize), &(m_pPar->mfx.GopPicSize), sizeof(mfxU16)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: GopPicSize";
                EXPECT_EQ(0, memcmp(&(requested_par.mfx.GopRefDist), &(m_pPar->mfx.GopRefDist), sizeof(mfxU16)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: GopRefDist";
                EXPECT_EQ(0, memcmp(&(requested_par.mfx.IdrInterval), &(m_pPar->mfx.IdrInterval), sizeof(mfxU16)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: IdrInterval";
                EXPECT_EQ(0, memcmp(&(requested_par.mfx.RateControlMethod), &(m_pPar->mfx.RateControlMethod), sizeof(mfxU16)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: RateControlMethod";
                EXPECT_EQ(0, memcmp(&(requested_par.mfx.QPI), &(m_pPar->mfx.QPI), sizeof(mfxU16)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: QPI";
                EXPECT_EQ(0, memcmp(&(requested_par.mfx.QPB), &(m_pPar->mfx.QPB), sizeof(mfxU16)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: QPB";
                EXPECT_EQ(0, memcmp(&(requested_par.mfx.QPP), &(m_pPar->mfx.QPP), sizeof(mfxU16)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: QPP";
                EXPECT_EQ(0, memcmp(&(requested_par.IOPattern), &(m_pPar->IOPattern), sizeof(mfxU16)))
                    << "ERROR: GetVideoParam() unexpectedly changed the following param: IOPattern";
            }

            if (tc.type == AFTER_ENCODING_CHECK || tc.type == AFTER_RESET_CHECK)
            {
                //encoding of a frame and then request the parameters once again to check nothing has changed
                EncodeFrames(1);

                mfxVideoParam par_after_encoding = {};
                mfxStatus after_encoding_status = GetVideoParam(m_session, &par_after_encoding);
                g_tsStatus.check(after_encoding_status);

                if (memcmp(&requested_par, &par_after_encoding, sizeof(mfxVideoParam)) != 0)
                {
                    TS_TRACE(requested_par);
                    TS_TRACE(par_after_encoding);

                    ADD_FAILURE() << "Requested params after encoding expected to be the same as before, but they are not"; throw tsFAIL;
                }

                if (tc.type == AFTER_RESET_CHECK)
                {
                    //now change several params and check GetVideoParam() returns them after Reset()
                    mfxU16 new_qpp = m_pPar->mfx.QPP = m_pPar->mfx.QPP / 2;
                    mfxU16 new_target_cropw = m_pPar->mfx.FrameInfo.CropW = m_pPar->mfx.FrameInfo.CropW / 2;
                    mfxU16 new_gop_size = m_pPar->mfx.GopPicSize = 10;

                    mfxStatus reset_status = Reset(m_session, m_pPar);
                    g_tsStatus.check(reset_status);

                    mfxVideoParam par_after_reset = {};
                    mfxStatus after_reset_status = GetVideoParam(m_session, &par_after_reset);
                    g_tsStatus.check(after_reset_status);

                    EXPECT_EQ(0, memcmp(&(par_after_reset.mfx.QPP), &new_qpp, sizeof(mfxU16)))
                        << "ERROR: After reset GetVideoParam() expected to return TargetKbps=" << new_qpp << ", but it is " << par_after_reset.mfx.TargetKbps;
                    EXPECT_EQ(0, memcmp(&(par_after_reset.mfx.FrameInfo.CropW), &new_target_cropw, sizeof(mfxU16)))
                        << "ERROR: After reset GetVideoParam() expected to return CropX=" << new_target_cropw << ", but it is " << par_after_reset.mfx.FrameInfo.CropW;
                    EXPECT_EQ(0, memcmp(&(par_after_reset.mfx.GopPicSize), &new_gop_size, sizeof(mfxU16)))
                        << "ERROR: After reset GetVideoParam() expected to return GopSize=" << new_gop_size << ", but it is " << par_after_reset.mfx.GopPicSize;
                }
            }
        }
        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_get_video_param,              RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_420_p010_get_video_param, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_8b_444_ayuv_get_video_param,  RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_444_y410_get_video_param, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
};
