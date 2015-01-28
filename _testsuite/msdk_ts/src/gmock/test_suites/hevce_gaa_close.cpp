#include "ts_encoder.h"
#include "ts_struct.h"
#include "mfxplugin.h"
#include "ts_enc.h"


namespace hevce_gaa_close
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
        NOT_INIT     = 2,
        FAILED_INIT   = 3,
        CLOSED       = 4,
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
    
    {/*00*/ MFX_ERR_NONE, 0},
    {/*01*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION},
    {/*02*/ MFX_ERR_NOT_INITIALIZED, NOT_INIT},
    {/*03*/ MFX_ERR_NONE, FAILED_INIT},
    {/*04*/ MFX_ERR_NOT_INITIALIZED, CLOSED},
    
   
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    mfxStatus sts = MFX_ERR_NONE;

    
    MFXInit();
    
    g_tsPlugin.Reg(MFX_PLUGINTYPE_VIDEO_ENC, MFX_CODEC_HEVC, MFX_PLUGINID_HEVCE_FEI_HW);
    m_uid = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_ENC, MFX_CODEC_HEVC);
    m_loaded = false;
    Load();
    
    // set defoult parameters
    m_pPar->mfx.FrameInfo.Width = 352;
    m_pPar->mfx.FrameInfo.Height = 288;
    m_pPar->mfx.NumRefFrame = 5;
   
    mfxExtFEIH265Param& FEIParam = m_par;
    FEIParam.MaxCUSize = 16;
    FEIParam.MPMode = 1;
    FEIParam.NumIntraModes = 1;
    
    m_ENCInput = new mfxENCInput();
    m_ENCOutput = new mfxENCOutput();
    
    mfxExtFEIH265Input In = {0};
    In.Header.BufferId = MFX_EXTBUFF_FEI_H265_INPUT;
    In.FEIOp = MFX_FEI_H265_OP_NOP;
    In.FrameType = MFX_FRAMETYPE_I;
    In.RefIdx = m_pPar->mfx.NumRefFrame - 1;

    mfxExtFEIH265Output Out = {0};
    Out.Header.BufferId = MFX_EXTBUFF_FEI_H265_OUTPUT;
    Out.feiOut = new mfxFEIH265Output;

    m_ENCInput->NumExtParam = 1;
    m_ENCInput->ExtParam = new mfxExtBuffer*[m_ENCInput->NumExtParam];
    m_ENCInput->ExtParam[0] = new mfxExtBuffer();
    m_ENCInput->ExtParam[0] = (mfxExtBuffer*)&In;

    m_ENCOutput->NumExtParam = 1;
    m_ENCOutput->ExtParam = new mfxExtBuffer*[m_ENCOutput->NumExtParam];
    m_ENCOutput->ExtParam[0] = new mfxExtBuffer();
    m_ENCOutput->ExtParam[0] = (mfxExtBuffer*)&Out;
    
    // set up parameters for case
    for(mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par[i].f)
        {
            tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
        }
    }
    sts = MFX_ERR_NONE;
    if (tc.mode == FAILED_INIT)
    {
        sts = MFX_ERR_NULL_PTR;
        m_pPar->NumExtParam = 0;
        m_pPar->ExtParam = NULL;
    }
    if(tc.mode != NOT_INIT)
    {
        g_tsStatus.expect(sts);
        Init(m_session, m_pPar);
        if (sts == MFX_ERR_NONE)
        {
            for(;;)
            {
                 g_tsStatus.expect(MFX_ERR_NONE);
                 ProcessFrameAsync();
                 if (g_tsStatus.get() == MFX_ERR_NONE)
                 {  
                     SyncOperation();
                     break;
                 }
            }
        }
    }
    if (tc.mode == CLOSED)
    {
        sts = MFX_ERR_NONE;
        g_tsStatus.expect(sts);
        Close();
    }
        
    g_tsStatus.expect(tc.sts);
    if (tc.mode == NULL_SESSION)
    {
        Close(NULL);
    }
    else
    {
        Close();
    }
   
    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_gaa_close);
};