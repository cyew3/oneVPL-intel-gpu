/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2016 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP) 

#ifndef __FAST_COMPOSITING_DDI
#define __FAST_COMPOSITING_DDI

#if defined (MFX_VA_WIN)

#include <d3d9.h>
#include <dxva.h>

#pragma warning(push)
#pragma warning(disable: 4201)
#include <dxva2api.h>
#pragma warning(pop)

#include "mfxdefs.h"
#include "mfx_vpp_interface.h"
#include "mfx_platform_headers.h"

#include "auxiliary_device.h"
#include "libmfx_allocator.h"
#include <set>
#include <algorithm>

namespace MfxHwVideoProcessing
{

    class FastCompositingDDI : public DriverVideoProcessing
    {
    public:
        // default constructor
        FastCompositingDDI(
            FASTCOMP_MODE iMode);
        
        virtual ~FastCompositingDDI();

        virtual mfxStatus CreateDevice(
            VideoCORE * core, 
            mfxVideoParam* par,
            bool isTemporal = false);

        virtual mfxStatus ReconfigDevice(mfxU32 indx) { indx; return MFX_ERR_NONE; }

        virtual mfxStatus DestroyDevice( void );

        virtual mfxStatus Register(
            mfxHDLPair* pSurfaces, 
            mfxU32 num, 
            BOOL bRegister);

        virtual mfxStatus QueryTaskStatus(
            mfxU32 taskIndex);

        virtual mfxStatus QueryCapabilities( 
            mfxVppCaps& caps );

        virtual mfxStatus Execute(
            mfxExecuteParams *pParams);

        virtual mfxStatus QueryVariance(
            mfxU32 frameIndex,
            std::vector<UINT> &variance);

    private:
        mfxStatus ConvertExecute2BltParams( 
            mfxExecuteParams *pExecuteParams, 
            FASTCOMP_BLT_PARAMS *pBltParams );

        mfxStatus ExecuteBlt(
            FASTCOMP_BLT_PARAMS *pBltParams);

        // create d3d surface
        mfxStatus CreateSurface(
            IDirect3DSurface9 **surface, 
            mfxU32 width, 
            mfxU32 height, 
            D3DFORMAT format, 
            D3DPOOL pool);

        BOOL IsSystemSurfaceWasCreated(void) { return m_bIsSystemSurface; }

        BOOL IsPresent() { return m_bIsPresent; }

        // reset blt parameters
        void ResetBltParams(
            FASTCOMP_BLT_PARAMS *pBltParams);
        
        // query main capabilities
        mfxStatus QueryCaps(FASTCOMP_CAPS& caps);

        // query advanced capabilities
        mfxStatus QueryCaps2(FASTCOMP_CAPS2& Caps2);

        // query maximum recommended rate
        mfxStatus QueryFrcCaps(FASTCOMP_FRC_CAPS& frcCaps);
        mfxStatus QueryVarianceCaps(
            FASTCOMP_VARIANCE_CAPS * pVarianceCaps);
        
         mfxStatus QuerySceneDetection(
             mfxU32 frameIndex, 
             mfxU16 &sceneChangeRate);

        // initialize service
        mfxStatus Initialize(
            IDirect3DDeviceManager9 *pD3DDeviceManager = NULL,
            IDirectXVideoDecoderService *pVideoDecoderService = NULL);
       
        // release class object
        mfxStatus Release(void);

        mfxStatus FastCopy(
            IDirect3DSurface9 *pTarget, 
            IDirect3DSurface9 *pSource,
            RECT *pTargetRect, 
            RECT *pSourceRect, 
            BOOL bInterlaced);

        IDirect3DSurface9 *m_pSystemMemorySurface;
        BOOL m_bIsSystemSurface;

        // second level private data
        BOOL m_bIsPresent;

        FASTCOMP_MODE  m_iMode;
        FASTCOMP_CAPS  m_caps;
        FASTCOMP_CAPS2 m_caps2;
        FASTCOMP_FRC_CAPS m_frcCaps;
        FASTCOMP_VARIANCE_CAPS m_varianceCaps;

        std::map <mfxU32, mfxU32> m_formatSupport;

        std::vector<FASTCOMP_VideoSample> m_inputSamples;

        std::set<mfxU32> m_cachedReadyTaskIndex;

        IDirect3DSurface9 *m_pDummySurface;

        // pointer on device manager object
        IDirectXVideoDecoderService *m_pD3DDecoderService;

        // ddi object
        AuxiliaryDevice *m_pAuxDevice;

        struct D3D9Frc
        {
            mfxU32 m_inputIndx;
            mfxU32 m_outputIndx;

            mfxU32 m_inputFramesOrFieldsPerCycle;
            mfxU32 m_outputIndexCountPerCycle;

            bool   m_isInited;
        } m_frcState;
    };

}; // namespace

#endif // MFX_VA_WIN
#endif // __FAST_COMPOSITING_DDI
#endif // MFX_ENABLE_VPP
