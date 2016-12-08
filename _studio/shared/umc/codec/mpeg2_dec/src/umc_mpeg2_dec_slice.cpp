//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#include <vector>
#include "umc_mpeg2_dec_defs_sw.h"
#include "umc_mpeg2_dec_sw.h"
#include "umc_mpeg2_dec_tbl.h"

#pragma warning(disable: 4244)

using namespace UMC;

const Ipp32s ALIGN_VALUE  = 16;

static void mp2_HuffmanTableFree(mp2_VLCTable *vlc)
{
    if (vlc->table0) {
        ippsFree(vlc->table0);
        vlc->table0 = NULL;
    }
    if (vlc->table1) {
        ippsFree(vlc->table1);
        vlc->table1 = NULL;
    }
}

static IppStatus mp2_HuffmanTableInitAlloc_HandleError(Ipp32s *buffer, Ipp16s *table0, Ipp16s *table1)
{
  if (buffer)
    delete[] buffer;
  if (table0)
    ippsFree(table0);
  if (table1)
    ippsFree(table1);
  return ippStsErr;
}

static IppStatus mp2_HuffmanTableInitAlloc(Ipp32s *tbl, Ipp32s bits_table0, mp2_VLCTable *vlc)
{
  Ipp32s *ptbl;
  Ipp16s bad_value = 0;
  Ipp32s max_bits;
  Ipp32s num_tbl;
  Ipp32s i, j, k, m, n;
  Ipp32s bits, code, value;
  Ipp32s bits0, bits1;
  Ipp32s min_value, max_value, spec_value;
  Ipp32s min_code0, min_code1;
  Ipp32s max_code0, max_code1;
  Ipp32s prefix_code1 = -1;
  Ipp32s bits_table1 = 0;
  Ipp32s *buffer = NULL;
  Ipp32s *codes;
  Ipp32s *cbits;
  Ipp32s *values;
  Ipp16s *table0 = NULL;
  Ipp16s *table1 = NULL;

  /* get number of entries (n) */
  max_bits = *tbl++;
  num_tbl = *tbl++;
  for (i = 0; i < num_tbl; i++) {
    *tbl++;
  }
  n = 0;
  ptbl = tbl;
  for (bits = 1; bits <= max_bits; bits++) {
    m = *ptbl;
    if (m < 0) break;
    n += m;
    ptbl += 2*m + 1;
  }

  /* alloc internal table */
  buffer = new Ipp32s[3*n];
  if (!buffer) return ippStsErr;
  codes = buffer;
  cbits = buffer + n;
  values = buffer + 2*n;

  /* read VLC to internal table */
  min_value = 0x7fffffff;
  max_value = 0;
  spec_value = 0;
  ptbl = tbl;
  k = 0;
  for (bits = 1; bits <= max_bits; bits++) {
    if (*ptbl < 0) break;
    m = *ptbl++;
    for (i = 0; i < m; i++) {
      code = *ptbl++;
      value = *ptbl++;
      code &= ((1 << bits) - 1);
      if (value < min_value) min_value = value;
      if (value > max_value) {
        if (!spec_value && value >= 0xffff) {
          spec_value = value;
        } else {
          max_value = value;
        }
      }
#ifdef M2D_DEBUG
      if (vlc_print) {
        for (j = 0; j < bits; j++) {
          printf("%c", (code & (1 << (bits - 1 - j))) ? '1' : '0');
        }
        for (j = bits; j < max_bits; j++) {
          printf("%c", ' ');
        }
        printf("  -> %d", value);
        printf("\n");
      }
#endif
      codes[k] = code << (30 - bits);
      cbits[k] = bits;
      values[k] = value;
      k++;
    }
  }
#ifdef M2D_DEBUG
  if (vlc_print) {
    printf("Values: [%d, %d]", min_value, max_value);
    if (spec_value) printf("   special value = %x\n", spec_value);
    printf("\n");
    printf("---------------------------\n");
  }
#endif

  if (!bits_table0) {
    bits_table0 = max_bits;
    bits_table1 = 0;
    vlc->threshold_table0 = 0;
  }
  bits0 = bits_table0;
  //for (bits0 = 1; bits0 < max_bits; bits0++)
  if (bits0 > 0 && bits0 < max_bits) {
    min_code0 = min_code1 = 0x7fffffff;
    max_code0 = max_code1 = 0;
    for (i = 0; i < n; i++) {
      code = codes[i];
      bits = cbits[i];
      if (bits <= bits0) {
        if (code > max_code0) max_code0 = code;
        if (code < min_code0) min_code0 = code;
      } else {
        if (code > max_code1) max_code1 = code;
        if (code < min_code1) min_code1 = code;
      }
    }
#ifdef M2D_DEBUG
    if (vlc_print) {
      printf("bits0 = %2d: table0 [0x%3x,0x%3x], table1 [0x%3x,0x%3x]",
        bits0,
        max_code0 >> (30 - max_bits),
        min_code0 >> (30 - max_bits),
        max_code1 >> (30 - max_bits),
        min_code1 >> (30 - max_bits));
    }
#endif
    if ((max_code0 < min_code1) || (max_code1 < min_code0)) {
      for (j = 0; j < 29; j++) {
        if ((min_code1 ^ max_code1) & (1 << (29 - j))) break;
      }
      bits1 = max_bits - j;
      if (bits0 == bits_table0) {
        bits_table1 = bits1;
        prefix_code1 = min_code1 >> (30 - bits0);
        vlc->threshold_table0 = min_code0 >> (30 - max_bits);
      }
#ifdef M2D_DEBUG
      if (vlc_print) {
        printf(" # Ok: Tables: %2d + %2d = %d bits", bits0, bits1, bits0 + bits1);
      }
#endif
    }
  }

  if (bits_table0 > 0 && bits_table0 < max_bits && !bits_table1) {
    if (buffer) delete[] buffer;
    return ippStsErr;
  }

  bad_value = (Ipp16s)((bad_value << 8) | VLC_BAD);

  table0 = ippsMalloc_16s(1 << bits_table0);
  if (NULL == table0)
    return mp2_HuffmanTableInitAlloc_HandleError(buffer, table0, table1);

  ippsSet_16s(bad_value, table0, 1 << bits_table0);
  if (bits_table1) {
    table1 = ippsMalloc_16s(1 << bits_table1);
    if (NULL == table1)
      return mp2_HuffmanTableInitAlloc_HandleError(buffer, table0, table1);

    ippsSet_16s(bad_value, table1, 1 << bits_table1);
  }
  for (i = 0; i < n; i++) {
    code = codes[i];
    bits = cbits[i];
    value = values[i];
    if (bits <= bits_table0) {
      code = code >> (30 - bits_table0);
      for (j = 0; j < (1 << (bits_table0 - bits)); j++) {
        table0[code + j] = (Ipp16s)((value << 8) | bits);
      }
    } else if(table1){
      code = code >> (30 - max_bits);
      code = code & ((1 << bits_table1) - 1);
      for (j = 0; j < (1 << (max_bits - bits)); j++) {
        table1[code + j] = (Ipp16s)((value << 8) | bits);
      }
    }
  }

  if (bits_table1) { // fill VLC_NEXTTABLE
    if (prefix_code1 == -1)
      return mp2_HuffmanTableInitAlloc_HandleError(buffer, table0, table1);

    bad_value = (Ipp16s)((bad_value &~ 255) | VLC_NEXTTABLE);
    for (j = 0; j < (1 << ((bits_table0 - (max_bits - bits_table1)))); j++) {
      table0[prefix_code1 + j] = bad_value;
    }
  }

  vlc->max_bits = max_bits;
  vlc->bits_table0 = bits_table0;
  vlc->bits_table1 = bits_table1;
  vlc->table0 = table0;
  vlc->table1 = table1;

  if (buffer) delete[] buffer;
  return ippStsNoErr;
}

