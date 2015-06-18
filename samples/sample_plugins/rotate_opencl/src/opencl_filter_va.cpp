/**********************************************************************************

 INTEL CORPORATION PROPRIETARY INFORMATION
 This software is supplied under the terms of a license agreement or nondisclosure
 agreement with Intel Corporation and may not be copied or disclosed except in
 accordance with the terms of that agreement
 Copyright(c) 2014 Intel Corporation. All Rights Reserved.

***********************************************************************************/

#if !defined(_WIN32) && !defined(_WIN64)

#include "opencl_filter_va.h"
#include "sample_defs.h"

using std::endl;

clGetDeviceIDsFromVA_APIMediaAdapterINTEL_fn    clGetDeviceIDsFromVA_APIMediaAdapterINTEL = NULL;
clCreateFromVA_APIMediaSurfaceINTEL_fn          clCreateFromVA_APIMediaSurfaceINTEL       = NULL;
clEnqueueAcquireVA_APIMediaSurfacesINTEL_fn     clEnqueueAcquireVA_APIMediaSurfacesINTEL  = NULL;
clEnqueueReleaseVA_APIMediaSurfacesINTEL_fn     clEnqueueReleaseVA_APIMediaSurfacesINTEL  = NULL;

OpenCLFilterVA::OpenCLFilterVA()
{
    for(size_t i = 0; i < c_shared_surfaces_num; i++)
    {
        m_SharedSurfaces[i] = VA_INVALID_ID;
    }
}

OpenCLFilterVA::~OpenCLFilterVA() {
}

cl_int OpenCLFilterVA::OCLInit(mfxHDL device)
{
    MSDK_CHECK_POINTER(device, MFX_ERR_NULL_PTR);

    m_vaDisplay = (VADisplay*)device;

    return OpenCLFilterBase::OCLInit(device);
}

cl_int OpenCLFilterVA::InitSurfaceSharingExtension()
{
    cl_int error = CL_SUCCESS;

    // Hook up the d3d sharing extension functions that we need
    INIT_CL_EXT_FUNC(clGetDeviceIDsFromVA_APIMediaAdapterINTEL);
    INIT_CL_EXT_FUNC(clCreateFromVA_APIMediaSurfaceINTEL);
    INIT_CL_EXT_FUNC(clEnqueueAcquireVA_APIMediaSurfacesINTEL);
    INIT_CL_EXT_FUNC(clEnqueueReleaseVA_APIMediaSurfacesINTEL);

    // Check for success
    if (!clGetDeviceIDsFromVA_APIMediaAdapterINTEL ||
        !clCreateFromVA_APIMediaSurfaceINTEL ||
        !clEnqueueAcquireVA_APIMediaSurfacesINTEL ||
        !clEnqueueReleaseVA_APIMediaSurfacesINTEL)
    {
        log.error() << "OpenCLFilter: Couldn't get all of the media sharing routines" << endl;
        return CL_INVALID_PLATFORM;
    }

    return error;
}

cl_int OpenCLFilterVA::InitDevice()
{
    cl_int error = CL_SUCCESS;
    log.debug() << "OpenCLFilter: Try to init OCL device" << endl;

    cl_uint nDevices = 0;
    error = clGetDeviceIDsFromVA_APIMediaAdapterINTEL(m_clplatform, CL_VA_API_DISPLAY_INTEL,
                                        m_vaDisplay, CL_PREFERRED_DEVICES_FOR_VA_API_INTEL, 1, &m_cldevice, &nDevices);
    if(error) {
        log.error() << "OpenCLFilter: clGetDeviceIDsFromVA_APIMediaAdapterINTEL failed. Error code: " << error << endl;
        return error;
    }

    if (!nDevices)
        return CL_INVALID_PLATFORM;


    // Initialize the shared context
    cl_context_properties props[] = { CL_CONTEXT_VA_API_DISPLAY_INTEL, (cl_context_properties) m_vaDisplay, CL_CONTEXT_INTEROP_USER_SYNC, 1, 0};
    m_clcontext = clCreateContext(props, 1, &m_cldevice, NULL, NULL, &error);

    if(error) {
        log.error() << "OpenCLFilter: clCreateContext failed. Error code: " << error << endl;
        return error;
    }

    log.debug() << "OpenCLFilter: OCL device inited" << endl;

    return error;
}

cl_mem OpenCLFilterVA::CreateSharedSurface(mfxMemId mid, int nView, bool bIsReadOnly)
{
    VASurfaceID* surf = NULL;
    mfxStatus sts = m_pAlloc->GetHDL(m_pAlloc->pthis, mid, reinterpret_cast<mfxHDL*>(&surf));
    if (sts) return 0;

    cl_int error = CL_SUCCESS;
    cl_mem mem = clCreateFromVA_APIMediaSurfaceINTEL(m_clcontext, bIsReadOnly ? CL_MEM_READ_ONLY : CL_MEM_READ_WRITE,
                                            surf, nView, &error);
    if (error) {
        log.error() << "clCreateFromVA_APIMediaSurfaceINTEL failed. Error code: " << error << endl;
        return 0;
    }
    return mem;
}


bool OpenCLFilterVA::EnqueueAcquireSurfaces(cl_mem* surfaces, int nSurfaces)
{
    cl_int error = clEnqueueAcquireVA_APIMediaSurfacesINTEL(m_clqueue, nSurfaces, surfaces, 0, NULL, NULL);
    if (error) {
        log.error() << "clEnqueueAcquireVA_APIMediaSurfacesINTEL failed. Error code: " << error << endl;
        return false;
    }
    return true;
}

bool OpenCLFilterVA::EnqueueReleaseSurfaces(cl_mem* surfaces, int nSurfaces)
{
    cl_int error = clEnqueueReleaseVA_APIMediaSurfacesINTEL(m_clqueue, nSurfaces, surfaces, 0, NULL, NULL);
    if (error) {
        log.error() << "clEnqueueReleaseVA_APIMediaSurfacesINTEL failed. Error code: " << error << endl;
        return false;
    }
    return true;
}

#endif // #if !defined(_WIN32) && !defined(_WIN64)
