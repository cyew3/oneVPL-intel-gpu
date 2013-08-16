/*
#***************************************************************************
# INTEL CONFIDENTIAL
# Copyright (C) 2008-2010 Intel Corporation All Rights Reserved. 
# 
# The source code contained or described herein and all documents 
# related to the source code ("Material") are owned by Intel Corporation 
# or its suppliers or licensors. Title to the Material remains with 
# Intel Corporation or its suppliers and licensors. The Material contains 
# trade secrets and proprietary and confidential information of Intel or 
# its suppliers and licensors. The Material is protected by worldwide 
# copyright and trade secret laws and treaty provisions. No part of the 
# Material may be used, copied, reproduced, modified, published, uploaded, 
# posted, transmitted, distributed, or disclosed in any way without 
# Intel's prior express written permission.
# 
# No license under any patent, copyright, trade secret or other intellectual 
# property right is granted to or conferred upon you by disclosure or 
# delivery of the Materials, either expressly, by implication, inducement, 
# estoppel or otherwise. Any license under such intellectual property rights 
# must be express and approved by Intel in writing.
#***************************************************************************
*/


/** 
 * @file
 *
 * @brief This file implements print routines for some epid math
 * objects
 */

#include <stdio.h>
#include <assert.h>
#include "printutils.h"
#include "epid_macros.h"
#include "epid_constants.h"

// dir 0 = big endian source
// otherwise = little endian source
void PrintOctString(const Ipp8u* pStr, const int strLen, const int dir)
{
    int i;
    const Ipp8u *pCur = NULL;
    Ipp8u cur;
    for (i = 0, pCur = pStr + (dir?(strLen - 1):0); 
         i < strLen; i++) {
        cur = *pCur;
        if (i > 0 && i%4 == 0) {
            printf (" ");
        }
        if (i > 0 && i%32 == 0) {
            printf ("\n");
            printf("         ");
        }
        printf ("%02X", cur);
        pCur += (dir?-1:1);
    }
    printf ("\n");
}

void PrintPrimeField(const IppsGFPState* pGF)
{
    IppStatus sts;
    

    const Ipp32u *pPrime;
    Ipp32u primeLen;
    sts = ippsGFPGet(pGF, &pPrime, &primeLen);
    assert(!sts);
    printf("xxxxx PrimeField xxxxx\n");
    printf(" prime : ");PrintOctString((Ipp8u*)pPrime, EPID_NUMBER_SIZE, 1);
    printf(" len   : ");PrintOctString((Ipp8u*)&primeLen, sizeof(primeLen), 1);
    printf("xxxxxxxxxxxxxxxxxxxxxx\n") ;   
}

void PrintGFPElement(const IppsGFPElement* pA, const IppsGFPState* pGF) 
{
    IppStatus sts;  

    Ipp8u databuf[EPID_NUMBER_SIZE];
    
    Ipp32u* pDataA = (Ipp32u*)databuf;
    Ipp32u dataLen = sizeof(databuf)/sizeof(Ipp32u);
    sts = ippsGFPGetElement(pA, 
                            pDataA, 
                            dataLen, 
                            pGF);
    assert(!sts);
    printf("+++++ PF Element +++++\n");
    printf(" elem  : ");PrintOctString((Ipp8u*)pDataA, EPID_NUMBER_SIZE, 1);
    printf("++++++++++++++++++++++\n") ;     
}