MPEG2VideoDecoderSW::MPEG2VideoDecoderSW()
{
    vlcMBAdressing.table0 = vlcMBAdressing.table1 = NULL;

    vlcMBType[0].table0 = vlcMBType[0].table1 = NULL;
    vlcMBType[1].table0 = vlcMBType[1].table1 = NULL;
    vlcMBType[2].table0 = vlcMBType[2].table1 = NULL;

    vlcMBPattern.table0 = vlcMBPattern.table1 = NULL;
    vlcMotionVector.table0 = vlcMotionVector.table1 = NULL;
}

MPEG2VideoDecoderSW::~MPEG2VideoDecoderSW()
{
}

bool MPEG2VideoDecoderSW::InitTables()
{
    if (ippStsNoErr != mp2_HuffmanTableInitAlloc(MBAdressing, 5, &vlcMBAdressing))
        return false;

    if (ippStsNoErr != mp2_HuffmanTableInitAlloc(IMBType, 0, &vlcMBType[0]))
        return false;

    if (ippStsNoErr != mp2_HuffmanTableInitAlloc(PMBType, 0, &vlcMBType[1]))
        return false;

    if (ippStsNoErr != mp2_HuffmanTableInitAlloc(BMBType, 0, &vlcMBType[2]))
        return false;

    if (ippStsNoErr != mp2_HuffmanTableInitAlloc(MBPattern, 5, &vlcMBPattern))
        return false;

    if (ippStsNoErr != mp2_HuffmanTableInitAlloc(MotionVector, 5, &vlcMotionVector))
        return false;

    return MPEG2VideoDecoderBase::InitTables();
}

