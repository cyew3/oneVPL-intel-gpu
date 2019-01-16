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

#include "tmmintrin.h"
#include "assert.h"

#include <cstdint>

namespace AV1PP
{

#if (defined(__GNUC__) && __GNUC__) || defined(__SUNPRO_C)
#define DECLARE_ALIGNED(n, typ, val) typ val __attribute__((aligned(n)))
#elif defined(_MSC_VER)
#define DECLARE_ALIGNED(n, typ, val) __declspec(align(n)) typ val
#else
#warning No alignment directives known for this compiler.
#define DECLARE_ALIGNED(n, typ, val) typ val
#endif


//---------------------------------------------------------
//
//---------------------------------------------------------
typedef void filter8_1dfunction(const uint8_t *src_ptr, int src_pitch,
                                uint8_t *output_ptr, int out_pitch,
                                uint32_t output_height, const int16_t *filter);

#define FUN_CONV_1D(name, step_q4, filter, dir, src_start, avg, opt)         \
  void convolve8_##name##_##opt(                                             \
      const uint8_t *src, int src_stride, uint8_t *dst,                      \
      int dst_stride, const int16_t *filter_x,                               \
      const int16_t *filter_y, int w, int h) {                               \
    filter_x,filter_y;                                                       \
    assert(filter[3] != 128);                                                \
    if (filter[0] | filter[1] | filter[2]) {                                 \
      while (w >= 16) {                                                      \
        vpx_filter_block1d16_##dir##8_##avg##opt(src_start, src_stride, dst, \
                                                 dst_stride, h, filter);     \
        src += 16;                                                           \
        dst += 16;                                                           \
        w -= 16;                                                             \
      }                                                                      \
      if (w == 8) {                                                          \
        vpx_filter_block1d8_##dir##8_##avg##opt(src_start, src_stride, dst,  \
                                                dst_stride, h, filter);      \
      } else if (w == 4) {                                                   \
        vpx_filter_block1d4_##dir##8_##avg##opt(src_start, src_stride, dst,  \
                                                dst_stride, h, filter);      \
      }                                                                      \
    } else {                                                                 \
      while (w >= 16) {                                                      \
        vpx_filter_block1d16_##dir##2_##avg##opt(src, src_stride, dst,       \
                                                 dst_stride, h, filter);     \
        src += 16;                                                           \
        dst += 16;                                                           \
        w -= 16;                                                             \
      }                                                                      \
      if (w == 8) {                                                          \
        vpx_filter_block1d8_##dir##2_##avg##opt(src, src_stride, dst,        \
                                                dst_stride, h, filter);      \
      } else if (w == 4) {                                                   \
        vpx_filter_block1d4_##dir##2_##avg##opt(src, src_stride, dst,        \
                                                dst_stride, h, filter);      \
      }                                                                      \
    }                                                                        \
  }


#define FUN_CONV_2D(avg, opt)                                                 \
void convolve8_##avg##both_##opt(                                             \
      const uint8_t *src, int src_stride, uint8_t *dst,                       \
      int dst_stride, const int16_t *filter_x,                                \
      const int16_t *filter_y, int w, int h) {                                \
    assert(filter_x[3] != 128);                                               \
    assert(filter_y[3] != 128);                                               \
    assert(w <= 64);                                                          \
    assert(h <= 64);                                                          \
    if (filter_x[0] | filter_x[1] | filter_x[2]) {                            \
      DECLARE_ALIGNED(16, uint8_t, fdata2[64 * 71]);                          \
      convolve8_horz_##opt(src - 3 * src_stride, src_stride, fdata2, 64,      \
                                filter_x, filter_y, w, h + 7);                \
      convolve8_##avg##vert_##opt(fdata2 + 3 * 64, 64, dst, dst_stride,       \
                                      filter_x, filter_y, w, h);              \
    } else {                                                                  \
      DECLARE_ALIGNED(16, uint8_t, fdata2[64 * 65]);                          \
      convolve8_horz_##opt(src, src_stride, fdata2, 64, filter_x,             \
                           filter_y, w, h + 1);                               \
      convolve8_##avg##vert_##opt(fdata2, 64, dst, dst_stride, filter_x,      \
                                  filter_y, w, h);                            \
    }                                                                         \
  }
