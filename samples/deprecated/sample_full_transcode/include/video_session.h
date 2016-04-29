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

#include "mfxvideo++.h"
#include "hw_device.h"
#include "base_allocator.h"
#include "av_alloc_request.h"
#include "isample.h"
#include <memory>


class PipelineFactory;

//Rationale:
//mediaSDK c++ wrapper wraps only c library functionality
//however OOP style code requires to have allocators connected to session as a property

class MFXVideoSessionExt : public MFXVideoSession {
public:
    MFXVideoSessionExt(PipelineFactory& factory);

    virtual mfxStatus GetFrameAllocator(mfxFrameAllocator *&refAllocator) {
        refAllocator = m_pAllocator.get();
        return MFX_ERR_NONE;
    }
    virtual mfxStatus Init(mfxIMPL impl, mfxVersion *ver) {
        mfxStatus sts = MFXInit(impl, ver, &m_session);
        if (!sts)
            SetDeviceAndAllocator();
        return sts;
    }
    virtual std::auto_ptr<CHWDevice>& GetDevice() {
        return m_pDevice;
    }
    virtual std::auto_ptr<BaseFrameAllocator>& GetAllocator() {
        return m_pAllocator;
    }
    virtual ~MFXVideoSessionExt() {
        // we need to explicitly close the session to make sure that it releases
        // all objects, otherwise we may release device or allocator while
        // it is still used by the session
        Close();
    }
protected:
    PipelineFactory& m_factory;

    std::auto_ptr<CHWDevice> m_pDevice;
    std::auto_ptr<BaseFrameAllocator> m_pAllocator;

    void CreateDevice(AllocatorImpl impl);
    void SetDeviceAndAllocator();
};
