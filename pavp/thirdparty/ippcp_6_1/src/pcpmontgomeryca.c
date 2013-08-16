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
//  File:       pcpmontgomeryca.c
//  Created:    2003/04/28    11:13
//
//  2003/11/06 modified by Andrey Somsikov
//      Functions redesigned as preparation for opening the source
//  2003/04/28 modified by Alexey Omeltchenko
//      ippsMontGetSize
//      MONT__MULADD_IPP32U
*/

#include "precomp.h"
#include "owncp.h"
#include "pcpbn.h"
#include "pcpbn32.h"
#include "pcpmontgomery.h"
#include "pcpmulbnuk.h"

/*F*
// Name: ippsMontGetSize
//
// Purpose: Specifies size of buffer in bytes.
//
// Returns:                Reason:
//      ippStsNoErr         Returns no error.
//      ippStsNullPtrErr    Returns an error when pointers are null.
//      ippStsLengthErr     Returns an error when length is less than or equal to 0.
//
// Parameters:
//      method  Selected exponential method.
//      length  Data field length for the modulus.
//      size    Size of the buffer required for initialization.
//
// Notes: Function always use method=IppsBinaryMethod, so this parameter
//      is ignored
*F*/
#if 0 //gres
IPPFUN(IppStatus, ippsMontGetSize, (IppsExpMethod method, int length, int *size))
{
    int bnSize;
    int len;

    IPP_BAD_PTR1_RET(size);
    IPP_BADARG_RET(length <= 0, ippStsLengthErr);

    /* Actually, IA32 and IA64 always use IppsBinaryMethod
     * Parameter 'method' was introduced for XScale compartibility
     * Next statement removes a warning.
     */
    if( method != IppsBinaryMethod )
        method = IppsBinaryMethod;

    *size = sizeof(IppsMontState);
    IPP_MAKE_MULTIPLE_OF_16(*size);

    ippsBigNumGetSize(length, &bnSize);
    *size += bnSize;

    len = (((length + BNUBASE_TYPE_SIZE - 1)/BNUBASE_TYPE_SIZE)*BNUBASE_TYPE_SIZE) * 2;
    ippsBigNumGetSize(len, &bnSize);
    *size += bnSize;

    bnSize = BNUBASE_TYPE_SIZE * sizeof(Ipp32u);
    IPP_MAKE_MULTIPLE_OF_16(bnSize);
    *size += bnSize;

    *size += MONT_ALIGN_SIZE;

    IPP_MAKE_MULTIPLE_OF_16(*size);

    return ippStsNoErr;
}
#endif

IPPFUN(IppStatus, ippsMontGetSize, (IppsExpMethod method, int length, int* pSize))
{
   IPP_BAD_PTR1_RET(pSize);
   IPP_BADARG_RET(length <= 0, ippStsLengthErr);

   /* Actually, IA32 and IA64 always use IppsBinaryMethod
   // Parameter 'method' was introduced for XScale compartibility
   // Next statement removes a warning.
   */
   UNREFERENCED_PARAMETER(method);

   {
      int modSize;
      int prodSize;
      int buffSize;
      int opLen;

      /* size of modulus and identity */
      ippsBigNumGetSize(length, &modSize);

      /* size of internal product */
      opLen = (((length + BNUBASE_TYPE_SIZE - 1)/BNUBASE_TYPE_SIZE)*BNUBASE_TYPE_SIZE);
      ippsBigNumGetSize(opLen*2, &prodSize);

      /* size of buffer */
      #if defined(_USE_KARATSUBA_)
      buffSize = cpKaratsubaBufferSize(opLen, IPP_KARATSUBA_BOUND);
      #if defined( _OPENMP )
      buffSize <<=1; /* double buffer size if OpenMP used */
      #endif
      #else
      buffSize = 0;
      #endif

      *pSize = sizeof(IppsMontState)
              +modSize     /* modulus  */
              +modSize     /* identity */
              +modSize     /* square R */
              +modSize     /* cube R */
              +prodSize    /* internal product */
              +buffSize    /* K-mul buffer */
              +MONT_ALIGN_SIZE;

      return ippStsNoErr;
   }
}


