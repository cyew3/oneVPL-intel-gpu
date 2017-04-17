//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_TASK_SUPPLIER_H
#define __UMC_H265_TASK_SUPPLIER_H

#include <vector>
#include <list>
#include "umc_h265_dec_defs.h"
#include "umc_media_data_ex.h"
#include "umc_h265_heap.h"
#include "umc_h265_frame_info.h"
#include "umc_h265_frame_list.h"

#include "umc_h265_headers.h"
#include "umc_frame_allocator.h"

#include "umc_h265_au_splitter.h"
#include "umc_h265_segment_decoder_base.h"

#include "umc_va_base.h"

#if (defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)) && !defined(MFX_PROTECTED_FEATURE_DISABLE)
#include "umc_h265_widevine_decrypter.h"
#endif

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

    // Disable deblocking filter to increase performance
    void PermanentDisableDeblocking(bool disable);
    // Check if deblocking should be skipped
    bool IsShouldSkipDeblocking(H265DecoderFrame * pFrame);
    // Check if frame should be skipped to decrease decoding delays
    bool IsShouldSkipFrame(H265DecoderFrame * pFrame);
    // Set decoding skip frame mode
    void ChangeVideoDecodingSpeed(Ipp32s& num);
    void Reset();

    struct SkipInfo
    {
        bool isDeblockingTurnedOff;
        Ipp32s numberOfSkippedFrames;
    };

    // Get current skip mode state
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

        size_t      size;
        size_t      offset;
        Ipp8u*      data;

        Ipp32s      nal_type;
        Ipp64f      timestamp;
        SEI_TYPE    type;

        Ipp32s      isUsed;

        SEI_Message()
        {
            clear();
        }

        bool empty() const
        { return !isUsed; }

        void clear()
        {
            frame = NULL;
            data =  NULL;
            size = offset = 0;
            nal_type = NAL_UT_INVALID;
            timestamp = 0;
            type = SEI_RESERVED;
            isUsed = 0;
        }
    };

    SEI_Storer_H265();

    virtual ~SEI_Storer_H265();

    // Initialize SEI storage
    void Init();

    // Deallocate SEI storage
    void Close();

    // Reset SEI storage
    void Reset();

    // Set timestamp for stored SEI messages
    void SetTimestamp(H265DecoderFrame * frame);

    // Put a new SEI message to the storage
    SEI_Message* AddMessage(UMC::MediaDataEx *nalUnit, SEI_TYPE type);

    // Retrieve a stored SEI message which was not retrieved before
    const SEI_Message * GetPayloadMessage();

    // Set SEI frame for stored SEI messages
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

    // Initialize the view, allocate resources
    UMC::Status Init();

    // Close the view and release all resources
    void Close(void);

    // Reset the view and reset all resource
    void Reset(void);

    // Reset the size of DPB for particular view item
    void SetDPBSize(H265SeqParamSet *pSps, Ipp32u & level_idc);

    // Pointer to the view's DPB
    mutable std::unique_ptr<H265DBPList> pDPB;

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

    // Update DPB contents marking frames for reuse
    UMC::Status UpdateRefPicMarking(ViewItem_H265 &view, const H265Slice * pSlice);

    void Reset();

    Ipp32u  GetDPBError() const;

protected:

    Ipp32u  m_isDPBErrorFound;
    Ipp32s  m_frameCount;

    void ResetError();
};

/****************************************************************************************************/
// Prepare data for asychronous processing
/****************************************************************************************************/
class TaskSupplier_H265 : public Skipping_H265, public AU_Splitter_H265, public MVC_Extension, public DecReferencePictureMarking_H265
{
    friend class TaskBroker_H265;
    friend class TaskBrokerSingleThreadDXVA;

public:
    Ipp32u m_SliceIdxInTaskSupplier; //for h265 sliceidx cursliceidx m_sliceidx m_currsliceidx m_inumber

    TaskSupplier_H265();
    virtual ~TaskSupplier_H265();

    // Initialize task supplier and creak task broker
    virtual UMC::Status Init(UMC::VideoDecoderParams *pInit);

    // create broker and segment decoders
    virtual void CreateTaskBroker();

    // Initialize what is necessary to decode bitstream header before the main part is initialized
    virtual UMC::Status PreInit(UMC::VideoDecoderParams *pInit);

    // Reset to default state
    virtual void Reset();
    // Release allocated resources
    virtual void Close();

    // Fill up current bitstream information
    UMC::Status GetInfo(UMC::VideoDecoderParams *lpInfo);

    // Add a new bitstream data buffer to decoding
    virtual UMC::Status AddSource(UMC::MediaData * pSource);

#if (defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)) && !defined(MFX_PROTECTED_FEATURE_DISABLE)
    // Add a new Widevine buffer to decoding
    virtual UMC::Status AddSource(DecryptParametersWrapper* pDecryptParams) {pDecryptParams; return MFX_ERR_UNSUPPORTED;}
#endif

    // Chose appropriate processing action for specified NAL unit
    UMC::Status ProcessNalUnit(UMC::MediaDataEx *nalUnit);

    void SetMemoryAllocator(UMC::MemoryAllocator *pMemoryAllocator)
    {
        m_pMemoryAllocator = pMemoryAllocator;
    }

    void SetFrameAllocator(UMC::FrameAllocator *pFrameAllocator)
    {
        m_pFrameAllocator = pFrameAllocator;
    }

