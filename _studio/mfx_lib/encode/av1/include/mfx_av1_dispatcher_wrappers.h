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


#pragma once

#include "mfx_common.h"
#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "mfx_av1_dispatcher_fptr.h"
#include "mfx_av1_tables.h"
#include "mfx_av1_defs.h"

namespace AV1PP {

    inline void predict_intra_palette(uint8_t *dst, int32_t pitch, int32_t txSize, const uint8_t *palette, const uint8_t *color_map) {
        assert(txSize >= 0 && txSize <= 3);
        assert(dst != nullptr);
        assert(palette != nullptr);
        assert(color_map != nullptr);
        assert(predict_intra_palette_fptr_arr[txSize] != nullptr);
        predict_intra_palette_fptr_arr[txSize](dst, pitch, palette, color_map);
    }

    inline void predict_intra_palette(uint16_t *dst, int32_t pitch, int32_t txSize, const uint8_t *palette, const uint8_t *color_map) {
        assert(!"not implemented");
    }

    inline void map_intra_palette(const uint8_t *src, uint8_t *dst, int pitch, int width, uint16_t *palette, uint8_t palette_size) {
        for (int32_t i = 0; i < width; i++) {
            for (int32_t j = 0; j < width; j++) {
                uint32_t best_color_idx = 0;
                uint32_t min_sad = 4096;
                for (uint32_t k = 0; k < palette_size; k++) {
                    uint32_t sad = abs((int)src[i * 64 + j] - (int)palette[k]);
                    if (sad < min_sad) {
                        min_sad = sad;
                        best_color_idx = k;
                    }
                }
                dst[i * pitch + j] = (uint8_t) best_color_idx;
            }
        }
    }

    inline void map_intra_palette(const uint16_t *src, uint8_t *dst, int pitch, int width, uint16_t *palette, uint8_t palette_size) {
        assert(!"not implemented");
    }

    inline void predict_intra_av1(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int txSize, int haveLeft, int haveAbove,
                                  int mode, int delta, int upTop, int upLeft) {
        assert(txSize >= 0 && txSize <= 3);
        assert(haveLeft == 0 || haveLeft == 1);
        assert(haveAbove == 0 || haveAbove == 1);
        assert(mode >= 0 && mode <= 12);
        assert(delta >= -3 && delta <= 3);
        assert(predict_intra_av1_fptr_arr[txSize][haveLeft][haveAbove][mode] != NULL);
        predict_intra_av1_fptr_arr[txSize][haveLeft][haveAbove][mode](topPels, leftPels, dst, pitch, delta, upTop, upLeft);
    }
    inline void predict_intra_av1(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int txSize,
                                  int haveLeft, int haveAbove, int mode, int delta, int upTop, int upLeft) {
        //assert(!"not implemented");
        assert(txSize >= 0 && txSize <= 3);
        assert(haveLeft == 0 || haveLeft == 1);
        assert(haveAbove == 0 || haveAbove == 1);
        assert(mode >= 0 && mode <= 12);
        assert(delta >= -3 && delta <= 3);
        assert(predict_intra_av1_fptr_arr[txSize][haveLeft][haveAbove][mode] != NULL);
        predict_intra_av1_hbd_fptr_arr[txSize][haveLeft][haveAbove][mode](topPels, leftPels, dst, pitch, delta, upTop, upLeft);
    }

    inline void predict_intra_nv12_av1(const uint8_t *topPels, const uint8_t *leftPels, uint8_t *dst, int pitch, int txSize,
                                       int haveLeft, int haveAbove, int mode, int delta, int upTop, int upLeft)
    {
        assert(txSize >= 0 && txSize <= 3);
        assert(haveLeft == 0 || haveLeft == 1);
        assert(haveAbove == 0 || haveAbove == 1);
        assert(mode >= 0 && mode <= 12);
        assert(delta >= -3 && delta <= 3);
        assert(predict_intra_nv12_av1_fptr_arr[txSize][haveLeft][haveAbove][mode]);
        predict_intra_nv12_av1_fptr_arr[txSize][haveLeft][haveAbove][mode](topPels, leftPels, dst, pitch, delta, upTop, upLeft);
    }

    inline void predict_intra_nv12_av1(const uint16_t *topPels, const uint16_t *leftPels, uint16_t *dst, int pitch, int txSize,
                                       int haveLeft, int haveAbove, int mode, int delta, int upTop, int upLeft)  {
        //assert(!"not implemented");
        assert(txSize >= 0 && txSize <= 3);
        assert(haveLeft == 0 || haveLeft == 1);
        assert(haveAbove == 0 || haveAbove == 1);
        assert(mode >= 0 && mode <= 12);
        assert(delta >= -3 && delta <= 3);
        assert(predict_intra_nv12_av1_fptr_arr[txSize][haveLeft][haveAbove][mode]);
        predict_intra_nv12_av1_hbd_fptr_arr[txSize][haveLeft][haveAbove][mode](topPels, leftPels, dst, pitch, delta, upTop, upLeft);
    }

    inline void predict_intra_all(const uint8_t* rec, int pitchRec, uint8_t *dst, int txSize, int haveLeft, int haveAbove) {
        assert(txSize >= 0 && txSize <= 3);
        assert(haveLeft == 0 || haveLeft == 1);
        assert(haveAbove == 0 || haveAbove == 1);
        predict_intra_all_fptr_arr[txSize][haveLeft][haveAbove](rec, pitchRec, dst);
    }
    inline void predict_intra_all(const uint16_t* rec, int pitchRec, uint16_t *dst, int txSize, int haveLeft, int haveAbove) {
        assert(!"not implemented");
    }

    inline void ftransform_vp9(const int16_t *src, int16_t *dst, int pitchSrc, int size, int type) {
        assert(size >= 0 && size <= 10);
        assert(type >= 0 && type <= 3);
        ftransform_vp9_fptr_arr[size][type](src, dst, pitchSrc);
    }

    inline void ftransform_av1(const int16_t *src, int16_t *dst, int pitchSrc, int size, int type) {
        assert(size >= 0 && size <= 10);
        assert(type >= 0 && type <= 3 || type == 9);
        ftransform_av1_fptr_arr[size][type](src, dst, pitchSrc);
    }
    inline void ftransform_av1(const int16_t *src, int32_t *dst, int pitchSrc, int size, int type) {
        assert(size >= 0 && size <= 10);
        assert(type >= 0 && type <= 3 || type == 9);
        ftransform_av1_hbd_fptr_arr[size][type](src, dst, pitchSrc);
    }
    /*inline void itransform_vp9(const int16_t *src, int16_t *dst, int pitchSrc, int size, int type) {
        assert(size >= 0 && size <= 3);
        assert(type >= 0 && type <= 3);
        itransform_vp9_fptr_arr[size][type](src, dst, pitchSrc);
    }*/

