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
#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "mfx_av1_tables.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_enc.h"
#include "mfx_av1_encode.h"
#include "mfx_av1_bitwriter.h"
#include "mfx_av1_scan.h"
#include "mfx_av1_scan.h"
#include "mfx_av1_prob_opt.h"
#include "mfx_av1_probabilities.h"
#include "mfx_av1_get_context.h"
#include "mfx_av1_binarization.h"

#include <algorithm>

#if USE_CMODEL_PAK
#include "av1_pak_hw.h"
#endif
#ifdef _DEBUG
#define AV1_DEBUG_CONTEXT_MEMOIZATION
#endif

using namespace AV1Enc;
using namespace MfxEnumShortAliases;

namespace {
    enum { IVF_HEADER_SIZE=8*32, IVF_FRAME_HEADER_SIZE=8*12 };
    enum { KEY_FRAME=0, NON_KEY_FRAME=1 };
    enum { VP9_FRAME_MARKER=2, VP9_SUPERFRAME_MARKER=6, VP9_SYNC_CODE_0=0x49, VP9_SYNC_CODE_1=0x83, VP9_SYNC_CODE_2=0x42 };
    enum { CS_UNKNOWN=0, CS_BT_601=1, CS_BT_709=2, CS_SMPTE_170=3, CS_SMPTE_240=4, CS_BT_2020=5, CS_RESERVED=6, CS_RGB=7 };
    enum { CR_STUDIO_RANGE=0, CR_FULL_RANGE=1 };
    enum { OBU_FORBIDDEN_BIT=0, OBU_RESERVED_2BITS=0 };
    enum { OBU_SEQUENCE_HEADER=1, OBU_TD=2, OBU_FRAME_HEADER=3, OBU_TILE_GROUP=4, OBU_METADATA=5, OBU_FRAME=6, OBU_PADDING=15 };

    enum { AOM_FRAME_MARKER = 2, AOM_SUPERFRAME_MARKER = 6, AOM_SYNC_CODE_0 = 0x49, AOM_SYNC_CODE_1 = 0x83, AOM_SYNC_CODE_2 = 0x43 };

    enum {
        CP_BT_709       =  1, // BT.709
        CP_UNSPECIFIED  =  2, // Unspecified
        CP_BT_470_M     =  4, // BT.470 System M (historical)
        CP_BT_470_B_G   =  5, // BT.470 System B, G (historical)
        CP_BT_601       =  6, // BT.601
        CP_SMPTE_240    =  7, // SMPTE 240
        CP_GENERIC_FILM =  8, // Generic film (color filters using illuminant C)
        CP_BT_2020      =  9, // BT.2020, BT.2100
        CP_XYZ          = 10, // SMPTE 428 (CIE 1921 XYZ)
        CP_SMPTE_431    = 11, // SMPTE RP 431-2
        CP_SMPTE_432    = 12, // SMPTE EG 432-1
        CP_EBU_3213     = 22, // EBU Tech. 3213-E
    };

    enum {
        TC_RESERVED_0     =  0, // For future use
        TC_BT_709         =  1, // BT.709
        TC_UNSPECIFIED    =  2, // Unspecified
        TC_RESERVED_3     =  3, // For future use
        TC_BT_470_M       =  4, // BT.470 System M (historical)
        TC_BT_470_B_G     =  5, // BT.470 System B, G (historical)
        TC_BT_601         =  6, // BT.601
        TC_SMPTE_240      =  7, // SMPTE 240 M
        TC_LINEAR         =  8, // Linear
        TC_LOG_100        =  9, // Logarithmic (100 : 1 range)
        TC_LOG_100_SQRT10 = 10, // Logarithmic (100 * Sqrt(10) : 1 range)
        TC_IEC_61966      = 11, // IEC 61966-2-4
        TC_BT_1361        = 12, // BT.1361
        TC_SRGB           = 13, // sRGB or sYCC
        TC_BT_2020_10_BIT = 14, // BT.2020 10-bit systems
        TC_BT_2020_12_BIT = 15, // BT.2020 12-bit systems
        TC_SMPTE_2084     = 16, // SMPTE ST 2084, ITU BT.2100 PQ
        TC_SMPTE_428      = 17, // SMPTE ST 428
        TC_HLG            = 18, // BT.2100 HLG, ARIB STD-B67
    };

    enum {
        MC_IDENTITY    =  0, // Identity matrix
        MC_BT_709      =  1, // BT.709
        MC_UNSPECIFIED =  2, // Unspecified
        MC_RESERVED_3  =  3, // For future use
        MC_FCC         =  4, // US FCC 73.628
        MC_BT_470_B_G  =  5, // BT.470 System B, G (historical)
        MC_BT_601      =  6, // BT.601
        MC_SMPTE_240   =  7, // SMPTE 240 M
        MC_SMPTE_YCGCO =  8, // YCgCo
        MC_BT_2020_NCL =  9, // BT.2020 non-constant luminance, BT.2100 YCbCr
        MC_BT_2020_CL  = 10, // BT.2020 constant luminance
        MC_SMPTE_2085  = 11, // SMPTE ST 2085 YDzDx
        MC_CHROMAT_NCL = 12, // Chromaticity-derived non-constant luminance
        MC_CHROMAT_CL  = 13, // Chromaticity-derived constant luminance
        MC_ICTCP       = 14, // BT.2100 ICtCp
    };

    enum {
        CSP_UNKNOWN   = 0, // Unknown (in this case the source video transfer function must be signaled outside the AV1 bitstream)
        CSP_VERTICAL  = 1, // Horizontally co-located with (0, 0) luma sample, vertical position in the middle between two luma samples
        CSP_COLOCATED = 2, // co-located with (0, 0) luma sample
        CSP_RESERVED  = 3,
    };

    enum { AV1_MIN_TILE_SIZE_BYTES = 1 };

    void Put8(uint8_t *buf, int32_t val) { *buf = (uint8_t)val; }
    void Put16LE(uint8_t *buf, int32_t val) { Put8(buf, val & 0xFF); Put8(buf+1, val >> 8); }
    void Put16BE(uint8_t *buf, int32_t val) { Put8(buf, val >> 8); Put8(buf+1, val & 0xFF); }
    void Put32LE(uint8_t *buf, int32_t val) { Put16LE(buf, val & 0xFFFF); Put16LE(buf+2, val >> 16); }
    void Put32BE(uint8_t *buf, int32_t val) { Put16BE(buf, val >> 16); Put16BE(buf+2, val & 0xFFFF); }
    void Put64LE(uint8_t *buf, int64_t val) { Put32LE(buf, (int32_t)(val & 0xFFFFFFFF)); Put32LE(buf+4, (int32_t)(val >> 32)); }
    void Put64BE(uint8_t *buf, int64_t val) { Put32BE(buf, (int32_t)(val >> 32)); Put32BE(buf+4, (int32_t)(val & 0xFFFFFFFF)); }

    void PutBit(uint8_t *buf, int32_t &off, int32_t bit)
    {
        //trace_putbit(bit);
        int32_t byteOff = off >> 3;
        int32_t bitOff = off & 7;
        if (bitOff == 0) {
            buf[byteOff] = bit << 7;
        } else {
            int32_t shift = 7 - bitOff;
            int32_t mask = ~(1 << shift);
            buf[byteOff] = (buf[byteOff] & mask) | (bit << shift);
        }
        ++off;
    }

    void PutBits(uint8_t *buf, int32_t &off, int32_t val, int32_t nbits) {
        while (nbits-->0) PutBit(buf, off, (val >> nbits) & 1);
    }

    void PutTrailingZeroBits(uint8_t *buf, int32_t &off) {
        int32_t numTrailingZeroBits = (8 - (off & 7)) & 7;
        PutBits(buf, off, 0, numTrailingZeroBits); // trailing zero bit
    }

    void WriteSyncCode(uint8_t *buf, int32_t &off, const AV1VideoParam &par) {
        PutBits(buf, off, AOM_SYNC_CODE_0, 8);
        PutBits(buf, off, AOM_SYNC_CODE_1, 8);
        PutBits(buf, off, AOM_SYNC_CODE_2, 8);
    }

    void WriteColorConfigVp9(uint8_t *buf, int32_t &off, const AV1VideoParam &par) {
        int32_t color_space = CS_UNKNOWN;
        int32_t color_range = CR_STUDIO_RANGE;

        if (par.Profile >= 2)
            PutBit(buf, off, int32_t(par.bitDepthLuma == 12));
        PutBits(buf, off, color_space, 3);
        if (color_space != CS_RGB) {
            PutBit(buf, off, color_range);
            if (par.Profile == 1 || par.Profile == 3) {
                PutBit(buf, off, par.subsamplingX);
                PutBit(buf, off, par.subsamplingY);
                PutBit(buf, off, 0);    // reserved_zero
            }
        } else {
            if (par.Profile == 1 || par.Profile == 3)
                PutBit(buf, off, 0);    // reserved_zero
        }
    }

    void WriteColorConfigAv1(uint8_t *buf, int32_t &off, const AV1VideoParam &par) {
#if USE_SRC10ENC8PAK10
        const int32_t high_bitdepth = par.src10Enable;
#else
        const int32_t high_bitdepth = (par.bitDepthLuma > 8);
#endif
        const int32_t twelve_bit = (par.bitDepthLuma == 12);
        const int32_t mono_chrome = 0;
        const int32_t color_description_present_flag = 0;
        const int32_t color_range = CR_STUDIO_RANGE;
        const int32_t color_primaries = CP_UNSPECIFIED;
        const int32_t transfer_characteristics = TC_UNSPECIFIED;
        const int32_t matrix_coefficients = MC_UNSPECIFIED;
        const int32_t chroma_sample_position = CSP_UNKNOWN;

        PutBit(buf, off, high_bitdepth);
        if (par.Profile >= 2)
            PutBit(buf, off, twelve_bit);

        if (par.Profile != 1)
            PutBit(buf, off, mono_chrome);

        PutBit(buf, off, color_description_present_flag);

        if (mono_chrome) {
            assert(0);
            return;
        }
        if (color_primaries == CP_BT_709 &&
            transfer_characteristics == TC_SRGB &&
             matrix_coefficients == MC_IDENTITY) {
            assert(0);
        } else {
            PutBit(buf, off, color_range);
            if (par.Profile == 0) {
            } else if (par.Profile == 1) {
                assert(0);
            } else {
                assert(0);
            }
            if (par.subsamplingX && par.subsamplingY)
                PutBits(buf, off, chroma_sample_position, 2);
        }

        PutBit(buf, off, par.seqParams.separateUvDeltaQ);
    }

    void WriteFrameSize(uint8_t *buf, int32_t &off, const AV1VideoParam &par) {
        PutBits(buf, off, par.Width - 1, 16);
        PutBits(buf, off, par.Height - 1, 16);
    }

    void WriteRenderSize(uint8_t *buf, int32_t &off) {
        PutBit(buf, off, 0);    // render_and_frame_size_different
    }

    void WriteLoopFilterParamsVp9(uint8_t *buf, int32_t &off, const Frame &frame) {
        PutBits(buf, off, frame.m_loopFilterParam.level, 6);
        PutBits(buf, off, frame.m_loopFilterParam.sharpness, 3);
        PutBits(buf, off, frame.m_loopFilterParam.deltaEnabled, 1);
        if (frame.m_loopFilterParam.deltaEnabled) {
            assert(frame.m_loopFilterParam.deltaUpdate == 0);
            PutBits(buf, off, frame.m_loopFilterParam.deltaUpdate, 1);
        }
    }

    void WriteLoopFilterParamsAv1(uint8_t *buf, int32_t &off, const Frame &frame) {
        PutBits(buf, off, frame.m_loopFilterParam.level, 6); // y hor
        PutBits(buf, off, frame.m_loopFilterParam.level, 6); // y ver
        if (frame.m_loopFilterParam.level) {
            PutBits(buf, off, frame.m_loopFilterParam.level, 6);    // u
            PutBits(buf, off, frame.m_loopFilterParam.level, 6);    // v
        }
        PutBits(buf, off, frame.m_loopFilterParam.sharpness, 3);
        PutBits(buf, off, frame.m_loopFilterParam.deltaEnabled, 1);
        if (frame.m_loopFilterParam.deltaEnabled) {
            assert(frame.m_loopFilterParam.deltaUpdate == 0);
            PutBits(buf, off, frame.m_loopFilterParam.deltaUpdate, 1);
        }
    }


    void WriteCdef(uint8_t *buf, int32_t &off, const Frame &frame)
    {
        const int32_t damping = 3 + (frame.m_sliceQpY >> 6);
        PutBits(buf, off, damping - 3, 2);
        PutBits(buf, off, frame.m_cdefParam.cdef_bits, 2);
        for (int32_t i = 0; i < frame.m_cdefParam.nb_cdef_strengths; i++) {
            PutBits(buf, off, frame.m_cdefParam.cdef_strengths[i], CDEF_STRENGTH_BITS);
            PutBits(buf, off, frame.m_cdefParam.cdef_uv_strengths[i], CDEF_STRENGTH_BITS);
        }
    }

    void WriteRestorationMode(uint8_t *buf, int32_t &off, const Frame &frame)
    {
        if (frame.m_par->lrFlag) {
            PutBits(buf, off, 2, 2);  // y - wiener
            PutBits(buf, off, 2, 2);  // u - wiener
            PutBits(buf, off, 2, 2);  // v - wiener

            PutBit(buf, off, 0); // 64x64 for y
            PutBit(buf, off, 1); // 32x32 for u/v
        }
        else {
            PutBits(buf, off, 0, 2);  // y
            PutBits(buf, off, 0, 2);  // u
            PutBits(buf, off, 0, 2);  // v
        }
    }

    void WriteQuantizationParamsAv1(uint8_t *buf, int32_t &off, const AV1VideoParam &par, const Frame &frame) {
        const int32_t diff_uv_delta = 0;
        const int32_t using_qmatrix = 0;

        PutBits(buf, off, frame.m_sliceQpY, 8);
        PutBit(buf, off, 0); // delta_coded y_dc
        if (par.seqParams.separateUvDeltaQ)
            PutBit(buf, off, diff_uv_delta);
        PutBit(buf, off, 0); // delta_coded uv_dc
        PutBit(buf, off, 0); // delta_coded uv_ac
        if (diff_uv_delta)
            assert(0);
        PutBit(buf, off, using_qmatrix); // using_qmatrix
        if (using_qmatrix)
            assert(0);
    }

    void WriteQuantizationParamsVp9(uint8_t *buf, int32_t &off, const Frame &frame) {
        PutBits(buf, off, frame.m_sliceQpY, 8);
        PutBit(buf, off, 0); // delta_coded y_dc
        PutBit(buf, off, 0); // delta_coded uv_dc
        PutBit(buf, off, 0); // delta_coded uv_ac
    }

    void WriteSegmentationParams(uint8_t *buf, int32_t &off, const Frame &frame) {
        PutBit(buf, off, frame.segmentationEnabled);
        if (frame.segmentationEnabled) {
            assert(0);
        }
    }

    void WriteUniform(uint8_t *buf, int32_t &off, int32_t n, int32_t v)
    {
        const int32_t l = n ? BSR(n) + 1 : 0;
        const int32_t m = (1 << l) - n;
        if (l == 0)
            return;
        if (v < m) {
            PutBits(buf, off, v, l - 1);
        } else {
            PutBits(buf, off, m + ((v - m) >> 1), l - 1);
            PutBits(buf, off, (v - m) & 1, 1);
        }
    }

    void WriteTileInfoAv1(uint8_t *buf, int32_t &off, const AV1VideoParam &par) {
        const int32_t loop_filter_across_tiles_enabled = 1;
        const int32_t tile_size_bytes = 4;

        PutBit(buf, off, par.tileParam.uniformSpacing);

        if (par.tileParam.uniformSpacing) {
            for (int32_t i = par.tileParam.minLog2Cols; i < par.tileParam.log2Cols; i++)
                PutBit(buf, off, 1);
            if (par.tileParam.log2Cols < par.tileParam.maxLog2Cols)
                PutBit(buf, off, 0);

            for (int32_t i = par.tileParam.minLog2Rows; i < par.tileParam.log2Rows; i++)
                PutBit(buf, off, 1);
            if (par.tileParam.log2Rows < par.tileParam.maxLog2Rows)
                PutBit(buf, off, 0);
        } else {
            assert(0);
        }

        if (par.tileParam.log2Cols + par.tileParam.log2Rows > 0) {
            // tile id used for cdf update
            PutBits(buf, off, 0, par.tileParam.log2Cols + par.tileParam.log2Rows);
            // Number of bytes in tile size - 1
            PutBits(buf, off, tile_size_bytes - 1, 2);
        }
    }

    void WriteTileInfoVp9(uint8_t *buf, int32_t &off, const AV1VideoParam &par) {
        for (int32_t i = par.tileParam.minLog2Cols; i < par.tileParam.log2Cols; i++)
            PutBit(buf, off, 1);
        if (par.tileParam.log2Cols < par.tileParam.maxLog2Cols)
            PutBit(buf, off, 0);
        PutBit(buf, off, par.tileParam.log2Rows != 0);
        if (par.tileParam.log2Rows != 0)
            PutBit(buf, off, par.tileParam.log2Rows != 1);
    }

    void WriteIvfFrameHeader(uint8_t *buf, int32_t &bitoff, int32_t frameSize, int32_t timeStamp)
    {
        buf += bitoff>>3;
        Put32LE(buf+0, frameSize);
        Put64LE(buf+4, timeStamp);
        bitoff += IVF_FRAME_HEADER_SIZE;
    }

    void WriteSuperFrameHeader(uint8_t *buf, int32_t &off, const Frame &frame, const AV1VideoParam &par,
                               int32_t szBytes)
    {
        uint32_t maxFrameSizeInBytes = frame.frameSizeInBytes;
        int32_t numHiddenFrames = (int32_t)frame.m_hiddenFrames.size();
        if (szBytes == 0) {
            for (int32_t i = 0; i < numHiddenFrames; i++)
                maxFrameSizeInBytes = std::max(maxFrameSizeInBytes, frame.m_hiddenFrames[i]->frameSizeInBytes);
            szBytes = (BSR(maxFrameSizeInBytes) >> 3) + 1;
        }

        // superframe header
        PutBits(buf, off, VP9_SUPERFRAME_MARKER, 3);
        PutBits(buf, off, szBytes - 1, 2);
        PutBits(buf, off, numHiddenFrames, 3);
        // superframe index
        for (int32_t i = 0; i < numHiddenFrames; i++) {
            int32_t frameSize = frame.m_hiddenFrames[i]->frameSizeInBytes-1;
            for (int32_t i = 0; i < szBytes; i++, frameSize >>= 8)
                PutBits(buf, off, frameSize, 8);
        }
        // superframe header
        PutBits(buf, off, VP9_SUPERFRAME_MARKER, 3);
        PutBits(buf, off, szBytes - 1, 2);
        PutBits(buf, off, numHiddenFrames, 3);
    }

    void CopyProbs(const void *src, void *dst, size_t size) {
        _ippsCopy((const uint8_t *)src, (uint8_t *)dst, size);
    }

    void build_tail_cdfs(aom_cdf_prob cdf_tail[CDF_SIZE(ENTROPY_TOKENS)], aom_cdf_prob cdf_head[CDF_SIZE(ENTROPY_TOKENS)], int band_zero) {
        int probNZ, prob1, prob_idx, i;
        int phead[HEAD_TOKENS + 1], sum, p;
        const int is_dc = !!band_zero;
        aom_cdf_prob prev_cdf = 0;
        for (i = 0; i < HEAD_TOKENS + is_dc; ++i) {
            phead[i] = AOM_ICDF(cdf_head[i]) - prev_cdf;
            prev_cdf = AOM_ICDF(cdf_head[i]);
        }
        // Do the tail
        probNZ = CDF_PROB_TOP - phead[ZERO_TOKEN + is_dc] - (is_dc ? phead[0] : 0);
        prob1 = phead[is_dc + ONE_TOKEN_EOB] + phead[is_dc + ONE_TOKEN_NEOB];
        prob_idx = std::min(COEFF_PROB_MODELS - 1, std::max(0, ((256 * prob1) / probNZ) - 1));

        sum = 0;
        for (i = 0; i < TAIL_TOKENS; ++i) {
            sum += av1_pareto8_tail_probs[prob_idx][i];
            cdf_tail[i] = AOM_ICDF(sum);
        }
    }

    typedef aom_cdf_prob coeff_cdf_table[TX_SIZES][PLANE_TYPES][REF_TYPES][COEF_BANDS][COEFF_CONTEXTS][CDF_SIZE(ENTROPY_TOKENS)];
    static const coeff_cdf_table *default_qctx_coef_cdfs[TOKEN_CDF_Q_CTXS] = {
        &default_coef_head_cdf_q0, &default_coef_head_cdf_q1,
        &default_coef_head_cdf_q2, &default_coef_head_cdf_q3
    };

    void SetDefaultCoefProbs(Frame *frame)
    {
        const int32_t index = get_q_ctx(frame->m_sliceQpY);
        for (size_t i = 0; i < frame->m_tileContexts.size(); i++) {
            EntropyContexts &ectx = frame->m_tileContexts[i].ectx;
            CopyProbs(default_txb_skip_cdfs[index],       ectx.txb_skip_cdf,       sizeof(ectx.txb_skip_cdf));
            CopyProbs(default_eob_extra_cdfs[index],      ectx.eob_extra_cdf,      sizeof(ectx.eob_extra_cdf));
            CopyProbs(default_dc_sign_cdfs[index],        ectx.dc_sign_cdf,        sizeof(ectx.dc_sign_cdf));
            CopyProbs(default_eob_multi16_cdfs[index],    ectx.eob_flag_cdf16,     sizeof(ectx.eob_flag_cdf16));
            CopyProbs(default_eob_multi32_cdfs[index],    ectx.eob_flag_cdf32,     sizeof(ectx.eob_flag_cdf32));
            CopyProbs(default_eob_multi64_cdfs[index],    ectx.eob_flag_cdf64,     sizeof(ectx.eob_flag_cdf64));
            CopyProbs(default_eob_multi128_cdfs[index],   ectx.eob_flag_cdf128,    sizeof(ectx.eob_flag_cdf128));
            CopyProbs(default_eob_multi256_cdfs[index],   ectx.eob_flag_cdf256,    sizeof(ectx.eob_flag_cdf256));
            CopyProbs(default_eob_multi512_cdfs[index],   ectx.eob_flag_cdf512,    sizeof(ectx.eob_flag_cdf512));
            CopyProbs(default_eob_multi1024_cdfs[index],  ectx.eob_flag_cdf1024,   sizeof(ectx.eob_flag_cdf1024));
            CopyProbs(default_coeff_base_eob_cdfs[index], ectx.coeff_base_eob_cdf, sizeof(ectx.coeff_base_eob_cdf));
            CopyProbs(default_coeff_base_cdfs[index],     ectx.coeff_base_cdf,     sizeof(ectx.coeff_base_cdf));
            CopyProbs(default_coeff_lps_cdfs[index],      ectx.coeff_br_cdf,       sizeof(ectx.coeff_br_cdf));
        }
    }

