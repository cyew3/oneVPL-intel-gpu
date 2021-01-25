// Copyright (c) 2018-2020 Intel Corporation
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

#include <av1_parser.h>
#include <algorithm>

using namespace BS_AV1;

const Bs8u MAX_LEB128_SIZE = 8;
const Bs8u LEB128_BYTE_MASK = 0x7f;

BSErr AV1_BitStream::av1_read_obu_size(size_t& size)
{
    size_t value = 0;
    for (size_t i = 0; i < MAX_LEB128_SIZE; ++i)
    {
        const Bs8u cur_byte = static_cast<Bs8u>(f(8));
        const Bs8u decoded_byte = cur_byte & LEB128_BYTE_MASK;
        value |= ((Bs64u)decoded_byte) << (i * 7);
        if ((cur_byte & ~LEB128_BYTE_MASK) == 0)
        {
            size = value;
            return BS_ERR_NONE;
        }
    }

    throw BS_ERR_INVALID_PARAMS;
}

BSErr AV1_BitStream::obu_header(Frame& frame, OBUHeader& header)
{
    size_t sizeFieldLength = 0;
    size_t obu_size = 0;

    f(); // first bit is obu_forbidden_bit (0)

    header.obu_type = (AV1_OBU_TYPE)f(4);

    const int obu_extension_flag = f();
    header.obu_has_size_field = f();
    f(); // reserved

    if (obu_extension_flag) // if has extension
    {
        header.temporal_id = f(3);
        header.spatial_id = f(2);
        f(3);
    }

    if (header.obu_has_size_field)
        av1_read_obu_size(obu_size);
    else if (header.obu_type != OBU_TEMPORAL_DELIMITER)
        throw BS_ERR_NOT_IMPLEMENTED; // no support for OBUs w/o size field so far

    return BS_ERR_NONE;
}

Bs32u AV1_BitStream::read_uvlc()
{
    Bs32u leading_zeros = 0;
    while (!f()) ++leading_zeros;

    // Maximum 32 bits.
    if (leading_zeros >= 32)
        return std::numeric_limits<Bs32u>::max();
    const Bs32u base = (1u << leading_zeros) - 1;
    const Bs32u value = f(leading_zeros);
    return base + value;
}

void AV1_BitStream::av1_timing_info(TimingInfo& info)
{
    info.num_units_in_display_tick = f(32);
    info.time_scale = f(32);
    info.equal_picture_interval = f();
    if (info.equal_picture_interval)
        info.num_ticks_per_picture_minus_1 = read_uvlc();
}

void AV1_BitStream::av1_decoder_model_info(DecoderModelInfo& info)
{
    info.buffer_delay_length_minus_1 = f(5);
    info.num_units_in_decoding_tick = f(32);
    info.buffer_removal_time_length_minus_1 = f(5);
    info.frame_presentation_time_length_minus_1 = f(5);
}

void AV1_BitStream::av1_operating_parameters_info(OperatingParametersInfo& info, Bs32u length)
{
    info.decoder_buffer_delay = f(length);
    info.encoder_buffer_delay = f(length);
    info.low_delay_mode_flag = f();
}

void AV1_BitStream::av1_color_config(ColorConfig& config, Bs32u profile)
{
    config.BitDepth = f() ? 10 : 8;
    if (profile == 2 && config.BitDepth != 8)
        config.BitDepth = f() ? 12 : 10;

    config.mono_chrome = profile != 1 ? f() : 0;

    Bs32u color_description_present_flag = f();
    if (color_description_present_flag)
    {
        config.color_primaries = f(8);
        config.transfer_characteristics = f(8);
        config.matrix_coefficients = f(8);
    }
    else
    {
        config.color_primaries = AOM_CICP_CP_UNSPECIFIED;
        config.transfer_characteristics = AOM_CICP_TC_UNSPECIFIED;
        config.matrix_coefficients = AOM_CICP_MC_UNSPECIFIED;
    }

    if (config.mono_chrome)
    {

        config.color_range = f();
        config.subsampling_y = config.subsampling_x = 1;
        config.chroma_sample_position = AOM_CSP_UNKNOWN;
        config.separate_uv_delta_q = 0;

        return;
    }

    if (config.color_primaries == AOM_CICP_CP_BT_709 &&
        config.transfer_characteristics == AOM_CICP_TC_SRGB &&
        config.matrix_coefficients == AOM_CICP_MC_IDENTITY)
    {
        config.color_range = AOM_CR_FULL_RANGE;
        config.subsampling_y = config.subsampling_x = 0;
    }
    else
    {
        // [16,235] (including xvycc) vs [0,255] range
        config.color_range = f();
        if (profile == 0) // 420 only
            config.subsampling_x = config.subsampling_y = 1;
        else if (profile == 1) // 444 only
            config.subsampling_x = config.subsampling_y = 0;
        else if (profile == 2)
        {
            if (config.BitDepth == 12)
            {
                config.subsampling_x = f();
                if (config.subsampling_x == 0)
                    config.subsampling_y = 0;  // 444
                else
                    config.subsampling_y = f();  // 422 or 420
            }
            else
            {
                // 422
                config.subsampling_x = 1;
                config.subsampling_y = 0;
            }
        }
        if (config.subsampling_x == 1 && config.subsampling_y == 1)
            config.chroma_sample_position = f(2);
    }

    config.separate_uv_delta_q = f();
}

