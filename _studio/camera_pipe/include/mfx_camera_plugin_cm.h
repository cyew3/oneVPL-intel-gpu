//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2017 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "mfx_common.h"
#include <vector>
#include <assert.h>
#include <map>

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
    CmRuntimeError() : std::exception() {
        assert(!"CmRuntimeError"); }
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
    CmContext(
        CameraParams const & video,
        CmDevice            *cmDevice,
        mfxCameraCaps       *pCaps,
        eMFXHWType platform);

    void Setup(
        CameraParams const & video,
        CmDevice            *cmDevice,
        mfxCameraCaps       *pCaps);

    void Reset(
        mfxVideoParam const & video,
        mfxCameraCaps       *pCaps,
        mfxCameraCaps       *pOldCaps);

    void Close() {
        ReleaseCmSurfaces();
        DestroyCmTasks();
        delete [] m_DenoiseDWM;
        delete [] m_DenoisePW;
        delete [] m_DenoiseRT;
    }

    int DestroyEvent( CmEvent*& e )
    {
        MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);
        if(m_queue)
        {
            if (e)
            {
                e->WaitForTaskFinished();
            }
            return m_queue->DestroyEvent(e);
        }
        else return 0;
    }

    int DestroyEventWithoutWait( CmEvent*& e )
    {
        MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);
        if(m_queue)
        {
            return m_queue->DestroyEvent(e);
        }
        else return 0;
    }

    void CreateThreadSpaces(CameraParams *pFrameSize);
    void CopyMfxSurfToCmSurf(CmSurface2D *cmSurf, mfxFrameSurface1* mfxSurf);
    void CopyMemToCmSurf(CmSurface2D *cmSurf, void *mem);
    CmEvent *EnqueueCopyGPUToCPU(CmSurface2D *cmSurf, void *mem, mfxU32 stride = 0);
    CmEvent *EnqueueCopyCPUToGPU(CmSurface2D *cmSurf, void *mem, mfxU32 stride = 0);

    void CreateTask_Padding(SurfaceIndex inSurfIndex,
                            SurfaceIndex paddedSurfIndex,
                            mfxU32 paddedWidth,
                            mfxU32 paddedHeight,
                            int bitDepth,
                            int bayerPattern,
                            mfxU32 task_bufId = 0);
    void CreateTask_Denoise(int first,
                            SurfaceIndex InPaddedIndex,
                            SurfaceIndex OutPaddedIndex,
                            SurfaceIndex DNRIndex,
                            mfxU16 Threshold,
                            int bitDepth,
                            int BayerType,
                            mfxU32 task_bufId=0);
    void CreateTask_HotPixel(int first,
                             SurfaceIndex InPaddedIndex,
                             SurfaceIndex OutPaddedIndex,
                             SurfaceIndex DNRIndex,
                             mfxU16 ThreshPixelDiff,
                             mfxU16 ThreshNumPix,
                             int bitDepth,
                             int BayerType,
                             mfxU32 task_bufId=0);

    void CreateTask_VignetteMaskUpSample(SurfaceIndex mask_4x4_Index,
                                         SurfaceIndex vignetteMaskIndex);

    void CreateTask_BayerCorrection(int first,
                                    SurfaceIndex PaddedSurfIndex,
                                    SurfaceIndex inoutSurfIndex,
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
                                    mfxU32 task_bufId=0);

    void CreateTask_GoodPixelCheck(SurfaceIndex inSurfIndex,
                                   SurfaceIndex paddedSurfIndex,
                                   CmSurface2D *goodPixCntSurf,
                                   CmSurface2D *bigPixCntSurf,
                                   int bitDepth,
                                   int doShiftSwap,
                                   int bayerPattern,
                                   mfxU32 task_bufId = 0);

    void CreateTask_RestoreGreen(SurfaceIndex inSurfIndex,
                                 CmSurface2D *goodPixCntSurf,
                                 CmSurface2D *bigPixCntSurf,
                                 CmSurface2D *greenHorSurf,
                                 CmSurface2D *greenVerSurf,
                                 CmSurface2D *greenAvgSurf,
                                 CmSurface2D *avgFlagSurf,
                                 mfxU32 bitDepth,
                                 int bayerPattern,
                                 mfxU32 task_bufId = 0);

    void CreateTask_RestoreBlueRed(SurfaceIndex inSurfIndex,
                                   CmSurface2D *greenHorSurf,
                                   CmSurface2D *greenVerSurf,
                                   CmSurface2D *greenAvgSurf,
                                   CmSurface2D *blueHorSurf,
                                   CmSurface2D *blueVerSurf,
                                   CmSurface2D *blueAvgSurf,
                                   CmSurface2D *redHorSurf,
                                   CmSurface2D *redVerSurf,
                                   CmSurface2D *redAvgSurf,
                                   CmSurface2D *avgFlagSurf,
                                   mfxU32 bitDepth,
                                   int bayerPattern,
                                   mfxU32 task_bufId = 0);

    void CreateTask_SAD(CmSurface2D *redHorSurf,
                        CmSurface2D *greenHorSurf,
                        CmSurface2D *blueHorSurf,
                        CmSurface2D *redVerSurf,
                        CmSurface2D *greenVerSurf,
                        CmSurface2D *blueVerSurf,
                        CmSurface2D *redOutSurf,
                        CmSurface2D *greenOutSurf,
                        CmSurface2D *blueOutSurf,
                        int bayerPattern,
                        mfxU32 task_bufId = 0);

    void CreateTask_DecideAverage(CmSurface2D *redAvgSurf,
                                  CmSurface2D *greenAvgSurf,
                                  CmSurface2D *blueAvgSurf,
                                  CmSurface2D *avgFlagSurf,
                                  CmSurface2D *redOutSurf,
                                  CmSurface2D *greenOutSurf,
                                  CmSurface2D *blueOutSurf,
                                  int bayerPattern,
                                  int bitDepth,
                                  mfxU32 task_bufId = 0);

    void CreateTask_GammaAndCCM( CmSurface2D *correctSurfR,
                                 CmSurface2D *correctSurfG,
                                 CmSurface2D *correctSurfB,
                                 CmSurface2D *pointSurf,
                                 CmSurface2D *redSurf,
                                 CmSurface2D *greenSurf,
                                 CmSurface2D *blueSurf,
                                 CameraPipe3x3ColorConversionParams *ccm,
                                 SurfaceIndex outSurfIndex,
                                 mfxU32 bitDepth,
                                 SurfaceIndex *LUTIndex_R,
                                 SurfaceIndex *LUTIndex_G,
                                 SurfaceIndex *LUTIndex_B,
                                 int BayerType,
                                 int argb8out,
                                 mfxU32 task_bufId = 0);

    void CreateTask_GammaAndCCM( CmSurface2D *correctSurfR,
                                 CmSurface2D *correctSurfG,
                                 CmSurface2D *correctSurfB,
                                 CmSurface2D *pointSurf,
                                 CmSurface2D *redSurf,
                                 CmSurface2D *greenSurf,
                                 CmSurface2D *blueSurf,
                                 CameraPipe3x3ColorConversionParams *ccm,
                                 CmSurface2D *outSurf,
                                 mfxU32 bitDepth,
                                 SurfaceIndex *LUTIndex_R,
                                 SurfaceIndex *LUTIndex_G,
                                 SurfaceIndex *LUTIndex_B,
                                 int BayerType,
                                 int argb8out,
                                 mfxU32 task_bufId = 0);

    void CreateTask_ARGB(CmSurface2D *redSurf,
                         CmSurface2D *greenSurf,
                         CmSurface2D *blueSurf,
                         SurfaceIndex outSurfIndex,
                         CameraPipe3x3ColorConversionParams *ccm,
                         mfxU32 bitDepth,
                         int BayerType,
                         mfxU32 task_bufId = 0);

    CmEvent *EnqueueTask_HP();
    CmEvent *EnqueueTask_Denoise();
    CmEvent *EnqueueTask_Padding();
    CmEvent *EnqueueTask_BayerCorrection();
    CmEvent *EnqueueTask_GoodPixelCheck();
    CmEvent *EnqueueTask_3x3CCM();
    CmEvent *EnqueueTask_ForwardGamma();
    CmEvent *EnqueueTask_ARGB();
    CmEvent *EnqueueTask_VignetteMaskUpSample();
    CmEvent *EnqueueSliceTasks(mfxU32 sliceNum)
    {
        int result;
        CmEvent *e1 = CM_NO_EVENT;
        if ((result = m_queue->Enqueue(CAM_PIPE_KERNEL_ARRAY(task_RestoreGreen, sliceNum), e1, TS_Slice_8x8)) != CM_SUCCESS)
            throw CmRuntimeError();
        CmEvent *e2 = CM_NO_EVENT;
        if ((result = m_queue->Enqueue(CAM_PIPE_KERNEL_ARRAY(task_RestoreBlueRed, sliceNum), e2, TS_Slice_8x8)) != CM_SUCCESS)
            throw CmRuntimeError();
        CmEvent *e3 = CM_NO_EVENT;
        if ((result = m_queue->Enqueue(CAM_PIPE_KERNEL_ARRAY(task_SAD, sliceNum), e3, TS_Slice_8x8_np)) != CM_SUCCESS)
            throw CmRuntimeError();
        CmEvent *e4 = CM_NO_EVENT;
        if ((result = m_queue->Enqueue(CAM_PIPE_KERNEL_ARRAY(task_DecideAvg, sliceNum), e4, TS_Slice_16x16_np)) != CM_SUCCESS)
            throw CmRuntimeError();

        return e4;
    };

    CmSurface2D * CreateCmSurface2D(void *pSrc);

