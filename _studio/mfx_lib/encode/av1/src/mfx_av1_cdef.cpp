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
#include "mfx_av1_cdef.h"
#include "mfx_av1_copy.h"

#include <algorithm>

#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

using namespace AV1Enc;

namespace AV1Enc {

    static const int32_t BORDERS_OFFSET = CDEF_VBORDER * CDEF_BSTRIDE + CDEF_HBORDER;
    static const uint16_t  XL1 = CDEF_VERY_LARGE;
    static const uint32_t  XL2 = XL1 | (XL1 << 16);
    static const uint64_t  XL4 = uint64_t(XL2) | (uint64_t(XL2) << 32);
    static const __m128i XL8 = _mm_set1_epi16(CDEF_VERY_LARGE);



    /* Compute the primary filter strength for an 8x8 block based on the
    directional variance difference. A high variance difference means
    that we have a highly directional pattern (e.g. a high contrast
    edge), so we can apply more deringing. A low variance means that we
    either have a low contrast edge, or a non-directional texture, so
    we want to be careful not to blur. */
    static int32_t adjust_strength(int32_t strength, int32_t var)
    {
        const int32_t i = (var >> 6) ? std::min(BSR(var >> 6), 12) : 0;
        /* We use the variance of 8x8 blocks to adjust the strength. */
        return var ? (strength * (4 + i) + 8) >> 4 : 0;
    }

    template <typename PixType>
    CdefLineBuffers<PixType>::CdefLineBuffers()
    {
    }
    template CdefLineBuffers<uint8_t>::CdefLineBuffers();
    template CdefLineBuffers<uint16_t>::CdefLineBuffers();

    template <typename PixType>
    CdefLineBuffers<PixType>::~CdefLineBuffers()
    {
        Free();
    }
    template CdefLineBuffers<uint8_t>::~CdefLineBuffers();
    template CdefLineBuffers<uint16_t>::~CdefLineBuffers();

    template <typename PixType>
    void CdefLineBuffers<PixType>::Alloc(const AV1VideoParam &par)
    {
        int32_t frameWidth = par.Width;
        pitchAbove = ((frameWidth + 63) & ~63) + 2 * CDEF_HBORDER;

        aboveBorder = (PixType *)AV1_Malloc(2*pitchAbove * sizeof(PixType) * par.sb64Rows*2);// both Y&UV
        ThrowIf(!aboveBorder, std::bad_alloc());
        aboveBorderY = aboveBorder + CDEF_HBORDER;
        aboveBorderUv = aboveBorderY + pitchAbove * par.sb64Rows*2;

        pitchLeft = 128 * par.sb64Cols;
        leftBorder = (PixType *)AV1_Malloc(sizeof(PixType) * par.sb64Rows * par.sb64Cols * 64 * 2 * 2);
        ThrowIf(!leftBorder, std::bad_alloc());
        leftBorderUv = leftBorder + par.sb64Rows * pitchLeft;
    }
    template void CdefLineBuffers<uint8_t>::Alloc(const AV1VideoParam &par);
    template void CdefLineBuffers<uint16_t>::Alloc(const AV1VideoParam &par);

    template <typename PixType>
    void CdefLineBuffers<PixType>::Free()
    {
        AV1_SafeFree(aboveBorder);
        AV1_SafeFree(leftBorder);
    }
    template void CdefLineBuffers<uint8_t>::Free();
    template void CdefLineBuffers<uint16_t>::Free();

    template <typename T>
    void SetBorder(T *p, int32_t pitch, T value, int32_t w, int32_t h)
    {
        p -= CDEF_VBORDER * pitch;
        for (int32_t i = 0; i < CDEF_VBORDER; i++, p += pitch) {
            p[-2] = value;
            p[-1] = value;
            for (int32_t j = 0; j < w; j++)
                p[j] = value;
            p[w + 0] = value;
            p[w + 1] = value;
        }
    }

    static inline void Convert(const uint16_t *src, uint16_t *dst, int len)
    {
        for (int i = 0; i < len; i++)
            dst[i] = src[i];
    }

    static inline void Convert4(const uint8_t *src, uint16_t *dst)
    {
        storel_epi64(dst, _mm_unpacklo_epi8(loadu_si32(src), _mm_setzero_si128()));
    }
    static inline void Convert4(const uint16_t *src, uint16_t *dst)
    {
        Convert(src, dst, 4);
    }

    static inline void Convert16(const uint8_t *src, uint16_t *dst)
    {
        __m128i s0 = loada_si128(src);
        storea_si128(dst + 0, _mm_unpacklo_epi8(s0, _mm_setzero_si128()));
        storea_si128(dst + 8, _mm_unpackhi_epi8(s0, _mm_setzero_si128()));
    }
    static inline void Convert16(const uint16_t *src, uint16_t *dst)
    {
        Convert(src, dst, 16);
    }

    static inline void Convert64(const uint8_t *src, uint16_t *dst)
    {
        Convert16(src + 0,  dst + 0);
        Convert16(src + 16, dst + 16);
        Convert16(src + 32, dst + 32);
        Convert16(src + 48, dst + 48);
    }
    static inline void Convert64(const uint16_t *src, uint16_t *dst)
    {
        Convert(src, dst, 64);
    }

    static inline void Convert16Unaligned(const uint8_t *src, uint16_t *dst)
    {
        __m128i s0 = loadu_si128(src);
        storeu_si128(dst + 0, _mm_unpacklo_epi8(s0, _mm_setzero_si128()));
        storeu_si128(dst + 8, _mm_unpackhi_epi8(s0, _mm_setzero_si128()));
    }
    static inline void Convert16Unaligned(const uint16_t *src, uint16_t *dst)
    {
        Convert(src, dst, 16);
    }

    static inline void Convert64Unaligned(const uint8_t *src, uint16_t *dst)
    {
        Convert16Unaligned(src + 0,  dst + 0);
        Convert16Unaligned(src + 16, dst + 16);
        Convert16Unaligned(src + 32, dst + 32);
        Convert16Unaligned(src + 48, dst + 48);
    }
    static inline void Convert64Unaligned(const uint16_t *src, uint16_t *dst)
    {
        Convert(src, dst, 64);
    }

