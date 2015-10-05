/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement.
This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
Copyright(c) 2005-2015 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#pragma once

#if defined(_WIN32) || defined(_WIN64)

#include "opencl_filter.h"
#include "d3d_utils.h"
#include "d3d_allocator.h"

class OpenCLFilterDX9 : public OpenCLFilterBase
{
public:
    OpenCLFilterDX9();
    virtual ~OpenCLFilterDX9();
    virtual cl_int OCLInit(mfxHDL device);

protected: // functions
    virtual cl_int InitDevice();
    virtual cl_int InitSurfaceSharingExtension();

    virtual cl_mem CreateSharedSurface(mfxMemId mid, int nView, bool bIsReadOnly);
    virtual bool EnqueueAcquireSurfaces(cl_mem* surfaces, int nSurfaces);
    virtual bool EnqueueReleaseSurfaces(cl_mem* surfaces, int nSurfaces);
protected: // variables
    std::auto_ptr<SafeD3DDeviceManager> m_SafeD3DDeviceManager;

    HANDLE              m_hSharedSurfaces[c_shared_surfaces_num]; // d3d9 surface shared handles
    IDirect3DSurface9*  m_pSharedSurfaces[c_shared_surfaces_num];
};

#endif // #if defined(_WIN32) || defined(_WIN64)
