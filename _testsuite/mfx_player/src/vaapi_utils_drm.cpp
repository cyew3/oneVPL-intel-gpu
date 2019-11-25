/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if defined(LIBVA_DRM_SUPPORT)

#include "vaapi_utils_drm.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <stdexcept>

#include <drm.h>
#include <drm_fourcc.h>

constexpr mfxU32 MFX_DRI_MAX_NODES_NUM = 16;
constexpr mfxU32 MFX_DRI_RENDER_START_INDEX = 128;
constexpr mfxU32 MFX_DRI_CARD_START_INDEX = 0;
constexpr  mfxU32 MFX_DRM_DRIVER_NAME_LEN = 4;
const char* MFX_DRM_INTEL_DRIVER_NAME = "i915";
const char* MFX_DRI_PATH = "/dev/dri/";
const char* MFX_DRI_NODE_RENDER = "renderD";
const char* MFX_DRI_NODE_CARD = "card";

int get_drm_driver_name(int fd, char *name, int name_size)
{
    drm_version_t version = {};
    version.name_len = name_size;
    version.name = name;
    return ioctl(fd, DRM_IOWR(0, drm_version), &version);
}

int open_first_intel_adapter(int type)
{
    std::string adapterPath = MFX_DRI_PATH;
    char driverName[MFX_DRM_DRIVER_NAME_LEN + 1] = {};
    mfxU32 nodeIndex;

    switch (type) {
    case MFX_LIBVA_DRM:
    case MFX_LIBVA_AUTO:
        adapterPath += MFX_DRI_NODE_RENDER;
        nodeIndex = MFX_DRI_RENDER_START_INDEX;
        break;
    case MFX_LIBVA_DRM_MODESET:
        adapterPath += MFX_DRI_NODE_CARD;
        nodeIndex = MFX_DRI_CARD_START_INDEX;
        break;
    default:
        throw std::invalid_argument("Wrong libVA backend type");
    }

    for (mfxU32 i = 0; i < MFX_DRI_MAX_NODES_NUM; ++i) {
        std::string curAdapterPath = adapterPath + std::to_string(nodeIndex + i);

        int fd = open(curAdapterPath.c_str(), O_RDWR);
        if (fd < 0) continue;

        if (!get_drm_driver_name(fd, driverName, MFX_DRM_DRIVER_NAME_LEN) &&
            !strcmp(driverName, MFX_DRM_INTEL_DRIVER_NAME)) {
            return fd;
        }
        close(fd);
    }

    return -1;
}

int open_intel_adapter(const std::string& devicePath, int type)
{
    if(devicePath.empty())
        return open_first_intel_adapter(type);

    int fd = open(devicePath.c_str(), O_RDWR);

    if (fd < 0) {
        printf("Failed to open specified device\n");
        return -1;
    }

    char driverName[MFX_DRM_DRIVER_NAME_LEN + 1] = {};
    if (!get_drm_driver_name(fd, driverName, MFX_DRM_DRIVER_NAME_LEN) &&
        !strcmp(driverName, MFX_DRM_INTEL_DRIVER_NAME)) {
            return fd;
    }
    else {
        close(fd);
        printf("Specified device is not Intel one\n");
        return -1;
    }
}

DRMLibVA::DRMLibVA(const std::string& devicePath, int type)
    : m_fd(-1)
{
    mfxStatus sts = MFX_ERR_NONE;

    m_fd = open_intel_adapter(devicePath, type);
    if (m_fd < 0) throw std::range_error("Intel GPU was not found");

    m_va_dpy = m_vadrmlib.vaGetDisplayDRM(m_fd);
    if (m_va_dpy)
    {
        int major_version = 0, minor_version = 0;
        VAStatus va_res = m_libva.vaInitialize(m_va_dpy, &major_version, &minor_version);
        sts = va_to_mfx_status(va_res);
    }
    else {
        sts = MFX_ERR_NULL_PTR;
    }

    if (MFX_ERR_NONE != sts)
    {
        if (m_va_dpy) m_libva.vaTerminate(m_va_dpy);
        close(m_fd);
        throw std::runtime_error("Loading of VA display was failed");
    }
}

DRMLibVA::~DRMLibVA(void)
{
    if (m_va_dpy)
    {
        m_libva.vaTerminate(m_va_dpy);
    }
    if (m_fd >= 0)
    {
        close(m_fd);
    }
}

#endif // #if defined(LIBVA_DRM_SUPPORT)
