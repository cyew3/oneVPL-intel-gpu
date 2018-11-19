// Copyright (c) 2007-2018 Intel Corporation
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

#if defined (UMC_ENABLE_UMC_SCENE_ANALYZER)

#ifndef __UMC_SCENE_INFO_H
#define __UMC_SCENE_INFO_H

#include "ippdefs.h"

namespace UMC
{

// declare motion estimation boundaries
enum
{
    SA_ESTIMATION_WIDTH         = 128,
    SA_ESTIMATION_HEIGHT        = 64
};

// declare types of predictions
enum
{
    SA_COLOR                    = 0,
    SA_INTRA                    = 1,
    SA_INTER                    = 2,
    SA_INTER_ESTIMATED          = 3,

    SA_PRED_TYPE                = 4
};

// declare data structure
struct UMC_SCENE_INFO
{
    uint32_t sumDev[SA_PRED_TYPE];                                // (uint32_t []) sum of deviations of blocks
    uint32_t averageDev[SA_PRED_TYPE];                            // (uint32_t []) average deviation of blocks
    uint32_t numItems[SA_PRED_TYPE];                              // (uint32_t) number of valid units
    uint32_t bestMatches;                                         // (uint32_t) number of excellent motion matches
};

inline
void AddIntraDeviation(UMC_SCENE_INFO *pDst, const UMC_SCENE_INFO *pSrc)
{
    pDst->sumDev[SA_INTRA] += pSrc->sumDev[SA_INTRA];
    pDst->sumDev[SA_COLOR] += pSrc->sumDev[SA_COLOR];

    pDst->numItems[SA_INTRA] += pSrc->numItems[SA_INTRA];
    pDst->numItems[SA_COLOR] += pSrc->numItems[SA_COLOR];

    pDst->bestMatches += pSrc->bestMatches;

} // void AddIntraDeviation(UMC_SCENE_INFO *pDst, const UMC_SCENE_INFO *pSrc)

inline
void GetAverageIntraDeviation(UMC_SCENE_INFO *pSrc)
{
    uint32_t numItems;

    // get average intra deviation
    numItems = MFX_MAX(1, pSrc->numItems[SA_INTRA]);
    pSrc->averageDev[SA_INTRA] = (pSrc->sumDev[SA_INTRA] + numItems / 2) /
                                 numItems;
    // get average color
    numItems = MFX_MAX(1, pSrc->numItems[SA_COLOR]);
    pSrc->averageDev[SA_COLOR] = (pSrc->sumDev[SA_COLOR] + numItems / 2) /
                                 numItems;

} // void GetAverageIntraDeviation(UMC_SCENE_INFO *pSrc)

inline
void AddInterDeviation(UMC_SCENE_INFO *pDst, const UMC_SCENE_INFO *pSrc)
{
    pDst->sumDev[SA_INTER] += pSrc->sumDev[SA_INTER];
    pDst->sumDev[SA_INTER_ESTIMATED] += pSrc->sumDev[SA_INTER_ESTIMATED];

    pDst->numItems[SA_INTER] += pSrc->numItems[SA_INTER];
    pDst->numItems[SA_INTER_ESTIMATED] += pSrc->numItems[SA_INTER_ESTIMATED];

} // void AddInterDeviation(UMC_SCENE_INFO *pDst, const UMC_SCENE_INFO *pSrc)

inline
void GetAverageInterDeviation(UMC_SCENE_INFO *pSrc)
{
    uint32_t numItems;

    // get average intra deviation
    numItems = MFX_MAX(1, pSrc->numItems[SA_INTER]);
    pSrc->averageDev[SA_INTER] = (pSrc->sumDev[SA_INTER] + numItems / 2) /
                                 numItems;
    // get average color
    numItems = MFX_MAX(1, pSrc->numItems[SA_INTER_ESTIMATED]);
    pSrc->averageDev[SA_INTER_ESTIMATED] = (pSrc->sumDev[SA_INTER_ESTIMATED] + numItems / 2) /
                                           numItems;

} // void GetAverageIntraDeviation(UMC_SCENE_INFO *pSrc)

inline
int32_t GetAbs(int32_t value)
{
    int32_t signExtended = value >> 31;

    value = (value ^ signExtended) - signExtended;

    return value;

} // int32_t GetAbs(int32_t value)

} // namespace UMC

#endif // __UMC_SCENE_INFO_H
#endif
