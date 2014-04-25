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
#include "cm_def.h"
#include "cm_vm.h"
#include "mfx_camera_plugin_utils.h"
#include "genx_hsw_camerapipe_isa.h"

namespace
{

using MfxCameraPlugin::CmRuntimeError;


CmProgram * ReadProgram(CmDevice * device, const mfxU8 * buffer, size_t len)
{
    int result = CM_SUCCESS;
    CmProgram * program = 0;

    if ((result = ::ReadProgram(device, program, buffer, (mfxU32)len)) != CM_SUCCESS)
//    if ((result = device->LoadProgram((void *)buffer, len, program)) != CM_SUCCESS)
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
, m_TS_16x16(0)
{
}

CmContext::CmContext(
    mfxVideoParam const & video,
    CmDevice *          cmDevice,
    mfxCameraCaps *     pCaps)
{
#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE
//    Zero(task_WhiteBalanceManual);
    Zero(task_GoodPixelCheck);
    Zero(task_RestoreGreen);
    Zero(task_RestoreBlueRed);
    Zero(task_SAD);
    Zero(task_DecideAvg);
//    Zero(task_CheckConfidence);
//    Zero(task_BadPixelCheck);
//    Zero(task_3x3CCM);
    Zero(task_FwGamma);
#endif
    Setup(video, cmDevice, pCaps);
}

#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE

void CmContext::CreateCameraKernels()
{
    int i;
    //if (m_caps.bWhiteBalance)
    //    kernel_whitebalance_manual = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(BAYER_GAIN), NULL);
    //CreateKernel(m_device,m_program, CM_KERNEL_FUNCTION(Padding_16bpp) , (void*)kernel_padding16bpp); // no padding currently

    kernel_good_pixel_check = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GPU_GOOD_PIXEL_CHECK_16x16_SKL), NULL);

    for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++) {
        CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GPU_RESTORE_GREEN_SKL), NULL);
    }
    for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++) {
        CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GPU_RESTORE_BR_SKL), NULL);
    }
    for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++) {
        CAM_PIPE_KERNEL_ARRAY(kernel_sad, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GPU_SAD_CONFCHK_SKL), NULL);
    }
    for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++) {
        CAM_PIPE_KERNEL_ARRAY(kernel_decide_average, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GPU_DECIDE_AVG16x16_SKL), NULL);
    }

    if (m_caps.bForwardGammaCorrection) {
        if (m_caps.OutputMemoryOperationMode == MEM_GPU || m_caps.OutputMemoryOperationMode == MEM_FASTGPUCPY)
            kernel_FwGamma = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_GPUV4_ARGB8_2D), NULL);
        else
            kernel_FwGamma = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_GPUV5_ARGB8_LINEAR), NULL);
    }
}
#else

void CmContext::CreateCameraKernels()
{
    int i;
    if (m_pipeline_flags.wbFlag)
        kernel_whitebalance_manual = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(BAYER_GAIN), NULL);
    //CreateKernel(m_device,m_program, CM_KERNEL_FUNCTION(Padding_16bpp) , (void*)kernel_padding16bpp); // no padding currently

    kernel_good_pixel_check = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GOOD_PIXEL_CHECK), NULL);

    for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++) {
        CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(RESTOREG), NULL);
    }
    for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++) {
        CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(RESTOREBandR), NULL);
    }
    for (i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++) {
        CAM_PIPE_KERNEL_ARRAY(kernel_sad, i) = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(SAD), NULL);
    }
    kernel_decide_average = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(DECIDE_AVG), NULL);
    //CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(CONFIDENCE_CHECK) , kernel_check_confidence);
    kernel_FwGamma = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_GPUV4_ARGB8), NULL);
}

#endif

