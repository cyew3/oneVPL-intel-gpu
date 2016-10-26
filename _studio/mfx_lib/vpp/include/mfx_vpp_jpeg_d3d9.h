//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)
#if defined (MFX_VA)
#if defined (MFX_ENABLE_MJPEG_VIDEO_DECODE)

#ifndef __MFX_VPP_JPEG_D3D9_H
#define __MFX_VPP_JPEG_D3D9_H

#include "umc_va_base.h"
#include "mfx_vpp_interface.h"
#include "umc_mutex.h"

#include <memory>
#include <algorithm>
#include <set>

class VideoVppJpegD3D9 
{
public:

    VideoVppJpegD3D9(VideoCORE *core, bool isD3DToSys, bool isOpaq);
    virtual ~VideoVppJpegD3D9(void);

    mfxStatus Init(const mfxVideoParam *par);
    mfxStatus Close(void);

    mfxStatus BeginHwJpegProcessing(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxU16* taskId);
    mfxStatus BeginHwJpegProcessing(mfxFrameSurface1 *inTop, mfxFrameSurface1 *inBottom, mfxFrameSurface1 *out, mfxU16* taskId);
    mfxStatus QueryTaskRoutine(mfxU16 taskId);
    mfxStatus EndHwJpegProcessing(mfxFrameSurface1 *in, mfxFrameSurface1 *out);
    mfxStatus EndHwJpegProcessing(mfxFrameSurface1 *inTop, mfxFrameSurface1 *inBottom, mfxFrameSurface1 *out);

protected:

    VideoCORE *m_pCore;

    bool   m_isD3DToSys;
    bool   m_isOpaq;
    mfxU16 m_taskId;
#ifdef MFX_ENABLE_MJPEG_ROTATE_VPP
    mfxI32 m_rotation;
#endif
    std::vector<mfxFrameSurface1>  m_surfaces;
    mfxFrameAllocResponse m_response;
    UMC::Mutex m_guard;

    std::map<mfxMemId, mfxI32> AssocIdx;

    VPPHWResMng  m_ddi;
};



#endif // __MFX_VPP_JPEG_D3D9_H

#endif // MFX_ENABLE_MJPEG_VIDEO_DECODE
#endif // MFX_VA
#endif // MFX_ENABLE_VPP