//---------------------------------------------------------
//    SSSE3
//---------------------------------------------------------
// filters only for the 4_h8 convolution
DECLARE_ALIGNED(16, static const uint8_t, filt1_4_h8[16]) = {
  0, 1, 1, 2, 2, 3, 3, 4, 2, 3, 3, 4, 4, 5, 5, 6
};

DECLARE_ALIGNED(16, static const uint8_t, filt2_4_h8[16]) = {
  4, 5, 5, 6, 6, 7, 7, 8, 6, 7, 7, 8, 8, 9, 9, 10
};

// filters for 8_h8 and 16_h8
DECLARE_ALIGNED(16, static const uint8_t, filt1_global[16]) = {
  0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8
};

DECLARE_ALIGNED(16, static const uint8_t, filt2_global[16]) = {
  2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10
};

DECLARE_ALIGNED(16, static const uint8_t, filt3_global[16]) = {
  4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12
};

DECLARE_ALIGNED(16, static const uint8_t, filt4_global[16]) = {
  6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14
};

// These are reused by the avx2 intrinsics.
filter8_1dfunction vpx_filter_block1d8_v8_intrin_ssse3;
filter8_1dfunction vpx_filter_block1d8_h8_intrin_ssse3;
filter8_1dfunction vpx_filter_block1d4_h8_intrin_ssse3;

void vpx_filter_block1d4_h8_intrin_ssse3(
    const uint8_t *src_ptr, int src_pixels_per_line, uint8_t *output_ptr,
    int output_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i firstFilters, secondFilters, shuffle1, shuffle2;
  __m128i srcRegFilt1, srcRegFilt2, srcRegFilt3, srcRegFilt4;
  __m128i addFilterReg64, filtersReg, srcReg, minReg;
  unsigned int i;

  // create a register with 0,64,0,64,0,64,0,64,0,64,0,64,0,64,0,64
  addFilterReg64 = _mm_set1_epi32((int)0x0400040u);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  // converting the 16 bit (short) to  8 bit (byte) and have the same data
  // in both lanes of 128 bit register.
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);

  // duplicate only the first 16 bits in the filter into the first lane
  firstFilters = _mm_shufflelo_epi16(filtersReg, 0);
  // duplicate only the third 16 bit in the filter into the first lane
  secondFilters = _mm_shufflelo_epi16(filtersReg, 0xAAu);
  // duplicate only the seconds 16 bits in the filter into the second lane
  // firstFilters: k0 k1 k0 k1 k0 k1 k0 k1 k2 k3 k2 k3 k2 k3 k2 k3
  firstFilters = _mm_shufflehi_epi16(firstFilters, 0x55u);
  // duplicate only the forth 16 bits in the filter into the second lane
  // secondFilters: k4 k5 k4 k5 k4 k5 k4 k5 k6 k7 k6 k7 k6 k7 k6 k7
  secondFilters = _mm_shufflehi_epi16(secondFilters, 0xFFu);

  // loading the local filters
  shuffle1 = _mm_load_si128((__m128i const *)filt1_4_h8);
  shuffle2 = _mm_load_si128((__m128i const *)filt2_4_h8);

  for (i = 0; i < output_height; i++) {
    srcReg = _mm_loadu_si128((const __m128i *)(src_ptr - 3));

    // filter the source buffer
    srcRegFilt1 = _mm_shuffle_epi8(srcReg, shuffle1);
    srcRegFilt2 = _mm_shuffle_epi8(srcReg, shuffle2);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt1 = _mm_maddubs_epi16(srcRegFilt1, firstFilters);
    srcRegFilt2 = _mm_maddubs_epi16(srcRegFilt2, secondFilters);

    // extract the higher half of the lane
    srcRegFilt3 = _mm_srli_si128(srcRegFilt1, 8);
    srcRegFilt4 = _mm_srli_si128(srcRegFilt2, 8);

    minReg = _mm_min_epi16(srcRegFilt3, srcRegFilt2);

    // add and saturate all the results together
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, srcRegFilt4);
    srcRegFilt3 = _mm_max_epi16(srcRegFilt3, srcRegFilt2);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, minReg);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, srcRegFilt3);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, addFilterReg64);

    // shift by 7 bit each 16 bits
    srcRegFilt1 = _mm_srai_epi16(srcRegFilt1, 7);

    // shrink to 8 bit each 16 bits
    srcRegFilt1 = _mm_packus_epi16(srcRegFilt1, srcRegFilt1);
    src_ptr += src_pixels_per_line;

    // save only 4 bytes
    *((int *)&output_ptr[0]) = _mm_cvtsi128_si32(srcRegFilt1);

    output_ptr += output_pitch;
  }
}

