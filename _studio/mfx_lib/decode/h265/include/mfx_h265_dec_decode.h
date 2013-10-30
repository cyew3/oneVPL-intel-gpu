/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2013 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"
#if defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "umc_defs.h"

#ifndef _MFX_H265_DEC_DECODE_H_
#define _MFX_H265_DEC_DECODE_H_

#include "mfx_common_int.h"
#include "umc_video_decoder.h"
#include "mfx_umc_alloc_wrapper.h"

#include "umc_mutex.h"
#include <queue>
#include <list>
#include <memory>

#include "mfx_task.h"
#include "mfxpcp.h"

namespace UMC
{
    class VideoData;
}

namespace UMC_HEVC_DECODER
{
    class MFXTaskSupplier_H265;
    class VATaskSupplier;
    class MFXTaskSupplier_H265;
    class H265DecoderFrame;
}

using namespace UMC_HEVC_DECODER;

#if defined (MFX_VA)
typedef VATaskSupplier  MFX_AVC_Decoder_H265;
#else
typedef MFXTaskSupplier_H265 MFX_AVC_Decoder_H265;
#endif

class VideoDECODE;
class VideoDECODEH265 : public VideoDECODE
{
public:
    static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);
    static mfxStatus DecodeHeader(VideoCORE *core, mfxBitstream *bs, mfxVideoParam *par);

    VideoDECODEH265(VideoCORE *core, mfxStatus * sts);
    virtual ~VideoDECODEH265(void);

    mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);
    virtual mfxTaskThreadingPolicy GetThreadingPolicy(void);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetDecodeStat(mfxDecodeStat *stat);
    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out);
    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, MFX_ENTRY_POINT *pEntryPoint);
    virtual mfxStatus DecodeFrame(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_out);
    virtual mfxStatus GetUserData(mfxU8 *ud, mfxU32 *sz, mfxU64 *ts);
    virtual mfxStatus GetPayload(mfxU64 *ts, mfxPayload *payload);
    virtual mfxStatus SetSkipMode(mfxSkipMode mode);

    mfxStatus RunThread(void * params, mfxU32 threadNumber);

protected:
    static mfxStatus QueryIOSurfInternal(eMFXPlatform platform, eMFXHWType type, mfxVideoParam *par, mfxFrameAllocRequest *request, VideoCORE *core);

    bool IsSameVideoParam(mfxVideoParam * newPar, mfxVideoParam * oldPar, eMFXHWType type);

    void FillOutputSurface(mfxFrameSurface1 **surface_out, mfxFrameSurface1 *surface_work, H265DecoderFrame * pFrame);
    H265DecoderFrame * GetFrameToDisplay_H265(UMC::VideoData * dst, bool force);

    mfxStatus DecodeFrame(mfxFrameSurface1 *surface_out, H265DecoderFrame * pFrame = 0);

    void CopySurfaceInfo(mfxFrameSurface1 *in, mfxFrameSurface1 *out);

    void FillVideoParam(mfxVideoParamWrapper *par, bool full);

    mfxStatus UpdateAllocRequest(mfxVideoParam *par,
                                mfxFrameAllocRequest *request,
                                mfxExtOpaqueSurfaceAlloc * &pOpaqAlloc,
                                bool &mapping);

    mfxFrameSurface1 * GetOriginalSurface(mfxFrameSurface1 *surface);

    std::auto_ptr<MFXTaskSupplier_H265>  m_pH265VideoDecoder;
    mfx_UMC_MemAllocator            m_MemoryAllocator;

    std::auto_ptr<mfx_UMC_FrameAllocator>    m_FrameAllocator;

    mfxVideoParamWrapper m_vInitPar;
    mfxVideoParamWrapper m_vFirstPar;
    mfxVideoParamWrapper m_vPar;
    ExtendedBuffer m_extBuffers;

    VideoCORE * m_core;

    bool    m_isInit;
    bool    m_isOpaq;

    mfxU16  m_frameOrder;

    mfxFrameAllocResponse m_response;
    mfxFrameAllocResponse m_response_alien;
    mfxDecodeStat m_stat;
    eMFXPlatform m_platform;

    UMC::Mutex m_mGuard;
    bool m_useDelayedDisplay;

    UMC::VideoAccelerator * m_va;
    enum
    {
        NUMBER_OF_ADDITIONAL_FRAMES = 10
    };

    volatile bool m_globalTask;
    bool m_isFirstRun;
};

#endif // _MFX_H265_DEC_DECODE_H_
#endif
