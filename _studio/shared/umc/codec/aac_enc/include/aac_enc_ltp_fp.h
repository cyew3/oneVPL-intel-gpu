//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

#ifndef __LTPENCODE_H
#define __LTPENCODE_H

#include "aac_filterbank_fp.h"
#include "aac_enc_own.h"

#include "ipps.h"

#ifdef  __cplusplus
extern "C" {
#endif

void ltpEncode(Ipp32f *inBuf,
               Ipp32f *ltpBuf,
               Ipp32f *predictedBuf,
               Ipp32s *ltpDelay,
               Ipp32s *ltpInd,
               IppsFFTSpec_R_32f* corrFft,
               Ipp8u* corrBuff);

void ltpBufferUpdate(Ipp32f **ltpBuffer,
                     Ipp32f **prevSamples,
                     Ipp32f **predictedSpectrum,
                     sEnc_channel_pair_element *pElement,
                     sFilterbank *filterbank,
                     Ipp32s *sfb_offset_short,
                     Ipp32s *prevWindowShape,
                     Ipp32s numCh);

#ifdef  __cplusplus
}
#endif

#endif//__LTPENCODE_H
