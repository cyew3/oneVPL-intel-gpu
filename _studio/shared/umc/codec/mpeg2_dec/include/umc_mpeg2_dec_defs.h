/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2016 Intel Corporation. All Rights Reserved.
//
*/

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

#define IPPVC_MC_FRAME     0x2
#define IPPVC_MC_FIELD     0x1
#define IPPVC_MC_16X8      0x2
#define IPPVC_MC_DP        0x3

#define IPPVC_DCT_FIELD    0x0
#define IPPVC_DCT_FRAME    0x1

#define IPPVC_MB_INTRA     0x1
#define IPPVC_MB_PATTERN   0x2
#define IPPVC_MB_BACKWARD  0x4
#define IPPVC_MB_FORWARD   0x8
#define IPPVC_MB_QUANT     0x10

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

#define ROW_CHROMA_SHIFT_420 3
#define ROW_CHROMA_SHIFT_422 4
#define COPY_CHROMA_MB_420 ippiCopy8x8_8u_C1R
#define COPY_CHROMA_MB_422 ippiCopy8x16_8u_C1R

extern Ipp16s q_scale[2][32];
///////////////////////

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
    UserDataVector   user_data_v;
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
    Ipp32s           num_of_skipped;
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

struct mp2_VLCTable
{
  Ipp32s max_bits;
  Ipp32s bits_table0;
  Ipp32s bits_table1;
  Ipp32u threshold_table0;
  Ipp16s *table0;
  Ipp16s *table1;
};

struct vlcStorageMPEG2
{
    Ipp32s *ippTableB5a;
    Ipp32s *ippTableB5b;
};

struct sliceInfo
{
  Ipp8u  *startPtr;
  Ipp8u  *endPtr;
  Ipp32s flag;
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

struct Mpeg2SeqHead
{
    Ipp32u bitrate;
    Ipp32u vbv_buffer_size;
    Ipp16u horizontal_size;
    Ipp16u vertical_size;
    Ipp8u aspect_ratio_information;
    Ipp8u frame_rate_code;
    Ipp8u constrained_parameters_flag;
    Ipp8u load_iqm;
    Ipp8u iqm[64];
    Ipp8u load_niqm;
    Ipp8u niqm[64];

    Ipp8u seqHeadExtPresent;
    Ipp8u profile;
    Ipp8u level;
    Ipp8u progressive_sequence;
    Ipp8u chroma_format;
    Ipp8u frame_rate_extension_n;
    Ipp8u frame_rate_extension_d;

    Ipp8u seqScaleExtPresent;
    Ipp8u scalable_mode;
};

struct Mpeg2GroupHead
{
    Ipp32s gop_second;
    Ipp32s gop_picture;
    Ipp8u closed_gop;
    Ipp8u broken_link;
};

struct Mpeg2MvCtx
{
    Mpeg2MvCtx() {}
    void Update(const Ipp8u* fCode)
    {
        for (Ipp32u i = 0; i < 4; i++)
        {
            rSize[i] = fCode[i] - 1;
            Ipp32s f = 1 << rSize[i];
            low[i] = -(f * 16);
            high[i] = (f * 16) - 1;
            range[i] = f * 32;
        }
    }

    Ipp32s rSize[4];
    Ipp32s low[4];
    Ipp32s high[4];
    Ipp32s range[4];
};

enum { FCodeFwdX = 0, FCodeFwdY = 1, FCodeBwdX = 2, FCodeBwdY = 3 };
struct Mpeg2PicHead
{
    Ipp16u temporal_reference;
    Ipp8u full_pel_forward_vector;
    Ipp8u full_pel_backward_vector;
    Ipp8u picture_coding_type;

    Ipp8u picHeadExtPresent;
    Ipp8u f_code[4];
    Ipp8u repeat_first_field;
    Ipp8u intra_dc_precision;
    Ipp8u picture_structure;
    Ipp8u top_field_first;
    Ipp8u frame_pred_frame_dct;
    Ipp8u concealment_motion_vectors;
    Ipp8u q_scale_type;
    Ipp8u intra_vlc_format;
    Ipp8u alternate_scan;
    Ipp8u progressive_frame;

    Ipp8u picHeadQuantPresent;
    Ipp8u load_iqm;
    Ipp8u iqm[64];
    Ipp8u load_niqm;
    Ipp8u niqm[64];
    Ipp8u load_ciqm;
    Ipp8u ciqm[64];
    Ipp8u load_cniqm;
    Ipp8u cniqm[64];

    void Reset() { memset(this, 0, sizeof(Mpeg2PicHead)); }
    void UpdateMvCtx() { m_mvCtx.Update(f_code); }
    const Mpeg2MvCtx& MvCtx() const { return m_mvCtx; }
private:
    Mpeg2MvCtx m_mvCtx;
};

struct Mpeg2Slice
{
    Ipp16u firstMbX;
    Ipp16u firstMbY;
    Ipp8u quantiser_scale_code;

    void Reset() { memset(this, 0, sizeof(Mpeg2Slice)); }
};

inline
Ipp32s GetMvIdx(Ipp32s r, Ipp32s s) { return 2 * r + s; }

#define MPEG2_MB_DCT_COEF_BLOCKS8x8

enum { Mpeg2MvR0Fwd = 0, Mpeg2MvR0Bwd = 1, Mpeg2MvR1Fwd = 2, Mpeg2MvR1Bwd = 3 };
struct Mpeg2Mb
{
    struct Mv { Ipp16s x; Ipp16s y; };