void PrintECPrimeField(const IppsGFPECState *pEC) 
{
    IppStatus sts;
    
    IppsGFPState* pGF;
    Ipp32u elementLen;
    IppsGFPElement *pA = NULL;
    IppsGFPElement *pB = NULL;
    IppsGFPElement *pX = NULL;
    IppsGFPElement *pY = NULL;
    const Ipp32u *pOrder;
    Ipp32u orderLen;
    Ipp32u cofactor;

    Ipp32u gfElemSize = 0;
    const Ipp32u a[] = {0};

    sts = ippsGFPECGet(pEC,
                       (const IppsGFPState**)&pGF, &elementLen,
                       pA, pB,
                       pX, pY,
                       &pOrder, &orderLen,
                       &cofactor);
    assert(!sts);

    //we must allocate space for the elements to actually extract them
    sts = ippsGFPElementGetSize(pGF, &gfElemSize);
    assert(!sts); 
    pA = SAFE_ALLOC(gfElemSize);
    pB = SAFE_ALLOC(gfElemSize);
    pX = SAFE_ALLOC(gfElemSize);
    pY = SAFE_ALLOC(gfElemSize);
    assert (pA && pB && pX && pY);
    sts = ippsGFPElementInit(pA, a, sizeof(a)/sizeof(Ipp32u), pGF);
    assert(!sts);
    sts = ippsGFPElementInit(pB, a, sizeof(a)/sizeof(Ipp32u), pGF);
    assert(!sts);
    sts = ippsGFPElementInit(pX, a, sizeof(a)/sizeof(Ipp32u), pGF);
    assert(!sts);
    sts = ippsGFPElementInit(pY, a, sizeof(a)/sizeof(Ipp32u), pGF);
    assert(!sts);
    
    // call again to get elements we missed
    sts = ippsGFPECGet(pEC,
                       (const IppsGFPState**)&pGF, &elementLen,
                       pA, pB,
                       pX, pY,
                       &pOrder, &orderLen,
                       &cofactor);
    assert(!sts);

    printf("!!!!! EC  PField !!!!!\n");
    PrintPrimeField(pGF);
    printf("elemlen: ");PrintOctString((Ipp8u*)&elementLen, sizeof(Ipp32u), 1);
    
    printf("      A: ");PrintGFPElement(pA, pGF);
    printf("      B: ");PrintGFPElement(pB, pGF);
    printf("      X: ");PrintGFPElement(pX, pGF);
    printf("      Y: ");PrintGFPElement(pY, pGF);
    

    printf("order  : ");
    PrintOctString((Ipp8u*)pOrder, orderLen * sizeof(Ipp32u), 1);
    printf("order_L: ");PrintOctString((Ipp8u*)&orderLen, sizeof(Ipp32u), 1);
    printf("cofactr: ");PrintOctString((Ipp8u*)&cofactor, sizeof(Ipp32u), 1);
    
    printf("!!!!!!!!!!!!!!!!!!!!!!\n") ; 
    
    SAFE_FREE(pA);
    SAFE_FREE(pB);
    SAFE_FREE(pX);
    SAFE_FREE(pY);
}

void PrintECPoint(const IppsGFPECPoint* pPoint, IppsGFPECState *pEC) 
{
    IppStatus sts;

    IppsGFPElement *pX = NULL;
    IppsGFPElement *pY = NULL;

    IppsGFPState* pGF;
    Ipp32u gfElemSize = 0;
    const Ipp32u a[] = {0};

    sts = ippsGFPECGet(pEC,
                       (const IppsGFPState**)&pGF, 0,
                       0, 0,
                       0, 0,
                       0, 0,
                       0);
    assert(!sts);

    sts = ippsGFPElementGetSize(pGF, &gfElemSize);
    assert(!sts); 

    pX = SAFE_ALLOC(gfElemSize);
    pY = SAFE_ALLOC(gfElemSize);
    assert (pX && pY);

    sts = ippsGFPElementInit(pX, a, sizeof(a)/sizeof(Ipp32u), pGF);
    assert(!sts);
    sts = ippsGFPElementInit(pY, a, sizeof(a)/sizeof(Ipp32u), pGF);
    assert(!sts);
    

    sts = ippsGFPECGetPoint(pPoint, 
                            pX, pY,
                            pEC);
    assert(!sts);
    printf("^^^^^ PF ECPoint ^^^^^\n");
    printf("      X: ");PrintGFPElement(pX, pGF);
    printf("      Y: ");PrintGFPElement(pY, pGF);
    printf("^^^^^^^^^^^^^^^^^^^^^^\n");

    SAFE_FREE(pX);
    SAFE_FREE(pY);
}

