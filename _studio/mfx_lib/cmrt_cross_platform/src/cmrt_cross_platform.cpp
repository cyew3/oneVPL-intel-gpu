/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2013-2016 Intel Corporation. All Rights Reserved.
//
*/

#if !defined(OSX)

#include "assert.h"
#include "vm_shared_object.h"
#include "cmrt_cross_platform.h"

#ifdef WIN64

#   ifdef CMRT_EMU
        const vm_char * DLL_NAME_DX9  = VM_STRING("igfxcmrt64_emu.dll");
        const vm_char * DLL_NAME_DX11 = VM_STRING("igfx11cmrt64_emu.dll");
#   else //CMRT_EMU
        const vm_char * DLL_NAME_DX9  = VM_STRING("igfxcmrt64.dll");
        const vm_char * DLL_NAME_DX11 = VM_STRING("igfx11cmrt64.dll");
#   endif //CMRT_EMU

#elif defined WIN32

#   ifdef CMRT_EMU
        const vm_char * DLL_NAME_DX9  = VM_STRING("igfxcmrt32_emu.dll");
        const vm_char * DLL_NAME_DX11 = VM_STRING("igfx11cmrt32_emu.dll");
#   else //CMRT_EMU
    const vm_char * DLL_NAME_DX9  = VM_STRING("igfxcmrt32.dll");
    const vm_char * DLL_NAME_DX11 = VM_STRING("igfx11cmrt32.dll");
#   endif //CMRT_EMU

#elif defined(LINUX64)
const vm_char * DLL_NAME_LINUX = VM_STRING("igfxcmrt64.so");
#elif defined(LINUX32)
const vm_char * DLL_NAME_LINUX = VM_STRING("igfxcmrt32.so");
#else
#error "undefined configuration"
#endif

#ifdef CMRT_EMU
    const char * FUNC_NAME_CREATE_CM_DEVICE  = "CreateCmDeviceEmu";
    const char * FUNC_NAME_CREATE_CM_DEVICE_EX  = "CreateCmDeviceEmu";
    const char * FUNC_NAME_DESTROY_CM_DEVICE = "DestroyCmDeviceEmu";
#else //CMRT_EMU
    const char * FUNC_NAME_CREATE_CM_DEVICE  = "CreateCmDevice";
    const char * FUNC_NAME_CREATE_CM_DEVICE_EX  = "CreateCmDeviceEx";
    const char * FUNC_NAME_DESTROY_CM_DEVICE = "DestroyCmDevice";
#endif //CMRT_EMU


#define IMPL_FOR_ALL(RET, FUNC, PROTO, PARAM)   \
    RET FUNC PROTO {                            \
        switch (m_platform) {                   \
        case DX9:   return m_dx9->FUNC PARAM;   \
        case DX11:  return m_dx11->FUNC PARAM;  \
        case VAAPI: return m_linux->FUNC PARAM; \
        default:    return CM_NOT_IMPLEMENTED;  \
        }                                       \
    }

#define IMPL_FOR_DX(RET, FUNC, PROTO, PARAM)    \
    RET FUNC PROTO {                            \
        switch (m_platform) {                   \
        case DX9:   return m_dx9->FUNC PARAM;   \
        case DX11:  return m_dx11->FUNC PARAM;  \
        default:    return CM_NOT_IMPLEMENTED;  \
        }                                       \
    }

#define IMPL_FOR_DX11(RET, FUNC, PROTO, PARAM)  \
    RET FUNC PROTO {                            \
        switch (m_platform) {                   \
        case DX11:  return m_dx11->FUNC PARAM;  \
        default:    return CM_NOT_IMPLEMENTED;  \
        }                                       \
    }

#ifndef __GNUC__

#include "dxgiformat.h"

#define CONVERT_FORMAT(FORMAT) \
    (m_platform == DX11 ? (D3DFORMAT)ConvertFormatToDx11(FORMAT) : (FORMAT))

