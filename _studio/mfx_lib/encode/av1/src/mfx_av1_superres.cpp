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

#include "cstdlib"
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

int32_t av1_get_upscale_convolve_step(int in_length, int out_length)
{
    return ((in_length << RS_SCALE_SUBPEL_BITS) + out_length / 2) / out_length;
}

static int32_t get_upscale_convolve_x0(int in_length, int out_length,
    int32_t x_step_qn) {
    const int err = out_length * x_step_qn - (in_length << RS_SCALE_SUBPEL_BITS);
    const int32_t x0 =
        (-((out_length - in_length) << (RS_SCALE_SUBPEL_BITS - 1)) +
            out_length / 2) /
        out_length +
        RS_SCALE_EXTRA_OFF - err / 2;
    return (int32_t)((uint32_t)x0 & RS_SCALE_SUBPEL_MASK);
}

#if 0
static void highbd_upscale_normative_rect(
    const uint8_t *const input,
    int height, int width, int in_stride,
    uint8_t *output,
    int height2, int width2, int out_stride,
    int x_step_qn, int x0_qn,
    int pad_left, int pad_right, int bd)
{
    assert(width > 0);
    assert(height > 0);
    assert(width2 > 0);
    assert(height2 > 0);
    assert(height2 == height);

    // Extend the left/right pixels of the tile column if needed
    // (either because we can't sample from other tiles, or because we're at
    // a frame edge).
    // Save the overwritten pixels into tmp_left and tmp_right.
    // Note: Because we pass input-1 to av1_convolve_horiz_rs, we need one extra
    // column of border pixels compared to what we'd naively think.
    const int border_cols = UPSCALE_NORMATIVE_TAPS / 2 + 1;
    const int border_size = border_cols * sizeof(uint16_t);
    uint16_t *tmp_left = NULL;  // Silence spurious "may be used uninitialized" warnings
    uint16_t *tmp_right = NULL;
    uint16_t *const input16 = CONVERT_TO_SHORTPTR(input);
    uint16_t *const in_tl = input16 - border_cols;
    uint16_t *const in_tr = input16 + width;
    if (pad_left) {
        tmp_left = (uint16_t *)aom_malloc(sizeof(*tmp_left) * border_cols * height);
        for (int i = 0; i < height; i++) {
            memcpy(tmp_left + i * border_cols, in_tl + i * in_stride, border_size);
            aom_memset16(in_tl + i * in_stride, input16[i * in_stride], border_cols);
        }
    }
    if (pad_right) {
        tmp_right = (uint16_t *)aom_malloc(sizeof(*tmp_right) * border_cols * height);
        for (int i = 0; i < height; i++) {
            memcpy(tmp_right + i * border_cols, in_tr + i * in_stride, border_size);
            aom_memset16(in_tr + i * in_stride, input16[i * in_stride + width - 1], border_cols);
        }
    }

    av1_highbd_convolve_horiz_rs(CONVERT_TO_SHORTPTR(input - 1), in_stride,
        CONVERT_TO_SHORTPTR(output), out_stride, width2,
        height2, &av1_resize_filter_normative[0][0],
        x0_qn, x_step_qn, bd);

    // Restore the left/right border pixels
    if (pad_left) {
        for (int i = 0; i < height; i++) {
            memcpy(in_tl + i * in_stride, tmp_left + i * border_cols, border_size);
        }
        aom_free(tmp_left);
    }
    if (pad_right) {
        for (int i = 0; i < height; i++) {
            memcpy(in_tr + i * in_stride, tmp_right + i * border_cols, border_size);
        }
        aom_free(tmp_right);
    }
}
#endif

#define UPSCALE_NORMATIVE_TAPS 8
#define RS_SUBPEL_BITS 6
#define RS_SUBPEL_MASK ((1 << RS_SUBPEL_BITS) - 1)
#define FILTER_BITS 7

