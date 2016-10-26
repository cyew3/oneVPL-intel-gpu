//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2010-2013 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_PLATFORM_HEADERS_H__
#define __MFX_PLATFORM_HEADERS_H__

#include <mfxvideo++int.h>

#if (defined(_WIN32) || defined(_WIN64)) 
#include <windows.h>
#include <d3d9.h>
#include <dxva.h>
#include <dxva2api.h>
#include <ddraw.h>

//#if defined(MFX_D3D11_ENABLED)
#include <d3d11.h>
//#endif

#ifndef D3DDDIFORMAT
#define D3DDDIFORMAT        D3DFORMAT
#endif

typedef IDirectXVideoDecoderService*    _mfxPlatformAccelerationService;
typedef IDirect3DSurface9*              _mfxPlatformVideoSurface;

#else // #if (defined(_WIN32) || defined(_WIN64)) 

#if (defined(LINUX32) || defined(LINUX64) )
    #if defined(MFX_VA)
        /* That's tricky: if LibVA will not be installed on the machine, you should be able
         * to build SW Media SDK and some apps in SW mode. Thus, va.h should not be visible.
         * Since we develop on machines where LibVA is installed, we forget about LibVA-free
         * build sometimes. So, that's the reminder why MFX_VA protection was added here.
         */
        #include <va/va.h>
        typedef VADisplay                       _mfxPlatformAccelerationService;
        typedef VASurfaceID                     _mfxPlatformVideoSurface;
    #endif // #if defined(MFX_VA)
#endif // #if (defined(LINUX32) || defined(LINUX64) )

#ifndef D3DDDIFORMAT
#define D3DDDIFORMAT        D3DFORMAT
#endif

typedef int             BOOL;
typedef char            CHAR;
typedef unsigned char   BYTE;
typedef short           SHORT;
typedef int             INT;


typedef unsigned char  UCHAR;
typedef unsigned short USHORT;
typedef unsigned int   UINT;

#if defined(_WIN32) || defined(_WIN64)
typedef long            LONG;
typedef unsigned long   ULONG;
typedef unsigned long   DWORD;
#else
typedef int            LONG;  // should be 32 bit to align with Windows
typedef unsigned int   ULONG;
typedef unsigned int   DWORD;
#endif

typedef unsigned long long UINT64;

#define FALSE               0
#define TRUE                1

// DXVA2 types
struct IDirect3DSurface9;
struct IDirect3DDeviceManager9;
struct ID3D11Texture2D;
struct ID3D11Device;

typedef int D3DFORMAT;

typedef struct _DXVA2_Frequency
{
    UINT Numerator;
    UINT Denominator;
}     DXVA2_Frequency;

typedef struct _DXVA2_ExtendedFormat
{
    union 
    {
        struct 
        {
            UINT SampleFormat    : 8;
            UINT VideoChromaSubsampling    : 4;
            UINT NominalRange    : 3;
            UINT VideoTransferMatrix    : 3;
            UINT VideoLighting    : 4;
            UINT VideoPrimaries    : 5;
            UINT VideoTransferFunction    : 5;
        }     ;
        UINT value;
    }     ;
}     DXVA2_ExtendedFormat;

typedef struct _DXVA2_VideoDesc
{
    UINT SampleWidth;
    UINT SampleHeight;
    DXVA2_ExtendedFormat SampleFormat;
    D3DFORMAT Format;
    DXVA2_Frequency InputSampleFreq;
    DXVA2_Frequency OutputFrameFreq;
    UINT UABProtectionLevel;
    UINT Reserved;
}     DXVA2_VideoDesc;


typedef struct _D3DAES_CTR_IV
{
    UINT64   IV;         // Big-Endian IV
    UINT64   Count;      // Big-Endian Block Count
} D3DAES_CTR_IV;

#undef  LOWORD
#define LOWORD(_dw)     ((_dw) & 0xffff)
#undef  HIWORD
#define HIWORD(_dw)     (((_dw) >> 16) & 0xffff)


//typedef int GUID;

//static const GUID DXVA2_Intel_Encode_AVC = 0;
//static const GUID DXVA2_Intel_Encode_MPEG2 = 1;

#endif // #if (defined(_WIN32) || defined(_WIN64)) 

#endif // __MFX_PLATFORM_HEADERS_H__
