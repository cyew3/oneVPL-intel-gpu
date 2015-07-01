#include "mfxplugin.h"
#include "ts_decoder.h"
#include "ts_encoder.h"
#include "ts_vpp.h"
#include "ts_struct.h"
#include "ts_plugin.h"


namespace plugin_load_unload
{
class TestSuite : tsSession{
private:
    enum
    {
        ENCODE = 1,
        DECODE = 2,
        VPP    = 4
    };

    enum
    {
        CHECK_CALLS = 1,
        REAL_PLUGINS
    };

    typedef struct
    {
        mfxU32 plugins;
        mfxU32 mode;
    }tc_struct;

    //function parametres
    mfxStatus sts; 
    mfxVideoParam* video_par;

    mfxCoreInterface* core_par;

    mfxPluginParam* plg_par;

    mfxThreadTask task_par; 

    mfxU32 uid_par;
    mfxU64* ts_par;

    mfxFrameAllocRequest* frameAllocReq_par; 

    mfxEncodeCtrl* ctrl_par;
    mfxFrameSurface1* surface_par;
    mfxFrameSurface1** surfacePtr_par; 

    mfxBitstream* bs_par;
    mfxThreadTask* taskPtr; 


    mfxPayload* payload_par;

    mfxExtVppAuxData* aux_par;

    int prm;
    void* p;

    static const tc_struct test_case[];

public:
    TestSuite() : tsSession() { }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    template<class mfxTPlugin, class FakeT>
    void call_func(mfxPluginType plgType, mfxTPlugin* pPlugin, FakeT fakePlg, std::vector<mfxU32> calls);

};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {CHECK_CALLS, DECODE},
    {CHECK_CALLS, ENCODE},
    {CHECK_CALLS, VPP},
    {CHECK_CALLS, DECODE|ENCODE},
    {CHECK_CALLS, DECODE|VPP},
    {CHECK_CALLS, ENCODE|VPP},
    {CHECK_CALLS, DECODE|ENCODE|VPP}
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

template<class mfxTPlugin, class FakeT>
void TestSuite::call_func(mfxPluginType plgType, mfxTPlugin* pPlugin, FakeT fakePlg, std::vector<mfxU32> calls)
{
    sts = MFXVideoUSER_Register(m_session, plgType, (mfxPlugin*)&pPlugin);

    //common
    fakePlg.Init(video_par);
    calls.push_back(APIfuncs::Init);

    fakePlg.QueryIOSurf(video_par, frameAllocReq_par, frameAllocReq_par);
    calls.push_back(APIfuncs::QueryIOSurf);

    fakePlg.Query(video_par, video_par);
    calls.push_back(APIfuncs::Query);

    fakePlg.Reset(video_par);
    calls.push_back(APIfuncs::Reset);

    fakePlg.GetVideoParam(video_par);
    calls.push_back(APIfuncs::GetVideoParam);

    fakePlg.PluginInit(core_par);
    calls.push_back(APIfuncs::PluginInit);

    fakePlg.GetPluginParam(plg_par);
    calls.push_back(APIfuncs::GetPluginParam);

    fakePlg.Execute(task_par, uid_par, uid_par);
    calls.push_back(APIfuncs::Execute);

    fakePlg.FreeResources(task_par, sts);
    calls.push_back(APIfuncs::FreeResources);

    fakePlg.SetAuxParams(p, prm);
    calls.push_back(APIfuncs::SetAuxParams);

    fakePlg.Release();
    calls.push_back(APIfuncs::Release);

    fakePlg.Close();
    calls.push_back(APIfuncs::Close);

    fakePlg.PluginClose();
    calls.push_back(APIfuncs::PluginClose);
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    tc_struct tc = test_case[id];
    MFXInit();

    if (tc.plugins == CHECK_CALLS)
    {
        if (tc.mode|ENCODE)
        {
            std::vector<mfxU32> calls_enc;
            MFXEncoderPlugin* pPlugin_enc = nullptr;
            FakeEncoder plg_enc;

            TestSuite::call_func(MFX_PLUGINTYPE_VIDEO_ENCODE, pPlugin_enc, plg_enc, calls_enc);

            plg_enc.EncodeFrameSubmit(ctrl_par, surface_par, bs_par, taskPtr);
            calls_enc.push_back(APIfuncs::EncodeFrameSubmit);

            if (!check_calls(calls_enc, plg_enc.m_calls))
            {
                g_tsStatus.check(MFX_ERR_ABORTED);
            }
        }
       if (tc.mode|DECODE)
       {
           std::vector<mfxU32> calls_dec;
           MFXDecoderPlugin* pPlugin_dec = nullptr;
           FakeDecoder plg_dec;

           TestSuite::call_func(MFX_PLUGINTYPE_VIDEO_DECODE, pPlugin_dec, plg_dec, calls_dec);

           plg_dec.DecodeHeader(bs_par, video_par);
           calls_dec.push_back(APIfuncs::DecodeHeader);

           plg_dec.GetPayload(ts_par, payload_par);
           calls_dec.push_back(APIfuncs::GetPayload);

           plg_dec.DecodeFrameSubmit(bs_par, surface_par, surfacePtr_par, taskPtr);
           calls_dec.push_back(APIfuncs::DecodeFrameSubmit);

           if (!check_calls(calls_dec, plg_dec.m_calls))
           {
               g_tsStatus.check(MFX_ERR_ABORTED);
           }

       }
       if (tc.mode|VPP)
       {
           std::vector<mfxU32> calls_vpp;
           MFXVPPPlugin* pPlugin_vpp = nullptr;
           FakeVPP plg_vpp;

           TestSuite::call_func(MFX_PLUGINTYPE_VIDEO_VPP, pPlugin_vpp, plg_vpp, calls_vpp);

           plg_vpp.VPPFrameSubmit(surface_par, surface_par, aux_par, taskPtr);
           calls_vpp.push_back(APIfuncs::VPPFrameSubmit);

           plg_vpp.VPPFrameSubmitEx(surface_par, surface_par, surfacePtr_par, taskPtr);
           calls_vpp.push_back(APIfuncs::VPPFrameSubmitEx);

           if (!check_calls(calls_vpp, plg_vpp.m_calls))
           {
               g_tsStatus.check(MFX_ERR_ABORTED);
           }
       }
    }
    /*else if (tc.mode == REAL_PLUGINS) {

    }*/

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(plugin_load_unload);
};
