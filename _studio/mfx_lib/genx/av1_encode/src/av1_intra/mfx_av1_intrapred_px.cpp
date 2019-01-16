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

#include "stdio.h"
#include "assert.h"
#include "string.h"
#include "mfx_av1_get_intra_pred_pels.h"

typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int uint32_t;

namespace VP9PP
{
    #define Saturate(min_val, max_val, val) IPP_MAX((min_val), IPP_MIN((max_val), (val)))
    /* Shift down with rounding */
    #define ROUND_POWER_OF_TWO(value, n) (((value) + (1 << ((n) - 1))) >> (n))

    #define AVG3(a, b, c) (((a) + 2 * (b) + (c) + 2) >> 2)
    #define AVG2(a, b) (((a) + (b) + 1) >> 1)
    #define DST(x, y) dst[(x) + (y)*pitch]
    #define DST0(x, y) dst[2*(x) + (y)*pitch]
    #define DST1(x, y) dst[2*(x) + 1 + (y)*pitch]

    const uint8_t mode_to_angle_map[] = {0, 90, 180, 45, 135, 113, 157, 203, 67, 0, 0, 0, 0};

    namespace details {
        static inline uint8_t clip_pixel(int val) {
            return (val > 255) ? 255 : (val < 0) ? 0 : val;
        }

    }; // namespace details

