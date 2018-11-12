// Copyright (c) 2011-2018 Intel Corporation
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

#ifndef __BSAC_DEC_H__
#define __BSAC_DEC_H__

#include "ippdefs.h"
#include "aaccmn_const.h"

//#define USE_LTP_IN_BSAC

#define BSAC_MAX_LAYER 100
#define BSAC_MAX_CHANNEL 2

typedef struct {
  Ipp32s bitBuf[1024];
  Ipp32s cbandSI[BSAC_MAX_CHANNEL][MAX_GROUP_NUMBER][32];
  Ipp32s MSBPlane[BSAC_MAX_CHANNEL][MAX_GROUP_NUMBER][32];
  Ipp32s *curSnf[BSAC_MAX_CHANNEL][MAX_GROUP_NUMBER];
  Ipp32s *signIsCoded[BSAC_MAX_CHANNEL][MAX_GROUP_NUMBER];
  Ipp32s *higherBitVector[BSAC_MAX_CHANNEL][MAX_GROUP_NUMBER];
  Ipp32s *offsetHGFE[BSAC_MAX_CHANNEL][MAX_GROUP_NUMBER];
  Ipp32s *offsetBCD[BSAC_MAX_CHANNEL][MAX_GROUP_NUMBER];
  Ipp32s curSnfBuf[BSAC_MAX_CHANNEL * 1024];
  Ipp32s signIsCodedBuf[BSAC_MAX_CHANNEL * 1024];
  Ipp32s higherBitVectorBuf[BSAC_MAX_CHANNEL * 1024];
  Ipp32s offsetHGFEBuf[BSAC_MAX_CHANNEL * 256];
  Ipp32s offsetBCDBuf[BSAC_MAX_CHANNEL * 256];
  Ipp32s layerGroup[BSAC_MAX_LAYER];
  Ipp32s layerStartCBand[BSAC_MAX_LAYER];
  Ipp32s layerEndCBand[BSAC_MAX_LAYER];
  Ipp32s layerStartIndex[BSAC_MAX_LAYER];
  Ipp32s layerEndIndex[BSAC_MAX_LAYER];
  Ipp32s layerStartSfb[BSAC_MAX_LAYER];
  Ipp32s layerEndSfb[BSAC_MAX_LAYER];
  Ipp32s layerBitOffset[BSAC_MAX_LAYER+1];
  Ipp32s terminalLayer[BSAC_MAX_LAYER];
  Ipp32s flashBits[BSAC_MAX_LAYER];
  Ipp32s layerBufferOffset[BSAC_MAX_LAYER];
  Ipp32s maxScalefactor[BSAC_MAX_CHANNEL];
  Ipp32s cbandSiType[BSAC_MAX_CHANNEL];
  Ipp32s bsaeScfModel[BSAC_MAX_CHANNEL];
  Ipp32s enhScfModel[BSAC_MAX_CHANNEL];
  Ipp32s pnsEnergyFlag[BSAC_MAX_CHANNEL];
  Ipp32s maxPnsPower[BSAC_MAX_CHANNEL];
  Ipp32s pnsFlag[BSAC_MAX_CHANNEL][MAX_GROUP_NUMBER][MAX_SFB];
  Ipp32s pnsMode[MAX_GROUP_NUMBER][MAX_SFB];
  Ipp32s sfbOffset[MAX_GROUP_NUMBER][MAX_SFB+1];
  Ipp32s noiseSfbState[MAX_GROUP_NUMBER][MAX_SFB];
  Ipp32s alreadyDecoded[MAX_GROUP_NUMBER][MAX_SFB];
  Ipp32s value;
  Ipp32s range;
  Ipp32s estCwLen;
  Ipp32s frameLength;
  Ipp32s headerLength;
  Ipp32s sbaMode;
  Ipp32s topLayer;
  Ipp32s baseSnfThr;
  Ipp32s baseBand;
  Ipp32s maxSfbSiLen;
  Ipp32s pnsDataPresent;
  Ipp32s pnsDataSfb;
  Ipp32s msMaskPresent;
  Ipp32s sLayerSize;
  Ipp32s maxLayer;
  Ipp32s intensityDataPresent;
  Ipp32s numChannels;
} BSACDec;

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif // __BSAC_DEC_H__
