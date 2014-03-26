#pragma once

#include "ts_trace.h"
#include <string>
#include <map>
#include <list>

class tsPlugin
{
private:
    std::map<std::pair<mfxU32, mfxU32>, mfxPluginUID*> m_puid;
    std::list<mfxPluginUID> m_uid;

public:
    tsPlugin(){}
    ~tsPlugin(){}

    inline mfxPluginUID* UID(mfxU32 type, mfxU32 id = 0) 
    { 
        return m_puid[std::make_pair(type, id)]; 
    }

    inline void Reg(mfxU32 type, mfxU32 id, const mfxPluginUID uid) 
    { 
        m_uid.push_back(uid);
        m_puid[std::make_pair(type, id)] = &m_uid.back(); 
    }

    void Init(std::string env, std::string platform);
};