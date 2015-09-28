//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2015 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "mfx_common.h"
#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_defs.h"

class CmDevice;
class CmProgram;
class CmKernel;
class CmQueue;
class CmTask;
class CmThreadSpace;

namespace H265Enc {

    class CmCopy : public NonCopyable
    {
    public:
        CmCopy();
        mfxStatus Init(mfxHDL handle, mfxHandleType handleType);
        void Close();
        mfxStatus SetParam(Ipp32s width, Ipp32s height, Ipp32s fourcc, Ipp32s pitchLuma, Ipp32s pitchChroma, Ipp32s paddingLuW, Ipp32s paddingChW);
        mfxStatus Copy(mfxHDL video, Ipp8u *luma, Ipp8u *chroma);
    private:
        CmDevice *m_device;
        CmProgram *m_program;
        CmKernel *m_kernel;
        CmQueue *m_queue;
        CmTask *m_task;
        CmThreadSpace *m_threadSpace;
        Ipp32s m_width;
        Ipp32s m_height;
        Ipp32s m_paddingLuW;
        Ipp32s m_paddingChW;
    };
};

#endif