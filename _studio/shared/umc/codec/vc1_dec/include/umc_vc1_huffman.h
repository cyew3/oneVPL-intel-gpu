//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#ifndef __UMC_VC1_HUFFMAN_H__
#define __UMC_VC1_HUFFMAN_H__

// return value 0 means ok, != 0 - failed.

int DecodeHuffmanOne(Ipp32u**  pBitStream, int* pOffset,
    Ipp32s*  pDst, const Ipp32s* pDecodeTable);

int DecodeHuffmanPair(Ipp32u **pBitStream, Ipp32s *pBitOffset,
    const Ipp32s *pTable, Ipp8s *pFirst, Ipp16s *pSecond);

int HuffmanTableInitAlloc(const Ipp32s* pSrcTable, Ipp32s** ppDstSpec);
int HuffmanRunLevelTableInitAlloc(const Ipp32s* pSrcTable, Ipp32s** ppDstSpec);
void HuffmanTableFree(Ipp32s *pDecodeTable);

#endif // __UMC_VC1_HUFFMAN_H__
#endif // #if defined (UMC_ENABLE_VC1_VIDEO_DECODER)