/*F*
// Name: ippsMontInit
//
// Purpose: Initializes the symbolic data structure and partitions the
//      specified buffer space.
//
// Returns:                Reason:
//      ippStsNoErr         Returns no error.
//      ippStsNullPtrErr    Returns an error when pointers are null.
//      ippStsLengthErr     Returns an error when length is less than or
//                          equal to 0.
//
// Parameters:
//      method  Selected exponential method.
//      buffer  Buffer for initializing m.
//      length  Data field length for the modulus.
//      m       Pointer to the symbolic data structure IppsMontState.
*F*/
#if 0 //gres
IPPFUN(IppStatus, ippsMontInit,(IppsExpMethod method, int length, IppsMontState *m))
{
    int size;
    int len;

    IPP_BAD_PTR1_RET(m);
    IPP_MAKE_ALIGNED_MONT(m);
    IPP_BADARG_RET(length <= 0, ippStsLengthErr);

    /* Actually, IA32 and IA64 always use IppsBinaryMethod
     * Parameter 'method' was introduced for XScale compartibility
     * Next statement removes a warning.
     */
    if( method != IppsBinaryMethod )
        method = IppsBinaryMethod;

    m->idCtx    = idCtxMontgomery;
    m->k        = length;
    m->n0       = 0;
    m->method   = method;

    size = sizeof(IppsMontState);
    IPP_MAKE_MULTIPLE_OF_16(size);

    m->n = (IppsBigNumState*)((Ipp8u*)m + size);
    ippsBigNumInit(length, m->n);
    ippsBigNumGetSize(length, &size);

    m->wb = (IppsBigNumState*)((Ipp8u*)m->n + size);
    len = IPP_MULTIPLE_OF(length, BNUBASE_TYPE_SIZE) * 2;
    ippsBigNumInit(len, m->wb);
    ippsBigNumGetSize(len, &size);

    m->nHi = (Ipp32u*)((Ipp8u*)m->wb + size);

    return ippStsNoErr;
}
#endif

IPPFUN(IppStatus, ippsMontInit,(IppsExpMethod method, int length, IppsMontState *m))
{
   IPP_BAD_PTR1_RET(m);
   IPP_BADARG_RET(length <= 0, ippStsLengthErr);
   IPP_MAKE_ALIGNED_MONT(m);

   /* Actually, IA32 and IA64 always use IppsBinaryMethod
   // Parameter 'method' was introduced for XScale compartibility
   // Next statement removes a warning.
   */
   UNREFERENCED_PARAMETER(method);

   MNT_ID(m)         = idCtxMontgomery;
   MNT_METHOD(m)     = IppsBinaryMethod;
   MNT_K(m)          = length;
   MNT_HELPER(m)[0]  = 0;

   {
      int modSize;
      int prodSize;
      int buffSize;
      int opLen;

      Ipp8u* ptr = (Ipp8u*)m;

      /* size of modulus and identity */
      ippsBigNumGetSize(length, &modSize);

      /* size of internal product */
      opLen = (((length + BNUBASE_TYPE_SIZE - 1)/BNUBASE_TYPE_SIZE)*BNUBASE_TYPE_SIZE);
      ippsBigNumGetSize(opLen*2, &prodSize);

      /* size of buffer */
      #if defined(_USE_KARATSUBA_)
      buffSize = cpKaratsubaBufferSize(opLen, IPP_KARATSUBA_BOUND);
      #if defined( _OPENMP )
      buffSize <<=1; /* double buffer size if OpenMP used */
      #endif
      #else
      buffSize = 0;
      #endif

      /* allocate internal objects */
      ptr += sizeof(IppsMontState);
      MNT_MODULO(m) = (IppsBigNumState*)( IPP_ALIGNED_PTR(ptr,BN_ALIGN_SIZE) );
      ptr += modSize;
      MNT_1(m)      = (IppsBigNumState*)( IPP_ALIGNED_PTR(ptr,BN_ALIGN_SIZE) );
      ptr += modSize;

      MNT_SQUARE_R(m)=(IppsBigNumState*)( IPP_ALIGNED_PTR(ptr,BN_ALIGN_SIZE) );
      ptr += modSize;
      MNT_CUBE_R(m)  = (IppsBigNumState*)( IPP_ALIGNED_PTR(ptr,BN_ALIGN_SIZE) );
      ptr += modSize;

      MNT_PRODUCT(m) =(IppsBigNumState*)( IPP_ALIGNED_PTR(ptr,BN_ALIGN_SIZE) );
      ptr += prodSize;
      MNT_BUFFER(m) = (buffSize)? (Ipp32u*)( IPP_ALIGNED_PTR(ptr,4) ) : NULL;

      /* init internal objects */
      ippsBigNumInit(length,  MNT_MODULO(m));
      ippsBigNumInit(length,  MNT_1(m));
      ippsBigNumInit(length,  MNT_SQUARE_R(m));
      ippsBigNumInit(length,  MNT_CUBE_R(m));
      ippsBigNumInit(opLen*2, MNT_PRODUCT(m));

      return ippStsNoErr;
   }
}

