/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008 Intel Corporation. All Rights Reserved.
*/

#ifndef _GF128MUL_H
#define _GF128MUL_H

#define BUF_INC         8
#define BUF_ADRMASK     7
#define GF_BYTE_LEN     16

typedef Ipp64u uint128[2];

__INLINE void MoveBlockAligned(const void *pSrc, void *pDst)
{
   ((Ipp64u*)pDst)[0] = ((Ipp64u*)pSrc)[0];
   ((Ipp64u*)pDst)[1] = ((Ipp64u*)pSrc)[1];
}

__INLINE void XorBlockAligned(const void *pSrc1, const void *pSrc2, void *pDst)
{
   ((Ipp64u*)pDst)[0] = ((Ipp64u*)pSrc1)[0] ^ ((Ipp64u*)pSrc2)[0];
   ((Ipp64u*)pDst)[1] = ((Ipp64u*)pSrc1)[1] ^ ((Ipp64u*)pSrc2)[1];
}

void InitTable(const uint128 pSrc, uint128* pDst);
void gfMulBlock(const uint128 pSrc, uint128 pDst);
void gfMul4k(const uint128* pSrc, uint128 pDst);

#endif
