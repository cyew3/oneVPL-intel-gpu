//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2020 Intel Corporation. All Rights Reserved.
//

#pragma once
#pragma warning(disable : 4201)

#include <map>
#include <memory>

#include "base_allocator.h"
#include "d3d_allocator.h"
#include "vaapi_allocator.h"
#include "vaapi_utils.h"
#include "sysmem_allocator.h"

#include "mfxvideo.h"
#include "mfxvideo++.h"

#if defined(LIBVA_SUPPORT)
#include <va/va.h>
#endif
#if defined(LIBVA_DRM_SUPPORT)
#include <va/va_drm.h>
#endif
#if defined(LIBVA_X11_SUPPORT)
#include <va/va_x11.h>
#endif


struct sMemoryAllocator
{
  MFXFrameAllocator*  pMfxAllocator;
  mfxAllocatorParams* pAllocatorParams;
  bool                bUsedAsExternalAllocator;

  mfxFrameSurface1*     pSurfaces[2];
  mfxFrameAllocResponse response[2];

#if defined(_WIN32) || defined(_WIN64)
  IDirect3DDeviceManager9* pd3dDeviceManager;
#endif
#if defined(LIBVA_SUPPORT)
  CLibVA * pLibVA;
  mfxHDL va_display;
#endif

  sMemoryAllocator()
      : pMfxAllocator(NULL)
      , pAllocatorParams(NULL)
      , bUsedAsExternalAllocator(false)
#if defined(_WIN32) || defined(_WIN64)
      , pd3dDeviceManager(NULL)
#endif
#if defined(LIBVA_SUPPORT)
      , pLibVA(0)
      , va_display(0)
#endif

  {};

};

class GeneralAllocator : public BaseFrameAllocator
{
public:
    GeneralAllocator();
    virtual ~GeneralAllocator();    

    virtual mfxStatus Init(mfxAllocatorParams *pParams);
    virtual mfxStatus Close();

protected:
    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle);       

    virtual mfxStatus ReleaseResponse(mfxFrameAllocResponse *response);
    virtual mfxStatus AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    virtual mfxStatus ReallocImpl(mfxMemId midIn, const mfxFrameInfo *info, mfxU16 memType, mfxMemId *midOut);

    void    StoreFrameMids(bool isHwFrames, mfxFrameAllocResponse *response);
    bool    isHwMid(mfxHDL mid);

    std::map<mfxHDL, bool>              m_Mids;

    std::auto_ptr<BaseFrameAllocator>   m_HwAllocator;

    std::auto_ptr<SysMemFrameAllocator> m_SYSAllocator;
	
};

//mfxStatus CreateDeviceManager(IDirect3DDeviceManager9** ppManager);
mfxStatus CreateEnvironmentHw( sMemoryAllocator* pAllocator );
/* EOF */
