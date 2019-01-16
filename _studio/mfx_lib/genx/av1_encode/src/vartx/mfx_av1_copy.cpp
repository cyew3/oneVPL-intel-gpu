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

#include <limits.h> /* for INT_MAX on Linux/Android */
#include <algorithm>
#include <assert.h>
#include "mfx_av1_ctb.h"
#include "mfx_av1_tables.h"
#include "mfx_av1_enc.h"
#include "mfx_av1_quant.h"
#include "mfx_av1_dispatcher_wrappers.h"

namespace H265Enc {
    void CopyCuData(const ModeInfo *src_, int32_t srcMiCols, ModeInfo *dst_, int32_t dstMiCols, int32_t miWidth, int32_t miHeight)
    {
        assert((size_t(dst_) & 15) == 0);
        assert((size_t(src_) & 15) == 0);
        assert(sizeof(ModeInfo) == 32);

        const int32_t srcPitch = srcMiCols * sizeof(ModeInfo);
        const int32_t dstPitch = dstMiCols * sizeof(ModeInfo);
        const uint8_t *src = reinterpret_cast<const uint8_t *>(src_);
        uint8_t *dst = reinterpret_cast<uint8_t *>(dst_);

        for (int32_t y = 0; y < miHeight; y++, src += srcPitch, dst += dstPitch) {
            for (int32_t x = 0; x < miWidth; x++) {
                storea_si128(dst + 32 * x + 0,  loada_si128(src + 0));
                storea_si128(dst + 32 * x + 16, loada_si128(src + 16));
            }
        }
    }

    void CopyCoeffs(const int16_t *src_, int16_t *dst_, int32_t numCoeff)
    {
        assert((size_t(dst_) & 31) == 0);
        assert((size_t(src_) & 31) == 0);

        const __m256i *src = reinterpret_cast<const __m256i*>(src_);
        __m256i *dst = reinterpret_cast<__m256i*>(dst_);

        switch (numCoeff) {
        case 4*4:
            storea_si256(dst, loada_si256(src++));
            break;
        case 4*4*2:  // chroma coeffs 422
            storea_si256(dst++, loada_si256(src++));
            storea_si256(dst,   loada_si256(src));
            break;
        default:
            assert((numCoeff & 63) == 0);
            for (uint32_t i = 0; i < numCoeff*sizeof(int16_t); i += 128) {
                storea_si256(dst++, loada_si256(src++));
                storea_si256(dst++, loada_si256(src++));
                storea_si256(dst++, loada_si256(src++));
                storea_si256(dst++, loada_si256(src++));
            }
            break;
        }
    }

    void ZeroCoeffs(int16_t *dst_, int32_t numCoeff)
    {
        assert((size_t(dst_) & 31) == 0);

        __m256i *dst = reinterpret_cast<__m256i*>(dst_);
        __m256i kZero = _mm256_setzero_si256();

        switch (numCoeff) {
        case 4*4:
            storea_si256(dst, kZero);
            break;
        case 4*4*2:  // chroma coeffs 422
            storea_si256(dst++, kZero);
            storea_si256(dst,   kZero);
            break;
        default:
            assert((numCoeff & 63) == 0);
            for (uint32_t i = 0; i < numCoeff*sizeof(int16_t); i += 128) {
                storea_si256(dst++, kZero);
                storea_si256(dst++, kZero);
                storea_si256(dst++, kZero);
                storea_si256(dst++, kZero);
            }
            break;
        }
    }

