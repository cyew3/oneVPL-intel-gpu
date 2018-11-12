// Copyright (c) 2012-2018 Intel Corporation
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

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_TABLES_H__
#define __MFX_H265_TABLES_H__

namespace H265Enc {

extern const Ipp8s h265_log2m2[257];

extern const Ipp8u h265_type2idx[4];
extern const Ipp32u h265_min_in_group[10];
extern const Ipp32u h265_group_idx[32];

extern const Ipp8u h265_scan_z2r4[256];
extern const Ipp8u h265_scan_r2z4[256];
extern const Ipp16u *h265_sig_last_scan[3][5];
extern const Ipp16u h265_sig_scan_CG32x32[64];
extern const Ipp16u h265_sig_last_scan_8x8[ 4 ][ 4 ];
extern const Ipp32s h265_filteredModes[];
extern const Ipp32s h265_numPu[];

extern const Ipp8u h265_chroma422IntraAngleMappingTable[NUM_INTRA_MODE];

extern const Ipp8u h265_pgop_layers[PGOP_PIC_SIZE];

extern const CostType h265_lambda[100];

extern const float h265_reci_1to116[16];

const int PITCH_TU = 16;

} // namespace

#endif // __MFX_H265_TABLES_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