/*F*
// Name: ippsMontSet
//
// Purpose: Sets the input big number integer to a value and computes the
//      Montgomery reduction index.
//
// Returns:                Reason:
//      ippStsNoErr         Returns no error.
//      ippStsNullPtrErr    Returns an error when pointers are null.
//      ippStsLengthErr     Returns an error when length is less than or
//                          equal to 0.
//      ippStsContextMatchErr Returns an error when the context parameter does
//                          not match the operation.
//
// Parameters:
//      n   Input big number modulus.
//      m   Pointer to the symbolic data structure IppsMontState capturing the
//          modulus and the least significant word of the multiplicative inverse Ni.
//
// Discussion:
//      The function sets the input positive big number integer n to be the modulus for the
//      symbolic structure IppsMontState *m , computes the Montgomery reduction index k with
//      respect to the input big number modulus n and the least significant 32-bit word of the
//      multiplicative inverse Ni with respect to the modulus R, that satisfies R*R^(-1) - n*Ni = 1.
*F*/
#if 0
IPPFUN(IppStatus, ippsMontSet,(const Ipp32u *n, int length, IppsMontState *m))
{
    IppStatus sts;
    int i;
    Ipp32u one;
    Ipp32u buf_n[BNUBASE_TYPE_SIZE];
    Ipp32u b[BNUBASE_TYPE_SIZE + 1];
    Ipp32u buf_b[IPP_COUNT_OF(b)];
    Ipp32u res[IPP_COUNT_OF(b)];
    Ipp32u buf_res[IPP_COUNT_OF(b)];

    Ipp32u size_res, size_n;

    IPP_BAD_PTR2_RET(n, m);
    IPP_MAKE_ALIGNED_MONT(m);
    IPP_BADARG_RET(idCtxMontgomery != m->idCtx, ippStsContextMatchErr);
    IPP_BADARG_RET(length <= 0, ippStsLengthErr);
    /* n != 0 AND n is not an odd number */
    IPP_BADARG_RET((n[0] & 1) == 0, ippStsBadModulusErr);
    IPP_BADARG_RET((int)m->n->buffer_size < length, ippStsOutOfRangeErr);

    ippsSet_BN(IppsBigNumPOS, length, n, m->n);
    m->k = m->n->size;

    /* b = {0, 0, ... , 0, 1} */
    for (i = 0; i < IPP_COUNT_OF(b) - 1; i++)
        b[i] = 0;
    b[IPP_COUNT_OF(b) - 1] = 1;

    /* compute real size of n1 */
    size_n = IPP_MIN(length, IPP_COUNT_OF(buf_n));
    while (size_n > 1 && n[size_n - 1] == 0)
        size_n--;
    /* Compute the last significant word of n1, where nHi*n1 = -1 mod R */
    if ((sts = cpModInv_BNU(n, size_n, b, IPP_COUNT_OF(b), res, &size_res,
        buf_n, buf_b, buf_res)) != ippStsNoErr)
        return sts;

    /* m->nHi = - m->nHi */
    while (size_res < BNUBASE_TYPE_SIZE)
        res[size_res++] = 0;
    one = 1;
    for (i = 0; i < BNUBASE_TYPE_SIZE; i++)
    {
        m->nHi[i] = ~res[i];
        if((m->nHi[i] += one) != 0)
            one = 0;
    }

    m->n0 = m->nHi[0];

    return ippStsNoErr;
}
#endif

IPPFUN(IppStatus, ippsMontSet,(const Ipp32u *n, int length, IppsMontState *m))
{
   IppStatus sts;
   int i;
   Ipp32u one;
   Ipp32u buf_n[BNUBASE_TYPE_SIZE+1];
   Ipp32u     b[BNUBASE_TYPE_SIZE+1+1];
   Ipp32u buf_b[BNUBASE_TYPE_SIZE+1+1];
   Ipp32u   res[BNUBASE_TYPE_SIZE+1];
   Ipp32u buf_res[BNUBASE_TYPE_SIZE+1];

   int size_res, size_n;

   IPP_BAD_PTR2_RET(n, m);
   IPP_MAKE_ALIGNED_MONT(m);
   IPP_BADARG_RET(idCtxMontgomery != m->idCtx, ippStsContextMatchErr);
   IPP_BADARG_RET(length <= 0, ippStsLengthErr);
   /* n != 0 AND n is not an odd number */
   IPP_BADARG_RET((n[0] & 1) == 0, ippStsBadModulusErr);
   IPP_BADARG_RET((int)m->n->buffer_size < length, ippStsOutOfRangeErr);

   ippsSet_BN(IppsBigNumPOS, length, n, m->n);
   m->k = m->n->size;

   #if (_IPP64==_IPP64_I7)
   if(m->n->size &1)
      m->n->number[m->n->size] = 0;
   #endif

   /* b = {0, 0, ... , 0, 1} */
   for (i = 0; i < BNUBASE_TYPE_SIZE; i++)
      b[i] = 0;
   b[BNUBASE_TYPE_SIZE] = 1;

   /* compute real size of n1 */
   size_n = IPP_MIN(length, BNUBASE_TYPE_SIZE);
   while (size_n > 1 && n[size_n - 1] == 0)
      size_n--;
   /* Compute the last significant word of n1, where nHi*n1 = -1 mod R */
   sts = cpModInv_BNU(n,   size_n,
                      b,   BNUBASE_TYPE_SIZE+1,
                      res,&size_res,
                      buf_n, buf_b, buf_res);
   if(sts != ippStsNoErr)
      return sts;

   /* m->nHi = - m->nHi */
   while (size_res < BNUBASE_TYPE_SIZE)
      res[size_res++] = 0;
   one = 1;
   for(i=0; i< BNUBASE_TYPE_SIZE; i++) {
      /*
      m->nHi[i] = ~res[i];
      if((m->nHi[i] += one) != 0)
      one = 0;
      */
      m->n0[i] = ~res[i];
      if((m->n0[i] += one) != 0)
         one = 0;
   }

   //m->n0 = m->nHi[0];

   /* set up identity */
   {
      #if defined(_IPP_TRS)
      DECLARE_BN_Word(_trs_one32_, 1);
      INIT_BN_Word(_trs_one32_);
      #endif
      ZEXPAND_BNU(BN_NUMBER(MNT_1(m)), 0, length);
      ZEXPAND_BNU(BN_NUMBER(MNT_SQUARE_R(m)), 0, length);
      ZEXPAND_BNU(BN_NUMBER(MNT_CUBE_R(m)), 0, length);

      if(ippStsNoErr != (sts = ippsMontForm(BN_ONE_REF(), m, MNT_1(m))))
         return sts;
      if(ippStsNoErr != (sts = ippsMontForm(MNT_1(m), m, MNT_SQUARE_R(m))))
         return sts;
      if(ippStsNoErr != (sts = ippsMontForm(MNT_SQUARE_R(m), m, MNT_CUBE_R(m))))
         return sts;
   }
   return sts;
}

