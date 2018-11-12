// Copyright (c) 2014-2018 Intel Corporation
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

#include "mfx_common.h"

#include "umc_defs.h"

#ifndef _MFX_VP9_DEC_DECODE_H_
#define _MFX_VP9_DEC_DECODE_H_

#if defined(MFX_ENABLE_VP9_VIDEO_DECODE) || defined(MFX_ENABLE_VP9_VIDEO_DECODE_HW)
#include "mfx_common_int.h"
#include "umc_video_decoder.h"

#include "mfx_umc_alloc_wrapper.h"
#include "mfx_vp9_dec_decode_utils.h"
#endif

#ifdef MFX_ENABLE_VP9_VIDEO_DECODE

#include "umc_mutex.h"
#include <queue>
#include <list>

#include "mfx_task.h"

using namespace MfxVP9Decode;

typedef struct
{
    mfxU16                           ShowFrame;
    mfxU16                           Width;
    mfxU16                           Height;
} VP9BaseFrameInfo;

typedef struct _IVF_FRAME_VP9
{
    uint32_t frame_size;
    unsigned long long time_stamp;
    uint8_t *p_frame_data;

} IVF_FRAME_VP9;

typedef struct _THREAD_TASK_INFO_VP9
{
    mfxFrameSurface1 *m_p_surface_work;
    mfxFrameSurface1 *m_p_surface_out;

    mfxU8     *m_p_bitstream;

    mfx_UMC_FrameAllocator *m_p_mfx_umc_frame_allocator;

    IVF_FRAME_VP9  m_frame;
    UMC::FrameMemID m_memId;

    mfxVideoParamWrapper m_video_params;

    UMC::VideoData *m_p_video_data;

} THREAD_TASK_INFO_VP9;

class VideoDECODE;
class VideoDECODEVP9: public VideoDECODE
{
    public:

        VideoDECODEVP9(VideoCORE *pCore, mfxStatus * sts);
        virtual ~VideoDECODEVP9(void);

        static mfxStatus Query(VideoCORE *pCore, mfxVideoParam *pIn, mfxVideoParam *pOut);
        static mfxStatus QueryIOSurf(VideoCORE *pCore, mfxVideoParam *pPar, mfxFrameAllocRequest *pRequest);
        static mfxStatus DecodeHeader(VideoCORE *pCore, mfxBitstream *pBs, mfxVideoParam *pPar);

        mfxStatus Init(mfxVideoParam *pPar);
        virtual mfxStatus Reset(mfxVideoParam *pPar);
        virtual mfxStatus Close(void);
        virtual mfxTaskThreadingPolicy GetThreadingPolicy(void);

        virtual mfxStatus GetVideoParam(mfxVideoParam *pPar);
        virtual mfxStatus GetDecodeStat(mfxDecodeStat *pStat);
        virtual mfxStatus DecodeFrameCheck(mfxBitstream *pBs, mfxFrameSurface1 *pSurfaceWork, mfxFrameSurface1 **ppSurfaceOut, MFX_ENTRY_POINT *pEntryPoint);
        virtual mfxStatus GetUserData(mfxU8 *pUserData, mfxU32 *pSize, mfxU64 *pTimeStamp);
        virtual mfxStatus GetPayload(mfxU64 *pTimeStamp, mfxPayload *pPayload);
        virtual mfxStatus SetSkipMode(mfxSkipMode mode);

        mfxStatus RunThread(void *pParams, mfxU32 threadNumber);

    protected:

        mfxFrameSurface1 *GetOriginalSurface(mfxFrameSurface1 *p_surface);
        mfxStatus GetOutputSurface(mfxFrameSurface1 **pp_surface_out, mfxFrameSurface1 *p_surface_work, UMC::FrameMemID index);
        void SetOutputParams(mfxFrameSurface1 *p_surface_work);

        static mfxStatus ReadFrameInfo(mfxU8 *pData, mfxU32 size, VP9BaseFrameInfo *out);

        static mfxStatus QueryIOSurfInternal(eMFXPlatform platform, mfxVideoParam *pPar, mfxFrameAllocRequest *pRequest);
        mfxStatus ConstructFrame(mfxBitstream *in, mfxBitstream *out, IVF_FRAME_VP9 & frame);

        bool IsSameVideoParam(mfxVideoParam *pNewPar, mfxVideoParam *pOldPar);

        std::auto_ptr<mfx_UMC_FrameAllocator> m_FrameAllocator;

        mfxVideoParamWrapper    m_vInitPar;

        VideoCORE *m_core;

        bool m_isInit;
        bool m_is_opaque_memory;

        mfxU32 m_num_output_frames;

        mfxF64 m_in_framerate;

        mfxFrameAllocResponse m_response;
        mfxFrameAllocResponse m_opaque_response;

        mfxFrameAllocRequest m_request;

        mfxDecodeStat m_stat;
        eMFXPlatform m_platform;
        VP9BaseFrameInfo m_frameInfo;

        UMC::Mutex m_mGuard;

        mfxBitstream m_bs;

        mfxHDL m_vpx_codec;
};

#endif // MFX_ENABLE_VP9_VIDEO_DECODE

#if defined(MFX_ENABLE_VP9_VIDEO_DECODE) || defined(MFX_ENABLE_VP9_VIDEO_DECODE_HW)
namespace MFX_VP9_Utility
{
    mfxStatus DecodeHeader(VideoCORE * /*core*/, mfxBitstream *bs, mfxVideoParam *params);
    mfxStatus FillVideoParam(VideoCORE* /*core*/, UMC_VP9_DECODER::VP9DecoderFrame const&, mfxVideoParam* params);
};

#endif // #if defined(MFX_ENABLE_VP9_VIDEO_DECODE) || defined(MFX_ENABLE_VP9_VIDEO_DECODE_HW)

#endif // _MFX_VP9_DEC_DECODE_H_

