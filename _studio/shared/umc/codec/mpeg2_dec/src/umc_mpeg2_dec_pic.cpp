/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2014 Intel Corporation. All Rights Reserved.
//
*/

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#include "umc_mpeg2_dec_defs.h"
#include "umc_mpeg2_dec_base.h"
#include "umc_video_data.h"
#include "umc_structures.h"

#if !(defined(_WIN32) || defined(_WIN64) || defined(__APPLE__))
#include <malloc.h>
#endif

#include "mfx_trace.h"
#include "mfx_common_int.h"

#pragma warning(disable: 4244)

#if defined (__ICL)
// integer conversion resulted in truncation
//#pragma warning(disable:69)
#endif

using namespace UMC;

//extern Ipp8u default_intra_quantizer_matrix[64];
static const Ipp8u default_intra_quantizer_matrix[64] =
{
     8, 16, 16, 19, 16, 19, 22, 22,
    22, 22, 22, 22, 26, 24, 26, 27,
    27, 27, 26, 26, 26, 26, 27, 27,
    27, 29, 29, 29, 34, 34, 34, 29,
    29, 29, 27, 27, 29, 29, 32, 32,
    34, 34, 37, 38, 37, 35, 35, 34,
    35, 38, 38, 40, 40, 40, 48, 48,
    46, 46, 56, 56, 58, 69, 69, 83
};