    void CopyNxN(const uint8_t *src, int32_t pitchSrc, uint8_t *dst, int32_t pitchDst, int32_t N)
    {
        assert(pitchSrc >= N);
        assert(pitchDst >= N);
        assert(N == 4 || N == 8 || N == 16 || N == 32 || N == 64);
        assert(N == 4 && (size_t(dst) & 3) == 0 && (size_t(src) & 3) == 0
            || N == 8 && (size_t(dst) & 7) == 0 && (size_t(src) & 7) == 0
            || (size_t(dst) & 15) == 0 && (size_t(src) & 15) == 0);

        switch (N) {
        case 4:
            for (int32_t i = 0; i < 4; i++)
                storel_si32(dst + i * pitchDst, loadu_si32(src + i * pitchSrc));
            return;
        case 8:
            for (int32_t i = 0; i < 8; i++)
                storel_epi64(dst + i * pitchDst, loadl_epi64(src + i * pitchSrc));
            return;
        case 16:
            for (int32_t i = 0; i < 16; i++)
                storea_si128(dst + i * pitchDst, loada_si128(src + i * pitchSrc));
            return;
        case 32:
            for (int32_t i = 0; i < 32; i++)
                storea_si256(dst + i * pitchDst, loada_si256(src + i * pitchSrc));
            return;
        case 64:
            for (int32_t i = 0; i < 64; i++) {
                storea_si256(dst + i * pitchDst +  0, loada_si256(src + i * pitchSrc +  0));
                storea_si256(dst + i * pitchDst + 32, loada_si256(src + i * pitchSrc + 32));
            }
            return;
        }
    }

    void CopyNxN(const uint16_t *src_, int32_t pitchSrc, uint16_t *dst_, int32_t pitchDst, int32_t N)
    {
        const int32_t W = N * sizeof(uint16_t);

        const uint8_t *src = reinterpret_cast<const uint8_t *>(src_);
        uint8_t *dst = reinterpret_cast<uint8_t *>(dst_);
        pitchDst *= sizeof(uint16_t);
        pitchSrc *= sizeof(uint16_t);

        assert(pitchSrc >= W);
        assert(pitchDst >= W);
        assert(N == 4 || N == 8 || N == 16 || N == 32 || N == 64);
        assert(W == 4 && (size_t(dst) & 3) == 0 && (size_t(src) & 3) == 0
            || W == 8 && (size_t(dst) & 7) == 0 && (size_t(src) & 7) == 0
            || (size_t(dst) & 15) == 0 && (size_t(src) & 15) == 0);

        switch (W) {
        case 4:
            Copy32(src, dst); src += pitchSrc; dst += pitchDst;
            Copy32(src, dst); src += pitchSrc; dst += pitchDst;
            Copy32(src, dst); src += pitchSrc; dst += pitchDst;
            Copy32(src, dst);
            break;
        case 8:
            assert(!(N & 3));
            for (int32_t i = 0; i < N; i += 4) {
                Copy64(src, dst); src += pitchSrc; dst += pitchDst;
                Copy64(src, dst); src += pitchSrc; dst += pitchDst;
                Copy64(src, dst); src += pitchSrc; dst += pitchDst;
                Copy64(src, dst); src += pitchSrc; dst += pitchDst;
            }
            break;
        case 16:
            assert(!(N & 7));
            for (int32_t i = 0; i < N; i += 8) {
                storea_si128(dst, loada_si128(src)); src += pitchSrc; dst += pitchDst;
                storea_si128(dst, loada_si128(src)); src += pitchSrc; dst += pitchDst;
                storea_si128(dst, loada_si128(src)); src += pitchSrc; dst += pitchDst;
                storea_si128(dst, loada_si128(src)); src += pitchSrc; dst += pitchDst;
                storea_si128(dst, loada_si128(src)); src += pitchSrc; dst += pitchDst;
                storea_si128(dst, loada_si128(src)); src += pitchSrc; dst += pitchDst;
                storea_si128(dst, loada_si128(src)); src += pitchSrc; dst += pitchDst;
                storea_si128(dst, loada_si128(src)); src += pitchSrc; dst += pitchDst;
            }
            break;
        case 32:
            assert(!(N & 15));
            for (int32_t i = 0; i < N; i += 4) {
                storea_si128(dst+0,  loada_si128(src+0));
                storea_si128(dst+16, loada_si128(src+16)); src += pitchSrc; dst += pitchDst;
                storea_si128(dst+0,  loada_si128(src+0));
                storea_si128(dst+16, loada_si128(src+16)); src += pitchSrc; dst += pitchDst;
                storea_si128(dst+0,  loada_si128(src+0));
                storea_si128(dst+16, loada_si128(src+16)); src += pitchSrc; dst += pitchDst;
                storea_si128(dst+0,  loada_si128(src+0));
                storea_si128(dst+16, loada_si128(src+16)); src += pitchSrc; dst += pitchDst;
            }
            break;
        case 64:
            assert(!(N & 31));
            for (int32_t i = 0; i < N; i += 2) {
                storea_si128(dst+0,  loada_si128(src+0));
                storea_si128(dst+16, loada_si128(src+16));
                storea_si128(dst+32, loada_si128(src+32));
                storea_si128(dst+48, loada_si128(src+48)); src += pitchSrc; dst += pitchDst;
                storea_si128(dst+0,  loada_si128(src+0));
                storea_si128(dst+16, loada_si128(src+16));
                storea_si128(dst+32, loada_si128(src+32));
                storea_si128(dst+48, loada_si128(src+48)); src += pitchSrc; dst += pitchDst;
            }
            break;
        case 128:
            assert(!(N & 63));
            for (int32_t i = 0; i < N; i++, src += pitchSrc, dst += pitchDst) {
                storea_si128(dst+0,   loada_si128(src+0));
                storea_si128(dst+16,  loada_si128(src+16));
                storea_si128(dst+32,  loada_si128(src+32));
                storea_si128(dst+48,  loada_si128(src+48));
                storea_si128(dst+64,  loada_si128(src+64));
                storea_si128(dst+80,  loada_si128(src+80));
                storea_si128(dst+96,  loada_si128(src+96));
                storea_si128(dst+112, loada_si128(src+112));
            }
            break;
        default:
            assert(0);
        }
    }


