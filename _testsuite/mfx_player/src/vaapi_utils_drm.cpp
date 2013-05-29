/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#if defined(LIBVA_DRM_SUPPORT)

#include "vaapi_utils_drm.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

DRMLibVA::DRMLibVA(void):
    m_fd(-1)
{
    VAStatus va_res = VA_STATUS_SUCCESS;
    mfxStatus sts = MFX_ERR_NONE;
    int major_version = 0, minor_version = 0;

    m_fd = open("/dev/dri/card0", O_RDWR);

    if (m_fd < 0) sts = MFX_ERR_NOT_INITIALIZED;
    if (MFX_ERR_NONE == sts)
    {
        m_va_dpy = vaGetDisplayDRM(m_fd);
        if (!m_va_dpy)
        {
            close(m_fd);
            sts = MFX_ERR_NULL_PTR;
        }
    }
    if (MFX_ERR_NONE == sts)
    {
        va_res = vaInitialize(m_va_dpy, &major_version, &minor_version);
        sts = va_to_mfx_status(va_res);
        if (MFX_ERR_NONE != sts)
        {
            close(m_fd);
            m_fd = -1;
        }
    }
    if (MFX_ERR_NONE != sts) throw std::bad_alloc();
}

DRMLibVA::~DRMLibVA(void)
{
    if (m_va_dpy)
    {
        vaTerminate(m_va_dpy);
    }
    if (m_fd >= 0)
    {
        close(m_fd);
    }
}

#endif // #if defined(LIBVA_DRM_SUPPORT)