void MPEG2VideoDecoderSW::DeleteHuffmanTables()
{
    MPEG2VideoDecoderBase::DeleteHuffmanTables();
    // release tables
    mp2_HuffmanTableFree(&vlcMBAdressing);
    mp2_HuffmanTableFree(&vlcMBType[0]);
    mp2_HuffmanTableFree(&vlcMBType[1]);
    mp2_HuffmanTableFree(&vlcMBType[2]);
    mp2_HuffmanTableFree(&vlcMBPattern);
    mp2_HuffmanTableFree(&vlcMotionVector);
}

Status MPEG2VideoDecoderSW::DecodeSliceHeader(VideoContext *video, int task_num)
{
    Ipp32u extra_bit_slice;
    Ipp32u code;
    bool isCorrupted = false;

    if (!video)
    {
        return UMC_ERR_NULL_PTR;
    }

    FIND_START_CODE(video->bs, code)

    if(code == (Ipp32u)UMC_ERR_NOT_ENOUGH_DATA)
    {
        SKIP_TO_END(video->bs);
        return UMC_ERR_NOT_ENOUGH_DATA;
    }

    if(code == PICTURE_START_CODE ||
       code > 0x1AF)
    {
      return UMC_ERR_NOT_ENOUGH_DATA;
    }
    // if (video) redundant checking
    {
        SKIP_BITS_32(video->bs);
    }

    video->slice_vertical_position = (code & 0xff);

    if(video->clip_info_height > 2800)
    {
        if(video->slice_vertical_position > 0x80)
          return UMC_ERR_INVALID_STREAM;
        GET_TO9BITS(video->bs, 3, code)
        video->slice_vertical_position += code << 7;
    }
    if( video->slice_vertical_position > PictureHeader[task_num].max_slice_vert_pos)
    {
        video->slice_vertical_position = PictureHeader[task_num].max_slice_vert_pos;
        isCorrupted = true;
        return UMC_WRN_INVALID_STREAM;
    }

    if((sequenceHeader.extension_start_code_ID[task_num] == SEQUENCE_SCALABLE_EXTENSION_ID) &&
        (sequenceHeader.scalable_mode[task_num] == DATA_PARTITIONING))
    {
        GET_TO9BITS(video->bs, 7, code)
        return UMC_ERR_UNSUPPORTED;
    }

    GET_TO9BITS(video->bs, 5, video->cur_q_scale)
    if(video->cur_q_scale == 0)
    {
        isCorrupted = true;
        //return UMC_ERR_INVALID_STREAM;
    }

    GET_1BIT(video->bs,extra_bit_slice)
    while(extra_bit_slice)
    {
        GET_TO9BITS(video->bs, 9, code)
        extra_bit_slice = code & 1;
    }

    if (video->stream_type != MPEG1_VIDEO)
    {
      video->cur_q_scale = q_scale[PictureHeader[task_num].q_scale_type][video->cur_q_scale];
    }

    RESET_PMV(video->PMV)

    video->macroblock_motion_forward  = 0;
    video->macroblock_motion_backward = 0;

    video->m_bNewSlice = true;

    if (true != frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].isCorrupted && true == isCorrupted)
    {
        frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].isCorrupted = true;
    }

    return UMC_OK;
}

Status MPEG2VideoDecoderSW::DecodeSlice(VideoContext  *video, int task_num)
{
    if (MPEG1_VIDEO == video->stream_type) 
    {
        return DecodeSlice_MPEG1(video, task_num);
    }

    try 
    {
        switch (video->color_format)
        {
            case YUV420:

                if (MPEG2_I_PICTURE == PictureHeader[task_num].picture_coding_type)
                {
                    return DecodeSlice_FrameI_420(video, task_num);
                }
                else 
                {
                    if (FRAME_PICTURE == PictureHeader[task_num].picture_structure)
                    {
                        return DecodeSlice_FramePB_420(video, task_num);
                    } 
                    else 
                    {
                        return DecodeSlice_FieldPB_420(video, task_num);
                    }
                }

                break;

            case YUV422:
                
                if (MPEG2_I_PICTURE == PictureHeader[task_num].picture_coding_type)
                {
                    return DecodeSlice_FrameI_422(video, task_num);
                }
                else 
                {
                    if (FRAME_PICTURE == PictureHeader[task_num].picture_structure) 
                    {
                        return DecodeSlice_FramePB_422(video, task_num);
                    }
                    else
                    {
                        return DecodeSlice_FieldPB_422(video, task_num);
                    }
                }
                
                break;
            
            default:
                return UMC_ERR_INVALID_STREAM;
        }
    }
    catch (...)
    {
        return UMC_ERR_INVALID_STREAM;
    }
}

