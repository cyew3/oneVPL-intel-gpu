/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

#include "mfx_common.h"
#include <vector>
#include <assert.h>

#include "cmrt_cross_platform.h"
#include "mfx_camera_plugin_utils.h"


class CmDevice;
class CmBuffer;
class CmBufferUP;
class CmSurface2D;
class CmEvent;
class CmQueue;
class CmProgram;
class CmKernel;
class SurfaceIndex;
class CmThreadSpace;
class CmTask;

namespace MfxCameraPlugin
{
class CmRuntimeError : public std::exception
{
public:
    CmRuntimeError() : std::exception() { assert(!"CmRuntimeError"); }
};


class CmDevicePtr
{
public:
    explicit CmDevicePtr(CmDevice * device = 0);

    ~CmDevicePtr();

    void Reset(CmDevice * device);

    CmDevice * operator -> ();

    CmDevicePtr & operator = (CmDevice * device);

    operator CmDevice * ();

private:
    CmDevicePtr(CmDevicePtr const &);
    void operator = (CmDevicePtr const &);

    CmDevice * m_device;
};


class CmSurface
{
public:
    CmSurface();

    CmSurface(CmDevice * device, IDirect3DSurface9 * d3dSurface);

    CmSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc);

    ~CmSurface();

    CmSurface2D * operator -> ();

    operator CmSurface2D * ();

    void Reset(CmDevice * device, IDirect3DSurface9 * d3dSurface);

    void Reset(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc);

    SurfaceIndex const & GetIndex();

    void Read(void * buf, CmEvent * e = 0);

    void Write(void * buf, CmEvent * e = 0);

private:
    CmDevice *      m_device;
    CmSurface2D *   m_surface;
};

class CmBuf
{
public:
    CmBuf();

    CmBuf(CmDevice * device, mfxU32 size);

    ~CmBuf();

    CmBuffer * operator -> ();

    operator CmBuffer * ();

    void Reset(CmDevice * device, mfxU32 size);

    SurfaceIndex const & GetIndex() const;

    void Read(void * buf, CmEvent * e = 0) const;

    void Write(void * buf, CmEvent * e = 0) const;

private:
    CmDevice *  m_device;
    CmBuffer *  m_buffer;
};

CmDevice * TryCreateCmDevicePtr(VideoCORE * core, mfxU32 * version = 0);

CmDevice * CreateCmDevicePtr(VideoCORE * core, mfxU32 * version = 0);

void DestroyGlobalCmDevice(CmDevice * device);

CmBuffer * CreateBuffer(CmDevice * device, mfxU32 size);

CmBufferUP * CreateBuffer(CmDevice * device, mfxU32 size, void * mem);

CmSurface2D * CreateSurface(CmDevice * device, IDirect3DSurface9 * d3dSurface);

CmSurface2D * CreateSurface(CmDevice * device, ID3D11Texture2D * d3dSurface);

CmSurface2D * CreateSurface2DSubresource(CmDevice * device, ID3D11Texture2D * d3dSurface);

CmSurface2D * CreateSurface(CmDevice * device, mfxHDL nativeSurface, eMFXVAType vatype);

CmSurface2D * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc);

CmSurface2DUP * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc, void * mem);

template <class T0>
void SetKernelArg(CmKernel * kernel, T0 const & arg)
{
    kernel->SetKernelArg(0, sizeof(T0), &arg);
}

template <class T0>
void SetKernelArgLast(CmKernel * kernel, T0 const & arg, unsigned int index)
{
    kernel->SetKernelArg(index, sizeof(T0), &arg);
}

template <class T0, class T1>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1)
{
    SetKernelArg(kernel, arg0);
    SetKernelArgLast(kernel, arg1, 1);
}

template <class T0, class T1, class T2>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2)
{
    SetKernelArg(kernel, arg0, arg1);
    SetKernelArgLast(kernel, arg2, 2);
}

template <class T0, class T1, class T2, class T3>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3)
{
    SetKernelArg(kernel, arg0, arg1, arg2);
    SetKernelArgLast(kernel, arg3, 3);
}

template <class T0, class T1, class T2, class T3, class T4>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3);
    SetKernelArgLast(kernel, arg4, 4);
}

template <class T0, class T1, class T2, class T3, class T4, class T5>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4);
    SetKernelArgLast(kernel, arg5, 5);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5);
    SetKernelArgLast(kernel, arg6, 6);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    SetKernelArgLast(kernel, arg7, 7);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7, T8 const & arg8)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    SetKernelArgLast(kernel, arg8, 8);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7, T8 const & arg8, T9 const & arg9)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    SetKernelArgLast(kernel, arg9, 9);
}

