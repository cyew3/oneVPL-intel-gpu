// Copyright (c) 2019 Intel Corporation
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

#include "cassert"
#include "algorithm"
#include "mfx_av1_defs.h"
#include "mfx_av1_frame.h"
#include "mfx_av1_scroll_detector.h"

using namespace AV1Enc;

namespace {
    void DownScale(Frame *frame)
    {
        const int32_t dstPitch = frame->m_widthLowRes4x;
        uint8_t *dst = frame->m_lowres4x;

        const int32_t srcPitch = frame->m_origin->pitch_luma_pix;
        const uint8_t *src = frame->m_origin->y;

        const __m256i zero = _mm256_setzero_si256();
        const __m256i offset = _mm256_set1_epi16(8);
        for (int32_t y = 0; y < frame->m_heightLowRes4x; y++) {
            for (int32_t x = 0; x < frame->m_widthLowRes4x; x += 8) {
                __m256i rowA = loada_si256(src + 0 * srcPitch + 4 * x);     // A00..A31
                __m256i rowB = loada_si256(src + 1 * srcPitch + 4 * x);     // B00..B31
                __m256i rowC = loada_si256(src + 2 * srcPitch + 4 * x);     // C00..C31
                __m256i rowD = loada_si256(src + 3 * srcPitch + 4 * x);     // D00..D31
                __m256i rowAB_lo = _mm256_unpacklo_epi32(rowA, rowB);       // A00..A03 B00..B03 A04..A07 B04..B07 A16..A19 B16..B19 A20..A23 B20..B23
                __m256i rowCD_lo = _mm256_unpacklo_epi32(rowC, rowD);       // C00..C03 D00..D03 C04..C07 D04..D07 C16..C19 D16..D19 C20..C23 D20..D23
                __m256i rowAB_hi = _mm256_unpackhi_epi32(rowA, rowB);       // A08..A11 B08..B11 A12..A15 B12..B15 A24..A27 B24..B27 A28..A31 B28..B31
                __m256i rowCD_hi = _mm256_unpackhi_epi32(rowC, rowD);       // C08..C11 D08..D11 C12..C15 D12..D15 C24..C27 D24..D27 C28..C31 D28..D31
                __m256i sum_0145 = _mm256_sad_epu8(rowAB_lo, zero);
                __m256i sum_2367 = _mm256_sad_epu8(rowAB_hi, zero);
                sum_0145 = _mm256_add_epi64(sum_0145, _mm256_sad_epu8(rowCD_lo, zero));
                sum_2367 = _mm256_add_epi64(sum_2367, _mm256_sad_epu8(rowCD_hi, zero));
                __m256i sum = _mm256_packus_epi32(sum_0145, sum_2367);      // 16w: sum0 zero sum1 zero sum2 zero .. sum7 zero
                sum = _mm256_add_epi16(sum, offset);
                sum = _mm256_srli_epi16(sum, 4);
                sum = _mm256_packus_epi16(sum, sum);                        // 32b: (avg0 zero avg1 zero avg2 zero avg3 zero)x2 (avg4 zero avg5 zero avg6 zero avg7 zero)x2
                sum = _mm256_packus_epi16(sum, sum);                        // 32b: (avg0 avg1 avg2 avg3)x4 (avg4 avg5 avg6 avg7)x4
                storel_si32(dst + x + 0, si128_lo(sum));
                storel_si32(dst + x + 4, si128_hi(sum));
            }
            src += 4 * srcPitch;
            dst += dstPitch;
        }
    }

#define _mm_movehl_epi64(A, B) _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(A), _mm_castsi128_ps(B)))

    static int32_t sad16x16(const uint8_t* src, int32_t srcPitch, const uint8_t* ref, int32_t refPitch)
    {
        assert(((uintptr_t)src & 15) == 0);
        assert(((uintptr_t)ref & 15) == 0);
        assert((srcPitch & 15) == 0);
        assert((refPitch & 15) == 0);
        const int32_t srcPitch2 = srcPitch * 2;
        const int32_t refPitch2 = refPitch * 2;

        __m128i sad0 = _mm_sad_epu8(loada_si128(ref), loada_si128(src));
        __m128i sad1 = _mm_sad_epu8(loada_si128(ref + refPitch), loada_si128(src + srcPitch));
        for (int i = 2; i < 16; i += 2) {
            ref += refPitch2;
            src += srcPitch2;
            sad0 = _mm_add_epi32(sad0,
                _mm_sad_epu8(loada_si128(ref), loada_si128(src)));
            sad1 = _mm_add_epi32(sad1,
                _mm_sad_epu8(loada_si128(ref + refPitch), loada_si128(src + srcPitch)));
        }
        sad0 = _mm_add_epi32(sad0, sad1);
        sad1 = _mm_movehl_epi64(sad1, sad0);
        sad1 = _mm_add_epi32(sad1, sad0);
        return _mm_cvtsi128_si32(sad1);
    }

