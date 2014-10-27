/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/
//#include "ipps.h"

#define MFX_VA

#include "mfx_common.h"

#include <fstream>
#include <algorithm>
#include <string>
#include <stdexcept> /* for std exceptions on Linux/Android */

#include "libmfx_core_interface.h"
//#include "cm_def.h"
//#include "cm_vm.h"
#include "mfx_camera_plugin_utils.h"
#include "genx_hsw_camerapipe_isa.h"

namespace
{

using MfxCameraPlugin::CmRuntimeError;


CmProgram * ReadProgram(CmDevice * device, const mfxU8 * buffer, size_t len)
{
    int result = CM_SUCCESS;
    CmProgram * program = 0;

    if ((result = ::ReadProgramJit(device, program, buffer, (mfxU32)len)) != CM_SUCCESS)
        throw CmRuntimeError();

    return program;
}

CmKernel * CreateKernel(CmDevice * device, CmProgram * program, char const * name, void * funcptr)
{
    int result = CM_SUCCESS;
    CmKernel * kernel = 0;

    if ((result = ::CreateKernel(device, program, name, funcptr, kernel)) != CM_SUCCESS)
        throw CmRuntimeError();

    return kernel;
}

SurfaceIndex & GetIndex(CmSurface2D * surface)
{
    SurfaceIndex * index = 0;
    int result = surface->GetIndex(index);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return *index;
}

SurfaceIndex & GetIndex(CmSurface2DUP * surface)
{
    SurfaceIndex * index = 0;
    int result = surface->GetIndex(index);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return *index;
}


SurfaceIndex & GetIndex(CmBuffer * buffer)
{
    SurfaceIndex * index = 0;
    int result = buffer->GetIndex(index);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return *index;
}

SurfaceIndex & GetIndex(CmBufferUP * buffer)
{
    SurfaceIndex * index = 0;
    int result = buffer->GetIndex(index);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return *index;
}

void Read(CmBuffer * buffer, void * buf, CmEvent * e = 0)
{
    int result = CM_SUCCESS;
    if ((result = buffer->ReadSurface(reinterpret_cast<unsigned char *>(buf), e)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void Write(CmBuffer * buffer, void * buf, CmEvent * e = 0)
{
    int result = CM_SUCCESS;
    if ((result = buffer->WriteSurface(reinterpret_cast<unsigned char *>(buf), e)) != CM_SUCCESS)
        throw CmRuntimeError();
}

};


namespace MfxCameraPlugin
{

CmDevice * TryCreateCmDevicePtr(VideoCORE * core, mfxU32 * version)
{
    mfxU32 versionPlaceholder = 0;
    if (version == 0)
        version = &versionPlaceholder;

    CmDevice * device = 0;

    int result = CM_SUCCESS;
    if (core->GetVAType() == MFX_HW_D3D9)
    {
#if defined(_WIN32) || defined(_WIN64)
        D3D9Interface * d3dIface = QueryCoreInterface<D3D9Interface>(core, MFXICORED3D_GUID);
        if (d3dIface == 0)
            return 0;
        if ((result = ::CreateCmDevice(device, *version, d3dIface->GetD3D9DeviceManager())) != CM_SUCCESS)
            return 0;
#endif
    }
    else if (core->GetVAType() == MFX_HW_D3D11)
    {
#if defined(_WIN32) || defined(_WIN64)
        D3D11Interface * d3dIface = QueryCoreInterface<D3D11Interface>(core, MFXICORED3D11_GUID);
        if (d3dIface == 0)
            return 0;
        if ((result = ::CreateCmDevice(device, *version, d3dIface->GetD3D11Device())) != CM_SUCCESS)
            return 0;
#endif
    }
    else if (core->GetVAType() == MFX_HW_VAAPI)
    {
        //throw std::logic_error("GetDeviceManager not implemented on Linux for Look Ahead");
#if defined(MFX_VA_LINUX)
        VADisplay display;
        mfxStatus res = core->GetHandle(MFX_HANDLE_VA_DISPLAY, &display); // == MFX_HANDLE_RESERVED2
        if (res != MFX_ERR_NONE || !display)
            return 0;

        if ((result = ::CreateCmDevice(device, *version, display)) != CM_SUCCESS)
            return 0;
#endif
    }

    return device;
}

CmDevice * CreateCmDevicePtr(VideoCORE * core, mfxU32 * version)
{
    CmDevice * device = TryCreateCmDevicePtr(core, version);
    if (device == 0)
        throw CmRuntimeError();
    return device;
}

CmDevicePtr::CmDevicePtr(CmDevice * device)
    : m_device(device)
{
}

CmDevicePtr::~CmDevicePtr()
{
    Reset(0);
}

void CmDevicePtr::Reset(CmDevice * device)
{
    if (m_device)
    {
        int result = ::DestroyCmDevice(m_device);
        result; assert(result == CM_SUCCESS);
    }
    m_device = device;
}

CmDevice * CmDevicePtr::operator -> ()
{
    return m_device;
}

CmDevicePtr & CmDevicePtr::operator = (CmDevice * device)
{
    Reset(device);
    return *this;
}

CmDevicePtr::operator CmDevice * ()
{
    return m_device;
}

CmBuffer * CreateBuffer(CmDevice * device, mfxU32 size)
{
    CmBuffer * buffer;
    int result = device->CreateBuffer(size, buffer);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return buffer;
}

CmBufferUP * CreateBuffer(CmDevice * device, mfxU32 size, void * mem)
{
    CmBufferUP * buffer;
    int result = device->CreateBufferUP(size, mem, buffer);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return buffer;
}


CmSurface2D * CreateSurface(CmDevice * device, IDirect3DSurface9 * d3dSurface)
{
    int result = CM_SUCCESS;
    CmSurface2D * cmSurface = 0;
    if (device && d3dSurface && (result = device->CreateSurface2D(d3dSurface, cmSurface)) != CM_SUCCESS)
        throw CmRuntimeError();
    return cmSurface;
}


CmSurface2D * CreateSurface(CmDevice * device, ID3D11Texture2D * d3dSurface)
{
    int result = CM_SUCCESS;
    CmSurface2D * cmSurface = 0;
    if (device && d3dSurface && (result = device->CreateSurface2D(d3dSurface, cmSurface)) != CM_SUCCESS)
        throw CmRuntimeError();
    return cmSurface;
}


CmSurface2D * CreateSurface2DSubresource(CmDevice * device, ID3D11Texture2D * d3dSurface)
{
    int result = CM_SUCCESS;
    CmSurface2D * cmSurface = 0;
    mfxU32 cmSurfaceCount = 1;
    if (device && d3dSurface && (result = device->CreateSurface2DSubresource(d3dSurface, 1, &cmSurface, cmSurfaceCount)) != CM_SUCCESS)
        throw CmRuntimeError();
    return cmSurface;
}


CmSurface2D * CreateSurface(CmDevice * device, AbstractSurfaceHandle vaSurface)
{
    int result = CM_SUCCESS;
    CmSurface2D * cmSurface = 0;
    if (device && (vaSurface) && (result = device->CreateSurface2D(vaSurface, cmSurface)) != CM_SUCCESS)
        throw CmRuntimeError();
    return cmSurface;
}


CmSurface2D * CreateSurface(CmDevice * device, mfxHDL nativeSurface, eMFXVAType vatype)
{
    switch (vatype)
    {
    case MFX_HW_D3D9:
        return CreateSurface(device, (IDirect3DSurface9 *)nativeSurface);
    case MFX_HW_D3D11:
        return CreateSurface2DSubresource(device, (ID3D11Texture2D *)nativeSurface);
    case MFX_HW_VAAPI:
        return CreateSurface(device, nativeSurface);
    default:
        throw CmRuntimeError();
    }
}


CmSurface2D * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc)
{
    int result = CM_SUCCESS;
    CmSurface2D * cmSurface = 0;
    if (device && (result = device->CreateSurface2D(width, height, CM_SURFACE_FORMAT(fourcc), cmSurface)) != CM_SUCCESS)
        throw CmRuntimeError();
    return cmSurface;
}


CmSurface2DUP * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc, void * mem)
{
    int result = CM_SUCCESS;
    CmSurface2DUP * cmSurface = 0;
    if (device && (result = device->CreateSurface2DUP(width, height, CM_SURFACE_FORMAT(fourcc), mem, cmSurface)) != CM_SUCCESS)
        throw CmRuntimeError();
    return cmSurface;
}


CmContext::CmContext()
: m_device(0)
, m_program(0)
, m_queue(0)
{
}

CmContext::CmContext(
    CameraFrameSizeExtra const & video,
    CmDevice *          cmDevice,
    mfxCameraCaps *     pCaps,
    eMFXHWType platform)
{
    m_nTiles = video.tileNum;
    Zero(task_BayerCorrection);
    Zero(task_Padding);
    Zero(task_GoodPixelCheck);
    Zero(task_RestoreGreen);
    Zero(task_RestoreBlueRed);
    Zero(task_SAD);
    Zero(task_DecideAvg);
//  Zero(task_CheckConfidence);
//  Zero(task_BadPixelCheck);
    Zero(task_3x3CCM);
    Zero(task_FwGamma);
    Zero(task_ARGB);
    kernel_ARGB     = 0;
    kernel_FwGamma  = 0;
    kernel_FwGamma1 = 0;

    m_platform = platform;
    Setup(video, cmDevice, pCaps);
}

void CmContext::CreateCameraKernels()
{
    int i;
    bool bNeedConversionToRGB = true;

    if (!m_caps.bNoPadding)
    {
        kernel_padding16bpp = CreateKernel(m_device, m_program, (CM_KERNEL_FUNCTION(Padding_16bpp)), NULL); // padding to be done
    }

    if (m_caps.bBlackLevelCorrection || m_caps.bWhiteBalance || m_caps.bVignetteCorrection)
    {
        // BlackLevel correction, White balance and vignette filter share the same kernel.
        kernel_BayerCorrection = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(BAYER_CORRECTION), NULL);
    }

    kernel_good_pixel_check = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GOOD_PIXEL_CHECK), NULL);

    for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(RESTOREG), NULL);
    }

    for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(RESTOREBandR), NULL);
    }

    if (m_video.BitDepth != 16)
    {
        for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
        {
            CAM_PIPE_KERNEL_ARRAY(kernel_sad, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(SAD), NULL);
        }
    }
    else
    {
        for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
        {
            CAM_PIPE_KERNEL_ARRAY(kernel_sad, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(SAD_16), NULL);
        }
    }

    for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        CAM_PIPE_KERNEL_ARRAY(kernel_decide_average, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(DECIDE_AVG), NULL);
    }

    if (m_caps.bForwardGammaCorrection && !m_caps.bColorConversionMatrix)
    {
        // Only gamma correction needed w/o color corection

        if (m_video.BitDepth == 16)
        {
            kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_ONLY_16bits), NULL);
            // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
            kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_ONLY_16bits), NULL);
        }
        else
        {
            if (m_caps.bOutToARGB16)
            {
                kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_ONLY), NULL);
                // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
                kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_ONLY), NULL);
            }
            else
            {
                kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_GPUV4_ARGB8_2D), NULL);
                // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
                kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_GPUV4_ARGB8_2D), NULL);

                // This kernel does a conversion, so separate conversion is not needed.
                bNeedConversionToRGB = false;
            }
        }
    }
    else if (m_caps.bForwardGammaCorrection && m_caps.bColorConversionMatrix)
    {
        // Both gamma correction and color corection needed

        if (m_video.BitDepth == 16)
        {
            kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_AND_GAMMA_16bits), NULL);
            // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
            kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_AND_GAMMA_16bits), NULL);
        }
        else
        {
            kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_AND_GAMMA), NULL);
            // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
            kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_AND_GAMMA), NULL);
        }
    }
    else if (! m_caps.bForwardGammaCorrection && m_caps.bColorConversionMatrix)
    {
        // color corection needed w/o gamma correction
        kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_ONLY), NULL);
        // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
        kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CCM_ONLY), NULL);

    }

    if (m_caps.bOutToARGB16)
    {
        kernel_ARGB = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(ARGB16), NULL);
    }
    else if (bNeedConversionToRGB)
    {
        kernel_ARGB = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(ARGB8), NULL);
    }
}