BSErr AV1_BitStream::sequence_header(Frame& frame)
{
    SequenceHeader& sh = frame.sh;

    sh.seq_profile = f(3);
    sh.still_picture = f();
    sh.reduced_still_picture_header = f();

    if (sh.reduced_still_picture_header)
        f(5); // seq_level_idx[0]
    else
    {
        sh.timing_info_present_flag = f();
        if (sh.timing_info_present_flag)
        {
            av1_timing_info(sh.timing_info);
            sh.decoder_model_info_present_flag = f();
            if (sh.decoder_model_info_present_flag)
                av1_decoder_model_info(sh.decoder_model_info);
        }

        const int initial_display_delay_present_flag = f();
        sh.operating_points_cnt_minus_1 = f(5);
        for (unsigned int i = 0; i <= sh.operating_points_cnt_minus_1; i++)
        {
            sh.operating_point_idc[i] = f(12);
            sh.seq_level_idx[i] = f(5);
            if (sh.seq_level_idx[i] > 7)
                sh.seq_tier[i] = f();

            if (sh.decoder_model_info_present_flag)
            {
                sh.decoder_model_present_for_this_op[i] = f();
                if (sh.decoder_model_present_for_this_op[i])
                    av1_operating_parameters_info(sh.operating_parameters_info[i],
                        sh.decoder_model_info.buffer_delay_length_minus_1 + 1);
            }

            const int BufferPoolMaxSize = 10; // av1 spec E.2
            sh.initial_display_delay_minus_1[i] = BufferPoolMaxSize - 1;
            if (initial_display_delay_present_flag)
            {
                if (f()) // initial_display_delay_present_for_this_op[i]
                    sh.initial_display_delay_minus_1[i] = f(4);
            }
        }
    }

    sh.frame_width_bits = f(4) + 1;
    sh.frame_height_bits = f(4) + 1;

    sh.max_frame_width = f(sh.frame_width_bits) + 1;
    sh.max_frame_height = f(sh.frame_height_bits) + 1;

    sh.frame_id_numbers_present_flag = f();
    if (sh.frame_id_numbers_present_flag) {
        sh.delta_frame_id_length = f(4) + 2;
        sh.idLen = f(3) + sh.delta_frame_id_length + 1;
    }

    sh.sbSize = f() ? BLOCK_128X128 : BLOCK_64X64;

    sh.enable_filter_intra = f();
    sh.enable_intra_edge_filter = f();

    if (sh.reduced_still_picture_header)
    {
        sh.seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
        sh.seq_force_integer_mv = SELECT_INTEGER_MV;
    }
    else
    {
        sh.enable_interintra_compound = f();
        sh.enable_masked_compound = f();
        sh.enable_warped_motion = f();
        sh.enable_dual_filter = f();
        sh.enable_order_hint = f();
        if (sh.enable_order_hint)
        {
            sh.enable_jnt_comp = f();
            sh.enable_ref_frame_mvs = f();
        }

        if (f()) // seq_choose_screen_content_tools
            sh.seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
        else
            sh.seq_force_screen_content_tools = f();

        if (sh.seq_force_screen_content_tools > 0)
        {
            if (f()) // seq_choose_integer_mv
                sh.seq_force_integer_mv = 2;
            else
                sh.seq_force_integer_mv = f();
        }
        else
            sh.seq_force_integer_mv = 2;

        sh.order_hint_bits_minus1 =
            sh.enable_order_hint ? f(3) : -1;
    }

    sh.enable_superres = f();
    sh.enable_cdef = f();
    sh.enable_restoration = f();

    av1_color_config(sh.color_config, sh.seq_profile);

    sh.film_grain_param_present = f();

    // trailing bits
    if (f(1) != 1) //trailing_one_bit
    {
        BS_TRACE(1, Invalid trailing bit (stream corrupted?) );
        return BS_ERR_INVALID_PARAMS;
    }
    while (bit & 7)
    {
        if (f(1) != 0) //trailing_zero_bit
        {
            BS_TRACE(1, Invalid trailing bit (stream corrupted?) );
            return BS_ERR_INVALID_PARAMS;
        }
    }

    return BS_ERR_NONE;
}

void AV1_BitStream::av1_read_timing_point_info(SequenceHeader const& sh, FrameHeader& fh)
{
    if (sh.decoder_model_info_present_flag && !sh.timing_info.equal_picture_interval)
    {
        const int n = sh.decoder_model_info.frame_presentation_time_length_minus_1 + 1;
        fh.frame_presentation_time = f(n);
    }
}

inline bool FrameIsIntraOnly(FrameHeader const& fh)
{
    return fh.frame_type == INTRA_ONLY_FRAME;
}

inline bool FrameIsIntra(FrameHeader const& fh)
{
    return (fh.frame_type == KEY_FRAME || FrameIsIntraOnly(fh));
}

inline bool FrameIsResilient(FrameHeader const& fh)
{
    return FrameIsIntra(fh) || fh.error_resilient_mode;
}

const Bs8u PRIMARY_REF_BITS = 3;
const Bs8u PRIMARY_REF_NONE = 7;
const Bs8u SCALE_NUMERATOR = 8;
const Bs8u SUPERRES_SCALE_BITS = 3;
const Bs8u SUPERRES_SCALE_DENOMINATOR_MIN = SCALE_NUMERATOR + 1;

void AV1_BitStream::av1_setup_superres(FrameHeader& fh, SequenceHeader const& sh)
{
    fh.UpscaledWidth = fh.FrameWidth;
    fh.SuperresDenom = SCALE_NUMERATOR;

    if (!sh.enable_superres)
    {
        return;
    }

    if (f())
    {
        Bs32u denom = f(SUPERRES_SCALE_BITS);
        denom += SUPERRES_SCALE_DENOMINATOR_MIN;
        if (denom != SCALE_NUMERATOR)
            fh.FrameWidth = (fh.FrameWidth * SCALE_NUMERATOR + denom / 2) / (denom);

        fh.SuperresDenom = denom;
    }
}

inline Bs32u AlignPowerOfTwo(Bs32u value, Bs32u n)
{
    return (((value)+((1 << (n)) - 1)) & ~((1 << (n)) - 1));
}

const Bs8u MI_SIZE_LOG2 = 2;

inline void av1_set_mi_and_sb_size(FrameHeader& fh, SequenceHeader const& sh)
{
    // set frame width and heignt in MI
    const Bs32u alignedWidth = AlignPowerOfTwo(fh.FrameWidth, 3);
    const int alignedHeight = AlignPowerOfTwo(fh.FrameHeight, 3);
    fh.MiCols = alignedWidth >> MI_SIZE_LOG2;
    fh.MiRows = alignedHeight >> MI_SIZE_LOG2;

    // set frame width and height in SB
    const Bs32u mibSizeLog2 = sh.sbSize == BLOCK_64X64 ? 4 : 5;
    const Bs32u widthMI = AlignPowerOfTwo(fh.MiCols, mibSizeLog2);
    const Bs32u heightMI = AlignPowerOfTwo(fh.MiRows, mibSizeLog2);
    fh.sbCols = widthMI >> mibSizeLog2;
    fh.sbRows = heightMI >> mibSizeLog2;
}

void AV1_BitStream::av1_setup_render_size(FrameHeader& fh)
{
    fh.RenderWidth = fh.UpscaledWidth;
    fh.RenderHeight = fh.FrameHeight;

    if (f())
    {
        fh.RenderWidth = f(16) + 1;
        fh.RenderHeight = f(16) + 1;
    }
}

BSErr AV1_BitStream::av1_setup_frame_size(FrameHeader& fh, SequenceHeader const& sh)
{
    if (fh.frame_size_override_flag)
    {
        fh.FrameWidth = f(sh.frame_width_bits) + 1;
        fh.FrameHeight = f(sh.frame_height_bits) + 1;
        if (fh.FrameWidth > sh.max_frame_width || fh.FrameHeight > sh.max_frame_height)
            BS_TRACE(1, WARNING: Width/Height exceed MaxWidth/MaxHeight! );
    }
    else
    {
        fh.FrameWidth = sh.max_frame_width;
        fh.FrameHeight = sh.max_frame_height;
    }

    av1_setup_superres(fh, sh);
    av1_set_mi_and_sb_size(fh, sh);
    av1_setup_render_size(fh);

    return BS_ERR_NONE;
}

void AV1_BitStream::av1_setup_frame_size_with_refs(FrameHeader& fh, SequenceHeader const& sh)
{
    bool bFound = false;
    for (Bs8u i = 0; i < INTER_REFS; ++i)
    {
        if (f())
        {
            bFound = true;
            av1_setup_superres(fh, sh);
            break;
        }
    }

    if (!bFound)
    {
        // here AV1 spec 1.0 contradicts to reference implementation:
        // AV1 spec calls frame_size() which internally checks frame_size_override_flag before reading width/heignt from bs
        // reference implementation reads width/height from bs right away w/o preliminary check of frame_size_override_flag
        // current code mimics behavior of reference implementation
        fh.FrameWidth = f(sh.frame_width_bits) + 1;
        fh.FrameHeight = f(sh.frame_height_bits) + 1;
        av1_setup_superres(fh, sh);
        av1_setup_render_size(fh);
    }

    av1_set_mi_and_sb_size(fh, sh);
}

const Bs8u LOG2_SWITCHABLE_FILTERS = 2;

void AV1_BitStream::av1_read_interpolation_filter(FrameHeader& fh)
{
    if (f()) // is_filter_switchable
        fh.interpolation_filter = SWITCHABLE;
    else
        fh.interpolation_filter = static_cast<INTERP_FILTER>(f(LOG2_SWITCHABLE_FILTERS));
}

inline bool FrameMightUsePrevFrameMVs(FrameHeader const& fh, SequenceHeader const& sh)
{
    return !FrameIsResilient(fh) && sh.enable_order_hint && sh.enable_ref_frame_mvs;
}

struct TileLimits
{
    Bs32u maxTileWidthSB;
    Bs32u maxTileHeightSB;
    Bs32u maxTileAreaInSB;

    Bs32u maxlog2_tile_cols;
    Bs32u maxlog2_tile_rows;

    Bs32u minlog2_tile_cols;
    Bs32u minlog2_tile_rows;

    Bs32u minLog2Tiles;
};

const Bs32u MAX_TILE_WIDTH = 4096;        // Max Tile width in pixels
const Bs32u MAX_TILE_AREA = 4096 * 2304;  // Maximum tile area in pixels
#define MAX_TILE_ROWS 1024
#define MAX_TILE_COLS 1024

inline Bs32u av1_tile_log2(Bs32u blockSize, Bs32u target)
{
    Bs32u k;
    for (k = 0; (blockSize << k) < target; k++) {}
    return k;
}

inline void av1_get_tile_limits(FrameHeader const& fh, Bs32u sbSize, TileLimits& limits)
{
    const Bs32u mibSizeLog2 = sbSize == BLOCK_64X64 ? 4 : 5;
    const Bs32u sbSizeLog2 = mibSizeLog2 + MI_SIZE_LOG2;

    limits.maxTileWidthSB = MAX_TILE_WIDTH >> sbSizeLog2;
    limits.maxTileAreaInSB = MAX_TILE_AREA >> (2 * sbSizeLog2);

    limits.minlog2_tile_cols = av1_tile_log2(limits.maxTileWidthSB, fh.sbCols);
    limits.maxlog2_tile_cols = av1_tile_log2(1, std::min(fh.sbCols, (Bs32u)MAX_TILE_COLS));
    limits.maxlog2_tile_rows = av1_tile_log2(1, std::min(fh.sbRows, (Bs32u)MAX_TILE_ROWS));
    limits.minLog2Tiles = av1_tile_log2(limits.maxTileAreaInSB, fh.sbCols * fh.sbRows);
    limits.minLog2Tiles = std::max(limits.minLog2Tiles, limits.minlog2_tile_cols);
}

inline
Bs32u GetMostSignificantBit(Bs32u n)
{
    Bs32u log = 0;
    Bs32u value = n;

    for (int8_t i = 4; i >= 0; --i)
    {
        const Bs32u shift = (1u << i);
        const Bs32u x = value >> shift;
        if (x != 0)
        {
            value = x;
            log += shift;
        }
    }

    return log;
}

