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
//    Addition Montgomery Definitions & Function Prototypes
//
//    Created: Thu 09-Dec-2004 14:45
//  Author(s): Sergey Kirillov
//
*/
#if !defined(_PCP_MONTEXT_H)
#define _PCP_MONTEXT_H

#include "pcpmontgomery.h"
#include "pcpbnuresource.h"


/* fast exponential */
void cpMontExp_32u(IppsBigNumState* pY,
             const IppsBigNumState* pX, Ipp32u e,
                   IppsMontState* pMont);

void cpMontExp_Binary(IppsBigNumState* pY,
                const IppsBigNumState* pX, const IppsBigNumState* pE,
                      IppsMontState* pMont);
#if defined( _OPENMP )
void cpmpMontExp_Binary(IppsBigNumState* pY,
                  const IppsBigNumState* pX, const IppsBigNumState* pE,
                        IppsMontState* pMont);
#endif

#if defined(_USE_WINDOW_EXP_)
int WindowExpSize(int bitsize);

void cpMontExp_Window(IppsBigNumState* pY,
                const IppsBigNumState* pX, const IppsBigNumState* pE,
                      IppsMontState* pMont,
                      BNUnode* pList);
#endif
#endif /* _PCP_MONTEXT_H */
