/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2012-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_SLICE_DECODING_H
#define __UMC_H265_SLICE_DECODING_H

#include <list>
#include "umc_h265_dec_defs.h"
#ifdef MFX_VA
#include "umc_h265_bitstream_headers.h"
#else
#include "umc_h265_bitstream.h"
#endif
#include "umc_automatic_mutex.h"
#include "umc_h265_heap.h"

namespace UMC_HEVC_DECODER
{
class H265DecoderFrame;
class H265DBPList;
class DecodingContext;

// Asynchronous task IDs
enum
{
    DEC_PROCESS_ID,
    REC_PROCESS_ID,
    DEB_PROCESS_ID,
    SAO_PROCESS_ID,

    LAST_PROCESS_ID
};

// Task completeness information structure
struct CUProcessInfo
{
    Ipp32s firstCU;
    Ipp32s maxCU;
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

        firstCU = firstCUAddr;
        m_width = width;
        m_isCompleted = false;
    }
};

// Slice descriptor class
class H265Slice : public HeapObject
{
public:
    // Default constructor
    H265Slice();
    // Destructor
    virtual
    ~H265Slice(void);

    // Decode slice header and initializ slice structure with parsed values
    virtual bool Reset(PocDecoding * pocDecoding);
    // Set current slice number
    void SetSliceNumber(Ipp32s iSliceNumber);

    // Initialize CABAC context depending on slice type
    void InitializeContexts();

    // Parse beginning of slice header to get PPS ID
    virtual Ipp32s RetrievePicParamSetNumber();

    //
    // method(s) to obtain slice specific information
    //

    // Obtain pointer to slice header
    const H265SliceHeader *GetSliceHeader() const {return &m_SliceHeader;}
    H265SliceHeader *GetSliceHeader() {return &m_SliceHeader;}
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
    virtual UMC::Status UpdateReferenceList(H265DBPList *dpb);

    bool IsError() const {return m_bError;}

public:

    MemoryPiece m_source;                                 // (MemoryPiece *) pointer to owning memory piece

public:  // DEBUG !!!! should remove dependence

    // Initialize slice structure to default values
    virtual void Reset();

    // Release resources
    virtual void Release();

    // Decoder slice header and calculate POC
    virtual bool DecodeSliceHeader(PocDecoding * pocDecoding);

    H265SliceHeader m_SliceHeader;                              // (H265SliceHeader) slice header

#ifdef MFX_VA
    H265HeadersBitstream m_BitStream;                                  // (H265Bitstream) slice bit stream

    // Obtain bit stream object
    H265HeadersBitstream *GetBitStream(){return &m_BitStream;}
#else
    H265Bitstream m_BitStream;                                  // (H265Bitstream) slice bit stream

    // Obtain bit stream object
    H265Bitstream *GetBitStream(){return &m_BitStream;}
#endif

protected:
    const H265PicParamSet* m_pPicParamSet;                      // (H265PicParamSet *) pointer to array of picture parameters sets
    const H265SeqParamSet* m_pSeqParamSet;                      // (H265SeqParamSet *) pointer to array of sequence parameters sets
public:
    H265DecoderFrame *m_pCurrentFrame;        // (H265DecoderFrame *) pointer to destination frame

    Ipp32s m_iNumber;                                           // (Ipp32s) current slice number
    Ipp32s m_iFirstMB;                                          // (Ipp32s) first MB number in slice
    Ipp32s m_iMaxMB;                                            // (Ipp32s) last unavailable  MB number in slice

    Ipp16u m_WidevineStatusReportNumber;

    CUProcessInfo processInfo;

    bool m_bError;                                              // (bool) there is an error in decoding

    // memory management tools
    DecodingContext        *m_context;

public:

    ReferencePictureSet*  getRPS() { return &m_SliceHeader.m_rps; }

    const ReferencePictureSet*  getRPS() const { return &GetSliceHeader()->m_rps; }

    int getNumRefIdx(EnumRefPicList e) const    { return m_SliceHeader.m_numRefIdx[e]; }

    // Returns number of used references in RPS
    int getNumRpsCurrTempList() const;

    Ipp32s m_tileCount;
    Ipp32u *m_tileByteLocation;

    Ipp32s getTileLocationCount() const   { return m_tileCount; }
    void allocateTileLocation(Ipp32s val)
    {
        if (m_tileCount < val)
            delete[] m_tileByteLocation;

        m_tileCount = val;
        m_tileByteLocation = new Ipp32u[val];
    }

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

// Returns true if slice is sublayer non-reference
inline
bool IsSubLayerNonReference(Ipp32s nal_unit_type)
{
    switch (nal_unit_type)
    {
    case NAL_UT_CODED_SLICE_RADL_N:
    case NAL_UT_CODED_SLICE_RASL_N:
    case NAL_UT_CODED_SLICE_STSA_N:
    case NAL_UT_CODED_SLICE_TSA_N:
    case NAL_UT_CODED_SLICE_TRAIL_N:
    //also need to add RSV_VCL_N10, RSV_VCL_N12, or RSV_VCL_N14,
        return true;
    }

    return false;

} // bool IsSubLayerNonReference(Ipp32s nal_unit_type)


} // namespace UMC_HEVC_DECODER

#endif // __UMC_H265_SLICE_DECODING_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
