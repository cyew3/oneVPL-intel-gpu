/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2005-2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#pragma once

#if !defined(_WIN32) && !defined(_WIN64)

#include "logger.h"
#include <string>
#include <stdio.h>
#include <stdlib.h>

#include <va/va.h>

#define DCL_USE_DEPRECATED_OPENCL_1_1_APIS
#include <CL/cl.h>
#include <CL/opencl.h>
#include <CL/va_ext.h>

#include "opencl_filter.h"

class OpenCLFilterVA : public OpenCLFilterBase
{
public:
    OpenCLFilterVA();
    virtual ~OpenCLFilterVA();
    virtual cl_int OCLInit(mfxHDL device);

protected: // functions
    cl_int InitDevice();
    cl_int InitSurfaceSharingExtension();
    cl_int PrepareSharedSurfaces(int width, int height, mfxMemId pSurfIn, mfxMemId pSurfOut);
    cl_int ProcessSurface();

protected: // variables
    VADisplay m_vaDisplay;
    VASurfaceID m_SharedSurfaces[c_shared_surfaces_num];
};

#endif // #if !defined(_WIN32) && !defined(_WIN64)
