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
#include "d3d11_allocator.h"
#include <initguid.h>
#include <guiddef.h>

class OpenCLFilterDX11 : public OpenCLFilterBase
{
public:
    OpenCLFilterDX11();
    virtual ~OpenCLFilterDX11();
    virtual cl_int OCLInit(mfxHDL device);

private:
    virtual cl_int InitSurfaceSharingExtension();
    virtual cl_int InitDevice();

    virtual cl_mem CreateSharedSurface(mfxMemId mid, int nView, bool bIsReadOnly);
    virtual bool EnqueueAcquireSurfaces(cl_mem* surfaces, int nSurfaces);
    virtual bool EnqueueReleaseSurfaces(cl_mem* surfaces, int nSurfaces);

    ID3D11Device*       m_pDevice;
    ID3D11Texture2D*    m_pSharedSurfaces[c_shared_surfaces_num];

};

#endif // #if defined(_WIN32) || defined(_WIN64)

