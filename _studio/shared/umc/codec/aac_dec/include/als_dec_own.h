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

#ifndef __ALS_DEC_OWN_H__
#define __ALS_DEC_OWN_H__

#include "aac_status.h"

#ifdef __cplusplus
extern "C" {
#endif
void alsgetBlocks(Ipp32s bsInfo,
                  Ipp32s len,
                  Ipp32s *blockLen,
                  Ipp32s *numBlocks);

Ipp32s alsdecParseBlockData(sBitsreamBuffer *pBS,
                            ALSDec        *state,
                            alsBlockState   *blockState);

void alsdecParseChannelData(sBitsreamBuffer *pBS,
                            ALSDec          *state,
                            Ipp32s ch,
                            Ipp32s *pLayer);

void alsReconstructData(ALSDec      *state,
                        alsBlockState *blockState);

Ipp32s alsRLSSynthesize(ALSDec         *state,
                        alsBlockState  *blockState,
                        alsRLSLMSState *rlState);

Ipp32s alsRLSSynthesizeJoint(ALSDec         *state,
                             alsBlockState  **blockState,
                             alsRLSLMSState **rlState);

void aasdecDiffMantissa(sBitsreamBuffer *pBS,
                        ALSDec          *state);

void alsMLZflushDict(alsMLZState *MLZState);


#ifdef __cplusplus
}
#endif

#endif // __ALS_DEC_OWN_H__
