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
 * @brief This file implements octet string conversion routines
 */

#include <assert.h>
#include "octstring.h"
#include "epid_macros.h"
#include "epid_constants.h"
#include "memutils.h"

IppStatus octStr2BNU (const Ipp8u* pStr, int strLen, Ipp32u* pBNU)
{
    int BNUsize = (strLen + sizeof(Ipp32u) - 1) / sizeof(Ipp32u);
    IppStatus sts;
    sts = ippsSetOctString_BNU(pStr, strLen, 
                               pBNU, &BNUsize);
    return (sts);
}

IppStatus BNU2OctStr (const Ipp32u* pBNU, Ipp8u* pStr, int strLen)
{
    int BNUsize = (strLen + sizeof(Ipp32u) - 1) / sizeof(Ipp32u);
    IppStatus sts;
    sts = ippsGetOctString_BNU(pBNU, BNUsize,
                               pStr, strLen);
    return (sts);
}

int getBitSize(Ipp32u* num, Ipp32u numLen)
{
    int   byteIndex;
    int   bitIndex;
    Ipp8u byte;
    for (byteIndex = numLen * 4 - 1; 0 <= byteIndex; byteIndex--) {
        byte = ((Ipp8u*) num)[byteIndex];
        for (bitIndex = 0; 8 > bitIndex; bitIndex++) {
            if (0x80 & byte) {
                return ((byteIndex + 1) * 8 - bitIndex);
            }
            byte = byte << 1;
        }
    }
    // The only way we get here is if the number is zero
    return (0);
}

int getBit(Ipp32u* num, Ipp32u bitIndex, Ipp32u bitLen)
{
    if (bitIndex >= bitLen) {
        return 0;
    } else {
        return 0 != (num[bitIndex>>5] & (1 << (bitIndex & 0x1F)));
    }    
}

IppStatus GetOctString_GFPElem(const IppsGFPElement* pA, 
                               Ipp8u* pStr, int strLen,
                               const IppsGFPState* pGF)
{
    IppStatus sts = ippStsNoErr; 

    do {
        Ipp8u databuf[EPID_NUMBER_SIZE];

        Ipp32u* pDataA = (Ipp32u*)databuf;
        Ipp32u dataLen = sizeof(databuf)/sizeof(Ipp32u);

        sts = ippsGFPGetElement(pA, 
            pDataA, 
            dataLen, 
            pGF);
        if (ippStsNoErr != sts) {break;}

        sts = ippsGetOctString_BNU(pDataA, 
                                   dataLen, 
                                   pStr, strLen);
        if (ippStsNoErr != sts) {break;}
    } while(0);
    return(sts); 
}

IppStatus SetOctString_GFPElem(const Ipp8u* pStr, const int strLen,
                               IppsGFPElement* pA, 
                               IppsGFPState* pGF)
{
    IppStatus sts = ippStsNoErr; 
    
    do {
        Ipp8u databuf[EPID_NUMBER_SIZE];

        Ipp32u* pDataA = (Ipp32u*)databuf;
        Ipp32u dataLen = sizeof(databuf)/sizeof(Ipp32u);

        sts = ippsSetOctString_BNU(pStr, 
                                   strLen, 
                                   pDataA, (int*)&dataLen);
        if (ippStsNoErr != sts) {break;}

        sts = ippsGFPSetElement(pDataA,  
                                dataLen, 
                                pA,
                                pGF);
        if (ippStsNoErr != sts) {break;}
    } while(0);
    return (sts);
}