    static void sad16x16x2(
        const uint8_t* src, int32_t srcPitch, const uint8_t* ref, int32_t refPitch,
        int32_t *outSad0, int32_t *outSad1)
    {
        assert(((uintptr_t)src & 31) == 0);
        assert(((uintptr_t)ref & 31) == 0);
        assert((srcPitch & 31) == 0);
        assert((refPitch & 31) == 0);
        const int32_t srcPitch2 = srcPitch * 2;
        const int32_t refPitch2 = refPitch * 2;

        __m256i sad0 = _mm256_sad_epu8(loada_si256(ref), loada_si256(src));
        __m256i sad1 = _mm256_sad_epu8(loada_si256(ref + refPitch), loada_si256(src + srcPitch));
        for (int i = 2; i < 16; i += 2) {
            ref += refPitch2;
            src += srcPitch2;
            sad0 = _mm256_add_epi32(sad0,
                _mm256_sad_epu8(loada_si256(ref), loada_si256(src)));
            sad1 = _mm256_add_epi32(sad1,
                _mm256_sad_epu8(loada_si256(ref + refPitch), loada_si256(src + srcPitch)));
        }
        sad0 = _mm256_add_epi32(sad0, sad1);   // s0 00 s1 00 | s2 00 s3 00
        *outSad0 = _mm256_extract_epi32(sad0, 0) + _mm256_extract_epi32(sad0, 2);
        *outSad1 = _mm256_extract_epi32(sad0, 4) + _mm256_extract_epi32(sad0, 6);
    }

    void FindVertMv2(const uint8_t* src, int32_t srcPitch, const uint8_t* ref, int32_t refPitch, int32_t start_y, int32_t height, volatile uint32_t *hist)
    {
        int32_t bestCost0 = INT_MAX;
        int32_t bestMv0 = 0;
        int32_t bestCost1 = INT_MAX;
        int32_t bestMv1 = 0;

        for (int32_t y = 0; y < height; y++) {
            int32_t cost0, cost1;
            sad16x16x2(src + start_y * srcPitch, srcPitch, ref + y * refPitch, refPitch, &cost0, &cost1);

            if ((cost0 < bestCost0) || (cost0 == bestCost0 && y == start_y)) {
                bestCost0 = cost0;
                bestMv0 = y - start_y;
            }

            if ((cost1 < bestCost1) || (cost1 == bestCost1 && y == start_y)) {
                bestCost1 = cost1;
                bestMv1 = y - start_y;
            }
        }

        if (bestMv0 != 0)
            vm_interlocked_inc32(&hist[bestMv0]);

        if (bestMv1 != 0)
            vm_interlocked_inc32(&hist[bestMv1]);
    }
};  // namespace


void AV1Enc::DetectScrollStart(const ThreadingTask &task)
{
    DownScale(task.frame);
    if (task.frame->m_picCodeType == MFX_FRAMETYPE_I)
        return;

    std::fill(std::begin(task.frame->m_vertMvHist), std::end(task.frame->m_vertMvHist), 0u);
}

void AV1Enc::DetectScrollRoutine(const ThreadingTask &task)
{
    if (task.frame->m_picCodeType == MFX_FRAMETYPE_I)
        return;

    const int32_t width = task.frame->m_widthLowRes4x;
    const int32_t height = task.frame->m_heightLowRes4x;
    const uint32_t numBlocks = (width >> 4) * (height >> 4);
    if (numBlocks < 300)
        return;

    const int32_t pitch = width;
    const uint8_t *src = task.frame->m_lowres4x;
    const uint8_t *ref = task.frame->refFramesVp9[LAST_FRAME]->m_lowres4x;

    uint32_t *mvHist = task.frame->m_vertMvHist.data() + height;

    assert(task.startX % 32 == 0);
    assert(task.endX % 32 == 0);
    for (int32_t x = task.startX; x < task.endX; x += 32)
        for (int32_t y = 0; y < height; y += 16)
            FindVertMv2(src + x, pitch, ref + x, pitch, y, height - 15, mvHist);
}

void AV1Enc::DetectScrollEnd(const ThreadingTask &task)
{
    task.frame->m_globalMvy = 0;
    if (task.frame->m_picCodeType == MFX_FRAMETYPE_I)
        return;

    const int32_t width = task.frame->m_widthLowRes4x;
    const int32_t height = task.frame->m_heightLowRes4x;
    const uint32_t *mvHist = task.frame->m_vertMvHist.data() + height;
    const uint32_t *maxCounter = std::max_element(mvHist - height, mvHist + height);
    const int16_t globMvy = 4 * int16_t(maxCounter - mvHist);
    const uint32_t numBlocks = (width >> 4) * (height >> 4);
    const uint32_t threshold = std::max(2u, numBlocks / 32);
    if (*maxCounter >= threshold)
        task.frame->m_globalMvy = globMvy;
}
