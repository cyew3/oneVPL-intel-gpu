/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2002-2008 Intel Corporation. All Rights Reserved.
//
//              Intel(R) Integrated Performance Primitives
//                  Cryptographic Primitives (ippcp)
//
//  File:       pcpbnca.c
//  Created:    2003/12/11 11:43
//
//  2003/12/11 modified by Andrey Somsikov
//      Functions redesigned as preparation for opening the source
*/
#include "precomp.h"
#include "owncp.h"
#include "pcpbn.h"



/*F*
//    Name: ippsBigNumGetSize
//
// Purpose: Returns size of BigNum spec (bytes).
//
// Returns:                Reason:
//    ippStsNullPtrErr        size == NULL
//    ippStsLengthErr         length < 1
//    ippStsNoErr             no errors
//
// Parameters:
//    size        pointer BigNum spec size
//
*F*/
IPPFUN(IppStatus, ippsBigNumGetSize, (int length, int *size))
{
   int len;

   IPP_BAD_PTR1_RET(size);
   IPP_BADARG_RET(length <= 0, ippStsLengthErr);

   /* ensure that length divides by BNUBASE_TYPE_SIZE */
   //gres: len = IPP_MULTIPLE_OF(length, BNUBASE_TYPE_SIZE);
   len = length+1; // gres: cpDiv_BNU, multiplication
   len = IPP_MULTIPLE_OF(len, BNUBASE_TYPE_SIZE);

   *size = sizeof(IppsBigNumState) + 2*len*sizeof(Ipp32u) + BN_ALIGN_SIZE;
   IPP_MAKE_MULTIPLE_OF_16(*size);

   return ippStsNoErr;
}


/*F*
//    Name: ippsBigNumInit
//
// Purpose: Init BigNum spec for future usage.
//
// Returns:                Reason:
//    ippStsNullPtrErr        b == NULL
//    ippStsLengthErr         length <1
//    ippStsNoErr             no errors
//
// Parameters:
//    length      max BN length (32-bits segments)
//    b           BigNum spec pointer
//
*F*/
IPPFUN(IppStatus, ippsBigNumInit, (int length, IppsBigNumState* b))
{
   int len;

   IPP_BAD_PTR1_RET(b);
   IPP_MAKE_ALIGNED_BN(b);
   IPP_BADARG_RET(length <= 0, ippStsLengthErr);

   /* ensure that length divides by BNUBASE_TYPE_SIZE */
   //gres: len = IPP_MULTIPLE_OF(length, BNUBASE_TYPE_SIZE);
   len = length+1; // gres: cpDiv_BNU, multiplication
   len = IPP_MULTIPLE_OF(len, BNUBASE_TYPE_SIZE);

   b->idCtx = idCtxBigNum;
   b->size = 0;
   b->buffer_size = length; /* Set length exactly passed by user */
   b->number = (Ipp32u*)((Ipp8u*)b + sizeof(IppsBigNumState));
   b->workBuffer  = (Ipp32u*)(b->number + len); /* use modified length here */

   return ippStsNoErr;
}


/*
// ----------------------------------------------------------------------------
// Scans the data field of the input const IppsBigNumState *b and report IS_ZERO,
// if the value held by IppsBigNumState *b is zero. In case of nonzero, it reports
// GREATER_THAN_ZERO, if the input is greater than zero or LESS_THAN_ZERO if
// the input is less than zero.
// ----------------------------------------------------------------------------
*/
IPPFUN(IppStatus, ippsCmpZero_BN, (const IppsBigNumState *b, Ipp32u *result))
{
    IPP_BAD_PTR2_RET(b, result);
    IPP_MAKE_ALIGNED_BN(b);
    IPP_BADARG_RET(!BN_VALID_ID(b), ippStsContextMatchErr);

    if (b->size == 1 && b->number[0] == 0)
        *result = IS_ZERO;
    else if (b->sgn == IppsBigNumPOS)
        *result = GREATER_THAN_ZERO;
    else if (b->sgn == IppsBigNumNEG)
        *result = LESS_THAN_ZERO;

    return ippStsNoErr;
}

IPPFUN(IppStatus, ippsCmp_BN,(const IppsBigNumState* pA, const IppsBigNumState* pB, Ipp32u *pResult))
{
   int res;

   IPP_BAD_PTR3_RET(pA,pB, pResult);
   IPP_MAKE_ALIGNED_BN(pA);
   IPP_MAKE_ALIGNED_BN(pB);
   IPP_BADARG_RET(!BN_VALID_ID(pA), ippStsContextMatchErr);
   IPP_BADARG_RET(!BN_VALID_ID(pB), ippStsContextMatchErr);

   if(BN_SIGN(pA)==BN_SIGN(pB)) {
      res = cpCompare_BNUs(BN_NUMBER(pA), (int)BN_SIZE(pA),
                           BN_NUMBER(pB), (int)BN_SIZE(pB));
      if(IppsBigNumNEG==BN_SIGN(pA))
         res = -res;
   }
   else
      res = (IppsBigNumPOS==BN_SIGN(pA))? 1 :-1;

   if(1==res)
      *pResult = GREATER_THAN_ZERO;
   else if(-1==res)
      *pResult = LESS_THAN_ZERO;
   else
      *pResult = IS_ZERO;

   return ippStsNoErr;
}


