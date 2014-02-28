/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2013 Intel Corporation. All Rights Reserved.

*********************************************************************************

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
    mfxStatus AllocD3D9(IDirect3DSurface9* pSurface);
#if MFX_D3D11_SUPPORT
    mfxStatus AllocD3D11(ID3D11Texture2D* p2DTexture, mfxMemId memId);
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
