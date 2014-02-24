/**********************************************************************************

 INTEL CORPORATION PROPRIETARY INFORMATION
 This software is supplied under the terms of a license agreement or nondisclosure
 agreement with Intel Corporation and may not be copied or disclosed except in
 accordance with the terms of that agreement
 Copyright(c) 2014 Intel Corporation. All Rights Reserved.

***********************************************************************************/

#if !defined(_WIN32) && !defined(_WIN64)

#include <fstream>

#include "opencl_filter_linux.h"
#include "sample_defs.h"
#include <stdexcept>
#include <utility>
#include <functional>
#include <iostream>

using std::endl;

#define INIT_CL_EXT_FUNC(x)                x = (x ## _fn)clGetExtensionFunctionAddress(#x);
#define SAFE_OCL_FREE(P, FREE_FUNC)        { if (P) { FREE_FUNC(P); P = NULL; } }

clGetDeviceIDsFromVA_APIMediaAdapterINTEL_fn    clGetDeviceIDsFromVA_APIMediaAdapterINTEL = NULL;
clCreateFromVA_APIMediaSurfaceINTEL_fn          clCreateFromVA_APIMediaSurfaceINTEL       = NULL;
clEnqueueAcquireVA_APIMediaSurfacesINTEL_fn     clEnqueueAcquireVA_APIMediaSurfacesINTEL  = NULL;
clEnqueueReleaseVA_APIMediaSurfacesINTEL_fn     clEnqueueReleaseVA_APIMediaSurfacesINTEL  = NULL;

OpenCLFilter::OpenCLFilter() : log(std::clog)
{
    m_bInit                  = false;
    m_bSharedSurfacesCreated = false;
    m_activeKernel        = 0;

    for(int i=0;i<c_shared_surfaces_num;i++)
    {
        m_pSharedSurfaces[i]    = NULL;
    }

    for(int i=0;i<c_ocl_surface_buffers_num;i++)
        m_clbuffer[i]            = NULL;

    m_clplatform         = 0;
    m_clqueue            = 0;
    m_clcontext          = 0;

    m_currentWidth       = 0;
    m_currentHeight      = 0;
    m_currentFormat      = 0;
}

OpenCLFilter::~OpenCLFilter() {
//     fclose(m_pFile);
}
#if 0
bool isInit = false;

void OpenCLFilter::DumpToFile(VASurfaceID *pSrc)
{
    printf("Dumping +\n");

    if (!isInit) {
        m_pFile = fopen("/svn/dump_in.yuv","wb");
        isInit = true;
    }

    VAImage vaImage;

    memset(&vaImage, 0, sizeof(VAImage));
    VAStatus va_res = vaDeriveImage(m_vaDisplay, *pSrc, &vaImage);

    mfxU8* data = NULL;
    if (VA_STATUS_SUCCESS == va_res) {
        printf("vaDeriveImage is successful\n");
        va_res = vaMapBuffer(m_vaDisplay, vaImage.buf, (void **)&data);
    }
    else
        printf("vaDeriveImage isn't successful\n");
    if (VA_STATUS_SUCCESS == va_res) {
        printf("vaMapBuffer is successful\n");
    }
    else
        printf("vaMapBuffer isn't successful\n");

    printf("vaImage.format.fourcc = %x\n",vaImage.format.fourcc); fflush(NULL);
    printf("offsets[0] = %d, offsets[1] = %d\n", vaImage.offsets[0], vaImage.offsets[1]); fflush(NULL);
    printf("pitches[0] = %d, pitches[1] = %d\n", vaImage.pitches[0], vaImage.pitches[1]); fflush(NULL);

    printf("vaImage.height = %d  vaImage.width = %d\n",vaImage.height, vaImage.width); fflush(NULL);

    printf("pFile = %p",m_pFile);
    mfxU8* curr;

    if (m_pFile != NULL) {
        for(int i = 0; i < vaImage.height; i++)
        {
            curr = data + vaImage.offsets[0] + vaImage.pitches[0]*i;
            fwrite(curr, vaImage.width, 1, m_pFile);
        }

        for(int i = 0; i < vaImage.height/2; i++)
        {
            curr = data + vaImage.offsets[1] + vaImage.pitches[1]*i;
            fwrite(curr, vaImage.width, 1, m_pFile);
        }
    }

    vaUnmapBuffer(m_vaDisplay, vaImage.buf);
    vaDestroyImage(m_vaDisplay, vaImage.image_id);

//    exit(-1);
    printf("Dumping -\n");

}
#endif

cl_int OpenCLFilter::ReleaseResources() {
    cl_int error = CL_SUCCESS;

    for(int i=0;i<c_ocl_surface_buffers_num;i++)
    {
        error = clReleaseMemObject( m_clbuffer[i] );
        if(error) {
            log.error() << "clReleaseMemObject failed. Error code: " << error << endl;
            return error;
        }
    }

    error = clFinish( m_clqueue );
    if(error)
    {
        log.error() << "clFinish failed. Error code: " << error << endl;
        return error;
    }

    for(int i=0;i<c_ocl_surface_buffers_num;i++)
    {
        m_clbuffer[i] = NULL;
    }

    return error;
}

