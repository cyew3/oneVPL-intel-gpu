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
    TASK_SAO_H265
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
    friend class TaskSupplier_H265;
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
    bool Reset(void *pSource, size_t nSourceSize, Ipp32s iConsumerNumber);
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
    void SetPicParam(const H265PicParamSet * pps) {m_pPicParamSet = pps;}
    // Obtain current sequence parameter set
    const H265SeqParamSet *GetSeqParam(void) const {return m_pSeqParamSet;}
    void SetSeqParam(const H265SeqParamSet * sps) {m_pSeqParamSet = sps;}

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
    bool DecodeSliceHeader();

    H265SliceHeader m_SliceHeader;                              // (H265SliceHeader) slice header
    H265Bitstream m_BitStream;                                  // (H265Bitstream) slice bit stream

    const H265VideoParamSet *m_pVideoParamSet;
    const H265PicParamSet* m_pPicParamSet;                      // (H265PicParamSet *) pointer to array of picture parameters sets
    const H265SeqParamSet* m_pSeqParamSet;                      // (H265SeqParamSet *) pointer to array of sequence parameters sets

    H265DecoderFrame *m_pCurrentFrame;        // (H265DecoderFrame *) pointer to destination frame

    Ipp32s m_iNumber;                                           // (Ipp32s) current slice number
    Ipp32s m_iFirstMB;                                          // (Ipp32s) first MB number in slice
    Ipp32s m_iMaxMB;                                            // (Ipp32s) last unavailable  MB number in slice

    Ipp32s m_iAvailableMB;                                      // (Ipp32s) available number of macroblocks (used in "unknown mode")

    Ipp32s m_iCurMBToDec;                                       // (Ipp32s) current MB number to decode
    Ipp32s m_iCurMBToRec;                                       // (Ipp32s) current MB number to reconstruct
    Ipp32s m_iCurMBToDeb;                                       // (Ipp32s *) current MB number to de-blocking
    Ipp32s m_curTileRec;                                          // (Ipp32s) current MB number to reconstruct
    Ipp32s m_curTileDec;                                          // (Ipp32s) current MB number to reconstruct

    bool m_bInProcess;                                          // (bool) slice is under whole decoding
    Ipp32s m_bDecVacant;                                        // (Ipp32s) decoding is vacant
    Ipp32s m_bRecVacant;                                        // (Ipp32s) reconstruct is vacant
    Ipp32s m_bDebVacant;                                        // (Ipp32s) de-blocking is vacant
    bool m_bError;                                              // (bool) there is an error in decoding

    bool m_bDecoded;                                            // (bool) "slice has been decoded" flag
    bool m_bPrevDeblocked;                                      // (bool) "previous slice has been deblocked" flag
    bool m_bDeblocked;                                          // (bool) "slice has been deblocked" flag
    bool m_bSAOed;

    int m_LCU;  // used in h/w decoder

    // memory management tools
    UMC::MemoryAllocator *m_pMemoryAllocator;                        // (MemoryAllocator *) pointer to memory allocation tool

    Heap_Objects           *m_pObjHeap;
    Heap                   *m_pHeap;

    DecodingContext        *m_context;

    static int m_prevPOC;

