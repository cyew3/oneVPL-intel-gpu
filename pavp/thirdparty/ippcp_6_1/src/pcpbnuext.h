/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2003-2006 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Addition BNU Definitions & Function Prototypes
//
//    Created: Thu 09-Dec-2004 14:01
//  Author(s): Sergey Kirillov
//
*/
#if !defined(_PCP_BNUEXT_H)
#define _PCP_BNUEXT_H

#include "pcpbnu.h"

/*
// BNU useful Macros
*/

/* Copy BNU content */
#define COPY_BNU(src,dst,len) \
{ \
   int gresIdx; \
   for(gresIdx=0; gresIdx<(len); gresIdx++) (dst)[gresIdx] = (src)[gresIdx]; \
}

/* Expand and Copy and Expand by zeros */
#define ZEXPAND_BNU(srcdst,srcLen, dstLen) \
{ \
   int gresIdx; \
   for(gresIdx=(srcLen); gresIdx<(dstLen); gresIdx++) (srcdst)[gresIdx] = 0; \
}

#define ZEXPAND_COPY_BNU(src,srcLen, dst,dstLen) \
{ \
   int gresIdx; \
   for(gresIdx=0; gresIdx<(srcLen); gresIdx++) (dst)[gresIdx] = (src)[gresIdx]; \
   for(; gresIdx<(dstLen); gresIdx++)    (dst)[gresIdx] = 0; \
}

/* compare */
#define CMP_BNU(sign, pA, pB, len) \
{ \
   for((sign)=(len); (sign)>0; (sign)--) { \
      if( (pA)[(sign)-1] != (pB)[(sign)-1] ) \
         break; \
   } \
   (sign) = (sign)? ((pA)[(sign)-1] > (pB)[(sign)-1])? 1:-1 : 0; \
}

/* Fix actual length */
#define FIX_BNU(src,srcLen) \
   for(; ((srcLen)>1) && (0==(src)[(srcLen)-1]); (srcLen)--)

/* Add BNU and 'digit' inplace */
#define ADDC_BNU_I(pSrcDst, len, w) \
{\
   int gresIdx; \
   for(gresIdx=0; gresIdx<(len) && (w); gresIdx++) { \
      (pSrcDst)[gresIdx] += (w); \
      (w) = (pSrcDst)[gresIdx] < (w); \
   } \
}

/* Sub BNU and 'digit' inplace */
#define SUBC_BNU_I(pSrcDst, len, w) \
{\
   int gresIdx; \
   for(gresIdx=0; gresIdx<(len) && (w); gresIdx++) { \
      (w) = (((pSrcDst)[gresIdx] -= (w)) > (IPP_MAX_32U - (w))); \
   } \
}

/* bitsizes */
#define BNU_BITSIZE(p,ns)  ((ns)*32-NLZ32u((p)[(ns)-1]))

/* bit operations */
#define BNU_BIT(bnu, ns,nbit) ((((nbit)>>5) < (ns))? (((bnu)[(nbit)>>5] >>((nbit)&0x1F)) &1) : 0)
#define TST_BIT(bnu, nbit)    (((bnu)[(nbit)>>5]) &  (1<<((nbit)&0x1F)))
#define SET_BIT(bnu, nbit)    (((bnu)[(nbit)>>5]) |= (1<<((nbit)&0x1F)))
#define CLR_BIT(bnu, nbit)    (((bnu)[(nbit)>>5]) &=~(1<<((nbit)&0x1F)))


/* copy and set */
void Cpy_BNU(const Ipp32u* pSrc, Ipp32u* pDst, int size);
void Set_BNU(Ipp32u w, Ipp32u* pDst, int size);

/* shifts */
int LSL_BNU(const Ipp32u* pSrc, Ipp32u* pDst, int ns, int nBits);
int LSR_BNU(const Ipp32u* pSrc, Ipp32u* pDst, int ns, int nBits);

/* less and most signigicant bits/'digit' */
int LSB_BNU(const Ipp32u* pData, int size);
int MSB_BNU(const Ipp32u* pData, int size);
int MSD_BNU(const Ipp32u* pData, int size);

/* comparison */
int Cmp_BNU(const Ipp32u* pA, int aSize, const Ipp32u* pB, int bSize);
int Tst_BNU(const Ipp32u* pA, int aSize);

/* count lead/trail zeros */
int NLZ32u(Ipp32u x);
int NTZ32u(Ipp32u x);

/* BNU <-> Octect String Conversion */
int BNU_OS(Ipp8u* pStr, int strLen, const Ipp32u* pBNU, int bnuSize);
int OS_BNU(Ipp32u* pBNU, const Ipp8u* pStr, int strLen);

#endif /* _PCP_BNUEXT_H*/