#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE
void CmContext::CreateThreadSpaces(mfxU32 sliceWidth)
{
    m_heightIn16 = (m_video.vpp.In.CropH + 16 + 15) >> 4;
    m_widthIn16 = (m_video.vpp.In.CropW + 16 + 15) >> 4;

    m_sliceHeightIn16_np = (m_video.vpp.In.CropH + 15) >> 4;
    m_sliceWidthIn16_np = (sliceWidth - 16) >> 4;

    m_sliceHeightIn8 = (m_video.vpp.In.CropH + 16 + 7) >> 3;
    m_sliceWidthIn8 = sliceWidth >> 3;   // slicewidth is a multiple of 16

    m_sliceHeightIn8_np = (m_video.vpp.In.CropH + 7) >> 3;
    m_sliceWidthIn8_np = (sliceWidth - 16) >> 3;

    int result = CM_SUCCESS;
    if ((result = m_device->CreateThreadSpace(m_widthIn16, m_heightIn16, m_TS_16x16)) != CM_SUCCESS)
        throw CmRuntimeError();
    if ((result = m_device->CreateThreadSpace(m_sliceWidthIn16_np, m_sliceHeightIn16_np, m_TS_Slice_16x16_np)) != CM_SUCCESS)
        throw CmRuntimeError();
    if ((result = m_device->CreateThreadSpace(m_sliceWidthIn8, m_sliceHeightIn8, m_TS_Slice_8x8)) != CM_SUCCESS)
        throw CmRuntimeError();
    if ((result = m_device->CreateThreadSpace(m_sliceWidthIn8_np, m_sliceHeightIn8_np, m_TS_Slice_8x8_np)) != CM_SUCCESS)
        throw CmRuntimeError();
}
#else
void CmContext::CreateThreadSpace(mfxU32 frameWidth, mfxU32 frameHeight)
{
//    mfxU32 widthPadded = m_video.vpp.In.Width + 16; // EXEC width/height ???
//    mfxU32 heightPadded = m_video.vpp.In.Height + 16;
    m_widthIn16 = (frameWidth + 15) >> 4;
    m_heightIn16 = (frameHeight + 15) >> 4;

    int result = CM_SUCCESS;
    if ((result = m_device->CreateThreadSpace(m_widthIn16, m_heightIn16, m_TS_16x16)) != CM_SUCCESS)
        throw CmRuntimeError();
}
#endif
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


void CmContext::CreateCmTasks()
{

    for (int i = 0; i < CAM_PIPE_NUM_TASK_BUFFERS; i++) {


        //if (m_pipeline_flags.wbFlag) {
        //    CreateTask(CAM_PIPE_TASK_ARRAY(task_WhiteBalanceManual, i));
        //}
        CreateTask(CAM_PIPE_TASK_ARRAY(task_GoodPixelCheck, i));

        for (int j = 0; j < CAM_PIPE_KERNEL_SPLIT; j++) {
            CreateTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreGreen, i), j));
            CreateTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreBlueRed, i), j));
            CreateTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_SAD, i), j));
            CreateTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_DecideAvg, i), j));
//            CreateTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_BadPixelCheck, i), j));
        }

//        CreateTask(CAM_PIPE_TASK_ARRAY(task_CheckConfidence, i));
//#if !ENABLE_SLM_GAMMA
//        CreateTask(task_PrepareLUT[i]);
//#endif

        //if (m_PipeCaps.bColorConversionMaxtrix){
//        CreateTask(CAM_PIPE_TASK_ARRAY(task_3x3CCM, i));
        //}

        CreateTask(CAM_PIPE_TASK_ARRAY(task_FwGamma, i));
    }

}

//CmEvent *CmContext::EnqueueTasks()
//{
//    int result, sliceNum;
//    CmEvent *e = CM_NO_EVENT;
//
//
//    if ((result = m_queue->Enqueue(task_GoodPixelCheck, e, m_TS_16x16)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    for (sliceNum = 0; sliceNum < CAM_PIPE_KERNEL_SPLIT; sliceNum++) {
//        if ((result = m_queue->Enqueue(CAM_PIPE_KERNEL_ARRAY(task_RestoreGreen, sliceNum), e, m_TS_Slice_8x8)) != CM_SUCCESS)
//            throw CmRuntimeError();
//        if ((result = m_queue->Enqueue(CAM_PIPE_KERNEL_ARRAY(task_RestoreBlueRed, sliceNum), e, m_TS_Slice_8x8)) != CM_SUCCESS)
//            throw CmRuntimeError();
//        if ((result = m_queue->Enqueue(CAM_PIPE_KERNEL_ARRAY(task_SAD, sliceNum), e, m_TS_Slice_8x8_np)) != CM_SUCCESS)
//            throw CmRuntimeError();
//        if ((result = m_queue->Enqueue(CAM_PIPE_KERNEL_ARRAY(task_DecideAvg, sliceNum), e, m_TS_Slice_16x16_np)) != CM_SUCCESS)
//            throw CmRuntimeError();
//    }
//    return e;
//}
//

//void CmContext::CreateTask_ManualWhiteBalance(CmSurface2D *pInSurf, CmSurface2D *pOutSurf, mfxF32 R, mfxF32 G1, mfxF32 B, mfxF32 G2, mfxU32 bitDepth, mfxU32 task_bufId)
//void CmContext::CreateTask_ManualWhiteBalance(SurfaceIndex inSurfIndex, CmSurface2D *pOutSurf, mfxF32 R, mfxF32 G1, mfxF32 B, mfxF32 G2, mfxU32 bitDepth, mfxU32 task_bufId)
//{
//    int result;
//    mfxU16 MaxInputLevel = (1<<bitDepth)-1;
//    kernel_whitebalance_manual->SetThreadCount(m_widthIn16 * m_heightIn16);
//    SetKernelArg(kernel_whitebalance_manual, inSurfIndex, GetIndex(pOutSurf), R, G1, B, G2, MaxInputLevel);
//
//    result = CAM_PIPE_TASK_ARRAY(task_WhiteBalanceManual, task_bufId)->Reset();
//    if (result != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    result = CAM_PIPE_TASK_ARRAY(task_WhiteBalanceManual, task_bufId)->AddKernel(kernel_whitebalance_manual);
//    if (result != CM_SUCCESS )
//        throw CmRuntimeError();
//}


void CmContext::CreateTask_GoodPixelCheck(SurfaceIndex inSurfIndex, CmSurface2D *goodPixCntSurf, CmSurface2D *bigPixCntSurf, mfxU32 bitDepth, mfxU32)
{
    int result;
    mfxI32 shift_amount = (bitDepth - 8);
    kernel_good_pixel_check->SetThreadCount(m_widthIn16 * m_heightIn16);
    SetKernelArg(kernel_good_pixel_check, inSurfIndex, GetIndex(goodPixCntSurf), GetIndex(bigPixCntSurf), shift_amount);

    task_GoodPixelCheck->Reset();

    if ((result = task_GoodPixelCheck->AddKernel(kernel_good_pixel_check)) != CM_SUCCESS)
        throw CmRuntimeError();
}

CmEvent *CmContext::EnqueueTask_GoodPixelCheck()
{
    int result;
    CmEvent *e = CM_NO_EVENT;
    if ((result = m_queue->Enqueue(task_GoodPixelCheck, e, m_TS_16x16)) != CM_SUCCESS)
        throw CmRuntimeError();
    return e;
}

CmEvent *CmContext::CreateEnqueueTask_GoodPixelCheck(SurfaceIndex inSurfIndex, CmSurface2D *goodPixCntSurf, CmSurface2D *bigPixCntSurf, mfxU32 bitDepth)
{
    int result;
    mfxI32 shift_amount = (bitDepth - 8);
    kernel_good_pixel_check->SetThreadCount(m_widthIn16 * m_heightIn16);
    SetKernelArg(kernel_good_pixel_check, inSurfIndex, GetIndex(goodPixCntSurf), GetIndex(bigPixCntSurf), shift_amount);

    CmTask *cmTask = 0;
    if ((result = m_device->CreateTask(cmTask)) != CM_SUCCESS)
        throw CmRuntimeError();

    //task_GoodPixelCheck->Reset();

    if ((result = cmTask->AddKernel(kernel_good_pixel_check)) != CM_SUCCESS)
//    if ((result = task_GoodPixelCheck->AddKernel(kernel_good_pixel_check)) != CM_SUCCESS)
        throw CmRuntimeError();

    CmEvent *e = CM_NO_EVENT;
    if ((result = m_queue->Enqueue(cmTask, e, m_TS_16x16)) != CM_SUCCESS)
    //if ((result = m_queue->Enqueue(task_GoodPixelCheck, e, m_TS_16x16)) != CM_SUCCESS)
        throw CmRuntimeError();

    m_device->DestroyTask(cmTask);
    
    return e;
}

#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE

void CmContext::CreateTask_RestoreGreen(SurfaceIndex inSurfIndex, CmSurface2D *goodPixCntSurf, CmSurface2D *bigPixCntSurf, CmSurface2D *greenHorSurf, CmSurface2D *greenVerSurf, CmSurface2D *greenAvgSurf, CmSurface2D *avgFlagSurf, mfxU32 bitDepth, mfxU32)
{
    int result;
    mfxU16  MaxIntensity = (1 << bitDepth) - 1;

    for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        int xbase = (i * m_sliceWidthIn8) - ((i == 0) ? 0 : i * 2);
        int ybase = 0;

        int wr_x_base = 0;
        int wr_y_base = 0;

        CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i)->SetThreadCount(m_sliceWidthIn8 * m_sliceHeightIn8);

        SetKernelArg(CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i),
                     inSurfIndex, GetIndex(goodPixCntSurf), GetIndex(bigPixCntSurf),
                     GetIndex(greenHorSurf), GetIndex(greenVerSurf), GetIndex(greenAvgSurf), GetIndex(avgFlagSurf), xbase, ybase, wr_x_base);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i), wr_y_base, 10);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i), MaxIntensity, 11);

        CAM_PIPE_KERNEL_ARRAY(task_RestoreGreen, i)->Reset();

        if ((result = CAM_PIPE_KERNEL_ARRAY(task_RestoreGreen, i)->AddKernel(CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i))) != CM_SUCCESS)
            throw CmRuntimeError();
    }
}


#else

CmEvent *CmContext::CreateEnqueueTask_RestoreGreen(SurfaceIndex inSurfIndex, CmSurface2D *goodPixCntSurf, CmSurface2D *bigPixCntSurf, CmSurface2D *greenHorSurf, CmSurface2D *greenVerSurf, CmSurface2D *greenAvgSurf, CmSurface2D *avgFlagSurf, mfxU32 bitDepth)
{
    int result;
    mfxU16  MaxIntensity = (1 << bitDepth) - 1;
    CmTask *cmTask;
    CmEvent *e = CM_NO_EVENT;

    for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        int xbase = (i % 2 == 0)? 0 : (m_widthIn16);
        int ybase = (i < 2 == 0)? 0 : (m_heightIn16);

        CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i)->SetThreadCount(m_widthIn16 * m_heightIn16);

        SetKernelArg(CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i),
                     inSurfIndex, GetIndex(goodPixCntSurf), GetIndex(bigPixCntSurf),
                     GetIndex(greenHorSurf), GetIndex(greenVerSurf), GetIndex(greenAvgSurf), GetIndex(avgFlagSurf), xbase, ybase, MaxIntensity);

        cmTask = 0;
        if ((result = m_device->CreateTask(cmTask)) != CM_SUCCESS)
            throw CmRuntimeError();

        if ((result = cmTask->AddKernel(CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i))) != CM_SUCCESS)
            throw CmRuntimeError();
        
        if ((result = m_queue->Enqueue(cmTask, e, m_TS_16x16)) != CM_SUCCESS)
            throw CmRuntimeError();

        m_device->DestroyTask(cmTask);
    }

    return e;
}

#endif


#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE

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

        int wr_x_base = (i * m_sliceWidthIn8) - ((i == 0) ? 0 : i * 2);
        int wr_y_base = 0;

        CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i)->SetThreadCount(m_sliceWidthIn8 * m_sliceHeightIn8);

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


#else

CmEvent *CmContext::CreateEnqueueTask_RestoreBlueRed(SurfaceIndex inSurfIndex,
                                                    CmSurface2D *greenHorSurf, CmSurface2D *greenVerSurf, CmSurface2D *greenAvgSurf,
                                                    CmSurface2D *blueHorSurf, CmSurface2D *blueVerSurf, CmSurface2D *blueAvgSurf,
                                                    CmSurface2D *redHorSurf, CmSurface2D *redVerSurf, CmSurface2D *redAvgSurf,
                                                    CmSurface2D *avgFlagSurf, mfxU32 bitDepth)
{
    int result;
    mfxU16  MaxIntensity = (1 << bitDepth) - 1;
    CmTask *cmTask;
    CmEvent *e = CM_NO_EVENT;

    for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        int xbase = (i % 2 == 0)? 0 : (m_widthIn16);
        int ybase = (i < 2 == 0)? 0 : (m_heightIn16);

        CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i)->SetThreadCount(m_widthIn16 * m_heightIn16);

        SetKernelArg(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i),
                     inSurfIndex,
                     GetIndex(greenHorSurf), GetIndex(greenVerSurf), GetIndex(greenAvgSurf),
                     GetIndex(blueHorSurf), GetIndex(blueVerSurf), GetIndex(blueAvgSurf),
                     GetIndex(redHorSurf),  GetIndex(redVerSurf), GetIndex(redAvgSurf));
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i), GetIndex(avgFlagSurf), 10);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i), xbase, 11);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i), ybase, 12);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i), MaxIntensity, 13);


        cmTask = 0;
        if ((result = m_device->CreateTask(cmTask)) != CM_SUCCESS)
            throw CmRuntimeError();

        if ((result = cmTask->AddKernel(CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i))) != CM_SUCCESS)
            throw CmRuntimeError();
        
        if ((result = m_queue->Enqueue(cmTask, e, m_TS_16x16)) != CM_SUCCESS)
            throw CmRuntimeError();

        m_device->DestroyTask(cmTask);
    }

    return e;
}

#endif


#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE

void CmContext::CreateTask_SAD(CmSurface2D *redHorSurf, CmSurface2D *greenHorSurf, CmSurface2D *blueHorSurf, CmSurface2D *redVerSurf, CmSurface2D *greenVerSurf, CmSurface2D *blueVerSurf, CmSurface2D *redOutSurf, CmSurface2D *greenOutSurf, CmSurface2D *blueOutSurf, mfxU32)
{
    int result;

    for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        int xbase = 0;
        int ybase = 0;

        int wr_x_base = (i * m_sliceWidthIn8_np);// - ((i == 0)? 0 : i*2);
        int wr_y_base = 0;

        CAM_PIPE_KERNEL_ARRAY(kernel_sad, i)->SetThreadCount(m_sliceWidthIn8_np * m_sliceHeightIn8_np);

        SetKernelArg(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i),
                     GetIndex(redHorSurf), GetIndex(greenHorSurf),  GetIndex(blueHorSurf),
                     GetIndex(redVerSurf), GetIndex(greenVerSurf),  GetIndex(blueVerSurf),
                     xbase, ybase, wr_x_base, wr_y_base);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i), GetIndex(redOutSurf), 10);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i), GetIndex(greenOutSurf), 11);
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i), GetIndex(blueOutSurf), 12);

        CAM_PIPE_KERNEL_ARRAY(task_SAD, i)->Reset();

        if ((result = CAM_PIPE_KERNEL_ARRAY(task_SAD, i)->AddKernel(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i))) != CM_SUCCESS)
            throw CmRuntimeError();
        
    }
}

#else

CmEvent *CmContext::CreateEnqueueTask_SAD(CmSurface2D *redHorSurf, CmSurface2D *greenHorSurf, CmSurface2D *blueHorSurf, CmSurface2D *redVerSurf, CmSurface2D *greenVerSurf, CmSurface2D *blueVerSurf, CmSurface2D *redOutSurf, CmSurface2D *greenOutSurf, CmSurface2D *blueOutSurf)
{
    int result;
    CmTask *cmTask;
    CmEvent *e = CM_NO_EVENT;

    for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        int xbase = (i % 2 == 0)? 0 : (m_widthIn16);
        int ybase = (i < 2 == 0)? 0 : (m_heightIn16);

        CAM_PIPE_KERNEL_ARRAY(kernel_sad, i)->SetThreadCount(m_widthIn16 * m_heightIn16);

        SetKernelArg(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i),
                     GetIndex(redHorSurf), GetIndex(greenHorSurf),  GetIndex(blueHorSurf),
                     GetIndex(redVerSurf), GetIndex(greenVerSurf),  GetIndex(blueVerSurf),
                     xbase, ybase,
                     GetIndex(redOutSurf), GetIndex(greenOutSurf));
        SetKernelArgLast(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i), GetIndex(blueOutSurf), 10);

        cmTask = 0;
        if ((result = m_device->CreateTask(cmTask)) != CM_SUCCESS)
            throw CmRuntimeError();

        if ((result = cmTask->AddKernel(CAM_PIPE_KERNEL_ARRAY(kernel_sad, i))) != CM_SUCCESS)
            throw CmRuntimeError();
        
        if ((result = m_queue->Enqueue(cmTask, e, m_TS_16x16)) != CM_SUCCESS)
            throw CmRuntimeError();

        m_device->DestroyTask(cmTask);
    }
    return e;
}


#endif


#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE

void CmContext::CreateTask_DecideAverage(CmSurface2D *redAvgSurf, CmSurface2D *greenAvgSurf, CmSurface2D *blueAvgSurf, CmSurface2D *avgFlagSurf, CmSurface2D *redOutSurf, CmSurface2D *greenOutSurf, CmSurface2D *blueOutSurf, mfxU32)
{
    int result;

    for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {

        int wr_x_base = (i * m_sliceWidthIn16_np);// - ((i == 0)? 0 : i*2);;
        int wr_y_base = 0;


        CAM_PIPE_KERNEL_ARRAY(kernel_decide_average, i)->SetThreadCount(m_sliceWidthIn16_np * m_sliceHeightIn16_np);

        SetKernelArg(CAM_PIPE_KERNEL_ARRAY(kernel_decide_average, i), GetIndex(redAvgSurf), GetIndex(greenAvgSurf), GetIndex(blueAvgSurf), 
                                                                      GetIndex(avgFlagSurf), GetIndex(redOutSurf), GetIndex(greenOutSurf), GetIndex(blueOutSurf),
                                                                      wr_x_base, wr_y_base);

        CAM_PIPE_KERNEL_ARRAY(task_DecideAvg, i)->Reset();

        if ((result = CAM_PIPE_KERNEL_ARRAY(task_DecideAvg, i)->AddKernel(CAM_PIPE_KERNEL_ARRAY(kernel_decide_average, i))) != CM_SUCCESS)
            throw CmRuntimeError();
    }
}


#else

CmEvent *CmContext::CreateEnqueueTask_DecideAverage(CmSurface2D *redAvgSurf, CmSurface2D *greenAvgSurf, CmSurface2D *blueAvgSurf, CmSurface2D *avgFlagSurf, CmSurface2D *redOutSurf, CmSurface2D *greenOutSurf, CmSurface2D *blueOutSurf)
{
    int result;
    kernel_decide_average->SetThreadCount(m_widthIn16 * m_heightIn16);
    SetKernelArg(kernel_decide_average, GetIndex(redAvgSurf), GetIndex(greenAvgSurf), GetIndex(blueAvgSurf), GetIndex(avgFlagSurf), GetIndex(redOutSurf), GetIndex(greenOutSurf), GetIndex(blueOutSurf));

    CmTask *cmTask = 0;
    if ((result = m_device->CreateTask(cmTask)) != CM_SUCCESS)
        throw CmRuntimeError();

    if ((result = cmTask->AddKernel(kernel_decide_average)) != CM_SUCCESS)
        throw CmRuntimeError();

    CmEvent *e = CM_NO_EVENT;
    if ((result = m_queue->Enqueue(cmTask, e, m_TS_16x16)) != CM_SUCCESS)
        throw CmRuntimeError();

    m_queue->DestroyEvent(e);
    m_device->DestroyTask(cmTask);

    return e;
}

#endif

void CmContext::CreateTask_ForwardGamma(CmSurface2D *correctSurf, CmSurface2D *pointSurf,  CmSurface2D *redSurf, CmSurface2D *greenSurf, CmSurface2D *blueSurf, SurfaceIndex outSurfIndex, mfxU32 bitDepth, mfxU32)
{

    int result;
    int threadswidth = m_video.vpp.In.CropW/16; // Out.Width/Height ??? here and below
    int threadsheight = m_video.vpp.In.CropH/16;
    mfxU32 gamma_threads_per_group = 64;
    mfxU32 gamma_groups_vert = ((threadswidth % gamma_threads_per_group) == 0)? (threadswidth/gamma_threads_per_group) : (threadswidth/gamma_threads_per_group + 1);
    int threadcount =  gamma_threads_per_group * gamma_groups_vert;

    kernel_FwGamma->SetThreadCount(threadcount);
    int framewidth_in_bytes = m_video.vpp.In.CropW * sizeof(int);

    mfxU32 height = (mfxU32)m_video.vpp.In.CropH;

    SetKernelArg(kernel_FwGamma, GetIndex(correctSurf), GetIndex(pointSurf), GetIndex(redSurf), GetIndex(greenSurf), GetIndex(blueSurf), outSurfIndex, threadsheight, bitDepth, framewidth_in_bytes,  height);

    task_FwGamma->Reset();

    if ((result = task_FwGamma->AddKernel(kernel_FwGamma)) != CM_SUCCESS)
        throw CmRuntimeError();

}

CmEvent *CmContext::EnqueueTask_ForwardGamma()
{
    int result;
    int threadswidth = m_video.vpp.In.CropW/16; // Out.Width/Height ??? here and below
    mfxU32 gamma_threads_per_group = 64;
    mfxU32 gamma_groups_vert = ((threadswidth % gamma_threads_per_group) == 0)? (threadswidth/gamma_threads_per_group) : (threadswidth/gamma_threads_per_group + 1);

    CmEvent *e = NULL;
    CmThreadGroupSpace* pTGS = NULL;
    if ((result = m_device->CreateThreadGroupSpace(1, gamma_threads_per_group, 1, gamma_groups_vert, pTGS)) != CM_SUCCESS)
        throw CmRuntimeError();

    if ((result = m_queue->EnqueueWithGroup(task_FwGamma, e, pTGS)) !=  CM_SUCCESS)
        throw CmRuntimeError();

    m_device->DestroyThreadGroupSpace(pTGS);
    return e;
}

CmEvent *CmContext::CreateEnqueueTask_ForwardGamma(CmSurface2D *correctSurf, CmSurface2D *pointSurf,  CmSurface2D *redSurf, CmSurface2D *greenSurf, CmSurface2D *blueSurf, SurfaceIndex outSurfIndex, mfxU32 bitDepth)
{
    int result;
    int threadswidth = m_video.vpp.In.CropW/16; // Out.Width/Height ??? here and below
    int threadsheight = m_video.vpp.In.CropH/16;
    mfxU32 gamma_threads_per_group = 64;
    mfxU32 gamma_groups_vert = ((threadswidth % gamma_threads_per_group) == 0)? (threadswidth/gamma_threads_per_group) : (threadswidth/gamma_threads_per_group + 1);
    int threadcount =  gamma_threads_per_group * gamma_groups_vert;

    kernel_FwGamma->SetThreadCount(threadcount);
    int framewidth_in_bytes = m_video.vpp.In.CropW * sizeof(int);

    mfxU32 height = (mfxU32)m_video.vpp.In.CropH;

    SetKernelArg(kernel_FwGamma, GetIndex(correctSurf), GetIndex(pointSurf), GetIndex(redSurf), GetIndex(greenSurf), GetIndex(blueSurf), outSurfIndex, threadsheight, bitDepth, framewidth_in_bytes,  height);

    CmTask *cmTask = 0;
    if ((result = m_device->CreateTask(cmTask)) != CM_SUCCESS)
        throw CmRuntimeError();

    //task_FwGamma->Reset();


    if ((result = cmTask->AddKernel(kernel_FwGamma)) != CM_SUCCESS)
    //if ((result = task_FwGamma->AddKernel(kernel_FwGamma)) != CM_SUCCESS)
        throw CmRuntimeError();

    CmEvent *e = NULL;
    CmThreadGroupSpace* pTGS = NULL;
    if ((result = m_device->CreateThreadGroupSpace(1, gamma_threads_per_group, 1, gamma_groups_vert, pTGS)) != CM_SUCCESS)
        throw CmRuntimeError();

    if ((result = m_queue->EnqueueWithGroup(cmTask, e, pTGS)) !=  CM_SUCCESS)
    //if ((result = m_queue->EnqueueWithGroup(task_FwGamma, e, pTGS)) !=  CM_SUCCESS)
        throw CmRuntimeError();


    m_device->DestroyThreadGroupSpace(pTGS);
    m_device->DestroyTask(cmTask);
    
    return e;
}


void CmContext::Setup(
    mfxVideoParam const & video,
    CmDevice *            cmDevice,
    mfxCameraCaps      *pCaps)
{
    m_video  = video;
    m_device = cmDevice;
    m_caps = *pCaps;

    if (m_device->CreateQueue(m_queue) != CM_SUCCESS)
        throw CmRuntimeError();

    m_program = ReadProgram(m_device, genx_hsw_camerapipe, SizeOf(genx_hsw_camerapipe));

    CreateCameraKernels();
#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE
    CreateCmTasks();
#endif

}

void CmContext::Reset(
    mfxVideoParam const & video,
    mfxCameraCaps      *pCaps)
{
    m_video  = video;

    // Demosaic is always on, so the DM kernels must be here already

    //if (m_video.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
    //    kernel_FwGamma = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_GPUV4_ARGB8_2D), NULL);
    //else
    //    kernel_FwGamma = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_GPUV5_ARGB8_LINEAR), NULL);

    if (pCaps->bForwardGammaCorrection) {
        bool recreate = true;
        if (m_caps.bForwardGammaCorrection) {
            if ((m_caps.OutputMemoryOperationMode == pCaps->OutputMemoryOperationMode) ||
                ((pCaps->OutputMemoryOperationMode == MEM_GPU || pCaps->OutputMemoryOperationMode == MEM_FASTGPUCPY) && (m_caps.OutputMemoryOperationMode == MEM_GPU || m_caps.OutputMemoryOperationMode == MEM_FASTGPUCPY)))
                recreate = false;
            else
                m_device->DestroyKernel(kernel_FwGamma);
        }

        if (recreate) {
            if (pCaps->OutputMemoryOperationMode == MEM_GPU || pCaps->OutputMemoryOperationMode == MEM_FASTGPUCPY)
                kernel_FwGamma = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_GPUV4_ARGB8_2D), NULL);
            else
                kernel_FwGamma = CreateKernel(m_device, m_program, CM_KERNEL_FUNCTION(GAMMA_GPUV5_ARGB8_LINEAR), NULL);
        }

        //if (!m_caps.bForwardGammaCorrection) {
        //    for (int i = 0; i < CAM_PIPE_NUM_TASK_BUFFERS; i++) {
        //        CreateTask(CAM_PIPE_TASK_ARRAY(task_FwGamma, i));
        //    }
        //}
    }

    m_caps = *pCaps;
}

}