public:

    // refcodec compatibility
    unsigned getSPSId() const   { return 0; }
    void setPPSId(Ipp32u val)     { m_SliceHeader.pic_parameter_set_id = (Ipp16u)val; }

    const H265PicParamSet *getPPS() const { return m_pPicParamSet; }
    void setPPS(const H265PicParamSet *pps)     { m_pPicParamSet = pps; }
    const H265SeqParamSet *getSPS() const { return m_pSeqParamSet; }
    void setSPS(const H265SeqParamSet *sps)     { m_pSeqParamSet = sps; }

    void setSliceCurStartCUAddr(unsigned val)           { m_SliceHeader.SliceCurStartCUAddr = val; }
    void setSliceCurEndCUAddr(unsigned val)             { m_SliceHeader.SliceCurEndCUAddr = val; }
    void setDependentSliceSegmentFlag(bool val)                { m_SliceHeader.m_DependentSliceSegmentFlag = val; }
    bool getDependentSliceSegmentFlag(void)                    { return m_SliceHeader.m_DependentSliceSegmentFlag; }
    void setSliceSegmentCurStartCUAddr    ( unsigned uiAddr )     { m_SliceHeader.m_sliceSegmentCurStartCUAddr = uiAddr;    }
    unsigned getSliceSegmentCurStartCUAddr    ()                  { return m_SliceHeader.m_sliceSegmentCurStartCUAddr;      }
    void setSliceSegmentCurEndCUAddr      ( unsigned uiAddr )     { m_SliceHeader.m_sliceSegmentCurEndCUAddr = uiAddr;      }
    unsigned getSliceSegmentCurEndCUAddr      ()                  { return m_SliceHeader.m_sliceSegmentCurEndCUAddr;        }
    bool isIntra() const                                { return  m_SliceHeader.slice_type == I_SLICE;  }
    bool isInterB() const                               { return  m_SliceHeader.slice_type == B_SLICE;  }
    bool isInterP() const                               { return  m_SliceHeader.slice_type == P_SLICE;  }
    SliceType getSliceType() const                      { return m_SliceHeader.slice_type; }
    void setSliceType(SliceType val)                    { m_SliceHeader.slice_type = val; }

    bool getPicOutputFlag() const           { return m_SliceHeader.pic_output_flag; }
    void setPicOutputFlag(bool f)           { m_SliceHeader.pic_output_flag = f; }
    bool getSaoEnabledFlag() const          { return  m_SliceHeader.m_SaoEnabledFlag; }
    void setSaoEnabledFlag(bool f)          { m_SliceHeader.m_SaoEnabledFlag = f; }
    bool getSaoEnabledFlagChroma() const        { return m_SliceHeader.m_SaoEnabledFlagChroma; }
    void setSaoEnabledFlagChroma(bool f)        { m_SliceHeader.m_SaoEnabledFlagChroma = f; }

    NalUnitType getNalUnitType() const              { return m_SliceHeader.nal_unit_type; }
    bool getIdrPicFlag()                            { return getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR || getNalUnitType() == NAL_UNIT_CODED_SLICE_IDR_N_LP; }

    unsigned getTLayer() const          { return m_SliceHeader.m_nuh_temporal_id;     }
    unsigned getMaxTLayers()            { return m_SliceHeader.m_uMaxTLayers; }
    void setMaxTLayers(unsigned val)    { /*VA_ASSERT( val <= MAX_TLAYER ); */m_SliceHeader.m_uMaxTLayers = val; }

    int getPOC() const  { return  m_SliceHeader.pic_order_cnt_lsb; }
    void setPOC (int i)
    {
        m_SliceHeader.pic_order_cnt_lsb = i;
        if (getTLayer() == 0)
            m_prevPOC = i;
    }

    int getPrevPOC() const                  { return  m_prevPOC; }

    ReferencePictureSet*  getRPS() const    { return m_SliceHeader.m_pRPS; }
    void setRPS(ReferencePictureSet *rps)   { m_SliceHeader.m_pRPS = rps; }
    ReferencePictureSet*  getLocalRPS()     { return &m_SliceHeader.m_localRPS; }

    void setBitsForPOC(unsigned val)    { m_SliceHeader.m_uBitsForPOC = val;      }
    unsigned getBitsForPOC() const      { return m_SliceHeader.m_uBitsForPOC;   }

    int getNumRefIdx(EnumRefPicList e) const    { return m_SliceHeader.m_numRefIdx[e]; }
    void setNumRefIdx(EnumRefPicList e, int i)  { m_SliceHeader.m_numRefIdx[e] = i; }

    bool getEnableTMVPFlag() const              { return m_SliceHeader.m_enableTMVPFlag; }
    void setEnableTMVPFlag(bool f)              { m_SliceHeader.m_enableTMVPFlag = f; }

    RefPicListModification* getRefPicListModification() { return &m_SliceHeader.m_RefPicListModification; }

    int getNumRpsCurrTempList() const;

    bool getMvdL1ZeroFlag() const   { return m_SliceHeader.m_MvdL1Zero; }
    void setMvdL1ZeroFlag(bool f)   { m_SliceHeader.m_MvdL1Zero = f; }
    bool getCabacInitFlag() const   { return m_SliceHeader.m_CabacInitFlag; }
    void setCabacInitFlag(bool f)   { m_SliceHeader.m_CabacInitFlag = f; }

    void setDeblockingFilterDisable( bool b )                { m_SliceHeader.m_deblockingFilterDisable = b;      }
    bool getDeblockingFilterDisable() const    { return m_SliceHeader.m_deblockingFilterDisable; }
    void setDeblockingFilterOverrideFlag( bool b )           { m_SliceHeader.m_deblockingFilterOverrideFlag = b; }
    bool getDeblockingFilterOverrideFlag()           { return m_SliceHeader.m_deblockingFilterOverrideFlag; }
    void setDeblockingFilterBetaOffsetDiv2( int i )          { m_SliceHeader.m_deblockingFilterBetaOffsetDiv2 = i; }
    int getLoopFilterBetaOffset() const        { return m_SliceHeader.m_deblockingFilterBetaOffsetDiv2; }
    void setDeblockingFilterTcOffsetDiv2( int i )            { m_SliceHeader.m_deblockingFilterTcOffsetDiv2 = i; }
    int getLoopFilterTcOffset() const          { return m_SliceHeader.m_deblockingFilterTcOffsetDiv2; }

    unsigned getColFromL0Flag() const      { return m_SliceHeader.collocated_from_l0_flag; }
    void setColFromL0Flag(unsigned val)    { m_SliceHeader.collocated_from_l0_flag = val; }
    unsigned getColRefIdx() const   { return m_SliceHeader.m_ColRefIdx; }
    void setColRefIdx(unsigned val) { m_SliceHeader.m_ColRefIdx = val; }

    void initWpScaling()    { initWpScaling(m_SliceHeader.m_weightPredTable); }
    void initWpScaling(wpScalingParam  wp[2][MAX_NUM_REF][3]);
    void getWpScaling(EnumRefPicList e, int iRefIdx, wpScalingParam *&wp) { wp = m_SliceHeader.m_weightPredTable[e][iRefIdx]; }
    void getWpScaling(int e, int iRefIdx, wpScalingParam *&wp) { wp = m_SliceHeader.m_weightPredTable[e][iRefIdx]; }

    unsigned getMaxNumMergeCand() const     { return m_SliceHeader.m_MaxNumMergeCand; }
    void setMaxNumMergeCand(unsigned val)   { m_SliceHeader.m_MaxNumMergeCand = val; }

    bool getLFCrossSliceBoundaryFlag() const    { return m_SliceHeader.m_LFCrossSliceBoundaryFlag;}
    void setLFCrossSliceBoundaryFlag(bool val)  { m_SliceHeader.m_LFCrossSliceBoundaryFlag = val; }

    int  getNumEntryPointOffsets() const   { return m_SliceHeader.m_numEntryPointOffsets; }
    void setNumEntryPointOffsets(int val)  { m_SliceHeader.m_numEntryPointOffsets = val;  }

    Ipp32s getTileLocationCount() const   { return m_SliceHeader.m_TileCount; }
    void setTileLocationCount(Ipp32s val)
    {
        m_SliceHeader.m_TileCount = val;

        if (NULL != m_SliceHeader.m_TileByteLocation)
            delete[] m_SliceHeader.m_TileByteLocation;

        m_SliceHeader.m_TileByteLocation = new Ipp32u[val];
    }

    void setTileLocation(int i, unsigned uiLOC) { m_SliceHeader.m_TileByteLocation[i] = uiLOC; }
    unsigned getTileLocation(int i)             { return m_SliceHeader.m_TileByteLocation[i];  }

    void allocSubstreamSizes(unsigned);
    unsigned* getSubstreamSizes() const     { return m_SliceHeader.m_SubstreamSizes; }

    void setSliceAddr(unsigned val)         { m_SliceHeader.m_sliceAddr = val; }
    unsigned getSliceAddr() const           { return m_SliceHeader.m_sliceAddr; }

    unsigned getRPSIndex() const            { return m_SliceHeader.m_RPSIndex; }
    void setRPSIndex(unsigned val)          { m_SliceHeader.m_RPSIndex = val; }

    int getLCU() const      { return m_LCU; }
    void setLCU(int val)    { m_LCU = val; }

    unsigned getLog2WeightDenomLuma() const     { return m_SliceHeader.m_uiLog2WeightDenomLuma; }
    void setLog2WeightDenomLuma(unsigned val)   { m_SliceHeader.m_uiLog2WeightDenomLuma = val; }
    unsigned getLog2WeightDenomChroma() const   { return m_SliceHeader.m_uiLog2WeightDenomChroma; }
    void setLog2WeightDenomChroma(unsigned val) { m_SliceHeader.m_uiLog2WeightDenomChroma = val; }

    int getRefPOC(EnumRefPicList list, int index) const { return m_SliceHeader.RefPOCList[list][index]; }
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
        (pOne->pic_parameter_set_id != pTwo->pic_parameter_set_id))
        return false;

    if (pOne->pic_order_cnt_lsb != pTwo->pic_order_cnt_lsb)
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
    }

    UMC::Status (H265SegmentDecoderMultiThreaded::*pFunction)(Ipp32s nCurMBNumber, Ipp32s &nMaxMBNumber); // (Status (*) (Ipp32s, Ipp32s &)) pointer to working function

    H265CoeffsPtrCommon m_pBuffer;                                  // (Ipp16s *) pointer to working buffer
    size_t          m_WrittenSize;

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
