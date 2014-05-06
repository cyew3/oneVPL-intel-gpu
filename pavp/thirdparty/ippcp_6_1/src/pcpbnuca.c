/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2002-2003 Intel Corporation. All Rights Reserved.
//
//              Intel(R) Integrated Performance Primitives
//                  Cryptographic Primitives (ippcp)
//
// Purpose:
//    Unsigned Big Number Operations
//
// Contents:
//    ippsAdd_BNU(),    ippsSub_BNU()
//
//    ippsMulOne_BNU(), ippsMACOne_BNU()
//
//    ippsMul_BNU4(),   ippsSqr_BNU4()
//    ippsMul_BNU8(),   ippsSqr_BNU8()
//
//    ippsSqr_32u64u()
//    ippsDiv_64u32u()
//
//  2003/12/11 modified by Andrey Somsikov
//      Functions redesigned as preparation for opening the source
*/
#include "precomp.h"
#include "owncp.h"
#include "pcpbnu.h"


/*F*
//    Name: ippsAdd_BNU
//
// Purpose: Adds BNUs of same length.
//
// Returns:                Reason:
//    ippStsNoErr             no errors
//    ippStsNullPtrErr        a==NULL, b==NULL, r==NULL, carry==NULL
//    ippStsLengthErr         ns < 1
//
// Parameters:
//    a        pointer to BNU summand of ns segment length
//    b        pointer to BNU addend  of ns segment length
//    r        pointer to BNU result  of ns segment length
//    n        number of segments
//    carry    pointer to carry (0/1)
//
*F*/
IPPFUN(IppStatus, ippsAdd_BNU, (const Ipp32u *a, const Ipp32u *b, Ipp32u *r,
                                int n, Ipp32u *carry))
{
   IPP_BAD_PTR4_RET(a, b, r, carry);
   IPP_BADARG_RET(n <= 0, ippStsLengthErr);

   cpAdd_BNU(a, b, r, n, carry);

   return ippStsNoErr;
}


/*F*
//    Name: ippsSub_BNU
//
// Purpose: Subtracts BNUs of same length.
//
// Returns:                Reason:
//    ippStsNoErr             no errors
//    ippStsNullPtrErr        a==NULL, r==NULL, r==NULL, carry==NULL
//    ippStsLengthErr         n < 1
//    (ippStsSizeErr)
//
// Parameters:
//    a        pointer to BNU             of ns segment length
//    b        pointer to BNU subtrahend  of ns segment length
//    r        pointer to BNU result      of ns segment length
//    n        number of  segments
//    carry    pointer to borrow (0/1)
//
*F*/
IPPFUN(IppStatus, ippsSub_BNU, (const Ipp32u *a, const Ipp32u *b, Ipp32u *r,
                                int n, Ipp32u *carry))
{
   IPP_BAD_PTR4_RET(a, b, r, carry);
   IPP_BADARG_RET(n <= 0, ippStsLengthErr);

   cpSub_BNU(a, b, r, n, carry);

   return ippStsNoErr;
}


/*F*
//    Name: ippsMulOne_BNU
//
// Purpose: Multiplays "digit" by BNU.
//
// Returns:                Reason:
//    ippStsNoErr             no errors
//    ippStsNullPtrErr        a==NULL, pR==NULL, carry==NULL
//    ippStsLengthErr         n < 1
//
// Parameters:
//    a        pointer to BNU multiplicant of ns segment length
//    r        pointer to BNU result       of ns segment length
//    n        number of segments
//    w        multiplier (digit)
//    carry    pointer to expansion (whole Ipp32u range)
//
*F*/
IPPFUN(IppStatus, ippsMulOne_BNU, (const Ipp32u *a, Ipp32u *r, int n,
                                   Ipp32u w, Ipp32u *carry))
{
   IPP_BAD_PTR3_RET(a, r, carry);
   IPP_BADARG_RET(n <= 0, ippStsLengthErr);

   cpMulDgt_BNU(a, r, n, w, carry);

   return ippStsNoErr;
}


/*F*
//    Name: ippsMACOne_BNU_I
//
// Purpose: Multiplay "digit" by BNU
//          and accumulates.
//
// Returns:                Reason:
//    ippStsNoErr             no errors
//    ippStsNullPtrErr        a==NULL, r==NULL, carry==NULL
//    ippStsLengthErr         n < 1
//
// Parameters:
//    a        pointer to BNU multiplicant of ns segment length
//    r        pointer to BNU acc/result   of ns segment length
//    n        number of segments
//    w        multiplier (digit)
//    carry    pointer to expansion (whole Ipp32u range)
//
*F*/
IPPFUN(IppStatus, ippsMACOne_BNU_I, (const Ipp32u *a, Ipp32u *r, int n,
                                     Ipp32u w, Ipp32u *carry))
{
   IPP_BAD_PTR3_RET(a, r, carry);
   IPP_BADARG_RET(n <= 0, ippStsLengthErr);

   cpAddMulDgt_BNU(a, r, n, w, carry);

   return ippStsNoErr;
}