    Ipp16u mbX;
    Ipp16u mbY;
    Ipp8u macroblock_type;
    Ipp8u motion_type;
    Ipp8u dct_type;
    Ipp8u quantiser_scale_code;
    Ipp16u coded_block_pattern;
    Ipp8u skipped;
    Ipp8u field_select[4];
    Ipp16s prevDc[3];
    Mv mv[4], pmv[4];

    Ipp16s* dctCoef;
    Ipp8s lastNzCoefInBlk[8];

    void AppendCoef(Ipp8u blk, Ipp16u idx, Ipp16s val)
    {
        dctCoef[64 * blk + idx] = val;
        lastNzCoefInBlk[blk] = (Ipp8s)idx;
    }

    void InitFirstMb(const Mpeg2Slice& slice, const Mpeg2PicHead& pic)
    {
        mbX = 0xffff;
        mbY = slice.firstMbY;
        quantiser_scale_code = slice.quantiser_scale_code;
        ResetMv();
        ResetDc(pic.intra_dc_precision);
    }

    void ResetDctCoefs(Ipp8u blk) { memset(dctCoef + 64 * blk, 0, 64 * sizeof(Ipp16s)); lastNzCoefInBlk[blk] = -1; }
    void ResetDctCoefsAll(Ipp8u numBlk) { for(Ipp8u i = 0; i < numBlk; i++) ResetDctCoefs(i); }
    void ResetDc(Ipp8u intraDcPrecision) { prevDc[0] = prevDc[1] = prevDc[2] = reset_dc[intraDcPrecision]; }
    void ResetMv() { memset(pmv, 0, sizeof(pmv)); memset(mv, 0, sizeof(mv)); }
    void ResetFieldSel(Ipp32u picStruct) { memset(field_select, (picStruct == BOTTOM_FIELD) ? 1 : 0, sizeof(field_select)); }
    void UpdateMv(Ipp32s s) { pmv[GetMvIdx(1, s)] = pmv[GetMvIdx(0, s)]; }
    bool IsIntra() const { return (macroblock_type & IPPVC_MB_INTRA) != 0; }
    bool IsFwdPred() const { return (macroblock_type & IPPVC_MB_FORWARD) != 0; }
    bool IsBwdPred() const { return (macroblock_type & IPPVC_MB_BACKWARD) != 0; }
    bool IsPattern() const { return (macroblock_type & IPPVC_MB_PATTERN) != 0; }
    bool IsQuant() const { return (macroblock_type & IPPVC_MB_QUANT) != 0; }

    void Reset()
    {
        macroblock_type = 0;
        motion_type = 0;
        dct_type = 0;
        coded_block_pattern = 0;
        memset(field_select, 0, 4*sizeof(Ipp8u));
        skipped = 0;
    }

};

} // namespace UMC

#pragma warning(default: 4324)

extern Ipp16s zero_memory[64*8];

/*******************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#define ippiCopy8x16_8u_C1R(pSrc, srcStep, pDst, dstStep)                   \
  ippiCopy8x8_8u_C1R(pSrc, srcStep, pDst, dstStep);                         \
  ippiCopy8x8_8u_C1R(pSrc + 8*(srcStep), srcStep, pDst + 8*(dstStep), dstStep);

#define ippiCopy8x16HP_8u_C1R(pSrc, srcStep, pDst, dstStep, acc, rounding)  \
  ippiCopy8x8HP_8u_C1R(pSrc, srcStep, pDst, dstStep, acc, rounding);        \
  ippiCopy8x8HP_8u_C1R(pSrc + 8 * srcStep, srcStep, pDst + 8 * dstStep, dstStep, acc, rounding);

#define ippiInterpolateAverage8x16_8u_C1IR(pSrc, srcStep, pDst, dstStep, acc, rounding)  \
  ippiInterpolateAverage8x8_8u_C1IR(pSrc, srcStep, pDst, dstStep, acc, rounding);        \
  ippiInterpolateAverage8x8_8u_C1IR(pSrc + 8 * srcStep, srcStep, pDst + 8 * dstStep, dstStep, acc, rounding);

/********************************************************************************/

typedef void (*ownvc_CopyHP_8u_C1R_func) (const Ipp8u *pSrc, Ipp32s srcStep, Ipp8u *pDst, Ipp32s dstStep);
typedef void (*ownvc_AverageHP_8u_C1R_func) (const Ipp8u *pSrc, Ipp32s srcStep, Ipp8u *pDst, Ipp32s dstStep);

/********************************************************************************/

#ifndef IPP_DIRECT_CALLS

#define FUNC_COPY_HP(W, H, pSrc, srcStep, pDst, dstStep, acc, rounding) \
  ippiCopy##W##x##H##HP_8u_C1R(pSrc, srcStep, pDst, dstStep, acc, rounding)

#define FUNC_AVE_HP(W, H, pSrc, srcStep, pDst, dstStep, mcType, roundControl) \
  ippiInterpolateAverage##W##x##H##_8u_C1IR(pSrc, srcStep, pDst, dstStep, mcType, roundControl);

