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
//    Internal ECC (prime) Resource List Function
//
//    Created: Sun 29-Jun-2003 12:24
//  Author(s): Sergey Kirillov
//
*/
#if defined(_IPP_v50_)

#include "precomp.h"
#include "owncp.h"
#include "pcpbnresource.h"
#include "pcpbn.h"

/*
// Size of BigNum List Buffer
*/
int cpBigNumListGetSize(int feBitSize, int nodes)
{
   /* size of buffer per single big number */
   int bnSize;
   ippsBigNumGetSize(BITS2WORD32_SIZE(feBitSize), &bnSize);

   /* size of buffer for whole list */
   return (ALIGN_VAL-1) + (sizeof(BigNumNode) + bnSize) * nodes;
}

/*
// Init list
//
// Note: buffer for BN list must have appropriate alignment
*/
void cpBigNumListInit(int feBitSize, int nodes, BigNumNode* pList)
{
   int itemSize;
   /* length of Big Num */
   int bnLen = BITS2WORD32_SIZE(feBitSize);
   /* size of buffer per single big number */
   ippsBigNumGetSize(bnLen, &itemSize);
   /* size of list item */
   itemSize += sizeof(BigNumNode);

   {
      int n;
      /* init all nodes */
      BigNumNode* pNode = (BigNumNode*)( (Ipp8u*)pList + (nodes-1)*itemSize );
      BigNumNode* pNext = NULL;
      for(n=0; n<nodes; n++) {
         Ipp8u* tbnPtr = (Ipp8u*)pNode + sizeof(BigNumNode);
         pNode->pNext = pNext;
         pNode->pBN = (IppsBigNumState*)( IPP_ALIGNED_PTR(tbnPtr, ALIGN_VAL) );
         ippsBigNumInit(bnLen, pNode->pBN);
         pNext = pNode;
         pNode = (BigNumNode*)( (Ipp8u*)pNode - itemSize);
      }
   }
}

/*
// Get BigNum reference
*/
IppsBigNumState* cpBigNumListGet(BigNumNode** ppList)
{
   if(*ppList) {
      IppsBigNumState* ret = (*ppList)->pBN;
      *ppList = (*ppList)->pNext;
      return ret;
   }
   else
      return NULL;
}

#endif /* _IPP_v50_ */
