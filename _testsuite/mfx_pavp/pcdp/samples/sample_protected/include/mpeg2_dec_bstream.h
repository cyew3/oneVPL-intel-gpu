/*
//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//    Copyright (c) 2001-2012 Intel Corporation. All Rights Reserved.
//
//
//
//              Bit Stream operations
*/


#ifndef __MPEG2_DEC_BSTREAM_H
#define __MPEG2_DEC_BSTREAM_H

#include "mfxdefs.h"

namespace ProtectedLibrary
{

typedef struct
{
//Bitstream
    mfxU8*       bs_curr_ptr;
    mfxI32       bs_bit_offset;
    mfxU8*       bs_start_ptr;
    mfxU8*       bs_end_ptr;

} VideoBs;


typedef struct
{
  mfxI32 max_bits;
  mfxI32 bits_table0;
  mfxI32 bits_table1;
  mfxU32 threshold_table0;
  mfxI16 *table0;
  mfxI16 *table1;
} mp2_VLCTable;

#if (defined( _MSC_VER ) || defined( __ICL )) && (!defined(_WIN32_WCE))
#define DECLALIGN(x) __declspec(align(x))
#else
#define DECLALIGN(x)
#endif

#define VLC_BAD        0x80
#define VLC_NEXTTABLE  0x40


#define GET_BYTE_PTR(video) \
  video##_curr_ptr + ((video##_bit_offset + 7) >> 3)

#define GET_END_PTR(video) \
  video##_end_ptr

#define GET_START_PTR(video) \
  video##_start_ptr

#define GET_OFFSET(video) \
  (GET_BYTE_PTR(video) - video##_start_ptr)

#define GET_BIT_OFFSET(video) \
  ((video##_curr_ptr - video##_start_ptr)*8 + video##_bit_offset)

#define SET_PTR(video, PTR) {           \
  video##_curr_ptr = (mfxU8*)(PTR);      \
  video##_bit_offset = 0;                \
}

#define GET_REMAINED_BYTES(video) \
  (GET_END_PTR(video) - GET_BYTE_PTR(video))

#define INIT_BITSTREAM(video, start_ptr, end_ptr) { \
  video##_start_ptr = (mfxU8*)(start_ptr); \
  video##_end_ptr = (mfxU8*)(end_ptr); \
  SET_PTR(video, video##_start_ptr); \
}

#define SHOW_BITS_32(video, CODE) { \
  mfxU32 _code = \
  (video##_curr_ptr[0] << 24) | \
  (video##_curr_ptr[1] << 16) | \
  (video##_curr_ptr[2] << 8) | \
  (video##_curr_ptr[3]); \
  CODE = (_code << video##_bit_offset) | (video##_curr_ptr[4] >> (8 - video##_bit_offset)); \
}

#define SHOW_BITS_LONG(video, NUM_BITS, CODE) { \
  mfxU32 _code2; \
  SHOW_BITS_32(video, _code2); \
  CODE = _code2 >> (32 - NUM_BITS); \
}

#define SHOW_HI9BITS(video, CODE) \
  CODE = (((video##_curr_ptr[0] << 24) | (video##_curr_ptr[1] << 16)) << video##_bit_offset);

#define EXPAND_17BITS(video, CODE) \
  CODE |= video##_curr_ptr[2] << (8 + video##_bit_offset);

#define SHOW_HI17BITS(video, CODE) \
  CODE = (((video##_curr_ptr[0] << 24) | (video##_curr_ptr[1] << 16) | (video##_curr_ptr[3]<<8)) << video##_bit_offset);

#define EXPAND_25BITS(video, CODE) \
  CODE |= video##_curr_ptr[3] << video##_bit_offset;

#define SHOW_BITS_SHORT(video, NUM_BITS, CODE) { \
  mfxU32 _code; \
  SHOW_HI9BITS(video, _code); \
  CODE = _code >> (32 - NUM_BITS); \
}

#define SHOW_BITS(video, NUM_BITS, CODE) { \
  mfxU32 _code = \
  (video##_curr_ptr[0] << 24) | \
  (video##_curr_ptr[1] << 16) | \
  (video##_curr_ptr[2] << 8); \
  CODE = (_code << video##_bit_offset) >> (32 - NUM_BITS); \
}

#define SHOW_1BIT(video, CODE) { \
  CODE = (video##_curr_ptr[0] >> (7-video##_bit_offset)) & 1; \
}

#define IS_NEXTBIT1(video) \
  (video##_curr_ptr[0] & (0x80 >> video##_bit_offset))

#define SHOW_TO9BITS(video, NUM_BITS, CODE) { \
  mfxU32 _code = \
  (video##_curr_ptr[0] << 8) | (video##_curr_ptr[1] ); \
  CODE = (_code >> (16 - NUM_BITS - video##_bit_offset)) & ((1<<NUM_BITS)-1); \
}

#define SKIP_TO_END(video) \
  video##_curr_ptr = video##_end_ptr; \
  video##_bit_offset = 0;


#define SKIP_BITS(video, NUM_BITS) \
  video##_bit_offset += NUM_BITS; \
  video##_curr_ptr += (video##_bit_offset >> 3); \
  video##_bit_offset &= 7;

#define GET_BITS(video, NUM_BITS, CODE) { \
  SHOW_BITS(video, NUM_BITS, CODE); \
  SKIP_BITS(video, NUM_BITS); \
}

#define GET_TO9BITS(video, NUM_BITS, CODE) { \
  SHOW_TO9BITS(video, NUM_BITS, CODE); \
  SKIP_BITS(video, NUM_BITS); \
}

#define GET_BITS_LONG(video, NUM_BITS, CODE) { \
  SHOW_BITS_LONG(video, NUM_BITS, CODE); \
  SKIP_BITS(video, NUM_BITS); \
}

#define SKIP_BITS_LONG(video, NUM_BITS) \
  SKIP_BITS(video, NUM_BITS)

#define SKIP_BITS_32(video) \
  SKIP_BITS(video, 32);

#define GET_BITS32(video, CODE) { \
  SHOW_BITS_32(video, CODE) \
  SKIP_BITS_32(video); \
}

#define GET_1BIT(video, CODE) {\
  SHOW_1BIT(video, CODE) \
  SKIP_BITS(video, 1) \
}

#define UNGET_BITS_32(video) \
  video##_curr_ptr -= 4;

#define COPY_BITSTREAM(DST_BS, SRC_BS) \
  DST_BS##_curr_ptr = SRC_BS##_curr_ptr; \
  DST_BS##_bit_offset = SRC_BS##_bit_offset;

/***************************************************************/

#define FIND_START_CODE(video, code) {                          \
  mfxU8 *ptr = GET_BYTE_PTR(video);                             \
  mfxU8 *end_ptr = GET_END_PTR(video) - 3;                      \
  while (ptr < end_ptr && (ptr[0] || ptr[1] || (1 != ptr[2]))) {\
    ptr++;                                                      \
  }                                                             \
  if (ptr < end_ptr) {                                          \
    code = 256 + ptr[3];                                        \
    SET_PTR(video, ptr);                                        \
  } else {                                                      \
    code = (mfxU32)MFX_ERR_MORE_DATA;                           \
  }                                                             \
}

#define GET_START_CODE(video, code) { \
  FIND_START_CODE(video, code)        \
  if(code != (mfxU32)MFX_ERR_MORE_DATA) { \
    SKIP_BITS_32(video);                    \
  }                                         \
}

#define DECODE_VLC(value, bitstream, pVLC) \
{ \
  mfxU32 __code; \
  mfxI32 tbl_value; \
  mfxI32 bits_table0 = pVLC.bits_table0; \
  \
  SHOW_HI9BITS(bitstream, __code); \
  \
  tbl_value = pVLC.table0[__code >> (32 - bits_table0)]; \
  \
  if (tbl_value & (VLC_BAD | VLC_NEXTTABLE)) { \
    mfxI32 max_bits = pVLC.max_bits; \
    mfxI32 bits_table1 = pVLC.bits_table1; \
  \
    if (tbl_value & VLC_BAD) return MFX_ERR_UNSUPPORTED; \
    EXPAND_17BITS(bitstream, __code); \
    tbl_value = pVLC.table1[(__code >> (32 - max_bits)) & ((1 << bits_table1) - 1)]; \
  } \
  \
  value = tbl_value >> 8; \
  \
  tbl_value &= 0x3F; \
  SKIP_BITS(bitstream, tbl_value); \
}

}//namespace ProtectedLibrary
#endif /* __MPEG2_DEC_BSTREAM_H */