#define CAM_PIPE_VERTICAL_SLICE_ENABLE

#define CAM_PIPE_KERNEL_SPLIT 4
#if (CAM_PIPE_KERNEL_SPLIT-1)
#define CAM_PIPE_KERNEL_ARRAY(name, i) name[i]
#else
#define CAM_PIPE_KERNEL_ARRAY(name, i) name
#endif

#define CAM_PIPE_NUM_TASK_BUFFERS 1
#if (CAM_PIPE_NUM_TASK_BUFFERS-1)
#define CAM_PIPE_TASK_ARRAY(name, i) name[i]
#else
#define CAM_PIPE_TASK_ARRAY(name, i) name
#endif

class CmContext
{
public:
    CmContext();

    CmContext(
        mfxVideoParam const & video,
        CmDevice            *cmDevice,
        mfxCameraCaps       *pCaps);

    void Setup(
        mfxVideoParam const & video,
        CmDevice            *cmDevice,
        mfxCameraCaps       *pCaps);

    //CmEvent* EnqueueKernel(
    //    CmKernel *            kernel,
    //    unsigned int          tsWidth,
    //    unsigned int          tsHeight,
    //    CM_DEPENDENCY_PATTERN tsPattern);

    void Reset(
        mfxVideoParam const & video,
        mfxCameraCaps       *pCaps);

    int DestroyEvent( CmEvent*& e ){
        if(m_queue) {
            if (e) {
                //e->WaitForTaskFinished();
                CM_STATUS status = CM_STATUS_QUEUED;
                while (status != CM_STATUS_FINISHED) {
                    if (e->GetStatus(status) != CM_SUCCESS)
                        throw CmRuntimeError();
                }
            }
            return m_queue->DestroyEvent(e);
        }
        else return 0;
    }
#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE
    void CreateThreadSpaces(mfxU32 sliceWidth);
#else
    void CreateThreadSpace(mfxU32 frameWidth, mfxU32 frameHeight);
#endif

    void CopyMfxSurfToCmSurf(CmSurface2D *cmSurf, mfxFrameSurface1* mfxSurf);
    void CopyMemToCmSurf(CmSurface2D *cmSurf, void *mem);

    void CreateTask_ManualWhiteBalance(SurfaceIndex inSurfIndex, CmSurface2D *pOutSurf, mfxF32 R, mfxF32 G1, mfxF32 B, mfxF32 G2, mfxU32 bitDepth, mfxU32 taks_bufId = 0);
    void CreateTask_GoodPixelCheck(SurfaceIndex inSurfIndex, CmSurface2D *goodPixCntSurf, CmSurface2D *bigPixCntSurf, mfxU32 bitDepth, mfxU32 task_bufId = 0);
    void CreateTask_RestoreGreen(SurfaceIndex inSurfIndex, CmSurface2D *goodPixCntSurf, CmSurface2D *bigPixCntSurf, CmSurface2D *greenHorSurf, CmSurface2D *greenVerSurf, CmSurface2D *greenAvgSurf, CmSurface2D *avgFlagSurf, mfxU32 bitDepth, mfxU32 task_bufId = 0);
    void CreateTask_RestoreBlueRed(SurfaceIndex inSurfIndex,
                                   CmSurface2D *greenHorSurf, CmSurface2D *greenVerSurf, CmSurface2D *greenAvgSurf,
                                   CmSurface2D *blueHorSurf, CmSurface2D *blueVerSurf, CmSurface2D *blueAvgSurf,
                                   CmSurface2D *redHorSurf, CmSurface2D *redVerSurf, CmSurface2D *redAvgSurf,
                                   CmSurface2D *avgFlagSurf, mfxU32 bitDepth, mfxU32 task_bufId = 0);
    void CreateTask_SAD(CmSurface2D *redHorSurf, CmSurface2D *greenHorSurf, CmSurface2D *blueHorSurf, CmSurface2D *redVerSurf, CmSurface2D *greenVerSurf, CmSurface2D *blueVerSurf, CmSurface2D *redOutSurf, CmSurface2D *greenOutSurf, CmSurface2D *blueOutSurf, mfxU32 task_bufId = 0);
    void CreateTask_DecideAverage(CmSurface2D *redAvgSurf, CmSurface2D *greenAvgSurf, CmSurface2D *blueAvgSurf, CmSurface2D *avgFlagSurf, CmSurface2D *redOutSurf, CmSurface2D *greenOutSurf, CmSurface2D *blueOutSurf, mfxU32 task_bufId = 0);
    void CreateTask_ForwardGamma(CmSurface2D *correcSurf, CmSurface2D *pointSurf,  CmSurface2D *redSurf, CmSurface2D *greenSurf, CmSurface2D *blueSurf, SurfaceIndex outSurfIndex, mfxU32 bitDepth, mfxU32 task_bufId = 0);


