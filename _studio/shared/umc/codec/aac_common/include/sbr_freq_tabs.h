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

#ifndef __SBR_FREQ_TABS_H__
#define __SBR_FREQ_TABS_H__

#include "sbr_struct.h"

#ifdef  __cplusplus
extern  "C" {
#endif

  Ipp32s  sbrCalcMasterFreqBoundary(Ipp32s bs_start_freq,
                                    Ipp32s bs_stop_freq,
                                    Ipp32s sbrFreqIndx,
                                    Ipp32s *k0,
                                    Ipp32s *k2);

  Ipp32s  sbrCalcMasterFreqBandTab(Ipp32s k0,
                                   Ipp32s k2,
                                   Ipp32s bs_freq_scale,
                                   Ipp32s bs_alter_scale,
                                   Ipp32s *fMasterBandTab,
                                   Ipp32s *nMasterBand);

  /* common function: sbrCalcNoiseTab + sbrCalcHiFreqTab + sbrCalcLoFreqTab */
  Ipp32s sbrCalcDerivedFreqTabs(sSBRFeqTabsState* pFTState,
                                Ipp32s  bs_xover_band,
                                Ipp32s  bs_noise_bands,
                                Ipp32s  k2,
                                Ipp32s* kx,
                                Ipp32s* M);

  Ipp32s sbrGetPowerVector(Ipp32s numBands0,
                           Ipp32s k1,
                           Ipp32s k0,
                           Ipp32s* pow_vec);

  Ipp32s sbrCalcNoiseTab(Ipp32s* fLoFreqTab,
                         Ipp32s  nLoBand,
                         Ipp32s  bs_noise_bands,
                         Ipp32s  k2,
                         Ipp32s  kx,
                         Ipp32s* fNoiseTab,
                         Ipp32s* nNoiseBand);

  Ipp32s sbrCalcLoFreqTab(Ipp32s* fHiFreqTab,
                          Ipp32s  nHiBand,
                          Ipp32s* fLoFreqTab,
                          Ipp32s* nLoBand);

  Ipp32s sbrCalcHiFreqTab(Ipp32s* fMasterBandTab,
                          Ipp32s  nMasterBand,
                          Ipp32s  bs_xover_band,
                          Ipp32s* fHiFreqTab,
                          Ipp32s* nHighBand);


  Ipp32s  sbrCalcLimBandTab(Ipp32s bs_limiter_bands,
                            Ipp32s *fLoBandTab,
                            Ipp32s nLoBand,
                            Ipp32s numPatches,
                            Ipp32s *patchNumSubbands,

                            Ipp32s *fLimBandTab,
                            Ipp32s *nLimBand);

  Ipp32s sbrCalcPatchConstructTab(Ipp32s* fMasterBandTab,
                                  Ipp32s  nMasterBand,
                                  Ipp32s  M,
                                  Ipp32s  kx,
                                  Ipp32s  k0,
                                  Ipp32s  sbrFreqIndx,

                                  Ipp32s* patchNumSubbandsTab,
                                  Ipp32s* patchStartSubbandTab,
                                  Ipp32s* numPatches);

  Ipp32s sbrCalcPatchConstruct_indxTab(Ipp32s* fMasterTab,
                                       Ipp32s  nMasterBand,
                                       Ipp32s  k0,
                                       Ipp32s  M,
                                       Ipp32s  sbrFreqIndx,
                                       Ipp32s* idxPatchMap);


#ifdef  __cplusplus
}
#endif

#endif/*__SBR_FREQ_TABS_H__ */