    void SetupPastIndependence(Frame *frame, int32_t isVp9) {

        if (isVp9) {
            FrameContexts &fctx = frame->m_contexts;
            CopyProbs(default_partition_probs,      fctx.partition,      sizeof(fctx.partition));
            CopyProbs(default_skip_prob,            fctx.skip,           sizeof(fctx.skip));
            CopyProbs(default_is_inter_prob,        fctx.is_inter,       sizeof(fctx.is_inter));
            CopyProbs(default_tx_probs,             fctx.tx,             sizeof(fctx.tx));
            CopyProbs(vp9_default_y_mode_probs,     fctx.vp9_y_mode,     sizeof(fctx.vp9_y_mode));
            CopyProbs(vp9_default_uv_mode_probs,    fctx.vp9_uv_mode,    sizeof(fctx.vp9_uv_mode));
            CopyProbs(default_coef_probs,           fctx.coef,           sizeof(fctx.coef));
            CopyProbs(vp9_default_single_ref_prob,  fctx.vp9_single_ref, sizeof(fctx.vp9_single_ref));
            CopyProbs(default_inter_mode_probs,     fctx.inter_mode,     sizeof(fctx.inter_mode));
            CopyProbs(default_interp_filter_probs_av1,  fctx.interp_filter,  sizeof(fctx.interp_filter));
            CopyProbs(default_comp_mode_probs,      fctx.comp_mode,      sizeof(fctx.comp_mode));
            CopyProbs(default_comp_ref_type_probs,  fctx.comp_ref_type,  sizeof(fctx.comp_ref_type));
            CopyProbs(vp9_default_comp_ref_probs,   fctx.vp9_comp_ref,   sizeof(fctx.vp9_comp_ref));
            CopyProbs(&vp9_default_nmv_context,     &fctx.mv_contexts,   sizeof(fctx.mv_contexts));
        }

        for (size_t i = 0; i < frame->m_tileContexts.size(); i++) {
            EntropyContexts &ectx = frame->m_tileContexts[i].ectx;
            CopyProbs(default_skip_cdf,                ectx.skip_cdf,                 sizeof(ectx.skip_cdf));
            CopyProbs(default_intra_inter_cdf,         ectx.intra_inter_cdf,          sizeof(ectx.intra_inter_cdf));
            CopyProbs(default_comp_inter_cdf,          ectx.comp_inter_cdf,           sizeof(ectx.comp_inter_cdf));
            CopyProbs(default_single_ref_cdf,          ectx.single_ref_cdf,           sizeof(ectx.single_ref_cdf));
            CopyProbs(default_comp_ref_type_cdf,       ectx.comp_ref_type_cdf,        sizeof(ectx.comp_ref_type_cdf));
            CopyProbs(default_comp_ref_cdf,            ectx.comp_ref_cdf,             sizeof(ectx.comp_ref_cdf));
            CopyProbs(default_comp_bwdref_cdf,         ectx.comp_bwdref_cdf,          sizeof(ectx.comp_bwdref_cdf));
            CopyProbs(default_newmv_cdf,               ectx.newmv_cdf,                sizeof(ectx.newmv_cdf));
            CopyProbs(default_zeromv_cdf,              ectx.zeromv_cdf,               sizeof(ectx.zeromv_cdf));
            CopyProbs(default_refmv_cdf,               ectx.refmv_cdf,                sizeof(ectx.refmv_cdf));
            CopyProbs(default_drl_cdf,                 ectx.drl_cdf,                  sizeof(ectx.drl_cdf));
            CopyProbs(default_av1_kf_y_mode_cdf,       ectx.kf_y_cdf,                 sizeof(ectx.kf_y_cdf));
            CopyProbs(default_if_y_mode_cdf,           ectx.y_mode_cdf,               sizeof(ectx.y_mode_cdf));
            CopyProbs(default_uv_mode_cdf,             ectx.uv_mode_cdf,              sizeof(ectx.uv_mode_cdf));
            CopyProbs(default_angle_delta_cdf,         ectx.angle_delta_cdf,          sizeof(ectx.angle_delta_cdf));
            CopyProbs(default_filter_intra_cdfs,       ectx.filter_intra_cdfs,        sizeof(ectx.filter_intra_cdfs));
            CopyProbs(default_switchable_interp_cdf,   ectx.switchable_interp_cdf,    sizeof(ectx.switchable_interp_cdf));
            CopyProbs(default_partition_cdf,           ectx.partition_cdf,            sizeof(ectx.partition_cdf));
            CopyProbs(default_inter_mode_cdf,          ectx.inter_mode_cdf,           sizeof(ectx.inter_mode_cdf));
            CopyProbs(default_inter_compound_mode_cdf, ectx.inter_compound_mode_cdf,  sizeof(ectx.inter_compound_mode_cdf));
            CopyProbs(default_intra_ext_tx_cdf,        ectx.intra_ext_tx_cdf,         sizeof(ectx.intra_ext_tx_cdf));
            CopyProbs(default_inter_ext_tx_cdf,        ectx.inter_ext_tx_cdf,         sizeof(ectx.inter_ext_tx_cdf));
            CopyProbs(default_tx_size_cdf,             ectx.tx_size_cdf,              sizeof(ectx.tx_size_cdf));
            CopyProbs(default_motion_mode_cdf,         ectx.motion_mode_cdf,          sizeof(ectx.motion_mode_cdf));
            CopyProbs(default_obmc_cdf,                ectx.obmc_cdf,                 sizeof(ectx.obmc_cdf));
            CopyProbs(default_txfm_partition_cdf,      ectx.txfm_partition_cdf,       sizeof(ectx.txfm_partition_cdf));
            CopyProbs(&av1_default_nmv_context,        &ectx.mv_contexts,             sizeof(ectx.mv_contexts));
        }
        SetDefaultCoefProbs(frame);
    }

    void SetupPastIndependenceAv1(Frame *frame, int32_t tile)
    {
        //for (size_t i = 0; i < frame->m_tileContexts.size(); i++)
        const size_t i = tile;
        {
            EntropyContexts &ectx = frame->m_tileContexts[i].ectx;
            CopyProbs(default_skip_cdf,                ectx.skip_cdf,                 sizeof(ectx.skip_cdf));
            CopyProbs(default_intra_inter_cdf,         ectx.intra_inter_cdf,          sizeof(ectx.intra_inter_cdf));
            CopyProbs(default_comp_inter_cdf,          ectx.comp_inter_cdf,           sizeof(ectx.comp_inter_cdf));
            CopyProbs(default_single_ref_cdf,          ectx.single_ref_cdf,           sizeof(ectx.single_ref_cdf));
            CopyProbs(default_comp_ref_type_cdf,       ectx.comp_ref_type_cdf,        sizeof(ectx.comp_ref_type_cdf));
            CopyProbs(default_comp_ref_cdf,            ectx.comp_ref_cdf,             sizeof(ectx.comp_ref_cdf));
            CopyProbs(default_comp_bwdref_cdf,         ectx.comp_bwdref_cdf,          sizeof(ectx.comp_bwdref_cdf));
            CopyProbs(default_newmv_cdf,               ectx.newmv_cdf,                sizeof(ectx.newmv_cdf));
            CopyProbs(default_zeromv_cdf,              ectx.zeromv_cdf,               sizeof(ectx.zeromv_cdf));
            CopyProbs(default_refmv_cdf,               ectx.refmv_cdf,                sizeof(ectx.refmv_cdf));
            CopyProbs(default_drl_cdf,                 ectx.drl_cdf,                  sizeof(ectx.drl_cdf));
            CopyProbs(default_av1_kf_y_mode_cdf,       ectx.kf_y_cdf,                 sizeof(ectx.kf_y_cdf));
            CopyProbs(default_if_y_mode_cdf,           ectx.y_mode_cdf,               sizeof(ectx.y_mode_cdf));
            CopyProbs(default_uv_mode_cdf,             ectx.uv_mode_cdf,              sizeof(ectx.uv_mode_cdf));
            CopyProbs(default_angle_delta_cdf,         ectx.angle_delta_cdf,          sizeof(ectx.angle_delta_cdf));
            CopyProbs(default_filter_intra_cdfs,       ectx.filter_intra_cdfs,        sizeof(ectx.filter_intra_cdfs));
            CopyProbs(default_switchable_interp_cdf,   ectx.switchable_interp_cdf,    sizeof(ectx.switchable_interp_cdf));
            CopyProbs(default_partition_cdf,           ectx.partition_cdf,            sizeof(ectx.partition_cdf));
            CopyProbs(default_inter_mode_cdf,          ectx.inter_mode_cdf,           sizeof(ectx.inter_mode_cdf));
            CopyProbs(default_inter_compound_mode_cdf, ectx.inter_compound_mode_cdf,  sizeof(ectx.inter_compound_mode_cdf));
            CopyProbs(default_intra_ext_tx_cdf,        ectx.intra_ext_tx_cdf,         sizeof(ectx.intra_ext_tx_cdf));
            CopyProbs(default_inter_ext_tx_cdf,        ectx.inter_ext_tx_cdf,         sizeof(ectx.inter_ext_tx_cdf));
            CopyProbs(default_tx_size_cdf,             ectx.tx_size_cdf,              sizeof(ectx.tx_size_cdf));
            CopyProbs(default_motion_mode_cdf,         ectx.motion_mode_cdf,          sizeof(ectx.motion_mode_cdf));
            CopyProbs(default_obmc_cdf,                ectx.obmc_cdf,                 sizeof(ectx.obmc_cdf));
            CopyProbs(default_txfm_partition_cdf,      ectx.txfm_partition_cdf,       sizeof(ectx.txfm_partition_cdf));
            CopyProbs(&av1_default_nmv_context,        &ectx.mv_contexts,             sizeof(ectx.mv_contexts));
        }

        const int32_t index = get_q_ctx(frame->m_sliceQpY);
        //for (size_t i = 0; i < frame->m_tileContexts.size(); i++)
        {
            EntropyContexts &ectx = frame->m_tileContexts[i].ectx;
            CopyProbs(default_txb_skip_cdfs[index],       ectx.txb_skip_cdf,       sizeof(ectx.txb_skip_cdf));
            CopyProbs(default_eob_extra_cdfs[index],      ectx.eob_extra_cdf,      sizeof(ectx.eob_extra_cdf));
            CopyProbs(default_dc_sign_cdfs[index],        ectx.dc_sign_cdf,        sizeof(ectx.dc_sign_cdf));
            CopyProbs(default_eob_multi16_cdfs[index],    ectx.eob_flag_cdf16,     sizeof(ectx.eob_flag_cdf16));
            CopyProbs(default_eob_multi32_cdfs[index],    ectx.eob_flag_cdf32,     sizeof(ectx.eob_flag_cdf32));
            CopyProbs(default_eob_multi64_cdfs[index],    ectx.eob_flag_cdf64,     sizeof(ectx.eob_flag_cdf64));
            CopyProbs(default_eob_multi128_cdfs[index],   ectx.eob_flag_cdf128,    sizeof(ectx.eob_flag_cdf128));
            CopyProbs(default_eob_multi256_cdfs[index],   ectx.eob_flag_cdf256,    sizeof(ectx.eob_flag_cdf256));
            CopyProbs(default_eob_multi512_cdfs[index],   ectx.eob_flag_cdf512,    sizeof(ectx.eob_flag_cdf512));
            CopyProbs(default_eob_multi1024_cdfs[index],  ectx.eob_flag_cdf1024,   sizeof(ectx.eob_flag_cdf1024));
            CopyProbs(default_coeff_base_eob_cdfs[index], ectx.coeff_base_eob_cdf, sizeof(ectx.coeff_base_eob_cdf));
            CopyProbs(default_coeff_base_cdfs[index],     ectx.coeff_base_cdf,     sizeof(ectx.coeff_base_cdf));
            CopyProbs(default_coeff_lps_cdfs[index],      ectx.coeff_br_cdf,       sizeof(ectx.coeff_br_cdf));
        }
    }

    void SaveProbs(const FrameContexts &from, FrameContexts *to) {
        CopyProbs(&from, to, sizeof(FrameContexts));
    }

    void LoadProbs(const FrameContexts &from, FrameContexts *to) {
        CopyProbs(&from, to, sizeof(FrameContexts));
    }


    int32_t WriteObuHeader(uint8_t *buf, int32_t &off, int32_t type)
    {
        assert(type == OBU_SEQUENCE_HEADER || type == OBU_TD || type == OBU_FRAME_HEADER ||
               type == OBU_TILE_GROUP || type == OBU_FRAME || type == OBU_METADATA || type == OBU_PADDING);
        const int32_t obu_extension_flag = 0;
        const int32_t obu_extension = 0;
        int32_t startOff = off;
        PutBit(buf, off, OBU_FORBIDDEN_BIT);
        PutBits(buf, off, type, 4);
        PutBit(buf, off, obu_extension_flag ? 1 : 0);
        PutBit(buf, off, 1);//obu_has_payload_length_field
        PutBit(buf, off, 0);//reserved
        if (obu_extension_flag) {
            PutBits(buf, off, obu_extension & 0xFF, 8);
        }
        int32_t bit_offset = (off - startOff);
        return  (bit_offset / CHAR_BIT + (bit_offset % CHAR_BIT > 0));
    }

    void WriteTrailingBits(uint8_t *buf, int32_t &off) {

        if (off % 8 == 0) {
            PutBits(buf, off, 0x80, 8);
            //aom_wb_write_literal(wb, 0x80, 8);
        } else {
            PutBit(buf, off, 1);
            while (off & 7)
                PutBit(buf, off, 0);
        }
    }

    void WritePaddingBits(uint8_t *buf, int32_t &off) {

        while (off & 7)
            PutBit(buf, off, 0);
    }

    void WriteObuSize(uint8_t *buf, int32_t obuStartOff, int32_t &obuEndOff) {
        assert(obuStartOff % 8 == 0);
        assert(obuEndOff % 8 == 0);
        assert(obuEndOff - obuStartOff >= 8);
        const int32_t size = (obuEndOff - obuStartOff) >> 3;

        const int32_t bytesPerSize = 1 + BSR(size) / 7;

        memmove(buf + (obuStartOff >> 3) + bytesPerSize, buf + (obuStartOff >> 3), size);

        int32_t tmpOff = obuStartOff;
        for (int32_t i = 0; i < bytesPerSize; i++) {
            int32_t uleb128_byte = (size >> (7 * i)) & 0x7f;
            if (i + 1 < bytesPerSize)
                uleb128_byte |= 0x80;
            PutBits(buf, tmpOff, uleb128_byte, 8);
        }

        obuEndOff += bytesPerSize * 8;
    }

    int32_t av1_setup_skip_mode_allowed(const AV1VideoParam &par, const Frame &frame) {
        if (!par.seqParams.enableOrderHint || par.errorResilientMode ||
            frame.IsIntra() || frame.referenceMode == SINGLE_REFERENCE)
            return 0;
        return 1;
    }

    void WriteFrameSizeAv1(uint8_t *buf, int32_t &off) {
        const int32_t frame_size_override_flag = 0;
        const int32_t use_superres = 0;
        const int32_t render_and_frame_size_different = 0;

        if (frame_size_override_flag) {
            assert(0);
        }
        if (use_superres) {
            PutBit(buf, off, use_superres);
            if (use_superres) {
                assert(0);
            }
        }
        PutBit(buf, off, render_and_frame_size_different);
        if (render_and_frame_size_different) {
            assert(0);
        }
    }

    void WriteSequenceHeaderObu(uint8_t *buf, int32_t &off, const AV1VideoParam &par, Frame &frame, int32_t showExistingFrame, int32_t showFrame)
    {
        const int32_t level = 0;
        const int32_t frame_width_bits = 16;
        const int32_t frame_height_bits = 16;
        const int32_t max_enhancement_layers_cnt = 0;
        const int32_t use_128x128_superblock = 0;
        const int32_t enable_jnt_comp = 0;
        const int32_t seq_choose_screen_content_tools = 0;
        const int32_t seq_force_screen_content_tools = 0;
        const int32_t seq_choose_integer_mv = 0;
        const int32_t seq_force_integer_mv = 0;
        const int32_t timing_info_present_flag = 0;
        const int32_t film_grain_params_present = 0;
        const int32_t enable_superres = 0;
        const int32_t enable_ref_frame_mvs = 1;
        const int32_t enable_interintra_compound = 0;
        const int32_t enable_masked_compound = 0;
        const int32_t enable_warped_motion = 0;
        const int32_t enable_filter_intra = 0;
        const int32_t enable_intra_edge_filter = 0;
#if (USE_CMODEL_PAK || USE_HWPAK_RESTRICT)
        const int32_t enable_dual_filter = 0;// cmodel limitation
#else
        const int32_t enable_dual_filter = 1;
#endif
        const int32_t enable_cdef = par.cdefFlag ? 1 : 0;
        const int32_t enable_restoration = par.lrFlag ? 1 : 0;

        //---------------
        const int32_t still_picture = 0;
        const int32_t reduced_still_picture_hdr = 0;
        const int32_t display_model_info_present_flag = 0;
        const int32_t decoder_model_info_present_flag = 0;

        if (frame.IsIntra()) {
            int32_t obuStartOff = off;
            int32_t obuHeaderSize = WriteObuHeader(buf, off, OBU_SEQUENCE_HEADER);

            PutBits(buf, off, par.Profile, 3);
            PutBit(buf, off, still_picture);
            PutBit(buf, off, reduced_still_picture_hdr);
            PutBit(buf, off, timing_info_present_flag);
            if (timing_info_present_flag) {
                assert(0);
            }
            PutBit(buf, off, display_model_info_present_flag);

            int32_t operating_points_minus1_cnt = max_enhancement_layers_cnt;
            PutBits(buf, off, operating_points_minus1_cnt, 5);
            for (int32_t i = 0; i < operating_points_minus1_cnt + 1; i++) {
                PutBits(buf, off, 0, 12);  // operating_point_idc[i]
                PutBits(buf, off, 0, 5);   // level[i]
                //PutBits(buf, off, 0, 1);   // decoder_rate_model_present_flag[i]
            }

            const int32_t num_bits_width  = get_msb(par.Width - 1) + 1;
            const int32_t num_bits_height = get_msb(par.Height - 1) + 1;

            PutBits(buf, off, num_bits_width - 1, 4);
            PutBits(buf, off, num_bits_height - 1, 4);
            PutBits(buf, off, par.Width - 1, num_bits_width);
            PutBits(buf, off, par.Height - 1, num_bits_height);

            if (!reduced_still_picture_hdr) {
                PutBit(buf, off, par.seqParams.frameIdNumbersPresentFlag);
                if (par.seqParams.frameIdNumbersPresentFlag) {
                    assert(par.seqParams.deltaFrameIdLength >= 2);
                    assert(par.seqParams.additionalFrameIdLength >= 1);
                    PutBits(buf, off, par.seqParams.deltaFrameIdLength - 2, 4);
                    PutBits(buf, off, par.seqParams.additionalFrameIdLength - 1, 3);
                }
            }

            PutBit(buf, off, use_128x128_superblock);
            PutBit(buf, off, enable_filter_intra);
            PutBit(buf, off, enable_intra_edge_filter);
            if (!reduced_still_picture_hdr) {
                PutBit(buf, off, enable_interintra_compound);
                PutBit(buf, off, enable_masked_compound);
                PutBit(buf, off, enable_warped_motion);
                PutBit(buf, off, enable_dual_filter);

                PutBit(buf, off, par.seqParams.enableOrderHint);

                if (par.seqParams.enableOrderHint) {
                    PutBit(buf, off, enable_jnt_comp);
                    PutBit(buf, off, enable_ref_frame_mvs);
                }
                //---
                PutBit(buf, off, seq_choose_screen_content_tools);
                if (seq_choose_screen_content_tools) {
                    assert(0);
                } else {
                    PutBit(buf, off, seq_force_screen_content_tools);
                }
                //---
                if (seq_force_screen_content_tools > 0) {
                    PutBit(buf, off, seq_choose_integer_mv);
                    if (!seq_choose_integer_mv)
                        PutBit(buf, off, seq_force_integer_mv);
                }

                if (par.seqParams.enableOrderHint)
                    PutBits(buf, off, par.seqParams.orderHintBits - 1, 3);
            }

            PutBit(buf, off, enable_superres);
            PutBit(buf, off, enable_cdef);
            PutBit(buf, off, enable_restoration);

            WriteColorConfigAv1(buf, off, par);

            PutBit(buf, off, film_grain_params_present);

            WriteTrailingBits(buf, off);
            WriteObuSize(buf, obuStartOff + obuHeaderSize*8, off);
        }
    }