    inline void itransform_av1(const int16_t *src, int16_t *dst, int pitchSrc, int size, int type, int bd) {
        assert(size >= 0 && size <= 3);
        assert(type >= 0 && type <= 3 || type == 9);
        itransform_av1_fptr_arr[size][type](src, dst, pitchSrc, bd);
    }

    inline void itransform_add_vp9(const int16_t *src, uint8_t *dst, int pitchSrc, int size, int type) {
        assert(size >= 0 && size <= 3);
        assert(type >= 0 && type <= 3);
        itransform_add_vp9_fptr_arr[size][type](src, dst, pitchSrc);
    }
    inline void itransform_add_vp9(const int16_t *src, uint16_t *dst, int pitchSrc, int size, int type) {
        assert(!"not implemented");
    }

    inline void itransform_add_av1(const int16_t *src, uint8_t *dst, int pitchSrc, int size, int type) {
        assert(size >= 0 && size <= 3);
        assert(type >= 0 && type <= 3 || type == 9);
        itransform_add_av1_fptr_arr[size][type](src, dst, pitchSrc);
    }
    inline void itransform_add_av1(const int16_t *src, uint16_t *dst, int pitchSrc, int size, int type) {
        ///assert(!"not implemented");
        itransform_add_av1_hbd_fptr_arr[size][type](src, dst, pitchSrc);
    }
    inline void itransform_add_av1(const int32_t *src, uint16_t *dst, int pitchSrc, int size, int type) {
        //assert(!"not implemented");
        itransform_add_av1_hbd_hbd_fptr_arr[size][type](src, dst, pitchSrc);
    }

    inline int quant(const int16_t *src, int16_t *dst, const AV1Enc::QuantParam &qpar, int txSize) {
        assert(txSize >= 0 && txSize <= 3);
        return quant_fptr_arr[txSize](src, dst, reinterpret_cast<const int16_t *>(&qpar));
    }

    inline int quant(const int32_t *src, int16_t *dst, const AV1Enc::QuantParam &qpar, int txSize) {
        assert(txSize >= 0 && txSize <= 3);
        return quant_hbd_fptr_arr[txSize](src, dst, reinterpret_cast<const int16_t *>(&qpar));
    }

    inline void dequant(const int16_t *src, int16_t *dst, const AV1Enc::QuantParam &qpar, int txSize, int bd) {
        assert(txSize >= 0 && txSize <= 3);
        dequant_fptr_arr[txSize](src, dst, qpar.dequant, bd);
    }

    inline void dequant(const int16_t *src, int32_t *dst, const AV1Enc::QuantParam &qpar, int txSize, int bd) {
        assert(txSize >= 0 && txSize <= 3);
        dequant_hbd_fptr_arr[txSize](src, dst, qpar.dequant, bd);
    }

    inline int quant_dequant(int16_t *srcdst, int16_t *dst, const AV1Enc::QuantParam &qpar, int txSize) {
        assert(txSize >= 0 && txSize <= 3);
        return quant_dequant_fptr_arr[txSize](srcdst, dst, reinterpret_cast<const int16_t *>(&qpar));
    }

    inline void average(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, uint8_t *dst, int pitchDst, int h, int log2w) {
        assert(log2w >= 0 && log2w <= 4);
        average_fptr_arr[log2w](src1, pitchSrc1, src2, pitchSrc2, dst, pitchDst, h);
    }
    inline void average(const uint16_t *src1, int pitchSrc1, const uint16_t *src2, int pitchSrc2, uint16_t *dst, int pitchDst, int h, int log2w) {
        assert(!"not implemented");
    }

    inline void average(const uint8_t *src1, const uint8_t *src2, uint8_t *dst, int h, int log2w) {
        assert(log2w >= 0 && log2w <= 4);
        average_pitch64_fptr_arr[log2w](src1, src2, dst, h);
    }
    inline void average(const uint16_t *src1, const uint16_t *src2, uint16_t *dst, int h, int log2w) {
        assert(!"not implemented");
    }

    inline void interp_vp9(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, int dx, int dy, int h, int log2w, int avg, int filterType) {
        assert(log2w >= 0 && log2w <= 4);
        assert(dx >= 0 && dx <= 15);
        assert(dy >= 0 && dy <= 15);
        assert(avg == 0 || avg == 1);
        assert(filterType >= 0 && filterType <= 3);
        const AV1Enc::InterpKernel *kernel = AV1Enc::vp9_filter_kernels[filterType];
        interp_fptr_arr[log2w][dx!=0][dy!=0][avg](src, pitchSrc, dst, pitchDst, kernel[dx], kernel[dy], h);
    }

    inline void interp_vp9(const uint16_t *src, int pitchSrc, uint16_t *dst, int pitchDst, int dx, int dy, int h, int log2w, int avg, int filterType) {
        assert(!"not implemented");
    }

    inline void interp_av1(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, int dx, int dy, int h, int log2w, int avg, int filterType) {
        assert(log2w >= 0 && log2w <= 4);
        assert(dx >= 0 && dx <= 15);
        assert(dy >= 0 && dy <= 15);
        int interp0 = filterType & 0x3;
        int interp1 = (filterType >> 4) & 0x3;
        assert(interp0 >= 0 && interp0 <= 3);
        assert(interp1 >= 0 && interp1 <= 3);
        const AV1Enc::InterpKernel *kernel_y = AV1Enc::av1_filter_kernels[interp0];
        const AV1Enc::InterpKernel *kernel_x = AV1Enc::av1_filter_kernels[interp1];
        interp_av1_single_ref_fptr_arr[log2w][dx!=0][dy!=0](src, pitchSrc, dst, pitchDst, kernel_x[dx], kernel_y[dy], h);
    }
    inline void interp_av1(const uint16_t *src, int pitchSrc, uint16_t *dst, int pitchDst, int dx, int dy, int h, int log2w, int avg, int filterType) {
        assert(!"not implemented");
    }

    inline void interp_vp9(const uint8_t *src, int pitchSrc, uint8_t *dst, int dx, int dy, int h, int log2w, int avg, int filterType) {
        assert(log2w >= 0 && log2w <= 4);
        assert(dx >= 0 && dx <= 15);
        assert(dy >= 0 && dy <= 15);
        assert(avg == 0 || avg == 1);
        assert(filterType >= 0 && filterType <= 3);
        const AV1Enc::InterpKernel *kernel = AV1Enc::vp9_filter_kernels[filterType];
        interp_pitch64_fptr_arr[log2w][dx!=0][dy!=0][avg](src, pitchSrc, dst, kernel[dx], kernel[dy], h);
    }
    inline void interp_vp9(const uint16_t *src, int pitchSrc, uint16_t *dst, int dx, int dy, int h, int log2w, int avg, int filterType) {
        assert(!"not implemented");
    }

