// Copyright (c) 2002-2018 Intel Corporation
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

#ifndef __MP3ENC_TABLES_H__
#define __MP3ENC_TABLES_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _scalefac_struct
{
  VM_ALIGN32_DECL(Ipp32u) l[23];
  VM_ALIGN32_DECL(Ipp32u) s[14];
  VM_ALIGN32_DECL(Ipp32u) si[40];
} scalefac_struct;

extern const scalefac_struct mp3enc_sfBandIndex[2][3];

typedef Ipp16s IXS[192][3];

extern const Ipp32s mp3enc_slen1_tab[16];
extern const Ipp32s mp3enc_slen2_tab[16];
extern const Ipp32s mp3enc_pretab[21];

extern const Ipp32s mp3enc_region01_table[23][2];

#ifdef __cplusplus
}
#endif

#endif  //  __MP3ENC_TABLES_H__
