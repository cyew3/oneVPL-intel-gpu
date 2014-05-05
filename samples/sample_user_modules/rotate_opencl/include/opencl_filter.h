/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2005-2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#pragma once

#if defined(_WIN32) || defined(_WIN64)

#include "logger.h"
#include "d3d_utils.h"
#include "d3d_allocator.h"
#include <vector>

#include <CL/cl.h>

typedef struct {
    std::string       program_source;
    std::string       kernelY_FuncName;
    std::string       kernelUV_FuncName;
    D3DFORMAT   format;
    cl_program  clprogram;
    cl_kernel   clkernelY;
    cl_kernel   clkernelUV;
} OCL_YUV_kernel;

class OpenCLFilter
{
public:
    OpenCLFilter();
    virtual ~OpenCLFilter();
    cl_int OCLInit(IDirect3DDeviceManager9* pD3DDeviceManager);
    cl_int AddKernel(const char* filename, const char* kernelY_name, const char* kernelUV_name, D3DFORMAT format);
    cl_int SelectKernel(unsigned kNo);
    cl_int ProcessSurface(int width, int height, directxMemId* pSurfIn, directxMemId* pSurfOut);

    static std::string readFile(const char *filename);
private:
    cl_int InitPlatform();
    cl_int InitD3D9SurfaceSharingExtension();
    cl_int InitDevice();
    cl_int BuildKernels();
    cl_int SetKernelArgs();
    cl_int PrepareSharedSurfaces(int width, int height, directxMemId* inputD3DSurf, directxMemId* outputD3DSurf);
    cl_int ProcessSurface();
    void   ReleaseResources();

    static size_t chooseLocalSize(size_t globalSize, size_t preferred);

    bool                                m_bInit;
    int                                 m_activeKernel;
    int                                 m_currentWidth;
    int                                 m_currentHeight;
    D3DFORMAT                           m_currentFormat;
    std::auto_ptr<SafeD3DDeviceManager> m_SafeD3DDeviceManager;
    std::vector<OCL_YUV_kernel>         m_kernels;

    cl_platform_id      m_clplatform;
    cl_device_id        m_cldevice;
    cl_context          m_clcontext;
    cl_command_queue    m_clqueue;

    static const size_t c_shared_surfaces_num = 2; // In and Out
    static const size_t c_ocl_surface_buffers_num = 2*c_shared_surfaces_num; // YIn, UVIn, YOut, UVOut

    cl_mem              m_clbuffer[c_ocl_surface_buffers_num];
    HANDLE              m_hSharedSurfaces[c_shared_surfaces_num]; // d3d9 surface shared handles
    IDirect3DSurface9*  m_pSharedSurfaces[c_shared_surfaces_num];

    size_t              m_GlobalWorkSizeY[2];
    size_t              m_GlobalWorkSizeUV[2];
    size_t              m_LocalWorkSizeY[2];
    size_t              m_LocalWorkSizeUV[2];

//  logger for filter depends on build configuration
    Logger<
#ifdef _DEBUG
        Debug
#else
        Error
#endif
    > log;
};

#endif // #if defined(_WIN32) || defined(_WIN64)
