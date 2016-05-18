/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2016 Intel Corporation. All Rights Reserved.

File Name: qet_plugin.cpp

\* ****************************************************************************** */
#include "ts_session.h"
#include "ts_struct.h"

namespace get_plugin
{

#define MAX_ACTIONS 10

const mfxPluginUID  MFX_PLUGINID_NONE = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

FakeDecoder plg_dec;
FakeEnc plg_enc;
FakeEncoder plg_encod;
FakeVPP plg_vpp;

mfxStatus CheckRegister(mfxSession session, mfxU32 type)
{
    std::vector<mfxU32> calls;

    switch (type)
    {
        case MFX_PLUGINTYPE_VIDEO_DECODE:
        {
            mfxVideoParam video_par;
            video_par.mfx.CodecId = 0;
            mfxFrameAllocRequest frameAllocReq_par;
            mfxThreadTask task_par = NULL;
            mfxU64 ts_par = 1;
            mfxPayload payload_par;
            mfxBitstream bs_par;

            MFXPluginAdapter<MFXDecoderPlugin> adapter(&plg_dec);
            TRACE_FUNC3(MFXVideoUSER_Register, session, type, (mfxPlugin*)&adapter);
            g_tsStatus.check(MFXVideoUSER_Register(session, type, (mfxPlugin*)&adapter));

            plg_dec.m_calls.clear();


            MFXVideoDECODE_Init(session, &video_par);
            calls.push_back(APIfuncs::Init);
            MFXVideoDECODE_QueryIOSurf(session, &video_par, &frameAllocReq_par);
            calls.push_back(APIfuncs::QueryIOSurf);
            MFXVideoDECODE_Query(session, &video_par, &video_par);
            calls.push_back(APIfuncs::Query);
            MFXVideoDECODE_Reset(session, &video_par);
            calls.push_back(APIfuncs::Reset);
            MFXVideoDECODE_GetVideoParam(session, &video_par);
            calls.push_back(APIfuncs::GetVideoParam);
            MFXVideoDECODE_Close(session);
            calls.push_back(APIfuncs::Close);

            MFXVideoDECODE_DecodeHeader(session, &bs_par, &video_par);
            calls.push_back(APIfuncs::DecodeHeader);

            MFXVideoDECODE_GetPayload(session, &ts_par, &payload_par);
            calls.push_back(APIfuncs::GetPayload);

            if (!check_calls(calls, plg_dec.m_calls))
            {
                g_tsLog << "ERROR: Fail check plugin's functions!\n";
                return MFX_ERR_ABORTED;
            }
            break;
        }
        case MFX_PLUGINTYPE_VIDEO_ENC:
        {
            mfxVideoParam video_par;
            video_par.mfx.CodecId = 0;
            mfxFrameAllocRequest frameAllocReq_par;

            MFXPluginAdapter<MFXEncPlugin> adapter(&plg_enc);
            TRACE_FUNC3(MFXVideoUSER_Register, session, type, (mfxPlugin*)&adapter);
            g_tsStatus.check(MFXVideoUSER_Register(session, type, (mfxPlugin*)&adapter));

            plg_enc.m_calls.clear();

            MFXVideoENC_Init(session, &video_par);
            calls.push_back(APIfuncs::Init);
            MFXVideoENC_QueryIOSurf(session, &video_par, &frameAllocReq_par);
            calls.push_back(APIfuncs::QueryIOSurf);
            MFXVideoENC_Query(session, &video_par, &video_par);
            calls.push_back(APIfuncs::Query);
            MFXVideoENC_Reset(session, &video_par);
            calls.push_back(APIfuncs::Reset);
            MFXVideoENC_GetVideoParam(session, &video_par);
            calls.push_back(APIfuncs::GetVideoParam);
            MFXVideoENC_Close(session);
            calls.push_back(APIfuncs::Close);

            if (!check_calls(calls, plg_enc.m_calls))
            {
                g_tsLog << "ERROR: Fail check plugin's functions!\n";
                return MFX_ERR_ABORTED;
            }
            break;
        }
        case MFX_PLUGINTYPE_VIDEO_ENCODE:
        {
            mfxVideoParam video_par;
            video_par.mfx.CodecId = 0;
            mfxFrameAllocRequest frameAllocReq_par;

            MFXPluginAdapter<MFXEncoderPlugin> adapter(&plg_encod);
            TRACE_FUNC3(MFXVideoUSER_Register, session, type, (mfxPlugin*)&adapter);
            g_tsStatus.check(MFXVideoUSER_Register(session, type, (mfxPlugin*)&adapter));

            plg_encod.m_calls.clear();

            MFXVideoENCODE_Init(session, &video_par);
            calls.push_back(APIfuncs::Init);
            MFXVideoENCODE_QueryIOSurf(session, &video_par, &frameAllocReq_par);
            calls.push_back(APIfuncs::QueryIOSurf);
            MFXVideoENCODE_Query(session, &video_par, &video_par);
            calls.push_back(APIfuncs::Query);
            MFXVideoENCODE_Reset(session, &video_par);
            calls.push_back(APIfuncs::Reset);
            MFXVideoENCODE_GetVideoParam(session, &video_par);
            calls.push_back(APIfuncs::GetVideoParam);
            MFXVideoENCODE_Close(session);
            calls.push_back(APIfuncs::Close);

            if (!check_calls(calls, plg_encod.m_calls))
            {
                g_tsLog << "ERROR: Fail check plugin's functions!\n";
                return MFX_ERR_ABORTED;
            }
            break;
        }
        case MFX_PLUGINTYPE_VIDEO_VPP:
        {
            mfxVideoParam video_par;
            video_par.mfx.CodecId = 0;
            mfxFrameAllocRequest frameAllocReq_par;

            MFXPluginAdapter<MFXVPPPlugin> adapter(&plg_vpp);
            TRACE_FUNC3(MFXVideoUSER_Register, session, type, (mfxPlugin*)&adapter);
            g_tsStatus.check(MFXVideoUSER_Register(session, type, (mfxPlugin*)&adapter));

            plg_vpp.m_calls.clear();

            MFXVideoVPP_Init(session, &video_par);
            calls.push_back(APIfuncs::Init);
            MFXVideoVPP_QueryIOSurf(session, &video_par, &frameAllocReq_par);
            calls.push_back(APIfuncs::QueryIOSurf);
            MFXVideoVPP_Query(session, &video_par, &video_par);
            calls.push_back(APIfuncs::Query);
            MFXVideoVPP_Reset(session, &video_par);
            calls.push_back(APIfuncs::Reset);
            MFXVideoVPP_GetVideoParam(session, &video_par);
            calls.push_back(APIfuncs::GetVideoParam);
            MFXVideoVPP_Close(session);
            calls.push_back(APIfuncs::Close);

            if (!check_calls(calls, plg_vpp.m_calls))
            {
                g_tsLog << "ERROR: Fail check plugin's functions!\n";
                return MFX_ERR_ABORTED;
            }
            break;
        }
        default:
        {
            g_tsLog << "ERROR: Unknown plugin type!\n";
            return MFX_ERR_ABORTED;
        }
    }
    return MFX_ERR_NONE;
}


mfxStatus Check(mfxPlugin plugin)
{
    if (!plugin.pthis)
    {
        g_tsLog << "ERROR: NULL plugin.pthis!\n";
        return MFX_ERR_NULL_PTR;
    }
    if ((!plugin.PluginInit) || (!plugin.PluginClose) || (!plugin.GetPluginParam) || (!plugin.FreeResources) || (!plugin.Execute))
    {
        g_tsLog << "ERROR: NULL pointer to plugin's function!\n";
        return MFX_ERR_NULL_PTR;
    }
    if ((!plugin.Video) || (!plugin.Audio))
    {
        g_tsLog << "ERROR: NULL codec!\n";
        return MFX_ERR_NULL_PTR;
    }
    return MFX_ERR_NONE;
}

class TestSuite : tsSession
{
public:
    TestSuite() : tsSession() {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 n_par = 5;

    enum
    {
        LOAD = 1,
        UNLOAD,
        REGISTER,
        UNREGISTER,
        GET
    };

    struct action
    {
        mfxU16 type;
        mfxPluginUID uid;
        mfxStatus sts;
        mfxU32 plugin_type;
    };

    struct tc_struct
    {
        mfxU16 num;
        action actions[MAX_ACTIONS];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 00*/ 3, {{LOAD, MFX_PLUGINID_HEVCD_SW}, {GET, MFX_PLUGINID_HEVCD_SW, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}, {UNLOAD, MFX_PLUGINID_HEVCD_SW}}},
    {/* 01*/ 5, {{LOAD, MFX_PLUGINID_HEVCD_SW}, {UNLOAD, MFX_PLUGINID_HEVCD_SW}, {LOAD, MFX_PLUGINID_HEVCD_SW}, {GET, MFX_PLUGINID_HEVCD_SW, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}, {UNLOAD, MFX_PLUGINID_HEVCD_SW}}},
    {/* 02*/ 3, {{LOAD, MFX_PLUGINID_HEVCD_HW}, {GET, MFX_PLUGINID_HEVCD_HW, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}, {UNLOAD, MFX_PLUGINID_HEVCD_HW}}},
    {/* 03*/ 5, {{LOAD, MFX_PLUGINID_HEVCD_HW}, {UNLOAD, MFX_PLUGINID_HEVCD_HW}, {LOAD, MFX_PLUGINID_HEVCD_HW}, {GET, MFX_PLUGINID_HEVCD_HW, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}, {UNLOAD, MFX_PLUGINID_HEVCD_HW}}},
    {/* 04*/ 3, {{LOAD, MFX_PLUGINID_HEVCE_FEI_HW}, {GET, MFX_PLUGINID_HEVCE_FEI_HW, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENC}, {UNLOAD, MFX_PLUGINID_HEVCE_FEI_HW}}},
    {/* 05*/ 5, {{LOAD, MFX_PLUGINID_HEVCE_FEI_HW}, {UNLOAD, MFX_PLUGINID_HEVCE_FEI_HW}, {LOAD, MFX_PLUGINID_HEVCE_FEI_HW}, {GET, MFX_PLUGINID_HEVCE_FEI_HW, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENC}, {UNLOAD, MFX_PLUGINID_HEVCE_FEI_HW}}},
    {/* 06*/ 9, {{LOAD, MFX_PLUGINID_HEVCE_HW}, {LOAD, MFX_PLUGINID_HEVCD_HW}, {LOAD, MFX_PLUGINID_HEVCE_FEI_HW},
                 {GET, MFX_PLUGINID_HEVCE_HW, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENCODE}, {GET, MFX_PLUGINID_HEVCD_HW, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}, {GET, MFX_PLUGINID_HEVCE_FEI_HW, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENC},
                 {UNLOAD, MFX_PLUGINID_HEVCE_HW}, {UNLOAD, MFX_PLUGINID_HEVCD_HW}, {UNLOAD, MFX_PLUGINID_HEVCE_FEI_HW}}
    },
    {/* 07*/ 3, {{REGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}, {GET, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}, {UNREGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}}},
    {/* 08*/ 5, {{REGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}, {UNREGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}, {REGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}, {GET, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}, {UNREGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}}},
    {/* 09*/ 3, {{REGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENCODE}, {GET, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENCODE}, {UNREGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENCODE}}},
    {/* 10*/ 5, {{REGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENCODE}, {UNREGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENCODE}, {REGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENCODE}, {GET, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENCODE}, {UNREGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENCODE}}},
    {/* 11*/ 3, {{REGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENC}, {GET, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENC}, {UNREGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENC}}},
    {/* 12*/ 5, {{REGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENC}, {UNREGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENC}, {REGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENC}, {GET, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENC}, {UNREGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENC}}},
    {/* 13*/ 3, {{REGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_VPP}, {GET, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_VPP}, {UNREGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_VPP}}},
    {/* 14*/ 5, {{REGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_VPP}, {UNREGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_VPP}, {REGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_VPP}, {GET, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_VPP}, {UNREGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_VPP}}},
    {/* 15*/ 9, {
                    {REGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENCODE}, {REGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}, {REGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_VPP},
                    {GET, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENCODE}, {GET, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}, {GET, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_VPP},
                    {UNREGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_ENCODE}, {UNREGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}, {UNREGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_VPP}
                 }
    },
    //bad type
    {/* 16*/ 3, {{LOAD, MFX_PLUGINID_HEVCD_HW}, {GET, MFX_PLUGINID_HEVCD_HW, MFX_ERR_UNDEFINED_BEHAVIOR, MFX_PLUGINTYPE_VIDEO_ENCODE}, {UNLOAD, MFX_PLUGINID_HEVCD_HW}}},
    {/* 17*/ 3, {{REGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}, {GET, MFX_PLUGINID_NONE, MFX_ERR_UNDEFINED_BEHAVIOR, MFX_PLUGINTYPE_VIDEO_ENCODE}, {UNREGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}}},
    // not loaded
    {/* 18*/ 1, {{GET, MFX_PLUGINID_HEVCD_HW, MFX_ERR_UNDEFINED_BEHAVIOR, MFX_PLUGINTYPE_VIDEO_DECODE}}},
    //get after unload
    {/* 19*/ 4, {{LOAD, MFX_PLUGINID_HEVCD_HW}, {GET, MFX_PLUGINID_HEVCD_HW, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}, {UNLOAD, MFX_PLUGINID_HEVCD_HW}, {GET, MFX_PLUGINID_HEVCD_HW, MFX_ERR_UNDEFINED_BEHAVIOR, MFX_PLUGINTYPE_VIDEO_DECODE}}},
    {/* 20*/ 4, {{REGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}, {GET, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}, {UNREGISTER, MFX_PLUGINID_NONE, MFX_ERR_NONE, MFX_PLUGINTYPE_VIDEO_DECODE}, {GET, MFX_PLUGINID_NONE, MFX_ERR_UNDEFINED_BEHAVIOR, MFX_PLUGINTYPE_VIDEO_DECODE}}},

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START
    const tc_struct& tc = test_case[id];
    mfxPlugin plugin;


    //int
    tsSession session(g_tsImpl, g_tsVersion);
    session.MFXInit();


    for (mfxU16 i = 0; i < tc.num; i++)
    {
        switch (tc.actions[i].type)
        {
        case LOAD:
            {
                Load(session.m_session, &tc.actions[i].uid,1);
                break;
            }
        case UNLOAD:
            {
                UnLoad(session.m_session, &tc.actions[i].uid);
                break;
            }
        case REGISTER:
            {
                g_tsStatus.check(CheckRegister(session.m_session, tc.actions[i].plugin_type));
                break;
            }
        case UNREGISTER:
            {
                TRACE_FUNC2(MFXVideoUSER_Unregister, session.m_session, tc.actions[i].plugin_type);
                g_tsStatus.check(MFXVideoUSER_Unregister(session.m_session, tc.actions[i].plugin_type));
                break;
            }
        case GET:
            {
                g_tsStatus.expect(tc.actions[i].sts);
                session.GetPlugin(session.m_session, tc.actions[i].plugin_type, &plugin);
                //check
                g_tsStatus.expect(MFX_ERR_NONE);
                if (tc.actions[i].sts == MFX_ERR_NONE)
                    g_tsStatus.check(Check(plugin));
                break;
            }
        default:
            break;
        }

    }

    TS_END
    return 0;
}

TS_REG_TEST_SUITE_CLASS(get_plugin);

}