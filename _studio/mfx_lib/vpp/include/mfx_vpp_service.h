//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2011 Intel Corporation. All Rights Reserved.
//

/* ****************************************************************************** */

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#ifndef __MFX_VPP_SERVICE_H
#define __MFX_VPP_SERVICE_H

#include "mfxvideo++int.h"
#include "mfx_vpp_defs.h"

class DynamicFramesPool {

public:
    DynamicFramesPool( void );
    virtual ~DynamicFramesPool();

    mfxStatus Init( VideoCORE *core, mfxFrameAllocRequest* pRequest, bool needAlloc);
    mfxStatus Close( void );
    mfxStatus Reset( void );
    mfxStatus Zero( void );
    mfxStatus Lock( void );
    mfxStatus Unlock( void );
    mfxStatus GetFreeSurface(mfxFrameSurface1** ppSurface);

    mfxStatus SetCrop( mfxFrameInfo* pInfo );

    mfxStatus PreProcessSync( void );
    mfxStatus PostProcessSync( void );
    mfxStatus GetFilledSurface(mfxFrameSurface1*  in, mfxFrameSurface1** ppSurface);

protected:
    mfxU16            m_numFrames;
    mfxFrameSurface1* m_pSurface;
    mfxMemId*         m_mids;
    VideoCORE*        m_core;
    mfxFrameData**    m_ptrDataTab;
    mfxU16*           m_savedLocked;
    bool*             m_bLockedOutput;
    bool              m_allocated;

private:
    // Declare private copy constructor to avoid accidental assignment
    // and klocwork complaining.
    DynamicFramesPool(const DynamicFramesPool &);
    DynamicFramesPool & operator = (const DynamicFramesPool &);
};

#endif // __MFX_VPP_SERVICE_H

#endif // MFX_ENABLE_VPP
/*EOF*/
