/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2013 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"
#if defined (MFX_ENABLE_MJPEG_VIDEO_DECODE)

#include "umc_defs.h"

#ifndef _MFX_MJPEG_DEC_DECODE_H_
#define _MFX_MJPEG_DEC_DECODE_H_

#include "mfx_common_int.h"
#include "umc_video_decoder.h"
#include "mfx_umc_alloc_wrapper.h"

#include "umc_mutex.h"
#include <queue>
#include <list>

#include "mfx_task.h"
#include "mfxpcp.h"

#if defined (MFX_VA)
#include "mfx_vpp_jpeg_d3d9.h"
#endif

namespace UMC
{
    class MJPEGVideoDecoderMFX_HW;
    class MJPEGVideoDecoderMFX;
    class JpegFrameConstructor;
};

#if defined (MFX_VA)
typedef UMC::MJPEGVideoDecoderMFX_HW    MFX_JPEG_Decoder;
#else
typedef UMC::MJPEGVideoDecoderMFX       MFX_JPEG_Decoder;
#endif

// Forward declaration of used classes
class CJpegTask;

class VideoDECODEMJPEG : public VideoDECODE
{
public:
    static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);
    static mfxStatus DecodeHeader(VideoCORE *core, mfxBitstream *bs, mfxVideoParam *par);

    VideoDECODEMJPEG(VideoCORE *core, mfxStatus * sts);
    virtual ~VideoDECODEMJPEG(void);

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

protected:
    static mfxStatus QueryIOSurfInternal(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);

    bool IsSameVideoParam(mfxVideoParam * newPar, mfxVideoParam * oldPar);


    mfxStatus UpdateAllocRequest(mfxVideoParam *par, 
                                mfxFrameAllocRequest *request,
                                mfxExtOpaqueSurfaceAlloc * &pOpaqAlloc,
                                bool &mapping);

    mfxFrameSurface1 * GetOriginalSurface(mfxFrameSurface1 *surface);

    mfx_UMC_MemAllocator            m_MemoryAllocator;

    std::auto_ptr<mfx_UMC_FrameAllocator>    m_FrameAllocator;

#if defined (MFX_VA)
    // Decoder's array
    std::auto_ptr<UMC::MJPEGVideoDecoderMFX_HW> m_pMJPEGVideoDecoder;
    // Output frame
    UMC::FrameData *m_dst;
    // Array of all currently using frames
    std::vector<UMC::FrameData*> m_dsts;
    // Number of pictures collected
    mfxU32 m_numPic;
    // True if we need special VPP color conversion after decoding
    bool   m_needVpp;
#endif

    // Frames collecting unit
    std::auto_ptr<UMC::JpegFrameConstructor> m_frameConstructor;
    // Free tasks queue guard (if SW is used)
    UMC::Mutex m_guard;
    // Free tasks queue (if SW is used)
    std::queue<CJpegTask *> m_freeTasks;
    // Count of created tasks (if SW is used)
    mfxU16  m_tasksCount;

    CJpegTask *pLastTask;

    UMC::VideoDecoderParams umcVideoParams;
    mfxVideoParamWrapper m_vFirstPar;
    mfxVideoParamWrapper m_vPar;

    VideoCORE * m_core;

    bool    m_isInit;
    bool    m_isOpaq;
    bool    m_isHeaderFound;
    bool    m_isHeaderParsed;

    mfxU32  m_frameOrder;

    mfxFrameAllocResponse m_response;
    mfxFrameAllocResponse m_response_alien;
    mfxDecodeStat m_stat;
    eMFXPlatform m_platform;

    UMC::Mutex m_mGuard;

#if defined (MFX_VA)
    VideoVppJpegD3D9 *m_pCc;
#endif

    UMC::VideoAccelerator * m_va;

    // Frame skipping rate
    mfxU32 m_skipRate;
    // Frame skipping count
    mfxU32 m_skipCount;

    //
    // Asynchronous processing functions
    //

    static
    mfxStatus MJPEGDECODERoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
    static
    mfxStatus MJPEGCompleteProc(void *pState, void *pParam, mfxStatus taskRes);

    // Thread decoding working function
#if defined (MFX_VA)
    mfxStatus RunThreadHW(void * params, mfxU32 threadNumber);
#endif
    mfxStatus RunThread(CJpegTask &task, mfxU32 threadNumber, mfxU32 callNumber);

};

#endif // _MFX_MJPEG_DEC_DECODE_H_
#endif // MFX_ENABLE_MJPEG_VIDEO_DECODE