#define FUNC_AVE_HP_B(W, H, pSrcRefF, srcStepF, mcTypeF, pSrcRefB, srcStepB, mcTypeB, \
                      pDst, dstStep, roundControl) \
  FUNC_COPY_HP(W, H, pSrcRefF, srcStepF, pDst, dstStep, mcTypeF, roundControl); \
  FUNC_AVE_HP(W, H, pSrcRefB, srcStepB, pDst, dstStep, mcTypeB, roundControl)

#else

extern const ownvc_CopyHP_8u_C1R_func ownvc_Copy8x4HP_8u_C1R[4];
extern const ownvc_CopyHP_8u_C1R_func ownvc_Copy8x8HP_8u_C1R[4];
extern const ownvc_CopyHP_8u_C1R_func ownvc_Copy16x8HP_8u_C1R[4];
extern const ownvc_CopyHP_8u_C1R_func ownvc_Copy16x16HP_8u_C1R[4];
extern const ownvc_CopyHP_8u_C1R_func ownvc_Copy8x16HP_8u_C1R[4];

extern const ownvc_AverageHP_8u_C1R_func ownvc_Average8x4HP_8u_C1R[4];
extern const ownvc_AverageHP_8u_C1R_func ownvc_Average8x8HP_8u_C1R[4];
extern const ownvc_AverageHP_8u_C1R_func ownvc_Average16x8HP_8u_C1R[4];
extern const ownvc_AverageHP_8u_C1R_func ownvc_Average16x16HP_8u_C1R[4];
extern const ownvc_AverageHP_8u_C1R_func ownvc_Average8x16HP_8u_C1R[4];

#define FUNC_COPY_HP(W, H, pSrc, srcStep, pDst, dstStep, acc, rounding) \
  ownvc_Copy##W##x##H##HP_8u_C1R[acc](pSrc, srcStep, pDst, dstStep);

#define FUNC_AVE_HP(W, H, pSrc, srcStep, pDst, dstStep, mcType, roundControl) \
  ownvc_Average##W##x##H##HP_8u_C1R[mcType](pSrc, srcStep, pDst, dstStep);

#define FUNC_AVE_HP_B(W, H, pSrcRefF, srcStepF, mcTypeF, pSrcRefB, srcStepB, mcTypeB, \
                      pDst, dstStep, roundControl) \
  FUNC_COPY_HP(W, H, pSrcRefF, srcStepF, pDst, dstStep, mcTypeF, roundControl); \
  FUNC_AVE_HP(W, H, pSrcRefB, srcStepB, pDst, dstStep, mcTypeB, roundControl)

#endif /* IPP_DIRECT_CALLS */

/********************************************************************************/

#ifdef __cplusplus
} /* extern "C" */
#endif

/*******************************************************/

#define HP_FLAG_MC(flag, x, y) \
  flag = ((x + x) & 2) | (y & 1); \
  flag = (flag << 2)

#define HP_FLAG_CP(flag, x, y) \
  flag = (x & 1) | ((y + y) & 2)

#define HP_FLAG_AV  HP_FLAG_CP

#define CHECK_OFFSET_L(offs, pitch, hh) \
  if (offs < 0 || (Ipp32s)(offs+(hh-1)*(pitch)+15) > (Ipp32s)video->pic_size) \
{ \
  return UMC_ERR_INVALID_STREAM; \
}

#define CALC_OFFSETS_FRAME_420(offs_l, offs_c, flag_l, flag_c, xl, yl, HP_FLAG) \
{ \
  Ipp32s xc = xl/2; \
  Ipp32s yc = yl/2; \
\
  offs_l = video->offset_l + (yl >> 1)*pitch_l + (xl >> 1); \
  offs_c = video->offset_c + (yc >> 1)*pitch_c + (xc >> 1); \
  HP_FLAG(flag_l, xl, yl); \
  HP_FLAG(flag_c, xc, yc); \
}

#define CALC_OFFSETS_FULLPEL(offs_l, offs_c, xl, yl, pitch_l, pitch_c) \
{ \
  Ipp32s xc = xl/2; \
  Ipp32s yc = yl/2; \
\
  offs_l = video->offset_l + (yl >> 1)*pitch_l + (xl >> 1); \
  offs_c = video->offset_c + (yc >> 1)*pitch_c + (xc >> 1); \
}

#define CALC_OFFSETS_FIELD_420(offs_l, offs_c, flag_l, flag_c, xl, yl, field_sel, HP_FLAG) \
{ \
  Ipp32s xc = xl/2; \
  Ipp32s yc = yl/2; \
\
  offs_l = video->offset_l + ((yl &~ 1) + field_sel)*pitch_l + (xl >> 1); \
  offs_c = video->offset_c + ((yc &~ 1) + field_sel)*pitch_c + (xc >> 1); \
  HP_FLAG(flag_l, xl, yl); \
  HP_FLAG(flag_c, xc, yc); \
}

#define CALC_OFFSETS_FIELDX_420(offs_l, offs_c, flag_l, flag_c, xl, yl, field_sel, HP_FLAG) \
{ \
  Ipp32s xc = xl/2; \
  Ipp32s yc = yl/2; \
  offs_l = ((yl &~ 1) + 2*video->row_l + field_sel)*pitch_l + (xl >> 1) + video->col_l; \
  offs_c = ((yc &~ 1) + 2*video->row_c + field_sel)*pitch_c + (xc >> 1) + video->col_c; \
  HP_FLAG(flag_l, xl, yl); \
  HP_FLAG(flag_c, xc, yc); \
}

#define CALC_OFFSETS_FRAME_422(offs_l, offs_c, flag_l, flag_c, xl, yl, HP_FLAG) \
{ \
  Ipp32s xc = xl/2; \
  Ipp32s yc = yl; \
\
  offs_l = video->offset_l + (yl >> 1)*pitch_l + (xl >> 1); \
  offs_c = video->offset_c + (yc >> 1)*pitch_c + (xc >> 1); \
  HP_FLAG(flag_l, xl, yl); \
  HP_FLAG(flag_c, xc, yc); \
}

#define CALC_OFFSETS_FIELD_422(offs_l, offs_c, flag_l, flag_c, xl, yl, field_sel, HP_FLAG) \
{ \
  Ipp32s xc = xl/2; \
  Ipp32s yc = yl; \
\
  offs_l = video->offset_l + ((yl &~ 1) + field_sel)*pitch_l + (xl >> 1); \
  offs_c = video->offset_c + ((yc &~ 1) + field_sel)*pitch_c + (xc >> 1); \
  HP_FLAG(flag_l, xl, yl); \
  HP_FLAG(flag_c, xc, yc); \
}

#define CALC_OFFSETS_FIELDX_422(offs_l, offs_c, flag_l, flag_c, xl, yl, field_sel, HP_FLAG) \
{ \
  Ipp32s xc = xl/2; \
  Ipp32s yc = yl; \
  offs_l = ((yl &~ 1) + 2*video->row_l + field_sel)*pitch_l + (xl >> 1) + video->col_l; \
  offs_c = ((yc &~ 1) + 2*video->row_c + field_sel)*pitch_c + (xc >> 1) + video->col_c; \
  HP_FLAG(flag_l, xl, yl); \
  HP_FLAG(flag_c, xc, yc); \
}

#define MCZERO_FRAME(ADD, ref_Y_data1, ref_U_data1, ref_V_data1, flag1, flag2) \
  FUNC_##ADD##_HP(16, 16, ref_Y_data1,pitch_l,     \
                          cur_Y_data,                   \
                          pitch_l, flag1, 0);           \
  FUNC_##ADD##_HP(8, 8, ref_U_data1,pitch_c,     \
                              cur_U_data,               \
                              pitch_c, flag2, 0);      \
  FUNC_##ADD##_HP(8, 8, ref_V_data1,pitch_c,     \
                              cur_V_data,               \
                              pitch_c, flag2, 0);

#define MCZERO_FRAME_422(ADD, ref_Y_data1, ref_U_data1, ref_V_data1, flag1, flag2) \
  FUNC_##ADD##_HP(16, 16, ref_Y_data1,pitch_l,     \
                          cur_Y_data,                   \
                          pitch_l, flag1, 0);           \
  FUNC_##ADD##_HP(8, 16, ref_U_data1,pitch_c,     \
                              cur_U_data,               \
                              pitch_c, flag2, 0);      \
  FUNC_##ADD##_HP(8, 16, ref_V_data1,pitch_c,     \
                              cur_V_data,               \
                              pitch_c, flag2, 0);

#define MCZERO_FIELD0(ADD, ref_Y_data1, ref_U_data1, ref_V_data1, flag1, flag2) \
  FUNC_##ADD##_HP(16, 8, ref_Y_data1, pitch_l2,      \
                      cur_Y_data, pitch_l2,            \
                      flag1, 0);                        \
  FUNC_##ADD##_HP(8, 4, ref_U_data1, pitch_c2,     \
                      cur_U_data, pitch_c2,       \
                      flag2, 0);                    \
  FUNC_##ADD##_HP(8, 4, ref_V_data1, pitch_c2,     \
                      cur_V_data, pitch_c2,       \
                      flag2, 0);

#define MCZERO_FIELD0_422(ADD, ref_Y_data1, ref_U_data1, ref_V_data1, flag1, flag2) \
  FUNC_##ADD##_HP(16, 8, ref_Y_data1, pitch_l2,      \
                      cur_Y_data, pitch_l2,            \
                      flag1, 0);                        \
  FUNC_##ADD##_HP(8, 8, ref_U_data1, pitch_c2,     \
                      cur_U_data, pitch_c2,       \
                      flag2, 0);                    \
  FUNC_##ADD##_HP(8, 8, ref_V_data1, pitch_c2,     \
                      cur_V_data, pitch_c2,       \
                      flag2, 0);

#define MCZERO_FIELD1(ADD, ref_Y_data2, ref_U_data2, ref_V_data2, flag1, flag2) \
  FUNC_##ADD##_HP(16, 8, ref_Y_data2, pitch_l2,       \
                      cur_Y_data + pitch_l, pitch_l2,   \
                      flag1, 0);                         \
  FUNC_##ADD##_HP(8, 4, ref_U_data2, pitch_c2,          \
                      cur_U_data + pitch_c, pitch_c2, \
                      flag2, 0);                         \
  FUNC_##ADD##_HP(8, 4, ref_V_data2, pitch_c2,          \
                      cur_V_data + pitch_c, pitch_c2, \
                      flag2, 0);

