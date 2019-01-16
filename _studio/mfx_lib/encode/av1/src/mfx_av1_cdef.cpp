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


    static int32_t priconv[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

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

    int32_t sb_compute_cdef_list(const AV1VideoParam &par, int32_t mi_row, int32_t mi_col, cdef_list *dlist, const ModeInfo *mi)
    {
        const int32_t maxc = std::min(MAX_MIB_SIZE, par.miCols - mi_col);
        const int32_t maxr = std::min(MAX_MIB_SIZE, par.miRows - mi_row);
        mi += mi_row * par.miPitch + mi_col;

        int32_t count = 0;
        for (int32_t r = 0; r < maxr; r++, mi += par.miPitch) {
            for (int32_t c = 0; c < maxc; c++) {
                if (!mi[c].skip) {
                    dlist[count].by = r;
                    dlist[count].bx = c;
                    count++;
                }
            }
        }
        return count;
    }


    CdefLineBuffers::CdefLineBuffers()
    {
    }

    CdefLineBuffers::~CdefLineBuffers()
    {
        Free();
    }

    void CdefLineBuffers::Alloc(const AV1VideoParam &par)
    {
        int32_t frameWidth = par.Width;
        pitchAbove = ((frameWidth + 63) & ~63) + 2 * CDEF_HBORDER;

        aboveBorder = (uint8_t *)AV1_Malloc(2*pitchAbove * sizeof(uint8_t) * par.sb64Rows*2);// both Y&UV
        ThrowIf(!aboveBorder, std::bad_alloc());
        aboveBorderY = aboveBorder + CDEF_HBORDER;
        aboveBorderUv = aboveBorderY + pitchAbove * par.sb64Rows*2;

        pitchLeft = 128 * par.sb64Cols;
        leftBorder = (uint8_t *)AV1_Malloc(sizeof(uint8_t) * par.sb64Rows * par.sb64Cols * 64 * 2 * 2);
        ThrowIf(!leftBorder, std::bad_alloc());
        leftBorderUv = leftBorder + par.sb64Rows * pitchLeft;
    }

    void CdefLineBuffers::Free()
    {
        AV1_SafeFree(aboveBorder);
        AV1_SafeFree(leftBorder);
    }

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

    static inline void Convert4(const uint8_t *src, uint16_t *dst)
    {
        storel_epi64(dst, _mm_unpacklo_epi8(loadu_si32(src), _mm_setzero_si128()));
    }

    static inline void Convert16(const uint8_t *src, uint16_t *dst)
    {
        __m128i s0 = loada_si128(src);
        storea_si128(dst + 0, _mm_unpacklo_epi8(s0, _mm_setzero_si128()));
        storea_si128(dst + 8, _mm_unpackhi_epi8(s0, _mm_setzero_si128()));
    }

    static inline void Convert64(const uint8_t *src, uint16_t *dst)
    {
        Convert16(src + 0,  dst + 0);
        Convert16(src + 16, dst + 16);
        Convert16(src + 32, dst + 32);
        Convert16(src + 48, dst + 48);
    }

    static inline void Convert16Unaligned(const uint8_t *src, uint16_t *dst)
    {
        __m128i s0 = loadu_si128(src);
        storeu_si128(dst + 0, _mm_unpacklo_epi8(s0, _mm_setzero_si128()));
        storeu_si128(dst + 8, _mm_unpackhi_epi8(s0, _mm_setzero_si128()));
    }

    static inline void Convert64Unaligned(const uint8_t *src, uint16_t *dst)
    {
        Convert16Unaligned(src + 0,  dst + 0);
        Convert16Unaligned(src + 16, dst + 16);
        Convert16Unaligned(src + 32, dst + 32);
        Convert16Unaligned(src + 48, dst + 48);
    }

    const uint16_t *CdefLineBuffers::PrepareSb(const int sbr, const int sbc, FrameData& frame, uint16_t* in0, uint16_t* in1)
    {
        const int32_t frameWidth  = frame.width;
        const int32_t frameHeight = frame.height;
        const uint16_t veryLarge4[] = {CDEF_VERY_LARGE, CDEF_VERY_LARGE, CDEF_VERY_LARGE, CDEF_VERY_LARGE};

        // copy above-left, above and above-right
        if (sbr > 0) {
            uint16_t *dst0 = in0 + BORDERS_OFFSET - (CDEF_VBORDER * CDEF_BSTRIDE);
            uint16_t *dst1 = in1 + BORDERS_OFFSET - (CDEF_VBORDER * CDEF_BSTRIDE);

            const uint8_t* above0 = aboveBorderY  + pitchAbove*(sbr)*2 + (sbc)*64;
            const uint8_t* above1 = aboveBorderUv + pitchAbove*(sbr)*2 + (sbc)*64;

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
            const uint8_t* srcR0 = leftBorder + sbr*pitchDstR0 + (sbc)*128;
            for (int32_t i = 0; i < CDEF_BLOCKSIZE; i++, dst0 += CDEF_BSTRIDE, srcR0 += 2) {
                dst0[0] = (uint16_t)srcR0[0];
                dst0[1] = (uint16_t)srcR0[1];
                //Convert4(srcR0 - 2, dst0 - 2);
            }

            const int32_t pitchDstR1 = pitchLeft;
            const uint8_t* srcR1 = leftBorderUv + sbr*pitchDstR1 + (sbc)*128;
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
            const uint8_t* src0 = frame.y  + pitchOrig0*sbr*64 + sbc * 64;
            const uint8_t* src1 = frame.uv + pitchOrig1*sbr*32 + sbc * 64;
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


    int32_t cdef_estimate(const AV1VideoParam &par, const ModeInfo *mi, int32_t sbr, int32_t sbc,
                         const uint8_t *org_[2], const int32_t ostride_[2], const uint8_t *rec8_[2], const int32_t rec8stride_[2],
                         const uint16_t *in_[2], int32_t pri_damping, int8_t (&dirs)[8][8], int32_t (&vars)[8][8])
    {
        const int32_t miCol = sbc * MAX_MIB_SIZE;
        const int32_t miRow = sbr * MAX_MIB_SIZE;
        const int32_t maxc = std::min(MAX_MIB_SIZE, par.miCols - miCol);
        const int32_t maxr = std::min(MAX_MIB_SIZE, par.miRows - miRow);
        mi += miRow * par.miPitch + miCol;

        alignas(16) int32_t totsse[2][16] = {};

        const uint8_t pri_strengths_y[]  = { 0, 2, 4, 6, 8};
        const uint8_t pri_strengths_uv[] = { 0, 2, 4, 8 };
        const uint8_t pri_strength_comb[8][2] = { {0, 0}, {2, 0}, {4, 0}, {4, 2}, {6, 2}, {6, 4}, {8, 4}, {8, 8} };

        const uint8_t *org;
        const uint16_t *in;
        int32_t ostride;

        for (int32_t r = 0; r < maxr; r++, mi += par.miPitch) {
            for (int32_t c = 0; c < maxc; c++) {
                if (mi[c].skip) {
                    dirs[r][c] = -1;
                    continue;
                }

                int32_t y = r << 3;
                int32_t x = c << 3;

                ostride = ostride_[0];
                org = org_[0] + y * ostride + x;
                in = in_[0] + y * CDEF_BSTRIDE + x;

                int32_t var;
                const int32_t dir = AV1PP::cdef_find_dir(in, CDEF_BSTRIDE, &var, 0);
                vars[r][c] = var;
                dirs[r][c] = dir;

                totsse[0][0] += AV1PP::sse(org, ostride, rec8_[0] + y * rec8stride_[0] + x, rec8stride_[0], 1, 1);
                for (uint32_t i = 1; i < sizeof(pri_strengths_y); i++) {
                    const int32_t pri = pri_strengths_y[i];
                    const int32_t adj_pri = adjust_strength(pri, var);
                    int32_t sse = 0;
                    //AV1PP::cdef_estimate_block_sec0(org, ostride, in, adj_pri, dir, pri_damping, &sse, 0);
                    totsse[0][pri] += sse;
                }

                y >>= 1;
                ostride = ostride_[1];
                org = org_[1] + y * ostride + x;
                in = in_[1] + y * CDEF_BSTRIDE + x;

                totsse[1][0] += AV1PP::sse(org, ostride, rec8_[1] + y * rec8stride_[1] + x, rec8stride_[1], 1, 0);
                for (uint32_t i = 1; i < sizeof(pri_strengths_uv); i++) {
                    const int32_t pri = pri_strengths_uv[i];
                    int32_t sse = 0;
                    //AV1PP::cdef_estimate_block_sec0(org, ostride, in, pri, dir, pri_damping - 1, &sse, 1);
                    totsse[1][pri] += sse;
                }
            }
        }

        int32_t bestStrengthId = 0;
        int32_t bestYuvSse = totsse[0][0] + totsse[1][0];
        for (uint32_t i = 1; i < sizeof(pri_strength_comb) / sizeof(pri_strength_comb[0]); i++) {
            const int32_t yuvSse = totsse[0][pri_strength_comb[i][0]] + totsse[1][pri_strength_comb[i][1]];
            if (bestYuvSse > yuvSse) {
                bestYuvSse = yuvSse;
                bestStrengthId = i;
            }
        }
        return bestStrengthId;
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

    void CdefApplySb(const AV1VideoParam &par, Frame *frame, int32_t sbr, int32_t sbc, uint16_t *in0, uint16_t *in1)
    {
        CdefLineBuffers &lineBufs = frame->m_cdefLineBufs;

        int32_t pri_damp = 3 + (frame->m_sliceQpY >> 6);
        int32_t sec_damp = 3 + (frame->m_sliceQpY >> 6);
        //alignas(16) uint16_t in0[CDEF_INBUF_SIZE];
        //alignas(16) uint16_t in1[CDEF_INBUF_SIZE];

        //for (int32_t sbr = 0; sbr < par.sb64Rows; sbr++)
        {
            //for (int32_t sbc = 0; sbc < par.sb64Cols; sbc++)
            {
                //lineBufs.PrepareSb(sbr, sbc, *frame->m_recon, in0, in1);
                const uint16_t *src16Y  = in0 + BORDERS_OFFSET;
                const uint16_t *src16Uv = in1 + BORDERS_OFFSET;

                const int8_t cdefStrengthId = frame->m_cdefStrenths[sbr * par.sb64Cols + sbc];
                if (cdefStrengthId < 0) {
                    //continue;
                    return;
                }
                assert(cdefStrengthId >= 0 && cdefStrengthId < 8);
                const int32_t str = frame->m_cdefParam.cdef_strengths[cdefStrengthId];
                const int32_t uvStr = frame->m_cdefParam.cdef_uv_strengths[cdefStrengthId];
                if ((str | uvStr) == 0) {
                    //continue;
                    return;
                }

                int32_t pri = str >> 2;
                int32_t sec = str & 3;
                sec += (sec == 3);

                int32_t uvPri = uvStr >> 2;
                int32_t uvSec = uvStr & 3;
                uvSec += (uvSec == 3);

                const int32_t miRows = std::min(8, par.miRows - sbr * 8);
                const int32_t miCols = std::min(8, par.miCols - sbc * 8);
                const ModeInfo *mi = frame->m_modeInfo + sbr * 8 * par.miPitch + sbc * 8;

                const int8_t  (&dirs)[8][8] = frame->m_cdefDirs[sbr * par.sb64Cols + sbc];
                const int32_t (&vars)[8][8] = frame->m_cdefVars[sbr * par.sb64Cols + sbc];

                for (int32_t miRow = 0; miRow < miRows; miRow++, mi += par.miPitch) {
                    for (int32_t miCol = 0; miCol < miCols; miCol++) {
                        int8_t dir = dirs[miRow][miCol];
                        if (dir < 0) // skip
                            continue;

                        int32_t x = miCol << 3;
                        int32_t y = miRow << 3;

                        const uint16_t *src = src16Y + y * CDEF_BSTRIDE + x;

                        if (str) {
                            const int32_t adj_pri = adjust_strength(pri, vars[miRow][miCol]);
                            const int32_t d = pri ? dir : 0;
                            int32_t pitchDst = frame->m_recon->pitch_luma_pix;
                            uint8_t *dst = frame->m_recon->y + (sbr * 64 + y) * pitchDst + (sbc * 64 + x);
                            AV1PP::cdef_filter_block(dst, pitchDst, src, adj_pri, sec, d, pri_damp, sec_damp, 0);
                        }

                        if (uvStr) {
                            const int32_t d = uvPri ? dir : 0;
                            y = miRow << 2;
                            src = src16Uv + y * CDEF_BSTRIDE + x;
                            int32_t pitchDst = frame->m_recon->pitch_chroma_pix;
                            uint8_t *dst = frame->m_recon->uv + (sbr * 32 + y) * pitchDst + (sbc * 64 + x);
                            AV1PP::cdef_filter_block(dst, pitchDst, src, uvPri, uvSec, d, pri_damp - 1, sec_damp - 1, 1);
                        }
                    }
                }
            }
        }
    }

    void CdefApplyFrame(const AV1VideoParam &par, Frame *frame)
    {
        return;
    }

    void CdefApplyFrameOld(const AV1VideoParam &par, Frame *frame)
    {
        CdefLineBuffers &lineBufs = frame->m_cdefLineBufs;

#if 1
        const int LPFOFFSET = 8;
        // sim of deblocking copy border pixs
        for (int32_t sbr = 0; sbr < par.sb64Rows; sbr++) {
            for (int32_t sbc = 0; sbc < par.sb64Cols; sbc++) {

                // [1] luma part
                const int32_t pitchSrc0 = frame->m_recon->pitch_luma_pix;
                uint8_t* src0 = frame->m_recon->y + sbr*64 * pitchSrc0 + sbc*64;
                const int32_t pitchDst0 = frame->m_cdefLineBufs.pitchAbove;
                uint8_t* dst0 = frame->m_cdefLineBufs.aboveBorderY + sbr * 2 * pitchDst0 + sbc*64;

#if 0
                // above
                {
                    int32_t len = (sbc + 1 == par.sb64Cols) ? CDEF_BLOCKSIZE + LPFOFFSET : CDEF_BLOCKSIZE;
                    small_memcpy(dst0 - LPFOFFSET,             src0 - 2 * pitchSrc0 - LPFOFFSET, len);
                    small_memcpy(dst0 - LPFOFFSET + pitchDst0, src0 - 1 * pitchSrc0 - LPFOFFSET, len);
                }
#endif

#if 0
                // left
                {
                    const int32_t pitchDstL0 = frame->m_cdefLineBufs.pitchLeft;
                    uint8_t* dstL0 = frame->m_cdefLineBufs.leftBorder + sbr*pitchDstL0 + sbc*128;
                    int32_t len = (sbr + 1 == par.sb64Rows) ? CDEF_BLOCKSIZE : CDEF_BLOCKSIZE - 2* LPFOFFSET;

                    // addon for above
                    if (sbr > 0) {
                        uint8_t* src0  = frame->m_recon->y + (sbr)*64 * pitchSrc0 + sbc*64 - 2 * LPFOFFSET * pitchSrc0;
                        uint8_t* dstL0 = frame->m_cdefLineBufs.leftBorder + (sbr-1)*pitchDstL0 + sbc*128 + (128-4*LPFOFFSET);

                        for (int i = 0; i < 2* LPFOFFSET; i++) {
                            dstL0[0] = src0[-2];
                            dstL0[1] = src0[-1];
                            dstL0 += 2;
                            src0 += pitchSrc0;
                        }
                    }
                }
#endif

                // [2] chrome
                const int32_t pitchSrc1 = frame->m_recon->pitch_chroma_pix;
                const uint8_t* src1 = frame->m_recon->uv + sbr*32 * pitchSrc1 + sbc*64;
                const int32_t pitchDst1 = frame->m_cdefLineBufs.pitchAbove;
                uint8_t* dst1 = frame->m_cdefLineBufs.aboveBorderUv + sbr * 2 * pitchDst1 + sbc*64;
#if 0
                // above
                {
                    int32_t len = (sbc + 1 == par.sb64Cols) ? CDEF_BLOCKSIZE + LPFOFFSET : CDEF_BLOCKSIZE;
                    small_memcpy(dst1 - LPFOFFSET,             src1 - 2 * pitchSrc1 - LPFOFFSET, len);
                    small_memcpy(dst1 - LPFOFFSET + pitchDst1, src1 - 1 * pitchSrc1 - LPFOFFSET, len);
                }
#endif

                // left
                {
#if 0
                    const int32_t pitchDstR1 = frame->m_cdefLineBufs.pitchLeft;
                    uint8_t* dstR1 = frame->m_cdefLineBufs.leftBorderUv + sbr*pitchDstR1 + sbc*128;
                    int32_t len = (sbr + 1 == par.sb64Rows) ? CDEF_BLOCKSIZE / 2 : CDEF_BLOCKSIZE / 2 - 2* LPFOFFSET / 2;

                    for (int i = 0; i < len; i++) {
                        dstR1[0] = src1[-4];
                        dstR1[1] = src1[-3];
                        dstR1[2] = src1[-2];
                        dstR1[3] = src1[-1];

                        dstR1 += 4;
                        src1 += pitchSrc1;
                    }
#endif

#if 0
                    // addon for above
                    if (sbr > 0) {
                        const uint8_t* src1 = frame->m_recon->uv + sbr*32 * pitchSrc1 + sbc*64 - LPFOFFSET * pitchSrc1;
                        const int32_t pitchDstL1 = frame->m_cdefLineBufs.pitchLeft;
                        uint8_t* dstL1 = frame->m_cdefLineBufs.leftBorderUv + (sbr - 1) * pitchDstL1 + sbc*128 + (128 - 16);

                        for (int i = 0; i < LPFOFFSET; i++) {
                            dstL1[0] = src1[-4];
                            dstL1[1] = src1[-3];
                            dstL1[2] = src1[-2];
                            dstL1[3] = src1[-1];

                            dstL1 += 4;
                            src1 += pitchSrc1;
                        }
                    }
#endif
                }
            }
        }
        //
#endif

        int32_t pri_damp = 3 + (frame->m_sliceQpY >> 6);
        int32_t sec_damp = 3 + (frame->m_sliceQpY >> 6);
        alignas(16) uint16_t in0[CDEF_INBUF_SIZE];
        alignas(16) uint16_t in1[CDEF_INBUF_SIZE_CHROMA];

        for (int32_t sbr = 0; sbr < par.sb64Rows; sbr++) {
            for (int32_t sbc = 0; sbc < par.sb64Cols; sbc++) {
                const uint16_t *src16Y  = lineBufs.PrepareSb(sbr, sbc, *frame->m_recon, in0, in1);
                const uint16_t *src16Uv = src16Y + CDEF_INBUF_SIZE;

                const int8_t cdefStrengthId = frame->m_cdefStrenths[sbr * par.sb64Cols + sbc];
                if (cdefStrengthId < 0) {
                    continue;
                }
                assert(cdefStrengthId >= 0 && cdefStrengthId < 8);
                const int32_t str = frame->m_cdefParam.cdef_strengths[cdefStrengthId];
                const int32_t uvStr = frame->m_cdefParam.cdef_uv_strengths[cdefStrengthId];
                if ((str | uvStr) == 0) {
                    continue;
                }

                int32_t pri = str >> 2;
                int32_t sec = str & 3;
                sec += (sec == 3);

                int32_t uvPri = uvStr >> 2;
                int32_t uvSec = uvStr & 3;
                uvSec += (uvSec == 3);

                const int32_t miRows = std::min(8, par.miRows - sbr * 8);
                const int32_t miCols = std::min(8, par.miCols - sbc * 8);
                const ModeInfo *mi = frame->m_modeInfo + sbr * 8 * par.miPitch + sbc * 8;

                const int8_t  (&dirs)[8][8] = frame->m_cdefDirs[sbr * par.sb64Cols + sbc];
                const int32_t (&vars)[8][8] = frame->m_cdefVars[sbr * par.sb64Cols + sbc];

                for (int32_t miRow = 0; miRow < miRows; miRow++, mi += par.miPitch) {
                    for (int32_t miCol = 0; miCol < miCols; miCol++) {
                        int8_t dir = dirs[miRow][miCol];
                        if (dir < 0) // skip
                            continue;

                        int32_t x = miCol << 3;
                        int32_t y = miRow << 3;

                        const uint16_t *src = src16Y + y * CDEF_BSTRIDE + x;

                        if (str) {
                            const int32_t adj_pri = adjust_strength(pri, vars[miRow][miCol]);
                            const int32_t d = pri ? dir : 0;
                            int32_t pitchDst = frame->m_recon->pitch_luma_pix;
                            uint8_t *dst = frame->m_recon->y + (sbr * 64 + y) * pitchDst + (sbc * 64 + x);
                            AV1PP::cdef_filter_block(dst, pitchDst, src, adj_pri, sec, d, pri_damp, sec_damp, 0);
                        }

                        if (uvStr) {
                            const int32_t d = uvPri ? dir : 0;
                            y = miRow << 2;
                            src = src16Uv + y * CDEF_BSTRIDE + x;
                            int32_t pitchDst = frame->m_recon->pitch_chroma_pix;
                            uint8_t *dst = frame->m_recon->uv + (sbr * 32 + y) * pitchDst + (sbc * 64 + x);
                            AV1PP::cdef_filter_block(dst, pitchDst, src, uvPri, uvSec, d, pri_damp - 1, sec_damp - 1, 1);
                        }
                    }
                }
            }
        }
    }

    static uint64_t search_one_dual(int32_t *lev0, int32_t *lev1, int32_t nb_strengths, int32_t (**mse)[STRENGTH_COUNT_FAST], int32_t sb_count)
    {
        //int32_t strengthCount = STRENGTH_COUNT_FAST;
        uint64_t tot_mse[STRENGTH_COUNT_FAST][STRENGTH_COUNT_FAST];
        int32_t i, j;
        uint64_t best_tot_mse = (uint64_t)1 << 63;
        int32_t best_id0 = 0;
        int32_t best_id1 = 0;
        memset(tot_mse, 0, sizeof(tot_mse));
        for (i = 0; i < sb_count; i++) {
            int32_t gi;
            uint64_t best_mse = (uint64_t)1 << 63;
            /* Find best mse among already selected options. */
            for (gi = 0; gi < nb_strengths; gi++) {
                uint64_t curr = mse[0][i][lev0[gi]];
                curr += mse[1][i][lev1[gi]];
                if (curr < best_mse) {
                    best_mse = curr;
                }
            }
            /* Find best mse when adding each possible new option. */
            for (j = 0; j < STRENGTH_COUNT_FAST; j++) {
                int32_t k;
                for (k = 0; k < STRENGTH_COUNT_FAST; k++) {
                    uint64_t best = best_mse;
                    uint64_t curr = mse[0][i][j];
                    curr += mse[1][i][k];
                    if (curr < best) best = curr;
                    tot_mse[j][k] += best;
                }
            }
        }
        for (j = 0; j < STRENGTH_COUNT_FAST; j++) {
            int32_t k;
            for (k = 0; k < STRENGTH_COUNT_FAST; k++) {
                if (tot_mse[j][k] < best_tot_mse) {
                    best_tot_mse = tot_mse[j][k];
                    best_id0 = j;
                    best_id1 = k;
                }
            }
        }
        lev0[nb_strengths] = best_id0;
        lev1[nb_strengths] = best_id1;
        return best_tot_mse;
    }

    /* Search for the set of luma+chroma strengths that minimizes mse. */
    static uint64_t joint_strength_search_dual(int32_t *best_lev0, int32_t *best_lev1, int32_t nb_strengths, int32_t (**mse)[STRENGTH_COUNT_FAST], int32_t sb_count)
    {
        uint64_t best_tot_mse;
        int32_t i;
        best_tot_mse = (uint64_t)1 << 63;
        /* Greedy search: add one strength options at a time. */
        for (i = 0; i < nb_strengths; i++) {
            best_tot_mse = search_one_dual(best_lev0, best_lev1, i, mse, sb_count);
        }

#if 0
        /* Trying to refine the greedy search by reconsidering each
        already-selected option. */
        for (i = 0; i < 4 * nb_strengths; i++) {
            int32_t j;
            for (j = 0; j < nb_strengths - 1; j++) {
                best_lev0[j] = best_lev0[j + 1];
                best_lev1[j] = best_lev1[j + 1];
            }
            best_tot_mse = search_one_dual(best_lev0, best_lev1, nb_strengths - 1, mse, sb_count);
        }
#endif
        return best_tot_mse;
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

    void CdefStoreBorderSb(const AV1VideoParam &par, Frame *frame, int32_t sbr, int32_t sbc)
    {
        const int LPFOFFSET = 8;

        const int32_t pitchSrc0 = frame->m_recon->pitch_luma_pix;
        uint8_t* src0 = frame->m_recon->y + sbr*64 * pitchSrc0 + sbc*64;
        const int32_t pitchDst0 = frame->m_cdefLineBufs.pitchAbove;
        uint8_t* dst0 = frame->m_cdefLineBufs.aboveBorderY + sbr * 2 * pitchDst0 + sbc*64;

        // above
        int32_t len = (sbc + 1 == par.sb64Cols) ? CDEF_BLOCKSIZE + LPFOFFSET : CDEF_BLOCKSIZE;
        small_memcpy(dst0 - LPFOFFSET,             src0 - 2 * pitchSrc0 - LPFOFFSET, len);
        small_memcpy(dst0 - LPFOFFSET + pitchDst0, src0 - 1 * pitchSrc0 - LPFOFFSET, len);

        // left
        const int32_t pitchDstL0 = frame->m_cdefLineBufs.pitchLeft;
        uint8_t* dstL0 = frame->m_cdefLineBufs.leftBorder + sbr*pitchDstL0 + sbc*128;
        len = (sbr + 1 == par.sb64Rows) ? CDEF_BLOCKSIZE : CDEF_BLOCKSIZE - 2* LPFOFFSET;

        for (int i = 0; i < len; i++) {
            *reinterpret_cast<uint16_t*>(dstL0) = *reinterpret_cast<const uint16_t*>(src0 - 2);
            dstL0 += 2;
            src0 += pitchSrc0;
        }

        // addon for above
        if (sbr > 0) {
            uint8_t* src0  = frame->m_recon->y + (sbr)*64 * pitchSrc0 + sbc*64 - 2 * LPFOFFSET * pitchSrc0;
            uint8_t* dstL0 = frame->m_cdefLineBufs.leftBorder + (sbr-1)*pitchDstL0 + sbc*128 + (128-4*LPFOFFSET);

            for (int i = 0; i < 2* LPFOFFSET; i++) {
                *reinterpret_cast<uint16_t*>(dstL0) = *reinterpret_cast<const uint16_t*>(src0 - 2);
                dstL0 += 2;
                src0 += pitchSrc0;
            }
        }

        // chrome
        const int32_t pitchSrc1 = frame->m_recon->pitch_chroma_pix;
        const uint8_t* src1 = frame->m_recon->uv + sbr*32 * pitchSrc1 + sbc*64;
        const int32_t pitchDst1 = frame->m_cdefLineBufs.pitchAbove;
        uint8_t* dst1 = frame->m_cdefLineBufs.aboveBorderUv + sbr * 2 * pitchDst1 + sbc*64;

        // above
        len = (sbc + 1 == par.sb64Cols) ? CDEF_BLOCKSIZE + LPFOFFSET : CDEF_BLOCKSIZE;
        small_memcpy(dst1 - LPFOFFSET,             src1 - 2 * pitchSrc1 - LPFOFFSET, len);
        small_memcpy(dst1 - LPFOFFSET + pitchDst1, src1 - 1 * pitchSrc1 - LPFOFFSET, len);

        // left
        int32_t pitchDstL1 = frame->m_cdefLineBufs.pitchLeft;
        uint8_t* dstL1 = frame->m_cdefLineBufs.leftBorderUv + sbr*pitchDstL1 + sbc*128;
        len = (sbr + 1 == par.sb64Rows) ? CDEF_BLOCKSIZE / 2 : CDEF_BLOCKSIZE / 2 - 2* LPFOFFSET / 2;

        for (int i = 0; i < len; i++) {
            Copy32(src1 - 4, dstL1);
            dstL1 += 4;
            src1 += pitchSrc1;
        }

        // addon for above
        if (sbr > 0) {
            const uint8_t* src1 = frame->m_recon->uv + sbr*32 * pitchSrc1 + sbc*64 - LPFOFFSET * pitchSrc1;
            const int32_t pitchDstL1 = frame->m_cdefLineBufs.pitchLeft;
            uint8_t* dstL1 = frame->m_cdefLineBufs.leftBorderUv + (sbr - 1) * pitchDstL1 + sbc*128 + (128 - 32);

            for (int i = 0; i < LPFOFFSET; i++) {
                Copy32(src1 - 4, dstL1);
                dstL1 += 4;
                src1 += pitchSrc1;
            }
        }
    }

    void CdefSearchSb(const AV1VideoParam &par, Frame *frame, int32_t sbr, int32_t sbc, uint16_t* in0, uint16_t* in1)
    {
        ModeInfo *mi = frame->m_modeInfo;
        int32_t *sb_index = frame->m_sbIndex.data();
        const int32_t nvsb = par.sb64Rows;
        const int32_t nhsb = par.sb64Cols;
        const int32_t sec_damping = 3 + (frame->m_sliceQpY >> 6);
        const int32_t pri_damping = 3 + (frame->m_sliceQpY >> 6);
        const int32_t coeff_shift = 0;
        cdef_list dlist[64];

        uint16_t *reconY  = in0;
        uint16_t *reconUv = in1;
        uint16_t *recon[2] = { reconY, reconUv };
        int32_t strideRecon = CDEF_BSTRIDE;

        if (sb_all_skip(sbr * MAX_MIB_SIZE, sbc * MAX_MIB_SIZE, par, mi)) {
            frame->m_cdefStrenths[sbr * par.sb64Cols + sbc] = -1;
            return;
        }

        const int32_t posx = sbc << 6;
        const int32_t posy = sbr << 6;
        const int32_t toff = CDEF_VBORDER * (sbr != 0);
        const int32_t boff = CDEF_VBORDER * (sbr != nvsb - 1);
        const int32_t loff = CDEF_HBORDER * (sbc != 0);
        const int32_t roff = CDEF_HBORDER * (sbc != nhsb - 1);
        const int32_t width = MIN(8, par.miCols - 8 * sbc) << 3;
        const int32_t heightY = MIN(8, par.miRows - 8 * sbr) << 3;
        const int32_t heightUv = heightY >> 1;
        const int32_t widthWithBorder = width + loff + roff;
        const int32_t heightWithBorderY = heightY + toff + boff;
        const int32_t heightWithBorderUv = heightUv + toff + boff;

        for (int32_t i = 0; i < CDEF_INBUF_SIZE; i++)
            recon[0][i] = CDEF_VERY_LARGE;
        for (int32_t i = 0; i < CDEF_INBUF_SIZE; i++)
            recon[1][i] = CDEF_VERY_LARGE;

        const uint16_t *in[2] = {
            recon[0] + CDEF_VBORDER * CDEF_BSTRIDE + CDEF_HBORDER,
            recon[1] + CDEF_VBORDER * CDEF_BSTRIDE + CDEF_HBORDER
        };

        frame->m_cdefLineBufs.PrepareSb(sbr, sbc, *frame->m_recon, recon[0], recon[1]);

        const uint8_t *recon8[2] = { frame->m_recon->y, frame->m_recon->uv };
        const uint8_t *origin[2] = { frame->m_origin->y, frame->m_origin->uv };
        int32_t strideRecon8[2] = { frame->m_recon->pitch_luma_pix, frame->m_recon->pitch_chroma_pix };
        int32_t strideOrigin[2] = { frame->m_origin->pitch_luma_pix, frame->m_origin->pitch_chroma_pix };
        origin[0] = frame->m_origin->y  + posx + frame->m_origin->pitch_luma_pix * posy;
        origin[1] = frame->m_origin->uv + posx + frame->m_origin->pitch_chroma_pix * (posy >> 1);
        recon8[0] = frame->m_recon->y  + posx + frame->m_recon->pitch_luma_pix * posy;
        recon8[1] = frame->m_recon->uv + posx + frame->m_recon->pitch_chroma_pix * (posy >> 1);

        const uint32_t sb_count = vm_interlocked_inc32(&frame->m_sbCount) - 1;
        sb_index[sb_count] = sbr * par.sb64Cols + sbc;

        int8_t  (&dirs)[8][8] = frame->m_cdefDirs[sbr * par.sb64Cols + sbc];
        int32_t (&vars)[8][8] = frame->m_cdefVars[sbr * par.sb64Cols + sbc];
        int32_t strId = cdef_estimate(par, mi, sbr, sbc, origin, strideOrigin, recon8, strideRecon8, in, pri_damping, dirs, vars);

        int8_t *cdef_strength = frame->m_cdefStrenths;
        cdef_strength[sb_index[sb_count]] = strId;

    }

    void CdefOnePassSb(const AV1VideoParam &par, Frame *frame, int32_t sbr, int32_t sbc)
    {
        if (sb_all_skip(sbr * MAX_MIB_SIZE, sbc * MAX_MIB_SIZE, par, frame->m_modeInfo)) {
            frame->m_cdefStrenths[sbr * par.sb64Cols + sbc] = -1;
            return;
        }

        alignas(32) uint8_t dst_intermediaY[4][64*64];
        alignas(32) uint8_t dst_intermediaUv[3][32*64];

        alignas(32) uint16_t in0[CDEF_INBUF_SIZE];
        alignas(32) uint16_t in1[CDEF_INBUF_SIZE_CHROMA];
        uint16_t *recon[2] = {in0, in1};
        int32_t strideRecon = CDEF_BSTRIDE;
        const uint16_t *input[2] = {recon[0] + BORDERS_OFFSET, recon[1] + BORDERS_OFFSET};

        frame->m_cdefLineBufs.PrepareSb(sbr, sbc, *frame->m_recon, recon[0], recon[1]);

        const int32_t sec_damping = 3 + (frame->m_sliceQpY >> 6);
        const int32_t pri_damping = 3 + (frame->m_sliceQpY >> 6);

        const int32_t posx = sbc << 6;
        const int32_t posy = sbr << 6;
        const uint8_t *recon8[2] = { frame->m_recon->y, frame->m_recon->uv };
        const uint8_t *origin[2] = { frame->m_origin->y, frame->m_origin->uv };
        const int32_t strideRecon8[2] = { frame->m_recon->pitch_luma_pix, frame->m_recon->pitch_chroma_pix };
        const int32_t strideOrigin[2] = { frame->m_origin->pitch_luma_pix, frame->m_origin->pitch_chroma_pix };
        origin[0] = frame->m_origin->y  + posx + frame->m_origin->pitch_luma_pix * posy;
        origin[1] = frame->m_origin->uv + posx + frame->m_origin->pitch_chroma_pix * (posy >> 1);
        recon8[0] = frame->m_recon->y  + posx + frame->m_recon->pitch_luma_pix * posy;
        recon8[1] = frame->m_recon->uv + posx + frame->m_recon->pitch_chroma_pix * (posy >> 1);

        const uint32_t sb_count = vm_interlocked_inc32(&frame->m_sbCount) - 1;
        int32_t *sb_index = frame->m_sbIndex.data();
        sb_index[sb_count] = sbr * par.sb64Cols + sbc;

        int8_t  (&dirs)[8][8] = frame->m_cdefDirs[sbr * par.sb64Cols + sbc];
        int32_t (&vars)[8][8] = frame->m_cdefVars[sbr * par.sb64Cols + sbc];

        const int32_t maxc = std::min(MAX_MIB_SIZE, par.miCols - sbc * MAX_MIB_SIZE);
        const int32_t maxr = std::min(MAX_MIB_SIZE, par.miRows - sbr * MAX_MIB_SIZE);
        const uint8_t pri_strengths_y[]  = { 0, 2, 4, 6, 8};
        const uint8_t pri_strengths_uv[] = { 0, 2, 4, 8 };
        const uint8_t pri_strength_comb[8][2] = { {0, 0}, {2, 0}, {4, 0}, {4, 2}, {6, 2}, {6, 4}, {8, 4}, {8, 8} };

        ModeInfo *mi = frame->m_modeInfo + (sbr * MAX_MIB_SIZE) * par.miPitch + (sbc * MAX_MIB_SIZE);

        alignas(16) int32_t totsse[2][16] = {};
        for (int32_t r = 0; r < maxr; r++, mi += par.miPitch) {
            for (int32_t c = 0; c < maxc; c++) {
                if (mi[c].skip) {
                    dirs[r][c] = -1;
                    continue;
                }

                int32_t y = r << 3;
                int32_t x = c << 3;

                int32_t ostride = strideOrigin[0];
                const uint8_t *org = origin[0] + y * ostride + x;
                const uint16_t* in = input[0] + y * CDEF_BSTRIDE + x;

                int32_t var;
                const int32_t dir = AV1PP::cdef_find_dir(in, CDEF_BSTRIDE, &var, 0);
                vars[r][c] = var;
                dirs[r][c] = dir;

                totsse[0][0] += AV1PP::sse(org, ostride, recon8[0] + y * strideRecon8[0] + x, strideRecon8[0], 1, 1);
                for (uint32_t i = 1; i < sizeof(pri_strengths_y); i++) {
                    const int32_t pri = pri_strengths_y[i];
                    const int32_t adj_pri = adjust_strength(pri, var);
                    int32_t pitchDst = 64;
                    uint8_t *dst = dst_intermediaY[i-1] + y * pitchDst + x;
                    int32_t sse;
                    AV1PP::cdef_estimate_block_sec0(org, ostride, in, adj_pri, dir, pri_damping, &sse, 0, dst, pitchDst);
                    totsse[0][pri] += sse;
                }

                y >>= 1;
                ostride = strideOrigin[1];
                org = origin[1] + y * ostride + x;
                in  = input[1] + y * CDEF_BSTRIDE + x;

                totsse[1][0] += AV1PP::sse(org, ostride, recon8[1] + y * strideRecon8[1] + x, strideRecon8[1], 1, 0);
                for (uint32_t i = 1; i < sizeof(pri_strengths_uv); i++) {
                    const int32_t pri = pri_strengths_uv[i];
                    int32_t pitchDst = 64;
                    uint8_t *dst = dst_intermediaUv[i-1] + y * pitchDst + x;
                    int32_t sse;
                    AV1PP::cdef_estimate_block_sec0(org, ostride, in, pri, dir, pri_damping - 1, &sse, 1, dst, pitchDst);
                    totsse[1][pri] += sse;
                }
            }
        }

        int32_t bestStrengthId = 0;
        int32_t bestYuvSse = totsse[0][0] + totsse[1][0];
        for (uint32_t i = 1; i < sizeof(pri_strength_comb) / sizeof(pri_strength_comb[0]); i++) {
            const int32_t yuvSse = totsse[0][pri_strength_comb[i][0]] + totsse[1][pri_strength_comb[i][1]];
            if (bestYuvSse > yuvSse) {
                bestYuvSse = yuvSse;
                bestStrengthId = i;
            }
        }

        frame->m_cdefStrenths[sb_index[sb_count]] = bestStrengthId;

        // test: if apply needed?
        if (!frame->m_isRef && !par.doDumpRecon) return;
        if (bestStrengthId < 0) return;
        const int32_t str = frame->m_cdefParam.cdef_strengths[bestStrengthId];
        const int32_t uvStr = frame->m_cdefParam.cdef_uv_strengths[bestStrengthId];
        if ((str | uvStr) == 0) return;


        // apply: just copy on demand
        uint32_t idxY  = pri_strength_comb[bestStrengthId][0];
        int32_t idxUv = pri_strength_comb[bestStrengthId][1];

        for (int32_t r = 0; r < maxr; r++) {
            for (int32_t c = 0; c < maxc; c++) {
                int8_t dir = dirs[r][c];
                if (dir < 0) // skip
                    continue;

                int32_t x = c << 3;
                int32_t y = r << 3;

                if (idxY) {
                    int32_t pitchDst = frame->m_recon->pitch_luma_pix;
                    uint8_t *dst = frame->m_recon->y + (sbr * 64 + y) * pitchDst + (sbc * 64 + x);
                    int32_t pitchSrc = 64;
                    uint8_t* src = dst_intermediaY[idxY/2-1] + y * pitchSrc + x;
                    CopyNxN(src, 64, dst, pitchDst, 8);
                }
                if (idxUv) {
                    y = r << 2;
                    int32_t pitchDst = frame->m_recon->pitch_chroma_pix;
                    uint8_t *dst = frame->m_recon->uv + (sbr * 32 + y) * pitchDst + (sbc * 64 + x);
                    int32_t pitchSrc = 64;
                    uint8_t* src = dst_intermediaUv[idxUv/4] + y * pitchSrc + x;
                    CopyNxM(src, 64, dst, pitchDst, 8, 4);
                }
            }
        }
    }

    void CdefSearchRow(const AV1VideoParam &par, Frame *frame, int32_t row)
    {
        assert(!"not implemented");
        /*for (int32_t sbc = 0; sbc < par.sb64Cols; sbc++)
            CdefSearchSb(par, frame, row, sbc);*/
    }

    void CdefSearchFrame(const AV1VideoParam &par, Frame *frame)
    {
        for (int32_t sbr = 0; sbr < par.sb64Rows; sbr++)
            CdefSearchRow(par, frame, sbr);
    }

    void CdefSearchSync(const AV1VideoParam &par, Frame *frame)
    {
#if 0
        int8_t *cdef_strength = frame->m_cdefStrenths;
        int32_t *sb_index = frame->m_sbIndex.data();
        uint32_t sb_count = frame->m_sbCount;
        int32_t clpf_damping = 3 + (frame->m_sliceQpY >> 6);
        int32_t nplanes = 3;

        float lambda = frame->m_lambda * 512;

        int32_t (*mse[2])[STRENGTH_COUNT_FAST] = { frame->m_mse[0], frame->m_mse[1] };
#endif

#if 0
        int32_t nb_strength_bits = 0;
        uint64_t best_tot_mse = (uint64_t)1 << 63;
        /* Search for different number of signalling bits. */
        for (int32_t i = 0; i <= 3; i++) {
            int32_t best_lev0[CDEF_MAX_STRENGTHS] = { 0 };
            int32_t best_lev1[CDEF_MAX_STRENGTHS] = { 0 };

            int32_t nb_strengths = 1 << i;
            //uint64_t tot_mse = joint_strength_search(best_lev, nb_strengths, mse[0], sb_count);
            uint64_t tot_mse = joint_strength_search_dual(best_lev0, best_lev1, nb_strengths, mse, sb_count);
            /* Count superblock signalling cost. */
            tot_mse += (uint64_t)(sb_count * lambda * i);
            /* Count header signalling cost. */
            tot_mse += (uint64_t)(nb_strengths * lambda * CDEF_STRENGTH_BITS);
            if (tot_mse < best_tot_mse) {
                best_tot_mse = tot_mse;
                nb_strength_bits = i;
                for (int32_t j = 0; j < 1 << nb_strength_bits; j++) {
                    frame->m_cdefParam.cdef_strengths[j] = best_lev0[j];
                    frame->m_cdefParam.cdef_uv_strengths[j] = best_lev1[j];
                }
            }
        }
#else // 0

#if 0
        int32_t nb_strength_bits = 3;
        int32_t predefined_strengths[2][8] = {
            { 0, 2, 4, 4, 6, 6, 8, 8 },
            { 0, 0, 0, 2, 2, 4, 4, 8 }
        };
        frame->m_cdefParam.cdef_bits = 3;
        frame->m_cdefParam.nb_cdef_strengths = 1 << nb_strength_bits;
        for (int i = 0; i < 8; i++) {
            frame->m_cdefParam.cdef_strengths[i]    = 4 * predefined_strengths[0][i];
            frame->m_cdefParam.cdef_uv_strengths[i] = 4 * predefined_strengths[1][i];
        }
#endif

#endif // 0

        //frame->m_cdefParam.cdef_bits = nb_strength_bits;
        //frame->m_cdefParam.nb_cdef_strengths = 1 << nb_strength_bits;
        //for (uint32_t i = 0; i < sb_count; i++) {
        //    int32_t best_gi = 0;
        //    uint64_t best_mse = (uint64_t)1 << 63;
        //    for (int32_t gi = 0; gi < frame->m_cdefParam.nb_cdef_strengths; gi++) {
        //        uint64_t curr = mse[0][i][frame->m_cdefParam.cdef_strengths[gi]];
        //        curr += mse[1][i][frame->m_cdefParam.cdef_uv_strengths[gi]];
        //        if (curr < best_mse) {
        //            best_gi = gi;
        //            best_mse = curr;
        //        }
        //    }
        //    cdef_strength[sb_index[i]] = best_gi;
        //}

        //for (int32_t j = 0; j < frame->m_cdefParam.nb_cdef_strengths; j++) {
        //    frame->m_cdefParam.cdef_strengths[j] = frame->m_cdefParam.cdef_strengths[j] * 4;
        //    frame->m_cdefParam.cdef_uv_strengths[j] = frame->m_cdefParam.cdef_uv_strengths[j] * 4;
        //}
    }
}

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE