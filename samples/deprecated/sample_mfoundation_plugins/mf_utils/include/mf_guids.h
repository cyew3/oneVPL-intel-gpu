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

File: mf_guids.h

Purpose: define common code for MSDK based MF plug-ins.

Defined Classes, Structures & Enumerations:
  * GUID_info - stores types of MFT (major & sub types)
  * ClassRegData - stores registration data of MF plug-ins

Defined Types:
  * CreateInstanceFn - defines type of plug-ins CreateInstance functions
  * FillParamsFn - defines type of parameters filling functions
  * ClassRegDataFn - defines type of helper function

Defined Macroses:
  * CHARS_IN_GUID - number of chars in GUID
  * SAFE_NEW - calls new for specified class, catches exceptions
  * SAFE_NEW_ARRAY - calls new for specified array, catches exceptions
  * SAFE_DELETE - deletes class object and frees pointer
  * SAFE_DELETE_ARRAY - deletes array object and frees pointer
  * SAFE_RELEASE - releases com object and frees pointer
  * SAFE_FREE - deletes variable and frees pointer
  * myfopen - redirection on fsopen
  * mywfopen - redirection on _wfsopen

Defined Global Variables:
  * g_UncompressedVideoTypes - list of uncompressed video types used in plug-ins
  * g_DecoderRegFlags - decoder MFTs registry flags
  * g_EncoderRegFlags - encoder MFTs registry flags
  # GUIDS
  * CLSID_MF_MPEG2EncFilter
  * CLSID_MF_H264EncFilter
  * CLSID_MF_MPEG2DecFilter
  * CLSID_MF_H264DecFilter
  * CLSID_MF_VC1DecFilter
  * MFVideoFormat_NV12_MFX
  * MEDIASUBTYPE_MPEG1_MFX
  * MEDIASUBTYPE_MPEG2_MFX
  * MEDIASUBTYPE_H264_MFX
  * MEDIASUBTYPE_VC1_MFX
  * MEDIASUBTYPE_VC1P_MFX

Defined Global Functions:
  * myDllMain
  * AMovieSetupRegisterServer
  * AMovieSetupUnregisterServer
  * CreateVDecPlugin - creates video decoder plug-in instance
  * CreateVEncPlugin - creates video encoder plug-in instance

*********************************************************************************/

#ifndef __MF_GUIDS_H__
#define __MF_GUIDS_H__

// switching off Microsoft warnings
#pragma warning(disable: 4201) // nameless structs/unions
#pragma warning(disable: 4995) // 'name' was declared deprecated
#include <windows.h>
#include <tchar.h>
#include <assert.h>
#include <strsafe.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <share.h>
#include <list>
#include <queue>
#include <objbase.h>
#include <uuids.h>
#include <initguid.h>
#include <atlbase.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <mftransform.h>
#include <dmoreg.h>      // DirectX SDK registration
#include <wmpservices.h>
#include <wmsdkidl.h>
#include <wmcodecdsp.h>
#include <evr.h>
#include <d3d9.h>
#if MFX_D3D11_SUPPORT
#include <d3d11.h>
#endif
#include <dxva2api.h>
#include <KS.h>
#include <Codecapi.h>
#include <vfwmsgs.h>
#include "mf_win_blue.h" // TODO: remove when switched to Windows SDK 8.1
#include <Psapi.h>
#include <Opmapi.h>
#pragma warning(default: 4995) // 'name' was declared deprecated
#pragma warning(default: 4201) // nameless structs/unions

/*------------------------------------------------------------------------------*/

#include "mfxdefs.h"
#include "mfxstructures.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"

/*------------------------------------------------------------------------------*/
// debug tracing

#define MFX_TRACE_DISABLE
#include "mfx_trace.h"

#define MF_TRACE_OFILE_NAME "C:\\mfx_trace_printf_plugins.log"
#define MF_TRACE_OSTAT_NAME "C:\\mfx_trace_stat_plugins.log"

//#define MF_SHORT_CAT_NAMES

// Levels
// Do not use this level, it is intended for debug purposes only
#define MF_TL_DEBUG    MFX_TRACE_LEVEL_0
// Critical: most important functions and parameters (performance analysis)
#define MF_TL_PERF     MFX_TRACE_LEVEL_1
// Macroses: special level for macroses (errors checking, etc.)
#define MF_TL_MACROSES MFX_TRACE_LEVEL_2
// General: most part of functions (which does not cause big trace)
#define MF_TL_GENERAL  MFX_TRACE_LEVEL_3
// Notes: all remained, paranoic details
#define MF_TL_NOTES    MFX_TRACE_LEVEL_4

/*------------------------------------------------------------------------------*/

#define CHARS_IN_GUID  39

//it is possible that exception happened in ctor