IppStatus GetOctString_GFPECPoint(const IppsGFPECPoint* pPoint, 
                                  Ipp8u* pStr, int strLen,
                                  IppsGFPECState* pEC)
{
    IppStatus sts = ippStsNoErr;
    IppECResult ecResult = ippECPointIsNotValid;
   
    IppsGFPElement *pX = NULL;
    IppsGFPElement *pY = NULL;

    IppsGFPState *pGF = NULL;
    do {
        Ipp32u gfElemSize = 0;
        const Ipp32u a[] = {0};

        sts = ippsGFPECGet(pEC,
                           (const IppsGFPState**)&pGF, 0,
                           0, 0,
                           0, 0,
                           0, 0,
                           0);
        if (ippStsNoErr != sts) {break;}

        sts = ippsGFPElementGetSize(pGF, &gfElemSize);
        if (ippStsNoErr != sts) {break;} 

        pX = SAFE_ALLOC(gfElemSize);
        pY = SAFE_ALLOC(gfElemSize);
        if (!(pX && pY)) {
            sts = ippStsNoMemErr; 
            break;
        }

        sts = ippsGFPElementInit(pX, a, sizeof(a)/sizeof(Ipp32u), pGF);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPElementInit(pY, a, sizeof(a)/sizeof(Ipp32u), pGF);
        if (ippStsNoErr != sts) {break;}

        sts = ippsGFPECVerifyPoint(pPoint, &ecResult, pEC);
        if (ippStsNoErr != sts) {break;}
        if (ippECValid == ecResult) {
            sts = ippsGFPECGetPoint(pPoint, 
                                    pX, pY,
                                    pEC);
            if (ippStsNoErr != sts) {break;}
        } else if (ippECPointIsAtInfinite == ecResult) {
            /*
             * pPoint is point at infinity.  Since pX, pY are
             * already (0,0), which we treat as point at infinity
             * for serilization purposes so all we have to do here
             * is just fallthru, and report no error
             */ 
            sts = ippStsNoErr; 
        } else {
            // result is ippECPointOutOfGroup or ippECPointIsNotValid, error
            sts = ippStsErr;
            break;
        }
                 
        // now convert the elements
        sts = GetOctString_GFPElem(pX, pStr, strLen/2, pGF);
        if (ippStsNoErr != sts) {break;}
        sts = GetOctString_GFPElem(pY, pStr + strLen/2, strLen/2, pGF);
        if (ippStsNoErr != sts) {break;}
    } while(0);
    SAFE_FREE(pX);
    SAFE_FREE(pY);
 
    return(sts);
}

IppStatus SetOctString_GFPECPoint(const Ipp8u* pStr, const int strLen, 
                                  IppsGFPECPoint* pPoint, 
                                  IppsGFPECState *pEC)
{
    IppStatus sts = ippStsNoErr;
   
    IppsGFPElement *pX = NULL;
    IppsGFPElement *pY = NULL;
    Ipp32u *pXData = NULL;
    Ipp32u *pYData = NULL;
    do {
        IppECResult ecResult;
        const Ipp32u eDataSize = strLen/2;
        unsigned int i;
        
        IppsGFPState* pGF;
        Ipp32u gfElemSize = 0;
        
        //if the string is all zeros then we take it as point at infinity
        for (i=0; i < strLen; i++) {
            if(0 != pStr[i]) {
                break;
            }
        }
        if (i >= strLen) {
            // pStr is point at infinity! Set it and we are done
            sts = ippsGFPECSetPointAtInfinity(pPoint, pEC);
            break;
        }
        
        //extract element data from Octstrings to local Ipp32u
        //Element size = strLen/2
        pXData = (Ipp32u*)SAFE_ALLOC(eDataSize);
        pYData = (Ipp32u*)SAFE_ALLOC(eDataSize);
        if (!(pXData && pYData)) {
            sts = ippStsNoMemErr; 
            break;
        }
        sts = octStr2BNU (pStr, eDataSize, pXData);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU (pStr + eDataSize, eDataSize, pYData);
        if (ippStsNoErr != sts) {break;}

        sts = ippsGFPECGet(pEC,
                           (const IppsGFPState**)&pGF, 0,
                           0, 0,
                           0, 0,
                           0, 0,
                           0);
        if (ippStsNoErr != sts) {break;}

        sts = ippsGFPElementGetSize(pGF, &gfElemSize);
        if (ippStsNoErr != sts) {break;}
        //create elements
        pX = createGFPElement(pXData, eDataSize/sizeof(Ipp32u), 
                              pGF,
                              NULL,0);
        pY = createGFPElement(pYData, eDataSize/sizeof(Ipp32u), 
                              pGF,
                              NULL,0);
        if (!(pX && pY)) {
            sts = ippStsNoMemErr; 
            break;
        }
        
        //Assemble the point
        sts = ippsGFPECSetPoint(pX, 
                                pY, 
                                pPoint,
                                pEC);
        if (ippStsNoErr != sts) {break;}
        // make sure the point is actually on the curve
        sts = ippsGFPECVerifyPoint(pPoint, &ecResult, pEC);
        if (ippStsNoErr != sts) {break;}
        if (ippECValid != ecResult) {sts = ippStsContextMatchErr; break;}
    } while (0);

    
    SAFE_FREE(pXData);
    SAFE_FREE(pYData);
    SAFE_FREE(pX);
    SAFE_FREE(pY);

    return(sts);
}