void CmContext::CreateThreadSpaces(CameraFrameSizeExtra *pFrameSize)
{
    mfxU32 sliceWidth = pFrameSize->vSliceWidth;
    int    result = CM_SUCCESS;

    heightIn16 = (pFrameSize->TileHeightPadded + 15) >> 4;
    widthIn16  = (pFrameSize->paddedFrameWidth  + 15) >> 4;

    sliceHeightIn16_np = (pFrameSize->TileHeight + 15) >> 4;
    sliceWidthIn16_np  = (sliceWidth - 16) >> 4;

    sliceHeightIn8 = (pFrameSize->TileHeightPadded + 7) >> 3;
    sliceWidthIn8  = sliceWidth >> 3;   // slicewidth is a multiple of 16

    sliceHeightIn8_np = (pFrameSize->TileHeight + 7) >> 3;
    sliceWidthIn8_np  = (sliceWidth - 16) >> 3;

    if ((result = m_device->CreateThreadSpace(widthIn16, heightIn16, TS_16x16)) != CM_SUCCESS)
        throw CmRuntimeError();
    if ((result = m_device->CreateThreadSpace(sliceWidthIn16_np, sliceHeightIn16_np, TS_Slice_16x16_np)) != CM_SUCCESS)
        throw CmRuntimeError();
    if ((result = m_device->CreateThreadSpace(sliceWidthIn8, sliceHeightIn8, TS_Slice_8x8)) != CM_SUCCESS)
        throw CmRuntimeError();
    if ((result = m_device->CreateThreadSpace(sliceWidthIn8_np, sliceHeightIn8_np, TS_Slice_8x8_np)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void CmContext::CopyMfxSurfToCmSurf(CmSurface2D *cmSurf, mfxFrameSurface1* mfxSurf) //, gpucopy/cpucopy ??? only for gpucopy so far)
{
    int result = CM_SUCCESS;
    CmEvent* event_transfer = NULL;
    if ((result = m_queue->EnqueueCopyCPUToGPU(cmSurf, (const unsigned char *)mfxSurf->Data.Y16, event_transfer)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void CmContext::CopyMemToCmSurf(CmSurface2D *cmSurf, void *mem) //, gpucopy/cpucopy ??? only for gpucopy so far)
{
    int result = CM_SUCCESS;
    CmEvent* event_transfer = NULL;
    if ((result = m_queue->EnqueueCopyCPUToGPU(cmSurf, (const unsigned char *)mem, event_transfer)) != CM_SUCCESS)
        throw CmRuntimeError();
}

CmEvent *CmContext::EnqueueCopyCPUToGPU(CmSurface2D *cmSurf, void *mem, mfxU16 stride)
{
    int result = CM_SUCCESS;
    CmEvent* event_transfer = NULL;

    if (stride > 0)
        result = m_queue->EnqueueCopyCPUToGPUStride(cmSurf, (unsigned char *)mem, (unsigned int)stride, event_transfer);
    else
        result = m_queue->EnqueueCopyCPUToGPU(cmSurf, (unsigned char *)mem, event_transfer);

    if (result != CM_SUCCESS)
        throw CmRuntimeError();

    return event_transfer;
}

CmEvent *CmContext::EnqueueCopyGPUToCPU(CmSurface2D *cmSurf, void *mem, mfxU16 stride)
{
    int result = CM_SUCCESS;
    CmEvent* event_transfer = NULL;

    if (stride > 0)
        result = m_queue->EnqueueCopyGPUToCPUStride(cmSurf, (unsigned char *)mem, (unsigned int)stride, event_transfer);
    else
        result = m_queue->EnqueueCopyGPUToCPU(cmSurf, (unsigned char *)mem, event_transfer);

    if (result != CM_SUCCESS)
        throw CmRuntimeError();

    return event_transfer;
}

void CmContext::CreateTask(CmTask *&task)
{
    int result;
    if ((result = m_device->CreateTask(task)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void CmContext::DestroyTask(CmTask *&task)
{
    int result;
    if ((result = m_device->DestroyTask(task)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void CmContext::CreateCmTasks()
{
    for (int i = 0; i < CAM_PIPE_NUM_TASK_BUFFERS; i++)
    {
        if (!m_caps.bNoPadding)
            CreateTask(CAM_PIPE_TASK_ARRAY(task_Padding, i));

        CreateTask(CAM_PIPE_TASK_ARRAY(task_BayerCorrection, i));
        CreateTask(CAM_PIPE_TASK_ARRAY(task_GoodPixelCheck, i));

        for (int j = 0; j < CAM_PIPE_KERNEL_SPLIT; j++)
        {
            CreateTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreGreen, i), j));
            CreateTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreBlueRed, i), j));
            CreateTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_SAD, i), j));
            CreateTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_DecideAvg, i), j));
        }

        if (m_caps.bForwardGammaCorrection || m_caps.bColorConversionMatrix)
            CreateTask(CAM_PIPE_TASK_ARRAY(task_FwGamma, i));


        if (m_caps.bOutToARGB16 || ( !m_caps.bForwardGammaCorrection ) || m_caps.bColorConversionMatrix )
            CreateTask(CAM_PIPE_TASK_ARRAY(task_ARGB, i));
    }

    CAMERA_DEBUG_LOG("CreateCmTasks end device %p task_ARGB=%p \n", m_device, task_ARGB);
}

