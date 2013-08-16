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
//    Internal BN Resource Definitions & Function Prototypes
//
//    Created: Sun 29-Jun-2003 12:16
//  Author(s): Sergey Kirillov
//
*/
#if !defined(_PCP_BNRESOURCE_H)
#define _PCP_BNRESOURCE_H


typedef struct {
   void*            pNext;
   IppsBigNumState* pBN;
} BigNumNode;


/* size (byte) of BN resource */
int  cpBigNumListGetSize(int feBitSize, int nodes);

/* init BN resource */
void cpBigNumListInit(int feBitSize, int nodes, BigNumNode* pList);

/* get BN from resource */
IppsBigNumState* cpBigNumListGet(BigNumNode** pList);

#endif /* _PCP_BNRESOURCE_H */
