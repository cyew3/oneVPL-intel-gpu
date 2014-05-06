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
//    EC (prime) Point
//
// Contents:
//    ippsECCPPointGetSize()
//    ippsECCPPointInit()
//
//
//    Created: Fri 27-Jun-2003 14:54
//  Author(s): Sergey Kirillov
*/
#if defined(_IPP_v50_)

#include "precomp.h"
#include "owncp.h"
#include "pcpeccppoint.h"


/*F*
//    Name: ippsECCPPointGetSize
//
// Purpose: Returns size of EC Point context (bytes).
//
// Returns:                Reason:
//    ippStsNullPtrErr        NULL == pSzie
//    ippStsSizeErr           2>feBitSize
//    ippStsNoErr             no errors
//
// Parameters:
//    feBitSize   size of field element (bits)
//    pSize       pointer to the size of EC Point context
//
*F*/
IPPFUN(IppStatus, ippsECCPPointGetSize, (int feBitSize, int* pSize))
{
   /* test size's pointer */
   IPP_BAD_PTR1_RET(pSize);

   /* test size of field element */
   IPP_BADARG_RET((2>feBitSize), ippStsSizeErr);

   {
      int bnSize;
      ippsBigNumGetSize(BITS2WORD32_SIZE(feBitSize), &bnSize);
      *pSize = sizeof(IppsECCPPointState)
              + bnSize              /* X coodinate */
              + bnSize              /* Y coodinate */
              + bnSize              /* Z coodinate */
              +(ALIGN_VAL-1);
   }
   return ippStsNoErr;
}


/*F*
//    Name: ippsECCPPointInit
//
// Purpose: Init EC Point context.
//
// Returns:                Reason:
//    ippStsNullPtrErr        NULL == pPoint
//    ippStsSizeErr           2>feBitSize
//    ippStsNoErr             no errors
//
// Parameters:
//    feBitSize   size of field element (bits)
//    pECC        pointer to ECC context
//
*F*/
IPPFUN(IppStatus, ippsECCPPointInit, (int feBitSize, IppsECCPPointState* pPoint))
{
   /* test pEC pointer */
   IPP_BAD_PTR1_RET(pPoint);

   /* use aligned context */
   pPoint = (IppsECCPPointState*)( IPP_ALIGNED_PTR(pPoint, ALIGN_VAL) );

   /* test size of field element */
   IPP_BADARG_RET((2>feBitSize), ippStsSizeErr);

   /* context ID */
   ECP_POINT_ID(pPoint) = idCtxECCPPoint;

   /* meaning: point was not set */
   ECP_POINT_AFFINE(pPoint) =-1;

   /*
   // init other context fields
   */
   {
      Ipp8u* ptr = (Ipp8u*)pPoint;
      int bnLen  = BITS2WORD32_SIZE(feBitSize);
      int bnSize;
      ippsBigNumGetSize(bnLen, &bnSize);

      /* allocate coordinate buffers */
      ptr += sizeof(IppsECCPPointState);
      ECP_POINT_X(pPoint) = (IppsBigNumState*)( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );
      ptr += bnSize;
      ECP_POINT_Y(pPoint) = (IppsBigNumState*)( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );
      ptr += bnSize;
      ECP_POINT_Z(pPoint) = (IppsBigNumState*)( IPP_ALIGNED_PTR(ptr,ALIGN_VAL) );

      /* init coordinate buffers */
      ippsBigNumInit(bnLen, ECP_POINT_X(pPoint));
      ippsBigNumInit(bnLen, ECP_POINT_Y(pPoint));
      ippsBigNumInit(bnLen, ECP_POINT_Z(pPoint));
   }
   return ippStsNoErr;
}

#endif /* _IPP_v50_ */
