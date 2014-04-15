/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_TASK_SUPPLIER_H
#define __UMC_H265_TASK_SUPPLIER_H

#include <vector>
#include <list>
#include "umc_h265_dec_defs_dec.h"
#include "umc_media_data_ex.h"
#include "umc_h265_heap.h"
#include "umc_h265_frame_info.h"
#include "umc_h265_frame_list.h"

#include "umc_h265_segment_decoder_mt.h"
#include "umc_h265_headers.h"

#include "umc_frame_allocator.h"

#include "umc_h265_au_splitter.h"

namespace UMC_HEVC_DECODER
{
class TaskBroker_H265;

class H265DBPList;
class H265DecoderFrame;
class H265Slice;
class MediaData;

class BaseCodecParams;
class H265SegmentDecoderMultiThreaded;
class TaskBrokerSingleThreadDXVA;

class MemoryAllocator;

enum
{
    BASE_VIEW                   = 0,
    INVALID_VIEW_ID             = -1
};

/****************************************************************************************************/
// Skipping_H265 class routine
/****************************************************************************************************/
class Skipping_H265
{
public:
    Skipping_H265();
    virtual
    ~Skipping_H265();

    void PermanentDisableDeblocking(bool disable);
    bool IsShouldSkipDeblocking(H265DecoderFrame * pFrame);
    bool IsShouldSkipFrame(H265DecoderFrame * pFrame);
    void ChangeVideoDecodingSpeed(Ipp32s& num);
    void Reset();

    struct SkipInfo
    {
        bool isDeblockingTurnedOff;
        Ipp32s numberOfSkippedFrames;
    };

    SkipInfo GetSkipInfo() const;

private:

    Ipp32s m_VideoDecodingSpeed;
    Ipp32s m_SkipCycle;
    Ipp32s m_ModSkipCycle;
    Ipp32s m_PermanentTurnOffDeblocking;
    Ipp32s m_SkipFlag;

    Ipp32s m_NumberOfSkippedFrames;
};

/****************************************************************************************************/
// TaskSupplier_H265
/****************************************************************************************************/
class SEI_Storer_H265
{
public:

    struct SEI_Message
    {
        H265DecoderFrame * frame;
        size_t  msg_size;
        size_t  offset;
        Ipp8u*  data;
        Ipp64f  timestamp;
        SEI_TYPE type;
        Ipp32s  isUsed;

        SEI_Message()
            : frame(0)
            , msg_size(0)
            , offset(0)
            , data(0)
            , timestamp(0)
            , type(SEI_RESERVED)
            , isUsed(0)
        {
        }
    };

    SEI_Storer_H265();

    virtual ~SEI_Storer_H265();

    void Init();

    void Close();

    void Reset();

    void SetTimestamp(H265DecoderFrame * frame);

    SEI_Message* AddMessage(UMC::MediaDataEx *nalUnit, SEI_TYPE type);

    const SEI_Message * GetPayloadMessage();

    void SetFrame(H265DecoderFrame * frame);

private:

    enum
    {
        MAX_BUFFERED_SIZE = 16 * 1024, // 16 kb
        START_ELEMENTS = 10,
        MAX_ELEMENTS = 128
    };

    std::vector<Ipp8u>  m_data;
    std::vector<SEI_Message> m_payloads;

    size_t m_offset;
    Ipp32s m_lastUsed;

    //std::list<> ;
};

/****************************************************************************************************/
// ViewItem_H265 class routine
/****************************************************************************************************/
struct ViewItem_H265
{
    // Default constructor
    ViewItem_H265(void);

    // Copy constructor
    ViewItem_H265(const ViewItem_H265 &src);

    ~ViewItem_H265();

    // Initialize th view, allocate resources
    UMC::Status Init();

    // Close the view and release all resources
    void Close(void);

    // Reset the view and reset all resource
    void Reset(void);

    // Reset the size of DPB for particular view item
    void SetDPBSize(H265SeqParamSet *pSps, Ipp32u & level_idc);

    // Pointer to the view's DPB
    mutable std::auto_ptr<H265DBPList> pDPB;