namespace
{
    DXGI_FORMAT ConvertFormatToDx11(D3DFORMAT format)
    {
        switch ((UINT)format) {
        case CM_SURFACE_FORMAT_UNKNOWN:     return DXGI_FORMAT_UNKNOWN;
        case CM_SURFACE_FORMAT_A8R8G8B8:    return DXGI_FORMAT_B8G8R8A8_UNORM;
        case CM_SURFACE_FORMAT_X8R8G8B8:    return DXGI_FORMAT_B8G8R8X8_UNORM;
        case CM_SURFACE_FORMAT_A8:          return DXGI_FORMAT_A8_UNORM;
        case CM_SURFACE_FORMAT_P8:          return DXGI_FORMAT_P8;
        case CM_SURFACE_FORMAT_R32F:        return DXGI_FORMAT_R32_FLOAT;
        case CM_SURFACE_FORMAT_NV12:        return DXGI_FORMAT_NV12;
        case CM_SURFACE_FORMAT_UYVY:        return DXGI_FORMAT_R8G8_B8G8_UNORM;
        case CM_SURFACE_FORMAT_YUY2:        return DXGI_FORMAT_YUY2;
        case CM_SURFACE_FORMAT_V8U8:        return DXGI_FORMAT_R8G8_SNORM;
        default: assert(0);                 return DXGI_FORMAT_UNKNOWN;
        }
    }
};

#else /* __GNUC__ */

#define CONVERT_FORMAT(FORMAT) (FORMAT)

#endif /* __GNUC__ */

enum { DX9=1, DX11=2, VAAPI=3 };

typedef INT (* CreateCmDeviceDx9FuncTypeEx)(CmDx9::CmDevice *&, UINT &, IDirect3DDeviceManager9 *, UINT);
typedef INT (* CreateCmDeviceDx11FuncTypeEx)(CmDx11::CmDevice *&, UINT &, ID3D11Device *, UINT);
typedef INT (* CreateCmDeviceLinuxFuncTypeEx)(CmLinux::CmDevice *&, UINT &, VADisplay, UINT);
typedef INT (* CreateCmDeviceDx9FuncType)(CmDx9::CmDevice *&, UINT &, IDirect3DDeviceManager9 *);
typedef INT (* CreateCmDeviceDx11FuncType)(CmDx11::CmDevice *&, UINT &, ID3D11Device *);
typedef INT (* CreateCmDeviceLinuxFuncType)(CmLinux::CmDevice *&, UINT &, VADisplay);
typedef INT (* DestroyCmDeviceDx9FuncType)(CmDx9::CmDevice *&);
typedef INT (* DestroyCmDeviceDx11FuncType)(CmDx11::CmDevice *&);
typedef INT (* DestroyCmDeviceVAAPIFuncType)(CmLinux::CmDevice *&);

class CmDeviceImpl : public CmDevice
{
public:
    vm_so_handle    m_dll;
    INT             m_platform;

    union
    {
        CmDx9::CmDevice *       m_dx9;
        CmDx11::CmDevice *      m_dx11;
        CmLinux::CmDevice *     m_linux;
    };

    virtual ~CmDeviceImpl(){}

    INT GetPlatform(){return m_platform;};

    INT GetDevice(AbstractDeviceHandle & pDevice)
    {
        switch (m_platform) {
        case DX9:   return m_dx9->GetD3DDeviceManager((IDirect3DDeviceManager9 *&)pDevice);
        case DX11:  return m_dx11->GetD3D11Device((ID3D11Device *&)pDevice);
        case VAAPI: return CM_NOT_IMPLEMENTED;
        default:    return CM_NOT_IMPLEMENTED;
        }
    }

