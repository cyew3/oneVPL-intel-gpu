/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_ddi.h

\* ****************************************************************************** */

#if !defined(__MFX_SCREEN_CAPTURE_DDI_H__)
#define __MFX_SCREEN_CAPTURE_DDI_H__

#include <list>
#include <d3d9.h>
#include <d3d11.h>
#include "mfxstructures.h"
#include "mfxplugin.h"

// {BF44DACD-217F-4370-A383-D573BC56707E}
DEFINE_GUID(DXVADDI_Intel_GetDesktopScreen, 
            0xbf44dacd, 0x217f, 0x4370, 0xa3, 0x83, 0xd5, 0x73, 0xbc, 0x56, 0x70, 0x7e);

#define D3DFMT_NV12 (D3DFORMAT)MAKEFOURCC('N','V','1','2')

namespace MfxCapture
{

typedef struct tagDESKTOP_QUERY_STATUS_PARAMS  
{
    UINT    StatusReportFeedbackNumber;
    UINT    uStatus;
} DESKTOP_QUERY_STATUS_PARAMS;

typedef struct tagDESKTOP_EXECUTE_PARAMS
{
    DXGI_FORMAT  DesktopFormat;
    UINT         StatusReportFeedbackNumber;
    USHORT       Width;
    USHORT      Height;
    UINT      Reserved;
} DESKTOP_EXECUTE_PARAMS;

typedef struct tagDESKTOP_FORMAT
{
    DXGI_FORMAT      DesktopFormat;
    USHORT           MaxWidth;
    USHORT           MaxHeight;
    UINT             Reserved[2];
} DESKTOP_FORMAT;

typedef struct tagDESKTOP_PARAM_STRUCT_SIZE
{
    UINT    SizeOfParamStruct;
    UINT    reserved;
} DESKTOP_PARAM_STRUCT_SIZE;

enum
{
    QUERY_SIZE = 32
};

enum
{
    DESKTOP_FORMAT_COUNT_ID = 0x100,
    DESKTOP_FORMATS_ID      = 0x101,
    DESKTOP_GETDESKTOP_ID   = 0x104,
    DESKTOP_QUERY_STATUS_ID = 0x105
};

struct AsyncParams
{
    mfxFrameSurface1 *surface_work;
    mfxFrameSurface1 *surface_out;
    mfxFrameSurface1 *real_surface;
    mfxU32 StatusReportFeedbackNumber;
};

enum CaptureMode{
    HW_D3D11 = 0x01,
    HW_D3D9  = 0x02,
    SW_D3D11 = 0x11,
    SW_D3D9  = 0x12
};

class Capturer
{
public:
    CaptureMode Mode;

    virtual ~Capturer(){}

    virtual
    mfxStatus CreateVideoAccelerator(mfxVideoParam const & par) = 0;

    virtual
    mfxStatus QueryVideoAccelerator(mfxVideoParam const & in, mfxVideoParam* out) = 0;

    virtual
    mfxStatus CheckCapabilities(mfxVideoParam const & in, mfxVideoParam* out) = 0;

    virtual
    mfxStatus Destroy() = 0;

    virtual
    mfxStatus BeginFrame(mfxMemId MemId) = 0;

    virtual
    mfxStatus EndFrame( ) = 0;

    virtual
    mfxStatus GetDesktopScreenOperation(mfxFrameSurface1 *surface_work, mfxU32& StatusReportFeedbackNumber) = 0;

    virtual
    mfxStatus QueryStatus(std::list<DESKTOP_QUERY_STATUS_PARAMS>& StatusList) = 0;

};

Capturer* CreatePlatformCapturer(mfxCoreInterface* core);
Capturer* CreateSWCapturer(mfxCoreInterface* core);

DXGI_FORMAT MfxFourccToDxgiFormat(const mfxU32& fourcc);
D3DFORMAT MfxFourccToD3dFormat(const mfxU32& fourcc);

} //namespace MfxCapture

#endif //#if !defined(__MFX_SCREEN_CAPTURE_DDI_H__)