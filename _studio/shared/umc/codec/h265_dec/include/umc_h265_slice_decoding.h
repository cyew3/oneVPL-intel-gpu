/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
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

// Asynchronous task IDs
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
    TASK_PROCESS_H265  = 0, // whole slice is decoded and reconstructed
    TASK_DEC_H265, // piece of slice is decoded
    TASK_REC_H265, // piece of slice is reconstructed
    TASK_DEB_H265, // piece of slice is deblocked
    TASK_DEC_REC_H265, // piece of slice is decoded and reconstructed
    TASK_SAO_H265  // piece of slice is saoed
};

class H265Task;
class MemoryPiece;
class H265DecoderFrame;
class H265DecoderFrameInfo;

// Task completeness information structure
struct CUProcessInfo
{
    Ipp32s m_curCUToProcess[LAST_PROCESS_ID];
    Ipp32s m_processInProgress[LAST_PROCESS_ID];
    bool m_isCompleted;
    Ipp32s m_width;

    void Initialize(Ipp32s firstCUAddr, Ipp32s width)
    {
        for (Ipp32s task = 0; task < LAST_PROCESS_ID; task++)
        {
            m_curCUToProcess[task] = firstCUAddr;
            m_processInProgress[task] = 0;
        }

        m_width = width;
        m_isCompleted = false;
    }
};

// Slice descriptor class
class H265Slice : public HeapObject
{
    friend class H265SegmentDecoderMultiThreaded;
    friend class TaskBroker_H265;
    friend class TaskBrokerTwoThread_H265;
    friend class H265DecoderFrameInfo;
    friend void PrintInfoStatus(H265DecoderFrameInfo * info);

public:
    // Default constructor
    H265Slice();
    // Destructor
    virtual
    ~H265Slice(void);

    // Decode slice header and initializ slice structure with parsed values
    bool Reset(PocDecoding * pocDecoding);
    // Set current slice number
    void SetSliceNumber(Ipp32s iSliceNumber);

    // Initialize CABAC context depending on slice type
    void InitializeContexts();

    // Parse beginning of slice header to get PPS ID
    Ipp32s RetrievePicParamSetNumber();

    //
    // method(s) to obtain slice specific information
    //

    // Obtain pointer to slice header
    const H265SliceHeader *GetSliceHeader() const {return &m_SliceHeader;}
    H265SliceHeader *GetSliceHeader() {return &m_SliceHeader;}
    // Obtain bit stream object
    H265Bitstream *GetBitStream(){return &m_BitStream;}
    Ipp32s GetFirstMB() const {return m_iFirstMB;}
    // Obtain current picture parameter set
    const H265PicParamSet *GetPicParam() const {return m_pPicParamSet;}
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

    // Build reference lists from slice reference pic set. HEVC spec 8.3.2
    UMC::Status UpdateReferenceList(H265DBPList *dpb);

    bool IsError() const {return m_bError;}

public:

    MemoryPiece m_source;                                 // (MemoryPiece *) pointer to owning memory piece

public:  // DEBUG !!!! should remove dependence

    // Initialize slice structure to default values
    void Reset();

    // Release resources
    void Release();

    // Decoder slice header and calculate POC
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

    CUProcessInfo processInfo;

    bool m_bError;                                              // (bool) there is an error in decoding

    bool m_bDeblocked;                                          // (bool) "slice has been deblocked" flag
    bool m_bSAOed;

    // memory management tools
    DecodingContext        *m_context;

public:

    ReferencePictureSet*  getRPS() const    { return m_SliceHeader.m_pRPS; }
    void setRPS(ReferencePictureSet *rps)   { m_SliceHeader.m_pRPS = rps; }
    ReferencePictureSet*  getLocalRPS()     { return &m_SliceHeader.m_localRPS; }

    int getNumRefIdx(EnumRefPicList e) const    { return m_SliceHeader.m_numRefIdx[e]; }

    // Returns number of used references in RPS
    int getNumRpsCurrTempList() const;

    Ipp32s getTileLocationCount() const   { return m_SliceHeader.m_TileCount; }
    void setTileLocationCount(Ipp32s val)
    {
        m_SliceHeader.m_TileCount = val;

        if (NULL != m_SliceHeader.m_TileByteLocation)
            delete[] m_SliceHeader.m_TileByteLocation;

        m_SliceHeader.m_TileByteLocation = new Ipp32u[val];
    }

    // Allocate a temporary array to hold slice substream offsets
    void allocSubstreamSizes(unsigned);
    // For dependent slice copy data from another slice
    void CopyFromBaseSlice(const H265Slice * slice);
};

// Check whether two slices are from the same picture. HEVC spec 7.4.2.4.5
inline
bool IsPictureTheSame(H265Slice *pSliceOne, H265Slice *pSliceTwo)
{
    if (!pSliceOne)
        return true;

    const H265SliceHeader *pOne = pSliceOne->GetSliceHeader();
    const H265SliceHeader *pTwo = pSliceTwo->GetSliceHeader();

    if (pOne->first_slice_segment_in_pic_flag == 1 && pOne->first_slice_segment_in_pic_flag == pTwo->first_slice_segment_in_pic_flag)
        return false;

    if (pOne->slice_pic_parameter_set_id != pTwo->slice_pic_parameter_set_id)
        return false;

    if (pOne->slice_pic_order_cnt_lsb != pTwo->slice_pic_order_cnt_lsb)
        return false;

    return true;

} // bool IsPictureTheSame(H265SliceHeader *pOne, H265SliceHeader *pTwo)

// Declaration of internal class(es)
class H265SegmentDecoderMultiThreaded;
struct TileThreadingInfo;

// Asynchronous task descriptor class
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
        m_taskPreparingGuard = 0;
        m_context = 0;
        m_threadingInfo = 0;
    }

    UMC::Status (H265SegmentDecoderMultiThreaded::*pFunction)(H265Task &task);

    H265CoeffsPtrCommon m_pBuffer;                                  // (Ipp16s *) pointer to working buffer
    size_t          m_WrittenSize;

    DecodingContext * m_context;
    H265Slice *m_pSlice;                                        // (H265Slice *) pointer to owning slice
    TileThreadingInfo * m_threadingInfo;
    H265DecoderFrameInfo * m_pSlicesInfo;
    UMC::AutomaticUMCMutex    * m_taskPreparingGuard;

    Ipp32s m_iThreadNumber;                                     // (Ipp32s) owning thread number
    Ipp32s m_iFirstMB;                                          // (Ipp32s) first MB in slice
    Ipp32s m_iMaxMB;                                            // (Ipp32s) maximum MB number in owning slice
    Ipp32s m_iMBToProcess;                                      // (Ipp32s) number of MB to processing
    Ipp32s m_iTaskID;                                           // (Ipp32s) task identificator
    bool m_bDone;                                               // (bool) task was done
    bool m_bError;                                              // (bool) there is a error
};

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H265_SLICE_DECODING_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
