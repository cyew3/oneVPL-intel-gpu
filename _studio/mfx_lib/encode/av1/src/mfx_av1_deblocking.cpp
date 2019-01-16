// Copyright (c) 2014-2019 Intel Corporation
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

#include "mfx_common.h"
#include "mfx_av1_defs.h"
#include "mfx_av1_enc.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_dispatcher_wrappers.h"
#include "mfx_av1_deblocking.h"
#include "mfx_av1_quant.h"

#include <smmintrin.h>
#include <algorithm>

#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

using namespace AV1Enc;

namespace AV1Enc {

static void filter_selectively_vert_row2(
    int subsampling_factor, uint8_t *s, int pitch, unsigned int mask_16x16,
    unsigned int mask_8x8, unsigned int mask_4x4, unsigned int mask_4x4_int,
    const LoopFilterThresh *lfthr, const uint8_t *lfl)
{
    const int dual_mask_cutoff = subsampling_factor ? 0xff : 0xffff;
    const int lfl_forward = subsampling_factor ? 4 : 8;
    const unsigned int dual_one = 1 | (1 << lfl_forward);
    unsigned int mask;
    uint8_t *ss[2];
    ss[0] = s;

    for (mask =
        (mask_16x16 | mask_8x8 | mask_4x4 | mask_4x4_int) & dual_mask_cutoff;
        mask; mask = (mask & ~dual_one) >> 1) {
            if (mask & dual_one) {
                const LoopFilterThresh *lfis[2];
                lfis[0] = lfthr + *lfl;
                lfis[1] = lfthr + *(lfl + lfl_forward);
                ss[1] = ss[0] + 8 * pitch;

                if (mask_16x16 & dual_one) {
                    if ((mask_16x16 & dual_one) == dual_one) {
                        AV1PP::lpf_vertical_16_dual(ss[0], pitch, lfis[0]->mblim, lfis[0]->lim,
                            lfis[0]->hev_thr);
                    } else {
                        const LoopFilterThresh *lfi = lfis[!(mask_16x16 & 1)];
                        AV1PP::lpf_vertical_16(ss[!(mask_16x16 & 1)], pitch, lfi->mblim,
                            lfi->lim, lfi->hev_thr);
                    }
                }

                if (mask_8x8 & dual_one) {
                    if ((mask_8x8 & dual_one) == dual_one) {
                        AV1PP::lpf_vertical_8_dual(ss[0], pitch, lfis[0]->mblim, lfis[0]->lim,
                            lfis[0]->hev_thr, lfis[1]->mblim,
                            lfis[1]->lim, lfis[1]->hev_thr);
                    } else {
                        const LoopFilterThresh *lfi = lfis[!(mask_8x8 & 1)];
                        AV1PP::lpf_vertical_8(ss[!(mask_8x8 & 1)], pitch, lfi->mblim, lfi->lim,
                            lfi->hev_thr);
                    }
                }

                if (mask_4x4 & dual_one) {
                    if ((mask_4x4 & dual_one) == dual_one) {
                        AV1PP::lpf_vertical_4_dual(ss[0], pitch, lfis[0]->mblim, lfis[0]->lim,
                            lfis[0]->hev_thr, lfis[1]->mblim,
                            lfis[1]->lim, lfis[1]->hev_thr);
                    } else {
                        const LoopFilterThresh *lfi = lfis[!(mask_4x4 & 1)];
                        AV1PP::lpf_vertical_4(ss[!(mask_4x4 & 1)], pitch, lfi->mblim, lfi->lim,
                            lfi->hev_thr);
                    }
                }

                if (mask_4x4_int & dual_one) {
                    if ((mask_4x4_int & dual_one) == dual_one) {
                        AV1PP::lpf_vertical_4_dual(
                            ss[0] + 4, pitch, lfis[0]->mblim, lfis[0]->lim, lfis[0]->hev_thr,
                            lfis[1]->mblim, lfis[1]->lim, lfis[1]->hev_thr);
                    } else {
                        const LoopFilterThresh *lfi = lfis[!(mask_4x4_int & 1)];
                        AV1PP::lpf_vertical_4(ss[!(mask_4x4_int & 1)] + 4, pitch, lfi->mblim,
                            lfi->lim, lfi->hev_thr);
                    }
                }
            }

            ss[0] += 8;
            lfl += 1;
            mask_16x16 >>= 1;
            mask_8x8 >>= 1;
            mask_4x4 >>= 1;
            mask_4x4_int >>= 1;
    }
}

#if CONFIG_AV1_HIGHBITDEPTH
static void highbd_filter_selectively_vert_row2(
    int subsampling_factor, uint16_t *s, int pitch, unsigned int mask_16x16,
    unsigned int mask_8x8, unsigned int mask_4x4, unsigned int mask_4x4_int,
    const LoopFilterThresh *lfthr, const uint8_t *lfl, int bd)
{
    const int dual_mask_cutoff = subsampling_factor ? 0xff : 0xffff;
    const int lfl_forward = subsampling_factor ? 4 : 8;
    const unsigned int dual_one = 1 | (1 << lfl_forward);
    unsigned int mask;
    uint16_t *ss[2];
    ss[0] = s;

    for (mask =
        (mask_16x16 | mask_8x8 | mask_4x4 | mask_4x4_int) & dual_mask_cutoff;
        mask; mask = (mask & ~dual_one) >> 1) {
            if (mask & dual_one) {
                const LoopFilterThresh *lfis[2];
                lfis[0] = lfthr + *lfl;
                lfis[1] = lfthr + *(lfl + lfl_forward);
                ss[1] = ss[0] + 8 * pitch;

                if (mask_16x16 & dual_one) {
                    if ((mask_16x16 & dual_one) == dual_one) {
                        AV1PP::lpf_vertical_16_dual(ss[0], pitch, lfis[0]->mblim,
                            lfis[0]->lim, lfis[0]->hev_thr, bd);
                    } else {
                        const LoopFilterThresh *lfi = lfis[!(mask_16x16 & 1)];
                        AV1PP::lpf_vertical_16(ss[!(mask_16x16 & 1)], pitch, lfi->mblim,
                            lfi->lim, lfi->hev_thr, bd);
                    }
                }

                if (mask_8x8 & dual_one) {
                    if ((mask_8x8 & dual_one) == dual_one) {
                        AV1PP::lpf_vertical_8_dual(
                            ss[0], pitch, lfis[0]->mblim, lfis[0]->lim, lfis[0]->hev_thr,
                            lfis[1]->mblim, lfis[1]->lim, lfis[1]->hev_thr, bd);
                    } else {
                        const LoopFilterThresh *lfi = lfis[!(mask_8x8 & 1)];
                        AV1PP::lpf_vertical_8(ss[!(mask_8x8 & 1)], pitch, lfi->mblim,
                            lfi->lim, lfi->hev_thr, bd);
                    }
                }

                if (mask_4x4 & dual_one) {
                    if ((mask_4x4 & dual_one) == dual_one) {
                        AV1PP::lpf_vertical_4_dual(
                            ss[0], pitch, lfis[0]->mblim, lfis[0]->lim, lfis[0]->hev_thr,
                            lfis[1]->mblim, lfis[1]->lim, lfis[1]->hev_thr, bd);
                    } else {
                        const LoopFilterThresh *lfi = lfis[!(mask_4x4 & 1)];
                        AV1PP::lpf_vertical_4(ss[!(mask_4x4 & 1)], pitch, lfi->mblim,
                            lfi->lim, lfi->hev_thr, bd);
                    }
                }

                if (mask_4x4_int & dual_one) {
                    if ((mask_4x4_int & dual_one) == dual_one) {
                        AV1PP::lpf_vertical_4_dual(
                            ss[0] + 4, pitch, lfis[0]->mblim, lfis[0]->lim, lfis[0]->hev_thr,
                            lfis[1]->mblim, lfis[1]->lim, lfis[1]->hev_thr, bd);
                    } else {
                        const LoopFilterThresh *lfi = lfis[!(mask_4x4_int & 1)];
                        AV1PP::lpf_vertical_4(ss[!(mask_4x4_int & 1)] + 4, pitch,
                            lfi->mblim, lfi->lim, lfi->hev_thr, bd);
                    }
                }
            }

            ss[0] += 8;
            lfl += 1;
            mask_16x16 >>= 1;
            mask_8x8 >>= 1;
            mask_4x4 >>= 1;
            mask_4x4_int >>= 1;
    }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

static void filter_selectively_horiz(
    uint8_t *s, int pitch, unsigned int mask_16x16, unsigned int mask_8x8,
    unsigned int mask_4x4, unsigned int mask_4x4_int,
    const LoopFilterThresh *lfthr, const uint8_t *lfl)
{
    unsigned int mask;
    int count;

    for (mask = mask_16x16 | mask_8x8 | mask_4x4 | mask_4x4_int; mask;
        mask >>= count) {
            count = 1;
            if (mask & 1) {
                const LoopFilterThresh *lfi = lfthr + *lfl;

                if (mask_16x16 & 1) {
                    if ((mask_16x16 & 3) == 3) {
                        AV1PP::lpf_horizontal_edge_16(s, pitch, lfi->mblim, lfi->lim,
                            lfi->hev_thr);
                        count = 2;
                    } else {
                        AV1PP::lpf_horizontal_edge_8(s, pitch, lfi->mblim, lfi->lim,
                            lfi->hev_thr);
                    }
                } else if (mask_8x8 & 1) {
                    if ((mask_8x8 & 3) == 3) {
                        // Next block's thresholds.
                        const LoopFilterThresh *lfin = lfthr + *(lfl + 1);

                        AV1PP::lpf_horizontal_8_dual(s, pitch, lfi->mblim, lfi->lim,
                            lfi->hev_thr, lfin->mblim, lfin->lim,
                            lfin->hev_thr);

                        if ((mask_4x4_int & 3) == 3) {
                            AV1PP::lpf_horizontal_4_dual(s + 4 * pitch, pitch, lfi->mblim,
                                lfi->lim, lfi->hev_thr, lfin->mblim,
                                lfin->lim, lfin->hev_thr);
                        } else {
                            if (mask_4x4_int & 1)
                                AV1PP::lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim, lfi->lim,
                                lfi->hev_thr);
                            else if (mask_4x4_int & 2)
                                AV1PP::lpf_horizontal_4(s + 8 + 4 * pitch, pitch, lfin->mblim,
                                lfin->lim, lfin->hev_thr);
                        }
                        count = 2;
                    } else {
                        AV1PP::lpf_horizontal_8(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr);

                        if (mask_4x4_int & 1)
                            AV1PP::lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim, lfi->lim,
                            lfi->hev_thr);
                    }
                } else if (mask_4x4 & 1) {
                    if ((mask_4x4 & 3) == 3) {
                        // Next block's thresholds.
                        const LoopFilterThresh *lfin = lfthr + *(lfl + 1);

                        AV1PP::lpf_horizontal_4_dual(s, pitch, lfi->mblim, lfi->lim,
                            lfi->hev_thr, lfin->mblim, lfin->lim,
                            lfin->hev_thr);
                        if ((mask_4x4_int & 3) == 3) {
                            AV1PP::lpf_horizontal_4_dual(s + 4 * pitch, pitch, lfi->mblim,
                                lfi->lim, lfi->hev_thr, lfin->mblim,
                                lfin->lim, lfin->hev_thr);
                        } else {
                            if (mask_4x4_int & 1)
                                AV1PP::lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim, lfi->lim,
                                lfi->hev_thr);
                            else if (mask_4x4_int & 2)
                                AV1PP::lpf_horizontal_4(s + 8 + 4 * pitch, pitch, lfin->mblim,
                                lfin->lim, lfin->hev_thr);
                        }
                        count = 2;
                    } else {
                        AV1PP::lpf_horizontal_4(s, pitch, lfi->mblim, lfi->lim, lfi->hev_thr);

                        if (mask_4x4_int & 1)
                            AV1PP::lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim, lfi->lim,
                            lfi->hev_thr);
                    }
                } else {
                    AV1PP::lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim, lfi->lim,
                        lfi->hev_thr);
                }
            }
            s += 8 * count;
            lfl += count;
            mask_16x16 >>= count;
            mask_8x8 >>= count;
            mask_4x4 >>= count;
            mask_4x4_int >>= count;
    }
}

#if CONFIG_AV1_HIGHBITDEPTH
static void filter_selectively_horiz(
    uint16_t *s, int pitch, unsigned int mask_16x16, unsigned int mask_8x8,
    unsigned int mask_4x4, unsigned int mask_4x4_int,
    const LoopFilterThresh *lfthr, const uint8_t *lfl, int bd)
{
    unsigned int mask;
    int count;

    for (mask = mask_16x16 | mask_8x8 | mask_4x4 | mask_4x4_int; mask;
        mask >>= count) {
            count = 1;
            if (mask & 1) {
                const LoopFilterThresh *lfi = lfthr + *lfl;

                if (mask_16x16 & 1) {
                    if ((mask_16x16 & 3) == 3) {
                        AV1PP::lpf_horizontal_edge_16(s, pitch, lfi->mblim, lfi->lim,
                            lfi->hev_thr, bd);
                        count = 2;
                    } else {
                        AV1PP::lpf_horizontal_edge_8(s, pitch, lfi->mblim, lfi->lim,
                            lfi->hev_thr, bd);
                    }
                } else if (mask_8x8 & 1) {
                    if ((mask_8x8 & 3) == 3) {
                        // Next block's thresholds.
                        const LoopFilterThresh *lfin = lfthr + *(lfl + 1);

                        AV1PP::lpf_horizontal_8_dual(s, pitch, lfi->mblim, lfi->lim,
                            lfi->hev_thr, lfin->mblim, lfin->lim,
                            lfin->hev_thr, bd);

                        if ((mask_4x4_int & 3) == 3) {
                            AV1PP::lpf_horizontal_4_dual(
                                s + 4 * pitch, pitch, lfi->mblim, lfi->lim, lfi->hev_thr,
                                lfin->mblim, lfin->lim, lfin->hev_thr, bd);
                        } else {
                            if (mask_4x4_int & 1) {
                                AV1PP::lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim,
                                    lfi->lim, lfi->hev_thr, bd);
                            } else if (mask_4x4_int & 2) {
                                AV1PP::lpf_horizontal_4(s + 8 + 4 * pitch, pitch, lfin->mblim,
                                    lfin->lim, lfin->hev_thr, bd);
                            }
                        }
                        count = 2;
                    } else {
                        AV1PP::lpf_horizontal_8(s, pitch, lfi->mblim, lfi->lim,
                            lfi->hev_thr, bd);

                        if (mask_4x4_int & 1) {
                            AV1PP::lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim,
                                lfi->lim, lfi->hev_thr, bd);
                        }
                    }
                } else if (mask_4x4 & 1) {
                    if ((mask_4x4 & 3) == 3) {
                        // Next block's thresholds.
                        const LoopFilterThresh *lfin = lfthr + *(lfl + 1);

                        AV1PP::lpf_horizontal_4_dual(s, pitch, lfi->mblim, lfi->lim,
                            lfi->hev_thr, lfin->mblim, lfin->lim,
                            lfin->hev_thr, bd);
                        if ((mask_4x4_int & 3) == 3) {
                            AV1PP::lpf_horizontal_4_dual(
                                s + 4 * pitch, pitch, lfi->mblim, lfi->lim, lfi->hev_thr,
                                lfin->mblim, lfin->lim, lfin->hev_thr, bd);
                        } else {
                            if (mask_4x4_int & 1) {
                                AV1PP::lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim,
                                    lfi->lim, lfi->hev_thr, bd);
                            } else if (mask_4x4_int & 2) {
                                AV1PP::lpf_horizontal_4(s + 8 + 4 * pitch, pitch, lfin->mblim,
                                    lfin->lim, lfin->hev_thr, bd);
                            }
                        }
                        count = 2;
                    } else {
                        AV1PP::lpf_horizontal_4(s, pitch, lfi->mblim, lfi->lim,
                            lfi->hev_thr, bd);

                        if (mask_4x4_int & 1) {
                            AV1PP::lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim,
                                lfi->lim, lfi->hev_thr, bd);
                        }
                    }
                } else {
                    AV1PP::lpf_horizontal_4(s + 4 * pitch, pitch, lfi->mblim, lfi->lim,
                        lfi->hev_thr, bd);
                }
            }
            s += 8 * count;
            lfl += count;
            mask_16x16 >>= count;
            mask_8x8 >>= count;
            mask_4x4 >>= count;
            mask_4x4_int >>= count;
    }
}
#endif  // CONFIG_AV1_HIGHBITDEPTH

