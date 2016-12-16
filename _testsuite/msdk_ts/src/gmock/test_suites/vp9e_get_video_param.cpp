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

namespace vp9e_get_video_param
{
    class TestSuite : tsVideoEncoder
    {
    public:
        static const unsigned int n_cases;

        TestSuite() : tsVideoEncoder(MFX_CODEC_VP9)
        {

        }
        ~TestSuite() {}
        int RunTest(unsigned int id);

    private:
        enum
        {
            MFX_PAR,
            NULL_SESSION,
            NOT_INIT,
            FAILED_INIT,
            NULL_PAR,
            NONE,
            DEFAULTS
        };

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

        static const tc_struct test_case[];
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
        {/*06*/ MFX_ERR_NONE, DEFAULTS,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW, 0},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH, 0},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 0},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.IdrInterval, 0},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, 0},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 0},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 0},
            }
        }
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

#define DEFAULT_ASPECT_RATIO_W 1
#define DEFAULT_ASPECT_RATIO_H 1
#define DEFAULT_GOP_SIZE 0xffff
#define DEFAULT_GOP_REF_DIST 1
#define DEFAULT_BRC MFX_RATECONTROL_CQP
#define DEFAULT_QPI 128
#define DEFAULT_QPP 133

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_case[id];
        mfxVideoParam new_par = {};

        MFXInit();
        Load();

        //set default params
        m_par.mfx.FrameInfo.PicStruct = 1;
        m_par.mfx.GopPicSize = 15;
        m_par.mfx.GopRefDist = 1;
        m_par.mfx.FrameInfo.AspectRatioW = 1;
        m_par.mfx.FrameInfo.AspectRatioH = 1;

        SETPARS(m_pPar, MFX_PAR);

        InitAndSetAllocator();

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_VP9E_HW.Data, sizeof(MFX_PLUGINID_VP9E_HW.Data)))
        {
            if (g_tsHWtype < MFX_HW_CNL) // unsupported on platform less CNL
            {
            g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            g_tsLog << "WARNING: Unsupported HW Platform!\n";
            Query();
            return 0;
            }
        } else {
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
            if (tc.type != DEFAULTS)
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
            else
            {
                EXPECT_EQ(new_par.mfx.FrameInfo.AspectRatioW, DEFAULT_ASPECT_RATIO_W)
                    << "ERROR: Default AspectRatioW isn't actual to " << DEFAULT_ASPECT_RATIO_W;
                EXPECT_EQ(new_par.mfx.FrameInfo.AspectRatioH, DEFAULT_ASPECT_RATIO_H)
                    << "ERROR: Default AspectRatioH isn't actual to " << DEFAULT_ASPECT_RATIO_H;
                EXPECT_EQ(new_par.mfx.GopPicSize, DEFAULT_GOP_SIZE)
                    << "ERROR: Default GopPicSize isn't actual to " << DEFAULT_GOP_SIZE;
                EXPECT_EQ(new_par.mfx.GopRefDist, DEFAULT_GOP_REF_DIST)
                    << "ERROR: Default GopRefDist isn't actual to " << DEFAULT_GOP_REF_DIST;
                EXPECT_EQ(new_par.mfx.RateControlMethod, DEFAULT_BRC)
                    << "ERROR: Default RateControlMethod isn't actual to " << DEFAULT_BRC;
                EXPECT_EQ(new_par.mfx.QPI, DEFAULT_QPI)
                    << "ERROR: Default QPI isn't actual to" << DEFAULT_QPI;
                EXPECT_EQ(new_par.mfx.QPP, DEFAULT_QPP)
                    << "ERROR: Default QPP isn't actual to " << DEFAULT_QPP;
            }
        }
        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(vp9e_get_video_param);
};
