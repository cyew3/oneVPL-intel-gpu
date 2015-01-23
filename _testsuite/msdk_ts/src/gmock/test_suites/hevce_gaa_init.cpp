#include "ts_encoder.h"
#include "ts_struct.h"
#include "mfxplugin.h"
#include "ts_enc.h"


namespace hevce_gaa_init
{


class TestSuite : tsVideoENC
{
public:
    TestSuite() : tsVideoENC(MFX_CODEC_HEVC, 0) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    struct tc_par;
    static const mfxU32 n_par = 6;

    enum
    {
        NULL_SESSION = 1,
        NULL_PARAMS  = 2,
        NULL_EXTBUFF = 3,
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        mfxU32 set_alloc;
        struct f_pair 
        {
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[n_par];
        f_pair in_fei[3];
    };

    static const tc_struct test_case[];
};


const TestSuite::tc_struct TestSuite::test_case[] = 
{
    
    
    {/*00*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION},
    {/*01*/ MFX_ERR_NULL_PTR, NULL_PARAMS},
    // IOPattern cases
    {/*02*/ MFX_ERR_NONE, 0, 1, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}},
    {/*03*/ MFX_ERR_NONE, 0, 1, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}},
    // RateControlMethods
    {/*04*/ MFX_ERR_NONE, 0, 1, {{&tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
                                 {&tsStruct::mfxVideoParam.mfx.QPI, 21},
                                 {&tsStruct::mfxVideoParam.mfx.QPB, 23},
                                 {&tsStruct::mfxVideoParam.mfx.QPB, 24}}},
    {/*05*/ MFX_ERR_NONE, 0, 1, {{&tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
                                 {&tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0},
                                 {&tsStruct::mfxVideoParam.mfx.TargetKbps, 500},
                                 {&tsStruct::mfxVideoParam.mfx.MaxKbps, 0}}},
    {/*06*/ MFX_ERR_NONE, 0, 1, {{&tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
                                 {&tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0},
                                 {&tsStruct::mfxVideoParam.mfx.TargetKbps, 500},
                                 {&tsStruct::mfxVideoParam.mfx.MaxKbps, 600}}},

    {/*07*/ MFX_ERR_NONE, 0, 1, {&tsStruct::mfxVideoParam.mfx.NumSlice, 4}},
    {/*08*/ MFX_ERR_NONE, 0, 1, {&tsStruct::mfxVideoParam.AsyncDepth, 4}},
    //tests for incorrect required parameters
    {/*09*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, {&tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0}},
    {/*10*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, {&tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0}},
    {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, {&tsStruct::mfxVideoParam.mfx.NumRefFrame, 9}}, // MFX_FEI_H265_MAX_NUM_REF_FRAMES == 8
    {/*12*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, {}, {&tsStruct::mfxExtFEIH265Param.MaxCUSize, 0}},
    {/*13*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, {}, {&tsStruct::mfxExtFEIH265Param.MPMode, 0}},
    {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, {}, {&tsStruct::mfxExtFEIH265Param.NumIntraModes, 4}},
    {/*15*/ MFX_ERR_NULL_PTR, NULL_EXTBUFF},
    

   
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();
    
    g_tsPlugin.Reg(MFX_PLUGINTYPE_VIDEO_ENC, MFX_CODEC_HEVC, MFX_PLUGINID_HEVCE_FEI_HW);
    m_uid = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_ENC, MFX_CODEC_HEVC);
    m_loaded = false;
    Load();
    
    // set default parameters
    m_pPar->mfx.FrameInfo.Width = 352;
    m_pPar->mfx.FrameInfo.Height = 288;
    m_pPar->mfx.NumRefFrame = 5;
   
    mfxExtFEIH265Param& FEIParam = m_par;
    FEIParam.MaxCUSize = 16;
    FEIParam.MPMode = 1;
    FEIParam.NumIntraModes = 1;
    
   
    // set up parameters for case
    for(mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par[i].f)
        {
            tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
        }
        if((i < 3) && (tc.in_fei[i].f))
        {
            tsStruct::set(&FEIParam, *tc.in_fei[i].f, tc.in_fei[i].v);
        }
    }
    
    
    if (tc.mode == NULL_EXTBUFF)
    {
        m_pPar->NumExtParam = 0;
        m_pPar->ExtParam = NULL;
    }
    
    g_tsStatus.expect(tc.sts);
    if (tc.mode == NULL_SESSION)
    {
        Init(NULL, m_pPar);
    } else if (tc.mode == NULL_PARAMS)
    {
        Init(m_session, NULL);
    }
    else
    {
        if(tc.set_alloc == 1)
            Init();
        else
            Init(m_session, m_pPar);
    }
    
    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_gaa_init);
};