alignas(32) const int16_t av1_resize_filter_normative[(1 << RS_SUBPEL_BITS)][UPSCALE_NORMATIVE_TAPS] =
{
#if UPSCALE_NORMATIVE_TAPS == 8
  { 0, 0, 0, 128, 0, 0, 0, 0 },        { 0, 0, -1, 128, 2, -1, 0, 0 },
  { 0, 1, -3, 127, 4, -2, 1, 0 },      { 0, 1, -4, 127, 6, -3, 1, 0 },
  { 0, 2, -6, 126, 8, -3, 1, 0 },      { 0, 2, -7, 125, 11, -4, 1, 0 },
  { -1, 2, -8, 125, 13, -5, 2, 0 },    { -1, 3, -9, 124, 15, -6, 2, 0 },
  { -1, 3, -10, 123, 18, -6, 2, -1 },  { -1, 3, -11, 122, 20, -7, 3, -1 },
  { -1, 4, -12, 121, 22, -8, 3, -1 },  { -1, 4, -13, 120, 25, -9, 3, -1 },
  { -1, 4, -14, 118, 28, -9, 3, -1 },  { -1, 4, -15, 117, 30, -10, 4, -1 },
  { -1, 5, -16, 116, 32, -11, 4, -1 }, { -1, 5, -16, 114, 35, -12, 4, -1 },
  { -1, 5, -17, 112, 38, -12, 4, -1 }, { -1, 5, -18, 111, 40, -13, 5, -1 },  // 16
  { -1, 5, -18, 109, 43, -14, 5, -1 }, { -1, 6, -19, 107, 45, -14, 5, -1 },
  { -1, 6, -19, 105, 48, -15, 5, -1 }, { -1, 6, -19, 103, 51, -16, 5, -1 },
  { -1, 6, -20, 101, 53, -16, 6, -1 }, { -1, 6, -20, 99, 56, -17, 6, -1 },
  { -1, 6, -20, 97, 58, -17, 6, -1 },  { -1, 6, -20, 95, 61, -18, 6, -1 },
  { -2, 7, -20, 93, 64, -18, 6, -2 },  { -2, 7, -20, 91, 66, -19, 6, -1 },
  { -2, 7, -20, 88, 69, -19, 6, -1 },  { -2, 7, -20, 86, 71, -19, 6, -1 },
  { -2, 7, -20, 84, 74, -20, 7, -2 },  { -2, 7, -20, 81, 76, -20, 7, -1 },
  { -2, 7, -20, 79, 79, -20, 7, -2 },  { -1, 7, -20, 76, 81, -20, 7, -2 },
  { -2, 7, -20, 74, 84, -20, 7, -2 },  { -1, 6, -19, 71, 86, -20, 7, -2 },
  { -1, 6, -19, 69, 88, -20, 7, -2 },  { -1, 6, -19, 66, 91, -20, 7, -2 },
  { -2, 6, -18, 64, 93, -20, 7, -2 },  { -1, 6, -18, 61, 95, -20, 6, -1 },
  { -1, 6, -17, 58, 97, -20, 6, -1 },  { -1, 6, -17, 56, 99, -20, 6, -1 },
  { -1, 6, -16, 53, 101, -20, 6, -1 }, { -1, 5, -16, 51, 103, -19, 6, -1 },
  { -1, 5, -15, 48, 105, -19, 6, -1 }, { -1, 5, -14, 45, 107, -19, 6, -1 },
  { -1, 5, -14, 43, 109, -18, 5, -1 }, { -1, 5, -13, 40, 111, -18, 5, -1 },
  { -1, 4, -12, 38, 112, -17, 5, -1 }, { -1, 4, -12, 35, 114, -16, 5, -1 }, // 48
  { -1, 4, -11, 32, 116, -16, 5, -1 }, { -1, 4, -10, 30, 117, -15, 4, -1 },
  { -1, 3, -9, 28, 118, -14, 4, -1 },  { -1, 3, -9, 25, 120, -13, 4, -1 },
  { -1, 3, -8, 22, 121, -12, 4, -1 },  { -1, 3, -7, 20, 122, -11, 3, -1 },
  { -1, 2, -6, 18, 123, -10, 3, -1 },  { 0, 2, -6, 15, 124, -9, 3, -1 },
  { 0, 2, -5, 13, 125, -8, 2, -1 },    { 0, 1, -4, 11, 125, -7, 2, 0 },
  { 0, 1, -3, 8, 126, -6, 2, 0 },      { 0, 1, -3, 6, 127, -4, 1, 0 },
  { 0, 1, -2, 4, 127, -3, 1, 0 },      { 0, 0, -1, 2, 128, -1, 0, 0 },
#else
#error "Invalid value of UPSCALE_NORMATIVE_TAPS"
#endif  // UPSCALE_NORMATIVE_TAPS == 8
};

