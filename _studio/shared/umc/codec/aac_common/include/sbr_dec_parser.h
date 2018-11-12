// Copyright (c) 2005-2018 Intel Corporation
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

#ifndef __SBR_DEC_PARSER_H__
#define __SBR_DEC_PARSER_H__

#include "sbr_dec_struct.h"
#include "bstream.h"

#ifdef  __cplusplus
extern  "C" {
#endif

  Ipp32s  sbr_grid(sBitsreamBuffer * BS, Ipp32s* bs_frame_class, Ipp32s* bs_pointer, Ipp32s* freqRes,
                   Ipp32s* bordersEnv, Ipp32s* bordersNoise, Ipp32s* nEnv, Ipp32s* nNoiseEnv, Ipp32s* status);

  void    sbr_grid_coupling(sSBRDecComState * pSbr);

  Ipp32s  sbr_dtdf(sBitsreamBuffer * BS, Ipp32s* bs_df_env, Ipp32s* bs_df_noise, Ipp32s nEnv, Ipp32s nNoiseEnv);

  Ipp32s  sbr_invf(Ipp32s ch, sBitsreamBuffer * BS, sSBRDecComState * pSbr);

  Ipp32s  sbr_envelope(Ipp32s ch, Ipp32s bs_coupling, Ipp32s bs_amp_res,
                       sBitsreamBuffer * BS, sSBRDecComState * pDst);

  Ipp32s  sbr_noise(sBitsreamBuffer* BS, Ipp16s* vNoise, Ipp32s* vSize, Ipp32s* bs_df_noise,
                    void* sbrHuffTables[10], Ipp32s ch, Ipp32s bs_coupling, Ipp32s nNoiseEnv, Ipp32s NQ);

  Ipp32s  sbr_sinusoidal_coding(sBitsreamBuffer * BS, Ipp32s* pDst, Ipp32s len);

  Ipp32s  sbrEnvNoiseDec(sSBRDecComState * pSbr, Ipp32s ch);

#ifdef  __cplusplus
}
#endif

#endif/*__SBR_DEC_PARSER_H__ */
