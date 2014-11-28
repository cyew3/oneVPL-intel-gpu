/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2005-2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "mfx_samples_config.h"

#if defined(_WIN32) || defined(_WIN64)

#include "opencl_filter_dx9.h"
#include "sample_defs.h"

// INTEL DX9 sharing functions declared deprecated in OpenCL 1.2
#pragma warning( disable : 4996 )

#define DX9_MEDIA_SHARING
#define DX_MEDIA_SHARING
#define CL_DX9_MEDIA_SHARING_INTEL_EXT

#include <CL/opencl.h>
#include <CL/cl_d3d9.h>

using std::endl;


clGetDeviceIDsFromDX9INTEL_fn            clGetDeviceIDsFromDX9INTEL             = NULL;
clCreateFromDX9MediaSurfaceINTEL_fn      clCreateFromDX9MediaSurfaceINTEL       = NULL;
clEnqueueAcquireDX9ObjectsINTEL_fn       clEnqueueAcquireDX9ObjectsINTEL        = NULL;
clEnqueueReleaseDX9ObjectsINTEL_fn       clEnqueueReleaseDX9ObjectsINTEL        = NULL;

OpenCLFilterDX9::OpenCLFilterDX9()
{
    for(size_t i=0;i<c_shared_surfaces_num;i++)
    {
        m_hSharedSurfaces[i] = NULL;
        m_pSharedSurfaces[i] = NULL;
    }
}

OpenCLFilterDX9::~OpenCLFilterDX9()
{
    m_SafeD3DDeviceManager.reset();
}

cl_int OpenCLFilterDX9::OCLInit(mfxHDL device)
{
    MSDK_CHECK_POINTER(device, MFX_ERR_NULL_PTR);

    // store IDirect3DDeviceManager9
    try
    {
        m_SafeD3DDeviceManager.reset(new SafeD3DDeviceManager(static_cast<IDirect3DDeviceManager9*>(device)));
    }
    catch (const std::exception &ex)
    {
        log.error() << ex.what() << endl;
        return CL_DEVICE_NOT_AVAILABLE;
    }

    return OpenCLFilterBase::OCLInit(device);
}

cl_int OpenCLFilterDX9::InitSurfaceSharingExtension()
{
    cl_int error = CL_SUCCESS;

    // Check if surface sharing is available
    size_t  len = 0;
    const size_t max_string_size = 1024;
    char extensions[max_string_size];
    error = clGetPlatformInfo(m_clplatform, CL_PLATFORM_EXTENSIONS, max_string_size, extensions, &len);
    log.info() << "OpenCLFilter: Platform extensions: " << extensions << endl;
    if(NULL == strstr(extensions, "cl_intel_dx9_media_sharing"))
    {
        log.error() << "OpenCLFilter: DX9 media sharing not available!" << endl;
        return CL_INVALID_PLATFORM;
    }

    // Hook up the d3d sharing extension functions that we need
    INIT_CL_EXT_FUNC(clGetDeviceIDsFromDX9INTEL);
    INIT_CL_EXT_FUNC(clCreateFromDX9MediaSurfaceINTEL);
    INIT_CL_EXT_FUNC(clEnqueueAcquireDX9ObjectsINTEL);
    INIT_CL_EXT_FUNC(clEnqueueReleaseDX9ObjectsINTEL);

    // Check for success
    if (!clGetDeviceIDsFromDX9INTEL ||
        !clCreateFromDX9MediaSurfaceINTEL ||
        !clEnqueueAcquireDX9ObjectsINTEL ||
        !clEnqueueReleaseDX9ObjectsINTEL)
    {
        log.error() << "OpenCLFilter: Couldn't get all of the media sharing routines" << endl;
        return CL_INVALID_PLATFORM;
    }

    return error;
}

