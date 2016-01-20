//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010-2013 Intel Corporation. All Rights Reserved.
//
//
//*/

#include <exception>
#include <stdio.h>
#include <assert.h>

#include "mpeg2_spl.h"

#include "sample_defs.h"

namespace ProtectedLibrary
{

#define MAX_LEN 1024 * 10000

enum
{
    PIC = 0x00,
    SEQ = 0xb3,
    EXT = 0xb5,
    END = 0xb7,
    GROUP = 0xb8
};

mfxI32 MBAdressing[] =
{

11, /* max bits */
2, /* total subtables */
3,8,/* subtable sizes */

 1, /* 1-bit codes */
0x0001, 0x0001,
 0, /* 2-bit codes */
 2, /* 3-bit codes */
0x0003, 0x0002, 0x0002, 0x0003,
 2, /* 4-bit codes */
0x0003, 0x0004, 0x0002, 0x0005,
 2, /* 5-bit codes */
0x0003, 0x0006, 0x0002, 0x0007,
 0, /* 6-bit codes */
 2, /* 7-bit codes */
0x0007, 0x0008, 0x0006, 0x0009,
 6, /* 8-bit codes */
0x000b, 0x000a, 0x000a, 0x000b, 0x0009, 0x000c, 0x0008, 0x000d,
0x0007, 0x000e, 0x0006, 0x000f,
 0, /* 9-bit codes */
 6, /* 10-bit codes */
0x0017, 0x0010, 0x0016, 0x0011, 0x0015, 0x0012, 0x0014, 0x0013,
0x0013, 0x0014, 0x0012, 0x0015,
 13, /* 11-bit codes */
0x0023, 0x0016, 0x0022, 0x0017, 0x0021, 0x0018, 0x0020, 0x0019,
0x001f, 0x001a, 0x001e, 0x001b, 0x001d, 0x001c, 0x001c, 0x001d,
0x001b, 0x001e, 0x001a, 0x001f, 0x0019, 0x0020, 0x0018, 0x0021,
0x0008, 0xffff,
-1 /* end of table */
};

#define DECODE_MB_INCREMENT(BS, macroblock_address_increment)         \
{                                                                     \
  mfxI32 cc;                                                          \
  SHOW_BITS(BS, 11, cc)                                               \
                                                                      \
  if(cc == 0) {                                                       \
    return MFX_ERR_NONE; /* end of slice or bad code. Anyway, stop slice */ \
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
  if (macroblock_address_increment > (m_width - mb_col)) { \
    macroblock_address_increment = m_width - mb_col;       \
  }                                                                               \
}

mfxStatus mp2_HuffmanTableInitAlloc(mfxI32 *tbl, mfxI32 bits_table0, mp2_VLCTable *vlc)
{
  mfxI32 *ptbl;
  mfxI16 bad_value = 0;
  mfxI32 max_bits;
  mfxI32 num_tbl;
  mfxI32 i, j, k, m, n, len;
  mfxI32 bits, code, value;
  mfxI32 bits0, bits1;
  mfxI32 min_value, max_value, spec_value;
  mfxI32 min_code0, min_code1;
  mfxI32 max_code0, max_code1;
  mfxI32 prefix_code1 = -1;
  mfxI32 bits_table1 = 0;
  mfxI32 *buffer = NULL;
  mfxI32 *codes;
  mfxI32 *cbits;
  mfxI32 *values;
  mfxI16 *table0 = NULL;
  mfxI16 *table1 = NULL;

  /* get number of entries (n) */
  max_bits = *tbl;
  tbl++;
  num_tbl = *tbl;
  tbl++;
  for (i = 0; i < num_tbl; i++) {
    tbl++;
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
  buffer = new mfxI32[3*n];
  if (!buffer) return MFX_ERR_MEMORY_ALLOC;
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

      codes[k] = code << (30 - bits);
      cbits[k] = bits;
      values[k] = value;
      k++;
    }
  }

  if (!bits_table0) {
    bits_table0 = max_bits;
    bits_table1 = 0;
    vlc->threshold_table0 = 0;
  }
  bits0 = bits_table0;
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
    }
  }

  if (bits_table0 > 0 && bits_table0 < max_bits && !bits_table1) {
    if (buffer) delete[] buffer;
    return MFX_ERR_UNSUPPORTED;
  }

  bad_value = (mfxI16)((bad_value << 8) | VLC_BAD);

  len = 1 << bits_table0;
  table0 = (mfxI16*)malloc(len*sizeof(mfxI16));
  if(NULL == table0)
  {
      delete []buffer;
      return MFX_ERR_MEMORY_ALLOC;
  }
  for(j = 0; j < len; j++)
      table0[j] = bad_value;
  if (bits_table1) {
    len = 1 << bits_table1;
    table1 = (mfxI16*)malloc(len*sizeof(mfxI16));
    if(NULL == table1)
    {
      delete []buffer;
      free(table0);
      return MFX_ERR_MEMORY_ALLOC;
    }
    for(j = 0; j < len; j++)
        table1[j] = bad_value;
  }
  for (i = 0; i < n; i++) {
    code = codes[i];
    bits = cbits[i];
    value = values[i];
    if (bits <= bits_table0) {
      code = code >> (30 - bits_table0);
      for (j = 0; j < (1 << (bits_table0 - bits)); j++) {
        table0[code + j] = (mfxI16)((value << 8) | bits);
      }
    } else if(table1){
      code = code >> (30 - max_bits);
      code = code & ((1 << bits_table1) - 1);
      for (j = 0; j < (1 << (max_bits - bits)); j++) {
        table1[code + j] = (mfxI16)((value << 8) | bits);
      }
    }
  }

  if (bits_table1) { // fill VLC_NEXTTABLE
    if (prefix_code1 == -1) {
      if (buffer) delete[] buffer;
      if(table1)
      {
          free(table1);
          table1=NULL;
      }
      if(table0)
      {
          free(table0);
          table1=NULL;
      }
      return MFX_ERR_UNSUPPORTED;
    }
    bad_value = (mfxI16)((bad_value &~ 255) | VLC_NEXTTABLE);
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
  return MFX_ERR_NONE;
}

