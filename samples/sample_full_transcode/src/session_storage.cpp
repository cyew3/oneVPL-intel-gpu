//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//
//
//*/

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
        if (MFX_ERR_UNSUPPORTED == sts) {
            MSDK_TRACE_INFO(MSDK_STRING("Audio library not found"));
            return 0;
        }
        if( MFX_ERR_NONE > sts) {
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