void CmContext::DestroyCmTasks()
{
    for (int i = 0; i < CAM_PIPE_NUM_TASK_BUFFERS; i++)
    {
        if (CAM_PIPE_TASK_ARRAY(task_Padding, i))
            DestroyTask(CAM_PIPE_TASK_ARRAY(task_Padding, i));

        if (CAM_PIPE_TASK_ARRAY(task_BayerCorrection, i))
            DestroyTask(CAM_PIPE_TASK_ARRAY(task_BayerCorrection, i));

        if (CAM_PIPE_TASK_ARRAY(task_GoodPixelCheck, i))
            DestroyTask(CAM_PIPE_TASK_ARRAY(task_GoodPixelCheck, i));

        for (int j = 0; j < CAM_PIPE_KERNEL_SPLIT; j++)
        {
            if (CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreGreen, i), j))
                DestroyTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreGreen, i), j));
            if (CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreBlueRed, i), j))
                DestroyTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreBlueRed, i), j));
            if (CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_SAD, i), j))
                DestroyTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_SAD, i), j));
            if (CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_DecideAvg, i), j))
                DestroyTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_DecideAvg, i), j));
        }

        if (CAM_PIPE_TASK_ARRAY(task_FwGamma, i))
            DestroyTask(CAM_PIPE_TASK_ARRAY(task_FwGamma, i));

        if (CAM_PIPE_TASK_ARRAY(task_ARGB, i))
            DestroyTask(CAM_PIPE_TASK_ARRAY(task_ARGB, i));
    }


    Zero(task_Padding);
    Zero(task_BayerCorrection);
    Zero(task_GoodPixelCheck);
    Zero(task_RestoreGreen);
    Zero(task_RestoreBlueRed);
    Zero(task_SAD);
    Zero(task_DecideAvg);
    Zero(task_FwGamma);
    Zero(task_ARGB);
}

