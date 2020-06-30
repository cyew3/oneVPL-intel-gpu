// Copyright (c) 2014-2020 Intel Corporation
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
#include "mfx_av1_superres.h"
#include "mfx_av1_copy.h"

#include <algorithm>

#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

using namespace AV1Enc;

typedef struct InterpFilterParams {
    const int16_t *filter_ptr;
    uint16_t taps;
    uint16_t subpel_shifts;
    InterpFilter interp_filter;
} InterpFilterParams;

typedef uint16_t CONV_BUF_TYPE;
typedef struct ConvolveParams {
    int ref;
    int do_average;
    CONV_BUF_TYPE *dst;
    int dst_stride;
    int round_0;
    int round_1;
    int plane;
    int is_compound;
    int use_jnt_comp_avg;
    int fwd_offset;
    int bck_offset;
} ConvolveParams;

static const int16_t *av1_get_interp_filter_subpel_kernel(const InterpFilterParams filter_params, const int subpel)
{
    return filter_params.filter_ptr + filter_params.taps * subpel;
}

#define EIGHTTAP_REGULAR EIGHTTAP

#define SUBPEL_BITS 4
#define SUBPEL_MASK ((1 << SUBPEL_BITS) - 1)
#define SUBPEL_SHIFTS (1 << SUBPEL_BITS)
#define SUBPEL_TAPS 8

#define DECLARE_ALIGNED(n, typ, val) __declspec(align(n)) typ val
DECLARE_ALIGNED(256, static const InterpKernel,
sub_pel_filters_4[SUBPEL_SHIFTS]) = {
{ 0, 0, 0, 128, 0, 0, 0, 0 },     { 0, 0, -4, 126, 8, -2, 0, 0 },
{ 0, 0, -8, 122, 18, -4, 0, 0 },  { 0, 0, -10, 116, 28, -6, 0, 0 },
{ 0, 0, -12, 110, 38, -8, 0, 0 }, { 0, 0, -12, 102, 48, -10, 0, 0 },
{ 0, 0, -14, 94, 58, -10, 0, 0 }, { 0, 0, -12, 84, 66, -10, 0, 0 },
{ 0, 0, -12, 76, 76, -12, 0, 0 }, { 0, 0, -10, 66, 84, -12, 0, 0 },
{ 0, 0, -10, 58, 94, -14, 0, 0 }, { 0, 0, -10, 48, 102, -12, 0, 0 },
{ 0, 0, -8, 38, 110, -12, 0, 0 }, { 0, 0, -6, 28, 116, -10, 0, 0 },
{ 0, 0, -4, 18, 122, -8, 0, 0 },  { 0, 0, -2, 8, 126, -4, 0, 0 }
};
DECLARE_ALIGNED(256, static const InterpKernel,
sub_pel_filters_4smooth[SUBPEL_SHIFTS]) = {
{ 0, 0, 0, 128, 0, 0, 0, 0 },   { 0, 0, 30, 62, 34, 2, 0, 0 },
{ 0, 0, 26, 62, 36, 4, 0, 0 },  { 0, 0, 22, 62, 40, 4, 0, 0 },
{ 0, 0, 20, 60, 42, 6, 0, 0 },  { 0, 0, 18, 58, 44, 8, 0, 0 },
{ 0, 0, 16, 56, 46, 10, 0, 0 }, { 0, 0, 14, 54, 48, 12, 0, 0 },
{ 0, 0, 12, 52, 52, 12, 0, 0 }, { 0, 0, 12, 48, 54, 14, 0, 0 },
{ 0, 0, 10, 46, 56, 16, 0, 0 }, { 0, 0, 8, 44, 58, 18, 0, 0 },
{ 0, 0, 6, 42, 60, 20, 0, 0 },  { 0, 0, 4, 40, 62, 22, 0, 0 },
{ 0, 0, 4, 36, 62, 26, 0, 0 },  { 0, 0, 2, 34, 62, 30, 0, 0 }
};

static const InterpFilterParams av1_interp_4tap[2] = {
  { (const int16_t *)sub_pel_filters_4, SUBPEL_TAPS, SUBPEL_SHIFTS,
    EIGHTTAP_REGULAR },
  { (const int16_t *)sub_pel_filters_4smooth, SUBPEL_TAPS, SUBPEL_SHIFTS,
    EIGHTTAP_SMOOTH },
};

//DECLARE_ALIGNED(256, static const InterpKernel,
//bilinear_filters[SUBPEL_SHIFTS]) = {
//{ 0, 0, 0, 128, 0, 0, 0, 0 },  { 0, 0, 0, 120, 8, 0, 0, 0 },
//{ 0, 0, 0, 112, 16, 0, 0, 0 }, { 0, 0, 0, 104, 24, 0, 0, 0 },
//{ 0, 0, 0, 96, 32, 0, 0, 0 },  { 0, 0, 0, 88, 40, 0, 0, 0 },
//{ 0, 0, 0, 80, 48, 0, 0, 0 },  { 0, 0, 0, 72, 56, 0, 0, 0 },
//{ 0, 0, 0, 64, 64, 0, 0, 0 },  { 0, 0, 0, 56, 72, 0, 0, 0 },
//{ 0, 0, 0, 48, 80, 0, 0, 0 },  { 0, 0, 0, 40, 88, 0, 0, 0 },
//{ 0, 0, 0, 32, 96, 0, 0, 0 },  { 0, 0, 0, 24, 104, 0, 0, 0 },
//{ 0, 0, 0, 16, 112, 0, 0, 0 }, { 0, 0, 0, 8, 120, 0, 0, 0 }
//};

//DECLARE_ALIGNED(256, static const InterpKernel,
//sub_pel_filters_8[SUBPEL_SHIFTS]) = {
//{ 0, 0, 0, 128, 0, 0, 0, 0 },      { 0, 2, -6, 126, 8, -2, 0, 0 },
//{ 0, 2, -10, 122, 18, -4, 0, 0 },  { 0, 2, -12, 116, 28, -8, 2, 0 },
//{ 0, 2, -14, 110, 38, -10, 2, 0 }, { 0, 2, -14, 102, 48, -12, 2, 0 },
//{ 0, 2, -16, 94, 58, -12, 2, 0 },  { 0, 2, -14, 84, 66, -12, 2, 0 },
//{ 0, 2, -14, 76, 76, -14, 2, 0 },  { 0, 2, -12, 66, 84, -14, 2, 0 },
//{ 0, 2, -12, 58, 94, -16, 2, 0 },  { 0, 2, -12, 48, 102, -14, 2, 0 },
//{ 0, 2, -10, 38, 110, -14, 2, 0 }, { 0, 2, -8, 28, 116, -12, 2, 0 },
//{ 0, 0, -4, 18, 122, -10, 2, 0 },  { 0, 0, -2, 8, 126, -6, 2, 0 }
//};

DECLARE_ALIGNED(256, static const InterpKernel,
bilinear_filters1[SUBPEL_SHIFTS]) = {
{ 0, 0, 0, 128, 0, 0, 0, 0 },  { 0, 0, 0, 120, 8, 0, 0, 0 },
{ 0, 0, 0, 112, 16, 0, 0, 0 }, { 0, 0, 0, 104, 24, 0, 0, 0 },
{ 0, 0, 0, 96, 32, 0, 0, 0 },  { 0, 0, 0, 88, 40, 0, 0, 0 },
{ 0, 0, 0, 80, 48, 0, 0, 0 },  { 0, 0, 0, 72, 56, 0, 0, 0 },
{ 0, 0, 0, 64, 64, 0, 0, 0 },  { 0, 0, 0, 56, 72, 0, 0, 0 },
{ 0, 0, 0, 48, 80, 0, 0, 0 },  { 0, 0, 0, 40, 88, 0, 0, 0 },
{ 0, 0, 0, 32, 96, 0, 0, 0 },  { 0, 0, 0, 24, 104, 0, 0, 0 },
{ 0, 0, 0, 16, 112, 0, 0, 0 }, { 0, 0, 0, 8, 120, 0, 0, 0 }
};

DECLARE_ALIGNED(256, static const InterpKernel,
sub_pel_filters1_8[SUBPEL_SHIFTS]) = {
{ 0, 0, 0, 128, 0, 0, 0, 0 },      { 0, 2, -6, 126, 8, -2, 0, 0 },
{ 0, 2, -10, 122, 18, -4, 0, 0 },  { 0, 2, -12, 116, 28, -8, 2, 0 },
{ 0, 2, -14, 110, 38, -10, 2, 0 }, { 0, 2, -14, 102, 48, -12, 2, 0 },
{ 0, 2, -16, 94, 58, -12, 2, 0 },  { 0, 2, -14, 84, 66, -12, 2, 0 },
{ 0, 2, -14, 76, 76, -14, 2, 0 },  { 0, 2, -12, 66, 84, -14, 2, 0 },
{ 0, 2, -12, 58, 94, -16, 2, 0 },  { 0, 2, -12, 48, 102, -14, 2, 0 },
{ 0, 2, -10, 38, 110, -14, 2, 0 }, { 0, 2, -8, 28, 116, -12, 2, 0 },
{ 0, 0, -4, 18, 122, -10, 2, 0 },  { 0, 0, -2, 8, 126, -6, 2, 0 }
};

DECLARE_ALIGNED(256, static const InterpKernel,
sub_pel_filters1_8sharp[SUBPEL_SHIFTS]) = {
{ 0, 0, 0, 128, 0, 0, 0, 0 },         { -2, 2, -6, 126, 8, -2, 2, 0 },
{ -2, 6, -12, 124, 16, -6, 4, -2 },   { -2, 8, -18, 120, 26, -10, 6, -2 },
{ -4, 10, -22, 116, 38, -14, 6, -2 }, { -4, 10, -22, 108, 48, -18, 8, -2 },
{ -4, 10, -24, 100, 60, -20, 8, -2 }, { -4, 10, -24, 90, 70, -22, 10, -2 },
{ -4, 12, -24, 80, 80, -24, 12, -4 }, { -2, 10, -22, 70, 90, -24, 10, -4 },
{ -2, 8, -20, 60, 100, -24, 10, -4 }, { -2, 8, -18, 48, 108, -22, 10, -4 },
{ -2, 6, -14, 38, 116, -22, 10, -4 }, { -2, 6, -10, 26, 120, -18, 8, -2 },
{ -2, 4, -6, 16, 124, -12, 6, -2 },   { 0, 2, -2, 8, 126, -6, 2, -2 }
};

DECLARE_ALIGNED(256, static const InterpKernel,
sub_pel_filters1_8smooth[SUBPEL_SHIFTS]) = {
{ 0, 0, 0, 128, 0, 0, 0, 0 },     { 0, 2, 28, 62, 34, 2, 0, 0 },
{ 0, 0, 26, 62, 36, 4, 0, 0 },    { 0, 0, 22, 62, 40, 4, 0, 0 },
{ 0, 0, 20, 60, 42, 6, 0, 0 },    { 0, 0, 18, 58, 44, 8, 0, 0 },
{ 0, 0, 16, 56, 46, 10, 0, 0 },   { 0, -2, 16, 54, 48, 12, 0, 0 },
{ 0, -2, 14, 52, 52, 14, -2, 0 }, { 0, 0, 12, 48, 54, 16, -2, 0 },
{ 0, 0, 10, 46, 56, 16, 0, 0 },   { 0, 0, 8, 44, 58, 18, 0, 0 },
{ 0, 0, 6, 42, 60, 20, 0, 0 },    { 0, 0, 4, 40, 62, 22, 0, 0 },
{ 0, 0, 4, 36, 62, 26, 0, 0 },    { 0, 0, 2, 34, 62, 28, 2, 0 }
};

static const InterpFilterParams
av1_interp_filter_params_list[SWITCHABLE_FILTERS + 1] = {
  { (const int16_t *)sub_pel_filters1_8, SUBPEL_TAPS, SUBPEL_SHIFTS,
    EIGHTTAP_REGULAR },
  { (const int16_t *)sub_pel_filters1_8smooth, SUBPEL_TAPS, SUBPEL_SHIFTS,
    EIGHTTAP_SMOOTH },
  { (const int16_t *)sub_pel_filters1_8sharp, SUBPEL_TAPS, SUBPEL_SHIFTS,
    EIGHTTAP_SHARP },
  { (const int16_t *)bilinear_filters1, SUBPEL_TAPS, SUBPEL_SHIFTS,
    BILINEAR }
};

InterpFilterParams av1_get_interp_filter_params_with_block_size(const InterpFilter interp_filter, const int w)
{
    if (w <= 4 &&
        (interp_filter == EIGHTTAP_SHARP || interp_filter == EIGHTTAP_REGULAR))
        return av1_interp_4tap[0];
    else if (w <= 4 && interp_filter == EIGHTTAP_SMOOTH)
        return av1_interp_4tap[1];

    return av1_interp_filter_params_list[interp_filter];
}

static const int MAX_SB_SIZE_LOG2 = 7;
static const int MAX_SB_SIZE = (1 << MAX_SB_SIZE_LOG2);
static const int MAX_FILTER_TAP = 8;
static const int SCALE_SUBPEL_BITS = 10;
static const int SCALE_SUBPEL_SHIFTS = (1 << SCALE_SUBPEL_BITS);
static const int SCALE_SUBPEL_MASK = (SCALE_SUBPEL_SHIFTS - 1);
static const int SCALE_EXTRA_BITS = (SCALE_SUBPEL_BITS - SUBPEL_BITS);
static const int FILTER_BITS = 7;
static const int ROUND0_BITS = 3;
static const int COMPOUND_ROUND1_BITS = 7;

