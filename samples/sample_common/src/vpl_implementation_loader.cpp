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

#include "vpl_implementation_loader.h"
#include "sample_defs.h"
#include "mfxdispatcher.h"

VPLImplementationLoader::VPLImplementationLoader()
{ 
    m_Loader = MFXLoad(); 
    m_isHW = false;
    m_idesc = nullptr;
}

VPLImplementationLoader::~VPLImplementationLoader()
{
    if (m_idesc)
    {
        MFXDispReleaseImplDescription(m_Loader, m_idesc);
    }
    MFXUnload(m_Loader);
}

mfxStatus VPLImplementationLoader::ConfigureImplementation(mfxIMPL impl)
{
    mfxConfig cfgImpl = MFXCreateConfig(m_Loader);

    mfxVariant ImplVariant;
    ImplVariant.Type = MFX_VARIANT_TYPE_U32;

    switch (impl)
    {
        case MFX_IMPL_SOFTWARE:
            ImplVariant.Data.U32 = MFX_IMPL_TYPE_SOFTWARE;
            break;
        case MFX_IMPL_HARDWARE:
        case MFX_IMPL_HARDWARE_ANY:
        case MFX_IMPL_HARDWARE2:
        case MFX_IMPL_HARDWARE3:
        case MFX_IMPL_HARDWARE4:
        case MFX_IMPL_VIA_D3D9:
        case MFX_IMPL_VIA_D3D11:
        case MFX_IMPL_VIA_VAAPI:
            ImplVariant.Data.U32 = MFX_IMPL_TYPE_HARDWARE;
            m_isHW = true;
            break;
        default:
            return MFX_ERR_UNSUPPORTED;
            break;
    }

    mfxStatus sts = MFXSetConfigFilterProperty(cfgImpl, (mfxU8*)"mfxImplDescription.Impl", ImplVariant);
    MSDK_CHECK_STATUS(sts, "MFXSetConfigFilterProperty failed");
    m_Configs.push_back(cfgImpl);
    return sts;
}

mfxStatus VPLImplementationLoader::ConfigureAccelerationMode(mfxAccelerationMode accelerationMode)
{
    mfxStatus sts = MFX_ERR_NONE;

    // configure accelerationMode, except when required implementation is MFX_IMPL_TYPE_HARDWARE, but m_accelerationMode not set
    if (accelerationMode != MFX_ACCEL_MODE_NA || !m_isHW)
    {
        mfxConfig cfgAccelerationMode = MFXCreateConfig(m_Loader);
        mfxVariant AccelerationModeVariant;
        AccelerationModeVariant.Type = MFX_VARIANT_TYPE_U32;
        AccelerationModeVariant.Data.U32 = accelerationMode;
        sts = MFXSetConfigFilterProperty(cfgAccelerationMode, (mfxU8*)"mfxImplDescription.AccelerationMode", AccelerationModeVariant);
        MSDK_CHECK_STATUS(sts, "MFXSetConfigFilterProperty failed")
        m_Configs.push_back(cfgAccelerationMode);
    }

    return sts;
}

mfxStatus VPLImplementationLoader::ConfigureVersion(mfxVersion const& version)
{
    mfxConfig cfgApiVersion = MFXCreateConfig(m_Loader);
    mfxVariant ApiVersionVariant;
    ApiVersionVariant.Type = MFX_VARIANT_TYPE_U32;
    ApiVersionVariant.Data.U32 = version.Version;
    mfxStatus sts = MFXSetConfigFilterProperty(cfgApiVersion, (mfxU8*)"mfxImplDescription.ApiVersion.Version", ApiVersionVariant);
    MSDK_CHECK_STATUS(sts, "MFXSetConfigFilterProperty failed");
    m_Configs.push_back(cfgApiVersion);

    return sts;
}

mfxStatus VPLImplementationLoader::InitImplementation()
{
    // pick the first available implementation
    mfxImplDescription * idesc;
    mfxStatus sts = MFXEnumImplementations(m_Loader, 0, MFX_IMPLCAPS_IMPLDESCSTRUCTURE, (mfxHDL*)&idesc);
    MSDK_CHECK_STATUS(sts, "MFXEnumImplementations failed");
    MSDK_CHECK_POINTER(idesc, MFX_ERR_NULL_PTR);
    m_idesc = idesc;

    return sts;
}

mfxStatus VPLImplementationLoader::ConfigureAndInitImplementation(mfxIMPL impl, mfxAccelerationMode accelerationMode, mfxVersion const& version)
{
    mfxStatus sts;

    sts = ConfigureImplementation(impl);
    MSDK_CHECK_STATUS(sts, "ConfigureImplementation failed");
    sts = ConfigureVersion(version);
    MSDK_CHECK_STATUS(sts, "ConfigureVersion failed");
    sts = ConfigureAccelerationMode(accelerationMode);
    MSDK_CHECK_STATUS(sts, "ConfigureAccelerationMode failed");
    sts = InitImplementation();
    MSDK_CHECK_STATUS(sts, "InitImplementation failed");

    return sts;
}

mfxLoader VPLImplementationLoader::GetLoader() { return m_Loader; }