// can decode all headers, except of slices
// bitstream should point to next after header byte
Status MPEG2VideoDecoderBase::DecodeHeader(Ipp32s startcode, IppVideoContext* video, int task_num)
{
  Ipp32u code;
  //m_IsUserDataPresent = false;
  bool prev_of_m_FirstHeaderAfterSequenceHeaderParsed = m_FirstHeaderAfterSequenceHeaderParsed;
  if (sequenceHeader.is_decoded && startcode != SEQUENCE_HEADER_CODE)
  {
     m_FirstHeaderAfterSequenceHeaderParsed = true;
  }

  switch(startcode) {
    case SEQUENCE_HEADER_CODE:
      m_ClipInfo.stream_type = UNDEF_VIDEO;//possible to be changed (sarnoff streams)
      m_IsSHDecoded = true;
      return (DecodeSequenceHeader(video,task_num));
    case SEQUENCE_END_CODE:
      return (UMC_OK);
    case GROUP_START_CODE:
      GET_BITS_LONG(video->bs, 27, code)
      sequenceHeader.broken_link = code & 1;
      sequenceHeader.closed_gop  = code & 2;
      sequenceHeader.time_code.gop_picture = (code>>2) & 0x3f;
      sequenceHeader.time_code.gop_seconds = (code>>8) & 0x3f;
      sequenceHeader.time_code.gop_minutes = (code>>15) & 0x3f;
      sequenceHeader.time_code.gop_hours   = (code>>21) & 0x1f;
      sequenceHeader.time_code.gop_drop_frame_flag = (code>>26) & 1;
      sequenceHeader.stream_time_temporal_reference = -1; //-2; // new count
      return (UMC_OK); // ignore for a while
    case EXTENSION_START_CODE:
      SHOW_TO9BITS(video->bs, 4, code)
      //extensions
      switch(code) {
        case SEQUENCE_EXTENSION_ID:
          {
            if (sequenceHeader.is_decoded && prev_of_m_FirstHeaderAfterSequenceHeaderParsed)
            {
                return UMC_ERR_INVALID_STREAM;
            }

            Ipp32u bit_rate_extension, chroma_format;
            GET_BITS32(video->bs, code)
            sequenceHeader.profile                          = (code >> 24) & 0x7;//ignore escape bit
            sequenceHeader.level                            = (code >> 20) & 0xf;
            sequenceHeader.progressive_sequence             = (code >> 19) & 1;
            chroma_format               = 1 << ((code >> 17) & ((1 << 2) - 1));
            m_ClipInfo.clip_info.width  = (m_ClipInfo.clip_info.width  & 0xfff)
              | ((code >> (15-12)) & 0x3000);
            m_ClipInfo.clip_info.height = (m_ClipInfo.clip_info.height & 0xfff)
              | ((code >> (13-12)) & 0x3000);
            bit_rate_extension = (code >> 1) & 0xfff;

            switch(sequenceHeader.level)
            {
                case  4:
                case  6:
                case  8:
                case 10:
                    break;
                default:
                    return UMC_ERR_INVALID_STREAM;
            }

            GET_BITS(video->bs, 16, code)

            if (false == m_isFrameRateFromInit)
            {
                sequenceHeader.frame_rate_extension_d           = code & 31;
                sequenceHeader.frame_rate_extension_n           = (code >> 5) & 3;
            }

            switch (chroma_format)
            {
            case 2:
                m_ClipInfo.color_format = YUV420;
                break;
            case 4:
                m_ClipInfo.color_format = YUV422;
                break;
            case 8:
                m_ClipInfo.color_format = YUV444;
                break;
            default:
                break;
            }

            if(bit_rate_extension != 0) {
              m_ClipInfo.bitrate = (m_ClipInfo.bitrate / 400) & 0x3ffff;
              m_ClipInfo.bitrate |= (bit_rate_extension<<18);
              if(m_ClipInfo.bitrate >= 0x40000000/100) // check if fit to 32u
                m_ClipInfo.bitrate = 0xffffffff;
              else
                m_ClipInfo.bitrate *= 400;
            }
            sequenceHeader.extension_start_code_ID[task_num]          = SEQUENCE_EXTENSION_ID; //needed?
            sequenceHeader.height = m_ClipInfo.clip_info.height;
            sequenceHeader.width = m_ClipInfo.clip_info.width;
            sequenceHeader.chroma_format = chroma_format;
          }
          break;
        case SEQUENCE_DISPLAY_EXTENSION_ID:
          sequence_display_extension(task_num);
          break;
        case QUANT_MATRIX_EXTENSION_ID:
          quant_matrix_extension(task_num);
          break;
        case COPYRIGHT_EXTENSION_ID:
          copyright_extension(task_num);
          break;
        case PICTURE_DISPLAY_EXTENSION_ID:
          picture_display_extension(task_num);
          break;
        case SEQUENCE_SCALABLE_EXTENSION_ID:
          sequence_scalable_extension(task_num);
          break;
        case PICTURE_CODING_EXTENSION_ID: // can move after picture header (if mpeg2)
          //picture_coding_extension();
          {
            GET_BITS32(video->bs,code)
            VM_ASSERT((code>>28) == PICTURE_CODING_EXTENSION_ID);
            PictureHeader[task_num].repeat_first_field           = (code >> 1) & 1;
            PictureHeader[task_num].alternate_scan               = (code >> 2) & 1;
            PictureHeader[task_num].intra_vlc_format             = (code >> 3) & 1;
            PictureHeader[task_num].q_scale_type                 = (code >> 4) & 1;
            PictureHeader[task_num].concealment_motion_vectors   = (code >> 5) & 1;
            PictureHeader[task_num].frame_pred_frame_dct         = (code >> 6) & 1;
            PictureHeader[task_num].top_field_first              = (code >> 7) & 1;
            PictureHeader[task_num].picture_structure            = (code >> 8) & 3;
            PictureHeader[task_num].intra_dc_precision           = (code >> 10) & 3;
            PictureHeader[task_num].f_code[3]                    = (code >> 12) & 15;
            PictureHeader[task_num].f_code[2]                    = (code >> 16) & 15;
            PictureHeader[task_num].f_code[1]                    = (code >> 20) & 15;
            PictureHeader[task_num].f_code[0]                    = (code >> 24) & 15;

            GET_1BIT(video->bs, PictureHeader[task_num].progressive_frame)
            GET_1BIT(video->bs, code)
            PictureHeader[task_num].r_size[0] = PictureHeader[task_num].f_code[0] - 1;
            PictureHeader[task_num].r_size[1] = PictureHeader[task_num].f_code[1] - 1;
            PictureHeader[task_num].r_size[2] = PictureHeader[task_num].f_code[2] - 1;
            PictureHeader[task_num].r_size[3] = PictureHeader[task_num].f_code[3] - 1;

            PictureHeader[task_num].curr_intra_dc_multi = intra_dc_multi[PictureHeader[task_num].intra_dc_precision];
            PictureHeader[task_num].curr_reset_dc          = reset_dc[PictureHeader[task_num].intra_dc_precision];
            Ipp32s i, f;
            for(i = 0; i < 4; i++) {
                f = 1 << PictureHeader[task_num].r_size[i];
                PictureHeader[task_num].low_in_range[i] = -(f * 16);
                PictureHeader[task_num].high_in_range[i] = (f * 16) - 1;
                PictureHeader[task_num].range[i] = f * 32;
            }
            if(code)
            {
                GET_BITS_LONG(video->bs, 20, code)
            }
            if(PictureHeader[task_num].picture_structure == FRAME_PICTURE) {
              frame_buffer.field_buffer_index[task_num] = 0; // avoid field parity loss in resync
            }

            for (i = 0; i < m_nNumberOfThreads; i++) {
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
          break;
        case PICTURE_SPARTIAL_SCALABLE_EXTENSION_ID:
          picture_spartial_scalable_extension(task_num);
          break;
        case PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID:
          picture_temporal_scalable_extension(task_num);
          break;
        default:
          //VM_ASSERT(0);
          break;
      }
      return (UMC_OK);
    case USER_DATA_START_CODE:
      {
        //Ipp32s i;
        //Ipp32s ATSC_identifier;
        //SHOW_BITS_32(video->bs, ATSC_identifier)
        ////Closed caption data
        //if(ATSC_identifier == 0x47413934)
        //{
        //    //ATSC_identifier       32 bslbf = 0x47413934
        //    SKIP_BITS_32(video->bs)
        //    //user_data_type_code   8 uimsbf
        //    GET_TO9BITS(video->bs, 8, code)

        //    if (code == 0x03)
        //    {
        //        Ipp32s cc_count;
        //        Ipp32s additional_data_flag;
        //        //process_em_data_flag  1 bslbf
        //        //process_cc_data_flag  1 bslbf
        //        //additional_data_flag  1 bslbf
        //        //cc_count              5 uimsbf
        //        GET_TO9BITS(video->bs, 8, cc_count)
        //        additional_data_flag = cc_count&0x20;
        //        cc_count &= 0x1F;
        //        //em_data               8 bslbf
        //        GET_TO9BITS(video->bs, 8, code)

        //        //TBD save and output DTV closed caption data
        //        for (i = 0 ; i < cc_count ; i++ )
        //        {
        //            //marker_bits   5 1111 1
        //            //cc_valid      1 bslbf
        //            //cc_type       2 bslbf
        //            GET_TO9BITS(video->bs, 8, code)
        //            //cc_data_1     8 bslbf
        //            GET_TO9BITS(video->bs, 8, code)
        //            //cc_data_2     8 bslbf
        //            GET_TO9BITS(video->bs, 8, code)
        //        }

        //        //marker_bits 8 1111 1111
        //        GET_TO9BITS(video->bs, 8, code)
        //        VM_ASSERT(code == 0xff);

        //        if (additional_data_flag)
        //        {
        //            //additional_user_data 8
        //            SHOW_BITS_LONG(video->bs,24,code)
        //            while(code != 0x000001)
        //            {
        //                GET_TO9BITS(video->bs, 8, code)
        //                SHOW_BITS_LONG(video->bs,24,code)
        //            }
        //        }
        //    }
        //    else
        //    {
        //        //ATSC_reserved_user_data 8
        //        SHOW_BITS_LONG(video->bs,24,code)
        //        while(code != 0x000001)
        //        {
        //            GET_TO9BITS(video->bs, 8, code)
        //            SHOW_BITS_LONG(video->bs,24,code)
        //        }
        //    }
        //}
        //else
        //m_IsUserDataPresent = true;
        //if(m_pCCData /*&& gop_flag*/)
        {
            ReadCCData(task_num);
        }
        /*
        else
        {
            //other user info, skip it
            SHOW_BITS_LONG(video->bs,24,code)
            while(code != 0x000001)
            {
                GET_TO9BITS(video->bs, 8, code)
                SHOW_BITS_LONG(video->bs,24,code)
            }
        }*/
      }
      return (UMC_OK);
    case PICTURE_START_CODE:
      return (DecodePictureHeader(task_num));
    default:
      return (UMC_ERR_INVALID_STREAM); // unexpected
  }

}

//Sequence Header search. Stops after header start code
Status MPEG2VideoDecoderBase::FindSequenceHeader(IppVideoContext *video)
{
  Ipp32u code;
  do {
    GET_START_CODE(video->bs, code);
    if(SEQUENCE_HEADER_CODE == code)
      return (UMC_OK);
  } while ( code != (Ipp32u)UMC_ERR_NOT_ENOUGH_DATA );

  return UMC_ERR_NOT_ENOUGH_DATA;
}

// when video == 0 uses m_ClipInfo values

enum {
    FIXED_SEQ_HEADER_EXT_SIZE   = 21,
};

Status MPEG2VideoDecoderBase::DecodeSequenceHeader(IppVideoContext* video, int task_num)
{
    Ipp32u load_non_intra_quantizer_matrix = 0;
    Ipp32u load_intra_quantizer_matrix = 0;
    Ipp32u constrained_parameters_flag;
    Ipp32u frame_rate_code = 0, dar_code = 0;
    Ipp32s i;
    Ipp32u code;
    Ipp8u  iqm[64];
    Ipp8u  niqm[64];


    if (0 != video)
    {
        if(GET_REMAINED_BYTES(video->bs) < 8)
        {
            // return header back
            UNGET_BITS_32(video->bs)
            return (UMC_ERR_NOT_ENOUGH_DATA);
        }
        video->bs_sequence_header_start = video->bs_curr_ptr;
        m_FirstHeaderAfterSequenceHeaderParsed = false;
        GET_BITS32(video->bs, code)
        frame_rate_code = code  & ((1 << 4) - 1);
        m_ClipInfo.clip_info.height    = (code >> 8) & ((1 << 12) - 1);
        m_ClipInfo.clip_info.width     = (code >> 20) & ((1 << 12) - 1);
        sequenceHeader.frame_count = 0;

        // this workaround for initialization (m_InitClipInfo not yet filled)
        if((!!m_InitClipInfo.clip_info.height) && (!!m_InitClipInfo.clip_info.width))
        {
            if(m_InitClipInfo.clip_info.height < m_ClipInfo.clip_info.height)
                return UMC_ERR_INVALID_PARAMS;
            if(m_InitClipInfo.clip_info.width < m_ClipInfo.clip_info.width)
                return UMC_ERR_INVALID_PARAMS;
        }

        dar_code = (code >> 4) & ((1 << 4) - 1);

        GET_BITS32(video->bs, code)
        m_ClipInfo.bitrate                 = (code >> 14) * 400;
        constrained_parameters_flag        = (code >> 2)  & 1;
        load_intra_quantizer_matrix        = (code >> 1)  & 1;
        load_non_intra_quantizer_matrix    = code  & 1; // could be changed

        if (load_intra_quantizer_matrix)
        {

            GET_BITS(video->bs, 7, code)
            code |= (load_non_intra_quantizer_matrix << 7);
            iqm[0] = (Ipp8u)code;

            for (i = 1; i < 64; i++)
            {
                GET_BITS(video->bs, 8, code);
                iqm[i] = (Ipp8u)code;
            }

            GET_1BIT(video->bs, load_non_intra_quantizer_matrix);
        }

        if (load_non_intra_quantizer_matrix)
        {
            for (i = 0; i < 64; i++)
            {
                GET_BITS(video->bs, 8, code);
                niqm[i] = (Ipp8u)code;
            }
        }

        // calculate size of sequence header
        shMask.memSize = (Ipp16u) (video->bs_sequence_header_start - video->bs_start_ptr);

        // init signal info structure members by default values
        m_signalInfo.VideoFormat = 5;
        m_signalInfo.ColourDescriptionPresent = 0;
        m_signalInfo.ColourPrimaries = 1;
        m_signalInfo.TransferCharacteristics = 1;
        m_signalInfo.MatrixCoefficients = 1;

        VideoStreamType newtype = MPEG1_VIDEO;

        if (!constrained_parameters_flag)
        {
            FIND_START_CODE(video->bs,code)
            if (EXTENSION_START_CODE == code)
            {
                SKIP_BITS_32(video->bs);
                SHOW_TO9BITS(video->bs, 4, code)

                if (code == SEQUENCE_EXTENSION_ID)
                {
                    newtype = MPEG2_VIDEO;

                    DecodeHeader(EXTENSION_START_CODE, video, task_num);
                    shMask.memSize += (Ipp16u) (video->bs_curr_ptr - (video->bs_sequence_header_start + shMask.memSize));
                }
            }
        }

#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)
        if (newtype == MPEG1_VIDEO)
            return UMC_ERR_UNSUPPORTED;
#endif

        if (UNDEF_VIDEO == m_ClipInfo.stream_type)
        {
            m_ClipInfo.stream_type = newtype;
        }
        else if(m_ClipInfo.stream_type != newtype)
        {
            return (UMC_ERR_INVALID_STREAM);
        }

        FIND_START_CODE(video->bs, code);

        while (EXTENSION_START_CODE == code || USER_DATA_START_CODE == code)
        {
            SKIP_BITS_32(video->bs);

            Ipp32u ext_name;
            SHOW_TO9BITS(video->bs, 4, ext_name)

            DecodeHeader(code, video, task_num);
            FIND_START_CODE(video->bs, code);

            if (SEQUENCE_DISPLAY_EXTENSION_ID == ext_name)
            {
                shMask.memSize += (Ipp16u) (video->bs_curr_ptr - (video->bs_sequence_header_start + shMask.memSize));
            }
        }
    }

#if defined UMC_VA_DXVA
    if(pack_w.va_mode == VA_VLD_W) {
      pack_w.QmatrixData.bNewQmatrix[0] = pack_w.QmatrixData.bNewQmatrix[2] = 1;
      if(load_intra_quantizer_matrix) {
        for(i=0; i<64; i++) {
          pack_w.QmatrixData.Qmatrix[0][i] =
          pack_w.QmatrixData.Qmatrix[2][i] = iqm[i];
        }
      } else {
        for(i=0; i<64; i++) {
          pack_w.QmatrixData.Qmatrix[0][i] =
          pack_w.QmatrixData.Qmatrix[2][i] = default_intra_quantizer_matrix[i];
        }
      }
      pack_w.QmatrixData.bNewQmatrix[1] = pack_w.QmatrixData.bNewQmatrix[3] = 1;
      if(load_non_intra_quantizer_matrix) {
        for(i=0; i<64; i++) {
          pack_w.QmatrixData.Qmatrix[1][i] =
          pack_w.QmatrixData.Qmatrix[3][i] = niqm[i];
        }
      } else {
        for(i=0; i<64; i++) {
          pack_w.QmatrixData.Qmatrix[1][i] =
          pack_w.QmatrixData.Qmatrix[3][i] = 16;
        }
      }
    }

#elif defined(UMC_VA_LINUX)

    if(pack_l.va_mode == VA_VLD_L)
    {
      pack_l.QmatrixData.load_intra_quantiser_matrix              = 1;
      pack_l.QmatrixData.load_non_intra_quantiser_matrix          = 1;
      pack_l.QmatrixData.load_chroma_intra_quantiser_matrix       = 1;
      pack_l.QmatrixData.load_chroma_non_intra_quantiser_matrix   = 1;
      if(load_intra_quantizer_matrix)
      {
        for(i=0; i<64; i++)
        {
          pack_l.QmatrixData.intra_quantiser_matrix[i] = iqm[i];
          pack_l.QmatrixData.chroma_intra_quantiser_matrix[i] = iqm[i];
        }
      }
      else
      {
        for(i=0; i<64; i++)
        {
          pack_l.QmatrixData.intra_quantiser_matrix[i] = default_intra_quantizer_matrix[i];
          pack_l.QmatrixData.chroma_intra_quantiser_matrix[i] = default_intra_quantizer_matrix[i];
        }
      }
      if(load_non_intra_quantizer_matrix)
      {
        for(i=0; i<64; i++)
        {
            pack_l.QmatrixData.non_intra_quantiser_matrix[i] = niqm[i];
            pack_l.QmatrixData.chroma_non_intra_quantiser_matrix[i] = niqm[i];
        }
      }
      else
      {
        for(i=0; i<64; i++)
        {
          pack_l.QmatrixData.non_intra_quantiser_matrix[i] = 16;
          pack_l.QmatrixData.chroma_non_intra_quantiser_matrix[i] = 16;
        }
      }
    }
#endif // UMC_VA_LINUX

    if (NULL != video)
    {
      // copy sps headers
      if (NULL != shMask.memMask)
      {
#if defined(_WIN32) || defined(_WIN64)
          _aligned_free(shMask.memMask);
#else
          free(shMask.memMask);
#endif
          shMask.memMask = NULL;
      }

#if defined(_WIN32) || defined(_WIN64)
        shMask.memMask = (Ipp8u *) _aligned_malloc(shMask.memSize, 16);
#elif   defined(__APPLE__)
        shMask.memMask = (Ipp8u *) malloc(shMask.memSize);      
#else
        shMask.memMask = (Ipp8u *) memalign(16, shMask.memSize);
#endif
        memset(shMask.memMask, 0, shMask.memSize);

        memcpy_s(shMask.memMask, shMask.memSize, video->bs_sequence_header_start, shMask.memSize);
    }

    if(m_ClipInfo.stream_type == MPEG1_VIDEO) {
      sequenceHeader.progressive_sequence = 1;
      sequenceHeader.chroma_format        = 2;
      m_ClipInfo.color_format             = YUV420;
    }
    if(m_ClipInfo.color_format != YUV422) {
      m_ClipInfo.color_format = YUV420;
    }

    if(m_ClipInfo.stream_type == UNDEF_VIDEO ||
       m_ClipInfo.clip_info.width <= 0 || m_ClipInfo.clip_info.width >= 0x10000 ||
       m_ClipInfo.clip_info.height <= 0 || m_ClipInfo.clip_info.height >= 0x10000)
      return UMC_ERR_INVALID_PARAMS; // if was not set in params
    if (m_ClipInfo.framerate <= 0)
      m_ClipInfo.framerate = 30; // to avoid divide by zero

    // now compute and set params

    //frame_buffer.field_buffer_index[task_num] = 0;

    sequenceHeader.mb_width[task_num] = (m_ClipInfo.clip_info.width + 15) >> 4;
    sequenceHeader.mb_height[task_num]= (m_ClipInfo.clip_info.height + 15) >> 4;
    if(!sequenceHeader.progressive_sequence) {
      sequenceHeader.mb_height[task_num] = (sequenceHeader.mb_height[task_num] + 1) & ~1;
    }
    sequenceHeader.numMB[task_num] = sequenceHeader.mb_width[task_num]*sequenceHeader.mb_height[task_num];

    if (UMC_OK != UpdateFrameBuffer(task_num)) { // create or update internal buffer
      return (UMC_ERR_ALLOC);
    }

    Ipp32s flag_mpeg1 = (m_ClipInfo.stream_type == MPEG1_VIDEO) ? IPPVC_MPEG1_STREAM : 0;
      ippiDecodeIntraInit_MPEG2(NULL, flag_mpeg1, PictureHeader[task_num].intra_vlc_format, PictureHeader[task_num].curr_intra_dc_multi, &m_Spec.decodeIntraSpec);
      ippiDecodeInterInit_MPEG2(NULL, flag_mpeg1, &m_Spec.decodeInterSpec);

      m_Spec.decodeInterSpecChroma = m_Spec.decodeInterSpec;
      m_Spec.decodeIntraSpecChroma = m_Spec.decodeIntraSpec;
      m_Spec.flag = flag_mpeg1;

    if (video != 0) {
      sequenceHeader.is_decoded = 1;

      if(load_intra_quantizer_matrix)
      {
          ippiDecodeIntraInit_MPEG2(iqm, flag_mpeg1, PictureHeader[task_num].intra_vlc_format, PictureHeader[task_num].curr_intra_dc_multi, &m_Spec.decodeIntraSpec);
          m_Spec.decodeIntraSpecChroma = m_Spec.decodeIntraSpec;
      }

      if(load_non_intra_quantizer_matrix)
      {
          ippiDecodeInterInit_MPEG2(niqm, flag_mpeg1, &m_Spec.decodeInterSpec);
          m_Spec.decodeInterSpecChroma = m_Spec.decodeInterSpec;
      }

      m_Spec.flag = flag_mpeg1;

      if (false == m_isFrameRateFromInit)
      {
          switch(frame_rate_code)
          {
          case 0:
              sequenceHeader.delta_frame_time = 1000./30000.; sequenceHeader.frame_rate_extension_n = 30; sequenceHeader.frame_rate_extension_d = 1; break;
          case 1:
              sequenceHeader.delta_frame_time = 1001./24000.; sequenceHeader.frame_rate_extension_n = 24000; sequenceHeader.frame_rate_extension_d = 1001; break;
          case 2:
              sequenceHeader.delta_frame_time = 1000./24000.; sequenceHeader.frame_rate_extension_n = 24; sequenceHeader.frame_rate_extension_d = 1; break;
          case 3:
              sequenceHeader.delta_frame_time = 1000./25000.; sequenceHeader.frame_rate_extension_n = 25; sequenceHeader.frame_rate_extension_d = 1; break;
          case 4:
              sequenceHeader.delta_frame_time = 1001./30000.;  sequenceHeader.frame_rate_extension_n = 30000; sequenceHeader.frame_rate_extension_d = 1001; break;
          case 5:
              sequenceHeader.delta_frame_time = 1000./30000.; sequenceHeader.frame_rate_extension_n = 30; sequenceHeader.frame_rate_extension_d = 1; break;
          case 6:
              sequenceHeader.delta_frame_time = 1000./50000.; sequenceHeader.frame_rate_extension_n = 50; sequenceHeader.frame_rate_extension_d = 1; break;
          case 7:
              sequenceHeader.delta_frame_time = 1001./60000.; sequenceHeader.frame_rate_extension_n = 60000; sequenceHeader.frame_rate_extension_d = 1001; break;
          case 8:
              sequenceHeader.delta_frame_time = 1000./60000.; sequenceHeader.frame_rate_extension_n = 60; sequenceHeader.frame_rate_extension_d = 1; break;
          default:
              sequenceHeader.delta_frame_time = 1000./30000.; sequenceHeader.frame_rate_extension_n = 30; sequenceHeader.frame_rate_extension_d = 1;
              //VM_ASSERT(0);
              break;
          }
      }

      m_ClipInfo.framerate        = 1.0 / sequenceHeader.delta_frame_time;

      if(m_ClipInfo.stream_type == MPEG1_VIDEO) {
        Ipp32s ar_m1_tab[][2] = { // 11172-2 2.4.3.2
          { 1, 1},    // 0   forbidden
          { 1, 1},    // 1   1.0000 VGA etc.
          {33,49},    // 2   0.6735
          {45,64},    // 3   0.7031 16:9, 625line
          {16,21},    // 4   0.7615
          {29,36},    // 5   0.8055
          {27,32},    // 6   0.8437 16:9, 525line
          {42,47},    // 7   0.8935
          {15,16},    // 8   0.9375 CCIR601, 625line
          {53,54},    // 9   0.9815
          {40,39},    // 10  1.0255
          {77,72},    // 11  1.0695
          { 9, 8},    // 12  1.1250 CCIR601, 525line
          {22,19},    // 13  1.1575
          {161,134},  // 14  1.2015
          { 1, 1}     // 15  reserved
        };

        if (false == m_isAspectRatioFromInit)
        {
            m_ClipInfo.aspect_ratio_width = ar_m1_tab[dar_code][0];
            m_ClipInfo.aspect_ratio_height = ar_m1_tab[dar_code][1];
        }

        sequenceHeader.aspect_ratio_code = dar_code;
        sequenceHeader.frame_rate_code = frame_rate_code;
        sequenceHeader.height = m_ClipInfo.clip_info.height;
        sequenceHeader.width = m_ClipInfo.clip_info.width;
        return (UMC_OK);
      }
      Ipp32s W, H;

      if(m_ClipInfo.disp_clip_info.width == 0 ||  m_ClipInfo.disp_clip_info.height == 0)
      {
          W = m_ClipInfo.clip_info.width;
          H = m_ClipInfo.clip_info.height;
      }
      else
      {
          W = m_ClipInfo.disp_clip_info.width;
          H = m_ClipInfo.disp_clip_info.height;
      }

      
      if (false == m_isAspectRatioFromInit)
      {
          switch (dar_code)
          {
            case 2:
                DARtoPAR(W, H, 4, 3, &m_ClipInfo.aspect_ratio_width, &m_ClipInfo.aspect_ratio_height);
                break;

            case 3:
                DARtoPAR(W, H, 16, 9, &m_ClipInfo.aspect_ratio_width, &m_ClipInfo.aspect_ratio_height);
                break;

            case 4:
                DARtoPAR(W, H, 221, 100, &m_ClipInfo.aspect_ratio_width, &m_ClipInfo.aspect_ratio_height);
                break;

            default:
                m_ClipInfo.aspect_ratio_width = 1;
                m_ClipInfo.aspect_ratio_height = 1;
          }
      }

      m_ClipInfo.framerate = 1.0 / sequenceHeader.delta_frame_time;
    }
    else
      sequenceHeader.delta_frame_time = 1.0 / m_ClipInfo.framerate;

    sequenceHeader.aspect_ratio_code = dar_code;
    sequenceHeader.frame_rate_code = frame_rate_code;
    sequenceHeader.height = m_ClipInfo.clip_info.height;
    sequenceHeader.width = m_ClipInfo.clip_info.width;
    sequenceHeader.aspect_ratio_w = (Ipp16u)m_ClipInfo.aspect_ratio_width;
    sequenceHeader.aspect_ratio_h = (Ipp16u)m_ClipInfo.aspect_ratio_height;
    return (UMC_OK);
}


void MPEG2VideoDecoderBase::sequence_display_extension(int task_num)
{
    Ipp32u  code;
    IppVideoContext* video = Video[task_num][0];
    Ipp8u* ptr = video->bs_curr_ptr - 4;

    // skip 3 bits, get 1
    Ipp16u video_format = (ptr[4] >> 1) & 0x07;
    m_signalInfo.VideoFormat = video_format;

    Ipp32u colour = ptr[4] & 0x01;
    m_signalInfo.ColourDescriptionPresent = colour;

    if (colour)
    {
        m_signalInfo.ColourPrimaries = ptr[5];
        m_signalInfo.TransferCharacteristics = ptr[6];
        m_signalInfo.MatrixCoefficients = ptr[7];

        // skip 4 + 24 bits (28 bit)
        code = ((ptr[8]) << 24) | (ptr[9] << 16) | (ptr[10] << 8) | ptr[11];
    }
    else
    {
        // skip 4 bits (28 bit)
        code = ((ptr[5]) << 24) | (ptr[6] << 16) | (ptr[7] << 8) | ptr[8];
    }

    m_ClipInfo.disp_clip_info.width = (code >> 18);
    m_ClipInfo.disp_clip_info.height = (code >> 3) & 0x00003fff;
}

void MPEG2VideoDecoderBase::sequence_scalable_extension(int task_num)
{
    Ipp32u code;
    IppVideoContext* video = Video[task_num][0];
    sequenceHeader.extension_start_code_ID[task_num] = SEQUENCE_SCALABLE_EXTENSION_ID;

    GET_TO9BITS(video->bs, 2, sequenceHeader.scalable_mode[task_num])//scalable_mode
    GET_TO9BITS(video->bs, 4, code);//layer_id
    if(sequenceHeader.scalable_mode[task_num] == SPARTIAL_SCALABILITY)
    {
        SKIP_BITS_32(video->bs)
        SKIP_BITS_LONG(video->bs, 17)
    }
    else if(sequenceHeader.scalable_mode[task_num] == TEMPORAL_SCALABILITY)
    {
        GET_1BIT(video->bs,code)//picture mux enable
        if(code)
            GET_1BIT(video->bs,code)//mux to progressive sequence
        SKIP_BITS(video->bs, 6)
    }
}

bool MPEG2VideoDecoderBase::PictureStructureValid(Ipp32u picture_structure)
{
    switch (picture_structure)
    {
        case FRAME_PICTURE:
        case TOP_FIELD:
        case BOTTOM_FIELD:
            return true;
    }

    return false;
}

// compute and store PT for input, return PT of the frame to be out
// called after field_buffer_index switching
// when FLAG_VDEC_REORDER isn't set, time can be wrong
//   if repeat_first_field happens != 0
void MPEG2VideoDecoderBase::CalculateFrameTime(Ipp64f in_time, Ipp64f * out_time, bool * isOriginal, int task_num, bool buffered)
{
    Ipp32s index;
    Ipp64f duration = sequenceHeader.delta_frame_time;

    bool isTelecineCalc = false;

    // probably this is not decoder responsibility (was commented in linux version)
    if (PictureHeader[task_num].repeat_first_field && (m_lFlags & FLAG_VDEC_TELECINE_PTS))
    {
        isTelecineCalc = true;
        if (sequenceHeader.progressive_sequence != 0)
        {
            duration += sequenceHeader.delta_frame_time;

            if (PictureHeader[task_num].top_field_first)
            {
                duration += sequenceHeader.delta_frame_time;
            }
        }
        else
        {
            duration *= 1.5;
        }
    }

    frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].duration = duration;

    if (PictureHeader[task_num].picture_structure == FRAME_PICTURE || frame_buffer.field_buffer_index[task_num] == 0)
    {
        // save time provided for the frame, ignore for second field
        if (!buffered)
            frame_buffer.frame_p_c_n [frame_buffer.curr_index[task_num]].frame_time = in_time;

        if (in_time >= 0 && isTelecineCalc == false)
            frame_buffer.frame_p_c_n [frame_buffer.curr_index[task_num]].is_original_frame_time = true;

        // compute current time, ignoring second field, don't recalc if absent
        if (in_time > 0)
        {

            if (PictureHeader[task_num].picture_coding_type == MPEG2_B_PICTURE)
            {
                sequenceHeader.stream_time = in_time;
            }
            else if (!(m_lFlags & FLAG_VDEC_REORDER) || m_IsDataFirst)
            {
                // can happen in the beginning
                // can become wrong
                if (0 > sequenceHeader.stream_time_temporal_reference)
                {
                    sequenceHeader.stream_time_temporal_reference = 0;
                }
                sequenceHeader.stream_time = in_time - sequenceHeader.delta_frame_time *
                                             (PictureHeader[task_num].temporal_reference - sequenceHeader.stream_time_temporal_reference);
            }
        }
    }
    if (frame_buffer.ret_index >= 0 && // stream time is of output other IP
        PictureHeader[task_num].picture_coding_type != MPEG2_B_PICTURE &&
        //frame_buffer.frame_p_c_n [1-frame_buffer.curr_index[task_num]].frame_time >= 0)
        frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].prev_index >= 0 &&
        frame_buffer.frame_p_c_n [frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].prev_index].frame_time >= 0)
    {
            sequenceHeader.stream_time = frame_buffer.frame_p_c_n [frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].prev_index].frame_time;
            //frame_buffer.frame_p_c_n [1-frame_buffer.curr_index[task_num]].frame_time;

    }

    // compute time to be out
    index = frame_buffer.ret_index;

    if (index >= 0 && frame_buffer.frame_p_c_n [index].frame_time < 0)
    {
        // frame to be out hasn't proper time
        if(PictureHeader[task_num].picture_coding_type == MPEG2_B_PICTURE || m_lFlags & FLAG_VDEC_REORDER)
        {
            // use current time
            *out_time = sequenceHeader.stream_time;
        }
        else
        {
            // compute next ref_frame, really curr_time + IPdistance. But repeat field!
            if (!(m_lFlags & FLAG_VDEC_REORDER) && m_IsDataFirst)
            {
                *out_time = 0;
            }
            else
            {
                *out_time = sequenceHeader.stream_time +  sequenceHeader.delta_frame_time *
                           (PictureHeader[task_num].temporal_reference - sequenceHeader.stream_time_temporal_reference);
            }
        }
    }
    else if (index >= 0)
    {
        *out_time = frame_buffer.frame_p_c_n [index].frame_time;
        if (frame_buffer.frame_p_c_n [index].is_original_frame_time == true)
            *isOriginal = true;
    }
    else
    {
        *out_time = -1;
    }

    // update current time after second field
    if (PictureHeader[task_num].picture_structure == FRAME_PICTURE || frame_buffer.field_buffer_index[task_num] == 1)
    {
        if (PictureHeader[task_num].picture_coding_type == MPEG2_B_PICTURE)
        {
            sequenceHeader.stream_time += frame_buffer.frame_p_c_n [frame_buffer.curr_index[task_num]].duration
                                          * (m_dPlaybackRate > 0 ? 1 : -1);
        }
        else if (index >= 0 && frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].prev_index >= 0)
        {
            // previous reference frame is returned
            sequenceHeader.stream_time += frame_buffer.frame_p_c_n [frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].prev_index].duration
                                          * (m_dPlaybackRate > 0 ? 1 : -1);
        }
    }

    if(frame_buffer.field_buffer_index[task_num] == 0) // not for second field
    {
        sequenceHeader.stream_time_temporal_reference++;
    }
}