void CmContext::CreateTask_Padding(SurfaceIndex inSurfIndex,
                                   SurfaceIndex paddedSurfIndex,
                                   mfxU32 paddedWidth,
                                   mfxU32 paddedHeight,
                                   int bitDepth,
                                   int bayerPattern,
                                   mfxU32)
{
    int result;
    int threadswidth  = paddedWidth  >> 3;
    int threadsheight = paddedHeight >> 3;
    int width  = m_video.TileInfo.CropW;
    int height = m_video.TileInfo.CropH;

    kernel_padding16bpp->SetThreadCount(widthIn16 * heightIn16);
    SetKernelArg(kernel_padding16bpp, inSurfIndex, paddedSurfIndex, threadswidth, threadsheight, width, height, bitDepth, bayerPattern);

    task_Padding->Reset();

    if ((result = task_Padding->AddKernel(kernel_padding16bpp)) != CM_SUCCESS)
        throw CmRuntimeError();
}

CmEvent *CmContext::EnqueueTask_Padding()
{
    int result;
    CmEvent *e = CM_NO_EVENT;
    if ((result = m_queue->Enqueue(task_Padding, e, TS_16x16)) != CM_SUCCESS)
        throw CmRuntimeError();
    return e;
}

void CmContext::CreateTask_BayerCorrection(SurfaceIndex inoutSurfIndex,
                                           SurfaceIndex vignetteMaskIndex,
                                           mfxU16 Enable_BLC,
                                           mfxU16 Enable_VIG,
                                           mfxU16 ENABLE_WB,
                                           short B_shift,
                                           short Gtop_shift,
                                           short Gbot_shift,
                                           short R_shift,
                                           mfxF32 R_scale,
                                           mfxF32 Gtop_scale,
                                           mfxF32 Gbot_scale,
                                           mfxF32 B_scale,
                                           int bitDepth,
                                           int BayerType,
                                           mfxU32)
{
    int result;
    mfxU16 MaxInputLevel = (1<<bitDepth)-1;
    kernel_BayerCorrection->SetThreadCount(widthIn16 * heightIn16);
    int i=0;
    // Having first equal to 1 means that kernel is the first one, thus padding will
    // be done. At the moment, padding is a separate kernel that is done before this
    // so first variable must be set to 0
    int first = 0;

    kernel_BayerCorrection->SetKernelArg( i++, sizeof(SurfaceIndex), &inoutSurfIndex   );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(SurfaceIndex), &inoutSurfIndex   );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(SurfaceIndex), &vignetteMaskIndex);
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(signed short), &Enable_BLC       );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(signed short), &Enable_VIG       );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(signed short), &ENABLE_WB        );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(signed short), &B_shift          );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(signed short), &Gtop_shift       );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(signed short), &Gbot_shift       );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(signed short), &R_shift          );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(float),        &R_scale          );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(float),        &Gtop_scale       );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(float),        &Gbot_scale       );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(float),        &B_scale          );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(signed short), &MaxInputLevel    );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(int),          &first            );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(int),          &bitDepth         );
    kernel_BayerCorrection->SetKernelArg( i++, sizeof(int),          &BayerType        );

    result = task_BayerCorrection->Reset();
    if (result != CM_SUCCESS)
        throw CmRuntimeError();

    result = task_BayerCorrection->AddKernel(kernel_BayerCorrection);
    if (result != CM_SUCCESS )
        throw CmRuntimeError();
}

CmEvent *CmContext::EnqueueTask_BayerCorrection()
{
    int result;
    CmEvent *e = CM_NO_EVENT;
    if ((result = m_queue->Enqueue(task_BayerCorrection, e, TS_16x16)) != CM_SUCCESS)
        throw CmRuntimeError();
    return e;
}