    template <int txSize, int leftAvail, int topAvail> void predict_intra_dc_av1_px(const uint8_t *topPel, const uint8_t *leftPel, uint8_t *dst, int pitch, int, int, int)
    {
        const int width = 4 << txSize;
        const int log2Width = txSize + 2;
        const uint8_t *above = topPel;
        const uint8_t *left  = leftPel;

        int sum = 0;

        if (leftAvail && topAvail) {
            for (int i = 0; i < width; i++)
                sum += above[i] + left[i];
            sum = (sum + width) >> (log2Width + 1);
        } else if (leftAvail) {
            for (int i = 0; i < width; i++)
                sum += left[i];
            sum = (sum + (1 << (log2Width - 1))) >> log2Width;
        } else if (topAvail) {
            for (int i = 0; i < width; i++)
                sum += above[i];
            sum = (sum + (1 << (log2Width - 1))) >> log2Width;
        } else {
            sum = 128;
        }

        for (int r = 0; r < width; r++) {
            memset(dst, sum, width);
            dst += pitch;
        }
    }
    template void predict_intra_dc_av1_px<0,0,0>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<0,0,1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<0,1,0>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<0,1,1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<1,0,0>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<1,0,1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<1,1,0>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<1,1,1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<2,0,0>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<2,0,1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<2,1,0>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<2,1,1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<3,0,0>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<3,0,1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<3,1,0>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_dc_av1_px<3,1,1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);

    namespace details {
        template <int txSize> void predict_intra_hor_px(const uint8_t *refPel, uint8_t *dst, int pitch)
        {
            const int width = 4 << txSize;
            const uint8_t* left  = refPel + 1 + 2*width;

            for (int r = 0; r < width; r++) {
                memset(dst, left[r], width);
                dst += pitch;
            }
        }
        template void predict_intra_hor_px<0>(const uint8_t*,uint8_t*,int);
        template void predict_intra_hor_px<1>(const uint8_t*,uint8_t*,int);
        template void predict_intra_hor_px<2>(const uint8_t*,uint8_t*,int);
        template void predict_intra_hor_px<3>(const uint8_t*,uint8_t*,int);

        template <int txSize> void predict_intra_ver_px(const uint8_t *refPel, uint8_t *dst, int pitch)
        {
            const int width = 4 << txSize;
            const uint8_t* above = refPel + 1;

            for (int r = 0; r < width; r++) {
                memcpy(dst, above, width);
                dst += pitch;
            }
        }
        template void predict_intra_ver_px<0>(const uint8_t*,uint8_t*,int);
        template void predict_intra_ver_px<1>(const uint8_t*,uint8_t*,int);
        template void predict_intra_ver_px<2>(const uint8_t*,uint8_t*,int);
        template void predict_intra_ver_px<3>(const uint8_t*,uint8_t*,int);

        template <int txSize> void predict_intra_d45_px(const uint8_t *refPel, uint8_t *dst, int pitch)
        {
            const int width = 4 << txSize;
            const uint8_t* above = refPel + 1;
            const uint8_t above_right = above[width];

            if (width == 4) {
                const int A = above[0];
                const int B = above[1];
                const int C = above[2];
                const int D = above[3];
                const int E = above[4];
                const int F = above[5];
                const int G = above[6];
                const int H = above[7];

                DST(0, 0) = AVG3(A, B, C);
                DST(1, 0) = DST(0, 1) = AVG3(B, C, D);
                DST(2, 0) = DST(1, 1) = DST(0, 2) = AVG3(C, D, E);
                DST(3, 0) = DST(2, 1) = DST(1, 2) = DST(0, 3) = AVG3(D, E, F);
                DST(3, 1) = DST(2, 2) = DST(1, 3) = AVG3(E, F, G);
                DST(3, 2) = DST(2, 3) = AVG3(F, G, H);
                DST(3, 3) = H;  // differs from vp8
            } else {
                const uint8_t *const dst_row0 = dst;
                int x, size;

                for (x = 0; x < width - 1; ++x) {
                    dst[x] = AVG3(above[x], above[x + 1], above[x + 2]);
                }
                dst[width - 1] = above_right;
                dst += pitch;
                for (x = 1, size = width - 2; x < width; ++x, --size) {
                    memcpy(dst, dst_row0 + x, size);
                    memset(dst + size, above_right, x + 1);
                    dst += pitch;
                }
            }
        }

        template void predict_intra_d45_px<0>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d45_px<1>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d45_px<2>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d45_px<3>(const uint8_t*,uint8_t*,int);

        template <int txSize> void predict_intra_hor_av1_px(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch)
        {
            topPels;
            const int width = 4 << txSize;
            for (int r = 0; r < width; r++, dst += pitch)
                memset(dst, leftPels[r], width);
        }
        template void predict_intra_hor_av1_px<0>(const uint8_t*,const uint8_t*,uint8_t*,int);
        template void predict_intra_hor_av1_px<1>(const uint8_t*,const uint8_t*,uint8_t*,int);
        template void predict_intra_hor_av1_px<2>(const uint8_t*,const uint8_t*,uint8_t*,int);
        template void predict_intra_hor_av1_px<3>(const uint8_t*,const uint8_t*,uint8_t*,int);

        template <int txSize> void predict_intra_ver_av1_px(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch)
        {
            leftPels;
            const int width = 4 << txSize;
            for (int r = 0; r < width; r++, dst += pitch)
                memcpy(dst, topPels, width);
        }
        template void predict_intra_ver_av1_px<0>(const uint8_t*,const uint8_t*,uint8_t*,int);
        template void predict_intra_ver_av1_px<1>(const uint8_t*,const uint8_t*,uint8_t*,int);
        template void predict_intra_ver_av1_px<2>(const uint8_t*,const uint8_t*,uint8_t*,int);
        template void predict_intra_ver_av1_px<3>(const uint8_t*,const uint8_t*,uint8_t*,int);

        template <int txSize> void predict_intra_d45_av1_px(const uint8_t *refPel, uint8_t *dst, int pitch)
        {
            const int width = 4 << txSize;
            const uint8_t* above = refPel + 1;
            //const uint8_t above_right = above[width];

            if (width == 4) {
                const int A = above[0];
                const int B = above[1];
                const int C = above[2];
                const int D = above[3];
                const int E = above[4];
                const int F = above[5];
                const int G = above[6];
                const int H = above[7];

                DST(0, 0) = AVG3(A, B, C);
                DST(1, 0) = DST(0, 1) = AVG3(B, C, D);
                DST(2, 0) = DST(1, 1) = DST(0, 2) = AVG3(C, D, E);
                DST(3, 0) = DST(2, 1) = DST(1, 2) = DST(0, 3) = AVG3(D, E, F);
                DST(3, 1) = DST(2, 2) = DST(1, 3) = AVG3(E, F, G);
                DST(3, 2) = DST(2, 3) = AVG3(F, G, H);
                DST(3, 3) = AVG3(G, H, H);
            } else {
                //const uint8_t *const dst_row0 = dst;
                int r,c;

                for (r = 0; r < width; ++r) {
                    for (c = 0; c < width; ++c) {
                        dst[c] = AVG3(above[r + c], above[r + c + 1], above[r + c + 1 + (r + c + 2 < width * 2)]);
                    }
                    dst += pitch;
                }
            }
        }

        template void predict_intra_d45_av1_px<0>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d45_av1_px<1>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d45_av1_px<2>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d45_av1_px<3>(const uint8_t*,uint8_t*,int);

        template <int txSize> void predict_intra_z1_av1_px(const uint8_t *topPels, uint8_t *dst, int pitch, int upTop, int dx)
        {
            const int width = 4 << txSize;
            const uint8_t *above = topPels;
            int r, c, x, base, shift, val;

            //const int dx = 256;
            //const int dy = 1;
            const int bs = width;
            const int stride = pitch;
            const int max_base_x = (2 * bs - 1) << upTop;
            const int frac_bits = 6 - upTop;
            const int base_inc = 1 << upTop;

            //(void)left;
            assert(dx > 0);

            x = dx;
            for (r = 0; r < bs; ++r, dst += stride, x += dx) {
                base = x >> frac_bits;
                shift = ((x << upTop) & 0x3F) >> 1;

                if (base >= max_base_x) {
                    int i;
                    for (i = r; i < bs; ++i) {
                        memset(dst, above[max_base_x], bs * sizeof(dst[0]));
                        dst += stride;
                    }
                    return;
                }

                for (c = 0; c < bs; ++c, base += base_inc) {
                    if (base < max_base_x) {
                        val = above[base] * (32 - shift) + above[base + 1] * shift;
                        val = ROUND_POWER_OF_TWO(val, 5);
                        dst[c] = clip_pixel(val);
                    }
                    else {
                        dst[c] = above[max_base_x];
                    }
                }
            }
        }

        template void predict_intra_z1_av1_px<0>(const uint8_t*,uint8_t*,int,int,int);
        template void predict_intra_z1_av1_px<1>(const uint8_t*,uint8_t*,int,int,int);
        template void predict_intra_z1_av1_px<2>(const uint8_t*,uint8_t*,int,int,int);
        template void predict_intra_z1_av1_px<3>(const uint8_t*,uint8_t*,int,int,int);

        template <int txSize> void predict_intra_d135_px(const uint8_t *refPel, uint8_t *dst, int pitch)
        {
            const int width = 4 << txSize;
            const uint8_t* above = refPel + 1;
            const uint8_t* left  = refPel + 1 + 2*width;

            int i;
            uint8_t border[32 + 32 - 1];  // outer border from bottom-left to top-right

            // dst(width, width - 2)[0], i.e., border starting at bottom-left
            for (i = 0; i < width - 2; ++i) {
                border[i] = AVG3(left[width - 3 - i], left[width - 2 - i], left[width - 1 - i]);
            }
            border[width - 2] = AVG3(above[-1], left[0], left[1]);
            border[width - 1] = AVG3(left[0], above[-1], above[0]);
            border[width - 0] = AVG3(above[-1], above[0], above[1]);
            // dst[0][2, size), i.e., remaining top border ascending
            for (i = 0; i < width - 2; ++i) {
                border[width + 1 + i] = AVG3(above[i], above[i + 1], above[i + 2]);
            }

            for (i = 0; i < width; ++i) {
                memcpy(dst + i * pitch, border + width - 1 - i, width);
            }
        }
        template void predict_intra_d135_px<0>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d135_px<1>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d135_px<2>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d135_px<3>(const uint8_t*,uint8_t*,int);


        template <int txSize> void predict_intra_z2_av1_px(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch,
                                                           int upTop, int upLeft, int dx, int dy)
        {
            const int width = 4 << txSize;
            const uint8_t* above = topPels;
            const uint8_t* left  = leftPels;

            int stride = pitch;
            int bs = width;

            int r, c, x, y, shift1, shift2, val, base1, base2;

            assert(dx > 0);
            assert(dy > 0);

            const int min_base_x = -(1 << upTop);
            const int frac_bits_x = 6 - upTop;
            const int frac_bits_y = 6 - upLeft;
            const int base_inc_x = 1 << upTop;
            x = -dx;
            for (r = 0; r < bs; ++r, x -= dx, dst += stride) {
                base1 = x >> frac_bits_x;
                y = (r << 6) - dy;
                for (c = 0; c < bs; ++c, base1 += base_inc_x, y -= dy) {
                    if (base1 >= min_base_x) {
                        shift1 = ((x << upTop) & 0x3F) >> 1;
                        val = above[base1] * (32 - shift1) + above[base1 + 1] * shift1;
                        val = ROUND_POWER_OF_TWO(val, 5);
                    }
                    else {
                        base2 = y >> frac_bits_y;
                        shift2 = ((y << upLeft) & 0x3F) >> 1;
                        val = left[base2] * (32 - shift2) + left[base2 + 1] * shift2;
                        val = ROUND_POWER_OF_TWO(val, 5);
                    }
                    dst[c] = clip_pixel(val);
                }
            }
        }
        template void predict_intra_z2_av1_px<0>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int,int);
        template void predict_intra_z2_av1_px<1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int,int);
        template void predict_intra_z2_av1_px<2>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int,int);
        template void predict_intra_z2_av1_px<3>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int,int);

        template <int txSize> void predict_intra_d117_px(const uint8_t *refPel, uint8_t *dst, int pitch)
        {
            const int width = 4 << txSize;
            const uint8_t* above = refPel + 1;
            const uint8_t* left  = refPel + 1 + 2*width;
            int r, c;

            // first row
            for (c = 0; c < width; c++)
                dst[c] = AVG2(above[c - 1], above[c]);
            dst += pitch;

            // second row
            dst[0] = AVG3(left[0], above[-1], above[0]);
            for (c = 1; c < width; c++)
                dst[c] = AVG3(above[c - 2], above[c - 1], above[c]);
            dst += pitch;

            // the rest of first col
            dst[0] = AVG3(above[-1], left[0], left[1]);
            for (r = 3; r < width; ++r)
                dst[(r - 2) * pitch] = AVG3(left[r - 3], left[r - 2], left[r - 1]);

            // the rest of the block
            for (r = 2; r < width; ++r) {
                for (c = 1; c < width; c++)
                    dst[c] = dst[-2 * pitch + c - 1];
                dst += pitch;
            }
        }
        template void predict_intra_d117_px<0>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d117_px<1>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d117_px<2>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d117_px<3>(const uint8_t*,uint8_t*,int);

        template <int txSize> void predict_intra_d153_px(const uint8_t *refPel, uint8_t *dst, int pitch)
        {
            const int width = 4 << txSize;
            const uint8_t* above = refPel + 1;
            const uint8_t* left  = refPel + 1 + 2*width;
            int r, c;
            dst[0] = AVG2(above[-1], left[0]);
            for (r = 1; r < width; r++)
                dst[r * pitch] = AVG2(left[r - 1], left[r]);
            dst++;

            dst[0] = AVG3(left[0], above[-1], above[0]);
            dst[pitch] = AVG3(above[-1], left[0], left[1]);
            for (r = 2; r < width; r++)
                dst[r * pitch] = AVG3(left[r - 2], left[r - 1], left[r]);
            dst++;

            for (c = 0; c < width - 2; c++)
                dst[c] = AVG3(above[c - 1], above[c], above[c + 1]);
            dst += pitch;

            for (r = 1; r < width; ++r) {
                for (c = 0; c < width - 2; c++)
                    dst[c] = dst[-pitch + c - 2];
                dst += pitch;
            }
        }
        template void predict_intra_d153_px<0>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d153_px<1>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d153_px<2>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d153_px<3>(const uint8_t*,uint8_t*,int);

        template <int txSize> void predict_intra_d207_px(const uint8_t *refPel, uint8_t *dst, int pitch)
        {
            const int width = 4 << txSize;
            int r, c;
            const uint8_t* left = refPel + 2 * width + 1;
            // first column
            for (r = 0; r < width - 1; ++r)
                dst[r * pitch] = AVG2(left[r], left[r + 1]);
            dst[(width - 1) * pitch] = left[width - 1];
            dst++;

            // second column
            for (r = 0; r < width - 2; ++r)
                dst[r * pitch] = AVG3(left[r], left[r + 1], left[r + 2]);
            dst[(width - 2) * pitch] = AVG3(left[width - 2], left[width - 1], left[width - 1]);
            dst[(width - 1) * pitch] = left[width - 1];
            dst++;

            // rest of last row
            for (c = 0; c < width - 2; ++c)
                dst[(width - 1) * pitch + c] = left[width - 1];

            for (r = width - 2; r >= 0; --r)
                for (c = 0; c < width - 2; ++c)
                    dst[r * pitch + c] = dst[(r + 1) * pitch + c - 2];
        }
        template void predict_intra_d207_px<0>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d207_px<1>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d207_px<2>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d207_px<3>(const uint8_t*,uint8_t*,int);

        template <int txSize> void predict_intra_d207_av1_px(const uint8_t *refPel, uint8_t *dst, int pitch)
        {
            const int width = 4 << txSize;
            const uint8_t* left = refPel + 2 * width + 1;
            const int bs = width;

            for (int r = 0; r < bs; ++r) {
                for (int c = 0; c < bs; ++c) {
                    dst[c] = c & 1
                        ? AVG3(left[(c >> 1) + r], left[(c >> 1) + r + 1], left[(c >> 1) + r + 2])
                        : AVG2(left[(c >> 1) + r], left[(c >> 1) + r + 1]);
                }
                dst += pitch;
            }
        }
        template void predict_intra_d207_av1_px<0>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d207_av1_px<1>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d207_av1_px<2>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d207_av1_px<3>(const uint8_t*,uint8_t*,int);

        template <int txSize> void predict_intra_z3_av1_px(const uint8_t *leftPels, uint8_t *dst, int pitch, int upLeft, int dy)
        {
            const int width = 4 << txSize;
            const uint8_t* left = leftPels;
            const int bs = width;
            const int stride = pitch;
            const int max_base_y = (2 * bs - 1) << upLeft;
            const int frac_bits = 6 - upLeft;
            const int base_inc = 1 << upLeft;

            int r, c, y, base, shift, val;

            assert(dy > 0);

            y = dy;
            for (c = 0; c < bs; ++c, y += dy) {
                base = y >> frac_bits;
                assert(base >= 0);
                shift = ((y << upLeft) & 0x3F) >> 1;

                for (r = 0; r < bs; ++r, base += base_inc) {
                    if (base < max_base_y) {
                        val = left[base] * (32 - shift) + left[base + 1] * shift;
                        val = ROUND_POWER_OF_TWO(val, 5);
                        dst[r * stride + c] = clip_pixel(val);
                    } else {
                        for (; r < bs; ++r) dst[r * stride + c] = left[max_base_y];
                        break;
                    }
                }
            }
        }
        template void predict_intra_z3_av1_px<0>(const uint8_t*,uint8_t*,int,int,int);
        template void predict_intra_z3_av1_px<1>(const uint8_t*,uint8_t*,int,int,int);
        template void predict_intra_z3_av1_px<2>(const uint8_t*,uint8_t*,int,int,int);
        template void predict_intra_z3_av1_px<3>(const uint8_t*,uint8_t*,int,int,int);

        void vpx_d63_predictor_4x4_c(uint8_t *dst, ptrdiff_t pitch, const uint8_t *above, const uint8_t *left)
        {
            const int A = above[0];
            const int B = above[1];
            const int C = above[2];
            const int D = above[3];
            const int E = above[4];
            const int F = above[5];
            const int G = above[6];
            (void)left;
            DST(0, 0) = AVG2(A, B);
            DST(1, 0) = DST(0, 2) = AVG2(B, C);
            DST(2, 0) = DST(1, 2) = AVG2(C, D);
            DST(3, 0) = DST(2, 2) = AVG2(D, E);
            DST(3, 2) = AVG2(E, F);  // differs from vp8

            DST(0, 1) = AVG3(A, B, C);
            DST(1, 1) = DST(0, 3) = AVG3(B, C, D);
            DST(2, 1) = DST(1, 3) = AVG3(C, D, E);
            DST(3, 1) = DST(2, 3) = AVG3(D, E, F);
            DST(3, 3) = AVG3(E, F, G);  // differs from vp8
        }
        template <int txSize> void predict_intra_d63_px(const uint8_t *refPel, uint8_t *dst, int pitch)
        {
            const int width = 4 << txSize;
            int r, c;
            int size;
            const uint8_t* above = refPel + 1;

            if (width == 4) {
                vpx_d63_predictor_4x4_c(dst, pitch, above, NULL) ;
                return;
            } else {
                for (c = 0; c < width; ++c) {
                    dst[c] = AVG2(above[c], above[c + 1]);
                    dst[pitch + c] = AVG3(above[c], above[c + 1], above[c + 2]);
                }
                for (r = 2, size = width - 2; r < width; r += 2, --size) {
                    memcpy(dst + (r + 0) * pitch, dst + (r >> 1), size);
                    memset(dst + (r + 0) * pitch + size, above[width - 1], width - size);
                    memcpy(dst + (r + 1) * pitch, dst + pitch + (r >> 1), size);
                    memset(dst + (r + 1) * pitch + size, above[width - 1], width - size);
                }
            }
        }
        template void predict_intra_d63_px<0>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d63_px<1>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d63_px<2>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d63_px<3>(const uint8_t*,uint8_t*,int);

        void predict_intra_d63_4x4_av1_px(uint8_t *dst, int pitch, const uint8_t *above, const uint8_t *left) {
                const int A = above[0];
                const int B = above[1];
                const int C = above[2];
                const int D = above[3];
                const int E = above[4];
                const int F = above[5];
                const int G = above[6];
                (void)left;
                DST(0, 0) = AVG2(A, B);
                DST(1, 0) = DST(0, 2) = AVG2(B, C);
                DST(2, 0) = DST(1, 2) = AVG2(C, D);
                DST(3, 0) = DST(2, 2) = AVG2(D, E);
                DST(3, 2) = AVG2(E, F);  // differs from vp8

                DST(0, 1) = AVG3(A, B, C);
                DST(1, 1) = DST(0, 3) = AVG3(B, C, D);
                DST(2, 1) = DST(1, 3) = AVG3(C, D, E);
                DST(3, 1) = DST(2, 3) = AVG3(D, E, F);
                DST(3, 3) = AVG3(E, F, G);  // differs from vp8
        }

        template <int txSize> void predict_intra_d63_av1_px(const uint8_t *refPel, uint8_t *dst, int pitch)
        {
            const int width = 4 << txSize;
            const uint8_t* above = refPel + 1;

            /*if (width == 4) {
                predict_intra_d63_4x4_av1_px(dst, pitch, above, NULL) ;
                return;
            } else */{
                int r, c;
                int bs = width;
                for (r = 0; r < bs; ++r) {
                    for (c = 0; c < bs; ++c) {
                        dst[c] = r & 1 ? AVG3(above[(r >> 1) + c], above[(r >> 1) + c + 1],
                            above[(r >> 1) + c + 2])
                            : AVG2(above[(r >> 1) + c], above[(r >> 1) + c + 1]);
                    }
                    dst += pitch;
                }
            }
        }
        template void predict_intra_d63_av1_px<0>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d63_av1_px<1>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d63_av1_px<2>(const uint8_t*,uint8_t*,int);
        template void predict_intra_d63_av1_px<3>(const uint8_t*,uint8_t*,int);

        template <int txSize> void predict_intra_tm_px(const uint8_t *refPel, uint8_t *dst, int pitch)
        {
            const int width = 4 << txSize;
            const uint8_t* above = refPel + 1;
            const uint8_t* left  = refPel + 1 + 2*width;
            int ytop_left = above[-1];

            for (int r = 0; r < width; r++) {
                for (int c = 0; c < width; c++)
                    dst[c] = clip_pixel(left[r] + above[c] - ytop_left);
                dst += pitch;
            }
        }
        template void predict_intra_tm_px<0>(const uint8_t*,uint8_t*,int);
        template void predict_intra_tm_px<1>(const uint8_t*,uint8_t*,int);
        template void predict_intra_tm_px<2>(const uint8_t*,uint8_t*,int);
        template void predict_intra_tm_px<3>(const uint8_t*,uint8_t*,int);

        // Weights are quadratic from '1' to '1 / block_size', scaled by
        // 2^sm_weight_log2_scale.
        const int sm_weight_log2_scale = 8;
        const int MAX_BLOCK_DIM = 32;
        const int NUM_BLOCK_DIMS = 5;
        const uint8_t sm_weight_arrays[NUM_BLOCK_DIMS][MAX_BLOCK_DIM] = {
            // bs = 2
            { 255, 128 },
            // bs = 4
            { 255, 149, 85, 64 },
            // bs = 8
            { 255, 197, 146, 105, 73, 50, 37, 32 },
            // bs = 16
            { 255, 225, 196, 170, 145, 123, 102, 84, 68, 54, 43, 33, 26, 20, 17, 16 },
            // bs = 32
            { 255, 240, 225, 210, 196, 182, 169, 157, 145, 133, 122, 111, 101, 92,  83,  74,
               66,  59,  52,  45,  39,  34,  29,  25,  21,  17,  14,  12,  10,  9,   8,   8 },
        };


        template <int txSize> void predict_intra_smooth_av1_px(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch)
        {
            const int width = 4 << txSize;
            const uint8_t* above = topPels;
            const uint8_t* left  = leftPels;
            const uint8_t below_pred = left[width - 1];   // estimated by bottom-left pixel
            const uint8_t right_pred = above[width - 1];  // estimated by top-right pixel
            const int arr_index = txSize + 1;
            assert(arr_index >= 0);
            assert(arr_index < details::NUM_BLOCK_DIMS);
            const uint8_t *const sm_weights = details::sm_weight_arrays[arr_index];
            // scale = 2 * 2^sm_weight_log2_scale
            const int log2_scale = 1 + details::sm_weight_log2_scale;
            const uint16_t scale = (1 << details::sm_weight_log2_scale);
            for (int r = 0; r < width; ++r) {
                for (int c = 0; c < width; ++c) {
                    const uint8_t pixels[] = { above[c], below_pred, left[r], right_pred };
                    const uint8_t weights[] = { sm_weights[r], (uint8_t)(scale - sm_weights[r]), sm_weights[c], (uint8_t)(scale - sm_weights[c]) };
                    uint32_t this_pred = 0;
                    assert(scale >= sm_weights[r] && scale >= sm_weights[c]);
                    for (int i = 0; i < 4; ++i)
                        this_pred += weights[i] * pixels[i];
                    dst[c] = details::clip_pixel(ROUND_POWER_OF_TWO(this_pred, log2_scale));
                }
                dst += pitch;
            }
        }
        template void predict_intra_smooth_av1_px<0>(const uint8_t*,const uint8_t*,uint8_t*,int);
        template void predict_intra_smooth_av1_px<1>(const uint8_t*,const uint8_t*,uint8_t*,int);
        template void predict_intra_smooth_av1_px<2>(const uint8_t*,const uint8_t*,uint8_t*,int);
        template void predict_intra_smooth_av1_px<3>(const uint8_t*,const uint8_t*,uint8_t*,int);

        // Some basic checks on weights for smooth predictor.