IPPFUN(IppStatus, ippsGetSize_BN, (const IppsBigNumState *b, int *size))
{
    IPP_BAD_PTR2_RET(b, size);
    IPP_MAKE_ALIGNED_BN(b);
    IPP_BADARG_RET(!BN_VALID_ID(b), ippStsContextMatchErr);

    *size = b->buffer_size;

    return ippStsNoErr;
}


/*F*
//    Name: ippsSet_BN
//
// Purpose: Set BigNum.
//
// Returns:                Reason:
//    ippStsNullPtrErr        x == NULL, data == NULL
//    ippStsContextMatchErr   x->idCtx != idCtxBigNum
//    ippStsLengthErr         length <1
//    ippStsOutOfRangeErr     length >max BN length
//    ippStsNoErr             no errors
//
// Parameters:
//    sgn         sign
//    length      data size (32-bits segments)
//    data        source data pointer
//    x           BigNum spec pointer
//
*F*/
IPPFUN(IppStatus, ippsSet_BN, (IppsBigNumSGN sgn, int length, const Ipp32u *data,
                               IppsBigNumState *x))
{
    int i;

    IPP_BAD_PTR2_RET(data, x);
    IPP_MAKE_ALIGNED_BN(x);
    IPP_BADARG_RET(!BN_VALID_ID(x), ippStsContextMatchErr);
    IPP_BADARG_RET( length <= 0, ippStsLengthErr);
    IPP_BADARG_RET( length > (int)x->buffer_size, ippStsOutOfRangeErr);

    /* compute real size */
    for (i = length - 1; i > 0; i--)
    {
        if (data[i] != 0)
            break;
    }
    x->size = i + 1;

    if (x->size == 1 && data[0] == 0 && sgn == IppsBigNumNEG)
        x->sgn = IppsBigNumPOS; /* zero must be always positive */
    else
        x->sgn = sgn;

    for (;i >= 0; i--)
        x->number[i] = data[i];

    return ippStsNoErr;
}


/*F*
//    Name: ippsGet_BN
//
// Purpose: Get BigNum.
//
// Returns:                Reason:
//    ippStsNullPtrErr        x == NULL, data == NULL, sgn == NULL, lenght==NULL
//    ippStsContextMatchErr   x->idCtx != idCtxBigNum
//    ippStsNoErr             no errors
//
// Parameters:
//    sgn         pointer to the sign
//    length      pointer to the data size (32-bits segments)
//    data        pointer to the data buffer
//    x           BigNum spec pointer
//
*F*/
IPPFUN(IppStatus, ippsGet_BN, (IppsBigNumSGN *sgn, int *length, Ipp32u *data,
                               const IppsBigNumState *x))
{
   IPP_BAD_PTR4_RET(sgn, length, data, x);
   IPP_MAKE_ALIGNED_BN(x);
   IPP_BADARG_RET(!BN_VALID_ID(x), ippStsContextMatchErr);

   *sgn = x->sgn;
   *length = x->size;
   cpMemcpy32u(data, x->number, x->size);

   return ippStsNoErr;
}

IPPFUN(IppStatus, ippsRef_BN, (IppsBigNumSGN *sgn, int *bitSize, Ipp32u** const ppData,
                               const IppsBigNumState *x))
{
   IPP_BAD_PTR1_RET(x);
   IPP_MAKE_ALIGNED_BN(x);
   IPP_BADARG_RET(!BN_VALID_ID(x), ippStsContextMatchErr);

   if(sgn)
      *sgn = x->sgn;
   if(bitSize)
      *bitSize= x->size*BITSIZE(Ipp32u) - NLZ32u(x->number[x->size-1]);

   if(ppData)
      *ppData = x->number;

   return ippStsNoErr;
}

IPPFUN(IppStatus, ippsExtGet_BN, (IppsBigNumSGN *sgn, int *bitSize, Ipp32u *data,
                               const IppsBigNumState *x))
{
   IPP_BAD_PTR1_RET(x);
   IPP_MAKE_ALIGNED_BN(x);
   IPP_BADARG_RET(!BN_VALID_ID(x), ippStsContextMatchErr);

   if(sgn)
      *sgn = x->sgn;
   if(bitSize)
      *bitSize= x->size*BITSIZE(Ipp32u) - NLZ32u(x->number[x->size-1]);

   if(data)
      cpMemcpy32u(data, x->number, x->size);

   return ippStsNoErr;
}


