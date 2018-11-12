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

#ifndef __SBR_HUFF_TABS_H__
#define __SBR_HUFF_TABS_H__

#include "ippdc.h"
#include "ippac.h"

#define NUM_SBR_HUFF_TABS 10

#define HUFF_NOISE_COMPRESS 0

#define HUFF_ENV_COMPRESS   1

#ifdef  __cplusplus
extern "C" {
#endif

/* SBR */
extern Ipp32s vlcSbrTableSizes[];
extern Ipp32s vlcSbrNumSubTables[];
extern Ipp32s *vlcSbrSubTablesSizes[];
extern IppsVLCTable_32s *vlcSbrBooks[];

/* PS */
extern Ipp32s vlcPsTableSizes[];
extern Ipp32s vlcPsNumSubTables[];
extern Ipp32s *vlcPsSubTablesSizes[];
extern IppsVLCTable_32s *vlcPsBooks[];
IppStatus ownInitSBREncHuffTabs(IppsVLCEncodeSpec_32s** ppSpec, Ipp32s *sizeAll);

Ipp32s ownVLCCountBits_16s32s(Ipp16s delta, IppsVLCEncodeSpec_32s* pHuffTab);

Ipp32s sbrencSetEnvHuffTabs(Ipp32s  bs_amp_res,
                            Ipp16s* LAV,
                            Ipp32s* bs_env_start_bits,

                            IppsVLCEncodeSpec_32s** pTimeHuffTab,
                            IppsVLCEncodeSpec_32s** pFreqHuffTab,
                            IppsVLCEncodeSpec_32s*  sbrHuffTabs[NUM_SBR_HUFF_TABS],

                            Ipp32s  typeCompress); /* [1] - envelope, [0] - noise */

#ifdef  __cplusplus
}
#endif

#endif/*__SBR_HUFF_TABS_H__ */