void CmContext::CreateTask_GoodPixelCheck(SurfaceIndex inSurfIndex,
                                          SurfaceIndex paddedSurfIndex,
                                          CmSurface2D *goodPixCntSurf,
                                          CmSurface2D *bigPixCntSurf,
                                          int bitDepth,
                                          int doShiftSwap,
                                          int bayerPattern,
                                          mfxU32)
{
    int result;
    mfxI32 shift_amount = (bitDepth - 8);
    kernel_good_pixel_check->SetThreadCount(widthIn16 * heightIn16);

    int height = (int)m_video.TileHeight; 

    SetKernelArg(kernel_good_pixel_check,
                 inSurfIndex,
                 paddedSurfIndex,
                 GetIndex(goodPixCntSurf),
                 GetIndex(bigPixCntSurf),
                 height,
                 shift_amount,
                 doShiftSwap,
                 bitDepth,
                 bayerPattern);

    task_GoodPixelCheck->Reset();

    if ((result = task_GoodPixelCheck->AddKernel(kernel_good_pixel_check)) != CM_SUCCESS)
        throw CmRuntimeError();
}

CmEvent *CmContext::EnqueueTask_GoodPixelCheck()
{
    int result;
    CmEvent *e = CM_NO_EVENT;
    if ((result = m_queue->Enqueue(task_GoodPixelCheck, e, TS_16x16)) != CM_SUCCESS)
        throw CmRuntimeError();
    return e;
}

void CmContext::CreateTask_RestoreGreen(SurfaceIndex inSurfIndex,
                                        CmSurface2D *goodPixCntSurf,
                                        CmSurface2D *bigPixCntSurf,
                                        CmSurface2D *greenHorSurf,
                                        CmSurface2D *greenVerSurf,
                                        CmSurface2D *greenAvgSurf,
                                        CmSurface2D *avgFlagSurf,
                                        mfxU32 bitDepth,
                                        mfxU32)
{
    int result;
    mfxU16  MaxIntensity = (1 << bitDepth) - 1;

    for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        int xbase = (i * sliceWidthIn8) - ((i == 0) ? 0 : i * 2);
        int ybase = 0;

        int wr_x_base = 0;
        int wr_y_base = 0;

        CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i)->SetThreadCount(sliceWidthIn8 * sliceHeightIn8);

        SetKernelArg(CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i),
                     inSurfIndex,
                     GetIndex(goodPixCntSurf),
                     GetIndex(bigPixCntSurf),
                     GetIndex(greenHorSurf),
                     GetIndex(greenVerSurf),
                     GetIndex(greenAvgSurf),
                     GetIndex(avgFlagSurf),
                     xbase,
                     ybase,
                     wr_x_base);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i),wr_y_base, 10);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i), MaxIntensity, 11);

        CAM_PIPE_KERNEL_ARRAY(task_RestoreGreen, i)->Reset();

        if ((result = CAM_PIPE_KERNEL_ARRAY(task_RestoreGreen, i)->AddKernel(CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i))) != CM_SUCCESS)
            throw CmRuntimeError();
    }
}

void CmContext::CreateTask_RestoreBlueRed(SurfaceIndex inSurfIndex,
                                            CmSurface2D *greenHorSurf, CmSurface2D *greenVerSurf, CmSurface2D *greenAvgSurf,
                                            CmSurface2D *blueHorSurf, CmSurface2D *blueVerSurf, CmSurface2D *blueAvgSurf,
                                            CmSurface2D *redHorSurf, CmSurface2D *redVerSurf, CmSurface2D *redAvgSurf,
                                            CmSurface2D *avgFlagSurf, mfxU32 bitDepth, mfxU32)
{
    int result;
    mfxU16  MaxIntensity = (1 << bitDepth) - 1;

    for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        int xbase = 0;
        int ybase = 0;

        int wr_x_base = (i * sliceWidthIn8) - ((i == 0) ? 0 : i * 2);
        int wr_y_base = 0;

        CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i)->SetThreadCount(sliceWidthIn8 * sliceHeightIn8);

        SetKernelArg(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i),
                     inSurfIndex,
                     GetIndex(greenHorSurf), GetIndex(greenVerSurf), GetIndex(greenAvgSurf),
                     GetIndex(blueHorSurf), GetIndex(blueVerSurf), GetIndex(blueAvgSurf),
                     GetIndex(redHorSurf),  GetIndex(redVerSurf), GetIndex(redAvgSurf));
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i), GetIndex(avgFlagSurf), 10);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i), xbase, 11);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i), ybase, 12);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i), wr_x_base, 13);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i), wr_y_base, 14);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i), MaxIntensity, 15);

        CAM_PIPE_KERNEL_ARRAY(task_RestoreBlueRed, i)->Reset();

        if ((result = CAM_PIPE_KERNEL_ARRAY(task_RestoreBlueRed, i)->AddKernel(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i))) != CM_SUCCESS)
            throw CmRuntimeError();

    }
}

void CmContext::CreateTask_SAD(CmSurface2D *redHorSurf,
                               CmSurface2D *greenHorSurf,
                               CmSurface2D *blueHorSurf,
                               CmSurface2D *redVerSurf,
                               CmSurface2D *greenVerSurf,
                               CmSurface2D *blueVerSurf,
                               CmSurface2D *redOutSurf,
                               CmSurface2D *greenOutSurf,
                               CmSurface2D *blueOutSurf,
                               int bayerPattern,
                               mfxU32)
{
    int result;

    for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        int xbase = 0;
        int ybase = 0;

        int wr_x_base = (i * sliceWidthIn8_np);// - ((i == 0)? 0 : i*2);
        int wr_y_base = 0;

        int height = (int)m_video.TileHeight;

        CAM_PIPE_KERNEL_ARRAY(kernel_sad, i)->SetThreadCount(sliceWidthIn8_np * sliceHeightIn8_np);

        SetKernelArg(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i),
                     GetIndex(redHorSurf), GetIndex(greenHorSurf),  GetIndex(blueHorSurf),
                     GetIndex(redVerSurf), GetIndex(greenVerSurf),  GetIndex(blueVerSurf),
                     xbase, ybase, wr_x_base, wr_y_base);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i), height, 10);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i), bayerPattern, 11);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i), GetIndex(redOutSurf), 12);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i), GetIndex(greenOutSurf), 13);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i), GetIndex(blueOutSurf), 14);

        CAM_PIPE_KERNEL_ARRAY(task_SAD, i)->Reset();

        if ((result = CAM_PIPE_KERNEL_ARRAY(task_SAD, i)->AddKernel(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i))) != CM_SUCCESS)
            throw CmRuntimeError();

    }
}