    void WriteFrameHeaderObu(uint8_t *buf, int32_t &off, const AV1VideoParam &par, Frame &frame, int32_t showExistingFrame, int32_t showFrame)
    {
        const int32_t level = 0;
        const int32_t frame_width_bits = 16;
        const int32_t frame_height_bits = 16;
        const int32_t max_enhancement_layers_cnt = 0;
        const int32_t use_128x128_superblock = 0;
        const int32_t enable_dual_filter = 1;
        const int32_t enable_jnt_comp = 0;
        const int32_t seq_choose_screen_content_tools = 0;
        const int32_t seq_force_screen_content_tools = 0;
        const int32_t seq_choose_integer_mv = 0;
        const int32_t seq_force_integer_mv = 0;
        const int32_t timing_info_present_flag = 0;
        const int32_t film_grain_params_present = 0;

        const int32_t frame_size_override_flag = 0;
        const int32_t frame_type = (frame.m_picCodeType == MFX_FRAMETYPE_I ? KEY_FRAME : NON_KEY_FRAME);
        const int32_t allow_filter_intra = 0;
        const int32_t disable_cdf_update = 0;
        const int32_t allow_screen_content_tools = 0;
        const int32_t refresh_frame_context = 1;
        const int32_t frame_context_idx = 0;
        const int32_t delta_q_present_flag = 0;
        const int32_t allow_interintra_compound = 0;
        const int32_t allow_masked_compound = 0;
        const int32_t skip_mode_flag = 0;
        const int32_t allow_intrabc = 0;
        const int32_t av1_superres_unscaled = 1;
        const int32_t frame_refs_short_signaling = 0;
        const int32_t force_integer_mv = 0;
        const int32_t switchable_motion_mode = 0;
        const int32_t error_resilient_mode = 0;
        const int32_t allow_warped_motion = 0;
        const int32_t enable_restoration = par.lrFlag ? 1 : 0;
#if (USE_CMODEL_PAK || USE_HWPAK_RESTRICT)
        const int32_t reduced_tx_set_used = 1;
#else
        const int32_t reduced_tx_set_used = 0;
#endif
        const int32_t enable_cdef = par.cdefFlag ? 1 : 0;

        int32_t obuStartOff = off;
        //int32_t obuHeaderSize = WriteObuHeader(buf, off, OBU_FRAME_HEADER);

        PutBit(buf, off, showExistingFrame);

        PutBits(buf, off, frame_type, 2);
        PutBit(buf, off, showFrame);

        const int32_t showable_frame = 1;
        if (!showFrame)
            PutBit(buf, off, showable_frame);

        if (!(frame_type == KEY_FRAME && showFrame))
            PutBit(buf, off, par.errorResilientMode);

        PutBit(buf, off, disable_cdf_update);
        if (seq_choose_screen_content_tools)
            PutBit(buf, off, allow_screen_content_tools);

        if (allow_screen_content_tools && seq_force_integer_mv)
            PutBit(buf, off, force_integer_mv);

        if (par.seqParams.frameIdNumbersPresentFlag)
            PutBits(buf, off, frame.m_encOrder, par.seqParams.idLen);

        PutBit(buf, off, frame_size_override_flag);

        PutBits(buf, off, frame.curFrameOffset, par.seqParams.orderHintBits);

        if (!par.errorResilientMode && !frame.IsIntra()) {
            PutBits(buf, off, PRIMARY_REF_NONE, 3);
        }

        if (frame_type == KEY_FRAME) {
            if (!showFrame)
                PutBits(buf, off, frame.refreshFrameFlags, 8);
            WriteFrameSizeAv1(buf, off);
            if (allow_screen_content_tools && av1_superres_unscaled)
                PutBit(buf, off, allow_intrabc);

        } else {
            if (frame.intraOnly) {
                PutBits(buf, off, frame.refreshFrameFlags, 8);
                WriteFrameSizeAv1(buf, off);
                if (allow_screen_content_tools && av1_superres_unscaled)
                    PutBit(buf, off, allow_intrabc);

            } else {
                PutBits(buf, off, frame.refreshFrameFlags, 8);
                if (!par.errorResilientMode && par.seqParams.enableOrderHint)
                    PutBit(buf, off, frame_refs_short_signaling);
                if (frame_refs_short_signaling) {
                    assert(0);
                }

                for (int32_t j = AV1_LAST_FRAME; j <= AV1_ALTREF_FRAME; j++) {
                    const int i = av1_to_vp9_ref_frame_mapping[j];
                    if (!frame_refs_short_signaling) {
                        assert(frame.refFrameIdx[i] < 8);
                        PutBits(buf, off, frame.refFrameIdx[i], 3);
                    }
                    if (par.seqParams.frameIdNumbersPresentFlag) {
                        int32_t deltaFrameId =
                            ((frame.m_encOrder - frame.m_refFrameId[frame.refFrameIdx[i]] +
                            (1 << par.seqParams.idLen)) % (1 << par.seqParams.idLen));
                        PutBits(buf, off, deltaFrameId - 1, par.seqParams.deltaFrameIdLength);
                    }
                }

                if (par.errorResilientMode == 0 && frame_size_override_flag) {
                    assert(0);
                } else {
                    WriteFrameSizeAv1(buf, off);
                }

                if (!force_integer_mv)
                    PutBit(buf, off, par.allowHighPrecisionMv);
                PutBit(buf, off, frame.interpolationFilter == SWITCHABLE);
                if (frame.interpolationFilter != SWITCHABLE)
                    PutBits(buf, off, frame.interpolationFilter, 2);
                PutBit(buf, off, par.switchableMotionMode);

                const int32_t frame_might_use_prev_frame_mvs = !par.errorResilientMode &&
                                                              !frame.IsIntra();
                const int32_t frame_can_use_prev_frame_mvs = frame_might_use_prev_frame_mvs &&
                                                            (frame.m_prevFrame != NULL);
                if (frame_might_use_prev_frame_mvs && par.seqParams.enableOrderHint)
                    PutBit(buf, off, frame_can_use_prev_frame_mvs);
            }
        }

        const int32_t reduced_still_picture_hdr = 0;
        if (!(reduced_still_picture_hdr)&& !(disable_cdf_update))
            PutBit(buf, off, refresh_frame_context);

        WriteTileInfoAv1(buf, off, par);

        WriteQuantizationParamsAv1(buf, off, par, frame);
        WriteSegmentationParams(buf, off, frame);

        const int32_t delta_q_allowed = 1;
        if (delta_q_allowed == 1 && frame.m_sliceQpY > 0)
            PutBit(buf, off, delta_q_present_flag);

        WriteLoopFilterParamsAv1(buf, off, frame);
        if (enable_cdef)
            WriteCdef(buf, off, frame);
        if (enable_restoration)
            WriteRestorationMode(buf, off, frame);

        PutBit(buf, off, par.txMode == TX_MODE_SELECT);

        const int32_t is_compound_reference_allowed = !frame.IsIntra() /*&& (frame.isUniq[LAST_FRAME] + frame.isUniq[GOLDEN_FRAME] + frame.isUniq[ALTREF_FRAME]) > 1*/;

        if (is_compound_reference_allowed)
            PutBit(buf, off, frame.referenceMode == REFERENCE_MODE_SELECT);

        if (av1_setup_skip_mode_allowed(par, frame))
            PutBit(buf, off, skip_mode_flag);

        PutBit(buf, off, reduced_tx_set_used);

        if (!frame.IsIntra())
            for (int frame = AV1_LAST_FRAME; frame <= AV1_ALTREF_FRAME; ++frame)
                PutBit(buf, off, 0); // global motion type = IDENTITY

        //WriteTrailingBits(buf, off);
        //WriteObuSize(buf, obuStartOff + 8*obuHeaderSize, off);
    }

    template <EnumCodecType CodecType>
    int32_t diff_update_prob(BoolCoder &bc, int32_t prob) {
        WriteBool<CodecType>(bc, 0, DIFF_UPDATE_PROB); // update_probs
        return prob;
    }

    template <EnumCodecType CodecType>
    int32_t update_mv_prob(BoolCoder &bc, int32_t prob) {
        WriteBool<CodecType>(bc, 0, DIFF_UPDATE_PROB); // update_probs
        return prob;
    }

    struct tree_node {
        vpx_tree_index index;
        uint8_t probs[16];
        uint8_t prob;
        int path;
        int len;
        int l;
        int r;
        aom_cdf_prob pdf;
    };

    /* Compute the probability of this node in Q23 */
    static uint32_t tree_node_prob(tree_node n, int i) {
        uint32_t prob;
        /* 1.0 in Q23 */
        prob = 16777216;
        for (; i < n.len; i++) {
            prob = prob * n.probs[i] >> 8;
        }
        return prob;
    }

    static int tree_node_cmp(tree_node a, tree_node b) {
        int i;
        uint32_t pa;
        uint32_t pb;
        for (i = 0; i < MIN(a.len, b.len) && a.probs[i] == b.probs[i]; i++) {
        }
        pa = tree_node_prob(a, i);
        pb = tree_node_prob(b, i);
        return pa > pb ? 1 : pa < pb ? -1 : 0;
    }

    static aom_cdf_prob tree_node_compute_probs(tree_node *tree, int n,
        aom_cdf_prob pdf) {
            if (tree[n].l == 0) {
                /* This prevents probability computations in Q15 that underflow from
                producing a symbol that has zero probability. */
                if (pdf == 0) pdf = 1;
                tree[n].pdf = pdf;
                return pdf;
            }
            else {
                /* We process the smaller probability first,  */
                if (tree[n].prob < 128) {
                    aom_cdf_prob lp;
                    aom_cdf_prob rp;
                    lp = (((uint32_t)pdf) * tree[n].prob + 128) >> 8;
                    lp = tree_node_compute_probs(tree, tree[n].l, lp);
                    rp = tree_node_compute_probs(tree, tree[n].r, lp > pdf ? 0 : pdf - lp);
                    return lp + rp;
                }
                else {
                    aom_cdf_prob rp;
                    aom_cdf_prob lp;
                    rp = (((uint32_t)pdf) * (256 - tree[n].prob) + 128) >> 8;
                    rp = tree_node_compute_probs(tree, tree[n].r, rp);
                    lp = tree_node_compute_probs(tree, tree[n].l, rp > pdf ? 0 : pdf - rp);
                    return lp + rp;
                }
            }
    }

    static int tree_node_extract(tree_node *tree, int n, int symb,
        aom_cdf_prob *pdf, vpx_tree_index *index,
        int *path, int *len) {
            if (tree[n].l == 0) {
                pdf[symb] = tree[n].pdf;
                if (index != NULL) index[symb] = tree[n].index;
                if (path != NULL) path[symb] = tree[n].path;
                if (len != NULL) len[symb] = tree[n].len;
                return symb + 1;
            }
            else {
                symb = tree_node_extract(tree, tree[n].l, symb, pdf, index, path, len);
                return tree_node_extract(tree, tree[n].r, symb, pdf, index, path, len);
            }
    }

    void WriteIntraSegmentId(const Frame &frame) {
        if (frame.segmentationEnabled && frame.segmentationUpdateMap) {
            assert(0);
        }
    }

    void WriteInterSegmentId(const Frame &frame) {
        if (frame.segmentationEnabled) {
            assert(0);
        }
    }

    template <EnumCodecType CodecType>
    void WriteSkip(BoolCoder &bc, SuperBlock &sb, const ModeInfo *mi) {
#ifdef AV1_DEBUG_CONTEXT_MEMOIZATION
        const int32_t miRow = (mi - sb.frame->m_modeInfo) / sb.par->miPitch;
        const int32_t miCol = (mi - sb.frame->m_modeInfo) % sb.par->miPitch;
        const ModeInfo *above = GetAbove(mi, sb.par->miPitch, miRow, sb.par->tileParam.miRowStart[sb.tileRow]);
        const ModeInfo *left = GetLeft(mi, miCol, sb.par->tileParam.miColStart[sb.tileCol]);
        int32_t testctx = GetCtxSkip(above, left);
        assert(testctx == mi->memCtx.skip);
#endif // AV1_DEBUG_CONTEXT_MEMOIZATION

        const int32_t ctx = mi->memCtx.skip;
        const int32_t skip = mi->skip;
        WriteSymbol(bc, skip, sb.frame->m_tileContexts[sb.tileIndex].ectx.skip_cdf[ctx], 2);
    }

     void txfm_partition_update(uint8_t *above_ctx, uint8_t *left_ctx, TxSize tx_size, TxSize txb_size) {
        const BlockSize bsize = txsize_to_bsize[txb_size];
        const int32_t bh = block_size_high_4x4[bsize];
        const int32_t bw = block_size_wide_4x4[bsize];
        const uint8_t txw = tx_size_wide[tx_size];
        const uint8_t txh = tx_size_high[tx_size];
        for (int32_t i = 0; i < bh; ++i)
            left_ctx[i] = txh;
        for (int32_t i = 0; i < bw; ++i)
            above_ctx[i] = txw;
    }

    void set_txfm_ctxs(SuperBlock &sb, const ModeInfo *mi, int32_t miRow, int32_t miCol) {
        uint8_t txw = tx_size_wide[mi->txSize];
        uint8_t txh = tx_size_high[mi->txSize];
        uint8_t bw4 = block_size_wide_4x4[mi->sbType];
        uint8_t bh4 = block_size_high_4x4[mi->sbType];
        if (mi->skip && mi->mode >= AV1_INTRA_MODES) {
            txw = bw4 << 2;
            txh = bh4 << 2;
        }
        const int32_t miColTileStart = sb.par->tileParam.miColStart[sb.tileCol];
        const int32_t miRowTileStart = sb.par->tileParam.miRowStart[sb.tileRow];
        uint8_t *aboveTxfm = sb.frame->m_tileContexts[sb.tileIndex].aboveTxfm + ((miCol - miColTileStart) << 1);
        uint8_t *leftTxfm  = sb.frame->m_tileContexts[sb.tileIndex].leftTxfm  + ((miRow - miRowTileStart) << 1);
        memset(aboveTxfm, txw, bw4);
        memset(leftTxfm,  txh, bh4);
    }

    void WriteTxSizeVarTx(BoolCoder &bc, SuperBlock &sb, const ModeInfo *mi, TxSize txSize, int32_t depth, int32_t x4, int32_t y4) {
        const int32_t txRow = y4 - (sb.par->tileParam.miRowStart[sb.tileRow] << 1);
        const int32_t txCol = x4 - (sb.par->tileParam.miColStart[sb.tileCol] << 1);
        TileContexts &tctx = sb.frame->m_tileContexts[sb.tileIndex];
        uint8_t *aboveTxfm = tctx.aboveTxfm + txCol;
        uint8_t *leftTxfm  = tctx.leftTxfm + txRow;
        const int32_t ctx = GetCtxTxfm(aboveTxfm, leftTxfm, mi->sbType, txSize);
        aom_cdf_prob *cdf = tctx.ectx.txfm_partition_cdf[ctx];
        if (depth == MAX_VARTX_DEPTH) {
            txfm_partition_update(aboveTxfm, leftTxfm, txSize, txSize);
            return;
        }
        if (txSize == mi->txSize) {
            WriteSymbol(bc, 0, cdf, 2);
            txfm_partition_update(aboveTxfm, leftTxfm, txSize, txSize);
        } else {
            const TxSize sub_txs = sub_tx_size_map[1][txSize];
            const int32_t off4 = tx_size_wide_unit[sub_txs];
            const int32_t off8 = off4 >> 1;
            WriteSymbol(bc, 1, cdf, 2);
            if (sub_txs == TX_4X4) {
                txfm_partition_update(aboveTxfm, leftTxfm, sub_txs, txSize);
                return;
            }
            WriteTxSizeVarTx(bc, sb, mi + 0    + 0    * sb.par->miPitch, sub_txs, depth + 1, x4 + 0,    y4 + 0);
            WriteTxSizeVarTx(bc, sb, mi + off8 + 0    * sb.par->miPitch, sub_txs, depth + 1, x4 + off4, y4 + 0);
            WriteTxSizeVarTx(bc, sb, mi + 0    + off8 * sb.par->miPitch, sub_txs, depth + 1, x4 + 0,    y4 + off4);
            WriteTxSizeVarTx(bc, sb, mi + off8 + off8 * sb.par->miPitch, sub_txs, depth + 1, x4 + off4, y4 + off4);
        }
    }

    int32_t tx_size_to_depth(TxSize tx_size, BlockSize bsize, int32_t is_inter) {
        TxSize ctx_size = max_txsize_rect_lookup[bsize];
        int32_t depth = 0;
        while (tx_size != ctx_size) {
            depth++;
            ctx_size = sub_tx_size_map[is_inter][ctx_size];
            assert(depth <= MAX_TX_DEPTH);
        }
        return depth;
    }

    int32_t bsize_to_max_depth(BlockSize bsize, int32_t is_inter) {
        TxSize tx_size = max_txsize_rect_lookup[bsize];
        int32_t depth = 0;
        while (depth < MAX_TX_DEPTH && tx_size != TX_4X4) {
            depth++;
            tx_size = sub_tx_size_map[is_inter][tx_size];
        }
        return depth;
    }

    int32_t bsize_to_tx_size_cat(BlockSize bsize, int32_t is_inter) {
        TxSize tx_size = max_txsize_rect_lookup[bsize];
        assert(tx_size != TX_4X4);
        int32_t depth = 0;
        while (tx_size != TX_4X4) {
            depth++;
            tx_size = sub_tx_size_map[is_inter][tx_size];
            assert(depth < 10);
        }
        assert(depth <= MAX_TX_CATS);
        return depth - 1;
    }

    void WriteTxSizeAV1(int32_t allowSelect, BoolCoder &bc, SuperBlock &sb, const ModeInfo *mi, int32_t miRow, int32_t miCol) {
        const BlockSize bsize = mi->sbType;
        const int32_t isInter = (mi->mode >= AV1_INTRA_MODES);
        if (sb.par->txMode == TX_MODE_SELECT && bsize > BLOCK_4X4 && !mi->skip && isInter) {
            const TxSize maxTxSize = max_txsize_rect_lookup[bsize];
            const int32_t bh = tx_size_high_unit[maxTxSize];
            const int32_t bw = tx_size_wide_unit[maxTxSize];
            const int32_t width = block_size_wide_4x4[bsize];
            const int32_t height = block_size_high_4x4[bsize];
            const int32_t y4 = miRow << 1;
            const int32_t x4 = miCol << 1;

            for (int32_t idy = 0; idy < height; idy += bh)
                for (int32_t idx = 0; idx < width; idx += bw)
                    WriteTxSizeVarTx(bc, sb, mi, maxTxSize, width != height, x4+idx, y4+idy);
        } else {
            if (allowSelect && sb.par->txMode == TX_MODE_SELECT && bsize >= BLOCK_8X8) {
                const ModeInfo *above = GetAbove(mi, sb.par->miPitch, miRow, sb.par->tileParam.miRowStart[sb.tileRow]);
                const ModeInfo *left = GetLeft(mi, miCol, sb.par->tileParam.miColStart[sb.tileCol]);
                const int32_t txRow = (miRow - sb.par->tileParam.miRowStart[sb.tileRow]) << 1;
                const int32_t txCol = (miCol - sb.par->tileParam.miColStart[sb.tileCol]) << 1;
                const TileContexts &tctx = sb.frame->m_tileContexts[sb.tileIndex];
                const uint8_t *aboveTxfm = tctx.aboveTxfm + txCol;
                const uint8_t *leftTxfm  = tctx.leftTxfm + txRow;
                const int32_t max_tx_size = max_txsize_rect_lookup[bsize];
                const int32_t tx_size_ctx = GetCtxTxSizeAV1(above, left, *aboveTxfm, *leftTxfm, max_tx_size);
                const int32_t depth = tx_size_to_depth(mi->txSize, bsize, 0);
                const int32_t max_depths = bsize_to_max_depth(bsize, 0);
                const int32_t tx_size_cat = bsize_to_tx_size_cat(bsize, 0);

                aom_cdf_prob *cdf = sb.frame->m_tileContexts[sb.tileIndex].ectx.tx_size_cdf[tx_size_cat][tx_size_ctx];
                WriteSymbol(bc, depth, cdf, max_depths + 1);
            }
            set_txfm_ctxs(sb, mi, miRow, miCol);
        }
    }

    template <EnumCodecType CodecType>
    void WriteKeyFrameIntraMode(BoolCoder &bc, SuperBlock &sb, int32_t miRow, int32_t miCol) {
        const ModeInfo *mi = sb.mi + (miCol & 7) + (miRow & 7) * sb.par->miPitch;
        const ModeInfo *above = GetAbove(mi, sb.par->miPitch, miRow, sb.par->tileParam.miRowStart[sb.tileRow]);
        const ModeInfo *left = GetLeft(mi, miCol, sb.par->tileParam.miColStart[sb.tileCol]);
        const int32_t modeU = (above ? above->mode : DC_PRED);
        const int32_t modeL = (left  ? left->mode  : DC_PRED);
        const int32_t ctxU = intra_mode_context[modeU];
        const int32_t ctxL = intra_mode_context[modeL];
        aom_cdf_prob *cdf = sb.frame->m_tileContexts[sb.tileIndex].ectx.kf_y_cdf[ctxU][ctxL];
        WriteSymbol(bc, mi->mode, cdf, AV1_INTRA_MODES);
    }

    void WriteTxType(BoolCoder &bc, SuperBlock &sb, int32_t blkRow, int32_t blkCol)
    {
        const ModeInfo *mi = sb.frame->m_modeInfo + (blkRow >> 1) * sb.par->miPitch + (blkCol >> 1);
        const int32_t isInter = (mi->mode >= AV1_INTRA_MODES);
        EntropyContexts &ectx = sb.frame->m_tileContexts[sb.tileIndex].ectx;

#if USE_HWPAK_RESTRICT
        const int32_t reduced_tx_set_used = 1;
#else
        const int32_t reduced_tx_set_used = 0;
#endif
        const TxSize txSize = mi->txSize;
        const TxType txType = mi->txType;
        const TxSize squareTxSize = txsize_sqr_map[mi->txSize];
        const BlockSize bsize = mi->sbType;

        if (get_ext_tx_types(txSize, bsize, isInter, reduced_tx_set_used) > 1 && !mi->skip) {
            const TxSetType txSetType = get_ext_tx_set_type(txSize, bsize, isInter, reduced_tx_set_used);
            const int32_t eset = get_ext_tx_set(txSize, bsize, isInter, reduced_tx_set_used);
            int32_t index = (blkRow & 15) * 16 + (blkCol & 15);
            const int32_t txType = sb.frame->m_txkTypes4x4[sb.sbIndex * 256 + index] & 3;
            assert(txType >= 0);
            // eset == 0 should correspond to a set with only DCT_DCT and there
            // is no need to send the tx_type
            assert(eset > 0);
            assert(av1_ext_tx_used[txSetType][txType]);
            if (isInter) {
                WriteSymbol(bc, av1_ext_tx_ind[txSetType][txType],
                    ectx.inter_ext_tx_cdf[eset][squareTxSize], av1_num_ext_tx_set[txSetType]);
            } else {
                const PredMode intraDir = mi->mode;
                WriteSymbol(bc, av1_ext_tx_ind[txSetType][txType],
                    ectx.intra_ext_tx_cdf[eset][squareTxSize][intraDir], av1_num_ext_tx_set[txSetType]);
            }
        }
    }

    void WriteIntraAngleInfo(BoolCoder &bc, SuperBlock &sb, BlockSize bsize, PredMode mode, int32_t angle_delta)
    {
        if (bsize >= BLOCK_8X8 && av1_is_directional_mode(mode)) {
            aom_cdf_prob *cdf = sb.frame->m_tileContexts[sb.tileIndex].ectx.angle_delta_cdf[mode - V_PRED];
            WriteSymbol(bc, angle_delta, cdf, 2 * MAX_ANGLE_DELTA + 1);
        }
    }

    void WriteFilterIntraMode(BoolCoder &bc, SuperBlock &sb, const ModeInfo *mi)
    {
        const int32_t allow_filter_intra = 0;
        if (allow_filter_intra && block_size_wide[mi->sbType] <= 32 && block_size_wide[mi->sbType] <= 32 && mi->mode == DC_PRED) {
            aom_cdf_prob *cdf = sb.frame->m_tileContexts[sb.tileIndex].ectx.filter_intra_cdfs[mi->txSize];
            WriteSymbol(bc, 0, cdf, 2);
        }
    }

    template <EnumCodecType CodecType>
    void WriteIntraFrameModeInfo(BoolCoder &bc, SuperBlock &sb, int32_t miRow, int32_t miCol) {
        const Frame &frame = *sb.frame;
        const ModeInfo *mi = frame.m_modeInfo + miCol + miRow * sb.par->miPitch;
        WriteIntraSegmentId(frame);
        WriteSkip<CodecType>(bc, sb, mi);

        if (sb.cdefPreset == -1 && !mi->skip &&  sb.par->cdefFlag) {
            WriteLiteral<CODEC_AV1>(bc, frame.m_cdefStrenths[sb.sbIndex], frame.m_cdefParam.cdef_bits);
            sb.cdefPreset = frame.m_cdefStrenths[sb.sbIndex];
        }

        if (mi->sbType >= BLOCK_8X8) {
            WriteKeyFrameIntraMode<CodecType>(bc, sb, miRow, miCol);
        } else if (mi->sbType == BLOCK_4X8) {
            WriteKeyFrameIntraMode<CodecType>(bc, sb, miRow, miCol);
            WriteKeyFrameIntraMode<CodecType>(bc, sb, miRow, miCol);
        } else if (mi->sbType == BLOCK_8X4) {
            WriteKeyFrameIntraMode<CodecType>(bc, sb, miRow, miCol);
            WriteKeyFrameIntraMode<CodecType>(bc, sb, miRow, miCol);
        } else { assert(mi->sbType == BLOCK_4X4);
            WriteKeyFrameIntraMode<CodecType>(bc, sb, miRow, miCol);
            WriteKeyFrameIntraMode<CodecType>(bc, sb, miRow, miCol);
            WriteKeyFrameIntraMode<CodecType>(bc, sb, miRow, miCol);
            WriteKeyFrameIntraMode<CodecType>(bc, sb, miRow, miCol);
        }

        WriteIntraAngleInfo(bc, sb, mi->sbType, mi->mode, mi->angle_delta_y);
        aom_cdf_prob *probs = sb.frame->m_tileContexts[sb.tileIndex].ectx.uv_mode_cdf[mi->mode];
        WriteSymbol(bc, mi->modeUV, probs, AV1_UV_INTRA_MODES);
        WriteIntraAngleInfo(bc, sb, mi->sbType, mi->modeUV, mi->angle_delta_uv);
        WriteFilterIntraMode(bc, sb, mi);
        WriteTxSizeAV1(1, bc, sb, mi, miRow, miCol);
    }