static uint8_t clip_pixel(int val)
{
    return (val > 255) ? uint8_t(255) : (val < 0) ? uint8_t(0) : uint8_t(val);
}

alignas(32) static const uint8_t pix_shuf_tab[32] = { 0, 1, 1, 2, 1, 2, 2, 3, 2, 3, 3, 4, 3, 4, 4, 5,  4, 5, 5, 6, 5, 6, 6, 7, 6, 7, 7, 8, 7, 8, 8, 9 };

void av1_convolve_horiz_rs_2x_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h)
{
    assert((w & 15) == 0);
    const int16_t* filter0 = av1_resize_filter_normative[48];
    const int16_t* filter1 = av1_resize_filter_normative[16];

    const __m256i offset = _mm256_set1_epi16(1 << (FILTER_BITS - 1));
    const __m256i filt_shuf_mask = _mm256_set1_epi32(0x09080100);
    const __m256i pix_shuf_mask = loada_si256(pix_shuf_tab);

    const __m128i filts128 = _mm_packs_epi16(loada_si128(filter0), loada_si128(filter1));
    const __m256i filts = _mm256_setr_m128i(filts128, filts128);                            // f00 f01 f02 f03 f04 f05 f06 f07 f10 f11 f12 f13 f14 f15 f16 f17 *2 times
    const __m256i f01 = _mm256_shuffle_epi8(filts, filt_shuf_mask);                         // f00 f01 f10 f11 *8 times
    const __m256i f23 = _mm256_shuffle_epi8(_mm256_srli_si256(filts, 2), filt_shuf_mask);   // f02 f03 f12 f13 *8 times
    const __m256i f45 = _mm256_shuffle_epi8(_mm256_srli_si256(filts, 4), filt_shuf_mask);   // etc
    const __m256i f67 = _mm256_shuffle_epi8(_mm256_srli_si256(filts, 6), filt_shuf_mask);

    src -= UPSCALE_NORMATIVE_TAPS / 2 - 1;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; x += 16) {

            __m256i in, a, b, c, d;

            in = _mm256_broadcastsi128_si256(loadu_si128(src + (x >> 1)));      // s00 s01 s02 s03 s04 s05 s06 s07 s08 s09 s10 s11 s12 s13 s14 s15 *2 times

            a = _mm256_shuffle_epi8(in, pix_shuf_mask);                         // s00 s01 s01 s02 s01 s02 s02 s03 s02 s03 and etc
            b = _mm256_shuffle_epi8(_mm256_srli_si256(in, 2), pix_shuf_mask);   // s02 s03 s03 s04 s03 s04 s04 s05 s04 s05 and etc
            c = _mm256_shuffle_epi8(_mm256_srli_si256(in, 4), pix_shuf_mask);   // s04 s05 s05 s06 s05 s06 s06 s07 s06 s07 and etc
            d = _mm256_shuffle_epi8(_mm256_srli_si256(in, 6), pix_shuf_mask);   // s06 s07 s07 s08 s07 s08 s08 s09 s08 s09 and etc

            a = _mm256_maddubs_epi16(a, f01);
            b = _mm256_maddubs_epi16(b, f23);
            c = _mm256_maddubs_epi16(c, f45);
            d = _mm256_maddubs_epi16(d, f67);

            a = _mm256_add_epi16(a, b);
            c = _mm256_add_epi16(c, d);
            a = _mm256_adds_epi16(a, c);

            a = _mm256_adds_epi16(a, offset);
            a = _mm256_srai_epi16(a, FILTER_BITS);
            a = _mm256_packus_epi16(a, a);
            a = permute4x64(a, 0, 2, 0, 2);
            storea_si128(dst + x, si128(a));
        }
        src += src_stride;
        dst += dst_stride;
    }
}