void CmContext::CreateTask_DecideAverage(CmSurface2D *redAvgSurf,
                                         CmSurface2D *greenAvgSurf,
                                         CmSurface2D *blueAvgSurf,
                                         CmSurface2D *avgFlagSurf,
                                         CmSurface2D *redOutSurf,
                                         CmSurface2D *greenOutSurf,
                                         CmSurface2D *blueOutSurf,
                                         int bayerPattern,
                                         mfxU32)
{
    int result;

    for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {

        int wr_x_base = (i * sliceWidthIn16_np);// - ((i == 0)? 0 : i*2);;
        int wr_y_base = 0;

        int height = (int)m_video.TileHeight;

        CAM_PIPE_KERNEL_ARRAY(kernel_decide_average, i)->SetThreadCount(sliceWidthIn16_np * sliceHeightIn16_np);

        SetKernelArg(CAM_PIPE_KERNEL_ARRAY(kernel_decide_average, i), GetIndex(redAvgSurf), GetIndex(greenAvgSurf), GetIndex(blueAvgSurf),
                                                                      GetIndex(avgFlagSurf), GetIndex(redOutSurf), GetIndex(greenOutSurf), GetIndex(blueOutSurf),
                                                                      wr_x_base, wr_y_base, height);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_decide_average, i), bayerPattern, 10);

        CAM_PIPE_KERNEL_ARRAY(task_DecideAvg, i)->Reset();

        if ((result = CAM_PIPE_KERNEL_ARRAY(task_DecideAvg, i)->AddKernel(CAM_PIPE_KERNEL_ARRAY(kernel_decide_average, i))) != CM_SUCCESS)
            throw CmRuntimeError();
    }
}


#define NEW_GAMMA_THREADS

void CmContext::CreateTask_GammaAndCCM(CmSurface2D  *correctSurf,
                                       CmSurface2D  *pointSurf,
                                       CmSurface2D  *redSurf,
                                       CmSurface2D  *greenSurf,
                                       CmSurface2D  *blueSurf,
                                       CameraPipe3x3ColorConversionParams  *ccm,
                                       CmSurface2D  *outSurf,
                                       mfxU32       bitDepth,
                                       SurfaceIndex *LUTIndex,
                                       mfxU32)
{
    int result;

#ifdef NEW_GAMMA_THREADS
    int gamma_threads_per_group =  m_video.TileWidth * 4 / 512; // 64;
    gamma_threads_per_group = (gamma_threads_per_group + 15) &~ 0xF;
    gamma_threads_per_group = (gamma_threads_per_group < 16) ? 16 : gamma_threads_per_group;
    kernel_FwGamma1->SetThreadCount(gamma_threads_per_group * 4);
    int threadsheight = (( m_video.TileHeight + 15) / 16); // not used in the kernel (?)
#else
    int threadswidth  = m_video.vpp.In.CropW/16; // Out.Width/Height ??? here and below
    int threadsheight = m_video.vpp.In.CropH/16;
    mfxU32 gamma_threads_per_group = 64;
    mfxU32 gamma_groups_vert = ((threadswidth % gamma_threads_per_group) == 0)? (threadswidth/gamma_threads_per_group) : (threadswidth/gamma_threads_per_group + 1);
    int threadcount =  gamma_threads_per_group * gamma_groups_vert;
    kernel_FwGamma1->SetThreadCount(threadcount);
#endif

    int framewidth_in_bytes = m_video.TileWidth * sizeof(int);

    mfxU32 height = (mfxU32)m_video.TileHeight;
    int i = 0;
    kernel_FwGamma1->SetKernelArg(i++, sizeof(SurfaceIndex), &GetIndex(correctSurf));
    kernel_FwGamma1->SetKernelArg(i++, sizeof(SurfaceIndex), &GetIndex(pointSurf)  );
    kernel_FwGamma1->SetKernelArg(i++, sizeof(SurfaceIndex), &GetIndex(redSurf)    );
    kernel_FwGamma1->SetKernelArg(i++, sizeof(SurfaceIndex), &GetIndex(greenSurf)  );
    kernel_FwGamma1->SetKernelArg(i++, sizeof(SurfaceIndex), &GetIndex(blueSurf)   );
    if ( ! ccm )
    {
        kernel_FwGamma1->SetKernelArg(i++, sizeof(SurfaceIndex), &GetIndex(outSurf));
    }
    else
    {
        for(int k = 0; k < 3; k++)
            for(int z = 0; z < 3; z++)
                m_ccm(k*3 + z) = (float)ccm->CCM[k][z];
        kernel_FwGamma1->SetKernelArg( i++, m_ccm.get_size_data(), m_ccm.get_addr_data());
    }
    kernel_FwGamma1->SetKernelArg(i++, sizeof(int), &threadsheight      );
    kernel_FwGamma1->SetKernelArg(i++, sizeof(int), &bitDepth           );
    kernel_FwGamma1->SetKernelArg(i++, sizeof(int), &framewidth_in_bytes);
    kernel_FwGamma1->SetKernelArg(i++, sizeof(int), &height             );
    if ( ccm )
    {
        kernel_FwGamma1->SetKernelArg(i++, sizeof(SurfaceIndex), LUTIndex);
    }

    task_FwGamma->Reset();

    if ((result = task_FwGamma->AddKernel(kernel_FwGamma1)) != CM_SUCCESS)
        throw CmRuntimeError();
}