    // Size of DPB capacity in frames
    Ipp32u dpbSize;
    // Maximum number frames used semultaneously
    Ipp32u sps_max_dec_pic_buffering;
    Ipp32u sps_max_num_reorder_pics;

    // Pointer to the frame being processed
    H265DecoderFrame *pCurFrame;

    Ipp64f localFrameTime;
};

/****************************************************************************************************/
// MVC extension class routine
/****************************************************************************************************/
class MVC_Extension
{
public:
    MVC_Extension();
    virtual ~MVC_Extension();

    UMC::Status Init();
    virtual void Close();
    virtual void Reset();

    ViewItem_H265 *GetView();

protected:
    Ipp32u m_temporal_id;
    Ipp32u m_priority_id;
    Ipp32u HighestTid;

    Ipp32u  m_level_idc;

    ViewItem_H265 m_view;
};

/****************************************************************************************************/
// DecReferencePictureMarking_H265
/****************************************************************************************************/
class DecReferencePictureMarking_H265
{
public:

    DecReferencePictureMarking_H265();

    UMC::Status UpdateRefPicMarking(ViewItem_H265 &view, const H265Slice * pSlice);

    void Reset();

    Ipp32u  GetDPBError() const;

protected:

    Ipp32u  m_isDPBErrorFound;
    Ipp32s  m_frameCount;

    void ResetError();
};

/****************************************************************************************************/
// TaskSupplier_H265
/****************************************************************************************************/
class TaskSupplier_H265 : public Skipping_H265, public AU_Splitter_H265, public MVC_Extension, public DecReferencePictureMarking_H265
{
    friend class TaskBroker_H265;
    friend class TaskBrokerSingleThreadDXVA;

public:
    Ipp32u m_SliceIdxInTaskSupplier; //for h265 sliceidx cursliceidx m_sliceidx m_currsliceidx m_inumber

    TaskSupplier_H265();
    virtual ~TaskSupplier_H265();

    virtual UMC::Status Init(UMC::BaseCodecParams *pInit);

    virtual UMC::Status PreInit(UMC::BaseCodecParams *pInit);

    virtual void Reset();
    virtual void Close();

    UMC::Status GetInfo(UMC::VideoDecoderParams *lpInfo);

    virtual UMC::Status AddSource(UMC::MediaData * pSource, UMC::MediaData *dst);

    UMC::Status ProcessNalUnit(UMC::MediaDataEx *nalUnit);

    void SetMemoryAllocator(UMC::MemoryAllocator *pMemoryAllocator)
    {
        m_pMemoryAllocator = pMemoryAllocator;
    }

    void SetFrameAllocator(UMC::FrameAllocator *pFrameAllocator)
    {
        m_pFrameAllocator = pFrameAllocator;
    }

    virtual H265DecoderFrame *GetFrameToDisplayInternal(bool force);

    UMC::Status GetUserData(UMC::MediaData * pUD);

    bool IsWantToShowFrame(bool force = false);

    H265DBPList *GetDPBList()
    {
        ViewItem_H265 *pView = GetView();

        if (NULL == pView)
        {
            return NULL;
        }

        return pView->pDPB.get();
    }

    TaskBroker_H265 * GetTaskBroker()
    {
        return m_pTaskBroker;
    }

    virtual UMC::Status RunDecodingAndWait(bool force, H265DecoderFrame ** decoded = 0);
    virtual UMC::Status RunDecoding();
    virtual H265DecoderFrame * FindSurface(UMC::FrameMemID id);

    void PostProcessDisplayFrame(UMC::MediaData *dst, H265DecoderFrame *pFrame);

    virtual void AfterErrorRestore();

    SEI_Storer_H265 * GetSEIStorer() const { return m_sei_messages;}

    bool IsShouldSuspendDisplay();

    Headers * GetHeaders() { return &m_Headers;}

    inline const H265SeqParamSet *GetCurrentSequence(void) const
    {
        return m_Headers.m_SeqParams.GetCurrentHeader();
    }

    virtual H265Slice * DecodeSliceHeader(UMC::MediaDataEx *nalUnit);

