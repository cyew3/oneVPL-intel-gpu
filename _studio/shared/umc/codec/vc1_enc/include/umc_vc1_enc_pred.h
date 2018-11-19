// Copyright (c) 2008-2018 Intel Corporation
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

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef _ENCODER_VC1_PRED
#define _ENCODER_VC1_PRED

#include "umc_vc1_enc_mb.h"
#include "ippi.h"


namespace UMC_VC1_ENCODER
{


bool DCACPredictionIntraFrameSM(VC1EncoderMBData*        pCurrMB,
                                NeighbouringMBsData*  pMBs,
                                VC1EncoderMBData*     pPredBlock,
                                int16_t                defPredictor,
                                eDirection*           direction);

bool DCACPredictionFrame(VC1EncoderMBData* pCurrMB,
                         NeighbouringMBsData*  pMBs,
                         VC1EncoderMBData* pPredBlock,
                         int16_t defPredictor,
                         eDirection* direction);

bool DCACPredictionFrame4MVIntraMB   (  VC1EncoderMBData*               pCurrMB,
                                        NeighbouringMBsData*            pMBs,
                                        NeighbouringMBsIntraPattern*    pMBsIntraPattern,
                                        VC1EncoderMBData*               pPredBlock,
                                        int16_t                          defPredictor,
                                        eDirection*                     direction);

int8_t DCACPredictionFrame4MVBlockMixed(  VC1EncoderMBData*               pCurrMB,
                                        uint32_t                          currIntraPattern,
                                        NeighbouringMBsData*            pMBs,
                                        NeighbouringMBsIntraPattern*    pMBsIntraPattern,
                                        VC1EncoderMBData*               pPredBlock,
                                        int16_t                          defPredictor,
                                        eDirection*                     direction);
}
#endif
#endif