void filter_block_plane_ss00(uint8_t *dst0, int32_t stride, int32_t mi_row, int32_t mi_rows, const LoopFilterThresh *lfthr, LoopFilterMask *lfm)
{
    uint8_t *dst = dst0;
    int32_t r;
    uint64_t mask_16x16 = lfm->left_y[TX_16X16];
    uint64_t mask_8x8 = lfm->left_y[TX_8X8];
    uint64_t mask_4x4 = lfm->left_y[TX_4X4];
    uint64_t mask_4x4_int = lfm->int_4x4_y;
    int32_t subsampling_x = 0, subsampling_y = 0;

    //  assert(plane->subsampling_x == 0 && plane->subsampling_y == 0);

    // Vertical pass: do 2 rows at one time
    for (r = 0; r < 8 && mi_row + r < mi_rows; r += 2) {
#if CONFIG_AV1_HIGHBITDEPTH
        if (cm->use_highbitdepth) {
            // Disable filtering on the leftmost column.
            highbd_filter_selectively_vert_row2(
                plane->subsampling_x, CONVERT_TO_SHORTPTR(dst->buf), dst->stride,
                (unsigned int)mask_16x16, (unsigned int)mask_8x8,
                (unsigned int)mask_4x4, (unsigned int)mask_4x4_int, lfthr,
                &lfm->lfl_y[r << 3], (int)cm->bit_depth);
        } else {
#endif  // CONFIG_AV1_HIGHBITDEPTH
            // Disable filtering on the leftmost column.
            filter_selectively_vert_row2(
                subsampling_x, dst, stride, (unsigned int)mask_16x16,
                (unsigned int)mask_8x8, (unsigned int)mask_4x4,
                (unsigned int)mask_4x4_int, lfthr, &lfm->lfl_y[r << 3]);
#if CONFIG_AV1_HIGHBITDEPTH
        }
#endif  // CONFIG_AV1_HIGHBITDEPTH
        dst += 16 * stride;
        mask_16x16 >>= 16;
        mask_8x8 >>= 16;
        mask_4x4 >>= 16;
        mask_4x4_int >>= 16;
    }

    // Horizontal pass
    dst = dst0;
    mask_16x16 = lfm->above_y[TX_16X16];
    mask_8x8 = lfm->above_y[TX_8X8];
    mask_4x4 = lfm->above_y[TX_4X4];
    mask_4x4_int = lfm->int_4x4_y;

    for (r = 0; r < 8 && mi_row + r < mi_rows; r++) {
        unsigned int mask_16x16_r;
        unsigned int mask_8x8_r;
        unsigned int mask_4x4_r;

        if (mi_row + r == 0) {
            mask_16x16_r = 0;
            mask_8x8_r = 0;
            mask_4x4_r = 0;
        } else {
            mask_16x16_r = mask_16x16 & 0xff;
            mask_8x8_r = mask_8x8 & 0xff;
            mask_4x4_r = mask_4x4 & 0xff;
        }

#if CONFIG_AV1_HIGHBITDEPTH
        if (cm->use_highbitdepth) {
            highbd_filter_selectively_horiz(
                CONVERT_TO_SHORTPTR(dst->buf), dst->stride, mask_16x16_r, mask_8x8_r,
                mask_4x4_r, mask_4x4_int & 0xff, lfthr,
                &lfm->lfl_y[r << 3], (int)cm->bit_depth);
        } else {
#endif  // CONFIG_AV1_HIGHBITDEPTH
            filter_selectively_horiz(dst, stride, mask_16x16_r, mask_8x8_r,
                mask_4x4_r, mask_4x4_int & 0xff,
                lfthr, &lfm->lfl_y[r << 3]);
#if CONFIG_AV1_HIGHBITDEPTH
        }
#endif  // CONFIG_AV1_HIGHBITDEPTH

        dst += 8 * stride;
        mask_16x16 >>= 8;
        mask_8x8 >>= 8;
        mask_4x4 >>= 8;
        mask_4x4_int >>= 8;
    }
}

void filter_block_plane_ss11(uint8_t *dst0, int32_t stride, int32_t mi_row, int32_t mi_rows, const LoopFilterThresh *lfthr, LoopFilterMask *lfm)
{
    uint8_t *dst = dst0;
    int32_t r, c;
    uint8_t lfl_uv[16];

    uint16_t mask_16x16 = lfm->left_uv[TX_16X16];
    uint16_t mask_8x8 = lfm->left_uv[TX_8X8];
    uint16_t mask_4x4 = lfm->left_uv[TX_4X4];
    uint16_t mask_4x4_int = lfm->int_4x4_uv;
    int32_t subsampling_x = 1, subsampling_y = 1;

    //  assert(plane->subsampling_x == 1 && plane->subsampling_y == 1);

    // Vertical pass: do 2 rows at one time
    for (r = 0; r < 8 && mi_row + r < mi_rows; r += 4) {
        for (c = 0; c < (8 >> 1); c++) {
            lfl_uv[(r << 1) + c] = lfm->lfl_y[(r << 3) + (c << 1)];
            lfl_uv[((r + 2) << 1) + c] = lfm->lfl_y[((r + 2) << 3) + (c << 1)];
        }

#if CONFIG_AV1_HIGHBITDEPTH
        if (cm->use_highbitdepth) {
            // Disable filtering on the leftmost column.
            highbd_filter_selectively_vert_row2(
                plane->subsampling_x, CONVERT_TO_SHORTPTR(dst->buf), dst->stride,
                (unsigned int)mask_16x16, (unsigned int)mask_8x8,
                (unsigned int)mask_4x4, (unsigned int)mask_4x4_int, cm->lf_info.lfthr,
                &lfl_uv[r << 1], (int)cm->bit_depth);
        } else {
#endif  // CONFIG_AV1_HIGHBITDEPTH
            // Disable filtering on the leftmost column.
            filter_selectively_vert_row2(
                subsampling_x, dst, stride, (unsigned int)mask_16x16,
                (unsigned int)mask_8x8, (unsigned int)mask_4x4,
                (unsigned int)mask_4x4_int, lfthr, &lfl_uv[r << 1]);
#if CONFIG_AV1_HIGHBITDEPTH
        }
#endif  // CONFIG_AV1_HIGHBITDEPTH

        dst += 16 * stride;
        mask_16x16 >>= 8;
        mask_8x8 >>= 8;
        mask_4x4 >>= 8;
        mask_4x4_int >>= 8;
    }

    // Horizontal pass
    dst = dst0;
    mask_16x16 = lfm->above_uv[TX_16X16];
    mask_8x8 = lfm->above_uv[TX_8X8];
    mask_4x4 = lfm->above_uv[TX_4X4];
    mask_4x4_int = lfm->int_4x4_uv;

    for (r = 0; r < 8 && mi_row + r < mi_rows; r += 2) {
        const int skip_border_4x4_r = mi_row + r == mi_rows - 1;
        const unsigned int mask_4x4_int_r =
            skip_border_4x4_r ? 0 : (mask_4x4_int & 0xf);
        unsigned int mask_16x16_r;
        unsigned int mask_8x8_r;
        unsigned int mask_4x4_r;

        if (mi_row + r == 0) {
            mask_16x16_r = 0;
            mask_8x8_r = 0;
            mask_4x4_r = 0;
        } else {
            mask_16x16_r = mask_16x16 & 0xf;
            mask_8x8_r = mask_8x8 & 0xf;
            mask_4x4_r = mask_4x4 & 0xf;
        }

#if CONFIG_AV1_HIGHBITDEPTH
        if (cm->use_highbitdepth) {
            highbd_filter_selectively_horiz(
                CONVERT_TO_SHORTPTR(dst->buf), dst->stride, mask_16x16_r, mask_8x8_r,
                mask_4x4_r, mask_4x4_int_r, cm->lf_info.lfthr, &lfl_uv[r << 1],
                (int)cm->bit_depth);
        } else {
#endif  // CONFIG_AV1_HIGHBITDEPTH
            filter_selectively_horiz(dst, stride, mask_16x16_r, mask_8x8_r,
                mask_4x4_r, mask_4x4_int_r, lfthr,
                &lfl_uv[r << 1]);
#if CONFIG_AV1_HIGHBITDEPTH
        }
#endif  // CONFIG_AV1_HIGHBITDEPTH

        dst += 8 * stride;
        mask_16x16 >>= 4;
        mask_8x8 >>= 4;
        mask_4x4 >>= 4;
        mask_4x4_int >>= 4;
    }
}

// These are used for masking the left and above borders.
static const uint64_t left_border = 0x1111111111111111ULL;
static const uint64_t above_border = 0x000000ff000000ffULL;
static const uint16_t left_border_uv = 0x1111;
static const uint16_t above_border_uv = 0x000f;
static const int mode_lf_lut[AV1_MB_MODE_COUNT] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // INTRA_MODES
    0,
    0, 0,
    1, 1, 0, 1,                     // INTER_MODES (ZEROMV == 0)
    1, 1, 1, 1, 1, 1, 0, 1  // INTER_COMPOUND_MODES (ZERO_ZEROMV == 0)
};

// 64 bit masks for prediction sizes (left). Each 1 represents a position
// where left border of an 8x8 block. These are aligned to the right most
// appropriate bit, and then shifted into place.
//
// In the case of TX_16x32 ->  ( low order byte first ) we end up with
// a mask that looks like this :
//
//  10000000
//  10000000
//  10000000
//  10000000
//  00000000
//  00000000
//  00000000
//  00000000
static const uint64_t left_prediction_mask[BLOCK_SIZES_ALL] = {
    0x0000000000000001ULL,  // BLOCK_2X2,
    0x0000000000000001ULL,  // BLOCK_2X4,
    0x0000000000000001ULL,  // BLOCK_4X2,
    0x0000000000000001ULL,  // BLOCK_4X4,
    0x0000000000000001ULL,  // BLOCK_4X8,
    0x0000000000000001ULL,  // BLOCK_8X4,
    0x0000000000000001ULL,  // BLOCK_8X8,
    0x0000000000000101ULL,  // BLOCK_8X16,
    0x0000000000000001ULL,  // BLOCK_16X8,
    0x0000000000000101ULL,  // BLOCK_16X16,
    0x0000000001010101ULL,  // BLOCK_16X32,
    0x0000000000000101ULL,  // BLOCK_32X16,
    0x0000000001010101ULL,  // BLOCK_32X32,
    0x0101010101010101ULL,  // BLOCK_32X64,
    0x0000000001010101ULL,  // BLOCK_64X32,
    0x0101010101010101ULL,  // BLOCK_64X64
    0x0000000000000101ULL,  // BLOCK_4X16,
    0x0000000000000001ULL,  // BLOCK_16X4,
    0x0000000001010101ULL,  // BLOCK_8X32,
    0x0000000000000001ULL,  // BLOCK_32X8,
    0x0101010101010101ULL,  // BLOCK_16X64,
    0x0000000000000101ULL,  // BLOCK_64X16
};

