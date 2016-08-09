/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.
//
//
//          HW MPEG2  encoder
//
*/

#ifndef __MFX_MPEG2_ENCODE_HW_HYBRID_H__
#define __MFX_MPEG2_ENCODE_HW_HYBRID_H__

#include "mfx_common.h"


#if defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE) && defined (MFX_ENABLE_MPEG2_VIDEO_PAK)

#include "mfxvideo++int.h"
#include "mfxvideopro++.h"
#include "mfx_mpeg2_encode_utils_hw.h"
#include "umc_mpeg2_enc.h"
#include "mfx_mpeg2_enc_hw.h"
#include "mfx_mpeg2_pak.h"



#ifdef MPEG2_ENCODE_HW_PERF
#include "vm_time.h"
#endif

#ifdef MPEG2_ENCODE_DEBUG_HW
#include "mfx_mpeg2_encode_debug_hw.h"
#endif // MPEG2_ENCODE_DEBUG_HW

#ifdef __SW_ENC
#define MFXVideoENCMPEG2_HW MFXVideoENCMPEG2_HW
#else
#define MFXVideoENCMPEG2_HW MFXVideoENCMPEG2_HW_DDI
#endif

#define ME_REF_ORIGINAL

namespace MPEG2EncoderHW
{
  class HybridEncode: public EncoderBase
    {
    public:

        static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out);
        static mfxStatus QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);

        HybridEncode(VideoCORE *core, mfxStatus *sts);
        virtual ~HybridEncode() {Close();}

        virtual mfxStatus Init(mfxVideoParam *par);
        virtual mfxStatus Reset(mfxVideoParam *par);
        virtual mfxStatus Close(void);

        virtual mfxStatus GetVideoParam(mfxVideoParam *par);
        virtual mfxStatus GetFrameParam(mfxFrameParam *par);
        virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat);

        virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, 
            mfxFrameSurface1 *surface, 
            mfxBitstream *bs, 
            mfxFrameSurface1 **reordered_surface, 
            mfxEncodeInternalParams *pInternalParams);

        virtual mfxStatus EncodeFrame(mfxEncodeCtrl *ctrl, 
            mfxEncodeInternalParams *pInternalParams, 
            mfxFrameSurface1 *surface, mfxBitstream *bs);

        virtual mfxStatus CancelFrame(mfxEncodeCtrl *ctrl, 
            mfxEncodeInternalParams *pInternalParams, 
            mfxFrameSurface1 *surface, 
            mfxBitstream *bs);

        virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl,
            mfxFrameSurface1 *surface,
            mfxBitstream *bs,
            mfxFrameSurface1 **reordered_surface,
            mfxEncodeInternalParams *pInternalParams,
            MFX_ENTRY_POINT *pEntryPoint);

        virtual   mfxTaskThreadingPolicy GetThreadingPolicy(void)
        {
            return MFX_TASK_THREADING_INTRA;
        }


        MFXVideoPAKMPEG2*          m_pPAK;
        clExtTasks1*          m_pExtTasks;


    protected:
        inline bool is_initialized() {return m_pController!=0;}
        mfxStatus ResetImpl();

    private:
        VideoCORE *                      m_pCore;
        ControllerBase*                  m_pController;
        MFXVideoENCMPEG2_HW*             m_pENC;
        MPEG2BRC_HW*                     m_pBRC;


        FrameStoreHybrid*                m_pFrameStore;
        EncodeFrameTaskHybrid*           m_pFrameTasks;
        mfxU32                           m_nFrameTasks;
        mfxU32                           m_nCurrTask;


#ifdef MPEG2_ENCODE_DEBUG_HW
        friend class MPEG2EncodeDebug_HW;
    public: MPEG2EncodeDebug_HW *mpeg2_debug_ENCODE_HW;
#endif //MPEG2_ENCODE_DEBUG_HW

#ifdef MPEG2_ENCODE_HW_PERF
            vm_time common_time;
            vm_time enc_time;
            vm_time pak_time;
            vm_time prepare_input_frames_time;
            vm_time prepare_ref_frames_time;
            int     num_input_copy;
            int     num_ref_copy;

#endif 

    };
}

#endif // MFX_ENABLE_MPEG2_VIDEO_ENCODE && MFX_ENABLE_MPEG2_VIDEO_PAK
#endif
