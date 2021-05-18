// Copyright (c) 2012-2020 Intel Corporation
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

#if defined  (MFX_D3D11_ENABLED)
#ifndef _LIBMFX_ALLOCATOR_D3D11_H_
#define _LIBMFX_ALLOCATOR_D3D11_H_

// disable the "nonstandard extension used : nameless struct/union" warning
// dxva2api.h warning
#pragma warning(disable: 4201)


#include <d3d9.h>
#include <d3d11.h>
#include <atlbase.h>
#include "libmfx_allocator.h"

#include <algorithm>

#define MFX_FOURCC_R16_BGGR MAKEFOURCC('I','R','W','0')
#define MFX_FOURCC_R16_RGGB MAKEFOURCC('I','R','W','1')
#define MFX_FOURCC_R16_GRBG MAKEFOURCC('I','R','W','2')
#define MFX_FOURCC_R16_GBRG MAKEFOURCC('I','R','W','3')

const char RESOURCE_EXTENSION_KEY[16] = {
    'I','N','T','C',
    'E','X','T','N',
    'R','E','S','O',
    'U','R','C','E' };

const char CAPS_EXTENSION_KEY[16] = {
    'I','N','T','C',
    'E','X','T','N',
    'C','A','P','S',
    'F','U','N','C' };

#define EXTENSION_INTERFACE_VERSION 0x00040000

struct EXTENSION_BASE
{
    // Input
    char   Key[16];
    UINT   ApplicationVersion;
};

struct CAPS_EXTENSION : EXTENSION_BASE
{
    // Output:
    UINT    DriverVersion;          // EXTENSION_INTERFACE_VERSION
    UINT    DriverBuildNumber;      // BUILD_NUMBER
};

struct RESOURCE_EXTENSION_1_0: EXTENSION_BASE
{
    // Enumeration of the extension
    UINT  Type;   //RESOURCE_EXTENSION_TYPE

    // Extension data
    union
    {
        UINT    Data[16];
        UINT64  Data64[8];
    };
};

typedef RESOURCE_EXTENSION_1_0 RESOURCE_EXTENSION;

struct STATE_EXTENSION_TYPE_1_0
{
    static const UINT STATE_EXTENSION_RESERVED = 0;
};

struct STATE_EXTENSION_TYPE_4_0: STATE_EXTENSION_TYPE_1_0
{
    static const UINT STATE_EXTENSION_CONSERVATIVE_PASTERIZATION = 1 + EXTENSION_INTERFACE_VERSION;
};

struct RESOURCE_EXTENSION_TYPE_1_0
{
    static const UINT RESOURCE_EXTENSION_RESERVED      = 0;
    static const UINT RESOURCE_EXTENSION_DIRECT_ACCESS = 1;
};

struct RESOURCE_EXTENSION_TYPE_4_0: RESOURCE_EXTENSION_TYPE_1_0
{
    static const UINT RESOURCE_EXTENSION_CAMERA_PIPE = 1 + EXTENSION_INTERFACE_VERSION;
};

struct RESOURCE_EXTENSION_CAMERA_PIPE
{
    enum FORMAT_FLAGS {
        INPUT_FORMAT_IRW0 = 0x0,
        INPUT_FORMAT_IRW1 = 0x1,
        INPUT_FORMAT_IRW2 = 0x2,
        INPUT_FORMAT_IRW3 = 0x3
    };
};

bool inline IsBayerFormat(mfxU32 fourCC)
{
    return (MFX_FOURCC_R16_BGGR == fourCC ||
            MFX_FOURCC_R16_RGGB == fourCC ||
            MFX_FOURCC_R16_GBRG == fourCC ||
            MFX_FOURCC_R16_GRBG == fourCC ||
            MFX_FOURCC_R16      == fourCC);
}


inline mfxU32 BayerFourCC2FourCC (mfxU32 fourcc)
{
    return IsBayerFormat(fourcc) ? fourcc : 0u;
}