    template <typename PixType>
    const uint16_t *CdefLineBuffers<PixType>::PrepareSb(const int sbr, const int sbc, FrameData& frame, uint16_t* in0, uint16_t* in1)
    {
        const int32_t frameWidth  = frame.width;
        const int32_t frameHeight = frame.height;
        const uint16_t veryLarge4[] = {CDEF_VERY_LARGE, CDEF_VERY_LARGE, CDEF_VERY_LARGE, CDEF_VERY_LARGE};

        // copy above-left, above and above-right
        if (sbr > 0) {
            uint16_t *dst0 = in0 + BORDERS_OFFSET - (CDEF_VBORDER * CDEF_BSTRIDE);
            uint16_t *dst1 = in1 + BORDERS_OFFSET - (CDEF_VBORDER * CDEF_BSTRIDE);

            const PixType* above0 = aboveBorderY  + pitchAbove*(sbr)*2 + (sbc)*64;
            const PixType* above1 = aboveBorderUv + pitchAbove*(sbr)*2 + (sbc)*64;

            for (int32_t i = 0; i < 2; i++) {
                if (sbc > 0) {
                    Convert4(above0-4, dst0-4);
                } else {
                    *(uint32_t *)(dst0 - 2) = XL2;
                }

                Convert64Unaligned(above0, dst0);

                //Copy32(src0 + CDEF_BLOCKSIZE, dst0 + CDEF_BLOCKSIZE);
                if (sbc + 1 == ((frameWidth + 63) >> 6)) {
                    *(uint32_t *)(dst0 + CDEF_BLOCKSIZE) = XL2;
                } else {
                    //dst0[CDEF_BLOCKSIZE+0] = (uint16_t)above0[CDEF_BLOCKSIZE+0];
                    //dst0[CDEF_BLOCKSIZE+1] = (uint16_t)above0[CDEF_BLOCKSIZE+1];
                    Convert4(above0 + CDEF_BLOCKSIZE, dst0 + CDEF_BLOCKSIZE);
                }

                // copy chroma above-left, above and above-right
                if (sbc > 0) {
                    Convert4(above1 - 4, dst1 - 4);
                } else {
                    //dst1[-4] = dst1[-3] = dst1[-2] = dst1[-1] = CDEF_VERY_LARGE;
                     *(uint64_t *)(dst1 - 4) = XL4;
                }
                Convert64Unaligned(above1, dst1);

                //Copy64(src1 + CDEF_BLOCKSIZE, dst1 + CDEF_BLOCKSIZE);
                if (sbc + 1 == ((frameWidth + 63) >> 6)) {
                    /*dst1[CDEF_BLOCKSIZE+0] = CDEF_VERY_LARGE;
                    dst1[CDEF_BLOCKSIZE+1] = CDEF_VERY_LARGE;
                    dst1[CDEF_BLOCKSIZE+2] = CDEF_VERY_LARGE;
                    dst1[CDEF_BLOCKSIZE+3] = CDEF_VERY_LARGE;*/
                    *(uint64_t *)(dst1 + CDEF_BLOCKSIZE) = XL4;
                } else {
                    Convert4(above1 + CDEF_BLOCKSIZE, dst1 + CDEF_BLOCKSIZE);
                }

                // upd
               dst0 += CDEF_BSTRIDE;
               dst1 += CDEF_BSTRIDE;
               above0 += pitchAbove;
               above1 += pitchAbove;
            }
        } else {
            SetBorder<uint16_t>(in0 + CDEF_HBORDER + CDEF_VBORDER * CDEF_BSTRIDE, CDEF_BSTRIDE, CDEF_VERY_LARGE, CDEF_BLOCKSIZE, CDEF_BLOCKSIZE);
            SetBorder<uint32_t>((uint32_t *)(in1 + CDEF_HBORDER + CDEF_VBORDER * CDEF_BSTRIDE), CDEF_BSTRIDE >> 1, CDEF_VERY_LARGE_CHROMA, CDEF_BLOCKSIZE >> 1, CDEF_BLOCKSIZE >> 1);
        }

        // copy left and bottom-left pixels
        if (sbc > 0) {
            uint16_t *dst0 = in0 + BORDERS_OFFSET - CDEF_BORDER;
            uint16_t *dst1 = in1 + BORDERS_OFFSET - CDEF_BORDER * 2;

            const int32_t pitchDstR0 = pitchLeft;
            const PixType* srcR0 = leftBorder + sbr*pitchDstR0 + (sbc)*128;
            for (int32_t i = 0; i < CDEF_BLOCKSIZE; i++, dst0 += CDEF_BSTRIDE, srcR0 += 2) {
                dst0[0] = (uint16_t)srcR0[0];
                dst0[1] = (uint16_t)srcR0[1];
                //Convert4(srcR0 - 2, dst0 - 2);
            }

            const int32_t pitchDstR1 = pitchLeft;
            const PixType* srcR1 = leftBorderUv + sbr*pitchDstR1 + (sbc)*128;
            for (int32_t i = 0; i < CDEF_BLOCKSIZE / 2; i++, dst1 += CDEF_BSTRIDE, srcR1 += 4) {
                /*dst1[0] = (uint16_t)srcR1[0];
                dst1[1] = (uint16_t)srcR1[1];
                dst1[2] = (uint16_t)srcR1[2];
                dst1[3] = (uint16_t)srcR1[3];*/
                Convert4(srcR1, dst1);
            }

            // copy bottom-left on demand
            if (sbr + 1 != ((frameHeight + 63) >> 6))
            {
                dst0 = in0 + BORDERS_OFFSET - CDEF_BORDER + CDEF_BLOCKSIZE * CDEF_BSTRIDE;
                dst1 = in1 + BORDERS_OFFSET - CDEF_BORDER * 2 + CDEF_BLOCKSIZE / 2 * CDEF_BSTRIDE;
                srcR0 = leftBorder   + (sbr+1)*pitchDstR0 + (sbc)*128;
                srcR1 = leftBorderUv + (sbr+1)*pitchDstR1 + (sbc)*128;
                for (int32_t i = 0; i < CDEF_VBORDER; i++) {
                    //Convert4(src0, dst0+2);
                    dst0[0] = (uint16_t)srcR0[0];
                    dst0[1] = (uint16_t)srcR0[1];
                    //Convert4(srcR0 - 2, dst0 - 2);

                    dst1[0] = (uint16_t)srcR1[0];
                    dst1[1] = (uint16_t)srcR1[1];
                    dst1[2] = (uint16_t)srcR1[2];
                    dst1[3] = (uint16_t)srcR1[3];
                    //Convert4(srcR1, dst1);

                    // upd
                    dst0 += CDEF_BSTRIDE;
                    dst1 += CDEF_BSTRIDE;
                    srcR0 += 2;
                    srcR1 += 4;
                }
            }

        } else {
            // reset left pixels
            uint16_t *p = in0 + BORDERS_OFFSET - 2;
            for (int32_t i = 0; i < CDEF_BLOCKSIZE + CDEF_VBORDER; i++, p += CDEF_BSTRIDE)
                storel_si32(p, XL8);

            p = in1 + BORDERS_OFFSET - 4;
            for (int32_t i = 0; i < CDEF_BLOCKSIZE / 2 + CDEF_VBORDER; i++, p += CDEF_BSTRIDE)
                storel_epi64(p, XL8);
        }

        // copy central part, right, bottom and right-bottom pixels
        {
            const int32_t pitchOrig0 = frame.pitch_luma_pix;
            const int32_t pitchOrig1 = frame.pitch_chroma_pix;
            const PixType* src0 = (PixType*)frame.y  + pitchOrig0*sbr*64 + sbc * 64;
            const PixType* src1 = (PixType*)frame.uv + pitchOrig1*sbr*32 + sbc * 64;
            uint16_t *dst0 = in0 + BORDERS_OFFSET;
            uint16_t *dst1 = in1 + BORDERS_OFFSET;

            for (int32_t i = 0; i < CDEF_BLOCKSIZE + CDEF_BORDER; i++, dst0 += CDEF_BSTRIDE, src0 += pitchOrig0) {
                Convert64(src0, dst0);
                Convert4(src0 + CDEF_BLOCKSIZE, dst0 + CDEF_BLOCKSIZE);
            }

            for (int32_t i = 0; i < CDEF_BLOCKSIZE / 2 + CDEF_BORDER; i++, dst1 += CDEF_BSTRIDE, src1 += pitchOrig1) {
                Convert64(src1, dst1);
                Convert4(src1 + CDEF_BLOCKSIZE, dst1 + CDEF_BLOCKSIZE);
            }
        }

        // check right frame border
        if (sbc + 1 == ((frameWidth + 63) >> 6)) {
            int32_t blockWidth = frameWidth - sbc * 64;
            uint16_t *dst0 = in0 + CDEF_HBORDER + blockWidth;
            uint16_t *dst1 = in1 + CDEF_HBORDER + blockWidth;
            for (int32_t i = 0; i < CDEF_BLOCKSIZE + 2 * CDEF_VBORDER; i++, dst0 += CDEF_BSTRIDE)
                *(uint32_t *)dst0 = XL2;
            for (int32_t i = 0; i < CDEF_BLOCKSIZE / 2 + 2 * CDEF_VBORDER; i++, dst1 += CDEF_BSTRIDE)
                *(uint64_t *)dst1 = XL4;
        }

        // check bottom frame border
        if (sbr + 1 == ((frameHeight + 63) >> 6)) {
            int32_t blockHeight = (frameHeight - sbr * 64);
            uint16_t *dst0 = in0 + BORDERS_OFFSET - CDEF_HBORDER + CDEF_BSTRIDE * blockHeight;
            uint16_t *dst1 = in1 + BORDERS_OFFSET - CDEF_HBORDER + CDEF_BSTRIDE * blockHeight / 2;

            for (int32_t i = 0; i < CDEF_VBORDER; i++, dst0 += CDEF_BSTRIDE, dst1 += CDEF_BSTRIDE)
                for (int32_t j = 0; j < CDEF_BSTRIDE; j += 8) {
                    storea_si128(dst0 + j, XL8);
                    storea_si128(dst1 + j, XL8);
                }
        }

        return in0 + BORDERS_OFFSET;
    }

