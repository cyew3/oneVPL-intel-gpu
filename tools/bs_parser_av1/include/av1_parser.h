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

#pragma once

#include <bs_reader.h>
#include <bs_splitter.h>
#include <av1_struct.h>
#include <av1_dpb.h>

#define AV1_PARSE_BLOCKS 0

using namespace BS_AV1;

class AV1_BitStream : public BS_Splitter::Reader, public BS_AV1::DPB
{
public:
    AV1_BitStream(Bs32u mode = 0)
        : m_splitter(0)
        //, m_bool_decoder(*this)
        , m_FirstCall(true)
        , container(BS_AV1::IVF)
    {
        last_err = set_container(mode & 0xFF);
    }

    virtual ~AV1_BitStream()
    {
        if (m_splitter)
            delete m_splitter;
    }

    inline Bs16u get_2_bytes_inv(){ Bs16u res = read_1_byte(); return (res|(read_1_byte()<<8));};
    inline Bs32u get_3_bytes_inv(){ Bs32u res = read_1_byte(); res |= (read_1_byte()<<8); return (res|(read_1_byte()<<16));};
    inline Bs32u get_4_bytes_inv(){ Bs32u res = read_1_byte(); res |= (read_1_byte()<<8); res |= (read_1_byte()<<16); return (res|(read_1_byte()<<24));};

    BSErr parse_ivf_frame_header();
    BSErr parse_ivf_header();

    BSErr parse_next_unit();

    BSErr set_container(Bs32u id)
    {
        if (m_splitter)
            delete m_splitter;

        m_splitter = BS_Splitter::NEW(0, *this);

        if (id == BS_AV1::ELEMENTARY_STREAM)
        {
            container = BS_AV1::ELEMENTARY_STREAM;
        }

        return m_splitter ? BS_ERR_NONE : last_err = BS_ERR_UNKNOWN;
    }
    inline BS_AV1::Frame* get_hdr() { return Ref(0); };

private:
    BS_Splitter::Interface* m_splitter;
    BS_AV1::header hdr;
    SequenceHeader *last_sh = nullptr;
    bool m_FirstCall;
    BS_AV1::CONTAINER container;

    inline Bs32u f() { return read_1_bit(); }
    inline Bs32u f(Bs32u n) { return read_bits(n); }


    BSErr obu_header(BS_AV1::Frame& frame, OBUHeader& header);
    BSErr sequence_header(Frame& frame);
    BSErr frame_header(Frame& frame);
    BSErr uncompressed_header(BS_AV1::Frame& f);
    BSErr av1_read_obu_size(size_t& size);
    void av1_timing_info(TimingInfo& info);
    Bs32u read_uvlc();
    void av1_decoder_model_info(DecoderModelInfo& info);
    void av1_operating_parameters_info(OperatingParametersInfo& info, uint32_t length);
    void av1_color_config(ColorConfig& config, uint32_t profile);
    void av1_read_timing_point_info(SequenceHeader const& sh, FrameHeader& fh);
    BSErr av1_setup_frame_size(FrameHeader& fh, SequenceHeader const& sh);
    void av1_setup_render_size(FrameHeader& fh);
    void av1_setup_frame_size_with_refs(FrameHeader& fh, SequenceHeader const& sh);
    void av1_setup_superres(FrameHeader& fh, SequenceHeader const& sh);
    void av1_read_interpolation_filter(FrameHeader& fh);
    void av1_read_tile_info(TileInfo& info, FrameHeader const& fh, uint32_t sbSize);
    void av1_read_tile_info_max_tile(TileInfo& info, FrameHeader const& fh, uint32_t sbSize);
    Bs32s read_uniform(uint32_t n);
    void av1_setup_quantization(QuantizationParams& params, SequenceHeader const& sh, int numPlanes);
    Bs32s av1_read_q_delta();
    int read_inv_signed_literal(int bits);
    bool av1_setup_segmentation(SegmentationParams& params, FrameHeader const& fh);
    void av1_read_delta_q_params(FrameHeader& fh);
    void av1_read_delta_lf_params(FrameHeader& fh);
    void av1_setup_loop_filter(LoopFilterParams& params, FrameHeader const& fh);
    void av1_read_cdef(CdefParams& params, SequenceHeader const& sh, FrameHeader const& fh);
    void av1_read_lr_params(LRParams& params, SequenceHeader const& sh, FrameHeader const& fh);
    void av1_read_tx_mode(FrameHeader& fh, bool allLosless);
    void av1_read_frame_reference_mode(FrameHeader& fh);
};