inline
Bs32u GetUnsignedBits(Bs32u numValues)
{
    return
        numValues > 0 ? GetMostSignificantBit(numValues) + 1 : 0;
}

Bs32s AV1_BitStream::read_uniform(Bs32u n)
{
    const Bs32u l = GetUnsignedBits(n);
    const Bs32u m = (1 << l) - n;
    const Bs32u v = f(l - 1);
    if (v < m)
        return v;
    else
        return (v << 1) - m + f(1);
}

static void av1_calculate_tile_cols(TileInfo& info, TileLimits& limits, FrameHeader const& fh)
{
    Bs32u i;

    if (info.uniform_tile_spacing_flag)
    {
        Bs32u startSB;
        Bs32u sizeSB = AlignPowerOfTwo(fh.sbCols, info.TileColsLog2);
        sizeSB >>= info.TileColsLog2;
        //VM_ASSERT(sizeSB > 0);
        for (i = 0, startSB = 0; startSB < fh.sbCols; i++)
        {
            info.SbColStarts[i] = startSB;
            startSB += sizeSB;
        }
        info.TileCols = i;
        info.SbColStarts[i] = fh.sbCols;
        limits.minlog2_tile_rows = std::max(static_cast<Bs32s>(limits.minLog2Tiles - info.TileColsLog2), 0);
        limits.maxTileHeightSB = fh.sbRows >> limits.minlog2_tile_rows;
    }
    else
    {
        limits.maxTileAreaInSB = (fh.sbRows * fh.sbCols);
        Bs32u widestTileSB = 1;
        info.TileColsLog2 = av1_tile_log2(1, info.TileCols);
        for (i = 0; i < info.TileCols; i++)
        {
            Bs32u sizeSB = info.SbColStarts[i + 1] - info.SbColStarts[i];
            widestTileSB = std::max(widestTileSB, sizeSB);
        }
        if (limits.minLog2Tiles)
            limits.maxTileAreaInSB >>= (limits.minLog2Tiles + 1);
        limits.maxTileHeightSB = std::max(limits.maxTileAreaInSB / widestTileSB, 1u);
    }
}

static void av1_calculate_tile_rows(TileInfo& info, FrameHeader const& fh)
{
    Bs32u startSB;
    Bs32u sizeSB;
    Bs32u i;

    if (info.uniform_tile_spacing_flag)
    {
        sizeSB = AlignPowerOfTwo(fh.sbRows, info.TileRowsLog2);
        sizeSB >>= info.TileRowsLog2;
        //VM_ASSERT(sizeSB > 0);
        for (i = 0, startSB = 0; startSB < fh.sbRows; i++)
        {
            info.SbRowStarts[i] = startSB;
            startSB += sizeSB;
        }
        info.TileRows = i;
        info.SbRowStarts[i] = fh.sbRows;
    }
    else
        info.TileRowsLog2 = av1_tile_log2(1, info.TileRows);
}

void AV1_BitStream::av1_read_tile_info_max_tile(TileInfo& info, FrameHeader const& fh, Bs32u sbSize)
{
    TileLimits limits = {};
    av1_get_tile_limits(fh, sbSize, limits);

    info.uniform_tile_spacing_flag = f();

    // Read tile columns
    if (info.uniform_tile_spacing_flag)
    {
        info.TileColsLog2 = limits.minlog2_tile_cols;
        while (info.TileColsLog2 < limits.maxlog2_tile_cols)
        {
            if (!f())
                break;
            info.TileColsLog2++;
        }
    }
    else
    {
        Bs32u i;
        Bs32u startSB;
        Bs32u sbCols = fh.sbCols;
        for (i = 0, startSB = 0; sbCols > 0 && i < MAX_TILE_COLS; i++)
        {
            const Bs32u sizeInSB =
                1 + read_uniform(std::min(sbCols, limits.maxTileWidthSB));
            info.SbColStarts[i] = startSB;
            startSB += sizeInSB;
            sbCols -= sizeInSB;
        }
        info.TileCols = i;
        info.SbColStarts[i] = startSB + sbCols;
    }
    av1_calculate_tile_cols(info, limits, fh);

    // Read tile rows
    if (info.uniform_tile_spacing_flag)
    {
        info.TileRowsLog2 = limits.minlog2_tile_rows;
        while (info.TileRowsLog2 < limits.maxlog2_tile_rows)
        {
            if (!f())
                break;
            info.TileRowsLog2++;
        }
    }
    else
    {
        Bs32u i;
        Bs32u startSB;
        Bs32u sbRows = fh.sbRows;
        for (i = 0, startSB = 0; sbRows > 0 && i < MAX_TILE_ROWS; i++)
        {
            const Bs32u sizeSB =
                1 + read_uniform(std::min(sbRows, limits.maxTileHeightSB));
            info.SbRowStarts[i] = startSB;
            startSB += sizeSB;
            sbRows -= sizeSB;
        }
        info.TileRows = i;
        info.SbRowStarts[i] = startSB + sbRows;
    }
    av1_calculate_tile_rows(info, fh);
}

inline Bs32u NumTiles(TileInfo const & info)
{
    return info.TileCols * info.TileRows;
}

void AV1_BitStream::av1_read_tile_info(TileInfo& info, FrameHeader const& fh, Bs32u sbSize)
{
    const bool large_scale_tile = 0; // this parameter isn't taken from the bitstream. Looks like decoder gets it from outside (e.g. container or some environment).
    if (large_scale_tile)
    {
        return;
    }
    av1_read_tile_info_max_tile(info, fh, sbSize);

    if (info.TileColsLog2 || info.TileRowsLog2)
        info.context_update_tile_id = f(info.TileColsLog2 + info.TileRowsLog2);


    if (NumTiles(info) > 1)
        info.TileSizeBytes = f(2) + 1;
}

inline Bs8u GetNumPlanes(SequenceHeader const& sh)
{
    return sh.color_config.mono_chrome ? 1 : MAX_MB_PLANE;
}

const Bs8u QINDEX_BITS = 8;

int AV1_BitStream::read_inv_signed_literal(int bits) {
    const unsigned sign = f();
    const unsigned literal = f(bits);
    if (sign == 0)
        return literal; // if positive: literal
    else
        return literal - (1 << bits); // if negative: complement to literal with respect to 2^bits
}

Bs32s AV1_BitStream::av1_read_q_delta()
{
    return (f()) ?
        read_inv_signed_literal(6) : 0;
}

const Bs8u QM_LEVEL_BITS = 4;

void AV1_BitStream::av1_setup_quantization(QuantizationParams& params, SequenceHeader const& sh, int numPlanes)
{
    params.base_q_idx = f(QINDEX_BITS);

    params.DeltaQYDc = av1_read_q_delta();

    if (numPlanes > 1)
    {
        Bs32s diffUVDelta = 0;
        if (sh.color_config.separate_uv_delta_q)
            diffUVDelta = f();

        params.DeltaQUDc = av1_read_q_delta();
        params.DeltaQUAc = av1_read_q_delta();

        if (diffUVDelta)
        {
            params.DeltaQVDc = av1_read_q_delta();
            params.DeltaQVAc = av1_read_q_delta();
        }
        else
        {
            params.DeltaQVDc = params.DeltaQUDc;
            params.DeltaQVAc = params.DeltaQUAc;
        }
    }

    params.using_qmatrix = f();
    if (params.using_qmatrix) {
        params.qm_y = f(QM_LEVEL_BITS);
        params.qm_u = f(QM_LEVEL_BITS);

        if (!sh.color_config.separate_uv_delta_q)
            params.qm_v = params.qm_u;
        else
            params.qm_v = f(QM_LEVEL_BITS);
    }
}

inline void ClearAllSegFeatures(SegmentationParams & seg)
{
    std::fill_n(reinterpret_cast<Bs32s*>(seg.FeatureData),
        std::extent<decltype(seg.FeatureData), 0>::value * std::extent<decltype(seg.FeatureData), 1>::value, 0);
    std::fill_n(seg.FeatureMask, std::extent<decltype(seg.FeatureMask)>::value, 0);
}

const Bs8u VP9_MAX_NUM_OF_SEGMENTS = 8;

inline void EnableSegFeature(SegmentationParams & seg, Bs8u segmentId, SEG_LVL_FEATURES featureId)
{
    seg.FeatureMask[segmentId] |= 1 << featureId;
}

const Bs8u MAXQ = 255;
const Bs8u MAX_LOOP_FILTER = 63;
const Bs8u MAX_REF_IDX_FOR_SEGMENT = 7;

const Bs8u SEG_FEATURE_DATA_SIGNED[SEG_LVL_MAX] = { 1, 1, 1, 1, 1, 0, 0 };
const Bs8u SEG_FEATURE_DATA_MAX[SEG_LVL_MAX] = { MAXQ,
                                                  MAX_LOOP_FILTER, MAX_LOOP_FILTER, MAX_LOOP_FILTER, MAX_LOOP_FILTER,
                                                  MAX_REF_IDX_FOR_SEGMENT,
                                                  0 };

Bs8u IsSegFeatureSigned(SEG_LVL_FEATURES featureId)
{
    return
        SEG_FEATURE_DATA_SIGNED[featureId];
}

template<class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi)
{
    return std::min(hi, std::max(v, lo));
}

void SetSegData(SegmentationParams & seg, Bs8u segmentId, SEG_LVL_FEATURES featureId, Bs32s seg_data)
{
    seg.FeatureData[segmentId][featureId] = seg_data;
}

bool AV1_BitStream::av1_setup_segmentation(SegmentationParams& params, FrameHeader const& fh)
{
    bool segmentQuantizerActive = false;
    params.segmentation_update_map = 0;
    params.segmentation_update_data = 0;

    params.segmentation_enabled = f();
    if (params.segmentation_enabled)
    {
        if (fh.primary_ref_frame == PRIMARY_REF_NONE)
        {
            params.segmentation_update_map = 1;
            params.segmentation_temporal_update = 0;
            params.segmentation_update_data = 1;
        }
        else
        {
            params.segmentation_update_map = f();
            if (params.segmentation_update_map)
                params.segmentation_temporal_update = f();
            params.segmentation_update_data = f();
        }

        if (params.segmentation_update_data)
        {
            ClearAllSegFeatures(params);

            for (Bs8u i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; ++i)
            {
                for (Bs8u j = 0; j < SEG_LVL_MAX; ++j)
                {
                    Bs32s data = 0;
                    if (f()) // feature_enabled
                    {
                        if (j == SEG_LVL_ALT_Q)
                            segmentQuantizerActive = true;
                        EnableSegFeature(params, i, (SEG_LVL_FEATURES)j);

                        const Bs32u nBits = GetUnsignedBits(SEG_FEATURE_DATA_MAX[j]);
                        const Bs32u isSigned = IsSegFeatureSigned((SEG_LVL_FEATURES)j);
                        const Bs32s dataMax = SEG_FEATURE_DATA_MAX[j];
                        const Bs32s dataMin = -dataMax;

                        if (isSigned)
                            data = read_inv_signed_literal(nBits);
                        else
                            data = f(nBits);

                        data = clamp(data, dataMin, dataMax);
                    }

                    SetSegData(params, i, (SEG_LVL_FEATURES)j, data);
                }
            }
        }
    }
    else
        ClearAllSegFeatures(params);

    return segmentQuantizerActive;
}

void AV1_BitStream::av1_read_delta_q_params(FrameHeader& fh)
{
    // here AV1 spec 1.0 contradicts to reference implementation:
    // spec sets fh.delta_q_res to 0 and reference sets it to 1
    // current code mimics behavior of reference implementation
    fh.delta_q_res = 1;
    if (fh.quantization_params.base_q_idx > 0)
        fh.delta_q_present = f();

    if (fh.delta_q_present)
        fh.delta_q_res = 1 << f(2);
}

