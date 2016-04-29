/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "mfx_samples_config.h"

#include "session_storage.h"
#include "pipeline_factory.h"
#include <iostream>

SessionStorage::SessionStorage( PipelineFactory& factory , const MFXSessionInfo &vSession, const MFXSessionInfo &aSession ) :
    m_factory(factory),
    m_vInfo(vSession),
    m_aInfo(aSession)
{
}

SessionStorage::~SessionStorage(){
    for (std::map<int, MFXAudioSession*>::iterator it = m_audstorage.begin(); it != m_audstorage.end(); ++it) {
        MSDK_SAFE_DELETE(it->second);
    }
    for (std::map<int, MFXVideoSessionExt*>::iterator it = m_vidstorage.begin(); it != m_vidstorage.end(); ++it) {
        MSDK_SAFE_DELETE(it->second);
    }
}

MFXAudioSession * SessionStorage::GetAudioSessionForID(int id){
    std::map<int, MFXAudioSession*>::iterator session = m_audstorage.find(id);
    if(session == m_audstorage.end())
    {
        std::auto_ptr<MFXAudioSession> pAudioSession (m_factory.CreateAudioSession());

        mfxStatus sts = pAudioSession->Init(m_aInfo.IMPL(), &m_aInfo.Version());
        if (MFX_ERR_NOT_FOUND == sts)
        {
            MSDK_TRACE_INFO(MSDK_STRING("Audio library was not found"));
            return 0;
        }
        else if (MFX_ERR_UNSUPPORTED == sts)
        {
            MSDK_TRACE_INFO(MSDK_STRING("Audio library of specified version was not found"));
            return 0;
        }
        if( MFX_ERR_NONE > sts)
        {
            MSDK_TRACE_ERROR(MSDK_STRING("pAudioSession->Init(m_impl, &m_ver), sts=") << sts);
            throw MFXAudioSessionInitError();
        }

        m_audstorage[id] = pAudioSession.get();
        return pAudioSession.release();
    }
    return (*session).second;
}

MFXVideoSessionExt * SessionStorage::GetVideoSessionForID(int id){
    std::map<int, MFXVideoSessionExt*>::iterator session = m_vidstorage.find(id);
    if(session == m_vidstorage.end())
    {
        std::auto_ptr<MFXVideoSessionExt> pVideoSession (m_factory.CreateVideoSession());

        mfxStatus sts = pVideoSession->Init(m_vInfo.IMPL(), &m_vInfo.Version());
        if( MFX_ERR_NONE > sts) {
            MSDK_TRACE_ERROR(MSDK_STRING("pVideoSession->Init(m_impl, &m_ver), sts=") << sts);
            throw MFXVideoSessionInitError();
        }
        mfxVersion ver = {{0, 0}};
        sts = pVideoSession->QueryVersion(&ver);
        if( MFX_ERR_NONE > sts) {
            MSDK_TRACE_ERROR(MSDK_STRING("pVideoSession->QueryVersion(&ver), sts=") << sts);
        }
        MSDK_TRACE_INFO(MSDK_STRING("Video library version: ") << ver.Major << MSDK_STRING(".") << ver.Minor);
        m_vidstorage[id] = pVideoSession.get();
        return pVideoSession.release();
    }
    return (*session).second;
}