void vpx_filter_block1d8_h8_intrin_ssse3(
    const uint8_t *src_ptr, int src_pixels_per_line, uint8_t *output_ptr,
    int output_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i firstFilters, secondFilters, thirdFilters, forthFilters, srcReg;
  __m128i filt1Reg, filt2Reg, filt3Reg, filt4Reg;
  __m128i srcRegFilt1, srcRegFilt2, srcRegFilt3, srcRegFilt4;
  __m128i addFilterReg64, filtersReg, minReg;
  unsigned int i;

  // create a register with 0,64,0,64,0,64,0,64,0,64,0,64,0,64,0,64
  addFilterReg64 = _mm_set1_epi32((int)0x0400040u);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  // converting the 16 bit (short) to  8 bit (byte) and have the same data
  // in both lanes of 128 bit register.
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);

  // duplicate only the first 16 bits (first and second byte)
  // across 128 bit register
  firstFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x100u));
  // duplicate only the second 16 bits (third and forth byte)
  // across 128 bit register
  secondFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x302u));
  // duplicate only the third 16 bits (fifth and sixth byte)
  // across 128 bit register
  thirdFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x504u));
  // duplicate only the forth 16 bits (seventh and eighth byte)
  // across 128 bit register
  forthFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x706u));

  filt1Reg = _mm_load_si128((__m128i const *)filt1_global);
  filt2Reg = _mm_load_si128((__m128i const *)filt2_global);
  filt3Reg = _mm_load_si128((__m128i const *)filt3_global);
  filt4Reg = _mm_load_si128((__m128i const *)filt4_global);

  for (i = 0; i < output_height; i++) {
    srcReg = _mm_loadu_si128((const __m128i *)(src_ptr - 3));

    // filter the source buffer
    srcRegFilt1 = _mm_shuffle_epi8(srcReg, filt1Reg);
    srcRegFilt2 = _mm_shuffle_epi8(srcReg, filt2Reg);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt1 = _mm_maddubs_epi16(srcRegFilt1, firstFilters);
    srcRegFilt2 = _mm_maddubs_epi16(srcRegFilt2, secondFilters);

    // filter the source buffer
    srcRegFilt3 = _mm_shuffle_epi8(srcReg, filt3Reg);
    srcRegFilt4 = _mm_shuffle_epi8(srcReg, filt4Reg);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt3 = _mm_maddubs_epi16(srcRegFilt3, thirdFilters);
    srcRegFilt4 = _mm_maddubs_epi16(srcRegFilt4, forthFilters);

    // add and saturate all the results together
    minReg = _mm_min_epi16(srcRegFilt2, srcRegFilt3);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, srcRegFilt4);

    srcRegFilt2 = _mm_max_epi16(srcRegFilt2, srcRegFilt3);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, minReg);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, srcRegFilt2);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, addFilterReg64);

    // shift by 7 bit each 16 bits
    srcRegFilt1 = _mm_srai_epi16(srcRegFilt1, 7);

    // shrink to 8 bit each 16 bits
    srcRegFilt1 = _mm_packus_epi16(srcRegFilt1, srcRegFilt1);

    src_ptr += src_pixels_per_line;

    // save only 8 bytes
    _mm_storel_epi64((__m128i *)&output_ptr[0], srcRegFilt1);

    output_ptr += output_pitch;
  }
}

