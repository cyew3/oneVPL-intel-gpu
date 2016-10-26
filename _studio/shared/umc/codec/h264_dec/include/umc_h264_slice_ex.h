//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_SLICE_EX_H
#define __UMC_H264_SLICE_EX_H

#include <list>
#include "umc_h264_slice_decoding.h"
#include "umc_h264_bitstream.h"
#include "umc_h264_dec_structures.h"

namespace UMC
{
class H264DecoderFrameEx;

// Slice decoding constant
enum
{
    MINIMUM_NUMBER_OF_ROWS      = 4,
    MAXIMUM_NUMBER_OF_ROWS      = 8,
    MB_BUFFER_SIZE              = 480,
    NUMBER_OF_PIECES            = 8,
    NUMBER_OF_DEBLOCKERS        = 2
};

// Task ID enumerator
enum
{
    // whole slice is processed
    TASK_PROCESS                = 0,
    // piece of slice is decoded
    TASK_DEC,
    // piece of future frame's slice is decoded
    TASK_REC,
    // whole slice is deblocked
    TASK_DEB_SLICE,
    // piece of slice is deblocked
    TASK_DEB,
    // piece of slice is deblocked by several threads
    TASK_DEB_THREADED,
    // whole frame is deblocked (when there is the slice groups)
    TASK_DEB_FRAME,
    // whole frame is deblocked (when there is the slice groups)
    TASK_DEB_FRAME_THREADED,
    // //whole frame is deblocked (when there is the slice groups)
    TASK_DEC_REC
};

typedef Ipp16s FactorArrayValue;

struct FactorArray
{
    FactorArrayValue values[MAX_NUM_REF_FRAMES][MAX_NUM_REF_FRAMES];
};

struct FactorArrayMV
{
    FactorArrayValue values[MAX_NUM_REF_FRAMES];
};

struct FactorArrayAFF
{
    FactorArrayValue values[MAX_NUM_REF_FRAMES][2][2][2][MAX_NUM_REF_FRAMES];
};

struct FactorArrayMVAFF
{
    FactorArrayValue values[2][2][2][MAX_NUM_REF_FRAMES];
};

class H264MemoryPiece;
class H264DecoderFrame;
class H264DecoderFrameInfo;

struct ViewItem;
typedef std::list<ViewItem> ViewList;

class H264SliceEx : public H264Slice
{
public:
    H264SliceEx(MemoryAllocator *pMemoryAllocator = 0);

    virtual void Release();
    virtual void FreeResources();
    virtual void ZeroedMembers();

    virtual bool Reset(H264NalExtension *pNalExt);

    void InitializeContexts();

    // Need to check slice edges
    bool NeedToCheckSliceEdges(void) const {return m_bNeedToCheckMBSliceEdges;}
    // Do we can doing deblocking
    bool GetDeblockingCondition(void) const;
    // Obtain scale factor arrays
    FactorArray *GetDistScaleFactor(void){return &m_DistScaleFactor;}
    FactorArrayMV *GetDistScaleFactorMV(void){return &m_DistScaleFactorMV;}
    FactorArrayAFF *GetDistScaleFactorAFF(void){return m_DistScaleFactorAFF;}
    FactorArrayMVAFF *GetDistScaleFactorMVAFF(void){return m_DistScaleFactorMVAFF;}

    // Obtain local MB information
    H264DecoderLocalMacroblockDescriptor &GetMBInfo(void){return *m_mbinfo;}
    // Obtain pointer to MB intra types
    IntraType *GetMBIntraTypes(void){return m_pMBIntraTypes;}

    // Obtain decoding state variables
    void GetStateVariables(Ipp32s &iMBSkipFlag,  Ipp32s &iQuantPrev, Ipp32s &iPrevDQuant);
    // Save decoding state variables
    void SetStateVariables(Ipp32s iMBSkipCount, Ipp32s iQuantPrev, Ipp32s iPrevDQuant);

    // Obtain owning slice field index
    Ipp32s GetFieldIndex(void) const {return m_field_index;}