cl_int OpenCLFilterDX9::InitDevice()
{
    log.debug() << "OpenCLFilter: Try to init OCL device" << endl;

    cl_int error = CL_SUCCESS;

    try
    {
        LockedD3DDevice pD3DDevice = m_SafeD3DDeviceManager->LockDevice();
        error = clGetDeviceIDsFromDX9INTEL(m_clplatform, CL_D3D9EX_DEVICE_INTEL, // note - you must use D3D9DeviceEx
                                           pD3DDevice.get(), CL_PREFERRED_DEVICES_FOR_DX9_INTEL, 1, &m_cldevice, NULL);
        if(error) {
            log.error() << "OpenCLFilter: clGetDeviceIDsFromDX9INTEL failed. Error code: " << error << endl;
            return error;
        }

        // Initialize the shared context
        cl_context_properties props[] =
        {
            CL_CONTEXT_D3D9EX_DEVICE_INTEL, (cl_context_properties)pD3DDevice.get(),
            CL_CONTEXT_PLATFORM, (cl_context_properties)m_clplatform,
            CL_CONTEXT_INTEROP_USER_SYNC, (cl_context_properties)CL_TRUE,
            0
        };
        m_clcontext = clCreateContext(props, 1, &m_cldevice, NULL, NULL, &error);
        if(error) {
            log.error() << "OpenCLFilter: clCreateContext failed. Error code: " << error << endl;
            return error;
        }
    }
    catch (const std::exception &ex)
    {
        log.error() << ex.what() << endl;
        return CL_DEVICE_NOT_AVAILABLE;
    }

    log.debug() << "OpenCLFilter: OCL device inited" << endl;

    return error;
}

cl_int OpenCLFilterDX9::PrepareSharedSurfaces(int width, int height, mfxMemId pSurfIn, mfxMemId pSurfOut)
{
    cl_int error = CL_SUCCESS;
    mfxStatus sts = MFX_ERR_NONE;
    IDirect3DSurface9 *inputD3DSurf = NULL;
    IDirect3DSurface9 *outputD3DSurf = NULL;
    HANDLE inputD3DSurfHandle = NULL;
    HANDLE outputD3DSurfHandle = NULL;

#if 1
    directxMemId* pDxInSurfMemId = static_cast<directxMemId*>(pSurfIn);
    directxMemId* pDxOutSurfMemId = static_cast<directxMemId*>(pSurfOut);

    inputD3DSurf = pDxInSurfMemId->m_surface;
    outputD3DSurf = pDxOutSurfMemId->m_surface;
    inputD3DSurfHandle = pDxInSurfMemId->m_handle;
    outputD3DSurfHandle = pDxOutSurfMemId->m_handle;
#else
    sts = m_pAlloc->GetHDL(m_pAlloc->pthis, pSurfIn, reinterpret_cast<mfxHDL*>(&inputD3DSurf));
    if(MFX_ERR_NONE != sts) return CL_INVALID_VALUE;

    sts = m_pAlloc->GetHDL(m_pAlloc->pthis, pSurfOut, reinterpret_cast<mfxHDL*>(&outputD3DSurf));
    if(MFX_ERR_NONE != sts) return CL_INVALID_VALUE;
#endif

    if(m_bInit)
    {
        m_currentWidth = width;
        m_currentHeight = height;

        //
        // Setup OpenCL buffers etc.
        //
        if(!m_clbuffer[0]) // Initialize OCL buffers in case of new workload
        {
            if(m_kernels[m_activeKernel].format == MFX_FOURCC_NV12)
            {
                // Working on NV12 surfaces

                // Associate the shared buffer with the kernel object
                m_clbuffer[0] = clCreateFromDX9MediaSurfaceINTEL(m_clcontext, CL_MEM_READ_ONLY, inputD3DSurf, inputD3DSurfHandle, 0, &error);
                if(error) {
                    log.error() << "clCreateFromDX9MediaSurfaceINTEL failed. Error code: " << error << endl;
                    return error;
                }
                m_clbuffer[1] = clCreateFromDX9MediaSurfaceINTEL(m_clcontext, CL_MEM_READ_ONLY, inputD3DSurf, inputD3DSurfHandle, 1, &error);
                if(error) {
                    log.error() << "clCreateFromDX9MediaSurfaceINTEL failed. Error code: " << error << endl;
                    return error;
                }
                m_clbuffer[2] = clCreateFromDX9MediaSurfaceINTEL(m_clcontext, CL_MEM_WRITE_ONLY, outputD3DSurf, outputD3DSurfHandle, 0, &error);
                if(error) {
                    log.error() << "clCreateFromDX9MediaSurfaceINTEL failed. Error code: " << error << endl;
                    return error;
                }
                m_clbuffer[3] = clCreateFromDX9MediaSurfaceINTEL(m_clcontext, CL_MEM_WRITE_ONLY, outputD3DSurf, outputD3DSurfHandle, 1, &error);
                if(error) {
                    log.error() << "clCreateFromDX9MediaSurfaceINTEL failed. Error code: " << error << endl;
                    return error;
                }

                // Work sizes for Y plane
                m_GlobalWorkSizeY[0] = m_currentWidth;
                m_GlobalWorkSizeY[1] = m_currentHeight;
                m_LocalWorkSizeY[0] = chooseLocalSize(m_GlobalWorkSizeY[0], 8);
                m_LocalWorkSizeY[1] = chooseLocalSize(m_GlobalWorkSizeY[1], 8);
                m_GlobalWorkSizeY[0] = m_LocalWorkSizeY[0]*(m_GlobalWorkSizeY[0]/m_LocalWorkSizeY[0]);
                m_GlobalWorkSizeY[1] = m_LocalWorkSizeY[1]*(m_GlobalWorkSizeY[1]/m_LocalWorkSizeY[1]);

                // Work size for UV plane
                m_GlobalWorkSizeUV[0] = m_currentWidth/2;
                m_GlobalWorkSizeUV[1] = m_currentHeight/2;
                m_LocalWorkSizeUV[0] = chooseLocalSize(m_GlobalWorkSizeUV[0], 8);
                m_LocalWorkSizeUV[1] = chooseLocalSize(m_GlobalWorkSizeUV[1], 8);
                m_GlobalWorkSizeUV[0] = m_LocalWorkSizeUV[0]*(m_GlobalWorkSizeUV[0]/m_LocalWorkSizeUV[0]);
                m_GlobalWorkSizeUV[1] = m_LocalWorkSizeUV[1]*(m_GlobalWorkSizeUV[1]/m_LocalWorkSizeUV[1]);

                error = SetKernelArgs();
                if (error) return error;
            }
            else
            {
                log.error() << "OpenCLFilter: Unsupported image format" << endl;
                return CL_INVALID_VALUE;
            }
        }

        return error;
    }
    else
        return CL_DEVICE_NOT_FOUND;
}

