/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2005-2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "mfx_samples_config.h"

#if defined(_WIN32) || defined(_WIN64)

#include <fstream>

#include "opencl_filter.h"
#include "sample_defs.h"

#include "stdexcept"

// INTEL DX9 sharing functions declared deprecated in OpenCL 1.2
#pragma warning( disable : 4996 )

// define for <CL/cl_ext.h>
#define DX9_MEDIA_SHARING
#define DX_MEDIA_SHARING

#include <CL/opencl.h>

using std::endl;

#define INIT_CL_EXT_FUNC(x)                x = (x ## _fn)clGetExtensionFunctionAddress(#x);
#define SAFE_OCL_FREE(P, FREE_FUNC)        { if (P) { FREE_FUNC(P); P = NULL; } }

clGetDeviceIDsFromDX9INTEL_fn            clGetDeviceIDsFromDX9INTEL             = NULL;
clCreateFromDX9MediaSurfaceINTEL_fn      clCreateFromDX9MediaSurfaceINTEL       = NULL;
clEnqueueAcquireDX9ObjectsINTEL_fn       clEnqueueAcquireDX9ObjectsINTEL        = NULL;
clEnqueueReleaseDX9ObjectsINTEL_fn       clEnqueueReleaseDX9ObjectsINTEL        = NULL;

OpenCLFilter::OpenCLFilter() : log(std::clog)
{
    m_bInit               = false;
    m_activeKernel        = 0;
    for(int i=0;i<c_shared_surfaces_num;i++)
    {
        m_hSharedSurfaces[i]    = NULL;
        m_pSharedSurfaces[i]    = NULL;
    }
    for(int i=0;i<c_ocl_surface_buffers_num;i++)
        m_clbuffer[i]            = NULL;

    m_clplatform         = 0;
    m_clqueue            = 0;
    m_clcontext          = 0;

    m_currentWidth       = 0;
    m_currentHeight      = 0;
    m_currentFormat      = D3DFMT_UNKNOWN;
}

OpenCLFilter::~OpenCLFilter()
{
    ReleaseResources();

    for(unsigned i=0;i<m_kernels.size();i++)
    {
        SAFE_OCL_FREE(m_kernels[i].clprogram, clReleaseProgram);
        SAFE_OCL_FREE(m_kernels[i].clkernelY, clReleaseKernel);
        SAFE_OCL_FREE(m_kernels[i].clkernelUV, clReleaseKernel);
    }

    SAFE_OCL_FREE(m_clqueue,clReleaseCommandQueue);
    SAFE_OCL_FREE(m_clcontext,clReleaseContext);
    m_SafeD3DDeviceManager.reset();
}

void OpenCLFilter::ReleaseResources()
{
    for(int i=0;i<c_shared_surfaces_num;i++)
    {
        MSDK_SAFE_RELEASE(m_pSharedSurfaces[i]);
        m_hSharedSurfaces[i] = NULL;
    }
    for(int i=0;i<c_ocl_surface_buffers_num;i++)
        SAFE_OCL_FREE(m_clbuffer[i], clReleaseMemObject);
}

cl_int OpenCLFilter::OCLInit(IDirect3DDeviceManager9* pD3DDeviceManager)
{
    MSDK_CHECK_POINTER(pD3DDeviceManager, MFX_ERR_NULL_PTR);

    // store IDirect3DDeviceManager9
    try
    {
        m_SafeD3DDeviceManager.reset(new SafeD3DDeviceManager(pD3DDeviceManager));
    }
    catch (const std::exception &ex)
    {
        log.error() << ex.what() << endl;
        return CL_DEVICE_NOT_AVAILABLE;
    }

    cl_int error = CL_SUCCESS;

    error = InitPlatform();
    if (error) return error;

    error = InitD3D9SurfaceSharingExtension();
    if (error) return error;

    error = InitDevice();
    if (error) return error;

    error = BuildKernels();
    if (error) return error;

    // Create a command queue
    log.debug() << "OpenCLFilter: Creating command queue" << endl;
    m_clqueue = clCreateCommandQueue(m_clcontext, m_cldevice, 0, &error);
    if (error) return error;
    log.debug() << "OpenCLFilter: Command queue created" << endl;

    m_bInit = true;

    return error;
}