/*F*
//    Name: ippsMul_BNU4
//
// Purpose: Multiplays BNUs of 4 segments length.
//
// Returns:                Reason:
//    ippStsNoErr             no errors
//    ippStsNullPtrErr        a==NULL, b==NULL, r==NULL
//
// Parameters:
//    a        pointer to BNU multiplicant
//    b        pointer to BNU multiplier
//    r        pointer to BNU result
//
*F*/
IPPFUN(IppStatus, ippsMul_BNU4, (const Ipp32u *a, const Ipp32u *b, Ipp32u *r))
{
   IPP_BAD_PTR3_RET(a, b, r);

   cpMul_BNU4(a,b,r);

   return ippStsNoErr;
}


/*F*
//    Name: ippsMul_BNU8
//
// Purpose: Multiplays BNUs of 8 segments length.
//
// Returns:                Reason:
//    ippStsNoErr             no errors
//    ippStsNullPtrErr        a == NULL, b==NULL, r==NULL
//
// Parameters:
//    a        pointer to BNU multiplicant
//    b        pointer to BNU multiplier
//    r        pointer to BNU result
//
*F*/
IPPFUN(IppStatus, ippsMul_BNU8, (const Ipp32u *a, const Ipp32u *b, Ipp32u *r))
{
   IPP_BAD_PTR3_RET(a, b, r);

   cpMul_BNU8(a,b,r);

   return ippStsNoErr;
}


/*F*
//    Name: ippsSqr_BNU4
//
// Purpose: Square BNU of 4 segments length.
//
// Returns:                Reason:
//    ippStsNoErr             no errors
//    ippStsNullPtrErr        a== NULL, r==NULL
//
// Parameters:
//    a        pointer to BNU to be square
//    r        pointer to BNU result
//
*F*/
IPPFUN(IppStatus, ippsSqr_BNU4, (const Ipp32u *a, Ipp32u *r))
{
   IPP_BAD_PTR2_RET(a, r);

   cpSqr_BNU4(a, r);

   return ippStsNoErr;
}


/*F*
//    Name: ippsSqr_BNU8
//
// Purpose: Square BNU of 8 segments length.
//
// Returns:                Reason:
//    ippStsNoErr             no errors
//    ippStsNullPtrErr        a==NULL, r==NULL
//
// Parameters:
//    a        pointer to BNU to be square
//    r        pointer to BNU result
//
*F*/
IPPFUN(IppStatus, ippsSqr_BNU8, (const Ipp32u *a, Ipp32u *r))
{
   IPP_BAD_PTR2_RET(a, r);

   cpSqr_BNU8(a, r);

   return ippStsNoErr;
}


/*F*
//    Name: ippsDiv_64u32u
//
// Purpose: Computes an integer division for a 64u dividend
//          by a 32u divisor
//
// Returns:                Reason:
//    ippStsNoErr             no errors
//    ippStsNullPtrErr        pQ==NULL, pR==NULL
//    ippStsDivByZeroErr      zero devisor (b)
//    ippStsOutOfRangeErr     quotient too big
//
// Parameters:
//    pSrc     pointer to source 32u vector
//    nSamples vector length (number of items)
//    pDst     pointer to target 64u vector
//
*F*/
IPPFUN(IppStatus, ippsDiv_64u32u,(Ipp64u a, Ipp32u b, Ipp32u* pQ, Ipp32u *pR))
{
   /* test target pointers */
   IPP_BAD_PTR2_RET(pQ, pR);
   /* test divisor */
   IPP_BADARG_RET((b==0), ippStsDivByZeroErr);

   if( HIDWORD(a) >= b )
      return ippStsOutOfRangeErr;
   else {
      Ipp64u qr = cpDiv64u32u(a, b);
      *pR = LODWORD(qr);
      *pQ = HIDWORD(qr);
      return ippStsNoErr;
   }
}


/*F*
//    Name: ippsSqr_32u64u
//
// Purpose: Square of unsigned 32u vector of nSamples length
//
// Returns:                Reason:
//    ippStsNoErr             no errors
//    ippStsNullPtrErr        src==NULL, dst==NULL
//    ippStsLengthErr         n < 1
//
// Parameters:
//    src      pointer to source 32u vector
//    n        vector length (number of items)
//    dst      pointer to target 64u vector
//
*F*/
IPPFUN(IppStatus, ippsSqr_32u64u, (const Ipp32u *src, int n, Ipp64u *dst))
{
    int i;

    IPP_BAD_PTR2_RET(src, dst);
    IPP_BADARG_RET(n <= 0, ippStsLengthErr);

    for(i=0; i<n; i++)
        dst[i] = (Ipp64u)src[i] * (Ipp64u)src[i];

    return ippStsNoErr;
}