void vpx_filter_block1d8_v8_intrin_ssse3(
    const uint8_t *src_ptr, int src_pitch, uint8_t *output_ptr,
    int out_pitch, uint32_t output_height, const int16_t *filter) {
  __m128i addFilterReg64, filtersReg, minReg;
  __m128i firstFilters, secondFilters, thirdFilters, forthFilters;
  __m128i srcRegFilt1, srcRegFilt2, srcRegFilt3, srcRegFilt5;
  __m128i srcReg1, srcReg2, srcReg3, srcReg4, srcReg5, srcReg6, srcReg7;
  __m128i srcReg8;
  unsigned int i;

  // create a register with 0,64,0,64,0,64,0,64,0,64,0,64,0,64,0,64
  addFilterReg64 = _mm_set1_epi32((int)0x0400040u);
  filtersReg = _mm_loadu_si128((const __m128i *)filter);
  // converting the 16 bit (short) to  8 bit (byte) and have the same data
  // in both lanes of 128 bit register.
  filtersReg = _mm_packs_epi16(filtersReg, filtersReg);

  // duplicate only the first 16 bits in the filter
  firstFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x100u));
  // duplicate only the second 16 bits in the filter
  secondFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x302u));
  // duplicate only the third 16 bits in the filter
  thirdFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x504u));
  // duplicate only the forth 16 bits in the filter
  forthFilters = _mm_shuffle_epi8(filtersReg, _mm_set1_epi16(0x706u));

  // load the first 7 rows of 8 bytes
  srcReg1 = _mm_loadl_epi64((const __m128i *)src_ptr);
  srcReg2 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch));
  srcReg3 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 2));
  srcReg4 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 3));
  srcReg5 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 4));
  srcReg6 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 5));
  srcReg7 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 6));

  for (i = 0; i < output_height; i++) {
    // load the last 8 bytes
    srcReg8 = _mm_loadl_epi64((const __m128i *)(src_ptr + src_pitch * 7));

    // merge the result together
    srcRegFilt1 = _mm_unpacklo_epi8(srcReg1, srcReg2);
    srcRegFilt3 = _mm_unpacklo_epi8(srcReg3, srcReg4);

    // merge the result together
    srcRegFilt2 = _mm_unpacklo_epi8(srcReg5, srcReg6);
    srcRegFilt5 = _mm_unpacklo_epi8(srcReg7, srcReg8);

    // multiply 2 adjacent elements with the filter and add the result
    srcRegFilt1 = _mm_maddubs_epi16(srcRegFilt1, firstFilters);
    srcRegFilt3 = _mm_maddubs_epi16(srcRegFilt3, secondFilters);
    srcRegFilt2 = _mm_maddubs_epi16(srcRegFilt2, thirdFilters);
    srcRegFilt5 = _mm_maddubs_epi16(srcRegFilt5, forthFilters);

    // add and saturate the results together
    minReg = _mm_min_epi16(srcRegFilt2, srcRegFilt3);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, srcRegFilt5);
    srcRegFilt2 = _mm_max_epi16(srcRegFilt2, srcRegFilt3);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, minReg);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, srcRegFilt2);
    srcRegFilt1 = _mm_adds_epi16(srcRegFilt1, addFilterReg64);

    // shift by 7 bit each 16 bit
    srcRegFilt1 = _mm_srai_epi16(srcRegFilt1, 7);

    // shrink to 8 bit each 16 bits
    srcRegFilt1 = _mm_packus_epi16(srcRegFilt1, srcRegFilt1);

    src_ptr += src_pitch;

    // shift down a row
    srcReg1 = srcReg2;
    srcReg2 = srcReg3;
    srcReg3 = srcReg4;
    srcReg4 = srcReg5;
    srcReg5 = srcReg6;
    srcReg6 = srcReg7;
    srcReg7 = srcReg8;

    // save only 8 bytes convolve result
    _mm_storel_epi64((__m128i *)&output_ptr[0], srcRegFilt1);

    output_ptr += out_pitch;
  }
}


