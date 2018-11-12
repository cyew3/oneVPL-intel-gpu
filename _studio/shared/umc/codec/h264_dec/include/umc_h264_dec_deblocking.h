// Copyright (c) 2003-2018 Intel Corporation
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
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_DEC_DEBLOCKING_H
#define __UMC_H264_DEC_DEBLOCKING_H

#include "umc_h264_dec_defs_dec.h"

namespace UMC
{

#if defined(_MSC_VER) && !defined(_WIN32_WCE)
#define __align(value) __declspec(align(value))
#else // !defined(_MSC_VER)
#define __align(value)
#endif // defined(_MSC_VER)

#define IClip(Min, Max, Val) (((Val) < (Min)) ? (Min) : (((Val) > (Max)) ? (Max) : (Val)))
#define SetEdgeStrength(edge, strength) {                                               \
    uint32_t val = (((((strength) * 256) + strength) * 256 + strength) * 256 + strength); \
    memcpy_s(edge, sizeof(uint32_t), &val, sizeof(uint32_t));                               \
}
#define CopyEdgeStrength(dst_edge, src_edge)  \
    memcpy_s(dst_edge, sizeof(uint32_t), src_edge, sizeof(uint32_t))
#define CompareEdgeStrength(strength, edge) \
    ((((((strength) * 256) + strength) * 256 + strength) * 256 + strength) == *((uint32_t *) (edge)))

// declare used types and constants

enum
{
    VERTICAL_DEBLOCKING     = 0,
    HORIZONTAL_DEBLOCKING   = 1,
    NUMBER_OF_DIRECTION     = 2
};

enum
{
    CURRENT_BLOCK           = 0,
    NEIGHBOUR_BLOCK         = 1
};

extern
uint8_t VERTICAL_DEBLOCKING_BLOCKS_MAP[16];

// alpha table
extern
uint8_t ALPHA_TABLE[52];

// beta table
extern
uint8_t BETA_TABLE[52];

// clipping table
extern
uint8_t CLIP_TAB[52][5];

// chroma scaling QP table
extern
uint8_t QP_SCALE_CR[52];

// masks for external blocks pair "coded bits"
extern
uint32_t EXTERNAL_BLOCK_MASK[NUMBER_OF_DIRECTION][2][4];

// masks for internal blocks pair "coded bits"
extern
uint32_t INTERNAL_BLOCKS_MASK[NUMBER_OF_DIRECTION][12];

#pragma pack(push, 16)

// turn off "non-virtual destructor" remark
#ifdef _MSVC_LANG
#pragma warning(disable : 444)
#endif

typedef struct DeblockingParameters
{
    uint8_t  Strength[NUMBER_OF_DIRECTION][16];                   // (uint8_t [][]) arrays of deblocking sthrengths
    int32_t DeblockingFlag[NUMBER_OF_DIRECTION];                 // (uint32_t []) flags to do deblocking
    int32_t ExternalEdgeFlag[NUMBER_OF_DIRECTION];               // (uint32_t []) flags to do deblocking on external edges
    int32_t nMaxMVector;                                         // (uint32_t) maximum vertical motion vector
    int32_t nNeighbour[NUMBER_OF_DIRECTION];                     // (uint32_t) neighbour macroblock addres
    size_t iReferences[4][2];                                   // (int32_t [][]) exact references for deblocking
    int32_t iReferenceNum[4];                                    // (int32_t []) number of valid exact references
    int32_t MBFieldCoded;                                        // (uint32_t) flag means macroblock is field coded (picture may not)
    int32_t nAlphaC0Offset;                                      // (int32_t) alpha c0 offset
    int32_t nBetaOffset;                                         // (int32_t) beta offset
    PlanePtrYCommon pLuma;                                      // (uint8_t *) pointer to luminance data
    PlanePtrUVCommon pChroma;                                   // (uint8_t *) pointer to chrominance data

    int32_t m_isSameSlice;

    int32_t pitch_luma;
    int32_t pitch_chroma;
} DeblockingParameters;

typedef struct DeblockingParametersMBAFF : public DeblockingParameters
{
    uint8_t  StrengthComplex[16];                                 // (uint8_t) arrays of deblocking sthrengths
    uint8_t  StrengthExtra[16];                                   // (uint8_t) arrays of deblocking sthrengths
    int32_t UseComplexVerticalDeblocking;                        // (uint32_t) flag to do complex deblocking on external vertical edge
    int32_t ExtraHorizontalEdge;                                 // (uint32_t) flag to do deblocking on extra horizontal edge
    int32_t nLeft[2];                                            // (uint32_t []) left couple macroblock numbers

} DeblockingParametersMBAFF;

// restore "non-virtual destructor" remark
#ifdef _MSVC_LANG
#pragma warning(default : 444)
#endif

#pragma pack(pop)

} // namespace UMC

#endif // __UMC_H264_DEC_DEBLOCKING_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