    // Update reference list
    virtual Status UpdateReferenceList(ViewList &views,
        Ipp32s dIdIndex);

    void InitDistScaleFactor(Ipp32s NumL0RefActive,
                                        Ipp32s NumL1RefActive,
                                        H264DecoderFrame **pRefPicList0,
                                        H264DecoderFrame **pRefPicList1,
                                        ReferenceFlags *pFields0,
                                        ReferenceFlags *pFields1);

    void CompleteDecoding();

    H264DecoderFrameEx *GetCurrentFrameEx(){return (H264DecoderFrameEx *)m_pCurrentFrame;}

public:
    FactorArray     m_DistScaleFactor;
    FactorArrayMV   m_DistScaleFactorMV;
    FactorArrayAFF  *m_DistScaleFactorAFF;                      // [curmb field],[ref1field],[ref0field]
    FactorArrayMVAFF *m_DistScaleFactorMVAFF;                   // [curmb field],[ref1field],[ref0field]

    H264CoeffsBuffer *GetCoeffsBuffers() const;

    H264CoeffsBuffer   *m_coeffsBuffers;

    H264DecoderLocalMacroblockDescriptor *m_mbinfo;             // (H264DecoderLocalMacroblockDescriptor*) current frame MB information
    IntraType *m_pMBIntraTypes;                                 // (IntraType *) array of macroblock intra types

    H264Bitstream   m_SliceDataBitStream;                           // (H264Bitstream) slice bit stream
    Ipp32s m_MVsDistortion;

    Ipp32s m_iCurMBToDec;                                       // (Ipp32s) current MB number to decode
    Ipp32s m_iCurMBToRec;                                       // (Ipp32s) current MB number to reconstruct
    Ipp32s m_iCurMBToDeb;                                       // (Ipp32s *) current MB number to de-blocking

    bool m_bInProcess;                                          // (bool) slice is under whole decoding
    Ipp32s m_bDecVacant;                                        // (Ipp32s) decoding is vacant
    Ipp32s m_bRecVacant;                                        // (Ipp32s) reconstruct is vacant
    Ipp32s m_bDebVacant;                                        // (Ipp32s) de-blocking is vacant

    // through-decoding variable(s)
    Ipp32s m_nMBSkipCount;                                      // (Ipp32u) current count of skipped macro blocks
    Ipp32s m_nQuantPrev;                                        // (Ipp32u) quantize value of previous macro block
    Ipp32s m_prev_dquant;
    Ipp32s m_field_index;                                       // (Ipp32s) current field index
    bool m_bNeedToCheckMBSliceEdges;                            // (bool) need to check inter-slice boundaries

    static FactorArrayAFF m_StaticFactorArrayAFF;
};

inline
void H264SliceEx::GetStateVariables(Ipp32s &iMBSkipFlag, Ipp32s &iQuantPrev, Ipp32s &iPrevDQuant)
{
    iMBSkipFlag = m_nMBSkipCount;
    iQuantPrev = m_nQuantPrev;
    iPrevDQuant = m_prev_dquant;

} // void H264Slice::GetStateVariables(Ipp32s &iMBSkipFlag, Ipp32s &iQuantPrev, bool &bSkipNextFDF, Ipp32s &iPrevDQuant)

inline
void H264SliceEx::SetStateVariables(Ipp32s iMBSkipFlag, Ipp32s iQuantPrev, Ipp32s iPrevDQuant)
{
    m_nMBSkipCount = iMBSkipFlag;
    m_nQuantPrev = iQuantPrev;
    m_prev_dquant = iPrevDQuant;

} // void H264Slice::SetStateVariables(Ipp32s iMBSkipFlag, Ipp32s iQuantPrev, bool bSkipNextFDF, Ipp32s iPrevDQuant)

// Declare function to swapping memory
extern void SwapMemoryAndRemovePreventingBytes(void *pDst, size_t &nDstSize, void *pSrc, size_t nSrcSize);

} // namespace UMC

#endif // __UMC_H264_SLICE_EX_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