    inline void interp_av1(const uint8_t *src, int pitchSrc, uint8_t *dst, int dx, int dy, int h, int log2w, int filterType) {
        assert(log2w >= 0 && log2w <= 4);
        assert(dx >= 0 && dx <= 15);
        assert(dy >= 0 && dy <= 15);
        int interp0 = filterType & 0x3;
        int interp1 = (filterType >> 4) & 0x3;
        assert(interp0 >= 0 && interp0 <= 3);
        assert(interp1 >= 0 && interp1 <= 3);
        const AV1Enc::InterpKernel *kernel_y = AV1Enc::av1_filter_kernels[interp0];
        const AV1Enc::InterpKernel *kernel_x = AV1Enc::av1_filter_kernels[interp1];
        interp_pitch64_av1_single_ref_fptr_arr[log2w][dx!=0][dy!=0](src, pitchSrc, dst, kernel_x[dx], kernel_y[dy], h);
    }
    inline void interp_av1(const uint16_t *src, int pitchSrc, uint16_t *dst, int dx, int dy, int h, int log2w, int filterType) {
        assert(log2w >= 0 && log2w <= 4);
        assert(dx >= 0 && dx <= 15);
        assert(dy >= 0 && dy <= 15);
        int interp0 = filterType & 0x3;
        int interp1 = (filterType >> 4) & 0x3;
        assert(interp0 >= 0 && interp0 <= 3);
        assert(interp1 >= 0 && interp1 <= 3);
        const AV1Enc::InterpKernel *kernel_y = AV1Enc::av1_filter_kernels[interp0];
        const AV1Enc::InterpKernel *kernel_x = AV1Enc::av1_filter_kernels[interp1];
        interp_pitch64_av1_single_ref_hbd_fptr_arr[log2w][dx != 0][dy != 0](src, pitchSrc, dst, kernel_x[dx], kernel_y[dy], h);
    }

    inline void interp_av1(const uint8_t *src, int pitchSrc, int16_t *dst, int dx, int dy, int h, int log2w, int filterType) {
        assert(log2w >= 0 && log2w <= 4);
        assert(dx >= 0 && dx <= 15);
        assert(dy >= 0 && dy <= 15);
        int interp0 = filterType & 0x3;
        int interp1 = (filterType >> 4) & 0x3;
        assert(interp0 >= 0 && interp0 <= 3);
        assert(interp1 >= 0 && interp1 <= 3);
        const AV1Enc::InterpKernel *kernel_y = AV1Enc::av1_filter_kernels[interp0];
        const AV1Enc::InterpKernel *kernel_x = AV1Enc::av1_filter_kernels[interp1];
        interp_pitch64_av1_first_ref_fptr_arr[log2w][dx!=0][dy!=0](src, pitchSrc, dst, kernel_x[dx], kernel_y[dy], h);
    }

    inline void interp_av1(const uint16_t *src, int pitchSrc, int16_t *dst, int dx, int dy, int h, int log2w, int filterType) {
        assert(log2w >= 0 && log2w <= 4);
        assert(dx >= 0 && dx <= 15);
        assert(dy >= 0 && dy <= 15);
        int interp0 = filterType & 0x3;
        int interp1 = (filterType >> 4) & 0x3;
        assert(interp0 >= 0 && interp0 <= 3);
        assert(interp1 >= 0 && interp1 <= 3);
        const AV1Enc::InterpKernel *kernel_y = AV1Enc::av1_filter_kernels[interp0];
        const AV1Enc::InterpKernel *kernel_x = AV1Enc::av1_filter_kernels[interp1];
        interp_pitch64_av1_first_ref_hbd_fptr_arr[log2w][dx!=0][dy!=0](src, pitchSrc, dst, kernel_x[dx], kernel_y[dy], h);
    }

    inline void interp_av1(const uint8_t *src, int pitchSrc, const int16_t *ref0, uint8_t *dst, int dx, int dy, int h, int log2w, int filterType) {
        assert(log2w >= 0 && log2w <= 4);
        assert(dx >= 0 && dx <= 15);
        assert(dy >= 0 && dy <= 15);
        int interp0 = filterType & 0x3;
        int interp1 = (filterType >> 4) & 0x3;
        assert(interp0 >= 0 && interp0 <= 3);
        assert(interp1 >= 0 && interp1 <= 3);
        const AV1Enc::InterpKernel *kernel_y = AV1Enc::av1_filter_kernels[interp0];
        const AV1Enc::InterpKernel *kernel_x = AV1Enc::av1_filter_kernels[interp1];
        interp_pitch64_av1_second_ref_fptr_arr[log2w][dx!=0][dy!=0](src, pitchSrc, ref0, dst, kernel_x[dx], kernel_y[dy], h);
    }

    inline void interp_av1(const uint16_t *src, int pitchSrc, const int16_t *ref0, uint16_t *dst, int dx, int dy, int h, int log2w, int filterType) {
        assert(log2w >= 0 && log2w <= 4);
        assert(dx >= 0 && dx <= 15);
        assert(dy >= 0 && dy <= 15);
        int interp0 = filterType & 0x3;
        int interp1 = (filterType >> 4) & 0x3;
        assert(interp0 >= 0 && interp0 <= 3);
        assert(interp1 >= 0 && interp1 <= 3);
        const AV1Enc::InterpKernel *kernel_y = AV1Enc::av1_filter_kernels[interp0];
        const AV1Enc::InterpKernel *kernel_x = AV1Enc::av1_filter_kernels[interp1];
        interp_pitch64_av1_second_ref_hbd_fptr_arr[log2w][dx!=0][dy!=0](src, pitchSrc, ref0, dst, kernel_x[dx], kernel_y[dy], h);
    }

    inline void interp_nv12_vp9(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, int dx, int dy, int h, int log2w, int avg, int filterType) {
        assert(dx >= 0 && dx <= 15);
        assert(dy >= 0 && dy <= 15);
        assert(avg == 0 || avg == 1);
        assert(filterType >= 0 && filterType <= 3);
        const AV1Enc::InterpKernel *kernel = AV1Enc::vp9_filter_kernels[filterType];
        interp_nv12_fptr_arr[log2w][dx!=0][dy!=0][avg](src, pitchSrc, dst, pitchDst, kernel[dx], kernel[dy], h);
    }
    inline void interp_nv12_vp9(const uint16_t *src, int pitchSrc, uint16_t *dst, int pitchDst, int dx, int dy, int h, int log2w, int avg, int filterType) {
        assert(!"not implemented");
    }

