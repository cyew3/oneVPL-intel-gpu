//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_MPEG2_DECODE_H__
#define __MFX_MPEG2_DECODE_H__

#ifndef MFX_VA
typedef struct _DXVA_Status_VC1 *DXVA_Status_VC1;
#endif

#include "mfx_common.h"
#ifdef MFX_ENABLE_MPEG2_VIDEO_DECODE
#include "mfxvideo++int.h"
#include "mfx_mpeg2_dec_common.h"
#include "umc_mpeg2_dec_base.h"
#include "mfx_umc_alloc_wrapper.h"
#include "mfxpcp.h"
#include "mfx_task.h"
#include "mfx_critical_error_handler.h"

#include <memory>
#include <list>

#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
#include "umc_va_base.h"
#endif

//#define MFX_PROFILE_MPEG1 8
#define DPB 10
#define NUM_FRAMES DPB
#define NUM_REST_BYTES 4
#define NUM_FRAMES_BUFFERED 0
#define END_FRAME true
#define NO_END_FRAME false
#define REMOVE_LAST_FRAME false
#define REMOVE_LAST_2_FRAMES true
#define NO_SURFACE_TO_UNLOCK -1
#define NO_TASK_TO_UNLOCK -1

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
    mfxBitstream *m_frame;
    bool *m_frame_in_use;

    bool m_isSoftwareBuffer;
    bool m_isCorrupted;
} MParam;


class VideoDECODEMPEG2InternalBase : public MfxCriticalErrorHandler
{
public:
    VideoDECODEMPEG2InternalBase();

    virtual ~VideoDECODEMPEG2InternalBase();

    virtual mfxStatus AllocFrames(mfxVideoParam *par);
    static mfxStatus QueryIOSurfInternal(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);

    virtual mfxStatus Reset(mfxVideoParam *par);

    virtual mfxStatus Init(mfxVideoParam *par, VideoCORE * core);
    virtual mfxStatus Close();

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);

    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs,
                               mfxFrameSurface1 *surface_work,
                               mfxFrameSurface1 **surface_out,
                               MFX_ENTRY_POINT *pEntryPoint) = 0;

    virtual mfxStatus TaskRoutine(void *pParam) = 0;

    virtual mfxStatus ConstructFrame(mfxBitstream *bs, mfxFrameSurface1 *surface_work);
    mfxStatus ConstructFrame(mfxBitstream *in, mfxBitstream *out, mfxFrameSurface1 *surface_work);
    mfxStatus ConstructFrameImpl(mfxBitstream *in, mfxBitstream *out, mfxFrameSurface1 *surface_work);

    mfxStatus GetOutputSurface(mfxFrameSurface1 **surface_out, mfxFrameSurface1 *surface_work, UMC::FrameMemID index);
    mfxStatus SetOutputSurfaceParams(mfxFrameSurface1 *surface, int display_index);
    mfxStatus SetSurfacePicStruct(mfxFrameSurface1 *surface, int display_index);
    mfxStatus UpdateOutputSurfaceParamsFromWorkSurface(mfxFrameSurface1 *outputSurface, int display_index);
    mfxFrameSurface1 *GetOriginalSurface(mfxFrameSurface1 *pSurface);
    mfxStatus UpdateWorkSurfaceParams(int task_num);

    virtual mfxStatus CompleteTasks(void *pParam) = 0;

public:
    Ipp32s display_frame_count;
    Ipp32s cashed_frame_count;
    Ipp32s skipped_frame_count;
    Ipp32s dec_frame_count;
    Ipp32s dec_field_count;
    Ipp32s last_frame_count;

    Ipp32s m_NumThreads;
    Ipp32u maxNumFrameBuffered;

    bool m_isFrameRateFromInit;
    bool m_reset_done;
    bool m_isDecodedOrder;

    UMC::MediaData m_in[2*DPB];
    mfxBitstream m_frame[NUM_FRAMES];
    bool m_frame_in_use[NUM_FRAMES];

    mfxF64 m_time[NUM_FRAMES];
    UMC::FrameMemID mid[DPB];

    UMC::Mutex m_guard;

    Ipp32s m_InitW;
    Ipp32s m_InitH;
    Ipp32s m_InitPicStruct;

    std::auto_ptr<UMC::MPEG2VideoDecoderBase> m_implUmc;

    bool m_isSWBuf;
    bool m_isOpaqueMemory;
    mfxVideoParam m_vPar;
    UMC::VideoDecoderParams m_vdPar;

    mfx_UMC_FrameAllocator *m_FrameAllocator;

    mfxFrameAllocRequest allocRequest;
    mfxFrameAllocResponse allocResponse;
    mfxFrameAllocResponse m_opaqueResponse;