    void CopyNxN(const uint8_t *src, int32_t pitchSrc, uint8_t *dst, int32_t N)
    {
        assert(pitchSrc >= N);
        assert(N == 4 || N == 8 || N == 16 || N == 32 || N == 64);
        assert(N == 4 && (size_t(dst) & 3) == 0 && (size_t(src) & 3) == 0
            || N == 8 && (size_t(dst) & 7) == 0 && (size_t(src) & 7) == 0
            || (size_t(dst) & 15) == 0 && (size_t(src) & 15) == 0);

        switch (N) {
        case 4:
            storea_si128(dst,
                _mm_unpacklo_epi64(
                    _mm_unpacklo_epi32(loadu_si32(src + pitchSrc * 0), loadu_si32(src + pitchSrc * 1)),
                    _mm_unpacklo_epi32(loadu_si32(src + pitchSrc * 2), loadu_si32(src + pitchSrc * 3))));
            return;
        case 8:
            storea_si128(dst + 0,  loadu2_epi64(src + pitchSrc * 0, src + pitchSrc * 1));
            storea_si128(dst + 16, loadu2_epi64(src + pitchSrc * 2, src + pitchSrc * 3));
            src += pitchSrc * 4;
            storea_si128(dst + 32, loadu2_epi64(src + pitchSrc * 0, src + pitchSrc * 1));
            storea_si128(dst + 48, loadu2_epi64(src + pitchSrc * 2, src + pitchSrc * 3));
            return;
        case 16:
            for (int32_t i = 0; i < 16; i++)
                storea_si128(dst + i * 16, loada_si128(src + i * pitchSrc));
            return;
        case 32:
            for (int32_t i = 0; i < 32; i++)
                storea_si256(dst + i * 32, loada_si256(src + i * pitchSrc));
            break;
        case 64:
            for (int32_t i = 0; i < 64; i++) {
                storea_si256(dst + i * 64 +  0, loada_si256(src + i * pitchSrc +  0));
                storea_si256(dst + i * 64 + 32, loada_si256(src + i * pitchSrc + 32));
            }
            break;
        default:
            assert(0);
        }
    }

    void CopyNxN(const uint16_t *src, int32_t pitchSrc, uint16_t *dst, int32_t N) {
        CopyNxN(src, pitchSrc, dst, N, N);
    }


