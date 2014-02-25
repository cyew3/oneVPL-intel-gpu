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

#include "cmrt_cross_platform.h"
#include "cm_def.h"
#include "cm_vm.h"
#include "mfx_camera_plugin_cm.h"
#include "mfx_camera_plugin_utils.h"
#include "genx_hsw_camerapipe_isa.h"


namespace
{

using MfxCameraPlugin::CmRuntimeError;

const char   CESDK_PROGRAM_NAME[] = "CameraPipe_genx.txt";
//const mfxU32 BATCHBUFFER_END   = 0x5000000;


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


CmSurface::CmSurface()
: m_device(0)
, m_surface(0)
{
}

CmSurface::CmSurface(CmDevice * device, IDirect3DSurface9 * d3dSurface)
: m_device(0)
, m_surface(0)
{
    Reset(device, d3dSurface);
}

CmSurface::CmSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc)
: m_device(0)
, m_surface(0)
{
    Reset(device, width, height, fourcc);
}

CmSurface::~CmSurface()
{
    Reset(0, 0);
}

CmSurface2D * CmSurface::operator -> ()
{
    return m_surface;
}

CmSurface::operator CmSurface2D * ()
{
    return m_surface;
}

void CmSurface::Reset(CmDevice * device, IDirect3DSurface9 * d3dSurface)
{
    CmSurface2D * newSurface = CreateSurface(device, d3dSurface);

    if (m_device && m_surface)
    {
        int result = m_device->DestroySurface(m_surface);
        result; assert(result == CM_SUCCESS);
    }

    m_device  = device;
    m_surface = newSurface;
}

void CmSurface::Reset(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc)
{
    CmSurface2D * newSurface = CreateSurface(device, width, height, D3DFORMAT(fourcc));

    if (m_device && m_surface)
    {
        int result = m_device->DestroySurface(m_surface);
        result; assert(result == CM_SUCCESS);
    }

    m_device  = device;
    m_surface = newSurface;
}

SurfaceIndex const & CmSurface::GetIndex()
{
    return ::GetIndex(m_surface);
}

void CmSurface::Read(void * buf, CmEvent * e)
{
    int result = m_surface->ReadSurface(reinterpret_cast<unsigned char *>(buf), e);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
}

void CmSurface::Write(void * buf, CmEvent * e)
{
    int result = m_surface->WriteSurface(reinterpret_cast<unsigned char *>(buf), e);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
}



CmBuf::CmBuf()
: m_device(0)
, m_buffer(0)
{
}

CmBuf::CmBuf(CmDevice * device, mfxU32 size)
: m_device(0)
, m_buffer(0)
{
    Reset(device, size);
}

CmBuf::~CmBuf()
{
    Reset(0, 0);
}

CmBuffer * CmBuf::operator -> ()
{
    return m_buffer;
}

CmBuf::operator CmBuffer * ()
{
    return m_buffer;
}

void CmBuf::Reset(CmDevice * device, mfxU32 size)
{
    CmBuffer * buffer = (device && size) ? CreateBuffer(device, size) : 0;

    if (m_device && m_buffer)
    {
        int result = m_device->DestroySurface(m_buffer);
        result; assert(result == CM_SUCCESS);
    }

    m_device = device;
    m_buffer = buffer;
}

SurfaceIndex const & CmBuf::GetIndex() const
{
    return ::GetIndex(m_buffer);
}

void CmBuf::Read(void * buf, CmEvent * e) const
{
    ::Read(m_buffer, buf, e);
}

void CmBuf::Write(void * buf, CmEvent * e) const
{
    ::Write(m_buffer, buf, e);
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


CmBufferUP *AllocCmBufferUp(CmDevice * device, mfxU32 width,  mfxU32 height)
{
    void *psysmem = CM_ALIGNED_MALLOC(width * height, 0x1000); // need to remember psysmem to free at the end??? PASS pMem here???

                                                                // CreateBufferUP(mfxSurface1)   CreateCmSurface(mfxSurface1) ???? - ask somebody!!!???
    return CreateBuffer(device, width * height, psysmem);
}

CmContext::CmContext()
: m_device(0)
, m_program(0)
, m_queue(0)
, m_thread_space(0)
{
}


CmContext::CmContext(
    mfxVideoParam const & video,
    CmDevice *            cmDevice,
    pipeline_config *pPipeFlags)
{
    Zero(task_WhiteBalanceManual);
    Zero(task_GoodPixelCheck);
    Zero(task_RestoreGreen);
    Zero(task_RestoreBlueRed);
    Zero(task_SAD);
    Zero(task_DecideAvg);
    Zero(task_CheckConfidence);
    Zero(task_BadPixelCheck);
    Zero(task_3x3CCM);
    Zero(task_FwGamma);
    Setup(video, cmDevice, pPipeFlags);
}

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


void CmContext::CreateThreadSpace(mfxU32 frameWidth, mfxU32 frameHeight)
{
//    mfxU32 widthPadded = m_video.vpp.In.Width + 16; // EXEC width/height ???
//    mfxU32 heightPadded = m_video.vpp.In.Height + 16;
    m_widthInMb = (frameWidth + 15) >> 4;
    m_heightInMb = (frameHeight + 15) >> 4;

    int result = CM_SUCCESS;
    if ((result = m_device->CreateThreadSpace(m_widthInMb, m_heightInMb, m_thread_space)) != CM_SUCCESS)
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


void CmContext::CreateTask(CmTask *&task)
{
    int result;
    if ((result = m_device->CreateTask(task)) != CM_SUCCESS)
        throw CmRuntimeError();
}


void CmContext::CreateCameraTasks()
{

    // general CmTask
    //CreateTask(m_pCmTask);

    for (int i = 0; i < CAM_PIPE_NUM_TASK_BUFFERS; i++) {

        //CreateTask(task_Padding[i]);
        //if (m_PipeCaps.bVignette){
            //CreateTask(task_GenMask[i]);
            //CreateTask(task_GenUpSampledMask[i]);
            //CreateTask(task_Vignette[i]);
        //}

        if (m_pipeline_flags.wbFlag) {
            CreateTask(CAM_PIPE_TASK_ARRAY(task_WhiteBalanceManual, i));
        }
        CreateTask(CAM_PIPE_TASK_ARRAY(task_GoodPixelCheck, i));

        for (int j = 0; j < CAM_PIPE_KERNEL_SPLIT; j++) {
            CreateTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreGreen, i), j));
            CreateTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreBlueRed, i), j));
            CreateTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_SAD, i), j));
            CreateTask(CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_BadPixelCheck, i), j));
        }

        CreateTask(CAM_PIPE_TASK_ARRAY(task_DecideAvg, i));
        CreateTask(CAM_PIPE_TASK_ARRAY(task_CheckConfidence, i));
//#if !ENABLE_SLM_GAMMA
//        CreateTask(task_PrepareLUT[i]);
//#endif

        //if (m_PipeCaps.bColorConversionMaxtrix){
        CreateTask(CAM_PIPE_TASK_ARRAY(task_3x3CCM, i));
        //}

        CreateTask(CAM_PIPE_TASK_ARRAY(task_FwGamma, i));
    }
}



//void CmContext::CreateTask_ManualWhiteBalance(CmSurface2D *pInSurf, CmSurface2D *pOutSurf, mfxF32 R, mfxF32 G1, mfxF32 B, mfxF32 G2, mfxU32 bitDepth, mfxU32 task_bufId)
void CmContext::CreateTask_ManualWhiteBalance(SurfaceIndex inSurfIndex, CmSurface2D *pOutSurf, mfxF32 R, mfxF32 G1, mfxF32 B, mfxF32 G2, mfxU32 bitDepth, mfxU32 task_bufId)
{
    int result;
    mfxU16 MaxInputLevel = (1<<bitDepth)-1;
    kernel_whitebalance_manual->SetThreadCount(m_widthInMb * m_heightInMb);
    SetKernelArg(kernel_whitebalance_manual, inSurfIndex, GetIndex(pOutSurf), R, G1, B, G2, MaxInputLevel);

    result = CAM_PIPE_TASK_ARRAY(task_WhiteBalanceManual, task_bufId)->Reset();
    if (result != CM_SUCCESS)
        throw CmRuntimeError();

    result = CAM_PIPE_TASK_ARRAY(task_WhiteBalanceManual, task_bufId)->AddKernel(kernel_whitebalance_manual);
    if (result != CM_SUCCESS )
        throw CmRuntimeError();
}


