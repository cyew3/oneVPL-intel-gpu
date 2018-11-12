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

#ifndef __QUANTIZATION_H
#define __QUANTIZATION_H

#include "aac_enc_psychoacoustic_fp.h"
#include "aac_enc_own.h"

#include "ipps.h"

#define  MAX_QUANT     8191
#define  MAGIC_NUMBER  0.4054

#ifndef  SIGN
#define  SIGN(x) ((x) > 0 ? (1): (-1))
#endif

typedef struct {
  Ipp32f* mdct_line;
  Ipp32f* noiseThr;
  Ipp32f* minSNR;
  Ipp32f* bitsToPeCoeff;
  Ipp32f* energy;
  Ipp32f* logEnergy;
  Ipp32f* sfbPE;
  Ipp32f* scalefactorDataBits;
  Ipp32f* PEtoNeededPECoeff;
  Ipp32f  outPe;
  Ipp32s  predAttackWindow;
  Ipp32s  allowHolesSfb;
} sQuantizationData;

#ifdef  __cplusplus
extern "C" {
#endif

  void Quantization(sQuantizationBlock* pBlock,
                    sEnc_individual_channel_stream* pStream,
                    sQuantizationData* qData);
#ifdef  __cplusplus
}
#endif

#endif//__QUANTIZATION_H