    INT CreateSurface2D(AbstractSurfaceHandle pD3DSurf, CmSurface2D *& pSurface)
    {
        switch (m_platform) {
        case DX9:   return m_dx9->CreateSurface2D((IDirect3DSurface9 *)pD3DSurf, pSurface);
        case DX11:  return m_dx11->CreateSurface2D((ID3D11Texture2D *)pD3DSurf, pSurface);
        case VAAPI: return m_linux->CreateSurface2D(*(VASurfaceID*)pD3DSurf, pSurface);
        default:    return CM_NOT_IMPLEMENTED;
        }
    }
/*
     INT CreateSurface2D(AbstractSurfaceHandle * pD3DSurf, const UINT surfaceCount, CmSurface2D ** pSurface)
     {
         switch (m_platform) {
         case DX9:   return m_dx9->CreateSurface2D((IDirect3DSurface9 **)pD3DSurf, surfaceCount, pSurface);
         case DX11:  return m_dx11->CreateSurface2D((ID3D11Texture2D **)pD3DSurf, surfaceCount, pSurface);
         case VAAPI: return m_linux->CreateSurface2D((VASurfaceID *)pD3DSurf, surfaceCount, pSurface);
         default:    return CM_NOT_IMPLEMENTED;
         }
     }
*/
    INT CreateSurface2DSubresource(AbstractSurfaceHandle pD3D11Texture2D, UINT subresourceCount, CmSurface2D ** ppSurfaces, UINT & createdSurfaceCount, UINT option)
    {
        switch (m_platform) {
        case DX9:   return CM_NOT_IMPLEMENTED;
        case DX11:  return m_dx11->CreateSurface2DSubresource((ID3D11Texture2D *)pD3D11Texture2D, subresourceCount, ppSurfaces, createdSurfaceCount, option);
        case VAAPI: return CM_NOT_IMPLEMENTED;
        default:    return CM_NOT_IMPLEMENTED;
        }
    }

    INT CreateSurface2DbySubresourceIndex(AbstractSurfaceHandle pD3D11Texture2D, UINT FirstArraySlice, UINT FirstMipSlice, CmSurface2D *& pSurface)
    {
        switch (m_platform) {
        case DX9:   return CM_NOT_IMPLEMENTED;
        case DX11:  return m_dx11->CreateSurface2DbySubresourceIndex((ID3D11Texture2D *)pD3D11Texture2D, FirstArraySlice, FirstMipSlice, pSurface);
        case VAAPI: return CM_NOT_IMPLEMENTED;
        default:    return CM_NOT_IMPLEMENTED;
        }
    }

