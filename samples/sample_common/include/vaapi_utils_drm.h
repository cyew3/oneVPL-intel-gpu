/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2015 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifndef __VAAPI_UTILS_DRM_H__
#define __VAAPI_UTILS_DRM_H__

#if defined(LIBVA_DRM_SUPPORT)

#include <va/va_drm.h>
#include <va/va_drmcommon.h>
#include "vaapi_utils.h"
#include "vaapi_allocator.h"

class drmRenderer;

class DRMLibVA : public CLibVA
{
public:
    DRMLibVA(int type);
    virtual ~DRMLibVA(void);

    inline int getFD() { return m_fd; }

protected:
    int m_fd;
    MfxLoader::VA_DRMProxy m_vadrmlib;

private:
    DISALLOW_COPY_AND_ASSIGN(DRMLibVA);
};

class drmRenderer : public vaapiAllocatorParams::Exporter
{
public:
    drmRenderer(int fd, mfxI32 monitorType);
    virtual ~drmRenderer();

    virtual mfxStatus render(mfxFrameSurface1 * pSurface);

    // vaapiAllocatorParams::Exporter methods
    virtual void* acquire(mfxMemId mid);
    virtual void release(mfxMemId mid, void * mem);

    static uint32_t getConnectorType(mfxI32 monitor_type);
    static const msdk_char* getConnectorName(uint32_t connector_type);

private:
    bool getConnector(drmModeRes *resource, uint32_t connector_type);
    bool setupConnection(drmModeRes *resource, drmModeConnector* connector);
    bool getPlane();

    bool setMaster();
    void dropMaster();
    bool restore();

    const MfxLoader::DRM_Proxy m_drmlib;
    const MfxLoader::DrmIntel_Proxy m_drmintellib;

    int m_fd;
    uint32_t m_connector_type;
    uint32_t m_connectorID;
    uint32_t m_encoderID;
    uint32_t m_crtcID;
    uint32_t m_crtcIndex;
    uint32_t m_planeID;
    drmModeModeInfo m_mode;
    drmModeCrtcPtr m_crtc;
    drm_intel_bufmgr* m_bufmgr;
    bool m_overlay_wrn;

private:
    DISALLOW_COPY_AND_ASSIGN(drmRenderer);
};

#endif // #if defined(LIBVA_DRM_SUPPORT)

#endif // #ifndef __VAAPI_UTILS_DRM_H__