void PrintFiniteField(const IppsGFPXState* pGFPExtCtx)
{
    IppStatus sts;
    
    IppsGFPState* pGF = NULL;
    Ipp32u* pModulus = NULL;
    Ipp32u modulusDegree = 0;
    Ipp32u modulusLen = 0;
    Ipp32u elementLen = 0;

    unsigned int i;
    
    sts = ippsGFPXGet(pGFPExtCtx,
                      (const IppsGFPState**)&pGF, 
                      pModulus, &modulusDegree, 
                      &modulusLen,
                      &elementLen);
    assert(!sts);
    
    // allocate space for modulus
    pModulus = SAFE_ALLOC(modulusLen * sizeof(Ipp32u));
    assert (pModulus);

    // get again to fill modulus
    sts = ippsGFPXGet(pGFPExtCtx,
                      (const IppsGFPState**)&pGF, 
                      pModulus, &modulusDegree, 
                      &modulusLen,
                      &elementLen);
    assert(!sts);


    printf("##### FiniteField #####\n");
    PrintPrimeField(pGF);
    printf(" degree: ");PrintOctString((Ipp8u*)&modulusDegree, sizeof(Ipp32u), 1);
    printf(" modlen: ");PrintOctString((Ipp8u*)&modulusLen, sizeof(Ipp32u), 1);
    printf(" elmlen: ");PrintOctString((Ipp8u*)&elementLen, sizeof(Ipp32u), 1);
    printf(" coeffs: \n");
    for (i=0;i < modulusDegree + 1 ;i++) {
        Ipp32u* pCoeff = pModulus + i*modulusLen/(modulusDegree+1);
        printf("    [%d]: ", i);
        PrintOctString((unsigned char*)pCoeff,
                       sizeof(Ipp32u)*modulusLen/(modulusDegree+1), 1);
    }
    
    printf("#######################\n") ;  

    SAFE_FREE(pModulus);
}


void PrintGFPXElement(const IppsGFPXElement* pA, const IppsGFPXState* pGF) 
{
    IppStatus sts;  

    Ipp8u databuf[3*EPID_NUMBER_SIZE];
    
    Ipp32u* pData0 = (Ipp32u*)databuf;
    Ipp32u* pData1 = (Ipp32u*)(databuf + EPID_NUMBER_SIZE);
    Ipp32u* pData2 = (Ipp32u*)(databuf + 2*EPID_NUMBER_SIZE);
    Ipp32u dataLen = sizeof(databuf)/sizeof(Ipp32u);
    sts = ippsGFPXGetElement(pA, 
                             pData0, 
                             dataLen, 
                             pGF);
    assert(!sts);
    
    printf("..... PFXElement .....\n");
    printf(" elem 0: ");PrintOctString((Ipp8u*)pData0, EPID_NUMBER_SIZE, 1);
    printf(" elem 1: ");PrintOctString((Ipp8u*)pData1, EPID_NUMBER_SIZE, 1);
    printf(" elem 2: ");PrintOctString((Ipp8u*)pData2, EPID_NUMBER_SIZE, 1);
    printf("......................\n") ;     
}