    inline void interp_nv12_av1(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, int dx, int dy, int h, int log2w, int avg, int filterType) {
        assert(dx >= 0 && dx <= 15);
        assert(dy >= 0 && dy <= 15);
        assert(avg == 0 || avg == 1);
        assert(filterType >= 0 && filterType <= 3);
        const AV1Enc::InterpKernel *kernel = AV1Enc::av1_filter_kernels[filterType];
        interp_nv12_fptr_arr[log2w][dx!=0][dy!=0][avg](src, pitchSrc, dst, pitchDst, kernel[dx], kernel[dy], h);
    }
    inline void interp_nv12_av1(const uint16_t *src, int pitchSrc, uint16_t *dst, int pitchDst, int dx, int dy, int h, int log2w, int avg, int filterType) {
        assert(!"not implemented");
    }

    inline void interp_nv12_vp9(const uint8_t *src, int pitchSrc, uint8_t *dst, int dx, int dy, int h, int log2w, int avg, int filterType) {
        assert(log2w >= 0 && log2w <= 3);
        assert(dx >= 0 && dx <= 15);
        assert(dy >= 0 && dy <= 15);
        assert(avg == 0 || avg == 1);
        assert(filterType >= 0 && filterType <= 3);
        const AV1Enc::InterpKernel *kernel = AV1Enc::vp9_filter_kernels[filterType];
        interp_nv12_pitch64_fptr_arr[log2w][dx!=0][dy!=0][avg](src, pitchSrc, dst, kernel[dx], kernel[dy], h);
    }
    inline void interp_nv12_vp9(const uint16_t *src, int pitchSrc, uint16_t *dst, int dx, int dy, int h, int log2w, int avg, int filterType) {
        assert(!"not implemented");
    }

    inline void interp_nv12_av1(const uint8_t *src, int pitchSrc, uint8_t *dst, int dx, int dy, int h, int log2w, int filterType) {
        assert(log2w >= 0 && log2w <= 3);
        assert(dx >= 0 && dx <= 15);
        assert(dy >= 0 && dy <= 15);
        int interp0 = filterType & 0x3;
        int interp1 = (filterType >> 4) & 0x3;
        assert(interp0 >= 0 && interp0 <= 3);
        assert(interp1 >= 0 && interp1 <= 3);
        const AV1Enc::InterpKernel *kernel_y =
            (h <= 4 ? AV1Enc::av1_short_filter_kernels : AV1Enc::av1_filter_kernels)[interp0];
        const AV1Enc::InterpKernel *kernel_x =
            (log2w == 0 ? AV1Enc::av1_short_filter_kernels : AV1Enc::av1_filter_kernels)[interp1];
        interp_nv12_pitch64_av1_single_ref_fptr_arr[log2w][dx!=0][dy!=0](src, pitchSrc, dst, kernel_x[dx], kernel_y[dy], h);
    }
    inline void interp_nv12_av1(const uint16_t *src, int pitchSrc, uint16_t *dst, int dx, int dy, int h, int log2w, int filterType) {
        //assert(!"not implemented");
        assert(log2w >= 0 && log2w <= 3);
        assert(dx >= 0 && dx <= 15);
        assert(dy >= 0 && dy <= 15);
        int interp0 = filterType & 0x3;
        int interp1 = (filterType >> 4) & 0x3;
        assert(interp0 >= 0 && interp0 <= 3);
        assert(interp1 >= 0 && interp1 <= 3);
        const AV1Enc::InterpKernel *kernel_y =
            (h <= 4 ? AV1Enc::av1_short_filter_kernels : AV1Enc::av1_filter_kernels)[interp0];
        const AV1Enc::InterpKernel *kernel_x =
            (log2w == 0 ? AV1Enc::av1_short_filter_kernels : AV1Enc::av1_filter_kernels)[interp1];
        interp_nv12_pitch64_av1_single_ref_hbd_fptr_arr[log2w][dx != 0][dy != 0](src, pitchSrc, dst, kernel_x[dx], kernel_y[dy], h);
    }

    inline void interp_nv12_av1(const uint8_t *src, int pitchSrc, int16_t *dst, int dx, int dy, int h, int log2w, int filterType) {
        assert(log2w >= 0 && log2w <= 3);
        assert(dx >= 0 && dx <= 15);
        assert(dy >= 0 && dy <= 15);
        int interp0 = filterType & 0x3;
        int interp1 = (filterType >> 4) & 0x3;
        assert(interp0 >= 0 && interp0 <= 3);
        assert(interp1 >= 0 && interp1 <= 3);
        const AV1Enc::InterpKernel *kernel_y =
            (h <= 4 ? AV1Enc::av1_short_filter_kernels : AV1Enc::av1_filter_kernels)[interp0];
        const AV1Enc::InterpKernel *kernel_x =
            (log2w == 0 ? AV1Enc::av1_short_filter_kernels : AV1Enc::av1_filter_kernels)[interp1];
        interp_nv12_pitch64_av1_first_ref_fptr_arr[log2w][dx!=0][dy!=0](src, pitchSrc, dst, kernel_x[dx], kernel_y[dy], h);
    }
    inline void interp_nv12_av1(const uint16_t *src, int pitchSrc, int16_t *dst, int dx, int dy, int h, int log2w, int filterType) {
        //assert(!"not implemented");
        assert(log2w >= 0 && log2w <= 3);
        assert(dx >= 0 && dx <= 15);
        assert(dy >= 0 && dy <= 15);
        int interp0 = filterType & 0x3;
        int interp1 = (filterType >> 4) & 0x3;
        assert(interp0 >= 0 && interp0 <= 3);
        assert(interp1 >= 0 && interp1 <= 3);
        const AV1Enc::InterpKernel *kernel_y =
            (h <= 4 ? AV1Enc::av1_short_filter_kernels : AV1Enc::av1_filter_kernels)[interp0];
        const AV1Enc::InterpKernel *kernel_x =
            (log2w == 0 ? AV1Enc::av1_short_filter_kernels : AV1Enc::av1_filter_kernels)[interp1];
        interp_nv12_pitch64_av1_first_ref_hbd_fptr_arr[log2w][dx!=0][dy!=0](src, pitchSrc, dst, kernel_x[dx], kernel_y[dy], h);
    }