static ConvolveParams get_conv_params_no_round(
    int ref, int do_average,
    int plane,
    CONV_BUF_TYPE *dst,
    int dst_stride,
    int is_compound, int bd)
{
    ConvolveParams conv_params = {};
    conv_params.ref = ref;
    conv_params.do_average = do_average;
    //assert(IMPLIES(do_average, is_compound));
    conv_params.is_compound = is_compound;
    conv_params.round_0 = ROUND0_BITS;
    conv_params.round_1 = is_compound ? COMPOUND_ROUND1_BITS
        : 2 * FILTER_BITS - conv_params.round_0;
    const int intbufrange = bd + FILTER_BITS - conv_params.round_0 + 2;
    //assert(IMPLIES(bd < 12, intbufrange <= 16));
    if (intbufrange > 16) {
        conv_params.round_0 += intbufrange - 16;
        if (!is_compound) conv_params.round_1 -= intbufrange - 16;
    }
    // TODO(yunqing): The following dst should only be valid while
    // is_compound = 1;
    conv_params.dst = dst;
    conv_params.dst_stride = dst_stride;
    conv_params.plane = plane;
    return conv_params;
}

void av1_highbd_convolve_2d_scale_c(
    const uint8_t *src, int src_stride,
    uint8_t *dst, int dst_stride, int w, int h,
    //InterpFilterParams *filter_params_x,
    //InterpFilterParams *filter_params_y,
    uint32_t interp_filters,
    const int subpel_x_qn, const int x_step_qn,
    const int subpel_y_qn, const int y_step_qn,
    ConvolveParams *conv_params, int bd)
{
    //ConvolveParams conv_params_ = get_conv_params_no_round(0, 0, /*plane*/0, /*tmp_dst*/nullptr, MAX_SB_SIZE, /*is_compound*/0, 8);
    //ConvolveParams* conv_params = &conv_params_;
    //
    InterpFilter filter_x = (interp_filters >> 4) & 0x3;//av1_extract_interp_filter(interp_filters, 1);
    InterpFilter filter_y = interp_filters & 0x3;// av1_extract_interp_filter(interp_filters, 0);
    InterpFilterParams filter_params_x_ = av1_get_interp_filter_params_with_block_size(filter_x, w);
    InterpFilterParams* filter_params_x = &filter_params_x_;
    InterpFilterParams filter_params_y_ = av1_get_interp_filter_params_with_block_size(filter_y, h);
    InterpFilterParams* filter_params_y = &filter_params_y_;


    int16_t im_block[(2 * MAX_SB_SIZE + MAX_FILTER_TAP) * MAX_SB_SIZE];
    int im_h = (((h - 1) * y_step_qn + subpel_y_qn) >> SCALE_SUBPEL_BITS) + filter_params_y->taps;
    int im_stride = w;
    const int fo_vert = filter_params_y->taps / 2 - 1;
    const int fo_horiz = filter_params_x->taps / 2 - 1;

    const int bits = FILTER_BITS * 2 - conv_params->round_0 - conv_params->round_1;
    assert(bits >= 0);

    // horizontal filter
    const uint8_t *src_horiz = src - fo_vert * src_stride;
    for (int y = 0; y < im_h; ++y) {
        int x_qn = subpel_x_qn;
        for (int x = 0; x < w; ++x, x_qn += x_step_qn) {
            const uint8_t *const src_x = &src_horiz[(x_qn >> SCALE_SUBPEL_BITS)];
            const int x_filter_idx = (x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
            assert(x_filter_idx < SUBPEL_SHIFTS);
            const int16_t *x_filter = av1_get_interp_filter_subpel_kernel(*filter_params_x, x_filter_idx);
            int32_t sum = (1 << (bd + FILTER_BITS - 1));
            for (int k = 0; k < filter_params_x->taps; ++k) {
                sum += x_filter[k] * src_x[k - fo_horiz];
            }
            assert(0 <= sum && sum < (1 << (bd + FILTER_BITS + 1)));
            im_block[y * im_stride + x] = (int16_t)ROUND_POWER_OF_TWO(sum, conv_params->round_0);
        }
        src_horiz += src_stride;
    }

    // vertical filter
    CONV_BUF_TYPE *dst16 = conv_params->dst;
    const int dst16_stride = conv_params->dst_stride;

    int16_t *src_vert = im_block + fo_vert * im_stride;
    const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
    for (int x = 0; x < w; ++x) {
        int y_qn = subpel_y_qn;
        for (int y = 0; y < h; ++y, y_qn += y_step_qn) {
            const int16_t *src_y = &src_vert[(y_qn >> SCALE_SUBPEL_BITS) * im_stride];
            const int y_filter_idx = (y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
            assert(y_filter_idx < SUBPEL_SHIFTS);
            const int16_t *y_filter = av1_get_interp_filter_subpel_kernel(*filter_params_y, y_filter_idx);
            int32_t sum = 1 << offset_bits;
            for (int k = 0; k < filter_params_y->taps; ++k) {
                sum += y_filter[k] * src_y[(k - fo_vert) * im_stride];
            }
            assert(0 <= sum && sum < (1 << (offset_bits + 2)));
            CONV_BUF_TYPE res = (CONV_BUF_TYPE)ROUND_POWER_OF_TWO(sum, conv_params->round_1);
#if 1
            if (conv_params->is_compound) {
                if (conv_params->do_average) {
                    int32_t tmp = dst16[y * dst16_stride + x];
                    /*if (conv_params->use_jnt_comp_avg) {
                        tmp = tmp * conv_params->fwd_offset + res * conv_params->bck_offset;
                        tmp = tmp >> DIST_PRECISION_BITS;
                    }
                    else*/
                    {
                        tmp += res;
                        tmp = tmp >> 1;
                    }
                    /* Subtract round offset and convolve round */
                    tmp = tmp - ((1 << (offset_bits - conv_params->round_1)) + (1 << (offset_bits - conv_params->round_1 - 1)));
                    if (conv_params->plane == 0 && w >= 8)
                        assert(dst[y * dst_stride + x] == (uint8_t)Saturate(0, 255, ROUND_POWER_OF_TWO(tmp, bits)));
                    dst[y * dst_stride + x] = (uint8_t)Saturate(0, 255, ROUND_POWER_OF_TWO(tmp, bits));
                }
                else {
                    if (conv_params->plane == 0 && w >= 8)
                        assert(dst16[y * dst16_stride + x] == res);
                    dst16[y * dst16_stride + x] = res;
                }
            }
            else
#endif
            {
                /* Subtract round offset and convolve round */
                int32_t tmp = res - ((1 << (offset_bits - conv_params->round_1)) +
                    (1 << (offset_bits - conv_params->round_1 - 1)));
                //dst[y * dst_stride + x] = clip_pixel_highbd(ROUND_POWER_OF_TWO(tmp, bits), bd);
                dst[y * dst_stride + x] = (uint8_t)Saturate(0, 255, ROUND_POWER_OF_TWO(tmp, bits));
            }
        }
        src_vert++;
    }
}

    const int FILTER_TAPS = 8;
    const int ROUND_STAGE0 = 3;
    const int ROUND_STAGE1 = 11;
    const int ROUND_PRE_COMPOUND = 7;
    const int ROUND_POST_COMPOUND = 2 * FILTER_BITS - ROUND_STAGE0 - ROUND_PRE_COMPOUND + 1;
    const int OFFSET_STAGE0 = 1 << ROUND_STAGE0 >> 1;
    const int OFFSET_STAGE1 = 1 << ROUND_STAGE1 >> 1;
    const int OFFSET_PRE_COMPOUND = 1 << ROUND_PRE_COMPOUND >> 1;
    const int OFFSET_POST_COMPOUND = 1 << ROUND_POST_COMPOUND >> 1;
    const int MAX_BLOCK_SIZE = 96;
    const int IMPLIED_PITCH = 64;
    const int ADDITIONAL_OFFSET_STAGE0 = 64 * 256;
    const int ADDITIONAL_OFFSET_STAGE1 = -(ADDITIONAL_OFFSET_STAGE0 >> ROUND_STAGE0);

    inline __m256i filter_u8_u8(__m256i ab, __m256i cd, __m256i ef, __m256i gh, __m256i filt01, __m256i filt23, __m256i filt45, __m256i filt67, __m256i offset)
    {
        __m256i tmp1, tmp2, res;
        ab = _mm256_maddubs_epi16(ab, filt01);
        cd = _mm256_maddubs_epi16(cd, filt23);
        ef = _mm256_maddubs_epi16(ef, filt45);
        gh = _mm256_maddubs_epi16(gh, filt67);
        tmp1 = _mm256_add_epi16(ab, ef); // order of addition matters to avoid overflow
        tmp2 = _mm256_add_epi16(cd, gh);
        res = _mm256_adds_epi16(tmp1, tmp2);
        res = _mm256_adds_epi16(res, offset);
        return _mm256_srai_epi16(res, FILTER_BITS);
    }

    inline __m256i filter_u8_u16(__m256i ab, __m256i cd, __m256i ef, __m256i gh, __m256i k01, __m256i k23, __m256i k45, __m256i k67)
    {
        ab = _mm256_maddubs_epi16(ab, k01);
        cd = _mm256_maddubs_epi16(cd, k23);
        ef = _mm256_maddubs_epi16(ef, k45);
        gh = _mm256_maddubs_epi16(gh, k67);
        __m256i res;
        res = _mm256_add_epi16(ab, cd);
        res = _mm256_add_epi16(res, ef);
        res = _mm256_add_epi16(res, gh);
        res = _mm256_add_epi16(res, _mm256_set1_epi16(OFFSET_STAGE0 + ADDITIONAL_OFFSET_STAGE0));
        res = _mm256_srli_epi16(res, ROUND_STAGE0);
        return res;
    }

    inline __m256i filter_s16_s32(__m256i ab, __m256i cd, __m256i ef, __m256i gh, __m256i k01, __m256i k23, __m256i k45, __m256i k67)
    {
        ab = _mm256_madd_epi16(ab, k01);
        cd = _mm256_madd_epi16(cd, k23);
        ef = _mm256_madd_epi16(ef, k45);
        gh = _mm256_madd_epi16(gh, k67);

        return _mm256_add_epi32(
            _mm256_add_epi32(ab, cd),
            _mm256_add_epi32(ef, gh));
    }

    void av1_highbd_convolve_2d_scale_w4_avx2(const uint8_t *src, int32_t pitchSrc, uint8_t *dst, int32_t pitchDst,
        int32_t width, int32_t height, uint32_t interp_filters,
        int32_t subpel_x_qn, int32_t subpel_y_qn)
    {
        assert((width & 3) == 0);
        assert((height & 3) == 0);

        alignas(32) int16_t tmpBlock[(32 + 8) * 8]; // max height = 32, width = 8
        const int32_t tmpBlockHeight = height - 1 + FILTER_TAPS;
        const int32_t pitchTmp = width;
        const int32_t filterOffX = FILTER_TAPS / 2 - 1;
        const int32_t filterOffY = FILTER_TAPS / 2 - 1;

        const int32_t ftypeX = (interp_filters >> 4) & 0x3;
        const int32_t ftypeY = interp_filters & 0x3;
        const int32_t dx = (subpel_x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
        const int32_t dy = (subpel_y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
        const int16_t *fx = AV1Enc::av1_short_filter_kernels[ftypeX][dx];
        const int16_t *fy = AV1Enc::av1_short_filter_kernels[ftypeY][dy];

        // horizontal filter
        __m128i filt;
        __m256i filt01, filt23, filt45, filt67;
        __m256i a, b, c, d, res;

        filt = loada_si128(fx);                                     // 8w: 0 1 2 3 4 5 6 7
        filt = _mm_packs_epi16(filt, filt);                         // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        filt01 = _mm256_broadcastw_epi16(filt);                     // 32b: (0 1) x 16
        filt23 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 2));  // 32b: (2 3) x 16
        filt45 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 4));  // 32b: (4 5) x 16
        filt67 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 6));  // 32b: (6 7) x 16

        const uint8_t *psrc = src - filterOffY * pitchSrc - filterOffX + (subpel_x_qn >> SCALE_SUBPEL_BITS);
        int16_t *ptmp = tmpBlock;

        if (dx != 0) {
            for (int y = 0; y < tmpBlockHeight; y += 2) {
                a = loadu2_m128i(psrc + 0, psrc + pitchSrc + 0);    // 32b: 00 01 .. 30 31
                b = loadu2_m128i(psrc + 2, psrc + pitchSrc + 2);    // 32b: 02 03 .. 32 33
                c = loadu2_m128i(psrc + 4, psrc + pitchSrc + 4);    // 32b: 04 05 .. 34 35
                d = loadu2_m128i(psrc + 6, psrc + pitchSrc + 6);    // 32b: 06 07 .. 36 37

                res = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                //storea_si256(ptmp, res);
                storel_epi64(ptmp + 0 * pitchTmp, si128_lo(res));
                storel_epi64(ptmp + 1 * pitchTmp, si128_hi(res));

                psrc += pitchSrc * 2;
                ptmp += pitchTmp * 2;
            }
        }
        else {
            const __m256i offset = _mm256_set1_epi16(ADDITIONAL_OFFSET_STAGE0 >> ROUND0_BITS);
            psrc += filterOffX;

            for (int y = 0; y < tmpBlockHeight; y += 2) {
                a = loadu2_m128i(psrc, psrc + pitchSrc);
                a = _mm256_slli_epi16(a, 8);
                a = _mm256_srli_epi16(a, 4);
                a = _mm256_add_epi16(a, offset);
                //storea_si256(ptmp, a);
                storel_epi64(ptmp + 0 * pitchTmp, si128_lo(a));
                storel_epi64(ptmp + 1 * pitchTmp, si128_hi(a));

                psrc += pitchSrc * 2;
                ptmp += pitchTmp * 2;
            }
        }

        // vertical filter
        if (dy != 0) {

            filt = loada_si128(fy);                                     // 8w: 0 1 2 3 4 5 6 7
            filt23 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 4));  // 16b: (2 3) x 8
            filt45 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 8));  // 16b: (4 5) x 8

            ptmp = tmpBlock;
            uint8_t *pdst = dst;

            __m256i ab, cd, res1, res2;

            a = loadu_si256(tmpBlock + 2 * 4);  // 16w: A0 A1 A2 A3 B0 B1 B2 B3 | C0 C1 C2 C3 D0 D1 D2 D3
            b = loadu_si256(tmpBlock + 3 * 4);  // 16w: B0 B1 B2 B3 C0 C1 C2 C3 | D0 D1 D2 D3 E0 E1 E2 E3
            c = loadu_si256(tmpBlock + 4 * 4);  // 16w: C0 C1 C2 C3 D0 D1 D2 D3 | E0 E1 E2 E3 F0 F1 F2 F3
            d = loadu_si256(tmpBlock + 5 * 4);  // 16w: D0 D1 D2 D3 E0 E1 E2 E3 | F0 F1 F2 F3 G0 G1 G2 G3

            ab = _mm256_unpacklo_epi16(a, b);   // 16w: A0 B0 A1 B1 A2 B2 A3 B3 | C0 D0 C1 D1 C2 D2 C3 D3
            cd = _mm256_unpacklo_epi16(c, d);   // 16w: C0 D0 C1 D1 C2 D2 C3 D3 | E0 F0 E1 F1 E2 F2 E3 F3
            ab = _mm256_madd_epi16(ab, filt23);
            cd = _mm256_madd_epi16(cd, filt45);
            res1 = _mm256_add_epi32(ab, cd);    // 8d: row0 | row2

            ab = _mm256_unpackhi_epi16(a, b);   // 16w: B0 C0 B1 C1 B2 C2 B3 C3 | D0 E0 D1 E1 D2 E2 D3 E3
            cd = _mm256_unpackhi_epi16(c, d);   // 16w: D0 E0 D1 E1 D2 E2 D3 E3 | F0 G0 F1 G1 F2 G2 F3 G3
            ab = _mm256_madd_epi16(ab, filt23);
            cd = _mm256_madd_epi16(cd, filt45);
            res2 = _mm256_add_epi32(ab, cd);    // 8d: row1 | row3

            const __m256i offset = _mm256_set1_epi32(OFFSET_STAGE1 + (ADDITIONAL_OFFSET_STAGE1 << FILTER_BITS));
            res1 = _mm256_add_epi32(res1, offset);
            res2 = _mm256_add_epi32(res2, offset);
            res1 = _mm256_srai_epi32(res1, ROUND_STAGE1);
            res2 = _mm256_srai_epi32(res2, ROUND_STAGE1);
            res1 = _mm256_packus_epi32(res1, res2);  // 16w: row0 row1 | row2 row3
            res1 = _mm256_packus_epi16(res1, res1);  // 32b: row0 row1 row0 row1 | row2 row3 row2 row3
            storel_si32(pdst + 0 * pitchDst, si128_lo(res1));
            storel_si32(pdst + 1 * pitchDst, _mm_srli_si128(si128_lo(res1), 4));
            storel_si32(pdst + 2 * pitchDst, si128_hi(res1));
            storel_si32(pdst + 3 * pitchDst, _mm_srli_si128(si128_hi(res1), 4));
        }
        else {
            ptmp = tmpBlock + filterOffY * pitchTmp;
            uint8_t *pdst = dst;
            const __m256i offset = _mm256_set1_epi16(8 + ADDITIONAL_OFFSET_STAGE1);
            for (int y = 0; y < height; y += 2) {
                a = loadu2_m128i(ptmp, ptmp + pitchTmp);
                a = _mm256_add_epi16(a, offset);
                a = _mm256_srai_epi16(a, 4);
                a = _mm256_packus_epi16(a, a);
                //storel_epi64(pdst + 0 * pitchDst, si128_lo(a));
                //storel_epi64(pdst + 1 * pitchDst, si128_hi(a));
                storel_si32(pdst + 0 * pitchDst, si128_lo(a));
                storel_si32(pdst + 1 * pitchDst, si128_hi(a));

                pdst += pitchDst * 2;
                ptmp += pitchTmp * 2;
            }
        }
    }

    void av1_highbd_convolve_2d_scale_w8_avx2(const uint8_t *src, int32_t pitchSrc, uint8_t *dst, int32_t pitchDst,
        int32_t width, int32_t height, uint32_t interp_filters,
        int32_t subpel_x_qn, int32_t subpel_y_qn)
    {
        assert((width & 7) == 0);

        alignas(32) int16_t tmpBlock[(32 + 8) * 8]; // max height = 32, width = 8
        const int32_t tmpBlockHeight = height - 1 + FILTER_TAPS;
        const int32_t pitchTmp = width;
        const int32_t filterOffX = FILTER_TAPS / 2 - 1;
        const int32_t filterOffY = FILTER_TAPS / 2 - 1;

        const int32_t ftypeX = (interp_filters >> 4) & 0x3;
        const int32_t ftypeY = interp_filters & 0x3;
        const int32_t dx = (subpel_x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
        const int32_t dy = (subpel_y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
        const int16_t *fx = AV1Enc::av1_filter_kernels[ftypeX][dx];
        const int16_t *fy = AV1Enc::av1_filter_kernels[ftypeY][dy];

        // horizontal filter
        __m128i filt;
        __m256i filt01, filt23, filt45, filt67;
        __m256i a, b, c, d, res;

        filt = loada_si128(fx);                                     // 8w: 0 1 2 3 4 5 6 7
        filt = _mm_packs_epi16(filt, filt);                         // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        filt01 = _mm256_broadcastw_epi16(filt);                     // 32b: (0 1) x 16
        filt23 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 2));  // 32b: (2 3) x 16
        filt45 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 4));  // 32b: (4 5) x 16
        filt67 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 6));  // 32b: (6 7) x 16

        const uint8_t *psrc = src - filterOffY * pitchSrc - filterOffX + (subpel_x_qn >> SCALE_SUBPEL_BITS);
        int16_t *ptmp = tmpBlock;

        if (dx != 0) {
            for (int y = 0; y < tmpBlockHeight; y += 2) {
                a = loadu2_m128i(psrc + 0, psrc + pitchSrc + 0);    // 32b: 00 01 .. 30 31
                b = loadu2_m128i(psrc + 2, psrc + pitchSrc + 2);    // 32b: 02 03 .. 32 33
                c = loadu2_m128i(psrc + 4, psrc + pitchSrc + 4);    // 32b: 04 05 .. 34 35
                d = loadu2_m128i(psrc + 6, psrc + pitchSrc + 6);    // 32b: 06 07 .. 36 37

                res = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                storea_si256(ptmp, res);

                psrc += pitchSrc * 2;
                ptmp += pitchTmp * 2;
            }
        }
        else {
            const __m256i offset = _mm256_set1_epi16(ADDITIONAL_OFFSET_STAGE0 >> ROUND0_BITS);
            psrc += filterOffX;

            for (int y = 0; y < tmpBlockHeight; y += 2) {
                a = loadu2_m128i(psrc, psrc + pitchSrc);
                a = _mm256_slli_epi16(a, 8);
                a = _mm256_srli_epi16(a, 4);
                a = _mm256_add_epi16(a, offset);
                storea_si256(ptmp, a);

                psrc += pitchSrc * 2;
                ptmp += pitchTmp * 2;
            }
        }

        // vertical filter
        if (dy != 0) {

            filt = loada_si128(fy);                                     // 8w: 0 1 2 3 4 5 6 7
            filt01 = _mm256_broadcastd_epi32(filt);                     // 16b: (0 1) x 8
            filt23 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 4));  // 16b: (2 3) x 8
            filt45 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 8));  // 16b: (4 5) x 8
            filt67 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 12)); // 16b: (6 7) x 8

            ptmp = tmpBlock;
            uint8_t *pdst = dst;

            __m256i ab = loada_si256(ptmp);                 // 16w: A0 A1 A2 A3 A4 A5 A6 A7 | B0 B1 B2 B3 B4 B5 B6 B7
            __m256i cd = loada_si256(ptmp + 2 * pitchTmp);  // 16w: C0 C1 C2 C3 C4 C5 C6 C7 | D0 D1 D2 D3 D4 D5 D6 D7
            __m256i ef = loada_si256(ptmp + 4 * pitchTmp);  // 16w: E0 E1 E2 E3 E4 E5 E6 E7 | F0 F1 F2 F3 F4 F5 F6 F7
            __m256i gh = loada_si256(ptmp + 6 * pitchTmp);  // 16w: G0 G1 G2 G3 G4 G5 G6 G7 | H0 H1 H2 H3 H4 H5 H6 H7
            __m256i ik;

            __m256i bc = permute2x128(ab, cd, 1, 2);        // 16w: B0 B1 B2 B3 B4 B5 B6 B7 | C0 C1 C2 C3 C4 C5 C6 C7
            __m256i de = permute2x128(cd, ef, 1, 2);        // 16w: D0 D1 D2 D3 D4 D5 D6 D7 | E0 E1 E2 E3 E4 E5 E6 E7
            __m256i fg = permute2x128(ef, gh, 1, 2);        // 16w: F0 F1 F2 F3 F4 F5 F6 F7 | G0 G1 G2 G3 G4 G5 G6 G7
            __m256i hi;

            for (int y = 0; y < height; y += 2) {
                ik = loada_si256(ptmp + 8 * pitchTmp);      // 16w: I0 I1 I2 I3 I4 I5 I6 I7 | K0 K1 K2 K3 K4 K5 K6 K7
                hi = permute2x128(gh, ik, 1, 2);            // 16w: H0 H1 H2 H3 H4 H5 H6 H7 | I0 I1 I2 I3 I4 I5 I6 I7

                __m256i abbc, cdde, effg, ghhi, res1, res2;

                abbc = _mm256_unpacklo_epi16(ab, bc);   // 16w: A0 B0 A1 B1 A2 B2 A3 B3 | B0 C0 B1 C1 B2 C2 B3 C3
                cdde = _mm256_unpacklo_epi16(cd, de);   // 16w: C0 D0 C1 D1 C2 D2 C3 D3 | D0 E0 D1 E1 D2 E2 D3 E3
                effg = _mm256_unpacklo_epi16(ef, fg);   // 16w: E0 F0 E1 F1 E2 F2 E3 F3 | F0 G0 F1 G1 F2 G2 F3 G3
                ghhi = _mm256_unpacklo_epi16(gh, hi);   // 16w: G0 H0 G1 H1 G2 H2 G3 H3 | H0 I0 H1 I1 H2 I2 H3 I3

                res1 = filter_s16_s32(abbc, cdde, effg, ghhi, filt01, filt23, filt45, filt67);  // 8d: row0[0..3] | row1[0..3]

                abbc = _mm256_unpackhi_epi16(ab, bc);   // 16w: A4 B4 A5 B5 A6 B6 A7 B7 | B4 C4 B5 C5 B6 C6 B7 C7
                cdde = _mm256_unpackhi_epi16(cd, de);   // 16w: C4 D4 C5 D5 C6 D6 C7 D7 | D4 E4 D5 E5 D6 E6 D7 E7
                effg = _mm256_unpackhi_epi16(ef, fg);   // 16w: E4 F4 E5 F5 E6 F6 E7 F7 | F4 G4 F5 G5 F6 G6 F7 G7
                ghhi = _mm256_unpackhi_epi16(gh, hi);   // 16w: G4 H4 G5 H5 G6 H6 G7 H7 | H4 I4 H5 I5 H6 I6 H7 I7

                res2 = filter_s16_s32(abbc, cdde, effg, ghhi, filt01, filt23, filt45, filt67);  // 8d: row0[4..7] | row1[4..7]

                const __m256i offset = _mm256_set1_epi32(OFFSET_STAGE1 + (ADDITIONAL_OFFSET_STAGE1 << FILTER_BITS));
                res1 = _mm256_add_epi32(res1, offset);
                res2 = _mm256_add_epi32(res2, offset);
                res1 = _mm256_srai_epi32(res1, ROUND_STAGE1);
                res2 = _mm256_srai_epi32(res2, ROUND_STAGE1);
                res1 = _mm256_packus_epi32(res1, res2);  // 16w: row0 | row1
                res1 = _mm256_packus_epi16(res1, res1);  // 32b: row0 row0 | row1 row1
                storel_epi64(pdst + 0 * pitchDst, si128_lo(res1));
                storel_epi64(pdst + 1 * pitchDst, si128_hi(res1));
                pdst += pitchDst * 2;
                ptmp += pitchTmp * 2;

                ab = cd; cd = ef; ef = gh; gh = ik;
                bc = de; de = fg; fg = hi;
            }
        }
        else {
            ptmp = tmpBlock + filterOffY * pitchTmp;
            uint8_t *pdst = dst;
            const __m256i offset = _mm256_set1_epi16(8 + ADDITIONAL_OFFSET_STAGE1);
            for (int y = 0; y < height; y += 2) {
                a = loadu2_m128i(ptmp, ptmp + pitchTmp);
                a = _mm256_add_epi16(a, offset);
                a = _mm256_srai_epi16(a, 4);
                a = _mm256_packus_epi16(a, a);
                storel_epi64(pdst + 0 * pitchDst, si128_lo(a));
                storel_epi64(pdst + 1 * pitchDst, si128_hi(a));

                pdst += pitchDst * 2;
                ptmp += pitchTmp * 2;
            }
        }
    }

    void av1_highbd_convolve_2d_scale_w16_avx2(const uint8_t *src, int32_t pitchSrc, uint8_t *dst, int32_t pitchDst,
                                               int32_t width, int32_t height, uint32_t interp_filters,
                                               int32_t subpel_x_qn, int32_t subpel_y_qn)
    {
        assert((width & 15) == 0);

        alignas(32) int16_t tmpBlock[(64 + 8) * 64];
        const int32_t tmpBlockHeight = height - 1 + FILTER_TAPS;
        const int32_t pitchTmp = width;
        const int32_t filterOffX = FILTER_TAPS / 2 - 1;
        const int32_t filterOffY = FILTER_TAPS / 2 - 1;

        const int32_t ftypeX = (interp_filters >> 4) & 0x3;
        const int32_t ftypeY = interp_filters & 0x3;
        const int32_t dx = (subpel_x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
        const int32_t dy = (subpel_y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
        const int16_t *fx = AV1Enc::av1_filter_kernels[ftypeX][dx];
        const int16_t *fy = AV1Enc::av1_filter_kernels[ftypeY][dy];

        // horizontal filter
        __m128i filt;
        __m256i filt01, filt23, filt45, filt67;
        __m256i a, b, c, d, res;

        filt = loada_si128(fx);                                     // 8w: 0 1 2 3 4 5 6 7
        filt = _mm_packs_epi16(filt, filt);                         // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        filt01 = _mm256_broadcastw_epi16(filt);                     // 32b: (0 1) x 16
        filt23 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 2));  // 32b: (2 3) x 16
        filt45 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 4));  // 32b: (4 5) x 16
        filt67 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 6));  // 32b: (6 7) x 16

        const uint8_t *psrc = src - filterOffY * pitchSrc - filterOffX + (subpel_x_qn >> SCALE_SUBPEL_BITS);
        int16_t *ptmp = tmpBlock;

        if (dx != 0) {
            for (int y = 0; y < tmpBlockHeight; ++y) {
                for (int x = 0; x < width; x += 16) {
                    a = loadu_si256(psrc + 2 * x + 0);      // 32b: 00 01 .. 30 31
                    b = loadu_si256(psrc + 2 * x + 2);      // 32b: 02 03 .. 32 33
                    c = loadu_si256(psrc + 2 * x + 4);      // 32b: 04 05 .. 34 35
                    d = loadu_si256(psrc + 2 * x + 6);      // 32b: 06 07 .. 36 37

                    res = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                    storea_si256(ptmp + x, res);
                }
                psrc += pitchSrc;
                ptmp += pitchTmp;
            }
        }
        else {
            const __m256i even_pels_mask = _mm256_set1_epi16(0x00ff);
            const __m256i additional_offset = _mm256_set1_epi16(ADDITIONAL_OFFSET_STAGE0 >> ROUND0_BITS);
            for (int y = 0; y < tmpBlockHeight; ++y) {
                for (int x = 0; x < width; x += 16) {
                    a = loadu_si256(psrc + 2 * x + 3);
                    a = _mm256_and_si256(a, even_pels_mask);
                    a = _mm256_slli_epi16(a, 4);
                    a = _mm256_add_epi16(a, additional_offset);
                    storea_si256(ptmp + x, a);
                }
                psrc += pitchSrc;
                ptmp += pitchTmp;
            }
        }

        // vertical filter
        if (dy != 0) {

            filt = loada_si128(fy);                                     // 8w: 0 1 2 3 4 5 6 7
            filt01 = _mm256_broadcastd_epi32(filt);                     // 16b: (0 1) x 8
            filt23 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 4));  // 16b: (2 3) x 8
            filt45 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 8));  // 16b: (4 5) x 8
            filt67 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 12)); // 16b: (6 7) x 8

            for (int x = 0; x < width; x += 16) {
                ptmp = tmpBlock + x;
                uint8_t *pdst = dst + x;

                __m256i e, f, g, h, ab, cd, ef, gh, res1, res2;

                a = loada_si256(ptmp);                 // 16w: A0 .. A15
                b = loada_si256(ptmp + 1 * pitchTmp);  // 16w: B0 .. B15
                c = loada_si256(ptmp + 2 * pitchTmp);  // 16w: C0 .. C15
                d = loada_si256(ptmp + 3 * pitchTmp);  // 16w: D0 .. D15
                e = loada_si256(ptmp + 4 * pitchTmp);  // 16w: E0 .. E15
                f = loada_si256(ptmp + 5 * pitchTmp);  // 16w: F0 .. F15
                g = loada_si256(ptmp + 6 * pitchTmp);  // 16w: G0 .. G15

                for (int y = 0; y < height; y++) {
                    h = loada_si256(ptmp + 7 * pitchTmp);      // 16w: H0 .. H15

                    ab = _mm256_unpacklo_epi16(a, b);           // 16w: A0 B0 .. A3 B3 | A8 B8 .. A11 B11
                    cd = _mm256_unpacklo_epi16(c, d);           // 16w: C0 D0 .. C3 D3 | C8 D8 .. C11 D11
                    ef = _mm256_unpacklo_epi16(e, f);           // 16w: E0 F0 .. E3 F3 | E8 F8 .. E11 F11
                    gh = _mm256_unpacklo_epi16(g, h);           // 16w: G0 H0 .. G3 H3 | G8 H8 .. G11 H11

                    res1 = filter_s16_s32(ab, cd, ef, gh, filt01, filt23, filt45, filt67);  // 8d: 0 1 2 3 | 8 9 10 11

                    ab = _mm256_unpackhi_epi16(a, b);           // 16w: A4 B4 .. A7 B7 | A12 B12 .. A15 B15
                    cd = _mm256_unpackhi_epi16(c, d);           // 16w: C4 D4 .. C7 D7 | C12 D12 .. C15 D15
                    ef = _mm256_unpackhi_epi16(e, f);           // 16w: E4 F4 .. E7 F7 | E12 F12 .. E15 F15
                    gh = _mm256_unpackhi_epi16(g, h);           // 16w: G4 H4 .. G7 H7 | G12 H12 .. G15 H15

                    res2 = filter_s16_s32(ab, cd, ef, gh, filt01, filt23, filt45, filt67);  // 8d: 4 5 6 7 | 12 13 14 15

                    const __m256i offset = _mm256_set1_epi32(OFFSET_STAGE1 + (ADDITIONAL_OFFSET_STAGE1 << FILTER_BITS));  // combine 2 offsets
                    res1 = _mm256_add_epi32(res1, offset);
                    res2 = _mm256_add_epi32(res2, offset);
                    res1 = _mm256_srai_epi32(res1, ROUND_STAGE1);
                    res2 = _mm256_srai_epi32(res2, ROUND_STAGE1);
                    res1 = _mm256_packus_epi32(res1, res2); // 16w: 0..7 | 8..15
                    res1 = _mm256_packus_epi16(res1, res1); // 32w: 0..7 0..7 | 8..15 8..15
                    res1 = permute4x64(res1, 0, 2, 0, 2);
                    storea_si128(pdst, si128(res1));

                    a = b; b = c; c = d; d = e; e = f; f = g; g = h;

                    pdst += pitchDst;
                    ptmp += pitchTmp;
                }
            }
        }
        else {
            ptmp = tmpBlock + filterOffY * pitchTmp;
            uint8_t *pdst = dst;
            const __m256i offset = _mm256_set1_epi16(8 + ADDITIONAL_OFFSET_STAGE1);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; x += 16) {
                    a = loada_si256(ptmp + x);
                    a = _mm256_add_epi16(a, offset);
                    a = _mm256_srai_epi16(a, 4);
                    a = _mm256_packus_epi16(a, a);
                    a = permute4x64(a, 0, 2, 1, 3);
                    storea_si128(pdst + x, si128(a));
                }
                pdst += pitchDst;
                ptmp += pitchTmp;
            }
        }
    }

    void av1_highbd_convolve_2d_scale_w8_avx2(const uint8_t *src, int32_t pitchSrc, int16_t *dst, int32_t pitchDst,
        int32_t width, int32_t height, uint32_t interp_filters,
        int32_t subpel_x_qn, int32_t subpel_y_qn)
    {
        assert(width == 8);

        alignas(32) int16_t tmpBlock[(32 + 8) * 8]; // max height = 32, width = 8
        const int32_t tmpBlockHeight = height - 1 + FILTER_TAPS;
        const int32_t pitchTmp = width;
        const int32_t filterOffX = FILTER_TAPS / 2 - 1;
        const int32_t filterOffY = FILTER_TAPS / 2 - 1;

        const int32_t ftypeX = (interp_filters >> 4) & 0x3;
        const int32_t ftypeY = interp_filters & 0x3;
        const int32_t dx = (subpel_x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
        const int32_t dy = (subpel_y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
        const int16_t *fx = AV1Enc::av1_filter_kernels[ftypeX][dx];
        const int16_t *fy = AV1Enc::av1_filter_kernels[ftypeY][dy];

        // horizontal filter
        __m128i filt;
        __m256i filt01, filt23, filt45, filt67;
        __m256i a, b, c, d, res;

        filt = loada_si128(fx);                                     // 8w: 0 1 2 3 4 5 6 7
        filt = _mm_packs_epi16(filt, filt);                         // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        filt01 = _mm256_broadcastw_epi16(filt);                     // 32b: (0 1) x 16
        filt23 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 2));  // 32b: (2 3) x 16
        filt45 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 4));  // 32b: (4 5) x 16
        filt67 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 6));  // 32b: (6 7) x 16

        const uint8_t *psrc = src - filterOffY * pitchSrc - filterOffX + (subpel_x_qn >> SCALE_SUBPEL_BITS);
        int16_t *ptmp = tmpBlock;

        if (dx != 0) {
            for (int y = 0; y < tmpBlockHeight; y += 2) {
                a = loadu2_m128i(psrc + 0, psrc + pitchSrc + 0);      // 32b: 00 01 .. 30 31
                b = loadu2_m128i(psrc + 2, psrc + pitchSrc + 2);      // 32b: 02 03 .. 32 33
                c = loadu2_m128i(psrc + 4, psrc + pitchSrc + 4);      // 32b: 04 05 .. 34 35
                d = loadu2_m128i(psrc + 6, psrc + pitchSrc + 6);      // 32b: 06 07 .. 36 37

                res = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                storea_si256(ptmp, res);

                psrc += pitchSrc * 2;
                ptmp += pitchTmp * 2;
            }
        }
        else {
            const __m256i even_pels_mask = _mm256_set1_epi16(0x00ff);
            const __m256i additional_offset = _mm256_set1_epi16(ADDITIONAL_OFFSET_STAGE0 >> ROUND0_BITS);
            psrc += filterOffX;

            for (int y = 0; y < tmpBlockHeight; y += 2) {
                a = loadu2_m128i(psrc, psrc + pitchSrc);
                a = _mm256_and_si256(a, even_pels_mask);
                a = _mm256_slli_epi16(a, 4);
                a = _mm256_add_epi16(a, additional_offset);
                storea_si256(ptmp, a);

                psrc += pitchSrc * 2;
                ptmp += pitchTmp * 2;
            }
        }

        // vertical filter
        if (dy != 0) {

            filt = loada_si128(fy);                                     // 8w: 0 1 2 3 4 5 6 7
            filt01 = _mm256_broadcastd_epi32(filt);                     // 16b: (0 1) x 8
            filt23 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 4));  // 16b: (2 3) x 8
            filt45 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 8));  // 16b: (4 5) x 8
            filt67 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 12)); // 16b: (6 7) x 8

            ptmp = tmpBlock;
            int16_t *pdst = dst;

            __m256i ab = loada_si256(ptmp);                 // 16w: A0 A1 A2 A3 A4 A5 A6 A7 | B0 B1 B2 B3 B4 B5 B6 B7
            __m256i cd = loada_si256(ptmp + 2 * pitchTmp);  // 16w: C0 C1 C2 C3 C4 C5 C6 C7 | D0 D1 D2 D3 D4 D5 D6 D7
            __m256i ef = loada_si256(ptmp + 4 * pitchTmp);  // 16w: E0 E1 E2 E3 E4 E5 E6 E7 | F0 F1 F2 F3 F4 F5 F6 F7
            __m256i gh = loada_si256(ptmp + 6 * pitchTmp);  // 16w: G0 G1 G2 G3 G4 G5 G6 G7 | H0 H1 H2 H3 H4 H5 H6 H7
            __m256i ik;

            __m256i bc = permute2x128(ab, cd, 1, 2);        // 16w: B0 B1 B2 B3 B4 B5 B6 B7 | C0 C1 C2 C3 C4 C5 C6 C7
            __m256i de = permute2x128(cd, ef, 1, 2);        // 16w: D0 D1 D2 D3 D4 D5 D6 D7 | E0 E1 E2 E3 E4 E5 E6 E7
            __m256i fg = permute2x128(ef, gh, 1, 2);        // 16w: F0 F1 F2 F3 F4 F5 F6 F7 | G0 G1 G2 G3 G4 G5 G6 G7
            __m256i hi;

            for (int y = 0; y < height; y += 2) {
                ik = loada_si256(ptmp + 8 * pitchTmp);      // 16w: I0 I1 I2 I3 I4 I5 I6 I7 | K0 K1 K2 K3 K4 K5 K6 K7
                hi = permute2x128(gh, ik, 1, 2);            // 16w: H0 H1 H2 H3 H4 H5 H6 H7 | I0 I1 I2 I3 I4 I5 I6 I7

                __m256i abbc, cdde, effg, ghhi, res1, res2;

                abbc = _mm256_unpacklo_epi16(ab, bc);   // 16w: A0 B0 A1 B1 A2 B2 A3 B3 | B0 C0 B1 C1 B2 C2 B3 C3
                cdde = _mm256_unpacklo_epi16(cd, de);   // 16w: C0 D0 C1 D1 C2 D2 C3 D3 | D0 E0 D1 E1 D2 E2 D3 E3
                effg = _mm256_unpacklo_epi16(ef, fg);   // 16w: E0 F0 E1 F1 E2 F2 E3 F3 | F0 G0 F1 G1 F2 G2 F3 G3
                ghhi = _mm256_unpacklo_epi16(gh, hi);   // 16w: G0 H0 G1 H1 G2 H2 G3 H3 | H0 I0 H1 I1 H2 I2 H3 I3

                res1 = filter_s16_s32(abbc, cdde, effg, ghhi, filt01, filt23, filt45, filt67);  // 8d: row0[0..3] | row1[0..3]

                abbc = _mm256_unpackhi_epi16(ab, bc);   // 16w: A4 B4 A5 B5 A6 B6 A7 B7 | B4 C4 B5 C5 B6 C6 B7 C7
                cdde = _mm256_unpackhi_epi16(cd, de);   // 16w: C4 D4 C5 D5 C6 D6 C7 D7 | D4 E4 D5 E5 D6 E6 D7 E7
                effg = _mm256_unpackhi_epi16(ef, fg);   // 16w: E4 F4 E5 F5 E6 F6 E7 F7 | F4 G4 F5 G5 F6 G6 F7 G7
                ghhi = _mm256_unpackhi_epi16(gh, hi);   // 16w: G4 H4 G5 H5 G6 H6 G7 H7 | H4 I4 H5 I5 H6 I6 H7 I7

                res2 = filter_s16_s32(abbc, cdde, effg, ghhi, filt01, filt23, filt45, filt67);  // 8d: row0[4..7] | row1[4..7]

                const __m256i offset = _mm256_set1_epi32(OFFSET_PRE_COMPOUND + (1 << (8 + 2 * FILTER_BITS - ROUND0_BITS)));
                res1 = _mm256_add_epi32(res1, offset);
                res2 = _mm256_add_epi32(res2, offset);
                res1 = _mm256_srai_epi32(res1, ROUND_PRE_COMPOUND);
                res2 = _mm256_srai_epi32(res2, ROUND_PRE_COMPOUND);
                res1 = _mm256_packs_epi32(res1, res2);
                storea2_m128i(pdst, pdst + pitchDst, res1);

                ab = cd; cd = ef; ef = gh; gh = ik;
                bc = de; de = fg; fg = hi;

                pdst += pitchDst * 2;
                ptmp += pitchTmp * 2;
            }
        }
        else {
            ptmp = tmpBlock + filterOffY * pitchTmp;
            int16_t *pdst = dst;
            const __m256i offset = _mm256_set1_epi16((1 << (8 + 1 * FILTER_BITS - ROUND0_BITS)));
            for (int y = 0; y < height; y += 2) {
                a = loadu_si256(ptmp);
                a = _mm256_add_epi16(a, offset);
                storea2_m128i(pdst, pdst + pitchDst, a);

                pdst += pitchDst * 2;
                ptmp += pitchTmp * 2;
            }
        }
    }

    void av1_highbd_convolve_2d_scale_w16_avx2(const uint8_t *src, int32_t pitchSrc, int16_t *dst, int32_t pitchDst,
                                               int32_t width, int32_t height, uint32_t interp_filters,
                                               int32_t subpel_x_qn, int32_t subpel_y_qn)
    {
        assert((width & 15) == 0);

        alignas(32) int16_t tmpBlock[(64 + 8) * 64];
        const int32_t tmpBlockHeight = height - 1 + FILTER_TAPS;
        const int32_t pitchTmp = width;
        const int32_t filterOffX = FILTER_TAPS / 2 - 1;
        const int32_t filterOffY = FILTER_TAPS / 2 - 1;

        const int32_t ftypeX = (interp_filters >> 4) & 0x3;
        const int32_t ftypeY = interp_filters & 0x3;
        const int32_t dx = (subpel_x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
        const int32_t dy = (subpel_y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
        const int16_t *fx = AV1Enc::av1_filter_kernels[ftypeX][dx];
        const int16_t *fy = AV1Enc::av1_filter_kernels[ftypeY][dy];

        // horizontal filter
        __m128i filt;
        __m256i filt01, filt23, filt45, filt67;
        __m256i a, b, c, d, res;

        filt = loada_si128(fx);                                     // 8w: 0 1 2 3 4 5 6 7
        filt = _mm_packs_epi16(filt, filt);                         // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        filt01 = _mm256_broadcastw_epi16(filt);                     // 32b: (0 1) x 16
        filt23 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 2));  // 32b: (2 3) x 16
        filt45 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 4));  // 32b: (4 5) x 16
        filt67 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 6));  // 32b: (6 7) x 16

        const uint8_t *psrc = src - filterOffY * pitchSrc - filterOffX + (subpel_x_qn >> SCALE_SUBPEL_BITS);
        int16_t *ptmp = tmpBlock;

        if (dx != 0) {
            for (int y = 0; y < tmpBlockHeight; ++y) {
                for (int x = 0; x < width; x += 16) {
                    a = loadu_si256(psrc + 2 * x + 0);      // 32b: 00 01 .. 30 31
                    b = loadu_si256(psrc + 2 * x + 2);      // 32b: 02 03 .. 32 33
                    c = loadu_si256(psrc + 2 * x + 4);      // 32b: 04 05 .. 34 35
                    d = loadu_si256(psrc + 2 * x + 6);      // 32b: 06 07 .. 36 37

                    res = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                    storea_si256(ptmp + x, res);
                }
                psrc += pitchSrc;
                ptmp += pitchTmp;
            }
        }
        else {
            const __m256i even_pels_mask = _mm256_set1_epi16(0x00ff);
            const __m256i additional_offset = _mm256_set1_epi16(ADDITIONAL_OFFSET_STAGE0 >> ROUND0_BITS);
            for (int y = 0; y < tmpBlockHeight; ++y) {
                for (int x = 0; x < width; x += 16) {
                    a = loadu_si256(psrc + 2 * x + 3);
                    a = _mm256_and_si256(a, even_pels_mask);
                    a = _mm256_slli_epi16(a, 4);
                    a = _mm256_add_epi16(a, additional_offset);
                    storea_si256(ptmp + x, a);
                }
                psrc += pitchSrc;
                ptmp += pitchTmp;
            }
        }

        // vertical filter
        if (dy != 0) {

            filt = loada_si128(fy);                                     // 8w: 0 1 2 3 4 5 6 7
            filt01 = _mm256_broadcastd_epi32(filt);                     // 16b: (0 1) x 8
            filt23 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 4));  // 16b: (2 3) x 8
            filt45 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 8));  // 16b: (4 5) x 8
            filt67 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 12)); // 16b: (6 7) x 8

            for (int x = 0; x < width; x += 16) {
                ptmp = tmpBlock + x;
                int16_t *pdst = dst + x;

                __m256i e, f, g, h, ab, cd, ef, gh, res1, res2;

                a = loada_si256(ptmp);                 // 16w: A0 .. A15
                b = loada_si256(ptmp + 1 * pitchTmp);  // 16w: B0 .. B15
                c = loada_si256(ptmp + 2 * pitchTmp);  // 16w: C0 .. C15
                d = loada_si256(ptmp + 3 * pitchTmp);  // 16w: D0 .. D15
                e = loada_si256(ptmp + 4 * pitchTmp);  // 16w: E0 .. E15
                f = loada_si256(ptmp + 5 * pitchTmp);  // 16w: F0 .. F15
                g = loada_si256(ptmp + 6 * pitchTmp);  // 16w: G0 .. G15

                for (int y = 0; y < height; y++) {
                    h = loada_si256(ptmp + 7 * pitchTmp);      // 16w: H0 .. H15

                    ab = _mm256_unpacklo_epi16(a, b);           // 16w: A0 B0 .. A3 B3 | A8 B8 .. A11 B11
                    cd = _mm256_unpacklo_epi16(c, d);           // 16w: C0 D0 .. C3 D3 | C8 D8 .. C11 D11
                    ef = _mm256_unpacklo_epi16(e, f);           // 16w: E0 F0 .. E3 F3 | E8 F8 .. E11 F11
                    gh = _mm256_unpacklo_epi16(g, h);           // 16w: G0 H0 .. G3 H3 | G8 H8 .. G11 H11

                    res1 = filter_s16_s32(ab, cd, ef, gh, filt01, filt23, filt45, filt67);  // 8d: 0 1 2 3 | 8 9 10 11

                    ab = _mm256_unpackhi_epi16(a, b);           // 16w: A4 B4 .. A7 B7 | A12 B12 .. A15 B15
                    cd = _mm256_unpackhi_epi16(c, d);           // 16w: C4 D4 .. C7 D7 | C12 D12 .. C15 D15
                    ef = _mm256_unpackhi_epi16(e, f);           // 16w: E4 F4 .. E7 F7 | E12 F12 .. E15 F15
                    gh = _mm256_unpackhi_epi16(g, h);           // 16w: G4 H4 .. G7 H7 | G12 H12 .. G15 H15

                    res2 = filter_s16_s32(ab, cd, ef, gh, filt01, filt23, filt45, filt67);  // 8d: 4 5 6 7 | 12 13 14 15

                    const __m256i offset = _mm256_set1_epi32(OFFSET_PRE_COMPOUND + (1 << (8 + 2 * FILTER_BITS - ROUND0_BITS)));
                    res1 = _mm256_add_epi32(res1, offset);
                    res2 = _mm256_add_epi32(res2, offset);
                    res1 = _mm256_srai_epi32(res1, ROUND_PRE_COMPOUND);
                    res2 = _mm256_srai_epi32(res2, ROUND_PRE_COMPOUND);
                    res1 = _mm256_packs_epi32(res1, res2);
                    storea_si256(pdst, res1);

                    a = b; b = c; c = d; d = e; e = f; f = g; g = h;

                    pdst += pitchDst;
                    ptmp += pitchTmp;
                }
            }
        }
        else {
            ptmp = tmpBlock + filterOffY * pitchTmp;
            int16_t *pdst = dst;
            const __m256i offset = _mm256_set1_epi16((1 << (8 + 1 * FILTER_BITS - ROUND0_BITS)));
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; x += 16) {
                    a = loada_si256(ptmp + x);
                    a = _mm256_add_epi16(a, offset);
                    storea_si256(pdst + x, a);
                }
                pdst += pitchDst;
                ptmp += pitchTmp;
            }
        }
    }

    void av1_highbd_convolve_2d_scale_w8_avx2(const uint8_t *src, int32_t pitchSrc, const int16_t *ref0, int32_t pitchRef0,
                                              uint8_t *dst, int32_t pitchDst, int32_t width, int32_t height,
                                              uint32_t interp_filters, int32_t subpel_x_qn, int32_t subpel_y_qn)
    {
        assert(width == 8);

        alignas(32) int16_t tmpBlock[(64 + 8) * 64];
        const int32_t tmpBlockHeight = height - 1 + FILTER_TAPS;
        const int32_t pitchTmp = width;
        const int32_t filterOffX = FILTER_TAPS / 2 - 1;
        const int32_t filterOffY = FILTER_TAPS / 2 - 1;

        const int32_t ftypeX = (interp_filters >> 4) & 0x3;
        const int32_t ftypeY = interp_filters & 0x3;
        const int32_t dx = (subpel_x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
        const int32_t dy = (subpel_y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
        const int16_t *fx = AV1Enc::av1_filter_kernels[ftypeX][dx];
        const int16_t *fy = AV1Enc::av1_filter_kernels[ftypeY][dy];

        // horizontal filter
        __m128i filt;
        __m256i filt01, filt23, filt45, filt67;
        __m256i a, b, c, d, res;

        filt = loada_si128(fx);                                     // 8w: 0 1 2 3 4 5 6 7
        filt = _mm_packs_epi16(filt, filt);                         // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        filt01 = _mm256_broadcastw_epi16(filt);                     // 32b: (0 1) x 16
        filt23 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 2));  // 32b: (2 3) x 16
        filt45 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 4));  // 32b: (4 5) x 16
        filt67 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 6));  // 32b: (6 7) x 16

        const uint8_t *psrc = src - filterOffY * pitchSrc - filterOffX + (subpel_x_qn >> SCALE_SUBPEL_BITS);
        int16_t *ptmp = tmpBlock;

        if (dx != 0) {
            for (int y = 0; y < tmpBlockHeight; y += 2) {
                a = loadu2_m128i(psrc + 0, psrc + pitchSrc + 0);      // 32b: 00 01 .. 30 31
                b = loadu2_m128i(psrc + 2, psrc + pitchSrc + 2);      // 32b: 02 03 .. 32 33
                c = loadu2_m128i(psrc + 4, psrc + pitchSrc + 4);      // 32b: 04 05 .. 34 35
                d = loadu2_m128i(psrc + 6, psrc + pitchSrc + 6);      // 32b: 06 07 .. 36 37

                res = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                storea_si256(ptmp, res);

                psrc += pitchSrc * 2;
                ptmp += pitchTmp * 2;
            }
        }
        else {
            const __m256i even_pels_mask = _mm256_set1_epi16(0x00ff);
            const __m256i additional_offset = _mm256_set1_epi16(ADDITIONAL_OFFSET_STAGE0 >> ROUND0_BITS);
            psrc += filterOffX;

            for (int y = 0; y < tmpBlockHeight; y += 2) {
                a = loadu2_m128i(psrc, psrc + pitchSrc);
                a = _mm256_and_si256(a, even_pels_mask);
                a = _mm256_slli_epi16(a, 4);
                a = _mm256_add_epi16(a, additional_offset);
                storea_si256(ptmp, a);

                psrc += pitchSrc * 2;
                ptmp += pitchTmp * 2;
            }
        }

        // vertical filter
        if (dy != 0) {

            filt = loada_si128(fy);                                     // 8w: 0 1 2 3 4 5 6 7
            filt01 = _mm256_broadcastd_epi32(filt);                     // 16b: (0 1) x 8
            filt23 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 4));  // 16b: (2 3) x 8
            filt45 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 8));  // 16b: (4 5) x 8
            filt67 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 12)); // 16b: (6 7) x 8

            ptmp = tmpBlock;
            const int16_t *pref = ref0;
            uint8_t *pdst = dst;

            __m256i ab = loada_si256(ptmp);                 // 16w: A0 A1 A2 A3 A4 A5 A6 A7 | B0 B1 B2 B3 B4 B5 B6 B7
            __m256i cd = loada_si256(ptmp + 2 * pitchTmp);  // 16w: C0 C1 C2 C3 C4 C5 C6 C7 | D0 D1 D2 D3 D4 D5 D6 D7
            __m256i ef = loada_si256(ptmp + 4 * pitchTmp);  // 16w: E0 E1 E2 E3 E4 E5 E6 E7 | F0 F1 F2 F3 F4 F5 F6 F7
            __m256i gh = loada_si256(ptmp + 6 * pitchTmp);  // 16w: G0 G1 G2 G3 G4 G5 G6 G7 | H0 H1 H2 H3 H4 H5 H6 H7
            __m256i ik;

            __m256i bc = permute2x128(ab, cd, 1, 2);        // 16w: B0 B1 B2 B3 B4 B5 B6 B7 | C0 C1 C2 C3 C4 C5 C6 C7
            __m256i de = permute2x128(cd, ef, 1, 2);        // 16w: D0 D1 D2 D3 D4 D5 D6 D7 | E0 E1 E2 E3 E4 E5 E6 E7
            __m256i fg = permute2x128(ef, gh, 1, 2);        // 16w: F0 F1 F2 F3 F4 F5 F6 F7 | G0 G1 G2 G3 G4 G5 G6 G7
            __m256i hi;

            for (int y = 0; y < height; y += 2) {
                ik = loada_si256(ptmp + 8 * pitchTmp);      // 16w: I0 I1 I2 I3 I4 I5 I6 I7 | K0 K1 K2 K3 K4 K5 K6 K7
                hi = permute2x128(gh, ik, 1, 2);            // 16w: H0 H1 H2 H3 H4 H5 H6 H7 | I0 I1 I2 I3 I4 I5 I6 I7

                __m256i abbc, cdde, effg, ghhi, res1, res2;

                abbc = _mm256_unpacklo_epi16(ab, bc);   // 16w: A0 B0 A1 B1 A2 B2 A3 B3 | B0 C0 B1 C1 B2 C2 B3 C3
                cdde = _mm256_unpacklo_epi16(cd, de);   // 16w: C0 D0 C1 D1 C2 D2 C3 D3 | D0 E0 D1 E1 D2 E2 D3 E3
                effg = _mm256_unpacklo_epi16(ef, fg);   // 16w: E0 F0 E1 F1 E2 F2 E3 F3 | F0 G0 F1 G1 F2 G2 F3 G3
                ghhi = _mm256_unpacklo_epi16(gh, hi);   // 16w: G0 H0 G1 H1 G2 H2 G3 H3 | H0 I0 H1 I1 H2 I2 H3 I3

                res1 = filter_s16_s32(abbc, cdde, effg, ghhi, filt01, filt23, filt45, filt67);  // 8d: row0[0..3] | row1[0..3]

                abbc = _mm256_unpackhi_epi16(ab, bc);   // 16w: A4 B4 A5 B5 A6 B6 A7 B7 | B4 C4 B5 C5 B6 C6 B7 C7
                cdde = _mm256_unpackhi_epi16(cd, de);   // 16w: C4 D4 C5 D5 C6 D6 C7 D7 | D4 E4 D5 E5 D6 E6 D7 E7
                effg = _mm256_unpackhi_epi16(ef, fg);   // 16w: E4 F4 E5 F5 E6 F6 E7 F7 | F4 G4 F5 G5 F6 G6 F7 G7
                ghhi = _mm256_unpackhi_epi16(gh, hi);   // 16w: G4 H4 G5 H5 G6 H6 G7 H7 | H4 I4 H5 I5 H6 I6 H7 I7

                res2 = filter_s16_s32(abbc, cdde, effg, ghhi, filt01, filt23, filt45, filt67);  // 8d: row0[4..7] | row1[4..7]


                res1 = _mm256_add_epi32(res1, _mm256_set1_epi32(OFFSET_PRE_COMPOUND + (1 << (8 + 2 * FILTER_BITS - ROUND0_BITS)) ));
                res2 = _mm256_add_epi32(res2, _mm256_set1_epi32(OFFSET_PRE_COMPOUND + (1 << (8 + 2 * FILTER_BITS - ROUND0_BITS))));
                res1 = _mm256_srai_epi32(res1, ROUND_PRE_COMPOUND);
                res2 = _mm256_srai_epi32(res2, ROUND_PRE_COMPOUND);
                res1 = _mm256_packs_epi32(res1, res2);  // 16w: 0..7 | 8..15

                res1 = _mm256_add_epi16(res1, loada2_m128i(pref, pref + pitchRef0));
                res1 = _mm256_srai_epi16(res1, 1);

                const __m256i offset = _mm256_set1_epi16(8/*OFFSET_POST_COMPOUND*/ - (1 << (8 + 2 * FILTER_BITS - ROUND0_BITS - ROUND_PRE_COMPOUND))
                                                                                - (1 << (8 + 2 * FILTER_BITS - ROUND0_BITS - ROUND_PRE_COMPOUND - 1)));

                res1 = _mm256_add_epi16(res1, offset);
                res1 = _mm256_srai_epi16(res1, FILTER_BITS - ROUND0_BITS);
                res1 = _mm256_packus_epi16(res1, res1); // 32w: 0..7 0..7 | 8..15 8..15
                res1 = permute4x64(res1, 0, 2, 0, 2);
                storel_epi64(pdst + 0 * pitchDst, si128(res1));
                storeh_epi64(pdst + 1 * pitchDst, si128(res1));

                ab = cd; cd = ef; ef = gh; gh = ik;
                bc = de; de = fg; fg = hi;

                pdst += pitchDst * 2;
                ptmp += pitchTmp * 2;
                pref += pitchRef0 * 2;
            }
        }
        else {
            ptmp = tmpBlock + filterOffY * pitchTmp;
            const int16_t *pref = ref0;
            uint8_t *pdst = dst;
            const __m256i offset = _mm256_set1_epi16(8/*OFFSET_POST_COMPOUND*/ - (1 << (8 + 2 * FILTER_BITS - ROUND0_BITS - ROUND_PRE_COMPOUND))
                                                                          - (1 << (8 + 2 * FILTER_BITS - ROUND0_BITS - ROUND_PRE_COMPOUND - 1)));

            const __m256i pre_compound_offset = _mm256_set1_epi16(1 << (8 + 2 * FILTER_BITS - ROUND0_BITS - ROUND_PRE_COMPOUND));
            for (int y = 0; y < height; y += 2) {
                a = loadu_si256(ptmp);
                a = _mm256_add_epi16(a, pre_compound_offset);
                a = _mm256_add_epi16(a, loada2_m128i(pref, pref + pitchRef0));
                a = _mm256_srai_epi16(a, 1);
                a = _mm256_add_epi16(a, offset);
                a = _mm256_srai_epi16(a, FILTER_BITS - ROUND0_BITS);
                a = _mm256_packus_epi16(a, a); // 32w: 0..7 0..7 | 8..15 8..15
                a = permute4x64(a, 0, 2, 0, 2);
                storel_epi64(pdst + 0 * pitchDst, si128(a));
                storeh_epi64(pdst + 1 * pitchDst, si128(a));

                pdst += pitchDst * 2;
                ptmp += pitchTmp * 2;
                pref += pitchRef0 * 2;
            }
        }
    }

    void av1_highbd_convolve_2d_scale_w16_avx2(const uint8_t *src, int32_t pitchSrc, const int16_t *ref0, int32_t pitchRef0,
                                               uint8_t *dst, int32_t pitchDst, int32_t width, int32_t height,
                                               uint32_t interp_filters, int32_t subpel_x_qn, int32_t subpel_y_qn)
    {
        assert((width & 15) == 0);

        alignas(32) int16_t tmpBlock[(64 + 8) * 64];
        const int32_t tmpBlockHeight = height - 1 + FILTER_TAPS;
        const int32_t pitchTmp = width;
        const int32_t filterOffX = FILTER_TAPS / 2 - 1;
        const int32_t filterOffY = FILTER_TAPS / 2 - 1;

        const int32_t ftypeX = (interp_filters >> 4) & 0x3;
        const int32_t ftypeY = interp_filters & 0x3;
        const int32_t dx = (subpel_x_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
        const int32_t dy = (subpel_y_qn & SCALE_SUBPEL_MASK) >> SCALE_EXTRA_BITS;
        const int16_t *fx = AV1Enc::av1_filter_kernels[ftypeX][dx];
        const int16_t *fy = AV1Enc::av1_filter_kernels[ftypeY][dy];

        // horizontal filter
        __m128i filt;
        __m256i filt01, filt23, filt45, filt67;
        __m256i a, b, c, d, res;

        filt = loada_si128(fx);                                     // 8w: 0 1 2 3 4 5 6 7
        filt = _mm_packs_epi16(filt, filt);                         // 16b: 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
        filt01 = _mm256_broadcastw_epi16(filt);                     // 32b: (0 1) x 16
        filt23 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 2));  // 32b: (2 3) x 16
        filt45 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 4));  // 32b: (4 5) x 16
        filt67 = _mm256_broadcastw_epi16(_mm_srli_si128(filt, 6));  // 32b: (6 7) x 16

        const uint8_t *psrc = src - filterOffY * pitchSrc - filterOffX + (subpel_x_qn >> SCALE_SUBPEL_BITS);
        int16_t *ptmp = tmpBlock;

        if (dx != 0) {
            for (int y = 0; y < tmpBlockHeight; ++y) {
                for (int x = 0; x < width; x += 16) {
                    a = loadu_si256(psrc + 2 * x + 0);      // 32b: 00 01 .. 30 31
                    b = loadu_si256(psrc + 2 * x + 2);      // 32b: 02 03 .. 32 33
                    c = loadu_si256(psrc + 2 * x + 4);      // 32b: 04 05 .. 34 35
                    d = loadu_si256(psrc + 2 * x + 6);      // 32b: 06 07 .. 36 37

                    res = filter_u8_u16(a, b, c, d, filt01, filt23, filt45, filt67);
                    storea_si256(ptmp + x, res);
                }
                psrc += pitchSrc;
                ptmp += pitchTmp;
            }
        }
        else {
            const __m256i even_pels_mask = _mm256_set1_epi16(0x00ff);
            const __m256i additional_offset = _mm256_set1_epi16(ADDITIONAL_OFFSET_STAGE0 >> ROUND0_BITS);
            for (int y = 0; y < tmpBlockHeight; ++y) {
                for (int x = 0; x < width; x += 16) {
                    a = loadu_si256(psrc + 2 * x + 3);
                    a = _mm256_and_si256(a, even_pels_mask);
                    a = _mm256_slli_epi16(a, 4);
                    a = _mm256_add_epi16(a, additional_offset);
                    storea_si256(ptmp + x, a);
                }
                psrc += pitchSrc;
                ptmp += pitchTmp;
            }
        }

        // vertical filter
        if (dy != 0) {

            filt = loada_si128(fy);                                     // 8w: 0 1 2 3 4 5 6 7
            filt01 = _mm256_broadcastd_epi32(filt);                     // 16b: (0 1) x 8
            filt23 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 4));  // 16b: (2 3) x 8
            filt45 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 8));  // 16b: (4 5) x 8
            filt67 = _mm256_broadcastd_epi32(_mm_srli_si128(filt, 12)); // 16b: (6 7) x 8

            for (int x = 0; x < width; x += 16) {
                ptmp = tmpBlock + x;
                const int16_t *pref = ref0 + x;
                uint8_t *pdst = dst + x;

                __m256i e, f, g, h, ab, cd, ef, gh, res1, res2;

                a = loada_si256(ptmp);                 // 16w: A0 .. A15
                b = loada_si256(ptmp + 1 * pitchTmp);  // 16w: B0 .. B15
                c = loada_si256(ptmp + 2 * pitchTmp);  // 16w: C0 .. C15
                d = loada_si256(ptmp + 3 * pitchTmp);  // 16w: D0 .. D15
                e = loada_si256(ptmp + 4 * pitchTmp);  // 16w: E0 .. E15
                f = loada_si256(ptmp + 5 * pitchTmp);  // 16w: F0 .. F15
                g = loada_si256(ptmp + 6 * pitchTmp);  // 16w: G0 .. G15

                for (int y = 0; y < height; y++) {
                    h = loada_si256(ptmp + 7 * pitchTmp);      // 16w: H0 .. H15

                    ab = _mm256_unpacklo_epi16(a, b);           // 16w: A0 B0 .. A3 B3 | A8 B8 .. A11 B11
                    cd = _mm256_unpacklo_epi16(c, d);           // 16w: C0 D0 .. C3 D3 | C8 D8 .. C11 D11
                    ef = _mm256_unpacklo_epi16(e, f);           // 16w: E0 F0 .. E3 F3 | E8 F8 .. E11 F11
                    gh = _mm256_unpacklo_epi16(g, h);           // 16w: G0 H0 .. G3 H3 | G8 H8 .. G11 H11

                    res1 = filter_s16_s32(ab, cd, ef, gh, filt01, filt23, filt45, filt67);  // 8d: 0 1 2 3 | 8 9 10 11

                    ab = _mm256_unpackhi_epi16(a, b);           // 16w: A4 B4 .. A7 B7 | A12 B12 .. A15 B15
                    cd = _mm256_unpackhi_epi16(c, d);           // 16w: C4 D4 .. C7 D7 | C12 D12 .. C15 D15
                    ef = _mm256_unpackhi_epi16(e, f);           // 16w: E4 F4 .. E7 F7 | E12 F12 .. E15 F15
                    gh = _mm256_unpackhi_epi16(g, h);           // 16w: G4 H4 .. G7 H7 | G12 H12 .. G15 H15

                    res2 = filter_s16_s32(ab, cd, ef, gh, filt01, filt23, filt45, filt67);  // 8d: 4 5 6 7 | 12 13 14 15

                    res1 = _mm256_add_epi32(res1, _mm256_set1_epi32(OFFSET_PRE_COMPOUND + (1 << (8 + 2 * FILTER_BITS - ROUND0_BITS)) ));
                    res2 = _mm256_add_epi32(res2, _mm256_set1_epi32(OFFSET_PRE_COMPOUND + (1 << (8 + 2 * FILTER_BITS - ROUND0_BITS))));
                    res1 = _mm256_srai_epi32(res1, ROUND_PRE_COMPOUND);
                    res2 = _mm256_srai_epi32(res2, ROUND_PRE_COMPOUND);
                    res1 = _mm256_packs_epi32(res1, res2);  // 16w: 0..7 | 8..15

                    res1 = _mm256_add_epi16(res1, loada_si256(pref));
                    res1 = _mm256_srai_epi16(res1, 1);

                    const __m256i offset = _mm256_set1_epi16(8/*OFFSET_POST_COMPOUND*/ - (1 << (8 + 2 * FILTER_BITS - ROUND0_BITS - ROUND_PRE_COMPOUND))
                                                                                  - (1 << (8 + 2 * FILTER_BITS - ROUND0_BITS - ROUND_PRE_COMPOUND - 1)));

                    res1 = _mm256_add_epi16(res1, offset);
                    res1 = _mm256_srai_epi16(res1, FILTER_BITS - ROUND0_BITS);
                    res1 = _mm256_packus_epi16(res1, res1); // 32w: 0..7 0..7 | 8..15 8..15
                    res1 = permute4x64(res1, 0, 2, 0, 2);
                    storea_si128(pdst, si128(res1));

                    a = b; b = c; c = d; d = e; e = f; f = g; g = h;

                    pdst += pitchDst;
                    ptmp += pitchTmp;
                    pref += pitchRef0;
                }
            }
        }
        else {
            ptmp = tmpBlock + filterOffY * pitchTmp;
            const int16_t *pref = ref0;
            uint8_t *pdst = dst;
            const __m256i offset = _mm256_set1_epi16(8/*OFFSET_POST_COMPOUND*/ - (1 << (8 + 2 * FILTER_BITS - ROUND0_BITS - ROUND_PRE_COMPOUND))
                                                                          - (1 << (8 + 2 * FILTER_BITS - ROUND0_BITS - ROUND_PRE_COMPOUND - 1)));

            const __m256i pre_compound_offset = _mm256_set1_epi16(1 << (8 + 2 * FILTER_BITS - ROUND0_BITS - ROUND_PRE_COMPOUND));
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; x += 16) {
                    a = loada_si256(ptmp + x);
                    a = _mm256_add_epi16(a, pre_compound_offset);
                    a = _mm256_add_epi16(a, loada_si256(pref + x));
                    a = _mm256_srai_epi16(a, 1);
                    a = _mm256_add_epi16(a, offset);
                    a = _mm256_srai_epi16(a, FILTER_BITS - ROUND0_BITS);
                    a = _mm256_packus_epi16(a, a); // 32w: 0..7 0..7 | 8..15 8..15
                    a = permute4x64(a, 0, 2, 0, 2);
                    storea_si128(pdst + x, si128(a));
                }
                pdst += pitchDst;
                ptmp += pitchTmp;
                pref += pitchRef0;
            }
        }
    }

    void AV1Enc::InterpolateAv1SingleRefLuma_ScaleMode_new(FrameData* refFrame, uint8_t *dst, AV1MV mv, int32_t x, int32_t y, int32_t w, int32_t h, int32_t interp)
    {
        SubpelParams subpel_params;
        PadBlock block;
        calc_subpel_params(mv, 0, &subpel_params, w, h, &block, x, y, refFrame->width, refFrame->height);
        const int pitch = refFrame->pitch_luma_pix;
        const uint8_t* refColoc = refFrame->y + block.y0 * pitch + block.x0;

        const int32_t x_step_qn = 2048;
        const int32_t y_step_qn = 1024;
        int subpel_x_qn = subpel_params.subpel_x;
        int subpel_y_qn = subpel_params.subpel_y;
        ConvolveParams conv_params_ = get_conv_params_no_round(0, 0, 0, nullptr, MAX_SB_SIZE, 0, 8);
        if (w >= 16)
            av1_highbd_convolve_2d_scale_w16_avx2(refColoc, pitch, dst, 64, w, h, interp, subpel_x_qn, subpel_y_qn);
        else if (w == 8)
            av1_highbd_convolve_2d_scale_w8_avx2(refColoc, pitch, dst, 64, w, h, interp, subpel_x_qn, subpel_y_qn);
        else {
            assert(0);
            //av1_highbd_convolve_2d_scale_c(refColoc, pitch, dst, 64, w, h, interp, subpel_x_qn, x_step_qn, subpel_y_qn, y_step_qn, &conv_params_, 8);
        }
    }
    void AV1Enc::InterpolateAv1FirstRefLuma_ScaleMode_new(FrameData* refFrame, int16_t *dst, AV1MV mv, int32_t x, int32_t y, int32_t w, int32_t h, int32_t interp)
    {
        SubpelParams subpel_params;
        PadBlock block;
        calc_subpel_params(mv, 0, &subpel_params, w, h, &block, x, y, refFrame->width, refFrame->height);
        const int pitch = refFrame->pitch_luma_pix;
        const uint8_t* refColoc = refFrame->y + block.y0 * pitch + block.x0;
        const int32_t x_step_qn = 2048;
        const int32_t y_step_qn = 1024;
        int subpel_x_qn = subpel_params.subpel_x;
        int subpel_y_qn = subpel_params.subpel_y;
        ConvolveParams conv_params_ = get_conv_params_no_round(0, 0, 0, (uint16_t *)dst, IMPLIED_PITCH, 1, 8);

        if (w >= 16) {
            av1_highbd_convolve_2d_scale_w16_avx2(refColoc, pitch, dst, IMPLIED_PITCH, w, h, interp, subpel_x_qn, subpel_y_qn);
            //av1_highbd_convolve_2d_scale_c(refColoc, pitch, nullptr, 64, w, h, interp, subpel_x_qn, x_step_qn, subpel_y_qn, y_step_qn, &conv_params_, 8);
        }
        else {
            assert(w == 8);
            av1_highbd_convolve_2d_scale_w8_avx2(refColoc, pitch, dst, IMPLIED_PITCH, w, h, interp, subpel_x_qn, subpel_y_qn);
            //av1_highbd_convolve_2d_scale_c(refColoc, pitch, nullptr, 64, w, h, interp, subpel_x_qn, x_step_qn, subpel_y_qn, y_step_qn, &conv_params_, 8);
        }
    }
    void AV1Enc::InterpolateAv1SecondRefLuma_ScaleMode_new(FrameData* refFrame, const int16_t* ref0, uint8_t *dst, AV1MV mv, int32_t x, int32_t y, int32_t w, int32_t h, int32_t interp)
    {
        SubpelParams subpel_params;
        PadBlock block;
        calc_subpel_params(mv, 0, &subpel_params, w, h, &block, x, y, refFrame->width, refFrame->height);
        const int pitch = refFrame->pitch_luma_pix;
        const uint8_t* refColoc = refFrame->y + block.y0 * pitch + block.x0;
        const int32_t x_step_qn = 2048;
        const int32_t y_step_qn = 1024;
        int subpel_x_qn = subpel_params.subpel_x;
        int subpel_y_qn = subpel_params.subpel_y;
        ConvolveParams conv_params_ = get_conv_params_no_round(0, 1, 0, (uint16_t*)ref0, IMPLIED_PITCH, 1, 8);

        if (w >= 16) {
            av1_highbd_convolve_2d_scale_w16_avx2(refColoc, pitch, ref0, IMPLIED_PITCH, dst, IMPLIED_PITCH, w, h, interp, subpel_x_qn, subpel_y_qn);
            //av1_highbd_convolve_2d_scale_c(refColoc, pitch, dst, 64, w, h, interp, subpel_x_qn, x_step_qn, subpel_y_qn, y_step_qn, &conv_params_, 8);
        }
        else {
            assert(w == 8);
            av1_highbd_convolve_2d_scale_w8_avx2(refColoc, pitch, ref0, IMPLIED_PITCH, dst, IMPLIED_PITCH, w, h, interp, subpel_x_qn, subpel_y_qn);
            //av1_highbd_convolve_2d_scale_c(refColoc, pitch, dst, 64, w, h, interp, subpel_x_qn, x_step_qn, subpel_y_qn, y_step_qn, &conv_params_, 8);
        }

    }

    static void convertNv12ToUv(const uint8_t* src_uv, int pitch_chroma_pix, uint8_t* src_u, uint8_t* src_v, int pitchDst, int width, int height)
    {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                src_u[y*pitchDst + x] = src_uv[y*pitch_chroma_pix + 2 * x];
                src_v[y*pitchDst + x] = src_uv[y*pitch_chroma_pix + 2 * x + 1];
            }
        }
    }

    static void convertUvToNv12(uint8_t *dst_u, uint8_t *dst_v, int pitchSrc, int width, int height, uint8_t *uv, int pitch_dst)
    {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                uv[y*pitch_dst + 2 * x] = dst_u[y*pitchSrc + x];
                uv[y*pitch_dst + 2 * x + 1] = dst_v[y*pitchSrc + x];
            }
        }
    }

    void AV1Enc::InterpolateAv1SingleRefChroma_ScaleMode(const uint8_t *refColoc, int32_t refPitch, uint8_t *dst, AV1MV mv, int32_t w, int32_t h, int32_t interp, SubpelParams *subpel_params)
    {
        const int32_t FILTER_PAD = 4;
        const int32_t MAX_WIDTH = 64;
        const int32_t MAX_HEIGHT = 64;
        const int32_t SUPERRES_SCALE = 2;

        alignas(32) uint8_t srcU[(MAX_WIDTH + 2 * FILTER_PAD) * (MAX_HEIGHT + 2 * FILTER_PAD)];
        alignas(32) uint8_t srcV[(MAX_WIDTH + 2 * FILTER_PAD) * (MAX_HEIGHT + 2 * FILTER_PAD)];
        const int32_t heightSrc = h + 2 * FILTER_PAD;
        const int32_t widthSrc = w * SUPERRES_SCALE + 2 * FILTER_PAD;
        const int32_t pitchSrc = (widthSrc + 31) & ~31;

        alignas(32) uint8_t dstV[MAX_WIDTH * MAX_HEIGHT];
        alignas(32) uint8_t dstU[MAX_WIDTH * MAX_HEIGHT];
        const int32_t pitchDst = w;

        refColoc -= FILTER_PAD * refPitch + 2 * FILTER_PAD;
        convertNv12ToUv(refColoc, refPitch, srcU, srcV, pitchSrc, widthSrc, heightSrc);

        const uint8_t *psrcU = srcU + FILTER_PAD * pitchSrc + FILTER_PAD;
        const uint8_t *psrcV = srcV + FILTER_PAD * pitchSrc + FILTER_PAD;

        const int32_t subpel_x_qn = subpel_params->subpel_x;
        const int32_t subpel_y_qn = subpel_params->subpel_y;

        if (w >= 16) {
            av1_highbd_convolve_2d_scale_w16_avx2(psrcU, pitchSrc, dstU, pitchDst, w, h, interp, subpel_x_qn, subpel_y_qn);
            av1_highbd_convolve_2d_scale_w16_avx2(psrcV, pitchSrc, dstV, pitchDst, w, h, interp, subpel_x_qn, subpel_y_qn);
        }
        else if (w == 8) {
            av1_highbd_convolve_2d_scale_w8_avx2(psrcU, pitchSrc, dstU, pitchDst, w, h, interp, subpel_x_qn, subpel_y_qn);
            av1_highbd_convolve_2d_scale_w8_avx2(psrcV, pitchSrc, dstV, pitchDst, w, h, interp, subpel_x_qn, subpel_y_qn);
        }
        else {
            assert(w == 4);
            av1_highbd_convolve_2d_scale_w4_avx2(psrcU, pitchSrc, dstU, pitchDst, w, h, interp, subpel_x_qn, subpel_y_qn);
            av1_highbd_convolve_2d_scale_w4_avx2(psrcV, pitchSrc, dstV, pitchDst, w, h, interp, subpel_x_qn, subpel_y_qn);
        }

        convertUvToNv12(dstU, dstV, pitchDst, w, h, dst, 64);
    }

    void AV1Enc::InterpolateAv1FirstRefChroma_ScaleMode(const uint8_t *refColoc, int32_t refPitch, uint16_t *dst, AV1MV mv, int32_t w, int32_t h, int32_t interp, SubpelParams *subpel_params)
    {
        const int32_t FILTER_PAD = 4;
        const int32_t MAX_WIDTH = 64;
        const int32_t MAX_HEIGHT = 64;
        const int32_t SUPERRES_SCALE = 2;

        alignas(32) uint8_t srcU[(MAX_WIDTH + 2 * FILTER_PAD) * (MAX_HEIGHT + 2 * FILTER_PAD)];
        alignas(32) uint8_t srcV[(MAX_WIDTH + 2 * FILTER_PAD) * (MAX_HEIGHT + 2 * FILTER_PAD)];
        const int32_t heightSrc = h + 2 * FILTER_PAD;
        const int32_t widthSrc = w * SUPERRES_SCALE + 2 * FILTER_PAD;
        const int32_t pitchSrc = (widthSrc + 31) & ~31;

        alignas(32) uint8_t dstV[MAX_WIDTH * MAX_HEIGHT];
        alignas(32) uint8_t dstU[MAX_WIDTH * MAX_HEIGHT];
        const int32_t pitchDst = w;

        refColoc -= FILTER_PAD * refPitch + 2 * FILTER_PAD;
        convertNv12ToUv(refColoc, refPitch, srcU, srcV, pitchSrc, widthSrc, heightSrc);

        const uint8_t *psrcU = srcU + FILTER_PAD * pitchSrc + FILTER_PAD;
        const uint8_t *psrcV = srcV + FILTER_PAD * pitchSrc + FILTER_PAD;

        const int32_t x_step_qn = 2048;
        const int32_t y_step_qn = 1024;
        int subpel_x_qn = subpel_params->subpel_x;
        int subpel_y_qn = subpel_params->subpel_y;
        const int IMPL_PITCH = 64;
        ConvolveParams conv_params_ = get_conv_params_no_round(0, 0, /*plane*/1, dst, IMPL_PITCH, /*is_compound*/1, 8);
        av1_highbd_convolve_2d_scale_c(psrcU, pitchSrc, dstU, pitchDst, w, h, interp, subpel_x_qn, x_step_qn, subpel_y_qn, y_step_qn, &conv_params_, 8);
        /*ConvolveParams */conv_params_ = get_conv_params_no_round(0, 0, /*plane*/1, dst + h * IMPL_PITCH, IMPL_PITCH, /*is_compound*/1, 8);
        av1_highbd_convolve_2d_scale_c(psrcV, pitchSrc, dstV, pitchDst, w, h, interp, subpel_x_qn, x_step_qn, subpel_y_qn, y_step_qn, &conv_params_, 8);
    }

    void AV1Enc::InterpolateAv1SecondRefChroma_ScaleMode(const uint8_t *refColoc, int32_t refPitch, uint16_t *ref0, uint8_t *dst, AV1MV mv, int32_t w, int32_t h, int32_t interp, SubpelParams *subpel_params)
    {
        const int32_t FILTER_PAD = 4;
        const int32_t MAX_WIDTH = 64;
        const int32_t MAX_HEIGHT = 64;
        const int32_t SUPERRES_SCALE = 2;

        alignas(32) uint8_t srcU[(MAX_WIDTH + 2 * FILTER_PAD) * (MAX_HEIGHT + 2 * FILTER_PAD)];
        alignas(32) uint8_t srcV[(MAX_WIDTH + 2 * FILTER_PAD) * (MAX_HEIGHT + 2 * FILTER_PAD)];
        const int32_t heightSrc = h + 2 * FILTER_PAD;
        const int32_t widthSrc = w * SUPERRES_SCALE + 2 * FILTER_PAD;
        const int32_t pitchSrc = (widthSrc + 31) & ~31;

        alignas(32) uint8_t dstV[MAX_WIDTH * MAX_HEIGHT];
        alignas(32) uint8_t dstU[MAX_WIDTH * MAX_HEIGHT];
        const int32_t pitchDst = w;

        refColoc -= FILTER_PAD * refPitch + 2 * FILTER_PAD;
        convertNv12ToUv(refColoc, refPitch, srcU, srcV, pitchSrc, widthSrc, heightSrc);

        const uint8_t *psrcU = srcU + FILTER_PAD * pitchSrc + FILTER_PAD;
        const uint8_t *psrcV = srcV + FILTER_PAD * pitchSrc + FILTER_PAD;

        const int32_t x_step_qn = 2048;
        const int32_t y_step_qn = 1024;
        int subpel_x_qn = subpel_params->subpel_x;
        int subpel_y_qn = subpel_params->subpel_y;
        const int IMPL_PITCH = 64;

        ConvolveParams conv_params_ = get_conv_params_no_round(0, 1, /*plane*/1, ref0, IMPL_PITCH, /*is_compound*/1, 8);
        av1_highbd_convolve_2d_scale_c(psrcU, pitchSrc, dstU, pitchDst, w, h, interp, subpel_x_qn, x_step_qn, subpel_y_qn, y_step_qn, &conv_params_, 8);
        /*ConvolveParams */conv_params_ = get_conv_params_no_round(0, 1, /*plane*/1, ref0 + h * IMPL_PITCH, IMPL_PITCH, /*is_compound*/1, 8);
        av1_highbd_convolve_2d_scale_c(psrcV, pitchSrc, dstV, pitchDst, w, h, interp, subpel_x_qn, x_step_qn, subpel_y_qn, y_step_qn, &conv_params_, 8);
        convertUvToNv12(dstU, dstV, pitchDst, w, h, dst, 64);
    }

