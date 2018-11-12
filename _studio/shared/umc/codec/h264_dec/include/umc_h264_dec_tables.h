// Copyright (c) 2003-2018 Intel Corporation
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
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_DEC_TABLES_H__
#define __UMC_H264_DEC_TABLES_H__

#include "ippdefs.h"

namespace UMC_H264_DECODER
{

extern const int32_t xyoff[16][2];
extern const int32_t xyoff8[4][2];

// Offset for 8x8 blocks
extern const uint32_t xoff8[4];
extern const uint32_t yoff8[4];

//////////////////////////////////////////////////////////
// scan matrices
extern const int32_t hp_scan4x4[2][4][16];
extern const int32_t hp_membership[2][64];

extern const int32_t hp_CtxIdxInc_sig_coeff_flag[2][63];
extern const int32_t hp_CtxIdxInc_last_sig_coeff_flag[63];

//////////////////////////////////////////////////////////
// Mapping from luma QP to chroma QP
extern const uint32_t QPtoChromaQP[52];

///////////////////////////////////////
// Tables for decoding CBP
extern const uint32_t dec_cbp_intra[2][48];
extern const uint32_t dec_cbp_inter[2][48];

// Mapping from 8x8 block number to 4x4 block number
extern const uint32_t block_subblock_mapping[16];
extern const uint32_t subblock_block_mapping[4];

// Tables used for finding if a block is on the edge
// of a macroblock
extern const uint32_t left_edge_tab4[4];
extern const uint32_t top_edge_tab4[4];
extern const uint32_t right_edge_tab4[4];
extern const uint32_t left_edge_tab16[16];
extern const uint32_t top_edge_tab16[16];
extern const uint32_t right_edge_tab16[16];
extern const uint32_t left_edge_tab16_8x4[16];
extern const uint32_t top_edge_tab16_8x4[16];
extern const uint32_t right_edge_tab16_8x4[16];
extern const uint32_t left_edge_tab16_4x8[16];
extern const uint32_t top_edge_tab16_4x8[16];
extern const uint32_t right_edge_tab16_4x8[16];

extern const uint32_t above_right_avail_8x4[16];
extern const uint32_t above_right_avail_4x8[16];
extern const uint32_t above_right_avail_4x4[16];
extern const uint32_t above_right_avail_4x4_lin[16];
extern const uint32_t subblock_block_membership[16];

extern const uint8_t default_intra_scaling_list4x4[16];
extern const uint8_t default_inter_scaling_list4x4[16];
extern const uint8_t default_intra_scaling_list8x8[64];
extern const uint8_t default_inter_scaling_list8x8[64];

extern const int8_t ClipQPTable[52*3];

extern const uint32_t num_blocks[];
extern const int32_t dec_values[];
extern const uint32_t mb_c_width[];
extern const uint32_t mb_c_height[];
extern const uint32_t x_pos_value[4][16];
extern const uint32_t y_pos_value[4][16];
extern const uint32_t block_y_values[4][4];
extern const uint32_t first_v_ac_block[];
extern const uint32_t last_v_ac_block[];

extern const
uint32_t SbPartNumMinus1[2][17];

extern const int32_t ChromaDC422RasterScan[8];

extern const uint32_t mask_[];
extern const uint32_t mask_bit[];
extern const int32_t iLeftBlockMask[];
extern const int32_t iTopBlockMask[];

extern const uint32_t blockcbp_table[];

extern const uint32_t BlockNumToMBRowLuma[17];
extern const uint32_t BlockNumToMBRowChromaAC[4][32];
extern const uint32_t BlockNumToMBColLuma[17];
extern const uint32_t BlockNumToMBColChromaAC[4][32];

extern const int32_t iBlockCBPMask[16];
extern const int32_t iBlockCBPMaskChroma[8];

extern const uint16_t SAspectRatio[17][2];

extern const uint32_t SubWidthC[4];
extern const uint32_t SubHeightC[4];

} // namespace UMC

using namespace UMC_H264_DECODER;

#endif //__UMC_H264_DEC_TABLES_H__
#endif // UMC_ENABLE_H264_VIDEO_DECODER