void CmContext::CreateTask_GammaAndCCM(CmSurface2D *correctSurf,
                                       CmSurface2D *pointSurf,
                                       CmSurface2D *redSurf,
                                       CmSurface2D *greenSurf,
                                       CmSurface2D *blueSurf,
                                       CameraPipe3x3ColorConversionParams *ccm,
                                       SurfaceIndex outSurfIndex,
                                       mfxU32 bitDepth,
                                       SurfaceIndex *LUTIndex,
                                       mfxU32)
{
    int result;

#ifdef NEW_GAMMA_THREADS
    int gamma_threads_per_group =  m_video.TileWidth * 4 / 512; // 64;
    gamma_threads_per_group     = (gamma_threads_per_group + 15) &~ 0xF;
    gamma_threads_per_group     = (gamma_threads_per_group < 16) ? 16 : gamma_threads_per_group;
    kernel_FwGamma->SetThreadCount(gamma_threads_per_group * 4);
    int threadsheight = (( m_video.TileHeight + 15) / 16); // not used in the kernel (?)
#else
    int threadswidth  = m_video.vpp.In.CropW/16; // Out.Width/Height ??? here and below
    int threadsheight = m_video.vpp.In.CropH/16;
    mfxU32 gamma_threads_per_group = 64;
    mfxU32 gamma_groups_vert = ((threadswidth % gamma_threads_per_group) == 0)? (threadswidth/gamma_threads_per_group) : (threadswidth/gamma_threads_per_group + 1);
    int threadcount =  gamma_threads_per_group * gamma_groups_vert;
    kernel_FwGamma->SetThreadCount(threadcount);
#endif

    int framewidth_in_bytes = m_video.TileWidth * sizeof(int);

    mfxU32 height = (mfxU32)m_video.TileHeight;
    int i = 0;
    kernel_FwGamma->SetKernelArg(i++, sizeof(SurfaceIndex), &GetIndex(correctSurf));
    kernel_FwGamma->SetKernelArg(i++, sizeof(SurfaceIndex), &GetIndex(pointSurf)  );
    kernel_FwGamma->SetKernelArg(i++, sizeof(SurfaceIndex), &GetIndex(redSurf)    );
    kernel_FwGamma->SetKernelArg(i++, sizeof(SurfaceIndex), &GetIndex(greenSurf)  );
    kernel_FwGamma->SetKernelArg(i++, sizeof(SurfaceIndex), &GetIndex(blueSurf)   );
    if ( ! ccm )
    {
        kernel_FwGamma->SetKernelArg(i++, sizeof(SurfaceIndex), &outSurfIndex     );
    }
    else
    {
        for(int k = 0; k < 3; k++)
            for(int z = 0; z < 3; z++)
                m_ccm(k*3 + z) = (float)ccm->CCM[k][z];
        kernel_FwGamma->SetKernelArg( i++, m_ccm.get_size_data(), m_ccm.get_addr_data());
    }
    kernel_FwGamma->SetKernelArg(i++, sizeof(int), &threadsheight      );
    kernel_FwGamma->SetKernelArg(i++, sizeof(int), &bitDepth           );
    kernel_FwGamma->SetKernelArg(i++, sizeof(int), &framewidth_in_bytes);
    kernel_FwGamma->SetKernelArg(i++, sizeof(int), &height             );
    if ( ccm )
    {
        kernel_FwGamma->SetKernelArg(i++, sizeof(SurfaceIndex), LUTIndex);
    }
    task_FwGamma->Reset();

    if ((result = task_FwGamma->AddKernel(kernel_FwGamma)) != CM_SUCCESS)
        throw CmRuntimeError();
}

CmEvent *CmContext::EnqueueTask_ForwardGamma()
{
    int result;
#ifdef NEW_GAMMA_THREADS
    int gamma_threads_per_group;
    gamma_threads_per_group =  m_video.TileWidth * 4 / 512; // 64;
    gamma_threads_per_group = (gamma_threads_per_group + 15) &~ 0xF;
    gamma_threads_per_group = (gamma_threads_per_group < 16) ? 16 : gamma_threads_per_group;
#else
    int threadswidth = m_video.vpp.In.CropW/16; // Out.Width/Height ??? here and below
    mfxU32 gamma_threads_per_group = 64;
    mfxU32 gamma_groups_vert = ((threadswidth % gamma_threads_per_group) == 0)? (threadswidth/gamma_threads_per_group) : (threadswidth/gamma_threads_per_group + 1);
#endif

    CmEvent *e = NULL;
    CmThreadGroupSpace* pTGS = NULL;
#ifdef NEW_GAMMA_THREADS
    if ((result = m_device->CreateThreadGroupSpace(1, gamma_threads_per_group, 1, 4, pTGS)) != CM_SUCCESS)
#else
    if ((result = m_device->CreateThreadGroupSpace(1, gamma_threads_per_group, 1, gamma_groups_vert, pTGS)) != CM_SUCCESS)
#endif
        throw CmRuntimeError();

    if ((result = m_queue->EnqueueWithGroup(task_FwGamma, e, pTGS)) !=  CM_SUCCESS)
        throw CmRuntimeError();

    m_device->DestroyThreadGroupSpace(pTGS);
    return e;
}

void CmContext::CreateTask_ARGB(CmSurface2D *redSurf,
                                CmSurface2D *greenSurf,
                                CmSurface2D *blueSurf,
                                SurfaceIndex outSurfIndex,
                                mfxU32 bitDepth,
                                mfxU32)
{

    int result;
    kernel_ARGB->SetThreadCount(widthIn16 * heightIn16);

    SetKernelArg(kernel_ARGB, GetIndex(redSurf), GetIndex(greenSurf), GetIndex(blueSurf), outSurfIndex, bitDepth);

    task_ARGB->Reset();

    if ((result = task_ARGB->AddKernel(kernel_ARGB)) != CM_SUCCESS)
        throw CmRuntimeError();

}

CmEvent *CmContext::EnqueueTask_ARGB()
{
    int result;
    CmEvent *e = NULL;
    CAMERA_DEBUG_LOG("EnqueueTask_ARGB:  task_ARGB=%p\n",task_ARGB);

    if ((result = m_queue->Enqueue(task_ARGB, e, TS_16x16)) != CM_SUCCESS)
        throw CmRuntimeError();
    return e;
}

