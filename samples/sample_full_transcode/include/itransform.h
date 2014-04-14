//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "isample.h"
#include "sample_defs.h"

#include "mfxvideo++.h"
#include "mfxaudio++.h"
#include "av_param.h"
#include "av_alloc_request.h"

#include "pipeline_wa.h"

class ITransform {
public:
    virtual ~ITransform() {};
    virtual void Configure(MFXAVParams& , ITransform *pNext) = 0;

    virtual void PutSample(SamplePtr&) = 0;
    virtual bool GetSample(SamplePtr&) = 0;
    //rationale need an interface to negotiate number and type of shared surfaces
    virtual void GetNumSurfaces(MFXAVParams &, IAllocRequest &) = 0;
};

template <class T>
class Transform : public ITransform {
};

#include "transform_adec.h"
#include "transform_aenc.h"
#include "transform_vdec.h"
#include "transform_venc.h"
#include "transform_vpp.h"
#include "transform_plugin.h"