void PrintECFiniteField(const IppsGFPXECState *pEC) 
{
    IppStatus sts;
    
    IppsGFPXState *pGF;
    Ipp32u elementLen;
    IppsGFPXElement *pA = NULL;
    IppsGFPXElement *pB = NULL;
    IppsGFPXElement *pX = NULL;
    IppsGFPXElement *pY = NULL;
    const Ipp32u *pOrder;  
    Ipp32u orderLen;
    Ipp32u cofactor;
    
    Ipp32u gfElemSize = 0;
    const Ipp32u a[] = {0};
    
    sts = ippsGFPXECGet(pEC,
                        (const IppsGFPXState**)&pGF, &elementLen,
                        pA, pB,
                        pX, pY,
                        &pOrder, &orderLen,
                        &cofactor);
    assert(!sts);
    
    //we must allocate space for the elements to actually extract them
    sts = ippsGFPXElementGetSize(pGF, &gfElemSize);
    assert(!sts); 
    pA = SAFE_ALLOC(gfElemSize);
    pB = SAFE_ALLOC(gfElemSize);
    pX = SAFE_ALLOC(gfElemSize);
    pY = SAFE_ALLOC(gfElemSize);
    assert (pA && pB && pX && pY);
    sts = ippsGFPXElementInit(pA, a, sizeof(a)/sizeof(Ipp32u), pGF);
    assert(!sts);
    sts = ippsGFPXElementInit(pB, a, sizeof(a)/sizeof(Ipp32u), pGF);
    assert(!sts);
    sts = ippsGFPXElementInit(pX, a, sizeof(a)/sizeof(Ipp32u), pGF);
    assert(!sts);
    sts = ippsGFPXElementInit(pY, a, sizeof(a)/sizeof(Ipp32u), pGF);
    assert(!sts);
    
    // call again to get elements we missed
    sts = ippsGFPXECGet(pEC,
                        (const IppsGFPXState**)&pGF, &elementLen,
                        pA, pB,
                        pX, pY,
                        &pOrder, &orderLen,
                        &cofactor);
    assert(!sts);
    
    printf("@@@@@ EC  FField @@@@@\n");
    PrintFiniteField(pGF);
    printf("elemlen: ");PrintOctString((Ipp8u*)&elementLen, sizeof(Ipp32u), 1);
    
    printf("      A: ");PrintGFPXElement(pA, pGF);
    printf("      B: ");PrintGFPXElement(pB, pGF);
    printf("      X: ");PrintGFPXElement(pX, pGF);
    printf("      Y: ");PrintGFPXElement(pY, pGF);
    

    printf("order  : ");
    PrintOctString((Ipp8u*)pOrder, orderLen * sizeof(Ipp32u), 1);
    printf("order_L: ");PrintOctString((Ipp8u*)&orderLen, sizeof(Ipp32u), 1);
    printf("cofactr: ");PrintOctString((Ipp8u*)&cofactor, sizeof(Ipp32u), 1);
    
    printf("@@@@@@@@@@@@@@@@@@@@@@\n") ; 
    
    SAFE_FREE(pA);
    SAFE_FREE(pB);
    SAFE_FREE(pX);
    SAFE_FREE(pY);
    
}

void PrintXECPoint(const IppsGFPXECPoint* pPoint, IppsGFPXECState *pEC)
{
    IppStatus sts;

    IppsGFPXElement *pX = NULL;
    IppsGFPXElement *pY = NULL;

    IppsGFPXState* pGF;
    Ipp32u gfElemSize = 0;
    const Ipp32u a[] = {0};

    sts = ippsGFPXECGet(pEC,
                        (const IppsGFPXState**)&pGF, 
                        0,
                        0, 0,
                        0, 0,
                        0, 0,
                        0);
    assert(!sts);

    sts = ippsGFPXElementGetSize(pGF, &gfElemSize);
    assert(!sts); 

    pX = SAFE_ALLOC(gfElemSize);
    pY = SAFE_ALLOC(gfElemSize);
    assert (pX && pY);

    sts = ippsGFPXElementInit(pX, a, sizeof(a)/sizeof(Ipp32u), pGF);
    assert(!sts);
    sts = ippsGFPXElementInit(pY, a, sizeof(a)/sizeof(Ipp32u), pGF);
    assert(!sts);
    
    
    sts = ippsGFPXECGetPoint(pPoint, 
                             pX, 
                             pY,
                             pEC);
    assert(!sts);

    printf("$$$$$ PFXECPoint $$$$$\n");
    printf("      X: ");PrintGFPXElement(pX, pGF);
    printf("      Y: ");PrintGFPXElement(pY, pGF);
    printf("$$$$$$$$$$$$$$$$$$$$$$\n");
    
    SAFE_FREE(pX);
    SAFE_FREE(pY);
}

