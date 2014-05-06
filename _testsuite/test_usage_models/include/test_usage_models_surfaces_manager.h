//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010 Intel Corporation. All Rights Reserved.
//

#pragma once

#include <vector>
#include "base_allocator.h"
#include "test_usage_models_utils.h"
#include "test_usage_models_allocator.h"

struct mfxFrameSurfaceEx
{
    mfxFrameSurface1 *pSurface;
    mfxSyncPoint      syncp;
};


class TUMSurfacesManager
{
public:
    TUMSurfacesManager( void ): m_pAllocator(NULL), m_bInited(false) {};
    ~TUMSurfacesManager( void );

    mfxStatus Init( mfxFrameAllocRequest&  request, MFXFrameAllocator* pAllocator );
    mfxStatus Close( void );
    
    mfxFrameSurface1* GetFreeSurface( void );    

private: 
    
    MFXFrameAllocator*    m_pAllocator;

    mfxFrameAllocResponse m_response;

    std::vector<mfxFrameSurface1> m_surfacePool;

    bool m_bInited;    
    
};
/* EOF */
