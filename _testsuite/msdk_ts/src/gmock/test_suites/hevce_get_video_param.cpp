/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2018 Intel Corporation. All Rights Reserved.

File Name: hevce_get_video_param.cpp

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace hevce_get_video_param
{
    class TestSuite : tsVideoEncoder
    {
    public:
        static const unsigned int n_cases;

        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC)
        {

        }
        ~TestSuite() {}

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

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(tc_struct tc, unsigned int fourcc_id);

    private:
        enum
        {
            MFX_PAR,
            NULL_SESSION,
            NOT_INIT,
            FAILED_INIT,
            NULL_PAR,
            NONE
        };

        static const tc_struct test_case[];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, NONE },
        {/*01*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION },
        {/*02*/ MFX_ERR_NOT_INITIALIZED, NOT_INIT },
        {/*03*/ MFX_ERR_NOT_INITIALIZED, FAILED_INIT },
        {/*04*/ MFX_ERR_NULL_PTR, NULL_PAR },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id)
    {
        TS_START;

        mfxHDL hdl;
        mfxHandleType type;
        mfxVideoParam new_par = {};

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))
        {
            if (g_tsHWtype < MFX_HW_SKL) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query();
                return 0;
            }

            if ((fourcc_id == MFX_FOURCC_P010)
                && (g_tsHWtype < MFX_HW_KBL)) {
                g_tsLog << "\n\nWARNING: P010 format only supported on KBL+!\n\n\n";
                throw tsSKIP;
            }
            if ((fourcc_id == MFX_FOURCC_Y210 || fourcc_id == MFX_FOURCC_YUY2 ||
                fourcc_id == MFX_FOURCC_AYUV || fourcc_id == MFX_FOURCC_Y410)
                && (g_tsHWtype < MFX_HW_ICL))
            {
                g_tsLog << "\n\nWARNING: RExt formats are only supported starting from ICL!\n\n\n";
                throw tsSKIP;
            }
            if ((fourcc_id == GMOCK_FOURCC_P012 || fourcc_id == GMOCK_FOURCC_Y212 || fourcc_id == GMOCK_FOURCC_Y412)
                && (g_tsHWtype < MFX_HW_TGL))
            {
                g_tsLog << "\n\nWARNING: 12b formats are only supported starting from TGL!\n\n\n";
                throw tsSKIP;
            }
            if ((fourcc_id == MFX_FOURCC_YUY2 || fourcc_id == MFX_FOURCC_Y210 || fourcc_id == GMOCK_FOURCC_Y212)
                && (g_tsConfig.lowpower == MFX_CODINGOPTION_ON))
            {
                g_tsLog << "\n\nWARNING: 4:2:2 formats are NOT supported on VDENC!\n\n\n";
                throw tsSKIP;
            }
            if ((fourcc_id == MFX_FOURCC_AYUV || fourcc_id == MFX_FOURCC_Y410 || fourcc_id == GMOCK_FOURCC_Y412)
                && (g_tsConfig.lowpower != MFX_CODINGOPTION_ON))
            {
                g_tsLog << "\n\nWARNING: 4:4:4 formats are only supported on VDENC!\n\n\n";
                throw tsSKIP;
            }

        }

        MFXInit();
        Load();

        //set default param
        m_par.mfx.FrameInfo.Width = 736;
        m_par.mfx.FrameInfo.Height = 576;
        m_par.mfx.FrameInfo.CropW = 720;
        m_par.mfx.FrameInfo.CropH = 576;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;
        m_par.mfx.FrameInfo.ChromaFormat = 1;
        m_par.mfx.FrameInfo.PicStruct = 1;
        m_par.mfx.FrameInfo.AspectRatioW = 16;
        m_par.mfx.FrameInfo.AspectRatioH = 9;
        m_par.mfx.GopPicSize = 15;
        m_par.mfx.GopRefDist = 1;
        m_par.mfx.IdrInterval = 2;
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        m_par.mfx.QPI = 26;
        m_par.mfx.QPB = 26;
        m_par.mfx.QPP = 26;
        m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

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
        else if (fourcc_id == GMOCK_FOURCC_P012)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P016;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
        }
        else if (fourcc_id == MFX_FOURCC_YUY2)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_Y210)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else if (fourcc_id == GMOCK_FOURCC_Y212)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y216;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
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
        else if (fourcc_id == GMOCK_FOURCC_Y412)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y416;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
        }
        else
        {
            g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        SETPARS(m_pPar, MFX_PAR);

        if (!GetAllocator())
        {
            UseDefaultAllocator(
                (m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
                );
        }

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))
        {
            //set handle
            if (!((m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)))
            {
                m_pFrameAllocator = GetAllocator();
                SetFrameAllocator();
                m_pVAHandle = m_pFrameAllocator;
                m_pVAHandle->get_hdl(type, hdl);
                SetHandle(m_session, type, hdl);
                m_is_handle_set = (g_tsStatus.get() >= 0);
            }
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
            Init();
        }
        //call tets function
        g_tsStatus.expect(tc.sts);
        if (tc.type == NULL_SESSION)
            GetVideoParam(NULL, &new_par);
        else if (tc.type == NULL_PAR)
            GetVideoParam(m_session, NULL);
        else
            GetVideoParam(m_session, &new_par);
        //check
        if (tc.sts == MFX_ERR_NONE)
        {
            EXPECT_EQ(0, memcmp(&(new_par.mfx.FrameInfo.FourCC), &(m_pPar->mfx.FrameInfo.FourCC), sizeof(mfxU32)))
                << "ERROR: Init parameters not equal output from GetVideoParam: FourCC";
            EXPECT_EQ(0, memcmp(&(new_par.mfx.FrameInfo.Width), &(m_pPar->mfx.FrameInfo.Width), sizeof(mfxU16)))
                << "ERROR: Init parameters not equal output from GetVideoParam: Width";
            EXPECT_EQ(0, memcmp(&(new_par.mfx.FrameInfo.Height), &(m_pPar->mfx.FrameInfo.Height), sizeof(mfxU16)))
                << "ERROR: Init parameters not equal output from GetVideoParam: Height";
            EXPECT_EQ(0, memcmp(&(new_par.mfx.FrameInfo.CropW), &(m_pPar->mfx.FrameInfo.CropW), sizeof(mfxU16)))
                << "ERROR: Init parameters not equal output from GetVideoParam: CropW";
            EXPECT_EQ(0, memcmp(&(new_par.mfx.FrameInfo.CropH), &(m_pPar->mfx.FrameInfo.CropH), sizeof(mfxU16)))
                << "ERROR: Init parameters not equal output from GetVideoParam: CropH";
            EXPECT_EQ(0, memcmp(&(new_par.mfx.FrameInfo.CropX), &(m_pPar->mfx.FrameInfo.CropX), sizeof(mfxU16)))
                << "ERROR: Init parameters not equal output from GetVideoParam: CropX";
            EXPECT_EQ(0, memcmp(&(new_par.mfx.FrameInfo.CropY), &(m_pPar->mfx.FrameInfo.CropY), sizeof(mfxU16)))
                << "ERROR: Init parameters not equal output from GetVideoParam: CropY";
            EXPECT_EQ(0, memcmp(&(new_par.mfx.FrameInfo.FrameRateExtN), &(m_pPar->mfx.FrameInfo.FrameRateExtN), sizeof(mfxU32)))
                << "ERROR: Init parameters not equal output from GetVideoParam: FrameRateExtN";
            EXPECT_EQ(0, memcmp(&(new_par.mfx.FrameInfo.FrameRateExtD), &(m_pPar->mfx.FrameInfo.FrameRateExtD), sizeof(mfxU32)))
                << "ERROR: Init parameters not equal output from GetVideoParam: FrameRateExtD";
            EXPECT_EQ(0, memcmp(&(new_par.mfx.FrameInfo.AspectRatioW), &(m_pPar->mfx.FrameInfo.AspectRatioW), sizeof(mfxU16)))
                << "ERROR: Init parameters not equal output from GetVideoParam: AspectRatioW";
            EXPECT_EQ(0, memcmp(&(new_par.mfx.FrameInfo.AspectRatioH), &(m_pPar->mfx.FrameInfo.AspectRatioH), sizeof(mfxU16)))
                << "ERROR: Init parameters not equal output from GetVideoParam: AspectRatioH";
            EXPECT_EQ(0, memcmp(&(new_par.mfx.FrameInfo.ChromaFormat), &(m_pPar->mfx.FrameInfo.ChromaFormat), sizeof(mfxU16)))
                << "ERROR: Init parameters not equal output from GetVideoParam: ChromaFormat";
            EXPECT_EQ(0, memcmp(&(new_par.mfx.GopPicSize), &(m_pPar->mfx.GopPicSize), sizeof(mfxU16)))
                << "ERROR: Init parameters not equal output from GetVideoParam: GopPicSize";
            EXPECT_EQ(0, memcmp(&(new_par.mfx.GopRefDist), &(m_pPar->mfx.GopRefDist), sizeof(mfxU16)))
                << "ERROR: Init parameters not equal output from GetVideoParam: GopRefDist";
            EXPECT_EQ(0, memcmp(&(new_par.mfx.IdrInterval), &(m_pPar->mfx.IdrInterval), sizeof(mfxU16)))
                << "ERROR: Init parameters not equal output from GetVideoParam: IdrInterval";
            EXPECT_EQ(0, memcmp(&(new_par.mfx.RateControlMethod), &(m_pPar->mfx.RateControlMethod), sizeof(mfxU16)))
                << "ERROR: Init parameters not equal output from GetVideoParam: RateControlMethod";
            EXPECT_EQ(0, memcmp(&(new_par.mfx.QPI), &(m_pPar->mfx.QPI), sizeof(mfxU16)))
                << "ERROR: Init parameters not equal output from GetVideoParam: QPI";
            EXPECT_EQ(0, memcmp(&(new_par.mfx.QPB), &(m_pPar->mfx.QPB), sizeof(mfxU16)))
                << "ERROR: Init parameters not equal output from GetVideoParam: QPB";
            EXPECT_EQ(0, memcmp(&(new_par.mfx.QPP), &(m_pPar->mfx.QPP), sizeof(mfxU16)))
                << "ERROR: Init parameters not equal output from GetVideoParam: QPP";
            EXPECT_EQ(0, memcmp(&(new_par.IOPattern), &(m_pPar->IOPattern), sizeof(mfxU16)))
                << "ERROR: Init parameters not equal output from GetVideoParam";
        }
        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_get_video_param, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_get_video_param, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_420_p016_get_video_param, RunTest_Subtype<GMOCK_FOURCC_P012>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_get_video_param, RunTest_Subtype<MFX_FOURCC_YUY2>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_get_video_param, RunTest_Subtype<MFX_FOURCC_Y210>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_422_y216_get_video_param, RunTest_Subtype<GMOCK_FOURCC_Y212>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_get_video_param, RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_get_video_param, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_444_y416_get_video_param, RunTest_Subtype<GMOCK_FOURCC_Y412>, n_cases);
};