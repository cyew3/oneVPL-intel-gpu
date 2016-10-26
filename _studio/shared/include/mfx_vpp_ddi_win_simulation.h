//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_VPP_DDI_WIN_SIMULATION_H
#define __MFX_VPP_DDI_WIN_SIMULATION_H

#include "mfx_common.h"

#if defined (MFX_VA_WIN)

//#include "mfxdefs.h"
#include "umc_va_base.h"
#include "mfx_vpp_driver.h"
#include "mfx_platform_headers.h"
#include <set>

namespace MfxHwVideoProcessing
{
    class WinSimulationDdiVpp : public DriverVideoProcessing
    {
    public:

        WinSimulationDdiVpp();
        
        virtual ~WinSimulationDdiVpp();

        virtual mfxStatus CreateDevice(VideoCORE * core);

        virtual mfxStatus DestroyDevice( void );

        virtual mfxStatus Register(_mfxPlatformVideoSurface *pSurfaces, 
                                   mfxU32 num, 
                                   BOOL bRegister);

        virtual mfxStatus QueryTaskStatus(PREPROC_QUERY_STATUS *pQueryStatus, mfxU32 numStructures);
        
        virtual mfxStatus QueryCapabilities( mfxVppCaps& caps );

        virtual BOOL IsRunning() { return m_bRunning; }

        virtual mfxStatus Execute(FASTCOMP_BLT_PARAMS *pBltParams);

    private:

        BOOL m_bRunning;

        VideoCORE* m_core;

        std::vector<mfxU32> m_cachedReadyTaskIndex;

        UMC::Mutex m_guard;
        
        _mfxPlatformAccelerationService m_vaDisplay;

        
        mfxStatus Init( _mfxPlatformAccelerationService* pVADisplay );

        mfxStatus Close( void );
    };

}; // namespace

#endif // #if defined (MFX_VA_WIN)
#endif // #define __MFX_VPP_DDI_WIN_SIMULATION_H
/* EOF */