    // Find a next frame ready to be output from decoder
    virtual H265DecoderFrame *GetFrameToDisplayInternal(bool force);

    // Retrieve decoded SEI data with SEI_USER_DATA_REGISTERED_TYPE type
    UMC::Status GetUserData(UMC::MediaData * pUD);

    bool IsShouldSuspendDisplay();

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

    // Start asynchronous decoding
    virtual UMC::Status RunDecoding();
    // Find a decoder frame instance with specified surface ID
    virtual H265DecoderFrame * FindSurface(UMC::FrameMemID id);

    // Set frame display time
    void PostProcessDisplayFrame(H265DecoderFrame *pFrame);

    // Attempt to recover after something unexpectedly went wrong
    virtual void AfterErrorRestore();

    SEI_Storer_H265 * GetSEIStorer() const { return m_sei_messages;}

    Headers * GetHeaders() { return &m_Headers;}

    inline const H265SeqParamSet *GetCurrentSequence(void) const
    {
        return m_Headers.m_SeqParams.GetCurrentHeader();
    }

    // Decode slice header start, set slice links to SPS and PPS and correct tile offsets table if needed
    virtual H265Slice * DecodeSliceHeader(UMC::MediaDataEx *nalUnit);

    Heap_Objects * GetObjHeap()
    {
        return &m_ObjHeap;
    }

protected:

    // Include a new slice into a set of frame slices
    void AddSliceToFrame(H265DecoderFrame *pFrame, H265Slice *pSlice);

    // Initialize scaling list data if needed
    void ActivateHeaders(H265SeqParamSet *sps, H265PicParamSet *pps);

    // Check whether this slice should be skipped because of random access conditions. HEVC spec 3.111
    bool IsSkipForCRAorBLA(const H265Slice *pSlice);

    // Calculate NoRaslOutputFlag flag for specified slice
    void CheckCRAOrBLA(const H265Slice *pSlice);

    // Try to find a reusable frame or allocate a new one and initialize it with slice parameters
    virtual H265DecoderFrame *AllocateNewFrame(const H265Slice *pSlice);
    // Initialize just allocated frame with slice parameters
    virtual UMC::Status InitFreeFrame(H265DecoderFrame *pFrame, const H265Slice *pSlice);
    // Initialize frame's counter and corresponding parameters
    virtual void InitFrameCounter(H265DecoderFrame *pFrame, const H265Slice *pSlice);

    // Add a new slice to frame
    UMC::Status AddSlice(H265Slice * pSlice, bool force);
    // Check whether all slices for the frame were found
    virtual void CompleteFrame(H265DecoderFrame * pFrame);
    // Mark frame as full with slices
    virtual void OnFullFrame(H265DecoderFrame * pFrame);

    // Update DPB contents marking frames for reuse
    void DPBUpdate(const H265Slice * slice);

    // Not implemented
    virtual void AddFakeReferenceFrame(H265Slice * pSlice);

    // Find NAL units in new bitstream buffer and process them
    virtual UMC::Status AddOneFrame(UMC::MediaData * pSource);

    // Allocate frame internals
    virtual UMC::Status AllocateFrameData(H265DecoderFrame * pFrame, IppiSize dimensions, const H265SeqParamSet* pSeqParamSet, const H265PicParamSet *pPicParamSet);

    // Decode a bitstream header NAL unit
    virtual UMC::Status DecodeHeaders(UMC::MediaDataEx *nalUnit);
    // Decode SEI NAL unit
    virtual UMC::Status DecodeSEI(UMC::MediaDataEx *nalUnit);

    // Search DPB for a frame which may be reused
    virtual H265DecoderFrame *GetFreeFrame();

    // If a frame has all slices found, add it to asynchronous decode queue
    UMC::Status CompleteDecodedFrames(H265DecoderFrame ** decoded);

    // Try to reset in case DPB has overflown
    void PreventDPBFullness();

    H265SegmentDecoderBase **m_pSegmentDecoder;
    Ipp32u m_iThreadNum;

    Ipp32s      m_maxUIDWhenWasDisplayed;
    Ipp64f      m_local_delta_frame_time;
    bool        m_use_external_framerate;
    bool        m_decodedOrder;
    bool        m_checkCRAInsideResetProcess;

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

    Ipp32s m_UIDFrameCounter;

    H265SEIPayLoad m_UserData;
    SEI_Storer_H265 *m_sei_messages;

    PocDecoding m_pocDecoding;

    bool m_isInitialized;

    UMC::Mutex m_mGuard;

private:
    // Decode video parameters set NAL unit
    UMC::Status xDecodeVPS(H265HeadersBitstream *);
    // Decode sequence parameters set NAL unit
    UMC::Status xDecodeSPS(H265HeadersBitstream *);
    // Decode picture parameters set NAL unit
    UMC::Status xDecodePPS(H265HeadersBitstream *);

    TaskSupplier_H265 & operator = (TaskSupplier_H265 &)
    {
        return *this;

    } // TaskSupplier_H265 & operator = (TaskSupplier_H265 &)

};

// Calculate maximum DPB size based on level and resolution
extern Ipp32s __CDECL CalculateDPBSize(Ipp32u &level_idc, Ipp32s width, Ipp32s height, Ipp32u num_ref_frames);

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H265_TASK_SUPPLIER_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
