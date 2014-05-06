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
//    Internal EC Point Definitions & Function Prototypes
//
//    Created: Fri 27-Jun-2003 14:37
//  Author(s): Sergey Kirillov
//
*/
#if defined(_IPP_v50_)

#if !defined(_PCP_ECCPPOINT_H)
#define _PCP_ECCPPOINT_H

#include "pcpeccp.h"


/*
// EC Point context
*/
struct _cpECCPPoint {
   IppCtxId         idCtx;   /* EC Point identifier      */

   IppsBigNumState* pX;      /* projective X             */
   IppsBigNumState* pY;      /*            Y             */
   IppsBigNumState* pZ;      /*            Z coordinates */
   int              affine;  /* impotrant case Z=1       */
};

/*
// Contetx Access Macros
*/
#define ECP_POINT_ID(ctx)       ((ctx)->idCtx)
#define ECP_POINT_X(ctx)        ((ctx)->pX)
#define ECP_POINT_Y(ctx)        ((ctx)->pY)
#define ECP_POINT_Z(ctx)        ((ctx)->pZ)
#define ECP_POINT_AFFINE(ctx)   ((ctx)->affine)
#define ECP_POINT_VALID_ID(ctx) (ECP_POINT_ID((ctx))==idCtxECCPPoint)

#endif /* _PCP_ECCPPOINT_H */

#endif /* _IPP_v50_ */
