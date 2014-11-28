/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#ifndef __MF_DXVA_SUPPORT_H__
#define __MF_DXVA_SUPPORT_H__

#include "mf_utils.h"
#include "mf_frame_allocator.h"

/*------------------------------------------------------------------------------*/

class IMfxVideoSession : public MFXVideoSession, public IUnknown
{
public:
    // IMfxVideoSession methods
    IMfxVideoSession(void);
    virtual ~IMfxVideoSession(void);

    // IUnknown methods
    virtual STDMETHODIMP_(ULONG) AddRef (void);
    virtual STDMETHODIMP_(ULONG) Release(void);
    virtual STDMETHODIMP QueryInterface(REFIID iid, void** ppv);
protected:
    long m_nRefCount; // reference count
};

/*------------------------------------------------------------------------------*/
// Interface (for encoder plug-in)
//
// Notes:
//  GetDeviceManager - returns d3d/dx11 device manager allocated by plug-in
//
//  InitPlg - inits plug-in (encoder or vpp); MFX_WRN_PARTIAL_ACCELERATION status
//              means that plug-in prefers SW frames on the input
//    - pDecVideoParams [in]     - mfx decoder parameters
//    - pDecSurfacesNum [in,out] - number of surfaces required by decoder;
//                               encoder (vpp) may change (increase) this number
//
//  GetFrameAllocator - returns frames allocator created by plug-in

class IMFDeviceDXVA: public IUnknown
{
public:
    // IMFDeviceDXVA methods
    virtual mfxStatus InitPlg(IMfxVideoSession* pSession,
                              mfxVideoParam*    pVideoParams,
                              mfxU32*           pSurfacesNum) = 0;

    virtual IUnknown*         GetDeviceManager (void) = 0;
    virtual MFFrameAllocator* GetFrameAllocator(void) = 0;
    virtual void              ReleaseFrameAllocator(void) = 0;
};

/*------------------------------------------------------------------------------*/

class MFDeviceBase : public IMFDeviceDXVA
{
public:
    MFDeviceBase();
    virtual MFFrameAllocator*   GetFrameAllocator(void);
    virtual void                ReleaseFrameAllocator(void);

protected:
    // Objects which encoder-vpp may share
    bool                     m_bShareAllocator;
    MFFrameAllocator*        m_pFrameAllocator;
};
/*------------------------------------------------------------------------------*/

class MFDeviceDXVA : public MFDeviceBase
{
public:
    MFDeviceDXVA(void);
    virtual ~MFDeviceDXVA(void);

    // MFDeviceDXVA methods
#if MFX_D3D11_SUPPORT
    HRESULT DXVASupportInit (void);
#else
    HRESULT DXVASupportInit (UINT nAdapter);
#endif
    void    DXVASupportClose(void);
    HRESULT DXVASupportInitWrapper (IMFDeviceDXVA* pDeviceDXVA);
    void    DXVASupportCloseWrapper(void);

    // IMFDeviceDXVA methods
    virtual IUnknown*           GetDeviceManager(void);

protected:
    bool                     m_bInitialized;
    // System-wide mutex
    MyNamedMutex            m_Mutex;
    HRESULT                 m_mutCreateRes;

    IDirect3D9Ex*            m_pD3D;
    IDirect3DDevice9Ex*      m_pDevice;
    IDirect3DDeviceManager9* m_pDeviceManager;
    D3DPRESENT_PARAMETERS    m_PresentParams;
    // External DXVA Device interface
    IMFDeviceDXVA*           m_pDeviceDXVA;

private:
    // avoiding possible problems by defining operator= and copy constructor
    MFDeviceDXVA(const MFDeviceDXVA&);
    MFDeviceDXVA& operator=(const MFDeviceDXVA&);
};

/*------------------------------------------------------------------------------*/

HRESULT MFCreateMfxVideoSession(IMfxVideoSession** ppSession);

#endif // #ifndef __MF_DXVA_SUPPORT_H__
