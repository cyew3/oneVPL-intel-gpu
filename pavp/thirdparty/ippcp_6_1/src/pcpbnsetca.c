/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2004-2006 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Unsigned Big Number Operations
//
// Contents:
//    ippsSetOctString_BN()
//    ippsGetOctString_BN()
//
//
//    Created: Thu 10-Jun 2004 12:15
//  Author(s): Sergey Kirillov
*/
#if defined(_IPP_v50_)

#include "precomp.h"
#include "owncp.h"
#include "pcpbn.h"
#include "pcpbnuext.h"


/*F*
//    Name: ippsSetOctString_BN
//
// Purpose: Convert octet string into the BN value.
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pOctStr
//                               NULL == pBN
//
//    ippStsLengthErr            0>strLen
//
//    ippStsSizeErr              BN_ROOM() is enough for keep actual strLen
//
//    ippStsNoErr                no errors
//
// Parameters:
//    pOctStr     pointer to the source octet string
//    strLen      octet string length
//    pBN         pointer to the target BN
//
*F*/
IPPFUN(IppStatus, ippsSetOctString_BN,(const Ipp8u* pOctStr, int strLen,
                                       IppsBigNumState* pBN))
{
   /* test size's pointer */
   IPP_BAD_PTR2_RET(pOctStr, pBN);
   /* test octet string length */
   IPP_BADARG_RET((0>strLen), ippStsLengthErr);

   /* align BN context */
   pBN = (IppsBigNumState*)( IPP_ALIGNED_PTR(pBN, BN_ALIGNMENT) );
   IPP_BADARG_RET(!BN_VALID_ID(pBN), ippStsContextMatchErr);

   /* remove trailing zeros */
   //while(strLen && (0==pOctStr[strLen-1]))
   //   strLen--;
   /* remove leading zeros */
   while(strLen && (0==pOctStr[0])) {
      strLen--;
      pOctStr++;
   }

   /* test BN size */
   IPP_BADARG_RET((int)(sizeof(Ipp32u)*BN_ROOM(pBN))<strLen, ippStsSizeErr);

   BN_SIZE(pBN) = OS_BNU(BN_NUMBER(pBN), pOctStr, strLen);
   BN_SIGN(pBN) = IppsBigNumPOS;

   return ippStsNoErr;
}


/*F*
//    Name: ippsGetOctString_BN
//
// Purpose: Convert BN value into the octet string.
//
// Returns:                   Reason:
//    ippStsNullPtrErr           NULL == pOctStr
//                               NULL == pBN
//
//    ippStsRangeErr             BN <0
//
//    ippStsLengthErr            strLen is enough for keep BN value
//
//    ippStsNoErr                no errors
//
// Parameters:
//    pBN         pointer to the source BN
//    pOctStr     pointer to the target octet string
//    strLen      octet string length
*F*/
IPPFUN(IppStatus, ippsGetOctString_BN,(Ipp8u* pOctStr, int strLen,
                                       const IppsBigNumState* pBN))
{
   /* test BN */
   IPP_BAD_PTR1_RET(pBN);
   pBN = (IppsBigNumState*)( IPP_ALIGNED_PTR(pBN, BN_ALIGNMENT) );
   IPP_BADARG_RET(!BN_VALID_ID(pBN), ippStsContextMatchErr);
   IPP_BADARG_RET(BN_NEGATIVE(pBN), ippStsRangeErr);

   /* test string pointer */
   IPP_BAD_PTR1_RET(pOctStr);

   return BNU_OS(pOctStr,strLen, BN_NUMBER(pBN),BN_SIZE(pBN))? ippStsNoErr : ippStsLengthErr;
}

#endif /* _IPP_v50_ */