    template <EnumCodecType CodecType>
    void WriteNonKeyFrameIntraMode(BoolCoder &bc, SuperBlock &sb, const ModeInfo *mi) {
        int32_t ctx = size_group_lookup[mi->sbType];
        aom_cdf_prob *probs = sb.frame->m_tileContexts[sb.tileIndex].ectx.y_mode_cdf[ctx];
        WriteSymbol(bc, mi->mode, probs, AV1_INTRA_MODES);
    }

    template <EnumCodecType CodecType>
    void WriteIntraBlockModeInfo(BoolCoder &bc, SuperBlock &sb, const ModeInfo *mi) {
        if (mi->sbType >= BLOCK_8X8) {
            WriteNonKeyFrameIntraMode<CodecType>(bc, sb, mi);
        } else if (mi->sbType == BLOCK_4X8) {
            WriteNonKeyFrameIntraMode<CodecType>(bc, sb, mi);
            WriteNonKeyFrameIntraMode<CodecType>(bc, sb, mi);
        } else if (mi->sbType == BLOCK_8X4) {
            WriteNonKeyFrameIntraMode<CodecType>(bc, sb, mi);
            WriteNonKeyFrameIntraMode<CodecType>(bc, sb, mi);
        } else { assert(mi->sbType == BLOCK_4X4);
        WriteNonKeyFrameIntraMode<CodecType>(bc, sb, mi);
        WriteNonKeyFrameIntraMode<CodecType>(bc, sb, mi);
        WriteNonKeyFrameIntraMode<CodecType>(bc, sb, mi);
        WriteNonKeyFrameIntraMode<CodecType>(bc, sb, mi);
        }
        WriteIntraAngleInfo(bc, sb, mi->sbType, mi->mode, mi->angle_delta_y);
        aom_cdf_prob *probs = sb.frame->m_tileContexts[sb.tileIndex].ectx.uv_mode_cdf[mi->mode];
        WriteSymbol(bc, mi->modeUV, probs, AV1_UV_INTRA_MODES);
        WriteIntraAngleInfo(bc, sb, mi->sbType, mi->modeUV, mi->angle_delta_uv);
        WriteFilterIntraMode(bc, sb, mi);
    }

    template <EnumCodecType CodecType>
    void WriteRefFrames(BoolCoder &bc, const SuperBlock &sb, const ModeInfo &mi, const ModeInfo *above, const ModeInfo *left) {
        Frame &frame = *sb.frame;
        EntropyContexts &ectx = frame.m_tileContexts[sb.tileIndex].ectx;
        const FrameContexts &fc = frame.m_contexts;
        const int32_t compMode = (mi.refIdx[1] != NONE_FRAME);
        if (frame.referenceMode == REFERENCE_MODE_SELECT) {
            WriteSymbol(bc, compMode, ectx.comp_inter_cdf[mi.memCtx.compMode], 2);
        } else {
            assert(frame.referenceMode == compMode);
        }

        uint8_t neighborsRefCounts[7];
        CollectNeighborsRefCounts(above, left, neighborsRefCounts);

        if (compMode == COMPOUND_REFERENCE) {
            assert(mi.refIdx[0] == /*AV1_*/LAST_FRAME && mi.refIdx[1] == /*AV1_*/ALTREF_FRAME);
            const int32_t ctxCompRefType = GetCtxCompRefType(above, left);
            const int32_t ctxCompRef0 = GetCtxCompRefP0(neighborsRefCounts);
            const int32_t ctxCompRef1 = GetCtxCompRefP1(neighborsRefCounts);
            const int32_t ctxCompBwdRef0 = GetCtxCompBwdRefP0(neighborsRefCounts);
            WriteSymbol(bc, BIDIR_COMP_REFERENCE, ectx.comp_ref_type_cdf[ctxCompRefType], 2);
            WriteSymbol(bc, 0, ectx.comp_ref_cdf[ctxCompRef0][0], 2);       // last3_golden_or_last_last2
            WriteSymbol(bc, 0, ectx.comp_ref_cdf[ctxCompRef1][1], 2);       // last2_or_last
            WriteSymbol(bc, 1, ectx.comp_bwdref_cdf[ctxCompBwdRef0][0], 2); // alt_or_alt2_bdw
        } else {
            assert(mi.refIdx[1] == NONE_FRAME);
            if (mi.refIdx[0] == /*AV1_*/LAST_FRAME) {
                const int32_t ctx1 = GetCtxSingleRefP1Av1(neighborsRefCounts);
                const int32_t ctx3 = GetCtxSingleRefP3(neighborsRefCounts);
                const int32_t ctx4 = GetCtxSingleRefP4(neighborsRefCounts);
                WriteSymbol(bc, 0, ectx.single_ref_cdf[ctx1][0], 2); // alt_alt2_bdw_or_last_last2_last3_golden
                WriteSymbol(bc, 0, ectx.single_ref_cdf[ctx3][2], 2); // last3_golden_or_last_last2
                WriteSymbol(bc, 0, ectx.single_ref_cdf[ctx4][3], 2); // last2_or_last
            }
            else if (mi.refIdx[0] == /*AV1_*/GOLDEN_FRAME) {
                const int32_t ctx1 = GetCtxSingleRefP1Av1(neighborsRefCounts);
                const int32_t ctx3 = GetCtxSingleRefP3(neighborsRefCounts);
                const int32_t ctx5 = GetCtxSingleRefP5(neighborsRefCounts);
                WriteSymbol(bc, 0, ectx.single_ref_cdf[ctx1][0], 2); // alt_alt2_bdw_or_last_last2_last3_golden
                WriteSymbol(bc, 1, ectx.single_ref_cdf[ctx3][2], 2); // last3_golden_or_last_last2
                WriteSymbol(bc, 1, ectx.single_ref_cdf[ctx5][4], 2); // golden_or_last3
            }
            else if (mi.refIdx[0] == /*AV1_*/ALTREF_FRAME) {
                const int32_t ctx1 = GetCtxSingleRefP1Av1(neighborsRefCounts);
                const int32_t ctx2 = GetCtxSingleRefP2Av1(neighborsRefCounts);
                WriteSymbol(bc, 1, ectx.single_ref_cdf[ctx1][0], 2); // alt_alt2_bdw_or_last_last2_last3_golden
                WriteSymbol(bc, 1, ectx.single_ref_cdf[ctx2][1], 2); // alt_or_alt2_bdw
            }
            else {
                assert(0);
            }
        }
    }

    bool IsInside(const TileBorders &tileBorders, int32_t candidateR, int32_t candidateC) {
        return candidateR >= tileBorders.rowStart && candidateR < tileBorders.rowEnd
            && candidateC >= tileBorders.colStart && candidateC < tileBorders.colEnd;
    }

    void AddMvRefList(AV1MV refListMv[2], int32_t &refMvCount, AV1MV candMV) {
        if (refMvCount == 0 || refMvCount == 1 && candMV != refListMv[0])
            refListMv[refMvCount++] = candMV;
    }

    void ScaleMv(const Frame &frame, RefIdx refFrame, AV1MV &candMv, RefIdx candFrame) {
        if (frame.refFrameSignBiasVp9[candFrame] != frame.refFrameSignBiasVp9[refFrame]) {
            candMv.mvx *= -1;
            candMv.mvy *= -1;
        }
    }

    AV1MV ScaleMv_(const Frame &frame, RefIdx refFrame, AV1MV candMv, RefIdx candFrame) {
        if (frame.refFrameSignBiasVp9[candFrame] != frame.refFrameSignBiasVp9[refFrame]) {
            candMv.mvx *= -1;
            candMv.mvy *= -1;
        }
        return candMv;
    }

    void IfDiffRefFrameAddMv(const Frame &frame, const ModeInfo &miCand, RefIdx refFrame, AV1MV refListMv[2], int32_t &refMvCount) {
        bool mvsSame = (miCand.mv[0] == miCand.mv[1]);
        AV1MV candMv[2] = { miCand.mv[0], miCand.mv[1] };
        if (miCand.refIdx[0] > INTRA_FRAME && miCand.refIdx[0] != refFrame) {
            ScaleMv(frame, refFrame, candMv[0], miCand.refIdx[0]);
            AddMvRefList(refListMv, refMvCount, candMv[0]);
        }
        if (miCand.refIdx[1] > INTRA_FRAME && miCand.refIdx[1] != refFrame && !mvsSame) {
            ScaleMv(frame, refFrame, candMv[1], miCand.refIdx[1]);
            AddMvRefList(refListMv, refMvCount, candMv[1]);
        }
    }

    int16_t clampMvRow(int16_t mvy, int32_t border, BlockSize sbType, int32_t miRow, int32_t miRows) {
        int32_t bh = block_size_high_8x8[sbType];
        int32_t mbToTopEdge = -((miRow * MI_SIZE) * 8);
        int32_t mbToBottomEdge = ((miRows - bh - miRow) * MI_SIZE) * 8;
        return (int16_t)Saturate(mbToTopEdge - border, mbToBottomEdge + border, mvy);
    }

    int16_t clampMvCol(int16_t mvx, int32_t border, BlockSize sbType, int32_t miCol, int32_t miCols) {
        int32_t bw = block_size_wide_8x8[sbType];
        int32_t mbToLeftEdge = -((miCol * MI_SIZE) * 8);
        int32_t mbToRightEdge = ((miCols - bw - miCol) * MI_SIZE) * 8;
        return (int16_t)Saturate(mbToLeftEdge - border, mbToRightEdge + border, mvx);
    }

    void WriteMvComponentAV1(BoolCoder &bc, int16_t diffmv, int32_t useHp, MvCompContextsAv1 &ctx) {
        assert(useHp || (diffmv & 1) == 0);
        assert(diffmv != 0);
        WriteSymbol(bc, (diffmv < 0), ctx.sign_cdf, 2);
        diffmv = abs(diffmv)-1;
        if (diffmv < 16) {
            WriteSymbol(bc, 0, ctx.classes_cdf, AV1_MV_CLASSES);
            int32_t mv_class0_bit = diffmv >> 3;
            int32_t mv_class0_fr = (diffmv >> 1) & 3;
            int32_t mv_class0_hp = diffmv & 1;
            WriteSymbol(bc, mv_class0_bit, ctx.class0_cdf, CLASS0_SIZE);
            WriteSymbol(bc, mv_class0_fr, ctx.class0_fp_cdf[mv_class0_bit], MV_FP_SIZE);
            if (useHp)
                WriteSymbol(bc, mv_class0_hp, ctx.class0_hp_cdf, 2);
        } else {
            int32_t mvClass = MIN(MV_CLASS_10, BSR(diffmv >> 3));
            WriteSymbol(bc, mvClass, ctx.classes_cdf, AV1_MV_CLASSES);
            diffmv -= CLASS0_SIZE << (mvClass + 2);
            int32_t intdiff = diffmv>>3;
            for (int32_t i = 0; i < mvClass; i++, intdiff >>= 1)
                WriteSymbol(bc, intdiff & 1, ctx.bits_cdf[i], 2);
            int32_t mv_fr = (diffmv >> 1) & 3;
            int32_t mv_hp = diffmv & 1;
            WriteSymbol(bc, mv_fr, ctx.fp_cdf, MV_FP_SIZE);
            if (useHp)
                WriteSymbol(bc, mv_hp, ctx.hp_cdf, 2);
        }
    }

    void WriteMvAV1(BoolCoder &bc, AV1MV mv, AV1MV mvd, int32_t allowHighPrecisionMv, EntropyContexts &ctx, int32_t c) {
        AV1MV bestMv = { (int16_t)(mv.mvx - mvd.mvx), (int16_t)(mv.mvy - mvd.mvy) };
        int32_t useHp = allowHighPrecisionMv && AV1Enc::UseMvHp(bestMv);

        int32_t mvJoint = (mvd.mvx == 0) ? MV_JOINT_ZERO : MV_JOINT_HNZVZ;
        if (mvd.mvy != 0) mvJoint |= MV_JOINT_HZVNZ;
        WriteMvJointAV1(bc, mvJoint, ctx.mv_contexts);

        if (mvJoint & MV_JOINT_HZVNZ)
            WriteMvComponentAV1(bc, mvd.mvy, useHp, ctx.mv_contexts.comps[MV_COMP_VER]);
        if (mvJoint & MV_JOINT_HNZVZ)
            WriteMvComponentAV1(bc, mvd.mvx, useHp, ctx.mv_contexts.comps[MV_COMP_HOR]);
    }

#define FWD_RF_OFFSET(ref) (ref - LAST_FRAME)
#define BWD_RF_OFFSET(ref) (ref - ALTREF_FRAME)
    static inline int8_t RefFrameTypeAV1(const int8_t *const rf) {
        if (rf[1] > INTRA_FRAME) {
            return TOTAL_REFS_PER_FRAME + FWD_RF_OFFSET(rf[0]) +
                BWD_RF_OFFSET(rf[1]) * FWD_REFS;
        }

        return rf[0];
    }

    inline void WriteDrlIdx(BoolCoder &bc, EntropyContexts &ectx, const ModeInfo &mi) {
        uint8_t refFrameType = RefFrameTypeAV1(mi.refIdx);

        assert(mi.refMvIdx < 3);

        if (mi.mode == AV1_NEWMV) {
            if (mi.memCtx.AV1.numDrlBits > 0) {
                WriteSymbol(bc, mi.refMvIdx != 0, ectx.drl_cdf[mi.memCtxAV1_.drl0], 2);
                if (mi.refMvIdx == 0)
                    return;
                if (mi.memCtx.AV1.numDrlBits > 1)
                    WriteSymbol(bc, mi.refMvIdx != 1, ectx.drl_cdf[mi.memCtxAV1_.drl1], 2);
            }
            return;
        }

        //if (mi.mode == AV1_NEARMV) {
        if (have_nearmv_in_inter_mode(mi.mode)) {
            if (mi.memCtx.AV1.numDrlBits > 0) {
                WriteSymbol(bc, mi.refMvIdx != 0, ectx.drl_cdf[mi.memCtxAV1_.drl0], 2);
                if (mi.refMvIdx == 0)
                    return;
                if (mi.memCtx.AV1.numDrlBits > 1)
                    WriteSymbol(bc, mi.refMvIdx != 1, ectx.drl_cdf[mi.memCtxAV1_.drl1], 2);
            }
        }
    }

    void RecordSamples(const ModeInfo *mi, int *pts, int *pts_inref, int *pts_mv, int global_offset_r, int global_offset_c,
                       int row_offset, int sign_r, int col_offset, int sign_c)
    {
        int bw = block_size_wide[mi->sbType];
        int bh = block_size_high[mi->sbType];
        int cr_offset = row_offset * MI_SIZE + sign_r * std::max(bh, MI_SIZE) / 2 - 1;
        int cc_offset = col_offset * MI_SIZE + sign_c * std::max(bw, MI_SIZE) / 2 - 1;
        int x = cc_offset + global_offset_c;
        int y = cr_offset + global_offset_r;

        pts[0] = (x * 8);
        pts[1] = (y * 8);
        pts_inref[0] = (x * 8) + mi->mv[0].mvx;
        pts_inref[1] = (y * 8) + mi->mv[0].mvy;
        pts_mv[0] = mi->mv[0].mvx;
        pts_mv[1] = mi->mv[0].mvy;
    }

    int32_t FindSamples(SuperBlock &sb, const ModeInfo *mi, int32_t miRow, int32_t miCol, int32_t *pts, int32_t *pts_inref, int32_t *pts_mv) {
        const ModeInfo *above = GetAbove(mi, sb.par->miPitch, miRow, sb.par->tileParam.miRowStart[sb.tileRow]);
        const ModeInfo *left = GetLeft(mi, miCol, sb.par->tileParam.miColStart[sb.tileCol]);
        const uint8_t currW = block_size_wide_8x8[mi->sbType];
        const uint8_t currH = block_size_high_8x8[mi->sbType];
        const int32_t global_offset_c = miCol * MI_SIZE;
        const int32_t global_offset_r = miRow * MI_SIZE;
        int32_t ref_frame = mi->refIdx[0];
        int32_t i, mi_step = 1, np = 0;

        int do_tl = 1;
        int do_tr = 1;

        // scan the nearest above rows
        if (above) {
            const uint8_t aboveW = block_size_wide_8x8[above->sbType];
            int mi_row_offset = -1;

            if (currW <= aboveW) {
                // Handle "current block width <= above block width" case.
                int col_offset = -miCol % aboveW;

                if (col_offset < 0)
                    do_tl = 0;
                if (col_offset + aboveW > currW)
                    do_tr = 0;

                if (above->refIdx[0] == ref_frame && above->refIdx[1] == NONE_FRAME) {
                    RecordSamples(above, pts, pts_inref, pts_mv, global_offset_r, global_offset_c, 0, -1, col_offset, 1);
                    pts += 2;
                    pts_inref += 2;
                    pts_mv += 2;
                    np++;
                }
            } else {
                // Handle "current block width > above block width" case.
                for (i = 0; i < std::min((int32_t)currW, sb.par->miCols - miCol); i += mi_step) {
                    int mi_col_offset = i;
                    const ModeInfo *otherMi = mi + mi_col_offset + mi_row_offset * sb.par->miPitch;
                    const uint8_t otherW = block_size_wide_8x8[otherMi->sbType];
                    mi_step = std::min(currW, otherW);

                    if (otherMi->refIdx[0] == ref_frame && otherMi->refIdx[1] == NONE_FRAME) {
                        RecordSamples(otherMi, pts, pts_inref, pts_mv, global_offset_r, global_offset_c, 0, -1, i, 1);
                        pts += 2;
                        pts_inref += 2;
                        pts_mv += 2;
                        np++;
                    }
                }
            }
        }
        assert(2 * np <= SAMPLES_ARRAY_SIZE);

        // scan the nearest left columns
        if (left) {
            int mi_col_offset = -1;

            uint8_t leftH = block_size_high_8x8[left->sbType];

            if (currH <= leftH) {
                // Handle "current block height <= above block height" case.
                int row_offset = -miRow % leftH;

                if (row_offset < 0)
                    do_tl = 0;

                if (left->refIdx[0] == ref_frame && left->refIdx[1] == NONE_FRAME) {
                    RecordSamples(left, pts, pts_inref, pts_mv, global_offset_r, global_offset_c, row_offset, 1, 0, -1);
                    pts += 2;
                    pts_inref += 2;
                    pts_mv += 2;
                    np++;
                }
            } else {
                // Handle "current block height > above block height" case.
                for (i = 0; i < std::min((int32_t)currH, sb.par->miRows - miRow); i += mi_step) {
                    int mi_row_offset = i;
                    const ModeInfo *otherMi = mi + mi_col_offset + mi_row_offset * sb.par->miPitch;
                    const uint8_t otherH = block_size_high_8x8[otherMi->sbType];
                    mi_step = std::min(currH, otherH);

                    if (otherMi->refIdx[0] == ref_frame && otherMi->refIdx[1] == NONE_FRAME) {
                        RecordSamples(otherMi, pts, pts_inref, pts_mv, global_offset_r, global_offset_c, i, 1, 0, -1);
                        pts += 2;
                        pts_inref += 2;
                        pts_mv += 2;
                        np++;
                    }
                }
            }
        }
        assert(2 * np <= SAMPLES_ARRAY_SIZE);

        // Top-left block
        if (do_tl && left && above) {
            int mi_row_offset = -1;
            int mi_col_offset = -1;

            const ModeInfo *topLeftMi = mi - 1 - sb.par->miPitch;

            if (topLeftMi->refIdx[0] == ref_frame && topLeftMi->refIdx[1] == NONE_FRAME) {
                RecordSamples(topLeftMi, pts, pts_inref, pts_mv, global_offset_r, global_offset_c, 0, -1, 0, -1);
                pts += 2;
                pts_inref += 2;
                pts_mv += 2;
                np++;
            }
        }
        assert(2 * np <= SAMPLES_ARRAY_SIZE);

        // Top-right block
        if (do_tr && hasTopRight[miRow & 7][(miCol & 7) + currW - 1]) {
            if (IsInside(GetTileBordersMi(sb.par->tileParam, miRow, miCol), miRow - 1, miCol + currW)) {
                int mi_row_offset = -1;
                int mi_col_offset = currW;

                const ModeInfo *otherMi = mi + mi_col_offset + mi_row_offset * sb.par->miPitch;
                if (otherMi->refIdx[0] == ref_frame && otherMi->refIdx[1] == NONE_FRAME) {
                    RecordSamples(otherMi, pts, pts_inref, pts_mv, global_offset_r, global_offset_c, 0, -1, currW, 1);
                    np++;
                }
            }
        }
        assert(2 * np <= SAMPLES_ARRAY_SIZE);

        return np;
    }

    std::pair<int32_t, int32_t> CountOverlappableNeighbors(SuperBlock &sb, const ModeInfo *mi, int32_t miRow, int32_t miCol) {
        std::pair<int32_t, int32_t> overlappable_neighbors(0, 0);
        if (std::min(block_size_wide[mi->sbType], block_size_high[mi->sbType]) < 8)
            return overlappable_neighbors;

        const ModeInfo *above = GetAbove(mi, sb.par->miPitch, miRow, sb.par->tileParam.miRowStart[sb.tileRow]);
        const ModeInfo *left = GetLeft(mi, miCol, sb.par->tileParam.miColStart[sb.tileCol]);
        const int32_t sbw8 = block_size_wide_8x8[mi->sbType];
        const int32_t sbh8 = block_size_high_8x8[mi->sbType];

        if (above) {
            // prev_row_mi points into the mi array, starting at the beginning of the previous row.
            const ModeInfo *prev_row_mi = mi - miCol - sb.par->miPitch;
            const int32_t endCol = std::min(miCol + sbw8, sb.par->miCols);
            uint8_t mi_step;
            for (int32_t aboveMiCol = miCol; aboveMiCol < endCol; aboveMiCol += mi_step) {
                const ModeInfo *aboveMi = prev_row_mi + aboveMiCol;
                mi_step = block_size_wide_8x8[aboveMi->sbType];
                // If we're considering a block with width 4, it should be treated as
                // half of a pair of blocks with chroma information in the second. Move
                // above_mi_col back to the start of the pair if needed, set above_mbmi
                // to point at the block with chroma information, and set mi_step to 2 to
                // step over the entire pair at the end of the iteration.
                assert(aboveMi->sbType >= BLOCK_8X8);
                if (aboveMi->refIdx[0] != INTRA_FRAME)
                    ++overlappable_neighbors.first;
            }
        }

        if (left) {
            // prev_col_mi points into the mi array, starting at the top of the
            // previous column
            const ModeInfo *prev_col_mi = mi - 1 - miRow * sb.par->miPitch;
            const int endRow = std::min(miRow + sbh8, sb.par->miRows);
            uint8_t mi_step;
            for (int left_mi_row = miRow; left_mi_row < endRow; left_mi_row += mi_step) {
                const ModeInfo *leftMi = prev_col_mi + left_mi_row * sb.par->miPitch;
                mi_step = block_size_high_8x8[leftMi->sbType];
                assert(leftMi->sbType >= BLOCK_8X8);
                if (leftMi->refIdx[0] != INTRA_FRAME)
                    ++overlappable_neighbors.second;
            }
        }

        return overlappable_neighbors;
    }

