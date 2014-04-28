#include "mfxplugin.h"
//#include "mfx_plugin_uids.h"
#include "ts_decoder.h"
#include "ts_encoder.h"
#include "ts_vpp.h"
//#include "ts_plugin.h"
#include "ts_struct.h"


namespace plugin_load_unload
{

int plugin_loaded[] = {0,0,0};

typedef struct cid_uid
{
        mfxU32        cid;
        mfxPluginUID  uid;
    
} tc_struct[3];

tc_struct test_case[] = 
{
    //load/unload one SW plugin 
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_SW}, {0, 0}, {0, 0} },
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCE_SW}, {0, 0}, {0, 0} },
    //load/unload one HW plugin (on SW platform plugin shuld not load)
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_HW}, {0, 0}, {0, 0} },
    { {MFX_CODEC_VP8, MFX_PLUGINID_VP8D_HW}, {0, 0}, {0, 0} },
    { {MFX_CODEC_VP8, MFX_PLUGINID_VP8E_HW}, {0, 0}, {0, 0} },
    { {MFX_MAKEFOURCC('P','T','I','R'), MFX_PLUGINID_ITELECINE_HW}, {0, 0}, {0, 0} },
//    { {MFX_CODEC_CAPTURE, MFX_PLUGINID_CAMERA_HW}, {0, 0}, {0, 0} },
//    { {MFX_CODEC_CAPTURE, MFX_PLUGINID_CAPTURE_HW}, {0, 0}, {0, 0} },
//    { {MFX_CODEC_AVC, MFX_PLUGINID_H264LA_HW}, {0, 0}, {0, 0} },
    //load two equal plugins (second plugin shuld not load)
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_SW}, {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_SW}, {0, 0} },
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_HW}, {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_HW}, {0, 0} },
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCE_SW}, {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCE_SW}, {0, 0} },
    { {MFX_CODEC_VP8, MFX_PLUGINID_VP8D_HW}, {MFX_CODEC_VP8, MFX_PLUGINID_VP8D_HW}, {0, 0} },
    { {MFX_CODEC_VP8, MFX_PLUGINID_VP8E_HW}, {MFX_CODEC_VP8, MFX_PLUGINID_VP8E_HW}, {0, 0} },
    { {MFX_MAKEFOURCC('P','T','I','R'), MFX_PLUGINID_ITELECINE_HW}, {MFX_MAKEFOURCC('P','T','I','R'), MFX_PLUGINID_ITELECINE_HW}, {0, 0} },
    //load two equal types of plugin (second plugin shuld not load)
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_HW}, {MFX_CODEC_VP8, MFX_PLUGINID_VP8D_HW}, {0, 0} },
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_SW}, {MFX_CODEC_VP8, MFX_PLUGINID_VP8D_HW}, {0, 0} },
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCE_SW}, {MFX_CODEC_VP8, MFX_PLUGINID_VP8E_HW}, {0, 0} },
    //load/unload two different plugins (on SW platform HW plugin shuld not load)
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_SW}, {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCE_SW}, {0, 0} },
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_SW}, {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCE_SW}, {0, 0} },
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_HW}, {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCE_SW}, {0, 0} },
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_HW}, {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCE_SW}, {0, 0} },
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_SW}, {MFX_CODEC_VP8, MFX_PLUGINID_VP8E_HW}, {0, 0} },
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_SW}, {MFX_CODEC_VP8, MFX_PLUGINID_VP8E_HW}, {0, 0} },
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_HW}, {MFX_CODEC_VP8, MFX_PLUGINID_VP8E_HW}, {0, 0} },
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_HW}, {MFX_CODEC_VP8, MFX_PLUGINID_VP8E_HW}, {0, 0} },
    //load/unload three different plugins (on SW platform HW plugin shuld not load)
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_SW}, {MFX_CODEC_HEVC,    MFX_PLUGINID_HEVCE_SW}, {MFX_MAKEFOURCC('P','T','I','R'), MFX_PLUGINID_ITELECINE_HW} },
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_SW}, {MFX_CODEC_HEVC,    MFX_PLUGINID_HEVCE_SW}, {MFX_MAKEFOURCC('P','T','I','R'), MFX_PLUGINID_ITELECINE_HW} },
    { {MFX_CODEC_VP8, MFX_PLUGINID_VP8D_HW}, {MFX_CODEC_HEVC,    MFX_PLUGINID_HEVCE_SW}, {MFX_MAKEFOURCC('P','T','I','R'), MFX_PLUGINID_ITELECINE_HW} },
    { {MFX_CODEC_VP8, MFX_PLUGINID_VP8D_HW}, {MFX_CODEC_HEVC,    MFX_PLUGINID_HEVCE_SW}, {MFX_MAKEFOURCC('P','T','I','R'), MFX_PLUGINID_ITELECINE_HW} },
    { {MFX_CODEC_VP8, MFX_PLUGINID_VP8D_HW}, {MFX_CODEC_VP8,    MFX_PLUGINID_VP8E_HW}, {MFX_MAKEFOURCC('P','T','I','R'), MFX_PLUGINID_ITELECINE_HW} },
    { {MFX_CODEC_VP8, MFX_PLUGINID_VP8D_HW}, {MFX_CODEC_VP8,    MFX_PLUGINID_VP8E_HW}, {MFX_MAKEFOURCC('P','T','I','R'), MFX_PLUGINID_ITELECINE_HW} },
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_SW}, {MFX_CODEC_VP8,    MFX_PLUGINID_VP8E_HW}, {MFX_MAKEFOURCC('P','T','I','R'), MFX_PLUGINID_ITELECINE_HW} },
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_SW}, {MFX_CODEC_VP8,    MFX_PLUGINID_VP8E_HW}, {MFX_MAKEFOURCC('P','T','I','R'), MFX_PLUGINID_ITELECINE_HW} },
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_HW}, {MFX_CODEC_VP8,    MFX_PLUGINID_VP8E_HW}, {MFX_MAKEFOURCC('P','T','I','R'), MFX_PLUGINID_ITELECINE_HW} },
    { {MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_HW}, {MFX_CODEC_VP8,    MFX_PLUGINID_VP8E_HW}, {MFX_MAKEFOURCC('P','T','I','R'), MFX_PLUGINID_ITELECINE_HW} },
   // {0, 0}, {0, 0}, {0, 0} },
};


