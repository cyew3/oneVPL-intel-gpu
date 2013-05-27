/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2009 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA

#include <windows.h>
#include <dwmapi.h>
#include <mmsystem.h>
#include <tchar.h>
#include <strsafe.h>
#include <initguid.h>
#include <d3d9.h>
#include <dxva2api.h>

#include "mfxvideo.h"
#include "mfx_common.h"
#include "ippi.h"
#include "ippcc.h"

class VideoVPPHW : public VideoVPP
{
public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) { printf("MFXHW VPP Query"); return MFX_ERR_UNSUPPORTED; }
    static mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request) { printf("MFXHW VPP QueryIOSurf"); return MFX_ERR_UNSUPPORTED; }

    VideoVPPHW(VideoCORE *core, mfxStatus *res);
    virtual ~VideoVPPHW(void);

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par) { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus Close(void);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par) { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus GetVPPStat(mfxVPPStat *stat) { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus VppFrameCheck(mfxFrameSurface1 *in, mfxFrameSurface1 *out )  { return MFX_ERR_NONE; }
    virtual mfxStatus RunFrameVPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux);

protected:
    BOOL CreateDXVA2VPDevice(REFGUID guid, D3DFORMAT VIDEO_RENDER_TARGET_FORMAT);
    INT  ComputeLongSteps(DXVA2_ValueRange &range);
    RECT KeepAspectRatio(const RECT& dst, const RECT& src);
    BOOL CopySurface(mfxFrameSurface1 *in, IDirect3DSurface9 *out);

    //mfxVideoParam m_VPPParams;
    VideoCORE *m_pCore;

    HANDLE                          m_hDevice;
    IDirectXVideoProcessorService*  m_pDXVAVPS;
    IDirectXVideoProcessor*         m_pDXVAVPD;
    DXVA2_VideoDesc                 m_VideoDesc;
    IDirect3DSurface9               *m_pInputSurface;
    LONGLONG                        m_FrameDuration;

    // BLT params
    DXVA2_ValueRange    m_ProcAmpRanges[4];
    DXVA2_ValueRange    m_NFilterRanges[6];
    DXVA2_ValueRange    m_DFilterRanges[6];
};

VideoVPP* CreateVideoVPPHW(VideoCORE *core, mfxStatus *res);

#endif // UMC_VA_DXVA