// 64 bit mask to shift and set for each prediction size.
static const uint64_t above_prediction_mask[BLOCK_SIZES_ALL] = {
    0x0000000000000001ULL,  // BLOCK_2X2
    0x0000000000000001ULL,  // BLOCK_2X4
    0x0000000000000001ULL,  // BLOCK_4X2
    0x0000000000000001ULL,  // BLOCK_4X4
    0x0000000000000001ULL,  // BLOCK_4X8
    0x0000000000000001ULL,  // BLOCK_8X4
    0x0000000000000001ULL,  // BLOCK_8X8
    0x0000000000000001ULL,  // BLOCK_8X16,
    0x0000000000000003ULL,  // BLOCK_16X8
    0x0000000000000003ULL,  // BLOCK_16X16
    0x0000000000000003ULL,  // BLOCK_16X32,
    0x000000000000000fULL,  // BLOCK_32X16,
    0x000000000000000fULL,  // BLOCK_32X32,
    0x000000000000000fULL,  // BLOCK_32X64,
    0x00000000000000ffULL,  // BLOCK_64X32,
    0x00000000000000ffULL,  // BLOCK_64X64
    0x0000000000000001ULL,  // BLOCK_4X16,
    0x0000000000000003ULL,  // BLOCK_16X4,
    0x0000000000000001ULL,  // BLOCK_8X32,
    0x000000000000000fULL,  // BLOCK_32X8,
    0x0000000000000003ULL,  // BLOCK_16X64,
    0x00000000000000ffULL,  // BLOCK_64X16
};
// 64 bit mask to shift and set for each prediction size. A bit is set for
// each 8x8 block that would be in the left most block of the given block
// size in the 64x64 block.
static const uint64_t size_mask[BLOCK_SIZES_ALL] = {
    0x0000000000000001ULL,  // BLOCK_2X2
    0x0000000000000001ULL,  // BLOCK_2X4
    0x0000000000000001ULL,  // BLOCK_4X2
    0x0000000000000001ULL,  // BLOCK_4X4
    0x0000000000000001ULL,  // BLOCK_4X8
    0x0000000000000001ULL,  // BLOCK_8X4
    0x0000000000000001ULL,  // BLOCK_8X8
    0x0000000000000101ULL,  // BLOCK_8X16,
    0x0000000000000003ULL,  // BLOCK_16X8
    0x0000000000000303ULL,  // BLOCK_16X16
    0x0000000003030303ULL,  // BLOCK_16X32,
    0x0000000000000f0fULL,  // BLOCK_32X16,
    0x000000000f0f0f0fULL,  // BLOCK_32X32,
    0x0f0f0f0f0f0f0f0fULL,  // BLOCK_32X64,
    0x00000000ffffffffULL,  // BLOCK_64X32,
    0xffffffffffffffffULL,  // BLOCK_64X64
    0x0000000000000101ULL,  // BLOCK_4X16,
    0x0000000000000003ULL,  // BLOCK_16X4,
    0x0000000001010101ULL,  // BLOCK_8X32,
    0x000000000000000fULL,  // BLOCK_32X8,
    0x0303030303030303ULL,  // BLOCK_16X64,
    0x000000000000ffffULL,  // BLOCK_64X16
};
// 16 bit left mask to shift and set for each uv prediction size.
static const uint16_t left_prediction_mask_uv[BLOCK_SIZES_ALL] = {
    0x0001,  // BLOCK_2X2,
    0x0001,  // BLOCK_2X4,
    0x0001,  // BLOCK_4X2,
    0x0001,  // BLOCK_4X4,
    0x0001,  // BLOCK_4X8,
    0x0001,  // BLOCK_8X4,
    0x0001,  // BLOCK_8X8,
    0x0001,  // BLOCK_8X16,
    0x0001,  // BLOCK_16X8,
    0x0001,  // BLOCK_16X16,
    0x0011,  // BLOCK_16X32,
    0x0001,  // BLOCK_32X16,
    0x0011,  // BLOCK_32X32,
    0x1111,  // BLOCK_32X64
    0x0011,  // BLOCK_64X32,
    0x1111,  // BLOCK_64X64
    0x0001,  // BLOCK_4X16,
    0x0001,  // BLOCK_16X4,
    0x0011,  // BLOCK_8X32,
    0x0001,  // BLOCK_32X8,
    0x1111,  // BLOCK_16X64,
    0x0001,  // BLOCK_64X16,
};
// 16 bit above mask to shift and set for uv each prediction size.
static const uint16_t above_prediction_mask_uv[BLOCK_SIZES_ALL] = {
    0x0001,  // BLOCK_2X2
    0x0001,  // BLOCK_2X4
    0x0001,  // BLOCK_4X2
    0x0001,  // BLOCK_4X4
    0x0001,  // BLOCK_4X8
    0x0001,  // BLOCK_8X4
    0x0001,  // BLOCK_8X8
    0x0001,  // BLOCK_8X16,
    0x0001,  // BLOCK_16X8
    0x0001,  // BLOCK_16X16
    0x0001,  // BLOCK_16X32,
    0x0003,  // BLOCK_32X16,
    0x0003,  // BLOCK_32X32,
    0x0003,  // BLOCK_32X64,
    0x000f,  // BLOCK_64X32,
    0x000f,  // BLOCK_64X64
    0x0001,  // BLOCK_4X16,
    0x0001,  // BLOCK_16X4,
    0x0001,  // BLOCK_8X32,
    0x0003,  // BLOCK_32X8,
    0x0001,  // BLOCK_16X64,
    0x000f,  // BLOCK_64X16
};

// 64 bit mask to shift and set for each uv prediction size
static const uint16_t size_mask_uv[BLOCK_SIZES_ALL] = {
    0x0001,  // BLOCK_2X2
    0x0001,  // BLOCK_2X4
    0x0001,  // BLOCK_4X2
    0x0001,  // BLOCK_4X4
    0x0001,  // BLOCK_4X8
    0x0001,  // BLOCK_8X4
    0x0001,  // BLOCK_8X8
    0x0001,  // BLOCK_8X16,
    0x0001,  // BLOCK_16X8
    0x0001,  // BLOCK_16X16
    0x0011,  // BLOCK_16X32,
    0x0003,  // BLOCK_32X16,
    0x0033,  // BLOCK_32X32,
    0x3333,  // BLOCK_32X64,
    0x00ff,  // BLOCK_64X32,
    0xffff,  // BLOCK_64X64
    0x0001,  // BLOCK_4X16,
    0x0001,  // BLOCK_16X4,
    0x0011,  // BLOCK_8X32,
    0x0003,  // BLOCK_32X8,
    0x1111,  // BLOCK_16X64,
    0x000f,  // BLOCK_64X16
};
// 64 bit masks for left transform size. Each 1 represents a position where
// we should apply a loop filter across the left border of an 8x8 block
// boundary.
//
// In the case of TX_16X16->  ( in low order byte first we end up with
// a mask that looks like this
//
//    10101010
//    10101010
//    10101010
//    10101010
//    10101010
//    10101010
//    10101010
//    10101010
//
// A loopfilter should be applied to every other 8x8 horizontally.
static const uint64_t left_64x64_txform_mask[TX_SIZES] = {
  0xffffffffffffffffULL,  // TX_4X4
  0xffffffffffffffffULL,  // TX_8x8
  0x5555555555555555ULL,  // TX_16x16
  0x1111111111111111ULL,  // TX_32x32
};

// 64 bit masks for above transform size. Each 1 represents a position where
// we should apply a loop filter across the top border of an 8x8 block
// boundary.
//
// In the case of TX_32x32 ->  ( in low order byte first we end up with
// a mask that looks like this
//
//    11111111
//    00000000
//    00000000
//    00000000
//    11111111
//    00000000
//    00000000
//    00000000
//
// A loopfilter should be applied to every other 4 the row vertically.
static const uint64_t above_64x64_txform_mask[TX_SIZES] = {
    0xffffffffffffffffULL,  // TX_4X4
    0xffffffffffffffffULL,  // TX_8x8
    0x00ff00ff00ff00ffULL,  // TX_16x16
    0x000000ff000000ffULL,  // TX_32x32
};
// 16 bit masks for uv transform sizes.
static const uint16_t left_64x64_txform_mask_uv[TX_SIZES] = {
    0xffff,  // TX_4X4
    0xffff,  // TX_8x8
    0x5555,  // TX_16x16
    0x1111,  // TX_32x32
};

static const uint16_t above_64x64_txform_mask_uv[TX_SIZES] = {
    0xffff,  // TX_4X4
    0xffff,  // TX_8x8
    0x0f0f,  // TX_16x16
    0x000f,  // TX_32x32
};

#define MI_BLOCK_SIZE 8
#define MI_BLOCK_SIZE_LOG2 3

static uint8_t get_filter_level(const LoopFilterInfoN *lfi_n, const ModeInfo *mi)
{
    const int32_t segId = 0;
    return lfi_n->lvl[segId][mi->refIdx[0] < 0 ? 0 : mi->refIdx[0] + 1][mode_lf_lut[mi->mode]];
}

static void fill_levels(const LoopFilterInfoN *const lfi_n, const ModeInfo *mi, const int shift_y,
                        const int shift_uv, LoopFilterMask *lfm, const AV1VideoParam &par, int y_only)
{
    const BlockSize block_size = mi->sbType;
    const int filter_level = get_filter_level(lfi_n, mi);
    const int w = block_size_wide_8x8[block_size];
    const int h = block_size_high_8x8[block_size];
    int index = shift_y;
    for (int i = 0; i < h; i++) {
        memset(&lfm->lfl_y[index], filter_level, w);
        index += 8;
    }

    if (filter_level == 0) {
        const TxSize tx_size_y = mi->txSize;
        const TxSize tx_size_uv = GetUvTxSize(block_size, tx_size_y, par);
        uint64_t *const left_y = &lfm->left_y[tx_size_y];
        uint64_t *const above_y = &lfm->above_y[tx_size_y];
        uint64_t *const int_4x4_y = &lfm->int_4x4_y;
        uint16_t *const left_uv = &lfm->left_uv[tx_size_uv];
        uint16_t *const above_uv = &lfm->above_uv[tx_size_uv];
        uint16_t *const int_4x4_uv = &lfm->int_4x4_uv;
        // These set 1 in the current block size for the block size edges.
        // For instance if the block size is 32x16, we'll set:
        //    above =   1111
        //              0000
        //    and
        //    left  =   1000
        //          =   1000
        // NOTE : In this example the low bit is left most ( 1000 ) is stored as
        //        1,  not 8...
        //
        // U and V set things on a 16 bit scale.
        //
        *above_y &= ~(above_prediction_mask[block_size] << shift_y);
        *left_y &= ~(left_prediction_mask[block_size] << shift_y);

        if (!y_only) {
            *above_uv &= ~(above_prediction_mask_uv[block_size] << shift_uv);
            *left_uv &= ~(left_prediction_mask_uv[block_size] << shift_uv);
        }

        // If the block has no coefficients and is not intra we skip applying
        // the loop filter on block edges.
        if (mi->skip && mi->refIdx[0] > INTRA_FRAME) return;

        // Here we are adding a mask for the transform size. The transform
        // size mask is set to be correct for a 64x64 prediction block size. We
        // mask to match the size of the block we are working on and then shift it
        // into place..
        *above_y &= ~((size_mask[block_size] & above_64x64_txform_mask[tx_size_y])
            << shift_y);
        *left_y &= ~((size_mask[block_size] & left_64x64_txform_mask[tx_size_y])
            << shift_y);

        if (!y_only) {
            *left_uv &= ~((size_mask_uv[block_size] & left_64x64_txform_mask_uv[tx_size_uv])
                << shift_uv);
            *above_uv &=
                ~((size_mask_uv[block_size] & above_64x64_txform_mask_uv[tx_size_uv])
                << shift_uv);
        }

        // Here we are trying to determine what to do with the internal 4x4 block
        // boundaries.  These differ from the 4x4 boundaries on the outside edge of
        // an 8x8 in that the internal ones can be skipped and don't depend on
        // the prediction block size.
        if (tx_size_y == TX_4X4) *int_4x4_y &= ~(size_mask[block_size] << shift_y);

        if (!y_only) {
            if (tx_size_uv == TX_4X4)
                *int_4x4_uv &= ~((size_mask_uv[block_size] & 0xffff) << shift_uv);
        }
    }
}

// This function ors into the current lfm structure, where to do loop
// filters for the specific mi we are looking at. It uses information
// including the block_size_type (32x16, 32x32, etc.), the transform size,
// whether there were any coefficients encoded, and the loop filter strength
// block we are currently looking at. Shift is used to position the
// 1's we produce.


static void build_masks(const ModeInfo *mi,
                        const int shift_y,
                        const int shift_uv,
                        LoopFilterMask *lfm,
                        AV1VideoParam &par)
{
    const BlockSize block_size = mi->sbType;
    const TxSize tx_size_y = mi->txSize;
    const TxSize tx_size_uv = GetUvTxSize(block_size, tx_size_y, par);
    uint64_t *const left_y = &lfm->left_y[tx_size_y];
    uint64_t *const above_y = &lfm->above_y[tx_size_y];
    uint64_t *const int_4x4_y = &lfm->int_4x4_y;
    uint16_t *const left_uv = &lfm->left_uv[tx_size_uv];
    uint16_t *const above_uv = &lfm->above_uv[tx_size_uv];
    uint16_t *const int_4x4_uv = &lfm->int_4x4_uv;
    int i;

    // These set 1 in the current block size for the block size edges.
    // For instance if the block size is 32x16, we'll set:
    //    above =   1111
    //              0000
    //    and
    //    left  =   1000
    //          =   1000
    // NOTE : In this example the low bit is left most ( 1000 ) is stored as
    //        1,  not 8...
    //
    // U and V set things on a 16 bit scale.
    //
    *above_y |= above_prediction_mask[block_size] << shift_y;
    *above_uv |= above_prediction_mask_uv[block_size] << shift_uv;
    *left_y |= left_prediction_mask[block_size] << shift_y;
    *left_uv |= left_prediction_mask_uv[block_size] << shift_uv;

    // If the block has no coefficients and is not intra we skip applying
    // the loop filter on block edges.
    if (mi->skip && mi->refIdx[0] > INTRA_FRAME) return;

    // Here we are adding a mask for the transform size. The transform
    // size mask is set to be correct for a 64x64 prediction block size. We
    // mask to match the size of the block we are working on and then shift it
    // into place..
    *above_y |= (size_mask[block_size] & above_64x64_txform_mask[tx_size_y])
        << shift_y;
    *above_uv |=
        (size_mask_uv[block_size] & above_64x64_txform_mask_uv[tx_size_uv])
        << shift_uv;

    *left_y |= (size_mask[block_size] & left_64x64_txform_mask[tx_size_y])
        << shift_y;
    *left_uv |= (size_mask_uv[block_size] & left_64x64_txform_mask_uv[tx_size_uv])
        << shift_uv;

    // Here we are trying to determine what to do with the internal 4x4 block
    // boundaries.  These differ from the 4x4 boundaries on the outside edge of
    // an 8x8 in that the internal ones can be skipped and don't depend on
    // the prediction block size.
    if (tx_size_y == TX_4X4) *int_4x4_y |= size_mask[block_size] << shift_y;

    if (tx_size_uv == TX_4X4)
        *int_4x4_uv |= (size_mask_uv[block_size] & 0xffff) << shift_uv;
}

