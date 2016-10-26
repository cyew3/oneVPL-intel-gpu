//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#pragma once
#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#include <ippi.h>
#include <ippvc.h>
#include "umc_structures.h"
#include "umc_memory_allocator.h"
#include "umc_mpeg2_dec_bstream.h"

#include <vector>

#define DPB_SIZE 10

namespace UMC
{

//start/end codes
#define PICTURE_START_CODE       0x00000100
#define USER_DATA_START_CODE     0x000001B2
#define SEQUENCE_HEADER_CODE     0x000001B3
#define SEQUENCE_ERROR_CODE      0x000001B4
#define EXTENSION_START_CODE     0x000001B5
#define SEQUENCE_END_CODE        0x000001B7
#define GROUP_START_CODE         0x000001B8

#define SEQUENCE_EXTENSION_ID                  0x00000001
#define SEQUENCE_DISPLAY_EXTENSION_ID          0x00000002
#define QUANT_MATRIX_EXTENSION_ID              0x00000003
#define COPYRIGHT_EXTENSION_ID                 0x00000004
#define SEQUENCE_SCALABLE_EXTENSION_ID         0x00000005
#define PICTURE_DISPLAY_EXTENSION_ID           0x00000007
#define PICTURE_CODING_EXTENSION_ID            0x00000008
#define PICTURE_SPARTIAL_SCALABLE_EXTENSION_ID 0x00000009
#define PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID 0x0000000a

#define DATA_PARTITIONING        0x00000000
#define SPARTIAL_SCALABILITY     0x00000001
#define SNR_SCALABILITY          0x00000002
#define TEMPORAL_SCALABILITY     0x00000003



enum MPEG2FrameType
{
    MPEG2_I_PICTURE               = 1,
    MPEG2_P_PICTURE               = 2,
    MPEG2_B_PICTURE               = 3
};

#define FRAME_PICTURE            3
#define TOP_FIELD                1
#define BOTTOM_FIELD             2
///////////////////////

extern Ipp16s q_scale[2][32];
extern Ipp16s reset_dc[4];
extern Ipp16s intra_dc_multi[4];


struct sVideoFrameBuffer 
{
    typedef std::vector< std::pair<Ipp8u *,size_t> > UserDataVector;

    sVideoFrameBuffer();

    Ipp8u*           Y_comp_data;
    Ipp8u*           U_comp_data;
    Ipp8u*           V_comp_data;
    Ipp8u*           user_data;
    FrameType        frame_type; // 1-I, 2-P, 3-B
    Ipp64f           frame_time;
    bool             is_original_frame_time;
    Ipp64f           duration;
    Ipp32s           va_index;
    bool             IsUserDataDecoded;
    Ipp32s           us_buf_size;
    Ipp32s           us_data_size;
    Ipp32s           prev_index;//index for frame_buffer for prev reference;
    Ipp32s           next_index;//index for frame_buffer for next reference;
    bool             isCorrupted;
};

struct sGOPTimeCode
{
    Ipp16u           gop_seconds;
    Ipp16u           gop_minutes;
    Ipp16u           gop_hours;
    Ipp16u           gop_picture;   // starting picture in gop_second
    Ipp16u           gop_drop_frame_flag;
};

struct sSequenceHeader 
{
    Ipp32s           mb_width[2*DPB_SIZE]; //the number of macroblocks in the row of the picture
    Ipp32s           mb_height[2*DPB_SIZE];//the number of macroblocks in the column of the picture
    Ipp32s           numMB[2*DPB_SIZE];    //the number of macroblocks in the picture

//sequence extension
    Ipp32u           profile;
    Ipp32u           level;
    Ipp32u           extension_start_code_ID[2*DPB_SIZE];
    Ipp32u           scalable_mode[2*DPB_SIZE];
    Ipp32u           progressive_sequence;

    Ipp32s           frame_rate_extension_d;
    Ipp32s           frame_rate_extension_n;
    Ipp32u           frame_rate_code;
    Ipp32u           aspect_ratio_code;
    Ipp16u           aspect_ratio_w;
    Ipp16u           aspect_ratio_h;
    Ipp32u           chroma_format;
    Ipp32u           width;
    Ipp32u           height;
    Ipp64f           delta_frame_time;
    Ipp64f           stream_time;
    Ipp32s           stream_time_temporal_reference; // for current stream_time
    Ipp32s           first_p_occure;
    Ipp32s           first_i_occure;
    Ipp32s           b_curr_number;
    Ipp32s           is_decoded;

    // GOP info.
    Ipp32s           closed_gop;    // no ref to previous GOP
    Ipp32s           broken_link;   // ref to absent prev GOP
    // GOP time code
    sGOPTimeCode     time_code;

    Ipp32s           frame_count;   // number of decoded pictures from last sequence header
};

struct sSHSavedPars
{
    Ipp32s mb_width;
    Ipp32s mb_height;
    Ipp32s numMB;
    Ipp32u extension_start_code_ID;
    Ipp32u scalable_mode;
};

// for prediction (forward and backward) and current frame;
struct sFrameBuffer
{
    sFrameBuffer();