    void CdefParamInit(CdefParam &param)
    {
        param.cdef_bits = 0;
        param.nb_cdef_strengths = 1 << param.cdef_bits;
        for (int32_t i = 0; i < CDEF_MAX_STRENGTHS; i++) {
            param.cdef_strengths[i] = 0;
            param.cdef_uv_strengths[i] = 0;
        }
    }

    static int32_t sb_all_skip(int32_t mi_row, int32_t mi_col, const AV1VideoParam &par, const ModeInfo *mi)
    {
        const int32_t maxc = std::min(MAX_MIB_SIZE, par.miCols - mi_col);
        const int32_t maxr = std::min(MAX_MIB_SIZE, par.miRows - mi_row);

        for (int32_t r = 0; r < maxr; r++)
            for (int32_t c = 0; c < maxc; c++)
                if (mi[(mi_row + r) * par.miPitch + mi_col + c].skip == 0)
                    return false;
        return true;
    }

    void CdefStoreBorderSb_8bit(const AV1VideoParam &par, Frame *frame, int32_t sbr, int32_t sbc)
    {
        const int LPFOFFSET = 8;

        const int32_t pitchSrc0 = frame->m_recon->pitch_luma_pix;
        uint8_t* src0 = frame->m_recon->y + sbr*64 * pitchSrc0 + sbc*64;
        const int32_t pitchDst0 = frame->m_fenc->m_cdefLineBufs.pitchAbove;
        uint8_t* dst0 = frame->m_fenc->m_cdefLineBufs.aboveBorderY + sbr * 2 * pitchDst0 + sbc*64;

        // above
        int32_t len = (sbc + 1 == par.sb64Cols) ? CDEF_BLOCKSIZE + LPFOFFSET : CDEF_BLOCKSIZE;
        small_memcpy(dst0 - LPFOFFSET,             src0 - 2 * pitchSrc0 - LPFOFFSET, len);
        small_memcpy(dst0 - LPFOFFSET + pitchDst0, src0 - 1 * pitchSrc0 - LPFOFFSET, len);

        // left
        const int32_t pitchDstL0 = frame->m_fenc->m_cdefLineBufs.pitchLeft;
        uint8_t* dstL0 = frame->m_fenc->m_cdefLineBufs.leftBorder + sbr*pitchDstL0 + sbc*128;
        len = (sbr + 1 == par.sb64Rows) ? CDEF_BLOCKSIZE : CDEF_BLOCKSIZE - 2* LPFOFFSET;

        for (int i = 0; i < len; i++) {
            *reinterpret_cast<uint16_t*>(dstL0) = *reinterpret_cast<const uint16_t*>(src0 - 2);
            dstL0 += 2;
            src0 += pitchSrc0;
        }

        // addon for above
        if (sbr > 0) {
            src0  = frame->m_recon->y + (sbr)*64 * pitchSrc0 + sbc*64 - 2 * LPFOFFSET * pitchSrc0;
            dstL0 = frame->m_fenc->m_cdefLineBufs.leftBorder + (sbr-1)*pitchDstL0 + sbc*128 + (128-4*LPFOFFSET);

            for (int i = 0; i < 2* LPFOFFSET; i++) {
                *reinterpret_cast<uint16_t*>(dstL0) = *reinterpret_cast<const uint16_t*>(src0 - 2);
                dstL0 += 2;
                src0 += pitchSrc0;
            }
        }

        // chrome
        const int32_t pitchSrc1 = frame->m_recon->pitch_chroma_pix;
        const uint8_t* src1 = frame->m_recon->uv + sbr*32 * pitchSrc1 + sbc*64;
        const int32_t pitchDst1 = frame->m_fenc->m_cdefLineBufs.pitchAbove;
        uint8_t* dst1 = frame->m_fenc->m_cdefLineBufs.aboveBorderUv + sbr * 2 * pitchDst1 + sbc*64;

        // above
        len = (sbc + 1 == par.sb64Cols) ? CDEF_BLOCKSIZE + LPFOFFSET : CDEF_BLOCKSIZE;
        small_memcpy(dst1 - LPFOFFSET,             src1 - 2 * pitchSrc1 - LPFOFFSET, len);
        small_memcpy(dst1 - LPFOFFSET + pitchDst1, src1 - 1 * pitchSrc1 - LPFOFFSET, len);

        // left
        int32_t pitchDstL1 = frame->m_fenc->m_cdefLineBufs.pitchLeft;
        uint8_t* dstL1 = frame->m_fenc->m_cdefLineBufs.leftBorderUv + sbr*pitchDstL1 + sbc*128;
        len = (sbr + 1 == par.sb64Rows) ? CDEF_BLOCKSIZE / 2 : CDEF_BLOCKSIZE / 2 - 2* LPFOFFSET / 2;

        for (int i = 0; i < len; i++) {
            Copy32(src1 - 4, dstL1);
            dstL1 += 4;
            src1 += pitchSrc1;
        }

        // addon for above
        if (sbr > 0) {
            src1 = frame->m_recon->uv + sbr*32 * pitchSrc1 + sbc*64 - LPFOFFSET * pitchSrc1;
            dstL1 = frame->m_fenc->m_cdefLineBufs.leftBorderUv + (sbr - 1) * pitchDstL1 + sbc*128 + (128 - 32);

            for (int i = 0; i < LPFOFFSET; i++) {
                Copy32(src1 - 4, dstL1);
                dstL1 += 4;
                src1 += pitchSrc1;
            }
        }
    }