    IMPL_FOR_ALL(INT, CreateBuffer, (UINT size, CmBuffer *& pSurface), (size, pSurface));
    IMPL_FOR_ALL(INT, CreateSurface2D, (UINT width, UINT height, CM_SURFACE_FORMAT format, CmSurface2D* & pSurface), (width, height, CONVERT_FORMAT(format), pSurface));
    IMPL_FOR_ALL(INT, CreateSurface3D, (UINT width, UINT height, UINT depth, CM_SURFACE_FORMAT format, CmSurface3D* & pSurface), (width, height, depth, CONVERT_FORMAT(format), pSurface));
    IMPL_FOR_ALL(INT, DestroySurface, (CmSurface2D *& pSurface), (pSurface));
    IMPL_FOR_ALL(INT, DestroySurface, (CmBuffer *& pSurface), (pSurface));
    IMPL_FOR_ALL(INT, DestroySurface, (CmSurface3D *& pSurface), (pSurface));
    IMPL_FOR_ALL(INT, CreateQueue, (CmQueue *& pQueue), (pQueue));
    IMPL_FOR_ALL(INT, LoadProgram, (void * pCommonISACode, const UINT size, CmProgram *& pProgram, const char * options), (pCommonISACode, size, pProgram, options));
    IMPL_FOR_ALL(INT, CreateKernel, (CmProgram * pProgram, const char * kernelName, CmKernel *& pKernel, const char * options), (pProgram, kernelName, pKernel, options));
    IMPL_FOR_ALL(INT, CreateKernel, (CmProgram * pProgram, const char * kernelName, const void * fncPnt, CmKernel *& pKernel, const char * options), (pProgram, kernelName, fncPnt, pKernel, options));
    IMPL_FOR_ALL(INT, CreateSampler, (const CM_SAMPLER_STATE & sampleState, CmSampler *& pSampler), (sampleState, pSampler));
    IMPL_FOR_ALL(INT, DestroyKernel, (CmKernel *& pKernel), (pKernel));
    IMPL_FOR_ALL(INT, DestroySampler, (CmSampler *& pSampler), (pSampler));
    IMPL_FOR_ALL(INT, DestroyProgram, (CmProgram *& pProgram), (pProgram));
    IMPL_FOR_ALL(INT, DestroyThreadSpace, (CmThreadSpace *& pTS), (pTS));
    IMPL_FOR_ALL(INT, CreateTask, (CmTask *& pTask), (pTask));
    IMPL_FOR_ALL(INT, DestroyTask, (CmTask *& pTask), (pTask));
    IMPL_FOR_ALL(INT, GetCaps, (CM_DEVICE_CAP_NAME capName, size_t & capValueSize, void * pCapValue), (capName, capValueSize, pCapValue));
    IMPL_FOR_ALL(INT, CreateVmeStateG6, (const VME_STATE_G6 & vmeState, CmVmeState *& pVmeState), (vmeState, pVmeState));
    IMPL_FOR_ALL(INT, DestroyVmeStateG6, (CmVmeState *& pVmeState), (pVmeState));
    IMPL_FOR_ALL(INT, CreateVmeSurfaceG6, (CmSurface2D * pCurSurface, CmSurface2D * pForwardSurface, CmSurface2D * pBackwardSurface, SurfaceIndex *& pVmeIndex), (pCurSurface, pForwardSurface, pBackwardSurface, pVmeIndex));
    IMPL_FOR_ALL(INT, DestroyVmeSurfaceG6, (SurfaceIndex *& pVmeIndex), (pVmeIndex));
    IMPL_FOR_ALL(INT, CreateThreadSpace, (UINT width, UINT height, CmThreadSpace *& pTS), (width, height, pTS));
    IMPL_FOR_ALL(INT, CreateBufferUP, (UINT size, void * pSystMem, CmBufferUP *& pSurface), (size, pSystMem, pSurface));
    IMPL_FOR_ALL(INT, DestroyBufferUP, (CmBufferUP *& pSurface), (pSurface));
    IMPL_FOR_ALL(INT, GetSurface2DInfo, (UINT width, UINT height, CM_SURFACE_FORMAT format, UINT & pitch, UINT & physicalSize), (width, height, CONVERT_FORMAT(format), pitch, physicalSize));
    IMPL_FOR_ALL(INT, CreateSurface2DUP, (UINT width, UINT height, CM_SURFACE_FORMAT format, void * pSysMem, CmSurface2DUP *& pSurface), (width, height, CONVERT_FORMAT(format), pSysMem, pSurface));
    IMPL_FOR_ALL(INT, DestroySurface2DUP, (CmSurface2DUP *& pSurface), (pSurface));
    IMPL_FOR_ALL(INT, CreateVmeSurfaceG7_5 , (CmSurface2D * pCurSurface, CmSurface2D ** pForwardSurface, CmSurface2D ** pBackwardSurface, const UINT surfaceCountForward, const UINT surfaceCountBackward, SurfaceIndex *& pVmeIndex), (pCurSurface, pForwardSurface, pBackwardSurface, surfaceCountForward, surfaceCountBackward, pVmeIndex));
    IMPL_FOR_ALL(INT, DestroyVmeSurfaceG7_5, (SurfaceIndex *& pVmeIndex), (pVmeIndex));
    IMPL_FOR_ALL(INT, CreateSampler8x8, (const CM_SAMPLER_8X8_DESCR & smplDescr, CmSampler8x8 *& psmplrState), (smplDescr, psmplrState));
    IMPL_FOR_ALL(INT, DestroySampler8x8, (CmSampler8x8 *& pSampler), (pSampler));
    IMPL_FOR_ALL(INT, CreateSampler8x8Surface, (CmSurface2D * p2DSurface, SurfaceIndex *& pDIIndex, CM_SAMPLER8x8_SURFACE surf_type, CM_SURFACE_ADDRESS_CONTROL_MODE mode), (p2DSurface, pDIIndex, surf_type, mode));
    IMPL_FOR_ALL(INT, DestroySampler8x8Surface, (SurfaceIndex* & pDIIndex), (pDIIndex));
    IMPL_FOR_ALL(INT, CreateThreadGroupSpace, (UINT thrdSpaceWidth, UINT thrdSpaceHeight, UINT grpSpaceWidth, UINT grpSpaceHeight, CmThreadGroupSpace *& pTGS), (thrdSpaceWidth, thrdSpaceHeight, grpSpaceWidth, grpSpaceHeight, pTGS));
    IMPL_FOR_ALL(INT, DestroyThreadGroupSpace, (CmThreadGroupSpace *& pTGS), (pTGS));
    IMPL_FOR_ALL(INT, SetL3Config, (L3_CONFIG_REGISTER_VALUES * l3_c), (l3_c));
    IMPL_FOR_ALL(INT, SetSuggestedL3Config, (L3_SUGGEST_CONFIG l3_s_c), (l3_s_c));
    IMPL_FOR_ALL(INT, SetCaps, (CM_DEVICE_CAP_NAME capName, size_t capValueSize, void * pCapValue), (capName, capValueSize, pCapValue));
    IMPL_FOR_ALL(INT, CreateGroupedVAPlusSurface, (CmSurface2D * p2DSurface1, CmSurface2D * p2DSurface2, SurfaceIndex *& pDIIndex, CM_SURFACE_ADDRESS_CONTROL_MODE mode), (p2DSurface1, p2DSurface2, pDIIndex, mode));
    IMPL_FOR_ALL(INT, DestroyGroupedVAPlusSurface, (SurfaceIndex *& pDIIndex), (pDIIndex));
    IMPL_FOR_ALL(INT, CreateSamplerSurface2D, (CmSurface2D * p2DSurface, SurfaceIndex *& pSamplerSurfaceIndex), (p2DSurface, pSamplerSurfaceIndex));
    IMPL_FOR_ALL(INT, CreateSamplerSurface3D, (CmSurface3D * p3DSurface, SurfaceIndex *& pSamplerSurfaceIndex), (p3DSurface, pSamplerSurfaceIndex));
    IMPL_FOR_ALL(INT, DestroySamplerSurface, (SurfaceIndex *& pSamplerSurfaceIndex), (pSamplerSurfaceIndex));
    IMPL_FOR_ALL(INT, GetRTDllVersion, (CM_DLL_FILE_VERSION * pFileVersion), (pFileVersion));
    IMPL_FOR_ALL(INT, GetJITDllVersion, (CM_DLL_FILE_VERSION * pFileVersion), (pFileVersion));
    IMPL_FOR_ALL(INT, InitPrintBuffer, (size_t printbufsize), (printbufsize));
    IMPL_FOR_ALL(INT, FlushPrintBuffer, (), ());
};