static void mp2_HuffmanTableFree(mp2_VLCTable *vlc) {
  if (vlc->table0) {
    free(vlc->table0);
    vlc->table0 = NULL;
  }
  if (vlc->table1) {
    free(vlc->table1);
    vlc->table1 = NULL;
  }
}

MPEG2_Spl::MPEG2_Spl()
{

    m_bFirstField = true;
    m_first_slice_ptr = NULL;
    memset(&m_frame, 0, sizeof(FrameSplitterInfo));
    memset(&m_bs, 0, sizeof(mfxBitstream));
    memset(&Video, 0, sizeof(VideoBs));
    ResetFcState(m_fcState);
    picture_structure = FRAME_PICTURE;

    Init();
}

MPEG2_Spl::~MPEG2_Spl()
{
    Close();
}

mfxStatus MPEG2_Spl::Init()
{
    mfxStatus res = MFX_ERR_NONE;
    m_bFirstField = true;
    m_first_slice_ptr = NULL;
    memset(&m_frame, 0, sizeof(m_frame));
    memset(&m_bs, 0, sizeof(mfxBitstream));
    memset(&Video, 0, sizeof(VideoBs));
    ResetFcState(m_fcState);

    mfxU32 MaxLength = MAX_LEN;
    m_frame.Data = new mfxU8[MaxLength];
    if(m_frame.Data == NULL)
        return MFX_ERR_MEMORY_ALLOC;

    m_bs.Data = m_frame.Data;
    first_slice_DataOffset = 0;

    m_width = 0;
    m_height = 0;
    extension_start_code_ID = 0;
    scalable_mode = 0;
    picture_structure = FRAME_PICTURE;

    m_slices.resize(128);
    m_frame.Slice = &m_slices[0];

    res = mp2_HuffmanTableInitAlloc(MBAdressing, 5, &vlcMBAdressing);
    if (MFX_ERR_NONE != res)
        return res;

    return MFX_ERR_NONE;
}

void MPEG2_Spl::Close()
{
    if(m_frame.Data)
    {
        delete [] m_frame.Data;
        m_frame.Data = NULL;
    }

    mp2_HuffmanTableFree(&vlcMBAdressing);
}

void MPEG2_Spl::ResetCurrentState()
{
    m_frame.DataLength = 0;
    m_frame.SliceNum = 0;
    m_frame.FirstFieldSliceNum = 0;
    m_bFirstField = true;
    memset(&Video, 0, sizeof(VideoBs));
    m_bs.DataOffset = 0;
    m_bs.DataLength = 0;
    ResetFcState(m_fcState);
    picture_structure = FRAME_PICTURE;
}

