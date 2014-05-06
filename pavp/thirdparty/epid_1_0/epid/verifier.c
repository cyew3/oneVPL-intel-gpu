/*
#***************************************************************************
# INTEL CONFIDENTIAL
# Copyright (C) 2009-2010 Intel Corporation All Rights Reserved. 
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
 * @brief This file implements the Verifier interface 
 */

#include "epid_verifier.h"

#include "context.h"
#include "octstring.h"
#include "memutils.h"
#include "sigutils.h"
#include "multiexp.h"

#include "ippcp.h"

struct EPIDVerifier
{
    EPIDParameters    params;
    EPIDGroup         group;
    IppsGFPXQElement *e12;
    IppsGFPXQElement *e22;
    IppsGFPXQElement *e2w;
    void             *revocationList; ///< Pointer to revocation information
};


EPID_RESULT 
epidVerifier_create(EPIDParameterCertificateBlob    *epidParameters,
                    EPIDGroupCertificateBlob        *groupCertificate,
                    void                            *revocationList,
                    int                              createPreComputationBlob,
                    EPIDVerifierPreComputationBlob  *verifierPreComputationBlob,
                    EPIDVerifier                   **pCtx)
{
    EPID_RESULT retValue = EPID_FAILURE;
    IppStatus  sts;
    
    EPIDVerifier * ctx = NULL;
    
    do {
        if ((NULL == epidParameters) ||
            (NULL == groupCertificate) ||
            (NULL == pCtx)) {
            retValue = EPID_NULL_PTR;
            break;
        }

        // make sure params and grp cert have right blobid
        if (epidParametersCertificate != HSTRING_TO_U16(epidParameters->blobid) 
            || epidGroupCertificate != HSTRING_TO_U16(groupCertificate->blobid)
            ) {
            retValue = EPID_BAD_ARGS;
            break;
        }
        
        // make sure input blobs have right sver value
        if (EPID_SVER    != HSTRING_TO_U16(epidParameters->sver)
            || EPID_SVER != HSTRING_TO_U16(groupCertificate->sver)
            || ((NULL != revocationList)
                && (EPID_SVER != GET_RL_SVER(revocationList)))
            ) {
            retValue = EPID_BAD_ARGS;
            break;
        }

        // If not creating precomp blob must have a valid one provided
        if (!createPreComputationBlob && NULL == verifierPreComputationBlob) {
            retValue = EPID_BAD_ARGS;
            break;
        }

        if (NULL != verifierPreComputationBlob
            && !createPreComputationBlob
            && (
                epidVerifierPreComputationBlob != HSTRING_TO_U16(verifierPreComputationBlob->blobid)
                || EPID_SVER != HSTRING_TO_U16(verifierPreComputationBlob->sver))
            ) {
            retValue = EPID_BAD_ARGS;
            break;
        }

        if (NULL != revocationList) {
            // Test to see if this is a valid revocation list
            if (epidKeyBasedRevocationList != GET_RL_BLOBID(revocationList)) {
                break;
            }

            // Make sure the gid in both group cert and revocation list match
            if (HSTRING_TO_U32(groupCertificate->gid) != GET_RL_GID(revocationList)) {
                break;
            }

            // Make sure the version of the revocation list matches what we support
            // @note: this logic assumes backwards compatability to older versions
            if (EPID_SVER < GET_RL_SVER(revocationList)) {
                break;
            }
        }
        
        ctx = (EPIDVerifier *) SAFE_ALLOC(sizeof(EPIDVerifier));
        if (NULL == ctx) {
            retValue = EPID_OUTOFMEMORY;
            break;
        }
        memset(ctx, 0, sizeof(EPIDVerifier));

        if (NULL == revocationList) {
            ctx->revocationList = NULL;
        } else {
            ctx->revocationList = (void *) SAFE_ALLOC(GET_RL_LENGTH(revocationList));
            if (NULL == ctx->revocationList) {
                retValue = EPID_OUTOFMEMORY;
                break;
            }
            memcpy(ctx->revocationList, revocationList, 
                   GET_RL_LENGTH(revocationList));
        }

        retValue = epidParametersContext_init(epidParameters, &ctx->params);
        if (EPID_IS_FAILURE(retValue)) {         
            break;
        } 

        retValue = epidGroupContext_init(groupCertificate, &ctx->params, 
                                         &ctx->group);
        if (EPID_IS_FAILURE(retValue)) {         
            break;
        } 

        // Now create the pre-computation values

        // Assume error.  Set back to "success" if we make it to the end
        retValue = EPID_FAILURE;

        ctx->e12 = createQuadFieldExtElement(NULL, 0, ctx->params.GT, NULL, 0);
        if (NULL == ctx->e12) {break;}
        ctx->e22 = createQuadFieldExtElement(NULL, 0, ctx->params.GT, NULL, 0);
        if (NULL == ctx->e22) {break;}
        ctx->e2w = createQuadFieldExtElement(NULL, 0, ctx->params.GT, NULL, 0);
        if (NULL == ctx->e2w) {break;}

        if (createPreComputationBlob) {
            sts = ippsTatePairingDE3Apply(ctx->group.h1, ctx->params.g2, 
                ctx->e12, ctx->params.tp);
            if (ippStsNoErr != sts) {break;}
            sts = ippsTatePairingDE3Apply(ctx->group.h2, ctx->params.g2, 
                ctx->e22, ctx->params.tp);
            if (ippStsNoErr != sts) {break;}
            sts = ippsTatePairingDE3Apply(ctx->group.h2, ctx->group.w,   
                ctx->e2w, ctx->params.tp);
            if (ippStsNoErr != sts) {break;} 

            if (verifierPreComputationBlob) {
                // copy the precomputation blob (in serialized form) to output
                U16_TO_HSTRING(verifierPreComputationBlob->sver, EPID_SVER);  
                U16_TO_HSTRING(verifierPreComputationBlob->blobid, 
                               epidVerifierPreComputationBlob);
                
                sts = GetOctString_GFPXQElem(ctx->e12, 
                                             (Ipp8u*)&verifierPreComputationBlob->e12,
                                             EPID_GTELM_SIZE,
                                             ctx->params.GT);
                if (ippStsNoErr != sts) {break;}
                sts = GetOctString_GFPXQElem(ctx->e22, 
                                             (Ipp8u*)&verifierPreComputationBlob->e22,
                                             EPID_GTELM_SIZE,
                                             ctx->params.GT);
                if (ippStsNoErr != sts) {break;}
                sts = GetOctString_GFPXQElem(ctx->e2w, 
                                             (Ipp8u*)&verifierPreComputationBlob->e2w,
                                             EPID_GTELM_SIZE,
                                             ctx->params.GT);
                if (ippStsNoErr != sts) {break;}
            }
        } else {
            // use incoming (serialized) precomputation blob instead
            // of computing a new one
            sts = SetOctString_GFPXQElem((Ipp8u*)&verifierPreComputationBlob->e12,
                                         EPID_GTELM_SIZE,
                                         ctx->e12,
                                         ctx->params.GT);
            if (ippStsNoErr != sts) {break;}
            sts = SetOctString_GFPXQElem((Ipp8u*)&verifierPreComputationBlob->e22,
                                         EPID_GTELM_SIZE,
                                         ctx->e22,
                                         ctx->params.GT);
            if (ippStsNoErr != sts) {break;}
            sts = SetOctString_GFPXQElem((Ipp8u*)&verifierPreComputationBlob->e2w,
                                         EPID_GTELM_SIZE,
                                         ctx->e2w,
                                         ctx->params.GT);
            if (ippStsNoErr != sts) {break;}
        }

        retValue = EPID_SUCCESS;
    } while (0);

    if (EPID_IS_FAILURE(retValue)) {
        if (ctx) {
            // no need to check return value here due to nullcheck
            epidVerifier_delete(&ctx);
        }
    }

    if (EPID_IS_SUCCESS(retValue)) {
        *pCtx = ctx;
    }

    return (retValue);
}

EPID_RESULT epidVerifier_delete(EPIDVerifier ** ctx)
{
    EPID_RESULT retValue = EPID_FAILURE;
    
    do {
      if (NULL == ctx) {
        retValue = EPID_NULL_PTR;
        break;
      }
      
      if (*ctx) {
        epidParametersContext_destroy(&(*ctx)->params);
        epidGroupContext_destroy(&(*ctx)->group);
        
        SAFE_FREE((*ctx)->revocationList);
        SAFE_FREE((*ctx)->e12);
        SAFE_FREE((*ctx)->e22);
        SAFE_FREE((*ctx)->e2w);
        SAFE_FREE(*ctx);
        *ctx = NULL;
      }
      retValue = EPID_SUCCESS;
    } while (0);

    return (retValue);
}

EPID_RESULT epidVerifier_verifyMemberSignature(EPIDVerifier        *ctx,
                                               unsigned char       *message,
                                               unsigned int         messageLength, 
                                               unsigned char       *baseName,
                                               unsigned int         baseNameLength,
                                               EPIDSignatureBlob   *epidSignature) 
{
    EPID_RESULT  retValue = EPID_FAILURE;
    IppStatus   sts;
    Ipp32u      wordLen;

    unsigned int index;

    IppECResult ecResult;

    Ipp8u       cComputed[32];

    // Following items all need to be "freed" at the end of this function!!!
    
    IppsGFPECPoint*   B  = NULL;
    IppsGFPECPoint*   G3hashBsn  = NULL;
    
    IppsGFPECPoint*   K  = NULL;
    IppsGFPECPoint*   T1 = NULL;
    IppsGFPECPoint*   T2 = NULL;

    IppsGFPECPoint*   t1 = NULL;
    IppsGFPECPoint*   t2 = NULL;
    IppsGFPECPoint*   t5 = NULL;

    IppsGFPECPoint*   R1 = NULL;
    IppsGFPECPoint*   R2 = NULL;
    IppsGFPECPoint*   R3 = NULL;

    IppsGFPElement*   Bx  = NULL;
    IppsGFPElement*   By  = NULL;
    IppsGFPElement*   Kx  = NULL;
    IppsGFPElement*   Ky  = NULL;
    IppsGFPElement*   T1x = NULL;
    IppsGFPElement*   T1y = NULL;
    IppsGFPElement*   T2x = NULL;
    IppsGFPElement*   T2y = NULL;

    IppsGFPElement*   c1 = NULL;
    IppsGFPElement*   c3 = NULL;
    
    IppsGFPElement*   sx     = NULL;
    IppsGFPElement*   sy     = NULL;
    IppsGFPElement*   sa     = NULL;
    IppsGFPElement*   sb     = NULL;
    IppsGFPElement*   salpha = NULL;
    IppsGFPElement*   sbeta  = NULL;

    IppsGFPElement*   nc1     = NULL;
    IppsGFPElement*   nc3     = NULL;
    IppsGFPElement*   nsx     = NULL;
    IppsGFPElement*   syalpha = NULL;

    IppsGFPXQElement* t3 = NULL;
    IppsGFPXQElement* R4 = NULL;
    // End of items that need to be "freed"


    Ipp32u            BxData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u            ByData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u            KxData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u            KyData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u            T1xData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u            T1yData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u            T2xData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u            T2yData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];

    Ipp32u            cData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];

    Ipp32u            sxData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u            syData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u            saData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u            sbData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u            salphaData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u            sbetaData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];

    Ipp32u            sfData[BITSIZE_WORD(EPID_SF_SIZE_BITS)];

    Ipp32u            nc1Data[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u            nc3Data[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u            nsxData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u            syalphaData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];

    Ipp32u            scratchData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];

    Ipp32u            fData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];

    do {
        //It's OK if baseName is NULL, no need to do error check here

        if ((NULL == ctx)           ||
            (NULL == message)       ||
            (0    == messageLength) ||
            (NULL == epidSignature)) {
            retValue = EPID_NULL_PTR;
            break;
        }

        memset(nc1Data,     0, EPID_NUMBER_SIZE);
        memset(nc3Data,     0, EPID_NUMBER_SIZE);
        memset(nsxData,     0, EPID_NUMBER_SIZE);
        memset(syalphaData, 0, EPID_NUMBER_SIZE);

        // Format the raw data from sig into form compatible with IPP functions
        sts = octStr2BNU(epidSignature->B.x,    EPID_NUMBER_SIZE, BxData);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(epidSignature->B.y,    EPID_NUMBER_SIZE, ByData);
        if (ippStsNoErr != sts) {break;}

        sts = octStr2BNU(epidSignature->K.x,    EPID_NUMBER_SIZE, KxData);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(epidSignature->K.y,    EPID_NUMBER_SIZE, KyData);
        if (ippStsNoErr != sts) {break;}

        sts = octStr2BNU(epidSignature->T1.x,   EPID_NUMBER_SIZE, T1xData);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(epidSignature->T1.y,   EPID_NUMBER_SIZE, T1yData);
        if (ippStsNoErr != sts) {break;}

        sts = octStr2BNU(epidSignature->T2.x,   EPID_NUMBER_SIZE, T2xData);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(epidSignature->T2.y,   EPID_NUMBER_SIZE, T2yData);
        if (ippStsNoErr != sts) {break;}

        sts = octStr2BNU(epidSignature->c,      EPID_NUMBER_SIZE, cData);
        if (ippStsNoErr != sts) {break;}

        sts = octStr2BNU(epidSignature->sx,     EPID_NUMBER_SIZE, sxData);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(epidSignature->sy,     EPID_NUMBER_SIZE, syData);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(epidSignature->sa,     EPID_NUMBER_SIZE, saData);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(epidSignature->sb,     EPID_NUMBER_SIZE, sbData);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(epidSignature->salpha, EPID_NUMBER_SIZE, salphaData);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(epidSignature->sbeta,  EPID_NUMBER_SIZE, sbetaData);
        if (ippStsNoErr != sts) {break;}

        sts = octStr2BNU(epidSignature->sf,     EPID_SF_SIZE, sfData);
        if (ippStsNoErr != sts) {break;}

        // This is the only error code that will be triggered by following logic
        retValue = EPID_OUTOFMEMORY;

        wordLen = BITSIZE_WORD(EPID_NUMBER_SIZE_BITS);

        Bx = createGFPElement(BxData, wordLen, ctx->params.Fq3, NULL, 0);
        if (NULL == Bx) {break;}
        By = createGFPElement(ByData, wordLen, ctx->params.Fq3, NULL, 0);
        if (NULL == By) {break;}

        Kx = createGFPElement(KxData, wordLen, ctx->params.Fq3, NULL, 0);
        if (NULL == Kx) {break;}
        Ky = createGFPElement(KyData, wordLen, ctx->params.Fq3, NULL, 0);
        if (NULL == Ky) {break;}

        T1x = createGFPElement(T1xData, wordLen, ctx->params.Fq12, NULL, 0);
        if (NULL == T1x) {break;}
        T1y = createGFPElement(T1yData, wordLen, ctx->params.Fq12, NULL, 0);
        if (NULL == T1y) {break;}

        T2x = createGFPElement(T2xData, wordLen, ctx->params.Fq12, NULL, 0);
        if (NULL == T2x) {break;}
        T2y = createGFPElement(T2yData, wordLen, ctx->params.Fq12, NULL, 0);
        if (NULL == T2y) {break;}

        B = createGFPECPoint(ctx->params.G3, NULL, 0);
        if (NULL == B) {break;}
        G3hashBsn = createGFPECPoint(ctx->params.G3, NULL, 0);
        if (NULL == G3hashBsn) {break;}
        K = createGFPECPoint(ctx->params.G3, NULL, 0);
        if (NULL == K) {break;}

        T1 = createGFPECPoint(ctx->params.G1, NULL, 0);
        if (NULL == T1) {break;}
        T2 = createGFPECPoint(ctx->params.G1, NULL, 0);
        if (NULL == T2) {break;}

        c1 = createGFPElement(cData, wordLen, ctx->params.Fp12, NULL, 0);
        if (NULL == c1) {break;}
        c3 = createGFPElement(cData, wordLen, ctx->params.Fp3, NULL, 0);
        if (NULL == c3) {break;}
        sx = createGFPElement(sxData, wordLen, ctx->params.Fp12, NULL, 0);
        if (NULL == sx) {break;}
        sy = createGFPElement(syData, wordLen, ctx->params.Fp12, NULL, 0);
        if (NULL == sy) {break;}
        sa = createGFPElement(saData, wordLen, ctx->params.Fp12, NULL, 0);
        if (NULL == sa) {break;}
        sb = createGFPElement(sbData, wordLen, ctx->params.Fp12, NULL, 0);
        if (NULL == sb) {break;}
        salpha = createGFPElement(salphaData, wordLen, ctx->params.Fp12, NULL, 0);
        if (NULL == salpha) {break;}
        sbeta = createGFPElement(sbetaData,   wordLen, ctx->params.Fp12, NULL, 0);
        if (NULL == sbeta) {break;}

        nc1 = createGFPElement(NULL, 0, ctx->params.Fp12, NULL, 0);
        if (NULL == nc1) {break;}
        nc3 = createGFPElement(NULL, 0, ctx->params.Fp3, NULL, 0);
        if (NULL == nc3) {break;}
        nsx = createGFPElement(NULL, 0, ctx->params.Fp12, NULL, 0);
        if (NULL == nsx) {break;}
        syalpha = createGFPElement(NULL, 0, ctx->params.Fp12, NULL, 0);
        if (NULL == syalpha) {break;}

        t1 = createGFPECPoint(ctx->params.G1, NULL, 0);
        if (NULL == t1) {break;}
        t2 = createGFPECPoint(ctx->params.G1, NULL, 0);
        if (NULL == t2) {break;}
        t5 = createGFPECPoint(ctx->params.G3, NULL, 0);
        if (NULL == t5) {break;}

        R1 = createGFPECPoint(ctx->params.G1, NULL, 0);
        if (NULL == R1) {break;}
        R2 = createGFPECPoint(ctx->params.G1, NULL, 0);
        if (NULL == R2) {break;}
        R3 = createGFPECPoint(ctx->params.G3, NULL, 0);
        if (NULL == R3) {break;}

        t3 = createQuadFieldExtElement(NULL, 0, ctx->params.GT, NULL, 0);
        if (NULL == t3) {break;}
        R4 = createQuadFieldExtElement(NULL, 0, ctx->params.GT, NULL, 0);
        if (NULL == R4) {break;}

        // default back to general error
        retValue = EPID_FAILURE;

        sts = ippsGFPECSetPoint(Bx, By, B, ctx->params.G3);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPECSetPoint(Kx, Ky, K, ctx->params.G3);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPECSetPoint(T1x, T1y, T1, ctx->params.G1);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPECSetPoint(T2x, T2y, T2, ctx->params.G1);
        if (ippStsNoErr != sts) {break;}

        // check FALSE == G3.isIdentity(B)
        sts = ippsGFPECVerifyPoint(B, &ecResult, ctx->params.G3);
        if (ippStsNoErr != sts) {break;}
        if (ippECPointIsAtInfinite == ecResult) {break;}

        if (baseName) {
            IppsElementCmpResult cmpResult;
            // if (baseName) check B == G3.hash(baseName)
            retValue = epidGFPECHash(baseName, baseNameLength,
                                     G3hashBsn, ctx->params.G3);
            if (EPID_IS_FAILURE(retValue)) {
                break;
            } else {
                // set to failure again to catch future errors 
                retValue = EPID_FAILURE;
            }
            sts = ippsGFPECCmpPoint(B, G3hashBsn, &cmpResult, ctx->params.G3);
            if (ippStsNoErr != sts) {break;} 
            if (IppsElementEQ != cmpResult) {break;}
        } else {
            // if (NULL == baseName) check TRUE == G3.inGroup(B)
            sts = ippsGFPECVerifyPoint(B, &ecResult, ctx->params.G3);
            if (ippStsNoErr != sts) {break;}
            if (ippECValid != ecResult) {break;}
        }

        // check TRUE == G3.inGroup(K)
        sts = ippsGFPECVerifyPoint(K, &ecResult, ctx->params.G3);
        if (ippStsNoErr != sts) {break;}
        if (ippECValid != ecResult) {break;}

        // check TRUE == G1.inGroup(T1)
        sts = ippsGFPECVerifyPoint(T1, &ecResult, ctx->params.G1);
        if (ippStsNoErr != sts) {break;}
        if (ippECValid != ecResult) {break;}

        // check TRUE == G1.inGroup(T2)
        sts = ippsGFPECVerifyPoint(T2, &ecResult, ctx->params.G1);
        if (ippStsNoErr != sts) {break;}
        if (ippECValid != ecResult) {break;}

        // check sx, sy, sa, sb, salpha, sbeta in [0,p-1]
        sts = ippsGFPGetElement(sx, scratchData, wordLen, ctx->params.Fp12);
        if (ippStsNoErr != sts) {break;}
        if (0 != memcmp(sxData, scratchData, sizeof(scratchData))) {break;}
        sts = ippsGFPGetElement(sy, scratchData, wordLen, ctx->params.Fp12);
        if (ippStsNoErr != sts) {break;}
        if (0 != memcmp(syData, scratchData, sizeof(scratchData))) {break;}
        sts = ippsGFPGetElement(sa, scratchData, wordLen, ctx->params.Fp12);
        if (ippStsNoErr != sts) {break;}
        if (0 != memcmp(saData, scratchData, sizeof(scratchData))) {break;}
        sts = ippsGFPGetElement(sb, scratchData, wordLen, ctx->params.Fp12);
        if (ippStsNoErr != sts) {break;}
        if (0 != memcmp(sbData, scratchData, sizeof(scratchData))) {break;}
        sts = ippsGFPGetElement(salpha, scratchData, wordLen, ctx->params.Fp12);
        if (ippStsNoErr != sts) {break;}
        if (0 != memcmp(salphaData, scratchData, sizeof(scratchData))) {break;}
        sts = ippsGFPGetElement(sbeta, scratchData, wordLen, ctx->params.Fp12);
        if (ippStsNoErr != sts) {break;}
        if (0 != memcmp(sbetaData, scratchData, sizeof(scratchData))) {break;}

        // check sf < 2**593
        if (EPID_SF_MAX_SIZE_BITS 
            <= getBitSize(sfData, 
                          BITSIZE_WORD(EPID_SF_SIZE_BITS))) {break;}

        // nc = (-c) mod p
        sts = ippsGFPNeg(c1, nc1, ctx->params.Fp12);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPGetElement(nc1, nc1Data, wordLen, ctx->params.Fp12);
        if (ippStsNoErr != sts) {break;}

        // nc' = (-c) mod p'
        sts = ippsGFPNeg(c3, nc3, ctx->params.Fp3);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPGetElement(nc3, nc3Data, wordLen, ctx->params.Fp3);
        if (ippStsNoErr != sts) {break;}

        // nsx = (-sx) mod p
        sts = ippsGFPNeg(sx, nsx, ctx->params.Fp12);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPGetElement(nsx, nsxData, wordLen, ctx->params.Fp12);
        if (ippStsNoErr != sts) {break;}

        // syalpha = (sy + salpha) mod p
        sts = ippsGFPAdd(sy, salpha, syalpha, ctx->params.Fp12);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPGetElement(syalpha, syalphaData, wordLen, ctx->params.Fp12);
        if (ippStsNoErr != sts) {break;}

        // R1 = G1.multiexp(h1, sa, h2, sb, T2, nc)
        retValue = ecpfMultiExp(ctx->params.G1,
                                ctx->group.h1, saData,  wordLen,
                                ctx->group.h2, sbData,  wordLen,
                                T2,            nc1Data, wordLen,
                                NULL,          NULL,    0,
                                R1);
        if (EPID_IS_FAILURE(retValue)) {break;}

        // R2 = G1.multiexp(h1, salpha, h2, sbeta, T2, nsx)
        retValue = ecpfMultiExp(ctx->params.G1,
                                ctx->group.h1, salphaData, wordLen,
                                ctx->group.h2, sbetaData,  wordLen,
                                T2,            nsxData,    wordLen,
                                NULL,          NULL,    0,
                                R2);
        if (EPID_IS_FAILURE(retValue)) {break;}

        // R3 = G3.multiexp(B, sf, K, nc')
        retValue = ecpfMultiExp(ctx->params.G3,
                                B,    sfData,  BITSIZE_WORD(EPID_SF_SIZE_BITS),
                                K,    nc3Data, wordLen,
                                NULL, NULL,    0,
                                NULL, NULL,    0,
                                R3);
        if (EPID_IS_FAILURE(retValue)) {break;}

        // t1 = G1.multiexp(T1, nsx, g1, c)
        retValue = ecpfMultiExp(ctx->params.G1,
                                T1,             nsxData, wordLen,
                                ctx->params.g1, cData,   wordLen,
                                NULL,           NULL,    0,
                                NULL,           NULL,    0,
                                t1);
        if (EPID_IS_FAILURE(retValue)) {
            break;
        } else {
            // set to failure again to catch future errors 
            retValue = EPID_FAILURE;
        }

        // t2 = G1.exp(T1, nc)
        sts = ippsGFPECMulPointScalar(T1, nc1Data, wordLen, t2, ctx->params.G1);
        if (ippStsNoErr != sts) {break;}

        // R4 = pairing(t1, g2)
        sts = ippsTatePairingDE3Apply(t1, ctx->params.g2, R4, ctx->params.tp);
        if (ippStsNoErr != sts) {break;}

        // t3 = pairing(t2, w)
        sts = ippsTatePairingDE3Apply(t2, ctx->group.w, t3, ctx->params.tp);
        if (ippStsNoErr != sts) {break;}

        // R4 = GT.mul(R4, t3)
        sts = ippsGFPXQMul(R4, t3, R4, ctx->params.GT);
        if (ippStsNoErr != sts) {break;}

        // t3 = GT.multiexp(e12, sf, e22, syalpha, e2w, sa)
        retValue = qfeMultiExp(ctx->params.GT,
                               ctx->e12, sfData, BITSIZE_WORD(EPID_SF_SIZE_BITS),
                               ctx->e22, syalphaData, wordLen,
                               ctx->e2w, saData, wordLen,
                               NULL, NULL, 0,
                               t3);
        if (EPID_IS_FAILURE(retValue)) {
            break;
        } else {
            // set to failure again to catch future errors 
            retValue = EPID_FAILURE;
        }

        // R4 = GT.mul(R4, t3)
        sts = ippsGFPXQMul(R4, t3, R4, ctx->params.GT);
        if (ippStsNoErr != sts) {break;}

        //Done with math computations.  Now do the hash computations
        retValue = computeSigmaC(&ctx->params,
                                 &ctx->group,
                                 B, K, T1, T2, R1, R2, R3, R4,
                                 epidSignature->nd, message, messageLength,
                                 cComputed,
                                 sizeof(cComputed)
                                 );
        if (EPID_IS_FAILURE(retValue)) {
            break;
        } else {
            // set to failure again to catch future errors 
            retValue = EPID_FAILURE;
        }

        // check c == H(t4 || nd || mSize || m)
        if (0 != memcmp(cComputed, epidSignature->c, sizeof(epidSignature->c))) {
            retValue = EPID_INVALID_SIGNATURE;
            break;
        }

        // FOR i = 1 to n1, compute t5 = G3.exp(B, f[i]) 
        // and verify that G3.isEqual(t5, K) = false
        if (ctx->revocationList) {
            for (index = 0; index < GET_RL_N1(ctx->revocationList); index++) {
                IppsElementCmpResult result;
                sts = octStr2BNU(GET_RL_F_PTR(ctx->revocationList, index), 
                                 EPID_NUMBER_SIZE, fData);
                if (ippStsNoErr != sts) {break;}
                sts = ippsGFPECMulPointScalar(B, fData, wordLen, t5, ctx->params.G3);
                if (ippStsNoErr != sts) {break;}
                sts = ippsGFPECCmpPoint(t5, K, &result, ctx->params.G3);
                if (ippStsNoErr != sts) {break;}
                if (IppsElementEQ == result) {
                    retValue = EPID_MEMBER_KEY_REVOKED;
                    break;
                }
            }
        }
        if (ippStsNoErr != sts) {break;}
        if (EPID_MEMBER_KEY_REVOKED == retValue) {break;}


        // If we get this far without breaking out of the one-pass
        // loop, then the verification has been successful!
        retValue = EPID_SUCCESS;
    } while (0);

    SAFE_FREE(B);
    SAFE_FREE(G3hashBsn);
    SAFE_FREE(K);
    SAFE_FREE(T1);
    SAFE_FREE(T2);
    SAFE_FREE(t1);
    SAFE_FREE(t2);
    SAFE_FREE(t5);
    SAFE_FREE(R1);
    SAFE_FREE(R2);
    SAFE_FREE(R3);
    SAFE_FREE(Bx);
    SAFE_FREE(By);
    SAFE_FREE(Kx);
    SAFE_FREE(Ky);
    SAFE_FREE(T1x);
    SAFE_FREE(T1y);
    SAFE_FREE(T2x);
    SAFE_FREE(T2y);
    SAFE_FREE(c1);
    SAFE_FREE(c3);
    SAFE_FREE(sx);
    SAFE_FREE(sy);
    SAFE_FREE(sa);
    SAFE_FREE(sb);
    SAFE_FREE(salpha);
    SAFE_FREE(sbeta);
    SAFE_FREE(nc1);
    SAFE_FREE(nc3);
    SAFE_FREE(nsx);
    SAFE_FREE(syalpha);
    SAFE_FREE(t3);
    SAFE_FREE(R4);

    return (retValue);
}
