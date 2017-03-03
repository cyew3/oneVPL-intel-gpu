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

template <class T>
class NullTransform : public ITransform {
};

#include "transform_adec.h"
#include "transform_aenc.h"
#include "transform_vdec.h"
#include "transform_venc.h"
#include "transform_vpp.h"
#include "transform_plugin.h"