//TODO: assuming it is true, and you'll be able to call destructor via delet ptr; but c standard prohibits deletion of partially constructed object
//that is why your pointer (p) - will be zero since = operator should be called after constructor that thrown an exception, and following delete 0 means nothing
// propose to remove SAVE_DELETE section
#ifndef SAFE_NEW
#define SAFE_NEW(P, C){ (P) = NULL; try { (P) = new C; } catch(...) { SAFE_DELETE(P); ATLASSERT(NULL != (P)); } }
#endif

#ifndef SAFE_NEW_ARRAY
#define SAFE_NEW_ARRAY(P, C, N){ (P) = NULL; try { (P) = new C[N]; } catch(...) { SAFE_DELETE_ARRAY(P); ATLASSERT(NULL != (P)); } }
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(P) {if (P) {delete (P); (P) = NULL;}}
#endif

#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(P) {if (P) {delete[] (P); (P) = NULL;}}
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(P) if (P) { (P)->Release(); (P) = NULL; }
#endif

#ifndef SAFE_FREE
#define SAFE_FREE(P) if (P) { free(P); (P) = NULL; }
#endif

#define SAFE_MFT_FREE(P) if (P) { CoTaskMemFree(P); (P) = NULL; }

#define myfopen(FNAME, FMODE) _fsopen(FNAME, FMODE, _SH_DENYNO)
#define mywfopen(FNAME, FMODE) _wfsopen(FNAME, FMODE, _SH_DENYNO)

/*------------------------------------------------------------------------------*/

// Structure to store input types of MFT
typedef struct {
    GUID major_type; // major type (VIDEO, AUDIO, etc)
    GUID subtype;    // subtype
} GUID_info;

struct ClassRegData;

// Function pointer for creating COM objects. (Used by the class factory.)
typedef HRESULT (*CreateInstanceFn)(REFIID iid,
                                    void **ppObject,
                                    ClassRegData *pRegistrationData);

// Function pointer to work with codec params
typedef mfxStatus (*CodecParamsFn)(mfxVideoParam* pVideoParams);

// Function pointer to some function
typedef HRESULT (*ClassRegDataFn)(void);

// Class factory data.
// Defines a look-up table of class IDs and corresponding creation functions.
struct ClassRegData
{
    GUID*             guidClassID;          // Class ID of plug-in
    LPWSTR            pPluginName;          // friendly name
    UINT32            iPluginCategory;      // category
    CreateInstanceFn  pCreationFunction;    // create function
    CodecParamsFn     pFillParams;          // function to fill codec params
    CodecParamsFn     pFreeParams;          // function to free codec params
    GUID_info*        pInputTypes;          // array of input types
    DWORD             cInputTypes;          // size of array of input types
    GUID_info*        pOutputTypes;         // array of output types
    DWORD             cOutputTypes;         // size of array of output types
    UINT32            iFlags;               // registry flags (for MFTs only)
    IMFAttributes*    pAttributes;          // MFT attributes
    LPWSTR*           pFileExtensions;      // for ByteStreamHandler only
    UINT32            cFileExtensions;      // for ByteStreamHandler only
    ClassRegDataFn    pDllRegisterFn;       // plug-in specific register function
    ClassRegDataFn    pDllUnregisterFn;     // plug-in specific unregister function
};

// MF plug-in types
enum
{
    REG_UNKNOWN                 = 0x000,
    REG_AS_BYTE_STREAM_HANDLER  = 0x001,
    REG_AS_AUDIO_DECODER        = 0x002,
    REG_AS_VIDEO_DECODER        = 0x004,
    REG_AS_AUDIO_ENCODER        = 0x008,
    REG_AS_VIDEO_ENCODER        = 0x010,
    REG_AS_AUDIO_EFFECT         = 0x020,
    REG_AS_VIDEO_EFFECT         = 0x040,
    REG_AS_VIDEO_PROCESSOR      = 0x080,
    REG_AS_SINK                 = 0x100,
    REG_AS_WMP_PLUGIN           = 0x200
};

BOOL myDllMain(HANDLE hModule,
               DWORD  ul_reason_for_call,
               ClassRegData *pClassRegData,
               UINT32 numberClassRegData);

STDAPI AMovieSetupRegisterServer(CLSID   clsServer,
                                 LPCWSTR szDescription,
                                 LPCWSTR szFileName,
                                 LPCWSTR szThreadingModel = L"Both",
                                 LPCWSTR szServerType = L"InprocServer32");

STDAPI AMovieSetupUnregisterServer(CLSID clsServer);

/*------------------------------------------------------------------------------*/
// Plug-ins GUIDs

