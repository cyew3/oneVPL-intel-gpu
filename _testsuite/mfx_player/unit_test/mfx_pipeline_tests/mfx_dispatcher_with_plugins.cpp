/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfxsession.h"
#include "mfxplugin.h"

SUITE(DispatcherWithPlugins) {
    TEST(testLoad_WrongGuid) {
        mfxSession session;
        mfxPluginUID guid1 = {};
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_DECODE, MFX_CODEC_HEVC, guid1);
        MFXClose(session);
    }
    TEST(testLoad_WrongCodecID) {
        mfxSession session;
        mfxPluginUID guid1 = {1,2,3,4,5,6,7,8,9,0x10,0x11,0x12,0x13,0x14,0x15,0x16};
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_DECODE, MFX_CODEC_AVC, guid1);
        MFXClose(session);
    }
    TEST(testLoad_WronType) {
        mfxSession session;
        mfxPluginUID guid1 = {1,2,3,4,5,6,7,8,9,0x10,0x11,0x12,0x13,0x14,0x15,0x16};
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_GENERAL, MFX_CODEC_HEVC, guid1);
        MFXClose(session);
    }
    TEST(testLoad) {
        mfxSession session;
        mfxPluginUID guid1 = {1,2,3,4,5,6,7,8,9,0x10,0x11,0x12,0x13,0x14,0x15,0x16};
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, guid1);
        MFXClose(session);
    }
    TEST(testLoadAndUnload) {
        mfxSession session;
        mfxPluginUID guid1 = {1,2,3,4,5,6,7,8,9,0x10,0x11,0x12,0x13,0x14,0x15,0x16};
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        MFXVideoUSER_Load(session, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, guid1);
        MFXVideoUSER_UnLoad(session, guid1);
        MFXClose(session);
    }
    TEST(testEnumerate_wrong_type) {
        mfxSession session;
        mfxPluginDescription dsc;
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        int oneMorePlugin = 0;
        while (MFX_ERR_NONE == MFXVideoUSER_Enumerate(session, MFX_PLUGINTYPE_VIDEO_DECODE, MFX_CODEC_HEVC, oneMorePlugin++, &dsc));
        MFXClose(session);
    }

    TEST(testEnumerate) {
        mfxSession session;
        mfxPluginDescription dsc = {};
        std::vector<mfxU8> name;
        MFXInit(MFX_IMPL_SOFTWARE, 0, &session);
        int oneMorePlugin = 0;
        while (MFX_ERR_NONE == MFXVideoUSER_Enumerate(session, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, oneMorePlugin++, &dsc));
        dsc.NameAlloc = dsc.NameLength;
        name.resize(dsc.NameAlloc);
        dsc.Name = &name.front();
        oneMorePlugin = 0;
        while (MFX_ERR_NONE == MFXVideoUSER_Enumerate(session, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_HEVC, oneMorePlugin++, &dsc));
        MFXClose(session);
    }

}