#define sm_weights_sanity_checks(weights_w, weights_h, weights_scale, \
    pred_scale)                          \
    assert(weights_w[0] < weights_scale);                               \
    assert(weights_h[0] < weights_scale);                               \
    assert(weights_scale - weights_w[bw - 1] < weights_scale);          \
    assert(weights_scale - weights_h[bh - 1] < weights_scale);          \
    assert(pred_scale < 31)  // ensures no overflow when calculating predictor.

        static const uint8_t sm_weight_vector[2 * MAX_BLOCK_DIM] = {
            // Unused, because we always offset by bs, which is at least 2.
            0, 0,
            // bs = 2
            255, 128,
            // bs = 4
            255, 149, 85, 64,
            // bs = 8
            255, 197, 146, 105, 73, 50, 37, 32,
            // bs = 16
            255, 225, 196, 170, 145, 123, 102, 84, 68, 54, 43, 33, 26, 20, 17, 16,
            // bs = 32
            255, 240, 225, 210, 196, 182, 169, 157, 145, 133, 122, 111, 101, 92, 83, 74,
            66, 59, 52, 45, 39, 34, 29, 25, 21, 17, 14, 12, 10, 9, 8, 8,
        };

        template <int txSize> void predict_intra_smooth_v_av1_px(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch)
        {
            const int width = 4 << txSize;
            const uint8_t *above = topPels;
            const uint8_t *left  = leftPels;
            int bw = width;
            int bh = width;
            const uint8_t below_pred = left[bh - 1];  // estimated by bottom-left pixel
            const uint8_t *const sm_weights = sm_weight_vector + bh;
            // scale = 2^sm_weight_log2_scale
            const int log2_scale = sm_weight_log2_scale;
            const uint16_t scale = (1 << sm_weight_log2_scale);
            sm_weights_sanity_checks(sm_weights, sm_weights, scale, log2_scale + sizeof(*dst));

            int r;
            for (r = 0; r < bh; r++) {
                int c;
                for (c = 0; c < bw; ++c) {
                    const uint8_t pixels[] = { above[c], below_pred };
                    const uint8_t weights[] = { sm_weights[r], (uint8_t)(scale - sm_weights[r]) };
                    uint32_t this_pred = 0;
                    assert(scale >= sm_weights[r]);
                    int i;
                    for (i = 0; i < 2; ++i) {
                        this_pred += weights[i] * pixels[i];
                    }
                    dst[c] = details::clip_pixel(ROUND_POWER_OF_TWO(this_pred, log2_scale));
                }
                dst += pitch;
            }
        }

        template void predict_intra_smooth_v_av1_px<0>(const uint8_t*,const uint8_t*,uint8_t*,int);
        template void predict_intra_smooth_v_av1_px<1>(const uint8_t*,const uint8_t*,uint8_t*,int);
        template void predict_intra_smooth_v_av1_px<2>(const uint8_t*,const uint8_t*,uint8_t*,int);
        template void predict_intra_smooth_v_av1_px<3>(const uint8_t*,const uint8_t*,uint8_t*,int);


        template <int txSize> void predict_intra_smooth_h_av1_px(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch)
        {
            const int width = 4 << txSize;
            const uint8_t *above = topPels;
            const uint8_t *left  = leftPels;
            int bw = width;
            int bh = width;
            const uint8_t right_pred = above[bw - 1];  // estimated by top-right pixel
            const uint8_t *const sm_weights = sm_weight_vector + bw;
            // scale = 2^sm_weight_log2_scale
            const int log2_scale = sm_weight_log2_scale;
            const uint16_t scale = (1 << sm_weight_log2_scale);
            sm_weights_sanity_checks(sm_weights, sm_weights, scale, log2_scale + sizeof(*dst));

            int r;
            for (r = 0; r < bh; r++) {
                int c;
                for (c = 0; c < bw; ++c) {
                    const uint8_t pixels[] = { left[r], right_pred };
                    const uint8_t weights[] = { sm_weights[c], (uint8_t)(scale - sm_weights[c]) };
                    uint32_t this_pred = 0;
                    assert(scale >= sm_weights[c]);
                    int i;
                    for (i = 0; i < 2; ++i) {
                        this_pred += weights[i] * pixels[i];
                    }
                    dst[c] = details::clip_pixel(ROUND_POWER_OF_TWO(this_pred, log2_scale));
                }
                dst += pitch;
            }
        }

        template void predict_intra_smooth_h_av1_px<0>(const uint8_t*,const uint8_t*,uint8_t*,int);
        template void predict_intra_smooth_h_av1_px<1>(const uint8_t*,const uint8_t*,uint8_t*,int);
        template void predict_intra_smooth_h_av1_px<2>(const uint8_t*,const uint8_t*,uint8_t*,int);
        template void predict_intra_smooth_h_av1_px<3>(const uint8_t*,const uint8_t*,uint8_t*,int);

        inline int abs_diff(int a, int b) { return (a > b) ? a - b : b - a; }

        uint8_t predict_intra_paeth_single_pel(uint8_t left, uint8_t above, uint8_t above_left) {
            const int base = above + left - above_left;
            const int diff_left = abs_diff(base, left);
            const int diff_above = abs_diff(base, above);
            const int diff_above_left = abs_diff(base, above_left);
            // Return nearest to base of left, top and top_left.
            return (diff_left <= diff_above && diff_left <= diff_above_left)
                ? left
                : (diff_above <= diff_above_left)
                    ? above
                    : above_left;
        }

        template <int txSize> void predict_intra_paeth_av1_px(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch)
        {
            const int width = 4 << txSize;
            const uint8_t *above = topPels;
            const uint8_t *left  = leftPels;
            const int above_left = topPels[-1];

            for (int r = 0; r < width; r++, dst += pitch)
                for (int c = 0; c < width; c++)
                    dst[c] = details::predict_intra_paeth_single_pel(left[r], above[c], above_left);
        }
        template void predict_intra_paeth_av1_px<0>(const uint8_t*,const uint8_t*,uint8_t*,int);
        template void predict_intra_paeth_av1_px<1>(const uint8_t*,const uint8_t*,uint8_t*,int);
        template void predict_intra_paeth_av1_px<2>(const uint8_t*,const uint8_t*,uint8_t*,int);
        template void predict_intra_paeth_av1_px<3>(const uint8_t*,const uint8_t*,uint8_t*,int);
    }; // namespace details

    template <int size, int mode> void predict_intra_av1_px(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int delta, int upTop, int upLeft) {
        if      (mode ==  9) details::predict_intra_smooth_av1_px  <size>(topPels, leftPels, dst, pitch);
        else if (mode == 10) details::predict_intra_smooth_v_av1_px<size>(topPels, leftPels, dst, pitch);
        else if (mode == 11) details::predict_intra_smooth_h_av1_px<size>(topPels, leftPels, dst, pitch);
        else if (mode == 12) details::predict_intra_paeth_av1_px   <size>(topPels, leftPels, dst, pitch);
        else {
            assert(mode >= 1 && mode <= 8);
            const int angle = mode_to_angle_map[mode] + delta * 3;
            const int dx = get_dx(angle);
            const int dy = get_dy(angle);

            if      (angle > 0 && angle < 90)    details::predict_intra_z1_av1_px<size>(topPels, dst, pitch, upTop, dx);
            else if (angle > 90 && angle < 180)  details::predict_intra_z2_av1_px<size>(topPels, leftPels, dst, pitch, upTop, upLeft, dx, dy);
            else if (angle > 180 && angle < 270) details::predict_intra_z3_av1_px<size>(leftPels, dst, pitch, upLeft, dy);
            else if (angle == 90)                details::predict_intra_ver_av1_px<size>(topPels, leftPels, dst, pitch);
            else if (angle == 180)               details::predict_intra_hor_av1_px<size>(topPels, leftPels, dst, pitch);
        }
    }

    const int16_t dr_intra_derivative[90] = {
        0,    0, 0,        //
        1023, 0, 0,        // 3, ...
        547,  0, 0,        // 6, ...
        372,  0, 0, 0, 0,  // 9, ...
        273,  0, 0,        // 14, ...
        215,  0, 0,        // 17, ...
        178,  0, 0,        // 20, ...
        151,  0, 0,        // 23, ... (113 & 203 are base angles)
        132,  0, 0,        // 26, ...
        116,  0, 0,        // 29, ...
        102,  0, 0, 0,     // 32, ...
        90,   0, 0,        // 36, ...
        80,   0, 0,        // 39, ...
        71,   0, 0,        // 42, ...
        64,   0, 0,        // 45, ... (45 & 135 are base angles)
        57,   0, 0,        // 48, ...
        51,   0, 0,        // 51, ...
        45,   0, 0, 0,     // 54, ...
        40,   0, 0,        // 58, ...
        35,   0, 0,        // 61, ...
        31,   0, 0,        // 64, ...
        27,   0, 0,        // 67, ... (67 & 157 are base angles)
        23,   0, 0,        // 70, ...
        19,   0, 0,        // 73, ...
        15,   0, 0, 0, 0,  // 76, ...
        11,   0, 0,        // 81, ...
        7,    0, 0,        // 84, ...
        3,    0, 0,        // 87, ...
    };

    // Get the shift (up-scaled by 256) in X w.r.t a unit change in Y.
    // If angle > 0 && angle < 90, dx = -((int)(256 / t));
    // If angle > 90 && angle < 180, dx = (int)(256 / t);
    // If angle > 180 && angle < 270, dx = 1;
    static int get_dx(int angle) {
        if (angle > 0 && angle < 90) {
            return dr_intra_derivative[angle];
        } else if (angle > 90 && angle < 180) {
            return dr_intra_derivative[180 - angle];
        } else {
            // In this case, we are not really going to use dx. We may return any value.
            return 1;
        }
    }

    // Get the shift (up-scaled by 256) in Y w.r.t a unit change in X.
    // If angle > 0 && angle < 90, dy = 1;
    // If angle > 90 && angle < 180, dy = (int)(256 * t);
    // If angle > 180 && angle < 270, dy = -((int)(256 * t));
    static int get_dy(int angle) {
        if (angle > 90 && angle < 180) {
            return dr_intra_derivative[angle - 90];
        } else if (angle > 180 && angle < 270) {
            return dr_intra_derivative[270 - angle];
        } else {
            // In this case, we are not really going to use dy. We may return any value.
            return 1;
        }
    }

    // specialization for AV1
    template void predict_intra_av1_px<0, 1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0, 2>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0, 3>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0, 4>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0, 5>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0, 6>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0, 7>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0, 8>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0, 9>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0,10>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0,11>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<0,12>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1, 1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1, 2>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1, 3>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1, 4>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1, 5>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1, 6>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1, 7>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1, 8>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1, 9>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1,10>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1,11>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<1,12>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2, 1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2, 2>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2, 3>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2, 4>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2, 5>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2, 6>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2, 7>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2, 8>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2, 9>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2,10>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2,11>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<2,12>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3, 1>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3, 2>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3, 3>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3, 4>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3, 5>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3, 6>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3, 7>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3, 8>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3, 9>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3,10>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3,11>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);
    template void predict_intra_av1_px<3,12>(const uint8_t*,const uint8_t*,uint8_t*,int,int,int,int);

}; // namespace VP9PP