#define REF_SCALE_SHIFT 14
#define SCALE_SUBPEL_BITS 10
#define SCALE_EXTRA_BITS (SCALE_SUBPEL_BITS - SUBPEL_BITS)
#define SCALE_EXTRA_OFF ((1 << SCALE_EXTRA_BITS) / 2)

#define AOM_BORDER_IN_PIXELS 288
#define AOM_INTERP_EXTEND 4

#define AOM_LEFT_TOP_MARGIN_PX(subsampling) \
  ((AOM_BORDER_IN_PIXELS >> subsampling) - AOM_INTERP_EXTEND)
#define AOM_LEFT_TOP_MARGIN_SCALED(subsampling) \
  (AOM_LEFT_TOP_MARGIN_PX(subsampling) << SCALE_SUBPEL_BITS)

    /* Shift down with rounding for use when n >= 0, value >= 0 for (64 bit) */
#define ROUND_POWER_OF_TWO_64(value, n) \
  (((value) + ((((int64_t)1 << (n)) >> 1))) >> (n))
/* Shift down with rounding for signed integers, for use when n >= 0 (64 bit) */
#define ROUND_POWER_OF_TWO_SIGNED_64(value, n)           \
  (((value) < 0) ? -ROUND_POWER_OF_TWO_64(-(value), (n)) \
                 : ROUND_POWER_OF_TWO_64((value), (n)))

    static int scale_value(int val, int scale_fp)
    {
        const int off = (scale_fp - (1 << REF_SCALE_SHIFT)) * (1 << (SUBPEL_BITS - 1));
        const int64_t tval = (int64_t)val * scale_fp + off;

        return (int)ROUND_POWER_OF_TWO_SIGNED_64(tval, REF_SCALE_SHIFT - SCALE_EXTRA_BITS);
    }

    void AV1Enc::calc_subpel_params(
        const AV1MV mv,
        int plane,
        SubpelParams *subpel_params,
        int bw, int bh, PadBlock *block,
        int mi_x, int mi_y,
        int frameWidth, int frameHeight)
        //MV32 *scaled_mv, int *subpel_x_mv, int *subpel_y_mv)
    {
        //struct macroblockd_plane *const pd = &xd->plane[plane];
        int x = 0;
        int y = 0;

        int pre_x = mi_x;
        int pre_y = mi_y;

        int ssx = plane ? 1 : 0;
        int ssy = plane ? 1 : 0;
        int orig_pos_y = (pre_y + y) << SUBPEL_BITS;
        orig_pos_y += /*mv.row*/mv.mvy * (1 << (1 - ssy));
        int orig_pos_x = (pre_x + x) << SUBPEL_BITS;
        orig_pos_x += /*mv.col*/mv.mvx * (1 << (1 - ssx));

        int y_scale_fp = 16384;
        int pos_y = scale_value(orig_pos_y, y_scale_fp);
        int x_scale_fp = 32768;
        int pos_x = scale_value(orig_pos_x, x_scale_fp);
        pos_x += SCALE_EXTRA_OFF;
        pos_y += SCALE_EXTRA_OFF;

        const int top = -AOM_LEFT_TOP_MARGIN_SCALED(ssy);
        const int left = -AOM_LEFT_TOP_MARGIN_SCALED(ssx);
        const int bottom = (frameHeight + AOM_INTERP_EXTEND) << SCALE_SUBPEL_BITS;
        const int right = (frameWidth + AOM_INTERP_EXTEND) << SCALE_SUBPEL_BITS;
        //pos_y = clamp(pos_y, top, bottom);
        pos_y = Saturate(top, bottom, pos_y);
        //pos_x = clamp(pos_x, left, right);
        pos_x = Saturate(left, right, pos_x);

        subpel_params->subpel_x = pos_x & SCALE_SUBPEL_MASK;
        subpel_params->subpel_y = pos_y & SCALE_SUBPEL_MASK;

        subpel_params->xs = 2048;// sf->x_step_q4;
        subpel_params->ys = 1024;// sf->y_step_q4;

        // Get reference block top left coordinate.
        block->x0 = pos_x >> SCALE_SUBPEL_BITS;
        block->y0 = pos_y >> SCALE_SUBPEL_BITS;

        // Get reference block bottom right coordinate.
        block->x1 = ((pos_x + (bw - 1) * subpel_params->xs) >> SCALE_SUBPEL_BITS) + 1;
        block->y1 = ((pos_y + (bh - 1) * subpel_params->ys) >> SCALE_SUBPEL_BITS) + 1;
#if 0
        MV temp_mv = clamp_mv_to_umv_border_sb(xd, &mv, bw, bh, pd->subsampling_x, pd->subsampling_y);
        *scaled_mv = av1_scale_mv(&temp_mv, (mi_x + x), (mi_y + y), sf);
        scaled_mv->row += SCALE_EXTRA_OFF;
        scaled_mv->col += SCALE_EXTRA_OFF;

        *subpel_x_mv = scaled_mv->col & SCALE_SUBPEL_MASK;
        *subpel_y_mv = scaled_mv->row & SCALE_SUBPEL_MASK;
#endif
    }
#endif