IppStatus GetOctString_GFPXElem(const IppsGFPXElement* pA, 
                                Ipp8u* pStr, int strLen,
                                const IppsGFPXState* pGF)
{
    IppStatus sts = ippStsNoErr; 
    
    do {
        Ipp8u databuf[3*EPID_NUMBER_SIZE];
        
        Ipp8u *pStr0 = pStr;
        Ipp8u *pStr1 = pStr + EPID_NUMBER_SIZE;
        Ipp8u *pStr2 = pStr + 2*EPID_NUMBER_SIZE;

        Ipp32u* pData0 = (Ipp32u*)databuf;
        Ipp32u* pData1 = (Ipp32u*)(databuf + EPID_NUMBER_SIZE);
        Ipp32u* pData2 = (Ipp32u*)(databuf + 2*EPID_NUMBER_SIZE);
        Ipp32u dataLen = sizeof(databuf)/sizeof(Ipp32u);
        
        if(strLen != sizeof(databuf)) {
            sts = ippStsSizeErr;
            break;
        }

        sts = ippsGFPXGetElement(pA, 
                                 pData0, 
                                 dataLen, 
                                 pGF);
        if (ippStsNoErr != sts) {break;}

        sts = ippsGetOctString_BNU(pData0, 
                                   EPID_NUMBER_SIZE/sizeof(Ipp32u), 
                                   pStr0, EPID_NUMBER_SIZE);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGetOctString_BNU(pData1, 
                                   EPID_NUMBER_SIZE/sizeof(Ipp32u), 
                                   pStr1, EPID_NUMBER_SIZE);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGetOctString_BNU(pData2, 
                                   EPID_NUMBER_SIZE/sizeof(Ipp32u), 
                                   pStr2, EPID_NUMBER_SIZE);
        if (ippStsNoErr != sts) {break;}
    } while(0);
    return (sts);
}

IppStatus GetOctString_GFPXECPoint(const IppsGFPXECPoint* pPoint, 
                                   Ipp8u* pStr, int strLen, 
                                   IppsGFPXECState* pEC)
{
    IppStatus sts = ippStsNoErr;
    IppECResult ecResult = ippECPointIsNotValid;
    
    IppsGFPXElement *pX = NULL;
    IppsGFPXElement *pY = NULL;

    do {
        IppsGFPXState* pGF;
        Ipp32u gfElemSize = 0;
        Ipp32u a[] = {0};

        sts = ippsGFPXECGet(pEC,
                            (const IppsGFPXState**)&pGF, 0,
                            0,0,
                            0,0,
                            0, 0,
                            0);
        if (ippStsNoErr != sts) {break;}

        sts = ippsGFPXElementGetSize(pGF, &gfElemSize);
        if (ippStsNoErr != sts) {break;} 

        pX = SAFE_ALLOC(gfElemSize);
        pY = SAFE_ALLOC(gfElemSize);
        if (!(pX && pY)) {
            sts = ippStsNoMemErr; 
            break;
        }

        sts = ippsGFPXElementInit(pX, a, sizeof(a)/sizeof(Ipp32u), pGF);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPXElementInit(pY, a, sizeof(a)/sizeof(Ipp32u), pGF);
        if (ippStsNoErr != sts) {break;}

        sts = ippsGFPXECVerifyPoint(pPoint, &ecResult, pEC);
        if (ippStsNoErr != sts) {break;}
        if (ippECValid == ecResult) {
            sts = ippsGFPXECGetPoint(pPoint, 
                                     pX, pY,
                                     pEC);
            if (ippStsNoErr != sts) {break;}
        } else if (ippECPointIsAtInfinite == ecResult) {
            /*
             * pPoint is point at infinity.  Since pX, pY are
             * already (0,0), which we treat as point at infinity
             * for serilization purposes so all we have to do here
             * is just fallthru, and report no error
             */ 
            sts = ippStsNoErr; 
        } else {
            // result is ippECPointOutOfGroup or ippECPointIsNotValid, error
            sts = ippStsErr;
            break;
        }
        
        // now convert the elements
        sts = GetOctString_GFPXElem(pX, pStr, strLen/2, pGF);
        if (ippStsNoErr != sts) {break;}
        sts =GetOctString_GFPXElem(pY, pStr + strLen/2, strLen/2, pGF);
        if (ippStsNoErr != sts) {break;}
    } while(0);

    SAFE_FREE(pX);
    SAFE_FREE(pY);

    return(sts);
}

IppStatus GetOctString_GFPXQElem(const IppsGFPXQElement* pA, 
                                 Ipp8u* pStr, int strLen,
                                 const IppsGFPXQState* pGFPQX)
{
    IppStatus sts = ippStsNoErr; 

    do {
        Ipp8u databuf[6*EPID_NUMBER_SIZE];
        Ipp32u* pData = (Ipp32u*) databuf;
        int index;

        if(strLen != sizeof(databuf)) {
            sts = ippStsSizeErr;
            break;
        }

        sts = ippsGFPXQGetElement(pA, 
                                  pData, 
                                  6*EPID_NUMBER_SIZE/sizeof(Ipp32u), 
                                  pGFPQX);
        if (ippStsNoErr != sts) {break;}

        for (index = 0; index < 6; index++) {
            sts = ippsGetOctString_BNU(pData, 
                                       EPID_NUMBER_SIZE/sizeof(Ipp32u), 
                                       pStr, EPID_NUMBER_SIZE);
            if (ippStsNoErr != sts) {break;}

            pData += EPID_NUMBER_SIZE/sizeof(Ipp32u);
            pStr  += EPID_NUMBER_SIZE;
        }
    } while(0);

    return(sts);
}

IppStatus SetOctString_GFPXQElem(const Ipp8u* pStr, const int strLen,
                                 IppsGFPXQElement* pA, 
                                 IppsGFPXQState* pGFPQX)
{  
    IppStatus sts = ippStsNoErr; 
    
    do {
        Ipp32u databuf[BITSIZE_WORD(6*EPID_NUMBER_SIZE_BITS)];
        
        Ipp32u* pData = databuf;
        int bnuSize = EPID_NUMBER_SIZE;

        int index;

        if(strLen != sizeof(databuf)) {
            sts = ippStsSizeErr;
            break;
        }

        for (index = 0; index < 6; index++) {
            sts = ippsSetOctString_BNU(pStr, 
                                       EPID_NUMBER_SIZE, 
                                       pData, &bnuSize);
            if (ippStsNoErr != sts) {break;}
            
            pData += EPID_NUMBER_SIZE/sizeof(Ipp32u);
            pStr  += EPID_NUMBER_SIZE;
        }

        sts = ippsGFPXQSetElement(databuf,  
                                  6*EPID_NUMBER_SIZE/sizeof(Ipp32u), 
                                  pA,
                                  pGFPQX);
        if (ippStsNoErr != sts) {break;}
    } while(0);

    return(sts);
}