/*F*
// Name: ippsMontGet
//
// Purpose: Extracts the big number modulus.
//
// Returns:                Reason:
//      ippStsNoErr         Returns no error.
//      ippStsNullPtrErr    Returns an error when pointers are null.
//      ippStsContextMatchErr Returns an error when the context parameter does
//                          not match the operation.
//
// Parameters:
//      m       Symbolic data structure IppsMontState.
//      n       Modulus data field.
//      length  Modulus data length.
*F*/
IPPFUN(IppStatus, ippsMontGet,(Ipp32u *n, int *length, const IppsMontState *m))
{
    IPP_BAD_PTR3_RET(n, length, m);
    IPP_MAKE_ALIGNED_MONT(m);
    IPP_BADARG_RET(idCtxMontgomery != m->idCtx, ippStsContextMatchErr);

    *length=m->n->size;
    cpMemcpy32u(n, m->n->number, *length);

    return ippStsNoErr;
}

/*F*
// Name: ippsMontForm
//
// Purpose: Converts input positive big number integer into Montgomery form.
//
// Returns:                Reason:
//      ippStsNoErr         Returns no error.
//      ippStsNullPtrErr    Returns an error when pointers are null.
//      ippStsBadArgErr     Returns an error when a is a negative integer.
//      ippStsScaleRangeErr Returns an error when a is more than m.
//      ippStsOutOfRangeErr Returns an error when IppsBigNumState *r is larger than
//                          IppsMontState *m.
//      ippStsContextMatchErr Returns an error when the context parameter does
//                          not match the operation.
//
// Parameters:
//      a   Input big number integer within the range [0, m - 1].
//      m   Input big number modulus of IppsBigNumState.
//      r   Resulting Montgomery form r = a*R mod m, where R=b^^k and b=2^^32.
*F*/
IPPFUN(IppStatus, ippsMontForm,(const IppsBigNumState *a, IppsMontState *m, IppsBigNumState *r))
{
    int len;

    IPP_BAD_PTR3_RET(a, m, r);
    IPP_MAKE_ALIGNED_MONT(m);
    IPP_MAKE_ALIGNED_BN(a);
    IPP_MAKE_ALIGNED_BN(r);
    IPP_BADARG_RET(idCtxMontgomery != m->idCtx, ippStsContextMatchErr);
    IPP_BADARG_RET(a->sgn != IppsBigNumPOS, ippStsBadArgErr);
    IPP_BADARG_RET(cpCompare_BNUs(a->number, a->size, m->n->number, m->n->size) > 0, ippStsScaleRangeErr);
    IPP_BADARG_RET(r->buffer_size < m->n->size, ippStsOutOfRangeErr);

    len = IPP_MULTIPLE_OF(m->n->size, BNUBASE_TYPE_SIZE);
    cpMemset32u(m->wb->number, 0, len);
    cpMemcpy32u(m->wb->number + len, a->number, a->size);

    cpMod_BNU(m->wb->number, len + a->size, m->n->number, m->n->size, &r->size);
    cpMemcpy32u(r->number, m->wb->number, r->size);
    r->sgn = IppsBigNumPOS;

    return ippStsNoErr;
}



