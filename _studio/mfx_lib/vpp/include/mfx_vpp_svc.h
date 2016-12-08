//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

/* ****************************************************************************** */

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP) && defined (MFX_ENABLE_VPP_SVC)

#ifndef __MFX_VPP_SVC_H
#define __MFX_VPP_SVC_H

#include <memory>
#include <map>

#include "mfxvideo++int.h"
#include "mfx_task.h"
#include "mfx_vpp_defs.h"
#include "mfx_vpp_base.h"
#include "mfx_vpp_sw.h"

using namespace std;

namespace MfxVideoProcessing
{
    class ImplementationSvc : public VideoVPP 
    {
    public:

        static mfxStatus Query(VideoCORE* core, mfxVideoParam *in, mfxVideoParam *out);
        static mfxStatus QueryIOSurf(VideoCORE* core, mfxVideoParam *par, mfxFrameAllocRequest *request);

        ImplementationSvc(VideoCORE *core);

        virtual ~ImplementationSvc();

        // VideoVPP
        virtual mfxStatus RunFrameVPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux);

        // VideoBase methods
        virtual mfxStatus Reset(mfxVideoParam *par);
        virtual mfxStatus Close(void);
        virtual mfxStatus Init(mfxVideoParam *par);

        virtual mfxStatus GetVideoParam(
            mfxVideoParam *par);

        virtual mfxStatus GetVPPStat(
            mfxVPPStat *stat);

        virtual mfxStatus VppFrameCheck(
            mfxFrameSurface1 *in, 
            mfxFrameSurface1 *out, 
            mfxExtVppAuxData *aux,
            MFX_ENTRY_POINT pEntryPoint[], 
            mfxU32 &numEntryPoints);

        virtual mfxStatus VppFrameCheck(
            mfxFrameSurface1 *, 
            mfxFrameSurface1 *)
        {
            return MFX_ERR_UNSUPPORTED;
        }

        virtual mfxTaskThreadingPolicy GetThreadingPolicy(void);

        // multi threading of SW_VPP functions
        mfxStatus RunVPPTask(
            mfxFrameSurface1 *in, 
            mfxFrameSurface1 *out, 
            FilterVPP::InternalParam *pParam );

        mfxStatus ResetTaskCounters();


    private:

        void GetLayerParam(
            mfxVideoParam *par,
            mfxVideoParam & layerParam,
            mfxU32 did);

        bool                      m_bInit;
        VideoCORE*                m_core;
        mfxExtSVCSeqDesc          m_svcDesc;
        std::auto_ptr<VideoVPPBase> m_VPP[8];

        std::vector<mfxU32>       m_declaredDidList;
        std::vector<mfxU32>       m_recievedDidList;
        mfxFrameSurface1*         m_inputSurface;

    };

}; // namespace MfxVideoProcessing

#endif // __MFX_VPP_SVC_H
#endif // defined (MFX_ENABLE_VPP) && defined (MFX_ENABLE_VPP_SVC)
/* EOF */