#ifndef __GNUC__
INT CreateCmDevice(CmDevice *& pD, UINT & version, IDirect3DDeviceManager9 * pD3DDeviceMgr, UINT mode )
{
    CmDeviceImpl * device = new CmDeviceImpl;

    device->m_platform = DX9;
    device->m_dll = vm_so_load(DLL_NAME_DX9);
    if (device->m_dll == 0)
    {
        delete device;
        return CM_FAILURE;
    }

    CreateCmDeviceDx9FuncTypeEx createFunc = (CreateCmDeviceDx9FuncTypeEx)vm_so_get_addr(device->m_dll, FUNC_NAME_CREATE_CM_DEVICE_EX);

    if (createFunc == 0)
    {
        delete device;
        return CM_FAILURE;
    }


    INT res = createFunc(device->m_dx9, version, pD3DDeviceMgr, mode);

    if (res != CM_SUCCESS)
    {
        delete device;
        return CM_FAILURE;
    }

    pD = device;
    return CM_SUCCESS;
}


INT CreateCmDevice(CmDevice* &pD, UINT& version, ID3D11Device * pD3D11Device, UINT mode )
{
    CmDeviceImpl * device = new CmDeviceImpl;

    device->m_platform = DX11;
    device->m_dll = vm_so_load(DLL_NAME_DX11);
    if (device->m_dll == 0)
    {
        delete device;
        return CM_FAILURE;
    }


    CreateCmDeviceDx11FuncTypeEx createFunc = (CreateCmDeviceDx11FuncTypeEx)vm_so_get_addr(device->m_dll, FUNC_NAME_CREATE_CM_DEVICE_EX);

    if (createFunc == 0)
    {
        delete device;
        return CM_FAILURE;
    }


    INT res = createFunc(device->m_dx11, version, pD3D11Device, mode);

    if (res != CM_SUCCESS)
    {
        delete device;
        return CM_FAILURE;
    }

    pD = device;
    return CM_SUCCESS;
}