void LoopFilterAdjustMask(const int32_t mi_row, const int32_t mi_col, int32_t mi_rows, int32_t mi_cols, LoopFilterMask *lfm)
{
    int32_t i;

    // The largest loopfilter we have is 16x16 so we use the 16x16 mask
    // for 32x32 transforms also.
    lfm->left_y[TX_16X16] |= lfm->left_y[TX_32X32];
    lfm->above_y[TX_16X16] |= lfm->above_y[TX_32X32];
    lfm->left_uv[TX_16X16] |= lfm->left_uv[TX_32X32];
    lfm->above_uv[TX_16X16] |= lfm->above_uv[TX_32X32];

    // We do at least 8 tap filter on every 32x32 even if the transform size
    // is 4x4. So if the 4x4 is set on a border pixel add it to the 8x8 and
    // remove it from the 4x4.
    lfm->left_y[TX_8X8] |= lfm->left_y[TX_4X4] & left_border;
    lfm->left_y[TX_4X4] &= ~left_border;
    lfm->above_y[TX_8X8] |= lfm->above_y[TX_4X4] & above_border;
    lfm->above_y[TX_4X4] &= ~above_border;
    lfm->left_uv[TX_8X8] |= lfm->left_uv[TX_4X4] & left_border_uv;
    lfm->left_uv[TX_4X4] &= ~left_border_uv;
    lfm->above_uv[TX_8X8] |= lfm->above_uv[TX_4X4] & above_border_uv;
    lfm->above_uv[TX_4X4] &= ~above_border_uv;

    // We do some special edge handling.
    if (mi_row + MI_BLOCK_SIZE > mi_rows) {
        const uint64_t rows = mi_rows - mi_row;

        // Each pixel inside the border gets a 1,
        const uint64_t mask_y = (((uint64_t)1 << (rows << 3)) - 1);
        const uint16_t mask_uv = (((uint16_t)1 << (((rows + 1) >> 1) << 2)) - 1);

        // Remove values completely outside our border.
        for (i = 0; i < TX_32X32; i++) {
            lfm->left_y[i] &= mask_y;
            lfm->above_y[i] &= mask_y;
            lfm->left_uv[i] &= mask_uv;
            lfm->above_uv[i] &= mask_uv;
        }
        lfm->int_4x4_y &= mask_y;
        lfm->int_4x4_uv &= mask_uv;

        // We don't apply a wide loop filter on the last uv block row. If set
        // apply the shorter one instead.
        if (rows == 1) {
            lfm->above_uv[TX_8X8] |= lfm->above_uv[TX_16X16];
            lfm->above_uv[TX_16X16] = 0;
        }
        if (rows == 5) {
            lfm->above_uv[TX_8X8] |= lfm->above_uv[TX_16X16] & 0xff00;
            lfm->above_uv[TX_16X16] &= ~(lfm->above_uv[TX_16X16] & 0xff00);
        }
    }

    if (mi_col + MI_BLOCK_SIZE > mi_cols) {
        const uint64_t columns = mi_cols - mi_col;

        // Each pixel inside the border gets a 1, the multiply copies the border
        // to where we need it.
        const uint64_t mask_y = (((1 << columns) - 1)) * 0x0101010101010101ULL;
        const uint16_t mask_uv = ((1 << ((columns + 1) >> 1)) - 1) * 0x1111;

        // Internal edges are not applied on the last column of the image so
        // we mask 1 more for the internal edges
        const uint16_t mask_uv_int = ((1 << (columns >> 1)) - 1) * 0x1111;

        // Remove the bits outside the image edge.
        for (i = 0; i < TX_32X32; i++) {
            lfm->left_y[i] &= mask_y;
            lfm->above_y[i] &= mask_y;
            lfm->left_uv[i] &= mask_uv;
            lfm->above_uv[i] &= mask_uv;
        }
        lfm->int_4x4_y &= mask_y;
        lfm->int_4x4_uv &= mask_uv_int;

        // We don't apply a wide loop filter on the last uv column. If set
        // apply the shorter one instead.
        if (columns == 1) {
            lfm->left_uv[TX_8X8] |= lfm->left_uv[TX_16X16];
            lfm->left_uv[TX_16X16] = 0;
        }
        if (columns == 5) {
            lfm->left_uv[TX_8X8] |= (lfm->left_uv[TX_16X16] & 0xcccc);
            lfm->left_uv[TX_16X16] &= ~(lfm->left_uv[TX_16X16] & 0xcccc);
        }
    }
    // We don't apply a loop filter on the first column in the image, mask that
    // out.
    if (mi_col == 0) {
        for (i = 0; i < TX_32X32; i++) {
            lfm->left_y[i] &= 0xfefefefefefefefeULL;
            lfm->left_uv[i] &= 0xeeee;
        }
    }

    // Assert if we try to apply 2 different loop filters at the same position.
    assert(!(lfm->left_y[TX_16X16] & lfm->left_y[TX_8X8]));
    assert(!(lfm->left_y[TX_16X16] & lfm->left_y[TX_4X4]));
    assert(!(lfm->left_y[TX_8X8] & lfm->left_y[TX_4X4]));
    assert(!(lfm->int_4x4_y & lfm->left_y[TX_16X16]));
    assert(!(lfm->left_uv[TX_16X16] & lfm->left_uv[TX_8X8]));
    assert(!(lfm->left_uv[TX_16X16] & lfm->left_uv[TX_4X4]));
    assert(!(lfm->left_uv[TX_8X8] & lfm->left_uv[TX_4X4]));
    assert(!(lfm->int_4x4_uv & lfm->left_uv[TX_16X16]));
    assert(!(lfm->above_y[TX_16X16] & lfm->above_y[TX_8X8]));
    assert(!(lfm->above_y[TX_16X16] & lfm->above_y[TX_4X4]));
    assert(!(lfm->above_y[TX_8X8] & lfm->above_y[TX_4X4]));
    assert(!(lfm->int_4x4_y & lfm->above_y[TX_16X16]));
    assert(!(lfm->above_uv[TX_16X16] & lfm->above_uv[TX_8X8]));
    assert(!(lfm->above_uv[TX_16X16] & lfm->above_uv[TX_4X4]));
    assert(!(lfm->above_uv[TX_8X8] & lfm->above_uv[TX_4X4]));
    assert(!(lfm->int_4x4_uv & lfm->above_uv[TX_16X16]));
}

// This function does the same thing as the one above with the exception that
// it only affects the y masks. It exists because for blocks < 16x16 in size,
// we only update u and v masks on the first block.
static void build_y_mask(const ModeInfo *mi,
                         const int shift_y,
                         LoopFilterMask *lfm)
{
    const BlockSize block_size = mi->sbType;
    const TxSize tx_size_y = mi->txSize;
    uint64_t *left_y = &lfm->left_y[tx_size_y];
    uint64_t *above_y = &lfm->above_y[tx_size_y];
    uint64_t *int_4x4_y = &lfm->int_4x4_y;
    int i;

    *above_y |= above_prediction_mask[block_size] << shift_y;
    *left_y |= left_prediction_mask[block_size] << shift_y;

    if (mi->skip && mi->refIdx[0] > INTRA_FRAME) return;

    *above_y |= (size_mask[block_size] & above_64x64_txform_mask[tx_size_y])
        << shift_y;

    *left_y |= (size_mask[block_size] & left_64x64_txform_mask[tx_size_y])
        << shift_y;

    if (tx_size_y == TX_4X4) *int_4x4_y |= size_mask[block_size] << shift_y;
}

static void LoopFilterSetupLevel(const int32_t mi_row, const int32_t mi_col,
                                 int32_t mi_rows, int32_t mi_cols,
                                 ModeInfo *mi, int32_t miPitch,
                                 LoopFilterMask *lfm,
                                 const LoopFilterInfoN *lfi_n,
                                 const AV1VideoParam &par)
{
    int idx_32, idx_16, idx_8;
    ModeInfo *mip = mi;
    ModeInfo *mip2 = mi;

    // These are offsets to the next mi in the 64x64 block. It is what gets
    // added to the mi ptr as we go through each loop. It helps us to avoid
    // setting up special row and column counters for each index. The last step
    // brings us out back to the starting position.

    int offset_32[] = { 4, miPitch * 4 - 4, 4, -miPitch * 4 - 4 };
    int offset_16[] = { 2, miPitch * 2 - 2, 2, -miPitch * 2 - 2 };
    int offset[]    = { 1, miPitch * 1 - 1, 1, -miPitch * 1 - 1 };

    // Following variables represent shifts to position the current block
    // mask over the appropriate block. A shift of 36 to the left will move
    // the bits for the final 32 by 32 block in the 64x64 up 4 rows and left
    // 4 rows to the appropriate spot.
    const int shift_32_y[] = { 0, 4, 32, 36 };
    const int shift_16_y[] = { 0, 2, 16, 18 };
    const int shift_8_y[] = { 0, 1, 8, 9 };
    const int shift_32_uv[] = { 0, 2, 8, 10 };
    const int shift_16_uv[] = { 0, 1, 4, 5 };
    const int max_rows = (mi_row + MI_BLOCK_SIZE > mi_rows ? mi_rows - mi_row : MI_BLOCK_SIZE);
    const int max_cols = (mi_col + MI_BLOCK_SIZE > mi_cols ? mi_cols - mi_col : MI_BLOCK_SIZE);

    switch (mip->sbType) {
    case BLOCK_64X64:
        fill_levels(lfi_n, mip, 0, 0, lfm, par, 0);
        break;
    case BLOCK_64X32:
        fill_levels(lfi_n, mip, 0, 0, lfm, par, 0);
        mip2 = mip + 4 * miPitch;
        if (4 >= max_rows)
            break;
        fill_levels(lfi_n, mip2, 32, 8, lfm, par, 0);
        break;
    case BLOCK_32X64:
        fill_levels(lfi_n, mip, 0, 0, lfm, par, 0);
        mip2 = mip + 4;
        if (4 >= max_cols)
            break;
        fill_levels(lfi_n, mip2, 4, 2, lfm, par, 0);
        break;
    default:
        for (idx_32 = 0; idx_32 < 4; mip += offset_32[idx_32], ++idx_32) {
            const int shift_y = shift_32_y[idx_32];
            const int shift_uv = shift_32_uv[idx_32];
            const int mi_32_col_offset = ((idx_32 & 1) << 2);
            const int mi_32_row_offset = ((idx_32 >> 1) << 2);
            if (mi_32_col_offset >= max_cols || mi_32_row_offset >= max_rows)
                continue;
            switch (mip->sbType) {
            case BLOCK_32X32:
                fill_levels(lfi_n, mip, shift_y, shift_uv, lfm, par, 0);
                break;
            case BLOCK_32X16:
                fill_levels(lfi_n, mip, shift_y, shift_uv, lfm, par, 0);
                if (mi_32_row_offset + 2 >= max_rows)
                    continue;
                mip2 = mip + 2 * miPitch;
                fill_levels(lfi_n, mip2, shift_y + 16, shift_uv + 4, lfm, par, 0);
                break;
            case BLOCK_16X32:
                fill_levels(lfi_n, mip, shift_y, shift_uv, lfm, par, 0);
                if (mi_32_col_offset + 2 >= max_cols)
                    continue;
                mip2 = mip + 2;
                fill_levels(lfi_n, mip2, shift_y + 2, shift_uv + 1, lfm, par, 0);
                break;
            default:
                for (idx_16 = 0; idx_16 < 4; mip += offset_16[idx_16], ++idx_16) {
                    const int shift_y = shift_32_y[idx_32] + shift_16_y[idx_16];
                    const int shift_uv = shift_32_uv[idx_32] + shift_16_uv[idx_16];
                    const int mi_16_col_offset = mi_32_col_offset + ((idx_16 & 1) << 1);
                    const int mi_16_row_offset = mi_32_row_offset + ((idx_16 >> 1) << 1);
                    if (mi_16_col_offset >= max_cols || mi_16_row_offset >= max_rows)
                        continue;

                    switch (mip->sbType) {
                    case BLOCK_16X16:
                        fill_levels(lfi_n, mip, shift_y, shift_uv, lfm, par, 0);
                        break;
                    case BLOCK_16X8:
                        fill_levels(lfi_n, mip, shift_y, shift_uv, lfm, par, 0);
                        if (mi_16_row_offset + 1 >= max_rows)
                            continue;
                        mip2 = mip + miPitch;
                        fill_levels(lfi_n, mip2, shift_y + 8, 0, lfm, par, 1);
                        break;
                    case BLOCK_8X16:
                        fill_levels(lfi_n, mip, shift_y, shift_uv, lfm, par, 0);
                        if (mi_16_col_offset + 1 >= max_cols)
                            continue;
                        mip2 = mip + 1;
                        fill_levels(lfi_n, mip2, shift_y + 1, 0, lfm, par, 1);
                        break;
                    default: {
                        const int shift_y = shift_32_y[idx_32] + shift_16_y[idx_16] + shift_8_y[0];
                        fill_levels(lfi_n, mip, shift_y, shift_uv, lfm, par, 0);
                        mip += offset[0];
                        for (idx_8 = 1; idx_8 < 4; mip += offset[idx_8], ++idx_8) {
                            const int shift_y = shift_32_y[idx_32] + shift_16_y[idx_16] + shift_8_y[idx_8];
                            const int mi_8_col_offset = mi_16_col_offset + ((idx_8 & 1));
                            const int mi_8_row_offset = mi_16_row_offset + ((idx_8 >> 1));
                            if (mi_8_col_offset >= max_cols || mi_8_row_offset >= max_rows)
                                continue;
                            fill_levels(lfi_n, mip, shift_y, 0, lfm, par, 1);
                        }
                        break;
                             }
                    }
                }
                break;
            }
        }
        break;
    }
}

