//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
//*/

#pragma once

#include "mfxvideo++.h"
#include "mfxaudio++.h"
#include "sample_utils.h"
#include "sample_defs.h"

#include <map>
#include "session_info.h"
#include "video_session.h"

#pragma warning(disable:4512)

class PipelineFactory;

class SessionStorage :private no_copy{
    std::map<int, MFXAudioSession*> m_audstorage;
    std::map<int, MFXVideoSessionExt*> m_vidstorage;
    PipelineFactory&          m_factory;
    MFXSessionInfo            m_vInfo;
    MFXSessionInfo            m_aInfo;
public:
    SessionStorage (PipelineFactory& factory , const MFXSessionInfo &vSession, const MFXSessionInfo &aSession );
    virtual ~SessionStorage();
    virtual  MFXAudioSession * GetAudioSessionForID(int id);
    virtual  MFXVideoSessionExt * GetVideoSessionForID(int id);
};