cl_int OpenCLFilterDX9::ProcessSurface()
{
    cl_int error = CL_SUCCESS;

    if(!m_bInit)
        error = CL_DEVICE_NOT_FOUND;

    if(m_clbuffer[0] && CL_SUCCESS == error)
    {
        if(m_kernels[m_activeKernel].format == MFX_FOURCC_NV12)
        {
            cl_mem    surfaces[4] ={m_clbuffer[0],
                                    m_clbuffer[1],
                                    m_clbuffer[2],
                                    m_clbuffer[3]};

            error = clEnqueueAcquireDX9ObjectsINTEL(m_clqueue, 4, surfaces, 0, NULL, NULL);
            if(error) {
                log.error() << "clEnqueueAcquireDX9ObjectsINTEL failed. Error code: " << error << endl;
                return error;
            }

            error = clEnqueueNDRangeKernel(m_clqueue, m_kernels[m_activeKernel].clkernelY, 2, NULL, m_GlobalWorkSizeY, m_LocalWorkSizeY, 0, NULL, NULL);
            if(error) {
                log.error() << "clEnqueueNDRangeKernel for Y plane failed. Error code: " << error << endl;
                return error;
            }

            error = clEnqueueNDRangeKernel(m_clqueue, m_kernels[m_activeKernel].clkernelUV, 2, NULL, m_GlobalWorkSizeUV, m_LocalWorkSizeUV, 0, NULL, NULL);
            if(error) {
                log.error() << "clEnqueueNDRangeKernel for UV plane failed. Error code: " << error << endl;
                return error;
            }

            error = clEnqueueReleaseDX9ObjectsINTEL(m_clqueue,4, surfaces,0,NULL,NULL);
            if(error) {
                log.error() << "clEnqueueReleaseDX9ObjectsINTEL failed. Error code: " << error << endl;
                return error;
            }

            // flush & finish the command queue
            error = clFlush(m_clqueue);
            if(error) {
                log.error() << "clFlush failed. Error code: " << error << endl;
                return error;
            }
            error = clFinish(m_clqueue);
            if(error) {
                log.error() << "clFinish failed. Error code: " << error << endl;
                return error;
            }
        }
    }
    return error;
}

#endif // #if defined(_WIN32) || defined(_WIN64)
