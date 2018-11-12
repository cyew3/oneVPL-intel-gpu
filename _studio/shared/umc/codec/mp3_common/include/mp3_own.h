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

#ifndef __MP3_OWN_H__
#define __MP3_OWN_H__

#include "mp3_alloc_tab.h"
#include "vm_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEN_MDCT_LINE 576

#define MP3_UPDATE_PTR(type, ptr, inc)    \
  if (ptr) {                              \
    ptr = (type *)((Ipp8u *)(ptr) + inc); \
  }

typedef struct {
    Ipp32s ext_bit_stream_present;                                // 4 bytes
    Ipp32s n_ad_bytes;                                            // 4 bytes
    Ipp32s center;                                                // 4 bytes
    Ipp32s surround;                                              // 4 bytes
    Ipp32s lfe;                                                   // 4 bytes
    Ipp32s audio_mix;                                             // 4 bytes
    Ipp32s dematrix_procedure;                                    // 4 bytes
    Ipp32s no_of_multi_lingual_ch;                                // 4 bytes
    Ipp32s multi_lingual_fs;                                      // 4 bytes
    Ipp32s multi_lingual_layer;                                   // 4 bytes
    Ipp32s copyright_identification_bit;                          // 4 bytes
    Ipp32s copyright_identification_start;                        // 4 bytes
} mp3_mc_header;                                                  // total 48 bytes

extern const Ipp32s mp3_bitrate[2][3][16];
extern const Ipp32s mp3_frequency[3][4];
extern const Ipp32s mp3_mc_pred_coef_table[6][16];
extern const Ipp8u mp3_mc_sb_group[32];

extern const Ipp32f mp3_lfe_filter[480];


Ipp32s mp3_SetAllocTable(Ipp32s header_id, Ipp32s mpg25, Ipp32s header_layer,
                         Ipp32s header_bitRate, Ipp32s header_samplingFreq,
                         Ipp32s stereo,
                         const Ipp32s **nbal_alloc_table,
                         const Ipp8u **alloc_table,
                         Ipp32s *sblimit);

#ifdef __cplusplus
}
#endif

#endif
