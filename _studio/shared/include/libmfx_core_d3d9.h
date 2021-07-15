// Copyright (c) 2007-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_common.h"
#if defined  (MFX_VA_WIN) && defined (MFX_D3D9_ENABLED)
#ifndef __LIBMFX_CORE__D3D9_H__
#define __LIBMFX_CORE__D3D9_H__


#include "libmfx_core.h"
#include "libmfx_allocator_d3d9.h"
#include "libmfx_core_factory.h"

#include "fast_compositing_ddi.h"
#include "auxiliary_device.h"

#include <memory>

// disable the "conditional expression is constant" warning
#pragma warning(disable: 4127)

class CmCopyWrapper;

namespace UMC
{
    class DXVA2Accelerator;
    class ProtectedVA;
};

class AuxiliaryDevice;

class D3D9DllCallHelper
{
public:
    D3D9DllCallHelper() :
        mDllHModule(0)
    {
        const wchar_t* d3d9dllname = L"d3d9.dll";
        DWORD prevErrorMode = 0;
        // set the silent error mode
#if (_WIN32_WINNT >= 0x0600) && !(__GNUC__)
        SetThreadErrorMode(SEM_FAILCRITICALERRORS, &prevErrorMode);
#else
        prevErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
#endif
        // this is safety check for universal build for d3d9 aviability.
        mDllHModule = LoadLibraryExW(d3d9dllname, NULL, 0);

        // set the previous error mode
#if (_WIN32_WINNT >= 0x0600) && !(__GNUC__)
        SetThreadErrorMode(prevErrorMode, NULL);
#else
        SetErrorMode(prevErrorMode);
#endif
    };

    HRESULT Direct3DCreate9Ex(UINT         SDKVersion, IDirect3D9Ex **ppD3D)
    {
        typedef HRESULT(WINAPI *Direct3DCreate9ExFn)(
            _In_  UINT         SDKVersion,
            _Out_ IDirect3D9Ex **ppD3D
            );

        if (ppD3D == NULL)
        {
            return  E_POINTER;
        }

        *ppD3D = NULL;

        if (mDllHModule == 0)
        {
            return  D3DERR_NOTAVAILABLE;
        }

        Direct3DCreate9ExFn pfn = (Direct3DCreate9ExFn)GetProcAddress(mDllHModule, "Direct3DCreate9Ex");

        if (pfn == NULL)
        {
            return ERROR_INVALID_FUNCTION;
        }

        return pfn(SDKVersion, ppD3D);
    }

    IDirect3D9 * Direct3DCreate9(UINT SDKVersion)
    {
        typedef IDirect3D9* (WINAPI *Direct3DCreate9Fn)(
            _In_  UINT         SDKVersion
            );
        if (mDllHModule == 0)
        {
            return  NULL;
        }

        Direct3DCreate9Fn pfn = (Direct3DCreate9Fn)GetProcAddress(mDllHModule, "Direct3DCreate9");

        if (pfn == NULL)
        {
            return NULL;
        }

        return pfn(SDKVersion);
    }

    bool isD3D9Available()
    {
        return mDllHModule != 0;
    }

    ~D3D9DllCallHelper()
    {
        if (mDllHModule)
        {
            FreeLibrary(mDllHModule);
            mDllHModule = 0;
        }
    }
private:
    HMODULE mDllHModule;
    // Do not allow copying of the object
    D3D9DllCallHelper(const D3D9DllCallHelper&);
    D3D9DllCallHelper& operator=(const D3D9DllCallHelper&);
};

mfxStatus               CreateD3DDevice(D3D9DllCallHelper& d3d9hlp,
                                        IDirect3D9        **pD3D,
                                        IDirect3DDevice9  **pDirect3DDevice,
                                        const mfxU32       adapterNum,
                                        mfxU16             width,
                                        mfxU16             height,
                                        mfxU32             DecId);

class D3D9VideoCORE : public CommonCORE
{
    //friend class VideoVPPHW;
public:
    friend class FactoryCORE;
    class D3D9Adapter : public D3D9Interface
    {
    public:
        D3D9Adapter(D3D9VideoCORE *pD3D9Core):m_pD3D9Core(pD3D9Core)
        {
        };
        virtual mfxStatus  GetD3DService(mfxU16 width,
                                         mfxU16 height,
                                         IDirectXVideoDecoderService **ppVideoService = NULL,
                                         bool isTemporal = false)
        {
            return m_pD3D9Core->GetD3DService(width, height, ppVideoService, isTemporal);
        };

        virtual IDirect3DDeviceManager9 * GetD3D9DeviceManager(void)
        {
            return m_pD3D9Core->m_pDirect3DDeviceManager;
        };

    protected:
        D3D9VideoCORE *m_pD3D9Core;

    };


