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
//    Internal Definitions and
//    Internal Prime Modulo Arithmetic Function Prototypes
//
//    Created: Sat 24-Jul-2004 10:10
//  Author(s): Sergey Kirillov
//
*/
#if defined(_IPP_v50_)

#if !defined(_PCP_PMAFIX_H)
#define _PCP_PMAFIX_H


#include "pcpbn.h"
#include "pcpbnu.h"


/*
// Arithmetic of particular GF(p)
*/

/*
// FE Addition
*/
#define FE_ADD(R,A,B,LEN) { \
   int idx; \
   Ipp64u t = 0; \
   for(idx=0; idx<(LEN); idx++) { \
      t += (Ipp64u)((A)[idx]) + (B)[idx]; \
      (R)[idx] = LODWORD(t); \
      t = (Ipp64u)HIDWORD(t); \
   } \
   (R)[idx] = (Ipp32u)t; \
}

/*
// FE Subtraction
*/
#define FE_SUB(R,A,B,LEN) { \
   int idx; \
   Ipp32s c = 0; \
   for(idx=0; idx<(LEN); idx++) { \
      Ipp64s t = (Ipp64s)((A)[idx])-(B)[idx] + c; \
      c = HIDWORD(t); \
      (R)[idx] = LODWORD(t); \
   } \
}

/*
// FE Multiptication
*/
#define FE_MUL(R,A,B,LEN) { \
   int aidx, bidx; \
   \
   for(aidx=0; aidx<(LEN); aidx++) (R)[aidx] = 0; \
   \
   for(bidx=0; bidx<(LEN); bidx++) { \
      Ipp64u b = (B)[bidx]; \
      Ipp32u c = 0; \
      for(aidx=0; aidx<(LEN); aidx++) { \
         Ipp64u t = (R)[bidx+aidx] + (A)[aidx] * b + c; \
         (R)[bidx+aidx] = LODWORD(t); \
         c = HIDWORD(t); \
      } \
      (R)[bidx+aidx] = c; \
   } \
}

/*
// FE Squaring
*/
#define FE_SQR(R,A,LEN) FE_MUL((R),(A),(A),(LEN))

#endif /* _PCP_PMAFIX_H */

#endif /* _IPP_v50_ */