inline RESOURCE_EXTENSION_CAMERA_PIPE::FORMAT_FLAGS BayerFourCC2FormatFlag (mfxU32 fourcc)
{
    switch (fourcc)
    {
    case MFX_FOURCC_R16_BGGR:
        return RESOURCE_EXTENSION_CAMERA_PIPE::FORMAT_FLAGS::INPUT_FORMAT_IRW0;
    case MFX_FOURCC_R16_RGGB:
        return RESOURCE_EXTENSION_CAMERA_PIPE::FORMAT_FLAGS::INPUT_FORMAT_IRW1;
    case MFX_FOURCC_R16_GRBG:
        return RESOURCE_EXTENSION_CAMERA_PIPE::FORMAT_FLAGS::INPUT_FORMAT_IRW2;
    case MFX_FOURCC_R16_GBRG:
        return RESOURCE_EXTENSION_CAMERA_PIPE::FORMAT_FLAGS::INPUT_FORMAT_IRW3;
    default:
        return RESOURCE_EXTENSION_CAMERA_PIPE::FORMAT_FLAGS::INPUT_FORMAT_IRW0;
    }
}

template<typename Type>
inline HRESULT GetExtensionCaps(
    ID3D11Device* pd3dDevice,
    Type* pCaps )
{
    D3D11_BUFFER_DESC desc;
    ZeroMemory( &desc, sizeof(desc) );

    desc.ByteWidth      = sizeof(Type);
    desc.Usage          = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    D3D11_SUBRESOURCE_DATA initData;
    initData.pSysMem          = pCaps;
    initData.SysMemPitch      = sizeof(Type);
    initData.SysMemSlicePitch = 0;

    ZeroMemory( pCaps, sizeof(Type) );

    static_assert(sizeof(CAPS_EXTENSION_KEY) <= sizeof(pCaps->Key), "");
    std::copy(std::begin(CAPS_EXTENSION_KEY), std::end(CAPS_EXTENSION_KEY), pCaps->Key);

    pCaps->ApplicationVersion = EXTENSION_INTERFACE_VERSION;

    ID3D11Buffer* pBuffer = nullptr;
    HRESULT result = pd3dDevice->CreateBuffer(
        &desc,
        &initData,
        &pBuffer );

    if( pBuffer )
        pBuffer->Release();

    return result;
};
template<typename Type>
inline HRESULT SetResourceExtension(
    ID3D11Device* pd3dDevice,
    const Type* pExtnDesc )
{
    D3D11_BUFFER_DESC desc;
    ZeroMemory( &desc, sizeof(desc) );

    desc.ByteWidth      = sizeof(Type);
    desc.Usage          = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    D3D11_SUBRESOURCE_DATA initData;
    ZeroMemory( &initData, sizeof(initData) );

    initData.pSysMem          = pExtnDesc;
    initData.SysMemPitch      = sizeof(Type);
    initData.SysMemSlicePitch = 0;

    ID3D11Buffer* pBuffer = nullptr;
    HRESULT result = pd3dDevice->CreateBuffer(
        &desc,
        &initData,
        &pBuffer );

    if( pBuffer )
        pBuffer->Release();

    return result;
}

// Internal Allocators
namespace mfxDefaultAllocatorD3D11
{


    DXGI_FORMAT MFXtoDXGI(mfxU32 format);