//---------------------------------------------------------
//
//---------------------------------------------------------
extern "C" filter8_1dfunction vpx_filter_block1d16_v8_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d16_h8_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d8_v8_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d8_h8_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d4_v8_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d4_h8_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d16_v8_avg_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d16_h8_avg_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d8_v8_avg_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d8_h8_avg_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d4_v8_avg_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d4_h8_avg_ssse3;

extern "C" filter8_1dfunction vpx_filter_block1d16_v2_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d16_h2_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d8_v2_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d8_h2_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d4_v2_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d4_h2_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d16_v2_avg_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d16_h2_avg_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d8_v2_avg_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d8_h2_avg_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d4_v2_avg_ssse3;
extern "C" filter8_1dfunction vpx_filter_block1d4_h2_avg_ssse3;

FUN_CONV_1D(horz, x_step_q4, filter_x, h, src, , ssse3);
FUN_CONV_1D(vert, y_step_q4, filter_y, v, src - src_stride * 3, , ssse3);
FUN_CONV_1D(avg_horz, x_step_q4, filter_x, h, src, avg_, ssse3);
FUN_CONV_1D(avg_vert, y_step_q4, filter_y, v, src - src_stride * 3, avg_, ssse3);
FUN_CONV_2D(, ssse3);
FUN_CONV_2D(avg_, ssse3);

extern "C" void vpx_convolve_copy_sse2(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
extern "C" void vpx_convolve_avg_sse2(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

    template <int w, int horz, int vert, int avg> void interp_ssse3(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h) {
        if (horz && vert) (avg) ? convolve8_avg_both_ssse3(src, pitchSrc, dst, pitchDst, fx, fy, w, h)
                                : convolve8_both_ssse3(src, pitchSrc, dst, pitchDst, fx, fy, w, h);
        else if (horz) (avg) ? convolve8_avg_horz_ssse3(src, pitchSrc, dst, pitchDst, fx, fy, w, h)
                             : convolve8_horz_ssse3(src, pitchSrc, dst, pitchDst, fx, fy, w, h);
        else if (vert) (avg) ? convolve8_avg_vert_ssse3(src, pitchSrc, dst, pitchDst, fx, fy, w, h)
                             : convolve8_vert_ssse3(src, pitchSrc, dst, pitchDst, fx, fy, w, h);
        else (avg) ? vpx_convolve_avg_sse2(src, pitchSrc, dst, pitchDst, fx, 16, fy, 16, w, h)
                   : vpx_convolve_copy_sse2(src, pitchSrc, dst, pitchDst, fx, 16, fy, 16, w, h);
    }
    template void interp_ssse3<4, 0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<4, 0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<4, 0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<4, 0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<4, 1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<4, 1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<4, 1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<4, 1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<8, 0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<8, 0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<8, 0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<8, 0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<8, 1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<8, 1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<8, 1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<8, 1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<16,0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<16,0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<16,0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<16,0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<16,1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<16,1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<16,1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<16,1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<32,0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<32,0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<32,0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<32,0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<32,1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<32,1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<32,1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<32,1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<64,0, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<64,0, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<64,0, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<64,0, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<64,1, 0, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<64,1, 0, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<64,1, 1, 0>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);
    template void interp_ssse3<64,1, 1, 1>(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int h);

}; // namespace AV1PP