mfxStatus MPEG2_Spl::Reset()
{
    ResetCurrentState();
    return MFX_ERR_NONE;
}

inline bool IsMpeg2StartCode(const mfxU8* p)
{
    return p[0] == 0 && p[1] == 0 && p[2] == 1 && (p[3] == PIC || p[3] == SEQ || p[3] == END || p[3] == GROUP);
}

const mfxU8* FindStartCode(const mfxU8* begin, const mfxU8* end)
{
    for (; begin + 3 < end; ++begin)
        if (IsMpeg2StartCode(begin))
            break;
    return begin;
}

mfxStatus AppendBitstream(mfxBitstream& bs, const mfxU8* ptr, mfxU32 bytes)
{
    if (bs.DataOffset + bs.DataLength + bytes > bs.MaxLength)
        return MFX_ERR_NOT_ENOUGH_BUFFER;
    MSDK_MEMCPY_BITSTREAM(bs, bs.DataOffset + bs.DataLength, ptr, bytes);
    bs.DataLength += bytes;
    return MFX_ERR_NONE;
}

void MoveBitstreamData(mfxBitstream& bs, mfxU32 offset)
{
    assert(offset <= bs.DataLength);
    bs.DataOffset += offset;
    bs.DataLength -= offset;
}

mfxStatus MPEG2_Spl::ConstructFrame(mfxBitstream *in, mfxBitstream *out)
{
    if(!in)
        return MFX_ERR_NULL_PTR;
    if(!out)
        return MFX_ERR_NULL_PTR;
    if(!in->Data)
        return MFX_ERR_NULL_PTR;
    if(!out->Data)
        return MFX_ERR_NULL_PTR;

    if (in->DataLength < 4)
    {
        AppendBitstream(*out, in->Data + in->DataOffset, in->DataLength);
        MoveBitstreamData(*in, (mfxU32)(in->DataLength));
        memset(m_last_bytes,0,NUM_REST_BYTES);
        return MFX_ERR_MORE_DATA;
    }

    const mfxU8* end = in->Data + in->DataOffset + in->DataLength;

    while (!m_fcState.picStart)
    {
        const mfxU8* beg = in->Data + in->DataOffset;
        const mfxU8* ptr = FindStartCode(beg, end);

        if (ptr >= end - 3)
        {
            MoveBitstreamData(*in, (mfxU32)(ptr - beg));
            memset(m_last_bytes,0,NUM_REST_BYTES);
            return MFX_ERR_MORE_DATA;
        }

        MoveBitstreamData(*in, (mfxU32)(ptr - beg));
        if (ptr[3] == SEQ || ptr[3] == EXT || ptr[3] == GROUP || ptr[3] == PIC)
        {
            mfxStatus res = AppendBitstream(*out, ptr, 4);
            if(res != MFX_ERR_NONE) return res;
            MoveBitstreamData(*in, 4);

            m_fcState.picStart = 1;
            if (ptr[3] == PIC)
            {
                m_fcState.picHeader = FcState::FRAME;
                if(in->DataFlag == MFX_BITSTREAM_COMPLETE_FRAME)
                {
                    mfxU32 len = in->DataLength;
                    const mfxU8* beg1 = in->Data + in->DataOffset;
                    const mfxU8* ptr1 = FindStartCode(beg1, end);
                    if(ptr1 < beg1)
                        return MFX_ERR_UNDEFINED_BEHAVIOR;

                    if (ptr1[3] == SEQ || ptr1[3] == EXT || ptr1[3] == GROUP || ptr1[3] == PIC || ptr1[3] == END)
                    {
                        len = (mfxU32)(ptr1 - beg1);
                    }
                    if(ptr1 + 3 >= end)//start code not found
                    {
                        len = (mfxU32)(end - beg1);
                    }
                    if(out->DataLength > out->MaxLength)
                        return MFX_ERR_UNDEFINED_BEHAVIOR;

                    mfxU32 len1 =  (mfxU32)(out->MaxLength - out->DataLength);
                    if(len1 < len)
                        len = len1;
                    res = AppendBitstream(*out, beg1, len);
                    if(res != MFX_ERR_NONE) return res;
                    MoveBitstreamData(*in, len);
                    m_fcState.picStart = 0;
                    m_fcState.picHeader = FcState::NONE;
                    memset(m_last_bytes,0,NUM_REST_BYTES);
                    return MFX_ERR_NONE;
                }
            }
        }
        else
        {
            MoveBitstreamData(*in, 4);
        }
    }

    for (;;)
    {
        const mfxU8* beg = in->Data + in->DataOffset;
        const mfxU8* ptr = FindStartCode(beg, end);
        if (ptr + 3 >= end)
        {
            mfxStatus res = AppendBitstream(*out, beg, (mfxU32)(ptr - beg));
            if(res != MFX_ERR_NONE) return res;
            MoveBitstreamData(*in, (mfxU32)(ptr - beg));
            mfxU8* p = (mfxU8*)ptr;
            m_last_bytes[3] = 0;
            while(p < end)
            {
                m_last_bytes[m_last_bytes[3]] = *p;
                p++;
                m_last_bytes[3]++;
            }
            return MFX_ERR_MORE_DATA;
        }
        else
        {
            if (ptr[3] == END && m_fcState.picHeader == FcState::FRAME)
                ptr += 4; // append end_sequence_code to the end of current picture

            mfxStatus res = AppendBitstream(*out, beg, (mfxU32)(ptr - beg));
            if(res != MFX_ERR_NONE) return res;
            MoveBitstreamData(*in, (mfxU32)(ptr - beg));

            if (m_fcState.picHeader == FcState::FRAME)
            {
                m_fcState.picStart = 0;
                m_fcState.picHeader = FcState::NONE;
                memset(m_last_bytes,0,NUM_REST_BYTES);
                return MFX_ERR_NONE;
            }

            if (ptr[3] == PIC)
            {
                m_fcState.picHeader = FcState::FRAME;
                if(in->DataFlag == MFX_BITSTREAM_COMPLETE_FRAME)
                {
                    res = AppendBitstream(*out, ptr, 4);
                    if(res != MFX_ERR_NONE) return res;
                    MoveBitstreamData(*in, 4);
                    mfxU32 len = in->DataLength;
                    const mfxU8* beg1 = in->Data + in->DataOffset;
                    const mfxU8* ptr1 = FindStartCode(beg1, end);

                    if(ptr1 < beg1)
                        return MFX_ERR_UNDEFINED_BEHAVIOR;
                    if (ptr1[3] == SEQ  || ptr1[3] == EXT || ptr1[3] == GROUP || ptr1[3] == PIC)
                    {
                        len = (mfxU32)(ptr1 - beg1);
                    }
                    if (ptr1[3] == END)
                    {
                        ptr1 += 4;// append end_sequence_code to the end of current picture
                        len = (mfxU32)(ptr1 - beg1);
                    }
                    if(out->DataLength > out->MaxLength)
                        return MFX_ERR_UNDEFINED_BEHAVIOR;

                    mfxU32 len1 =  (mfxU32)(out->MaxLength - out->DataLength);
                    if(len1 < len)
                        len = len1;
                    res = AppendBitstream(*out, beg1, len);
                    if(res != MFX_ERR_NONE) return res;
                    MoveBitstreamData(*in, len);
                    m_fcState.picStart = 0;
                    m_fcState.picHeader = FcState::NONE;
                    memset(m_last_bytes,0,NUM_REST_BYTES);
                    return MFX_ERR_NONE;
                }
            }

            res = AppendBitstream(*out, ptr, 4);
            if(res != MFX_ERR_NONE) return res;
            MoveBitstreamData(*in, 4);
        }
    }
}