void MPEG2VideoDecoderSW::SetOutputPointers(MediaData *output, int task_num)
{
    VideoData *out_data = DynamicCast<VideoData, MediaData>(output);
    Ipp32s curr_index = frame_buffer.curr_index[task_num];
    Ipp32s pitch_l, pitch_c;

    Ipp32u threadsNum = GetCurrThreadsNum(task_num);

    if(frame_buffer.field_buffer_index[task_num] == 1)
    {
        if(curr_index < DPB_SIZE)
            return;
        frame_buffer.frame_p_c_n[curr_index].Y_comp_data = frame_buffer.frame_p_c_n[curr_index-DPB_SIZE].Y_comp_data;
        frame_buffer.frame_p_c_n[curr_index].U_comp_data = frame_buffer.frame_p_c_n[curr_index-DPB_SIZE].U_comp_data;
        frame_buffer.frame_p_c_n[curr_index].V_comp_data = frame_buffer.frame_p_c_n[curr_index-DPB_SIZE].V_comp_data;
    //    frame_buffer.frame_p_c_n[curr_index].IsUserDataDecoded = false;
     //   frame_buffer.frame_p_c_n[curr_index].us_data_size = 0;
        pitch_l = frame_buffer.Y_comp_pitch;
        pitch_c = frame_buffer.U_comp_pitch;

        frame_buffer.frame_p_c_n[curr_index].isCorrupted = false;

        for (Ipp32u i = 0; i < threadsNum; i += 1)
        {
            VideoContext* video = Video[task_num][i];

            video->Y_comp_pitch = pitch_l;
            video->U_comp_pitch = pitch_c;
            video->V_comp_pitch = pitch_c;

            video->Y_comp_height = out_data->GetHeight();

            video->pic_size = sequenceHeader.mb_height[task_num]*16*pitch_l;

            video->blkOffsets[0][0] = 0;
            video->blkOffsets[0][1] = 8;
            video->blkOffsets[0][2] = 8*pitch_l;
            video->blkOffsets[0][3] = 8*pitch_l + 8;
            video->blkOffsets[0][4] = 0;
            video->blkOffsets[0][5] = 0;
            video->blkOffsets[0][6] = 8*pitch_c;
            video->blkOffsets[0][7] = 8*pitch_c;
            video->blkOffsets[1][0] = 0;
            video->blkOffsets[1][1] = 8;
            video->blkOffsets[1][2] = pitch_l;
            video->blkOffsets[1][3] = pitch_l + 8;
            video->blkOffsets[1][4] = 0;
            video->blkOffsets[1][5] = 0;
            video->blkOffsets[1][6] = pitch_c;
            video->blkOffsets[1][7] = pitch_c;
            video->blkOffsets[2][0] = 0;
            video->blkOffsets[2][1] = 8;
            video->blkOffsets[2][2] = 16*pitch_l;
            video->blkOffsets[2][3] = 16*pitch_l + 8;
            video->blkOffsets[2][4] = 0;
            video->blkOffsets[2][5] = 0;
            video->blkOffsets[2][6] = 16*pitch_c;
            video->blkOffsets[2][7] = 16*pitch_c;

            video->blkPitches[0][0] = pitch_l;
            video->blkPitches[0][1] = pitch_c;
            video->blkPitches[1][0] = 2*pitch_l;
            video->blkPitches[1][1] = pitch_c;
            video->blkPitches[2][0] = 2*pitch_l;
            video->blkPitches[2][1] = 2*pitch_c;
        }

        return;
    }
    else
    {
        if(curr_index >= DPB_SIZE)
            return;
        frame_buffer.frame_p_c_n[curr_index].Y_comp_data = (Ipp8u*)out_data->GetPlanePointer(0);
        frame_buffer.frame_p_c_n[curr_index].U_comp_data = (Ipp8u*)out_data->GetPlanePointer(1);
        frame_buffer.frame_p_c_n[curr_index].V_comp_data = (Ipp8u*)out_data->GetPlanePointer(2);
      //  frame_buffer.frame_p_c_n[curr_index].IsUserDataDecoded = false;
      //  frame_buffer.frame_p_c_n[curr_index].us_data_size = 0;
        pitch_l = (Ipp32s)out_data->GetPlanePitch(0);
        pitch_c = pitch_l >> 1;
        frame_buffer.Y_comp_pitch = pitch_l;
        frame_buffer.U_comp_pitch = pitch_c;
        frame_buffer.V_comp_pitch = pitch_c;
        frame_buffer.Y_comp_height = out_data->GetHeight();
        Ipp32s l_size, c_size;
        //frame_buffer.pic_size = (frame_buffer.Y_comp_pitch*frame_buffer.Y_comp_height*3)/2;
        frame_buffer.pic_size = l_size = sequenceHeader.mb_height[task_num]*16*pitch_l;

// ------------------------------------------------

        for (Ipp32u i = 0; i < threadsNum; i += 1)
        {
            VideoContext* video = Video[task_num][i];

            video->Y_comp_pitch = pitch_l;
            video->U_comp_pitch = pitch_c;
            video->V_comp_pitch = pitch_c;
            video->Y_comp_height = out_data->GetHeight();

            video->pic_size = sequenceHeader.mb_height[task_num]*16*pitch_l;
        }
// ------------------------------------------------

        c_size = sequenceHeader.mb_height[task_num]*8*pitch_c;
        if (m_ClipInfo.color_format == YUV422)
            c_size *= 2;
        else if(m_ClipInfo.color_format == YUV444)
            c_size = l_size;

        memset(frame_buffer.frame_p_c_n[curr_index].Y_comp_data,0,l_size);
        memset(frame_buffer.frame_p_c_n[curr_index].U_comp_data,0,c_size);
        memset(frame_buffer.frame_p_c_n[curr_index].V_comp_data,0,c_size);
    }


// ------------------------------------------------

    for (Ipp32u i = 0; i < threadsNum; i += 1)
    {
        VideoContext* video = Video[task_num][i];

        video->blkOffsets[0][0] = 0;
        video->blkOffsets[0][1] = 8;
        video->blkOffsets[0][2] = 8*pitch_l;
        video->blkOffsets[0][3] = 8*pitch_l + 8;
        video->blkOffsets[0][4] = 0;
        video->blkOffsets[0][5] = 0;
        video->blkOffsets[0][6] = 8*pitch_c;
        video->blkOffsets[0][7] = 8*pitch_c;
        video->blkOffsets[1][0] = 0;
        video->blkOffsets[1][1] = 8;
        video->blkOffsets[1][2] = pitch_l;
        video->blkOffsets[1][3] = pitch_l + 8;
        video->blkOffsets[1][4] = 0;
        video->blkOffsets[1][5] = 0;
        video->blkOffsets[1][6] = pitch_c;
        video->blkOffsets[1][7] = pitch_c;
        video->blkOffsets[2][0] = 0;
        video->blkOffsets[2][1] = 8;
        video->blkOffsets[2][2] = 16*pitch_l;
        video->blkOffsets[2][3] = 16*pitch_l + 8;
        video->blkOffsets[2][4] = 0;
        video->blkOffsets[2][5] = 0;
        video->blkOffsets[2][6] = 16*pitch_c;
        video->blkOffsets[2][7] = 16*pitch_c;

        video->blkPitches[0][0] = pitch_l;
        video->blkPitches[0][1] = pitch_c;
        video->blkPitches[1][0] = 2*pitch_l;
        video->blkPitches[1][1] = pitch_c;
        video->blkPitches[2][0] = 2*pitch_l;
        video->blkPitches[2][1] = 2*pitch_c;
    }
}

