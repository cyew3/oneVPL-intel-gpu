/******************************************************************************\
Copyright (c) 2021, Intel Corporation
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

#ifndef __VPL_IMPLEMENTATION_LOADER_H__
#define __VPL_IMPLEMENTATION_LOADER_H__

#include "mfxdispatcher.h"
#include "mfxvideo++.h"
#include <vector>
#include <string>

class VPLImplementationLoader {
    mfxLoader m_Loader;
    std::vector<mfxConfig> m_Configs;
    mfxImplDescription* m_idesc;
    mfxU32 m_ImplIndex;
    mfxChar devIDAndAdapter[MFX_STRFIELD_LEN] = {};
    mfxU32 m_MinVersion;

public:
    VPLImplementationLoader();
    ~VPLImplementationLoader();

    mfxStatus CreateConfig(mfxU16 data, const char* propertyName);
    mfxStatus CreateConfig(mfxU32 data, const char* propertyName);
    mfxStatus ConfigureImplementation(mfxIMPL impl);
    mfxStatus ConfigureAccelerationMode(mfxAccelerationMode accelerationMode, mfxIMPL impl);
    mfxStatus ConfigureVersion(mfxVersion const version);
    void SetDeviceAndAdapter(mfxU16 deviceID, mfxU32 adapterNum);
    mfxStatus EnumImplementations();
    mfxStatus ConfigureAndEnumImplementations(mfxIMPL impl, mfxAccelerationMode accelerationMode);
    mfxLoader GetLoader() const;
    mfxU32 GetImplIndex() const;
    std::string GetImplName() const;
    mfxVersion GetVersion() const;
    std::pair<mfxI16, mfxI32> GetDeviceIDAndAdapter() const;
    void SetMinVersion(mfxVersion const version);
};

class MainVideoSession : public MFXVideoSession {
public:
    mfxStatus CreateSession(VPLImplementationLoader* Loader);
};

#endif //__VPL_IMPLEMENTATION_LOADER_H__
