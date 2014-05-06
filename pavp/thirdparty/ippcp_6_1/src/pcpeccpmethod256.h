/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2004-2007 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Definitions of EC methods over GF(P256)
//
//    Created: Thu 15-Aug-2004 15:14
//  Author(s): Sergey Kirillov
//
*/
#if defined(_IPP_v50_)

#if !defined(_PCP_ECCPMETHOD256_H)
#define _PCP_ECCPMETHOD256_H

#include "pcpeccp.h"


#if !defined(_IPP_TRS)
/*
// Returns reference
*/
ECCP_METHOD* ECCP256_Methods(void);

/*
// Point Set. These operations implies
// transformation of regular coordinates into internal format
*/
void ECCP256_SetPointProjective(const IppsBigNumState* pX,
                             const IppsBigNumState* pY,
                             const IppsBigNumState* pZ,
                             IppsECCPPointState* pPoint,
                             const IppsECCPState* pECC);

void ECCP256_SetPointAffine(const IppsBigNumState* pX,
                         const IppsBigNumState* pY,
                         IppsECCPPointState* pPoint,
                         const IppsECCPState* pECC);

/*
// Get Point. These operations implies
// transformation of internal format coordinates into regular
*/
void ECCP256_GetPointProjective(IppsBigNumState* pX,
                             IppsBigNumState* pY,
                             IppsBigNumState* pZ,
                             const IppsECCPPointState* pPoint,
                             const IppsECCPState* pECC);

void ECCP256_GetPointAffine(IppsBigNumState* pX,
                         IppsBigNumState* pY,
                         const IppsECCPPointState* pPoint,
                         const IppsECCPState* pECC,
                         BigNumNode* pList);

/*
// Test is On EC
*/
int ECCP256_IsPointOnCurve(const IppsECCPPointState* pPoint,
                           const IppsECCPState* pECC,
                           BigNumNode* pList);

/*
// Operations
*/
int ECCP256_ComparePoint(const IppsECCPPointState* pP,
                      const IppsECCPPointState* pQ,
                      const IppsECCPState* pECC,
                      BigNumNode* pList);

void ECCP256_NegPoint(const IppsECCPPointState* pP,
                   IppsECCPPointState* pR,
                   const IppsECCPState* pECC);

void ECCP256_DblPoint(const IppsECCPPointState* pP,
                   IppsECCPPointState* pR,
                   const IppsECCPState* pECC,
                   BigNumNode* pList);

void ECCP256_AddPoint(const IppsECCPPointState* pP,
                   const IppsECCPPointState* pQ,
                   IppsECCPPointState* pR,
                   const IppsECCPState* pECC,
                   BigNumNode* pList);

void ECCP256_MulPoint(const IppsECCPPointState* pP,
                   const IppsBigNumState* pK,
                   IppsECCPPointState* pR,
                   const IppsECCPState* pECC,
                   BigNumNode* pList);

void ECCP256_ProdPoint(const IppsECCPPointState* pP,
                    const IppsBigNumState*    bnPscalar,
                    const IppsECCPPointState* pQ,
                    const IppsBigNumState*    bnQscalar,
                    IppsECCPPointState* pR,
                    const IppsECCPState* pECC,
                    BigNumNode* pList);

#endif /* !_IPP_TRS */

#endif /* _PCP_ECCPMETHOD256_H */

#endif /* _IPP_v50_ */
