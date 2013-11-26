/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_SLICE_DECODING_H
#define __UMC_H265_SLICE_DECODING_H

#include <list>
#include "umc_h265_dec_defs_dec.h"
#include "umc_h265_dec.h"
#include "umc_h265_bitstream.h"
#include "umc_automatic_mutex.h"
#include "umc_event.h"

#include "umc_h265_heap.h"
#include "umc_h265_segment_decoder_mt.h"

namespace UMC_HEVC_DECODER
{
class H265DBPList;
class H265DecoderFrameList;

enum
{
    DEC_PROCESS_ID,
    REC_PROCESS_ID,
    DEB_PROCESS_ID,
    SAO_PROCESS_ID,

    LAST_PROCESS_ID
};

// Task ID enumerator
enum
{
    // whole slice is processed
    TASK_PROCESS_H265                = 0,
    // piece of slice is decoded
    TASK_DEC_H265,
    // piece of future frame's slice is decoded
    TASK_REC_H265,
    // whole slice is deblocked
    TASK_DEB_SLICE_H265,
    // piece of slice is deblocked
    TASK_DEB_H265,
    // //whole frame is deblocked (when there is the slice groups)
    TASK_DEC_REC_H265,
    TASK_SAO_H265,
    TASK_SAO_FRAME_H265
};

struct H265RefListInfo
{
    Ipp32s m_iNumShortEntriesInList;
    Ipp32s m_iNumLongEntriesInList;
    Ipp32s m_iNumFramesInL0List;
    Ipp32s m_iNumFramesInL1List;
    Ipp32s m_iNumFramesInLTList;

    H265RefListInfo()
        : m_iNumShortEntriesInList(0)
        , m_iNumLongEntriesInList(0)
        , m_iNumFramesInL0List(0)
        , m_iNumFramesInL1List(0)
        , m_iNumFramesInLTList(0)
    {
    }
};

class H265Task;
class MemoryPiece;
class H265DecoderFrame;
class H265DecoderFrameInfo;

class H265Slice : public HeapObject
{
    // It is OK. H264SliceStore is owner of H265Slice object.
    // He can do what he wants.
    friend class H265SegmentDecoderMultiThreaded;
    friend class TaskBroker_H265;
    friend class TaskBrokerTwoThread_H265;
    friend class H265DecoderFrameInfo;
    friend void PrintInfoStatus(H265DecoderFrameInfo * info);

public:
    // Default constructor
    H265Slice(UMC::MemoryAllocator *pMemoryAllocator = 0);
    // Destructor
    virtual
    ~H265Slice(void);

    // Initialize slice
    bool Init(Ipp32s iConsumerNumber);
    // Set slice source data
    bool Reset(void *pSource, size_t nSourceSize, PocDecoding * pocDecoding);
    // Set current slice number
    void SetSliceNumber(Ipp32s iSliceNumber);

    void InitializeContexts();

    Ipp32s RetrievePicParamSetNumber(void *pSource, size_t nSourceSize);

    //
    // method(s) to obtain slice specific information
    //

    // Obtain pointer to slice header
    const H265SliceHeader *GetSliceHeader(void) const {return &m_SliceHeader;}
    H265SliceHeader *GetSliceHeader(void) {return &m_SliceHeader;}
    // Obtain bit stream object
    H265Bitstream *GetBitStream(void){return &m_BitStream;}
    Ipp32s GetFirstMB(void) const {return m_iFirstMB;}
    void SetFirstMBNumber(Ipp32s x) {m_iFirstMB = x;}
    // Obtain current picture parameter set
    const H265PicParamSet *GetPicParam(void) const {return m_pPicParamSet;}
    void SetPicParam(const H265PicParamSet * pps)
    {
        m_pPicParamSet = pps;
        if (m_pPicParamSet)
            m_pPicParamSet->IncrementReference();
    }
    // Obtain current sequence parameter set
    const H265SeqParamSet *GetSeqParam(void) const {return m_pSeqParamSet;}
    void SetSeqParam(const H265SeqParamSet * sps)
    {
        m_pSeqParamSet = sps;
        if (m_pSeqParamSet)
            m_pSeqParamSet->IncrementReference();
    }