    Heap_Objects * GetObjHeap()
    {
        return &m_ObjHeap;
    }

protected:

    void InitColorConverter(H265DecoderFrame *source, UMC::VideoData * videoData);

    void AddSliceToFrame(H265DecoderFrame *pFrame, H265Slice *pSlice);

    void ActivateHeaders(H265SeqParamSet *sps, H265PicParamSet *pps);

    bool IsSkipForCRAorBLA(const H265Slice *pSlice);

    void CheckCRAOrBLA(const H265Slice *pSlice);

    // Allocate a new frame and initialize it with slice parameters
    virtual H265DecoderFrame *AllocateNewFrame(const H265Slice *pSlice);
    // Initialize just allocated frame with slice parameters
    virtual UMC::Status InitFreeFrame(H265DecoderFrame *pFrame, const H265Slice *pSlice);
    // Initialize frame's counter and corresponding parameters
    virtual void InitFrameCounter(H265DecoderFrame *pFrame, const H265Slice *pSlice);

    UMC::Status AddSlice(H265Slice * pSlice, bool force);
    // Check whether all slices for the frame were found
    virtual void CompleteFrame(H265DecoderFrame * pFrame);
    virtual void OnFullFrame(H265DecoderFrame * pFrame);

    void DPBUpdate(const H265Slice * slice);

    virtual void AddFakeReferenceFrame(H265Slice * pSlice);

    UMC::Status AddOneFrame(UMC::MediaData * pSource, UMC::MediaData *dst);

    virtual UMC::Status AllocateFrameData(H265DecoderFrame * pFrame, IppiSize dimensions, Ipp32s bit_depth, const H265SeqParamSet* pSeqParamSet, const H265PicParamSet *pPicParamSet);

    virtual UMC::Status DecodeHeaders(UMC::MediaDataEx *nalUnit);
    virtual UMC::Status DecodeSEI(UMC::MediaDataEx *nalUnit);

    // Obtain free frame from queue
    virtual H265DecoderFrame *GetFreeFrame();

    UMC::Status CompleteDecodedFrames(H265DecoderFrame ** decoded);

    H265DecoderFrame *GetAnyFrameToDisplay(bool force);

    void PreventDPBFullness();

    Heap_Objects           m_ObjHeap;

    H265SegmentDecoderMultiThreaded **m_pSegmentDecoder;
    Ipp32u m_iThreadNum;

    H265ThreadGroup  m_threadGroup;

    Ipp64f      m_local_delta_frame_time;
    bool        m_use_external_framerate;

    H265Slice * m_pLastSlice;

    H265DecoderFrame *m_pLastDisplayed;

    UMC::MemoryAllocator *m_pMemoryAllocator;
    UMC::FrameAllocator  *m_pFrameAllocator;

    // Keep track of which parameter set is in use.
    bool              m_WaitForIDR;

    Ipp32s m_RA_POC;
    Ipp8u  NoRaslOutputFlag;
    NalUnitType m_IRAPType;

    Ipp32u            m_DPBSizeEx;
    Ipp32s            m_frameOrder;

    TaskBroker_H265 * m_pTaskBroker;

    UMC::VideoDecoderParams     m_initializationParams;
    UMC::VideoData              m_LastNonCropDecodedFrame;

    Ipp32s m_UIDFrameCounter;

    H265SEIPayLoad m_UserData;
    SEI_Storer_H265 *m_sei_messages;

    PocDecoding m_pocDecoding;

    bool m_isInitialized;
    bool m_isUseDelayDPBValues;

    UMC::Mutex m_mGuard;

private:
    UMC::Status xDecodeVPS(H265Bitstream &);
    UMC::Status xDecodeSPS(H265Bitstream &);
    UMC::Status xDecodePPS(H265Bitstream &);

    TaskSupplier_H265 & operator = (TaskSupplier_H265 &)
    {
        return *this;

    } // TaskSupplier_H265 & operator = (TaskSupplier_H265 &)

};

extern Ipp32s __CDECL CalculateDPBSize(Ipp32u level_idc, Ipp32s width, Ipp32s height);

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H264_TASK_SUPPLIER_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