void MPEG2VideoDecoderSW::quant_matrix_extension(int task_num)
{
    Ipp32s i;
    Ipp32u code;
    VideoContext* video = Video[task_num][0];
    Ipp32s load_intra_quantizer_matrix, load_non_intra_quantizer_matrix, load_chroma_intra_quantizer_matrix, load_chroma_non_intra_quantizer_matrix;
    Ipp8u q_matrix[4][64];

    GET_TO9BITS(video->bs, 4 ,code)
    GET_1BIT(video->bs,load_intra_quantizer_matrix)
    if(load_intra_quantizer_matrix)
    {
        for(i= 0; i < 64; i++) {
            GET_BITS(video->bs, 8, code);
            q_matrix[0][i] = (Ipp8u)code;
        }
        for (i = 0; i < m_nNumberOfThreads; i++) {
          ippiDecodeIntraInit_MPEG2(q_matrix[0], IPPVC_LEAVE_SCAN_UNCHANGED, PictureHeader[task_num].intra_vlc_format, PictureHeader[task_num].curr_intra_dc_multi, &m_Spec.decodeIntraSpec);
        }
        m_Spec.flag = IPPVC_LEAVE_SCAN_UNCHANGED;
    }

    GET_1BIT(video->bs,load_non_intra_quantizer_matrix)
    if(load_non_intra_quantizer_matrix)
    {
        for(i= 0; i < 64; i++) {
            GET_BITS(video->bs, 8, code);
            q_matrix[1][i] = (Ipp8u)code;
        }
        for (i = 0; i < m_nNumberOfThreads; i++) {
          ippiDecodeInterInit_MPEG2(q_matrix[1], IPPVC_LEAVE_SCAN_UNCHANGED, &m_Spec.decodeInterSpec);
        }
        m_Spec.flag = IPPVC_LEAVE_SCAN_UNCHANGED;
    }

    GET_1BIT(video->bs,load_chroma_intra_quantizer_matrix);
    if(load_chroma_intra_quantizer_matrix && m_ClipInfo.color_format != YUV420)
    {
        for(i= 0; i < 64; i++) {
            GET_TO9BITS(video->bs, 8, code);
            q_matrix[2][i] = (Ipp8u)code;
        }
        for (i = 0; i < m_nNumberOfThreads; i++) {
            ippiDecodeIntraInit_MPEG2(q_matrix[2], IPPVC_LEAVE_SCAN_UNCHANGED, PictureHeader[task_num].intra_vlc_format, PictureHeader[task_num].curr_intra_dc_multi, &m_Spec.decodeIntraSpecChroma);
        }
        m_Spec.flag = IPPVC_LEAVE_SCAN_UNCHANGED;
    }
    else
    {
        for (i = 0; i < m_nNumberOfThreads; i++) {
            m_Spec.decodeIntraSpecChroma = m_Spec.decodeIntraSpec;
        }
    }

    GET_1BIT(video->bs,load_chroma_non_intra_quantizer_matrix);
    if(load_chroma_non_intra_quantizer_matrix && m_ClipInfo.color_format != YUV420)
    {
        for(i= 0; i < 64; i++) {
            GET_TO9BITS(video->bs, 8, code);
            q_matrix[2][i] = (Ipp8u)code;
        }
        for (i = 0; i < m_nNumberOfThreads; i++) {
            ippiDecodeInterInit_MPEG2(q_matrix[3], IPPVC_LEAVE_SCAN_UNCHANGED, &m_Spec.decodeInterSpecChroma);
        }
        m_Spec.flag = IPPVC_LEAVE_SCAN_UNCHANGED;
    }
    else
    {
        for (i = 0; i < m_nNumberOfThreads; i++) {
            m_Spec.decodeInterSpecChroma = m_Spec.decodeInterSpec;
        }
    }

} //void quant_matrix_extension()

