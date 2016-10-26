//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2013 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H264_VIDEO_ENCODE)

#include "umc_defs.h"

#ifndef __MFX_H264_ENCODE_H__
#define __MFX_H264_ENCODE_H__

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
#include "mfxsvc.h"
#include "umc_svc_brc.h"

#include "mfx_umc_alloc_wrapper.h"

class MFXVideoENCODEH264;
class mfxVideoInternalParam;

#ifdef VM_SLICE_THREADING_H264
struct threadSpecificDataH264
{
  void* state;
  H264CoreEncoder_8u16s* core_enc;
  Ipp32s slice;
  Ipp32s slice_qp_delta_default;
  EnumSliceType default_slice_type;
  ThreadDataVM_MBT_8u16s mbt_data;
};

struct threadInfoH264
{
  Ipp32s    numTh;
  MFXVideoENCODEH264 *m_lpOwner;
  vm_event  start_event;
  vm_event  stop_event;
  vm_event  quit_event;
  vm_thread thread;
};
#endif

#ifdef H264_NEW_THREADING
struct EncodeTaskInputParams
{
    mfxEncodeCtrl *ctrl;
    mfxFrameSurface1 *surface;
    mfxBitstream *bs;
    mfxU8 picState;  // field-based output: tell async part is it called for 1st or 2nd field
};
#endif // H264_NEW_THREADING

struct AVCTemporalLayers
{
    mfxU8 NumLayers;
    mfxU8 LayerScales[8];
    mfxU8 LayerPIDs[8];
    mfxU8 LayerNumRefs[8];
};

struct AVCInitValues
{
    mfxU16 FrameWidth;
    mfxU16 FrameHeight;
};

enum {
    FIELD_OUTPUT_NO_TYPE      = 0,
    FIELD_OUTPUT_FIRST_FIELD  = 1,
    FIELD_OUTPUT_SECOND_FIELD = 2
};

#define POC_UNSPECIFIED 0x7fffffff

// parameters for single SVC layer
// taken from mfxExtSVCSeqDesc
typedef mfxExtSVCDepLayer svcLayerInfo;

struct LayerHRDParams {
    mfxU32 BufferSizeInKB;
    mfxU32 InitialDelayInKB;
    mfxU32 TargetKbps;
    mfxU32 MaxKbps;
};

struct h264Layer
{
    // SVC layer specific
    H264CoreEncoder_8u16s* enc; //Pointer to encoder instance
    UMC::VideoBrc *m_brc[MAX_QUALITY_LEVELS];
    mfxU32 m_representationFrameSize[MAX_QUALITY_LEVELS][MAX_TEMP_LEVELS];
    mfxI64 m_representationBF[MAX_QUALITY_LEVELS][MAX_TEMP_LEVELS];
    struct{
        mfxU16  QPI;
        mfxU16  QPP;
        mfxU16  QPB;
    } m_cqp[MAX_QUALITY_LEVELS][MAX_TEMP_LEVELS];
    
    mfxVideoInternalParam  m_mfxVideoParam;
    mfxExtCodingOption     m_extOption;
    mfxExtBuffer          *m_pExtBuffer;

    mfxU32  m_frameCountSync;         // counter for sync. part
    mfxU32  m_frameCountBufferedSync; // counter for sync. part

    mfxU32  m_frameCount;    // counter for Async. part
    mfxU32  m_frameCountBuffered;    // counter for Async. part
    mfxU32  m_lastIDRframe;
    mfxU32  m_lastIframe;
    mfxU32  m_lastIframeEncCount;
    mfxU32  m_lastRefFrame;

    //bool    m_fieldOutput; // true for field-based output
    //mfxU8   m_fieldOutputState; // field-based output: state of encoding (1st or 2nd field)
    //mfxStatus    m_fieldOutputStatus;


    mfxU32  m_totalBits;
    mfxU32  m_encodedFrames;
    bool    m_Initialized;
    bool    m_putEOSeq;  //write End of Sequence
    bool    m_putEOStream; //write End of Stream
    bool    m_putTimingSEI; //write Timing SEI
#ifdef H264_INTRA_REFRESH
    bool    m_putRecoveryPoint;
#endif // H264_INTRA_REFRESH
    bool    m_putClockTimestamp; //write clock timestamps
    bool    m_putDecRefPicMarkinfRepSEI; //write Dec ref pic marking repetition info
    bool    m_resetRefList;

    bool    m_useAuxInput;
    //bool    m_useSysOpaq;
    //bool    m_useVideoOpaq;
    //bool    m_isOpaque;
    mfxFrameSurface1 m_auxInput;
    mfxFrameAllocResponse m_response;
    mfxFrameAllocResponse m_response_alien;

