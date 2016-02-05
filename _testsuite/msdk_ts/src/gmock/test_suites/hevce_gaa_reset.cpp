#include "ts_encoder.h"
#include "ts_struct.h"
#include "mfxplugin.h"
#include "ts_enc.h"


namespace hevce_gaa_reset
{


class TestSuite : tsVideoENC
{
public:
    TestSuite() : tsVideoENC(MFX_CODEC_HEVC, 1) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    struct tc_par;
    static const mfxU32 n_par = 6;

    enum
    {
        NULL_SESSION = 1,
        NOT_INIT     = 2,
        CLOSED       = 3,
    };

    struct tc_struct
    {
        mfxStatus sts_init;
        mfxStatus sts_init_e;
        mfxStatus sts_reset;
        mfxStatus sts_reset_e;
        std::string file_init;
        std::string file_reset;
        mfxU16 nframes;
        mfxU32 mode;
        struct f_pair
        {
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par_init[n_par];
        f_pair set_par_reset[n_par];
        f_pair in_fei_init[3];
        f_pair in_fei_reset[3];
        f_pair in_init[3];
        f_pair in_reset[3];

    };

    static const tc_struct test_case[];
};


const TestSuite::tc_struct TestSuite::test_case[] =
{

    {/*00*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", "forBehaviorTest/foreman_cif.yuv", 1, 0},
    {/*01*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", "forBehaviorTest/foreman_cif.yuv", 1, 0,
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0},
        {},
        {},
        {},
        {},
        {}
    },// fail init
    {/*02*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NOT_INITIALIZED, MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", "forBehaviorTest/foreman_cif.yuv", 1, NOT_INIT,
        {},
        {},
        {},
        {},
        {},
        {}
    },
    {/*03*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NOT_INITIALIZED, MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", "forBehaviorTest/foreman_cif.yuv", 1, CLOSED,
        {},
        {},
        {},
        {},
        {},
        {}
    },
    {/*04*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", "forBehaviorTest/foreman_cif.yuv", 1, 0,
        {},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0},
        {},
        {},
        {},
        {}
    },
    {/*05*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", "forBehaviorTest/foreman_cif.yuv", 1, 0,
        {},
        {&tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0},
        {},
        {},
        {},
        {}
    },
    {/*06*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", "forBehaviorTest/foreman_cif.yuv", 1, 0,
        {},
        {&tsStruct::mfxVideoParam.mfx.NumRefFrame, 9},
        {},
        {},
        {},
        {}
    },
    {/*07*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", "forBehaviorTest/foreman_cif.yuv", 1, 0,
        {},
        {},
        {},
        {&tsStruct::mfxExtFEIH265Param.MaxCUSize, 0},
        {},
        {}
    },
    {/*08*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", "forBehaviorTest/foreman_cif.yuv", 1, 0,
        {},
        {},
        {},
        {&tsStruct::mfxExtFEIH265Param.MPMode, 0},
        {},
        {}
    },
    {/*09*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", "forBehaviorTest/foreman_cif.yuv", 1, 0,
        {},
        {},
        {},
        {&tsStruct::mfxExtFEIH265Param.NumIntraModes, 4},
        {},
        {}
    },
    {/*10*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", "forBehaviorTest/foreman_cif.yuv", 1, 0,
        {},
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY},
        {},
        {},
        {},
        {}
    },
    {/*11*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", "forBehaviorTest/foreman_cif.yuv", 1, 0,
        {},
        {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY},
        {},
        {},
        {},
        {}
    },
    {/*12*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", "forBehaviorTest/foreman_cif.yuv", 1, 0,
        {},
        {{&tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
         {&tsStruct::mfxVideoParam.mfx.QPI, 21},
         {&tsStruct::mfxVideoParam.mfx.QPB, 23},
         {&tsStruct::mfxVideoParam.mfx.QPB, 24}
         },
        {},
        {},
        {},
        {}
    },
    {/*13*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", "forBehaviorTest/foreman_cif.yuv", 1, 0,
        {},
        {{&tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
         {&tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0},
         {&tsStruct::mfxVideoParam.mfx.TargetKbps, 500},
         {&tsStruct::mfxVideoParam.mfx.MaxKbps, 0}
         },
        {},
        {},
        {},
        {}
    },
    {/*14*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", "forBehaviorTest/foreman_cif.yuv", 1, 0,
        {},
        {{&tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
         {&tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0},
         {&tsStruct::mfxVideoParam.mfx.TargetKbps, 500},
         {&tsStruct::mfxVideoParam.mfx.MaxKbps, 600}
         },
        {},
        {},
        {},
        {}
    },
    {/*15*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", "forBehaviorTest/foreman_cif.yuv", 1, 0,
        {},
        {&tsStruct::mfxVideoParam.mfx.NumSlice, 4},
        {},
        {},
        {},
        {}
    },
    {/*16*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", "forBehaviorTest/foreman_cif.yuv", 1, 0,
        {},
        {&tsStruct::mfxVideoParam.AsyncDepth, 4},
        {},
        {},
        {},
        {}
    },
    {/*17*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", "forBehaviorTest/foster_720x576.yuv", 1, 0,
        {},
        {{&tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 720},
         {&tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 576}},
        {},
        {},
        {},
        {}
    },
    {/*18*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, "forBehaviorTest/foster_720x576.yuv", "forBehaviorTest/foreman_cif.yuv", 1, 0,
        {{&tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 720},
         {&tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 576}},
        {},
        {},
        {},
        {},
        {}
    }

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    mfxU32 encframes = 0;
    std::string file_name;


    MFXInit();

    g_tsPlugin.Reg(MFX_PLUGINTYPE_VIDEO_ENC, MFX_CODEC_HEVC, MFX_PLUGINID_HEVCE_FEI_HW);
    m_uid = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_ENC, MFX_CODEC_HEVC);
    m_loaded = false;
    Load();

    // set defoult parameters
    m_pPar->mfx.FrameInfo.Width = 352;
    m_pPar->mfx.FrameInfo.Height = 288;
    m_pPar->mfx.FrameInfo.CropW = 0;
    m_pPar->mfx.FrameInfo.CropH = 0;
    m_pPar->mfx.NumRefFrame = 5;
    m_pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_YV12;

    m_ENCInput = new mfxENCInput();
    m_ENCOutput = new mfxENCOutput();

    mfxExtFEIH265Param& FEIParam = m_par;
    FEIParam.MaxCUSize = 16;
    FEIParam.MPMode = 1;
    FEIParam.NumIntraModes = 1;

    mfxExtFEIH265Input In = {0};
    In.Header.BufferId = MFX_EXTBUFF_FEI_H265_INPUT;
    In.FEIOp = MFX_FEI_H265_OP_NOP;
    In.FrameType = MFX_FRAMETYPE_I;
    In.RefIdx = m_pPar->mfx.NumRefFrame - 1;

    mfxExtFEIH265Output Out = {0};
    Out.Header.BufferId = MFX_EXTBUFF_FEI_H265_OUTPUT;
    mfxFEIH265Output tmp_out = {0};
    Out.feiOut = &tmp_out;

    m_ENCInput->NumExtParam = 1;
    m_ENCInput->ExtParam = new mfxExtBuffer*[m_ENCInput->NumExtParam];
    m_ENCInput->ExtParam[0] = new mfxExtBuffer();
    m_ENCInput->ExtParam[0] = (mfxExtBuffer*)&In;

    m_ENCOutput->NumExtParam = 1;
    m_ENCOutput->ExtParam = new mfxExtBuffer*[m_ENCOutput->NumExtParam];
    m_ENCOutput->ExtParam[0] = new mfxExtBuffer();
    m_ENCOutput->ExtParam[0] = (mfxExtBuffer*)&Out;

    // set init parameters
    for(mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par_init[i].f)
        {
            tsStruct::set(m_pPar, *tc.set_par_init[i].f, tc.set_par_init[i].v);
        }
        if((i < 3) && (tc.in_fei_init[i].f))
        {
            tsStruct::set(&FEIParam, *tc.in_fei_init[i].f, tc.in_fei_init[i].v);
        }
        if((i < 3) && (tc.in_init[i].f))
        {
            tsStruct::set(&In, *tc.in_init[i].f, tc.in_init[i].v);
        }
    }

    if(tc.mode != NOT_INIT)
    {
        g_tsStatus.expect(tc.sts_init);
        Init(m_session, m_pPar);
        if (tc.sts_init >= MFX_ERR_NONE)
        {
            //set surfce processor
            file_name = g_tsStreamPool.Get(tc.file_init);
            tsRawReader SurfProc_Init = tsRawReader(file_name.c_str(), m_pPar->mfx.FrameInfo, tc.nframes);
            g_tsStreamPool.Reg();
            m_filler = &SurfProc_Init;

            for(;;)
            {
                g_tsStatus.expect(tc.sts_init_e);
                ProcessFrameAsync();
                if (g_tsStatus.get() == MFX_ERR_NONE)
                    SyncOperation();
                else if (g_tsStatus.get() == MFX_ERR_MORE_DATA)
                    continue;
                else
                {
                    if (tc.sts_init_e == MFX_ERR_NONE)
                    {
                        g_tsLog<<"FILED ProcessEncode return wrong ststus: "<<g_tsStatus.get()<<"\n";
                        throw tsFAIL;
                    }
                    else
                    {
                        g_tsStatus.expect(MFX_ERR_NONE);
                        SyncOperation();
                    }
                }
                encframes++;
                if (encframes >= tc.nframes)
                    break;
            }

            if (tc.mode == CLOSED)
            {
                g_tsStatus.expect(MFX_ERR_NONE);
                Close();
            }
        }

    }


    // set defoult reset parameters
    m_pPar->mfx.FrameInfo.Width = 352;
    m_pPar->mfx.FrameInfo.Height = 288;
    m_pPar->mfx.FrameInfo.CropW = 0;
    m_pPar->mfx.FrameInfo.CropH = 0;
    m_pPar->mfx.NumRefFrame = 5;
    m_pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_YV12;


    FEIParam.MaxCUSize = 16;
    FEIParam.MPMode = 1;
    FEIParam.NumIntraModes = 1;

    In.FEIOp = MFX_FEI_H265_OP_NOP;
    In.FrameType = MFX_FRAMETYPE_I;
    In.RefIdx = m_pPar->mfx.NumRefFrame - 1;


    //set reset parameters
    for(mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par_reset[i].f)
        {
            tsStruct::set(m_pPar, *tc.set_par_reset[i].f, tc.set_par_reset[i].v);
        }
        if((i < 3) && (tc.in_fei_reset[i].f))
        {
            tsStruct::set(&FEIParam, *tc.in_fei_reset[i].f, tc.in_fei_reset[i].v);
        }
        if((i < 3) && (tc.in_reset[i].f))
        {
            tsStruct::set(&In, *tc.in_reset[i].f, tc.in_reset[i].v);
        }
    }


    g_tsStatus.expect(tc.sts_reset);
    Reset();

    if(tc.sts_reset == MFX_ERR_NONE)
    {
        g_tsStreamPool.Init(0);
        file_name = g_tsStreamPool.Get(tc.file_reset);
        tsRawReader SurfProc_Reset  = tsRawReader(file_name.c_str(), m_pPar->mfx.FrameInfo, tc.nframes);
        g_tsStreamPool.Reg();
        m_filler = &SurfProc_Reset;

        for(;;)
        {
            g_tsStatus.expect(tc.sts_reset_e);
            ProcessFrameAsync();
            if (g_tsStatus.get() == MFX_ERR_NONE)
                SyncOperation();
            else if (g_tsStatus.get() == MFX_ERR_MORE_DATA)
                continue;
            else
            {
                if (tc.sts_reset_e == MFX_ERR_NONE)
                {
                    g_tsLog<<"FILED ProcessEncode return wrong ststus: "<<g_tsStatus.get()<<"\n";
                    throw tsFAIL;
                }
                else
                {
                    g_tsStatus.expect(MFX_ERR_NONE);
                    SyncOperation();
                }
            }
            encframes++;
            if (encframes >= tc.nframes)
                break;
        }
    }

    if (tc.sts_reset == MFX_ERR_NONE)
    {
        g_tsStatus.expect(MFX_ERR_NONE);
        Close();
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_gaa_reset);
};