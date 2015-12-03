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
        int RunTest(unsigned int id);

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

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_case[id];
        mfxHDL hdl;
        mfxHandleType type;
        mfxVideoParam new_par = {};

        MFXInit();
        Load();

        //set defoult param
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
            if (g_tsHWtype < MFX_HW_SKL) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
            {
            g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            g_tsLog << "WARNING: Unsupported HW Platform!\n";
            Query();
            return 0;
            }

            //HEVCE_HW need aligned width and height for 32
            m_par.mfx.FrameInfo.Width = ((m_par.mfx.FrameInfo.Width + 32 - 1) & ~(32 - 1));
            m_par.mfx.FrameInfo.Height = ((m_par.mfx.FrameInfo.Height + 32 - 1) & ~(32 - 1));


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

    TS_REG_TEST_SUITE_CLASS(hevce_get_video_param);
};