void CmContext::Setup(
    CameraFrameSizeExtra const & video,
    CmDevice *            cmDevice,
    mfxCameraCaps      *pCaps)
{
    m_video  = video;
    m_device = cmDevice;
    m_caps   = *pCaps;

    if (m_device->CreateQueue(m_queue) != CM_SUCCESS)
        throw CmRuntimeError();

    m_program = ReadProgram(m_device, genx_hsw_camerapipe, SizeOf(genx_hsw_camerapipe));

    CreateCameraKernels();
    CreateCmTasks();
    m_tableCmRelations.clear();
    m_tableCmIndex.clear();
}

void CmContext::Reset(
    mfxVideoParam const & video,
    mfxCameraCaps      *pCaps)
{
    // Demosaic is always on, so the DM kernels must be here already
    for (int i = 0; i < CAM_PIPE_NUM_TASK_BUFFERS; i++)
    {
        if (CAM_PIPE_TASK_ARRAY(task_FwGamma, i))
            DestroyTask(CAM_PIPE_TASK_ARRAY(task_FwGamma, i));

        if (CAM_PIPE_TASK_ARRAY(task_ARGB, i))
            DestroyTask(CAM_PIPE_TASK_ARRAY(task_ARGB, i));
    }

    if (kernel_FwGamma)
        m_device->DestroyKernel(kernel_FwGamma);
    if (kernel_FwGamma1)
        m_device->DestroyKernel(kernel_FwGamma1);
    if (kernel_ARGB)
        m_device->DestroyKernel(kernel_ARGB);

    kernel_ARGB = 0;
    kernel_FwGamma = 0;
    kernel_FwGamma1 = 0;

    if (pCaps->bForwardGammaCorrection)
        CreateTask(CAM_PIPE_TASK_ARRAY(task_FwGamma, i));

    if (pCaps->bOutToARGB16 || !pCaps->bForwardGammaCorrection)
        CreateTask(CAM_PIPE_TASK_ARRAY(task_ARGB, i));

    // TODO: change to reflect new CCM-Gamma combined kernels
    if (pCaps->bForwardGammaCorrection)
    {
        if (m_video.BitDepth == 16)
        {
            kernel_FwGamma = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_ONLY_16bits), NULL);
            // Workaround - otherwise if gamma_argb8 is run with 2DUP out first, and then - with 2D, Enqueue never returns (CM bug)
            kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_ONLY_16bits), NULL);
        }
        else
        {
            if (pCaps->bOutToARGB16)
            {
                kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_ONLY), NULL);
                kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_ONLY), NULL);
            }
            else
            {
                kernel_FwGamma  = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_GPUV4_ARGB8_2D), NULL);
                kernel_FwGamma1 = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_GPUV4_ARGB8_2D), NULL);
            }
        }
    }

    if (pCaps->bOutToARGB16)
        kernel_ARGB = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(ARGB16), NULL);
    else if (!pCaps->bForwardGammaCorrection || video.vpp.In.BitDepthLuma == 16)
        kernel_ARGB = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(ARGB8), NULL);

    if ((video.vpp.In.BitDepthLuma | m_video.BitDepth) > 16)
    {
        int i;
        for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
        {
            if (CAM_PIPE_KERNEL_ARRAY(kernel_sad, i))
                m_device->DestroyKernel(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i));
        }

        if (video.vpp.In.BitDepthLuma != 16)
        {
            for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
            {
                CAM_PIPE_KERNEL_ARRAY(kernel_sad, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(SAD), NULL);
            }
        }
        else
        {
            for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
            {
                CAM_PIPE_KERNEL_ARRAY(kernel_sad, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(SAD_16), NULL);
            }
        }
    }

    m_caps = *pCaps;
    // Fix me
    m_video.TileInfo = video.vpp.In;

    ReleaseCmSurfaces();
}

CmSurface2D *CmContext::CreateCmSurface2D(void *pSrc)
{
    std::map<void *, CmSurface2D *>::iterator it;
    it = m_tableCmRelations.find(pSrc);
    INT cmSts = 0;

    CmSurface2D *pCmSurface2D;
    SurfaceIndex *pCmSrcIndex;

    if (m_tableCmRelations.end() == it)
    {
        cmSts = m_device->CreateSurface2D((AbstractSurfaceHandle *)pSrc, pCmSurface2D);
        if (cmSts != CM_SUCCESS)
        {
            CAMERA_DEBUG_LOG("CreateCmSurface2D:  CreateSurface2D pSrc=%p sts=%d \n", pSrc, cmSts);
            return NULL;
        }

        m_tableCmRelations.insert(std::pair<void *, CmSurface2D *>(pSrc, pCmSurface2D));
        cmSts = pCmSurface2D->GetIndex(pCmSrcIndex);
        CAMERA_DEBUG_LOG("CreateCmSurface2D:  GetIndex pCmSrcIndex %p pCmSurface2D %p pSrc %p sts=%d \n", pCmSrcIndex, pCmSurface2D, pSrc, cmSts);
        if (cmSts != CM_SUCCESS)
            return NULL;
        m_tableCmIndex.insert(std::pair<CmSurface2D *, SurfaceIndex *>(pCmSurface2D, pCmSrcIndex));
    }
    else
    {
        pCmSurface2D = it->second;
    }
    return pCmSurface2D;
}

mfxStatus CmContext::ReleaseCmSurfaces(void)
{
    std::map<void *, CmSurface2D *>::iterator itSrc;

    for (itSrc = m_tableCmRelations.begin() ; itSrc != m_tableCmRelations.end(); itSrc++)
    {
        CmSurface2D *temp = itSrc->second;
        CAMERA_DEBUG_LOG(f, "ReleaseCmSurfaces:   %p \n", temp);
        m_device->DestroySurface(temp);
    }
    m_tableCmRelations.clear();
    return MFX_ERR_NONE;
}
}