cl_int OpenCLFilter::OCLInit(VADisplay* pD3DDeviceManager)
{
    MSDK_CHECK_POINTER(pD3DDeviceManager, MFX_ERR_NULL_PTR);
    m_vaDisplay = *pD3DDeviceManager;
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
        log.error() << "OpenCLFilter: Couldn't get platform IDs. \
                        Make sure your platform \
                        supports OpenCL and can find a proper library." << endl;
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
    if(NULL == strstr(extensions, "cl_intel_va_api_media_sharing"))
    {
        log.error() << "OpenCLFilter: VAAPI media sharing is not available!" << endl;
        return CL_INVALID_PLATFORM;
    }

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

cl_int OpenCLFilter::InitDevice()
{
    cl_int error = CL_SUCCESS;
    log.debug() << "OpenCLFilter: Try to init OCL device" << endl;

    cl_uint nDevices = 0;
    error = clGetDeviceIDsFromVA_APIMediaAdapterINTEL(m_clplatform, CL_VA_API_DISPLAY_INTEL,
                                        m_vaDisplay, CL_PREFERRED_DEVICES_FOR_VA_API_INTEL, 1, &m_cldevice, &nDevices);
    if(error) {
        log.error() << "OpenCLFilter: clGetDeviceIDsFromDX9INTEL failed. Error code: " << error << endl;
        return error;
    }

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

cl_int OpenCLFilter::BuildKernels()
{
    log.debug() << "OpenCLFilter: Reading and compiling OCL kernels" << endl;

    cl_int error = CL_SUCCESS;

    char buildOptions[] = "-I. -Werror -cl-fast-relaxed-math";

    for(unsigned int i = 0; i < m_kernels.size(); i++)
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

cl_int OpenCLFilter::AddKernel(const char* filename, const char* kernelY_name, const char* kernelUV_name, int format)
{
    OCL_YUV_kernel kernel;
    kernel.program_source = std::string(filename);
    kernel.kernelY_FuncName = std::string(kernelY_name);
    kernel.kernelUV_FuncName = std::string(kernelUV_name);
    kernel.format = VA_RT_FORMAT_YUV420;
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

cl_int OpenCLFilter::PrepareSharedSurfaces(int width, int height, VASurfaceID* inputD3DSurf, VASurfaceID* outputD3DSurf)
{
    cl_int error = CL_SUCCESS;
    int va_res = VA_STATUS_SUCCESS;

    if(m_bInit) {
        if (!m_bSharedSurfacesCreated) {

            m_bSharedSurfacesCreated = true;
        }

        m_currentWidth = width;
        m_currentHeight = height;
        m_currentFormat = m_kernels[m_activeKernel].format;

        // Setup OpenCL buffers etc.
        if(!m_clbuffer[0]) // Initialize OCL buffers in case of new workload
        {
            if(m_kernels[m_activeKernel].format == VA_RT_FORMAT_YUV420)
            {
                // Associate the shared buffer with the kernel object
                m_clbuffer[0] = clCreateFromVA_APIMediaSurfaceINTEL(m_clcontext, CL_MEM_READ_ONLY, inputD3DSurf, 0, &error);
                if(error) {
                    log.error() << "clCreateFromVA_APIMediaSurfaceINTEL failed. Error code: " << error << endl;
                    return error;
                }
                m_clbuffer[1] = clCreateFromVA_APIMediaSurfaceINTEL(m_clcontext, CL_MEM_READ_ONLY, inputD3DSurf, 1, &error);
                if(error) {
                    log.error() << "clCreateFromVA_APIMediaSurfaceINTEL failed. Error code: " << error << endl;
                    return error;
                }
                m_clbuffer[2] = clCreateFromVA_APIMediaSurfaceINTEL(m_clcontext, CL_MEM_READ_ONLY, outputD3DSurf, 0, &error);
                if(error) {
                    log.error() << "clCreateFromVA_APIMediaSurfaceINTEL failed. Error code: " << error << endl;
                    return error;
                }
                m_clbuffer[3] = clCreateFromVA_APIMediaSurfaceINTEL(m_clcontext, CL_MEM_WRITE_ONLY, outputD3DSurf, 1, &error);
                if(error) {
                    log.error() << "clCreateFromVA_APIMediaSurfaceINTEL failed. Error code: " << error << endl;
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
            } else {
                log.error() << "OpenCLFilter: Unsupported image format" << endl;
                return CL_INVALID_VALUE;
            }
        }
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
        if(m_kernels[m_activeKernel].format == VA_RT_FORMAT_YUV420)
        {
            cl_mem    surfaces[4] ={m_clbuffer[0],
                                    m_clbuffer[1],
                                    m_clbuffer[2],
                                    m_clbuffer[3]};

            error = clEnqueueAcquireVA_APIMediaSurfacesINTEL(m_clqueue,4,surfaces,0,NULL,NULL);
            if(error) {
                log.error() << "clEnqueueAcquireVA_APIMediaSurfacesINTEL failed. Error code: " << error << endl;
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

            error = clEnqueueReleaseVA_APIMediaSurfacesINTEL(m_clqueue,4, surfaces,0,NULL,NULL);
            if(error) {
                log.error() << "clEnqueueReleaseVA_APIMediaSurfacesINTEL failed. Error code: " << error << endl;
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

cl_int OpenCLFilter::ProcessSurface(int width, int height, VASurfaceID* pSurfIn, VASurfaceID* pSurfOut)
{
    cl_int error = CL_SUCCESS;

    //DumpToFile(pSurfIn);

    error = PrepareSharedSurfaces(width, height, pSurfIn, pSurfOut);
    if (error) return error;

    error = ProcessSurface();
    if (error) return error;

    error =  ReleaseResources();
    return error;
}

std::string OpenCLFilter::readFile(const char *filename)
{
    char* mfx_home = getenv("MFX_HOME");

    // TODO need to process exception
    if (!mfx_home) throw std::logic_error("MFX_HOME environment variable was not set!");

    std::string path =
        std::string(mfx_home) +
        std::string("/") +
        std::string("samples/sample_user_modules/rotate_opencl/src/") +
        std::string(filename);

    std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);

    input.seekg(0, std::ios::end);
    std::vector<char> program_source(static_cast<int>(input.tellg()));
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

#endif // #if !defined(_WIN32) && !defined(_WIN64)