Status MPEG2VideoDecoderSW::ProcessRestFrame(int task_num)
{
    for (Ipp32s i = 0; i < m_nNumberOfThreads; i++)
    {
        int j;

        //Intra
        Video[task_num][i]->decodeIntraSpec.intraVLCFormat = m_Spec.decodeIntraSpec.intraVLCFormat;
        Video[task_num][i]->decodeIntraSpec.intraShiftDC = m_Spec.decodeIntraSpec.intraShiftDC;

        Video[task_num][i]->stream_type = m_ClipInfo.stream_type;
        Video[task_num][i]->color_format = m_ClipInfo.color_format;
        Video[task_num][i]->clip_info_width = m_ClipInfo.clip_info.width;
        Video[task_num][i]->clip_info_height = m_ClipInfo.clip_info.height;

        for (j = 0; j < 64; j++)
            Video[task_num][i]->decodeIntraSpec._quantMatrix[j] = m_Spec.decodeIntraSpec._quantMatrix[j];

        if (m_Spec.decodeIntraSpec.quantMatrix)
        {
            Video[task_num][i]->decodeIntraSpec.quantMatrix = Video[task_num][i]->decodeIntraSpec._quantMatrix;
        }
        else
        {
            Video[task_num][i]->decodeIntraSpec.quantMatrix = NULL;
        }

        Video[task_num][i]->decodeIntraSpec.scanMatrix = m_Spec.decodeIntraSpec.scanMatrix;

        //IntraChroma
        Video[task_num][i]->decodeIntraSpecChroma.intraVLCFormat = m_Spec.decodeIntraSpecChroma.intraVLCFormat;
        Video[task_num][i]->decodeIntraSpecChroma.intraShiftDC = m_Spec.decodeIntraSpecChroma.intraShiftDC;

        for (j = 0; j < 64; j++)
            Video[task_num][i]->decodeIntraSpecChroma._quantMatrix[j] = m_Spec.decodeIntraSpecChroma._quantMatrix[j];

        if (m_Spec.decodeIntraSpecChroma.quantMatrix)
        {
            Video[task_num][i]->decodeIntraSpecChroma.quantMatrix = Video[task_num][i]->decodeIntraSpecChroma._quantMatrix;
        }
        else
        {
            Video[task_num][i]->decodeIntraSpecChroma.quantMatrix = NULL;
        }

        Video[task_num][i]->decodeIntraSpecChroma.scanMatrix = m_Spec.decodeIntraSpecChroma.scanMatrix;

        //Inter
        for (j = 0; j < 64; j++)
            Video[task_num][i]->decodeInterSpec._quantMatrix[j] = m_Spec.decodeInterSpec._quantMatrix[j];

        if (m_Spec.decodeInterSpec.quantMatrix)
        {
            Video[task_num][i]->decodeInterSpec.quantMatrix = Video[task_num][i]->decodeInterSpec._quantMatrix;
        }
        else
        {
            Video[task_num][i]->decodeInterSpec.quantMatrix = NULL;
        }

        Video[task_num][i]->decodeInterSpec.scanMatrix = m_Spec.decodeInterSpec.scanMatrix;

        //InterChroma
        for (j = 0; j < 64; j++)
            Video[task_num][i]->decodeInterSpecChroma._quantMatrix[j] = m_Spec.decodeInterSpecChroma._quantMatrix[j];

        if (m_Spec.decodeInterSpecChroma.quantMatrix)
        {
            Video[task_num][i]->decodeInterSpecChroma.quantMatrix = Video[task_num][i]->decodeInterSpecChroma._quantMatrix;
        }
        else
        {
            Video[task_num][i]->decodeInterSpecChroma.quantMatrix = NULL;
        }

        Video[task_num][i]->decodeInterSpecChroma.scanMatrix = m_Spec.decodeInterSpecChroma.scanMatrix;

    } // for

    return MPEG2VideoDecoderBase::ProcessRestFrame(task_num);
}