CmEvent *CmContext::CreateEnqueueTask_GoodPixelCheck(SurfaceIndex inSurfIndex, CmSurface2D *goodPixCntSurf, CmSurface2D *bigPixCntSurf, mfxU32 bitDepth)
{
    int result;
    mfxI32 shift_amount = (bitDepth - 8);
    kernel_good_pixel_check->SetThreadCount(m_widthInMb * m_heightInMb);
    SetKernelArg(kernel_good_pixel_check, inSurfIndex, GetIndex(goodPixCntSurf), GetIndex(bigPixCntSurf), shift_amount);

    CmTask *cmTask = 0;
    if ((result = m_device->CreateTask(cmTask)) != CM_SUCCESS)
        throw CmRuntimeError();

    if ((result = cmTask->AddKernel(kernel_good_pixel_check)) != CM_SUCCESS)
        throw CmRuntimeError();

    CmEvent *e = 0;
    if ((result = m_queue->Enqueue(cmTask, e, m_thread_space)) != CM_SUCCESS)
        throw CmRuntimeError();

    //m_device->DestroyThreadSpace(cmThreadSpace);
    m_device->DestroyTask(cmTask);
    
    return e;
}

CmEvent *CmContext::CreateEnqueueTask_RestoreGreen(SurfaceIndex inSurfIndex, CmSurface2D *goodPixCntSurf, CmSurface2D *bigPixCntSurf, CmSurface2D *greenHorSurf, CmSurface2D *greenVerSurf, CmSurface2D *greenAvgSurf, CmSurface2D *avgFlagSurf, mfxU32 bitDepth)
{
    int result;
    mfxU16  MaxIntensity = (1 << bitDepth) - 1;
    CmTask *cmTask;
    CmEvent *e;

    for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        int xbase = (i % 2 == 0)? 0 : (m_widthInMb);
        int ybase = (i < 2 == 0)? 0 : (m_heightInMb);

        CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i)->SetThreadCount(m_widthInMb * m_heightInMb);

        SetKernelArg(CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i),
                     inSurfIndex, GetIndex(goodPixCntSurf), GetIndex(bigPixCntSurf),
                     GetIndex(greenHorSurf), GetIndex(greenVerSurf), GetIndex(greenAvgSurf), GetIndex(avgFlagSurf), xbase, ybase, MaxIntensity);

        cmTask = 0;
        if ((result = m_device->CreateTask(cmTask)) != CM_SUCCESS)
            throw CmRuntimeError();

        if ((result = cmTask->AddKernel(CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, i))) != CM_SUCCESS)
            throw CmRuntimeError();
        
        e = 0;
        if ((result = m_queue->Enqueue(cmTask, e, m_thread_space)) != CM_SUCCESS)
            throw CmRuntimeError();

        m_device->DestroyTask(cmTask);
    }

    return e;
}


CmEvent *CmContext::CreateEnqueueTask_RestoreBlueRed(SurfaceIndex inSurfIndex,
                                                    CmSurface2D *greenHorSurf, CmSurface2D *greenVerSurf, CmSurface2D *greenAvgSurf,
                                                    CmSurface2D *blueHorSurf, CmSurface2D *blueVerSurf, CmSurface2D *blueAvgSurf,
                                                    CmSurface2D *redHorSurf, CmSurface2D *redVerSurf, CmSurface2D *redAvgSurf,
                                                    CmSurface2D *avgFlagSurf, mfxU32 bitDepth)
{
    int result;
    mfxU16  MaxIntensity = (1 << bitDepth) - 1;
    CmTask *cmTask;
    CmEvent *e;

    for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        int xbase = (i % 2 == 0)? 0 : (m_widthInMb);
        int ybase = (i < 2 == 0)? 0 : (m_heightInMb);

        CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, i)->SetThreadCount(m_widthInMb * m_heightInMb);

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
        
        e = 0;
        if ((result = m_queue->Enqueue(cmTask, e, m_thread_space)) != CM_SUCCESS)
            throw CmRuntimeError();

        m_device->DestroyTask(cmTask);
    }

    return e;
}


