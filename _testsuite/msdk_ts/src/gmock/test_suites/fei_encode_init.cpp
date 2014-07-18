#include "ts_encoder.h"
#include "ts_struct.h"

namespace fei_encode_init
{

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_FEI_FUNCTION_ENCPAK, MFX_CODEC_AVC, true) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    struct tc_par;
    static const mfxU32 n_par = 6;

    enum
    {
        NULL_SESSION = 1,
        NULL_PARAMS,
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair 
        {
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[n_par];
    };

    static const tc_struct test_case[];
};


const TestSuite::tc_struct TestSuite::test_case[] = 
{
    //{/*00*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION},
    //{/*01*/ MFX_ERR_NULL_PTR, NULL_PARAMS},

    // IOPattern cases
    //{/*02*/ MFX_ERR_NONE, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY}},
    {/*03*/ MFX_ERR_NONE, 0, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}},

    // RateControlMethods
    {/*04*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
                              {&tsStruct::mfxVideoParam.mfx.QPI, 21},
                              {&tsStruct::mfxVideoParam.mfx.QPB, 23},
                              {&tsStruct::mfxVideoParam.mfx.QPB, 24}}},
    {/*05*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
                              {&tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0},
                              {&tsStruct::mfxVideoParam.mfx.TargetKbps, 500},
                              {&tsStruct::mfxVideoParam.mfx.MaxKbps, 0}}},
    {/*06*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
                              {&tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0},
                              {&tsStruct::mfxVideoParam.mfx.TargetKbps, 500},
                              {&tsStruct::mfxVideoParam.mfx.MaxKbps, 600}}},
    {/*07*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR},
                              {&tsStruct::mfxVideoParam.mfx.Accuracy, 5},
                              {&tsStruct::mfxVideoParam.mfx.TargetKbps, 500},
                              {&tsStruct::mfxVideoParam.mfx.Convergence, 1}}},
//    {/*00*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ},
//                              {&tsStruct::mfxVideoParam.mfx.QPI, 0},
//                              {&tsStruct::mfxVideoParam.mfx.ICQQuality, 25},
//                              {&tsStruct::mfxVideoParam.mfx.QPB, 0}}},
//    {/*00*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
//                              {&tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0},
//                              {&tsStruct::mfxVideoParam.mfx.TargetKbps, 500},
//                              {&tsStruct::mfxVideoParam.mfx.MaxKbps, 0}}},
    //{/*00*/ MFX_ERR_NONE, 0, {&tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ}},
    //{/*00*/ MFX_ERR_NONE, 0, {&tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VCM}},

    {/*00*/ MFX_ERR_NONE, 0, {&tsStruct::mfxVideoParam.mfx.NumSlice, 4}},
    {/*00*/ MFX_ERR_NONE, 0, {&tsStruct::mfxVideoParam.AsyncDepth, 4}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    if (!(tc.mode & NULL_SESSION))
    {
        MFXInit();
    }

    // set up parameters for case
    for(mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par[i].f)
        {
            tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
        }
    }

    bool hw_surf = m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY;

    if (!(tc.mode & NULL_SESSION))
    {
        UseDefaultAllocator(!hw_surf);
        SetFrameAllocator(m_session, GetAllocator());

        // set handle
        mfxHDL hdl;
        mfxHandleType type;
        if (hw_surf)
        {
            GetAllocator()->get_hdl(type, hdl);
        }
        else
        {
            tsSurfacePool alloc;
            alloc.UseDefaultAllocator(false);
            alloc.GetAllocator()->get_hdl(type, hdl);
        }

        SetHandle(m_session, type, hdl);
    }

    ///////////////////////////////////////////////////////////////////////////
    mfxVideoParam orig_par;
    memcpy(&orig_par, m_pPar, sizeof(mfxVideoParam));
    g_tsStatus.expect(tc.sts);
    Init(m_session, m_pPar);

    EXPECT_EQ(0, memcmp(&orig_par, m_pPar, sizeof(mfxVideoParam)))
        << "ERROR: Input parameters must not be changed in Init()";

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_init);
};
