//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)
#if defined (MFX_VA_LINUX)

#ifndef __MFX_VPP_VAAPI
#define __MFX_VPP_VAAPI

#include "umc_va_base.h"
#include "mfx_vpp_interface.h"
#include "mfx_platform_headers.h"

#include <va/va.h>
#include <va/va_vpp.h>

#include <assert.h>
#include <set>

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) {assert(EXPR); MFX_CHECK(EXPR,ERR); }

namespace MfxHwVideoProcessing
{
    class VAAPIVideoProcessing : public DriverVideoProcessing
    {
    public:

        VAAPIVideoProcessing();
        
        virtual ~VAAPIVideoProcessing();

        virtual mfxStatus CreateDevice(VideoCORE * core, mfxVideoParam *pParams, bool isTemporal = false);

        virtual mfxStatus ReconfigDevice(mfxU32 /*indx*/) { return MFX_ERR_NONE; }

        virtual mfxStatus DestroyDevice( void );

        virtual mfxStatus Register(mfxHDLPair* pSurfaces, 
                                   mfxU32 num, 
                                   BOOL bRegister);

        virtual mfxStatus QueryTaskStatus(mfxU32 taskIndex);

        virtual mfxStatus QueryCapabilities( mfxVppCaps& caps );

        virtual mfxStatus QueryVariance(
            mfxU32 frameIndex,
            std::vector<mfxU32> &variance) { return MFX_ERR_UNSUPPORTED; }

        virtual BOOL IsRunning() { return m_bRunning; }

        virtual mfxStatus Execute(mfxExecuteParams *pParams);

        virtual mfxStatus Execute_Composition(mfxExecuteParams *pParams);

        virtual mfxStatus Execute_Composition_TiledVideoWall(mfxExecuteParams *pParams);

    private:

        // VideoSignalInfo contains infor for backward reference, current and forward reference
        // frames. Indices in the info array are:
        //    { <indices for bwd refs>, <current>, <indices for fwd refs> }
        // For example, in case of ADI we will have: 1 bwd ref, 1 current, 0 fwd ref; indices will be:
        //    VideoSignalInfo[0] - for bwd ref
        //    VideoSignalInfo[1] - for cur reference
        inline int GetCurFrameSignalIdx(mfxExecuteParams* pParams)
        {
            if (!pParams) return -1;
            if ((size_t)pParams->bkwdRefCount < pParams->VideoSignalInfo.size()) {
                return pParams->bkwdRefCount;
            }
            return -1;
        }

        typedef struct _compositionStreamElement
        {
            mfxU16 index;
            BOOL   active;
            mfxU16 x;
            mfxU16 y;
            _compositionStreamElement()
                : index(0)
                , active(false) 
                , x(0)
                , y(0)
            {};
        } compStreamElem;

        typedef struct vppTileVW{
            mfxU32 numChannels;
            VARectangle targerRect;
            mfxU8 channelIds[8];
        } m_tiledVideoWallParams;

        BOOL    isVideoWall(mfxExecuteParams *pParams);

        BOOL m_bRunning;

        VideoCORE* m_core;

        VADisplay   m_vaDisplay;
        VAConfigID  m_vaConfig;
        VAContextID m_vaContextVPP;

        VAProcFilterCap m_denoiseCaps;
        VAProcFilterCap m_detailCaps;

        VAProcPipelineCaps           m_pipelineCaps;
        VAProcFilterCapColorBalance  m_procampCaps[VAProcColorBalanceCount];
        VAProcFilterCapDeinterlacing m_deinterlacingCaps[VAProcDeinterlacingCount];
#ifdef MFX_ENABLE_VPP_FRC
        VAProcFilterCapFrameRateConversion m_frcCaps[2]; /* only two modes, 24p->60p and 30p->60p */
#endif

        VABufferID m_denoiseFilterID;
        VABufferID m_detailFilterID;
        VABufferID m_deintFilterID;
        VABufferID m_procampFilterID;
        VABufferID m_frcFilterID;
        mfxU32     m_deintFrameCount;
        BOOL       m_bFakeOutputEnabled;
        VASurfaceID m_refForFRC[5];
        mfxU32 m_frcCyclicCounter;

        VABufferID m_filterBufs[VAProcFilterCount];
        mfxU32 m_numFilterBufs;

        std::vector<VAProcPipelineParameterBuffer> m_pipelineParam;
        std::vector<VABufferID> m_pipelineParamID;

        std::set<mfxU32> m_cachedReadyTaskIndex;

        typedef struct 
        {
            VASurfaceID surface;
            mfxU32 number;
        } ExtVASurface;

        VASurfaceID* m_primarySurface4Composition ;

        std::vector<ExtVASurface> m_feedbackCache; 

        UMC::Mutex m_guard;

        mfxStatus Init( _mfxPlatformAccelerationService* pVADisplay, mfxVideoParam *pParams);

        mfxStatus Close( void );

        mfxStatus RemoveBufferFromPipe(VABufferID);
    };

}; // namespace

#endif //__MFX_VPP_VAAPI
#endif // MFX_VA_LINUX
#endif // MFX_ENABLE_VPP

/* EOF */
