//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2005-2014 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#include "h263.h"

/*
//  calculate number of bytes in buffer
*/
Ipp32s h263_RemainStream(h263_Info* pInfo)
{
   return (Ipp32s)(pInfo->buflen - (pInfo->bufptr - pInfo->buffer));
}

#ifndef H263_USE_INLINE_BITS_FUNC

Ipp32u h263_ShowBits(h263_Info* pInfo, Ipp32s n)
{
    Ipp8u* ptr = pInfo->bufptr;
    Ipp32u tmp = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] <<  8) | (ptr[3]);
    tmp <<= pInfo->bitoff;
    tmp >>= 32 - n;
    return tmp;
}

Ipp32u h263_ShowBit(h263_Info* pInfo)
{
    Ipp32u tmp = pInfo->bufptr[0];
    tmp >>= 7 - pInfo->bitoff;
    return (tmp & 1);
}

Ipp32u h263_ShowBits9(h263_Info* pInfo, Ipp32s n)
{
    Ipp8u* ptr = pInfo->bufptr;
    Ipp32u tmp = (ptr[0] <<  8) | ptr[1];
    tmp <<= (pInfo->bitoff + 16);
    tmp >>= 32 - n;
    return tmp;
}

void h263_FlushBits(h263_Info* pInfo, Ipp32s n)
{
    n = n + pInfo->bitoff;
    pInfo->bufptr += n >> 3;
    pInfo->bitoff = n & 7;
}

Ipp32u h263_GetBits(h263_Info* pInfo, Ipp32s n)
{
    Ipp8u* ptr = pInfo->bufptr;
    Ipp32u tmp = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] <<  8) | (ptr[3]);
    tmp <<= pInfo->bitoff;
    tmp >>= 32 - n;
    n = n + pInfo->bitoff;
    pInfo->bufptr += n >> 3;
    pInfo->bitoff = n & 7;
    return tmp;
}

Ipp32u h263_GetBits9(h263_Info* pInfo, Ipp32s n)
{
    Ipp8u* ptr = pInfo->bufptr;
    Ipp32u tmp = (ptr[0] << 8) | ptr[1];
    tmp <<= (pInfo->bitoff + 16);
    tmp >>= 32 - n;
    n = n + pInfo->bitoff;
    pInfo->bufptr += n >> 3;
    pInfo->bitoff = n & 7;
    return tmp;
}

/*
//  align bit stream to the byte boundary
*/
void h263_AlignBits(h263_Info* pInfo)
{
    if (pInfo->bitoff > 0) {
        pInfo->bitoff = 0;
        (pInfo->bufptr)++;
    }
}

Ipp32u h263_ShowBitsAlign(h263_Info* pInfo, Ipp32s n)
{
    Ipp8u* ptr = pInfo->bitoff ? (pInfo->bufptr + 1) : pInfo->bufptr;
    Ipp32u tmp = (ptr[0] << 24) | (ptr[1] << 16) | (ptr[2] <<  8) | (ptr[3]);
    tmp >>= 32 - n;
    return tmp;
}

#endif

/*
//  find H.263 start code in buffer
*/
Ipp8u* h263_FindStartCodePtr(h263_Info* pInfo)
{
    Ipp32s i, len = h263_RemainStream(pInfo);
    Ipp8u* ptr = pInfo->bufptr;
#ifndef UMC_H263_DISABLE_SORENSON
    Ipp8u  mask = pInfo->sorenson ? 0xF8 : 0xFC;
#else
    Ipp8u  mask = 0xFC;
#endif

    for (i = 0; i < len - 3; i++) {
      if (ptr[i] == 0 && ptr[i + 1] == 0 && (ptr[i + 2] & mask) == 0x80) {
        return ptr + i;
      }
    }
    return NULL;
}

/*
//  find picture start code in the stream
*/
Ipp32s h263_SeekStartCodePtr(h263_Info* pInfo)
{
    Ipp8u* ptr;

    if (pInfo->bitoff) {
        pInfo->bufptr ++;
        pInfo->bitoff = 0;
    }
    ptr = h263_FindStartCodePtr(pInfo);
    if (ptr) {
        pInfo->bufptr = ptr;
        return 1;
    } else {
        pInfo->bufptr = pInfo->buffer + pInfo->buflen - 3;
        return 0;
    }
}

Ipp32s h263_SeekGOBStartCodePtr(h263_Info* pInfo)
{
#ifndef UMC_H263_DISABLE_SORENSON
  if (pInfo->sorenson)
    return 0;
#endif
  for (; pInfo->bufptr < pInfo->buffer + pInfo->buflen - 2; pInfo->bufptr++) {
    if (pInfo->bufptr[0] == 0) {
      pInfo->bitoff = 0;
      if (pInfo->bufptr[0] == 0 && pInfo->bufptr[1] == 0 && (pInfo->bufptr[2] & 0xFC) == 0x80)
        return 0;
      pInfo->bufptr--;
      for (pInfo->bitoff = 1; pInfo->bitoff <= 7; pInfo->bitoff++) {
        if (h263_ShowBits(pInfo, 17) == 1)
          return 1;
      }
      pInfo->bufptr ++;
      for (pInfo->bitoff = 0; pInfo->bitoff <= 7; pInfo->bitoff++) {
        if (h263_ShowBits(pInfo, 17) == 1)
          return 1;
      }
      pInfo->bufptr ++;
    }
  }
  return 0;
}