Status MPEG2VideoDecoderBase::DecodeSlices(Ipp32s threadID, int task_num)
{
    IppVideoContext *video = Video[task_num][threadID];
    Status umcRes = UMC_OK;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "MPEG2Dec_work");

    for (;;) 
    {
        umcRes = DecodeSliceHeader(video, task_num);
        UMC_CHECK_STATUS(umcRes);

        umcRes = DecodeSlice(video, task_num);
        if (UMC_ERR_INVALID_STREAM == umcRes)
        {
            break;
        }
#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)
      pack_w.pSliceInfo++;
#endif
    }

    return (umcRes);
}


Status MPEG2VideoDecoderBase::DecodePicture()
{
    return (UMC_OK);
}
//$$$$$!!!!!

Status MPEG2VideoDecoderBase::SaveDecoderState()
{
    frameBuffer_backup_previous = frameBuffer_backup;
    frameBuffer_backup = frame_buffer;
    b_curr_number_backup = sequenceHeader.b_curr_number;
    first_i_occure_backup = sequenceHeader.first_i_occure;
    frame_count_backup = sequenceHeader.frame_count;
    return UMC_OK;
}

Status MPEG2VideoDecoderBase::RestoreDecoderState()
{
    frame_buffer = frameBuffer_backup;
    frameBuffer_backup = frameBuffer_backup_previous;
    sequenceHeader.b_curr_number = b_curr_number_backup;
    sequenceHeader.first_i_occure = first_i_occure_backup;
    sequenceHeader.frame_count = frame_count_backup;
    return UMC_OK;
}