    int32_t is_global_mv_block(const ModeInfo *mi, TransformationType type) {
        const int32_t block_size_allowed = std::min(block_size_wide[mi->sbType], block_size_high[mi->sbType]) >= 8;
        return (mi->mode == AV1_ZEROMV || mi->mode == ZERO_ZEROMV) && type > TRANSLATION && block_size_allowed;
    }

    int32_t is_motion_variation_allowed_bsize(BlockSize bsize) {
        return std::min(block_size_wide[bsize], block_size_high[bsize]) >= 8;
    }

    int32_t is_inter_mode(PredMode mode) {
        return mode >= AV1_NEARESTMV && mode <= NEW_NEWMV;
    }

    int32_t is_motion_variation_allowed_compound(const ModeInfo *mi) {
        if (mi->refIdx[1] == NONE_FRAME)
            return 1;
        else
            return 0;
    }

    int32_t check_num_overlappable_neighbors(std::pair<int32_t, int32_t> num_overlappable_neighbors) {
        return (num_overlappable_neighbors.first | num_overlappable_neighbors.second) != 0;
    }

    MotionMode MotionModeAllowed(const ModeInfo *mi, TransformationType gmtype, int32_t numProjRef, std::pair<int32_t, int32_t> numOverlappableNeighbors) {
        if (is_global_mv_block(mi, gmtype))
            return SIMPLE_TRANSLATION;
        if (is_motion_variation_allowed_bsize(mi->sbType) &&
            is_inter_mode(mi->mode) /*&& mi->refIdx[1] != INTRA_FRAME*/ &&
            is_motion_variation_allowed_compound(mi))
        {
            if (!check_num_overlappable_neighbors(numOverlappableNeighbors))
                return SIMPLE_TRANSLATION;
            if (mi->refIdx[1] == NONE_FRAME && numProjRef >= 1)
                return WARPED_CAUSAL;
            return OBMC_CAUSAL;
        } else {
            return SIMPLE_TRANSLATION;
        }
    }

    void WriteMotionMode(BoolCoder &bc, SuperBlock &sb, const ModeInfo *mi, int32_t numProjRef, std::pair<int32_t, int32_t> numOverlappableNeighbors) {
        const MotionMode lastMotionModeAllowed = MotionModeAllowed(mi, IDENTITY, numProjRef, numOverlappableNeighbors);
        if (lastMotionModeAllowed != SIMPLE_TRANSLATION) {
            EntropyContexts &ectx = sb.frame->m_tileContexts[sb.tileIndex].ectx;
            if (lastMotionModeAllowed == OBMC_CAUSAL)
                WriteSymbol(bc, 0, ectx.obmc_cdf[mi->sbType], 2);  // SIMPLE_TRANSLATION
            else
                WriteSymbol(bc, SIMPLE_TRANSLATION, ectx.motion_mode_cdf[mi->sbType], MOTION_MODES);  // SIMPLE_TRANSLATION
        }
    }

    template <EnumCodecType CodecType>
    void WriteInterBlockModeInfo(BoolCoder &bc, SuperBlock &sb, const ModeInfo *mi, int32_t miRow, int32_t miCol) {
        const AV1VideoParam &par = *sb.par;
        const Frame &frame = *sb.frame;
        const ModeInfo *above = GetAbove(mi, sb.par->miPitch, miRow, sb.par->tileParam.miRowStart[sb.tileRow]);
        const ModeInfo *left = GetLeft(mi, miCol, sb.par->tileParam.miColStart[sb.tileCol]);
        EntropyContexts &ectx = sb.frame->m_tileContexts[sb.tileIndex].ectx;

        //if (CodecType == CODEC_AV1) {
        //    const int32_t sbw8 = block_size_wide_8x8[mi->sbType];
        //    const int32_t sbh8 = block_size_high_8x8[mi->sbType];
        //    ModeInfo *mmi = const_cast<ModeInfo *>(mi);
        //    if (mmi->refIdx[0] == GOLDEN_FRAME) mmi->refIdx[0] = AV1_GOLDEN_FRAME;
        //    else if (mmi->refIdx[0] == ALTREF_FRAME) mmi->refIdx[0] = AV1_ALTREF_FRAME;
        //    if (mmi->refIdx[1] == ALTREF_FRAME) mmi->refIdx[1] = AV1_ALTREF_FRAME;
        //    PropagateSubPart(mmi, sb.par->miPitch, sbw8, sbh8);
        //}

        WriteRefFrames<CodecType>(bc, sb, *mi, above, left);
        if (0/*seg_feature_active(SEG_LVL_SKIP)*/) {
            assert(0);
            //y_mode = ZEROMV;
        } else if (mi->sbType >= BLOCK_8X8) {
            (mi->refIdx[1] > INTRA_FRAME)
                ? WriteInterCompoundModeAV1(bc, mi->mode, ectx, mi->memCtx.AV1.interMode)
                : WriteInterModeAV1(bc, mi->mode, ectx, mi->memCtx.AV1.interMode);
            if (mi->mode == AV1_NEARMV || mi->mode == AV1_NEWMV || have_nearmv_in_inter_mode(mi->mode))
                WriteDrlIdx(bc, ectx, *mi);
        }
        if (mi->sbType < BLOCK_8X8) {
            assert(0);
        } else {
            if (mi->mode == AV1_NEWMV) {
                WriteMvAV1(bc, mi->mv[0], mi->mvd[0], sb.par->allowHighPrecisionMv, ectx, mi->memCtxAV1_.nmv0);
                if (mi->refIdx[1] >= 0)
                    WriteMvAV1(bc, mi->mv[1], mi->mvd[1], sb.par->allowHighPrecisionMv, ectx, mi->memCtxAV1_.nmv1);
            }
            else if (mi->mode == NEW_NEARESTMV) {
                WriteMvAV1(bc, mi->mv[0], mi->mvd[0], sb.par->allowHighPrecisionMv, ectx, mi->memCtxAV1_.nmv0);
            }
            else if (mi->mode == NEAREST_NEWMV) {
                WriteMvAV1(bc, mi->mv[1], mi->mvd[1], sb.par->allowHighPrecisionMv, ectx, mi->memCtxAV1_.nmv1);
            }
            else if (mi->mode == NEAR_NEWMV) {
                WriteMvAV1(bc, mi->mv[1], mi->mvd[1], sb.par->allowHighPrecisionMv, ectx, mi->memCtxAV1_.nmv1);
            }
            else if (mi->mode == NEW_NEARMV) {
                WriteMvAV1(bc, mi->mv[0], mi->mvd[0], sb.par->allowHighPrecisionMv, ectx, mi->memCtxAV1_.nmv0);
            }
            trace_motion_vector(mi->mv);

            if (par.switchableMotionMode) {
                int32_t numProjRef = 0;
                if (mi->sbType >= BLOCK_8X8 && mi->refIdx[1] == NONE_FRAME) {
                    int pts[SAMPLES_ARRAY_SIZE], pts_inref[SAMPLES_ARRAY_SIZE], pts_mv[SAMPLES_ARRAY_SIZE];
                    numProjRef = FindSamples(sb, mi, miRow, miCol, pts, pts_inref, pts_mv);
                }
                const std::pair<int32_t, int32_t> numOverlappableNeighbors = CountOverlappableNeighbors(sb, mi, miRow, miCol);
                WriteMotionMode(bc, sb, mi, numProjRef, numOverlappableNeighbors);
            }

            int enable_dual_filter = 1;
#if USE_HWPAK_RESTRICT
            enable_dual_filter = 0;
#endif
            if (frame.interpolationFilter == SWITCHABLE) {
                if (av1_is_interp_needed(mi)) {
                    const int32_t ctx = GetCtxInterpBothAV1(above, left, mi->refIdx);
                    WriteInterpAV1(bc, mi->interp0, ectx, ctx & 15);
                    if (enable_dual_filter) {
                        WriteInterpAV1(bc, mi->interp1, ectx, ctx >> 8);
                    }
                }
            }
        }
    }

    template <EnumCodecType CodecType>
    void WriteInterFrameModeInfo(BoolCoder &bc, SuperBlock &sb, int32_t miRow, int32_t miCol) {
        const ModeInfo *mi = sb.frame->m_modeInfo + miCol + miRow * sb.par->miPitch;
        WriteInterSegmentId(*sb.frame);

#ifdef AV1_DEBUG_CONTEXT_MEMOIZATION
        const ModeInfo *above = GetAbove(mi, sb.par->miPitch, miRow, sb.par->tileParam.miRowStart[sb.tileRow]);
        const ModeInfo *left = GetLeft(mi, miCol, sb.par->tileParam.miColStart[sb.tileCol]);
        const int32_t testctxSkip = GetCtxSkip(above, left);
        assert(testctxSkip == mi->memCtx.skip);
        const int32_t testctxIsInter = GetCtxIsInter(above, left);
        assert(testctxIsInter == mi->memCtx.isInter);
#endif // AV1_DEBUG_CONTEXT_MEMOIZATION

        EntropyContexts &ectx = sb.frame->m_tileContexts[sb.tileIndex].ectx;

        const int32_t skip = mi->skip;
        WriteSymbol(bc, skip, ectx.skip_cdf[mi->memCtx.skip], 2);

        if (sb.cdefPreset == -1 && !mi->skip && sb.par->cdefFlag) {
            WriteLiteral<CODEC_AV1>(bc, sb.frame->m_cdefStrenths[sb.sbIndex], sb.frame->m_cdefParam.cdef_bits);
            sb.cdefPreset = sb.frame->m_cdefStrenths[sb.sbIndex];
        }

        const int32_t isInter = (mi->refIdx[0] != INTRA_FRAME);
        WriteSymbol(bc, isInter, ectx.intra_inter_cdf[mi->memCtx.isInter], 2);

        if (isInter)
            WriteInterBlockModeInfo<CodecType>(bc, sb, mi, miRow, miCol);
        else
            WriteIntraBlockModeInfo<CodecType>(bc, sb, mi);

        WriteTxSizeAV1((!skip || !isInter), bc, sb, mi, miRow, miCol);
    }

    const aom_cdf_prob av1_cat1_cdf0[CDF_SIZE(2)] = { AOM_CDF2(20352) };
    const aom_cdf_prob *av1_cat1_cdf[] = { av1_cat1_cdf0 };

    const aom_cdf_prob av1_cat2_cdf0[CDF_SIZE(4)] = { AOM_CDF4(11963, 21121, 27719) };
    const aom_cdf_prob *av1_cat2_cdf[] = { av1_cat2_cdf0 };

    const aom_cdf_prob av1_cat3_cdf0[CDF_SIZE(8)] = { AOM_CDF8(7001, 12802, 17911, 22144, 25503, 28286, 30737) };
    const aom_cdf_prob *av1_cat3_cdf[] = { av1_cat3_cdf0 };

    const aom_cdf_prob av1_cat4_cdf0[CDF_SIZE(16)] = { AOM_CDF16(3934, 7460, 10719, 13640, 16203, 18500, 20624, 22528, 24316, 25919, 27401, 28729, 29894, 30938, 31903) };
    const aom_cdf_prob *av1_cat4_cdf[] = { av1_cat4_cdf0 };

    const aom_cdf_prob av1_cat5_cdf0[CDF_SIZE(16)] = { AOM_CDF16(2942, 5794, 8473, 11069, 13469, 15795, 17980, 20097, 21952, 23750, 25439, 27076, 28589, 30056, 31434) };
    const aom_cdf_prob av1_cat5_cdf1[CDF_SIZE(2)] = { AOM_CDF2(23040) };
    const aom_cdf_prob *av1_cat5_cdf[] = { av1_cat5_cdf0, av1_cat5_cdf1 };

    const aom_cdf_prob av1_cat6_cdf0[CDF_SIZE(16)] = { AOM_CDF16(2382, 4727, 7036, 9309, 11512, 13681, 15816, 17918, 19892, 21835, 23748, 25632, 27458, 29255, 31024) };
    const aom_cdf_prob av1_cat6_cdf1[CDF_SIZE(16)] = { AOM_CDF16(9314, 15584, 19741, 22540, 25391, 27310, 28583, 29440, 30493, 31202, 31672, 31988, 32310, 32527, 32671) };
    const aom_cdf_prob av1_cat6_cdf2[CDF_SIZE(16)] = { AOM_CDF16(29548, 31129, 31960, 32004, 32473, 32498, 32511, 32512, 32745, 32757, 32763, 32764, 32765, 32766, 32767) };
    const aom_cdf_prob av1_cat6_cdf3[CDF_SIZE(16)] = { AOM_CDF16(32006, 32258, 32510, 32512, 32638, 32639, 32640, 32641, 32761, 32762, 32763, 32764, 32765, 32766, 32767) };
    const aom_cdf_prob av1_cat6_cdf4[CDF_SIZE(4)]  = { AOM_CDF4(32513, 32641, 32767) };
    const aom_cdf_prob *av1_cat6_cdf[] = { av1_cat6_cdf0, av1_cat6_cdf1, av1_cat6_cdf2, av1_cat6_cdf3, av1_cat6_cdf4 };

    struct av1_extra_bit {
        const aom_cdf_prob **cdf;
        int len;
        int base_val;
    };

    const av1_extra_bit av1_extra_bits[ENTROPY_TOKENS] = {
        { 0, 0, 0 },                        // ZERO_TOKEN
        { 0, 0, 1 },                        // ONE_TOKEN
        { 0, 0, 2 },                        // TWO_TOKEN
        { 0, 0, 3 },                        // THREE_TOKEN
        { 0, 0, 4 },                        // FOUR_TOKEN
        { av1_cat1_cdf, 1, CAT1_MIN_VAL },  // CATEGORY1_TOKEN
        { av1_cat2_cdf, 2, CAT2_MIN_VAL },  // CATEGORY2_TOKEN
        { av1_cat3_cdf, 3, CAT3_MIN_VAL },  // CATEGORY3_TOKEN
        { av1_cat4_cdf, 4, CAT4_MIN_VAL },  // CATEGORY4_TOKEN
        { av1_cat5_cdf, 5, CAT5_MIN_VAL },  // CATEGORY5_TOKEN
        { av1_cat6_cdf, 18, CAT6_MIN_VAL }, // CATEGORY6_TOKEN
        { 0, 0, 0 }                         // EOB_TOKEN
    };

    void WriteExtraBitsAV1(BoolCoder &bc, int32_t token, int32_t extraBits, int32_t numExtraBits) {
        const aom_cdf_prob **cdf = av1_extra_bits[token].cdf;
        int32_t count = 0;
        while (count < numExtraBits) {
            const int32_t size = std::min(numExtraBits - count, 4);
            const int32_t mask = (1 << size) - 1;
            WriteSymbolNoUpdate(bc, extraBits & mask, *cdf, 1 << size);
            extraBits >>= 4;
            count += 4;
            cdf++;
        }
    }