/*******************************************************************************
// Name:             ippsMontExp
// Description: ippsMontExp() computes the Montgomery exponentiation with exponent
//              IppsBigNumState *e to the given big number integer of Montgomery form
//              IppsBigNumState *a with respect to the modulus IppsMontState *m.
// Input Arguments: a - big number integer of Montgomery form within the
//                      range [0,m-1]
//                  e - big number exponent
//                  m - Montgomery modulus of IppsMontState.
// Output Arguments: r - the Montgomery exponentiation result.
// Returns: IPPC_STATUS_OK - No Error
//          IPPC_STATUS_MONT_BAD_MODULUS - If a>m or b>m or m>R or P_MONT *m has
//                                         not been initialized by the primitive
//                                         function ippsMontInit( ).
//          IPPC_STATUS_BAD_ARG - Bad Arguments
// Notes: IppsBigNumState *r should possess enough memory space as to hold the result
//        of the operation. i.e. both pointers r->d and r->buffer should possess
//        no less than (m->n->length) number of 32-bit words.
*******************************************************************************/
#if 0
#if !defined(_USE_ERNIE_CBA_MITIGATION_) && !defined(_USE_GRES_CBA_MITIGATION_) // unsafe version
IPPFUN(IppStatus, ippsMontExp, (const IppsBigNumState *a, const IppsBigNumState *e, IppsMontState *m, IppsBigNumState *r))
{
    Ipp32u flag;
    int j, k;
    Ipp32u* r_number;
    int     r_size;
    Ipp32u power, powd;

    IPP_BAD_PTR4_RET(a, e, m, r);
    IPP_MAKE_ALIGNED_MONT(m);
    IPP_MAKE_ALIGNED_BN(a);
    IPP_MAKE_ALIGNED_BN(e);
    IPP_MAKE_ALIGNED_BN(r);
    IPP_BADARG_RET(idCtxMontgomery != m->idCtx, ippStsContextMatchErr);
    IPP_BADARG_RET(r->buffer_size < m->n->size, ippStsOutOfRangeErr);
    /* check a */
    IPP_BADARG_RET(a->sgn != IppsBigNumPOS, ippStsBadArgErr);
    IPP_BADARG_RET(cpCompare_BNUs(a->number, a->size, m->n->number, m->n->size) > 0, ippStsScaleRangeErr);
    /* check e */
    IPP_BADARG_RET(e->sgn != IppsBigNumPOS, ippStsBadArgErr);
    IPP_BADARG_RET(cpCompare_BNUs(e->number, e->size, m->n->number, m->n->size) > 0, ippStsScaleRangeErr);

    /* if e==0 then r=R mod m */
    if (e->size == 1 && e->number[0] == 0)
    {
        int len = IPP_MULTIPLE_OF(m->n->size, BNUBASE_TYPE_SIZE);
        cpMemset32u(m->wb->number, 0, len);
        m->wb->number[len] = 1;

        cpMod_BNU(m->wb->number, len + 1, m->n->number, m->n->size, &r->size);

        cpMemcpy32u(r->number, m->wb->number, r->size);
        r->sgn = IppsBigNumPOS;

        return ippStsNoErr;
    }

    r_number = r->workBuffer;
    r_size = r->size;

    flag=1;
    power=e->number[e->size-1];
    for( k = 31; k >= 0; k-- )
    {
        powd = power & 0x80000000;/* from top to bottom*/
        power <<= 1;
        if((flag == 1) && (powd == 0))
            continue;
        else if (flag == 0)
        {
           #if defined(_USE_NN_MONTMUL_)
           cpMontMul(r_number, r_size, r_number,r_size, m->n->number,m->n->size,
                     r_number,&r_size, /*m->nHi*/m->n0, m->wb->number);
           #else
           cpMontMul(r_number, r_size, r_number,r_size, m->n->number,m->n->size,
                     r_number,&r_size, /*m->nHi*/m->n0, m->wb->number, m->pBuffer);
           #endif
           if (powd)
               #if defined(_USE_NN_MONTMUL_)
               cpMontMul(r_number, r_size, a->number,a->size, m->n->number,m->n->size,
                         r_number,&r_size, /*m->nHi*/m->n0, m->wb->number);
               #else
               cpMontMul(r_number, r_size, a->number,a->size, m->n->number,m->n->size,
                         r_number,&r_size, /*m->nHi*/m->n0, m->wb->number, m->pBuffer);
               #endif
        }
        else
        {
            int i;
            flag = 0;
            r_size = m->n->size;
            if( a->size < m->n->size )
                for(i = r_size - 1; i >= a->size; i-- )
                    r_number[i] = 0;

            for( i = a->size - 1; i >= 0; i-- )
                r_number[i] = a->number[i];
        }
    }
    if (e->size > 1)
    {
        struct BNU {
            Ipp32u *number;
            int    *size;
        } BNUs[2];
        BNUs[0].number = r_number;
        BNUs[0].size   = &r_size;
        BNUs[1].number = a->number;
        BNUs[1].size   = &(((IppsBigNumState*)a)->size);

        for( k = e->size - 2; k >= 0; k-- )
        {
            power = e->number[k];
            powd = 0;
            for( j = 31; j >= 0; j-- )
            {
                #if defined(_USE_NN_MONTMUL_)
                cpMontMul(r_number, r_size, BNUs[powd].number, *(BNUs[powd].size), m->n->number,m->n->size,
                          r_number,&r_size, /*m->nHi*/m->n0, m->wb->number);
                #else
                cpMontMul(r_number, r_size, BNUs[powd].number, *(BNUs[powd].size), m->n->number,m->n->size,
                          r_number,&r_size, /*m->nHi*/m->n0, m->wb->number, m->pBuffer);
                #endif
                //gres: powd = ((power >> j) & 0x1) * (powd ^ 1);
                powd = ((power >> j) & 0x1) & (powd ^ 1);
                j += powd;
            }
        }
    }

    for( k = r_size - 1; k >= 0; k-- )
        r->number[k] = r_number[k];

    r->sgn = IppsBigNumPOS;
    r->size = r_size;

    while ((r->size > 1) && (r->number[r->size-1] == 0))
        r->size--;

    return ippStsNoErr;
}
#endif /* _xUSE_ERNIE_CBA_MITIGATION_,  _xUSE_GRES_CBA_MITIGATION_ */


