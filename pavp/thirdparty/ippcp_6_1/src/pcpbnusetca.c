/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2004-2007 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Unsigned Big Number Operations
//
// Contents:
//    ippsSetOctString_BNU()
//    ippsSetHexString_BNU()
//
//    ippsGetOctString_BNU()
//    ippsGetHexString_BNU()
//
//
//    Created: Thu 10-Jun 2004 12:15
//  Author(s): Sergey Kirillov
*/
#if defined(_IPP_v50_)

#include "precomp.h"
#include "owncp.h"
#include "pcpbnuext.h"


/*
// Convert Octet String into BNU representation
*/
int OS_BNU(Ipp32u* pBNU, const Ipp8u* pOctStr, int strLen)
{
   int bnuSize=0;
   *pBNU = 0;

   /* start from the end of string */
   for(; strLen>=4; bnuSize++,strLen-=4) {
      /* pack 4 bytes into single Ipp32u value*/
      *pBNU++ = ( pOctStr[strLen-4]<<(8*3) )
               +( pOctStr[strLen-3]<<(8*2) )
               +( pOctStr[strLen-2]<<(8*1) )
               +  pOctStr[strLen-1];
   }

   /* convert the beginning of the string */
   if(strLen) {
      Ipp32u x;
      for(x=0; strLen>0; strLen--) {
         Ipp32u d = *pOctStr++;
         x = x*256 + d;
       }
       *pBNU++ = x;
       bnuSize++;
   }

   return bnuSize? bnuSize : 1;
}

/*
// Convert BNU into Octet String representation
*/
#define EBYTE(w,n) ((Ipp8u)((w) >> (8 * (n))))

int BNU_OS(Ipp8u* pStr, int strLen, const Ipp32u* pBNU, int bnuSize)
{
   FIX_BNU(pBNU, bnuSize);
   {
      int bnuBitSize = BNU_BITSIZE(pBNU, bnuSize);
      if(bnuBitSize <= strLen*BYTESIZE) {
         Ipp32u x = pBNU[bnuSize-1];

         cpMemset(pStr, 0, strLen);
         pStr += strLen - BITS2WORD8_SIZE(bnuBitSize);

         if(x) {
            int nb;
            for(nb=NLZ32u(x)/BYTESIZE; nb<4; nb++)
               *pStr++ = EBYTE(x,3-nb);

            for(--bnuSize; bnuSize>0; bnuSize--) {
               x = pBNU[bnuSize-1];
               *pStr++ = EBYTE(x,3);
               *pStr++ = EBYTE(x,2);
               *pStr++ = EBYTE(x,1);
               *pStr++ = EBYTE(x,0);
            }
         }
         return 1;
      }
      else
         return 0;
   }
}



/*F*
//    Name: ippsSetOctString_BNU
//
// Purpose: Convert octet string into the BNU value.
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pOctStr
//                               NULL == pBNU
//
//    ippStsLengthErr            0>strLen
//
//    ippStsSizeErr              *pBNUsize is enough for keep actual strLen
//
//    ippStsNoErr                no errors
//
// Parameters:
//    pOctStr     pointer to the source octet string
//    strLen      octet string length
//    pBNU        pointer to the target BNU
//    pBNUsize    pointer to the resultant BNU size (Ipp32u frgments)
//
// Notes:
//    1. if strLen==0, then BNU==0
//    2. on entry *pBNUsize contains available BNU size
//       on exit  *pBNUsize will contais actual size in Ipp32u fragments
//    3. octet string is string of bytes
//    4. example of conversion:
//       octet string: b[0],b[1],b[2],b[3],b[4]
//       BNU[0] = bytes_to_dword(b[1],b[2],b[3],b[4])
//       BNU[1] = bytes_to_dword(0,   0,   0,   b[0])
//
*F*/
IPPFUN(IppStatus, ippsSetOctString_BNU,(const Ipp8u* pOctStr, int strLen,
                                        Ipp32u* pBNU, int *pBNUsize))
{
   /* test size's pointer */
   IPP_BAD_PTR2_RET(pOctStr, pBNU);

   /* test octet string length */
   IPP_BADARG_RET((0>strLen), ippStsLengthErr);
   /* test available BNU space */
   IPP_BADARG_RET(strLen > (int)( sizeof(Ipp32u)*(*pBNUsize) ), ippStsSizeErr);

   *pBNUsize = OS_BNU(pBNU, pOctStr, strLen);
   return ippStsNoErr;
}


#if 0
/*F*
//    Name: ippsSetHexString_BNU
//
// Purpose: Convert hexadecimal string to the BNU value.
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pHexStr
//                               NULL == pBNU
//
//    ippStsSizeErr              0>strLen
//                               *pBNUsize is enough for keep actual strLen
//
//    ippStsOutOfRangeErr        pHexStr[] isn't a hex digit
//                               ('0','1','2','3','4','5','6','7',
//                                '8','9','a','b','c','d','e','f')
//
//    ippStsNoErr                no errors
//
// Parameters:
//    pHexStr     pointer to the source octet string
//    strLen      octet string length
//    pBNU        pointer to the target BNU
//    pBNUsize    pointer to the resultant BNU size (Ipp32u frgments)
//
// Notes:
//    1. if strLen==0, then BNU==0
//    2. on entry *pBNUsize contains available BNU size
//       on exit  *pBNUsize will contais actual size in Ipp32u fragments
//    3. hexadecimal string is string of hex characters
//    4. example of conversion:
//       hex string: "abcDEf0123" (the 0xabcdef0123 in "human" notation)
//       BNU[0] = 0xcdef0123
//       BNU[1] = 0xab
//
*F*/
static
int Char_HexDigit(Ipp8u c)
{
   if( ('0'<=c) && (c<='9') ) return c-'0';
   else if( ('a'<=c) && (c<='f') ) return 0xA + (c-'a');
   else if( ('A'<=c) && (c<='F') ) return 0xA + (c-'A');
   else return -1;
}