// {07F19984-4FC7-45ba-9AD0-418449E81283}
DEFINE_GUID(CLSID_MF_MPEG2EncFilter,
0x07f19984, 0x4fc7, 0x45ba, 0x9a, 0xd0, 0x41, 0x84, 0x49, 0xe8, 0x12, 0x83);

// {4BE8D3C0-0515-4a37-AD55-E4BAE19AF471}
DEFINE_GUID(CLSID_MF_H264EncFilter,
0x4be8d3c0, 0x515, 0x4a37, 0xad, 0x55, 0xe4, 0xba, 0xe1, 0x9a, 0xf4, 0x71);

// {CD5BA7FF-9071-40e9-A462-8DC5152B1776}
DEFINE_GUID(CLSID_MF_MPEG2DecFilter,
0xcd5ba7ff, 0x9071, 0x40e9, 0xa4, 0x62, 0x8d, 0xc5, 0x15, 0x2b, 0x17, 0x76);

// {45E5CE07-5AC7-4509-94E9-62DB27CF8F96}
DEFINE_GUID(CLSID_MF_H264DecFilter,
0x45e5ce07, 0x5ac7, 0x4509, 0x94, 0xe9, 0x62, 0xdb, 0x27, 0xcf, 0x8f, 0x96);

// {059A5BAE-5D7A-4c5e-8F7A-BFD57D1D6AAA}
DEFINE_GUID(CLSID_MF_VC1DecFilter,
0x59a5bae, 0x5d7a, 0x4c5e, 0x8f, 0x7a, 0xbf, 0xd5, 0x7d, 0x1d, 0x6a, 0xaa);

// {00C69F81-0524-48C0-A353-4DD9D54F9A6E}
DEFINE_GUID(CLSID_MF_MJPEGDecFilter,
0xc69f81, 0x524, 0x48c0, 0xa3, 0x53, 0x4d, 0xd9, 0xd5, 0x4f, 0x9a, 0x6e);

// {EE69B504-1CBF-4ea6-8137-BB10F806B014}
DEFINE_GUID(CLSID_MF_VppFilter,
0xee69b504, 0x1cbf, 0x4ea6, 0x81, 0x37, 0xbb, 0x10, 0xf8, 0x6, 0xb0, 0x14);

// {763AA0AC-05EC-43ac-968C-73D949CDE876}
DEFINE_GUID(CLSID_MF_HEVCDecFilter,
0x763aa0ac, 0x5ec, 0x43ac, 0x96, 0x8c, 0x73, 0xd9, 0x49, 0xcd, 0xe8, 0x76);


/*--------------------------------------------------------------------*/
// Additional Color Format GUIDs

// {CB08E88B-3961-42ae-BA67-FF47CCC13EED}
DEFINE_GUID(MFVideoFormat_NV12_MFX,
MAKEFOURCC('N','V','1','2'), 0x3961, 0x42ae, 0xba, 0x67, 0xff, 0x47, 0xcc, 0xc1, 0x3e, 0xed);

/*--------------------------------------------------------------------*/
// Additional Media Types GUIDs