#if defined(_USE_ERNIE_CBA_MITIGATION_)
/*
// The version below was designed according to recommendation
// from Ernie Brickell and Mattew Wood.
// The reason was to mitigate "cache monitoring" attack on RSA
// Note: this version hurt pre-mitigated version ~ 30-40%
*/
#define SET_BNU(dst,val,len) \
{ \
   int n; \
   for(n=0; n<(len); n++) (dst)[n] = (val); \
}

#define AND_BNU(dst,src1,src2,len) \
{ \
   int n; \
   for(n=0; n<(len); n++) (dst)[n] = (src1)[n] & (src2)[n]; \
}

IPPFUN(IppStatus, ippsMontExp, (const IppsBigNumState *a, const IppsBigNumState *e, IppsMontState *m, IppsBigNumState *r))
{
   IPP_BAD_PTR4_RET(a, e, m, r);
   IPP_MAKE_ALIGNED_MONT(m);
   IPP_MAKE_ALIGNED_BN(a);
   IPP_MAKE_ALIGNED_BN(e);
   IPP_MAKE_ALIGNED_BN(r);
   IPP_BADARG_RET(idCtxMontgomery != m->idCtx, ippStsContextMatchErr);
   IPP_BADARG_RET(r->buffer_size < m->n->size, ippStsOutOfRangeErr);
   /* check a */
   IPP_BADARG_RET(a->sgn != IppsBigNumPOS, ippStsBadArgErr);
   IPP_BADARG_RET(cpCompare_BNUs(a->number, a->size, m->n->number, m->n->size) > 0, ippStsScaleRangeErr);
   /* check e */
   IPP_BADARG_RET(e->sgn != IppsBigNumPOS, ippStsBadArgErr);
   IPP_BADARG_RET(cpCompare_BNUs(e->number, e->size, m->n->number, m->n->size) > 0, ippStsScaleRangeErr);

   {
      /* MontEnc(1) */
      IppsBigNumState* oneMon = MNT_PRODUCT(m);

      #if defined(_IPP_TRS)
      DECLARE_BN_Word(_trs_one32_, 1);
      INIT_BN_Word(_trs_one32_);
      #endif

      ippsMontForm(BN_ONE_REF(), m, oneMon);

      /* if e==0 then r=R mod m (i.e MontEnc(1)) */
      if (e->size == 1 && e->number[0] == 0) {
         cpBN_copy(oneMon, r);
         return ippStsNoErr;
      }

      /*
      // modulo exponentiation
      */

      /* init result */
      if(r!=a)
         cpBN_copy(a, r);

      {
         Ipp32u* pE = BN_NUMBER(e);
         int eSize = BN_SIZE(e);
         Ipp32u eValue;
         int nBits;

         Ipp32u* pModulus = BN_NUMBER(MNT_MODULO(m));
         int mSize = BN_SIZE(MNT_MODULO(m));
         Ipp32u* pHelper = MNT_HELPER(m);

         Ipp32u* pR = BN_NUMBER(r);
         Ipp32u* pA = BN_BUFFER(r);
         int rSize = BN_SIZE(r);

         Ipp32u* pT      = BN_NUMBER(MNT_PRODUCT(m));
         Ipp32u* pBuffer = BN_BUFFER(MNT_PRODUCT(m));

         Ipp32u* pMontOne = BN_BUFFER(MNT_MODULO(m));

         /* store MontEnc(1) */
         ZEXPAND_COPY_BNU(BN_NUMBER(oneMon),BN_SIZE(oneMon), pMontOne,mSize);

         /* copy base */
         ZEXPAND_COPY_BNU(pR,rSize, pA,mSize);


         /* execute most significant part pE */
         eValue = pE[eSize-1];
         nBits = 32-NLZ32u(eValue);
         eValue <<= (32-nBits);

         nBits--;
         eValue <<=1;
         for(; nBits>0; nBits--, eValue<<=1) {
            Ipp32u carry;

            /* squaring: R^2 mod Modulus */
            #if defined(_USE_NN_MONTMUL_)
            cpMontMul(pR,        rSize,
                      pR,        rSize,
                      pModulus,  mSize,
                      pR,       &rSize,
                      pHelper,   pBuffer);
            #else
            cpMontMul(pR,        rSize,
                      pR,        rSize,
                      pModulus,  mSize,
                      pR,       &rSize,
                      pHelper,   pBuffer, MNT_BUFFER(m));
            #endif

            /* T = (A-1)*bitof(e,j) + 1 */
            SET_BNU(pBuffer, ((Ipp32s)eValue)>>31, mSize);
            cpSub_BNU(pA, pMontOne, pT, mSize, &carry);
            AND_BNU(pT, pT, pBuffer, mSize);
            cpAdd_BNU(pT, pMontOne, pT, mSize, &carry);

            /* multiply: R*T mod Modulus */
            #if defined(_USE_NN_MONTMUL_)
            cpMontMul(pR,       rSize,
                      pT,       mSize,
                      pModulus, mSize,
                      pR,      &rSize,
                      pHelper, pBuffer);
            #else
            cpMontMul(pR,       rSize,
                      pT,       mSize,
                      pModulus, mSize,
                      pR,      &rSize,
                      pHelper, pBuffer, MNT_BUFFER(m));
            #endif
         }

         /* execute rest bits of pE */
         eSize--;
         for(; eSize>0; eSize--) {
            eValue = pE[eSize-1];

            for(nBits=32; nBits>0; nBits--, eValue<<=1) {
               Ipp32u carry;

               /* squaring: R^2 mod Modulus */
               #if defined(_USE_NN_MONTMUL_)
               cpMontMul(pR,        rSize,
                         pR,        rSize,
                         pModulus,  mSize,
                         pR,       &rSize,
                         pHelper,   pBuffer);
               #else
               cpMontMul(pR,        rSize,
                         pR,        rSize,
                         pModulus,  mSize,
                         pR,       &rSize,
                         pHelper,   pBuffer, MNT_BUFFER(m));
               #endif

               /* T = (A-1)*bitof(e,j) + 1 */
               SET_BNU(pBuffer, ((Ipp32s)eValue)>>31, mSize);
               cpSub_BNU(pA, pMontOne, pT, mSize, &carry);
               AND_BNU(pT, pT, pBuffer, mSize);
               cpAdd_BNU(pT, pMontOne, pT, mSize, &carry);

               /* multiply: R*T mod Modulus */
               #if defined(_USE_NN_MONTMUL_)
               cpMontMul(pR,       rSize,
                         pT,       mSize,
                         pModulus, mSize,
                         pR,      &rSize,
                         pHelper, pBuffer);
               #else
               cpMontMul(pR,       rSize,
                         pT,       mSize,
                         pModulus, mSize,
                         pR,      &rSize,
                         pHelper, pBuffer, MNT_BUFFER(m));
               #endif
            }
         }

         BN_SIZE(r) = rSize;
         return ippStsNoErr;
      }
   }
}
#endif /* _USE_ERNIE_CBA_MITIGATION_ */


