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

using namespace BS_AV1;

BSErr AV1_BitStream::parse_ivf_header(){
    BS_TRACE(1, ===================== IVF HEADER ====================);
    BS_SET( get_4_bytes_inv(), hdr.ivf.signature);
    if (hdr.ivf.signature != 0x46494b44)
    {
        offset -= 4;
        return BS_ERR_INVALID_PARAMS;
    }

    BS_SET( get_2_bytes_inv(), hdr.ivf.version);
    if (hdr.ivf.version != 0)
    {
        offset -= (4 + 2);
        return BS_ERR_INVALID_PARAMS;
    }

    BS_SET( get_2_bytes_inv(), hdr.ivf.length);
    BS_SET( get_4_bytes_inv(), hdr.ivf.codec_FourCC);
    BS_SET( get_2_bytes_inv(), hdr.ivf.width);
    BS_SET( get_2_bytes_inv(), hdr.ivf.height);
    BS_SET( get_4_bytes_inv(), hdr.ivf.frame_rate);
    BS_SET( get_4_bytes_inv(), hdr.ivf.time_scale);
    BS_SET( get_4_bytes_inv(), hdr.ivf.num_frames);
    BS_TRACE( get_4_bytes_inv(), unused);

    return last_err;
}

BSErr AV1_BitStream::parse_ivf_frame_header(){
    BS_TRACE(1, ================== IVF FRAME HEADER =================);
    BS_SET( get_4_bytes_inv(), hdr.ivf_frame.frame_size);
    BS_SET( get_4_bytes_inv(), hdr.ivf_frame.time_stamp_l);
    BS_SET( get_4_bytes_inv(), hdr.ivf_frame.time_stamp_h);

    return last_err;
}

BSErr AV1_BitStream::parse_next_unit()
{
    last_err = BS_ERR_NONE;

    BS_FADDR_TYPE frame_end = 0;
    Frame* pf = NewFrame();

    if (!pf)
        return last_err = BS_ERR_MEM_ALLOC;

    Frame& f = *pf;

    if(IVF == container){
        if(m_FirstCall){
            m_FirstCall = 0;

            BSErr ivf_header_error = parse_ivf_header();
            if (ivf_header_error == BS_ERR_INVALID_PARAMS)
            {
                // Keep parsing on failed IVF header for cases when it is ommited (i.e. func-reset test's substreams)
                BS_TRACE(1, WARNING: IVF-header incorrect or not found (continue parsing...));
                memset(&hdr.ivf, 0, sizeof(ivf_header));
            }
        }
        if (parse_ivf_frame_header())
        {
            memset(&hdr.ivf_frame, 0, sizeof(ivf_frame_header));
            return last_err;
        }

        frame_end = file_pos + offset + hdr.ivf_frame.frame_size;
    }
    else
    {
        memset(&hdr.ivf, 0, sizeof(ivf_header));
        memset(&hdr.ivf_frame, 0, sizeof(ivf_frame_header));
    }

    memcpy(&f.ivf, &hdr.ivf, sizeof(ivf_header));
    memcpy(&f.ivf_frame, &hdr.ivf_frame, sizeof(ivf_frame_header));

    trace_on(TRACE_LEVEL_UH);

    uncompressed_header(f);
    if (last_err && last_err != BS_ERR_INCOMPLETE_DATA)
    {
        return last_err;
    }

    if (last_err != BS_ERR_INCOMPLETE_DATA)
    {
        trace_on(TRACE_LEVEL_FRAME);
    }

    if (frame_end) {
        if ((file_pos + offset) > frame_end)
            return last_err = BS_ERR_UNKNOWN;
        shift_forward((Bs32u)(frame_end - file_pos - offset));

        if (last_err == BS_ERR_MORE_DATA)
        {
            // It is the last frame in the stream flag
            last_err = BS_ERR_NONE;
        }
    }

    return last_err;
}