    mfxStatus AllocFramesHW(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    mfxStatus LockFrameHW(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
    mfxStatus GetHDLHW(mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
    mfxStatus UnlockFrameHW(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr=0);
    mfxStatus FreeFramesHW(mfxHDL pthis, mfxFrameAllocResponse *response);
    mfxStatus ReallocFrameHW(mfxHDL pthis, const mfxMemId mid, const mfxFrameInfo *info);

    mfxStatus SetFrameData(const D3D11_TEXTURE2D_DESC& desc, const D3D11_MAPPED_SUBRESOURCE& LockedRect, mfxFrameData& frame_data);

    class mfxWideHWFrameAllocator : public  mfxBaseWideFrameAllocator
    {
    public:
        mfxWideHWFrameAllocator(mfxU16 type, ID3D11Device *pD11Device, ID3D11DeviceContext *pD11DeviceContext);
        virtual ~mfxWideHWFrameAllocator(void){};

        std::vector<ID3D11Texture2D*> m_SrfPool; // array of pointers
        ID3D11Texture2D* m_StagingSrfPool;

        //we are sure that Device & Context already queried
        ID3D11Device            *m_pD11Device;
        ID3D11DeviceContext     *m_pD11DeviceContext;

        mfxU32 m_NumSurface;

        bool isBufferKeeper;

    private:
        mfxWideHWFrameAllocator(const mfxWideHWFrameAllocator &);
        void operator=(const mfxWideHWFrameAllocator &);
    };

}

#if defined(MFX_ONEVPL)

class staging_texture
{
public:
    staging_texture(bool acquired, ID3D11Texture2D* texture, const D3D11_TEXTURE2D_DESC& description, std::mutex& mutex)
        : m_acquired(acquired)
        , m_description(description)
        , m_mutex(mutex)
    {
        m_texture.Attach(texture);
    }

    void revoke()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_acquired = false;
    }

    bool                     m_acquired;
    CComPtr<ID3D11Texture2D> m_texture;
    D3D11_TEXTURE2D_DESC     m_description;

private:
    std::mutex&              m_mutex;
};


static inline bool operator==(const DXGI_SAMPLE_DESC& l, const DXGI_SAMPLE_DESC& r)
{
    return MFX_EQ_FIELD(Count)
        && MFX_EQ_FIELD(Quality);
}

static inline bool operator==(const D3D11_TEXTURE2D_DESC& l, const D3D11_TEXTURE2D_DESC& r)
{
        return MFX_EQ_FIELD(Width)
            && MFX_EQ_FIELD(Height)
            && MFX_EQ_FIELD(MipLevels)
            && MFX_EQ_FIELD(ArraySize)
            && MFX_EQ_FIELD(Format)
            && MFX_EQ_FIELD(SampleDesc)
            //&& MFX_EQ_FIELD(Usage)
            //&& MFX_EQ_FIELD(BindFlags)
            //&& MFX_EQ_FIELD(CPUAccessFlags)
            && MFX_EQ_FIELD(MiscFlags);
}

class d3d11_texture_wrapper;

class staging_adapter_d3d11_texture
{
public:
    staging_adapter_d3d11_texture(mfxHDL device = nullptr)
        : m_pD11Device(reinterpret_cast<ID3D11Device*>(device))
    {}

    staging_texture* get_staging_texture(d3d11_texture_wrapper* main_texture, const D3D11_TEXTURE2D_DESC& descr);

    void UpdateCache(d3d11_texture_wrapper* texture)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_textures.erase(texture);
    }

    void UpdateCache(d3d11_texture_wrapper* texture, const D3D11_TEXTURE2D_DESC& descr)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // If old staging texture is suitable for mapping reallocated texture - do nothing
        if (m_textures.find(texture) == std::end(m_textures) || m_textures[texture]->m_description == descr)
            return;

        m_textures.erase(texture);
    }

    void SetDevice(mfxHDL device)
    {
        m_pD11Device = reinterpret_cast<ID3D11Device*>(device);
    }

private:
    ID3D11Device*              m_pD11Device;

    std::mutex                 m_mutex;

    // map: texture <-> preferred texture for staging
    std::map<d3d11_texture_wrapper*, std::shared_ptr<staging_texture>> m_textures;
};

class d3d11_resource_wrapper
{
public:
    d3d11_resource_wrapper(ID3D11Device* device)
        : m_pD11Device(device)
    {}

    virtual ~d3d11_resource_wrapper() {};