#define MCZERO_FIELD1_422(ADD, ref_Y_data2, ref_U_data2, ref_V_data2, flag1, flag2) \
  FUNC_##ADD##_HP(16, 8, ref_Y_data2, pitch_l2,       \
                      cur_Y_data + pitch_l, pitch_l2,   \
                      flag1, 0);                         \
  FUNC_##ADD##_HP(8, 8, ref_U_data2, pitch_c2,          \
                      cur_U_data + pitch_c, pitch_c2, \
                      flag2, 0);                         \
  FUNC_##ADD##_HP(8, 8, ref_V_data2, pitch_c2,          \
                      cur_V_data + pitch_c, pitch_c2, \
                      flag2, 0);

#define SWAP(TYPE, _val0, _val1) { \
  TYPE _tmp = _val0; \
  _val0 = _val1; \
  _val1 = _tmp; \
}

#define RESET_PMV(array) {                                      \
  for(unsigned int nn=0; nn<sizeof(array)/sizeof(Ipp32s); nn++) \
    ((Ipp32s*)array)[nn] = 0;                                   \
}

#define COPY_PMV(adst,asrc) {                                   \
  for(unsigned int nn=0; nn<sizeof(adst)/sizeof(Ipp32s); nn++)  \
    ((Ipp32s*)adst)[nn] = ((Ipp32s*)asrc)[nn];                  \
}

#define RECONSTRUCT_INTRA_MB(BITSTREAM, NUM_BLK, DCT_TYPE)               \
{                                                                        \
  Ipp32s *pitch = video->blkPitches[DCT_TYPE];                                  \
  Ipp32s *offsets = video->blkOffsets[DCT_TYPE];                                \
  Ipp32s curr_index = frame_buffer.curr_index[task_num];               \
  Ipp8u* yuv[3] = {                                                      \
    frame_buffer.frame_p_c_n[curr_index].Y_comp_data + video->offset_l,  \
    frame_buffer.frame_p_c_n[curr_index].U_comp_data + video->offset_c,  \
    frame_buffer.frame_p_c_n[curr_index].V_comp_data + video->offset_c   \
  };                                                                     \
  Ipp32s blk;                                                            \
  IppiDecodeIntraSpec_MPEG2 *intraSpec = &video->decodeIntraSpec;        \
                                                                         \
  for (blk = 0; blk < NUM_BLK; blk++) {                                  \
    IppStatus sts;                                                       \
    Ipp32s chromaFlag, cc;                                               \
    chromaFlag = blk >> 2;                                               \
    cc = chromaFlag + (blk & chromaFlag);                                \
    CHR_SPECINTRA_##NUM_BLK                                              \
                                                                         \
    sts = ippiDecodeIntra8x8IDCT_MPEG2_1u8u(                             \
      &BITSTREAM##_curr_ptr,                                             \
      &BITSTREAM##_bit_offset,                                           \
      intraSpec,                                                         \
      video->cur_q_scale,                                                \
      chromaFlag,                                                        \
      &video->dct_dc_past[cc],                                           \
      yuv[cc] + offsets[blk],                                            \
      pitch[chromaFlag]);                                                \
    if(sts != ippStsOk)                                                  \
      return sts;                                                        \
  }                                                                      \
}

#define CHR_SPECINTRA_6
#define CHR_SPECINTRA_8 intraSpec = chromaFlag ? &video->decodeIntraSpecChroma : &video->decodeIntraSpec; \

#define RECONSTRUCT_INTRA_MB_420(BITSTREAM, DCT_TYPE) \
  RECONSTRUCT_INTRA_MB(BITSTREAM, 6, DCT_TYPE)

#define RECONSTRUCT_INTRA_MB_422(BITSTREAM, DCT_TYPE) \
  RECONSTRUCT_INTRA_MB(BITSTREAM, 8, DCT_TYPE)


#define DECODE_MBPATTERN_6(code, BITSTREAM, vlcMBPattern)                \
  DECODE_VLC(code, BITSTREAM, vlcMBPattern)

#define DECODE_MBPATTERN_8(code, BITSTREAM, vlcMBPattern)                \
{                                                                        \
  Ipp32s cbp_1;                                                          \
  DECODE_VLC(code, BITSTREAM, vlcMBPattern);                             \
  GET_TO9BITS(video->bs, 2, cbp_1);                                      \
  code = (code << 2) | cbp_1;                                            \
}

#define RECONSTRUCT_INTER_MB(BITSTREAM, NUM_BLK, DCT_TYPE)               \
{                                                                        \
  IppiDecodeInterSpec_MPEG2 *interSpec = &video->decodeInterSpec;        \
  Ipp32s cur_q_scale = video->cur_q_scale;                               \
  Ipp32s *pitch = video->blkPitches[DCT_TYPE];                           \
  Ipp32s *offsets = video->blkOffsets[DCT_TYPE];                         \
  Ipp32s mask = 1 << (NUM_BLK - 1);                                      \
  Ipp32s code;                                                           \
  Ipp32s blk;                                                            \
                                                                         \
  DECODE_MBPATTERN_##NUM_BLK(code, BITSTREAM, vlcMBPattern);             \
                                                                         \
  for (blk = 0; blk < NUM_BLK; blk++) {                                  \
    if (code & mask) {                                                   \
      IppStatus sts;                                                     \
      Ipp32s chromaFlag = blk >> 2;                                      \
      Ipp32s cc = chromaFlag + (blk & chromaFlag);                       \
      CHR_SPECINTER_##NUM_BLK                                                 \
                                                                         \
      sts = ippiDecodeInter8x8IDCTAdd_MPEG2_1u8u(                        \
        &BITSTREAM##_curr_ptr,                                           \
        &BITSTREAM##_bit_offset,                                         \
        interSpec,                                                       \
        cur_q_scale,                                                     \
        video->blkCurrYUV[cc] + offsets[blk],                            \
        pitch[chromaFlag]);                                              \
      if(sts != ippStsOk)                                                \
        return sts;                                                      \
    }                                                                    \
    code += code;                                                        \
  }                                                                      \
}