    struct h264Layer* ref_layer; // lower layer, used to predict
    struct h264Layer* src_layer; // upper layer, to construct this if not provided
    svcLayerInfo m_InterLayerParam;
};


class MFXVideoENCODEH264 : public VideoENCODE {
public:
#ifdef H264_NEW_THREADING
    virtual mfxTaskThreadingPolicy GetThreadingPolicy(void)
    {
        return MFX_TASK_THREADING_INTRA;
    }
#endif // H264_NEW_THREADING

    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);

    MFXVideoENCODEH264(VideoCORE *core, mfxStatus *status);
    virtual ~MFXVideoENCODEH264();
    virtual mfxStatus Init(mfxVideoParam *par);

    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);
    //virtual mfxStatus GetSliceParam(mfxSliceParam *par);

    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat);
    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams);
#ifdef H264_NEW_THREADING
    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams,  MFX_ENTRY_POINT *pEntryPoint);
#endif // H264_NEW_THREADING
    virtual mfxStatus EncodeFrame(mfxEncodeCtrl *ctrl, mfxEncodeInternalParams *pInternalParams, mfxFrameSurface1 *surface, mfxBitstream *bs);
    virtual mfxStatus CancelFrame(mfxEncodeCtrl * /*ctrl*/, mfxEncodeInternalParams * /*pInternalParams*/, mfxFrameSurface1 * /*surface*/, mfxBitstream * /*bs*/) {return MFX_ERR_UNSUPPORTED;}
    virtual Status  Encode( void* state, VideoData* src,  MediaData* dst, MediaData* dst_ext, Ipp16u picStruct, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface);
    virtual Status H264CoreEncoder_CompressFrame( void* state, EnumPicCodType& ePictureType, EnumPicClass& ePic_Class, MediaData* dst, MediaData* dst_ext, mfxEncodeCtrl *ctrl);
    virtual Status H264CoreEncoder_encodeSEI( void* state, H264BsReal_8u16s* bs, MediaData* dst, bool bIDR_Pic, bool& startPicture, mfxEncodeCtrl *ctrl, Ipp8u recode );
    virtual Status H264CoreEncoder_updateSEIData( void* state );

    Status SVC_encodeBufferingPeriodSEI_inScalableNestingSEI(void* state, H264BsReal_8u16s* bs, MediaData* dst, bool& startPicture);

    Status ReorderListSVC( H264CoreEncoder_8u16s* core_enc, EnumPicClass& ePic_Class); // exclude higher temporal levels references
    Status H264CoreEncoder_ModifyRefPicList( H264CoreEncoder_8u16s* core_enc, mfxEncodeCtrl *ctrl); // reordering and cut of ref pic lists L0 and L1
    mfxStatus AnalyzePicStruct(h264Layer *layer, const mfxU16 picStructInSurface, mfxU16 *picStructForEncoding);
    mfxFrameSurface1 *GetOriginalSurface(mfxFrameSurface1 *surface);

#ifdef VM_SLICE_THREADING_H264
    Status ThreadedSliceCompress(Ipp32s numTh);
    static Ipp32u VM_THREAD_CALLCONVENTION ThreadWorkingRoutine(void* ptr);
#ifdef VM_MB_THREADING_H264
    Status ThreadCallBackVM_MBT(threadSpecificDataH264 &tsd);
    Status H264CoreEncoder_Compress_Slice_MBT(void* state, H264Slice_8u16s *curr_slice);
#endif
#endif

    mfxStatus WriteEndOfSequence(mfxBitstream *bs);
    mfxStatus WriteEndOfStream(mfxBitstream *bs);

    //virtual mfxStatus InsertUserData(mfxU8 *ud, mfxU32 len, mfxU64 ts);
    mfxI32    SelectFrameType_forLast( bool bNotLast, mfxEncodeCtrl *ctrl);
    void      MarkBRefs(mfxI32 lastIPdist); // selects buffered B to become references after normal reference frame came
    mfxU16    SetBEncOrder (sH264EncoderFrame_8u16s *left, sH264EncoderFrame_8u16s *right, mfxU16 order, mfxI16 freerefs);
    mfxStatus ReleaseBufferedFrames(h264Layer* layer);

    mfxStatus InitSVCLayer(const mfxExtSVCRateControl *rc, mfxU16 did, h264Layer *layer);
    mfxStatus CloseSVCLayer(h264Layer* layer);
    mfxStatus SetScalabilityInfoSE();

#ifdef USE_TEMPTEMP
    mfxStatus InitGopTemplate(mfxVideoParam* par);