    void CdefStoreBorderSb_10bit(const AV1VideoParam &par, Frame *frame, int32_t sbr, int32_t sbc)
    {
        const int LPFOFFSET = 8;

        const int32_t pitchSrc0 = frame->m_recon10->pitch_luma_pix;
        uint16_t* src0 = (uint16_t*)frame->m_recon10->y + sbr*64 * pitchSrc0 + sbc*64;
        const int32_t pitchDst0 = frame->m_fenc->m_cdefLineBufs10.pitchAbove;
        uint16_t* dst0 = (uint16_t*)frame->m_fenc->m_cdefLineBufs10.aboveBorderY + sbr * 2 * pitchDst0 + sbc*64;

        // above
        int32_t len = (sbc + 1 == par.sb64Cols) ? CDEF_BLOCKSIZE + LPFOFFSET : CDEF_BLOCKSIZE;
        small_memcpy(dst0 - LPFOFFSET,             src0 - 2 * pitchSrc0 - LPFOFFSET, len*sizeof(uint16_t));
        small_memcpy(dst0 - LPFOFFSET + pitchDst0, src0 - 1 * pitchSrc0 - LPFOFFSET, len*sizeof(uint16_t));

        // left
        const int32_t pitchDstL0 = frame->m_fenc->m_cdefLineBufs10.pitchLeft;
        uint16_t* dstL0 = (uint16_t*)frame->m_fenc->m_cdefLineBufs10.leftBorder + sbr*pitchDstL0 + sbc*128;
        len = (sbr + 1 == par.sb64Rows) ? CDEF_BLOCKSIZE : CDEF_BLOCKSIZE - 2* LPFOFFSET;

        for (int i = 0; i < len; i++) {
            *reinterpret_cast<uint32_t*>(dstL0) = *reinterpret_cast<const uint32_t*>(src0 - 2);
            dstL0 += 2;
            src0 += pitchSrc0;
        }

        // addon for above
        if (sbr > 0) {
            src0  = (uint16_t*)frame->m_recon10->y + (sbr)*64 * pitchSrc0 + sbc*64 - 2 * LPFOFFSET * pitchSrc0;
            dstL0 = (uint16_t*)frame->m_fenc->m_cdefLineBufs10.leftBorder + (sbr-1)*pitchDstL0 + sbc*128 + (128-4*LPFOFFSET);

            for (int i = 0; i < 2* LPFOFFSET; i++) {
                *reinterpret_cast<uint32_t*>(dstL0) = *reinterpret_cast<const uint32_t*>(src0 - 2);
                dstL0 += 2;
                src0 += pitchSrc0;
            }
        }

        // chrome
        const int32_t pitchSrc1 = frame->m_recon10->pitch_chroma_pix;
        const uint16_t* src1 = (uint16_t*)frame->m_recon10->uv + sbr*32 * pitchSrc1 + sbc*64;
        const int32_t pitchDst1 = frame->m_fenc->m_cdefLineBufs10.pitchAbove;
        uint16_t* dst1 = frame->m_fenc->m_cdefLineBufs10.aboveBorderUv + sbr * 2 * pitchDst1 + sbc*64;

        // above
        len = (sbc + 1 == par.sb64Cols) ? CDEF_BLOCKSIZE + LPFOFFSET : CDEF_BLOCKSIZE;
        small_memcpy(dst1 - LPFOFFSET,             src1 - 2 * pitchSrc1 - LPFOFFSET, len*sizeof(uint16_t));
        small_memcpy(dst1 - LPFOFFSET + pitchDst1, src1 - 1 * pitchSrc1 - LPFOFFSET, len*sizeof(uint16_t));

        // left
        int32_t pitchDstL1 = frame->m_fenc->m_cdefLineBufs10.pitchLeft;
        uint16_t* dstL1 = frame->m_fenc->m_cdefLineBufs10.leftBorderUv + sbr*pitchDstL1 + sbc*128;
        len = (sbr + 1 == par.sb64Rows) ? CDEF_BLOCKSIZE / 2 : CDEF_BLOCKSIZE / 2 - 2* LPFOFFSET / 2;

        for (int i = 0; i < len; i++) {
            //Copy32(src1 - 4, dstL1);//!!! AYA
            dstL1[0] = src1[-4];
            dstL1[0+1] = src1[-4+1];
            dstL1[0+2] = src1[-4+2];
            dstL1[0+3] = src1[-4+3];

            dstL1 += 4;
            src1 += pitchSrc1;
        }

        // addon for above
        if (sbr > 0) {
            src1 = (uint16_t*)frame->m_recon10->uv + sbr*32 * pitchSrc1 + sbc*64 - LPFOFFSET * pitchSrc1;
            dstL1 = frame->m_fenc->m_cdefLineBufs10.leftBorderUv + (sbr - 1) * pitchDstL1 + sbc*128 + (128 - 32);

            for (int i = 0; i < LPFOFFSET; i++) {
                //Copy32(src1 - 4, dstL1);//AYA!!!
                dstL1[0] = src1[-4];
                dstL1[0 + 1] = src1[-4 + 1];
                dstL1[0 + 2] = src1[-4 + 2];
                dstL1[0 + 3] = src1[-4 + 3];

                dstL1 += 4;
                src1 += pitchSrc1;
            }
        }
    }

