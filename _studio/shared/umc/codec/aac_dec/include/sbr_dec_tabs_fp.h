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

#ifndef __SBR_DEC_TABS_FP_H__
#define __SBR_DEC_TABS_FP_H__

/* SBR */
#include "sbr_huff_tabs.h"
/* PS */
#include "ps_dec_struct.h"

/********************************************************************/

#ifdef  __cplusplus
extern "C" {
#endif

  /********************************************************************/
  /*                       SPECTRAL BAND REPLICATION                  */
  /********************************************************************/
/* QMF WINDOW */
extern Ipp32f  SBR_TABLE_QMF_WINDOW_640[];
extern Ipp32f  SBR_TABLE_QMF_WINDOW_320[];
/* TABLE is used by HFadjustment */
extern Ipp32f  SBR_TABLE_FI_NOISE_RE[];
extern Ipp32f  SBR_TABLE_FI_NOISE_IM[];
extern Ipp32f* SBR_TABLE_V[2];

  /********************************************************************/
  /*                   PARAMETRIC STEREO PART                         */
  /********************************************************************/
extern const Ipp32s tabResBand1020[];
extern const Ipp32s tabResBand34[];

extern const Ipp32f tabQuantIidStd[];
extern const Ipp32f tabQuantIidFine[];
extern const Ipp32f tabScaleIidStd[];
extern const Ipp32f tabScaleIidFine[];

extern const Ipp32f tabQuantRHO[];

extern const Ipp32f* pCoefTabs[13];

/********************************************************************/
// configuration set [band1020, band24]

extern const sTDSpec setConfBand20;
extern const sTDSpec setConfBand34;

#ifdef  __cplusplus
}
#endif

/********************************************************************/

#endif/*__SBR_DEC_TABS_FP_H__ */
