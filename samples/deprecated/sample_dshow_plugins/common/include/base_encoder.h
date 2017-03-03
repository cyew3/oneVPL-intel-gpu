/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "mfx_samples_config.h"

#pragma once

#include <list>
#include <process.h>

#include "windows.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"

#include "sample_utils.h"
#include "sample_defs.h"
#include "memory_allocator.h"
#include "bitstream_pool.h"

#define MFX_THREAD_WAIT_TIME 1
#define MFX_THREAD_WAIT Sleep(MFX_THREAD_WAIT_TIME)
#define BREAK_ON_EPOINTER(P, STS) {if(!(P)) { STS = MFX_ERR_MEMORY_ALLOC; break;}}

//#define MFX_EMULATE_HW

struct sMFXSample
{
    IUnknown*           pSample;
    mfxFrameSurface1*   pmfxSurface;
};

interface INotifier
{
    virtual mfxStatus OnFrameReady(mfxBitstream* pBS) = 0;
};


class CBaseEncoder
{
    friend class CEncVideoFilter;
    friend class CEncoderInputPin;

public:
    CBaseEncoder(mfxU16 APIVerMinor = 1, mfxU16 APIVerMajor = 1, mfxStatus *pSts = NULL);
    ~CBaseEncoder(void);

    mfxStatus   Init(mfxVideoParam* pVideoParams, mfxVideoParam* pVPPParams, INotifier* pNotifier);
    mfxStatus   RunEncode(IUnknown *pSample, mfxFrameSurface1* pFrameSurface);
    mfxStatus   Reset(mfxVideoParam* pVideoParams, mfxVideoParam* pVPPParams);
    mfxStatus   Close();

    mfxStatus   GetVideoParams(mfxVideoParam* pVideoParams);
    mfxStatus   GetEncodeStat(mfxEncodeStat* pVideoStat);

    mfxStatus   SetAcceleration(mfxIMPL impl);

    mfxStatus   DynamicBitrateChange(mfxVideoParam* pVideoParams);
protected:

    mfxStatus   InternalReset(mfxVideoParam* pVideoParams, mfxVideoParam* pVPPParams, bool bInited);
    mfxStatus   InternalClose();
    mfxU8       FindFreeSurfaceVPPOut();

    static mfxU32 _stdcall  DeliverNextFrame(void* pvParam);

    mfxStatus AllocateBitstream(sOutputBitstream** pOutputBitstream, int bs_size);
    mfxStatus WipeBitstream(sOutputBitstream* pOutputBitstream);

    mfxStatus   SetExtFrameAllocator(MFXFrameAllocator* pAllocator)
    {
        // set allocator and device handle once
        if (!m_pExternalFrameAllocator)
        {
            m_pExternalFrameAllocator = pAllocator;
            mfxStatus sts = m_mfxVideoSession.SetFrameAllocator(m_pExternalFrameAllocator);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            D3DFrameAllocator* pAlloc = dynamic_cast<D3DFrameAllocator*> (m_pExternalFrameAllocator);
            if (pAlloc)
            {
                sts = m_mfxVideoSession.SetHandle(MFX_HANDLE_D3D9_DEVICE_MANAGER, pAlloc->GetDeviceManager());
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
        }

        return MFX_ERR_NONE;
    }

    mfxStatus QueryIMPL(mfxIMPL *impl)
    {
        mfxStatus sts = m_mfxVideoSession.QueryIMPL(impl);

#ifdef MFX_EMULATE_HW
        *impl = MFX_IMPL_HARDWARE;
#endif
        return sts;
    };

protected:

    mfxVideoParam                   m_VideoParam;

    // parameters for VPP configuration
    mfxVideoParam                   m_VideoParamVPP;
    mfxExtBuffer*                   m_pVppExtBuf;        // pointer to external buffer for VPP, stores the address of m_VppExtDoNotUse
    mfxExtVPPDoNotUse               m_VppExtDoNotUse;    // external buffer to configure VPP, used to turn off certain algorithms
    mfxU32                          m_tabDoNotUseAlg[4]; // list of vpp algorithms ID's (default algorithms will be turned off)

    // parameters for encoder configuration
    mfxExtBuffer*                   m_pCodingExtBuf;
    mfxExtCodingOption              m_CodingOption;

    MFXVideoSession                 m_mfxVideoSession;

    MFXVideoENCODE*                 m_pmfxENC;
    MFXVideoVPP*                    m_pmfxVPP;

    BitstreamPool                   m_BitstreamPool;

    std::list<sMFXSample>           m_InputList; // input list
    std::list<sOutputBitstream *>   m_ResList; // output list
    mfxFrameSurface1**              m_ppFrameSurfaces; // frames shared by VPP output and ENC input
    mfxU16                          m_nRequiredFramesNum; // required number of frames

    MFXFrameAllocator*              m_pExternalFrameAllocator;
    MFXFrameAllocator*              m_pInternalFrameAllocator;
    mfxFrameAllocResponse           m_InternalAllocResponse; // allocation response for frames shared by VPP output and ENC input

    INotifier*                      m_pNotifier;

    HANDLE                          m_Thread;

    bool                            m_bStop;

    CRITICAL_SECTION                m_CriticalSection;

    bool                            m_bUseVPP;

    mfxU16                          m_nEncoderFramesNum;       // required number of frames for encoding
    bool                            m_bEncPartialAcceleration;
    bool                            m_bInitialized;
    bool                            m_bSessionHasChild;
    bool                            m_bSurfaceStored; // flag shows that mfxSurface was added to input list

    bool m_bmfxSample;

    // Media SDK API version the object requires
    mfxU16 m_nAPIVerMinor;
    mfxU16 m_nAPIVerMajor;

private:
    DISALLOW_COPY_AND_ASSIGN(CBaseEncoder);
};