#if defined(_USE_GRES_CBA_MITIGATION_)
/*
// The reason was to mitigate "cache monitoring" attack on RSA
//
// This is improved version of modular exponentiation.
// Current version provide both either mitigation and perrformance.
// This version in comparison with previous (IPP 4.1.3) one ~30-40% faster,
// i.e the the performance stayed as was for pre-mitigated version
//
*/
IPPFUN(IppStatus, ippsMontExp, (const IppsBigNumState* a, const IppsBigNumState* e,
                                IppsMontState* m,
                                IppsBigNumState* r))
{
   IPP_BAD_PTR4_RET(a, e, m, r);
   IPP_MAKE_ALIGNED_MONT(m);
   IPP_MAKE_ALIGNED_BN(a);
   IPP_MAKE_ALIGNED_BN(e);
   IPP_MAKE_ALIGNED_BN(r);
   IPP_BADARG_RET(idCtxMontgomery != m->idCtx, ippStsContextMatchErr);
   IPP_BADARG_RET(r->buffer_size < m->n->size, ippStsOutOfRangeErr);
   /* check a */
   IPP_BADARG_RET(a->sgn != IppsBigNumPOS, ippStsBadArgErr);
   IPP_BADARG_RET(cpCompare_BNUs(a->number, a->size, m->n->number, m->n->size) > 0, ippStsScaleRangeErr);
   /* check e */
   IPP_BADARG_RET(e->sgn != IppsBigNumPOS, ippStsBadArgErr);
   IPP_BADARG_RET(cpCompare_BNUs(e->number, e->size, m->n->number, m->n->size) > 0, ippStsScaleRangeErr);

   /*
   // if e==0 then r=R mod m (i.e MontEnc(1))
   */
   if (e->size == 1 && e->number[0] == 0) {
      #if defined(_IPP_TRS)
      DECLARE_BN_Word(_trs_one32_, 1);
      INIT_BN_Word(_trs_one32_);
      #endif
      return ippsMontForm(BN_ONE_REF(), m, r);
   }

   /*
   // modulo exponentiation
   */
   if(r!=a) /* init result */
      cpBN_copy(a, r);

   {
      Ipp32u eValue;
      int j;
      int back_step;

      Ipp32u* pE = BN_NUMBER(e);
      int eSize = BN_SIZE(e);

      Ipp32u* pModulus = BN_NUMBER(MNT_MODULO(m));
      int mSize = BN_SIZE(MNT_MODULO(m));
      Ipp32u* pHelper = MNT_HELPER(m);

      Ipp32u* pR = BN_NUMBER(r);
      Ipp32u* pA = BN_BUFFER(r);
      int rSize = BN_SIZE(r);

      Ipp32u* pT      = BN_NUMBER(MNT_PRODUCT(m));
      Ipp32u* pBuffer = BN_BUFFER(MNT_PRODUCT(m));

      /* expand result */
      ZEXPAND_BNU(pR,rSize, mSize);
      /* copy base */
      COPY_BNU(pR,pA, mSize);

      /* execute most significant part pE */
      eValue = pE[eSize-1];

      j=32-NLZ32u(eValue)-1;

      back_step = 0;
      for(j-=1; j>=0; j--) {
         int i;
         Ipp32u mask_pattern = (Ipp32u)(back_step-1);

         /* T = (R[] and mask_pattern) or (A[] and ~mask_pattern) */
         for(i=0; i<mSize; i++)
            pT[i] = (pR[i] & mask_pattern) | (pA[i] & ~mask_pattern);

         /* squaring/multiplication: R = R*T mod Modulus */
         #if defined(_USE_NN_MONTMUL_)
         cpMontMul(pR,       rSize,
                   pT,       mSize,
                   pModulus, mSize,
                   pR,      &rSize,
                   pHelper, pBuffer);
         #else
         cpMontMul(pR,       rSize,
                   pT,       mSize,
                   pModulus, mSize,
                   pR,      &rSize,
                   pHelper, pBuffer, MNT_BUFFER(m));
         #endif

         /* update back_step and j */
         back_step = ((eValue>>j) & 0x1) & (back_step^1);
         j += back_step;
      }

      /* execute rest bits of pE */
      eSize--;
      for(; eSize>0; eSize--) {
         eValue = pE[eSize-1];

         for(j=31; j>=0; j--) {
            int i;
            Ipp32u mask_pattern = (Ipp32u)(back_step-1);

            /* T = (R[] and mask_pattern) or (A[] and ~mask_pattern) */
            for(i=0; i<mSize; i++)
               pT[i] = (pR[i] & mask_pattern) | (pA[i] & ~mask_pattern);

            /* squaring/multiplication: R = R*T mod Modulus */
            #if defined(_USE_NN_MONTMUL_)
            cpMontMul(pR,       rSize,
                      pT,       mSize,
                      pModulus, mSize,
                      pR,      &rSize,
                      pHelper, pBuffer);
            #else
            cpMontMul(pR,       rSize,
                      pT,       mSize,
                      pModulus, mSize,
                      pR,      &rSize,
                      pHelper, pBuffer, MNT_BUFFER(m));
            #endif

            /* update back_step and j */
            back_step = ((eValue>>j) & 0x1) & (back_step^1);
            j += back_step;
         }
      }

      BN_SIZE(r) = rSize;
      return ippStsNoErr;
   }
}
#endif /* _USE_GRES_CBA_MITIGATION_ */