mfxStatus MPEG2_Spl::PrepareBuffer()
{
    VideoBs *video = &Video;
    mfxU8  *ptr;
    size_t size;

    if (m_frame.Data == NULL) {
      return MFX_ERR_NULL_PTR;
    }
    ptr = m_frame.Data;
    size = m_frame.DataLength;

    INIT_BITSTREAM(video->bs, ptr, ptr + size);

    return MFX_ERR_NONE;
}


mfxStatus MPEG2_Spl::FindSlice(mfxU32 *c)
{
  mfxU32 code;
  VideoBs *video = &Video;
  do {
    GET_START_CODE(video->bs, code);
    if(code >= 0x00000101 && code <= 0x000001AF)
    {
        *c = code;
        return MFX_ERR_NONE;
    }
  } while ( code != (mfxU32)MFX_ERR_MORE_DATA );
  return MFX_ERR_MORE_DATA;
}

mfxStatus MPEG2_Spl::ParseBsOnSlices()
{
    mfxU32 code;
    mfxStatus res = MFX_ERR_NONE;
    VideoBs *video = &Video;
    mfxU32 pic_type_mpeg2=0;
    SliceTypeCode pic_type=TYPE_UNKNOWN;

    res = PrepareBuffer();
    if(res != MFX_ERR_NONE)
        return res;

    mfxU8* ptr = video->bs_start_ptr;

    do
    {
        GET_START_CODE(video->bs, code);
        if(code == PICTURE_START_CODE)
        {
            SKIP_BITS(video->bs, 10)
            GET_TO9BITS(video->bs, 3, pic_type_mpeg2);
            if(pic_type_mpeg2 == 1)
                pic_type=TYPE_I;
            else if(pic_type_mpeg2 == 2)
                pic_type=TYPE_P;
            else if(pic_type_mpeg2 == 3)
                pic_type=TYPE_B;
            else
                return MFX_ERR_UNKNOWN;
            break;
        }
        if(code >= 0x00000101 && code <= 0x000001AF)
          break;
    } while ( code != (mfxU32)MFX_ERR_MORE_DATA);

    video->bs_curr_ptr = ptr;
    video->bs_bit_offset = 0;

    do
    {
        GET_START_CODE(video->bs, code);
        if(code == SEQUENCE_HEADER_CODE)
        {
            GET_BITS32(video->bs, code)
            m_height    = (code >> 8) & ((1 << 12) - 1);
            m_width     = (code >> 20) & ((1 << 12) - 1);
            ptr = GET_BYTE_PTR(video->bs);
            break;
        }
        if(code >= 0x00000101 && code <= 0x000001AF)
          break;
    } while ( code != (mfxU32)MFX_ERR_MORE_DATA);

    video->bs_curr_ptr = ptr;
    video->bs_bit_offset = 0;

    do
    {
        GET_START_CODE(video->bs, code);
        if(code == EXTENSION_START_CODE)
        {
            ptr = GET_BYTE_PTR(video->bs);
            SHOW_TO9BITS(video->bs, 4, code)
            if(code == SEQUENCE_EXTENSION_ID)
            {
                GET_BITS32(video->bs,code)
                m_width  = (m_width  & 0xfff)|((code >> (15-12)) & 0x3000);
                m_height = (m_height & 0xfff)|((code >> (13-12)) & 0x3000);
                ptr = GET_BYTE_PTR(video->bs);
                break;
            }
            else if(code == SEQUENCE_SCALABLE_EXTENSION_ID)
            {
                GET_TO9BITS(video->bs, 2, scalable_mode)//scalable_mode
                extension_start_code_ID = SEQUENCE_SCALABLE_EXTENSION_ID;
                break;
            }
            else if(code == PICTURE_CODING_EXTENSION_ID)
            {
                GET_BITS32(video->bs,code)
                picture_structure            = (code >> 8) & 3;
                break;
            }
        }
        if(code >= 0x00000101 && code <= 0x000001AF)
          break;
    } while ( code != (mfxU32)MFX_ERR_MORE_DATA);

    video->bs_curr_ptr = ptr;
    video->bs_bit_offset = 0;

    if(code == MFX_ERR_MORE_DATA)
        return MFX_ERR_MORE_DATA;

    for(;;)
    {
        mfxStatus res = FindSlice(&code);
        if(res == MFX_ERR_MORE_DATA)
            break;
        if(m_frame.SliceNum == 0)
        {
            m_first_slice_ptr = video->bs_curr_ptr-4;
        }

        m_frame.SliceNum++;

        if (m_slices.size() <= m_frame.SliceNum)
        {
            m_slices.resize(m_frame.SliceNum + 10);
            m_frame.Slice = &m_slices[0];
        }
    }
    //slice header parsing:
    video->bs_curr_ptr = m_first_slice_ptr;
    video->bs_bit_offset = 0;

    if(picture_structure != FRAME_PICTURE && m_bFirstField)
    {
        m_frame.FirstFieldSliceNum = m_frame.SliceNum-1;
        m_bFirstField = false;
    }
    else if(picture_structure != FRAME_PICTURE && !m_bFirstField)
    {
        m_bFirstField = true;
    }

    mfxI32 count = 0;
    int size = 0;
    first_slice_DataOffset = 0;

    for(;;){
        mfxStatus res = FindSlice(&code);
        if(res == MFX_ERR_MORE_DATA)
            break;

        ptr = GET_BYTE_PTR(video->bs)-4;//pointer to start code
        m_slices[count].DataOffset = (mfxU32)(ptr - GET_START_PTR(video->bs));

        if(!count)
            first_slice_DataOffset = m_slices[count].DataOffset;

        res = DecodeSliceHeader(code);
        if(res != MFX_ERR_NONE)
            return res;

        m_slices[count].HeaderLength = (mfxU32)(GET_BYTE_PTR(video->bs)-ptr);

        if(video->bs_bit_offset)
            m_slices[count].HeaderLength += 1;

        FIND_START_CODE(video->bs,code);

        if(code != (mfxU32)MFX_ERR_MORE_DATA)
            m_slices[count].DataLength = (mfxU32)(GET_BYTE_PTR(video->bs)-ptr);
        else
            m_slices[count].DataLength = (mfxU32)(GET_END_PTR(video->bs)-ptr);

        size += m_slices[count].DataLength;

        m_slices[count].SliceType = pic_type;

        if(code == MFX_ERR_MORE_DATA)
            break;

        count++;

    }

    return MFX_ERR_NONE;
}