#else /* #ifndef __GNUC__ */

INT CreateCmDevice(CmDevice *& pD, UINT & version, VADisplay va_dpy, UINT mode)
{
    CmDeviceImpl * device = new CmDeviceImpl;

    device->m_platform = VAAPI;
    device->m_dll = vm_so_load(DLL_NAME_LINUX);
    if (device->m_dll == 0)
    {
        delete device;
        return CM_FAILURE;
    }

    CreateCmDeviceLinuxFuncTypeEx createFunc = (CreateCmDeviceLinuxFuncTypeEx)vm_so_get_addr(device->m_dll, FUNC_NAME_CREATE_CM_DEVICE_EX);
    if (createFunc == 0)
    {
        delete device;
        return CM_FAILURE;
    }

    INT res = createFunc(device->m_linux, version, va_dpy, mode);
    if (res != CM_SUCCESS)
    {
        delete device;
        return CM_FAILURE;
    }

    pD = device;
    return CM_SUCCESS;
}

#endif /* #ifndef __GNUC__ */

INT DestroyCmDevice(CmDevice *& pD)
{
    CmDeviceImpl * device = (CmDeviceImpl *)pD;

    if (pD == 0 || device->m_dll == 0)
        return CM_SUCCESS;

    INT res = CM_SUCCESS;

    if (vm_so_func destroyFunc = vm_so_get_addr(device->m_dll, FUNC_NAME_DESTROY_CM_DEVICE))
    {
        switch (device->GetPlatform())
        {
        case DX9:
            res = ((DestroyCmDeviceDx9FuncType)destroyFunc)(device->m_dx9);
            break;
        case DX11:
            res = ((DestroyCmDeviceDx11FuncType)destroyFunc)(device->m_dx11);
            break;
        case VAAPI:
            res = ((DestroyCmDeviceVAAPIFuncType)destroyFunc)(device->m_linux);
            break;
        }
    }

    vm_so_free(device->m_dll);

    device->m_dll  = 0;
    device->m_dx9  = 0;
    device->m_dx11 = 0;
    device->m_linux = 0;

    delete device;
    pD = 0;
    return res;
}

int ReadProgram(CmDevice * device, CmProgram *& program, const unsigned char * buffer, unsigned int len)
{
#ifdef CMRT_EMU
    buffer; len;
    return device->LoadProgram(0, 0, program, "nojitter");
#else //CMRT_EMU
    return device->LoadProgram((void *)buffer, len, program, "nojitter");
#endif //CMRT_EMU
}

int ReadProgramJit(CmDevice * device, CmProgram *& program, const unsigned char * buffer, unsigned int len)
{
#ifdef CMRT_EMU
    buffer; len;
    return device->LoadProgram(0, 0, program);
#else //CMRT_EMU
    return device->LoadProgram((void *)buffer, len, program);
#endif //CMRT_EMU
}

int CreateKernel(CmDevice * device, CmProgram * program, const char * kernelName, const void * fncPnt, CmKernel *& kernel, const char * options)
{
#ifdef CMRT_EMU
    return device->CreateKernel(program, kernelName, fncPnt, kernel, options);
#else //CMRT_EMU
    fncPnt;
    return device->CreateKernel(program, kernelName, kernel, options);
#endif //CMRT_EMU
}

CmEvent *CM_NO_EVENT = ((CmEvent *)(-1));

#endif // !defined(OSX)

