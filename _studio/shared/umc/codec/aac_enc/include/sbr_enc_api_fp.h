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

#ifndef __SBR_ENC_API_FP_H__
#define __SBR_ENC_API_FP_H__

#include "bstream.h"
#include "aac_status.h"
#include "sbr_settings.h"
#include "sbr_struct.h"
#include "sbr_huff_tabs.h"
#include "sbr_enc_settings.h"
#include "sbr_enc_struct.h"

#include "ippac.h"
#include "ipps.h"

/********************************************************************/

typedef struct {

  /* main params */
  Ipp32s sbrFreqIndx;

  Ipp32f splitThr;

  Ipp32s sbr_offset;

  Ipp32s indx_CE_Tab[SBR_MAX_CH];

  /* may be: <SCE> or <CPE> or <SCE> + <CPE> */
  Ipp32s indx_FreqOffset_Tab[2];

  /* HUFF_TABS */
  IppsVLCEncodeSpec_32s* sbrHuffTabs[NUM_SBR_HUFF_TABS];

  /* header params: [0] - SCE, [1] - CPE */
  sSBRHeader* sbrHeader;

  /* sbr freq tables && actual sizes: [0] - SCE, [1] - CPE */
  sSBRFeqTabsState* sbrFreqTabsState;

  /* ALL SBR_CHANNEL W/O LFE */
  sSBREnc_SCE_State* pSCE_Element;

  /* LFE CH */
  Ipp32f* pInputBufferLFE;

} sSBREncState;

/********************************************************************/

#ifdef  __cplusplus
extern  "C" {
#endif

 /* algorithm */
  /* NULL */

/********************************************************************/

  /* SBR GENERAL HIGH LEVEL API: FLOAT POINT VERSION */

/********************************************************************/

/* sbr tuning tables */
  AACStatus sbrencCheckParams( Ipp32s sampleRate, Ipp32s numCh, Ipp32s bitRate, Ipp32s* indx );

/* sbr encoder */
  AACStatus sbrencInit( sSBREncState** ppState, Ipp32s* indxTuningTab, Ipp32s* indx_CE_Tabs );

  AACStatus sbrencReset( sSBREncState* pState, Ipp32s* indxTuningTab );

  void sbrencFree( sSBREncState* pState );

  AACStatus sbrencGetFrame( sSBREncState* pState, ownFilterSpec_SBR_C_32fc* pSpec );

/********************************************************************/

/* resampler */
  AACStatus sbrencResampler_v2_32f(Ipp32f* pSrc, Ipp32f* pDst);

/********************************************************************/

/* analysis QMF */
  AACStatus ownAnalysisFilterInitAlloc_SBREnc_RToC_32f32fc(ownFilterSpec_SBR_C_32fc* pQMFSpec,
                                                           Ipp32s *sizeAll);
  AACStatus ownAnalysisFilter_SBREnc_RToC_32f32fc(Ipp32f* pSrc, Ipp32fc* pDst, ownFilterSpec_SBR_C_32fc* pSpec);

/********************************************************************/

/* payloadBit formatter */
  Ipp32s enc_fill_element( sSBREncState* pState, sBitsreamBuffer* pBS);

  Ipp32s enc_fill_element_write( sBitsreamBuffer *pBS, Ipp8u* bufAuxBS, Ipp32s payloadBits );

/********************************************************************/

#ifdef  __cplusplus
}
#endif

#endif             /* __SBR_ENC_API_FP_H__ */