void AV1_BitStream::av1_read_delta_lf_params(FrameHeader& fh)
{
    // here AV1 spec 1.0 contradicts to reference implementation:
    // spec sets fh.delta_lf_res to 0 and reference sets it to 1
    // current code mimics behavior of reference implementation
    fh.delta_lf_res = 1;
    if (fh.delta_q_present)
    {
        if (!fh.allow_intrabc)
            fh.delta_lf_present = f();

        if (fh.delta_lf_present)
        {
            fh.delta_lf_res = 1 << f(2);
            fh.delta_lf_multi = f();
        }
    }
}

inline bool IsSegFeatureActive(SegmentationParams const& seg, Bs8u segmentId, SEG_LVL_FEATURES featureId)
{
    return
        seg.segmentation_enabled && (seg.FeatureMask[segmentId] & (1 << featureId));
}

inline Bs32s GetSegData(SegmentationParams const& seg, Bs8u segmentId, SEG_LVL_FEATURES featureId)
{
    return
        seg.FeatureData[segmentId][featureId];
}

inline Bs32u av1_get_qindex(FrameHeader const& fh, Bs8u segmentId)
{
    if (IsSegFeatureActive(fh.segmentation_params, segmentId, SEG_LVL_ALT_Q))
    {
        const Bs32s segQIndex = fh.quantization_params.base_q_idx +
            GetSegData(fh.segmentation_params, segmentId, SEG_LVL_ALT_Q);;
        return clamp(segQIndex, 0, static_cast<Bs32s>(MAXQ));
    }
    else
        return fh.quantization_params.base_q_idx;
}

int IsCodedLossless(FrameHeader const& fh)
{
    int CodedLossless = 1;

    for (Bs8u i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; ++i)
    {
        const Bs32u qindex = av1_get_qindex(fh, i);

        if (qindex || fh.quantization_params.DeltaQYDc ||
            fh.quantization_params.DeltaQUAc || fh.quantization_params.DeltaQUDc ||
            fh.quantization_params.DeltaQVAc || fh.quantization_params.DeltaQVDc)
        {
            CodedLossless = 0;
            break;
        }
    }

    return CodedLossless;
}

void AV1_BitStream::av1_read_lr_params(LRParams& params, SequenceHeader const& sh, FrameHeader const& fh)
{
    if (fh.AllLossless || fh.allow_intrabc || !sh.enable_restoration)
    {
        return;
    }

    bool usesLR = false;
    bool usesChromaLR = false;

    for (Bs8u p = 0; p < fh.NumPlanes; ++p)
    {
        if (f()) {
            params.lr_type[p] =
                f() ? RESTORE_SGRPROJ : RESTORE_WIENER;
        }
        else
        {
            params.lr_type[p] =
                f() ? RESTORE_SWITCHABLE : RESTORE_NONE;
        }
        if (params.lr_type[p] != RESTORE_NONE)
        {
            usesLR = true;
            if (p != 0)
                usesChromaLR = true;
        }
    }

    if (usesLR)
    {
        if (sh.sbSize == BLOCK_128X128)
            params.lr_unit_shift = f() + 1;
        else
        {
            params.lr_unit_shift = f();
            if (params.lr_unit_shift)
                params.lr_unit_shift += f(); // lr_unit_extra_shift
        }

        if (sh.color_config.subsampling_x && sh.color_config.subsampling_y && usesChromaLR)
            params.lr_uv_shift = f();
    }
}

void AV1_BitStream::av1_read_cdef(CdefParams& params, SequenceHeader const& sh, FrameHeader const& fh)
{
    if (fh.CodedLossless || fh.allow_intrabc || !sh.enable_cdef)
    {
        params.cdef_damping = 3;
        std::fill_n(params.cdef_y_strength, std::extent<decltype(params.cdef_y_strength)>::value, 0);
        std::fill_n(params.cdef_uv_strength, std::extent<decltype(params.cdef_uv_strength)>::value, 0);

        return;
    }

    params.cdef_damping = f(2) + 3;
    params.cdef_bits = f(2);
    Bs32u numStrengths = 1 << params.cdef_bits;
    for (Bs8u i = 0; i < numStrengths; i++)
    {
        params.cdef_y_pri_strength[i] = f(4);
        params.cdef_y_sec_strength[i] = f(2);
        if (fh.NumPlanes > 1)
        {
            params.cdef_uv_pri_strength[i] = f(4);
            params.cdef_uv_sec_strength[i] = f(2);
        }
    }
}

void AV1_BitStream::av1_setup_loop_filter(LoopFilterParams& params, FrameHeader const& fh)
{
    if (fh.CodedLossless || fh.allow_intrabc)
    {
        return;
    }

    params.loop_filter_level[0] = f(6);
    params.loop_filter_level[1] = f(6);

    if (fh.NumPlanes > 1)
    {
        if (params.loop_filter_level[0] || params.loop_filter_level[1])
        {
            params.loop_filter_level[2] = f(6);
            params.loop_filter_level[3] = f(6);
        }
    }

    params.loop_filter_sharpness = f(3);

    params.loop_filter_delta_update = 0;

    params.loop_filter_delta_enabled = (Bs8u)f();
    if (params.loop_filter_delta_enabled)
    {
        params.loop_filter_delta_update = (Bs8u)f();

        if (params.loop_filter_delta_update)
        {
            Bs8u bits = 6;
            for (Bs32u i = 0; i < TOTAL_REFS; i++)
            {
                if (f())
                {
                    const Bs8u nbits = sizeof(unsigned) * 8 - bits - 1;
                    const Bs32u value = (unsigned)f(bits + 1) << nbits;
                    params.loop_filter_ref_deltas[i] = (int8_t)(((Bs32s)value) >> nbits);
                }
            }

            for (Bs32u i = 0; i < MAX_MODE_LF_DELTAS; i++)
            {
                if (f())
                {
                    const Bs8u nbits = sizeof(unsigned) * 8 - bits - 1;
                    const Bs32u value = (unsigned)f(bits + 1) << nbits;
                    params.loop_filter_mode_deltas[i] = (int8_t)(((Bs32s)value) >> nbits);
                }
            }
        }
    }
}