    inline void interp_nv12_av1(const uint8_t *src, int pitchSrc, const int16_t *ref0, uint8_t *dst, int dx, int dy, int h, int log2w, int filterType) {
        assert(log2w >= 0 && log2w <= 3);
        assert(dx >= 0 && dx <= 15);
        assert(dy >= 0 && dy <= 15);
        int interp0 = filterType & 0x3;
        int interp1 = (filterType >> 4) & 0x3;
        assert(interp0 >= 0 && interp0 <= 3);
        assert(interp1 >= 0 && interp1 <= 3);
        const AV1Enc::InterpKernel *kernel_y =
            (h <= 4 ? AV1Enc::av1_short_filter_kernels : AV1Enc::av1_filter_kernels)[interp0];
        const AV1Enc::InterpKernel *kernel_x =
            (log2w == 0 ? AV1Enc::av1_short_filter_kernels : AV1Enc::av1_filter_kernels)[interp1];

        interp_nv12_pitch64_av1_second_ref_fptr_arr[log2w][dx!=0][dy!=0](src, pitchSrc, ref0, dst, kernel_x[dx], kernel_y[dy], h);
    }
    inline void interp_nv12_av1(const uint16_t *src, int pitchSrc, const int16_t *ref0, uint16_t *dst, int dx, int dy, int h, int log2w, int filterType) {
        //assert(!"not implemented");
        assert(log2w >= 0 && log2w <= 3);
        assert(dx >= 0 && dx <= 15);
        assert(dy >= 0 && dy <= 15);
        int interp0 = filterType & 0x3;
        int interp1 = (filterType >> 4) & 0x3;
        assert(interp0 >= 0 && interp0 <= 3);
        assert(interp1 >= 0 && interp1 <= 3);
        const AV1Enc::InterpKernel *kernel_y =
            (h <= 4 ? AV1Enc::av1_short_filter_kernels : AV1Enc::av1_filter_kernels)[interp0];
        const AV1Enc::InterpKernel *kernel_x =
            (log2w == 0 ? AV1Enc::av1_short_filter_kernels : AV1Enc::av1_filter_kernels)[interp1];

        interp_nv12_pitch64_av1_second_ref_hbd_fptr_arr[log2w][dx!=0][dy!=0](src, pitchSrc, ref0, dst, kernel_x[dx], kernel_y[dy], h);
    }

    //
    inline unsigned nearest_pow2(unsigned v) {
        --v;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;

        return ++v;
    }

    inline void interp_flex(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, int dx, int dy, int w, int h, int avg, int filterType, int isAV1) {
        assert(dx >= 0 && dx <= 15);
        assert(dy >= 0 && dy <= 15);
        assert(avg == 0 || avg == 1);
        assert(filterType >= 0 && filterType <= 3);
        const AV1Enc::InterpKernel *kernel = isAV1 ? AV1Enc::av1_filter_kernels[filterType] : AV1Enc::vp9_filter_kernels[filterType];
        //int w2 = nearest_pow2(w);
        //const int32_t log2w = AV1Enc::BSR(w2) - 2;
        const int32_t log2w = AV1Enc::BSR(w) - 2;
        interp_av1_single_ref_fptr_arr[log2w][dx != 0][dy != 0](src, pitchSrc, dst, pitchDst, kernel[dx], kernel[dy], h);

        int w2 = (1 << (log2w + 2));
        int rem = w - w2;
        if(rem) {
            const int32_t log2r = AV1Enc::BSR(rem) - 2;
            assert(rem == (1 << (log2r + 2)));
            interp_av1_single_ref_fptr_arr[log2r][dx != 0][dy != 0](src + w2, pitchSrc, dst + w2, pitchDst, kernel[dx], kernel[dy], h);
        }
    }

    inline void interp_flex(const uint16_t *src, int pitchSrc, uint16_t *dst, int pitchDst, int dx, int dy, int w, int h, int avg, int filterType, int isAV1) {
        assert(!"not implemented");
    }

    inline void diff_nxn(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, int16_t *dst, int pitchDst, int log2w) {
        assert(log2w >= 0 && log2w <= 4);
        diff_nxn_fptr_arr[log2w](src1, pitchSrc1, src2, pitchSrc2, dst, pitchDst);
    }
    inline void diff_nxn(const uint16_t *src1, int pitchSrc1, const uint16_t *src2, int pitchSrc2, int16_t *dst, int pitchDst, int log2w) {
        //assert(!"not implemented");
        diff_nxn_hbd_fptr_arr[log2w](src1, pitchSrc1, src2, pitchSrc2, dst, pitchDst);
    }

    inline void diff_nxn(const uint8_t *src1, const uint8_t *src2, int16_t *dst, int log2w) {
        assert(log2w >= 0 && log2w <= 4);
        diff_nxn_p64_p64_pw_fptr_arr[log2w](src1, src2, dst);
    }
    inline void diff_nxn(const uint16_t *src1, const uint16_t *src2, int16_t *dst, int log2w) {
        assert(log2w >= 0 && log2w <= 4);
        diff_nxn_p64_p64_pw_hbd_fptr_arr[log2w](src1, src2, dst);
    }

    inline void diff_nxm(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, int16_t *dst, int pitchDst, int h, int log2w) {
        assert(log2w >= 0 && log2w <= 4);
        diff_nxm_fptr_arr[log2w](src1, pitchSrc1, src2, pitchSrc2, dst, pitchDst, h);
    }
    inline void diff_nxm(const uint16_t *src1, int pitchSrc1, const uint16_t *src2, int pitchSrc2, int16_t *dst, int pitchDst, int h, int log2w) {
        assert(!"not implemented");
    }

    inline void diff_nxm(const uint8_t *src1, const uint8_t *src2, int16_t *dst, int h, int log2w) {
        assert(log2w >= 0 && log2w <= 4);
        diff_nxm_p64_p64_pw_fptr_arr[log2w](src1, src2, dst, h);
    }
    inline void diff_nxm(const uint16_t *src1, const uint16_t *src2, int16_t *dst, int h, int log2w) {
        assert(!"not implemented");
    }

    inline void diff_nv12(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, int16_t *dstU, int16_t *dstV, int pitchDst, int height, int log2w) {
        assert(log2w >= 0 && log2w <= 3);
        diff_nv12_fptr_arr[log2w](src1, pitchSrc1, src2, pitchSrc2, dstU, dstV, pitchDst, height);
    }
    inline void diff_nv12(const uint16_t *src1, int pitchSrc1, const uint16_t *src2, int pitchSrc2, int16_t *dstU, int16_t *dstV, int pitchDst, int height, int log2w) {
        assert(log2w >= 0 && log2w <= 3);
        diff_nv12_hbd_fptr_arr[log2w](src1, pitchSrc1, src2, pitchSrc2, dstU, dstV, pitchDst, height);
    }

