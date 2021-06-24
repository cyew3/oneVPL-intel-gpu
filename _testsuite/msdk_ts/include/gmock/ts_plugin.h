/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2020 Intel Corporation. All Rights Reserved.
//
*/
#pragma once

enum MsdkPluginType {
    MSDK_PLUGIN_TYPE_NONE = 0,
    MSDK_PLUGIN_TYPE_FEI,
};

#include "ts_trace.h"
#include <stdio.h>
#include <string>
#include <cstring>
#include <map>
#include <list>
#include <utility>
#include <vector>
#include <algorithm>
#include "mfxplugin.h"
#include <unordered_set>

typedef enum func_list {
    // common
    Init = 1,
    QueryIOSurf,
    Query,
    Reset,
    GetVideoParam,
    Close,
    PluginInit,
    PluginClose,
    GetPluginParam,
    Execute,
    FreeResources,
    Release,
    SetAuxParams,

    // VPP
    VPPFrameSubmit,
    VPPFrameSubmitEx,

    //Encoder
    EncodeFrameSubmit,

    //Enc
    EncFrameSubmit,

    //Decoder
    DecodeHeader,
    GetPayload,
    DecodeFrameSubmit
} APIfuncs;

class tsPlugin {
private:
    std::map<std::tuple<mfxU32, mfxU32, MsdkPluginType>, mfxPluginUID*> m_puid;
    std::list<mfxPluginUID> m_uid;
    std::list<mfxPluginUID> m_list;

public:
    tsPlugin() {
    }
    ~tsPlugin() {
    }

    inline mfxPluginUID* UID(mfxU32 type, mfxU32 id = 0, MsdkPluginType extType = MSDK_PLUGIN_TYPE_NONE) {
        return m_puid[std::make_tuple(type, id, extType)];
    }

    inline void Reg(mfxU32 type, mfxU32 id, const mfxPluginUID uid, MsdkPluginType extType = MSDK_PLUGIN_TYPE_NONE) {
        m_uid.push_back(uid);
        m_puid[std::make_tuple(type, id, extType)] = &m_uid.back();
    }

    inline std::list<mfxPluginUID> GetUserPlugins() {
        return m_list;
    }

    mfxU32 GetType(const mfxPluginUID& uid);
    void Init(std::string env, std::string platform);
};

void func_name(std::unordered_multiset<mfxU32>::const_iterator it);
bool check_calls(std::vector<mfxU32> real_calls, std::vector<mfxU32> expected_calls);