#endif /* 0 0*/

IPPFUN(IppStatus, ippsMontExp, (const IppsBigNumState* a, const IppsBigNumState* e,
                                IppsMontState* m,
                                IppsBigNumState* r))
{
   IPP_BAD_PTR4_RET(a, e, m, r);
   IPP_MAKE_ALIGNED_MONT(m);
   IPP_MAKE_ALIGNED_BN(a);
   IPP_MAKE_ALIGNED_BN(e);
   IPP_MAKE_ALIGNED_BN(r);
   IPP_BADARG_RET(idCtxMontgomery != m->idCtx, ippStsContextMatchErr);
   IPP_BADARG_RET(r->buffer_size < m->n->size, ippStsOutOfRangeErr);
   /* check a */
   IPP_BADARG_RET(a->sgn != IppsBigNumPOS, ippStsBadArgErr);
   IPP_BADARG_RET(cpCompare_BNUs(a->number, a->size, m->n->number, m->n->size) > 0, ippStsScaleRangeErr);
   /* check e */
   IPP_BADARG_RET(e->sgn != IppsBigNumPOS, ippStsBadArgErr);
   IPP_BADARG_RET(cpCompare_BNUs(e->number, e->size, m->n->number, m->n->size) > 0, ippStsScaleRangeErr);

   #if defined( _OPENMP )
   cpmpMontExp_Binary(r, a, e, m);
   #else
   cpMontExp_Binary(r, a, e, m);
   #endif
   return ippStsNoErr;
}