    void CopyNxM(const uint8_t *src, int32_t pitchSrc, uint8_t *dst, int32_t pitchDst, int32_t N, int32_t M)
    {
        assert(pitchSrc >= N);
        assert(pitchDst >= N);
        assert(N == 4 || N == 8 || N == 16 || N == 32 || N == 64);
        assert(M == 4 || M == 8 || M == 16 || M == 32 || M == 64);
        assert(N == 4 && (size_t(dst)&3) == 0  && (pitchDst&3) == 0  && (size_t(src)&3) == 0  && (pitchSrc&3) == 0
            || N == 8 && (size_t(dst)&7) == 0  && (pitchDst&7) == 0  && (size_t(src)&7) == 0  && (pitchSrc&7) == 0
            ||           (size_t(dst)&15) == 0 && (pitchDst&15) == 0 && (size_t(src)&15) == 0 && (pitchSrc&15) == 0);

        switch (N) {
        case 4:
            assert((M & 3) == 0);
            for (int32_t i = 0; i < M; i += 4, src += pitchSrc * 4, dst += pitchDst * 4) {
                Copy32(src + pitchSrc * 0, dst + pitchDst * 0);
                Copy32(src + pitchSrc * 1, dst + pitchDst * 1);
                Copy32(src + pitchSrc * 2, dst + pitchDst * 2);
                Copy32(src + pitchSrc * 3, dst + pitchDst * 3);
            }
            break;
        case 8:
            assert((M & 3) == 0);
            for (int32_t i = 0; i < M; i += 4, src += pitchSrc * 4, dst += pitchDst * 4) {
                Copy64(src + pitchSrc * 0, dst + pitchDst * 0);
                Copy64(src + pitchSrc * 1, dst + pitchDst * 1);
                Copy64(src + pitchSrc * 2, dst + pitchDst * 2);
                Copy64(src + pitchSrc * 3, dst + pitchDst * 3);
            }
            break;
        case 16:
            assert((M & 3) == 0);
            for (int32_t i = 0; i < M; i += 4, src += pitchSrc * 4, dst += pitchDst * 4) {
                storea_si128(dst + pitchDst * 0, loada_si128(src + pitchSrc * 0));
                storea_si128(dst + pitchDst * 1, loada_si128(src + pitchSrc * 1));
                storea_si128(dst + pitchDst * 2, loada_si128(src + pitchSrc * 2));
                storea_si128(dst + pitchDst * 3, loada_si128(src + pitchSrc * 3));
            }
            break;
        case 32:
            assert((M & 3) == 0);
            for (int32_t i = 0; i < M; i += 4, src += pitchSrc * 4, dst += pitchDst * 4) {
                storea_si128(dst + pitchDst * 0,      loada_si128(src + pitchSrc * 0));
                storea_si128(dst + pitchDst * 0 + 16, loada_si128(src + pitchSrc * 0 + 16));
                storea_si128(dst + pitchDst * 1,      loada_si128(src + pitchSrc * 1));
                storea_si128(dst + pitchDst * 1 + 16, loada_si128(src + pitchSrc * 1 + 16));
                storea_si128(dst + pitchDst * 2,      loada_si128(src + pitchSrc * 2));
                storea_si128(dst + pitchDst * 2 + 16, loada_si128(src + pitchSrc * 2 + 16));
                storea_si128(dst + pitchDst * 3,      loada_si128(src + pitchSrc * 3));
                storea_si128(dst + pitchDst * 3 + 16, loada_si128(src + pitchSrc * 3 + 16));
            }
            break;
        case 64:
            assert((M & 1) == 0);
            for (int32_t i = 0; i < M; i += 2, src += pitchSrc * 2, dst += pitchDst * 2) {
                storea_si128(dst + pitchDst * 0,      loada_si128(src + pitchSrc * 0));
                storea_si128(dst + pitchDst * 0 + 16, loada_si128(src + pitchSrc * 0 + 16));
                storea_si128(dst + pitchDst * 0 + 32, loada_si128(src + pitchSrc * 0 + 32));
                storea_si128(dst + pitchDst * 0 + 48, loada_si128(src + pitchSrc * 0 + 48));
                storea_si128(dst + pitchDst * 1,      loada_si128(src + pitchSrc * 1));
                storea_si128(dst + pitchDst * 1 + 16, loada_si128(src + pitchSrc * 1 + 16));
                storea_si128(dst + pitchDst * 1 + 32, loada_si128(src + pitchSrc * 1 + 32));
                storea_si128(dst + pitchDst * 1 + 48, loada_si128(src + pitchSrc * 1 + 48));
            }
            break;
        default:
            assert(!"invalid width");
        }
    }