IPPFUN(IppStatus, ippsSetHexString_BNU, (
            const Ipp8u* pHexStr, int strLen,
                  Ipp32u* pBNU, int *pBNUsize))
{
   /* test size's pointer */
   IPP_BAD_PTR2_RET(pHexStr, pBNU);
   /* test octet string length */
   IPP_BADARG_RET((0>strLen), ippStsSizeErr);

   /* remove leading zeros */
   while(('0' ==(*pHexStr)) && strLen) {
      pHexStr++;
      strLen--;
   }

   /*
   // zero case
   */
   if(!strLen) {
      *pBNUsize = 1;
      pBNU[0] = 0;
      return ippStsNoErr;
   }

   /*
   // general case
   */
   else {
      /* necessary size to keep BNU */
      int bnuSize = (strLen+7)/8;

      /* test amount of reserved space */
      IPP_BADARG_RET( (*pBNUsize < bnuSize), ippStsSizeErr)

      /* conversion*/
      {
         Ipp32u x;

         /* start from the end of string */
         for(; strLen>=8; strLen-=8) {
            int n;
            /* pack 8 hex digits into single Ipp32u value*/
            for(x=0,n=8; n>0; n--) {
               int d = Char_HexDigit(pHexStr[strLen-n]);
               if(0>d) return ippStsOutOfRangeErr;
               x = x*16 + d;
            }
            *pBNU++ = x;
         }

         if(strLen) {
            for(x=0; strLen>0; strLen--) {
               int d = Char_HexDigit(*pHexStr++);
               if(0>d) return ippStsOutOfRangeErr;
               x = x*16 + d;
            }
            *pBNU++ = x;
         }

         *pBNUsize = bnuSize;
         return ippStsNoErr;
      }
   }
}
#endif


/*F*
//    Name: ippsGetOctString_BNU
//
// Purpose: Convert BNU value into the octet string.
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pOctStr
//                               NULL == pBNU
//
//    ippStsLengthErr            1>bnuSize
//
//    ippStsSizeErr              strLen is enough for keep BNU
//
//    ippStsNoErr                no errors
//
// Parameters:
//    pBNU        pointer to the source BNU
//    bnuSize     size of BNU (Ipp32u frgments)
//    pOctStr     pointer to the target octet string
//    strLen      alaivable octet string length
*F*/
IPPFUN(IppStatus, ippsGetOctString_BNU, (
            const Ipp32u* pBNU, int bnuSize,
                  Ipp8u* pStr, int strLen))
{
   /* test size's pointer */
   IPP_BAD_PTR2_RET(pStr, pBNU);
   /* test BNU size */
   IPP_BADARG_RET((1>bnuSize), ippStsLengthErr);

   return BNU_OS(pStr, strLen, pBNU, bnuSize)? ippStsNoErr : ippStsSizeErr;
}


#if 0
/*F*
//    Name: ippsGetHexString_BNU
//
// Purpose: Convert BNU value into the hexadecimal string.
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pHexStr
//                               NULL == pBNU
//
//    ippStsSizeErr              1>bnuSize
//                               *pStrLen is enough for keep BNU
//
//    ippStsNoErr                no errors
//
// Parameters:
//    pBNU        pointer to the source BNU
//    bnuSize     size of BNU (Ipp32u frgments)
//    pOctStr     pointer to the target hex string
//    pStrLen     pointer to the resultant hex string length
//
// Notes:
//    1. on entry *pStrLen contains available octet string length
//       on exit  *pStrLen will contais actual length of string
//    2. hex string contains ending 0x00
//
*F*/
IPPFUN(IppStatus, ippsGetHexString_BNU, (
            const Ipp32u* pBNU, int bnuSize,
                  Ipp8u* pHexStr, int* pStrLen))
{
   Ipp8u HexDgtTbl[] = "0123456789ABCDEF";

   /* test size's pointer */
   IPP_BAD_PTR2_RET(pHexStr, pBNU);
   /* test BNU size */
   IPP_BADARG_RET((1>bnuSize), ippStsSizeErr);
   /* test octet string length */
   FIX_BNU(pBNU,bnuSize);
   IPP_BADARG_RET(((bnuSize*4*2) >= *pStrLen), ippStsSizeErr);

   *pStrLen = bnuSize*4*2;

   for(; bnuSize>0; bnuSize--) {
      Ipp32u x = pBNU[bnuSize-1];

      int bIdx;
      for(bIdx=3; bIdx>=0; bIdx--) {
         Ipp8u b = EBYTE(x,bIdx);
         *pHexStr++ = HexDgtTbl[b>>4];
         *pHexStr++ = HexDgtTbl[b&0xF];
      }
   }
   *pHexStr = (Ipp8u)0;

   return ippStsNoErr;
}
#endif

#endif /* _IPP_v50_ */
