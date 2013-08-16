/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2002-2006 Intel Corporation. All Rights Reserved.
//
//              Intel(R) Integrated Performance Primitives
//                  Cryptographic Primitives (ippcp)
//
//  File:       pcpmontmulca.c
//  Created:    2003/04/22    13:54
//
//  2003/12/04 modified by Andrey Somsikov
//      Functions redesigned as preparation for opening the source
//  2003/04/22 modified by Alexey Omeltchenko
*/

#include "precomp.h"
#include "owncp.h"
#include "pcpbn.h"
#include "pcpmontgomery.h"

/*F*
// Name: ippsMontMul
//
// Purpose: Computes Montgomery modular multiplication for positive big
//      number integers of Montgomery form. The following pseudocode
//      represents this function:
//      r <- ( a * b * R^(-1) ) mod m
//
// Returns:                Reason:
//      ippStsNoErr         Returns no error.
//      ippStsNullPtrErr    Returns an error when pointers are null.
//      ippStsBadArgErr     Returns an error when a or b is a negative integer.
//      ippStsScaleRangeErr Returns an error when a or b is more than m.
//      ippStsOutOfRangeErr Returns an error when IppsBigNumState *r is larger than
//                          IppsMontState *m.
//      ippStsContextMatchErr Returns an error when the context parameter does
//                          not match the operation.
//
// Parameters:
//      a   Multiplicand within the range [0, m - 1].
//      b   Multiplier within the range [0, m - 1].
//      m   Modulus.
//      r   Montgomery multiplication result.
//
// Notes: The size of IppsBigNumState *r should not be less than the data
//      length of the modulus m.
*F*/
IPPFUN(IppStatus, ippsMontMul, (const IppsBigNumState *x, const IppsBigNumState *y, IppsMontState *pMont, IppsBigNumState *r))
{
   IPP_BAD_PTR4_RET(x, y, pMont, r);
   IPP_MAKE_ALIGNED_MONT(pMont);
   IPP_MAKE_ALIGNED_BN(x);
   IPP_MAKE_ALIGNED_BN(y);
   IPP_MAKE_ALIGNED_BN(r);
   IPP_BADARG_RET(idCtxMontgomery != pMont->idCtx, ippStsContextMatchErr);
   IPP_BADARG_RET(x->sgn != IppsBigNumPOS || y->sgn != IppsBigNumPOS, ippStsBadArgErr);
   IPP_BADARG_RET(cpCompare_BNUs(x->number, x->size, pMont->n->number, pMont->n->size) > 0, ippStsScaleRangeErr);
   IPP_BADARG_RET(cpCompare_BNUs(y->number, y->size, pMont->n->number, pMont->n->size) > 0, ippStsScaleRangeErr);
   IPP_BADARG_RET(r->buffer_size < pMont->n->size, ippStsOutOfRangeErr);

   #if defined(_USE_NN_MONTMUL_)
   cpMontMul(x->number, x->size, y->number,y->size, pMont->n->number,pMont->n->size,
             r->number,&r->size, pMont->n0, pMont->wb->number);
   r->sgn = IppsBigNumPOS;

   while ((r->size > 1) && (r->number[r->size-1] == 0))
      r->size--;

   #else
   cpMontMul(x->number, x->size, y->number,y->size, pMont->n->number,pMont->n->size,
             r->number,&r->size, pMont->n0, pMont->wb->number, pMont->pBuffer);
   r->sgn = IppsBigNumPOS;
   #endif

   return ippStsNoErr;
}