/*F*
//    Name: ippsAdd_BN
//
// Purpose: Add BigNum.
//
// Returns:                Reason:
//    ippStsNullPtrErr        a  == NULL
//                            b  == NULL
//                            r  == NULL
//    ippStsContextMatchErr   a->idCtx != idCtxBigNum
//                            b->idCtx != idCtxBigNum
//                            r->idCtx != idCtxBigNum
//    ippStsOutOfRangeErr     r can not fit result
//    ippStsNoErr             no errors
//
// Parameters:
//    a           source BigNum spec pointer
//    b           source BigNum spec pointer
//    r           resultant BigNum spec pointer
//
*F*/
IPPFUN(IppStatus, ippsAdd_BN, (IppsBigNumState *a, IppsBigNumState *b, IppsBigNumState *r))
{
    IPP_BAD_PTR3_RET(a, b, r);
    IPP_MAKE_ALIGNED_BN(a);
    IPP_MAKE_ALIGNED_BN(b);
    IPP_MAKE_ALIGNED_BN(r);
    IPP_BADARG_RET(!BN_VALID_ID(a), ippStsContextMatchErr);
    IPP_BADARG_RET(!BN_VALID_ID(b), ippStsContextMatchErr);
    IPP_BADARG_RET(!BN_VALID_ID(r), ippStsContextMatchErr);
    IPP_BADARG_RET(r->buffer_size < IPP_MAX(a->size, a->size), ippStsOutOfRangeErr);

    if(a->sgn == b->sgn)
    {
        if (cpAdd_BNUs(a->number, a->size, b->number, b->size,
                r->number, &r->size, r->buffer_size) == 0)
            return ippStsOutOfRangeErr;
        r->sgn = a->sgn;
    }
    else
    {
        IppsBigNumSGN sgn;
        cpSub_BNUs(a->number, a->size, b->number, b->size, r->number, &r->size, &sgn);
        if (a->sgn == IppsBigNumPOS)
            r->sgn = sgn;
        else
            r->sgn = (sgn == IppsBigNumPOS ? IppsBigNumNEG : IppsBigNumPOS);
    }

    if (r->size == 1 && r->number[0] == 0)
        r->sgn = IppsBigNumPOS;

    return ippStsNoErr;
}


/*F*
//    Name: ippsSub_BN
//
// Purpose: Subtect BigNum.
//
// Returns:                Reason:
//    ippStsNullPtrErr        a == NULL
//                            b == NULL
//                            r == NULL
//    ippStsContextMatchErr   a->idCtx != idCtxBigNum
//                            b->idCtx != idCtxBigNum
//                            r->idCtx != idCtxBigNum
//    ippStsOutOfRangeErr     r can not fit result
//    ippStsNoErr             no errors
//
// Parameters:
//    a           source BigNum spec pointer
//    b           source BigNum spec pointer
//    r           resultant BigNum spec pointer
//
*F*/
IPPFUN(IppStatus, ippsSub_BN, (IppsBigNumState *a, IppsBigNumState *b, IppsBigNumState *r))
{
    IPP_BAD_PTR3_RET(a, b, r);
    IPP_MAKE_ALIGNED_BN(a);
    IPP_MAKE_ALIGNED_BN(b);
    IPP_MAKE_ALIGNED_BN(r);
    IPP_BADARG_RET(!BN_VALID_ID(a), ippStsContextMatchErr);
    IPP_BADARG_RET(!BN_VALID_ID(b), ippStsContextMatchErr);
    IPP_BADARG_RET(!BN_VALID_ID(r), ippStsContextMatchErr);
    IPP_BADARG_RET(r->buffer_size < IPP_MAX(a->size, a->size), ippStsOutOfRangeErr);

    if(a->sgn != b->sgn)
    {
        if (cpAdd_BNUs(a->number, a->size, b->number, b->size,
                r->number, &r->size, r->buffer_size) == 0)
            return ippStsOutOfRangeErr;
        r->sgn = a->sgn;
    }
    else
    {
        IppsBigNumSGN sgn;
        cpSub_BNUs(a->number, a->size, b->number, b->size, r->number, &r->size, &sgn);
        if (a->sgn == IppsBigNumPOS)
            r->sgn = sgn;
        else
            r->sgn = (sgn == IppsBigNumPOS ? IppsBigNumNEG : IppsBigNumPOS);
    }

    if (r->size == 1 && r->number[0] == 0)
        r->sgn = IppsBigNumPOS;

    return ippStsNoErr;
}


/*F*
//    Name: ippsMul_BN
//
// Purpose: Multiply BigNum.
//
// Returns:                Reason:
//    ippStsNullPtrErr        a == NULL
//                            b == NULL
//                            r == NULL
//    ippStsContextMatchErr   a->idCtx != idCtxBigNum
//                            b->idCtx != idCtxBigNum
//                            r->idCtx != idCtxBigNum
//    ippStsOutOfRangeErr     r can not fit result
//    ippStsNoErr             no errors
//
// Parameters:
//    a           source BigNum spec pointer
//    b           source BigNum spec pointer
//    r           resultant BigNum spec pointer
//
*F*/
IPPFUN(IppStatus, ippsMul_BN, (IppsBigNumState *a, IppsBigNumState *b, IppsBigNumState *r))
{
    int i;
    Ipp32u *number_a, *number_b;
    int size_r;

    IPP_BAD_PTR3_RET(a, b, r);
    IPP_MAKE_ALIGNED_BN(a);
    IPP_MAKE_ALIGNED_BN(b);
    IPP_MAKE_ALIGNED_BN(r);
    IPP_BADARG_RET(!BN_VALID_ID(a), ippStsContextMatchErr);
    IPP_BADARG_RET(!BN_VALID_ID(b), ippStsContextMatchErr);
    IPP_BADARG_RET(!BN_VALID_ID(r), ippStsContextMatchErr);
    IPP_BADARG_RET(r->buffer_size < (a->size + b->size - 1), ippStsOutOfRangeErr);
/*
//    IPP_BADARG_RET(
//        (a->number == r->number || b->number == r->number) && !r->workBuffer,
//        ippStsNullPtrErr);
*/

    if (a->number == r->number && a->number != b->number)
    {
        number_a        = r->workBuffer;
        for (i = 0; i < a->size; i++)
            number_a[i] = a->number[i];

        number_b        = b->number;
    }
    else if (b->number == r->number && a->number != b->number)
    {
        number_b        = r->workBuffer;
        for (i = 0; i < b->size; i++)
            number_b[i] = b->number[i];

        number_a        = a->number;
    }
    else if (a->number == r->number && b->number == r->number)
    {
        IPP_BADARG_RET(r->buffer_size < a->size + b->size, ippStsOutOfRangeErr);

        number_a        = r->workBuffer;
        for (i = 0; i < a->size; i++)
            number_a[i] = a->number[i];

        number_b        = r->workBuffer + a->size;
        for (i = 0; i < b->size; i++)
            number_b[i] = b->number[i];
    }
    else
    {
        number_a        = a->number;
        number_b        = b->number;
    }

    size_r = r->buffer_size;
    if (cpMul_BNUs(number_a, a->size, number_b, b->size, r->number, &(size_r)) == 0)
        return ippStsOutOfRangeErr;
    r->size = size_r;
    r->sgn = (a->sgn == b->sgn ? IppsBigNumPOS : IppsBigNumNEG);

    return ippStsNoErr;
}


/*F*
//    Name: ippsMAC_BN_I
//
// Purpose: Multiply and Accumulate BigNum.
//
// Returns:                Reason:
//    ippStsNullPtrErr        a == NULL
//                            b == NULL
//                            r == NULL
//    ippStsContextMatchErr   a->idCtx != idCtxBigNum
//                            b->idCtx != idCtxBigNum
//                            r->idCtx != idCtxBigNum
//    ippStsOutOfRangeErr     r can not fit result
//    ippStsNoErr             no errors
//
// Parameters:
//    a           source BigNum spec pointer
//    b           source BigNum spec pointer
//    r           resultant BigNum spec pointer
//
*F*/
IPPFUN(IppStatus, ippsMAC_BN_I, (IppsBigNumState *a, IppsBigNumState *b, IppsBigNumState *r))
{
    int k = 0;
    int j, i, t;
    Ipp32u carry;
    IppsBigNumSGN   a__sgn, b__sgn;
    int             a__size, b__size;
    Ipp32u         *a__number, *b__number;

    IPP_BAD_PTR3_RET(a, b, r);
    IPP_MAKE_ALIGNED_BN(a);
    IPP_MAKE_ALIGNED_BN(b);
    IPP_MAKE_ALIGNED_BN(r);
    IPP_BADARG_RET(!BN_VALID_ID(a), ippStsContextMatchErr);
    IPP_BADARG_RET(!BN_VALID_ID(b), ippStsContextMatchErr);
    IPP_BADARG_RET(!BN_VALID_ID(r), ippStsContextMatchErr);
    IPP_BADARG_RET(r->buffer_size < (a->size + b->size - 1), ippStsOutOfRangeErr);
    IPP_BADARG_RET(
        (a->number == r->number || b->number == r->number) && !r->workBuffer,
        ippStsNullPtrErr);

    if (a->number == r->number && a->number != b->number)
    {
        a__sgn           = a->sgn;
        a__size          = a->size;
        a__number        = r->workBuffer;
        for (i = 0; i < a__size; i++)
            a__number[i] = a->number[i];

        b__sgn           = b->sgn;
        b__size          = b->size;
        b__number        = b->number;
    }
    else if (b->number == r->number && a->number != b->number)
    {
        a__sgn           = a->sgn;
        a__size          = a->size;
        a__number        = a->number;

        b__sgn           = b->sgn;
        b__size          = b->size;
        b__number        = r->workBuffer;
        for (i = 0; i < b__size; i++)
            b__number[i] = b->number[i];
    }
    else if (a->number == r->number && b->number == r->number)
    {
        IPP_BADARG_RET(r->buffer_size < a->size + b->size, ippStsOutOfRangeErr);
        a__sgn           = a->sgn;
        a__size          = a->size;
        a__number        = r->workBuffer;
        for (i = 0; i < a__size; i++)
            a__number[i] = a->number[i];

        b__sgn           = b->sgn;
        b__size          = b->size;
        b__number        = r->workBuffer + a->size;
        for (i = 0; i < b__size; i++)
            b__number[i] = b->number[i];
    }
    else
    {
        a__sgn           = a->sgn;
        a__size          = a->size;
        a__number        = a->number;

        b__sgn           = b->sgn;
        b__size          = b->size;
        b__number        = b->number;
    }

    /* cleanup the rest of destination buffer */
    for (i = r->size; i < r->buffer_size; i++)
        r->number[i] = 0;

    if ((r->sgn == IppsBigNumNEG) ^ ((a__sgn == IppsBigNumPOS) ^ (b__sgn == IppsBigNumPOS)))
    {
        /* Substraction */
        carry = 0;
        for (j = 0; j < b__size; j++)
        {

            cpSubMulDgt_BNU(a__number, r->number + j, a__size, b__number[j], &carry);
            if (carry != 0)
            {
                k = a__size + j;
                do
                {
                    if (k >= r->buffer_size)
                        break;
                    carry = ((r->number[k] -= carry) > (IPP_MAX_32U - carry));
                    k++;
                } while (carry != 0);
                if (carry)
                    break;
            }
        }

        if (carry)
        {
            if (carry != 1)
                return ippStsOutOfRangeErr;

            /* r = -r  */
            for (t = 0; t < k && r->number[t] == 0; t++)
                ;
            r->number[t] = 0 - r->number[t];
            for (++t; t < k; t++)
                r->number[t] = 0 - r->number[t] - 1;

            /* continue with addition operation */
            carry = 0;
            for (++j; j < b__size; j++)
            {
                cpAddMulDgt_BNU(a__number, r->number + j, a__size, b__number[j], &carry);
                if (carry != 0)
                {
                    k = a__size + j;
                    do
                    {
                        if (k >= r->buffer_size)
                            return ippStsOutOfRangeErr;
                        carry = ((r->number[k] += carry) < carry);
                        k++;
                    } while (carry != 0);
                }
            }
            r->sgn = r->sgn == IppsBigNumPOS ? IppsBigNumNEG : IppsBigNumPOS;
                /* if (carry == 0) r->sgn remains unchanged */
        }

    }
    else
    {
        /* Addition */
        carry = 0;
        for (j = 0; j < b__size; j++)
        {
            cpAddMulDgt_BNU(a__number, r->number + j, a__size, b__number[j], &carry);
            if (carry != 0)
            {
                k = a__size + j;
                do
                {
                    if (k >= r->buffer_size)
                        return ippStsOutOfRangeErr;
                    carry = ((r->number[k] += carry) < carry);
                    k++;
                } while (carry != 0);
            }
        }
        /* r->sgn remains unchanged */
    }

    for (i = r->buffer_size - 1; i > 0; i--)
        if (r->number[i] != 0)
            break;
    r->size = i + 1;

    if (r->size == 1 && r->number[0] == 0)
        r->sgn = IppsBigNumPOS;

    return ippStsNoErr;
}


/*
Description:
    For a and b compute the quotient q and remainder r such that
    x = q*y + r, 0 <= r < y
*/
IPPFUN(IppStatus, ippsDiv_BN, (IppsBigNumState *a, IppsBigNumState *b, IppsBigNumState *q, IppsBigNumState *r))
{
    IPP_BAD_PTR4_RET(a, b, q, r);
    IPP_MAKE_ALIGNED_BN(a);
    IPP_MAKE_ALIGNED_BN(b);
    IPP_MAKE_ALIGNED_BN(q);
    IPP_MAKE_ALIGNED_BN(r);
    IPP_BADARG_RET(!BN_VALID_ID(a), ippStsContextMatchErr);
    IPP_BADARG_RET(!BN_VALID_ID(b), ippStsContextMatchErr);
    IPP_BADARG_RET(!BN_VALID_ID(q), ippStsContextMatchErr);
    IPP_BADARG_RET(!BN_VALID_ID(r), ippStsContextMatchErr);
    IPP_BADARG_RET(r->buffer_size < b->size, ippStsOutOfRangeErr);
    IPP_BADARG_RET(b->size == 1 && b->number[0] == 0, ippStsDivByZeroErr);

    cpMemcpy32u(a->workBuffer, a->number, a->size);
    cpDiv_BNU(a->workBuffer, a->size, b->number, b->size, q->number, &q->size, &r->size);
    cpMemcpy32u(r->number, a->workBuffer, r->size);

    /* Set signs */
    if (a->sgn == IppsBigNumPOS)
        q->sgn = b->sgn;
    else
        q->sgn = (b->sgn == IppsBigNumPOS) ? IppsBigNumNEG : IppsBigNumPOS;
    r->sgn = a->sgn;

    if (q->size == 1 && q->number[0] == 0)
        q->sgn = IppsBigNumPOS;
    if (r->size == 1 && r->number[0] == 0)
        r->sgn = IppsBigNumPOS;

    return ippStsNoErr;
}


/*
Description:
    For a and m compute the modulo r such that r = x mod m, 0 <= r < m
*/
IPPFUN(IppStatus, ippsMod_BN, (IppsBigNumState *a, IppsBigNumState *m, IppsBigNumState *r))
{
    IppsBigNumSGN sgn;

    IPP_BAD_PTR3_RET(a, m, r);
    IPP_MAKE_ALIGNED_BN(a);
    IPP_MAKE_ALIGNED_BN(m);
    IPP_MAKE_ALIGNED_BN(r);
    IPP_BADARG_RET(!BN_VALID_ID(a), ippStsContextMatchErr);
    IPP_BADARG_RET(!BN_VALID_ID(m), ippStsContextMatchErr);
    IPP_BADARG_RET(!BN_VALID_ID(r), ippStsContextMatchErr);
    IPP_BADARG_RET(r->buffer_size < m->size, ippStsOutOfRangeErr);
    IPP_BADARG_RET(m->size == 1 && m->number[0] == 0, ippStsBadModulusErr);
    IPP_BADARG_RET(m->sgn == IppsBigNumNEG, ippStsBadModulusErr);

    cpMemcpy32u(a->workBuffer, a->number, a->size);
    cpMod_BNU(a->workBuffer, a->size, m->number, m->size, &r->size);
    cpMemcpy32u(r->number, a->workBuffer, r->size);

    r->sgn = IppsBigNumPOS;

    /* if (a < 0) then r = m - r */
    if (a->sgn == IppsBigNumNEG && !(r->size == 1 && r->number[0] == 0))
        cpSub_BNUs(m->number, m->size, r->number, r->size, r->number, &r->size, &sgn);

    return ippStsNoErr;
}


