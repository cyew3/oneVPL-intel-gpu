//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
#include "av_alloc_request.h"

template <>
MFXAVAllocRequest<mfxFrameAllocRequest>::MFXAVAllocRequest() {
    m_type = PARAM_VIDEO;
}

template <>
MFXAVAllocRequest<mfxAudioAllocRequest>::MFXAVAllocRequest() {
    m_type = PARAM_AUDIO;
}