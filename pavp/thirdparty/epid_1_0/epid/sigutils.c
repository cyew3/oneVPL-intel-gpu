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
 * @brief This file implements common utlity functions for signing
 */

#include "sigutils.h"
#include "octstring.h"
#include "memutils.h"

//Max number of times to try to compute hash before giving up  
#define EPID_ECHASH_WATCHDOG (100) 

//#define SIGUTIL_DEBUG

#if defined(SIGUTIL_DEBUG)
#include <stdio.h>
#include "printutils.h"
#endif


#define MSIZE (4)

#define T3HBUF_SIZE (EPID_NUMBER_SIZE                                   \
                     + EPID_G1ELM_SIZE + EPID_G2ELM_SIZE + EPID_G3ELM_SIZE \
                     + EPID_G1ELM_SIZE + EPID_G1ELM_SIZE + EPID_G2ELM_SIZE \
                     + EPID_G3ELM_SIZE + EPID_G3ELM_SIZE                \
                     + 4*EPID_G1ELM_SIZE + EPID_G3ELM_SIZE + EPID_GTELM_SIZE)

EPID_RESULT computeSigmaC(const EPIDParameters   *params,
                          const EPIDGroup        *group,
                          const IppsGFPECPoint   *B, 
                          const IppsGFPECPoint   *K, 
                          const IppsGFPECPoint   *T1, 
                          const IppsGFPECPoint   *T2, 
                          const IppsGFPECPoint   *R1, 
                          const IppsGFPECPoint   *R2, 
                          const IppsGFPECPoint   *R3,
                          const IppsGFPXQElement *R4, 
                          const Ipp8u            *nd, 
                          const Ipp8u            *msg,
                          const Ipp32u            msgSize,
                          Ipp8u                  *sigmaC,
                          Ipp32u                  sigmaCLen
                          )
{
    EPID_RESULT retValue = EPID_FAILURE;
    Ipp8u *chbuf = NULL;

    do {
        IppStatus sts;
        Ipp8u *pBuf = NULL;
        Ipp32u eSize = 0;
        Ipp8u t3[EPID_NUMBER_SIZE] = {0};
        Ipp8u t3hbuf[T3HBUF_SIZE] = {0};
        Ipp32u chbufSize = EPID_NUMBER_SIZE + EPID_ND_SIZE + MSIZE + msgSize;

        //check params
        if ((NULL == params) 
            || (NULL == group)
            || (NULL == B)
            || (NULL == K)
            || (NULL == T1)
            || (NULL == T2)
            || (NULL == R1)
            || (NULL == R2)
            || (NULL == R3)
            || (NULL == R4)
            || (NULL == nd)
            || (NULL == msg)
            || (NULL == sigmaC)) {
            retValue = EPID_NULL_PTR;
            break;
        }
        if (EPID_NUMBER_SIZE != sigmaCLen) {
            retValue = EPID_BAD_ARGS;
            break;
        }   
        chbuf = SAFE_ALLOC(chbufSize);
        if (NULL == chbuf) {
            retValue = EPID_OUTOFMEMORY;
            break;
        }
        
        retValue = EPID_FAILURE;

        //t3 = Hash(p g1 g2 g3 h1 h2 w B K T1 T2 R1 R2 R3 R4)
        //setup buf to hash 
        pBuf = t3hbuf;
        eSize = EPID_NUMBER_SIZE;
        sts = BNU2OctStr (params->p12, pBuf, eSize);
        if (ippStsNoErr != sts) {break;}
        pBuf += eSize;
        
        eSize = EPID_G1ELM_SIZE;
        sts = GetOctString_GFPECPoint(params->g1, pBuf, eSize, params->G1);
        if (ippStsNoErr != sts) {break;}
        pBuf += eSize;

        eSize = EPID_G2ELM_SIZE;
        sts = GetOctString_GFPXECPoint(params->g2, pBuf, eSize, params->G2);
        if (ippStsNoErr != sts) {break;}
        pBuf += eSize;

        eSize = EPID_G3ELM_SIZE;
        sts = GetOctString_GFPECPoint(params->g3, pBuf, eSize, params->G3);
        if (ippStsNoErr != sts) {break;}
        pBuf += eSize;

        eSize = EPID_G1ELM_SIZE;
        sts = GetOctString_GFPECPoint(group->h1, pBuf, eSize, params->G1);
        if (ippStsNoErr != sts) {break;}
        pBuf += eSize;

        eSize = EPID_G1ELM_SIZE;
        sts = GetOctString_GFPECPoint(group->h2, pBuf, eSize, params->G1);
        if (ippStsNoErr != sts) {break;}
        pBuf += eSize;

        eSize = EPID_G2ELM_SIZE;
        sts = GetOctString_GFPXECPoint(group->w, pBuf, eSize, params->G2);
        if (ippStsNoErr != sts) {break;}
        pBuf += eSize;

        eSize = EPID_G3ELM_SIZE;
        sts = GetOctString_GFPECPoint(B, pBuf, eSize, params->G3);
        if (ippStsNoErr != sts) {break;}
        pBuf += eSize;
        
        eSize = EPID_G3ELM_SIZE;
        sts = GetOctString_GFPECPoint(K, pBuf, eSize, params->G3);
        if (ippStsNoErr != sts) {break;}
        pBuf += eSize;

        eSize = EPID_G1ELM_SIZE;
        sts = GetOctString_GFPECPoint(T1, pBuf, eSize, params->G1);
        if (ippStsNoErr != sts) {break;}
        pBuf += eSize;
        
        eSize = EPID_G1ELM_SIZE;
        sts = GetOctString_GFPECPoint(T2, pBuf, eSize, params->G1);
        if (ippStsNoErr != sts) {break;}
        pBuf += eSize;
        
        eSize = EPID_G1ELM_SIZE;
        sts = GetOctString_GFPECPoint(R1, pBuf, eSize, params->G1);
        if (ippStsNoErr != sts) {break;}
        pBuf += eSize;

        eSize = EPID_G1ELM_SIZE;
        sts = GetOctString_GFPECPoint(R2, pBuf, eSize, params->G1);
        if (ippStsNoErr != sts) {break;}
        pBuf += eSize;
        
        eSize = EPID_G3ELM_SIZE;
        sts = GetOctString_GFPECPoint(R3, pBuf, eSize, params->G3);
        if (ippStsNoErr != sts) {break;}
        pBuf += eSize;

        eSize = EPID_GTELM_SIZE;
        sts = GetOctString_GFPXQElem(R4, pBuf, eSize, params->GT);
        if (ippStsNoErr != sts) {break;}
        pBuf += eSize;
        
#if defined(SIGUTIL_DEBUG)  
        printf("t3hbuff ");PrintOctString(t3hbuf, sizeof(t3hbuf), 0);
#endif
        sts = ippsSHA256MessageDigest(t3hbuf, sizeof(t3hbuf), t3);
        if (ippStsNoErr != sts) {break;}
#if defined(SIGUTIL_DEBUG)  
        printf("digest = Hash(p g1 g2 g3 h1 h2 w B K T1 T2 R1 R2 R3 R4) ");
        PrintOctString(t3, sizeof(t3), 0);
#endif
        

        //c = hash(t3 nd mSize m) where mSize is 4 byte int = size of m in bytes
        pBuf = chbuf;
        
        eSize = sizeof(t3);
      
        memcpy (pBuf, t3, eSize);
        pBuf += eSize;

        eSize = EPID_ND_SIZE;
        memcpy (pBuf, nd, eSize);
        pBuf += eSize;
    
        eSize = MSIZE;
        sts = BNU2OctStr ((Ipp32u*)&msgSize, pBuf, eSize);
        if (ippStsNoErr != sts) {break;}
        pBuf += eSize;
        
        eSize = msgSize;
        memcpy (pBuf, msg, eSize);
        pBuf += eSize;
        
#if defined(SIGUTIL_DEBUG)  
        printf("chbuf =(t3 nd mSize m)  ");
        PrintOctString(chbuf, chbufSize, 0);
#endif
        sts = ippsSHA256MessageDigest(chbuf, chbufSize, sigmaC);
        if (ippStsNoErr != sts) {break;}

#if defined(SIGUTIL_DEBUG)  
        printf("c = hash(t3 nd mSize m)  ");
        PrintOctString(sigmaC, sigmaCLen, 0);
#endif
        
        retValue = EPID_SUCCESS;
    } while(0);

    SAFE_FREE(chbuf);
    
    return (retValue);
}

// let b = first bit of H
// t = next 336bits of H (336 = length(q) + slen)
IppStatus splitHashBits(Ipp8u* str, Ipp32u strlen, 
                   Ipp32u* pFirstBit, Ipp32u* pBNU, Ipp32u BNUlen)
{
    IppStatus sts = ippStsNoErr;
    do {
        Ipp8u* pDest = (Ipp8u *)pBNU;
        Ipp32u bnuBytes = BNUlen * sizeof(Ipp32u);
        Ipp32u carry = 0;

        *pFirstBit = 0;

        if(!(bnuBytes <= strlen)) {
            sts = ippStsSizeErr;
            break;
        }
        if(11 != BNUlen) {
             sts = ippStsSizeErr;
            break;
        }
        memset (pBNU, 0, bnuBytes);
        while (bnuBytes--) {
            *pFirstBit = str[bnuBytes] & 0x80;
            pDest[bnuBytes] = (((str[bnuBytes] << 1) & 0xFF) | carry) & 0xFF;
            carry = *pFirstBit;
        }
        // now convert to BNU from octStr
        sts = ippsSetOctString_BNU(pDest, BNUlen * sizeof(Ipp32u),
                                   pBNU, (int*)&BNUlen);
        if (ippStsNoErr != sts) {break;}
    } while (0);

    return(sts);
}

EPID_RESULT epidGFPECHash(const Ipp8u* msg, const Ipp32u msgSize, 
                          IppsGFPECPoint* pR, IppsGFPECState* pEC)
{
    EPID_RESULT retValue = EPID_FAILURE;
    
    Ipp8u* hashBuf = NULL;
    Ipp32u hashBufSize = 2*sizeof(Ipp32u) + 2*msgSize;
    
    IppsGFPElement* a = NULL;
    IppsGFPElement* b = NULL;

    IppsGFPElement* Rx = NULL;
    IppsGFPElement* t1 = NULL;
    IppsGFPElement* t2 = NULL;

    do {
        IppStatus sts;
        Ipp32u i = 0;
        Ipp32u highBit = 0;
        
        IppsGFPState* pGF = NULL;
        Ipp32u h = 0;  //cofactor

        Ipp32u elemLen;
        
        int sqrtLoopCount = EPID_ECHASH_WATCHDOG;
        Ipp8u messageDigest[2*EPID_HASH_SIZE] = {0};
        Ipp32u t[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS + EPID_SLEN)];
        Ipp32u tLen = BITSIZE_WORD(EPID_NUMBER_SIZE_BITS + EPID_SLEN);
        Ipp8u* pDigest1 = messageDigest;
        Ipp8u* pDigest2 = messageDigest + EPID_HASH_SIZE;
        Ipp8u* pMsg1 = NULL;
        Ipp8u* pMsg2 = NULL;
        Ipp8u* pI1 = NULL;
        Ipp8u* pI2 = NULL;
        
                
        hashBuf = SAFE_ALLOC(hashBufSize);
        if (!hashBuf) {
            retValue = EPID_OUTOFMEMORY;
            break;
        }
   
        pI1 = hashBuf;
        pMsg1 = hashBuf + sizeof(Ipp32u);
        pI2 = hashBuf + sizeof(Ipp32u) + msgSize;
        pMsg2 = hashBuf + sizeof(Ipp32u) + msgSize + sizeof(Ipp32u);
        memcpy(pMsg1, msg, msgSize);
        memcpy(pMsg2, msg, msgSize);

        do {
            retValue = EPID_FAILURE;
            //check parameters
            if ((NULL == msg) 
                || (NULL == pR) 
                || (NULL == pEC)) {
                retValue = EPID_NULL_PTR;
                break;
            }
            
            sts = ippsGFPECGet(pEC, 
                               (const IppsGFPState**)&pGF, &elemLen,
                               0, 0,
                               0, 0,
                               0, 0,
                               &h);
            if (ippStsNoErr != sts) {break;}

            if(BITSIZE_WORD(EPID_NUMBER_SIZE_BITS) != elemLen) {
                retValue = EPID_BAD_ARGS;
                break;
            }

            a = createGFPElement(0, 0, pGF, NULL, 0);
            b = createGFPElement(0, 0, pGF, NULL, 0);
            Rx = createGFPElement(0, 0, pGF, NULL, 0);
            t1 = createGFPElement(0, 0, pGF, NULL, 0);
            t2 = createGFPElement(0, 0, pGF, NULL, 0);
            if ( !(a && b && Rx && t1 && t2)) {
                retValue = EPID_OUTOFMEMORY;
                break;
            }
            
            sts = ippsGFPECGet(pEC, 
                               (const IppsGFPState**)&pGF, &elemLen,
                               a, b,
                               0, 0,
                               0, 0,
                               &h);
            if (ippStsNoErr != sts) {break;}

            //compute H = hash (i || m) || Hash (i+1 || m) where (i =ipp32u)
            U32_TO_HSTRING(pI1, i);
            U32_TO_HSTRING(pI2, i + 1);
            sts = ippsSHA256MessageDigest(pI1, hashBufSize/2, pDigest1);
            if (ippStsNoErr != sts) {break;}
            sts = ippsSHA256MessageDigest(pI2, hashBufSize/2, pDigest2);
            if (ippStsNoErr != sts) {break;}
            // let b = first bit of H
            // t = next 336bits of H (336 = length(q) + slen)    
            sts = splitHashBits(&messageDigest[0], sizeof(messageDigest), 
                          &highBit, t, tLen);
            if (ippStsNoErr != sts) {break;}
            
            // compute Rx = t mod q (aka prime field based on q)
            sts = ippsGFPSetElement(t, tLen, Rx, pGF); 
            if (ippStsNoErr != sts) {break;}

            // t1 = (Rx^3 + a*Rx + b) mod q
            sts = ippsGFPMul(Rx, Rx, t1, pGF);
            if (ippStsNoErr != sts) {break;}
            sts = ippsGFPMul(t1, Rx, t1, pGF);
            if (ippStsNoErr != sts) {break;}
            sts = ippsGFPMul(a, Rx, t2, pGF);
            if (ippStsNoErr != sts) {break;}
            sts = ippsGFPAdd(t1, t2, t1, pGF);
            if (ippStsNoErr != sts) {break;}
            sts = ippsGFPAdd(t1, b, t1, pGF);
            if (ippStsNoErr != sts) {break;}
                        
            // t2 = Fq.sqrt(t1)
            sts = ippsGFPSqrt(t1, t2, pGF);
            if (ippStsSqrtNegArg == sts) {
                // if sqrt fail set i = i+ 2 and repeat from top
                retValue = EPID_UNLUCKY;
                i +=2;
                continue;
            } else if (ippStsNoErr != sts) {
                retValue = EPID_FAILURE;
                break;
            } else {
                retValue = EPID_SUCCESS;
                break;
            }             
        } while (--sqrtLoopCount);

        if (EPID_IS_FAILURE(retValue)) {         
            break;
        } else {
            //reset to fail to catch other errors
            retValue = EPID_FAILURE;
        }

        // y[0] = min (t2, q-t2), y[1] = max(t2, q-t2)
        if (0 == highBit) {
            //q-t2 = Fq.neg(t2)
            sts = ippsGFPNeg(t2, t2, pGF);
            if (ippStsNoErr != sts) {break;}
        }
        // Ry = y[b]
        sts = ippsGFPECSetPoint(Rx, t2, pR, pEC);
        if (ippStsNoErr != sts) {break;}
        // R = E(Fq).exp(R,h)
        sts = ippsGFPECMulPointScalar(pR, &h, 1, pR, pEC);
        if (ippStsNoErr != sts) {break;}

        retValue = EPID_SUCCESS;
    } while(0);

    SAFE_FREE(hashBuf);
    SAFE_FREE(a);
    SAFE_FREE(b);
    SAFE_FREE(Rx);
    SAFE_FREE(t1);
    SAFE_FREE(t2);

    return (retValue);
}


