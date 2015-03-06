/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#ifndef __MF_FRAME_ALLOCATOR_H__
#define __MF_FRAME_ALLOCATOR_H__

#include "d3d_allocator.h"
#if MFX_D3D11_SUPPORT
    #ifdef MF_D3D11_COPYSURFACES
        #include "d3d11_allocator.h"
    #else
        #include "mf_d3d11_allocator.h"
    #endif
#endif
#include "mf_utils.h"

/*------------------------------------------------------------------------------*/

class IMFAllocatorHelper
{
public:
    virtual ~IMFAllocatorHelper(){}
    //TODO: does it make sense to use query interface???
    //unified interface to HW device, probably better to redesign allocators
    virtual IUnknown * GetDeviceDxGI() = 0;
    //used for d3d9 allocator,
    virtual IUnknown * GetProcessorService() = 0;
    virtual MFXFrameAllocator * GetAllocator() = 0;

};
/*------------------------------------------------------------------------------*/
//remove readwritemid wrapper, used for d3d9 allocator for example
template <class T>
class MFFrameAllocatorRWDetach : public T
{
public:

    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr)
    {
        mfxMemId raw_mid = MFXReadWriteMid(mid).raw();
        return T::LockFrame(raw_mid, ptr);
    }
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr)
    {
        mfxMemId raw_mid = MFXReadWriteMid(mid).raw();
        return T::UnlockFrame(raw_mid, ptr);
    }
};
/*------------------------------------------------------------------------------*/

class MFFrameAllocator
    : public IUnknown
{
public:
    MFFrameAllocator(IMFAllocatorHelper * pActual);
    virtual ~MFFrameAllocator() {};

    // IUnknown methods
    virtual STDMETHODIMP_(ULONG) AddRef (void);
    virtual STDMETHODIMP_(ULONG) Release(void);
    virtual STDMETHODIMP QueryInterface(REFIID iid, void** ppv);

    virtual MFXFrameAllocator* GetMFXAllocator();
    virtual IUnknown * GetDeviceManager();
#ifdef CM_COPY_RESOURCE
    template <class T>
    CmCopyWrapper<T>& CreateCMCopyWrapper(const CComPtr<T> &device) {
        return m_factory.CreateCMCopyWrapper(device);
    }
#endif
protected:
    std::auto_ptr<IMFAllocatorHelper> m_pAllocator;
    long m_nRefCount; // reference count
#ifdef CM_COPY_RESOURCE
    CMCopyFactory m_factory;
#endif
};
/*------------------------------------------------------------------------------*/

//not an iunknown class
template <class T>
class MFAllocatorHelper
{
};

template <class T>
class MFAllocatorHelperBase
    : public T, public IMFAllocatorHelper
{
public:
    virtual MFXFrameAllocator * GetAllocator()
    {
        return (T*)this;
    }
    virtual IUnknown * GetProcessorService()
    {
        return NULL;
    }
};


//classes to be used in client code as prototypes for MFAllocator
template <>
class MFAllocatorHelper <D3DFrameAllocator> : public MFAllocatorHelperBase< MFFrameAllocatorRWDetach <D3DFrameAllocator> >
{
public:
    IUnknown * GetDeviceDxGI()
    {
        return D3DFrameAllocator::GetDeviceManager();
    }
    IUnknown * GetProcessorService()
    {
        return m_decoderService.p;
    }

};

#if MFX_D3D11_SUPPORT

template<>
class MFAllocatorHelper<D3D11FrameAllocator> : public MFAllocatorHelperBase<D3D11FrameAllocator>
{
public:
    IUnknown * GetDeviceDxGI()
    {
        return D3D11FrameAllocator::GetD3D11Device();
    }
};

#ifndef MF_D3D11_COPYSURFACES

template<>
class MFAllocatorHelper<MFD3D11FrameAllocator> : public MFAllocatorHelperBase<MFD3D11FrameAllocator>
{
public:
    IUnknown * GetDeviceDxGI()
    {
        return MFD3D11FrameAllocator::GetD3D11Device();
    }
};

#endif //#ifndef MF_D3D11_COPYSURFACES
#endif //#if MFX_D3D11_SUPPORT

#endif
