//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2010-2013 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_MVC_VIDEO_ENCODER)

#ifndef __MFX_MVC_ENCODE_H__
#define __MFX_MVC_ENCODE_H__

#include "umc_h264_video_encoder.h"
#include "umc_h264_core_enc.h"
#include "ippdefs.h"
#include "ippvc.h"

#define VM_SLICE_THREADING_H264
#define VM_MB_THREADING_H264
#define H264_NEW_THREADING

#ifdef VM_SLICE_THREADING_H264
#include "vm_thread.h"
#endif

#include "mfxdefs.h"
#include "mfxvideo.h"
#include "mfxvideo++int.h"

#include "mfx_umc_alloc_wrapper.h"
#include "mfx_h264_encode.h"

#include <vector>
#include <queue>

class MFXVideoENCODEMVC;

typedef struct __mvcMVCEncJobDesc {
    mfxEncodeCtrl    *ctrl;
    mfxFrameSurface1 *surface;
    mfxBitstream *bs;
} mvcMVCEncJobDesc;

struct MVCEncodeTaskInputParams
{
    std::vector<mvcMVCEncJobDesc> *jobs;
    bool buffered;
};

typedef struct __mfxMVCView {
    H264CoreEncoder_8u16s *enc;
    mfxVideoInternalParam          m_mfxVideoParam;
    mfxExtCodingOption     m_extOption;
    mfxExtBuffer          *m_pExtBuffer;
    UMC::VideoBrc         *m_brc;

    mfxU32                 m_frameCountSync;
    mfxU32                 m_frameCountBufferedSync;
    mfxU32                 m_frameCount;
    mfxU32                 m_frameCountBuffered;
    mfxU32                 m_lastIDRframe;
    mfxU32                 m_lastIframe;
    mfxU32                 m_lastRefFrame;
    mfxU32                 m_totalBits;
    mfxU32                 m_encodedFrames;

    bool                   m_Initialized;
    bool                   m_putEOSeq;
    bool                   m_putEOStream;
    bool                   m_putTimingSEI;
    bool                   m_resetRefList;
    bool                   m_useAuxInput;
    bool                   m_useSysOpaq;
    bool                   m_useVideoOpaq;
    bool                   m_is_ref4anchor;
    bool                   m_is_ref4nonanchor;

    mfxFrameSurface1       m_auxInput;
    mfxFrameAllocResponse  m_response;
    mfxFrameAllocResponse  m_response_alien;

    mfxMVCViewDependency   m_dependency;
    mfxU16                 m_maxdepvoid;
} mfxMVCView;

class MFXVideoENCODEMVC : public VideoENCODE {
public:
    virtual mfxTaskThreadingPolicy GetThreadingPolicy(void)
    {
        return MFX_TASK_THREADING_INTRA;
    }

    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);

    MFXVideoENCODEMVC(VideoCORE *core, mfxStatus *status);
    virtual ~MFXVideoENCODEMVC();
    virtual mfxStatus Init(mfxVideoParam *par);
    mfxStatus InitMVCView(mfxVideoParam *par, mfxExtCodingOption *opts, mfxI16 viewoid);

    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);
    //virtual mfxStatus GetSliceParam(mfxSliceParam *par);

    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat);
    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl * /*ctrl*/, mfxFrameSurface1 * /*surface*/, mfxBitstream * /*bs*/, mfxFrameSurface1 ** /*reordered_surface*/, mfxEncodeInternalParams * /*pInternalParams*/){return MFX_ERR_UNSUPPORTED;}
    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams,  MFX_ENTRY_POINT *pEntryPoint);
    virtual mfxStatus EncodeFrame(mfxEncodeCtrl *ctrl, mfxEncodeInternalParams *pInternalParams, mfxFrameSurface1 *surface, mfxBitstream *bs);
    virtual mfxStatus CancelFrame(mfxEncodeCtrl * /*ctrl*/, mfxEncodeInternalParams * /*pInternalParams*/, mfxFrameSurface1 * /*surface*/, mfxBitstream * /*bs*/) {return MFX_ERR_UNSUPPORTED;}
    virtual Status  Encode( void* state, VideoData* src,  MediaData* dst, Ipp32s frameNum, Ipp16u picStruct, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface);
    virtual Status H264CoreEncoder_CompressFrame( void* state, EnumPicCodType& ePictureType, EnumPicClass& ePic_Class, MediaData* dst,mfxEncodeCtrl *ctrl);
    virtual Status H264CoreEncoder_encodeSEI(void* state, bool is_base_view_sei, H264BsReal_8u16s* bs, MediaData* dst, bool bIDR_Pic, bool& startPicture, mfxEncodeCtrl *ctrl, Ipp8u recode );
    virtual mfxU32 GetSEISize_BufferingPeriod(const H264SeqParamSet& seq_parms, const Ipp8u NalHrdBpPresentFlag, const Ipp8u VclHrdBpPresentFlag);
    virtual mfxU32 GetSEISize_PictureTiming(const H264SeqParamSet& seq_parms, const Ipp8u CpbDpbDelaysPresentFlag, const Ipp8u pic_struct_present_flag, const H264SEIData_PictureTiming& timing_data);
    virtual Status PutSEI_MVCScalableNesting(H264BsReal_8u16s* bs, mfxU32 size);
    virtual Status H264CoreEncoder_updateSEIData( void* state );
    Status H264CoreEncoder_ModifyRefPicList( H264CoreEncoder_8u16s* core_enc, mfxEncodeCtrl *ctrl); // reordering and cut of ref pic lists L0 and L1
    mfxStatus AnalyzePicStruct(mfxMVCView *view, const mfxU16 picStructInSurface, mfxU16 *picStructForEncoding);
    mfxFrameSurface1 *GetOriginalSurface(mfxFrameSurface1 *surface);

