/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#ifndef _MFX_VP9_DECODE_HW_H_
#define _MFX_VP9_DECODE_HW_H_

#include "mfx_common.h"

#if defined(MFX_ENABLE_VP9_VIDEO_DECODE_HW) && defined(MFX_VA)

#include "mfx_umc_alloc_wrapper.h"
#include "umc_mutex.h"

#include "mfx_task.h"
#include "mfx_vp9_dec_decode_utils.h"

using namespace MfxVP9Decode;

class VideoDECODEVP9_HW : public VideoDECODE
{
public:

    VideoDECODEVP9_HW(VideoCORE *pCore, mfxStatus *sts);

    static mfxStatus Query(VideoCORE *pCore, mfxVideoParam *pIn, mfxVideoParam *pOut);
    static mfxStatus QueryIOSurf(VideoCORE *pCore, mfxVideoParam *pPar, mfxFrameAllocRequest *pRequest);

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *pPar);
    virtual mfxStatus Close();

    virtual mfxTaskThreadingPolicy GetThreadingPolicy();
    virtual mfxStatus GetVideoParam(mfxVideoParam *pPar);
    virtual mfxStatus GetDecodeStat(mfxDecodeStat *pStat);

    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out);
    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *pSurfaceWork, mfxFrameSurface1 **ppSurfaceOut, MFX_ENTRY_POINT *pEntryPoint);
    virtual mfxStatus DecodeFrame(mfxBitstream *bs, mfxFrameSurface1 *pSurfaceWork, mfxFrameSurface1 *pSurfaceOut);

    virtual mfxStatus GetUserData(mfxU8 *pUserData, mfxU32 *pSize, mfxU64 *pTimeStamp);
    virtual mfxStatus GetPayload(mfxU64 *pTimeStamp, mfxPayload *pPayload);
    virtual mfxStatus SetSkipMode(mfxSkipMode mode);

protected:
    void CalculateTimeSteps(mfxFrameSurface1 *);
    mfxStatus PreDecodeFrame(mfxBitstream *, mfxFrameSurface1 *);
    mfxStatus QueryIOSurfInternal(eMFXPlatform, mfxVideoParam *, mfxFrameAllocRequest *);

    mfxStatus UpdateRefFrames(const mfxU8 refreshFrameFlags, VP9FrameInfo & info);

    mfxStatus DecodeFrameHeader(mfxBitstream *in, VP9FrameInfo & info);
    mfxStatus PackHeaders(mfxBitstream *bs, VP9FrameInfo const & info);

    bool CheckHardwareSupport(VideoCORE *p_core, mfxVideoParam *p_par);

    mfxStatus GetOutputSurface(mfxFrameSurface1 **surface_out, mfxFrameSurface1 *surface_work, UMC::FrameMemID index);

private:
    bool                    m_is_initialized;
    VideoCORE*              m_core;
    eMFXPlatform            m_platform;

    mfxVideoParamWrapper    m_on_init_video_params,
                            m_video_params;
    mfxU32                  m_init_w,
                            m_init_h;
    mfxU32                  m_num_output_frames;
    mfxF64                  m_in_framerate;
    mfxU16                  m_frameOrder;

    vm_mutex                m_mutex;

    std::auto_ptr<mfx_UMC_FrameAllocator> m_FrameAllocator;

    bool m_is_software_buffer;

    mfxFrameAllocResponse   m_response;
    mfxDecodeStat           m_stat;

    //////////////////////////////////////////

    UMC::VideoAccelerator * m_va;

    VP9FrameInfo m_frameInfo;

    mfxI32 m_baseQIndex;
    mfxI32 m_y_dc_delta_q;
    mfxI32 m_uv_dc_delta_q;
    mfxI32 m_uv_ac_delta_q;
};

#endif // _MFX_VP9_DECODE_HW_H_
#endif // MFX_ENABLE_VP8_VIDEO_DECODE && MFX_VA