#define CHR_SPECINTER_6
#define CHR_SPECINTER_8 interSpec = chromaFlag ? &video->decodeInterSpecChroma : &video->decodeInterSpec;

#define RECONSTRUCT_INTER_MB_420(BITSTREAM, DCT_TYPE) \
  RECONSTRUCT_INTER_MB(BITSTREAM, 6, DCT_TYPE)

#define RECONSTRUCT_INTER_MB_422(BITSTREAM, DCT_TYPE) \
  RECONSTRUCT_INTER_MB(BITSTREAM, 8, DCT_TYPE)

#define UPDATE_MV(val, mcode, S, task_num) \
  if(PictureHeader[task_num].r_size[S]) { \
    GET_TO9BITS(video->bs,PictureHeader[task_num].r_size[S], residual); \
    if(mcode < 0) { \
        val += ((mcode + 1) << PictureHeader[task_num].r_size[S]) - (residual + 1); \
        if(val < PictureHeader[task_num].low_in_range[S]) \
          val += PictureHeader[task_num].range[S]; \
    } else { \
        val += ((mcode - 1) << PictureHeader[task_num].r_size[S]) + (residual + 1); \
        if(val > PictureHeader[task_num].high_in_range[S]) \
          val -= PictureHeader[task_num].range[S]; \
    } \
  } else { \
    val += mcode; \
    if(val < PictureHeader[task_num].low_in_range[S]) \
      val += PictureHeader[task_num].range[S]; \
    else if(val > PictureHeader[task_num].high_in_range[S]) \
      val -= PictureHeader[task_num].range[S]; \
  }
#ifdef FIX_CUST_BUG_24419
#define VECTOR_BOUNDS_CORRECT_X(vX,task_num)\
    if(video->mb_col*16 + (vX >> 1) < 0) {          \
        vX = vX; }                            \
    if((video->mb_col + 1) * 16 + (vX >> 1) > sequenceHeader.mb_width[task_num] * 16) { \
        vX = (sequenceHeader.mb_width[task_num] - video->mb_col - 1) * 16 - 1;  \
  }
#endif

#ifdef FIX_CUST_BUG_24419
#define DECODE_MV(BS, R, S, vectorX, vectorY, task_num)           \
  /* R = 2*(2*r + s); S = 2*s */                        \
  /* Decode x vector */                                 \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&video->PMV[R], S, video, task_num);                \
  }                                                     \
  vectorX = video->PMV[R];                              \
  VECTOR_BOUNDS_CORRECT_X(vectorX,task_num);            \
                                                        \
  /* Decode y vector */                                 \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&video->PMV[R + 1], S + 1, video, task_num);        \
  }                                                     \
  vectorY = video->PMV[R + 1];

#define DECODE_MV_FIELD(BS, R, S, vectorX, vectorY, task_num)     \
  /* R = 2*(2*r + s); S = 2*s */                        \
  /* Decode x vector */                                 \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&video->PMV[R], S, video,task_num);                \
  }                                                     \
  vectorX = video->PMV[R];                              \
  VECTOR_BOUNDS_CORRECT_X(vectorX,task_num);            \
                                                        \
  /* Decode y vector */                                 \
  vectorY = (Ipp16s)(video->PMV[R + 1] >> 1);           \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&vectorY, S + 1, video, task_num);                  \
  }                                                     \
  video->PMV[R + 1] = (Ipp16s)(vectorY << 1)


#define DECODE_MV_FULLPEL(BS, R, S, vectorX, vectorY, task_num)   \
  /* R = 2*(2*r + s); S = 2*s */                        \
  /* Decode x vector */                                 \
  vectorX = (Ipp16s)(video->PMV[R] >> 1);               \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&vectorX, S, video, task_num);                      \
  }                                                     \
  VECTOR_BOUNDS_CORRECT_X(vectorX,task_num);            \
  video->PMV[R] = (Ipp16s)(vectorX << 1);               \
                                                        \
  /* Decode y vector */                                 \
  vectorY = (Ipp16s)(video->PMV[R + 1] >> 1);           \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&vectorY, S + 1, video, task_num);                  \
  }                                                     \
  video->PMV[R + 1] = (Ipp16s)(vectorY << 1)
#else
#define DECODE_MV(BS, R, S, vectorX, vectorY, task_num)           \
  /* R = 2*(2*r + s); S = 2*s */                        \
  /* Decode x vector */                                 \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&video->PMV[R], S, video, task_num);                \
  }                                                     \
  vectorX = video->PMV[R];                              \
                                                        \
  /* Decode y vector */                                 \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&video->PMV[R + 1], S + 1, video, task_num);        \
  }                                                     \
  vectorY = video->PMV[R + 1];