    // Obtain current destination frame
    H265DecoderFrame *GetCurrentFrame(void) const {return m_pCurrentFrame;}
    void SetCurrentFrame(H265DecoderFrame * pFrame){m_pCurrentFrame = pFrame;}

    // Obtain slice number
    Ipp32s GetSliceNum(void) const {return m_iNumber;}
    // Obtain maximum of macroblock
    Ipp32s GetMaxMB(void) const {return m_iMaxMB;}
    void SetMaxMB(Ipp32s x) {m_iMaxMB = x;}

    Ipp32s GetMBCount() const { return m_iMaxMB - m_iFirstMB;}

    // Update reference list
    UMC::Status UpdateReferenceList(H265DBPList *dpb);

    bool IsError() const {return m_bError;}

    void SetHeap(Heap_Objects  *pObjHeap, Heap *pHeap)
    {
        m_pObjHeap = pObjHeap;
        m_pHeap = pHeap;
    }

public:

    MemoryPiece *m_pSource;                                 // (MemoryPiece *) pointer to owning memory piece
    Ipp64f m_dTime;                                             // (Ipp64f) slice's time stamp

public:  // DEBUG !!!! should remove dependence

    void Reset();

    void Release();

    // Decode slice header
    bool DecodeSliceHeader(PocDecoding * pocDecoding);

    H265SliceHeader m_SliceHeader;                              // (H265SliceHeader) slice header
    H265Bitstream m_BitStream;                                  // (H265Bitstream) slice bit stream

private:
    const H265PicParamSet* m_pPicParamSet;                      // (H265PicParamSet *) pointer to array of picture parameters sets
    const H265SeqParamSet* m_pSeqParamSet;                      // (H265SeqParamSet *) pointer to array of sequence parameters sets
public:
    H265DecoderFrame *m_pCurrentFrame;        // (H265DecoderFrame *) pointer to destination frame

    Ipp32s m_iNumber;                                           // (Ipp32s) current slice number
    Ipp32s m_iFirstMB;                                          // (Ipp32s) first MB number in slice
    Ipp32s m_iMaxMB;                                            // (Ipp32s) last unavailable  MB number in slice

    Ipp32s m_iAvailableMB;                                      // (Ipp32s) available number of macroblocks (used in "unknown mode")

    Ipp32s m_mvsDistortion;

    Ipp32s m_curTileRec;                                          // (Ipp32s) current MB number to reconstruct
    Ipp32s m_curTileDec;                                          // (Ipp32s) current MB number to reconstruct

    Ipp32s m_curMBToProcess[LAST_PROCESS_ID];

    bool m_bInProcess;                                          // (bool) slice is under whole decoding
    Ipp32s m_processVacant[LAST_PROCESS_ID];
    bool m_bError;                                              // (bool) there is an error in decoding

    bool m_bDecoded;                                            // (bool) "slice has been decoded" flag
    bool m_bPrevDeblocked;                                      // (bool) "previous slice has been deblocked" flag
    bool m_bDeblocked;                                          // (bool) "slice has been deblocked" flag
    bool m_bSAOed;

    // memory management tools
    UMC::MemoryAllocator *m_pMemoryAllocator;                        // (MemoryAllocator *) pointer to memory allocation tool

    Heap_Objects           *m_pObjHeap;
    Heap                   *m_pHeap;

    DecodingContext        *m_context;

public:

    ReferencePictureSet*  getRPS() const    { return m_SliceHeader.m_pRPS; }
    void setRPS(ReferencePictureSet *rps)   { m_SliceHeader.m_pRPS = rps; }
    ReferencePictureSet*  getLocalRPS()     { return &m_SliceHeader.m_localRPS; }

    int getNumRefIdx(EnumRefPicList e) const    { return m_SliceHeader.m_numRefIdx[e]; }

