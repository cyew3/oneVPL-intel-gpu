// Copyright (c) 2011-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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