cl_int OpenCLFilter::InitPlatform()
{
    cl_int error = CL_SUCCESS;

    // Determine the number of installed OpenCL platforms
    cl_uint num_platforms = 0;
    error = clGetPlatformIDs(0, NULL, &num_platforms);
    if(error)
    {
        log.error() << "OpenCLFilter: Couldn't get platform IDs. "
                        "Make sure your platform "
                        "supports OpenCL and can find a proper dll." << endl;
        return error;
    }

    // Get all of the handles to the installed OpenCL platforms
    std::vector<cl_platform_id> platforms(num_platforms);
    error = clGetPlatformIDs(num_platforms, &platforms[0], &num_platforms);
    if(error) {
        log.error() << "OpenCLFilter: Failed to get OCL platform IDs. Error Code; " << error;
        return error;
    }

    // Find the platform handle for the installed Gen driver
    const size_t max_string_size = 1024;
    char platform[max_string_size];
    for (unsigned int platform_index = 0; platform_index < num_platforms; platform_index++)
    {
        error = clGetPlatformInfo(platforms[platform_index], CL_PLATFORM_NAME, max_string_size, platform, NULL);
        if(error) return error;

        if(strstr(platform, "Intel")) // Use only Intel platfroms
        {
            log.info() << "OpenCL platform \"" << platform << "\" is used" << endl;
            m_clplatform = platforms[platform_index];
        }
    }
    if (0 == m_clplatform)
    {
        log.error() << "OpenCLFilter: Didn't find an Intel platform!" << endl;
        return CL_INVALID_PLATFORM;
    }

    return error;
}

cl_int OpenCLFilter::InitD3D9SurfaceSharingExtension()
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

cl_int OpenCLFilter::InitDevice()
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
        cl_context_properties props[] = { CL_CONTEXT_D3D9EX_DEVICE_INTEL, (cl_context_properties)pD3DDevice.get(), NULL};
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

cl_int OpenCLFilter::BuildKernels()
{
    log.debug() << "OpenCLFilter: Reading and compiling OCL kernels" << endl;

    cl_int error = CL_SUCCESS;

    char buildOptions[] = "-I. -Werror -cl-fast-relaxed-math";

    for(unsigned i=0; i<m_kernels.size(); i++)
    {
        // Create a program object from the source file
        const char *program_source_buf = m_kernels[i].program_source.c_str();

        log.debug() << "OpenCLFilter: program source:\n" << program_source_buf << endl;
        m_kernels[i].clprogram = clCreateProgramWithSource(m_clcontext, 1, &program_source_buf, NULL, &error);
        if(error) {
            log.error() << "OpenCLFilter: clCreateProgramWithSource failed. Error code: " << error << endl;
            return error;
        }

        // Build OCL kernel
        error = clBuildProgram(m_kernels[i].clprogram, 1, &m_cldevice, buildOptions, NULL, NULL);
        if (error == CL_BUILD_PROGRAM_FAILURE)
        {
            size_t buildLogSize = 0;
            cl_int logStatus = clGetProgramBuildInfo (m_kernels[i].clprogram, m_cldevice, CL_PROGRAM_BUILD_LOG, 0, NULL, &buildLogSize);
            std::vector<char> buildLog(buildLogSize + 1);
            logStatus = clGetProgramBuildInfo (m_kernels[i].clprogram, m_cldevice, CL_PROGRAM_BUILD_LOG, buildLogSize, &buildLog[0], NULL);
            log.error() << std::string(buildLog.begin(), buildLog.end()) << endl;
            return error;
        }
        else if (error)
            return error;

        // Create the kernel objects
        m_kernels[i].clkernelY = clCreateKernel(m_kernels[i].clprogram, m_kernels[i].kernelY_FuncName.c_str(), &error);
        if (error) {
            log.error() << "OpenCLFilter: clCreateKernel failed. Error code: " << error << endl;
            return error;
        }

        m_kernels[i].clkernelUV = clCreateKernel(m_kernels[i].clprogram, m_kernels[i].kernelUV_FuncName.c_str(), &error);
        if (error) {
            log.error() << "OpenCLFilter: clCreateKernel failed. Error code: " << error << endl;
            return error;
        }
    }

    log.debug() << "OpenCLFilter: " << m_kernels.size() << " kernels built" << endl;

    return error;
}

cl_int OpenCLFilter::AddKernel(const char* filename, const char* kernelY_name, const char* kernelUV_name, D3DFORMAT format)
{
    OCL_YUV_kernel kernel;
    kernel.program_source = std::string(filename);
    kernel.kernelY_FuncName = std::string(kernelY_name);
    kernel.kernelUV_FuncName = std::string(kernelUV_name);
    kernel.format = format;
    kernel.clprogram = 0;
    kernel.clkernelY = kernel.clkernelUV = 0;
    m_kernels.push_back(kernel);
    return CL_SUCCESS;
}