int isSW(const mfxPluginUID &uid)
{
    return (!memcmp(&uid, &MFX_PLUGINID_HEVCD_SW,sizeof(uid)) || 
            !memcmp(&uid, &MFX_PLUGINID_HEVCE_SW,sizeof(uid)) ||
            !memcmp(&uid, &MFX_PLUGINID_ITELECINE_HW,sizeof(uid)) 
           );
};

int isHW(const mfxPluginUID &uid)
{
    return (!isSW(uid));
};



int isDec(const mfxPluginUID &uid)
{
    return (!memcmp(&uid, &MFX_PLUGINID_HEVCD_SW,sizeof(uid)) || 
            !memcmp(&uid, &MFX_PLUGINID_HEVCD_HW,sizeof(uid)) || 
            !memcmp(&uid, &MFX_PLUGINID_VP8D_HW,sizeof(uid)) 
           );
};

int isEnc(const mfxPluginUID &uid)
{
    return (!memcmp(&uid, &MFX_PLUGINID_HEVCE_SW,sizeof(uid)) || 
            !memcmp(&uid, &MFX_PLUGINID_VP8E_HW,sizeof(uid)) 
           );
};

int isVpp(const mfxPluginUID &uid)
{
    return (!isDec(uid) && !isEnc(uid));
};

int equal_uids(const mfxPluginUID &uid1, const mfxPluginUID &uid2)
{
    return !memcmp(&uid1, &uid2 ,sizeof(mfxPluginUID)); 
};

int equal_types(const mfxPluginUID &uid1, const mfxPluginUID &uid2)
{
    return (isDec(uid1) && isDec(uid2) || isEnc(uid1) && isEnc(uid2) || isVpp(uid1) && isVpp(uid2)); 
};

void check_load(tc_struct &tc, int i, tsSession &session, mfxIMPL &impl)
{

    if (i > 0 && (equal_uids(tc[i].uid, tc[i-1].uid) || equal_types(tc[i].uid, tc[i-1].uid)))
    {
        g_tsStatus.expect(MFX_ERR_UNDEFINED_BEHAVIOR); 
    } 
    else 
    {
       if ((impl == 1 && isSW(tc[i].uid)) || (impl != 1))
       {
           g_tsStatus.expect(MFX_ERR_NONE); 
       } 
       else
       {
           g_tsStatus.expect(MFX_ERR_NOT_FOUND); 
       }
    }

    session.Load(session.m_session, &tc[i].uid, 1); 

};

template <class T>
void check_loaded(tc_struct &tc, int i, tsSession &session, mfxIMPL &impl)
{
    T plugin(tc[i].cid);
    if ((impl == 1 && isSW(tc[i].uid)) || (impl != 1))
    {
       g_tsStatus.expect(MFX_ERR_NONE); 
       plugin.Query(session.m_session, 0, plugin.m_pPar); 
       if (i == 0 || (i == 1 && !equal_uids(tc[i].uid, tc[0].uid) && plugin_loaded[0]) || (i == 2 && !equal_uids(tc[i].uid, tc[0].uid) && plugin_loaded[0]))
       {
          plugin_loaded[i] = 1;
       }
    }    
    else
    {
        g_tsStatus.expect(MFX_ERR_UNSUPPORTED); 
        plugin.Query(session.m_session, 0, plugin.m_pPar); 
    }

};