    inline void diff_nv12(const uint8_t *src1, const uint8_t *src2, int16_t *dstU, int16_t *dstV, int height, int log2w) {
        assert(log2w >= 0 && log2w <= 3);
        diff_nv12_p64_p64_pw_fptr_arr[log2w](src1, src2, dstU, dstV, height);
    }
    inline void diff_nv12(const uint16_t *src1, const uint16_t *src2, int16_t *dstU, int16_t *dstV, int height, int log2w) {
        //assert(!"not implemented");
        assert(log2w >= 0 && log2w <= 3);
        diff_nv12_p64_p64_pw_hbd_fptr_arr[log2w](src1, src2, dstU, dstV, height);
    }

    inline int satd_4x4(const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2) {
        return satd_4x4_fptr(src1, pitch1, src2, pitch2);
    }
    inline int satd_4x4(const uint16_t* src1, int pitch1, const uint16_t* src2, int pitch2) {
        assert(!"not implemented");
        return 0;
    }

    inline int satd_4x4(const uint8_t* src1, const uint8_t* src2, int pitch2) {
        return satd_4x4_pitch64_fptr(src1, src2, pitch2);
    }
    inline int satd_4x4(const uint16_t* src1, const uint16_t* src2, int pitch2) {
        assert(!"not implemented");
        return 0;
    }

    inline int satd_4x4(const uint8_t* src1, const uint8_t* src2) {
        return satd_4x4_pitch64_both_fptr(src1, src2);
    }
    inline int satd_4x4(const uint16_t* src1, const uint16_t* src2) {
        assert(!"not implemented");
        return 0;
    }

    inline int satd_8x8(const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2) {
        return satd_8x8_fptr(src1, pitch1, src2, pitch2);
    }
    inline int satd_8x8(const uint16_t* src1, int pitch1, const uint16_t* src2, int pitch2) {
        assert(!"not implemented");
        return 0;
    }

    inline int satd_8x8(const uint8_t* src1, const uint8_t* src2, int pitch2) {
        return satd_8x8_pitch64_fptr(src1, src2, pitch2);
    }
    inline int satd_8x8(const uint16_t* src1, const uint16_t* src2, int pitch2) {
        assert(!"not implemented");
        return 0;
    }

    inline int satd_8x8(const uint8_t* src1, const uint8_t* src2) {
        return satd_8x8_pitch64_both_fptr(src1, src2);
    }
    inline int satd_8x8(const uint16_t* src1, const uint16_t* src2) {
        assert(!"not implemented");
        return 0;
    }

    inline void satd_4x4_pair(const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2, int *satdPair) {
        return satd_4x4_pair_fptr(src1, pitch1, src2, pitch2, satdPair);
    }
    inline void satd_4x4_pair(const uint16_t* src1, int pitch1, const uint16_t* src2, int pitch2, int *satdPair) {
        assert(!"not implemented");
    }

    inline void satd_4x4_pair(const uint8_t* src1, const uint8_t* src2, int pitch2, int *satdPair) {
        return satd_4x4_pair_pitch64_fptr(src1, src2, pitch2, satdPair);
    }
    inline void satd_4x4_pair(const uint16_t* src1, const uint16_t* src2, int pitch2, int *satdPair) {
        assert(!"not implemented");
    }

    inline void satd_4x4_pair(const uint8_t* src1, const uint8_t* src2, int *satdPair) {
        return satd_4x4_pair_pitch64_both_fptr(src1, src2, satdPair);
    }
    inline void satd_4x4_pair(const uint16_t* src1, const uint16_t* src2, int *satdPair) {
        assert(!"not implemented");
    }

    inline void satd_8x8_pair(const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2, int *satdPair) {
        return satd_8x8_pair_fptr(src1, pitch1, src2, pitch2, satdPair);
    }
    inline void satd_8x8_pair(const uint16_t* src1, int pitch1, const uint16_t* src2, int pitch2, int *satdPair) {
        assert(!"not implemented");
    }

    inline void satd_8x8_pair(const uint8_t* src1, const uint8_t* src2, int pitch2, int *satdPair) {
        satd_8x8_pair_pitch64_fptr(src1, src2, pitch2, satdPair);
    }
    inline void satd_8x8_pair(const uint16_t* src1, const uint16_t* src2, int pitch2, int *satdPair) {
        assert(!"not implemented");
    }

    inline void satd_8x8_pair(const uint8_t* src1, const uint8_t* src2, int *satdPair) {
        satd_8x8_pair_pitch64_both_fptr(src1, src2, satdPair);
    }
    inline void satd_8x8_pair(const uint16_t* src1, const uint16_t* src2, int *satdPair) {
        assert(!"not implemented");
    }

    inline void satd_8x8x2(const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2, int *satdPair) {
        satd_8x8x2_fptr(src1, pitch1, src2, pitch2, satdPair);
    }
    inline void satd_8x8x2(const uint16_t* src1, int pitch1, const uint16_t* src2, int pitch2, int *satdPair) {
        assert(!"not implemented");
    }

    inline void satd_8x8x2(const uint8_t* src1, const uint8_t* src2, int pitch2, int *satdPair) {
        satd_8x8x2_pitch64_fptr(src1, src2, pitch2, satdPair);
    }
    inline void satd_8x8x2(const uint16_t* src1, const uint16_t* src2, int pitch2, int *satdPair) {
        assert(!"not implemented");
    }

    inline int satd(const uint8_t* src1, int pitch1, const uint8_t* src2, int pitch2, int log2w, int log2h) {
        assert(log2w >= 0 && log2w <= 4);
        assert(log2h >= 0 && log2h <= 4);
        assert(satd_fptr_arr[log2w][log2h] != NULL);
        return satd_fptr_arr[log2w][log2h](src1, pitch1, src2, pitch2);
    }
    inline int satd(const uint16_t* src1, int pitch1, const uint16_t* src2, int pitch2, int log2w, int log2h) {
        assert(!"not implemented");
        return 0;
    }

    inline int satd(const uint8_t* src1, const uint8_t* src2, int pitch2, int log2w, int log2h) {
        assert(log2w >= 0 && log2w <= 4);
        assert(log2h >= 0 && log2h <= 4);
        assert(satd_pitch64_fptr_arr[log2w][log2h] != NULL);
        return satd_pitch64_fptr_arr[log2w][log2h](src1, src2, pitch2);
    }
    inline int satd(const uint16_t* src1, const uint16_t* src2, int pitch2, int log2w, int log2h) {
        assert(!"not implemented");
        return 0;
    }

    inline int satd_with_const(const uint8_t* src1, const uint8_t src2, int log2w, int log2h) {
        assert(log2w >= 0 && log2w <= 4);
        assert(log2h >= 0 && log2h <= 4);
        assert(satd_with_const_pitch64_fptr_arr[log2w][log2h] != NULL);
        return satd_with_const_pitch64_fptr_arr[log2w][log2h](src1, src2);
    }
    inline int satd_with_const(const uint16_t* src1, const uint16_t src2, int log2w, int log2h) {
        assert(!"not implemented");
        return 0;
    }