cl_int OpenCLFilter::SelectKernel(unsigned kNo)
{
    if(m_bInit)
    {
        if(kNo < m_kernels.size())
        {
            if(m_kernels[m_activeKernel].format != m_kernels[kNo].format)
                ReleaseResources(); // Kernel format changed, OCL buffers must be released & recreated

            m_activeKernel = kNo;

            return CL_SUCCESS;
        }
        else {
            return CL_INVALID_PROGRAM;
        }
    }
    else
        return CL_DEVICE_NOT_FOUND;
}

cl_int OpenCLFilter::SetKernelArgs()
{
    cl_int error = CL_SUCCESS;

    // set kernelY parameters
    error = clSetKernelArg(m_kernels[m_activeKernel].clkernelY, 0, sizeof(cl_mem), &m_clbuffer[0]);
    if(error) {
        log.error() << "clSetKernelArg failed. Error code: " << error << endl;
        return error;
    }
    error = clSetKernelArg(m_kernels[m_activeKernel].clkernelY, 1, sizeof(cl_mem), &m_clbuffer[2]);
    if(error) {
        log.error() << "clSetKernelArg failed. Error code: " << error << endl;
        return error;
    }

    // set kernelUV parameters
    error = clSetKernelArg(m_kernels[m_activeKernel].clkernelUV, 0, sizeof(cl_mem), &m_clbuffer[1]);
    if(error) {
        log.error() << "clSetKernelArg failed. Error code: " << error << endl;
        return error;
    }
    error = clSetKernelArg(m_kernels[m_activeKernel].clkernelUV, 1, sizeof(cl_mem), &m_clbuffer[3]);
    if(error) {
        log.error() << "clSetKernelArg failed. Error code: " << error << endl;
        return error;
    }

    return error;
}

