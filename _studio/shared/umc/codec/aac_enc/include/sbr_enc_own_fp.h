// Copyright (c) 2006-2018 Intel Corporation
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

#ifndef __SBR_ENC_OWN_FP_H__
#define __SBR_ENC_OWN_FP_H__

#include "aac_status.h"
#include "aaccmn_const.h"
#include "sbr_settings.h"
#include "sbr_struct.h"
#include "sbr_huff_tabs.h"
#include "sbr_enc_api_fp.h"

#include "ipps.h"

/********************************************************************/

extern const sSBREncTuningTab SBR_TUNING_TABS[];

/********************************************************************/

#ifdef  __cplusplus
extern  "C" {
#endif

void sbrencCalcNrg(Ipp32fc bufX[][64], Ipp32fc bufXFlip[][32], Ipp32f bufNrg[][64], Ipp32f bufNrgFlip[][64]);

void sbrencTransientDetector(Ipp32f  bufNrgFlip[][64],
                             Ipp32f* pThr,
                             Ipp32f* pTransient,
                             Ipp32s* tranPos,
                             Ipp32s* tranFlag);

AACStatus sbrencGetTuningTab(Ipp32s sampleRate, enum eId id, Ipp32s bitRate, Ipp32s* indx);

Ipp32s sbrencFrameSplitter(Ipp32f bufNrg[][64],
                           Ipp32s* pTabF,
                           Ipp32s nBand,
                           Ipp32f* nrgPrevLow,
                           const Ipp32f splitThr);

Ipp32s sbrencFrameGenerator(sSBREnc_SCE_State* pState, Ipp32s splitFlag);

IppStatus ownEstimateTNR_SBR_32f(const Ipp32fc* pSrc, Ipp32f* pTNR0, Ipp32f* pTNR1, Ipp32f* pMeanNrg );

Ipp32s sbrencInvfEstimation (sSBRInvfEst* pState,
                             Ipp32f bufT[][64],
                             Ipp32f* bufNrg,
                             Ipp32s* tabPatchMap,
                             Ipp32s transientFlag,
                             Ipp32s* pNoiseFreqTab,
                             Ipp32s nNoiseBand,
                             Ipp32s* infVec);

Ipp32s sbrencSinEstimation(sSBREnc_SCE_State* pState, sSBRFeqTabsState* pFreqTabsState);

Ipp32s sbrencNoisefEstimation(sSBREnc_SCE_State* pState,
                              Ipp32s* pFreqTab,
                              Ipp32s  nNoiseBand,
                              Ipp16s* bufNoiseQuant);

Ipp32s sbrencEnvEstimation (Ipp32f bufNrg[][64],
                            sSBRFrameInfoState* pState,

                            Ipp16s* bufEnvQuant,
                            Ipp32s  nBand[2],
                            Ipp32s* pFreqTabs[2],
                            Ipp32s  amp_res,
                            Ipp32s* add_harmonic,
                            Ipp32s  add_harmonic_flag);

Ipp32s
sbrencDeltaCoding (Ipp16s*  bufEnvQuant,
                   Ipp16s*  bufEnvQuantPrev,

                   Ipp32s*  freq_res,
                   Ipp32s   nSfBands[2],
                   Ipp32s   bs_amp_res,
                   Ipp32s*  bs_df_env,
                   //Ipp32s   coupling,
                   Ipp32s   offset,
                   Ipp32s   nEnv,
                   //Ipp32s   channel,

                   Ipp32s   headerActive,
                   Ipp32s*  flagUpDate,
                   Ipp32s   cur_dF_edge_gain_fac,

                   Ipp32s   typeCompress,

                   IppsVLCEncodeSpec_32s*  sbrHuffTabs[NUM_SBR_HUFF_TABS]);

/********************************************************************/

#ifdef  __cplusplus
}
#endif

#endif //__SBR_ENC_OWN_FP_H__