void AV1_BitStream::av1_read_tx_mode(FrameHeader& fh, bool allLosless)
{
    if (allLosless)
        fh.TxMode = ONLY_4X4;
    else
        fh.TxMode = f() ? TX_MODE_SELECT : TX_MODE_LARGEST;
}

void AV1_BitStream::av1_read_frame_reference_mode(FrameHeader& fh)
{
    fh.reference_mode = SINGLE_REFERENCE;

    if (!FrameIsIntra(fh))
    {
        fh.reference_mode = f() ? REFERENCE_MODE_SELECT :
            SINGLE_REFERENCE;
    }
}

BSErr AV1_BitStream::frame_header(Frame& frame)
{
    SequenceHeader const& sh = frame.sh;
    FrameHeader& fh = frame.fh;

    if (sh.reduced_still_picture_header)
    {
        fh.frame_type = KEY_FRAME;
        fh.show_frame = 1;
    }
    else
    {
        fh.show_existing_frame = f();
        if (fh.show_existing_frame)
        {
            fh.frame_to_show_map_idx = f(3);
            av1_read_timing_point_info(sh, fh);

            if (sh.frame_id_numbers_present_flag)
            {
                fh.display_frame_id = f(sh.idLen);

                fh.current_frame_id = fh.display_frame_id;
            }

            // get frame resolution
            fh.FrameWidth = 0;
            fh.FrameHeight = 0;

            fh.show_frame = 1;

            return BS_ERR_NONE;
        }

        fh.frame_type = static_cast<FRAME_TYPE>(f(2));
        fh.show_frame = f();

        if (fh.show_frame)
        {
            av1_read_timing_point_info(sh, fh);
            fh.showable_frame = (fh.frame_type != KEY_FRAME);
        }
        else
            fh.showable_frame = f();

        if (fh.frame_type == SWITCH_FRAME ||
            (fh.frame_type == KEY_FRAME && fh.show_frame))
            fh.error_resilient_mode = 1;
        else
            fh.error_resilient_mode = f();
    }

    fh.disable_cdf_update = f();
    if (sh.seq_force_screen_content_tools == 2)
        fh.allow_screen_content_tools = f();
    else
        fh.allow_screen_content_tools = sh.seq_force_screen_content_tools;

    if (fh.allow_screen_content_tools)
    {
        if (sh.seq_force_integer_mv == 2)
            fh.force_integer_mv = f();
        else
            fh.force_integer_mv = sh.seq_force_integer_mv;
    }
    else
        fh.force_integer_mv = 0;

    if (FrameIsIntra(fh))
        fh.force_integer_mv = 1;

    if (sh.frame_id_numbers_present_flag)
    {
        fh.current_frame_id = f(sh.idLen);

    }

    if (fh.frame_type == SWITCH_FRAME)
        fh.frame_size_override_flag = 1;
    else if (sh.reduced_still_picture_header)
        fh.frame_size_override_flag = 0;
    else
        fh.frame_size_override_flag = f(1);

    fh.order_hint = f(sh.order_hint_bits_minus1 + 1);
    const Bs8u allFrames = (1 << NUM_REF_FRAMES) - 1;

    if (FrameIsResilient(fh))
        fh.primary_ref_frame = PRIMARY_REF_NONE;
    else
        fh.primary_ref_frame = f(PRIMARY_REF_BITS);

    if (sh.decoder_model_info_present_flag)
    {
        const int buffer_removal_time_present_flag = f();
        if (buffer_removal_time_present_flag)
        {
            for (Bs32u opNum = 0; opNum <= sh.operating_points_cnt_minus_1; opNum++)
            {
                if (sh.decoder_model_present_for_this_op[opNum])
                {
                    const int opPtIdc = sh.operating_point_idc[opNum];
                    const int inTemporalLayer = (opPtIdc >> frame.obu_hdr_f.temporal_id) & 1;
                    const int inSpatialLayer = (opPtIdc >> (frame.obu_hdr_f.spatial_id + 8)) & 1;
                    if (opPtIdc == 0 || (inTemporalLayer && inSpatialLayer))
                    {
                        const int n = sh.decoder_model_info.buffer_removal_time_length_minus_1 + 1;
                        f(n); // buffer_removal_time
                    }
                }
            }
        }
    }

    if (fh.frame_type == SWITCH_FRAME ||
        (fh.frame_type == KEY_FRAME && fh.show_frame))
        fh.refresh_frame_flags = allFrames;
    else
        fh.refresh_frame_flags = static_cast<Bs8u>(f(NUM_REF_FRAMES));

    if (!FrameIsIntra(fh) || fh.refresh_frame_flags != allFrames)
    {
        if (fh.error_resilient_mode && sh.enable_order_hint)
        {
            for (int i = 0; i < NUM_REF_FRAMES; i++)
            {
                const int OrderHintBits = sh.order_hint_bits_minus1 + 1;
                fh.ref_order_hint[i] = f(OrderHintBits);
            }
        }
    }

    if (KEY_FRAME == fh.frame_type)
    {
        for (Bs8u i = 0; i < INTER_REFS; ++i)
        {
            fh.ref_frame_idx[i] = -1;
        }

        av1_setup_frame_size(fh, sh);

        if (fh.allow_screen_content_tools && fh.FrameWidth == fh.UpscaledWidth)
            fh.allow_intrabc = f();
    }
    else
    {
        if (FrameIsIntraOnly(fh))
        {
            av1_setup_frame_size(fh, sh);

            if (fh.allow_screen_content_tools && fh.FrameWidth == fh.UpscaledWidth)
                fh.allow_intrabc = f();
        }
        else
        {
            Bs32u frame_refs_short_signalling = 0;

            if (sh.enable_order_hint)
                frame_refs_short_signalling = f();

            if (frame_refs_short_signalling)
            {
                const Bs32u last_frame_idx = f(REF_FRAMES_LOG2);
                const Bs32u gold_frame_idx = f(REF_FRAMES_LOG2);

                // set last and gold reference frames
                fh.ref_frame_idx[LAST_FRAME - LAST_FRAME] = last_frame_idx;
                fh.ref_frame_idx[GOLDEN_FRAME - LAST_FRAME] = gold_frame_idx;
            }

            for (Bs8u i = 0; i < INTER_REFS; ++i)
            {
                if (!frame_refs_short_signalling)
                    fh.ref_frame_idx[i] = f(REF_FRAMES_LOG2);
            }

            if (fh.error_resilient_mode == 0 && fh.frame_size_override_flag)
                av1_setup_frame_size_with_refs(fh, sh);
            else
                av1_setup_frame_size(fh, sh);

            if (fh.force_integer_mv)
                fh.allow_high_precision_mv = 0;
            else
                fh.allow_high_precision_mv = f();

            av1_read_interpolation_filter(fh);
            fh.is_motion_mode_switchable = f();

            if (FrameMightUsePrevFrameMVs(fh, sh))
            {
                fh.use_ref_frame_mvs = f();
            }
        }
    }

    if (!fh.FrameWidth || !fh.FrameHeight)
    {
        BS_TRACE(1, Width or Height not found in FH (stream corrupted?));
        return BS_ERR_INVALID_PARAMS;
    }

    const int mightBwdAdapt = !sh.reduced_still_picture_header && !fh.disable_cdf_update;

    if (mightBwdAdapt)
        fh.disable_frame_end_update_cdf = f();
    else
        fh.disable_frame_end_update_cdf = 1;

    av1_read_tile_info(fh.tile_info, fh, sh.sbSize);

    fh.NumPlanes = GetNumPlanes(sh);

    av1_setup_quantization(fh.quantization_params, sh, fh.NumPlanes);

    av1_setup_segmentation(fh.segmentation_params, fh);

    av1_read_delta_q_params(fh);

    av1_read_delta_lf_params(fh);

    fh.CodedLossless = IsCodedLossless(fh);
    fh.AllLossless = fh.CodedLossless && (fh.FrameWidth == fh.UpscaledWidth);

    av1_setup_loop_filter(fh.loop_filter_params, fh);

    av1_read_cdef(fh.cdef_params, sh, fh);

    av1_read_lr_params(fh.lr_params, sh, fh);

    av1_read_tx_mode(fh, fh.CodedLossless);

    av1_read_frame_reference_mode(fh);

    if (!FrameIsResilient(fh) && sh.enable_warped_motion)
        fh.allow_warped_motion = f();

    fh.reduced_tx_set = f();

    if (!FrameIsIntra(fh))
    {
        TRANSFORMATION_TYPE type;
        int frame;
        for (frame = LAST_FRAME; frame <= ALTREF_FRAME; ++frame)
        {
            type = static_cast<TRANSFORMATION_TYPE>(f());
            if (type != IDENTITY) {
                BS_TRACE(1, Global Motion not implemented in Parser);
                return BS_ERR_NOT_IMPLEMENTED;
            }
        }
    }

    if (fh.show_frame || fh.showable_frame)
    {
        if (sh.film_grain_param_present)
        {
            bool apply_film_grain = f();
            if (apply_film_grain) {
                BS_TRACE(1, Film Grain not implemented in Parser);
                return BS_ERR_NOT_IMPLEMENTED;
            }
        }
    }

    // trailing bits
    if (f(1) != 1) //trailing_one_bit
    {
        BS_TRACE(1, Invalid trailing bit (stream corrupted?));
        return BS_ERR_INVALID_PARAMS;
    }
    while (bit & 7)
    {
        if (f(1) != 0) //trailing_zero_bit
        {
            BS_TRACE(1, Invalid trailing bit (stream corrupted?));
            return BS_ERR_INVALID_PARAMS;
        }
    }

    return BS_ERR_NONE;
}

