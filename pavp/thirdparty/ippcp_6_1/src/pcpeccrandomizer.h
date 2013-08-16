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
//    Internal Definitions and
//    Internal Randomizer Function Prototypes
//
//    Created: Mon 14-Jul-2003 16:40
//  Author(s): Sergey Kirillov
//
*/
#if !defined(_PCP_RANDOMIZER_H)
#define _PCP_RANDOMIZER_H

#include "pcpprng_nnv.h"


// Randomizer accessory macros (be care _cpPRNG structure was not change !!)
//
#define RAND_ID(ctx)       ((ctx)->idCtx)
#define RAND_ROOM(ctx)     ((ctx)->RecLength)
#define RAND_Q(ctx)        ((ctx)->Q)
#define RAND_XKEYLEN(ctx)  ((ctx)->b)
#define RAND_XKEY(ctx)     ((ctx)->XKey)
#define RAND_BITSIZE(ctx)  ((ctx)->CurLength)
#define RAND_BITS(ctx)     ((ctx)->rand)
#define RAND_HASH(ctx)     ((ctx)->pHashCtx)

#define RAND_CONTENT_LEN   (16)

#endif /* _PCP_RANDOMIZER_H */

