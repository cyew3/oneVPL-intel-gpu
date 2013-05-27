//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010 Intel Corporation. All Rights Reserved.
//

#include "test_usage_models_allocator.h"
#include "test_usage_models_surfaces_manager.h"

mfxStatus TUMSurfacesManager::Init(mfxFrameAllocRequest&  request, MFXFrameAllocator* pAllocator)
{  
    mfxStatus sts;

    MSDK_CHECK_POINTER(pAllocator, MFX_ERR_NULL_PTR);    
    m_pAllocator = pAllocator;

    sts = (m_pAllocator)->Alloc( (m_pAllocator)->pthis, &request, &m_response );
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    int surfIndx, surfCount = m_response.NumFrameActual;

    for (surfIndx = 0; surfIndx < surfCount; surfIndx++)
    {
        mfxFrameSurface1 surface;
        MSDK_ZERO_MEMORY(surface);
        surface.Info = request.Info;
        
        if( request.Type & MFX_MEMTYPE_SYSTEM_MEMORY )
        {
            sts = (m_pAllocator)->Lock( (m_pAllocator)->pthis, m_response.mids[surfIndx], &(surface.Data) );
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
        else
        {
            surface.Data.MemId = m_response.mids[surfIndx];
        }
        
        m_surfacePool.push_back(surface);
    }

    m_bInited = true;

    return sts;

} // mfxStatus TUMSurfacesManager::Init(mfxFrameAllocRequest&  request)


mfxStatus TUMSurfacesManager::Close( void )
{
    if( m_bInited )
    {
        if ( m_pAllocator )
        {
            int surfIndx, surfCount = m_response.NumFrameActual;

            for (surfIndx = 0; surfIndx < surfCount; surfIndx++)
            {                   
                m_surfacePool.pop_back();
            }

            (m_pAllocator)->Free( (m_pAllocator)->pthis, &m_response );

            m_pAllocator = NULL;
        }

        m_bInited = false;
    }

    return MFX_ERR_NONE;

} // mfxStatus TUMSurfacesManager::Close( void )


TUMSurfacesManager::~TUMSurfacesManager( void )
{
    if ( m_bInited )
    {
        Close();
    }    

} // TUMSurfacesManager::~TUMSurfacesManager( void )


mfxFrameSurface1* TUMSurfacesManager::GetFreeSurface( void )
{
    int surfIndx, surfCount = (int)m_surfacePool.size();

    for(surfIndx = 0; surfIndx < surfCount; surfIndx++)
    {
        if ( 0 == m_surfacePool[surfIndx].Data.Locked )
        {
            return &(m_surfacePool[surfIndx]);
        }
    }

    return NULL;    

} // mfxFrameSurface1* TUMSurfacesManager::GetFreeSurface( void )

/* EOF */