    CmEvent *CreateEnqueueTask_GoodPixelCheck(SurfaceIndex inSurfIndex, CmSurface2D *goodPixCntSurf, CmSurface2D *bigPixCntSurf, mfxU32 bitDepth);
    CmEvent *CreateEnqueueTask_RestoreGreen(SurfaceIndex inSurfIndex, CmSurface2D *goodPixCntSurf, CmSurface2D *bigPixCntSurf, CmSurface2D *greenHorSurf, CmSurface2D *greenVerSurf, CmSurface2D *greenAvgSurf, CmSurface2D *avgFlagSurf, mfxU32 bitDepth);

    CmEvent *CreateEnqueueTask_RestoreBlueRed(SurfaceIndex inSurfIndex,
                                                CmSurface2D *greenHorSurf, CmSurface2D *greenVerSurf, CmSurface2D *greenAvgSurf,
                                                CmSurface2D *blueHorSurf, CmSurface2D *blueVerSurf, CmSurface2D *blueAvgSurf,
                                                CmSurface2D *redHorSurf, CmSurface2D *redVerSurf, CmSurface2D *redAvgSurf,
                                                CmSurface2D *avgFlagSurf, mfxU32 bitDepth);

    CmEvent *CreateEnqueueTask_SAD(CmSurface2D *redHorSurf, CmSurface2D *greenHorSurf, CmSurface2D *blueHorSurf, CmSurface2D *redVerSurf, CmSurface2D *greenVerSurf, CmSurface2D *blueVerSurf, CmSurface2D *redOutSurf, CmSurface2D *greenOutSurf, CmSurface2D *blueOutSurf);
    CmEvent *CreateEnqueueTask_DecideAverage(CmSurface2D *redAvgSurf, CmSurface2D *greenAvgSurf, CmSurface2D *blueAvgSurf, CmSurface2D *avgFlagSurf, CmSurface2D *redOutSurf, CmSurface2D *greenOutSurf, CmSurface2D *blueOutSurf);
    CmEvent *CreateEnqueueTask_ForwardGamma(CmSurface2D *correcSurf, CmSurface2D *pointSurf,  CmSurface2D *redSurf, CmSurface2D *greenSurf, CmSurface2D *blueSurf, SurfaceIndex outSurfIndex, mfxU32 bitDepth);

//    CmEvent *EnqueueTasks();
    CmEvent *EnqueueTask_GoodPixelCheck();
    CmEvent *EnqueueTask_ForwardGamma();

    CmEvent *EnqueueSliceTasks(mfxU32 sliceNum)
    {
        int result;
        CmEvent *e = CM_NO_EVENT;
        if ((result = m_queue->Enqueue(CAM_PIPE_KERNEL_ARRAY(task_RestoreGreen, sliceNum), e, m_TS_Slice_8x8)) != CM_SUCCESS)
            throw CmRuntimeError();
        if ((result = m_queue->Enqueue(CAM_PIPE_KERNEL_ARRAY(task_RestoreBlueRed, sliceNum), e, m_TS_Slice_8x8)) != CM_SUCCESS)
            throw CmRuntimeError();
        if ((result = m_queue->Enqueue(CAM_PIPE_KERNEL_ARRAY(task_SAD, sliceNum), e, m_TS_Slice_8x8_np)) != CM_SUCCESS)
            throw CmRuntimeError();
        if ((result = m_queue->Enqueue(CAM_PIPE_KERNEL_ARRAY(task_DecideAvg, sliceNum), e, m_TS_Slice_16x16_np)) != CM_SUCCESS)
            throw CmRuntimeError();

        return e;
    };

protected:

private:

    void CreateCameraKernels();
    void CreateCmTasks();
    void CreateTask(CmTask *&task);

    mfxVideoParam m_video;
    mfxCameraCaps m_caps;

    CmDevice *  m_device;
    CmQueue *   m_queue;
    CmProgram * m_program;
#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE
    CmThreadSpace * m_TS_16x16;
    CmThreadSpace * m_TS_Slice_8x8;
    CmThreadSpace * m_TS_Slice_16x16_np;
    CmThreadSpace * m_TS_Slice_8x8_np;