mfxStatus MPEG2_Spl::DecodeSliceHeader(mfxU32 c)
{

    mfxU32 code;
    VideoBs *video = &Video;

    assert(c >= 0x00000101 && c <= 0x000001AF);

    mfxI32 slice_vertical_position = (c & 0xff);

    if(m_height > 2800)
    {
        if(slice_vertical_position > 0x80)
          return MFX_ERR_UNSUPPORTED;
        GET_TO9BITS(video->bs, 3, code)
        slice_vertical_position += code << 7;
    }

    if((extension_start_code_ID == SEQUENCE_SCALABLE_EXTENSION_ID) &&
        (scalable_mode == DATA_PARTITIONING))
    {
        GET_TO9BITS(video->bs, 7, code)
        return MFX_ERR_UNSUPPORTED;
    }

    GET_TO9BITS(video->bs, 5, code)//q_scale
    if(code == 0)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    mfxI32 extra_bit_slice = 0;

    GET_1BIT(video->bs,extra_bit_slice)
    while(extra_bit_slice)
    {
        GET_TO9BITS(video->bs, 9, code)
        extra_bit_slice = code & 1;
    }

    mfxI32 mb_col = 0;
    mfxI32 macroblock_address_increment = 0;

    mfxI32 remained = (mfxI32)GET_REMAINED_BYTES(video->bs);
    if (remained == 0) {
      return MFX_ERR_NONE;
    } else if (remained < 0) {
      return MFX_ERR_UNSUPPORTED;
    }

    if (IS_NEXTBIT1(video->bs)) {
      SKIP_BITS(video->bs, 1)
    } else {
      DECODE_MB_INCREMENT(video->bs, macroblock_address_increment);
      mb_col += macroblock_address_increment;
    }

    return MFX_ERR_NONE;
}