    static const int32_t MAX_TX_SIZE_UNIT = 16;
    static const int8_t signs[3] = { 0, -1, 1 };
    static const int8_t dc_sign_contexts[4 * MAX_TX_SIZE_UNIT + 1] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
    };
    static const uint8_t skip_contexts[5][5] = {
        { 1, 2, 2, 2, 3 },
        { 1, 4, 4, 4, 5 },
        { 1, 4, 4, 4, 5 },
        { 1, 4, 4, 4, 5 },
        { 1, 4, 4, 4, 6 }
    };

    int32_t combine_entropy_contexts(uint8_t a, uint8_t b) { return (a != 0) + (b != 0); }

    int32_t get_entropy_context(TxSize txSize, const uint8_t *a, const uint8_t *l)
    {
        uint8_t above_ec = 0, left_ec = 0;

        switch (txSize) {
        case TX_4X4:
            above_ec = a[0] != 0;
            left_ec = l[0] != 0;
            break;
        case TX_4X8:
            above_ec = a[0] != 0;
            left_ec = !!*(const uint16_t *)l;
            break;
        case TX_8X4:
            above_ec = !!*(const uint16_t *)a;
            left_ec = l[0] != 0;
            break;
        case TX_8X16:
            above_ec = !!*(const uint16_t *)a;
            left_ec = !!*(const uint32_t *)l;
            break;
        case TX_16X8:
            above_ec = !!*(const uint32_t *)a;
            left_ec = !!*(const uint16_t *)l;
            break;
        case TX_16X32:
            above_ec = !!*(const uint32_t *)a;
            left_ec = !!*(const uint64_t *)l;
            break;
        case TX_32X16:
            above_ec = !!*(const uint64_t *)a;
            left_ec = !!*(const uint32_t *)l;
            break;
        case TX_8X8:
            above_ec = !!*(const uint16_t *)a;
            left_ec = !!*(const uint16_t *)l;
            break;
        case TX_16X16:
            above_ec = !!*(const uint32_t *)a;
            left_ec = !!*(const uint32_t *)l;
            break;
        case TX_32X32:
            above_ec = !!*(const uint64_t *)a;
            left_ec = !!*(const uint64_t *)l;
            break;
        case TX_64X64:
            above_ec = !!(*(const uint64_t *)a | *(const uint64_t *)(a + 8));
            left_ec = !!(*(const uint64_t *)l | *(const uint64_t *)(l + 8));
            break;
        case TX_32X64:
            above_ec = !!*(const uint64_t *)a;
            left_ec = !!(*(const uint64_t *)l | *(const uint64_t *)(l + 8));
            break;
        case TX_64X32:
            above_ec = !!(*(const uint64_t *)a | *(const uint64_t *)(a + 8));
            left_ec = !!*(const uint64_t *)l;
            break;
        case TX_4X16:
            above_ec = a[0] != 0;
            left_ec = !!*(const uint32_t *)l;
            break;
        case TX_16X4:
            above_ec = !!*(const uint32_t *)a;
            left_ec = l[0] != 0;
            break;
        case TX_8X32:
            above_ec = !!*(const uint16_t *)a;
            left_ec = !!*(const uint64_t *)l;
            break;
        case TX_32X8:
            above_ec = !!*(const uint64_t *)a;
            left_ec = !!*(const uint16_t *)l;
            break;
        case TX_16X64:
            above_ec = !!*(const uint32_t *)a;
            left_ec = !!(*(const uint64_t *)l | *(const uint64_t *)(l + 8));
            break;
        case TX_64X16:
            above_ec = !!(*(const uint64_t *)a | *(const uint64_t *)(a + 8));
            left_ec = !!*(const uint32_t *)l;
            break;
        default: assert(0 && "Invalid transform size."); break;
        }
        return combine_entropy_contexts(above_ec, left_ec);
    }

    void GetTxbCtx(BlockSize planeBsize, TxSize txSize, int32_t plane, const uint8_t *aboveCtx, const uint8_t *leftCtx,
                   int32_t *txbSkipCtx, int32_t *dcSignCtx)
    {
        const int32_t txb_w_unit = tx_size_wide_unit[txSize];
        const int32_t txb_h_unit = tx_size_high_unit[txSize];
        int32_t dc_sign = 0;
        int32_t k = 0;

        do {
            const uint32_t sign = aboveCtx[k] >> COEFF_CONTEXT_BITS;
            assert(sign <= 2);
            dc_sign += signs[sign];
        } while (++k < txb_w_unit);

        k = 0;
        do {
            const uint32_t sign = leftCtx[k] >> COEFF_CONTEXT_BITS;
            assert(sign <= 2);
            dc_sign += signs[sign];
        } while (++k < txb_h_unit);

        *dcSignCtx = dc_sign_contexts[dc_sign + 2 * MAX_TX_SIZE_UNIT];

        if (plane == 0) {
            if (planeBsize == txsize_to_bsize[txSize]) {
                *txbSkipCtx = 0;
            } else {
                int top = 0;
                int left = 0;

                k = 0;
                do {
                    top |= aboveCtx[k];
                } while (++k < txb_w_unit);
                top &= COEFF_CONTEXT_MASK;

                k = 0;
                do {
                    left |= leftCtx[k];
                } while (++k < txb_h_unit);
                left &= COEFF_CONTEXT_MASK;
                const int32_t max = std::min(top | left, 4);
                const int32_t min = std::min(std::min(top, left), 4);

                *txbSkipCtx = skip_contexts[min][max];
            }
        } else {
            const int32_t ctx_base = get_entropy_context(txSize, aboveCtx, leftCtx);
            const int32_t ctx_offset =
                (num_pels_log2_lookup[planeBsize] > num_pels_log2_lookup[txsize_to_bsize[txSize]]) ? 10 : 7;
            *txbSkipCtx = ctx_base + ctx_offset;
        }
    }

    TxSize get_txsize_entropy_ctx(TxSize txSize) {
        return (TxSize)((txsize_sqr_map[txSize] + txsize_sqr_up_map[txSize] + 1) >> 1);
    }

    int32_t GetEob(const int16_t *coeffs, const int16_t *scan, int32_t maxEob) {
        int32_t lastNz = maxEob - 1;
        for (; lastNz >= 0; lastNz--)
            if (coeffs[scan[lastNz]])
                break;
        return lastNz + 1;
    }

    const int8_t eob_to_pos_small[33] = {
        0, 1, 2,                                        // 0-2
        3, 3,                                           // 3-4
        4, 4, 4, 4,                                     // 5-8
        5, 5, 5, 5, 5, 5, 5, 5,                         // 9-16
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6  // 17-32
    };

    const int8_t eob_to_pos_large[17] = {
        6,                               // place holder
        7,                               // 33-64
        8,  8,                           // 65-128
        9,  9,  9,  9,                   // 129-256
        10, 10, 10, 10, 10, 10, 10, 10,  // 257-512
        11                               // 513-
    };

    const int16_t k_eob_group_start[12] = { 0, 1, 2, 3, 5, 9, 17, 33, 65, 129, 257, 513 };
    const int16_t k_eob_offset_bits[12] = { 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

    int32_t get_eob_pos_token(int32_t eob)
    {
        assert(eob > 0);
        //const int32_t t = (eob < 33)
        //    ? eob_to_pos_small[eob]
        //    : eob_to_pos_large[ std::min((eob - 1) >> 5, 16) ];
        return (eob == 1) ? eob : BSR(eob - 1) + 2;
    }

    int32_t get_eob_extra(int32_t eob, int32_t eobPt)
    {
        assert(eobPt > 0);
        //*extra = eob - k_eob_group_start[t];
        const int32_t mask = (1 << (eobPt - 2)) - 1;
        return (eob - 1) & mask;
    }

    TxSize av1_get_adjusted_tx_size(TxSize tx_size) {
        if (tx_size == TX_64X64 || tx_size == TX_64X32 || tx_size == TX_32X64)
            return TX_32X32;
        if (tx_size == TX_16X64)
            return TX_16X32;
        if (tx_size == TX_64X16)
            return TX_32X16;
        return tx_size;
    }

    int32_t get_txb_bwl(TxSize tx_size) {
        tx_size = av1_get_adjusted_tx_size(tx_size);
        return tx_size_wide_log2[tx_size];
    }

    int32_t get_txb_wide(TxSize tx_size) {
        tx_size = av1_get_adjusted_tx_size(tx_size);
        return tx_size_wide[tx_size];
    }

    int32_t get_txb_high(TxSize tx_size) {
        tx_size = av1_get_adjusted_tx_size(tx_size);
        return tx_size_high[tx_size];
    }

    const int32_t SIG_REF_DIFF_OFFSET_NUM = 3;

    const int32_t sig_ref_diff_offset[SIG_REF_DIFF_OFFSET_NUM][2] = {
        { 1, 1 }, { 0, 2 }, { 2, 0 }
    };

    const int32_t sig_ref_diff_offset_vert[SIG_REF_DIFF_OFFSET_NUM][2] = {
        { 2, 0 }, { 3, 0 }, { 4, 0 }
    };

    const int32_t sig_ref_diff_offset_horiz[SIG_REF_DIFF_OFFSET_NUM][2] = {
        { 0, 2 }, { 0, 3 }, { 0, 4 }
    };

    const int32_t pos_to_offset[3] = {
        SIG_COEF_CONTEXTS_2D, SIG_COEF_CONTEXTS_2D + 5, SIG_COEF_CONTEXTS_2D + 10
    };


    int32_t get_padded_idx(const int idx, const int bwl) {
        return idx + ((idx >> bwl) << TX_PAD_HOR_LOG2);
    }

    int32_t get_nz_mag(const uint8_t *const levels, const int bwl, const TxClass tx_class)
    {
        int32_t mag;
        // Note: std::min(level, 3) is useless for decoder since level < 3.
        mag = std::min(levels[1], (uint8_t)3);                         // { 0, 1 }
        mag += std::min(levels[(1 << bwl) + TX_PAD_HOR], (uint8_t)3);  // { 1, 0 }

        for (int idx = 0; idx < SIG_REF_DIFF_OFFSET_NUM; ++idx) {
            const int32_t row_offset = ((tx_class == TX_CLASS_2D)
                ? sig_ref_diff_offset[idx][0]
                : ((tx_class == TX_CLASS_VERT)
                    ? sig_ref_diff_offset_vert[idx][0]
                    : sig_ref_diff_offset_horiz[idx][0]));
            const int32_t col_offset = ((tx_class == TX_CLASS_2D)
                ? sig_ref_diff_offset[idx][1]
                : ((tx_class == TX_CLASS_VERT)
                    ? sig_ref_diff_offset_vert[idx][1]
                    : sig_ref_diff_offset_horiz[idx][1]));
            const int32_t nb_pos = (row_offset << bwl) + (row_offset << TX_PAD_HOR_LOG2) + col_offset;
            mag += std::min(levels[nb_pos], (uint8_t)3);
        }
        return mag;
    }

    int32_t get_nz_map_ctx_from_stats(int32_t stats, int32_t coeff_idx,  // raster order
                                     int32_t bwl, TxSize tx_size, TxClass tx_class)
    {
        const int32_t row = coeff_idx >> bwl;
        const int32_t col = coeff_idx - (row << bwl);
        int32_t ctx = (stats + 1) >> 1;
        ctx = std::min(ctx, 4);

        if (tx_class == TX_CLASS_2D) {
            if (row == 0 && col == 0)
                return 0;
            return ctx + av1_nz_map_ctx_offset[tx_size][std::min(row, 4)][std::min(col, 4)];
        } else {
            const int idx = (tx_class == TX_CLASS_VERT) ? row : col;
            const int *map = pos_to_offset;
            return ctx + map[std::min(idx, 2)];
        }
    }

    int32_t get_nz_map_ctx(const uint8_t *levels, int32_t coeff_idx, int32_t bwl, int32_t height, int32_t scan_idx,
                          int32_t is_eob, TxSize tx_size, TxType tx_type) {
        if (is_eob) {
            if (scan_idx == 0) return 0;
            if (scan_idx <= (height << bwl) / 8) return 1;
            if (scan_idx <= (height << bwl) / 4) return 2;
            return 3;
        }
        const TxClass tx_class = tx_type_to_class[tx_type];
        const int32_t stats = get_nz_mag(levels + get_padded_idx(coeff_idx, bwl), bwl, tx_class);
        return get_nz_map_ctx_from_stats(stats, coeff_idx, bwl, tx_size, tx_class);
    }

    int32_t get_coeff_contexts_null(const int16_t *, int8_t *, int8_t *) { assert(0); return 0; }

    void get_context_from_levels(const __m128i &l0, const __m128i &l1, const __m128i &l2, const __m128i &l3, const __m128i &l4,
                                 const __m128i &ctx_offset1, const __m128i &ctx_offset2, __m128i *sum1, __m128i *sum2)
    {
        const __m128i k0 = _mm_setzero_si128();
        const __m128i k3 = _mm_set1_epi8(3);
        const __m128i k4 = _mm_set1_epi8(4);
        const __m128i k6 = _mm_set1_epi8(6);
        __m128i sum;
        sum = _mm_add_epi8(_mm_add_epi8(l0, l1), l2);
        *sum1 = _mm_add_epi8(ctx_offset1, _mm_min_epu8(k6, _mm_avg_epu8(sum, k0)));
        sum = _mm_min_epu8(l0, k3);
        sum = _mm_add_epi8(sum, _mm_min_epu8(l1, k3));
        sum = _mm_add_epi8(sum, _mm_min_epu8(l2, k3));
        sum = _mm_add_epi8(sum, _mm_min_epu8(l3, k3));
        sum = _mm_add_epi8(sum, _mm_min_epu8(l4, k3));
        sum = _mm_avg_epu8(sum, k0); // sum = (sum + 1) >> 1
        sum = _mm_min_epu8(sum, k4);
        sum = _mm_add_epi8(sum, ctx_offset2);
        *sum2 = sum;
    }

    int32_t get_dc_sign_offset(int16_t dc)
    {
        return (dc < 0) ? (1 << COEFF_CONTEXT_BITS)
             : (dc > 0) ? (2 << COEFF_CONTEXT_BITS)
                        : 0;
    }

    void set_dc_sign(int32_t *cul_level, int16_t v)
    {
        if (v < 0)
            *cul_level |= 1 << COEFF_CONTEXT_BITS;
        else if (v > 0)
            *cul_level += 2 << COEFF_CONTEXT_BITS;
    }

    template <TxSize txSize> int32_t get_coeff_contexts_2d(const int16_t *coeff, int8_t *ctx, int8_t *br_ctx)
    {
        alignas(16) uint8_t levels_buf[(32 + TX_PAD_HOR) * (32 + TX_PAD_VER)];
        uint8_t *const levels = levels_buf + (32 + TX_PAD_HOR) * TX_PAD_TOP;
        const int32_t width = (4 << txSize);
        const int32_t height = (4 << txSize);
        const int32_t stride = width + TX_PAD_HOR;
        uint8_t *ls = levels;
        memset(levels + stride * height, 0, sizeof(*levels) * (TX_PAD_BOTTOM * stride));

        for (int32_t i = 0; i < height; i++) {
            for (int32_t j = 0; j < width; j++)
                *ls++ = (uint8_t)Saturate(0, 15, abs(coeff[i * width + j]));
            for (int32_t j = 0; j < TX_PAD_HOR; j++)
                *ls++ = 0;
        }

        assert(txSize <= TX_32X32);
        const int32_t bwl = txSize + 2;
        const int32_t size = 4 << txSize;
        const int32_t pitch = size + TX_PAD_HOR;
        for (int32_t y = 0, pos = 0; y < size; y++, levels += pitch) {
            for (int32_t x = 0; x < size; x++, pos++) {
                int32_t stats;
                stats  = std::min(levels[x + 1], 3);             // { 0, 1 }
                stats += std::min(levels[x + pitch], 3);         // { 1, 0 }
                stats += std::min(levels[x + pitch + 1], 3);     // { 1, 1 }
                stats += std::min(levels[x + 2], 3);             // { 0, 2 }
                stats += std::min(levels[x + pitch + pitch], 3); // { 2, 0 }

                const int32_t row = pos >> bwl;
                const int32_t col = pos - (row << bwl);
                const int32_t c = std::min((stats + 1) >> 1, 4);
                ctx[pos] = c + av1_nz_map_ctx_offset[txSize][std::min(row, 4)][std::min(col, 4)];
            }
        }
        ctx[0] = 0;

        int32_t culLevel = av1_get_txb_entropy_context(coeff, av1_default_scan[BSR(width) - 2], width * width);
        return culLevel;
    }
    template <> int32_t get_coeff_contexts_2d<TX_4X4>(const int16_t *coeff, int8_t *ctx, int8_t *br_ctx)
    {
        const __m128i kzero = _mm_setzero_si128();
        const __m128i k15 = _mm_set1_epi8(MAX_BASE_BR_RANGE);

        __m128i l = _mm_packus_epi16(
            _mm_abs_epi16(loadu2_epi64(coeff + 0, coeff + 4)),
            _mm_abs_epi16(loadu2_epi64(coeff + 8, coeff + 12)));
        __m128i sum = l;
        l = _mm_min_epu8(k15, l);

        sum = _mm_sad_epu8(sum, kzero);
        int32_t culLevel = std::min(COEFF_CONTEXT_MASK, _mm_cvtsi128_si32(sum) + _mm_extract_epi32(sum, 2));
        culLevel += get_dc_sign_offset(coeff[0]);

        const __m128i ctx_offset1 = _mm_setr_epi8(0, 7, 14, 14, 7, 7, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14);
        // av1_nz_map_ctx_offset[TX_4X4]
        const __m128i ctx_offset2 = _mm_setr_epi8(0, 1, 6, 6, 1, 6, 6, 21, 6, 6, 21, 21, 6, 21, 21, 21);
        const __m128i k3 = _mm_set1_epi8(3);

        __m128i sum1, sum2;
        __m128i l0 = _mm_srli_epi32(l, 8);  // + 1
        __m128i l1 = _mm_srli_si128(l, 4);  // + pitch
        __m128i l2 = _mm_srli_si128(l0, 4); // + 1 + pitch
        __m128i l3 = _mm_srli_epi32(l, 16); // + 2
        __m128i l4 = _mm_srli_si128(l, 8);  // + 2 * pitch
        get_context_from_levels(l0, l1, l2, l3, l4, ctx_offset1, ctx_offset2, &sum1, &sum2);
        storea_si128(br_ctx, sum1);
        storea_si128(ctx, sum2);
        ctx[0] = 0;

        return culLevel;
    }

    template <> int32_t get_coeff_contexts_2d<TX_8X8>(const int16_t *coeff, int8_t *ctx, int8_t *br_ctx)
    {
        const __m128i kzero = _mm_setzero_si128();
        const __m128i k15 = _mm_set1_epi8(MAX_BASE_BR_RANGE);

        const __m128i br_ctx_offset = _mm_setr_epi8(0, 7, 14, 14, 14, 14, 14, 14, 7,  7, 14, 14, 14, 14, 14, 14);
        const __m128i all_14 = _mm_set1_epi8(14);
        // av1_nz_map_ctx_offset[TX_8X8]
        const __m128i ctx_offset0 = _mm_setr_epi8(0, 1,  6,  6, 21, 21, 21, 21, 1,  6,  6, 21, 21, 21, 21, 21);
        const __m128i ctx_offset1 = _mm_setr_epi8(6, 6, 21, 21, 21, 21, 21, 21, 6, 21, 21, 21, 21, 21, 21, 21);
        const __m128i all_21 = _mm_set1_epi8(21);

        __m128i l01 = _mm_abs_epi8(_mm_packs_epi16(loada_si128(coeff +  0), loada_si128(coeff +  8)));
        __m128i l23 = _mm_abs_epi8(_mm_packs_epi16(loada_si128(coeff + 16), loada_si128(coeff + 24)));
        __m128i l45 = _mm_abs_epi8(_mm_packs_epi16(loada_si128(coeff + 32), loada_si128(coeff + 40)));
        __m128i l67 = _mm_abs_epi8(_mm_packs_epi16(loada_si128(coeff + 48), loada_si128(coeff + 56)));
        __m128i sum = _mm_adds_epu8(l01, l23);
        sum = _mm_adds_epu8(sum, l45);
        sum = _mm_adds_epu8(sum, l67);
        l01 = _mm_min_epu8(k15, l01);
        l23 = _mm_min_epu8(k15, l23);
        l45 = _mm_min_epu8(k15, l45);
        l67 = _mm_min_epu8(k15, l67);

        __m128i sum1, sum2;
        get_context_from_levels(
            _mm_srli_epi64(l01, 8),                           // + 1
            _mm_alignr_epi8(l23, l01, 8),                     // + pitch
            _mm_srli_epi64(_mm_alignr_epi8(l23, l01, 8), 8),  // + 1 + pitch
            _mm_srli_epi64(l01, 16),                          // + 2
            l23,                                              // + 2 * pitch
            br_ctx_offset, ctx_offset0, &sum1, &sum2);
        storea_si128(br_ctx, sum1);
        storea_si128(ctx, sum2);

        get_context_from_levels(
            _mm_srli_epi64(l23, 8),                           // + 1
            _mm_alignr_epi8(l45, l23, 8),                     // + pitch
            _mm_srli_epi64(_mm_alignr_epi8(l45, l23, 8), 8),  // + 1 + pitch
            _mm_srli_epi64(l23, 16),                          // + 2
            l45,                                              // + 2 * pitch
            all_14, ctx_offset1, &sum1, &sum2);
        storea_si128(br_ctx + 16, sum1);
        storea_si128(ctx + 16, sum2);

        get_context_from_levels(
            _mm_srli_epi64(l45, 8),                           // + 1
            _mm_alignr_epi8(l67, l45, 8),                     // + pitch
            _mm_srli_epi64(_mm_alignr_epi8(l67, l45, 8), 8),  // + 1 + pitch
            _mm_srli_epi64(l45, 16),                          // + 2
            l67,                                              // + 2 * pitch
            all_14, all_21, &sum1, &sum2);
        storea_si128(br_ctx + 32, sum1);
        storea_si128(ctx + 32, sum2);

        get_context_from_levels(
            _mm_srli_epi64(l67, 8),                     // + 1
            _mm_srli_si128(l67, 8),                     // + pitch
            _mm_srli_epi64(_mm_srli_si128(l67, 8), 8),  // + 1 + pitch
            _mm_srli_epi64(l67, 16),                    // + 2
            kzero,                                      // + 2 * pitch
            all_14, all_21, &sum1, &sum2);
        storea_si128(br_ctx + 48, sum1);
        storea_si128(ctx + 48, sum2);

        ctx[0] = 0;
        sum = _mm_sad_epu8(sum, kzero);
        int32_t culLevel = std::min(COEFF_CONTEXT_MASK, _mm_cvtsi128_si32(sum) + _mm_extract_epi32(sum, 2));
        culLevel += get_dc_sign_offset(coeff[0]);
        return culLevel;
    }
    template <> int32_t get_coeff_contexts_2d<TX_16X16>(const int16_t *coeff, int8_t *ctx, int8_t *br_ctx)
    {
        const __m128i kzero = _mm_setzero_si128();
        const __m128i k15 = _mm_set1_epi8(MAX_BASE_BR_RANGE);

        __m128i br_ctx_offset0 = _mm_setr_epi8(0, 7, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14);
        __m128i br_ctx_offset1 = _mm_setr_epi8(7, 7, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14);
        const __m128i all_14 = _mm_set1_epi8(14);
        // av1_nz_map_ctx_offset[TX_16X16]
        __m128i ctx_offset0 = _mm_setr_epi8( 0,  1,  6,  6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21);
        __m128i ctx_offset1 = _mm_setr_epi8( 1,  6,  6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21);
        __m128i ctx_offset2 = _mm_setr_epi8( 6,  6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21);
        __m128i ctx_offset3 = _mm_setr_epi8( 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21);
        const __m128i all_21 = _mm_set1_epi8(21);

        __m128i sum, sum1, sum2;
        __m128i row0 = _mm_abs_epi8(_mm_packs_epi16(loada_si128(coeff +  0), loada_si128(coeff +  8)));
        __m128i row1 = _mm_abs_epi8(_mm_packs_epi16(loada_si128(coeff + 16), loada_si128(coeff + 24)));
        __m128i row2;
        sum = _mm_adds_epu8(row0, row1);
        row0 = _mm_min_epu8(k15, row0);
        row1 = _mm_min_epu8(k15, row1);

        for (int y = 0; y < 14; y++) {
            row2 = _mm_abs_epi8(_mm_packs_epi16(loada_si128(coeff + y * 16 + 32), loada_si128(coeff + y * 16 + 40)));
            sum = _mm_adds_epu8(sum, row2);
            row2 = _mm_min_epu8(k15, row2);

            get_context_from_levels(
                _mm_srli_si128(row0, 1),    // + 1
                row1,                       // + pitch
                _mm_srli_si128(row1, 1),    // + 1 + pitch
                _mm_srli_si128(row0, 2),    // + 2
                row2,                       // + 2 * pitch
                br_ctx_offset0, ctx_offset0, &sum1, &sum2);
            storea_si128(br_ctx + y * 16, sum1);
            storea_si128(ctx + y * 16, sum2);

            row0 = row1;
            row1 = row2;
            ctx_offset0 = ctx_offset1;
            ctx_offset1 = ctx_offset2;
            ctx_offset2 = ctx_offset3;
            ctx_offset3 = all_21;
            br_ctx_offset0 = br_ctx_offset1;
            br_ctx_offset1 = all_14;
        }

        get_context_from_levels(
            _mm_srli_si128(row0, 1),    // + 1
            row1,                       // + pitch
            _mm_srli_si128(row1, 1),    // + 1 + pitch
            _mm_srli_si128(row0, 2),    // + 2
            kzero,                      // + 2 * pitch
            all_14, all_21, &sum1, &sum2);
        storea_si128(br_ctx + 14 * 16, sum1);
        storea_si128(ctx + 14 * 16, sum2);

        get_context_from_levels(
            _mm_srli_si128(row1, 1),    // + 1
            kzero,                      // + pitch
            kzero,                      // + 1 + pitch
            _mm_srli_si128(row1, 2),    // + 2
            kzero,                      // + 2 * pitch
            all_14, all_21, &sum1, &sum2);
        storea_si128(br_ctx + 15 * 16, sum1);
        storea_si128(ctx + 15 * 16, sum2);

        ctx[0] = 0;

        sum = _mm_sad_epu8(sum, kzero);
        int32_t culLevel = std::min(COEFF_CONTEXT_MASK, _mm_cvtsi128_si32(sum) + _mm_extract_epi32(sum, 2));
        culLevel += get_dc_sign_offset(coeff[0]);
        return culLevel;
    }
    template <> int32_t get_coeff_contexts_2d<TX_32X32>(const int16_t *coeff, int8_t *ctx, int8_t *br_ctx)
    {
        const __m128i kzero = _mm_setzero_si128();
        const __m128i k15 = _mm_set1_epi8(MAX_BASE_BR_RANGE);

        __m128i br_ctx_offset0 = _mm_setr_epi8(0, 7, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14);
        __m128i br_ctx_offset1 = _mm_setr_epi8(7, 7, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14);
        const __m128i all_14 = _mm_set1_epi8(14);
        // av1_nz_map_ctx_offset[TX_32X32]
        __m128i ctx_offset0 = _mm_setr_epi8( 0,  1,  6,  6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21);
        __m128i ctx_offset1 = _mm_setr_epi8( 1,  6,  6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21);
        __m128i ctx_offset2 = _mm_setr_epi8( 6,  6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21);
        __m128i ctx_offset3 = _mm_setr_epi8( 6, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21);
        const __m128i all_21 = _mm_set1_epi8(21);

        __m128i sum, sum1, sum2;
        __m128i row0_lo = _mm_abs_epi8(_mm_packs_epi16(loada_si128(coeff +  0), loada_si128(coeff +  8)));
        __m128i row0_hi = _mm_abs_epi8(_mm_packs_epi16(loada_si128(coeff + 16), loada_si128(coeff + 24)));
        __m128i row1_lo = _mm_abs_epi8(_mm_packs_epi16(loada_si128(coeff + 32), loada_si128(coeff + 40)));
        __m128i row1_hi = _mm_abs_epi8(_mm_packs_epi16(loada_si128(coeff + 48), loada_si128(coeff + 56)));
        __m128i row2_lo;
        __m128i row2_hi;
        sum = _mm_adds_epu8(row0_lo, row0_hi);
        sum = _mm_adds_epu8(sum, row1_lo);
        sum = _mm_adds_epu8(sum, row1_hi);
        row0_lo = _mm_min_epu8(k15, row0_lo);
        row0_hi = _mm_min_epu8(k15, row0_hi);
        row1_lo = _mm_min_epu8(k15, row1_lo);
        row1_hi = _mm_min_epu8(k15, row1_hi);

        for (int y = 0; y < 30; y++) {
            row2_lo = _mm_abs_epi8(_mm_packs_epi16(loada_si128(coeff + y * 32 + 64), loada_si128(coeff + y * 32 + 72)));
            row2_hi = _mm_abs_epi8(_mm_packs_epi16(loada_si128(coeff + y * 32 + 80), loada_si128(coeff + y * 32 + 88)));
            sum = _mm_adds_epu8(sum, row2_lo);
            sum = _mm_adds_epu8(sum, row2_hi);
            row2_lo = _mm_min_epu8(k15, row2_lo);
            row2_hi = _mm_min_epu8(k15, row2_hi);

            get_context_from_levels(
                _mm_alignr_epi8(row0_hi, row0_lo, 1),   // + 1
                row1_lo,                                // + pitch
                _mm_alignr_epi8(row1_hi, row1_lo, 1),   // + 1 + pitch
                _mm_alignr_epi8(row0_hi, row0_lo, 2),   // + 2
                row2_lo,                                // + 2 * pitch
                br_ctx_offset0, ctx_offset0, &sum1, &sum2);
            storea_si128(br_ctx + y * 32, sum1);
            storea_si128(ctx + y * 32, sum2);

            get_context_from_levels(
                _mm_srli_si128(row0_hi, 1),    // + 1
                row1_hi,                       // + pitch
                _mm_srli_si128(row1_hi, 1),    // + 1 + pitch
                _mm_srli_si128(row0_hi, 2),    // + 2
                row2_hi,                       // + 2 * pitch
                all_14, all_21, &sum1, &sum2);
            storea_si128(br_ctx + y * 32 + 16, sum1);
            storea_si128(ctx + y * 32 + 16, sum2);

            row0_lo = row1_lo;
            row0_hi = row1_hi;
            row1_lo = row2_lo;
            row1_hi = row2_hi;
            ctx_offset0 = ctx_offset1;
            ctx_offset1 = ctx_offset2;
            ctx_offset2 = ctx_offset3;
            ctx_offset3 = all_21;
            br_ctx_offset0 = br_ctx_offset1;
            br_ctx_offset1 = all_14;
        }

        get_context_from_levels(
            _mm_alignr_epi8(row0_hi, row0_lo, 1),   // + 1
            row1_lo,                                // + pitch
            _mm_alignr_epi8(row1_hi, row1_lo, 1),   // + 1 + pitch
            _mm_alignr_epi8(row0_hi, row0_lo, 2),   // + 2
            kzero,                                  // + 2 * pitch
            all_14, all_21, &sum1, &sum2);
        storea_si128(br_ctx + 30 * 32, sum1);
        storea_si128(ctx + 30 * 32, sum2);

        get_context_from_levels(
            _mm_srli_si128(row0_hi, 1),     // + 1
            row1_hi,                        // + pitch
            _mm_srli_si128(row1_hi, 1),     // + 1 + pitch
            _mm_srli_si128(row0_hi, 2),     // + 2
            kzero,                          // + 2 * pitch
            all_14, all_21, &sum1, &sum2);
        storea_si128(br_ctx + 30 * 32 + 16, sum1);
        storea_si128(ctx + 30 * 32 + 16, sum2);

        get_context_from_levels(
            _mm_alignr_epi8(row1_hi, row1_lo, 1),   // + 1
            kzero,                                  // + pitch
            kzero,                                  // + 1 + pitch
            _mm_alignr_epi8(row1_hi, row1_lo, 2),   // + 2
            kzero,                                  // + 2 * pitch
            all_14, all_21, &sum1, &sum2);
        storea_si128(br_ctx + 31 * 32, sum1);
        storea_si128(ctx + 31 * 32, sum2);

        get_context_from_levels(
            _mm_srli_si128(row1_hi, 1),     // + 1
            kzero,                          // + pitch
            kzero,                          // + 1 + pitch
            _mm_srli_si128(row1_hi, 2),     // + 2
            kzero,                          // + 2 * pitch
            all_14, all_21, &sum1, &sum2);
        storea_si128(br_ctx + 31 * 32 + 16, sum1);
        storea_si128(ctx + 31 * 32 + 16, sum2);

        ctx[0] = 0;

        sum = _mm_sad_epu8(sum, kzero);
        int32_t culLevel = std::min(COEFF_CONTEXT_MASK, _mm_cvtsi128_si32(sum) + _mm_extract_epi32(sum, 2));
        culLevel += get_dc_sign_offset(coeff[0]);
        return culLevel;
    }

    typedef int32_t (* get_coeff_contexts_fptr_t)(const int16_t *coeffs, int8_t *ctx, int8_t *br_ctx);
    extern const get_coeff_contexts_fptr_t get_coeff_contexts_fptr_arr[TX_CLASSES][4] = {
        { get_coeff_contexts_2d<0>, get_coeff_contexts_2d<1>, get_coeff_contexts_2d<2>, get_coeff_contexts_2d<3> },  // 2D
        { get_coeff_contexts_null,  get_coeff_contexts_null,  get_coeff_contexts_null,  get_coeff_contexts_null  },  // HORIZ
        { get_coeff_contexts_null,  get_coeff_contexts_null,  get_coeff_contexts_null,  get_coeff_contexts_null  },  // VERT
    };

    int32_t av1_get_nz_map_contexts_sse(const int16_t *coeff, const int16_t *scan, uint16_t eob,
                                       TxSize txSize, TxClass txClass, int8_t *coeff_contexts, int8_t *br_contexts)
    {
        assert(eob > 0);
        if (eob == 1) {
            coeff_contexts[0] = 0;
            br_contexts[0] = 0;

            const int32_t culLevel =
                (coeff[0] > 0) ? (2 << COEFF_CONTEXT_BITS) + std::min(COEFF_CONTEXT_MASK,  (int32_t)coeff[0]) :
                (coeff[0] < 0) ? (1 << COEFF_CONTEXT_BITS) + std::min(COEFF_CONTEXT_MASK, (int32_t)-coeff[0])
                               : 0;
            return culLevel;
        }

        const int32_t culLevel = get_coeff_contexts_fptr_arr[txClass][txSize](coeff, coeff_contexts, br_contexts);

        const int32_t numCoeff = 16 << (txSize << 1);
        const int32_t lastPos = scan[eob - 1];
        if (eob - 1 <= (numCoeff >> 3))
            coeff_contexts[lastPos] = 1;
        else if (eob - 1 <= (numCoeff >> 2))
            coeff_contexts[lastPos] = 2;
        else
            coeff_contexts[lastPos] = 3;

        return culLevel;
    }

    void av1_get_nz_map_contexts_c(const uint8_t *levels, const int16_t *scan, uint16_t eob,
                                   TxSize tx_size, TxClass txClass, int8_t *coeff_contexts)
    {
        const int32_t bwl = get_txb_bwl(tx_size);
        const int32_t height = get_txb_high(tx_size);
        for (int32_t i = 0; i < eob; ++i) {
            const int32_t pos = scan[i];
            coeff_contexts[pos] = get_nz_map_ctx(levels, pos, bwl, height, i, i == eob - 1, tx_size, txClass);
        }
    }

    const int32_t CONTEXT_MAG_POSITION_NUM = 3;
    const int32_t mag_ref_offset_with_txclass[3][CONTEXT_MAG_POSITION_NUM][2] = {
        { { 0, 1 }, { 1, 0 }, { 1, 1 } },
        { { 0, 1 }, { 1, 0 }, { 0, 2 } },
        { { 0, 1 }, { 1, 0 }, { 2, 0 } }
    };
    const int32_t mag_ref_offset[CONTEXT_MAG_POSITION_NUM][2] = {
        { 0, 1 }, { 1, 0 }, { 1, 1 }
    };

    void get_level_mag_with_txclass(const uint8_t *levels, int32_t stride, int32_t row, int32_t col, int32_t *mag, TxClass tx_class) {
        for (int idx = 0; idx < CONTEXT_MAG_POSITION_NUM; ++idx) {
            const int ref_row = row + mag_ref_offset_with_txclass[tx_class][idx][0];
            const int ref_col = col + mag_ref_offset_with_txclass[tx_class][idx][1];
            const int pos = ref_row * stride + ref_col;
            mag[idx] = levels[pos];
        }
    }

    int32_t get_br_ctx(const uint8_t *levels, int32_t c/*raster order*/, int32_t bwl, TxClass tx_class)
    {
        const int32_t row = c >> bwl;
        const int32_t col = c - (row << bwl);
        const int32_t stride = (1 << bwl) + TX_PAD_HOR;
        int32_t mag = 0;
        int32_t nb_mag[3] = { 0 };
        get_level_mag_with_txclass(levels, stride, row, col, nb_mag, tx_class);

        mag = std::min((nb_mag[0] + nb_mag[1] + nb_mag[2] + 1) >> 1, 6);
        if (c == 0) return mag;
        if (tx_class == TX_CLASS_2D) {
            if ((row < 2) && (col < 2)) return mag + 7;
        } else {
            if (tx_class == TX_CLASS_HORIZ) {
                if (col == 0)
                    return mag + 7;
            } else if (tx_class == TX_CLASS_VERT && row == 0) {
                return mag + 7;
            }
        }
        return mag + 14;
    }

    inline void WriteGolomb(BoolCoder &bc, int32_t level)
    {
        const int32_t x = level + 1;
        const int32_t length = BSR(level + 1) + 1;

        od_ec_encode_bool_q15_128_0_num(&bc.ec, length-1);
        od_ec_encode_bool_q15_128_x_num(&bc.ec, x, length - 1);
    }

    int32_t av1_get_txb_entropy_context(const int16_t *qcoeff, const int16_t *scan, int32_t eob)
    {
        if (eob == 0)
            return 0;

        int32_t cul_level = 0;
        for (int32_t c = 0; c < eob; ++c) {
            cul_level += abs(qcoeff[scan[c]]);
        }

        cul_level = std::min(COEFF_CONTEXT_MASK, cul_level);
        set_dc_sign(&cul_level, qcoeff[0]);

        return cul_level;
    }

    void WriteCoeffsTxb(BoolCoder &bc, SuperBlock &sb, int32_t blkRow, int32_t blkCol,
                        BlockSize planeBsize, TxSize txSize, int32_t plane)
    {
        const int32_t planeType = !!plane;
        const int32_t ssx = planeType;
        const int32_t ssy = planeType;
        const int32_t index = (blkRow & 15) * 16 + (blkCol & 15);
        const int32_t txType = sb.frame->m_txkTypes4x4[sb.sbIndex * 256 + index] & 3;
        //const ScanOrder &so = av1_scans[txSize][txType];
        const int16_t *scan = av1_scans[txSize][txType].scan;
        const int32_t blockIdx = h265_scan_r2z4[(blkRow & 15) * 16 + (blkCol & 15)];
        const CoeffsType *coeffs = sb.coeffs[plane] + (blockIdx << 4 >> (ssx + ssy));
        const int32_t eob = GetEob(coeffs, scan, max_eob[txSize]);

        TileContexts &tileCtx = sb.frame->m_tileContexts[sb.tileIndex];
        EntropyContexts &ectx = tileCtx.ectx;
        uint8_t *actx = tileCtx.aboveNonzero[plane] + ((blkCol - (sb.par->tileParam.miColStart[sb.tileCol] << 1)) >> planeType);
        uint8_t *lctx = tileCtx.leftNonzero [plane] + ((blkRow - (sb.par->tileParam.miRowStart[sb.tileRow] << 1)) >> planeType);
        int32_t txbSkipCtx;
        int32_t dcSignCtx;
        GetTxbCtx(planeBsize, txSize, plane, actx, lctx, &txbSkipCtx, &dcSignCtx);
        const TxSize txSizeCtx = get_txsize_entropy_ctx(txSize);

        if (eob == 0) {
            WriteSymbol(bc, 1, tileCtx.ectx.txb_skip_cdf[txSizeCtx][txbSkipCtx], 2);
            SetCulLevel(actx, lctx, 0, txSize);
            return;
        }

        WriteSymbol(bc, 0, tileCtx.ectx.txb_skip_cdf[txSizeCtx][txbSkipCtx], 2);
        if (plane == 0)
            WriteTxType(bc, sb, blkRow, blkCol);

        const int32_t eobPt = get_eob_pos_token(eob);
        const int32_t eobMultiSize = (txSize << 1);
        const int32_t eobMultiCtx = 0;
        assert(txSize <= TX_32X32);
        assert(eobMultiSize == txsize_log2_minus4[txSize]);
        assert(eobMultiCtx == (tx_type_to_class[txType] == TX_CLASS_2D) ? 0 : 1);

        switch (eobMultiSize) {
        case 0: WriteSymbol(bc, eobPt - 1, ectx.eob_flag_cdf16  [planeType][eobMultiCtx],  5); break;
        case 2: WriteSymbol(bc, eobPt - 1, ectx.eob_flag_cdf64  [planeType][eobMultiCtx],  7); break;
        case 4: WriteSymbol(bc, eobPt - 1, ectx.eob_flag_cdf256 [planeType][eobMultiCtx],  9); break;
        case 6: WriteSymbol(bc, eobPt - 1, ectx.eob_flag_cdf1024[planeType][eobMultiCtx], 11); break;
        default: assert(0); break;
        //case 1:  WriteSymbol(bc, eobPt - 1, ectx.eob_flag_cdf32  [planeType][eobMultiCtx],  6); break;
        //case 3:  WriteSymbol(bc, eobPt - 1, ectx.eob_flag_cdf128 [planeType][eobMultiCtx],  8); break;
        //case 5:  WriteSymbol(bc, eobPt - 1, ectx.eob_flag_cdf512 [planeType][eobMultiCtx], 10); break;
        //default: WriteSymbol(bc, eobPt - 1, ectx.eob_flag_cdf1024[planeType][eobMultiCtx], 11); break;
        }

        if (eobPt > 2) {
            const uint32_t eobExtra = get_eob_extra(eob, eobPt);
            int32_t eobShift = eobPt - 3;
            const int eob_ctx = eobPt - 3;
            WriteSymbol(bc, (eobExtra >> eobShift) & 1, ectx.eob_extra_cdf[txSizeCtx][planeType][eob_ctx], 2);
            od_ec_encode_bool_q15_128_x_num(&bc.ec, eobExtra, eobShift - 1);
//            while (eobShift--)  //for (int i = num; i >= 0; i--) {
//                od_ec_encode_bool_q15_128(&bc.ec, (eobExtra >> eobShift) & 1);
        }

        const int32_t bwl = txSize + 2;
        const int32_t width = (1 << bwl);
        const int32_t height = (1 << bwl);
        const TxClass txClass = tx_type_to_class[txType];
        assert(bwl == get_txb_bwl(txSize));
        assert(width == get_txb_wide(txSize));
        assert(height == get_txb_high(txSize));

        alignas(16) int8_t coeff_contexts[32 * 32];
        alignas(16) int8_t br_contexts[32 * 32];

        const int32_t culLevel = av1_get_nz_map_contexts_sse(coeffs, scan, eob, txSize, txClass, coeff_contexts, br_contexts);
        SetCulLevel(actx, lctx, culLevel, txSize);

        const int32_t pos = scan[eob - 1];
        const int32_t coeffCtx = coeff_contexts[pos];
        const int32_t level = abs(coeffs[pos]);
        if (level <= NUM_BASE_LEVELS) {
            WriteSymbol(bc, level - 1, ectx.coeff_base_eob_cdf[txSizeCtx][planeType][coeffCtx], 3);
        } else {
            WriteSymbol(bc, 2, ectx.coeff_base_eob_cdf[txSizeCtx][planeType][coeffCtx], 3);
            int32_t baseRange = std::min(COEFF_BASE_RANGE, level - 1 - NUM_BASE_LEVELS);
            const int32_t brCtx = br_contexts[pos];

            // Expression x * 10 + 6 >> 5 is an approximation for x / 3:
            //   0..2  -> 0
            //   3..5  -> 1
            //   6..8  -> 2
            //   9..12 -> 3
            const int32_t num3 = (baseRange * 10 + 6) >> 5;
            for (int32_t i = 0; i < num3; i++, baseRange -= 3)
                WriteSymbol4(bc, 3, ectx.coeff_br_cdf[txSizeCtx][planeType][brCtx]);
            WriteSymbol4(bc, baseRange, ectx.coeff_br_cdf[txSizeCtx][planeType][brCtx]);
        }

        int32_t prevCtx = 0, prevBrCtx = 0;
        AV1Enc::aom_cdf_prob* bcdf = ectx.coeff_base_cdf[txSizeCtx][planeType][0];
        AV1Enc::aom_cdf_prob* brcdf = ectx.coeff_br_cdf[txSizeCtx][planeType][0];
        for (int32_t c = eob - 2; c >= 0; --c) {
            const int32_t pos = scan[c];
            const int32_t coeffCtx = coeff_contexts[pos];
            const int32_t level = abs(coeffs[pos]);
            const int l = level - 1 - NUM_BASE_LEVELS;
            const int d = (coeffCtx - prevCtx);
            bcdf += (d<<2) + d;
            prevCtx = coeffCtx;
            WriteSymbol4(bc, l >= 0 ? 3 : level, bcdf);
            if (l >= 0) {
                int32_t baseRange = std::min(COEFF_BASE_RANGE, l);
                const int32_t brCtx = br_contexts[pos];
                const int d = (brCtx - prevBrCtx);
                brcdf += (d << 2) + d;
                prevBrCtx = brCtx;

                const int32_t num3 = (baseRange * 10 + 6) >> 5;
                for (int32_t i = 0; i < num3; i++, baseRange -= 3)
                    WriteSymbol4(bc, 3, brcdf);
                WriteSymbol4(bc, baseRange, brcdf);
            }
        }

        // Loop to code all signs in the transform block,
        // starting with the sign of DC (if applicable)
        const int16_t v = coeffs[0];
        if (v) {
            const uint16_t level = abs(v);
            WriteSymbol(bc, uint16_t(v) >> 15, ectx.dc_sign_cdf[planeType][dcSignCtx], 2);
            if (level > COEFF_BASE_RANGE + NUM_BASE_LEVELS)
                WriteGolomb(bc, level - COEFF_BASE_RANGE - 1 - NUM_BASE_LEVELS);
        }

        for (int32_t c = 1; c < eob; ++c) {
            const int16_t v = coeffs[scan[c]];
            if (v) {
                const uint16_t level = abs(v);
                od_ec_encode_bool_q15_128(&bc.ec, uint16_t(v) >> 15 /* sign */); //  WriteBool128<CODEC_AV1>(bc, uint16_t(v) >> 15);
                if (level > COEFF_BASE_RANGE + NUM_BASE_LEVELS)
                    WriteGolomb(bc, level - COEFF_BASE_RANGE - 1 - NUM_BASE_LEVELS);
            }
        }
    }

    void WriteInterCoeffsTxb(BoolCoder &bc, SuperBlock &sb, int32_t blkRow, int32_t blkCol,
                             BlockSize planeBsize, TxSize txSize, int32_t plane)
    {
        const ModeInfo *mi = sb.frame->m_modeInfo + (blkRow >> 1) * sb.par->miPitch + (blkCol >> 1);

        if (plane || txSize == mi->txSize) {
            WriteCoeffsTxb(bc, sb, blkRow, blkCol, planeBsize, txSize, plane);
            return;
        }

        const TxSize sub_txs = sub_tx_size_map[1][txSize];
        const int bsw = tx_size_wide_unit[sub_txs];
        const int bsh = tx_size_high_unit[sub_txs];

        WriteInterCoeffsTxb(bc, sb, blkRow + 0,   blkCol + 0,   planeBsize, sub_txs, plane);
        WriteInterCoeffsTxb(bc, sb, blkRow + 0,   blkCol + bsw, planeBsize, sub_txs, plane);
        WriteInterCoeffsTxb(bc, sb, blkRow + bsh, blkCol + 0,   planeBsize, sub_txs, plane);
        WriteInterCoeffsTxb(bc, sb, blkRow + bsh, blkCol + bsw, planeBsize, sub_txs, plane);
    }

    void WriteResidualAV1(BoolCoder &bc, SuperBlock &sb, int32_t miRow, int32_t miCol)
    {
        const ModeInfo *mi = sb.frame->m_modeInfo + miCol + miRow * sb.par->miPitch;

        if (mi->skip) {
            TileContexts &tileCtx = sb.frame->m_tileContexts[sb.tileIndex];
            const int32_t block_wide = block_size_wide_4x4[mi->sbType];
            const int32_t block_high = block_size_high_4x4[mi->sbType];
            const int32_t miBlocksToLeft = miCol - sb.par->tileParam.miColStart[sb.tileCol];
            const int32_t miBlocksToTop  = miRow - sb.par->tileParam.miRowStart[sb.tileRow];
            uint8_t *actx = tileCtx.aboveNonzero[0] + miBlocksToLeft;
            uint8_t *lctx = tileCtx.leftNonzero [0] + miBlocksToTop;
            memset(tileCtx.aboveNonzero[0] + (miBlocksToLeft << 1), 0, block_wide);
            memset(tileCtx.leftNonzero [0] + (miBlocksToTop  << 1), 0, block_high);
            memset(tileCtx.aboveNonzero[1] + miBlocksToLeft, 0, block_wide >> 1);
            memset(tileCtx.leftNonzero [1] + miBlocksToTop,  0, block_high >> 1);
            memset(tileCtx.aboveNonzero[2] + miBlocksToLeft, 0, block_wide >> 1);
            memset(tileCtx.leftNonzero [2] + miBlocksToTop,  0, block_high >> 1);
            return;
        }

        const int32_t max_blocks_wide = block_size_wide_4x4[mi->sbType];
        const int32_t max_blocks_high = block_size_high_4x4[mi->sbType];

        if (mi->mode < AV1_INTRA_MODES) {
            for (int32_t plane = 0; plane < 3; ++plane) {
                const int32_t planeType = !!plane;
                const BlockSize plane_bsize = (plane ? ss_size_lookup[mi->sbType][1][1] : mi->sbType);
                const TxSize tx_size = (plane ? max_txsize_rect_lookup[plane_bsize] : mi->txSize);
                const int32_t stepr = tx_size_high_unit[tx_size];
                const int32_t stepc = tx_size_wide_unit[tx_size];
                const int32_t step = stepr * stepc;
                const int32_t unit_width  = plane ? (max_blocks_wide >> 1) : max_blocks_wide;
                const int32_t unit_height = plane ? (max_blocks_high >> 1) : max_blocks_high;

                int32_t block = 0;
                for (int32_t blkRow = 0; blkRow < unit_height; blkRow += stepr) {
                    for (int32_t blkCol = 0; blkCol < unit_width; blkCol += stepc) {
                        WriteCoeffsTxb(bc, sb, miRow * 2 + (blkRow << planeType),
                                       miCol * 2 + (blkCol << planeType),
                                       plane_bsize, tx_size, plane);
                        block += step;
                    }
                }
            }
        } else {
            for (int32_t plane = 0; plane < 3; ++plane) {
                const int32_t planeType = !!plane;
                const BlockSize plane_bsize = (plane ? ss_size_lookup[mi->sbType][1][1] : mi->sbType);
                const TxSize max_tx_size = max_txsize_rect_lookup[plane_bsize];
                const int32_t unit_width  = plane ? (max_blocks_wide >> 1) : max_blocks_wide;
                const int32_t unit_height = plane ? (max_blocks_high >> 1) : max_blocks_high;
                const int32_t bkh = tx_size_high_unit[max_tx_size];
                const int32_t bkw = tx_size_wide_unit[max_tx_size];
                const int32_t step = bkh * bkw;

                int32_t block = 0;
                for (int32_t blkRow = 0; blkRow < unit_height; blkRow += bkh) {
                    for (int32_t blkCol = 0; blkCol < unit_width; blkCol += bkw) {
                        WriteInterCoeffsTxb(bc, sb, miRow * 2 + (blkRow << planeType),
                                            miCol * 2 + (blkCol << planeType),
                                            plane_bsize, max_tx_size, plane);
                        block += step;
                    }
                }
            }
        }
    }

    template <EnumCodecType CodecType>
    void WriteModes(BoolCoder &bc, SuperBlock &sb, int32_t miRow, int32_t miCol) {
        if (sb.frame->IsIntra())
            WriteIntraFrameModeInfo<CodecType>(bc, sb, miRow, miCol);
        else
            WriteInterFrameModeInfo<CodecType>(bc, sb, miRow, miCol);

        WriteResidualAV1(bc, sb, miRow, miCol);
    }

    static int sb_all_skip(int mi_row, int mi_col, SuperBlock &sb, ModeInfo* mi)
    {
        int r, c;
        int maxc, maxr;
        int skip = 1;
        //maxc = cm->mi_cols - mi_col;
        maxc = sb.par->miCols - mi_col;
        maxr = sb.par->miRows - mi_row;
        if (maxr > MAX_MIB_SIZE) maxr = MAX_MIB_SIZE;
        if (maxc > MAX_MIB_SIZE) maxc = MAX_MIB_SIZE;


        for (r = 0; r < maxr; r++) {
            for (c = 0; c < maxc; c++) {
                skip = skip && mi[(mi_row + r) * sb.par->miPitch + mi_col + c].skip;
            }
        }
        return skip;
    }

    int32_t GetPartitionCdfLength(BlockSize bsize) {
        if (bsize <= BLOCK_8X8) return PARTITION_TYPES;
        if (bsize == BLOCK_128X128) return EXT_PARTITION_TYPES - 2;
        return EXT_PARTITION_TYPES;
    }

    BlockSize get_subsize(BlockSize bsize, PartitionType partition) {
        if (partition == PARTITION_INVALID)
            return BLOCK_INVALID;
        else
            return subsize_lookup[partition][bsize];
    }

    void update_partition_context(uint8_t *aboveCtx, uint8_t *leftCtx, BlockSize subsize, BlockSize bsize) {
        const int32_t bw = block_size_wide_8x8[bsize];
        const int32_t bh = block_size_high_8x8[bsize];
        memset(aboveCtx, partition_context_lookup[subsize].above, bw);
        memset(leftCtx,  partition_context_lookup[subsize].left,  bh);
    }

    void update_ext_partition_context(uint8_t *aboveCtx, uint8_t *leftCtx, BlockSize subsize, BlockSize bsize, PartitionType partition) {
        if (bsize >= BLOCK_8X8) {
            const int32_t hbs = block_size_wide_8x8[bsize] / 2;
            const BlockSize bsize2 = get_subsize(bsize, PARTITION_SPLIT);
            switch (partition) {
            case PARTITION_SPLIT:
                if (bsize != BLOCK_8X8) break;
                // fallthrough intended
            case PARTITION_NONE:
            case PARTITION_HORZ:
            case PARTITION_VERT:
            case PARTITION_HORZ_4:
            case PARTITION_VERT_4:
                update_partition_context(aboveCtx, leftCtx, subsize, bsize);
                break;
            case PARTITION_HORZ_A:
                update_partition_context(aboveCtx, leftCtx, bsize2, subsize);
                update_partition_context(aboveCtx, leftCtx + hbs, subsize, subsize);
                break;
            case PARTITION_HORZ_B:
                update_partition_context(aboveCtx, leftCtx, subsize, subsize);
                update_partition_context(aboveCtx, leftCtx + hbs, bsize2, subsize);
                break;
            case PARTITION_VERT_A:
                update_partition_context(aboveCtx, leftCtx, bsize2, subsize);
                update_partition_context(aboveCtx + hbs, leftCtx, subsize, subsize);
                break;
            case PARTITION_VERT_B:
                update_partition_context(aboveCtx, leftCtx, subsize, subsize);
                update_partition_context(aboveCtx + hbs, leftCtx, bsize2, subsize);
                break;
            default: assert(0 && "Invalid partition type");
            }
        }
    }

    template <EnumCodecType CodecType>
    void WritePartition(BoolCoder &bc, SuperBlock &sb, int32_t miRow, int32_t miCol, int32_t log2SbSize) {
        fprintf_trace_cabac(stderr, "\n Partition at poc=%d sbIndex=%d miRow=%d miCol=%d SB%dx%d\n", sb.frame->m_frameOrder, sb.sbIndex, miRow, miCol, 1<<log2SbSize, 1<<log2SbSize);
        const Frame &frame = *sb.frame;
        Frame &frameMutable = *sb.frame;
        TileContexts &tileCtx = frameMutable.m_tileContexts[sb.tileIndex];
        const int32_t miColTileStart = sb.par->tileParam.miColStart[sb.tileCol];
        const int32_t miRowTileStart = sb.par->tileParam.miRowStart[sb.tileRow];
        if (miRow >= sb.par->miRows || miCol >= sb.par->miCols)
            return;

        const ModeInfo *mi = frame.m_modeInfo + miCol + miRow * sb.par->miPitch;
        assert(mi->sbType < BLOCK_SIZES);
        const BlockSize bsize = 3 * (log2SbSize - 2);
        const PartitionType partition = GetPartition(bsize, mi->sbType);

        assert(log2SbSize <= 6 && log2SbSize >= 3);
        int32_t num8x8 = 1 << (log2SbSize - 3);
        int32_t halfBlock8x8 = num8x8 >> 1;
        int32_t hasRows = (miRow + halfBlock8x8) < sb.par->miRows;
        int32_t hasCols = (miCol + halfBlock8x8) < sb.par->miCols;

        int32_t bsl = log2SbSize - 3;
        int32_t above = (tileCtx.abovePartition[miCol - miColTileStart] >> bsl) & 1;
        int32_t left  = (tileCtx.leftPartition [miRow - miRowTileStart] >> bsl) & 1;
        int32_t ctx = bsl * 4 + left * 2 + above;

        aom_cdf_prob tmp[2];
        aom_cdf_prob *cdf = frameMutable.m_tileContexts[sb.tileIndex].ectx.partition_cdf[ctx];

        if (hasRows && hasCols) {
            WriteSymbol(bc, partition, cdf, GetPartitionCdfLength(bsize));
        }
        else if (!hasRows && hasCols) {
            partition_gather_vert_alike(tmp, cdf, bsize);
            WriteSymbolNoUpdate(bc, partition == PARTITION_SPLIT, tmp, 2);
        }
        else if (hasRows && !hasCols) {
            partition_gather_horz_alike(tmp, cdf, bsize);
            WriteSymbolNoUpdate(bc, partition == PARTITION_SPLIT, tmp, 2);
        }

        if (log2SbSize == 3 || partition == PARTITION_NONE) {
            WriteModes<CodecType>(bc, sb, miRow, miCol);
        } else if (partition == PARTITION_HORZ) {
            WriteModes<CodecType>(bc, sb, miRow, miCol);
            if (hasRows)
                WriteModes<CodecType>(bc, sb, miRow + halfBlock8x8, miCol);
        } else if (partition == PARTITION_VERT) {
            WriteModes<CodecType>(bc, sb, miRow, miCol);
            if (hasCols)
                WriteModes<CodecType>(bc, sb, miRow, miCol + halfBlock8x8);
        } else {
            WritePartition<CodecType>(bc, sb, miRow, miCol, log2SbSize - 1);
            WritePartition<CodecType>(bc, sb, miRow, miCol + halfBlock8x8, log2SbSize - 1);
            WritePartition<CodecType>(bc, sb, miRow + halfBlock8x8, miCol, log2SbSize - 1);
            WritePartition<CodecType>(bc, sb, miRow + halfBlock8x8, miCol + halfBlock8x8, log2SbSize - 1);
        }

        uint8_t *aboveCtx = tileCtx.abovePartition + miCol - miColTileStart;
        uint8_t *leftCtx  = tileCtx.leftPartition  + miRow - miRowTileStart;
        const BlockSize subsize = subsize_lookup[partition][bsize];
        update_ext_partition_context(aboveCtx, leftCtx, subsize, bsize, partition);
    }

    template <EnumCodecType CodecType>
    int32_t WriteTile(const AV1VideoParam &par, Frame *frame, const CoeffsType *coefs, const void *tokens, uint8_t *buf, int32_t &off,
                     uint8_t *entBuf, int32_t entBufSize, int32_t tileRow, int32_t tileCol) {
        int32_t sbRowStart = par.tileParam.rowStart[tileRow];
        int32_t sbColStart = par.tileParam.colStart[tileCol];
        int32_t sbRowEnd = par.tileParam.rowEnd[tileRow];
        int32_t sbColEnd = par.tileParam.colEnd[tileCol];
        SuperBlock sb;

        frame->m_tileContexts[tileRow * par.tileParam.cols + tileCol].Clear();

        BoolCoder bc;
        InitBool<CodecType>(bc, buf + (off >> 3), entBuf, entBufSize/3);
        for (int32_t r = sbRowStart; r < sbRowEnd; r++) {
            for (int32_t c = sbColStart; c < sbColEnd; c++) {
                InitSuperBlock(&sb, r, c, par, frame, coefs, const_cast<void *>(tokens));
                fprintf_trace_cabac(stderr, "\n ====== SB %d ======\n", sb.sbIndex);
                WritePartition<CodecType>(bc, sb, r<<3, c<<3, 6);
            }
        }
        ExitBool<CodecType>(bc);

        off += bc.pos<<3;
        return bc.pos;
    }

    void WriteTiles(const AV1VideoParam &par, Frame *frame, const CoeffsType *coefs, const void *tokens, uint8_t *buf, int32_t &off,
                    uint8_t *entBuf, int32_t entBufSize)
    {
        const int32_t tileCols = par.tileParam.cols;
        const int32_t tileRows = par.tileParam.rows;

        for (int32_t tileRow = 0; tileRow < tileRows; tileRow++) {
            for (int32_t tileCol = 0; tileCol < tileCols; tileCol++) {
                bool lastTile = (tileRow == tileRows - 1) && (tileCol == tileCols - 1);
                uint8_t *ptrTileSize = NULL;
                if (!lastTile) {
                    ptrTileSize = buf + (off >> 3);
                    Put32BE(ptrTileSize, 0);
                    off += 32;
                }

                const int32_t tileSize = WriteTile<CODEC_AV1>(par, frame, coefs, tokens, buf, off, entBuf, entBufSize, tileRow, tileCol);

                if (!lastTile)
                    Put32BE(ptrTileSize, tileSize);
            }
        }
    }
};