    void CdefStoreBorderSb(const AV1VideoParam &par, Frame *frame, int32_t sbr, int32_t sbc)
    {
        if (par.src10Enable)
            CdefStoreBorderSb_10bit(par, frame, sbr, sbc);
        else
            CdefStoreBorderSb_8bit(par, frame, sbr, sbc);
    }

    template <typename PixType> void CdefOnePassSb_predict(const AV1VideoParam &par, Frame *frame, int32_t sbr, int32_t sbc, int32_t strengthId)
    {

        alignas(32) PixType dst_intermediaY[64 * 64];
        alignas(32) PixType dst_intermediaUv[32 * 64];
        alignas(32) uint16_t in0[CDEF_INBUF_SIZE];
        alignas(32) uint16_t in1[CDEF_INBUF_SIZE_CHROMA];
        const uint16_t *input[2] = { in0 + BORDERS_OFFSET, in1 + BORDERS_OFFSET };

        if (sizeof(PixType) == 1)
            frame->m_fenc->m_cdefLineBufs.PrepareSb(sbr, sbc, *frame->m_recon, in0, in1);
        else
            frame->m_fenc->m_cdefLineBufs10.PrepareSb(sbr, sbc, *frame->m_recon10, in0, in1);

        const int32_t sec_damping = 3 + (frame->m_sliceQpY >> 6);
        const int32_t pri_damping = 3 + (frame->m_sliceQpY >> 6);
        const int32_t posx = sbc << 6;
        const int32_t posy = sbr << 6;

        FrameData* reconFrame  = sizeof(PixType) == 1 ? frame->m_recon  : frame->m_recon10;
        FrameData* originFrame = sizeof(PixType) == 1 ? frame->m_origin : frame->m_origin10;

        const PixType *recon8[2] = { (PixType*)reconFrame->y, (PixType*)reconFrame->uv };
        const PixType *origin[2] = { (PixType*)originFrame->y, (PixType*)originFrame->uv };

        const int32_t strideRecon8[2] = { reconFrame->pitch_luma_pix, reconFrame->pitch_chroma_pix };
        const int32_t strideOrigin[2] = { originFrame->pitch_luma_pix, originFrame->pitch_chroma_pix };
        origin[0] = (PixType*)originFrame->y  + posx + originFrame->pitch_luma_pix   * posy;
        origin[1] = (PixType*)originFrame->uv + posx + originFrame->pitch_chroma_pix * (posy >> 1);
        recon8[0] = (PixType*)reconFrame->y  + posx + reconFrame->pitch_luma_pix * posy;
        recon8[1] = (PixType*)reconFrame->uv + posx + reconFrame->pitch_chroma_pix * (posy >> 1);

        const int32_t maxc = std::min(MAX_MIB_SIZE, par.miCols - sbc * MAX_MIB_SIZE);
        const int32_t maxr = std::min(MAX_MIB_SIZE, par.miRows - sbr * MAX_MIB_SIZE);

        ModeInfo *mi = frame->m_modeInfo + (sbr * MAX_MIB_SIZE) * par.miPitch + (sbc * MAX_MIB_SIZE);

        alignas(16) int32_t totsse[2][2] = { 0 };
        int8_t  dirs[8][8];
        for (int32_t r = 0; r < maxr; r++, mi += par.miPitch) {
            for (int32_t c = 0; c < maxc; c++) {
                if (mi[c].skip) {
                    dirs[r][c] = -1;
                    continue;
                }

                int32_t y = r << 3;
                int32_t x = c << 3;

                int32_t ostride = strideOrigin[0];
                const PixType *org = origin[0] + y * ostride + x;
                const uint16_t* in = input[0] + y * CDEF_BSTRIDE + x;

                int32_t var;
                const int32_t dir = AV1PP::cdef_find_dir(in, CDEF_BSTRIDE, &var, 0);
                dirs[r][c] = (int8_t)dir;

                totsse[0][0] += AV1PP::sse(org, ostride, recon8[0] + y * strideRecon8[0] + x, strideRecon8[0], 1, 1);
                {
                    uint32_t i = strengthId;
                    //const int32_t pri = pri_strengths_y[i];
                    int str = frame->m_cdefParam.cdef_strengths[i];
                    const int32_t pri = str >> 2;
                    const int32_t sec = str & 0x3;
                    const int32_t adj_pri = adjust_strength(pri, var);
                    int32_t pitchDst = 64;
                    PixType *dst = dst_intermediaY + y * pitchDst + x;
                    int32_t sse;
                    if (sec) {
                        AV1PP::cdef_estimate_block(org, ostride, in, adj_pri, sec + (sec == 3), dir, pri_damping, sec_damping, &sse, 0, dst, pitchDst);
                    } else {
                        AV1PP::cdef_estimate_block_sec0(org, ostride, in, adj_pri, dir, pri_damping, &sse, 0, dst, pitchDst);
                    }
                    totsse[0][1] += sse;
                }

                y >>= 1;
                ostride = strideOrigin[1];
                org = origin[1] + y * ostride + x;
                in = input[1] + y * CDEF_BSTRIDE + x;

                totsse[1][0] += AV1PP::sse(org, ostride, recon8[1] + y * strideRecon8[1] + x, strideRecon8[1], 1, 0);
                {
                    uint32_t i = strengthId;
                    //const int32_t pri = pri_strengths_uv[i];
                    int str = frame->m_cdefParam.cdef_uv_strengths[i];
                    const int32_t pri = str >> 2;
                    const int32_t sec = str & 0x3;
                    int32_t pitchDst = 64;
                    PixType *dst = dst_intermediaUv + y * pitchDst + x;
                    int32_t sse;
                    if (sec) {
                        AV1PP::cdef_estimate_block(org, ostride, in, pri, sec + (sec == 3), dir, pri_damping - 1, sec_damping - 1, &sse, 1, dst, pitchDst);
                    } else {
                        AV1PP::cdef_estimate_block_sec0(org, ostride, in, pri, dir, pri_damping - 1, &sse, 1, dst, pitchDst);
                    }
                    totsse[1][1] += sse;
                }
            }
        }


        int32_t sseNoFilter = totsse[0][0] + totsse[1][0];
        int32_t sseFilter   = totsse[0][1] + totsse[1][1];
        int32_t bestStrengthId = sseNoFilter < sseFilter ? 0 : strengthId;

        frame->m_fenc->m_cdefStrenths[sbr * par.sb64Cols + sbc] = (int8_t)bestStrengthId;

        // test: if apply needed?
        if (bestStrengthId == 0) return;


        // apply: just copy on demand

        for (int32_t r = 0; r < maxr; r++) {
            for (int32_t c = 0; c < maxc; c++) {
                int8_t dir = dirs[r][c];
                if (dir < 0) // skip
                    continue;

                int32_t x = c << 3;
                int32_t y = r << 3;

                //if (idxY)
                {
                    int32_t pitchDst = reconFrame->pitch_luma_pix;
                    PixType *dst = (PixType*)reconFrame->y + (sbr * 64 + y) * pitchDst + (sbc * 64 + x);
                    int32_t pitchSrc = 64;
                    PixType* src = dst_intermediaY + y * pitchSrc + x;
                    CopyNxN(src, 64, dst, pitchDst, 8);
                }
                //if (idxUv)
                {
                    y = r << 2;
                    int32_t pitchDst = reconFrame->pitch_chroma_pix;
                    PixType *dst = (PixType*)reconFrame->uv + (sbr * 32 + y) * pitchDst + (sbc * 64 + x);
                    int32_t pitchSrc = 64;
                    PixType* src = dst_intermediaUv + y * pitchSrc + x;
                    CopyNxM(src, 64, dst, pitchDst, 8, 4);
                }
            }
        }
    }
    template void CdefOnePassSb_predict<uint8_t>(const AV1VideoParam &par, Frame *frame, int32_t sbr, int32_t sbc, int32_t strengthId);
    template void CdefOnePassSb_predict<uint16_t>(const AV1VideoParam &par, Frame *frame, int32_t sbr, int32_t sbc, int32_t strengthId);