protected:

private:

    void CreateCameraKernels();
    void CreateCmTasks();
    void DestroyCmTasks();
    void CreateTask(CmTask *&task);
    void DestroyTask(CmTask *&task);

    mfxStatus ReleaseCmSurfaces(void);

    CameraParams  m_video;
    mfxCameraCaps m_caps;

    CmDevice *  m_device;
    CmQueue *   m_queue;
    CmProgram * m_program;
    eMFXHWType  m_platform;

    std::map<void *, CmSurface2D *> m_tableCmRelations;
    std::map<CmSurface2D *, SurfaceIndex *> m_tableCmIndex;

    int m_nTilesHor;
    int m_nTilesVer;
    int m_MaxNumOfThreadsPerGroup;
    int m_NumEuPerSubSlice;

    CmThreadSpace *TS_16x16;
    CmThreadSpace *TS_Slice_8x8;
    CmThreadSpace *TS_Slice_16x16_np;
    CmThreadSpace *TS_Slice_8x8_np;
    CmThreadSpace *TS_VignetteUpSample;

    mfxU32 widthIn16;
    mfxU32 heightIn16;
    mfxU32 sliceWidthIn8;
    mfxU32 sliceHeightIn8;
    mfxU32 sliceWidthIn16_np;
    mfxU32 sliceHeightIn16_np;
    mfxU32 sliceWidthIn8_np;
    mfxU32 sliceHeightIn8_np;

    CmKernel*   kernel_hotpixel;
    CmKernel*   kernel_denoise;
    CmKernel*   kernel_padding16bpp;
    CmKernel*   kernel_VignetteUpSample;
    CmKernel*   kernel_BayerCorrection;
    CmKernel*   kernel_whitebalance_manual;

    // Demosaic kernels ---------------
    CmKernel*   kernel_good_pixel_check;
    CmKernel*   CAM_PIPE_KERNEL_ARRAY(kernel_restore_green,    CAM_PIPE_KERNEL_SPLIT);
    CmKernel*   CAM_PIPE_KERNEL_ARRAY(kernel_restore_blue_red, CAM_PIPE_KERNEL_SPLIT);
    CmKernel*   CAM_PIPE_KERNEL_ARRAY(kernel_sad,              CAM_PIPE_KERNEL_SPLIT);
    CmKernel*   CAM_PIPE_KERNEL_ARRAY(kernel_decide_average,   CAM_PIPE_KERNEL_SPLIT);
    //CmKernel*   kernel_check_confidence;
    //CmKernel*   CAM_PIPE_KERNEL_ARRAY(kernel_bad_pixel_check, CAM_PIPE_KERNEL_SPLIT); // removed for SKL-light arch

#if !ENABLE_SLM_GAMMA
    CmKernel*   kernel_PrepLUT;
#endif
    CmKernel*   kernel_FwGamma;
    CmKernel*   kernel_FwGamma1;
    CmKernel*   kernel_ARGB;

    CmTask*      CAM_PIPE_TASK_ARRAY(task_Padding,         CAM_PIPE_NUM_TASK_BUFFERS);
    CmTask*      CAM_PIPE_TASK_ARRAY(task_HP,              CAM_PIPE_NUM_TASK_BUFFERS);
    CmTask*      CAM_PIPE_TASK_ARRAY(task_Denoise,         CAM_PIPE_NUM_TASK_BUFFERS);
    CmTask*      CAM_PIPE_TASK_ARRAY(task_VignetteUpSample, CAM_PIPE_NUM_TASK_BUFFERS);
    CmTask*      CAM_PIPE_TASK_ARRAY(task_BayerCorrection, CAM_PIPE_NUM_TASK_BUFFERS);
    CmTask*      CAM_PIPE_TASK_ARRAY(task_GoodPixelCheck,  CAM_PIPE_NUM_TASK_BUFFERS);
    CmTask*      CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreGreen,   CAM_PIPE_NUM_TASK_BUFFERS), CAM_PIPE_KERNEL_SPLIT);
    CmTask*      CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_RestoreBlueRed, CAM_PIPE_NUM_TASK_BUFFERS), CAM_PIPE_KERNEL_SPLIT);
    CmTask*      CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_SAD,            CAM_PIPE_NUM_TASK_BUFFERS), CAM_PIPE_KERNEL_SPLIT);
    CmTask*      CAM_PIPE_KERNEL_ARRAY(CAM_PIPE_TASK_ARRAY(task_DecideAvg,      CAM_PIPE_NUM_TASK_BUFFERS), CAM_PIPE_KERNEL_SPLIT);
    CmTask*      CAM_PIPE_TASK_ARRAY(task_3x3CCM,  CAM_PIPE_NUM_TASK_BUFFERS);
    CmTask*      CAM_PIPE_TASK_ARRAY(task_FwGamma, CAM_PIPE_NUM_TASK_BUFFERS);
    CmTask*      CAM_PIPE_TASK_ARRAY(task_ARGB,    CAM_PIPE_NUM_TASK_BUFFERS);
    CmBuffer*    CCM_Matrix;

    // Matrix with CCM related params
    vector<float, 9> m_ccm;

    // Denoise Pixel Range Threshold
    unsigned short* m_DenoiseRT;
    // Denoise Pixel Range Weight Array
    unsigned short* m_DenoisePW;
    // Distance Weight Matrix
    unsigned short* m_DenoiseDWM;
};
};

class CMCameraProcessor: public CameraProcessor
{
public:
    CMCameraProcessor():
        m_Params()
        , m_platform(MFX_HW_UNKNOWN)
        , m_FramesTillHardReset(CAMERA_FRAMES_TILL_HARD_RESET)
    {
        m_cmSurfIn         = 0;
        m_gammaPointSurf   = 0;
        m_gammaCorrectSurfR= 0;
        m_gammaCorrectSurfG= 0;
        m_gammaCorrectSurfB= 0;
        m_gammaOutSurf     = 0;
        m_paddedSurf       = 0;
        m_dnrSurf          = 0;
        m_denoiseSurf      = 0;
        m_correctedSurf    = 0;
        m_vignetteMaskSurf = 0;
        m_avgFlagSurf      = 0;
        m_LUTSurf_R        = 0;
        m_LUTSurf_G        = 0;
        m_LUTSurf_B        = 0;
        m_vignette_4x4     = 0;

        m_activeThreadCount = 0;
        m_isInitialized = false;
    };
    ~CMCameraProcessor() {};

    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);

    virtual mfxStatus Init(mfxVideoParam * /*par*/) { return MFX_ERR_NONE; };
    virtual mfxStatus Init(CameraParams *FrameParams)
    {
        MFX_CHECK_NULL_PTR1(FrameParams);

        mfxStatus sts;
        if ( ! m_core )
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        m_FramesTillHardReset = CAMERA_FRAMES_TILL_HARD_RESET;

        m_Params = *FrameParams;
        m_cmDevice.Reset(CreateCmDevicePtr(m_core));

        m_platform = m_core->GetHWType();

        m_cmCtx.reset(new CmContext(m_Params, m_cmDevice, &m_Params.Caps, m_platform));
        m_cmCtx->CreateThreadSpaces(&m_Params);


        sts = AllocateInternalSurfaces();

        m_isInitialized = true;
        return sts;
    }
    virtual mfxStatus Reset(mfxVideoParam *par, CameraParams *PipeParams);
    virtual mfxStatus Close()
    {
        m_FramesTillHardReset = CAMERA_FRAMES_TILL_HARD_RESET;
        m_raw16padded.Free();
        m_raw16aligned.Free();
        m_aux8.Free();

        if (m_cmSurfIn)
            m_cmDevice->DestroySurface(m_cmSurfIn);

        if (m_avgFlagSurf)
            m_cmDevice->DestroySurface(m_avgFlagSurf);
        if (m_gammaCorrectSurfR)
            m_cmDevice->DestroySurface(m_gammaCorrectSurfR);
        if (m_gammaCorrectSurfG)
            m_cmDevice->DestroySurface(m_gammaCorrectSurfG);
        if (m_gammaCorrectSurfB)
            m_cmDevice->DestroySurface(m_gammaCorrectSurfB);
        if (m_gammaPointSurf)
            m_cmDevice->DestroySurface(m_gammaPointSurf);

        if (m_gammaOutSurf)
            m_cmDevice->DestroySurface(m_gammaOutSurf);

        if (m_paddedSurf)
            m_cmDevice->DestroySurface(m_paddedSurf);

        if (m_vignetteMaskSurf)
            m_cmDevice->DestroySurface(m_vignetteMaskSurf);

        // TODO: need to delete LUT buffer in case CCM was used.
        m_cmCtx->Close();

        m_cmDevice.Reset(0);
        return MFX_ERR_NONE;
    }
    virtual mfxStatus AsyncRoutine(AsyncParams *pParam);

    virtual mfxStatus CompleteRoutine(AsyncParams *pPArams);