    inline int satd(const uint8_t* src1, const uint8_t* src2, int log2w, int log2h) {
        assert(log2w >= 0 && log2w <= 4);
        assert(log2h >= 0 && log2h <= 4);
        assert(satd_pitch64_both_fptr_arr[log2w][log2h] != NULL);
        return satd_pitch64_both_fptr_arr[log2w][log2h](src1, src2);
    }
    inline int satd(const uint16_t* src1, const uint16_t* src2, int log2w, int log2h) {
        //assert(!"not implemented");
        return 0;
    }

    inline int sse(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int log2w, int log2h) {
        assert(log2w >= 0 && log2w <= 4);
        assert(log2h >= 0 && log2h <= 4);
        assert(sse_fptr_arr[log2w][log2h] != NULL);
        return sse_fptr_arr[log2w][log2h](src1, pitch1, src2, pitch2);
    }
    inline int sse(const uint16_t *src1, int pitch1, const uint16_t *src2, int pitch2, int log2w, int log2h) {
        //assert(!"not implemented");
        assert(log2w >= 0 && log2w <= 4);
        assert(log2h >= 0 && log2h <= 4);
        assert(sse_hbd_fptr_arr[log2w][log2h] != NULL);
        return sse_hbd_fptr_arr[log2w][log2h](src1, pitch1, src2, pitch2);
    }

    inline int sse_p64_pw(const uint8_t *src1, const uint8_t *src2, int log2w, int log2h) {
        assert(log2w >= 0 && log2w <= 4);
        assert(log2h >= 0 && log2h <= 4);
        assert(sse_p64_pw_fptr_arr[log2w][log2h] != NULL);
        return sse_p64_pw_fptr_arr[log2w][log2h](src1, src2);
    }
    inline int sse_p64_pw(const uint16_t *src1, const uint16_t *src2, int log2w, int log2h) {
        assert(!"not implemented");
        return 0;
    }

    inline void sse_p64_pw_uv(const uint8_t *src1, const uint8_t *src2, int log2w, int log2h, int& costU, int& costV) {
        assert(log2w >= 0 && log2w <= 4);
        assert(log2h >= 0 && log2h <= 4);
        assert(sse_p64_pw_uv_fptr_arr[log2w][log2h] != NULL);
        sse_p64_pw_uv_fptr_arr[log2w][log2h](src1, src2, costU, costV);
    }
    inline void sse_p64_pw_uv(const uint16_t *src1, const uint16_t *src2, int log2w, int log2h, int& costU, int& costV) {
        assert(!"not implemented");
    }


    inline int sse_p64_p64(const uint8_t *src1, const uint8_t *src2, int log2w, int log2h) {
        assert(log2w >= 0 && log2w <= 4);
        assert(log2h >= 0 && log2h <= 4);
        assert(sse_p64_p64_fptr_arr[log2w][log2h] != NULL);
        return sse_p64_p64_fptr_arr[log2w][log2h](src1, src2);
    }
    inline int sse_p64_p64(const uint16_t *src1, const uint16_t *src2, int log2w, int log2h) {
        assert(log2w >= 0 && log2w <= 4);
        assert(log2h >= 0 && log2h <= 4);
        assert(sse_p64_p64_hbd_fptr_arr[log2w][log2h] != NULL);
        return sse_p64_p64_hbd_fptr_arr[log2w][log2h](src1, src2);
    }

    inline int sse_flexh(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int h, int log2w) {
        assert(log2w >= 0 && log2w <= 4);
        assert(sse_flexh_fptr_arr[log2w] != NULL);
        return sse_flexh_fptr_arr[log2w](src1, pitch1, src2, pitch2, h);
    }
    inline int sse_flexh(const uint16_t *src1, int pitch1, const uint16_t *src2, int pitch2, int h, int log2w) {
        assert(!"not implemented");
        return 0;
    }

    inline int64_t sse_cont(const int16_t *coeff, const int16_t *dqcoeff, int block_size) {
        return sse_cont_fptr(coeff, dqcoeff, block_size);
    }

    inline int64_t ssz_cont(const int16_t *coeff, int block_size) {
        return ssz_cont_fptr(coeff, block_size);
    }

    inline int sad_special(const uint8_t *src1, int pitch1, const uint8_t *src2, int log2w, int log2h) {
        assert(log2w >= 0 && log2w <= 4);
        assert(log2h >= 0 && log2h <= 4);
        assert(sad_special_fptr_arr[log2w][log2h] != NULL);
        return sad_special_fptr_arr[log2w][log2h](src1, pitch1, src2);
    }
    inline int sad_special(const uint16_t *src1, int pitch1, const uint16_t *src2, int log2w, int log2h) {
        assert(!"not implemented");
        return 0;
    }

    inline int sad_general(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int log2w, int log2h) {
        assert(log2w >= 0 && log2w <= 4);
        assert(log2h >= 0 && log2h <= 4);
        assert(sad_general_fptr_arr[log2w][log2h] != NULL);
        return sad_general_fptr_arr[log2w][log2h](src1, pitch1, src2, pitch2);
    }
    inline int sad_general(const uint16_t *src1, int pitch1, const uint16_t *src2, int pitch2, int log2w, int log2h) {
        assert(!"not implemented");
        return 0;
    }

    inline int sad_store8x8(const uint8_t *src1, const uint8_t *src2, int *sads8x8, int log2size) {
        assert(log2size >= 1 && log2size <= 4);
        assert(sad_store8x8_fptr_arr[log2size] != NULL);
        return sad_store8x8_fptr_arr[log2size](src1, src2, sads8x8);
    }
    inline int sad_store8x8(const uint16_t *src1, const uint16_t *src2, int *sads8x8, int log2size) {
        assert(!"not implemented");
        return 0;
    }

    inline void loopfilter(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, AV1Enc::EdgeDir dir, int filterLenIdx) {
        assert(dir == AV1Enc::VERT_EDGE || dir == AV1Enc::HORZ_EDGE);
        assert(filterLenIdx < 4);
        lpf_fptr_arr[dir][filterLenIdx](s, pitch, blimit, limit, thresh);
    }

    inline void loopfilter(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, AV1Enc::EdgeDir dir, int filterLenIdx) {
        assert(dir == AV1Enc::VERT_EDGE || dir == AV1Enc::HORZ_EDGE);
        assert(filterLenIdx < 4);
        lpf_hbd_fptr_arr[dir][filterLenIdx](s, pitch, blimit, limit, thresh);
    }

