/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2015 Intel Corporation. All Rights Reserved.

File Name: mfx_camera_plugin_dx11.h

\* ****************************************************************************** */
#pragma once

#include "mfx_camera_plugin_utils.h"

#define MFX_VA_WIN
#define MFX_ENABLE_VPP
#define MFX_D3D11_ENABLED

#include "d3d11_video_processor.h"

#define MFX_FOURCC_R16_BGGR MAKEFOURCC('I','R','W','0')
#define MFX_FOURCC_R16_RGGB MAKEFOURCC('I','R','W','1')
#define MFX_FOURCC_R16_GRBG MAKEFOURCC('I','R','W','2')
#define MFX_FOURCC_R16_GBRG MAKEFOURCC('I','R','W','3')

using namespace MfxCameraPlugin;
using namespace MfxHwVideoProcessing;

class D3D11CameraProcessor: public CameraProcessor
{
public:
    D3D11CameraProcessor()
    {
        m_ddi.reset(0);
    };

    ~D3D11CameraProcessor() {
        m_core->FreeFrames(&m_InternalSurfes);
    };

    virtual mfxStatus Init(CameraParams *CameraParams);

    virtual mfxStatus Init(mfxVideoParam *par)
    {
        par;
        return MFX_ERR_NONE;
    }

    virtual mfxStatus Reset(mfxVideoParam *par, CameraParams * FrameParams)
    {
        par; FrameParams;
        return MFX_ERR_NONE;
    }

    virtual mfxStatus Close()
    {
        return MFX_ERR_NONE;
    }

    virtual mfxStatus AsyncRoutine(AsyncParams *pParam);
    virtual mfxStatus CompleteRoutine(AsyncParams * pParam);

protected:
    mfxStatus CheckIOPattern(mfxU16  IOPattern);
    mfxStatus PreWorkOutSurface(mfxFrameSurface1 *surf);
    mfxStatus PreWorkInSurface(mfxFrameSurface1 *surf);
    mfxU32    BayerToFourCC(mfxU32);

private:
    MfxHwVideoProcessing::mfxExecuteParams           m_executeParams;
    std::auto_ptr<D3D11VideoProcessor>               m_ddi;
    std::vector<MfxHwVideoProcessing::mfxDrvSurface> m_executeSurf;
    mfxFrameAllocResponse                            m_InternalSurfes;
    CameraParams                                     m_CameraParams;
    UMC::Mutex                                       m_guard;
};