    void CopyNxM_unaligned_src(const uint8_t *src, int32_t pitchSrc, uint8_t *dst, int32_t pitchDst, int32_t N, int32_t M)
    {
        assert(pitchSrc >= N);
        assert(pitchDst >= N);
        assert(N == 4 || N == 8 || N == 16 || N == 32 || N == 64);
        assert(M == 4 || M == 8 || M == 16 || M == 32 || M == 64);
        assert(N == 4 && (size_t(dst) & 3) == 0 && (pitchDst & 3) == 0
            || N == 8 && (size_t(dst) & 7) == 0 && (pitchDst & 7) == 0
            || (size_t(dst) & 15) == 0 && (pitchDst & 15) == 0);

        switch (N) {
        case 4:
            assert((M & 3) == 0);
            for (int y = 0; y < M; y += 4) {
                *(uint32_t *)(dst + 0 * pitchDst) = *(const uint32_t *)(src + 0 * pitchSrc);
                *(uint32_t *)(dst + 1 * pitchDst) = *(const uint32_t *)(src + 1 * pitchSrc);
                *(uint32_t *)(dst + 2 * pitchDst) = *(const uint32_t *)(src + 2 * pitchSrc);
                *(uint32_t *)(dst + 3 * pitchDst) = *(const uint32_t *)(src + 3 * pitchSrc);
                src += 4 * pitchSrc;
                dst += 4 * pitchDst;
            }
            break;
        case 8:
            assert((M & 3) == 0);
            for (int y = 0; y < M; y += 4) {
                *(uint64_t *)(dst + 0 * pitchDst) = *(const uint64_t *)(src + 0 * pitchSrc);
                *(uint64_t *)(dst + 1 * pitchDst) = *(const uint64_t *)(src + 1 * pitchSrc);
                *(uint64_t *)(dst + 2 * pitchDst) = *(const uint64_t *)(src + 2 * pitchSrc);
                *(uint64_t *)(dst + 3 * pitchDst) = *(const uint64_t *)(src + 3 * pitchSrc);
                src += 4 * pitchSrc;
                dst += 4 * pitchDst;
            }
            break;
        case 16:
            assert((M & 3) == 0);
            for (int32_t i = 0; i < M; i += 4, src += pitchSrc * 4, dst += pitchDst * 4) {
                storea_si128(dst + pitchDst * 0, loadu_si128(src + pitchSrc * 0));
                storea_si128(dst + pitchDst * 1, loadu_si128(src + pitchSrc * 1));
                storea_si128(dst + pitchDst * 2, loadu_si128(src + pitchSrc * 2));
                storea_si128(dst + pitchDst * 3, loadu_si128(src + pitchSrc * 3));
            }
            break;
        case 32:
            assert((M & 3) == 0);
            for (int32_t i = 0; i < M; i += 4, src += pitchSrc * 4, dst += pitchDst * 4) {
                storea_si128(dst + pitchDst * 0, loadu_si128(src + pitchSrc * 0));
                storea_si128(dst + pitchDst * 0 + 16, loadu_si128(src + pitchSrc * 0 + 16));
                storea_si128(dst + pitchDst * 1, loadu_si128(src + pitchSrc * 1));
                storea_si128(dst + pitchDst * 1 + 16, loadu_si128(src + pitchSrc * 1 + 16));
                storea_si128(dst + pitchDst * 2, loadu_si128(src + pitchSrc * 2));
                storea_si128(dst + pitchDst * 2 + 16, loadu_si128(src + pitchSrc * 2 + 16));
                storea_si128(dst + pitchDst * 3, loadu_si128(src + pitchSrc * 3));
                storea_si128(dst + pitchDst * 3 + 16, loadu_si128(src + pitchSrc * 3 + 16));
            }
            break;
        case 64:
            assert((M & 1) == 0);
            for (int32_t i = 0; i < M; i += 2, src += pitchSrc * 2, dst += pitchDst * 2) {
                storea_si128(dst + pitchDst * 0, loadu_si128(src + pitchSrc * 0));
                storea_si128(dst + pitchDst * 0 + 16, loadu_si128(src + pitchSrc * 0 + 16));
                storea_si128(dst + pitchDst * 0 + 32, loadu_si128(src + pitchSrc * 0 + 32));
                storea_si128(dst + pitchDst * 0 + 48, loadu_si128(src + pitchSrc * 0 + 48));
                storea_si128(dst + pitchDst * 1, loadu_si128(src + pitchSrc * 1));
                storea_si128(dst + pitchDst * 1 + 16, loadu_si128(src + pitchSrc * 1 + 16));
                storea_si128(dst + pitchDst * 1 + 32, loadu_si128(src + pitchSrc * 1 + 32));
                storea_si128(dst + pitchDst * 1 + 48, loadu_si128(src + pitchSrc * 1 + 48));
            }
            break;
        default:
            assert(!"invalid width");
        }
    }