Status MPEG2VideoDecoderBase::RestoreDecoderStateAndRemoveLastField()
{
    RestoreDecoderState();
    std::copy(frameBuffer_backup.frame_p_c_n, frameBuffer_backup.frame_p_c_n + 2*DPB_SIZE, frame_buffer.frame_p_c_n);
    frame_buffer.latest_next = frameBuffer_backup.latest_next;
    frame_buffer.latest_prev = frameBuffer_backup.latest_prev;
    return UMC_OK;
}

Status MPEG2VideoDecoderBase::DecodePictureHeader(int task_num)
{
    Ipp32u          code;
    Ipp32s          pic_type = 0;
    IppVideoContext *video   = Video[task_num][0];
    sPictureHeader  *pPic    = &PictureHeader[task_num];

    bool isCorrupted = false;

    if (GET_REMAINED_BYTES(video->bs) < 4) 
    {
        // return header back
        UNGET_BITS_32(video->bs)
        return (UMC_ERR_NOT_ENOUGH_DATA);
    }

    sequenceHeader.frame_count++;

    memset(&PictureHeader[task_num], 0, sizeof(sPictureHeader));
    PictureHeader[task_num].time_code = sequenceHeader.time_code;
    PictureHeader[task_num].first_in_sequence = sequenceHeader.frame_count == 1;

    GET_BITS(video->bs, 10, PictureHeader[task_num].temporal_reference)
    GET_TO9BITS(video->bs, 3, pic_type)
    GET_BITS(video->bs, 16 ,code)

    PictureHeader[task_num].d_picture = 0;

    switch (pic_type)
    {
        case 1: 
            PictureHeader[task_num].picture_coding_type = MPEG2_I_PICTURE;
            break;

        case 2:
            PictureHeader[task_num].picture_coding_type = MPEG2_P_PICTURE;
            break;

        case 3:
            PictureHeader[task_num].picture_coding_type = MPEG2_B_PICTURE;
            break;

        case 4:
            PictureHeader[task_num].picture_coding_type = MPEG2_I_PICTURE;
            PictureHeader[task_num].d_picture = 1;
            break;

        default:
            m_IsFrameSkipped = true;
            return UMC_ERR_INVALID_STREAM;
    }

    if (PictureHeader[task_num].d_picture) 
        sequenceHeader.first_i_occure = 1; // no refs in this case

    if(m_dPlaybackRate < 0 && m_dPlaybackRate > -4 && PictureHeader[task_num].picture_coding_type == MPEG2_I_PICTURE)
        Reset();

    pPic->low_in_range[0] = pPic->low_in_range[1] =
    pPic->low_in_range[2] = pPic->low_in_range[3] = -16;
    pPic->high_in_range[0] = pPic->high_in_range[1] =
    pPic->high_in_range[2] = pPic->high_in_range[3] = 15;
    pPic->range[0] = pPic->range[1] =
    pPic->range[2] = pPic->range[3] = 32;

    if(PictureHeader[task_num].picture_coding_type == MPEG2_B_PICTURE)
    {
        GET_1BIT(video->bs,PictureHeader[task_num].full_pel_forward_vector)
        GET_TO9BITS(video->bs, 3, PictureHeader[task_num].f_code[0])
        GET_1BIT(video->bs,PictureHeader[task_num].full_pel_backward_vector)
        GET_TO9BITS(video->bs, 3, PictureHeader[task_num].f_code[2])

        PictureHeader[task_num].f_code[1] = PictureHeader[task_num].f_code[0];
        PictureHeader[task_num].f_code[3] = PictureHeader[task_num].f_code[2];
        if(PictureHeader[task_num].f_code[0] == 0 || PictureHeader[task_num].f_code[2] == 0)
        {
            m_IsFrameSkipped = true;
            return UMC_ERR_INVALID_STREAM;
        }
        pPic->r_size[0] = PictureHeader[task_num].f_code[0] - 1;
        pPic->r_size[1] = PictureHeader[task_num].f_code[1] - 1;
        pPic->r_size[2] = PictureHeader[task_num].f_code[2] - 1;
        pPic->r_size[3] = PictureHeader[task_num].f_code[3] - 1;
    }
    else if(PictureHeader[task_num].picture_coding_type == MPEG2_P_PICTURE )
    {
        GET_1BIT(video->bs,PictureHeader[task_num].full_pel_forward_vector)
        GET_TO9BITS(video->bs, 3, PictureHeader[task_num].f_code[0])
        PictureHeader[task_num].f_code[1] = PictureHeader[task_num].f_code[0];

        if (PictureHeader[task_num].f_code[0] == 0)
        {
            m_IsFrameSkipped = true;
            return UMC_ERR_INVALID_STREAM;
        }

        pPic->r_size[0] = PictureHeader[task_num].f_code[0] - 1;
        pPic->r_size[1] = PictureHeader[task_num].f_code[1] - 1;
        pPic->r_size[2] = 0;
        pPic->r_size[3] = 0;
    }
    GET_1BIT(video->bs,code)
    while(code)
    {
        GET_TO9BITS(video->bs, 8 ,code) //extra information picture
        GET_1BIT(video->bs,code)
    }
    //set for MPEG1
    PictureHeader[task_num].picture_structure          = FRAME_PICTURE;
    PictureHeader[task_num].frame_pred_frame_dct       = 1;
    PictureHeader[task_num].intra_vlc_format           = 0;
    PictureHeader[task_num].progressive_frame          = 1;
    PictureHeader[task_num].curr_intra_dc_multi        = intra_dc_multi[0];
    PictureHeader[task_num].curr_reset_dc              = reset_dc[0];

    Ipp32s i, f;
    for(i = 0; i < 4; i++) {
        f = 1 << pPic->r_size[i];
        pPic->low_in_range[i] = -(f * 16);
        pPic->high_in_range[i] = (f * 16) - 1;
        pPic->range[i] = f * 32;
    }

    FIND_START_CODE(video->bs, code);

    //VM_ASSERT(code == EXTENSION_START_CODE);
    Ipp32u ecode = 0;
    if(code == EXTENSION_START_CODE)
    {
        SKIP_BITS_32(video->bs);
        SHOW_TO9BITS(video->bs, 4, ecode)
    }
    if((ecode == PICTURE_CODING_EXTENSION_ID) != (m_ClipInfo.stream_type == MPEG2_VIDEO))
    {
     // return (UMC_ERR_INVALID_STREAM);
        isCorrupted = true;
    }
    // compute maximum slice vertical position
    if(PictureHeader[task_num].picture_structure == FRAME_PICTURE)
      PictureHeader[task_num].max_slice_vert_pos = sequenceHeader.mb_height[task_num];
    else
      PictureHeader[task_num].max_slice_vert_pos = sequenceHeader.mb_height[task_num] >> 1;

    if(code == EXTENSION_START_CODE)
    {
        DecodeHeader(EXTENSION_START_CODE, video, task_num);

        FIND_START_CODE(video->bs, code);
    }

    while (code == EXTENSION_START_CODE || code == USER_DATA_START_CODE)
    {
        SKIP_BITS_32(video->bs);
        DecodeHeader(code, video, task_num);

        FIND_START_CODE(video->bs, code);
    }

    if(!sequenceHeader.first_i_occure)
    {
      if (MPEG2_I_PICTURE != PictureHeader[task_num].picture_coding_type) // including D-picture
      {
        // To support only P frames streams this check is disabled
        // return (UMC_ERR_NOT_ENOUGH_DATA);
      }
    }

    m_IsFrameSkipped = IsPictureToSkip(task_num);

    if(m_IsFrameSkipped)
    {
        return UMC_OK;
    }

    {
        // Bitstream error, not clear picture type
        // Skipping picture because wrong picture type (frame or field) can take decoder to unrecoverable state

        if (!PictureStructureValid(PictureHeader[task_num].picture_structure))
        {
            m_IsFrameSkipped = true;
            return UMC_OK;
        }

        // The check below is from progressive_frame field description in 6.3.10 part of MPEG2 standard
        if (PictureHeader[task_num].progressive_frame && PictureHeader[task_num].picture_structure != FRAME_PICTURE)
        {
            m_IsFrameSkipped = true;
            return UMC_OK;
        }

        // second field must be the same, except IP
        if (frame_buffer.field_buffer_index[task_num] != 0)
            if (m_picture_coding_type_save != PictureHeader[task_num].picture_coding_type &&
                m_picture_coding_type_save != MPEG2_I_PICTURE && PictureHeader[task_num].picture_coding_type != MPEG2_P_PICTURE)
            {
                frame_buffer.field_buffer_index[task_num] = 0;
                m_IsFrameSkipped = true;
                return UMC_ERR_INVALID_STREAM;
            }
        m_picture_coding_type_save = PictureHeader[task_num].picture_coding_type;
    }

    if (PictureHeader[task_num].picture_structure == FRAME_PICTURE
        || frame_buffer.field_buffer_index[task_num] == 0)
    {

        frame_buffer.common_curr_index = task_num;
        frame_buffer.curr_index[task_num] = frame_buffer.common_curr_index;
    }
    else//(PictureHeader[task_num].picture_structure != FRAME_PICTURE
        //&& frame_buffer.field_buffer_index[task_num] == 1)
    {
        frame_buffer.curr_index[task_num] = frame_buffer.common_curr_index+DPB_SIZE;
    }

    switch (PictureHeader[task_num].picture_coding_type)
    {
        case MPEG2_I_PICTURE:
        case MPEG2_P_PICTURE:

            sequenceHeader.b_curr_number = 0;

            if (PictureHeader[task_num].picture_structure == FRAME_PICTURE
                || frame_buffer.field_buffer_index[task_num] == 0)
            {
                // reset index(es) of frames
                frame_buffer.latest_prev = frame_buffer.latest_next;
                frame_buffer.latest_next = frame_buffer.curr_index[task_num];
            }
            
            frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].prev_index = frame_buffer.latest_prev;
            frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].next_index = 0xffffffff;

            // to optimize logic !!
            if (PictureHeader[task_num].picture_structure == FRAME_PICTURE
                || frame_buffer.field_buffer_index[task_num] == 1)
            { // complete frame
              // reorder frames
                if (m_lFlags & FLAG_VDEC_REORDER) {
                {
                    if (frame_buffer.latest_prev >= 0)
                    {
                        frame_buffer.ret_array[frame_buffer.ret_array_free] = frame_buffer.latest_prev;
                        frame_buffer.ret_index = frame_buffer.latest_prev;
                        frame_buffer.ret_array_free++;
                        
                        if(frame_buffer.ret_array_free >= DPB_SIZE)
                            frame_buffer.ret_array_free = 0;
                        
                        frame_buffer.ret_array_len++;
                    }
                }
            }
            else // no reordering
            {
                int ret_index = frame_buffer.curr_index[task_num];

                if (ret_index >= DPB_SIZE)
                {
                    ret_index -= DPB_SIZE;
                }
                
                frame_buffer.ret_array[frame_buffer.ret_array_free] = ret_index;
                frame_buffer.ret_index = ret_index;
                frame_buffer.ret_array_free++;

                if (frame_buffer.ret_array_free >= DPB_SIZE)
                {
                    frame_buffer.ret_array_free = 0;
                }

                frame_buffer.ret_array_len++;
            }
        }
        
        break;

    case MPEG2_B_PICTURE:

        if (frame_buffer.field_buffer_index[task_num] == 0) 
        {
            sequenceHeader.b_curr_number++;
        }

        // reset index(es) of frames
        frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].prev_index = frame_buffer.latest_prev;
        frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].next_index = frame_buffer.latest_next;

        if (frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].next_index < 0)
        {
            frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].next_index = 
                frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].prev_index;
        }

        //fix for VCSD100019408. B frame with no forward prediction -> use backward instead of forward
        if (frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].prev_index < 0)
        {
            frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].prev_index =
                frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].next_index;
        }
        //end of fix for VCSD100019408

        if (PictureHeader[task_num].picture_structure == FRAME_PICTURE
            || frame_buffer.field_buffer_index[task_num] == 1)
        { // complete frame
              Ipp32u ret_index = frame_buffer.curr_index[task_num];

              if (ret_index >= DPB_SIZE)
              {
                  ret_index -= DPB_SIZE;
              }

              frame_buffer.ret_array[frame_buffer.ret_array_free] = ret_index;
              frame_buffer.ret_index = ret_index;
              frame_buffer.ret_array_free++;
              
              if (frame_buffer.ret_array_free >= DPB_SIZE)
              {
                  frame_buffer.ret_array_free = 0;
              }

              frame_buffer.ret_array_len++;
        } 

        break;

   // default:

        //return (UMC_ERR_INVALID_STREAM);
    }

    switch (PictureHeader[task_num].picture_coding_type)
    {
        case MPEG2_I_PICTURE:

            frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].frame_type = I_PICTURE;
            break;

        case MPEG2_B_PICTURE:

            frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].frame_type = B_PICTURE;
            break;

        default: // MPEG2_P_PICTURE:
            frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].frame_type = P_PICTURE;
    }

    frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].isCorrupted = false;

    if (true == isCorrupted)
    {
        frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].isCorrupted = true;
    }

