//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2017 Intel Corporation. All Rights Reserved.
//

#include <mfxcommon.h>
#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#include "umc_defs.h"

#ifndef _MFX_H264_DEC_DECODE_H_
#define _MFX_H264_DEC_DECODE_H_

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
    class MFXTaskSupplier;
    class MFX_SW_TaskSupplier;
    class VATaskSupplier;
    class H264DecoderFrame;
    class VideoData;
} // namespace UMC

#if defined (MFX_VA)
typedef UMC::VATaskSupplier  MFX_AVC_Decoder;
#else
typedef UMC::MFX_SW_TaskSupplier MFX_AVC_Decoder;
#endif

class VideoDECODE;
class VideoDECODEH264 : public VideoDECODE
{
public:
    static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);
    static mfxStatus DecodeHeader(VideoCORE *core, mfxBitstream *bs, mfxVideoParam *par);

    VideoDECODEH264(VideoCORE *core, mfxStatus * sts);
    virtual ~VideoDECODEH264(void);

    mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);
    virtual mfxTaskThreadingPolicy GetThreadingPolicy(void);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetDecodeStat(mfxDecodeStat *stat);

    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, MFX_ENTRY_POINT *pEntryPoint);
    virtual mfxStatus DecodeFrame(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_out);
    virtual mfxStatus GetUserData(mfxU8 *ud, mfxU32 *sz, mfxU64 *ts);
    virtual mfxStatus GetPayload(mfxU64 *ts, mfxPayload *payload);
    virtual mfxStatus SetSkipMode(mfxSkipMode mode);

    mfxStatus RunThread(void * params, mfxU32 threadNumber);

protected:
    static mfxStatus QueryIOSurfInternal(eMFXPlatform platform, eMFXHWType type, mfxVideoParam *par, mfxFrameAllocRequest *request);

    bool IsSameVideoParam(mfxVideoParam * newPar, mfxVideoParam * oldPar, eMFXHWType type);

    void FillOutputSurface(mfxFrameSurface1 **surface_out, mfxFrameSurface1 *surface_work, UMC::H264DecoderFrame * pFrame);
    UMC::H264DecoderFrame * GetFrameToDisplay(bool force);

    mfxStatus DecodeFrame(mfxFrameSurface1 *surface_out, UMC::H264DecoderFrame * pFrame = 0);

    mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out);

    void CopySurfaceInfo(mfxFrameSurface1 *in, mfxFrameSurface1 *out);

    mfxStatus SetTargetViewList(mfxVideoParam *par);

    mfxU16 GetChangedProfile(mfxVideoParam *par);

    void FillVideoParam(mfxVideoParamWrapper *par, bool full);

    mfxStatus UpdateAllocRequest(mfxVideoParam *par, 
                                mfxFrameAllocRequest *request,
                                mfxExtOpaqueSurfaceAlloc * &pOpaqAlloc,
                                bool &mapping);

    mfxFrameSurface1 * GetOriginalSurface(mfxFrameSurface1 *surface);

    std::unique_ptr<UMC::MFXTaskSupplier>  m_pH264VideoDecoder;
    mfx_UMC_MemAllocator            m_MemoryAllocator;

    std::unique_ptr<mfx_UMC_FrameAllocator>    m_FrameAllocator;

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

#endif // _MFX_H264_DEC_DECODE_H_
#endif