    void CopyNxM_unaligned_src(const uint16_t *src, int32_t pitchSrc, uint16_t *dst, int32_t pitchDst, int32_t N, int32_t M) {
        assert(!"not implemented");
    }

    void CopyNxM(const uint16_t *src_, int32_t pitchSrc, uint16_t *dst_, int32_t pitchDst, int32_t N, int32_t M)
    {
        const int32_t W = N * sizeof(uint16_t);
        const int32_t H = M;
        const uint8_t *src = reinterpret_cast<const uint8_t*>(src_);
        uint8_t *dst = reinterpret_cast<uint8_t*>(dst_);
        pitchDst *= sizeof(uint16_t);
        pitchSrc *= sizeof(uint16_t);
        assert(pitchSrc >= W);
        assert(pitchDst >= W);
        assert(N == 4 || N == 8 || N == 16 || N == 32 || N == 64);
        assert(M == 4 || M == 8 || M == 16 || M == 32 || M == 64);
        assert(W == 4 && (size_t(dst)&3) == 0  && (pitchDst&3) == 0  && (size_t(src)&3) == 0  && (pitchSrc&3) == 0
            || W == 8 && (size_t(dst)&7) == 0  && (pitchDst&7) == 0  && (size_t(src)&7) == 0  && (pitchSrc&7) == 0
            ||           (size_t(dst)&15) == 0 && (pitchDst&15) == 0 && (size_t(src)&15) == 0 && (pitchSrc&15) == 0);

        switch (W) {
        case 4:
            assert((H&3) == 0);
            for (int32_t i = 0; i < H; i += 4) {
                Copy32(src, dst); src += pitchSrc; dst += pitchDst;
                Copy32(src, dst); src += pitchSrc; dst += pitchDst;
                Copy32(src, dst); src += pitchSrc; dst += pitchDst;
                Copy32(src, dst); src += pitchSrc; dst += pitchDst;
            }
            break;
        case 8:
            assert((H&3) == 0);
            for (int32_t i = 0; i < H; i += 4) {
                Copy64(src, dst); src += pitchSrc; dst += pitchDst;
                Copy64(src, dst); src += pitchSrc; dst += pitchDst;
                Copy64(src, dst); src += pitchSrc; dst += pitchDst;
                Copy64(src, dst); src += pitchSrc; dst += pitchDst;
            }
            break;
        case 16:
            assert((H&3) == 0);
            for (int32_t i = 0; i < H; i += 4) {
                storea_si128(dst, loada_si128(src)); src += pitchSrc; dst += pitchDst;
                storea_si128(dst, loada_si128(src)); src += pitchSrc; dst += pitchDst;
                storea_si128(dst, loada_si128(src)); src += pitchSrc; dst += pitchDst;
                storea_si128(dst, loada_si128(src)); src += pitchSrc; dst += pitchDst;
            }
            break;
        case 32:
            assert((H&3) == 0);
            for (int32_t i = 0; i < H; i += 4) {
                storea_si128(dst,    loada_si128(src));
                storea_si128(dst+16, loada_si128(src+16)); src += pitchSrc; dst += pitchDst;
                storea_si128(dst,    loada_si128(src));
                storea_si128(dst+16, loada_si128(src+16)); src += pitchSrc; dst += pitchDst;
                storea_si128(dst,    loada_si128(src));
                storea_si128(dst+16, loada_si128(src+16)); src += pitchSrc; dst += pitchDst;
                storea_si128(dst,    loada_si128(src));
                storea_si128(dst+16, loada_si128(src+16)); src += pitchSrc; dst += pitchDst;
            }
            break;
        case 64:
            assert((H&1) == 0);
            for (int32_t i = 0; i < H; i += 2) {
                storea_si128(dst,    loada_si128(src));
                storea_si128(dst+16, loada_si128(src+16));
                storea_si128(dst+32, loada_si128(src+32));
                storea_si128(dst+48, loada_si128(src+48)); src += pitchSrc; dst += pitchDst;
                storea_si128(dst,    loada_si128(src));
                storea_si128(dst+16, loada_si128(src+16));
                storea_si128(dst+32, loada_si128(src+32));
                storea_si128(dst+48, loada_si128(src+48)); src += pitchSrc; dst += pitchDst;
            }
            break;
        case 128:
            for (int32_t i = 0; i < H; i++, src += pitchSrc, dst += pitchDst) {
                storea_si128(dst,     loada_si128(src));
                storea_si128(dst+16,  loada_si128(src+16));
                storea_si128(dst+32,  loada_si128(src+32));
                storea_si128(dst+48,  loada_si128(src+48));
                storea_si128(dst+64,  loada_si128(src+64));
                storea_si128(dst+80,  loada_si128(src+80));
                storea_si128(dst+96,  loada_si128(src+96));
                storea_si128(dst+112, loada_si128(src+112));
            }
            break;
        default:
            assert(!"invalid width");
        }
    }


