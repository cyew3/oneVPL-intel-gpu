// Copyright (c) 2003-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

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

typedef int16_t FactorArrayValue;

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
    void GetStateVariables(int32_t &iMBSkipFlag,  int32_t &iQuantPrev, int32_t &iPrevDQuant);
    // Save decoding state variables
    void SetStateVariables(int32_t iMBSkipCount, int32_t iQuantPrev, int32_t iPrevDQuant);

    // Obtain owning slice field index
    int32_t GetFieldIndex(void) const {return m_field_index;}

    // Update reference list
    virtual Status UpdateReferenceList(ViewList &views,
        int32_t dIdIndex);

    void InitDistScaleFactor(int32_t NumL0RefActive,
                                        int32_t NumL1RefActive,
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
    int32_t m_MVsDistortion;

    int32_t m_iCurMBToDec;                                       // (int32_t) current MB number to decode
    int32_t m_iCurMBToRec;                                       // (int32_t) current MB number to reconstruct
    int32_t m_iCurMBToDeb;                                       // (int32_t *) current MB number to de-blocking

    bool m_bInProcess;                                          // (bool) slice is under whole decoding
    int32_t m_bDecVacant;                                        // (int32_t) decoding is vacant
    int32_t m_bRecVacant;                                        // (int32_t) reconstruct is vacant
    int32_t m_bDebVacant;                                        // (int32_t) de-blocking is vacant

    // through-decoding variable(s)
    int32_t m_nMBSkipCount;                                      // (uint32_t) current count of skipped macro blocks
    int32_t m_nQuantPrev;                                        // (uint32_t) quantize value of previous macro block
    int32_t m_prev_dquant;
    int32_t m_field_index;                                       // (int32_t) current field index
    bool m_bNeedToCheckMBSliceEdges;                            // (bool) need to check inter-slice boundaries

    static FactorArrayAFF m_StaticFactorArrayAFF;
};

inline
void H264SliceEx::GetStateVariables(int32_t &iMBSkipFlag, int32_t &iQuantPrev, int32_t &iPrevDQuant)
{
    iMBSkipFlag = m_nMBSkipCount;
    iQuantPrev = m_nQuantPrev;
    iPrevDQuant = m_prev_dquant;

} // void H264Slice::GetStateVariables(int32_t &iMBSkipFlag, int32_t &iQuantPrev, bool &bSkipNextFDF, int32_t &iPrevDQuant)

inline
void H264SliceEx::SetStateVariables(int32_t iMBSkipFlag, int32_t iQuantPrev, int32_t iPrevDQuant)
{
    m_nMBSkipCount = iMBSkipFlag;
    m_nQuantPrev = iQuantPrev;
    m_prev_dquant = iPrevDQuant;

} // void H264Slice::SetStateVariables(int32_t iMBSkipFlag, int32_t iQuantPrev, bool bSkipNextFDF, int32_t iPrevDQuant)

// Declare function to swapping memory
extern void SwapMemoryAndRemovePreventingBytes(void *pDst, size_t &nDstSize, void *pSrc, size_t nSrcSize);

} // namespace UMC

#endif // __UMC_H264_SLICE_EX_H
#endif // MFX_ENABLE_H264_VIDEO_DECODE
