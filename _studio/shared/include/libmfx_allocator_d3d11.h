/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2016 Intel Corporation. All Rights Reserved.

File Name: libmfx_allocator_d3d11.h

\* ****************************************************************************** */
#if defined  (MFX_VA)
#if defined  (MFX_D3D11_ENABLED)
#ifndef _LIBMFX_ALLOCATOR_D3D11_H_
#define _LIBMFX_ALLOCATOR_D3D11_H_

// disable the "nonstandard extension used : nameless struct/union" warning
// dxva2api.h warning
#pragma warning(disable: 4201)


#include <d3d9.h>
#include <d3d11.h>
#include "libmfx_allocator.h"

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
    if (MFX_FOURCC_R16_BGGR == fourCC ||
        MFX_FOURCC_R16_RGGB == fourCC ||
        MFX_FOURCC_R16_GBRG == fourCC ||
        MFX_FOURCC_R16_GRBG == fourCC ||
        MFX_FOURCC_R16 == fourCC )
    {
        return true;
    }

    return false;
}


inline mfxU32 BayerFourCC2FourCC (mfxU32 fourcc)
{
    if ( MFX_FOURCC_R16_BGGR == fourcc )
    {
        return MFX_FOURCC_R16_BGGR;
    }
    else if ( MFX_FOURCC_R16_RGGB == fourcc )
    {
        return MFX_FOURCC_R16_RGGB;
    }
    else if ( MFX_FOURCC_R16_GRBG == fourcc )
    {
        return MFX_FOURCC_R16_GRBG;
    }
    else if ( MFX_FOURCC_R16_GBRG == fourcc )
    {
        return MFX_FOURCC_R16_GBRG;
    }

    return 0;

}

inline RESOURCE_EXTENSION_CAMERA_PIPE::FORMAT_FLAGS BayerFourCC2FormatFlag (mfxU32 fourcc)
{
    if ( MFX_FOURCC_R16_BGGR == fourcc )
    {
        return RESOURCE_EXTENSION_CAMERA_PIPE::FORMAT_FLAGS::INPUT_FORMAT_IRW0;
    }
    else if ( MFX_FOURCC_R16_RGGB == fourcc )
    {
        return RESOURCE_EXTENSION_CAMERA_PIPE::FORMAT_FLAGS::INPUT_FORMAT_IRW1;
    }
    else if ( MFX_FOURCC_R16_GRBG == fourcc )
    {
        return RESOURCE_EXTENSION_CAMERA_PIPE::FORMAT_FLAGS::INPUT_FORMAT_IRW2;
    }
    else if ( MFX_FOURCC_R16_GBRG == fourcc )
    {
        return RESOURCE_EXTENSION_CAMERA_PIPE::FORMAT_FLAGS::INPUT_FORMAT_IRW3;
    }

    return RESOURCE_EXTENSION_CAMERA_PIPE::FORMAT_FLAGS::INPUT_FORMAT_IRW0;

}

template<typename Type>
inline HRESULT GetExtensionCaps(
    ID3D11Device* pd3dDevice,
    Type* pCaps )
{
    D3D11_BUFFER_DESC desc;
    ZeroMemory( &desc, sizeof(desc) );
    desc.ByteWidth = sizeof(Type);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    D3D11_SUBRESOURCE_DATA initData;
    initData.pSysMem = pCaps;
    initData.SysMemPitch = sizeof(Type);
    initData.SysMemSlicePitch = 0;
    ZeroMemory( pCaps, sizeof(Type) );
    memcpy_s( pCaps->Key,sizeof(pCaps->Key), CAPS_EXTENSION_KEY,16
         );
    pCaps->ApplicationVersion = EXTENSION_INTERFACE_VERSION;
    ID3D11Buffer* pBuffer = NULL;
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
    desc.ByteWidth = sizeof(Type);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    D3D11_SUBRESOURCE_DATA initData;
    ZeroMemory( &initData, sizeof(initData) );
    initData.pSysMem = pExtnDesc;
    initData.SysMemPitch = sizeof(Type);
    initData.SysMemSlicePitch = 0;

    ID3D11Buffer* pBuffer = NULL;
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

    class mfxWideHWFrameAllocator : public  mfxBaseWideFrameAllocator
    {
    public:
        std::vector<ID3D11Texture2D*> m_SrfPool; // array of pointers
        ID3D11Texture2D* m_StagingSrfPool;
        mfxWideHWFrameAllocator(mfxU16 type, ID3D11Device *pD11Device, ID3D11DeviceContext *pD11DeviceContext);
        virtual ~mfxWideHWFrameAllocator(void){};

        //we are sure that Device & Context already queryied
        ID3D11Device            *m_pD11Device;
        ID3D11DeviceContext     *m_pD11DeviceContext;

        mfxU32 m_NumSurface;

        bool isBufferKeeper;

    };

}

#endif
#endif
#endif