/****************************************************************************/
// Lehmer's GCD algorithm
/****************************************************************************/
IPPFUN(IppStatus, ippsGcd_BN, (IppsBigNumState *a, IppsBigNumState *b, IppsBigNumState *g))
{
    int        cmp_res;

    Ipp64u    xx, yy;
    Ipp64s    AA, BB, CC, DD;
    Ipp64s    t;

    Ipp64u    q, q1;

    Ipp32u* T;
    Ipp32u* u;
    int       size_x, size_y, size_T;
    Ipp32u    carry;
    Ipp32u    a1, b1, c1, d1;
    Ipp32u    ret_val;
    int       max_size;
    Ipp32u    bb[2] = {0, 1};        /* bb = 2^32; */

    IppsBigNumState*  x;
    IppsBigNumState*  y;
    IppsBigNumState*  temp;

    IPP_BAD_PTR3_RET(a, b, g);
    IPP_MAKE_ALIGNED_BN(a);
    IPP_MAKE_ALIGNED_BN(b);
    IPP_MAKE_ALIGNED_BN(g);
    IPP_BADARG_RET(!BN_VALID_ID(a), ippStsContextMatchErr);
    IPP_BADARG_RET(!BN_VALID_ID(b), ippStsContextMatchErr);
    IPP_BADARG_RET(!BN_VALID_ID(g), ippStsContextMatchErr);
    max_size = (a->size > b->size)? a->size : b->size;
    IPP_BADARG_RET(g->buffer_size < max_size, ippStsOutOfRangeErr);

    g->sgn  = IppsBigNumPOS;
    g->size = (a->size <= b->size) ? a->size : b->size;

    /*
    // Lehmer's algorithm requres that first number must be greater than second
    // x is the first, y is the second
    */
    if(a->size < b->size){
        x = b;
        y = a;
    } else {
        if (a->size == b->size) {
            cmp_res = cpCompare_BNUs(a->number, a->size, b->number, a->size);
            if(cmp_res == -1) {
                x = b;
                y = a;
            } else {
                if(cmp_res == 0) {
                    cpMemcpy32u(g->number, a->number, a->size);
                    return ippStsNoErr;
                } else {
                    x = a;
                    y = b;
                }
            }
        } else {
            x = a;
            y = b;
        }
    }
    /*--------------------------------------------------*/

    /* Clear buffers */
    cpMemset32u(x->workBuffer, 0, x->buffer_size);
    cpMemset32u(y->workBuffer, 0, y->buffer_size);

    /* Init buffers */
    cpMemcpy32u(x->workBuffer, x->number, x->size);
    cpMemcpy32u(y->workBuffer, y->number, y->size);

    T = g->workBuffer;
    u = g->number;
    cpMemset32u(T, 0, g->buffer_size);
    cpMemset32u(u, 0, g->buffer_size);

    for(size_x=x->size; size_x > 0 && !x->workBuffer[size_x-1]; size_x-- );
    for(size_y=y->size; size_y > 0 && !y->workBuffer[size_y-1]; size_y-- );

    if (size_x == 0 && size_y == 0)
        return ippStsBadArgErr;

    if (size_y > size_x) {
        if (size_x == 0) {
            /* End evaluation */
            cpMemset32u(g->number, 0, g->buffer_size);
            cpMemcpy32u(g->number, y->workBuffer, size_y);
            g->size = size_y;
            return ippStsNoErr;
        }
        temp = x;
        x = y;
        y = temp;
        carry = size_x;
        size_x = size_y;
        size_y = carry;
    } else {
        if (size_y == 0) {
            /* End evaluation */
            cpMemset32u(g->number, 0, g->buffer_size);
            cpMemcpy32u(g->number, x->workBuffer, size_x);
            g->size = size_x;
            return ippStsNoErr;
        }
    }

    /* compare b and 2^32 */
    if(size_x > 2){
        cmp_res = 1;
    } else {
        if(size_x < 2){
            cmp_res = -1;
        } else {
            cmp_res = cpCompare_BNUs(x->workBuffer, 2, bb, 2);
        }
    }

    while( cmp_res >= 0){
        /* xx and yy is the high-order digits of x and y (yy could be 0) */

        xx = (Ipp64u)(x->workBuffer[size_x-1]);
        yy = (Ipp64u)(y->workBuffer[size_y-1]);
        if (size_y < size_x)
            yy = 0;

        AA = DD = 1; BB = CC = 0;
        while((yy+CC)!=0 && (yy+DD)!=0){
            q  = ( xx + AA ) / ( yy + CC );
            q1 = ( xx + BB ) / ( yy + DD );
            if(q!=q1)
                break;
            t = AA - q*CC;
            AA = CC;
            CC = t;
            t = BB - q*DD;
            BB = DD;
            DD = t;
            t = xx - q*yy;
            xx = yy;
            yy = t;
        }

        if(BB == 0){
            /* T = x mod y */
            cpMod_BNU( x->workBuffer, size_x,
                       y->workBuffer, size_y,
                       &size_T);
            cpMemset32u(T, 0, g->buffer_size);
            cpMemcpy32u(T, x->workBuffer, size_T);
            /* a = b; b = T; */
            cpMemset32u(x->workBuffer, 0, x->buffer_size);
            cpMemcpy32u(x->workBuffer, y->workBuffer, size_y);
            cpMemset32u(y->workBuffer, 0, y->buffer_size);
            cpMemcpy32u(y->workBuffer, T, size_y);
        } else {
            /*
            // T = AA*x + BB*y;
            // u = CC*x + DD*y;
            // b = u; a = T;
            */
            if((AA <= 0)&&(BB>=0)){
                a1 = (Ipp32u)(-AA);
                ippsMulOne_BNU( y->workBuffer, T, size_y, (Ipp32u)BB, &carry);
                ippsMulOne_BNU( x->workBuffer, u, size_y, a1, &carry);
                /* T = BB*y - AA*x; */
                ippsSub_BNU(T, u, T, size_y, &carry);
            } else {
                if((AA >= 0)&&(BB<=0)){
                    b1 = (Ipp32u)(-BB);
                    ippsMulOne_BNU( x->workBuffer, T, size_y, (Ipp32u)AA, &carry);
                    ippsMulOne_BNU( y->workBuffer, u, size_y, b1, &carry);
                    /* T = AA*x - BB*y; */
                    ippsSub_BNU(T, u, T, size_y, &carry);
                } else {
                    /*AA*BB>=0 */
                    ippsMulOne_BNU( x->workBuffer, T, size_y, (Ipp32u)AA, &carry);
                    ippsMulOne_BNU( y->workBuffer, u, size_y, (Ipp32u)BB, &carry);
                    /* T = AA*x + BB*y; */
                    ippsAdd_BNU(T, u, T, size_y, &carry);

                }
            }
            /* Now T is reserved. We use only u for intermediate results. */
            if((CC <= 0)&&(DD>=0)){
                c1 = (Ipp32u)(-CC);
                /* u = x*CC; x = u; */
                ippsMulOne_BNU( x->workBuffer, u, size_y, c1, &carry);
                cpMemcpy32u(x->workBuffer, u, size_y);
                /* u = y*DD; */
                ippsMulOne_BNU( y->workBuffer, u, size_y, (Ipp32u)DD, &carry);
                /* u = DD*y - CC*x; */
                ippsSub_BNU(u, x->workBuffer, u, size_y, &carry);
            } else {
                if((CC >= 0)&&(DD<=0)){
                    d1 = (Ipp32u)(-DD);
                    /* u = y*DD; y = u */
                    ippsMulOne_BNU( y->workBuffer, u, size_y, d1, &carry);
                    cpMemcpy32u(y->workBuffer, u, size_y);
                    /* u = CC*x; */
                    ippsMulOne_BNU( x->workBuffer, u, size_y, (Ipp32u)CC, &carry);
                    /* u = CC*x - DD*y; */
                    ippsSub_BNU(u, y->workBuffer, u, size_y, &carry);

                } else {
                    /*CC*DD>=0 */
                    /* y = y*DD */
                    ippsMulOne_BNU( y->workBuffer, u, size_y, (Ipp32u)DD, &carry);
                    cpMemcpy32u(y->workBuffer, u, size_y);
                    /* u = x*CC */
                    ippsMulOne_BNU( x->workBuffer, u, size_y, (Ipp32u)CC, &carry);
                    /* u = x*CC + y*DD */
                    ippsAdd_BNU(u, y->workBuffer, u, size_y, &carry);
                }
            }
            /* y = u; x = T; */
            cpMemcpy32u(y->workBuffer, u, size_y);
            cpMemcpy32u(x->workBuffer, T, size_y);
        }

        while( size_x > 0 && !x->workBuffer[size_x-1] )
            size_x--;
        while( size_y > 0 && !y->workBuffer[size_y-1] )
            size_y--;

        if (size_y > size_x) {
            temp = x;
            x = y;
            y = temp;
            carry = size_x;
            size_x = size_y;
            size_y = carry;
        }

        if (size_y == 0) {
            /* End evaluation */
            cpMemset32u(g->number, 0, g->buffer_size);
            cpMemcpy32u(g->number, x->workBuffer, size_x);
            g->size = size_x;
            return ippStsNoErr;
        }


        /* compare x and 2^32 */
        if(size_x > 2){
            cmp_res = 1;
        } else {
            if(size_x < 2){
                cmp_res = -1;
            } else {
                cmp_res = cpCompare_BNUs(x->workBuffer, 2, bb, 2);
            }
        }

    }

    ret_val = cpGCD32u(*x->workBuffer, *y->workBuffer);

    cpMemset32u(g->number, 0, g->buffer_size);
    g->number[0] = ret_val;
    g->size = 1;

    return ippStsNoErr;
}