void check_unload(tc_struct &tc, int i, tsSession &session, mfxIMPL &impl)
{
    if (plugin_loaded[i])
    {
       g_tsStatus.expect(MFX_ERR_NONE); 
       session.UnLoad(session.m_session, &tc[i].uid); 
    } 
};

template <class T>
void check_unloaded(tc_struct &tc, int i, tsSession &session, mfxIMPL &impl)
{
    T plugin(tc[i].cid);
    g_tsStatus.expect(MFX_ERR_UNSUPPORTED); 
    plugin.Query(session.m_session, 0, plugin.m_pPar); 
    plugin_loaded[i] = 0;
};


int test(unsigned int id)
{
    TS_START;

    tsSession session; 
    session.MFXInit();

    cid_uid tc[3]; 
    memcpy ( &tc, &test_case[id], sizeof(tc_struct));
    
    mfxIMPL impl;
    MFXQueryIMPL(session.m_session, &impl);

    for (int i=0; i<=2; i++)
    {
        if (tc[i].cid == 0) { break; }
        //Load current plugin 
        check_load(tc, i, session, impl); 
        for (int j=i; j>=0; j--)
        {
            //Current and all previos loaded plugins shuld stay loaded
            if (j != i && !plugin_loaded[j]) {continue;}
            if (isDec(tc[j].uid)) { check_loaded<tsVideoDecoder>(tc, j, session, impl); } else
            if (isEnc(tc[j].uid)) { check_loaded<tsVideoEncoder>(tc, j, session, impl); } else 
            if (isVpp(tc[j].uid)) { check_loaded<tsVideoVPP>(tc, j, session, impl); }
        }
    };
    if (id > 0 && !memcmp(test_case[id], test_case[id-1], sizeof(tc_struct)))
    {
       //Unloading in direct order (fist load - first unload) for second from two equal test cases
       for (int i=0; i<=2; i++)
       {
           if (tc[i].cid == 0 || !plugin_loaded[i]) {continue;}
           //Unload current plugin 
           check_unload(tc, i, session, impl); 
           //Current plugin shuld be unloaded
           if (isDec(tc[i].uid)) { check_unloaded<tsVideoDecoder>(tc, i, session, impl); } else
           if (isEnc(tc[i].uid)) { check_unloaded<tsVideoEncoder>(tc, i, session, impl); } else 
           if (isVpp(tc[i].uid)) { check_unloaded<tsVideoVPP>(tc, i, session, impl); }
           for (int j=i+1; j<=2; j++)
           {
               //All next loaded plugins shuld stay loaded
               if (!plugin_loaded[j]) {continue;}
               if (isDec(tc[j].uid)) { check_loaded<tsVideoDecoder>(tc, j, session, impl); } else
               if (isEnc(tc[j].uid)) { check_loaded<tsVideoEncoder>(tc, j, session, impl); } else 
               if (isVpp(tc[j].uid)) { check_loaded<tsVideoVPP>(tc, j, session, impl); }
            }
       }

    }
    else
    {
       //Unloading in revers order (last load - first unload) for first from two equal test cases or for independent test case
       for (int i=2; i>=0; i--)
       {
           if (tc[i].cid == 0 || !plugin_loaded[i]) {continue;}
           //Unload current plugin 
           check_unload(tc, i, session, impl); 
           //Current plugin shuld be unloaded
           if (isDec(tc[i].uid)) { check_unloaded<tsVideoDecoder>(tc, i, session, impl); } else
           if (isEnc(tc[i].uid)) { check_unloaded<tsVideoEncoder>(tc, i, session, impl); } else 
           if (isVpp(tc[i].uid)) { check_unloaded<tsVideoVPP>(tc, i, session, impl); }
           for (int j=i-1; j>=0; j--)
           {
               //All previos loaded plugins shuld stay loaded
               if (!plugin_loaded[j]) {continue;}
               if (isDec(tc[j].uid)) { check_loaded<tsVideoDecoder>(tc, j, session, impl); } else
               if (isEnc(tc[j].uid)) { check_loaded<tsVideoEncoder>(tc, j, session, impl); } else 
               if (isVpp(tc[j].uid)) { check_loaded<tsVideoVPP>(tc, j, session, impl); }
            }
       }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(plugin_load_unload, test, sizeof(test_case)/sizeof(tc_struct));
}