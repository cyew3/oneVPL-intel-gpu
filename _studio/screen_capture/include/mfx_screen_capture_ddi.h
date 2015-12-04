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

#include "ipp.h"

#define ENABLE_WORKAROUND_FOR_DRIVER_RESIZE_ISSUE
#define ENABLE_RUNTIME_SW_FALLBACK

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

typedef struct tagDESKTOP_DISPLAY_SELECT
{
    UINT    DisplayID;
    UINT    reserved;
} DESKTOP_DISPLAY_SELECT;

enum
{
    QUERY_SIZE = 32
};

enum
{
    DESKTOP_FORMAT_COUNT_ID   = 0x100,
    DESKTOP_FORMATS_ID        = 0x101,
    DESKTOP_DISPLAY_SELECT_ID = 0x102,
    DESKTOP_GETDESKTOP_ID     = 0x104,
    DESKTOP_QUERY_STATUS_ID   = 0x105
};

struct AsyncParams
{
    mfxFrameSurface1 *surface_out;            //output user surface, system or video or opaq
    mfxFrameSurface1 *real_surface;           //internal video or user-video surface
    mfxFrameSurface1 *dirty_rect_surface;     //user-provided surface, system or video or opaq
    mfxFrameSurface1 *dirty_rect_surface_int; //internal video or user video surface
    mfxU32 StatusReportFeedbackNumber;
    mfxExtDirtyRect  *ext_rect;
    bool rt_fallback_dxgi;
    bool rt_fallback_d3d;
};

enum CaptureMode{
    HW_D3D11 = 0x01,
    HW_D3D9  = 0x02,
    SW_D3D11 = 0x11,
    SW_D3D9  = 0x12
};

enum DirtyRectMode{
    SW_DR = 0x01,
    CM_DR = 0x02
};

class Capturer
{
public:
    CaptureMode Mode;

    virtual ~Capturer(){}

    virtual
    mfxStatus CreateVideoAccelerator(mfxVideoParam const & par, const mfxU32 targetId = 0) = 0;

    virtual
    mfxStatus QueryVideoAccelerator(mfxVideoParam const & in, mfxVideoParam* out, const mfxU32 targetId = 0) = 0;

    virtual
    mfxStatus CheckCapabilities(mfxVideoParam const & in, mfxVideoParam* out) = 0;

    virtual
    mfxStatus Destroy() = 0;

    virtual
    mfxStatus BeginFrame( mfxFrameSurface1* pSurf ) = 0;

    virtual
    mfxStatus EndFrame( ) = 0;

    virtual
    mfxStatus GetDesktopScreenOperation(mfxFrameSurface1 *surface_work, mfxU32& StatusReportFeedbackNumber) = 0;

    virtual
    mfxStatus QueryStatus(std::list<DESKTOP_QUERY_STATUS_PARAMS>& StatusList) = 0;

};

class DirtyRectFilter
{
public:
    DirtyRectMode Mode;

    virtual ~DirtyRectFilter(){}

    virtual
    mfxStatus Init(const mfxVideoParam* par, bool isSysMem, bool isOpaque = false) = 0;

    virtual
    mfxStatus Init(const mfxFrameInfo& in, const mfxFrameInfo& out) = 0;

    virtual
    mfxStatus GetParam(mfxFrameInfo& in, mfxFrameInfo& out) = 0;

    virtual
    mfxStatus RunFrameVPP(mfxFrameSurface1& in, mfxFrameSurface1& out) = 0;
};

Capturer* CreatePlatformCapturer(mfxCoreInterface* core);
Capturer* CreateSWCapturer(mfxCoreInterface* core);

DXGI_FORMAT MfxFourccToDxgiFormat(const mfxU32& fourcc);
D3DFORMAT MfxFourccToD3dFormat(const mfxU32& fourcc);

bool IsWin10orLater();

#define MAX_DISPLAYS 32
struct dispDescr
{
    unsigned int IndexNumber; //Like Windows screen resolution window enumeration
    LUID         DXGIAdapterLuid;
    unsigned int TargetID; //DISPLAYCONFIG_PATH_TARGET_INFO.id  /* The target identifier on the specified adapter that this path relates to. */
    unsigned int width;
    unsigned int height;
    POINTL       position;
};
struct displaysDescr
{
    unsigned int n;
    dispDescr display[MAX_DISPLAYS];
};
void FindAllConnectedDisplays(displaysDescr& displays);
dispDescr GetTargetId(mfxU32 dispIndex);
mfxU32 GetDisplayIndex(mfxU32 targetId);

class OwnResizeFilter
{
public:
    OwnResizeFilter();
    virtual ~OwnResizeFilter();
    mfxStatus Init(const mfxFrameInfo& in, const mfxFrameInfo& out);
    mfxStatus GetParam(mfxFrameInfo& in, mfxFrameInfo& out);
    mfxStatus RunFrameVPP(mfxFrameSurface1& in, mfxFrameSurface1& out);

protected:
    mfxStatus rs_RGB4( mfxFrameSurface1* in, mfxFrameSurface1* out );
    mfxStatus rs_NV12( mfxFrameSurface1* in, mfxFrameSurface1* out );

    IppiResizeYUV420Spec* m_pSpec;
    IppiResizeSpec_32f*   m_pRGBSpec;
    mfxU8*                m_pWorkBuffer;
    IppiInterpolationType m_interpolation;
    mfxFrameInfo          m_in;
    mfxFrameInfo          m_out;

private:
    //prohobit copy constructor
    OwnResizeFilter(const OwnResizeFilter& that);
    //prohibit assignment operator
    OwnResizeFilter& operator=(const OwnResizeFilter&);
};

} //namespace MfxCapture

#endif //#if !defined(__MFX_SCREEN_CAPTURE_DDI_H__)