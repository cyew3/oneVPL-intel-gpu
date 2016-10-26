//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#ifdef MFX_ENABLE_VP8_VIDEO_DECODE

#include "umc_defs.h"

#ifndef _MFX_VP8_DEC_DECODE_H_
#define _MFX_VP8_DEC_DECODE_H_

#include "mfx_common_int.h"
#include "umc_video_decoder.h"

#include "mfx_vp8_dec_decode_vp8_defs.h"
#include "mfx_umc_alloc_wrapper.h"

#include "umc_mutex.h"
#include <queue>
#include <list>

#include "mfx_task.h"
#include "mfxpcp.h"

#include "mfx_vp8_dec_decode_common.h"

using namespace UMC;

typedef struct _THREAD_TASK_INFO
{
    mfxFrameSurface1 *m_p_surface_work;
    mfxFrameSurface1 *m_p_surface_out;
    
    vm_mutex  *m_p_mutex;
    mfxU8     *m_p_bitstream;

    mfx_UMC_FrameAllocator *m_p_mfx_umc_frame_allocator;

    VP8DecodeCommon::IVF_FRAME  m_frame;
    UMC::FrameMemID m_memId;

    mfxVideoParamWrapper m_video_params;

    UMC::VideoData *m_p_video_data;

} THREAD_TASK_INFO;

class VideoDECODE;
class VideoDECODEVP8: public VideoDECODE
{
    public:

        VideoDECODEVP8(VideoCORE *pCore, mfxStatus * sts);
        virtual ~VideoDECODEVP8(void);

        static mfxStatus Query(VideoCORE *pCore, mfxVideoParam *pIn, mfxVideoParam *pOut);
        static mfxStatus QueryIOSurf(VideoCORE *pCore, mfxVideoParam *pPar, mfxFrameAllocRequest *pRequest);

        mfxStatus Init(mfxVideoParam *pPar);
        virtual mfxStatus Reset(mfxVideoParam *pPar);
        virtual mfxStatus Close(void);
        virtual mfxTaskThreadingPolicy GetThreadingPolicy(void);

        virtual mfxStatus GetVideoParam(mfxVideoParam *pPar);
        virtual mfxStatus GetDecodeStat(mfxDecodeStat *pStat);
        virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out);
        virtual mfxStatus DecodeFrameCheck(mfxBitstream *pBs, mfxFrameSurface1 *pSurfaceWork, mfxFrameSurface1 **ppSurfaceOut, MFX_ENTRY_POINT *pEntryPoint);
        virtual mfxStatus DecodeFrame(mfxBitstream *pBs, mfxFrameSurface1 *pSurfaceWork, mfxFrameSurface1 *pSurfaceOut);
        virtual mfxStatus GetUserData(mfxU8 *pUserData, mfxU32 *pSize, mfxU64 *pTimeStamp);
        virtual mfxStatus GetPayload(mfxU64 *pTimeStamp, mfxPayload *pPayload);
        virtual mfxStatus SetSkipMode(mfxSkipMode mode);

        mfxStatus RunThread(void *pParams, mfxU32 threadNumber);

    protected:

        mfxFrameSurface1 *GetOriginalSurface(mfxFrameSurface1 *p_surface);
        mfxStatus GetOutputSurface(mfxFrameSurface1 **pp_surface_out, mfxFrameSurface1 *p_surface_work, UMC::FrameMemID index);
        void SetOutputParams(mfxFrameSurface1 *p_surface_work);

        static mfxStatus QueryIOSurfInternal(eMFXPlatform platform, mfxVideoParam *pPar, mfxFrameAllocRequest *pRequest);
        mfxStatus ConstructFrame(mfxBitstream *in, mfxBitstream *out, VP8DecodeCommon::IVF_FRAME & frame);

        mfxStatus PreDecodeFrame(mfxBitstream *p_bs, mfxFrameSurface1 *p_surface);

        bool IsSameVideoParam(mfxVideoParam *pNewPar, mfxVideoParam *pOldPar);

        mfx_UMC_MemAllocator m_memoryAllocator;

        std::auto_ptr<mfx_UMC_FrameAllocator> m_p_frame_allocator;

        mfxVideoParamWrapper m_on_init_video_params;
        mfxVideoParamWrapper m_video_params;

        VideoCORE *m_p_core;

        bool m_is_initialized;
        bool m_is_image_available;
        bool m_is_opaque_memory;

        mfxU32 m_curr_decode_index;
        mfxU32 m_curr_display_index;
        mfxU32 m_num_output_frames;

        mfxF64 m_in_framerate;

        vm_mutex  m_mutex;

        mfxFrameAllocResponse m_response;
        mfxFrameAllocResponse m_opaque_response;

        mfxFrameAllocRequest m_request;

        mfxDecodeStat m_decode_stat;
        eMFXPlatform m_platform;

        UMC::FrameMemID m_mid[4];

        UMC::Mutex m_mGuard;

        mfxBitstream m_bs;

        UMC::VideoAccelerator *m_p_video_accelerator;

        mfxHDL m_vpx_codec; // vpx_codec_ctx_t
        
        mfxU32 m_init_w;
        mfxU32 m_init_h;
};

#endif // _MFX_VP8_DEC_DECODE_H_
#endif // MFX_ENABLE_VP8_VIDEO_DECODE