void MPEG2VideoDecoderSW::OnDecodePicHeaderEx(int task_num)
{
    for (Ipp32s i = 0; i < m_nNumberOfThreads; i++) {
        Ipp32s flag = PictureHeader[task_num].alternate_scan | IPPVC_LEAVE_QUANT_UNCHANGED;
        ippiDecodeInterInit_MPEG2(NULL, flag, &m_Spec.decodeInterSpec);
        m_Spec.decodeIntraSpec.intraVLCFormat = PictureHeader[task_num].intra_vlc_format;
        m_Spec.decodeIntraSpec.intraShiftDC = PictureHeader[task_num].curr_intra_dc_multi;
        ippiDecodeIntraInit_MPEG2(NULL, flag, m_Spec.decodeIntraSpec.intraVLCFormat, m_Spec.decodeIntraSpec.intraShiftDC, &m_Spec.decodeIntraSpec);

        ippiDecodeInterInit_MPEG2(NULL, flag, &m_Spec.decodeInterSpecChroma);
        ippiDecodeIntraInit_MPEG2(NULL, flag, m_Spec.decodeIntraSpec.intraVLCFormat, m_Spec.decodeIntraSpec.intraShiftDC, &m_Spec.decodeIntraSpecChroma);
        m_Spec.flag = flag;
    }
}

