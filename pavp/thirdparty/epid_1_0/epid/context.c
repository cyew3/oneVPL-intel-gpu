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
 * @brief This file implements functions for initializing internal
 * context structures
 */

#include "context.h"
#include "epid_errors.h"
#include "epid_macros.h"
#include "memutils.h"

#include <stdio.h>
#include "printutils.h"

//#define CONTEXT_DEBUG

EPID_RESULT 
epidParametersContext_init(const EPIDParameterCertificateBlob *paramsCert,
                           EPIDParameters *ctx)
{
    EPID_RESULT retValue = EPID_FAILURE;
    
    EPIDParameters *lctx = ctx;
    do {
        IppStatus sts;
        Ipp32u twista[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
        Ipp32u twistb[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];

        
        // check params
        if (NULL == paramsCert 
            || NULL == ctx) {
            retValue = EPID_NULL_PTR;
            break;
        }

        // load parameters into local context
        sts = octStr2BNU(paramsCert->p12, EPID_NUMBER_SIZE, lctx->p12);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(paramsCert->q12, EPID_NUMBER_SIZE, lctx->q12);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(paramsCert->h1, sizeof(Ipp32u), &(lctx->h1));
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(paramsCert->a12, EPID_NUMBER_SIZE, lctx->a12);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(paramsCert->b12, EPID_NUMBER_SIZE, lctx->b12);
        if (ippStsNoErr != sts) {break;}

        sts = octStr2BNU(paramsCert->coeff0, EPID_NUMBER_SIZE, lctx->coeffs);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(paramsCert->coeff1, EPID_NUMBER_SIZE, 
                         lctx->coeffs + 1*BITSIZE_WORD(EPID_NUMBER_SIZE_BITS));
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(paramsCert->coeff2, EPID_NUMBER_SIZE, 
                         lctx->coeffs + 2*BITSIZE_WORD(EPID_NUMBER_SIZE_BITS));
        if (ippStsNoErr != sts) {break;}
        // set high order coefficent to 1
        {
            Ipp32u* ptr = lctx->coeffs + 3*BITSIZE_WORD(EPID_NUMBER_SIZE_BITS);
            memset (ptr,0,EPID_NUMBER_SIZE);
            *ptr = 1;
        }

        sts = octStr2BNU(paramsCert->qnr, EPID_NUMBER_SIZE, lctx->qnr);
        if (ippStsNoErr != sts) {break;}
        
        sts = octStr2BNU(paramsCert->orderg2 ,3*EPID_NUMBER_SIZE, lctx->orderG2);
        if (ippStsNoErr != sts) {break;}
        
        sts = octStr2BNU(paramsCert->p3, EPID_NUMBER_SIZE, lctx->p3);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(paramsCert->q3, EPID_NUMBER_SIZE, lctx->q3);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(paramsCert->h3, sizeof(Ipp32u),  &(lctx->h3));
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(paramsCert->a3, EPID_NUMBER_SIZE, lctx->a3);
        if (ippStsNoErr != sts) {break;}
        sts =octStr2BNU(paramsCert->b3, EPID_NUMBER_SIZE, lctx->b3);
        if (ippStsNoErr != sts) {break;}
        
        sts = octStr2BNU(paramsCert->g1.x, EPID_NUMBER_SIZE, lctx->g1x);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(paramsCert->g1.y, EPID_NUMBER_SIZE, lctx->g1y);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(paramsCert->g2.x0, EPID_NUMBER_SIZE, 
                         lctx->g2x);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(paramsCert->g2.x1, EPID_NUMBER_SIZE, 
                         lctx->g2x + BITSIZE_WORD(EPID_NUMBER_SIZE_BITS));
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(paramsCert->g2.x2, EPID_NUMBER_SIZE, 
                         lctx->g2x + 2*BITSIZE_WORD(EPID_NUMBER_SIZE_BITS));
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(paramsCert->g2.y0, EPID_NUMBER_SIZE, 
                         lctx->g2y);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(paramsCert->g2.y1, EPID_NUMBER_SIZE, 
                         lctx->g2y + BITSIZE_WORD(EPID_NUMBER_SIZE_BITS));
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(paramsCert->g2.y2, EPID_NUMBER_SIZE, 
                         lctx->g2y + 2*BITSIZE_WORD(EPID_NUMBER_SIZE_BITS));
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(paramsCert->g3.x, EPID_NUMBER_SIZE, lctx->g3x);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(paramsCert->g3.y, EPID_NUMBER_SIZE, lctx->g3y);
        if (ippStsNoErr != sts) {break;}

        // setup EC Prime Field G1
        lctx->G1  = createECPrimeField(EPID_NUMBER_SIZE_BITS,
                                       lctx->a12, lctx->b12, 
                                       lctx->g1x, lctx->g1y,
                                       lctx->q12, lctx->p12,
                                       lctx->h1, 
                                       &(lctx->Fq12),
                                       &(lctx->g1),
                                       NULL, 0,
                                       NULL, 0);
        if (NULL == lctx->G1) {
            retValue = EPID_OUTOFMEMORY;
            break;
        }
        
        {
            IppStatus sts;
            // do setup work for G2 params to get twists
            //twista = (a * qnr * qnr ) mod q
            //twistb = (b * qnr * qnr * qnr ) mod q
            IppsGFPElement* pGFEtwistA = NULL;
            IppsGFPElement* pGFEtwistB = NULL;
            IppsGFPElement* pGFEqnr = NULL;
            IppsGFPElement* pGFEqnr2 = NULL;
            IppsGFPElement* pGFEqnr3 = NULL;
            
            pGFEtwistA = createGFPElement(lctx->a12, 
                                          BITSIZE_WORD(EPID_NUMBER_SIZE_BITS),
                                          lctx->Fq12, 
                                          NULL, 0);
            pGFEtwistB = createGFPElement(lctx->b12, 
                                          BITSIZE_WORD(EPID_NUMBER_SIZE_BITS),
                                          lctx->Fq12, 
                                          NULL, 0);
            pGFEqnr = createGFPElement(lctx->qnr,
                                       BITSIZE_WORD(EPID_NUMBER_SIZE_BITS), 
                                       lctx->Fq12, 
                                       NULL, 0);
            pGFEqnr2 = createGFPElement(lctx->qnr,
                                        BITSIZE_WORD(EPID_NUMBER_SIZE_BITS), 
                                        lctx->Fq12, 
                                        NULL, 0);
            pGFEqnr3 = createGFPElement(lctx->qnr,
                                        BITSIZE_WORD(EPID_NUMBER_SIZE_BITS), 
                                        lctx->Fq12, 
                                        NULL, 0);
              
            if (NULL == pGFEtwistA  
                || NULL == pGFEtwistB
                || NULL == pGFEqnr
                || NULL == pGFEqnr2
                || NULL == pGFEqnr3
                ) {
                retValue = EPID_OUTOFMEMORY;
                SAFE_FREE(pGFEtwistA);
                SAFE_FREE(pGFEtwistB);
                SAFE_FREE(pGFEqnr);
                SAFE_FREE(pGFEqnr2);
                SAFE_FREE(pGFEqnr3);
                break;
            } 
            
            //twista = (a * qnr * qnr ) mod q
            //     qnr2 = qnr*qnr
            sts = ippsGFPMul(pGFEqnr, pGFEqnr, pGFEqnr2, lctx->Fq12);
            if (ippStsNoErr != sts) {break;}
            //    twista = a * qnr2
            sts = ippsGFPMul(pGFEtwistA, pGFEqnr2, pGFEtwistA, lctx->Fq12);
            if (ippStsNoErr != sts) {break;}
            
            //twistb = (b * qnr * qnr * qnr ) mod q
            //    qnr3 = qnr2 * qnr
            sts = ippsGFPMul(pGFEqnr2, pGFEqnr, pGFEqnr3, lctx->Fq12);
            if (ippStsNoErr != sts) {break;}
            //    twistb = b * qnr3
            sts = ippsGFPMul(pGFEtwistB, pGFEqnr3, pGFEtwistB, lctx->Fq12);
            if (ippStsNoErr != sts) {break;}
            
            // extract element data to buffer
            sts = ippsGFPGetElement(pGFEtwistA, 
                                    twista, 
                                    sizeof(twista)/sizeof(Ipp32u), 
                                    lctx->Fq12);
            if (ippStsNoErr != sts) {break;}
            sts = ippsGFPGetElement(pGFEtwistB, 
                                    twistb, 
                                    sizeof(twistb)/sizeof(Ipp32u), 
                                    lctx->Fq12);
            if (ippStsNoErr != sts) {break;}
            
            SAFE_FREE(pGFEtwistA);
            SAFE_FREE(pGFEtwistB);
            SAFE_FREE(pGFEqnr);
            SAFE_FREE(pGFEqnr2);
            SAFE_FREE(pGFEqnr3);
        }
                    
        // setup EC Finite Field G2
        lctx->G2 = createECFiniteField(EPID_NUMBER_SIZE_BITS,
                                       twista, twistb, 
                                       lctx->g2x, lctx->g2y, 
                                       lctx->coeffs, 3,
                                       lctx->orderG2,
                                       3*BITSIZE_WORD(EPID_NUMBER_SIZE_BITS),
                                       lctx->Fq12,
                                       &(lctx->Fqd),
                                       &(lctx->g2),
                                       NULL, 0,
                                       NULL, 0);
        if (NULL == lctx->G2) {
            retValue = EPID_OUTOFMEMORY;
            break;
        }
            
    
        
        // setup EC Prime Field G3
        lctx->G3  = createECPrimeField(EPID_NUMBER_SIZE_BITS,
                                       lctx->a3, lctx->b3, 
                                       lctx->g3x, lctx->g3y, 
                                       lctx->q3, lctx->p3,
                                       lctx->h3, 
                                       &(lctx->Fq3),
                                       &(lctx->g3),
                                       NULL, 0,
                                       NULL, 0);
        if (NULL == lctx->G3) {
            retValue = EPID_OUTOFMEMORY;
            break;
        }

        // setup Quadratic Field Extension GT
        lctx->GT = createQuadFieldExt(lctx->Fqd,
                                      lctx->qnr, 
                                      BITSIZE_WORD(EPID_NUMBER_SIZE_BITS),
                                      NULL, 0);
        if (NULL == lctx->GT) {
            retValue = EPID_OUTOFMEMORY;
            break;
        }
        
        // setup tate pairing state
        lctx->tp = createTatePairing(lctx->G1, lctx->G2, lctx->GT,
                                     NULL, 0);
        if (NULL == lctx->tp) {
            retValue = EPID_OUTOFMEMORY;
            break;
        }

        lctx->Fp12 = createPrimeField(lctx->p12, EPID_NUMBER_SIZE_BITS,
                                      NULL, 0); 
        if (NULL == lctx->Fp12) {
            retValue = EPID_OUTOFMEMORY;
            break;
        }

        lctx->Fp3 = createPrimeField(lctx->p3, EPID_NUMBER_SIZE_BITS,
                                     NULL, 0); 
        if (NULL == lctx->Fp3) {
            retValue = EPID_OUTOFMEMORY;
            break;
        }

        retValue = EPID_SUCCESS;
    } while (0);

    if ((EPID_OUTOFMEMORY == retValue) && lctx) {
        epidParametersContext_destroy(lctx);
    }

    return (retValue);
}

void epidParametersContext_destroy(EPIDParameters *ctx)
{
    if (ctx) {
        SAFE_FREE(ctx->G1);
        SAFE_FREE(ctx->G2);
        SAFE_FREE(ctx->G3);
        SAFE_FREE(ctx->GT);
        SAFE_FREE(ctx->tp);
        SAFE_FREE(ctx->g1);
        SAFE_FREE(ctx->g2);
        SAFE_FREE(ctx->g3);
        SAFE_FREE(ctx->Fq12);
        SAFE_FREE(ctx->Fqd);
        SAFE_FREE(ctx->Fq3);
        SAFE_FREE(ctx->Fp12);
        SAFE_FREE(ctx->Fp3);
    }
}


EPID_RESULT epidGroupContext_init(const EPIDGroupCertificateBlob *groupCert,
                                  EPIDParameters                 *params,
                                  EPIDGroup                      *ctx)
{
    EPID_RESULT retValue = EPID_FAILURE;
    
    do {
        IppStatus sts;
        Ipp32u wordLen;
        
        // check params
        if (NULL == groupCert
            || NULL == params
            || NULL == ctx) {
            retValue = EPID_NULL_PTR;
            break;
        }

        wordLen = BITSIZE_WORD(EPID_NUMBER_SIZE_BITS);

        memcpy(ctx->gid, groupCert->gid, sizeof(ctx->gid));

        sts = octStr2BNU(groupCert->h1.x, EPID_NUMBER_SIZE, ctx->h1xData);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(groupCert->h1.y, EPID_NUMBER_SIZE, ctx->h1yData);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(groupCert->h2.x, EPID_NUMBER_SIZE, ctx->h2xData);
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(groupCert->h2.y, EPID_NUMBER_SIZE, ctx->h2yData);
        if (ippStsNoErr != sts) {break;}

        sts = octStr2BNU(groupCert->w.x0, EPID_NUMBER_SIZE, 
                         ctx->wxData + 0*BITSIZE_WORD(EPID_NUMBER_SIZE_BITS));
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(groupCert->w.x1, EPID_NUMBER_SIZE, 
                         ctx->wxData + 1*BITSIZE_WORD(EPID_NUMBER_SIZE_BITS));
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(groupCert->w.x2, EPID_NUMBER_SIZE, 
                         ctx->wxData + 2*BITSIZE_WORD(EPID_NUMBER_SIZE_BITS));
        if (ippStsNoErr != sts) {break;}

        sts = octStr2BNU(groupCert->w.y0, EPID_NUMBER_SIZE, 
                         ctx->wyData + 0*BITSIZE_WORD(EPID_NUMBER_SIZE_BITS));
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(groupCert->w.y1, EPID_NUMBER_SIZE, 
                         ctx->wyData + 1*BITSIZE_WORD(EPID_NUMBER_SIZE_BITS));
        if (ippStsNoErr != sts) {break;}
        sts = octStr2BNU(groupCert->w.y2, EPID_NUMBER_SIZE, 
                         ctx->wyData + 2*BITSIZE_WORD(EPID_NUMBER_SIZE_BITS));
        if (ippStsNoErr != sts) {break;}

        // This is the only error code that will be triggered by following logic
        retValue = EPID_OUTOFMEMORY;

        ctx->h1x = createGFPElement(ctx->h1xData, wordLen, params->Fq12, 
                                    NULL, 0);
        if (NULL == ctx->h1x) {break;}
        ctx->h1y = createGFPElement(ctx->h1yData, wordLen, params->Fq12, 
                                    NULL, 0);
        if (NULL == ctx->h1y) {break;}

        ctx->h2x = createGFPElement(ctx->h2xData, wordLen, params->Fq12, 
                                    NULL, 0);
        if (NULL == ctx->h2x) {break;}
        ctx->h2y = createGFPElement(ctx->h2yData, wordLen, params->Fq12, 
                                    NULL, 0);
        if (NULL == ctx->h2y) {break;}

        ctx->wx = createFiniteFieldElement(params->Fqd, 
                                           ctx->wxData, wordLen * 3, 
                                           NULL, 0);
        if (NULL == ctx->wx) {break;}
        ctx->wy = createFiniteFieldElement(params->Fqd, 
                                           ctx->wyData, wordLen * 3, 
                                           NULL, 0);
        if (NULL == ctx->wy) {break;}

        ctx->h1 = createGFPECPoint(params->G1, NULL, 0);
        if (NULL == ctx->h1) {break;}
        ctx->h2 = createGFPECPoint(params->G1, NULL, 0);
        if (NULL == ctx->h2) {break;}

        ctx->w = createGFPXECPoint(params->G2, NULL, 0);
        if (NULL == ctx->w) {break;}

        // default back to general error
        retValue = EPID_FAILURE;

        sts = ippsGFPECSetPoint(ctx->h1x, ctx->h1y, ctx->h1, params->G1);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPECSetPoint(ctx->h2x, ctx->h2y, ctx->h2, params->G1);
        if (ippStsNoErr != sts) {break;}

        sts = ippsGFPXECSetPoint(ctx->wx, ctx->wy, ctx->w, params->G2);
        if (ippStsNoErr != sts) {break;}

        retValue = EPID_SUCCESS;
    }while (0);

    return (retValue);
}

void epidGroupContext_destroy(EPIDGroup *ctx)
{
    if (ctx) {
        SAFE_FREE(ctx->h1);
        SAFE_FREE(ctx->h2);
        SAFE_FREE(ctx->w);

        SAFE_FREE(ctx->h1x);
        SAFE_FREE(ctx->h1y);

        SAFE_FREE(ctx->h2x);
        SAFE_FREE(ctx->h2y);

        SAFE_FREE(ctx->wx);
        SAFE_FREE(ctx->wy);
    }
}

void epidParametersContext_display(EPIDParameters *ctx) 
{
    printf("======= Little Endian Context ======= \n");
    if (ctx) {
        //p12
        printf("p12    : ");
        PrintOctString((Ipp8u*)ctx->p12, EPID_NUMBER_SIZE, 1);
        //q12
        printf("q12    : ");
        PrintOctString((Ipp8u*)ctx->q12, EPID_NUMBER_SIZE, 1);
        //h1
        printf("h1     : ");
        PrintOctString((Ipp8u*)&ctx->h1, sizeof(Ipp32u), 1);
        //a12
        printf("a12    : ");
        PrintOctString((Ipp8u*)ctx->a12, EPID_NUMBER_SIZE, 1);
        //b12
        printf("b12    : ");
        PrintOctString((Ipp8u*)ctx->b12, EPID_NUMBER_SIZE, 1);
        //coeffs
        printf("coeffs : \n");
        printf("coeff0 : ");
        PrintOctString((Ipp8u*)ctx->coeffs, EPID_NUMBER_SIZE, 1);
        printf("coeff1 : ");
        PrintOctString((Ipp8u*)ctx->coeffs + 1*EPID_NUMBER_SIZE, 
            EPID_NUMBER_SIZE, 1);
        printf("coeff2 : ");
        PrintOctString((Ipp8u*)ctx->coeffs + 2*EPID_NUMBER_SIZE, 
            EPID_NUMBER_SIZE, 1);
        printf("coeff3 : ");
        PrintOctString((Ipp8u*)ctx->coeffs + 3*EPID_NUMBER_SIZE, 
            EPID_NUMBER_SIZE, 1); 
        //qnr
        printf("qnr    : ");
        PrintOctString((Ipp8u*)ctx->qnr, EPID_NUMBER_SIZE, 1);
        //orderG2
        printf("orderG2: ");
        PrintOctString((Ipp8u*)ctx->orderG2, 3*EPID_NUMBER_SIZE, 1);
        //p3
        printf("p3     : ");
        PrintOctString((Ipp8u*)ctx->p3, EPID_NUMBER_SIZE, 1);
        //q3
        printf("q3     : ");
        PrintOctString((Ipp8u*)ctx->q3, EPID_NUMBER_SIZE, 1);
        //h3
        printf("h3     : ");
        PrintOctString((Ipp8u*)&ctx->h3, sizeof(Ipp32u), 1);
        //a3
        printf("a3     : ");
        PrintOctString((Ipp8u*)ctx->a3, EPID_NUMBER_SIZE, 1);
        //b3
        printf("b3     : ");
        PrintOctString((Ipp8u*)ctx->b3, EPID_NUMBER_SIZE, 1);
        //g1x
        printf("g1x    : ");
        PrintOctString((Ipp8u*)ctx->g1x, EPID_NUMBER_SIZE, 1);
        //g1y
        printf("g1y    : ");
        PrintOctString((Ipp8u*)ctx->g1y, EPID_NUMBER_SIZE, 1);
        //g2x
        printf("g2x[0] : ");
        PrintOctString((Ipp8u*)ctx->g2x, EPID_NUMBER_SIZE, 1);
        printf("   [1] : ");
        PrintOctString((Ipp8u*)ctx->g2x + EPID_NUMBER_SIZE, EPID_NUMBER_SIZE, 1);
        printf("   [2] : ");
        PrintOctString((Ipp8u*)ctx->g2x + 2*EPID_NUMBER_SIZE, EPID_NUMBER_SIZE, 1);
        //g2y
        printf("g2y[0] : ");
        PrintOctString((Ipp8u*)ctx->g2y, EPID_NUMBER_SIZE, 1);
        printf("   [1] : ");
        PrintOctString((Ipp8u*)ctx->g2y + EPID_NUMBER_SIZE, EPID_NUMBER_SIZE, 1);
        printf("   [2] : ");
        PrintOctString((Ipp8u*)ctx->g2y + 2*EPID_NUMBER_SIZE, EPID_NUMBER_SIZE, 1);
        //g3x
        printf("g3x    : ");
        PrintOctString((Ipp8u*)ctx->g3x, EPID_NUMBER_SIZE, 1);
        //g3y
        printf("g3y    : ");
        PrintOctString((Ipp8u*)ctx->g3y, EPID_NUMBER_SIZE, 1);
    } else {
        printf("UNDEFINED\n");
    }
    printf("--------------------------------------\n");
}