    sVideoFrameBuffer     frame_p_c_n[DPB_SIZE*2];    // previous, current and next frames
    Ipp8u                 *ptr_context_data; // pointer to allocated data
    MemID                 mid_context_data;  // pointer to allocated data
    Ipp32u                Y_comp_height;
    Ipp32u                Y_comp_pitch;
    Ipp32u                U_comp_pitch;
    Ipp32u                V_comp_pitch;
    Ipp32u                pic_size;
    Ipp32s                curr_index[2*DPB_SIZE]; // 0 initially
    Ipp32s                common_curr_index;// 0 initially
    Ipp32s                latest_prev; // 0 initially
    Ipp32s                latest_next; // 0 initially
    Ipp32s                retrieve;   // index of retrieved frame; -1 initially
    Ipp32s                field_buffer_index[2*DPB_SIZE];
    Ipp32s                allocated_mb_width;
    Ipp32s                allocated_mb_height;
    ColorFormat           allocated_cformat;
    Ipp32s                ret_array[2*DPB_SIZE];
    Ipp32s                ret_array_curr;
    Ipp32s                ret_index;
    Ipp32s                ret_array_free;
    Ipp32s                ret_array_len;
};


struct sPictureHeader
{
    MPEG2FrameType   picture_coding_type;
    Ipp32u           d_picture;

    Ipp32s           full_pel_forward_vector;
    Ipp32s           full_pel_backward_vector;

    //extensions
    Ipp32s           f_code[4];
    Ipp32s           r_size[4];
    Ipp32s           low_in_range[4];
    Ipp32s           high_in_range[4];
    Ipp32s           range[4];
    Ipp32u           picture_structure;
    Ipp32u           intra_dc_precision;
    Ipp32u           top_field_first;
    Ipp32u           frame_pred_frame_dct;
    Ipp32u           concealment_motion_vectors;
    Ipp32u           q_scale_type;
    Ipp32u           repeat_first_field;
    Ipp32u           progressive_frame;
    Ipp32s           temporal_reference;

    Ipp32s           curr_reset_dc;
    Ipp32s           intra_vlc_format;
    Ipp32s           alternate_scan;
    Ipp32s           curr_intra_dc_multi;
    Ipp32s           max_slice_vert_pos;

    sGOPTimeCode     time_code;
    bool             first_in_sequence;
};

struct  IppVideoContext
{
//Slice
    Ipp32s       slice_vertical_position;
    Ipp32u       m_bNewSlice;//l
    Ipp32s       cur_q_scale;

    Ipp32s       mb_row;
    Ipp32s       mb_col;

//Macroblock
    Ipp32u       macroblock_motion_forward;
    Ipp32u       macroblock_motion_backward;
    Ipp32s       prediction_type;

DECLALIGN(16) Ipp16s PMV[8];
DECLALIGN(16) Ipp16s vector[8];

    Ipp16s       dct_dc_past[3]; // y,u,v

    Ipp32s       mb_address_increment;//l
    Ipp32s       row_l, col_l, row_c, col_c;
    Ipp32s       offset_l, offset_c;

    Ipp8u        *blkCurrYUV[3];

//Block
DECLALIGN(16)
    IppiDecodeIntraSpec_MPEG2 decodeIntraSpec;
DECLALIGN(8)
    IppiDecodeInterSpec_MPEG2 decodeInterSpec;
DECLALIGN(8)
    IppiDecodeIntraSpec_MPEG2 decodeIntraSpecChroma;
DECLALIGN(8)
    IppiDecodeInterSpec_MPEG2 decodeInterSpecChroma;

//Bitstream
    Ipp8u*       bs_curr_ptr;
    Ipp32s       bs_bit_offset;
    Ipp8u*       bs_start_ptr;
    Ipp8u*       bs_end_ptr;
    Ipp8u*       bs_sequence_header_start;

    Ipp32s Y_comp_pitch;
    Ipp32s U_comp_pitch;
    Ipp32s V_comp_pitch;
    Ipp32s Y_comp_height;
    Ipp32u pic_size;

    Ipp32s blkOffsets[3][8];
    Ipp32s blkPitches[3][2];

    Ipp32s clip_info_width;
    Ipp32s clip_info_height;

    VideoStreamType stream_type;
    ColorFormat color_format;

};

struct DecodeSpec
{
//Block
DECLALIGN(16)
    IppiDecodeIntraSpec_MPEG2 decodeIntraSpec;
DECLALIGN(8)
    IppiDecodeInterSpec_MPEG2 decodeInterSpec;
DECLALIGN(8)
    IppiDecodeIntraSpec_MPEG2 decodeIntraSpecChroma;
DECLALIGN(8)
    IppiDecodeInterSpec_MPEG2 decodeInterSpecChroma;

Ipp32s      flag;
};

struct Mpeg2Bitstream
{
    Ipp8u* bs_curr_ptr;
    Ipp32s bs_bit_offset;
    Ipp8u* bs_start_ptr;
    Ipp8u* bs_end_ptr;
};

enum { FCodeFwdX = 0, FCodeFwdY = 1, FCodeBwdX = 2, FCodeBwdY = 3 };

} // namespace UMC

#pragma warning(default: 4324)

extern Ipp16s zero_memory[64*8];

/*******************************************************/

#define RESET_PMV(array) {                                      \
  for(unsigned int nn=0; nn<sizeof(array)/sizeof(Ipp32s); nn++) \
    ((Ipp32s*)array)[nn] = 0;                                   \
}

#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
