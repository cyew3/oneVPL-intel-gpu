/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2005-2006 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Internal RSA Resource List Definitions & Function Prototypes
//
//    Created: Wed 22-Jun-2005 13:15
//  Author(s): Sergey Kirillov
//
*/
#if !defined(_PCP_BNULISTRC_H)
#define _PCP_BNULISTRC_H


typedef struct {
   void*    pNext;
   Ipp32u*  pBNU;
   int      num;
} BNUnode;


/* size (byte) of BNU resource */
int cpBNUListGetSize(int bitSize, int nodes);

/* init BNU resource */
void cpBNUListInit(BNUnode* pHead, int bitSize, int nodes);

/* get BNU from resource */
Ipp32u* cpBNUListGet(BNUnode** pHead);

/* get BNU array from resource */
Ipp32u* cpBNUListGetArray(BNUnode** pHead, int num);

#endif /* _PCP_BNULISTRC_H */
