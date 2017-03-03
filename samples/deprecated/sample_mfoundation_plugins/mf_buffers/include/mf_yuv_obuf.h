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

/*********************************************************************************

File: mf_yuv_obuf.h

Purpose: define code for support of output buffering in Intel MediaFoundation
decoder and vpp plug-ins.

Defined Classes, Structures & Enumerations:
  * MFYuvOutSurface - enables synchronization between mfxFrameSurface1 and IMFSample
  * MFYuvOutData - one output buffer item; map between mfxFrameSurface1,
  mfxSyncPoint and MFSurface

*********************************************************************************/
#ifndef __MF_YUV_OBUF_H__
#define __MF_YUV_OBUF_H__

#include "mf_utils.h"
#include "mf_vbuf.h"
#include "base_allocator.h"

/*------------------------------------------------------------------------------*/

enum MFYuvOutSurfaceState
{
    stSurfaceFree,         // no surface available (not allocated or not returned
                           // from downstream plug-in)
    stSurfaceReady,        // surface is available, free and ready to be used;
                           // should not be released by Release() method
    stSurfaceLocked,       // surface locked (by MFX) for internal use;
                           // should not be released by Release() method
    stSurfaceNotDisplayed, // surface locked (by plug-in and maybe by MFX) and
                           // will be sent to downstream plug-in;
                           // should not be released by Release() method
    stSurfaceDisplayed     // surface sent to downstream plug-in;
                           // should be released by Release() method
};

/*------------------------------------------------------------------------------*/

class MFYuvOutSurface : public MFDebugDump
{
public:
    MFYuvOutSurface (void);
    ~MFYuvOutSurface(void);

    mfxStatus Init   (mfxFrameInfo*  pParams,
                      MFSamplesPool* pSamplesPool,
                      mfxFrameAllocator *pAlloc = NULL,
                      bool isD3D9 = true);
    void      Close  (void);
    mfxStatus Alloc  (mfxMemId memid);
    HRESULT   Release(void);
    HRESULT   Sync   (void);
    HRESULT   Pretend(mfxU16 w, mfxU16 h);

    mfxFrameSurface1* GetSurface (void){ return (m_pComMfxSurface)? m_pComMfxSurface->GetMfxFrameSurface(): NULL; }
    IMFSample*        GetSample  (void){ if (m_pSample) m_pSample->AddRef(); return m_pSample; }
    void              IsDisplayed(bool bDisplayed){ m_State = (bDisplayed)? stSurfaceDisplayed: stSurfaceNotDisplayed; }
    void              IsFakeSrf  (bool bFake){ m_bFake = bFake; }
    void              IsGapSrf   (bool bGap){ m_bGap = bGap; }

protected: // functions
    mfxStatus AllocSW(void);
    mfxStatus AllocD3D9(mfxMemId memId);
#if MFX_D3D11_SUPPORT
    mfxStatus AllocD3D11(mfxMemId memId, ID3D11Texture2D* p2DTexture);
#endif

protected: // variables
    mfxFrameAllocator*   m_pAlloc;
    bool                 m_bIsD3D9;
    MFYuvOutSurfaceState m_State;
    IMFMediaBuffer*      m_pMediaBuffer;
    IMFSample*           m_pSample;
    IMF2DBuffer*         m_p2DBuffer;
    IMfxFrameSurface*    m_pComMfxSurface;
    bool                 m_bLocked;        // flag to indicate lock of MF buffer
    bool                 m_bDoNotAlloc;    // flag to indicate that surface should not be allocated
    bool                 m_bFake;          // flag to indicate fake surface
    bool                 m_bGap;           // flag to indicate discontinuity surface
    bool                 m_bInitialized;   // flag to indicate initialization
    MFSamplesPool*       m_pSamplesPool;
    mfxU16               m_nPitch;

private:
    // avoiding possible problems by defining operator= and copy constructor
    MFYuvOutSurface(const MFYuvOutSurface&);
    MFYuvOutSurface& operator=(const MFYuvOutSurface&);
};

/*------------------------------------------------------------------------------*/

struct MFYuvOutData
{
    mfxFrameSurface1* pSurface;
    mfxSyncPoint*     pSyncPoint;
    bool              bSyncPointUsed;
    mfxStatus         iSyncPointSts;
    MFYuvOutSurface*  pMFSurface;
};

#endif // #ifndef __MF_YUV_OBUF_H__