void PrintQuadField(const IppsGFPXQState* pGF)
{
    IppStatus sts; 
    IppsGFPXState* pGroundField = NULL;
    Ipp32u* pModulus = NULL;
    Ipp32u modulusDegree = 0;
    Ipp32u modulusLen = 0;
    Ipp32u elementLen = 0;

    unsigned int i;
    
    sts = ippsGFPXQGet(pGF,
                       (const IppsGFPXState**)&pGroundField, 
                       pModulus, 
                       &modulusDegree, 
                       &modulusLen,
                       &elementLen);
    assert(!sts);
    
    // allocate space for modulus
    pModulus = SAFE_ALLOC(modulusLen * sizeof(Ipp32u));
    assert (pModulus);
    
    // get again to fill modulus
    sts = ippsGFPXQGet(pGF,
                       (const IppsGFPXState**)&pGroundField, 
                       pModulus, 
                       &modulusDegree, 
                       &modulusLen,
                       &elementLen);
    assert(!sts);

    printf("##### QuadFieldExt #####\n");
    PrintFiniteField(pGroundField);
    printf(" degree: ");PrintOctString((Ipp8u*)&modulusDegree, sizeof(Ipp32u), 1);
    printf(" modlen: ");PrintOctString((Ipp8u*)&modulusLen, sizeof(Ipp32u), 1);
    printf(" elmlen: ");PrintOctString((Ipp8u*)&elementLen, sizeof(Ipp32u), 1);
    printf(" coeffs: \n");
    //PrintOctString(pModulus, modulusLen*sizeof(Ipp32u), 1);
    for (i=0;i < modulusDegree + 1 ;i++) {
        Ipp32u* pCoeff = pModulus + i*modulusLen/(modulusDegree+1);
        printf("    [%d]: ", i);
        PrintOctString((unsigned char*)pCoeff, 
                       sizeof(Ipp32u)*modulusLen/(modulusDegree+1), 1);
    }
    
    printf("########################\n") ; 

    SAFE_FREE(pModulus);
}

void PrintGFPXQElement(const IppsGFPXQElement* pA, const IppsGFPXQState* pGF)
{
    IppStatus sts;  
    
    Ipp8u databuf[6*EPID_NUMBER_SIZE];
    
    Ipp32u* pData0 = (Ipp32u*)databuf;
    Ipp32u* pData1 = (Ipp32u*)(databuf + EPID_NUMBER_SIZE);
    Ipp32u* pData2 = (Ipp32u*)(databuf + 2*EPID_NUMBER_SIZE);
    Ipp32u* pData3 = (Ipp32u*)(databuf + 3*EPID_NUMBER_SIZE);
    Ipp32u* pData4 = (Ipp32u*)(databuf + 4*EPID_NUMBER_SIZE);
    Ipp32u* pData5 = (Ipp32u*)(databuf + 5*EPID_NUMBER_SIZE);
    Ipp32u dataLen = sizeof(databuf)/sizeof(Ipp32u);
    sts = ippsGFPXQGetElement(pA, 
                              pData0, 
                              dataLen, 
                              pGF);
    assert(!sts);
    
    printf("_____ PFXQElement ____\n");
    printf(" elem 0: ");PrintOctString((Ipp8u*)pData0, EPID_NUMBER_SIZE, 1);
    printf(" elem 1: ");PrintOctString((Ipp8u*)pData1, EPID_NUMBER_SIZE, 1);
    printf(" elem 2: ");PrintOctString((Ipp8u*)pData2, EPID_NUMBER_SIZE, 1);
    printf(" elem 3: ");PrintOctString((Ipp8u*)pData3, EPID_NUMBER_SIZE, 1);
    printf(" elem 4: ");PrintOctString((Ipp8u*)pData4, EPID_NUMBER_SIZE, 1);
    printf(" elem 5: ");PrintOctString((Ipp8u*)pData5, EPID_NUMBER_SIZE, 1);
    printf("______________________\n") ;     
}