void av1_convolve_horiz_rs_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const int16_t *x_filters, int x0_qn, int x_step_qn)
{
    src -= UPSCALE_NORMATIVE_TAPS / 2 - 1;
    for (int y = 0; y < h; ++y) {
        int x_qn = x0_qn;
        for (int x = 0; x < w; ++x) {
            const uint8_t *const src_x = &src[x_qn >> RS_SCALE_SUBPEL_BITS];
            const int x_filter_idx = (x_qn & RS_SCALE_SUBPEL_MASK) >> RS_SCALE_EXTRA_BITS;
            assert(x_filter_idx <= RS_SUBPEL_MASK);
            const int16_t *const x_filter = &x_filters[x_filter_idx * UPSCALE_NORMATIVE_TAPS];
            int sum = 0;
            for (int k = 0; k < UPSCALE_NORMATIVE_TAPS; ++k)
                sum += src_x[k] * x_filter[k];
            dst[x] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
            x_qn += x_step_qn;
        }
        src += src_stride;
        dst += dst_stride;
    }
}

void av1_upscale_normative_rows(int32_t miCols, const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int plane, int rows)
{
    const int32_t ssx = (plane > 0);
    const int32_t width = (miCols << MI_SIZE_LOG2) >> ssx;
    const int32_t upscaledWidth = width * 2;

    const int border_cols = UPSCALE_NORMATIVE_TAPS / 2;
    uint8_t *const in_tl = const_cast<uint8_t *>(src - border_cols);
    uint8_t *const in_tr = const_cast<uint8_t *>(src + width);

    // pad left
    for (int i = 0; i < rows; i++) {
        memset(in_tl + i * src_stride, src[i * src_stride], border_cols);
        memset(in_tr + i * src_stride, src[i * src_stride + width - 1], border_cols);
    }

    av1_convolve_horiz_rs_2x_c(src - 1, src_stride, dst, dst_stride, upscaledWidth, rows);

    //const int32_t x_step_qn = av1_get_upscale_convolve_step(width, upscaledWidth);
    //int32_t x0_qn = get_upscale_convolve_x0(width, upscaledWidth, x_step_qn);
    //av1_convolve_horiz_rs_c(src - 1, src_stride, dst, dst_stride, upscaledWidth, rows, &av1_resize_filter_normative[0][0], x0_qn, x_step_qn);
}

const int SUPERRES_SCALE_MASK = (1 << 14) - 1;
const int SUPERRES_EXTRA_BITS = 8;
const int SUPERRES_FILTER_TAPS = 8;
const int SUPERRES_FILTER_OFFSET = 3;

int Round2(int x, int n)
{
    if (n == 0)
        return x;

    return (x + (1 << (n - 1))) >> n;
}