    void CopyNxM(const uint8_t *src, int32_t pitchSrc, uint8_t *dst, int32_t N, int32_t M) {
        assert(pitchSrc >= N);
        assert(N == 4 || N == 8 || N == 16 || N == 32 || N == 64);
        assert(M == 4 || M == 8 || M == 16 || M == 32 || M == 64);
        assert(N == 4 && (size_t(dst)&3) == 0  && (size_t(src)&3) == 0  && (pitchSrc&3) == 0
            || N == 8 && (size_t(dst)&7) == 0  && (size_t(src)&7) == 0  && (pitchSrc&7) == 0
            ||           (size_t(dst)&15) == 0 && (size_t(src)&15) == 0 && (pitchSrc&15) == 0);

        switch (N) {
        case 4:
            assert((M & 3) == 0);
            for (int32_t i = 0; i < M; i += 4, src += pitchSrc * 4, dst += 16)
                storea_si128(dst,
                    _mm_unpacklo_epi64(
                        _mm_unpacklo_epi32(loadu_si32(src + pitchSrc * 0), loadu_si32(src + pitchSrc * 1)),
                        _mm_unpacklo_epi32(loadu_si32(src + pitchSrc * 2), loadu_si32(src + pitchSrc * 3))));
            return;
        case 8:
            assert((M & 3) == 0);
            for (int32_t i = 0; i < M; i += 4, src += pitchSrc * 4, dst += 32) {
                storea_si128(dst + 0,  loadu2_epi64(src + pitchSrc * 0, src + pitchSrc * 1));
                storea_si128(dst + 16, loadu2_epi64(src + pitchSrc * 2, src + pitchSrc * 3));
            }
            return;
        case 16:
            assert((M & 3) == 0);
            for (int32_t i = 0; i < M; i += 4, src += pitchSrc * 4, dst += 64) {
                storea_si128(dst + 0,  loada_si128(src + pitchSrc * 0));
                storea_si128(dst + 16, loada_si128(src + pitchSrc * 1));
                storea_si128(dst + 32, loada_si128(src + pitchSrc * 2));
                storea_si128(dst + 48, loada_si128(src + pitchSrc * 3));
            }
            return;
        case 32:
            for (int32_t i = 0; i < M; i++)
                storea_si256(dst + i * 32, loada_si256(src + i * pitchSrc));
            return;
        case 64:
            for (int32_t i = 0; i < M; i++) {
                storea_si256(dst + i * 64 +  0, loada_si256(src + i * pitchSrc +  0));
                storea_si256(dst + i * 64 + 32, loada_si256(src + i * pitchSrc + 32));
            }
            return;
        }
    }


    void CopyNxM(const uint16_t *src, int32_t pitchSrc, uint16_t *dst, int32_t N, int32_t M) {
        CopyNxM(src, pitchSrc, dst, N, N, M);
    }


    void CopyNxN(const int16_t *src_, int32_t pitchSrc, int16_t *dst_, int32_t pitchDst, int32_t N) {
        CopyNxN(reinterpret_cast<const int16_t*>(src_), pitchSrc, reinterpret_cast<int16_t*>(dst_), pitchDst, N);
    }


