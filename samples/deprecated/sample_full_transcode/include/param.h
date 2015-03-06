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


class MFXAVParams{
    mfxAudioParam* audioParam;
    mfxVideoParam* videoParam;

public:
    enum{
        PARAM_AUDIO,
        PARAM_VIDEO
    };
    MFXAVParams(mfxAudioParam& aParam) : videoParam(){
        audioParam = &aParam;
    }
    MFXAVParams(mfxVideoParam& vParam) : audioParam(){
        videoParam = &vParam;
    }
    int GetParamType() const {
        return audioParam ? PARAM_AUDIO : PARAM_VIDEO;
    }
    mfxAudioParam& GetAudioParam() const {
        if( GetParamType() != PARAM_AUDIO) {
            MSDK_TRACE_ERROR(MSDK_STRING("Incompatible param type error"));
            throw IncompatibleParamTypeError();
        }
        return *audioParam;
    }
    mfxVideoParam& GetVideoParam() const {
        if( GetParamType() != PARAM_VIDEO) {
            MSDK_TRACE_ERROR(MSDK_STRING("Incompatible param type error"));
            throw IncompatibleParamTypeError();
        }
        return *videoParam;
    }
    operator mfxAudioParam(){
        return GetAudioParam();
    }
    operator mfxVideoParam() {
        return GetVideoParam();
    }
};