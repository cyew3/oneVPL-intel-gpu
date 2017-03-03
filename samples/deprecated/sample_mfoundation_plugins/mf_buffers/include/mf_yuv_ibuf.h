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

File: mf_yuv_ibuf.h

Purpose: define code for support of input buffering in Intel MediaFoundation
encoder and vpp plug-ins.

Defined Classes, Structures & Enumerations:
  * MFYuvInSurface - enables synchronization between mfxFrameSurface1 and IMFSample

*********************************************************************************/
#ifndef __MF_YUV_IBUF_H__
#define __MF_YUV_IBUF_H__

#include "mf_utils.h"
#include "mf_vbuf.h"
#include "atlbase.h"
#include "mf_frame_allocator.h"

#if MFX_D3D11_SUPPORT
    #include <d3d11.h>
#endif

/*------------------------------------------------------------------------------*/

class mfxEncodeCtrlManaged
{
public:
    mfxEncodeCtrlManaged(mfxEncodeCtrl* pSource) : m_pEncodeCtrl(pSource)
    {
    }

    mfxEncodeCtrlManaged & operator=(mfxEncodeCtrl* pSource)
    {
        Free();
        m_pEncodeCtrl = pSource;
        return *this;
    }

    operator mfxEncodeCtrl*() const
    {
        return m_pEncodeCtrl;
    }

    ~mfxEncodeCtrlManaged(void)
    {
        Free();
    }

    void Free()
    {
        if ( NULL != m_pEncodeCtrl )
        {
            if ( NULL != m_pEncodeCtrl->ExtParam )
            {
                for ( ; m_pEncodeCtrl->NumExtParam > 0; m_pEncodeCtrl->NumExtParam-- )
                {
                    SAFE_FREE(m_pEncodeCtrl->ExtParam[m_pEncodeCtrl->NumExtParam-1]);
                }
                SAFE_FREE(m_pEncodeCtrl->ExtParam);
            }
            if ( NULL != m_pEncodeCtrl->Payload )
            {
                for ( ; m_pEncodeCtrl->NumPayload > 0; m_pEncodeCtrl->NumPayload-- )
                {
                    SAFE_FREE(m_pEncodeCtrl->Payload[m_pEncodeCtrl->NumPayload-1]);
                }
                SAFE_FREE(m_pEncodeCtrl->Payload);
            }
            SAFE_FREE(m_pEncodeCtrl);
        }
    }

    mfxEncodeCtrl *m_pEncodeCtrl;
};

class MFYuvInSurface : public MFDebugDump
{
public:
    MFYuvInSurface (void);
    ~MFYuvInSurface(void);

    // pInfo - MediaSDK initialization parameters
    // pInInfo - parameters of actual input frames
    mfxStatus Init   ( mfxFrameInfo* pInfo
                     , mfxFrameInfo* pInInfo
                     , mfxMemId memid
                     , bool bAlloc
                     , IUnknown * pDevice
                     , MFFrameAllocator *pAlloc);
    void      Close  (void);
    HRESULT   Load   (IMFSample* pSample, bool bIsIntetnalSurface);
    HRESULT   Release(bool bIgnoreLock = false);
    HRESULT   PseudoLoad(void);

    mfxFrameSurface1* GetSurface  (void){ return (m_pComMfxSurface)? m_pComMfxSurface->GetMfxFrameSurface(): &m_mfxSurface; }
    bool IsFakeSrf(void) { return m_bFake; }
    void DropDataToFile(void);

    void SetEncodeCtrl(mfxEncodeCtrl *pEncodeCtrl)
    {
        ATLASSERT( NULL == m_EncodeCtrl );
        m_EncodeCtrl = pEncodeCtrl;
    }

    mfxEncodeCtrl *GetEncodeCtrl() const
    {
        return m_EncodeCtrl;
    }

protected: // functions
    HRESULT   LoadSW(void);

    HRESULT   LoadD3D11();

#if MFX_D3D11_SUPPORT
    //used in noncopymode to store reference to dxgibuffer
    CComQIPtr<IMFDXGIBuffer> m_dxgiBuffer;
#endif

protected: // variables
    MFFrameAllocator * m_pMFAllocator;
    mfxFrameAllocator *m_pAlloc;
    CComPtr<IUnknown> m_pDevice;
    IMFSample*        m_pSample;
    IMFMediaBuffer*   m_pMediaBuffer;
    IMF2DBuffer*      m_p2DBuffer;

    IMfxFrameSurface* m_pComMfxSurface;
    mfxFrameSurface1  m_mfxSurface;
    mfxFrameSurface1  m_mfxInSurface;
    bool              m_bAllocated;   // flag to indicate allocation inside this class
    bool              m_bLocked;      // flag to indicate existence of data
    bool              m_bReleased;    // flag to skip release if it was done
    bool              m_bFake;        // flag to indicate fake frame
    bool              m_bInitialized; // flag to indicate initialization
    bool              m_bFallBackToNonCMCopy;
    mfxU8*            m_pVideo16;
    size_t            m_nVideo16AllocatedSize;
    mfxU16            m_nPitch;
    mfxEncodeCtrlManaged m_EncodeCtrl;

#ifdef CM_COPY_RESOURCE
    CMCopyFactory *   m_factory;
#endif

private:
    // avoiding possible problems by defining operator= and copy constructor
    MFYuvInSurface(const MFYuvInSurface&);
    MFYuvInSurface& operator=(const MFYuvInSurface&);
};

/*------------------------------------------------------------------------------*/

#endif // #ifndef __MF_YUV_IBUF_H__