void LoopFilterSetupMask(const int32_t mi_row, const int32_t mi_col,
                         int32_t mi_rows, int32_t mi_cols,
                         ModeInfo *mi, int32_t miPitch,
                         LoopFilterMask *lfm,
                         AV1VideoParam &par)
{
    int idx_32, idx_16, idx_8;
    ModeInfo *mip = mi;
    ModeInfo *mip2 = mi;

    // These are offsets to the next mi in the 64x64 block. It is what gets
    // added to the mi ptr as we go through each loop. It helps us to avoid
    // setting up special row and column counters for each index. The last step
    // brings us out back to the starting position.

    int offset_32[] = { 4, miPitch * 4 - 4, 4, -miPitch * 4 - 4 };
    int offset_16[] = { 2, miPitch * 2 - 2, 2, -miPitch * 2 - 2 };
    int offset[]    = { 1, miPitch * 1 - 1, 1, -miPitch * 1 - 1 };

    // Following variables represent shifts to position the current block
    // mask over the appropriate block. A shift of 36 to the left will move
    // the bits for the final 32 by 32 block in the 64x64 up 4 rows and left
    // 4 rows to the appropriate spot.
    const int shift_32_y[] = { 0, 4, 32, 36 };
    const int shift_16_y[] = { 0, 2, 16, 18 };
    const int shift_8_y[] = { 0, 1, 8, 9 };
    const int shift_32_uv[] = { 0, 2, 8, 10 };
    const int shift_16_uv[] = { 0, 1, 4, 5 };
    const int max_rows =
        (mi_row + MI_BLOCK_SIZE > mi_rows ? mi_rows - mi_row
        : MI_BLOCK_SIZE);
    const int max_cols =
        (mi_col + MI_BLOCK_SIZE > mi_cols ? mi_cols - mi_col
        : MI_BLOCK_SIZE);

    Zero(*lfm);

    switch (mip->sbType) {
    case BLOCK_64X64: build_masks(mip, 0, 0, lfm, par); break;
    case BLOCK_64X32:
        build_masks(mip, 0, 0, lfm, par);
        mip2 = mip + 4 * miPitch;
        if (4 >= max_rows) break;
        build_masks(mip2, 32, 8, lfm, par);
        break;
    case BLOCK_32X64:
        build_masks(mip, 0, 0, lfm, par);
        mip2 = mip + 4;
        if (4 >= max_cols) break;
        build_masks(mip2, 4, 2, lfm, par);
        break;
    default:
        for (idx_32 = 0; idx_32 < 4; mip += offset_32[idx_32], ++idx_32) {
            const int shift_y = shift_32_y[idx_32];
            const int shift_uv = shift_32_uv[idx_32];
            const int mi_32_col_offset = ((idx_32 & 1) << 2);
            const int mi_32_row_offset = ((idx_32 >> 1) << 2);
            if (mi_32_col_offset >= max_cols || mi_32_row_offset >= max_rows)
                continue;
            switch (mip->sbType) {
            case BLOCK_32X32:
                build_masks(mip, shift_y, shift_uv, lfm, par);
                break;
            case BLOCK_32X16:
                build_masks(mip, shift_y, shift_uv, lfm, par);
                if (mi_32_row_offset + 2 >= max_rows) continue;
                mip2 = mip + 2 * miPitch;
                build_masks(mip2, shift_y + 16, shift_uv + 4, lfm, par);
                break;
            case BLOCK_16X32:
                build_masks(mip, shift_y, shift_uv, lfm, par);
                if (mi_32_col_offset + 2 >= max_cols) continue;
                mip2 = mip + 2;
                build_masks(mip2, shift_y + 2, shift_uv + 1, lfm, par);
                break;
            default:
                for (idx_16 = 0; idx_16 < 4; mip += offset_16[idx_16], ++idx_16) {
                    const int shift_y = shift_32_y[idx_32] + shift_16_y[idx_16];
                    const int shift_uv = shift_32_uv[idx_32] + shift_16_uv[idx_16];
                    const int mi_16_col_offset =
                        mi_32_col_offset + ((idx_16 & 1) << 1);
                    const int mi_16_row_offset =
                        mi_32_row_offset + ((idx_16 >> 1) << 1);

                    if (mi_16_col_offset >= max_cols || mi_16_row_offset >= max_rows)
                        continue;

                    switch (mip->sbType) {
                    case BLOCK_16X16:
                        build_masks(mip, shift_y, shift_uv, lfm, par);
                        break;
                    case BLOCK_16X8:
                        build_masks(mip, shift_y, shift_uv, lfm, par);
                        if (mi_16_row_offset + 1 >= max_rows) continue;
                        mip2 = mip + miPitch;
                        build_y_mask(mip2, shift_y + 8, lfm);
                        break;
                    case BLOCK_8X16:
                        build_masks(mip, shift_y, shift_uv, lfm, par);
                        if (mi_16_col_offset + 1 >= max_cols) continue;
                        mip2 = mip + 1;
                        build_y_mask(mip2, shift_y + 1, lfm);
                        break;
                    default: {
                        const int shift_y =
                            shift_32_y[idx_32] + shift_16_y[idx_16] + shift_8_y[0];
                        build_masks(mip, shift_y, shift_uv, lfm, par);
                        mip += offset[0];
                        for (idx_8 = 1; idx_8 < 4; mip += offset[idx_8], ++idx_8) {
                            const int shift_y = shift_32_y[idx_32] +
                                shift_16_y[idx_16] + shift_8_y[idx_8];
                            const int mi_8_col_offset =
                                mi_16_col_offset + ((idx_8 & 1));
                            const int mi_8_row_offset =
                                mi_16_row_offset + ((idx_8 >> 1));

                            if (mi_8_col_offset >= max_cols ||
                                mi_8_row_offset >= max_rows)
                                continue;
                            build_y_mask(mip, shift_y, lfm);
                        }
                        break;
                             }
                    }
                }
                break;
            }
        }
        break;
    }
}

static inline int clamp(int value, int low, int high) {
    return value < low ? low : (value > high ? high : value);
}

// Get the superblock lfm for a given mi_row, mi_col.
static inline LoopFilterMask *get_lfm(LoopFilterMask *lfm,
                                      const int32_t mi_row, const int32_t mi_col, const int32_t lfm_stride) {
  return &lfm[(mi_col >> 3) + ((mi_row >> 3) * lfm_stride)];
}

void LoopFilterInitLevels(int32_t default_filt_lvl, LoopFilterInfoN *lfi, LoopFilterFrameParam *params)
{
    // n_shift is the multiplier for lf_deltas
    // the multiplier is 1 for when filter_lvl is between 0 and 31;
    // 2 when filter_lvl is between 32 and 63
    const int scale = 1 << (default_filt_lvl >> 5);

    int seg_id = 0;
    {
        int lvl_seg = default_filt_lvl;

        if (!params->deltaEnabled) {
            // we could get rid of this if we assume that deltas are set to
            // zero when not in use; encoder always uses deltas
            memset(lfi->lvl[seg_id], lvl_seg, sizeof(lfi->lvl[seg_id]));
        } else {
            int ref, mode;
            const int intra_lvl = lvl_seg + params->refDeltas[0] * scale;
            lfi->lvl[seg_id][0][0] = clamp(intra_lvl, 0, MAX_LOOP_FILTER);
            lfi->lvl[seg_id][0][1] = 0; // tmp workaround for aom bug

            for (ref = 1; ref < MAX_REF_FRAMES; ++ref) {
                for (mode = 0; mode < MAX_MODE_LF_DELTAS; ++mode) {
                    const int inter_lvl = lvl_seg + params->refDeltas[ref] * scale +
                        params->modeDeltas[mode] * scale;
                    lfi->lvl[seg_id][ref][mode] = clamp(inter_lvl, 0, MAX_LOOP_FILTER);
                }
            }
        }
    }
}



void LoopFilterSetupLevelFrame(int32_t frame_filter_level, int32_t partial_frame, int32_t mi_rows, int32_t mi_cols, int32_t sb64Cols,
                               LoopFilterInfoN *lfi, LoopFilterFrameParam *lfp, LoopFilterMask *lfm,
                               ModeInfo *mi_, const AV1VideoParam &par)
{
    int start_mi_row, end_mi_row, mi_rows_to_filter;
    int mi_col, mi_row;
    if (!frame_filter_level) return;
    start_mi_row = 0;
    mi_rows_to_filter = mi_rows;
    if (partial_frame && mi_rows > 8) {
        start_mi_row = mi_rows >> 1;
        start_mi_row &= 0xfffffff8;
        mi_rows_to_filter = MAX(mi_rows / 8, 8);
    }
    end_mi_row = start_mi_row + mi_rows_to_filter;

    LoopFilterInitLevels(frame_filter_level, lfi, lfp);
/*
    for (mi_row = start_mi_row; mi_row < end_mi_row; mi_row += MI_BLOCK_SIZE) {
        ModeInfo *mi = mi_ + ((mi_row >> 3) * sb64Cols << 8);
        for (mi_col = 0; mi_col < mi_cols; mi_col += MI_BLOCK_SIZE) {
            LoopFilterSetupLevel(mi_row, mi_col, mi_rows, mi_cols, mi + ((mi_col >> 3) << 8),
                get_lfm(lfm, mi_row, mi_col, sb64Cols), lfi, par);
        }
    }
*/
}

static void LoopFilterRows(int start, int stop, int y_only,
                             int32_t mi_rows, int32_t mi_cols, int32_t sb64Cols,
                             ModeInfo *mi_, int32_t miPitch, const LoopFilterThresh *lfthr, LoopFilterMask *lfm_,
                             LoopFilterInfoN *lfi, const AV1VideoParam &par,
                             uint8_t *dst[3], int32_t stride[3])
{
    const int num_planes = y_only ? 1 : 3;
    int mi_row, mi_col;
    alignas(16) LoopFilterMask lfm_work;

    for (mi_row = start; mi_row < stop; mi_row += MI_BLOCK_SIZE) {
        LoopFilterMask *lfm = get_lfm(lfm_, mi_row, 0, sb64Cols);
        ModeInfo *mi = mi_ + mi_row * miPitch;

        for (mi_col = 0; mi_col < mi_cols; mi_col += MI_BLOCK_SIZE, ++lfm) {
            //for debug (single-threaded)
            small_memcpy(&lfm_work, lfm, sizeof(LoopFilterMask));
            LoopFilterSetupLevel(mi_row, mi_col, mi_rows, mi_cols, mi + mi_col, miPitch, &lfm_work, lfi, par);
            LoopFilterAdjustMask(mi_row, mi_col, mi_rows, mi_cols, &lfm_work);

            filter_block_plane_ss00(dst[0] + mi_row * 8 * stride[0] + mi_col * 8, stride[0], mi_row, mi_rows, lfthr, &lfm_work);
            for (int plane = 1; plane < num_planes; ++plane) {
                filter_block_plane_ss11(dst[plane] + mi_row * 4 * stride[plane] + mi_col * 4, stride[plane], mi_row, mi_rows, lfthr, &lfm_work);
            }
        }
    }
}

/*static */int32_t FilterLevelFromQp(int32_t base_qindex, uint8_t isKeyFrame)
{
    const int min_filter_level = 0;
    const int max_filter_level = MAX_LOOP_FILTER;
    const int q = vp9_ac_quant(base_qindex, 0, 8);
    int filt_guess = ROUND_POWER_OF_TWO(q * 20723 + 1015158, 18);
    if (isKeyFrame) filt_guess -= 4;
    return clamp(filt_guess, min_filter_level, max_filter_level);
}

static int64_t lf_sse(Frame *frame, const AV1VideoParam *par)
{
    int64_t cost = 0;
    for (int r = 0; r < par->sb64Rows; r++) {
        for (int c = 0; c < par->sb64Cols; c++) {
            int32_t pitch = frame->m_lf->pitch_luma_bytes;
            int32_t offset = (r * pitch + c) << 6;
            int32_t w = MIN(64, par->Width - (r<<6));
            int32_t h = MIN(64, par->Height - (c<<6));

            cost += AV1PP::sse_flexh(frame->m_lf->y + offset, pitch, frame->m_origin->y + offset, pitch, h, BSR(w)-2);
        }
    }
    return cost;
}

int64_t LoopFilterCost(Frame* frame, const AV1VideoParam *par, uint8_t level) {
    uint8_t *dst = frame->m_lf->y;
    int32_t stride = frame->m_lf->pitch_luma_bytes;
    LoopFilterInfoN lfi;
    int32_t luma_size = frame->m_recon->pitch_luma_bytes * frame->m_recon->height;

    assert(frame->m_recon->pitch_luma_bytes == frame->m_lf->pitch_luma_bytes);

    memcpy(dst, frame->m_recon->y, luma_size);

    if (level > 0) {
        LoopFilterSetupLevelFrame(level, 0, par->miRows, par->miCols, par->sb64Cols, &lfi, &frame->m_loopFilterParam, frame->m_lfm, frame->m_modeInfo, *par);
        LoopFilterRows(0, par->miRows, 1, par->miRows, par->miCols, par->sb64Cols, frame->m_modeInfo, par->miPitch, &par->lfts[frame->m_loopFilterParam.sharpness][0], frame->m_lfm, &lfi, *par, &dst, &stride);
    }
    return lf_sse(frame, par);
}

int32_t FilterLevelSearchFull(Frame* frame, const AV1VideoParam *par)
{
    uint8_t level;
    int64_t cost_best = LLONG_MAX;
    int32_t level_best = 0;

    for (level = 0; level <= MAX_LOOP_FILTER; level++) {
        int64_t cost = LoopFilterCost(frame, par, level);
        if (cost_best > cost) {
            cost_best = cost;
            level_best = level;
        }
    }
    return level_best;
}

int32_t FilterLevelSearchSmart(Frame* frame, const AV1VideoParam *par, uint8_t last_level)
{
    int32_t dir = 0;
    int64_t cost_best;
    int32_t level_best;
    int64_t costs[MAX_LOOP_FILTER+1];
    int32_t level_cur = last_level;
    int32_t level_step = level_cur < 16 ? 4 : level_cur / 4;
    memset(costs, 0xff, sizeof(costs));

    cost_best = LoopFilterCost(frame, par, level_cur);
    level_best = level_cur;
    costs[level_cur] = cost_best;

    while (level_step > 0) {
        const int32_t level_high = MIN(level_cur + level_step, MAX_LOOP_FILTER);
        const int32_t level_low = MAX(level_cur - level_step, 0);

        int64_t bias = 0;//(((cost_best >> (15 - (level_cur / 8))) * level_step) >> (1 + (par->bitDepthLumaShift<<1)));

        if (dir <= 0 && level_low != level_cur) {
            // Get Low filter error score
            if (costs[level_low] < 0) {
                costs[level_low] = LoopFilterCost(frame, par, level_low);
            }
            // If value is close to the best so far then bias towards a lower loop
            // filter value.
            if ((costs[level_low] - bias) < cost_best) {
                // Was it actually better than the previous best?
                if (costs[level_low] < cost_best) cost_best = costs[level_low];

                level_best = level_low;
            }
        }

        // Now look at filt_high
        if (dir >= 0 && level_high != level_cur) {
            if (costs[level_high] < 0) {
                costs[level_high] = LoopFilterCost(frame, par, level_high);
            }
            // Was it better than the previous best?
            if (costs[level_high] < (cost_best - bias)) {
                cost_best = costs[level_high];
                level_best = level_high;
            }
        }

        // Half the step distance if the best filter value was the same as last time
        if (level_best == level_cur) {
            level_step /= 2;
            dir = 0;
        } else {
            dir = (level_best < level_cur) ? -1 : 1;
            level_cur = level_best;
        }
    }
    return level_best;
}