#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)
    if (pack_w.m_va)
    {
        pack_w.bNeedNewFrame = true;
    }
#endif

    return (UMC_OK);
}

void MPEG2VideoDecoderBase::quant_matrix_extension(int task_num)
{
    Ipp32s i;
    Ipp32u code;
    IppVideoContext* video = Video[task_num][0];
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
#if defined UMC_VA_DXVA
    if(pack_w.va_mode == VA_VLD_W) {
      if(load_intra_quantizer_matrix) {
        pack_w.QmatrixData.bNewQmatrix[0] = 1;
        for(i=0; i<64; i++) {
          pack_w.QmatrixData.Qmatrix[0][i] = q_matrix[0][i];
        }
      }
      if(load_non_intra_quantizer_matrix) {
        pack_w.QmatrixData.bNewQmatrix[1] = 1;
        for(i=0; i<64; i++) {
          pack_w.QmatrixData.Qmatrix[1][i] = q_matrix[1][i];
        }
      }
      if(load_chroma_intra_quantizer_matrix) {
        pack_w.QmatrixData.bNewQmatrix[2] = 1;
        for(i=0; i<64; i++) {
          pack_w.QmatrixData.Qmatrix[2][i] = q_matrix[2][i];
        }
      }
      if(load_chroma_non_intra_quantizer_matrix) {
        pack_w.QmatrixData.bNewQmatrix[3] = 1;
        for(i=0; i<64; i++) {
          pack_w.QmatrixData.Qmatrix[3][i] = q_matrix[3][i];
        }
      }
    }
#elif defined UMC_VA_LINUX
    if(pack_l.va_mode == VA_VLD_L) {
      if(load_intra_quantizer_matrix)
      {
          pack_l.QmatrixData.load_intra_quantiser_matrix              = load_intra_quantizer_matrix;
          for(i=0; i<64; i++)
              pack_l.QmatrixData.intra_quantiser_matrix[i] = q_matrix[0][i];
      }
      if(load_non_intra_quantizer_matrix)
      {
          pack_l.QmatrixData.load_non_intra_quantiser_matrix          = load_non_intra_quantizer_matrix;
          for(i=0; i<64; i++)
              pack_l.QmatrixData.non_intra_quantiser_matrix[i] = q_matrix[1][i];
      }
      if(load_chroma_intra_quantizer_matrix)
      {
          pack_l.QmatrixData.load_chroma_intra_quantiser_matrix       = load_chroma_intra_quantizer_matrix;
          for(i=0; i<64; i++)
              pack_l.QmatrixData.chroma_intra_quantiser_matrix[i] = q_matrix[2][i];
      }
      if(load_chroma_non_intra_quantizer_matrix)
      {
          pack_l.QmatrixData.load_chroma_non_intra_quantiser_matrix   = load_chroma_non_intra_quantizer_matrix;
          for(i=0; i<64; i++)
              pack_l.QmatrixData.chroma_non_intra_quantiser_matrix[i] = q_matrix[3][i];
      }
    }//if(va_mode == VA_VLD_L)
#endif

} //void quant_matrix_extension()