    inline int diff_dc(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2, int size) {
        return diff_dc_fptr(src1, pitch1, src2, pitch2, size);
    }
    inline int diff_dc(const uint16_t *src1, int pitch1, const uint16_t *src2, int pitch2, int size) {
        assert(!"not implemented");
        return 0;
    }

    inline void adds_nv12(uint8_t *dst, int pitchDst, const uint8_t *src1, int pitchSrc1, const int16_t *src2u, const int16_t *src2v, int pitchSrc2, int size) {
        adds_nv12_fptr(dst, pitchDst, src1, pitchSrc1, src2u, src2v, pitchSrc2, size);
    }
    inline void adds_nv12(uint16_t *dst, int pitchDst, const uint16_t *src1, int pitchSrc1, const int16_t *src2u, const int16_t *src2v, int pitchSrc2, int size) {
        //assert(!"not implemented");
        adds_nv12_hbd_fptr(dst, pitchDst, src1, pitchSrc1, src2u, src2v, pitchSrc2, size);
    }

    inline void compute_rscs(const uint8_t *src, int pitchSrc, int *lcuRs, int *lcuCs, int pitchRsCs, int width, int height) {
        compute_rscs_fptr(src, pitchSrc, lcuRs, lcuCs, pitchRsCs, width, height);
    }
    inline void compute_rscs(const uint16_t *src, int pitchSrc, int *lcuRs, int *lcuCs, int pitchRsCs, int width, int height) {
        assert(!"not implemented");
    }

    inline void compute_rscs_4x4(const uint8_t* pSrc, int srcPitch, int wblocks, int hblocks, float *pRs, float *pCs) {
        compute_rscs_4x4_fptr(pSrc, srcPitch, wblocks, hblocks, pRs, pCs);
    }

    inline void compute_rscs_diff(float *pRs0, float *pCs0, float *pRs1, float *pCs1, int len, float *pRsDiff, float *pCsDiff) {
        compute_rscs_diff_fptr(pRs0, pCs0, pRs1, pCs1, len, pRsDiff, pCsDiff);
    }

    inline void search_best_block8x8(uint8_t *src, uint8_t *ref, int pitch, int xrange, int yrange, uint32_t *bestSAD, int *bestX, int *bestY) {
        search_best_block8x8_fptr(src, ref, pitch, xrange, yrange, bestSAD, bestX, bestY);
    }

    inline void image_diff_histogram(uint8_t* pSrc, uint8_t* pRef, int pitch, int width, int height, int histogram[5], long long*pSrcDC, long long *pRefDC) {
        diff_histogram_fptr(pSrc, pRef, pitch, width, height, histogram, pSrcDC, pRefDC);
    }

    inline int cdef_find_dir(const uint16_t *img, int stride, int *var, int coeff_shift) {
        return cdef_find_dir_fptr(img, stride, var, coeff_shift);
    }

    inline void cdef_filter_block(uint8_t *dst, int dstride, const uint16_t *src, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int isUv) {
        assert(isUv == 0 || isUv == 1); // 4x4 or 8x8
        return cdef_filter_block_u8_fptr_arr[isUv](dst, dstride, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
    }

    inline void cdef_filter_block(uint16_t *dst, int dstride, const uint16_t *src, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int isUv) {
        assert(isUv == 0 || isUv == 1); // Luma or Chroma Nv12
        return cdef_filter_block_u16_fptr_arr[isUv](dst, dstride, src, pri_strength, sec_strength, dir, pri_damping, sec_damping);
    }

    inline void cdef_estimate_block(const uint8_t *org, int ostride, const uint16_t *src, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int *sse, int isUv, uint8_t *dst, int dstride) {
        assert(isUv == 0 || isUv == 1); // 4x4 or 8x8
        return cdef_estimate_block_fptr_arr[isUv](org, ostride, src, pri_strength, sec_strength, dir, pri_damping, sec_damping, sse, dst, dstride);
    }

    inline void cdef_estimate_block(const uint16_t *org, int ostride, const uint16_t *src, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int *sse, int isUv, uint16_t *dst, int dstride) {
        assert(isUv == 0 || isUv == 1); // 4x4 or 8x8
        //assert(!"not implemented");
        return cdef_estimate_block_hbd_fptr_arr[isUv](org, ostride, src, pri_strength, sec_strength, dir, pri_damping, sec_damping, sse, dst, dstride);
    }

    inline void cdef_estimate_block_sec0(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int *sse, int isUv, uint8_t *dst, int dstride) {
        assert(isUv == 0 || isUv == 1); // 4x4 or 8x8
        return cdef_estimate_block_sec0_fptr_arr[isUv](org, ostride, in, pri_strength, dir, pri_damping, sse, dst, dstride);
    }

    inline void cdef_estimate_block_sec0(const uint16_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int *sse, int isUv, uint16_t *dst, int dstride) {
        assert(isUv == 0 || isUv == 1); // 4x4 or 8x8
        //assert(!"not implemented");
        return cdef_estimate_block_sec0_hbd_fptr_arr[isUv](org, ostride, in, pri_strength, dir, pri_damping, sse, dst, dstride);
    }

    inline void cdef_estimate_block_pri0(const uint8_t *org, int ostride, const uint16_t *in, int sec_damping, int *sse, int isUv) {
        assert(isUv == 0 || isUv == 1); // 4x4 or 8x8
        return cdef_estimate_block_pri0_fptr_arr[isUv](org, ostride, in, sec_damping, sse);
    }

    inline void cdef_estimate_block_all_sec(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int sec_damping, int *sse, int isUv, uint8_t **dst, int dstride) {
        assert(isUv == 0 || isUv == 1); // 4x4 or 8x8
        return cdef_estimate_block_all_sec_fptr_arr[isUv](org, ostride, in, pri_strength, dir, pri_damping, sec_damping, sse, dst, dstride);
    }
    inline void cdef_estimate_block_all_sec(const uint16_t *org, int ostride, const uint16_t *in, int pri_strength, int dir, int pri_damping, int sec_damping, int *sse, int isUv, uint16_t **dst, int dstride) {
        assert(isUv == 0 || isUv == 1); // 4x4 or 8x8
        //assert(!"not implemented");

        return cdef_estimate_block_all_sec_hbd_fptr_arr[isUv](org, ostride, in, pri_strength, dir, pri_damping, sec_damping, sse, dst, dstride);
    }

    inline void cdef_estimate_block_2pri(const uint8_t *org, int ostride, const uint16_t *in, int pri_strength0, int pri_strength1, int dir, int pri_damping, int sec_damping, int *sse, int isUv) {
        assert(isUv == 0 || isUv == 1); // 4x4 or 8x8
        return cdef_estimate_block_2pri_fptr_arr[isUv](org, ostride, in, pri_strength0, pri_strength1, dir, pri_damping, sec_damping, sse);
    }

};

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
