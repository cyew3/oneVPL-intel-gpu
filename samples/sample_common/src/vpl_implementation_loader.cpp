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
#include "sample_utils.h"
#include "sample_defs.h"
#include "mfxdispatcher.h"

#if defined(LINUX32) || defined(LINUX64)
#include <link.h>
#include <string.h>
#endif

#include <map>
#include <regex>

static mfxU32 GetAdapterNumber(const mfxChar* cDeviceID) {
    std::string strDevID(cDeviceID);
    size_t idx = strDevID.rfind('/');
    mfxU32 adapterIdx = -1;

    if (idx != std::string::npos && (idx + 1) < strDevID.size())
        adapterIdx = std::stoi(strDevID.substr(idx + 1));

    return adapterIdx;
}

 const std::map<mfxAccelerationMode, const msdk_tstring> mfxAccelerationModeNames = {
    { MFX_ACCEL_MODE_NA, MSDK_STRING("MFX_ACCEL_MODE_NA") },
    { MFX_ACCEL_MODE_VIA_D3D9, MSDK_STRING("MFX_ACCEL_MODE_VIA_D3D9") },
    { MFX_ACCEL_MODE_VIA_D3D11, MSDK_STRING("MFX_ACCEL_MODE_VIA_D3D11") },
    { MFX_ACCEL_MODE_VIA_VAAPI, MSDK_STRING("MFX_ACCEL_MODE_VIA_VAAPI") },
    { MFX_ACCEL_MODE_VIA_VAAPI_DRM_RENDER_NODE, MSDK_STRING("MFX_ACCEL_MODE_VIA_VAAPI_DRM_RENDER_NODE") },
    { MFX_ACCEL_MODE_VIA_VAAPI_DRM_MODESET, MSDK_STRING("MFX_ACCEL_MODE_VIA_VAAPI_DRM_MODESET") },
    { MFX_ACCEL_MODE_VIA_VAAPI_GLX, MSDK_STRING("MFX_ACCEL_MODE_VIA_VAAPI_GLX") },
    { MFX_ACCEL_MODE_VIA_VAAPI_X11, MSDK_STRING("MFX_ACCEL_MODE_VIA_VAAPI_X11") },
    { MFX_ACCEL_MODE_VIA_VAAPI_WAYLAND, MSDK_STRING("MFX_ACCEL_MODE_VIA_VAAPI_WAYLAND") },
    { MFX_ACCEL_MODE_VIA_HDDLUNITE, MSDK_STRING("MFX_ACCEL_MODE_VIA_HDDLUNITE") }
};

VPLImplementationLoader::VPLImplementationLoader()
{
    m_Loader = MFXLoad();
    m_idesc = nullptr;
    m_ImplIndex = 0;
    m_MinVersion = MakeVersion(2, 0);
    m_adapterType = mfxMediaAdapterType::MFX_MEDIA_UNKNOWN;
    m_adapterNum = -1;
}

VPLImplementationLoader::~VPLImplementationLoader()
{
    if (m_idesc)
    {
        MFXDispReleaseImplDescription(m_Loader, m_idesc);
    }
    MFXUnload(m_Loader);
}

mfxStatus VPLImplementationLoader::CreateConfig(mfxU16 data, const char* propertyName)
{
    mfxConfig cfg = MFXCreateConfig(m_Loader);
    mfxVariant variant;
    variant.Type = MFX_VARIANT_TYPE_U16;
    variant.Data.U32 = data;
    mfxStatus sts = MFXSetConfigFilterProperty(cfg, (mfxU8*)propertyName, variant);
    MSDK_CHECK_STATUS(sts, "MFXSetConfigFilterProperty failed");
    m_Configs.push_back(cfg);

    return sts;
}

mfxStatus VPLImplementationLoader::CreateConfig(mfxU32 data, const char* propertyName)
{
    mfxConfig cfg = MFXCreateConfig(m_Loader);
    mfxVariant variant;
    variant.Type = MFX_VARIANT_TYPE_U32;
    variant.Data.U32 = data;
    mfxStatus sts = MFXSetConfigFilterProperty(cfg, (mfxU8*)propertyName, variant);
    MSDK_CHECK_STATUS(sts, "MFXSetConfigFilterProperty failed");
    m_Configs.push_back(cfg);

    return sts;
}

mfxStatus VPLImplementationLoader::ConfigureImplementation(mfxIMPL impl)
{
    mfxConfig cfgImpl = MFXCreateConfig(m_Loader);

    mfxVariant ImplVariant;
    ImplVariant.Type = MFX_VARIANT_TYPE_U32;

    std::vector<mfxU32> hwImpls = { MFX_IMPL_HARDWARE, MFX_IMPL_HARDWARE_ANY,
                                    MFX_IMPL_HARDWARE2, MFX_IMPL_HARDWARE3,
                                    MFX_IMPL_HARDWARE4, MFX_IMPL_VIA_D3D9,
                                    MFX_IMPL_VIA_D3D11 };

    std::vector<mfxU32>::iterator hwImplsIt = std::find_if(hwImpls.begin(),
                                                        hwImpls.end(),
                                                        [impl](const mfxU32 & val)
                                                        {
                                                            return (val == MFX_IMPL_VIA_MASK(impl) || val == MFX_IMPL_BASETYPE(impl));
                                                        });

    if (MFX_IMPL_BASETYPE(impl) == MFX_IMPL_SOFTWARE)
    {
        ImplVariant.Data.U32 = MFX_IMPL_TYPE_SOFTWARE;
    }
    else if (hwImplsIt != hwImpls.end())
    {
        ImplVariant.Data.U32 = MFX_IMPL_TYPE_HARDWARE;
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

    mfxStatus sts = MFXSetConfigFilterProperty(cfgImpl, (mfxU8*)"mfxImplDescription.Impl", ImplVariant);
    MSDK_CHECK_STATUS(sts, "MFXSetConfigFilterProperty failed");
    m_Configs.push_back(cfgImpl);
    msdk_printf(MSDK_STRING("CONFIGURE LOADER: required implemetation: %s \n"), ImplVariant.Data.U32 == MFX_IMPL_TYPE_HARDWARE ? MSDK_STRING("hw") : MSDK_STRING("sw"));
    return sts;
}

mfxStatus VPLImplementationLoader::ConfigureAccelerationMode(mfxAccelerationMode accelerationMode, mfxIMPL impl)
{
    mfxStatus sts = MFX_ERR_NONE;
    bool isHW = MFX_IMPL_BASETYPE(impl) != MFX_IMPL_SOFTWARE;

    // configure accelerationMode, except when required implementation is MFX_IMPL_TYPE_HARDWARE, but m_accelerationMode not set
    if (accelerationMode != MFX_ACCEL_MODE_NA || !isHW)
    {
        sts = CreateConfig((mfxU32)accelerationMode, "mfxImplDescription.AccelerationMode");
        msdk_printf(MSDK_STRING("CONFIGURE LOADER: required implemetation mfxAccelerationMode: %s \n"), mfxAccelerationModeNames.at(accelerationMode).c_str());
    }

    return sts;
}

mfxStatus VPLImplementationLoader::ConfigureVersion(mfxVersion const version)
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = CreateConfig(version.Version, "mfxImplDescription.ApiVersion.Version");
    sts = CreateConfig(version.Major, "mfxImplDescription.ApiVersion.Major");
    sts = CreateConfig(version.Minor, "mfxImplDescription.ApiVersion.Minor");
    msdk_printf(MSDK_STRING("CONFIGURE LOADER: required version: %d.%d.%d\n"), version.Major, version.Minor, version.Version);

    return sts;
}

void VPLImplementationLoader::SetAdapterTypeAndNum(mfxU16 adapterType, mfxU32 adapterNum)
{
    if (adapterType != mfxMediaAdapterType::MFX_MEDIA_UNKNOWN)
    {
        m_adapterType = adapterType;
        m_adapterNum = adapterNum;

        msdk_stringstream ss;
        ss << MSDK_STRING("CONFIGURE LOADER: required adapter type: ")
            << (m_adapterType == mfxMediaAdapterType::MFX_MEDIA_INTEGRATED ? MSDK_STRING("integrated") : MSDK_STRING("discrete"));
        if (m_adapterNum != -1)
            ss << MSDK_STRING(" with index ") << m_adapterNum;
        ss << std::endl;

        msdk_printf(MSDK_STRING("%s"), ss.str().c_str());
    }
}

mfxStatus VPLImplementationLoader::EnumImplementations()
{
    mfxImplDescription * idesc;
    mfxStatus sts = MFX_ERR_NONE;

    int impl = 0;
    m_ImplIndex = (mfxU32)-1;
    while (sts == MFX_ERR_NONE)
    {
        sts = MFXEnumImplementations(m_Loader, impl, MFX_IMPLCAPS_IMPLDESCSTRUCTURE, (mfxHDL*)&idesc);
        MSDK_CHECK_STATUS(sts, "MFXEnumImplementations failed");
        if (!idesc)
        {
            sts = MFX_ERR_NULL_PTR;
            break;
        }
        else if ((idesc->Dev.MediaAdapterType == m_adapterType || mfxMediaAdapterType::MFX_MEDIA_UNKNOWN == m_adapterType)
            && (m_adapterNum == -1 || GetAdapterNumber(idesc->Dev.DeviceID) == m_adapterNum)
            && MakeVersion(idesc->ApiVersion) >= m_MinVersion)
        {
            m_idesc = idesc;
            m_ImplIndex = impl;
            break;
        }
        impl++;
    }

    if (m_ImplIndex == (mfxU32)-1)
    {
        msdk_printf(MSDK_STRING("Library was not found with required deviceIDAndAdapter, use implemetation: 0 \n"));
        m_ImplIndex = 0;
    }

    return sts;
}

mfxStatus VPLImplementationLoader::ConfigureAndEnumImplementations(mfxIMPL impl, mfxAccelerationMode accelerationMode)
{
    mfxStatus sts;

    sts = ConfigureImplementation(impl);
    MSDK_CHECK_STATUS(sts, "ConfigureImplementation failed");
    sts = ConfigureAccelerationMode(accelerationMode, impl);
    MSDK_CHECK_STATUS(sts, "ConfigureAccelerationMode failed");
    sts = EnumImplementations();
    MSDK_CHECK_STATUS(sts, "EnumImplementations failed");

    return sts;
}

mfxLoader VPLImplementationLoader::GetLoader() const { return m_Loader; }

mfxU32 VPLImplementationLoader::GetImplIndex() const
{
    return m_ImplIndex;
}

mfxVersion VPLImplementationLoader::GetVersion() const
{
    return m_idesc ? m_idesc->ApiVersion : mfxVersion({{0, 0}});
}

std::string VPLImplementationLoader::GetImplName() const
{
    if (m_idesc)
    {
        return std::string(m_idesc->ImplName);
    }
    else
    {
        return "";
    }
}

std::pair<mfxI16, mfxI32> VPLImplementationLoader::GetDeviceIDAndAdapter() const
{
    auto result = std::make_pair(-1, -1);
    if (!m_idesc)
        return result;

    std::string deviceAdapterInfo(m_idesc->Dev.DeviceID);
    std::regex pattern("([0-9a-fA-F]+)(?:/([0-9]|[1-9][0-9]+))?");
    std::smatch match;
    if (!std::regex_match(deviceAdapterInfo, match, pattern))
        return result;

    try
    {
        result.first = std::stoi(match[1].str(), 0, 16);
        if (match[2].matched)
            result.second = std::stoi(match[2].str());
    }
    catch (std::exception const &)
    { 
        return result;
    }

    return result;
}

void VPLImplementationLoader::SetMinVersion(mfxVersion const version)
{
    m_MinVersion = MakeVersion(version);
}

mfxStatus MainVideoSession::CreateSession(VPLImplementationLoader* Loader)
{
    mfxStatus sts = MFXCreateSession(Loader->GetLoader(), Loader->GetImplIndex(), &m_session);
    mfxVersion version = Loader->GetVersion();
    msdk_printf(MSDK_STRING("Loaded Library Version: %d.%d \n"), version.Major, version.Minor);
    std::string implName = Loader->GetImplName();
    msdk_tstring strImplName;
    std::copy(std::begin(implName), std::end(implName), back_inserter(strImplName));
    msdk_printf(MSDK_STRING("Loaded Library ImplName: %s \n"),  strImplName.c_str());

#if (defined(LINUX32) || defined(LINUX64))
    msdk_printf(MSDK_STRING("Used implementation number: %d \n"), Loader->GetImplIndex());
    msdk_printf(MSDK_STRING("Loaded modules:\n"));
    int numLoad = 0;
    dl_iterate_phdr(PrintLibMFXPath, &numLoad);
#else
#if !defined(MFX_DISPATCHER_LOG)
    PrintLoadedModules();
#endif // !defined(MFX_DISPATCHER_LOG)
#endif //(defined(LINUX32) || defined(LINUX64))
    msdk_printf(MSDK_STRING("\n"));

    return sts;
}