void LoopFilterRow(Frame *frame, int32_t rowSb, const AV1VideoParam *par)
{
    LoopFilterInfoN lfi;

    uint8_t *dst[3];
    int32_t stride[3];

    int32_t chroma_pitch = UMC::align_value<int32_t>(par->Width/2+8, 64);
    int32_t chroma_size = chroma_pitch * (par->Height/2+8);

    LoopFilterFrameParam *params = &frame->m_loopFilterParam;
    LoopFilterResetParams(params);

    params->sharpness = par->deblockingSharpness;
    params->level = FilterLevelFromQp(frame->m_sliceQpY, frame->IsIntra());

    if (!frame->m_isRef && !par->doDumpRecon)
        return;

    dst[0] = frame->m_recon->y;
    dst[1] = frame->m_lf->uv;
    dst[2] = frame->m_lf->uv + chroma_size;

    stride[0] = frame->m_recon->pitch_luma_bytes;
    stride[1] = chroma_pitch;
    stride[2] = chroma_pitch;

    if (params->level > 0) {
        // convert chroma planes of current row of superblocks to YV12 for deblocking
        __m128i mask = _mm_srli_epi16(_mm_cmpeq_epi8(_mm_setzero_si128(), _mm_setzero_si128()), 8); //_mm_set1_epi16(0xff);
        const int32_t chromaHeight = frame->m_recon->height >> 1;
        const int32_t pitchUv = frame->m_recon->pitch_chroma_bytes;
        const int32_t pitchU = chroma_pitch, pitchV = chroma_pitch;
        const uint8_t *puv = frame->m_recon->uv + rowSb*32 * pitchUv;
        uint8_t *pu = dst[1] + rowSb * 32 * pitchU;
        uint8_t *pv = dst[2] + rowSb * 32 * pitchV;
        for (int i = rowSb*32; i < std::min((rowSb+1)*32, chromaHeight); i++, puv += pitchUv, pu += pitchU, pv += pitchV) {
            for (int j = 0; j < frame->m_recon->width/2; j += 16) {
                __m128i s1 = loada_si128(puv + 2 * j);
                __m128i s2 = loada_si128(puv + 2 * j + 16);
                __m128i u1 = _mm_and_si128(s1, mask);
                __m128i u2 = _mm_and_si128(s2, mask);
                __m128i v1 = _mm_srli_epi16(s1, 8);
                __m128i v2 = _mm_srli_epi16(s2, 8);
                storea_si128(pu + j, _mm_packus_epi16(u1, u2));
                storea_si128(pv + j, _mm_packus_epi16(v1, v2));
            }
        }

        // deblock current row of superblocks (64 luma pixels high)
        LoopFilterSetupLevelFrame(params->level, 1, par->miRows, par->miCols, par->sb64Cols, &lfi, params, frame->m_lfm, frame->m_modeInfo, *par);

        if (rowSb > 0) {
            // convert chroma of previosly deblocked row of superblocks to NV12
            const int32_t startY = (rowSb - 1) * 32;
            const int32_t endY = rowSb * 32;
            const uint8_t *pu = dst[1] + startY * pitchU;
            const uint8_t *pv = dst[2] + startY * pitchV;
            uint8_t *puv = frame->m_recon->uv + startY * pitchUv;
            for (int i = startY; i < endY; i++, puv += pitchUv, pu += pitchU, pv += pitchV) {
                for (int j = 0; j < frame->m_recon->width/2; j += 16) {
                    __m128i u = loada_si128(pu + j);
                    __m128i v = loada_si128(pv + j);
                    storea_si128(puv + 2 * j + 0,  _mm_unpacklo_epi8(u, v));
                    storea_si128(puv + 2 * j + 16, _mm_unpackhi_epi8(u, v));
                }
            }
        }

        if (rowSb + 1 == par->sb64Rows) {
            // if we just deblocked last row of superblocks also convert it to NV12
            const int32_t startY = rowSb * 32;
            const int32_t endY = chromaHeight;
            const uint8_t *pu = dst[1] + startY * pitchU;
            const uint8_t *pv = dst[2] + startY * pitchV;
            uint8_t *puv = frame->m_recon->uv + startY * pitchUv;
            for (int i = startY; i < endY; i++, puv += pitchUv, pu += pitchU, pv += pitchV) {
                for (int j = 0; j < frame->m_recon->width/2; j += 16) {
                    __m128i u = loada_si128(pu + j);
                    __m128i v = loada_si128(pv + j);
                    storea_si128(puv + 2 * j + 0,  _mm_unpacklo_epi8(u, v));
                    storea_si128(puv + 2 * j + 16, _mm_unpackhi_epi8(u, v));
                }
            }
        }
    }
}

void LoopFilterFrame(Frame *frame, const AV1VideoParam *par, uint8_t *last_level)
{
    LoopFilterInfoN lfi;

    uint8_t *dst[3];
    int32_t stride[3];

    int32_t chroma_pitch = UMC::align_value<int32_t>(par->Width/2+8, 64);
    int32_t chroma_size = chroma_pitch * (par->Height/2+8);

    LoopFilterFrameParam *params = &frame->m_loopFilterParam;
    LoopFilterResetParams(params);

    params->sharpness = par->deblockingSharpness;

    switch (par->deblockingLevelMethod) {
    case QPBASED:
        params->level = FilterLevelFromQp(frame->m_sliceQpY, frame->IsIntra());
        break;
    case FULLSEARCH_FULLPIC:
        params->level = FilterLevelSearchFull(frame, par);
        break;
    case SMARTSEARCH_FULLPIC:
        if (frame->m_pyramidLayer == 0)
          *last_level = 0;
        params->level = FilterLevelSearchSmart(frame, par, *last_level);
        break;
    default:
        assert(0);
    }
    *last_level = params->level;

    dst[0] = frame->m_recon->y;
    dst[1] = frame->m_lf->uv;
    dst[2] = frame->m_lf->uv + chroma_size;

    for (int i = 0; i < frame->m_recon->height/2; i++) {
        for (int j = 0; j < frame->m_recon->width/2; j++) {
            dst[1][i*chroma_pitch+j] = frame->m_recon->uv[i * frame->m_recon->pitch_chroma_bytes + 2 * j];
            dst[2][i*chroma_pitch+j] = frame->m_recon->uv[i * frame->m_recon->pitch_chroma_bytes + 2 * j + 1];
        }
    }

    stride[0] = frame->m_recon->pitch_luma_bytes;
    stride[1] = chroma_pitch;
    stride[2] = chroma_pitch;

    if (params->level > 0) {
        LoopFilterSetupLevelFrame(params->level, 0, par->miRows, par->miCols, par->sb64Cols, &lfi, params, frame->m_lfm, frame->m_modeInfo, *par);
        LoopFilterRows(0, par->miRows, 0, par->miRows, par->miCols, par->sb64Cols, frame->m_modeInfo, par->miPitch, &par->lfts[params->sharpness][0], frame->m_lfm, &lfi, *par, dst, stride);
    }

    for (int i = 0; i < frame->m_recon->height/2; i++) {
        for (int j = 0; j < frame->m_recon->width/2; j++) {
            frame->m_recon->uv[i * frame->m_recon->pitch_chroma_bytes + 2 * j] = dst[1][i*chroma_pitch+j];
            frame->m_recon->uv[i * frame->m_recon->pitch_chroma_bytes + 2 * j + 1] = dst[2][i*chroma_pitch+j];
        }
    }
}

#define SIMD_WIDTH 16

void LoopFilterInitThresh(LoopFilterThresh (*lfts)[MAX_LOOP_FILTER + 1])
{
    for (int sharpness_lvl = 0; sharpness_lvl < 8; sharpness_lvl++) {
        for (int lvl = 0; lvl <= MAX_LOOP_FILTER; lvl ++) {
            LoopFilterThresh *lft = &lfts[sharpness_lvl][lvl];

            // Set loop filter parameters that control sharpness.
            int block_inside_limit = lvl >> ((sharpness_lvl > 0) + (sharpness_lvl > 4));

            if (sharpness_lvl > 0) {
                if (block_inside_limit > (9 - sharpness_lvl))
                    block_inside_limit = (9 - sharpness_lvl);
            }

            if (block_inside_limit < 1) block_inside_limit = 1;

            memset(lft->lim, block_inside_limit, SIMD_WIDTH);
            memset(lft->mblim, (2 * (lvl + 2) + block_inside_limit),
                SIMD_WIDTH);

            memset(lft->hev_thr, (lvl >> 4), SIMD_WIDTH);
        }
    }
}

void LoopFilterResetParams(LoopFilterFrameParam *par)
{
    par->deltaEnabled = 0;
    par->deltaUpdate = 0;

    par->refDeltas[0] = 0;
    par->refDeltas[1] = 0;
    par->refDeltas[2] = 0;
    par->refDeltas[3] = 0;

    par->modeDeltas[0] = 0;
    par->modeDeltas[1] = 0;
}


#define CONFIG_PARALLEL_DEBLOCKING 1

#if CONFIG_PARALLEL_DEBLOCKING
#define CONFIG_CHROMA_SUB8X8 1
#define PARALLEL_DEBLOCKING_15TAPLUMAONLY 1
#define PARALLEL_DEBLOCKING_DISABLE_15TAP 0
//#define CONFIG_LOOPFILTERING_ACROSS_TILES 1
#define IF_EXT_PARTITION(...)
#define CONFIG_CB4X4 1

    namespace CB4X4Units {
        const int32_t MAX_MIB_SIZE       = 16;
        const int32_t MI_SIZE_LOG2       = 2;
        const int32_t MI_SIZE            = 4;
    };

    void aom_lpf_vertical_4_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void aom_lpf_vertical_8_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void aom_lpf_vertical_16_c(uint8_t *s, int p, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

    void aom_lpf_horizontal_4_c(uint8_t *s, int p /* pitch */,const uint8_t *blimit, const uint8_t *limit,const uint8_t *thresh) ;
    void aom_lpf_horizontal_8_c(uint8_t *s, int p, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
    void aom_lpf_horizontal_edge_16_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

    static const uint32_t av1_prediction_masks[NUM_EDGE_DIRS][BLOCK_SIZES_ALL] = {
        // mask for vertical edges filtering
        {
            4 - 1,   // BLOCK_4X4
            4 - 1,   // BLOCK_4X8
            8 - 1,   // BLOCK_8X4
            8 - 1,   // BLOCK_8X8
            8 - 1,   // BLOCK_8X16
            16 - 1,  // BLOCK_16X8
            16 - 1,  // BLOCK_16X16
            16 - 1,  // BLOCK_16X32
            32 - 1,  // BLOCK_32X16
            32 - 1,  // BLOCK_32X32
            32 - 1,  // BLOCK_32X64
            64 - 1,  // BLOCK_64X32
            64 - 1,  // BLOCK_64X64
            64 - 1,  // BLOCK_64X128
            128 - 1, // BLOCK_128X64
            128 - 1, // BLOCK_128X128
            4 - 1,   // BLOCK_4X16,
            16 - 1,  // BLOCK_16X4,
            8 - 1,   // BLOCK_8X32,
            32 - 1,  // BLOCK_32X8,
            16 - 1,  // BLOCK_16X64,
            64 - 1,  // BLOCK_64X16
            32 - 1,  // BLOCK_32X128
            128 - 1, // BLOCK_128X32
        },
        // mask for horizontal edges filtering
        {
            4 - 1,   // BLOCK_4X4
            8 - 1,   // BLOCK_4X8
            4 - 1,   // BLOCK_8X4
            8 - 1,   // BLOCK_8X8
            16 - 1,  // BLOCK_8X16
            8 - 1,   // BLOCK_16X8
            16 - 1,  // BLOCK_16X16
            32 - 1,  // BLOCK_16X32
            16 - 1,  // BLOCK_32X16
            32 - 1,  // BLOCK_32X32
            64 - 1,  // BLOCK_32X64
            32 - 1,  // BLOCK_64X32
            64 - 1,  // BLOCK_64X64
            128 - 1, // BLOCK_64X128
            64 - 1,  // BLOCK_128X64
            128 - 1, // BLOCK_128X128
            16 - 1,  // BLOCK_4X16,
            4 - 1,   // BLOCK_16X4,
            32 - 1,  // BLOCK_8X32,
            8 - 1,   // BLOCK_32X8,
            64 - 1,  // BLOCK_16X64,
            16 - 1,  // BLOCK_64X16
            128 - 1, // BLOCK_32X128
            32 - 1,  // BLOCK_128X32
        },
    };

    static const uint32_t av1_transform_masks[NUM_EDGE_DIRS][TX_SIZES_ALL] = {
        {
            4 - 1,   // TX_4X4
            8 - 1,   // TX_8X8
            16 - 1,  // TX_16X16
            32 - 1,  // TX_32X32
            64 - 1,  // TX_64X64
            4 - 1,   // TX_4X8
            8 - 1,   // TX_8X4
            8 - 1,   // TX_8X16
            16 - 1,  // TX_16X8
            16 - 1,  // TX_16X32
            32 - 1,  // TX_32X16
            32 - 1,  // TX_32X64
            64 - 1,  // TX_64X32
            4 - 1,   // TX_4X16
            16 - 1,  // TX_16X4
            8 - 1,   // TX_8X32
            32 - 1,  // TX_32X8
            16 - 1,  // TX_16X64
            64 - 1,  // TX_64X16
        },
        {
            4 - 1,   // TX_4X4
            8 - 1,   // TX_8X8
            16 - 1,  // TX_16X16
            32 - 1,  // TX_32X32
            64 - 1,  // TX_64X64
            8 - 1,   // TX_4X8
            4 - 1,   // TX_8X4
            16 - 1,  // TX_8X16
            8 - 1,   // TX_16X8
            32 - 1,  // TX_16X32
            16 - 1,  // TX_32X16
            64 - 1,  // TX_32X64
            32 - 1,  // TX_64X32
            16 - 1,  // TX_4X16
            4 - 1,   // TX_16X4
            32 - 1,  // TX_8X32
            8 - 1,   // TX_32X8
            64 - 1,  // TX_16X64
            16 - 1,  // TX_64X16
        }
    };

    static const uint8_t num_8x8_blocks_high_lookup[BLOCK_SIZES_ALL] = {
        1, 1, 1, 1, 2, 1, 2, 4, 2, 4, 8, 4, 8, 16, 8, 16, 2, 1, 4, 1, 8, 2, 16, 4
    };

    static const uint8_t num_8x8_blocks_wide_lookup[BLOCK_SIZES_ALL] = {
        1, 1, 1, 1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 8, 16, 16, 1, 2, 1, 4, 2, 8, 4, 16
    };

    static const uint8_t mi_size_high[BLOCK_SIZES_ALL] = {
        1, 2, 1, 2, 4, 2, 4, 8, 4, 8, 16, 8, 16, 32, 16, 32, 4, 1, 8, 2, 16, 4, 32, 8
    };

    static const uint8_t mi_size_wide[BLOCK_SIZES_ALL] = {
        1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 8, 16, 16, 16, 32, 32, 1, 4, 2, 8, 4, 16, 8, 32
    };

#define TX_UNIT_WIDE_LOG2 (MI_SIZE_LOG2 - 2/*tx_size_wide_log2[0]*/)
#define TX_UNIT_HIGH_LOG2 (MI_SIZE_LOG2 - 2/*tx_size_high_log2[0]*/)

    typedef TxSize TX_SIZE;
    typedef BlockSize BLOCK_SIZE;

    static const TX_SIZE txsize_horz_map[TX_SIZES_ALL] = {
        TX_4X4,    // TX_4X4
        TX_8X8,    // TX_8X8
        TX_16X16,  // TX_16X16
        TX_32X32,  // TX_32X32
        TX_64X64,  // TX_64X64
        TX_4X4,    // TX_4X8
        TX_8X8,    // TX_8X4
        TX_8X8,    // TX_8X16
        TX_16X16,  // TX_16X8
        TX_16X16,  // TX_16X32
        TX_32X32,  // TX_32X16
        TX_32X32,  // TX_32X64
        TX_64X64,  // TX_64X32
        TX_4X4,    // TX_4X16
        TX_16X16,  // TX_16X4
        TX_8X8,    // TX_8X32
        TX_32X32,  // TX_32X8
        TX_16X16,  // TX_16X64
        TX_64X64,  // TX_64X16
    };

    static const TX_SIZE txsize_vert_map[TX_SIZES_ALL] = {
        TX_4X4,    // TX_4X4
        TX_8X8,    // TX_8X8
        TX_16X16,  // TX_16X16
        TX_32X32,  // TX_32X32
        TX_64X64,  // TX_64X64
        TX_8X8,    // TX_4X8
        TX_4X4,    // TX_8X4
        TX_16X16,  // TX_8X16
        TX_8X8,    // TX_16X8
        TX_32X32,  // TX_16X32
        TX_16X16,  // TX_32X16
        TX_64X64,  // TX_32X64
        TX_32X32,  // TX_64X32
        TX_16X16,  // TX_4X16
        TX_4X4,    // TX_16X4
        TX_32X32,  // TX_8X32
        TX_8X8,    // TX_32X8
        TX_64X64,  // TX_16X64
        TX_16X16,  // TX_64X16
    };

    struct AV1_DEBLOCKING_PARAMETERS {
        // length of the filter applied to the outer edge
        uint32_t filter_length;
        // deblocking limits
        const uint8_t *lim;
        const uint8_t *mblim;
        const uint8_t *hev_thr;
    };



    int32_t is_inter_block(const ModeInfo *mi) { return mi->refIdx[0] > INTRA_FRAME; }

    TX_SIZE av1_get_transform_size_fast(const ModeInfo *mi, const EdgeDir edge_dir, const int plane)
    {
        TX_SIZE txSize = (plane == PLANE_TYPE_Y)
            ? mi->txSize
            : max_txsize_rect_lookup[ ss_size_lookup[mi->sbType][1][1] ];
        assert(txSize <= TX_32X32); // this fast version supports only square TUs
        return txSize;
    }

    TX_SIZE av1_get_transform_size(const ModeInfo *mi, const EdgeDir edge_dir, /*int mi_row, int mi_col, */int plane)
    {
        TX_SIZE tx_size = (plane == PLANE_TYPE_Y)
            ? mi->txSize
            : max_txsize_rect_lookup[ ss_size_lookup[mi->sbType][1][1] ];
        assert(tx_size < TX_SIZES_ALL);

        // commented since no real vartx so far
        //if ((plane == PLANE_TYPE_Y) && is_inter_block(mi) && !mi->skip) {
        //    const BLOCK_SIZE sb_type = mi->sbType;
        //    const int blk_row = mi_row & (mi_size_high[sb_type] - 1);
        //    const int blk_col = mi_col & (mi_size_wide[sb_type] - 1);
        //    const TX_SIZE mb_tx_size = mbmi->inter_tx_size[av1_get_txb_size_index(sb_type, blk_row, blk_col)]
        //    assert(mb_tx_size < TX_SIZES_ALL);
        //    tx_size = mb_tx_size;
        //}

        // since in case of chrominance or non-square transorm need to convert
        // transform size into transform size in particular direction.
        // for vertical edge, filter direction is horizontal, for horizontal
        // edge, filter direction is vertical.
        tx_size = (VERT_EDGE == edge_dir) ? txsize_horz_map[tx_size] : txsize_vert_map[tx_size];
        return tx_size;
    }

    bool no_loop_filter_deltas(const LoopFilterInfoN *lfi_n) {
        const uint8_t *p = (const uint8_t *)lfi_n->lvl;
        for (size_t i = 1; i < sizeof(lfi_n->lvl); i++)
            if (p[0] != p[i])
                return 0;
        return 1;
    }

    uint8_t get_filter_level_fast(const LoopFilterInfoN *lfi_n, const ModeInfo *mi)
    {
        // assumes no loopfilter level deltas
        assert(no_loop_filter_deltas(lfi_n));
        return lfi_n->lvl[0][0][0];
    }

    template <EdgeDir edge_dir, int32_t plane>
    void set_lpf_parameters_and_filter(uint8_t *dst, int32_t pitchDst, int32_t coord, const ModeInfo *currMi,
                                       const ModeInfo *prevMi, const LoopFilterThresh &limits)
    {
        if (coord == 0)
            return;

        // prepare outer edge parameters. deblock the edge if it's an edge of a TU
        const TX_SIZE ts = av1_get_transform_size_fast(currMi, edge_dir, plane);
        const int32_t tu_edge = !(coord & av1_transform_masks[edge_dir][ts]);
        if (tu_edge) {
            const BlockSize planeSz = (plane == 0) ? currMi->sbType : ss_size_lookup[currMi->sbType][1][1];
            const int32_t inside_pu = (coord & av1_prediction_masks[edge_dir][planeSz]) != 0;
            const int32_t inside_skipped_pu = inside_pu & currMi->skip & is_inter_block(currMi);

            // if the current and the previous blocks are skipped,
            // deblock the edge if the edge belongs to a PU's edge only.
            if (!inside_skipped_pu) {
                const TX_SIZE pv_ts = av1_get_transform_size_fast(prevMi, edge_dir, plane);
                const TX_SIZE min_ts = std::min(ts, pv_ts);
                assert(min_ts <= TX_32X32);  // current implementation doesn't use other transform sizes
                int32_t filter_length_idx = (plane == 0)
                    ? (min_ts < TX_32X32 ? min_ts : 2)  // 4, 8 or 16 tap filter for luma
                    : (min_ts == TX_4X4 ? 0 : 3);       // 4 or 6 tap filter for chroma

                AV1PP::loopfilter(dst, pitchDst, limits.mblim, limits.lim, limits.hev_thr, edge_dir, filter_length_idx);
            }
        }
    }


    // ____h0____ ____h1____
    //           |          || <- right frame border
    //   mi_row  |          ||
    //   mi_col  v0         ||
    //           |          ||
    // ____h2____|____h3____||
    //           |          ||
    //           |          ||
    //           v1         ||
    //           |          ||
    //           |          ||
    //
    // ModeInfo has 8x8 granularity, miRow and miCol have 4x4 granularity.
    // But since both miRow and miCol are even: (mi_row & 1) == 0 and (mi_col & 1) == 0
    // ModeInfo is always mapped to 4x4 grid like this:
    //
    //   curr-miPitch | curr-miPitch
    //                |
    //  ------h0------|------h1------
    //  dst           |dst+4
    //       curr     v0    curr
    //                |
    //  ------h2------|------h3------
    //  dst2          |
    //       curr     v1    curr
    //                |
    //
    void av1_loopfilter_luma_last_8x8(uint8_t *dst, int32_t dstride, int32_t mi_row, int32_t mi_col,
                                      const ModeInfo *mi8x8, int32_t miPitch, const LoopFilterThresh &limits)
    {
        assert((mi_row & 1) == 0);
        assert((mi_col & 1) == 0);

        const int32_t y = mi_row << 2;
        const int32_t x = mi_col << 2;
        const ModeInfo *curr = mi8x8 + (mi_row >> 1) * miPitch + (mi_col >> 1);
        const ModeInfo *above = curr - miPitch;
        uint8_t *dst2 = dst + 4 * dstride;

        set_lpf_parameters_and_filter<VERT_EDGE, 0>(dst  + 4, dstride, x + 4, curr, curr, limits); // v0
        set_lpf_parameters_and_filter<VERT_EDGE, 0>(dst2 + 4, dstride, x + 4, curr, curr, limits); // v1

        set_lpf_parameters_and_filter<HORZ_EDGE, 0>(dst  + 0, dstride, y + 0, curr, above, limits); // h0
        set_lpf_parameters_and_filter<HORZ_EDGE, 0>(dst  + 4, dstride, y + 0, curr, above, limits); // h1
        set_lpf_parameters_and_filter<HORZ_EDGE, 0>(dst2 + 0, dstride, y + 4, curr, curr,  limits); // h2
        set_lpf_parameters_and_filter<HORZ_EDGE, 0>(dst2 + 4, dstride, y + 4, curr, curr,  limits); // h3
    }


    // ____h0____|____h1____|__
    //           |          |
    //   mi_row  |          |
    //   mi_col  v0         v1
    //           |          |
    // ____h2____|____h3____|__
    //           |          |
    //           |          |
    //           v2         v3
    //           |          |
    //           |          |
    //
    // ModeInfo has 8x8 granularity, miRow and miCol have 4x4 granularity.
    // But since both miRow and miCol are even: (mi_row & 1) == 0 and (mi_col & 1) == 0
    // ModeInfo is always mapped to 4x4 grid like this:
    //
    //   curr-miPitch | curr-miPitch |
    //                |              |
    //  ------h0------|------h1------|--------------
    //  dst           |dst+4         |dst+8
    //       curr     v0    curr     v1   curr+1
    //                |              |
    //  ------h2------|------h3------|--------------
    //  dst2          |              |
    //       curr     v2    curr     v3   curr+1
    //                |              |
    //
    void av1_loopfilter_luma_8x8(uint8_t *dst, int32_t dstride, int32_t mi_row, int32_t mi_col,
                                 const ModeInfo *mi8x8, int32_t miPitch, const LoopFilterThresh &limits)
    {
        assert((mi_row & 1) == 0);
        assert((mi_col & 1) == 0);

        const int32_t y = mi_row << 2;
        const int32_t x = mi_col << 2;
        const ModeInfo *curr = mi8x8 + (mi_row >> 1) * miPitch + (mi_col >> 1);
        const ModeInfo *right = curr + 1;
        const ModeInfo *above = curr - miPitch;
        uint8_t *dst2 = dst + 4 * dstride;

        set_lpf_parameters_and_filter<VERT_EDGE, 0>(dst  + 4, dstride, x + 4, curr,  curr, limits); // v0
        set_lpf_parameters_and_filter<VERT_EDGE, 0>(dst  + 8, dstride, x + 8, right, curr, limits); // v1
        set_lpf_parameters_and_filter<VERT_EDGE, 0>(dst2 + 4, dstride, x + 4, curr,  curr, limits); // v2
        set_lpf_parameters_and_filter<VERT_EDGE, 0>(dst2 + 8, dstride, x + 8, right, curr, limits); // v3

        set_lpf_parameters_and_filter<HORZ_EDGE, 0>(dst  + 0, dstride, y + 0, curr, above, limits); // h0
        set_lpf_parameters_and_filter<HORZ_EDGE, 0>(dst  + 4, dstride, y + 0, curr, above, limits); // h1
        set_lpf_parameters_and_filter<HORZ_EDGE, 0>(dst2 + 0, dstride, y + 4, curr, curr,  limits); // h0
        set_lpf_parameters_and_filter<HORZ_EDGE, 0>(dst2 + 4, dstride, y + 4, curr, curr,  limits); // h1
    }


    // ____h0____|| <- right frame border
    //           ||
    //   mi_row  ||
    //   mi_col  ||
    //           ||
    //           ||
    //
    // ModeInfo has 8x8 granularity, miRow and miCol have 4x4 granularity.
    // Also each Chroma 4:2:0 4x4 block is mapped to one 8x8 ModeInfo.
    // Also longest filter length for Chroma is 8 which mean up to 4 pixels may be modified from each side of edge
    // ModeInfo is always mapped to Chroma 4x4 grid like this:
    //
    //   curr-miPitch || <- right frame border
    //                ||
    //  ------h0------||
    //  dst           ||
    //       curr     ||
    //                ||
    //
    void av1_loopfilter_nv12_last_4x4(uint8_t *dst, int32_t dst_stride, int32_t mi_row, int32_t mi_col,
                                      const ModeInfo *mi8x8, int32_t miPitch, const LoopFilterThresh &limits)
    {
        const int32_t y = mi_row << 1;
        const int32_t x = mi_col << 1;
        const ModeInfo *curr = mi8x8 + (mi_row >> 1) * miPitch + (mi_col >> 1);
        const ModeInfo *above = curr - miPitch;
        set_lpf_parameters_and_filter<HORZ_EDGE, 1>(dst + 0, dst_stride, y + 0, curr, above, limits);
    }


    // ____h0____|____
    //           |
    //   mi_row  |
    //   mi_col  v0
    //           |
    //           |
    //
    // ModeInfo has 8x8 granularity, miRow and miCol have 4x4 granularity.
    // Also each Chroma 4:2:0 4x4 block is mapped to one 8x8 ModeInfo.
    // Also longest filter length for Chroma is 8 which mean up to 4 pixels may be modified from each side of edge
    // ModeInfo is always mapped to Chroma 4x4 grid like this:
    //
    //   curr-miPitch |
    //                |
    //  ------h0------|-----------
    //  dst           |dst+4
    //       curr     v0    curr+1
    //                |
    //                |
    //
    void av1_loopfilter_nv12_4x4(uint8_t *dst, int32_t dst_stride, int32_t mi_row, int32_t mi_col,
                                 const ModeInfo *mi8x8, int32_t miPitch, const LoopFilterThresh &limits)
    {
        const int32_t y = mi_row << 1;
        const int32_t x = mi_col << 1;
        const ModeInfo *curr = mi8x8 + (mi_row >> 1) * miPitch + (mi_col >> 1);
        const ModeInfo *above = curr - miPitch;
        const ModeInfo *right = curr + 1;
        set_lpf_parameters_and_filter<VERT_EDGE, 1>(dst + 4, dst_stride, x + 4, right, curr, limits);
        set_lpf_parameters_and_filter<HORZ_EDGE, 1>(dst + 0, dst_stride, y + 0, curr, above, limits);
    }


    template <EdgeDir edge_dir, int32_t plane>
    void av1_filter_block_plane(uint8_t *dst0, int32_t dst_stride, int32_t mi_row, int32_t mi_col,
                                const AV1VideoParam &par, const ModeInfo *mi8x8, int32_t miPitch,
                                const LoopFilterInfoN *lfi_n)
    {
        const int32_t scale_horz = plane ? 1 : 0;
        const int32_t scale_vert = plane ? 1 : 0;

        int32_t curr_y = ((mi_row * CB4X4Units::MI_SIZE) >> scale_vert);

        for (int y = 0; y < (CB4X4Units::MAX_MIB_SIZE >> scale_vert); y++, curr_y += 4) {
            if (curr_y >= (par.Height >> scale_vert))
                break;

            uint8_t *p = dst0 + y * CB4X4Units::MI_SIZE * dst_stride;
            int32_t curr_x = ((mi_col * CB4X4Units::MI_SIZE) >> scale_horz);

            for (int x = 0; x < (CB4X4Units::MAX_MIB_SIZE >> scale_horz); x++, p += 4, curr_x += 4) {
                if (curr_x >= (par.Width >> scale_horz))
                    break;

                AV1_DEBLOCKING_PARAMETERS params = {};
                set_lpf_parameters<edge_dir, plane>(&params, curr_x, curr_y, mi8x8, miPitch, par, lfi_n, lfthr);

                if (edge_dir == VERT_EDGE) {
                    switch (params.filter_length) {
                    case 4:  AV1PP::lpf_vertical_4(p, dst_stride, params.mblim, params.lim, params.hev_thr); break;
                    case 8:  AV1PP::lpf_vertical_8(p, dst_stride, params.mblim, params.lim, params.hev_thr); break;
                    case 16: AV1PP::lpf_vertical_16(p, dst_stride, params.mblim, params.lim, params.hev_thr); break;
                    default: break;
                    }
                } else {
                    switch (params.filter_length) {
                    case 4:  AV1PP::lpf_horizontal_4(p, dst_stride, params.mblim, params.lim, params.hev_thr); break;
                    case 8:  AV1PP::lpf_horizontal_8(p, dst_stride, params.mblim, params.lim, params.hev_thr); break;
                    case 16: AV1PP::lpf_horizontal_edge_16(p, dst_stride, params.mblim, params.lim, params.hev_thr); break;
                    default: break;
                    }
                }
            }
        }
    }

#endif // CONFIG_PARALLEL_DEBLOCKING

    namespace details {
        void Split4(const uint8_t *src, uint8_t *dstu, uint8_t *dstv) {
            __m128i mask = _mm_srli_epi16(_mm_cmpeq_epi8(_mm_setzero_si128(), _mm_setzero_si128()), 8); //_mm_set1_epi16(0xff);
            __m128i s = loadl_epi64(src);
            __m128i d = _mm_packus_epi16(_mm_and_si128(s, mask), _mm_srli_epi16(s, 8));
            *(int *)dstu = _mm_cvtsi128_si32(d);
            *(int *)dstv = _mm_extract_epi32(d, 2);
        }

        void Split8(const uint8_t *src, uint8_t *dstu, uint8_t *dstv) {
            __m128i mask = _mm_srli_epi16(_mm_cmpeq_epi8(_mm_setzero_si128(), _mm_setzero_si128()), 8); //_mm_set1_epi16(0xff);
            __m128i s = loada_si128(src);
            __m128i d = _mm_packus_epi16(_mm_and_si128(s, mask), _mm_srli_epi16(s, 8));
            storel_epi64(dstu, d);
            storeh_epi64(dstv, d);
        }

        void Split16(const uint8_t *src, uint8_t *dstu, uint8_t *dstv) {
            __m128i mask = _mm_srli_epi16(_mm_cmpeq_epi8(_mm_setzero_si128(), _mm_setzero_si128()), 8); //_mm_set1_epi16(0xff);
            __m128i s0 = loada_si128(src);
            __m128i s1 = loada_si128(src + 16);
            storea_si128(dstu, _mm_packus_epi16(_mm_and_si128(s0, mask), _mm_and_si128(s1, mask)));
            storea_si128(dstv, _mm_packus_epi16(_mm_srli_epi16(s0, 8), _mm_srli_epi16(s1, 8)));
        }

        void Split32(const uint8_t *src, uint8_t *dstu, uint8_t *dstv) {
            __m128i mask = _mm_srli_epi16(_mm_cmpeq_epi8(_mm_setzero_si128(), _mm_setzero_si128()), 8); //_mm_set1_epi16(0xff);
            __m128i s0 = loada_si128(src);
            __m128i s1 = loada_si128(src + 16);
            __m128i s2 = loada_si128(src + 32);
            __m128i s3 = loada_si128(src + 48);
            storea_si128(dstu +  0, _mm_packus_epi16(_mm_and_si128(s0, mask), _mm_and_si128(s1, mask)));
            storea_si128(dstu + 16, _mm_packus_epi16(_mm_and_si128(s2, mask), _mm_and_si128(s3, mask)));
            storea_si128(dstv +  0, _mm_packus_epi16(_mm_srli_epi16(s0, 8), _mm_srli_epi16(s1, 8)));
            storea_si128(dstv + 16, _mm_packus_epi16(_mm_srli_epi16(s2, 8), _mm_srli_epi16(s3, 8)));
        }

        void Interleave4(const uint8_t *srcu, const uint8_t *srcv, uint8_t *dst) {
            __m128i u = _mm_cvtsi32_si128(*(const int *)srcu);
            __m128i v = _mm_cvtsi32_si128(*(const int *)srcv);
            storel_epi64(dst, _mm_unpacklo_epi8(u, v));
        }

        void Interleave8(const uint8_t *srcu, const uint8_t *srcv, uint8_t *dst) {
            storea_si128(dst, _mm_unpacklo_epi8(loadl_epi64(srcu), loadl_epi64(srcv)));
        }

        void Interleave16(const uint8_t *srcu, const uint8_t *srcv, uint8_t *dst) {
            __m128i u = loada_si128(srcu);
            __m128i v = loada_si128(srcv);
            storea_si128(dst + 0,  _mm_unpacklo_epi8(u, v));
            storea_si128(dst + 16, _mm_unpackhi_epi8(u, v));
        }

        void Interleave32(const uint8_t *srcu, const uint8_t *srcv, uint8_t *dst) {
            __m128i u0 = loada_si128(srcu + 0);
            __m128i u1 = loada_si128(srcu + 16);
            __m128i v0 = loada_si128(srcv + 0);
            __m128i v1 = loada_si128(srcv + 16);
            storea_si128(dst + 0,  _mm_unpacklo_epi8(u0, v0));
            storea_si128(dst + 16, _mm_unpackhi_epi8(u0, v0));
            storea_si128(dst + 32, _mm_unpacklo_epi8(u1, v1));
            storea_si128(dst + 48, _mm_unpackhi_epi8(u1, v1));
        }
    };

    void LoopFilterSbAV1(Frame *frame, int32_t sbRow, int32_t sbCol, const AV1VideoParam &par)
    {
        //fprintf(stderr, "start %2d %2d\n", sbRow, sbCol);
        LoopFilterInfoN lfi;

        LoopFilterFrameParam *params = &frame->m_loopFilterParam;
        LoopFilterResetParams(params);

        params->sharpness = par.deblockingSharpness;
        params->level = FilterLevelFromQp(frame->m_sliceQpY, frame->IsIntra());
        if (params->level == 0)
            return;
        LoopFilterInitLevels(params->level, &lfi, params);

        const LoopFilterThresh &limits = par.lfts[par.deblockingSharpness][params->level];

        // process 8 rows by 8 8x8, shifted to the left and above by 1 8x8 block
        // __ __   __ __  __ __  __ __  __ __  __ __  __ __  __ __
        // __|__|| __|__| __|__| __|__| __|__| __|__| __|__| __|__|      ||
        //   |  ||   |  |   |  |   |  |   |  |   |  |   |  |   |  |      ||
        // == ==   == ==  == ==  == ==  == ==  == ==  == ==  == ==  == ==   <-------- top SB border
        // __|__|| __|__| __|__| __|__| __|__| __|__| __|__| __|__|      ||
        //   |  ||   |  |   |  |   |  |   |  |   |  |   |  |   |  |      ||
        // __ __   __ __  __ __  __ __  __ __  __ __  __ __  __ __       ||
        // __|__|| __|__| __|__| __|__| __|__| __|__| __|__| __|__|      ||
        //   |  ||   |  |   |  |   |  |   |  |   |  |   |  |   |  |      ||
        //       ^                                                       ^
        //       |---------------left and right SB borders---------------|
        //

        // Start column (4x4) is 2 blocks to the left from SB left border (but within picture borders)
        const int32_t startC4x4 = std::max((sbCol << 4) - 2, 0);
        // Stop column is 14 blocks to the right from SB left border (but within picture borders)
        const int32_t stopC4x4 = std::min((sbCol << 4) + 14, (par.Width >> 2) - 2);
        // Start row (4x4) is 2 blocks above SB top border (but within picture borders)
        const int32_t startR4x4 = std::max((sbRow << 4) - 2, 0);
        // Stop row (4x4) is 14 blocks below SB top border or at bottom picture border if we are processing last SB row
        const int32_t stopR4x4 = (sbRow + 1 == par.sb64Rows)
            ? (par.Height >> 2)
            : std::min((sbRow << 4) + 14, (par.Height >> 2) - 2);


        /* Chroma block structure.
         *   Size is 32+2=34 rows, 12+32+2+2=48 columns.
         *     Note that for last SB row there may be 36+2=38 rows
         *   Alignment is 16 bytes
         *   Legend:
         *     x - center pixels (32x32), modified
         *     + - additional pixels (2*32 + 2*32), also modified
         *     . - padding pixels, not modified, not shared with other threads
         *     ! - padding pixels, must not be modified since may be shared with other threads
         *
         * ............++++++++++++++++++++++++++++++++!!!!
         * ............xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx++..
         * ............xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx++..
         * ............xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx++..
         * ............xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx++..
         * ............xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx++..
         * ............xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx++..
         * ............xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx++..
         * ............xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx++..
         * ............xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx++..
         * ............xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx++..
         * ............xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx++..
         * ............xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx++..
         * ............xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx++..
         * ............xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx++..
         * ............xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx++..
         * ............xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx++..
         */

        const int32_t LPF_CHROMA_BLOCK_WIDE = 32;
        // Deblocking region may be 36 lines (72 Luma lines) at the bottom of picture
        const int32_t LPF_CHROMA_BLOCK_HIGH = 36;
        // Deblocking modifies up to 2 pels beyond top and left edges
        const int32_t LPF_BORDER = 2;
        // Left padding to ensure 16-byte alignment for pixel on left SB border
        const int32_t LPF_LEFT_PAD = 12;
        // Right padding to ensure 16-byte alignment for pixel on left SB border
        const int32_t LPF_RIGHT_PAD = 2;
        // Ensure 16 byte alignent
        const int32_t LPF_CHROMA_PITCH = LPF_CHROMA_BLOCK_WIDE + LPF_BORDER + LPF_LEFT_PAD + LPF_RIGHT_PAD;
        alignas(16) uint8_t chromaPlanes[2][LPF_CHROMA_PITCH * (LPF_BORDER + LPF_CHROMA_BLOCK_HIGH)];

        // Split chroma planes of current SB for deblocking
        const int32_t blockHeight = ((stopR4x4 - startR4x4) << 1);
        const int32_t nv12pitch = frame->m_recon->pitch_chroma_bytes;
        uint8_t *pnv12 = frame->m_recon->uv + sbCol * 64 + (startR4x4 * 2 - LPF_BORDER) * nv12pitch;
        uint8_t *pu = chromaPlanes[0] + LPF_LEFT_PAD + 4;
        uint8_t *pv = chromaPlanes[1] + LPF_LEFT_PAD + 4;
        for (int32_t y = 0; y < LPF_BORDER + blockHeight; y++) {
            details::Split4(pnv12 - 8, pu - 4, pv - 4);
            details::Split32(pnv12, pu, pv);
            pnv12 += nv12pitch;
            pu += LPF_CHROMA_PITCH;
            pv += LPF_CHROMA_PITCH;
        }

        {
            const int32_t miPitch = par.miPitch;
            const ModeInfo *mi = frame->m_modeInfo;

            // Filter by 8x8 blocks
            // Filter 2 vertical edges before filtering 1 horizontal edge
            const int32_t pitch = frame->m_recon->pitch_luma_pix;
            uint8_t *p = frame->m_recon->y + startR4x4 * frame->m_recon->pitch_luma_pix * 4;
            for (int32_t r = startR4x4; r < stopR4x4; r += 2, p += 8 * pitch)
                for (int32_t c = startC4x4; c < stopC4x4; c += 2)
                    av1_loopfilter_luma_8x8(p + (c << 2), pitch, r, c, mi, miPitch, limits);

            if (sbCol + 1 == par.sb64Cols) {
                const int32_t c = stopC4x4;
                uint8_t *p = frame->m_recon->y + startR4x4 * frame->m_recon->pitch_luma_pix * 4 + (c << 2);
                for (int32_t r = startR4x4; r < stopR4x4; r += 2, p += 8 * pitch)
                    av1_loopfilter_luma_last_8x8(p, pitch, r, c, mi, miPitch, limits);
            }

            // Chroma processed as 8 rows by 8 4x4 blocks, shifted to the left by 1 4x4 block
            // __   __  __  __  __  __  __  __
            //   ||   |   |   |   |   |   |   |   ||
            //    ^                               ^
            //    |---left and right SB borders---|
            //
            for (int32_t plane = 1; plane < 3; plane++) {
                const int32_t pitch = LPF_CHROMA_PITCH;
                uint8_t *pdst = chromaPlanes[plane - 1] + LPF_BORDER * LPF_CHROMA_PITCH + (sbCol == 0 ? 16 : 12);
                for (int32_t r = startR4x4; r < stopR4x4; r += 2) {
                    uint8_t *p = pdst + ((r - startR4x4) << 1) * pitch;
                    for (int32_t c = startC4x4; c < stopC4x4; c += 2, p += 4)
                        av1_loopfilter_nv12_4x4(p, pitch, r, c + 0, mi, miPitch, limits);
                }
                if (sbCol + 1 == par.sb64Cols) {
                    const int32_t c = stopC4x4;
                    for (int32_t r = startR4x4; r < stopR4x4; r += 2) {
                        uint8_t *p = pdst + ((r - startR4x4) << 1) * pitch + ((c - startC4x4) << 1);
                        av1_loopfilter_nv12_last_4x4(p + 0, pitch, r, c, mi, miPitch, limits);
                    }
                }
            }
        }

        // Now interleave deblocked chroma planes back to nv12
        pnv12 = frame->m_recon->uv + sbCol * 64 + (startR4x4 * 2 - LPF_BORDER) * nv12pitch;
        pu = chromaPlanes[0] + LPF_LEFT_PAD + 4;
        pv = chromaPlanes[1] + LPF_LEFT_PAD + 4;
        for (int32_t y = 0; y < LPF_BORDER; y++) {
            details::Interleave4(pu - 4, pv - 4, pnv12 - 8);
            if (sbCol + 1 == par.sb64Cols) {
                details::Interleave32(pu, pv, pnv12);
            } else {
                details::Interleave16(pu, pv, pnv12);
                details::Interleave8(pu + 16, pv + 16, pnv12 + 32);
                details::Interleave4(pu + 24, pv + 24, pnv12 + 48);
            }
            pnv12 += nv12pitch;
            pu += LPF_CHROMA_PITCH;
            pv += LPF_CHROMA_PITCH;
        }
        for (int32_t y = 0; y < blockHeight; y++) {
            details::Interleave4(pu - 4, pv - 4, pnv12 - 8);
            details::Interleave32(pu, pv, pnv12);
            pnv12 += nv12pitch;
            pu += LPF_CHROMA_PITCH;
            pv += LPF_CHROMA_PITCH;
        }

    }


}
#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