    void Copy16to16(const uint16_t *src, int32_t pitchSrc, uint16_t *dst, int32_t pitchDst, int32_t width, int32_t height) {
        assert(size_t(src) % 16 == 0);
        assert(size_t(dst) % 16 == 0);
        assert(size_t(pitchSrc) * sizeof(*src) % 16 == 0);
        assert(size_t(pitchDst) * sizeof(*dst) % 16 == 0);
        assert(width % 8 == 0);
        for (int32_t y = 0; y < height; y++) {
            for (int32_t i = 0; i < width; i += 8, src += 8, dst += 8)
                storea_si128(dst, loada_si128(src));
            src += pitchSrc - width;
            dst += pitchDst - width;
        }
    }


    namespace details {
        template <int N> H265_FORCEINLINE void Unpack16(const uint8_t *src, uint16_t *dst) {
            for (int i = 0; i < N; i++, src += 16, dst += 16) {
                __m128i s = loada_si128(src);
                storea_si128(dst + 0, _mm_unpacklo_epi8(s, _mm_setzero_si128()));
                storea_si128(dst + 8, _mm_unpackhi_epi8(s, _mm_setzero_si128()));
            }
        }
    };  // namespace details

    void Copy8To16(const uint8_t *src, int32_t pitchSrc, uint16_t *dst, int32_t pitchDst, int32_t width, int32_t height) {
        assert(size_t(src) % 8 == 0);
        assert(size_t(dst) % 16 == 0);
        assert(width <= 80);
        assert(width % 8 == 0); // Luma width is 8 byte aligned
        for (int32_t y = 0; y < height; y++) {
            int w = width;
            if (size_t(src) & 8) {
                storea_si128(dst, _mm_unpacklo_epi8(loadl_epi64(src), _mm_setzero_si128()));
                src += 8;
                dst += 8;
                w -= 8;
            }
            if (w & 64) {
                details::Unpack16<4>(src, dst);
                src += 64;
                dst += 64;
            }
            if (w & 32) {
                details::Unpack16<2>(src, dst);
                src += 32;
                dst += 32;
            }
            if (w & 16) {
                details::Unpack16<1>(src, dst);
                src += 16;
                dst += 16;
            }
            if (w & 8) {
                __m128i s = loadl_epi64(src);
                storea_si128(dst, _mm_unpacklo_epi8(s, _mm_setzero_si128()));
                src += 8;
                dst += 8;
            }
            src += pitchSrc - width;
            dst += pitchDst - width;
        }
    }

    void Copy8To16_unaligned(const uint8_t *src, int32_t pitchSrc, uint16_t *dst, int32_t pitchDst, int32_t width, int32_t height) {
        assert(size_t(src) % 8 == 0);
        assert(size_t(dst) % 8 == 0);
        assert(width % 4 == 0);
        int32_t width16 = width & ~15;
        if ((size_t(src) & 15) | (pitchSrc & 15)) {
            for (int32_t y = 0; y < height; y++) {
                for (int32_t i = 0; i < width16; i += 16, src += 16, dst += 16) {
                    __m128i s = loadu_si128(src);
                    storea_si128(dst + 0, _mm_unpacklo_epi8(s, _mm_setzero_si128()));
                    storea_si128(dst + 8, _mm_unpackhi_epi8(s, _mm_setzero_si128()));
                }
                if (width & 8) {
                    __m128i s = loadl_epi64(src);
                    storea_si128(dst, _mm_unpacklo_epi8(s, _mm_setzero_si128()));
                    src += 8;
                    dst += 8;
                }
                src += pitchSrc - width;
                dst += pitchDst - width;
            }
        } else {
            for (int32_t y = 0; y < height; y++) {
                for (int32_t i = 0; i < width16; i += 16, src += 16, dst += 16)
                    details::Unpack16<1>(src, dst);
                if (width & 8) {
                    __m128i s = loadl_epi64(src);
                    storea_si128(dst, _mm_unpacklo_epi8(s, _mm_setzero_si128()));
                    src += 8;
                    dst += 8;
                }
                src += pitchSrc - width;
                dst += pitchDst - width;
            }
        }
    }

}  // namespace H265Enc