    virtual mfxStatus Lock(mfxFrameData& frame_data, mfxU32 flags) = 0;
    virtual mfxStatus Unlock()                                     = 0;
    virtual mfxStatus Realloc(const mfxFrameInfo & info)           = 0;

    ID3D11Resource* GetHandle() const { return m_resource; }

protected:
    CComPtr<ID3D11Resource>  m_resource;
    ID3D11Device*            m_pD11Device;
    D3D11_MAPPED_SUBRESOURCE m_LockedRect = {};
};

class d3d11_buffer_wrapper : public d3d11_resource_wrapper
{
public:
    d3d11_buffer_wrapper(const mfxFrameInfo &info, mfxHDL device);
    virtual mfxStatus Lock(mfxFrameData& frame_data, mfxU32 flags) override;
    virtual mfxStatus Unlock()                                     override;
    virtual mfxStatus Realloc(const mfxFrameInfo & info)           override;

private:
    mfxStatus AllocBuffer(const mfxFrameInfo & info);
};

struct unique_ptr_staging_texture : public std::unique_ptr<staging_texture, void(*)(staging_texture* texture)>
{
    unique_ptr_staging_texture(staging_texture* texture)
        : std::unique_ptr<staging_texture, void(*)(staging_texture* texture)>(
           texture, [](staging_texture* texture)
           {
               texture->revoke();
           })
    {}
};

class d3d11_texture_wrapper : public d3d11_resource_wrapper
{
public:
    d3d11_texture_wrapper(const mfxFrameInfo &info, mfxU16 type, staging_adapter_d3d11_texture& stg_adapter, mfxHDL device);

    virtual ~d3d11_texture_wrapper()
    {
        m_staging_adapter.UpdateCache(this);
    }

    virtual mfxStatus Lock(mfxFrameData& frame_data, mfxU32 flags) override;
    virtual mfxStatus Unlock()                                     override;
    virtual mfxStatus Realloc(const mfxFrameInfo & info)           override;

private:
    mfxStatus AllocFrame(const mfxFrameInfo & info);

    staging_adapter_d3d11_texture&  m_staging_adapter;
    unique_ptr_staging_texture      m_staging_surface;
    mfxU16                          m_type;
    bool                            m_was_locked_for_write = false;
};

struct mfxFrameSurface1_hw_d3d11 : public RWAcessSurface
{
    mfxFrameSurface1_hw_d3d11(const mfxFrameInfo & info, mfxU16 type, mfxMemId id, staging_adapter_d3d11_texture& stg_adapter,
                                mfxHDL device, mfxU32 context, FrameAllocatorBase& allocator);

    ~mfxFrameSurface1_hw_d3d11()
    {
        // Unmap surface if it is still mapped
        while (Locked())
        {
            if (MFX_FAILED(Unlock()))
                break;
        }
    }

    mfxStatus Lock(mfxU32 flags)                               override;
    mfxStatus Unlock()                                         override;
    std::pair<mfxHDL, mfxResourceType> GetNativeHandle() const override;
    std::pair<mfxHDL, mfxHandleType>   GetDeviceHandle() const override;

    mfxStatus GetHDL(mfxHDL& handle) const;
    mfxStatus Realloc(const mfxFrameInfo & info);

    static mfxU16 AdjustType(mfxU16 type)
    {
        type = mfxFrameSurface1_sw::AdjustType(type);

        if ((MFX_MEMTYPE_FROM_VPPOUT & type) && !(MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET & type))
        {
            type &= 0xFF0F;
            type |= MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
        }

        return type;
    }

private:
    mutable std::shared_timed_mutex         m_hdl_mutex;

    ID3D11Device*                           m_pD11Device;
    std::unique_ptr<d3d11_resource_wrapper> m_resource_wrapper;
    staging_adapter_d3d11_texture&          m_staging_adapter;
};

using FlexibleFrameAllocatorHW_D3D11 = FlexibleFrameAllocator<mfxFrameSurface1_hw_d3d11, staging_adapter_d3d11_texture>;

#endif

#endif
#endif
