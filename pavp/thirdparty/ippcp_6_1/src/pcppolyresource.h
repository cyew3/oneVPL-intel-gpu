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
//    Internal ECC (binary) Resource List Definitions & Function Prototypes
//
//    Created: Mon 15-Sep-2003 11:28
//  Author(s): Sergey Kirillov
//
*/
#if !defined(_PCP_ECCBLISTRC_H)
#define _PCP_ECCBLISTRC_H

#include "pcpelpoly.h"


typedef struct {
   void*    pNext;
   EPOLY*   pPoly;
} PolyNode;


/* size (byte) of polynomial resource */
int cpPolyListGetSize(int bitSize, int nodes);

/* init polynomial resource */
void cpPolyListInit(PolyNode* pHead, int bitSize, int nodes);

/* get polynomial from resource */
EPOLY* cpPolyListGet(PolyNode** pHead);

#endif /* _PCP_ECCBLISTRC_H */