protected:
    virtual mfxStatus CheckIOPattern(mfxU16  /*IOPattern*/) { return MFX_ERR_NONE; };
    mfxStatus  AllocateInternalSurfaces();
    mfxStatus  CreateEnqueueTasks(AsyncParams *pParam);
    mfxStatus  SetExternalSurfaces(AsyncParams *pParam);
    mfxStatus  ReallocateInternalSurfaces(mfxVideoParam &newParam, CameraParams &frameSizeExtra);
    mfxStatus  WaitForActiveThreads();

private:
    UMC::Mutex               m_guard;
    UMC::Mutex               m_guard_hard_reset;
    bool                     m_isInitialized;
    CameraParams             m_Params;
    CmDevicePtr              m_cmDevice;
    std::auto_ptr<CmContext> m_cmCtx;
    eMFXHWType               m_platform;

    mfxU32              m_FramesTillHardReset;
    mfxU16              m_activeThreadCount;

    CmSurface2D         *m_cmSurfIn;
    CmSurface2D         *m_paddedSurf;
    CmSurface2D         *m_dnrSurf;
    CmSurface2D         *m_correctedSurf;
    CmSurface2D         *m_denoiseSurf;
    CmSurface2D         *m_gammaCorrectSurfR;
    CmSurface2D         *m_gammaCorrectSurfG;
    CmSurface2D         *m_gammaCorrectSurfB;
    CmSurface2D         *m_gammaPointSurf;
    CmSurface2D         *m_gammaOutSurf;
    CmSurface2D         *m_avgFlagSurf;
    CmSurface2D         *m_vignetteMaskSurf;
    CmSurface2D         *m_vignette_4x4;
    CmBuffer            *m_LUTSurf_R;
    CmBuffer            *m_LUTSurf_G;
    CmBuffer            *m_LUTSurf_B;

    MfxFrameAllocResponse   m_raw16padded;
    MfxFrameAllocResponse   m_raw16aligned;
    MfxFrameAllocResponse   m_aux8;
};