void MPEG2VideoDecoderBase::copyright_extension(int task_num)
{
    IppVideoContext* video = Video[task_num][0];

    SKIP_BITS_32(video->bs)
    SKIP_BITS_32(video->bs)
    SKIP_BITS_LONG(video->bs, 22)
}


void MPEG2VideoDecoderBase::picture_display_extension(int task_num)
{
    Ipp32s number_of_frame_center_offsets = 0, i;
    //Ipp32u code;
    IppVideoContext* video = Video[task_num][0];

    //GET_TO9BITS(video->bs, 4 ,code)
    SKIP_BITS(video->bs, 4);

    if(sequenceHeader.progressive_sequence)
    {
        if(PictureHeader[task_num].repeat_first_field)
        {
            if(PictureHeader[task_num].top_field_first)
                number_of_frame_center_offsets = 3;
            else
                number_of_frame_center_offsets = 2;
        }
        else
            number_of_frame_center_offsets = 1;
    }
    else
    {
        if(PictureHeader[task_num].picture_structure != FRAME_PICTURE)
            number_of_frame_center_offsets = 1;
        else
        {
            if(PictureHeader[task_num].repeat_first_field)
                number_of_frame_center_offsets = 3;
            else
                number_of_frame_center_offsets = 2;
        }
    }
    for(i = 0; i < number_of_frame_center_offsets; i++)
    {
        SKIP_BITS_32(video->bs)
        SKIP_BITS(video->bs, 2)
    }

} //void picture_display_extension()


void MPEG2VideoDecoderBase::picture_spartial_scalable_extension(int task_num)
{
    IppVideoContext* video = Video[task_num][0];
    SKIP_BITS_32(video->bs)
    SKIP_BITS_LONG(video->bs, 18)
}


void MPEG2VideoDecoderBase::picture_temporal_scalable_extension(int task_num)
{
    IppVideoContext* video = Video[task_num][0];
    SKIP_BITS_LONG(video->bs, 27)
}

Status MPEG2VideoDecoderBase::GetUserData(MediaData* pCC)
{
    Status umcRes = UMC_OK;

    if (m_ccCurrData.GetBufferSize())
    {
        m_pCCData->UnLockOutputBuffer(&m_ccCurrData);
        m_ccCurrData.SetBufferPointer(0,0);
    }

    umcRes = m_pCCData->LockOutputBuffer(&m_ccCurrData);

    if (UMC_OK == umcRes)
    {
        *pCC = m_ccCurrData;
        m_ccCurrData.SetDataSize(0);
    }

    return (umcRes);
}


#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