    int getNumRpsCurrTempList() const;

    Ipp32s getTileLocationCount() const   { return m_SliceHeader.m_TileCount; }
    void setTileLocationCount(Ipp32s val)
    {
        m_SliceHeader.m_TileCount = val;

        if (NULL != m_SliceHeader.m_TileByteLocation)
            delete[] m_SliceHeader.m_TileByteLocation;

        m_SliceHeader.m_TileByteLocation = new Ipp32u[val];
    }

    void allocSubstreamSizes(unsigned);
    void setRefPOCList();
    void CopyFromBaseSlice(const H265Slice * slice);
};

inline
bool IsPictureTheSame(H265Slice *pSliceOne, H265Slice *pSliceTwo)
{
    if (!pSliceOne)
        return true;

    const H265SliceHeader *pOne = pSliceOne->GetSliceHeader();
    const H265SliceHeader *pTwo = pSliceTwo->GetSliceHeader();

    // this function checks two slices are from same picture or not
    // 7.4.2.4.5 of hevc standard

    if (pOne->first_slice_segment_in_pic_flag == 1 && pOne->first_slice_segment_in_pic_flag == pTwo->first_slice_segment_in_pic_flag)
        return false;

    if (/*(pOne->SliceCurStartCUAddr == pTwo->SliceCurStartCUAddr) ||
        (pOne->m_sliceSegmentCurStartCUAddr == pTwo->m_sliceSegmentCurStartCUAddr) ||
        (pOne->m_sliceAddr == pTwo->m_sliceAddr) ||*/
        (pOne->slice_pic_parameter_set_id != pTwo->slice_pic_parameter_set_id))
        return false;

    if (pOne->slice_pic_order_cnt_lsb != pTwo->slice_pic_order_cnt_lsb)
        return false;

    return true;

} // bool IsPictureTheSame(H265SliceHeader *pOne, H265SliceHeader *pTwo)

// Declaration of internal class(es)
class H265SegmentDecoder;
class H265SegmentDecoderMultiThreaded;

class H265Task
{
public:
    // Default constructor
    H265Task(Ipp32s iThreadNumber)
        : m_iThreadNumber(iThreadNumber)
    {
        m_pSlice = 0;

        pFunction = 0;
        m_pBuffer = 0;
        m_WrittenSize = 0;

        m_iFirstMB = -1;
        m_iMaxMB = -1;
        m_iMBToProcess = -1;
        m_iTaskID = 0;
        m_bDone = false;
        m_bError = false;
        m_mvsDistortion = 0;
        m_taskPreparingGuard = 0;
        m_context = 0;
    }

    UMC::Status (H265SegmentDecoderMultiThreaded::*pFunction)(H265Task &task);

    H265CoeffsPtrCommon m_pBuffer;                                  // (Ipp16s *) pointer to working buffer
    size_t          m_WrittenSize;

    DecodingContext * m_context;
    H265Slice *m_pSlice;                                        // (H265Slice *) pointer to owning slice
    H265DecoderFrameInfo * m_pSlicesInfo;
    UMC::AutomaticUMCMutex    * m_taskPreparingGuard;

    Ipp32s m_mvsDistortion;
    Ipp32s m_iThreadNumber;                                     // (Ipp32s) owning thread number
    Ipp32s m_iFirstMB;                                          // (Ipp32s) first MB in slice
    Ipp32s m_iMaxMB;                                            // (Ipp32s) maximum MB number in owning slice
    Ipp32s m_iMBToProcess;                                      // (Ipp32s) number of MB to processing
    Ipp32s m_iTaskID;                                           // (Ipp32s) task identificator
    bool m_bDone;                                               // (bool) task was done
    bool m_bError;                                              // (bool) there is a error
};

// Declare function to swapping memory
extern
void SwapMemoryAndRemovePreventingBytes_H265(void *pDst, size_t &nDstSize, void *pSrc, size_t nSrcSize, std::vector<Ipp32u> *pRemovedOffsets);

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H264_SLICE_DECODING_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