protected:

    VideoCORE *m_pCore;

    bool m_resizing;
    bool m_isSWDecoder;

    struct FcState
    {
        enum { NONE = 0, TOP = 1, BOT = 2, FRAME = 3 };
        mfxU8 picStart : 1;
        mfxU8 picHeader : 2;
    } m_fcState;

    bool m_found_SH;
    bool m_first_SH;
    bool m_new_bs;

    Ipp8u m_last_bytes[NUM_REST_BYTES];
    Ipp32s m_frame_curr;
    Ipp32s m_frame_free;
    bool m_frame_constructed;

    MParam m_task_param[2*DPB];
    Ipp32s m_task_num;
    Ipp32s m_prev_task_num;

    Ipp32s prev_index;
    Ipp32s next_index;
    Ipp32s curr_index;
    Ipp32s display_index;
    Ipp32s display_order;
    Ipp64u last_timestamp;
    mfxF64 last_good_timestamp;

    Ipp32s m_Protected;

    void ResetFcState(FcState& state) { state.picStart = state.picHeader = 0; }
    mfxStatus UpdateCurrVideoParams(mfxFrameSurface1 *surface_work, int task_num);

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
};

#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
namespace UMC
{
    class MPEG2VideoDecoderHW;
};
class VideoDECODEMPEG2Internal_HW : public VideoDECODEMPEG2InternalBase
{
public:
    VideoDECODEMPEG2Internal_HW();

    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs,
                               mfxFrameSurface1 *surface_work,
                               mfxFrameSurface1 **surface_out,
                               MFX_ENTRY_POINT *pEntryPoint);

    virtual mfxStatus TaskRoutine(void *pParam);
    virtual mfxStatus Init(mfxVideoParam *par, VideoCORE * core);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close();

    virtual mfxStatus CompleteTasks(void *pParam);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);

protected:

#ifdef UMC_VA_DXVA
    enum {
        MPEG2_STATUS_REPORT_NUM = 512,
    };

    DXVA_Status_VC1 m_pStatusReport[MPEG2_STATUS_REPORT_NUM];
    std::list<DXVA_Status_VC1> m_pStatusList;
#endif

    UMC::MPEG2VideoDecoderHW * m_implUmcHW;

#ifndef MFX_PROTECTED_FEATURE_DISABLE
    mfxExtPAVPOption m_pavpOpt;
#endif

    virtual mfxStatus ConstructFrame(mfxBitstream *bs, mfxFrameSurface1 *surface_work);
    mfxStatus PerformStatusCheck(void *pParam);
    mfxStatus GetStatusReport(mfxFrameSurface1 *displaySurface, UMC::FrameMemID surface_id);
    mfxStatus GetStatusReportByIndex(mfxFrameSurface1 *displaySurface, mfxU32 currIdx);
    virtual mfxStatus AllocFrames(mfxVideoParam *par);
    mfxStatus RestoreDecoder(Ipp32s frame_buffer_num, UMC::FrameMemID mem_id_to_unlock, Ipp32s task_num_to_unlock, bool end_frame, bool remove_2frames, int decrease_dec_field_count);
};

#endif // #if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)

#if !defined(MFX_ENABLE_HW_ONLY_MPEG2_DECODER) || !defined (MFX_VA)
namespace UMC
{
    class MPEG2VideoDecoderSW;
};

class VideoDECODEMPEG2Internal_SW : public VideoDECODEMPEG2InternalBase
{
public:
    VideoDECODEMPEG2Internal_SW();

    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs,
                               mfxFrameSurface1 *surface_work,
                               mfxFrameSurface1 **surface_out,
                               MFX_ENTRY_POINT *pEntryPoint);

    virtual mfxStatus TaskRoutine(void *pParam);

    virtual mfxStatus Init(mfxVideoParam *par, VideoCORE * core);
    virtual mfxStatus Close();
    virtual mfxStatus Reset(mfxVideoParam *par);

    virtual mfxStatus CompleteTasks(void *pParam);

protected:
    UMC::VideoData m_out;
    UMC::FrameData * m_FrameData[32];
    UMC::MPEG2VideoDecoderSW * m_implUmcSW;
};
#endif

class VideoDECODEMPEG2 : public VideoDECODE
{

public:
    std::auto_ptr<VideoDECODEMPEG2InternalBase> internalImpl;
    
    static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);
    static mfxStatus DecodeHeader(VideoCORE *core, mfxBitstream *bs, mfxVideoParam *par);

    VideoDECODEMPEG2(VideoCORE *core, mfxStatus *sts);
    virtual ~VideoDECODEMPEG2();

    mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close();

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    static mfxStatus CheckProtectionSettings(mfxVideoParam *input, mfxVideoParam *output, VideoCORE *core);
#endif

    virtual mfxStatus DecodeFrame(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_out)
        {bs; surface_work; surface_out;return MFX_ERR_NONE;};


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
    VideoCORE *m_pCore;

    bool m_isInitialized;
    bool m_isSWImpl;


    enum SkipLevel
    {
        SKIP_NONE = 0,
        SKIP_B,
        SKIP_PB,
        SKIP_ALL
    };
    Ipp32s m_SkipLevel;
};

#pragma warning(default: 4324)

#endif //MFX_ENABLE_MPEG2_VIDEO_DECODE
#endif //__MFX_MPEG2_DECODE_H__
