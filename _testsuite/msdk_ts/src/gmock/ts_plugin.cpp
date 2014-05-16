#include "ts_plugin.h"

mfxPluginUID readUID(char* s)
{
    mfxPluginUID uid = {};
    mfxU32 b = 0;

    for(mfxU32 i = 0; i < 16; i ++)
    {
#pragma warning(disable:4996)
        sscanf(s+(i<<1), "%2x", &b);
#pragma warning(default:4996)
        uid.Data[i] = (mfxU8)b;
    }
    return uid;
}

void tsPlugin::Init(std::string env, std::string platform)
{
    bool isHW = (std::string::npos == platform.find_first_of("_sw_"));

    Reg(MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, MFX_PLUGINID_HEVCE_SW);
    Reg(MFX_PLUGINTYPE_VIDEO_DECODE, MFX_CODEC_HEVC, MFX_PLUGINID_HEVCD_SW);
    Reg(MFX_PLUGINTYPE_VIDEO_VPP, MFX_MAKEFOURCC('P','T','I','R'), MFX_PLUGINID_ITELECINE_HW);
    Reg(MFX_PLUGINTYPE_VIDEO_VPP, MFX_MAKEFOURCC('C','A','M','R'), MFX_PLUGINID_CAMERA_HW);

    if(isHW)
    {
        Reg(MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_VP8, MFX_PLUGINID_VP8E_HW);
        Reg(MFX_PLUGINTYPE_VIDEO_DECODE, MFX_CODEC_VP8, MFX_PLUGINID_VP8D_HW);
    }

    // Example: TS_PLUGINS=HEVC,1,15dd936825ad475ea34e35f3f54217a6
    while(env.size())
    {
        std::string::size_type pos = env.find_first_of(';');
            
        if(pos == std::string::npos)
        {
            pos = env.size();
        }

        if(    pos >= sizeof(mfxPluginUID) + 6
            && env[4] == ',' && env[6] == ',')
        {
            mfxU32 id   = MFX_MAKEFOURCC(env[0], env[1], env[2], env[3]);
            mfxU32 type = env[5] - '0';
            Reg(type, id, readUID(&env[7]));
        }
        env.erase(0, pos+1);
    }
}