    mfxU32 m_widthIn16;
    mfxU32 m_heightIn16;
    mfxU32 m_sliceWidthIn8;
    mfxU32 m_sliceHeightIn8;
    mfxU32 m_sliceWidthIn16_np;
    mfxU32 m_sliceHeightIn16_np;
    mfxU32 m_sliceWidthIn8_np;
    mfxU32 m_sliceHeightIn8_np;

#else
    CmThreadSpace * m_TS_16x16;
    mfxU32 m_widthIn16;
    mfxU32 m_heightIn16;
#endif

    //mfxU32 m_SrcFrameWidth;
    //mfxU32 m_SrcFrameHeight;

    //CmKernel*   kernel_padding16bpp;
    //CmKernel*   kernel_hotpixel;

    // Vignette kernels ---------------
    //CmKernel*   kernel_GenMask;
    //CmKernel*   kernel_GenUpSampledMask;
    //CmKernel*   kernel_BayerMaskReduce;
    //CmKernel*   kernel_Vcorrect;

    // Auto WhiteBalance kernels ------
    //CmKernel*   kernel_CalcHist;
    //CmKernel*   kernel_CalcYBright;
    //CmKernel*   kernel_ApplyScaling;

    // Manual WhiteBalance ------------
    CmKernel*   kernel_whitebalance_manual;

    // Demosaic kernels ---------------
    CmKernel*   kernel_good_pixel_check;
    CmKernel*   CAM_PIPE_KERNEL_ARRAY(kernel_restore_green, CAM_PIPE_KERNEL_SPLIT);
    CmKernel*   CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, CAM_PIPE_KERNEL_SPLIT);
    CmKernel*   CAM_PIPE_KERNEL_ARRAY(kernel_sad, CAM_PIPE_KERNEL_SPLIT);
#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE
    CmKernel*   CAM_PIPE_KERNEL_ARRAY(kernel_decide_average, CAM_PIPE_KERNEL_SPLIT);
#else
    CmKernel*   kernel_decide_average;
#endif
    //CmKernel*   kernel_check_confidence;
    //CmKernel*   CAM_PIPE_KERNEL_ARRAY(kernel_bad_pixel_check, CAM_PIPE_KERNEL_SPLIT); // removed for SKL-light arch

    // CCM kernels ---------------
    CmKernel*   kernel_3x3ccm;

    CmKernel*   kernel_argb8;

#if !ENABLE_SLM_GAMMA
    CmKernel*   kernel_PrepLUT;
#endif
    CmKernel*   kernel_FwGamma;


    //CmTask*         task_GenMask[CAM_PIPE_NUM_TASK_BUFFERS];
    //CmTask*         task_GenUpSampledMask[CAM_PIPE_NUM_TASK_BUFFERS];
    //CmTask*         task_Vignette[CAM_PIPE_NUM_TASK_BUFFERS];
//    CmTask*         CAM_PIPE_TASK_ARRAY(task_WhiteBalanceManual, CAM_PIPE_NUM_TASK_BUFFERS);


#ifdef CAM_PIPE_VERTICAL_SLICE_ENABLE
    // Demosaic - good pix check
    CmTask*         CAM_PIPE_TASK_ARRAY(task_GoodPixelCheck, CAM_PIPE_NUM_TASK_BUFFERS);


    // Demosaic - restore green
    CmTask*         CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreGreen, CAM_PIPE_NUM_TASK_BUFFERS), CAM_PIPE_KERNEL_SPLIT);

    // Demosaic - restore blue and red
    CmTask*         CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreBlueRed, CAM_PIPE_NUM_TASK_BUFFERS), CAM_PIPE_KERNEL_SPLIT);

    // Demosaic - SAD
    CmTask*         CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_SAD, CAM_PIPE_NUM_TASK_BUFFERS), CAM_PIPE_KERNEL_SPLIT);

    // Demosaic - decide average
    CmTask*         CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_DecideAvg, CAM_PIPE_NUM_TASK_BUFFERS), CAM_PIPE_KERNEL_SPLIT);

    // Demoisaic - check confidence
//    CmTask*         CAM_PIPE_TASK_ARRAY(task_CheckConfidence, CAM_PIPE_NUM_TASK_BUFFERS);

    // Demoisaic - bad pixel check
//    CmTask*         CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_BadPixelCheck, CAM_PIPE_NUM_TASK_BUFFERS), CAM_PIPE_KERNEL_SPLIT);

    // CCM
//    CmTask*         CAM_PIPE_TASK_ARRAY(task_3x3CCM, CAM_PIPE_NUM_TASK_BUFFERS);
//    CmBuffer*       CCM_Matrix;

     //Forward Gamma Correction
    CmTask*         CAM_PIPE_TASK_ARRAY(task_FwGamma, CAM_PIPE_NUM_TASK_BUFFERS);
#endif
};



}
