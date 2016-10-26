//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)
#if defined (MFX_VA_LINUX)

#ifndef __MFX_VPP_VAAPI
#define __MFX_VPP_VAAPI

#include "umc_va_base.h"
#include "mfx_vpp_driver.h"
#include "mfx_platform_headers.h"
//#include <va/va.h>

namespace MfxHwVideoProcessing
{
    class VAAPIVideoProcessing : public DriverVideoProcessing
    {
    public:

        VAAPIVideoProcessing();
        
        virtual ~VAAPIVideoProcessing();

        virtual mfxStatus CreateDevice(VideoCORE * core);

        virtual mfxStatus DestroyDevice( void );

        virtual mfxStatus Register(mfxHDL *pSurfaces, 
                                   mfxU32 num, 
                                   BOOL bRegister);

        virtual mfxStatus QueryTaskStatus(PREPROC_QUERY_STATUS *pQueryStatus, mfxU32 numStructures);
        
        virtual mfxStatus QueryCapabilities( mfxVppCaps& caps );

        virtual BOOL IsRunning() { return m_bRunning; }

        virtual mfxStatus Execute(mfxExecuteParams *pParams);

    private:

        BOOL m_bRunning;

        VideoCORE* m_core;
        
        //VADisplay  m_vaDisplay;

        std::vector<mfxU32> m_cachedReadyTaskIndex;

        UMC::Mutex m_guard;
        _mfxPlatformAccelerationService m_vaDisplay;

        mfxStatus Init( _mfxPlatformAccelerationService* pVADisplay );

        mfxStatus Close( void );
        
        mfxStatus SimulationExecute( 
            VASurfaceID in, 
            VASurfaceID out,
            mfxFrameInfo srcRect,
            mfxFrameInfo dstRect);
    };
    
}; // namespace

#endif //__MFX_VPP_VAAPI
#endif // MFX_VA_LINUX
#endif // MFX_ENABLE_VPP

/* EOF */