#ifdef VM_SLICE_THREADING_H264
    Status ThreadedSliceCompress(Ipp32s numTh);
    static Ipp32u VM_THREAD_CALLCONVENTION ThreadWorkingRoutine(void* ptr);
#ifdef VM_MB_THREADING_H264
    Status ThreadCallBackVM_MBT(threadSpecificDataH264 &tsd);
    Status H264CoreEncoder_Compress_Slice_MBT(void* state, H264Slice_8u16s *curr_slice);
#endif
#endif
    mfxStatus InitAllocMVCSeqDesc(mfxExtMVCSeqDesc *depinfo);
    mfxStatus InitAllocMVCExtension();
    mfxStatus EncodeFrameCtrlCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface);
    Status UpdateRefPicList(void* state, H264Slice_8u16s *curr_slice, EncoderRefPicList_8u16s * ref_pic_list, H264SliceHeader &SHdr, RefPicListReorderInfo *pReorderInfo_L0, RefPicListReorderInfo *pReorderInfo_L1);

    mfxStatus WriteEndOfSequence(mfxBitstream *bs);
    mfxStatus WriteEndOfStream(mfxBitstream *bs);

    //virtual mfxStatus InsertUserData(mfxU8 *ud, mfxU32 len, mfxU64 ts);
    mfxI32 SelectFrameType_forLast(mfxI32 frameNum, bool bNotLast, mfxEncodeCtrl *ctrl);
    void MarkBRefs(mfxI32 lastIPdist); // selects buffered B to become references after normal reference frame came
    mfxStatus ReleaseBufferedFrames(H264CoreEncoder_8u16s *enc);


protected:
#ifdef H264_NEW_THREADING
    struct EncodeFrameTaskParams
    {
        // Counter to determine the thread which does single threading tasks
        volatile mfxI32 single_thread_selected;
        // Variable which distributes thread number among parallel tasks after single thread code finishes
        volatile mfxI32 thread_number;
        // Variable which distributes slices inside of parallel region
        volatile mfxI32 slice_counter;
        // Number of slice where it is necessary to encoding them in parallel
        volatile mfxI32 slice_number_limit;
        // Flag which signals that code before parallel region is finished
        volatile bool single_thread_done;
        // Event which allows single thread to wait for all parallel executing threads
        vm_event parallel_region_done;
        // Counter for threads that work in addition to the single thread
        volatile mfxI32 parallel_executing_threads;
        // Signal that parallel tasks may exit pRoutine
        volatile bool parallel_encoding_finished;
        // Flag that parallel cabac encoding is enabled for MBT
        volatile mfxI32 cabac_encoding_in_progress;

        threadSpecificDataH264 *threads_data;
    } m_taskParams;

    static mfxStatus TaskRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
    static mfxStatus TaskCompleteProc(void *pState, void *pParam, mfxStatus taskRes);
    Status EncodeNextSliceTask(mfxI32 thread_number, mfxI32 *slice_number);
#endif // H264_NEW_THREADING

    VideoCORE              *m_core;
    mfxHDL                  m_allocator_id;
    mfx_UMC_MemAllocator   *m_allocator;

    bool                    m_isinitialized;
    bool                    m_isOpaque;

    mfxU32                  m_numviews;         // Number of MVC views
    mfxMVCView             *m_views;            // Array of view structures
    mfxMVCView             *m_cview;            // Current encoder for async enc part
    mfxI16                  m_oidxmap[1024];    // view_id to voidx map
    mfxU32                  m_mvcext_sz;        // size of SPS mvc extension buffer
    mfxU32                 *m_mvcext_data;      // payload of SPS mvc extension buffer
    AVCInitValues           m_initValues;       // Used for dynamic resolution change

    H264SeqParamSet        *m_sps_views;        // SPS array (last SPS initialized with zeros)
    H264PicParamSet        *m_pps_views;        // PPS array (last PPS initialized with zeros)
    mfxU32                  m_num_sps;          // number of valid SPSes + 1
    mfxU32                  m_num_pps;          // number of valid PPSes + 1

    mfxExtMVCSeqDesc        m_mvcdesc;          // Dependancy structure used for encoding

    mfxU32                  m_au_count;         // AU counter
    mfxU32                  m_au_view_count;    // Current AU view counter
    mfxU32                  m_vo_current_view;  // Current view index in view output mode

    std::vector<std::queue<mvcMVCEncJobDesc> >  m_mvcjobs; // Per-view queues of frames to encode
    UMC::MediaData          m_bf_stream;        // Bitstream for bottom fields
    Ipp8u*                  m_bf_data;          // Pointer to allocated bottom fields bitstream data

#ifdef VM_SLICE_THREADING_H264
    // threads
    Ipp32s      threadsAllocated;
    threadSpecificDataH264 *threadSpec;
    threadInfoH264  **threads;
#endif

//    std::vector<mfxEncodeInternalParams> m_inFrames;
//    std::vector<mfxEncodeInternalParams> m_codFrames;

    mfxU32  m_ConstQP;
};

#endif // __MFX_H264_ENCODE_H__

#endif // MFX_ENABLE_H264_VIDEO_ENCODE