/********************************************************************************
 Name:             ippsModInv_BN
 Description: ippsModInv_BN() uses the extended Euclidean algorithm to compute
              the multiplicative inverse of a given big number integer pointed
              by IppsBigNumState *e with respect to the modulus IppsBigNumState *m, where
              gcd(e, m) = 1.
 Input Arguments: e - a big number integer of IppsBigNumState.
                  m - modulus integer of IppsBigNumState.
 Output Arguments: d - the multiplicative inverse.
 Returns: ippStsNoErr - No Error
          ippStsBadArgErr - If e < or = 0.
          ippStsBadModulusErr - If e > m, or gcd(e, m)>1.
          ippStsNullPtrErr - Null pointers
          ippStsOutOfRangeErr - If IppsBigNumState *d can not hold the result
 Notes: It requires that the size of IppsBigNumState *d should be no less than the
        length of IppsBigNumState *m.
********************************************************************************/

IPPFUN(IppStatus, ippsModInv_BN, (IppsBigNumState *e, IppsBigNumState *m, IppsBigNumState *d) )
{
    IppStatus sts;

    IPP_BAD_PTR3_RET(e, m, d);
    IPP_MAKE_ALIGNED_BN(e);
    IPP_MAKE_ALIGNED_BN(m);
    IPP_MAKE_ALIGNED_BN(d);
    IPP_BADARG_RET(!BN_VALID_ID(e), ippStsContextMatchErr);
    IPP_BADARG_RET(!BN_VALID_ID(m), ippStsContextMatchErr);
    IPP_BADARG_RET(!BN_VALID_ID(d), ippStsContextMatchErr);
    IPP_BADARG_RET(d->buffer_size < m->size, ippStsOutOfRangeErr);
    IPP_BADARG_RET((e->sgn == IppsBigNumNEG) || (e->size == 1 && e->number[0] == 0), ippStsBadArgErr);
    IPP_BADARG_RET((m->sgn == IppsBigNumNEG) || (m->size == 1 && m->number[0] == 0), ippStsBadModulusErr);
    IPP_BADARG_RET(cpCompare_BNUs(e->number, e->size, m->number, m->size) >= 0, ippStsScaleRangeErr);

    if ((sts = cpModInv_BNU(e->number, e->size, m->number, m->size, d->number, &d->size,
        e->workBuffer, m->workBuffer, d->workBuffer)) != ippStsNoErr)
        return sts;
    d->sgn = IppsBigNumPOS;

    return ippStsNoErr;
}

