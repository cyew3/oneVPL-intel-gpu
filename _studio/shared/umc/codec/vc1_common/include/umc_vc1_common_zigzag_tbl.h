// Copyright (c) 2004-2018 Intel Corporation
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

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER) || defined (UMC_ENABLE_VC1_SPLITTER) || defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef __UMC_VC1_COMMON_ZIGZAG_TBL_H__
#define __UMC_VC1_COMMON_ZIGZAG_TBL_H__

//VC-1 Table 234: Intra Normal Scan
//remapped as src index for continues dst index
const extern  Ipp8u VC1_Intra_Normal_Scan_chroma[64];
const extern  Ipp8u VC1_Intra_Normal_Scan_luma[64];

//VC-1 Table 235: Intra Horizontal Scan
//remapped as src index for continues dst index
const extern  Ipp8u VC1_Intra_Horizontal_Scan_chroma[64];
const extern  Ipp8u VC1_Intra_Horizontal_Scan_luma[64];

//VC-1 Table 236: Intra Vertical Scan
//remapped as src index for continues dst index
const extern Ipp8u VC1_Intra_Vertical_Scan_chroma[64];
const extern Ipp8u VC1_Intra_Vertical_Scan_luma[64];

//Table 237: Inter 8x8 Scan for Simple and Main Profiles
//and Progressive Mode in Advanced Profile
//remapped as src index for continues dst index
const extern Ipp8u VC1_Inter_8x8_Scan_chroma[64];
const extern Ipp8u VC1_Inter_8x8_Scan_luma[64];

//VC-1 Table 238: Inter 8x4 Scan for Simple and Main Profiles
//remapped as src index for continues dst index
const extern Ipp8u VC1_Inter_8x4_Scan_chroma[64];
const extern Ipp8u VC1_Inter_8x4_Scan_luma[64];

//VC-1 Table 239: Inter 4x8 Scan for Simple and Main Profiles
//remapped as src index for continues dst index
const extern Ipp8u VC1_Inter_4x8_Scan_chroma[64];
const extern Ipp8u VC1_Inter_4x8_Scan_luma[64];

//Table 240: Inter 4x4 Scan for Simple and Main Profiles and
//Progressive Mode in Advanced Profile
//remapped as src index for continues dst index
const extern Ipp8u VC1_Inter_4x4_Scan_chroma[64];
const extern Ipp8u VC1_Inter_4x4_Scan_luma[64];

//Table 240: Progressive Mode Inter 8x4 Scan for Advanced Profile
const extern Ipp8u VC1_Inter_8x4_Scan_Adv_chroma[64];
const extern Ipp8u VC1_Inter_8x4_Scan_Adv_luma[64];

//Table 241: Progressive Mode Inter 4x8 Scan for Advanced Profile
const extern Ipp8u VC1_Inter_4x8_Scan_Adv_chroma[64];
const extern Ipp8u VC1_Inter_4x8_Scan_Adv_luma[64];

const extern Ipp8u VC1_Inter_InterlaceIntra_8x8_Scan_Adv_chroma[64];
const extern Ipp8u VC1_Inter_InterlaceIntra_8x8_Scan_Adv_luma[64];

/*Table 243 (SMPTE-421M-FDS1): Interlace Mode Inter 8x4 Scan for Advanced Profile*/
const extern Ipp8u VC1_Inter_Interlace_8x4_Scan_Adv_chroma[64];
const extern Ipp8u VC1_Inter_Interlace_8x4_Scan_Adv_luma[64];

/*Table 244 (SMPTE-421M-FDS1): Interlace Mode Inter 4x8 Scan for Advanced Profile*/
const extern Ipp8u VC1_Inter_Interlace_4x8_Scan_Adv_chroma[64];
const extern Ipp8u VC1_Inter_Interlace_4x8_Scan_Adv_luma[64];

/*Table 245 (SMPTE-421M-FDS1): Interlace Mode Inter 4x4 Scan for Advanced Profile*/
const extern Ipp8u VC1_Inter_Interlace_4x4_Scan_Adv_chroma[64];
const extern Ipp8u VC1_Inter_Interlace_4x4_Scan_Adv_luma[64];

//Advanced profile
const extern Ipp8u* AdvZigZagTables_IProgressive_luma[2][7];
const extern Ipp8u* AdvZigZagTables_IProgressive_chroma[2][7];

const extern Ipp8u* AdvZigZagTables_IInterlace_luma[2][7];
const extern Ipp8u* AdvZigZagTables_IInterlace_chroma[2][7];

const extern Ipp8u* AdvZigZagTables_IField_luma[2][7];
const extern Ipp8u* AdvZigZagTables_IField_chroma[2][7];

const extern Ipp8u* AdvZigZagTables_PBProgressive_luma[2][7];
const extern Ipp8u* AdvZigZagTables_PBProgressive_chroma[2][7];

const extern Ipp8u* AdvZigZagTables_PBInterlace_luma[2][7];
const extern Ipp8u* AdvZigZagTables_PBInterlace_chroma[2][7];

const extern Ipp8u* AdvZigZagTables_PBField_luma[2][7];
const extern Ipp8u* AdvZigZagTables_PBField_chroma[2][7];

//Simple/main
const extern Ipp8u* ZigZagTables_I_luma[2][7];
const extern Ipp8u* ZigZagTables_I_chroma[2][7];
const extern Ipp8u* ZigZagTables_PB_luma[2][7];
const extern Ipp8u* ZigZagTables_PB_chroma[2][7];

#endif //__umc_vc1_common_zigzag_tbl_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