Status MPEG2VideoDecoderSW::UpdateFrameBuffer(int task_num, Ipp8u* iqm, Ipp8u*niqm)
{
      Ipp32s pitch_l, pitch_c;
      Ipp32s height_l, height_c;
      Ipp32s size_l, size_c;

      pitch_l = align_value<Ipp32s>(sequenceHeader.mb_width[task_num]*16, ALIGN_VALUE);
      height_l = align_value<Ipp32s>(sequenceHeader.mb_height[task_num]*16, ALIGN_VALUE);
      size_l = height_l*pitch_l;
      if (m_ClipInfo.color_format != YUV444) {

        pitch_c = pitch_l >> 1;
        height_c = height_l >> 1;
        size_c = height_c*pitch_c;
        if (m_ClipInfo.color_format == YUV422)
          size_c *= 2;
      } else {
        pitch_c = pitch_l;
        size_c = size_l;
      }

      frame_buffer.Y_comp_pitch = pitch_l;
      frame_buffer.U_comp_pitch = pitch_c;
      frame_buffer.V_comp_pitch = pitch_c;
      frame_buffer.pic_size = size_l;

      Ipp32u threadsNum = GetCurrThreadsNum(task_num);

    for (Ipp32u i = 0; i < threadsNum; i += 1)
    {
        VideoContext* video = Video[task_num][i];

        video->Y_comp_pitch = pitch_l;
        video->U_comp_pitch = pitch_c;
        video->V_comp_pitch = pitch_c;
        video->pic_size = size_l;
    }

  for (Ipp32u i = 0; i < threadsNum; i += 1)
    {
      VideoContext* video = Video[task_num][i];

      video->blkOffsets[0][0] = 0;
      video->blkOffsets[0][1] = 8;
      video->blkOffsets[0][2] = 8*pitch_l;
      video->blkOffsets[0][3] = 8*pitch_l + 8;
      video->blkOffsets[0][4] = 0;
      video->blkOffsets[0][5] = 0;
      video->blkOffsets[0][6] = 8*pitch_c;
      video->blkOffsets[0][7] = 8*pitch_c;
      video->blkOffsets[1][0] = 0;
      video->blkOffsets[1][1] = 8;
      video->blkOffsets[1][2] = pitch_l;
      video->blkOffsets[1][3] = pitch_l + 8;
      video->blkOffsets[1][4] = 0;
      video->blkOffsets[1][5] = 0;
      video->blkOffsets[1][6] = pitch_c;
      video->blkOffsets[1][7] = pitch_c;
      video->blkOffsets[2][0] = 0;
      video->blkOffsets[2][1] = 8;
      video->blkOffsets[2][2] = 16*pitch_l;
      video->blkOffsets[2][3] = 16*pitch_l + 8;
      video->blkOffsets[2][4] = 0;
      video->blkOffsets[2][5] = 0;
      video->blkOffsets[2][6] = 16*pitch_c;
      video->blkOffsets[2][7] = 16*pitch_c;

      video->blkPitches[0][0] = pitch_l;
      video->blkPitches[0][1] = pitch_c;
      video->blkPitches[1][0] = 2*pitch_l;
      video->blkPitches[1][1] = pitch_c;
      if (video->color_format != YUV420) video->blkPitches[1][1] = 2 * pitch_c;
      video->blkPitches[2][0] = 2*pitch_l;
      video->blkPitches[2][1] = 2*pitch_c;

    }

  frame_buffer.allocated_mb_width = sequenceHeader.mb_width[task_num];
  frame_buffer.allocated_mb_height = sequenceHeader.mb_height[task_num];
  frame_buffer.allocated_cformat = m_ClipInfo.color_format;

  // Alloc frames

    Ipp32s flag_mpeg1 = (m_ClipInfo.stream_type == MPEG1_VIDEO) ? IPPVC_MPEG1_STREAM : 0;
    ippiDecodeIntraInit_MPEG2(NULL, flag_mpeg1, PictureHeader[task_num].intra_vlc_format, PictureHeader[task_num].curr_intra_dc_multi, &m_Spec.decodeIntraSpec);
    ippiDecodeInterInit_MPEG2(NULL, flag_mpeg1, &m_Spec.decodeInterSpec);

    m_Spec.decodeInterSpecChroma = m_Spec.decodeInterSpec;
    m_Spec.decodeIntraSpecChroma = m_Spec.decodeIntraSpec;
    m_Spec.flag = flag_mpeg1;

    if (iqm)
    {
        ippiDecodeIntraInit_MPEG2(iqm, flag_mpeg1, PictureHeader[task_num].intra_vlc_format, PictureHeader[task_num].curr_intra_dc_multi, &m_Spec.decodeIntraSpec);
        m_Spec.decodeIntraSpecChroma = m_Spec.decodeIntraSpec;
    }

    if (niqm)
    {
        ippiDecodeInterInit_MPEG2(niqm, flag_mpeg1, &m_Spec.decodeInterSpec);
        m_Spec.decodeInterSpecChroma = m_Spec.decodeInterSpec;
    }

    m_Spec.flag = flag_mpeg1;
  
    return UMC_OK;
}

Status MPEG2VideoDecoderSW::ThreadingSetup(Ipp32s maxThreads)
{
    memset(&m_Spec.decodeIntraSpec, 0, sizeof(IppiDecodeIntraSpec_MPEG2));
    memset(&m_Spec.decodeInterSpec, 0, sizeof(IppiDecodeInterSpec_MPEG2));
    ippiDecodeInterInit_MPEG2(NULL, IPPVC_MPEG1_STREAM, &m_Spec.decodeInterSpec);
    m_Spec.decodeInterSpecChroma = m_Spec.decodeInterSpec;
    m_Spec.decodeInterSpec.idxLastNonZero = 63;
    m_Spec.decodeIntraSpec.intraVLCFormat = PictureHeader[0].intra_vlc_format;
    m_Spec.decodeIntraSpec.intraShiftDC = PictureHeader[0].curr_intra_dc_multi;

    Status sts = MPEG2VideoDecoderBase::ThreadingSetup(maxThreads);
    if (sts != UMC_OK)
        return sts;

    for(int j = 0; j < 2*DPB_SIZE; j++)
    {
        for(int i = 0; i < m_nNumberOfThreads; i++)
        {
          // Intra&inter spec
          memset(&Video[j][i]->decodeIntraSpec, 0, sizeof(IppiDecodeIntraSpec_MPEG2));
          memset(&Video[j][i]->decodeInterSpec, 0, sizeof(IppiDecodeInterSpec_MPEG2));
          ippiDecodeInterInit_MPEG2(NULL, IPPVC_MPEG1_STREAM, &Video[j][i]->decodeInterSpec);
          Video[j][i]->decodeInterSpecChroma = Video[j][i]->decodeInterSpec;
          Video[j][i]->decodeInterSpec.idxLastNonZero = 63;
          Video[j][i]->decodeIntraSpec.intraVLCFormat = PictureHeader[0].intra_vlc_format;
          Video[j][i]->decodeIntraSpec.intraShiftDC = PictureHeader[0].curr_intra_dc_multi;
        }
    }

    return UMC_OK;
}

#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