cl_int OpenCLFilter::PrepareSharedSurfaces(int width, int height, IDirect3DSurface9* pD3DSurf)
{
    cl_int error = CL_SUCCESS;

    if(m_bInit)
    {
        HRESULT hr;

        if(!(m_currentWidth == width && m_currentHeight == height && m_currentFormat == m_kernels[m_activeKernel].format))
        {
            if(m_hSharedSurfaces[0])
                ReleaseResources(); // Release existing surfaces

            //
            // Setup share surfaces/handles
            //
            try
            {
                LockedD3DDevice pD3DDevice = m_SafeD3DDeviceManager->LockDevice();
                for(int i=0;i<c_shared_surfaces_num;i++)
                {
                    // Create the input surface
                    hr = pD3DDevice.get()->CreateOffscreenPlainSurface(
                                               width,
                                               height,
                                               m_kernels[m_activeKernel].format,
                                               D3DPOOL_DEFAULT,
                                               &m_pSharedSurfaces[i],
                                               &m_hSharedSurfaces[i]);
                    if (FAILED(hr))
                    {
                        log.error() << "OpenCLFilter: Couldn't create surfaces" << endl;
                        return CL_DEVICE_NOT_AVAILABLE;
                    }
                }
            }
            catch (const std::exception &ex)
            {
                log.error() << ex.what() << endl;
                return CL_DEVICE_NOT_AVAILABLE;
            }

            m_currentWidth        = width;
            m_currentHeight       = height;
            m_currentFormat       = m_kernels[m_activeKernel].format;
        }

        //
        // Setup OpenCL buffers etc.
        //
        if(!m_clbuffer[0]) // Initialize OCL buffers in case of new workload
        {
            if(m_kernels[m_activeKernel].format == D3DFMT_NV12)
            {
                // Working on NV12 surfaces

                // Associate the shared buffer with the kernel object
                m_clbuffer[0] = clCreateFromDX9MediaSurfaceINTEL(m_clcontext,0,m_pSharedSurfaces[0],m_hSharedSurfaces[0],0,&error);
                if(error) {
                    log.error() << "clCreateFromDX9MediaSurfaceINTEL failed. Error code: " << error << endl;
                    return error;
                }
                m_clbuffer[1] = clCreateFromDX9MediaSurfaceINTEL(m_clcontext,0,m_pSharedSurfaces[0],m_hSharedSurfaces[0],1,&error);
                if(error) {
                    log.error() << "clCreateFromDX9MediaSurfaceINTEL failed. Error code: " << error << endl;
                    return error;
                }
                m_clbuffer[2] = clCreateFromDX9MediaSurfaceINTEL(m_clcontext,0,m_pSharedSurfaces[1],m_hSharedSurfaces[1],0,&error);
                if(error) {
                    log.error() << "clCreateFromDX9MediaSurfaceINTEL failed. Error code: " << error << endl;
                    return error;
                }
                m_clbuffer[3] = clCreateFromDX9MediaSurfaceINTEL(m_clcontext,0,m_pSharedSurfaces[1],m_hSharedSurfaces[1],1,&error);
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


        // Copy the decoded surface to the input shared surface
        error = CopySurface(pD3DSurf, m_pSharedSurfaces[0]);

        return error;
    }
    else
        return CL_DEVICE_NOT_FOUND;
}

cl_int OpenCLFilter::ProcessSurface()
{
    cl_int error = CL_SUCCESS;

    if(!m_bInit)
        error = CL_DEVICE_NOT_FOUND;

    if(m_clbuffer[0] && CL_SUCCESS == error)
    {
        if(m_kernels[m_activeKernel].format == D3DFMT_NV12)
        {
            cl_mem    surfaces[4] ={m_clbuffer[0],
                                    m_clbuffer[1],
                                    m_clbuffer[2],
                                    m_clbuffer[3]};

            error = clEnqueueAcquireDX9ObjectsINTEL(m_clqueue,4,surfaces,0,NULL,NULL);
            if(error) {
                log.error() << "clEnqueueAcquireDX9ObjectsINTEL failed. Error code: " << error << endl;
                return error;
            }

            // enqueue kernels
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

cl_int OpenCLFilter::CopySurface(IDirect3DSurface9* pSrc, IDirect3DSurface9* pDst)
{
    D3DSURFACE_DESC src_desc, dst_desc;
    HRESULT hr = S_OK;
    hr = pSrc->GetDesc(&src_desc);
    if (FAILED(hr)) return CL_DEVICE_NOT_AVAILABLE;

    hr = pDst->GetDesc(&dst_desc);
    if (FAILED(hr)) return CL_DEVICE_NOT_AVAILABLE;

    RECT src_rect = {0, 0, src_desc.Width, src_desc.Height};
    RECT dst_rect = {0, 0, dst_desc.Width, dst_desc.Height};

    try
    {
        LockedD3DDevice pD3DDevice = m_SafeD3DDeviceManager->LockDevice();
        HRESULT hr = pD3DDevice.get()->StretchRect(
                                           pSrc,
                                           &src_rect,
                                           pDst,
                                           &dst_rect,
                                           D3DTEXF_NONE);
        if (FAILED(hr))
            return CL_DEVICE_NOT_AVAILABLE;
    }
    catch (const std::exception &ex)
    {
        log.error() << ex.what() << endl;
        return CL_DEVICE_NOT_AVAILABLE;
    }
    return CL_SUCCESS;
}

cl_int OpenCLFilter::ProcessSurface(int width, int height, IDirect3DSurface9* pSurfIn, IDirect3DSurface9* pSurfOut)
{
    cl_int error = CL_SUCCESS;

    error = PrepareSharedSurfaces(width, height, pSurfIn);
    if (error) return error;

    error = ProcessSurface();
    if (error) return error;

    error = CopySurface(m_pSharedSurfaces[1], pSurfOut);
    if (error) return error;

    return error;
}

std::string OpenCLFilter::readFile(const char *filename)
{
    std::ifstream input(filename, std::ios::in | std::ios::binary);
    if(!input.good())
    {
        // look in folder with executable
        input.clear();
        const size_t module_length = 1024;
        char module_name[module_length];
        GetModuleFileNameA(0, module_name, module_length);
        char *p = strrchr(module_name, '\\');
        if (p)
        {
            strncpy_s(p + 1, module_length - (p + 1 - module_name), filename, _TRUNCATE);
            input.open(module_name, std::ios::binary);
        }
    }

    if (!input)
        return std::string("Error_opening_file_\"") + std::string(filename) + std::string("\"");

    input.seekg(0, std::ios::end);
    std::vector<char> program_source(static_cast<int>(input.tellg().seekpos()));
    input.seekg(0);

    input.read(&program_source[0], program_source.size());

    return std::string(program_source.begin(), program_source.end());
}

size_t OpenCLFilter::chooseLocalSize(size_t globalSize, // frame width or height
                                     size_t preferred)  // preferred local size
{
    size_t ret = 1;
    while ((globalSize % ret == 0) && ret <= preferred)
    {
         ret <<= 1;
    }

    return ret >> 1;
}

#endif // #if defined(_WIN32) || defined(_WIN64)
