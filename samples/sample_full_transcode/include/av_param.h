//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "mfxstructures.h"
#include "mfxastructures.h"
#include "exceptions.h"


class MFXAVParams {
    mfxAudioParam m_audioParam;
    mfxVideoParam m_videoParam;
public:
    enum {
        PARAM_AUDIO,
        PARAM_VIDEO
    } m_type;
    MFXAVParams(mfxAudioParam& aParam) 
        : m_audioParam(aParam) 
        , m_videoParam()
        , m_type(PARAM_AUDIO) {
    }
    MFXAVParams(mfxVideoParam& vParam) 
        : m_audioParam()
        , m_videoParam (vParam)
        , m_type(PARAM_VIDEO){
    }
    int GetParamType() {
        return m_type;
    }
    mfxAudioParam& GetAudioParam() {
        if( GetParamType() != PARAM_AUDIO)
            throw IncompatibleParamTypeError();
        return m_audioParam;
    }
    mfxVideoParam& GetVideoParam() {
        if( GetParamType() != PARAM_VIDEO)
            throw IncompatibleParamTypeError();
        return m_videoParam;
    }
    operator mfxAudioParam(){
        return GetAudioParam();
    }
    operator mfxVideoParam() {
        return GetVideoParam();
    }
};