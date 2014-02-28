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

#pragma warning(disable:4239)

class IAllocRequest {
public:
    virtual mfxFrameAllocRequest& Video() = 0;
    virtual mfxAudioAllocRequest& Audio() = 0;
};

template <class T>
class MFXAVAllocRequest : public IAllocRequest {
    mfxFrameAllocRequest m_videoRequest;
    mfxAudioAllocRequest m_audioRequest;
    enum {
        PARAM_AUDIO,
        PARAM_VIDEO
    } m_type;
public:
    MFXAVAllocRequest();
    virtual mfxFrameAllocRequest& Video() {
        if (m_type != PARAM_VIDEO)
            throw IncompatibleParamTypeError();
        return m_videoRequest;
    }
    virtual mfxAudioAllocRequest& Audio() {
        if (m_type != PARAM_AUDIO)
            throw IncompatibleParamTypeError();
        return m_audioRequest;
    }
};
