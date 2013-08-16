/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2003-2007 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Internal EC Function Prototypes
//
//    Created: Sun 25-Jul-2004 10:37
//  Author(s): Sergey Kirillov
//
*/
#if defined(_IPP_v50_)


#if !defined(_IPP_TRS)
/*
// Point Operation Prototypes
// May be varied for different kinf of GP(p)
*/
struct eccp_method_st {
   //void (*CopyPoint)(const IppsECCPPointState* pSrc, IppsECCPPointState* pDst);

    void (*SetPointProjective)(const IppsBigNumState* pX,
                              const IppsBigNumState* pY,
                              const IppsBigNumState* pZ,
                              IppsECCPPointState* pPoint,
                              const IppsECCPState* pECC);
   void (*SetPointAffine)(const IppsBigNumState* pX,
                          const IppsBigNumState* pY,
                          IppsECCPPointState* pPoint,
                          const IppsECCPState* pECC);

   void (*GetPointProjective)(IppsBigNumState* pX,
                              IppsBigNumState* pY,
                              IppsBigNumState* pZ,
                              const IppsECCPPointState* pPoint,
                              const IppsECCPState* pECC);
   void (*GetPointAffine)(IppsBigNumState* pX,
                          IppsBigNumState* pY,
                          const IppsECCPPointState* pPoint,
                          const IppsECCPState* pECC,
                          BigNumNode* pList);

   //void (*SetPointToInfinity)(IppsECCPPointState* pPoint);
   //void (*SetPointToAffineInfinity0)(IppsBigNumState* pX, IppsBigNumState* pY);
   //void (*SetPointToAffineInfinity1)(IppsBigNumState* pX, IppsBigNumState* pY);

   //int (*IsPointAtInfinity)(const IppsECCPPointState* pPoint);
   //int (*IsPointAtAffineInfinity0)(const IppsBigNumState* pX, const IppsBigNumState* pY);
   //int (*IsPointAtAffineInfinity1)(const IppsBigNumState* pX, const IppsBigNumState* pY);
   int (*IsPointOnCurve)(const IppsECCPPointState* pPoint,
                         const IppsECCPState* pECC,
                         BigNumNode* pList);

   int (*ComparePoint)(const IppsECCPPointState* pP,
                       const IppsECCPPointState* pQ,
                       const IppsECCPState* pECC,
                       BigNumNode* pList);
   void (*NegPoint)(const IppsECCPPointState* pP,
                    IppsECCPPointState* pR,
                    const IppsECCPState* pECC);
   void (*DblPoint)(const IppsECCPPointState* pP,
                    IppsECCPPointState* pR,
                    const IppsECCPState* pECC,
                    BigNumNode* pList);
   void (*AddPoint)(const IppsECCPPointState* pP,
                    const IppsECCPPointState* pQ,
                    IppsECCPPointState* pR,
                    const IppsECCPState* pECC,
                    BigNumNode* pList);
   void (*MulPoint)(const IppsECCPPointState* pP,
                    const IppsBigNumState* pK,
                    IppsECCPPointState* pR,
                    const IppsECCPState* pECC,
                    BigNumNode* pList);
   void (*ProdPoint)(const IppsECCPPointState* pP,
                     const IppsBigNumState*    bnPscalar,
                     const IppsECCPPointState* pQ,
                     const IppsBigNumState*    bnQscalar,
                     IppsECCPPointState* pR,
                     const IppsECCPState* pECC,
                     BigNumNode* pList);
};

#endif /* !_IPP_TRS */

#endif  /* _IPP_v50_ */