    class CMEnabledCoreAdapter : public CMEnabledCoreInterface
    {
    public:
        CMEnabledCoreAdapter(D3D9VideoCORE *pD3D9Core) : m_pD3D9Core(pD3D9Core)
        {
        };
        virtual mfxStatus SetCmCopyStatus(bool enable)
        {
            return m_pD3D9Core->SetCmCopyStatus(enable);
        };
    protected:
        D3D9VideoCORE *m_pD3D9Core;
    };

    virtual ~D3D9VideoCORE();

    virtual mfxStatus     SetHandle(mfxHandleType type, mfxHDL handle);
    virtual mfxStatus     GetHandle(mfxHandleType type, mfxHDL *handle);
    virtual mfxStatus     AllocFrames(mfxFrameAllocRequest *request, 
                                      mfxFrameAllocResponse *response, bool isNeedCopy = true);

    virtual mfxStatus ReallocFrame(mfxFrameSurface1 *surf);

    virtual void          GetVA(mfxHDL* phdl, mfxU16 type) 
    {
        (type & MFX_MEMTYPE_FROM_DECODE)?(*phdl = m_pVA.get()):(*phdl = 0);
    };
    // Get the current working adapter's number
    virtual mfxU32 GetAdapterNumber(void) {return m_adapterNum;}
    virtual eMFXPlatform  GetPlatformType() {return  MFX_PLATFORM_HARDWARE;}

    virtual mfxStatus DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc);
    virtual mfxStatus DoFastCopyWrapper(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType);

    virtual eMFXHWType     GetHWType();

    mfxStatus              CreateVA(mfxVideoParam * param, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, UMC::FrameAllocator *allocator);
    // to check HW capatbilities
    mfxStatus              IsGuidSupported(const GUID guid, mfxVideoParam *par, bool isEncoder = false);
    #if defined (MFX_ENABLE_VPP)
    virtual void          GetVideoProcessing(mfxHDL* phdl) 
    {
        *phdl = &m_vpp_hw_resmng;
    };
    #endif
    mfxStatus              CreateVideoProcessing(mfxVideoParam * param);

    virtual eMFXVAType   GetVAType() const {return MFX_HW_D3D9; };

    virtual void* QueryCoreInterface(const MFX_GUID &guid);

    virtual mfxU16 GetAutoAsyncDepth() {return MFX_AUTO_ASYNC_DEPTH_VALUE;};//it can be platform based
    
    virtual bool IsCompatibleForOpaq();

protected:
    
    virtual mfxStatus              GetD3DService(mfxU16 width,
                                                 mfxU16 height,
                                                IDirectXVideoDecoderService **ppVideoService = NULL,
                                                bool isTemporal = false);

    D3D9VideoCORE(const mfxU32 adapterNum, const mfxU32 numThreadsAvailable, const mfxSession session = NULL);
    virtual void           Close();

    virtual mfxStatus      DefaultAllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);

    mfxStatus              CreateVideoAccelerator(mfxVideoParam * param, int NumOfRenderTarget, IDirect3DSurface9 **RenderTargets, UMC::FrameAllocator *allocator);
    mfxStatus              ProcessRenderTargets(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxBaseWideFrameAllocator* pAlloc);

private:

    mfxStatus              InitializeService(bool isTemporalCall);

    mfxStatus              InternalInit();
    mfxStatus              GetIntelDataPrivateReport(const GUID guid, DXVA2_ConfigPictureDecode & config);
    void                   ReleaseHandle();
    // this function should not be virtual
    mfxStatus SetCmCopyStatus(bool enable);

    HANDLE                                     m_hDirectXHandle; // if m_pDirect3DDeviceManager was used
    std::unique_ptr<UMC::DXVA2Accelerator>       m_pVA;
    #if defined (MFX_ENABLE_VPP)
    VPPHWResMng                          m_vpp_hw_resmng;
    #endif
    IDirect3D9                          *m_pD3D;
    // Ordinal number of adapter to work
    const mfxU32                         m_adapterNum;
    IDirect3DDeviceManager9             *m_pDirect3DDeviceManager;
    IDirect3DDevice9                    *m_pDirect3DDevice;
    IDirectXVideoDecoderService         *m_pDirectXVideoService;
    bool                                 m_bUseExtAllocForHWFrames;

    std::unique_ptr<mfxDefaultAllocatorD3D9::mfxWideHWFrameAllocator> m_pcHWAlloc;

    eMFXHWType                           m_HWType;
    eMFXGTConfig                         m_GTConfig;

    IDirect3DSurface9 *m_pSystemMemorySurface;

    bool m_bCmCopy;
    bool m_bCmCopySwap;
    bool m_bCmCopyAllowed;

    std::unique_ptr<CmCopyWrapper>            m_pCmCopy;
    std::unique_ptr<D3D9Adapter>              m_pAdapter;
    std::unique_ptr<CMEnabledCoreAdapter>     m_pCmAdapter;

    D3D9DllCallHelper m_d3d9hlp;
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    bool m_bIsBlockingTaskSyncEnabled;
#endif
};

#endif

#endif

