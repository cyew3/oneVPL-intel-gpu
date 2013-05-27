//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010 Intel Corporation. All Rights Reserved.
//

#pragma once
#pragma warning(disable : 4201)

#include <map>
#include <d3d9.h>
#include <dxva2api.h>

#include "base_allocator.h"
#include "d3d_allocator.h"
#include "sysmem_allocator.h"

#include "mfxvideo.h"
#include "mfxvideo++.h"

struct sMemoryAllocator
{
  MFXFrameAllocator*  pMfxAllocator;
  mfxAllocatorParams* pAllocatorParams;
  bool                bUsedAsExternalAllocator;

  mfxFrameSurface1*     pSurfaces[2];
  mfxFrameAllocResponse response[2];

  IDirect3DDeviceManager9* pd3dDeviceManager;

  sMemoryAllocator()
      :pMfxAllocator(NULL)
      ,pAllocatorParams(NULL)
      ,bUsedAsExternalAllocator(false)
      ,pd3dDeviceManager(NULL)
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

    void    StoreFrameMids(bool isD3DFrames, mfxFrameAllocResponse *response);
    bool    isD3DMid(mfxHDL mid);

    std::map<mfxHDL, bool>              m_Mids;
    std::auto_ptr<D3DFrameAllocator>    m_D3DAllocator;
    std::auto_ptr<SysMemFrameAllocator> m_SYSAllocator;

};

//mfxStatus CreateDeviceManager(IDirect3DDeviceManager9** ppManager);
mfxStatus CreateEnvironmentD3D( sMemoryAllocator* pAllocator );
/* EOF */