#define DECODE_MV_FIELD(BS, R, S, vectorX, vectorY, task_num)     \
  /* R = 2*(2*r + s); S = 2*s */                        \
  /* Decode x vector */                                 \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&video->PMV[R], S, video,task_num);                \
  }                                                     \
  vectorX = video->PMV[R];                              \
                                                        \
  /* Decode y vector */                                 \
  vectorY = (Ipp16s)(video->PMV[R + 1] >> 1);           \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&vectorY, S + 1, video, task_num);                  \
  }                                                     \
  video->PMV[R + 1] = (Ipp16s)(vectorY << 1)


#define DECODE_MV_FULLPEL(BS, R, S, vectorX, vectorY, task_num)   \
  /* R = 2*(2*r + s); S = 2*s */                        \
  /* Decode x vector */                                 \
  vectorX = (Ipp16s)(video->PMV[R] >> 1);               \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&vectorX, S, video, task_num);                      \
  }                                                     \
  video->PMV[R] = (Ipp16s)(vectorX << 1);               \
                                                        \
  /* Decode y vector */                                 \
  vectorY = (Ipp16s)(video->PMV[R + 1] >> 1);           \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&vectorY, S + 1, video, task_num);                  \
  }                                                     \
  video->PMV[R + 1] = (Ipp16s)(vectorY << 1)
#endif

#define DECODE_QUANTIZER_SCALE(BS, Q_SCALE) \
{                                           \
  Ipp32s _q_scale;                             \
  GET_TO9BITS(video->bs, 5, _q_scale)       \
  if (_q_scale < 1) {                       \
    return UMC_ERR_INVALID_STREAM;                  \
  }                                         \
  Q_SCALE = q_scale[PictureHeader[task_num].q_scale_type][_q_scale]; \
}

#define DECODE_MB_INCREMENT(BS, macroblock_address_increment)         \
{                                                                     \
  Ipp32s cc;                                                          \
  SHOW_BITS(BS, 11, cc)                                               \
                                                                      \
  if(cc == 0) {                                                       \
    return UMC_OK; /* end of slice or bad code. Anyway, stop slice */ \
  }                                                                   \
                                                                      \
  macroblock_address_increment = 0;                                   \
  while(cc == 8)                                                      \
  {                                                                   \
    macroblock_address_increment += 33; /* macroblock_escape */       \
    GET_BITS(BS, 11, cc);                                             \
    SHOW_BITS(BS, 11, cc)                                             \
  }                                                                   \
  DECODE_VLC(cc, BS, vlcMBAdressing);                                 \
  macroblock_address_increment += cc;                                 \
  macroblock_address_increment--;                                     \
  if (static_cast<unsigned int>(macroblock_address_increment) > static_cast<unsigned int>(sequenceHeader.mb_width[task_num] - video->mb_col)) { \
    macroblock_address_increment = sequenceHeader.mb_width[task_num] - video->mb_col;       \
  }                                                                               \
}

////////////////////////////////////////////////////////////////////

#define APPLY_SIGN(val, sign)  \
  val += sign;                 \
  if (val > 2047) val = 2047; /* with saturation */ \
  val ^= sign

#define SAT(val) \
  if (val > 2047) val = 2047;   \
  if (val < -2048) val = -2048;

#ifdef LOCAL_BUFFERS
#define DEF_BLOCK(NAME) \
  Ipp16s NAME[64];
#else
#define DEF_BLOCK(NAME) \
  Ipp16s *NAME = pQuantSpec->NAME;
#endif

#define MP2_FUNC(type, name, arg)  type name arg

#ifdef IPP_DIRECT_CALLS
#ifdef __cplusplus
extern "C" {
#endif
extern void dct_8x8_inv_2x2_16s(Ipp16s* pSrc, Ipp16s* pDst);
extern void dct_8x8_inv_4x4_16s(Ipp16s* pSrc, Ipp16s* pDst);
extern void dct_8x8_inv_16s(Ipp16s* pSrc, Ipp16s* pDst);
extern void dct_8x8_inv_16s8uR(Ipp16s* pSrc, Ipp8u* pDst, Ipp32s dstStep);
extern void ownvc_Add8x8_16s8u_C1IRS(const Ipp16s* pSrc, Ipp32s srcStep, Ipp8u* pSrcDst, Ipp32s srcDstStep);
#ifdef __cplusplus
} /* extern "C" */
#endif
#define FUNC_DCT8x8      dct_8x8_inv_16s
#define FUNC_DCT4x4      dct_8x8_inv_4x4_16s
#define FUNC_DCT2x2      dct_8x8_inv_2x2_16s
#define FUNC_DCT8x8Intra dct_8x8_inv_16s8uR
#define FUNC_ADD8x8      ownvc_Add8x8_16s8u_C1IRS
#else
#define FUNC_DCT8x8      ippiDCT8x8Inv_16s_C1
#define FUNC_DCT4x4      ippiDCT8x8Inv_4x4_16s_C1
#define FUNC_DCT2x2      ippiDCT8x8Inv_2x2_16s_C1
#define FUNC_DCT8x8Intra ippiDCT8x8Inv_16s8u_C1R
#define FUNC_ADD8x8      ippiAdd8x8_16s8u_C1IRS
#endif

