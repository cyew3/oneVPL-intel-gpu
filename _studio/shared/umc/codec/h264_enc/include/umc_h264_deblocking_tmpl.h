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

#if PIXBITS == 8

#define PIXTYPE Ipp8u
#define COEFFSTYPE Ipp16s
#define H264ENC_MAKE_NAME(NAME) NAME##_8u16s

#elif PIXBITS == 16

#define PIXTYPE Ipp16u
#define COEFFSTYPE Ipp32s
#define H264ENC_MAKE_NAME(NAME) NAME##_16u32s

#else

void H264EncoderFakeFunction() { UNSUPPORTED_PIXBITS; }

#endif //PIXBITS

#define DeblockingParametersType H264ENC_MAKE_NAME(DeblockingParameters)
#define DeblockingParametersMBAFFType H264ENC_MAKE_NAME(DeblockingParametersMBAFF)

#pragma pack(16)

typedef struct H264ENC_MAKE_NAME(sDeblockingParameters)
{
    Ipp8u    Strength[NUMBER_OF_DIRECTION][16];                   // (PixType [][]) arrays of deblocking sthrengths
    Ipp32u   DeblockingFlag[NUMBER_OF_DIRECTION];                 // (Ipp32s []) flags to do deblocking
    Ipp32u   ExternalEdgeFlag[NUMBER_OF_DIRECTION];               // (Ipp32s []) flags to do deblocking on external edges
    Ipp32s   nMBAddr;                                             // (Ipp32s) macroblock number
    Ipp32s   nMaxMVector;                                         // (Ipp32s) maximum vertical motion vector
    Ipp32s   nNeighbour[NUMBER_OF_DIRECTION];                     // (Ipp32s) neighbour macroblock addres
    Ipp32s   MBFieldCoded;                                        // (Ipp32s) flag means macroblock is field coded (picture may not)
    Ipp32s   nAlphaC0Offset;                                      // (Ipp32s) alpha c0 offset
    Ipp32s   nBetaOffset;                                         // (Ipp32s) beta offset
    PIXTYPE *pY;                                                  // (PixType *) pointer to Y data
    PIXTYPE *pU;                                                  // (PixType *) pointer to U data
    PIXTYPE *pV;                                                  // (PixType *) pointer to V data
    Ipp32s   pitchPixels;                                         // (Ipp32s) working pitch in pixels

    Ipp32s   deblockChroma;
    Ipp32s   isNeedToDeblock;
    Ipp32s   deblockEdgesStage2;
    Ipp32s   is_profile_baseline_scalable;
} DeblockingParametersType;

typedef struct H264ENC_MAKE_NAME(sDeblockingParametersMBAFF)
{
    DeblockingParametersType m_base;
    Ipp8u  StrengthComplex[16];                                 // (Ipp8u) arrays of deblocking sthrengths
    Ipp8u  StrengthExtra[16];                                   // (Ipp8u) arrays of deblocking sthrengths
    Ipp32s UseComplexVerticalDeblocking;                        // (Ipp32u) flag to do complex deblocking on external vertical edge
    Ipp32s ExtraHorizontalEdge;                                 // (Ipp32u) flag to do deblocking on extra horizontal edge
    Ipp32s nLeft[2];                                            // (Ipp32u []) left couple macroblock numbers
} DeblockingParametersMBAFFType;

inline
Ipp8u H264ENC_MAKE_NAME(getEncoderBethaTable)(
    Ipp32s index)
{
    return(ENCODER_BETA_TABLE_8u[index]);
}

inline
Ipp8u H264ENC_MAKE_NAME(getEncoderAlphaTable)(
    Ipp32s index)
{
    return(ENCODER_ALPHA_TABLE_8u[index]);
}

inline
Ipp8u* H264ENC_MAKE_NAME(getEncoderClipTab)(
    Ipp32s index)
{
    return(ENCODER_CLIP_TAB_8u[index]);
}

#pragma pack()

#undef DeblockingParametersType
#undef DeblockingParametersMBAFFType
#undef H264ENC_MAKE_NAME
#undef COEFFSTYPE
#undef PIXTYPE