#endif // USE_TEMPTEMP

    Status H264CoreEncoder_encodeFrameHeader(
        void* state,
        H264BsReal_8u16s* bs,
        MediaData* dst,
        bool bIDR_Pic,
        bool& startPicture );

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

    ////H264CoreEncoder_8u16s* enc; //Pointer to encoder instance

    VideoCORE *m_core;
    ////UMC::VideoBrc  *m_brc;
    mfxHDL m_allocator_id;
    mfx_UMC_MemAllocator *m_allocator;
    ////mfxVideoParam          m_mfxVideoParam;
    ////mfxExtCodingOption     m_extOption;
    ////mfxExtBuffer          *m_pExtBuffer;
    //mfxSliceParam          m_mfxSliceParam;
    mfxBitstream           m_extBitstream; // field-based output: put 2nd encoded field to this bitstream

    ////// the only 2 variables for synchronous part
    ////mfxU32  m_frameCountSync;         // counter
    ////mfxU32  m_frameCountBufferedSync; // counter for sync. part

    ////mfxU32  m_frameCount;    // counter for Async. part
    ////mfxU32  m_frameCountBuffered;    // counter for Async. part
    ////mfxU32  m_lastIDRframe;
    ////mfxU32  m_lastIframe;
    ////mfxU32  m_lastRefFrame;

    bool    m_fieldOutput; // true for field-based output
    mfxU8   m_fieldOutputState; // field-based output: state of encoding (1st or 2nd field)
    mfxStatus    m_fieldOutputStatus;

    bool m_internalFrameOrders;
    AVCTemporalLayers m_temporalLayers;
    AVCInitValues m_initValues;

    ////mfxU32  m_totalBits;
    ////mfxU32  m_encodedFrames;
    ////bool    m_Initialized;
    ////bool    m_putEOSeq;  //write End of Sequence
    ////bool    m_putEOStream; //write End of Stream
    ////bool    m_putTimingSEI; //write Timing SEI
    ////bool    m_putClockTimestamp; //write clock timestamps
    ////bool    m_putDecRefPicMarkinfRepSEI; //write Dec ref pic marking repetition info
    ////bool    m_resetRefList;
    bool    m_separateSEI;

    ////bool    m_useAuxInput;
    bool    m_useSysOpaq;
    bool    m_useVideoOpaq;
    bool    m_isOpaque;
    ////mfxFrameSurface1 m_auxInput;
    ////mfxFrameAllocResponse m_response;
    ////mfxFrameAllocResponse m_response_alien;

    struct h264Layer m_base;
    struct h264Layer* m_layer; // current layer, being encoded
    struct h264Layer* m_layerSync; // current layer for sync. part (EncodeFrameCheck)

    ////bool    m_resetRefList;


#ifdef VM_SLICE_THREADING_H264
    // threads
    Ipp32s      threadsAllocated;
    threadSpecificDataH264 *threadSpec;
    threadInfoH264  **threads;
#endif

    mfxU32  m_ConstQP;

    h264Layer * m_svc_layers[MAX_DEP_LAYERS]; // 8 is enough, not MAX_SVC_LAYERS
    mfxU32 m_svc_count;
    mfxI32 m_selectedPOC;                     // selected when input surface became 0
    mfxI32 m_maxDid;                          // max Did, used to reset selectedPOC

#ifdef USE_TEMPTEMP
    // common temporal template
    SVCRefControl* m_refctrl; // For temporal; contains index in GOP of the last frame, which refers to it (display order)
    mfxI32  m_temptempstart;  // FrameTag for first frame in temporal template
    mfxI32  m_temptemplen;    // length of the temporal template
    mfxI32  m_temptempwidth;  // step back when went to the end of template
    mfxI32  m_usetemptemp;    // flag to use temporal template
    mfxI32  m_maxTempScale;   // highest ratio in frame rates
#endif // USE_TEMPTEMP

    H264SVCScalabilityInfoSEI m_svc_ScalabilityInfoSEI;

    struct LayerHRDParams m_HRDparams[MAX_DEP_LAYERS][MAX_QUALITY_LEVELS][MAX_TEMP_LEVELS];

    // big buffers for svc resampling, provided to all layers encoders
    Ipp32s * allocated_tmpBuf;
    Ipp32s * m_tmpBuf0; // upmost layer size
    Ipp32s * m_tmpBuf1; // upmost height
    Ipp32s * m_tmpBuf2; // upmost layer size
};

#endif // __MFX_H264_ENCODE_H__

#endif // MFX_ENABLE_H264_VIDEO_ENCODE