void IDCTAdd_1x1to8x8(Ipp32s val, Ipp8u* y, Ipp32s step);
void IDCTAdd_1x4to8x8(const Ipp16s* x, Ipp8u* y, Ipp32s step);
void Pack8x8(Ipp16s* x, Ipp8u* y, Ipp32s step);


#ifdef MPEG2_USE_REF_IDCT

extern "C" {
void Reference_IDCT(Ipp16s *block, Ipp16s *out, Ipp32s step);
//#define Reference_IDCT(in, out, step) ippiDCT8x8Inv_16s_C1R(in, out, 2*(step))
} /* extern "C" */

#define IDCT_INTER(SRC, NUM, BUFF, DST, STEP)        \
  Reference_IDCT(SRC, BUFF, 8);                      \
  FUNC_ADD8x8(BUFF, 16, DST, STEP)

#define IDCT_INTER1(val, idct, pSrcDst, srcDstStep)  \
  SAT(val);                                          \
  for (k = 0; k < 64; k++) pDstBlock[k] = 0;         \
  pDstBlock[0] = val;                                \
  mask = 1 ^ val;                                    \
  pDstBlock[63] ^= mask & 1;                         \
  IDCT_INTER(pDstBlock, 0, idct, pSrcDst, srcDstStep)

#define IDCT_INTRA(SRC, NUM, BUFF, DST, STEP)       \
  Ipp32s ii, jj;                                    \
  SRC[0] -= 1024;                                   \
  Reference_IDCT(SRC, BUFF, 8);                     \
  for(ii = 0; ii < 8; ii++) {                       \
    for(jj = 0; jj < 8; jj++) {                     \
      BUFF[ii * 8 + jj] += 128;                     \
    }                                               \
  }                                                 \
  Pack8x8(BUFF, DST, STEP)

#define IDCT_INTRA1(val, idct, pSrcDst, srcDstStep)  \
  SAT(val);                                          \
  for (k = 0; k < 64; k++) pDstBlock[k] = 0;         \
  pDstBlock[0] = val;                                \
  mask = 1 ^ val;                                    \
  pDstBlock[63] ^= mask & 1;                         \
  IDCT_INTRA(pDstBlock, 0, idct, pSrcDst, srcDstStep)

#else /* MPEG2_USE_REF_IDCT */

#ifdef USE_INTRINSICS
#define IDCT_INTER_1x4(SRC, NUM, DST, STEP)        \
  if (NUM < 4 && !SRC[1]) {                        \
    IDCTAdd_1x4to8x8(SRC, DST, STEP);              \
  } else
#else
#define IDCT_INTER_1x4(SRC, NUM, DST, STEP)
#endif

#define IDCT_INTER(SRC, NUM, BUFF, DST, STEP)      \
  if (NUM < 10) {                                  \
    if (!NUM) {                                    \
      IDCTAdd_1x1to8x8(SRC[0], DST, STEP);         \
    } else                                         \
    IDCT_INTER_1x4(SRC, NUM, DST, STEP)            \
    /*if (NUM < 2) {                                 \
      FUNC_DCT2x2(SRC, BUFF);                      \
      FUNC_ADD8x8(BUFF, 16, DST, STEP);            \
    } else*/ {                                       \
      FUNC_DCT4x4(SRC, BUFF);                      \
      FUNC_ADD8x8(BUFF, 16, DST, STEP);            \
    }                                              \
  } else {                                         \
    FUNC_DCT8x8(SRC, BUFF);                        \
    FUNC_ADD8x8(BUFF, 16, DST, STEP);              \
  }

#define IDCT_INTER1(val, idct, pSrcDst, srcDstStep) \
  ippiAddC8x8_16s8u_C1IR((Ipp16s)((val + 4) >> 3), pSrcDst, srcDstStep)

#ifdef USE_INTRINSICS

#define IDCT_INTRA(SRC, NUM, BUFF, DST, STEP)                   \
  if (NUM < 10) {                                               \
    if (!NUM) {                                                 \
      ippiDCT8x8Inv_AANTransposed_16s8u_C1R(SRC, DST, STEP, 0); \
    } else {                                                    \
      FUNC_DCT4x4(SRC, BUFF);                                   \
      Pack8x8(BUFF, DST, STEP);                                 \
    }                                                           \
  } else {                                                      \
    FUNC_DCT8x8Intra(SRC, DST, STEP);                           \
  }

#else

#define IDCT_INTRA(SRC, NUM, BUFF, DST, STEP)                   \
  if (!NUM) {                                                   \
    ippiDCT8x8Inv_AANTransposed_16s8u_C1R(SRC, DST, STEP, 0);   \
  } else {                                                      \
    FUNC_DCT8x8Intra(SRC, DST, STEP);                           \
  }

#endif

#define IDCT_INTRA1(val, idct, pSrcDst, srcDstStep) \
  pDstBlock[0] = (Ipp16s)val; \
  ippiDCT8x8Inv_AANTransposed_16s8u_C1R(pDstBlock, pSrcDst, srcDstStep, 0)

#endif // MPEG2_USE_REF_IDCT

#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
