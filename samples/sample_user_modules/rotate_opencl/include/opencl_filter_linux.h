//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2005-2014 Intel Corporation. All Rights Reserved.
//

#pragma once

#if !defined(_WIN32) && !defined(_WIN64)

#include "logger.h"
#include <string>
#include <iostream>
#include <vector>

#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <va/va.h>
#include <va/va_drm.h>
#include <va/va_x11.h>
#include <va/va_backend.h>

#define DCL_USE_DEPRECATED_OPENCL_1_1_APIS
#include <CL/cl.h>
#include <CL/opencl.h>
#include <CL/va_ext.h>


typedef struct {
    std::string       program_source;
    std::string       kernelY_FuncName;
    std::string       kernelUV_FuncName;
    int format;

    cl_program  clprogram;
    cl_kernel   clkernelY;
    cl_kernel   clkernelUV;
} OCL_YUV_kernel;

class OpenCLFilter
{
public:
    OpenCLFilter();
    virtual ~OpenCLFilter();

    cl_int OCLInit(VADisplay* pD3DDeviceManager);
    cl_int AddKernel(const char* filename, const char* kernelY_name, const char* kernelUV_name, int format);
    cl_int SelectKernel(unsigned int kNo);

    cl_int ProcessSurface(int width, int height, VASurfaceID* pSurfIn, VASurfaceID* pSurfOut);
    //void DumpToFile(VASurfaceID *pSrc);
    static std::string readFile(const char *filename);
private:
    cl_int InitPlatform();
    cl_int InitD3D9SurfaceSharingExtension();
    cl_int InitDevice();
    cl_int BuildKernels();
    cl_int SetKernelArgs();

    cl_int PrepareSharedSurfaces(int width, int height, VASurfaceID* pD3DSurf);
    cl_int ProcessSurface();
    cl_int CopySurface(VASurfaceID* , VASurfaceID* );
    cl_int CopySurfacePlane(VASurfaceID *pSrc, VASurfaceID *pDst, bool isLuma);

    void   ReleaseResources();    

    static size_t chooseLocalSize(size_t globalSize, size_t preferred);

    bool                                m_bInit;
    int                                 m_activeKernel;
    int                                 m_currentWidth;
    int                                 m_currentHeight;

    VADisplay                           m_vaDisplay;
    std::vector<OCL_YUV_kernel>         m_kernels;

    int                 m_currentFormat;

    cl_platform_id      m_clplatform;
    cl_device_id        m_cldevice;
    cl_context          m_clcontext;
    cl_command_queue    m_clqueue;
    bool                m_bSharedSurfacesCreated;

    static const size_t c_shared_surfaces_num = 2; // In and Out
    static const size_t c_ocl_surface_buffers_num = 2*c_shared_surfaces_num; // YIn, UVIn, YOut, UVOut

    cl_mem              m_clbuffer[c_ocl_surface_buffers_num];
    VASurfaceID         m_pSharedSurfaces[c_shared_surfaces_num];

    size_t              m_GlobalWorkSizeY[2];
    size_t              m_GlobalWorkSizeUV[2];
    size_t              m_LocalWorkSizeY[2];
    size_t              m_LocalWorkSizeUV[2];

    FILE* m_pFile;
//  logger for filter depends on build configuration
    Logger<
#ifdef _DEBUG
        Debug
#else
        Error
#endif
    > log;
};

#endif // #if !defined(_WIN32) && !defined(_WIN64)