DEFINE_GUID(MEDIASUBTYPE_MPEG1_MFX,
MAKEFOURCC('M','P','G','1'), 0x0000, 0x0010, 0x90, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

DEFINE_GUID(MEDIASUBTYPE_MPEG2_MFX,
MAKEFOURCC('M','P','G','2'), 0x0000, 0x0010, 0x90, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

DEFINE_GUID(MEDIASUBTYPE_H264_MFX,
MAKEFOURCC('H','2','6','4'), 0x0000, 0x0010, 0x90, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

DEFINE_GUID(MEDIASUBTYPE_VC1_MFX, // pure VC1
0x59e58851, 0x02b2, 0x431c, 0xa3, 0x7f, 0xe5, 0x5d, 0x98, 0xeb, 0x09, 0x72);

DEFINE_GUID(MEDIASUBTYPE_VC1P_MFX,
0xe77f04f1, 0xd2cf, 0x42d3, 0xbd, 0x8f, 0xe7, 0xfe, 0x63, 0x19, 0xc2, 0x6a);

/*--------------------------------------------------------------------*/
// HW support GUIDs

// {26F6BB9A-EAA2-45f8-86D2-69DCD984B9B1}
DEFINE_GUID(IID_IMFDeviceDXVA,
0x26f6bb9a, 0xeaa2, 0x45f8, 0x86, 0xd2, 0x69, 0xdc, 0xd9, 0x84, 0xb9, 0xb1);

// {85E4DCCF-F1FE-4117-854D-7CDA2ACC2C77}
// Media type attribute containing IUnknown pointer to IMFDeviceDXVA
DEFINE_GUID(MF_MT_D3D_DEVICE,
0x85e4dccf, 0xf1fe, 0x4117, 0x85, 0x4d, 0x7c, 0xda, 0x2a, 0xcc, 0x2c, 0x77);

// {80860F81-298D-4828-BB7A-323558ECB6AF}
// Media type attribute containing decoder's subtype
DEFINE_GUID(MF_MT_DEC_SUBTYPE,
0x80860f81, 0x298d, 0x4828, 0xbb, 0x7a, 0x32, 0x35, 0x58, 0xec, 0xb6, 0xaf);

// {4ACC65CB-BE19-4cd6-80BD-B28F8E112054}
// Media type attribute containing error occurred in dowsntream plug-in
DEFINE_GUID(MF_MT_DOWNSTREAM_ERROR,
0x4acc65cb, 0xbe19, 0x4cd6, 0x80, 0xbd, 0xb2, 0x8f, 0x8e, 0x11, 0x20, 0x54);

// {7E151065-C321-4e28-A6A5-BA4C84DAA7B9}
DEFINE_GUID(MF_MT_MFX_FRAME_SRF,
0x7e151065, 0xc321, 0x4e28, 0xa6, 0xa5, 0xba, 0x4c, 0x84, 0xda, 0xa7, 0xb9);

// {FC8875C8-8B57-479e-B89E-D4D10E174645}
DEFINE_GUID(MF_MT_FAKE_SRF,
0xfc8875c8, 0x8b57, 0x479e, 0xb8, 0x9e, 0xd4, 0xd1, 0xe, 0x17, 0x46, 0x45);

/*--------------------------------------------------------------------*/
// Other GUIDs

// {31670B7E-6A65-4a0f-BC78-62B0AE86DDC3}
DEFINE_GUID(IID_IMfxFrameSurface,
0x31670b7e, 0x6a65, 0x4a0f, 0xbc, 0x78, 0x62, 0xb0, 0xae, 0x86, 0xdd, 0xc3);

// {7F48BBA1-8680-4af9-83BD-DE8FBBD3B32D}
DEFINE_GUID(IID_MFVideoBuffer,
0x7f48bba1, 0x8680, 0x4af9, 0x83, 0xbd, 0xde, 0x8f, 0xbb, 0xd3, 0xb3, 0x2d);

/*--------------------------------------------------------------------*/
// Global variables

static const GUID_info g_UncompressedVideoTypes[] =
{
    { MFMediaType_Video, MFVideoFormat_NV12_MFX },
    { MFMediaType_Video, MFVideoFormat_NV12 },
    //{ MFMediaType_Video, MFVideoFormat_YV12 },
};

#if MFX_D3D11_SUPPORT
static const UINT32 g_DecoderRegFlags = MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_ASYNCMFT;
#else
static const UINT32 g_DecoderRegFlags = MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_TRANSCODE_ONLY;
#endif

static const UINT32 g_EncoderRegFlags = MFT_ENUM_FLAG_HARDWARE;
static const UINT32 g_VppRegFlags     = MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_TRANSCODE_ONLY;

extern HINSTANCE g_hInst;

/*--------------------------------------------------------------------*/

static const mfxU32 g_tabDoNotUseVppAlg[] =
{
    MFX_EXTBUFF_VPP_DENOISE,
    MFX_EXTBUFF_VPP_SCENE_ANALYSIS,
    MFX_EXTBUFF_VPP_PROCAMP,
    MFX_EXTBUFF_VPP_DETAIL
};

static const mfxExtVPPDoNotUse g_extVppDoNotUse =
{
    {
        MFX_EXTBUFF_VPP_DONOTUSE, // BufferId
        sizeof(mfxExtVPPDoNotUse) // BufferSz
    }, // Header
    sizeof(g_tabDoNotUseVppAlg)/sizeof(mfxU32), // NumAlg
    (mfxU32*)&g_tabDoNotUseVppAlg //AlgList
};

static const mfxExtBuffer* g_pVppExtBuf[] =
{
    (mfxExtBuffer*)&g_extVppDoNotUse
};

/*------------------------------------------------------------------------------*/

extern mfxVersion g_MfxVersion;

/*------------------------------------------------------------------------------*/

extern void DllAddRef(void);
extern void DllRelease(void);

/*--------------------------------------------------------------------*/
// Create functions

extern HRESULT CreateVEncPlugin(REFIID iid, void **ppObject, ClassRegData *pRegistrationData);
extern HRESULT CreateVDecPlugin(REFIID iid, void **ppObject, ClassRegData *pRegistrationData);
extern HRESULT CreateVppPlugin (REFIID iid, void **ppObject, ClassRegData *pRegistrationData);

#define MFT_ENUM_HARDWARE_URL_STRING _T("AA243E5D-2F73-48c7-97F7-F6FA17651651")

#endif // #ifndef __MF_GUIDS_H__