mfxStatus MPEG2_Spl::GetFrame(mfxBitstream * bs_in, FrameSplitterInfo ** frame)
{
    mfxStatus res = MFX_ERR_NONE;
    if(NULL == bs_in)
    {
        if(!m_bs.Data || !m_bs.DataLength)
        {
            return MFX_ERR_MORE_DATA;
        }
        int len = m_bs.DataLength;
        for(int i = 0; i < m_last_bytes[3]; i++)
            m_bs.Data[len + i] = m_last_bytes[i];
        m_bs.DataLength += m_last_bytes[3];
        m_last_bytes[3] = 0;
    }
    else
    {
        m_bs.Data = m_frame.Data;
        m_bs.DataOffset = m_frame.DataLength;
        m_bs.MaxLength = MAX_LEN;

        res = ConstructFrame(bs_in, &m_bs);
        if(res != MFX_ERR_NONE)
        {
            return res;
        }
    }

    m_frame.DataLength = m_bs.DataLength;
    m_bs.DataLength = 0;
    m_bs.DataOffset = 0;

    res = ParseBsOnSlices();
    if(res == MFX_ERR_NONE)
    {
        *frame = &m_frame;
    }

    return res;
}

    mfxStatus MPEG2_Spl::PostProcessing(FrameSplitterInfo *frame, mfxU32 sliceNum)
    {
        UNREFERENCED_PARAMETER(sliceNum);
        UNREFERENCED_PARAMETER(frame);
        return MFX_ERR_NONE;
    }
} // namespace ProtectedLibrary