TxSetType AV1Enc::get_ext_tx_set_type(TxSize tx_size, BlockSize bs, int32_t is_inter, int32_t use_reduced_set) {
    const TxSize tx_size_sqr_up = txsize_sqr_up_map[tx_size];
    const TxSize tx_size_sqr = txsize_sqr_map[tx_size];
    (void)bs;
    if (tx_size_sqr > TX_32X32) return EXT_TX_SET_DCTONLY;
    if (tx_size_sqr_up == TX_32X32)
        return is_inter ? EXT_TX_SET_DCT_IDTX : EXT_TX_SET_DCTONLY;
    if (use_reduced_set)
        return is_inter ? EXT_TX_SET_DCT_IDTX : EXT_TX_SET_DTT4_IDTX;
    if (is_inter)
        return (tx_size_sqr == TX_16X16 ? EXT_TX_SET_DTT9_IDTX_1DDCT : EXT_TX_SET_ALL16);
    else
        return (tx_size_sqr == TX_16X16 ? EXT_TX_SET_DTT4_IDTX : EXT_TX_SET_DTT4_IDTX_1DDCT);
}

const int32_t ext_tx_set_index[2][EXT_TX_SET_TYPES] = {
  { 0, -1,  2, -1,  1, -1, -1, -1, -16 }, // Intra
  { 0,  3, -1, -1, -1, -1,  2, -1,   1 }, // Inter
};

int32_t AV1Enc::get_ext_tx_set(TxSize tx_size, BlockSize bs, int32_t is_inter, int32_t use_reduced_set) {
    const TxSetType set_type = get_ext_tx_set_type(tx_size, bs, is_inter, use_reduced_set);
    return ext_tx_set_index[is_inter][set_type];
}

int32_t AV1Enc::get_ext_tx_types(TxSize tx_size, BlockSize bs, int32_t is_inter, int32_t use_reduced_set) {
    const int set_type = get_ext_tx_set_type(tx_size, bs, is_inter, use_reduced_set);
    return num_ext_tx_set[set_type];
}

int32_t AV1Enc::UseMvHp(const AV1Enc::AV1MV &deltaMv) {
    int32_t sx = deltaMv.mvx >> 15;
    int32_t sy = deltaMv.mvy >> 15;
    int32_t absx = (deltaMv.mvx + sx) ^ sx;
    int32_t absy = (deltaMv.mvy + sy) ^ sy;
    return ((absx | absy) >> 6) == 0;
    //return ((abs(deltaMv.mvx) | abs(deltaMv.mvy)) >> 6) == 0;
    //return ((abs(deltaMv.mvx) >> 3) < COMPANDED_MVREF_THRESH && (abs(deltaMv.mvy) >> 3) < COMPANDED_MVREF_THRESH);
}

namespace {
    const ModeInfo *GetCandModeInfo(const ModeInfo *frameMi, int32_t sb64Cols, int32_t candidateR, int32_t candidateC) {
        int32_t candSbR = candidateR >> 3;
        int32_t candSbC = candidateC >> 3;
        int32_t candR4 = (candidateR & 7) << 1;
        int32_t candC4 = (candidateC & 7) << 1;
        const ModeInfo *miCand = frameMi + ((candSbR * sb64Cols + candSbC) << 8);
        assert(!(candC4 & 1) && !(candR4 & 1)); // 8x8 granularity
        return miCand + h265_scan_r2z4[candC4 + (candR4 << 4)];
    }
};

int32_t AV1Enc::FindBestRefMvs(AV1MV refListMv[2], const AV1VideoParam &par) {
    if (!par.allowHighPrecisionMv) {
        __m128i mvs = loadl_epi64(refListMv);
        __m128i sign = _mm_srli_epi16(mvs, 15);
        __m128i ones = _mm_slli_epi16(_mm_cmpeq_epi16(_mm_setzero_si128(), _mm_setzero_si128()), 1);
        mvs = _mm_add_epi16(mvs, sign);
        mvs = _mm_and_si128(mvs, ones);
        storel_epi64(refListMv, mvs);
        return 0;
    } else {
        int32_t useMvHpForNearest = UseMvHp(refListMv[0]);
        if (!useMvHpForNearest) {
            refListMv[0].mvx = (refListMv[0].mvx - (refListMv[0].mvx >> 15)) & ~1;
            refListMv[0].mvy = (refListMv[0].mvy - (refListMv[0].mvy >> 15)) & ~1;
        }
        if (!UseMvHp(refListMv[1])) {
            refListMv[1].mvx = (refListMv[1].mvx - (refListMv[1].mvx >> 15)) & ~1;
            refListMv[1].mvy = (refListMv[1].mvy - (refListMv[1].mvy >> 15)) & ~1;
        }
        return useMvHpForNearest;
    }
    // same as:
    //for (int32_t i = 0; i < MAX_MV_REF_CANDIDATES; i++) {
    //    int16_t deltaRow = refListMv[i].mvy;
    //    int16_t deltaCol = refListMv[i].mvx;
    //    if (!par.allowHighPrecisionMv || !UseMvHp(refListMv[i])) {
    //        if (deltaRow & 1)
    //            deltaRow += (deltaRow > 0 ? -1 : 1);
    //        if (deltaCol & 1)
    //            deltaCol += (deltaCol > 0 ? -1 : 1);
    //    }
    //    refListMv[i].mvy = clampMvRow(deltaRow, (BORDERINPIXELS - INTERP_EXTEND) << 3, blockSize, miRow, par.miRows);
    //    refListMv[i].mvx = clampMvCol(deltaCol, (BORDERINPIXELS - INTERP_EXTEND) << 3, blockSize, miCol, par.miCols);
    //}
}

int AV1Enc::av1_is_interp_needed(const ModeInfo *mi) {
    //const MB_MODE_INFO *const mbmi = xd->mi[0];
    //if (mi->skip) return 0;
    //if (mbmi->motion_mode == WARPED_CAUSAL) return 0;
    if (mi->mode == AV1_ZEROMV) return 0;
    return 1;
}

void AV1Enc::WriteIvfHeader(uint8_t *buf, int32_t &bitoff, const AV1VideoParam &par)
{
    buf += bitoff>>3;
    Put8(buf+0, 'D');
    Put8(buf+1, 'K');
    Put8(buf+2, 'I');
    Put8(buf+3, 'F');
    Put16LE(buf+4, 0);              // version
    Put16LE(buf+6, 32);             // header size
    Put8(buf+8, 'A');           // codecId
    Put8(buf+9, 'V');
    Put8(buf+10, '0');
    Put8(buf+11, '1');
    Put16LE(buf+12, par.Width);     // width
    Put16LE(buf+14, par.Height);    // height
    Put32LE(buf+16, 1000);          // rate
    Put32LE(buf+20, 1);             // scale
    Put32LE(buf+24, 0);             // frame count (unknown at this point)
    Put32LE(buf+28, 0);             // unused
    bitoff += IVF_HEADER_SIZE;
}

void AV1Enc::WriteRepeatedFrame(const AV1VideoParam &par, const Frame &frame, mfxBitstream &mfxBS)
{
    assert(frame.showExistingFrame == 1);
    uint8_t buf[128];
    Put32LE(buf+0, 0); // frame.frameSize
    Put64LE(buf+4, static_cast<int32_t>(frame.m_timeStamp/90.f));
    int32_t written = IVF_FRAME_HEADER_SIZE;

    int32_t frameStart = written;

    int32_t obuSizeOff = written;
    int32_t obuHeaderSize = WriteObuHeader(buf, written, OBU_FRAME_HEADER);
    PutBit(buf, written, 1); // showExistingFrame
    PutBits(buf, written, frame.frameToShowMapIdx, 3);

    if (par.seqParams.frameIdNumbersPresentFlag)
        PutBits(buf, written, frame.m_encOrder, par.seqParams.idLen);

    WriteTrailingBits(buf, written);
    WriteObuSize(buf, obuSizeOff + 8 * obuHeaderSize, written);


    assert(written % 8 == 0);
    uint32_t totalFrameSizeInBytes = written >> 3;
    uint32_t vp9FrameSizeInBytes = (written - frameStart) >> 3;
    assert(totalFrameSizeInBytes <= mfxBS.MaxLength - mfxBS.DataOffset - mfxBS.DataLength);
    if (totalFrameSizeInBytes > mfxBS.MaxLength - mfxBS.DataOffset - mfxBS.DataLength) {
        int32_t cut = totalFrameSizeInBytes - (mfxBS.MaxLength - mfxBS.DataOffset - mfxBS.DataLength);
        vp9FrameSizeInBytes -= cut;
        totalFrameSizeInBytes -= cut;
    }
    Put32LE(buf+0, vp9FrameSizeInBytes);

    memcpy(mfxBS.Data + mfxBS.DataOffset + mfxBS.DataLength, buf, totalFrameSizeInBytes);
    mfxBS.DataLength += totalFrameSizeInBytes;

    // fill bitstream params
    mfxI32 dpbOutputDelay = par.maxNumReorderPics + frame.m_frameOrder - frame.m_encOrder;
    mfxBS.TimeStamp = frame.m_timeStamp;
    mfxBS.DecodeTimeStamp = mfxI64(mfxBS.TimeStamp - par.tcDuration90KHz * dpbOutputDelay); // calculate DTS from PTS
    mfxBS.FrameType = (mfxU16)frame.m_frameType;
}

void AV1FrameEncoder::PackTile(int32_t tile)
{
    fprintf_trace_cabac(stderr, "\n ====== Pack tile %d (poc=%d) ======\n\n", tile, m_frame->m_frameOrder);

#if USE_CMODEL_PAK
    PackTile_viaCmodel(tile);
    return;
#endif
    assert(tile < m_numTiles);

    SetupPastIndependenceAv1(m_frame, tile);

    const int32_t tileRow = tile / m_videoParam.tileParam.cols;
    const int32_t tileCol = tile % m_videoParam.tileParam.cols;
    CompressedBuf &compTile = m_compressedTiles[tile];
    uint8_t *dst = compTile.buf;
    int32_t written = 0;
    compTile.size = WriteTile<CODEC_AV1>(m_videoParam, m_frame, m_coeffWork, m_tokens, dst, written, compTile.entBuf, compTile.entBufCapacity, tileRow, tileCol);
    assert(compTile.size <= compTile.capacity);
}

namespace {
    void SafeCopyToExternalBitstream(mfxBitstream *extBs, const uint8_t *ptr, int32_t nbytes) {
        if (nbytes > int32_t(extBs->MaxLength - extBs->DataOffset - extBs->DataLength))
            nbytes = int32_t(extBs->MaxLength - extBs->DataOffset - extBs->DataLength);
        memcpy(extBs->Data + extBs->DataOffset + extBs->DataLength, ptr, nbytes);
        extBs->DataLength += nbytes;
    }
};

int32_t AV1FrameEncoder::GetOutputData(mfxBitstream &mfxBS)
{
    assert(m_frame->showExistingFrame == 0);

    const AV1VideoParam &par = m_topEnc.m_videoParam;
    uint8_t *dst = mfxBS.Data + mfxBS.DataOffset + mfxBS.DataLength;
    int32_t written = 0;

    if (m_frame->m_encOrder == 0) {
        WriteIvfHeader(dst, written, par);
        mfxBS.DataLength += IVF_HEADER_SIZE >> 3;
    }

    int32_t pos_frame_size = written;
    const int32_t frameSize = 0; // unknown at this point
    const uint64_t timeStamp = static_cast<uint64_t>(m_frame->m_timeStamp / 90.f);
    WriteIvfFrameHeader(dst, written, frameSize, timeStamp);
    mfxBS.DataLength += IVF_FRAME_HEADER_SIZE >> 3;

    //int32_t superFrameIndexPos = written;
    //if (m_videoParam.codecType == CODEC_AV1) {
    //    if (!m_frame->m_hiddenFrames.empty()) {
    //        // make room for superframe index
    //        const int32_t superFrameIndexBytes = 2 + 4 * m_frame->m_hiddenFrames.size();
    //        written += superFrameIndexBytes * 8;
    //        mfxBS.DataLength += superFrameIndexBytes;
    //    }
    //}

    for (int32_t i = 0; i <= (int32_t)m_frame->m_hiddenFrames.size(); i++) {
        Frame *frame = (i == m_frame->m_hiddenFrames.size()) ? m_frame : m_frame->m_hiddenFrames[i];
        int32_t showFrame = (frame == m_frame);
        AV1FrameEncoder &fenc = *frame->m_fenc;

        int32_t startFramePos = written;
        WriteSequenceHeaderObu(dst, written, par, *frame, 0, showFrame);
        mfxBS.DataLength += (written - startFramePos) >> 3;

        int32_t obuTileGroupSizeOff = written;
        int32_t obuHeaderSize = 0;
        obuHeaderSize = WriteObuHeader(dst, written, OBU_FRAME);

        WriteFrameHeaderObu(dst, written, par, *frame, 0, showFrame);
        WritePaddingBits(dst, written);
        // write_tile_group_header()
        {
            const int32_t tiles_log2 = par.tileParam.log2Cols + par.tileParam.log2Rows;
            const int32_t tile_start_and_end_present_flag = 0;//1; spec requirement for OBU_FRAME

            if (tiles_log2) {
                PutBit(dst, written, tile_start_and_end_present_flag);
                if (tile_start_and_end_present_flag) {
                    PutBits(dst, written, 0, par.tileParam.log2Cols + par.tileParam.log2Rows);
                    PutBits(dst, written, fenc.m_numTiles - 1, par.tileParam.log2Cols + par.tileParam.log2Rows);
                }
            }
            WritePaddingBits(dst, written);
        }
        mfxBS.DataLength += (written - obuTileGroupSizeOff) >> 3;

        for (int32_t tile = 0; tile < fenc.m_numTiles; tile++) {
            if (tile + 1 != fenc.m_numTiles) {
                Put32LE(dst + (written >> 3), fenc.m_compressedTiles[tile].size - AV1_MIN_TILE_SIZE_BYTES);
                written += 4 << 3;
                mfxBS.DataLength += 4;
            }
            SafeCopyToExternalBitstream(&mfxBS, fenc.m_compressedTiles[tile].buf, fenc.m_compressedTiles[tile].size);
            written += fenc.m_compressedTiles[tile].size << 3;
        }
        frame->frameSizeInBytes = (written - startFramePos) >> 3;
        const int32_t oldOffset = written;
        WriteObuSize(dst, obuTileGroupSizeOff + 8 * obuHeaderSize, written);
        mfxBS.DataLength += (written - oldOffset) >> 3;
    }
    assert(mfxBS.DataOffset + mfxBS.DataLength <= mfxBS.MaxLength);

    uint32_t vp9FrameSizeInBytes = (written - pos_frame_size - IVF_FRAME_HEADER_SIZE) >> 3;
    Put32LE(dst + (pos_frame_size >> 3), vp9FrameSizeInBytes); // patch frame size in IVF frame header

    // fill bitstream params
    mfxI32 dpbOutputDelay = m_videoParam.maxNumReorderPics + m_frame->m_frameOrder - m_frame->m_encOrder;
    mfxBS.TimeStamp = m_frame->m_timeStamp;
    mfxBS.DecodeTimeStamp = mfxI64(mfxBS.TimeStamp - m_videoParam.tcDuration90KHz * dpbOutputDelay); // calculate DTS from PTS
    mfxBS.FrameType = (mfxU16)m_frame->m_frameType;

    return 0;
}
#endif // MFX_ENABLE_AV1_VIDEO_ENCODE