CmEvent *CmContext::CreateEnqueueTask_SAD(CmSurface2D *redHorSurf, CmSurface2D *greenHorSurf, CmSurface2D *blueHorSurf, CmSurface2D *redVerSurf, CmSurface2D *greenVerSurf, CmSurface2D *blueVerSurf, CmSurface2D *redOutSurf, CmSurface2D *greenOutSurf, CmSurface2D *blueOutSurf)
{
    int result;
    CmTask *cmTask;
    CmEvent *e;

    for (int i = 0; i < CAM_PIPE_KERNEL_SPLIT; i++)
    {
        int xbase = (i % 2 == 0)? 0 : (m_widthInMb);
        int ybase = (i < 2 == 0)? 0 : (m_heightInMb);

        CAM_PIPE_KERNEL_ARRAY(kernel_sad, i)->SetThreadCount(m_widthInMb * m_heightInMb);

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
        
        e = 0;
        if ((result = m_queue->Enqueue(cmTask, e, m_thread_space)) != CM_SUCCESS)
            throw CmRuntimeError();

        m_device->DestroyTask(cmTask);
    }
    return e;
}

CmEvent *CmContext::CreateEnqueueTask_DecideAverage(CmSurface2D *redAvgSurf, CmSurface2D *greenAvgSurf, CmSurface2D *blueAvgSurf, CmSurface2D *avgFlagSurf, CmSurface2D *redOutSurf, CmSurface2D *greenOutSurf, CmSurface2D *blueOutSurf)
{
    int result;
    kernel_decide_average->SetThreadCount(m_widthInMb * m_heightInMb);
    SetKernelArg(kernel_decide_average, GetIndex(redAvgSurf), GetIndex(greenAvgSurf), GetIndex(blueAvgSurf), GetIndex(avgFlagSurf), GetIndex(redOutSurf), GetIndex(greenOutSurf), GetIndex(blueOutSurf));

    CmTask *cmTask = 0;
    if ((result = m_device->CreateTask(cmTask)) != CM_SUCCESS)
        throw CmRuntimeError();

    if ((result = cmTask->AddKernel(kernel_decide_average)) != CM_SUCCESS)
        throw CmRuntimeError();

    CmEvent *e = 0;
    if ((result = m_queue->Enqueue(cmTask, e, m_thread_space)) != CM_SUCCESS)
        throw CmRuntimeError();

    m_device->DestroyTask(cmTask);
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

    if ((result = cmTask->AddKernel(kernel_FwGamma)) != CM_SUCCESS)
        throw CmRuntimeError();

    CmEvent *e = 0;
    CmThreadGroupSpace* pTGS = NULL;
    if ((result = m_device->CreateThreadGroupSpace(1, gamma_threads_per_group, 1, gamma_groups_vert, pTGS)) != CM_SUCCESS)
        throw CmRuntimeError();

    if ((result = m_queue->EnqueueWithGroup(cmTask, e, pTGS)) !=  CM_SUCCESS)
        throw CmRuntimeError();

    m_device->DestroyThreadGroupSpace(pTGS);
    m_device->DestroyTask(cmTask);
    
    return e;
}


void CmContext::Setup(
    mfxVideoParam const & video,
    CmDevice *            cmDevice,
    pipeline_config      *pPipeFlags)
{
    m_video  = video;
    m_device = cmDevice;
    m_pipeline_flags = *pPipeFlags;

    if (m_device->CreateQueue(m_queue) != CM_SUCCESS)
        throw CmRuntimeError();

    m_program = ReadProgram(m_device, genx_hsw_camerapipe, SizeOf(genx_hsw_camerapipe));

    CreateCameraKernels();

//    CreateCameraTasks();

    //m_nullBuf.Reset(m_device, 4);

}

CmEvent * CmContext::EnqueueTask(
    CmTask *            task)
{
    int result = CM_SUCCESS;

    CmEvent * e = 0;
    if ((result = m_queue->Enqueue(task, e, m_thread_space)) != CM_SUCCESS)
        throw CmRuntimeError();

    return e;
}




}