// Parsing according to AV1 SPEC "1.0.0 with Errata 1"
BSErr AV1_BitStream::uncompressed_header(Frame& frame)
{
    OBUHeader obu_hdr;

    while (1)
    {
        memset(&obu_hdr, 0, sizeof(OBUHeader));
        obu_header(frame, obu_hdr);
        if (obu_hdr.obu_type == OBU_SEQUENCE_HEADER)
        {
            memcpy(&frame.obu_hdr_s, &obu_hdr, sizeof(OBUHeader));
            last_err = sequence_header(frame);
            if (last_err != BS_ERR_NONE)
            {
                return last_err;
            }
            if (!last_sh)
            {
                last_sh = new SequenceHeader;
            }
            // store Seq header to use it later for parsing Frame headers
            memcpy(last_sh, &frame.sh, sizeof(SequenceHeader));
        }
        else if (obu_hdr.obu_type == OBU_FRAME_HEADER)
        {
            if (!last_sh)
            {
                BS_TRACE(1, No Seq header detected to parse Frame header! );
                last_err = BS_ERR_INVALID_PARAMS;
                return last_err;
            }
            else
            {
                memcpy(&frame.sh, last_sh, sizeof(SequenceHeader));
            }
            memcpy(&frame.obu_hdr_f, &obu_hdr, sizeof(OBUHeader));
            last_err = frame_header(frame);
            if (last_err != BS_ERR_NONE)
            {
                return last_err;
            }
            else
            {
                // Frame header is the last step in parsing now,
                //  so exit with success
                return BS_ERR_NONE;
            }
        }
        else if (obu_hdr.obu_type < OBU_SEQUENCE_HEADER)
        {
            BS_TRACE(1, Invalid OBU header (obu_type = 0)!);
            last_err = BS_ERR_INVALID_PARAMS;
            return last_err;
        }
        else
        {
            // just skip all valid OBU frames until
            //  parser for this OBU type is implemented
            break;
        }
    }

    return last_err;
}