void convertNv12ToUv(uint8_t* src_uv, int pitch_chroma_pix, uint8_t* src_u, uint8_t* src_v, int pitchDst, int width, int height)
{
    const uint8_t *puv = src_uv;
    uint8_t *pu = src_u;
    uint8_t *pv = src_v;

    const __m128i mask = _mm_srli_epi16(_mm_cmpeq_epi8(_mm_setzero_si128(), _mm_setzero_si128()), 8); //_mm_set1_epi16(0xff);
    for (int i = 0; i < height; i++, puv += pitch_chroma_pix, pu += pitchDst, pv += pitchDst) {
        for (int j = 0; j < width; j += 16) {
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

    //for (int y = 0; y < height; y++) {
    //    for (int x = 0; x < width; x++) {
    //        assert(src_u[y*pitchDst + x] == src_uv[y*pitch_chroma_pix + 2 * x]);
    //        assert(src_v[y*pitchDst + x] == src_uv[y*pitch_chroma_pix + 2 * x + 1]);

    //        src_u[y*pitchDst + x] = src_uv[y*pitch_chroma_pix + 2 * x];
    //        src_v[y*pitchDst + x] = src_uv[y*pitch_chroma_pix + 2 * x + 1];
    //    }
    //}
}

void convertUvToNv12(uint8_t *dst_u, uint8_t *dst_v, int pitchSrc, int width, int height, uint8_t *uv, int pitch_dst)
{
    uint8_t *puv = uv;
    const uint8_t *pu = dst_u;
    const uint8_t *pv = dst_v;

    for (int i = 0; i < height; i++, puv += pitch_dst, pu += pitchSrc, pv += pitchSrc) {
        for (int j = 0; j < width; j += 16) {
            __m128i u = loada_si128(pu + j);
            __m128i v = loada_si128(pv + j);
            storea_si128(puv + 2 * j + 0, _mm_unpacklo_epi8(u, v));
            storea_si128(puv + 2 * j + 16, _mm_unpackhi_epi8(u, v));
        }
    }

    //for (int y = 0; y < height; y++) {
    //    for (int x = 0; x < width; x++) {
    //        assert(uv[y*pitch_dst + 2 * x + 0] == dst_u[y*pitchSrc + x]);
    //        assert(uv[y*pitch_dst + 2 * x + 1] == dst_v[y*pitchSrc + x]);
    //        uv[y*pitch_dst + 2*x] = dst_u[y*pitchSrc + x];
    //        uv[y*pitch_dst + 2*x+1] = dst_v[y*pitchSrc + x];
    //    }
    //}
}

namespace AV1Enc {
    void av1_upscale_normative_and_extend_frame(const AV1VideoParam& par, FrameData *src, FrameData *dst)
    {
        av1_upscale_normative_rows(par.miCols, src->y, src->pitch_luma_pix, dst->y, dst->pitch_luma_pix, 0, par.Height);

        const int PAD = 64;
        const int WIDTH_U = par.Width / 2;
        const int HEIGHT_U = par.Height / 2;
        const int PITCH_U = (PAD + WIDTH_U + PAD + 15) & ~15;
        const int sizeOfPlaneU = PAD * (PITCH_U)+HEIGHT_U * PITCH_U + PAD * (PITCH_U);

        std::vector<uint8_t> workBuffer(2 * sizeOfPlaneU);
        uint8_t *src_u = workBuffer.data() + PAD * PITCH_U + PAD;
        uint8_t *src_v = src_u + sizeOfPlaneU;

        const int WIDTH_U2 = par.Width;
        const int HEIGHT_U2 = par.Height / 2;
        const int PITCH_U2 = (PAD + WIDTH_U2 + PAD + 15) & ~15;
        const int sizeOfPlaneU2 = PAD * (PITCH_U2)+HEIGHT_U2 * PITCH_U2 + PAD * (PITCH_U2);

        uint8_t *workBuffer2 = ippsMalloc_8u(2 * sizeOfPlaneU2);
        ThrowIf(workBuffer2 == nullptr, std::bad_alloc());

        uint8_t *dst_u = workBuffer2 + PAD * PITCH_U2 + PAD;
        uint8_t *dst_v = dst_u + sizeOfPlaneU2;

        convertNv12ToUv(src->uv, src->pitch_chroma_pix, src_u, src_v, PITCH_U, WIDTH_U, HEIGHT_U);

        av1_upscale_normative_rows(par.miCols, src_u, PITCH_U, dst_u, PITCH_U2, 1, par.Height >> 1);
        av1_upscale_normative_rows(par.miCols, src_v, PITCH_U, dst_v, PITCH_U2, 1, par.Height >> 1);

        convertUvToNv12(dst_u, dst_v, PITCH_U2, WIDTH_U2, HEIGHT_U2, dst->uv, dst->pitch_chroma_pix);

        ippsFree(workBuffer2);
        //aom_extend_frame_borders(dst, num_planes);
    }
};

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE