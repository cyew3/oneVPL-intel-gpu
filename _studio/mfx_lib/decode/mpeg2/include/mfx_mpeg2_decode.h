/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2013 Intel Corporation. All Rights Reserved.

File Name: mfx_mpeg2_decode.h

\* ****************************************************************************** */

#ifndef __MFX_MPEG2_DECODE_H__
#define __MFX_MPEG2_DECODE_H__

#ifndef MFX_VA
typedef struct _DXVA_Status_VC1 *DXVA_Status_VC1;
#endif

//#include <deque>
#include "mfx_common.h"
#ifdef MFX_ENABLE_MPEG2_VIDEO_DECODE
#include "mfxvideo++int.h"
#include "mfx_mpeg2_dec_common.h"
#include "umc_mpeg2_dec_base.h"
#include "umc_data_pointers_copy.h"
#include "umc_video_processing.h"
#include "mfx_umc_alloc_wrapper.h"
#include "mfxpcp.h"
#include <mfx_task.h>

#include <list>

//#define MFX_PROFILE_MPEG1 8
#define DPB 10
#define NUM_FRAMES DPB
#define NUM_REST_BYTES 4
#define NUM_FRAMES_BUFFERED 0

enum
{
    MINIMAL_BITSTREAM_LENGTH    = 4,
};

enum FrameType
{
    I_PICTURE               = 1,
    P_PICTURE               = 2,
    B_PICTURE               = 3
};


typedef enum {
    STATUS_REPORT_OPERATION_SUCCEEDED   = 0,
    STATUS_REPORT_MINOR_PROBLEM         = 1,
    STATUS_REPORT_SIGNIFICANT_PROBLEM   = 2,
    STATUS_REPORT_SEVERE_PROBLEM        = 3,
    STATUS_REPORT_SEVERE_PROBLEM_OTHER  = 4,

} DXVAStatusReportFeedback;

typedef struct _DisplayBuffer
{
    int mid;
    int index;
    Ipp64u timestamp;
}DisplayBuffer;

typedef struct _MParam
{
    Ipp32s m_curr_thread_idx;
    Ipp32s m_thread_completed;
    Ipp32s task_num;
    Ipp32s NumThreads;
    bool m_isDecodedOrder;

    Ipp32s  m_frame_curr;
    Ipp64u last_timestamp;
    Ipp32s curr_index;
    Ipp32s prev_index;
    Ipp32s next_index;
    Ipp32s last_frame_count;
    int display_index;
    bool IsSWImpl;

    UMC::MediaData *in;
    mfxFrameSurface1 *surface_out;
    mfxFrameSurface1 *surface_work;
    mfx_UMC_FrameAllocator *m_FrameAllocator;
    mfxVideoParam m_vPar;
    Ipp32s *mid;
    UMC::Mutex *m_pGuard;
    mfxBitstream *m_frame;
    bool *m_frame_in_use;

    bool m_isSoftwareBuffer;
    bool m_isCorrupted;

    VideoCORE *pCore;

} MParam;


class VideoDECODEMPEG2 : public VideoDECODE
{

    enum {
    MPEG2_STATUS_REPORT_NUM = 512,
};

public:
    static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);
    static mfxStatus DecodeHeader(VideoCORE *core, mfxBitstream *bs, mfxVideoParam *par);
    static mfxStatus TaskRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
    static mfxStatus CompleteTasks(void *pState, void *pParam, mfxStatus res);

    VideoDECODEMPEG2(VideoCORE *core, mfxStatus *sts);
    virtual ~VideoDECODEMPEG2();

    mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close();

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);
    virtual mfxStatus GetSliceParam(mfxSliceParam *par);

    mfxStatus GetStatusReport(mfxFrameSurface1 *displaySurface);
    mfxStatus GetStatusReportByIndex(mfxFrameSurface1 *displaySurface, mfxU32 currIdx);
    static mfxStatus CheckProtectionSettings(mfxVideoParam *input, mfxVideoParam *output, VideoCORE *core);
    mfxFrameSurface1 *GetOriginalSurface(mfxFrameSurface1 *pSurface);
    mfxStatus GetOutputSurface(mfxFrameSurface1 **surface_out, mfxFrameSurface1 *surface_work, UMC::FrameMemID index);

    virtual mfxStatus DecodeFrame(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_out)
        {bs; surface_work; surface_out;return MFX_ERR_NONE;};

    virtual mfxStatus ConstructFrameImpl(mfxBitstream *in, mfxBitstream *out, mfxFrameSurface1 *surface_work);
    virtual mfxStatus ConstructFrame(mfxBitstream *in, mfxBitstream *out, mfxFrameSurface1 *surface_work);

    virtual mfxStatus GetDecodeStat(mfxDecodeStat *stat);
    virtual mfxStatus DecodeFrameCheck(mfxBitstream *, mfxFrameSurface1 *, mfxFrameSurface1 **)
    {
        return MFX_ERR_NONE;
    };
    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs,
                               mfxFrameSurface1 *surface_work,
                               mfxFrameSurface1 **surface_out,
                               MFX_ENTRY_POINT *pEntryPoint);
  
    mfxStatus CheckFrameData(const mfxFrameSurface1 *pSurface);

    virtual mfxStatus GetUserData(mfxU8 *ud, mfxU32 *sz, mfxU64 *ts,mfxU16 bufsize);
    virtual mfxStatus GetPayload(mfxU64 *ts, mfxPayload *payload);
    virtual mfxStatus SetSkipMode(mfxSkipMode mode);

protected:
    mfxStatus InternalReset(mfxVideoParam* par);
    mfxStatus InternalResetUMC(mfxVideoParam* par, UMC::MediaData* data);
    static mfxStatus QueryIOSurfInternal(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);
    static bool  IsHWSupported(VideoCORE *pCore, mfxVideoParam *par);
    //mfxStatus LookHeader(VideoCORE *core, mfxBitstream* bs, mfxVideoParam* par, mfxFrameSurface1 *surface_work);

    UMC::MPEG2VideoDecoderBase m_implUmc;
    //UMC::DataPointersCopy m_PostProcessing;
    UMC::VideoProcessing m_PostProcessing;

    VideoCORE *m_pCore;

#ifdef UMC_VA_DXVA
    DXVA_Status_VC1 m_pStatusReport[MPEG2_STATUS_REPORT_NUM];
    std::list<DXVA_Status_VC1> m_pStatusList;
#endif

    mfx_UMC_FrameAllocator *m_FrameAllocator;

#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
    mfx_UMC_FrameAllocator_D3D     *m_FrameAllocator_d3d;
    mfx_UMC_FrameAllocator_NV12    *m_FrameAllocator_nv12;
#else
    mfx_UMC_FrameAllocator_NV12    *m_FrameAllocator_nv12;
#endif

    UMC::FrameMemID            mid[DPB];
    UMC::FrameData *           m_FrameData[32];
    mfxFrameAllocRequest allocRequest;
    mfxFrameAllocResponse allocResponse;
    MParam m_task_param[2*DPB];
    Ipp32s m_task_num;
    Ipp32s m_prev_task_num;
#if defined (ELK_WORKAROUND)
    mfxFrameAllocResponse allocResponseAdd;
#endif
    Ipp32s prev_index;
    Ipp32s next_index;
    Ipp32s curr_index;
    Ipp32s display_index;
    Ipp32s display_order;
    Ipp32s dec_frame_count;
    Ipp32s dec_field_count;
    Ipp32s last_frame_count;
    Ipp64u last_timestamp;
    mfxF64 last_good_timestamp;

    Ipp32s display_frame_count;
    Ipp32s cashed_frame_count;
    Ipp32s skipped_frame_count;

    mfxVideoParam m_vPar;
    mfxExtPAVPOption m_pavpOpt;

    UMC::VideoDecoderParams m_vdPar;
    mfxFrameParam m_fPar;
    mfxSliceParam m_sPar;
    UMC::VideoData m_out;
    UMC::MediaData m_in[2*DPB];

    mfxBitstream m_frame[NUM_FRAMES];
    bool m_frame_in_use[NUM_FRAMES];
    Ipp8u m_last_bytes[NUM_REST_BYTES];
    Ipp32s m_frame_curr;
    Ipp32s m_frame_free;
    bool m_frame_constructed;
    Ipp32s m_InitW;
    Ipp32s m_InitH;
    Ipp32s m_InitPicStruct;
    Ipp32s m_Protected;
    bool m_found_SH;
    bool m_first_SH;
    bool m_new_bs;
    bool m_isFrameRateFromInit;
    mfxF64 m_time[NUM_FRAMES];
    mfxU8 m_aspect_ratio_information;

    Ipp32s m_NumThreads;
    Ipp32u maxNumFrameBuffered;

    mfxU16 m_extendedPicStruct;
    UMC::Mutex m_guard;
    //get index to read input data:
    bool SetCurr_m_frame()
    {
        m_frame_curr++;
        if(m_frame_curr == NUM_FRAMES)
            m_frame_curr = 0;
        if(!m_frame_in_use[m_frame_curr])
            return false;
        return true;
    }
    //set index to write input data:
    bool SetFree_m_frame()
    {
        int prev = m_frame_free;
        m_frame_free++;
        if(m_frame_free == NUM_FRAMES)
            m_frame_free = 0;
        if(m_frame_in_use[m_frame_free])
        {
            m_frame_free = prev;
            return false;
        }
        return true;
    }

    //bool m_deferredInit;
    bool m_isInitialized;
    bool m_isSWBuf;
    bool m_reset_done;
    bool m_resizing;
    bool m_isShDecoded;
    bool m_isSWImpl;
    bool m_isOpaqueMemory;

    mfxFrameAllocResponse m_opaqueResponse;

    bool m_isDecodedOrder;

    enum SkipLevel
    {
        SKIP_NONE = 0,
        SKIP_B,
        SKIP_PB,
        SKIP_ALL
    };
    Ipp32s m_SkipLevel;

    struct FcState
    {
        enum { NONE = 0, TOP = 1, BOT = 2, FRAME = 3 };
        mfxU8 picStart : 1;
        mfxU8 picHeader : 2;
    } m_fcState;

    void ResetFcState(FcState& state) { state.picStart = state.picHeader = 0; }
    mfxStatus UpdateCurrVideoParams(mfxFrameSurface1 *surface_work, int task_num);
    mfxStatus SetSurfacePicStruct(mfxFrameSurface1 *surface, int task_num);
};

#pragma warning(default: 4324)

#endif //MFX_ENABLE_MPEG2_VIDEO_DECODE
#endif //__MFX_MPEG2_DECODE_H__