    template <typename PixType> void CdefOnePassSb_func(const AV1VideoParam &par, Frame *frame, int32_t sbr, int32_t sbc)
    {
        if (sb_all_skip(sbr * MAX_MIB_SIZE, sbc * MAX_MIB_SIZE, par, frame->m_modeInfo)) {
            frame->m_fenc->m_cdefStrenths[sbr * par.sb64Cols + sbc] = -1;
            return;
        }

        int predMode = ((sbr + sbc) & 0x1) && (frame->m_picCodeType != MFX_FRAMETYPE_I);
        int predL = sbr * par.sb64Cols + sbc - 1;
        int predA = (sbr - 1) * par.sb64Cols + sbc;
        if (!par.src10Enable && predMode) {
            if (sbc > 0 && frame->m_fenc->m_cdefStrenths[predL] > 0) {
                int predictor = frame->m_fenc->m_cdefStrenths[predL];
                return CdefOnePassSb_predict<uint8_t>(par, frame, sbr, sbc, predictor);
            }
            else if (sbr > 0 && frame->m_fenc->m_cdefStrenths[predA] > 0) {
                int predictor = frame->m_fenc->m_cdefStrenths[predA];
                return CdefOnePassSb_predict<uint8_t>(par, frame, sbr, sbc, predictor);
            }

            // no predictor but we set default for speed
            return;
        }

        alignas(32) PixType dst_intermediaY[3][64 * 64];
        alignas(32) PixType dst_intermediaUv[3][32*64];
        alignas(32) uint16_t in0[CDEF_INBUF_SIZE];
        alignas(32) uint16_t in1[CDEF_INBUF_SIZE_CHROMA];
        const uint16_t *input[2] = {in0 + BORDERS_OFFSET, in1 + BORDERS_OFFSET};

        if (sizeof(PixType) == 1) {
            frame->m_fenc->m_cdefLineBufs.PrepareSb(sbr, sbc, *frame->m_recon, in0, in1);
        } else {
            frame->m_fenc->m_cdefLineBufs10.PrepareSb(sbr, sbc, *frame->m_recon10, in0, in1);
        }

        const int32_t sec_damping = 3 + (frame->m_sliceQpY >> 6);
        const int32_t pri_damping = 3 + (frame->m_sliceQpY >> 6);

        const int32_t posx = sbc << 6;
        const int32_t posy = sbr << 6;

        const int32_t bit_depth = sizeof(PixType) == 1 ? 8 : 10;
        const int32_t coeff_shift = std::max(bit_depth - 8, 0);

        FrameData* reconFrame  = sizeof(PixType) == 1 ? frame->m_recon  : frame->m_recon10;
        FrameData* originFrame = sizeof(PixType) == 1 ? frame->m_origin : frame->m_origin10;
        const PixType *recon8[2] = { (PixType*)reconFrame->y,  (PixType*)reconFrame->uv };
        const PixType *origin[2] = { (PixType*)originFrame->y, (PixType*)originFrame->uv };
        const int32_t strideRecon8[2] = { reconFrame->pitch_luma_pix,  reconFrame->pitch_chroma_pix };
        const int32_t strideOrigin[2] = { originFrame->pitch_luma_pix, originFrame->pitch_chroma_pix };

        origin[0] = (PixType*)originFrame->y  + posx + originFrame->pitch_luma_pix * posy;
        origin[1] = (PixType*)originFrame->uv + posx + originFrame->pitch_chroma_pix * (posy >> 1);
        recon8[0] = (PixType*)reconFrame->y  + posx + reconFrame->pitch_luma_pix * posy;
        recon8[1] = (PixType*)reconFrame->uv + posx + reconFrame->pitch_chroma_pix * (posy >> 1);

        const int32_t maxc = std::min(MAX_MIB_SIZE, par.miCols - sbc * MAX_MIB_SIZE);
        const int32_t maxr = std::min(MAX_MIB_SIZE, par.miRows - sbr * MAX_MIB_SIZE);

        ModeInfo *mi = frame->m_modeInfo + (sbr * MAX_MIB_SIZE) * par.miPitch + (sbc * MAX_MIB_SIZE);

        alignas(16) int32_t totsse[2][4] = { 0 };
        int8_t  dirs[8][8];
        for (int32_t r = 0; r < maxr; r++, mi += par.miPitch) {
            for (int32_t c = 0; c < maxc; c++) {
                if (mi[c].skip) {
                    dirs[r][c] = -1;
                    continue;
                }

                int32_t y = r << 3;
                int32_t x = c << 3;

                int32_t ostride = strideOrigin[0];
                const PixType *org = origin[0] + y * ostride + x;
                const uint16_t* in = input[0] + y * CDEF_BSTRIDE + x;

                int32_t var;
                const int32_t dir = AV1PP::cdef_find_dir(in, CDEF_BSTRIDE, &var, coeff_shift);
                dirs[r][c] = (int8_t)dir;

                totsse[0][0] += AV1PP::sse(org, ostride, recon8[0] + y * strideRecon8[0] + x, strideRecon8[0], 1, 1);
                if (frame->m_cdefParam.mode == CDEF_456) {
                    const int32_t pri = 1;
                    const int32_t adj_pri = adjust_strength(pri << coeff_shift, var);
                    int32_t pitchDst = 64;
                    PixType *dst[4] = { dst_intermediaY[0] + y * pitchDst + x,
                    dst_intermediaY[1] + y * pitchDst + x,
                    dst_intermediaY[2] + y * pitchDst + x,
                    nullptr };
                    int sse[4] = { 0 };

                    AV1PP::cdef_estimate_block_all_sec(org, ostride, in, adj_pri, dir, pri_damping + coeff_shift, sec_damping + coeff_shift, sse, 0, dst, pitchDst);

                    totsse[0][1] += sse[0];
                    totsse[0][2] += sse[1];
                    totsse[0][3] += sse[2];
                } else {
                    for (int32_t i = 1; i < frame->m_cdefParam.nb_cdef_strengths; i++) {
                        int str = frame->m_cdefParam.cdef_strengths[i];
                        const int32_t pri = str >> 2;
                        const int32_t sec = str & 0x3;
                        const int32_t adj_pri = adjust_strength(pri << coeff_shift, var);
                        int32_t pitchDst = 64;
                        PixType *dst = dst_intermediaY[i - 1] + y * pitchDst + x;
                        int32_t sse;
                        if (sec) {
                            AV1PP::cdef_estimate_block(org, ostride, in, adj_pri, (sec + (sec == 3)) << coeff_shift, dir, pri_damping + coeff_shift, sec_damping + coeff_shift, &sse, 0, dst, pitchDst);
                        } else {
                            AV1PP::cdef_estimate_block_sec0(org, ostride, in, adj_pri, dir, pri_damping + coeff_shift, &sse, 0, dst, pitchDst);
                        }
                        totsse[0][i] += sse;
                    }
                }

                y >>= 1;
                ostride = strideOrigin[1];
                org = origin[1] + y * ostride + x;
                in = input[1] + y * CDEF_BSTRIDE + x;

                totsse[1][0] += AV1PP::sse(org, ostride, recon8[1] + y * strideRecon8[1] + x, strideRecon8[1], 1, 0);
                if (frame->m_cdefParam.mode == CDEF_456) {
                    const int32_t pri = 1;
                    int32_t pitchDst = 64;
                    PixType *dst[4] = { dst_intermediaUv[0] + y * pitchDst + x,
                                        dst_intermediaUv[1] + y * pitchDst + x,
                                        dst_intermediaUv[2] + y * pitchDst + x,
                                        nullptr
                    };
                    int32_t sse[4] = { 0 };
                    AV1PP::cdef_estimate_block_all_sec(org, ostride, in, pri << coeff_shift, dir, pri_damping - 1 + coeff_shift, sec_damping - 1 + coeff_shift, sse, 1, dst, pitchDst);
                    totsse[1][1] += sse[0];
                    totsse[1][2] += sse[1];
                    totsse[1][3] += sse[2];
                }
                else {
                    for (int32_t i = 1; i < frame->m_cdefParam.nb_cdef_strengths; i++) {
                        int str = frame->m_cdefParam.cdef_uv_strengths[i];
                        const int32_t pri = str >> 2;
                        const int32_t sec = str & 0x3;
                        int32_t pitchDst = 64;
                        PixType *dst = dst_intermediaUv[i - 1] + y * pitchDst + x;
                        int32_t sse;
                        if (sec) {
                            AV1PP::cdef_estimate_block(org, ostride, in, pri << coeff_shift, (sec + (sec == 3)) << coeff_shift, dir, pri_damping - 1 + coeff_shift, sec_damping - 1 + coeff_shift, &sse, 1, dst, pitchDst);
                        } else {
                            AV1PP::cdef_estimate_block_sec0(org, ostride, in, pri << coeff_shift, dir, pri_damping - 1 + coeff_shift, &sse, 1, dst, pitchDst);
                        }
                        totsse[1][i] += sse;
                    }
                }
            }
        }

        int32_t bestStrengthId = 0;
        int32_t bestYuvSse = totsse[0][0] + totsse[1][0];
        for (int32_t i = 1; i < frame->m_cdefParam.nb_cdef_strengths; i++) {
            const int32_t yuvSse = totsse[0][i] + totsse[1][i];
            if (bestYuvSse > yuvSse) {
                bestYuvSse = yuvSse;
                bestStrengthId = i;
            }
        }
        frame->m_fenc->m_cdefStrenths[sbr * par.sb64Cols + sbc] = (int8_t)bestStrengthId;

#if 0
        // statistics
        {
            int log2BlkSize = 6;
            int width = 64;
            int height = 64;
            int32_t pitchRsCs = frame->m_stats[0]->m_pitchRsCs4[log2BlkSize - 2];
            int32_t idx = ((sbr * 64) >> log2BlkSize)*pitchRsCs + ((sbc * 64) >> log2BlkSize);
            int32_t Rs2 = frame->m_stats[0]->m_rs[log2BlkSize - 2][idx];
            int32_t Cs2 = frame->m_stats[0]->m_cs[log2BlkSize - 2][idx];
            int32_t RsCs2 = Rs2 + Cs2;
            float SCpp = RsCs2 * h265_reci_1to116[(width >> 2) - 1] * h265_reci_1to116[(height >> 2) - 1];

            int target = (bestStrengthId == 0) ? 0 : 1;
            {
                std::lock_guard<std::mutex> guard(g_dumpGuard);
                FILE* fp = fopen("cdef_stats.csv", "a+");
                fprintf(fp, "%i; %i; %i; %15.3f; %i\n", Rs2, Cs2, RsCs2, SCpp, target);
                fclose(fp);
            }
        }
#endif

        // test: if apply needed?
        if (bestStrengthId == 0) return;

        for (int32_t r = 0; r < maxr; r++) {
            for (int32_t c = 0; c < maxc; c++) {
                int8_t dir = dirs[r][c];
                if (dir < 0) // skip
                    continue;

                int32_t x = c << 3;
                int32_t y = r << 3;

                //if (str)
                {
                    int32_t pitchDst = reconFrame->pitch_luma_pix;
                    PixType *dst = (PixType*)reconFrame->y + (sbr * 64 + y) * pitchDst + (sbc * 64 + x);
                    int32_t pitchSrc = 64;
                    PixType* src = dst_intermediaY[bestStrengthId - 1] + y * pitchSrc + x;
                    CopyNxN(src, 64, dst, pitchDst, 8);
                }
                //if (uvStr)
                {
                    y = r << 2;
                    int32_t pitchDst = reconFrame->pitch_chroma_pix;
                    PixType *dst = (PixType*)reconFrame->uv + (sbr * 32 + y) * pitchDst + (sbc * 64 + x);
                    int32_t pitchSrc = 64;
                    PixType* src = dst_intermediaUv[bestStrengthId - 1] + y * pitchSrc + x;
                    CopyNxM(src, 64, dst, pitchDst, 8, 4);
                }
            }
        }
    }

    template void CdefOnePassSb_func<uint8_t>(const AV1VideoParam &par, Frame *frame, int32_t sbr, int32_t sbc);
    template void CdefOnePassSb_func<uint16_t>(const AV1VideoParam &par, Frame *frame, int32_t sbr, int32_t sbc);

    void CdefOnePassSb(const AV1VideoParam &par, Frame *frame, int32_t sbr, int32_t sbc)
    {
        if (par.src10Enable)
            return CdefOnePassSb_func<uint16_t>(par, frame, sbr, sbc);
        else
            return CdefOnePassSb_func<uint8_t>(par, frame, sbr, sbc);
    }
}

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE