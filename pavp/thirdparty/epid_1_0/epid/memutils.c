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
 * @brief This file implements functions for allocating and releasing
 * memory buffers for underlying data types
 */
#include "memutils.h"
#include "epid_macros.h"  
#include "epid_constants.h"


IppsBigNumState * createBigNum(void * data, int dataSize, 
                               DataEndianness direction, 
                               void * buf, int bufSize)
{
    IppStatus sts = ippStsNoErr;
    IppsBigNumState * ctx = NULL;
    do {
        int ctxSize;
        int wordSize = (dataSize + 3) / 4;

        // Determine how much memory is required for the context structure
        sts = ippsBigNumGetSize(wordSize, &ctxSize);
        if (ippStsNoErr != sts) {break;}

        if (buf) {
            // Make sure the user has provided sufficient memory for the context
            if (ctxSize < bufSize) {
                break;
            }
            ctx = (IppsBigNumState *) buf;
        } else {
            // use dynamic memory allocation for the structure
            ctx = (IppsBigNumState *) SAFE_ALLOC(ctxSize);
        }

        if (ctx) {
            // Initialize the structure
            sts = ippsBigNumInit(wordSize, ctx);
            if (ippStsNoErr != sts) {break;}

            if(data) {
                // user has provided data.  Insert the data into the structure
                if (Little == direction) {
                    sts = ippsSet_BN(IppsBigNumPOS, wordSize, data, ctx);
                    if (ippStsNoErr != sts) {break;}
                } else {
                    sts = ippsSetOctString_BN(data, dataSize, ctx);
                    if (ippStsNoErr != sts) {break;}
                }
            }
        }
    } while (0);
    if (ippStsNoErr != sts) {
        if (!buf) {
            // we can only do this free if we allocated the buffer,
            // not if it was passed in 
            SAFE_FREE(ctx);
        }
        ctx = NULL;
    }
    return ctx;
}

IppsGFPElement* createGFPElement(const Ipp32u *pData, const Ipp32u dataLen,
                                 IppsGFPState* pGF, 
                                 void * buf, unsigned int bufSize)
{
    IppStatus sts = ippStsNoErr;
    IppsGFPElement* pR = NULL;
    Ipp32u gfElemSize;
    

    do {
        // Determine how much memory is required for the context structure
        sts = ippsGFPElementGetSize(pGF, &gfElemSize);
        if (ippStsNoErr != sts) {break;}
        
        if (buf) {
            // Make sure the user has provided sufficient memory for the context
            if (gfElemSize < bufSize) {
                break;
            }
            pR = (IppsGFPElement *) buf;
        } else {
            // use dynamic memory allocation for the structure
            pR = (IppsGFPElement *) SAFE_ALLOC(gfElemSize);
        }    

        if (pR) {      
            sts = ippsGFPElementInit(pR, pData, dataLen, pGF);
            if (ippStsNoErr != sts) {break;}
        }
    } while (0);
    if (ippStsNoErr != sts) {
        if (!buf) {
            // we can only do this free if we allocated the buffer,
            // not if it was passed in 
            SAFE_FREE(pR);
        }
        pR = NULL;
    }
    return pR;
}

IppsGFPState* createPrimeField(const Ipp32u * pPrime, 
                               const Ipp32u bitSize, 
                               const void * buf, 
                               const unsigned int bufSize)
{
    IppStatus sts = ippStsNoErr;
    IppsGFPState* pGFpCtx = NULL;
    
    Ipp32u ctxSizeInBytes;

    do {
        //determine how much memory is required for the context structure
        sts = ippsGFPGetSize (bitSize, &ctxSizeInBytes);
        if (ippStsNoErr != sts) {break;}
        
        if (buf) {
            // Insure user provided sufficent memory for the context
            if (ctxSizeInBytes < bufSize) {
                break;
            }
            pGFpCtx = (IppsGFPState *) buf;
        } else {
            // use dynamic memory allocation 
            pGFpCtx = (IppsGFPState *) SAFE_ALLOC(ctxSizeInBytes);
        }
        
        if (pGFpCtx) {
            // Initialize the prime Field
            sts = ippsGFPInit (pGFpCtx, pPrime, bitSize); 
            if (ippStsNoErr != sts) {break;}
        }
        
    } while (0);
    if (ippStsNoErr != sts) {
        if (!buf) {
            // we can only do this free if we allocated the buffer,
            // not if it was passed in 
            SAFE_FREE(pGFpCtx);
        }
        pGFpCtx = NULL;
    }
    
    return pGFpCtx;
}

IppsGFPXState* createFiniteField(IppsGFPState* pGFpCtx, 
                                 const Ipp32u degree,
                                 const Ipp32u* pCoeffs,
                                 const void * buf, 
                                 const unsigned int bufSize)
{
    IppStatus sts = ippStsNoErr;
    IppsGFPXState* pGFPExtCtx = NULL;
    
    Ipp32u ctxSizeInBytes;

    do {
        //determine how much memory is required for the context structure
        sts = ippsGFPXGetSize (pGFpCtx, degree, &ctxSizeInBytes);
        if (ippStsNoErr != sts) {break;}
        
        if (buf) {
            // Insure user provided sufficent memory for the context
            if (ctxSizeInBytes < bufSize) {
                break;
            }
            pGFPExtCtx = (IppsGFPXState *) buf;
        } else {
            // use dynamic memory allocation 
            pGFPExtCtx = (IppsGFPXState *) SAFE_ALLOC(ctxSizeInBytes);
        }
        
        if (pGFPExtCtx) {
            // Initialize the finite Field
            sts = ippsGFPXInit(pGFPExtCtx, pGFpCtx, degree, pCoeffs);
            if (ippStsNoErr != sts) {break;}
        }
    } while (0);
    if (ippStsNoErr != sts) {
        // probably too large bitsize passed in
        if (!buf) {
            // free previous alloc and exit gracefully
            SAFE_FREE(pGFPExtCtx);
        }
        pGFPExtCtx = NULL;
    }
    return pGFPExtCtx;
}

IppsGFPXElement* createFiniteFieldElement(IppsGFPXState* pGFPExtCtx, 
                                          const Ipp32u* pA, 
                                          Ipp32u lenA,
                                          const void * buf, 
                                          const unsigned int bufSize)
{
    IppStatus sts = ippStsNoErr;
    IppsGFPXElement* element = NULL;
    
    Ipp32u ctxSizeInBytes;

    do {
        //determine how much memory is required for the context structure
        sts = ippsGFPXElementGetSize (pGFPExtCtx, &ctxSizeInBytes);
        if (ippStsNoErr != sts) {break;}
        
        if (buf) {
            // Insure user provided sufficent memory for the context
            if (ctxSizeInBytes < bufSize) {
                break;
            }
            element = (IppsGFPXElement *) buf;
        } else {
            // use dynamic memory allocation 
            element = (IppsGFPXElement *) SAFE_ALLOC(ctxSizeInBytes);
        }
        
        if (element) {
            // Initialize the finite Field element
            sts = ippsGFPXElementInit(element, pA, lenA, pGFPExtCtx);
            if (ippStsNoErr != sts) {break;}
        }
    } while (0);
    if (ippStsNoErr != sts) {
        // probably too large bitsize passed in
        if (!buf) {
            // free previous alloc and exit gracefully
            SAFE_FREE(element);
        }
        element = NULL;
    }
    return element;
}


// side effect: create generator point, create primefield
IppsGFPECState *createECPrimeField(const Ipp32u bitSize,
                                   const Ipp32u* pA, const Ipp32u* pB,
                                   const Ipp32u* pX, const Ipp32u* pY,
                                   const Ipp32u* pQ, const Ipp32u* pOrder,
                                   const Ipp32u cofactor,
                                   IppsGFPState** ppGFpCtx,
                                   IppsGFPECPoint** ppPoint,
                                   void * ecbuf, const unsigned int ecbufSize,
                                   void * buf, const unsigned int bufSize)
{
    IppStatus sts = ippStsNoErr;
    IppsGFPECState *pState = NULL;
    Ipp32u stateSize = 0;
    Ipp32u pointSize = 0;
    
    IppsGFPElement* pgfeA = NULL;
    IppsGFPElement* pgfeB = NULL;
    IppsGFPElement* pgfeX = NULL;
    IppsGFPElement* pgfeY = NULL;
    Ipp32u gfElemSize = 0;
            
    do {
        // validate input pointers
        if (NULL == ppGFpCtx 
            || NULL == ppPoint) {
            break;
        }

        //@warning possible memory leak if there are already real pointers
        *ppGFpCtx = NULL;
        *ppPoint = NULL;

        // build the Prime Field first        
        *ppGFpCtx = createPrimeField (pQ, bitSize,
                                      NULL, 0);
        if(NULL == *ppGFpCtx) {
            break;
        }
                
        //setup EC PF paramaters as PF Elements (A, B, X, Y)
        sts = ippsGFPElementGetSize(*ppGFpCtx, &gfElemSize);
        if (ippStsNoErr != sts) {break;}
        pgfeA = (IppsGFPElement *) SAFE_ALLOC(gfElemSize);
        pgfeB = (IppsGFPElement *) SAFE_ALLOC(gfElemSize);
        pgfeX = (IppsGFPElement *) SAFE_ALLOC(gfElemSize);
        pgfeY = (IppsGFPElement *) SAFE_ALLOC(gfElemSize);
        if(!(pgfeA && pgfeB && pgfeX && pgfeY)) {
            break;
        }
        sts = ippsGFPElementInit(pgfeA, pA, BITSIZE_WORD(bitSize), *ppGFpCtx);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPElementInit(pgfeB, pB, BITSIZE_WORD(bitSize), *ppGFpCtx);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPElementInit(pgfeX, pX, BITSIZE_WORD(bitSize), *ppGFpCtx);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPElementInit(pgfeY, pY, BITSIZE_WORD(bitSize), *ppGFpCtx);
        if (ippStsNoErr != sts) {break;}

        //construct the ECPrimeField
        sts = ippsGFPECGetSize(*ppGFpCtx, &stateSize);
        if (ippStsNoErr != sts) {break;}    
        if (buf) {
            // Make sure the user has provided sufficient memory for the context
            if (stateSize < bufSize) {
                break;
            }
            pState = (IppsGFPECState *) buf;
        } else {
            // use dynamic memory allocation for the structure
            pState = (IppsGFPECState *) SAFE_ALLOC(stateSize);
            if (!pState) {sts = ippStsErr; break;}
        }   
        //@warning pGFpCtx needs to live on, and must be freed
        // when done with the ECpf and not before!        
        sts = ippsGFPECInit(pState, pgfeA, pgfeB, pgfeX, pgfeY, 
                            pOrder, BITSIZE_WORD(bitSize), cofactor, 
                            *ppGFpCtx);
        if (ippStsNoErr != sts) {break;}  

        //construct the ECPrimField generator
        sts = ippsGFPECPointGetSize(pState, &pointSize);
        if (ippStsNoErr != sts) {break;}
        if (ecbuf) {
            // Make sure the user has provided sufficient memory for the context
            if (pointSize < ecbufSize) {
                break;
            }
            *ppPoint = (IppsGFPECPoint *) ecbuf;
        } else {
            // use dynamic memory allocation for the structure
            *ppPoint = (IppsGFPECPoint *) SAFE_ALLOC(pointSize);
            if (!*ppPoint) {sts = ippStsErr; break;}
        }   
        
        sts = ippsGFPECPointInit(*ppPoint, pgfeX, pgfeY, pState); 
        if (ippStsNoErr != sts) {break;}
        
    } while (0);
    
    if (ippStsNoErr != sts && ppPoint && ppGFpCtx) {
        // we had a problem during init, free any allocated memory
        SAFE_FREE(*ppGFpCtx);
        if (!ecbuf) {
            SAFE_FREE(*ppPoint);
        }
        if (!buf) {
            SAFE_FREE(pState);
        }
        pState = NULL;
    }
    
    SAFE_FREE(pgfeA);
    SAFE_FREE(pgfeB);
    SAFE_FREE(pgfeX);
    SAFE_FREE(pgfeY);
    return pState;
}

IppsGFPECPoint *createGFPECPoint(IppsGFPECState *pEC, 
                                 void * buf, const unsigned int bufSize)
{
    IppStatus sts = ippStsNoErr;
    IppsGFPECPoint *pPoint = NULL;
    Ipp32u pointSize = 0;

    do {
        sts = ippsGFPECPointGetSize(pEC, &pointSize);
        if (ippStsNoErr != sts) {break;}
        if (buf) {
            // Make sure the user has provided sufficient memory for the context
            if (pointSize < bufSize) {
                break;
            }
            pPoint = (IppsGFPECPoint *) buf;
        } else {
            // use dynamic memory allocation for the structure
            pPoint = (IppsGFPECPoint *) SAFE_ALLOC(pointSize);
            if (!pPoint) {sts = ippStsErr; break;}
        }   
                    
        sts = ippsGFPECPointInit (pPoint, 
                                  0, 
                                  0,
                                  pEC);
        if (ippStsNoErr != sts) {break;}
            
    } while (0);
    if (ippStsNoErr != sts) {
        // we had a problem during init, free any allocated memory
        if (!buf) {
            SAFE_FREE(pPoint);
        }
        pPoint = NULL;
    }
    return pPoint;
}


// side effect: create generator point &finite field
IppsGFPXECState *createECFiniteField(const Ipp32u bitSize,
                                     const Ipp32u* pA, const Ipp32u* pB,
                                     const Ipp32u* pXff, const Ipp32u* pYff,
                                     const Ipp32u* pCoeffs, 
                                     const Ipp32u degree, 
                                     const Ipp32u* pOrderG2,
                                     const Ipp32u orderLen,
                                     IppsGFPState* pGFpCtx,
                                     IppsGFPXState** ppGFPExtCtx,
                                     IppsGFPXECPoint **ppPoint,
                                     void * ecbuf, const unsigned int ecbufSize,
                                     void * buf, const unsigned int bufSize)
{
    IppStatus sts = ippStsNoErr;
    IppsGFPXECState *pState = NULL;
    Ipp32u stateSize = 0;
    Ipp32u pointSize = 0;
    
        
    IppsGFPXElement* pgfxeA = NULL;
    IppsGFPXElement* pgfxeB = NULL;
    IppsGFPXElement* pgfxeX = NULL;
    IppsGFPXElement* pgfxeY = NULL;
    Ipp32u gfxElemSize = 0;
    
    do {
        // validate input pointers
        if (NULL == pGFpCtx 
            || NULL == ppGFPExtCtx
            || NULL == ppPoint) {
            break;
        }
        //@warning possible memory leak if there are already real pointers
        *ppGFPExtCtx = NULL;
        *ppPoint = NULL;
        
        //build Finite Field
        *ppGFPExtCtx  = createFiniteField(pGFpCtx, degree, pCoeffs, 
                                          NULL, 0);
        if(NULL == *ppGFPExtCtx) {
            break;
        }       

        //setup EC FF parameters as FF elements (A, B, X, Y)
        sts = ippsGFPXElementGetSize(*ppGFPExtCtx, &gfxElemSize);
        if (ippStsNoErr != sts) {break;}
        pgfxeA = (IppsGFPXElement *) SAFE_ALLOC(gfxElemSize);
        pgfxeB = (IppsGFPXElement *) SAFE_ALLOC(gfxElemSize);
        pgfxeX = (IppsGFPXElement *) SAFE_ALLOC(gfxElemSize);
        pgfxeY = (IppsGFPXElement *) SAFE_ALLOC(gfxElemSize);
        if(!(pgfxeA && pgfxeB && pgfxeX && pgfxeY)) {
            break;
        }
                
        sts = ippsGFPXElementInit(pgfxeA, pA, BITSIZE_WORD(bitSize), 
                                  *ppGFPExtCtx);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPXElementInit(pgfxeB, pB, BITSIZE_WORD(bitSize), 
                                  *ppGFPExtCtx);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPXElementInit(pgfxeX, pXff, 3*BITSIZE_WORD(bitSize), 
                                  *ppGFPExtCtx);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPXElementInit(pgfxeY, pYff, 3*BITSIZE_WORD(bitSize), 
                                  *ppGFPExtCtx);
        if (ippStsNoErr != sts) {break;}

        //construct the ECFiniteField
        sts = ippsGFPXECGetSize(*ppGFPExtCtx, &stateSize);
        if (ippStsNoErr != sts) {break;}
        if (buf) {
            // Make sure the user has provided sufficient memory for the context
            if (stateSize < bufSize) {
                break;
            }
            pState = (IppsGFPXECState *) buf;
        } else {
            // use dynamic memory allocation for the structure
            pState = (IppsGFPXECState *) SAFE_ALLOC(stateSize);
            if (!pState) {sts = ippStsErr; break;}
        }   
        
        //@note cofactor is 1 for epid G2
        sts = ippsGFPXECInit(pState, pgfxeA, pgfxeB, pgfxeX, pgfxeY, 
                             pOrderG2, orderLen, 1, 
                             *ppGFPExtCtx);
        if (ippStsNoErr != sts) {break;}

        //construct the ECFiniteField generator
        sts = ippsGFPXECPointGetSize(pState, &pointSize);
        if (ippStsNoErr != sts) {break;}
        if (ecbuf) {
            // Make sure the user has provided sufficient memory for the context
            if (pointSize < ecbufSize) {
                break;
            }
            *ppPoint = (IppsGFPXECPoint *) ecbuf;
        } else {
            // use dynamic memory allocation for the structure
            *ppPoint = (IppsGFPXECPoint *) SAFE_ALLOC(pointSize);
            if (!*ppPoint) {sts = ippStsErr; break;}
        }   
        
        sts = ippsGFPXECPointInit(*ppPoint, pgfxeX, pgfxeY, pState); 
        if (ippStsNoErr != sts) {break;}
    } while (0);

    if (ippStsNoErr != sts && ppPoint && ppGFPExtCtx) {
        // we had a problem during init, free any allocated memory
        SAFE_FREE(*ppGFPExtCtx);
        if (!ecbuf) {
            SAFE_FREE(*ppPoint);
        }
        if (!buf) {
            SAFE_FREE(pState);
        }
        pState = NULL;
    }
    
    SAFE_FREE(pgfxeA);
    SAFE_FREE(pgfxeB);
    SAFE_FREE(pgfxeX);
    SAFE_FREE(pgfxeY);
    
    return pState;
}

IppsGFPXECPoint *createGFPXECPoint(IppsGFPXECState *pEC,
                                   void * buf, const unsigned int bufSize)
{
    IppStatus sts = ippStsNoErr;
    IppsGFPXECPoint *pPoint = NULL;
    Ipp32u pointSize = 0;

    do {
        sts = ippsGFPXECPointGetSize(pEC, &pointSize);
        if (ippStsNoErr != sts) {break;}
        if (buf) {
            // Make sure the user has provided sufficient memory for the context
            if (pointSize < bufSize) {
                break;
            }
            pPoint = (IppsGFPXECPoint *) buf;
        } else {
            // use dynamic memory allocation for the structure
            pPoint = (IppsGFPXECPoint *) SAFE_ALLOC(pointSize);
            if (!pPoint) {sts = ippStsErr; break;}
        }   
        // Init point 
        sts = ippsGFPXECPointInit(pPoint, 0, 0, pEC);
        if (ippStsNoErr != sts) {break;}
    } while (0);
    if (ippStsNoErr != sts) {
        // we had a problem during init, free any allocated memory
        if (!buf) {
            SAFE_FREE(pPoint);
        }
        pPoint = NULL;
    }
    return pPoint;
}

IppsGFPXQState *createQuadFieldExt(IppsGFPXState* pGFPExtCtx,
                                   const Ipp32u* pQnr, const unsigned int qnrLen,
                                   const void * buf, const unsigned int bufSize)
{
    IppStatus sts = ippStsNoErr;
    IppsGFPXQState *pState = NULL;
    Ipp32u stateSize = 0;

    Ipp32u* pModulus = NULL;
    IppsGFPXElement* pQnrGFXE = NULL;
    Ipp32u gfxElemSize = 0;
    Ipp32u gfxElemLen = 0;

    Ipp32u gfxDegree;

    do {
        sts = ippsGFPXGet(pGFPExtCtx,
                          0, 0, &gfxDegree, 0, &gfxElemLen);
        if (ippStsNoErr != sts) {break;}


        pModulus = SAFE_ALLOC (qnrLen * sizeof(Ipp32u) * gfxDegree * 3);
        if(NULL == pModulus) {
            break;
        }
        
        //setup Modulus = {-qnr, 0, 1}
        memset (pModulus, 0 , qnrLen * sizeof(Ipp32u) * gfxDegree * 3);
        // high order coeff = 1
        *(pModulus + 2 * gfxDegree * qnrLen) = 1; 

        
        sts = ippsGFPXElementGetSize(pGFPExtCtx, 
                                     &gfxElemSize);
        if (ippStsNoErr != sts) {break;}
        
        pQnrGFXE = SAFE_ALLOC (gfxElemSize);
        if(NULL == pQnrGFXE) {
            break;
        }
        
        sts = ippsGFPXElementInit(pQnrGFXE, 
                                  pQnr, 
                                  qnrLen, 
                                  pGFPExtCtx);
        if (ippStsNoErr != sts) {break;}
        
        sts = ippsGFPXNeg(pQnrGFXE, pQnrGFXE, pGFPExtCtx);
        if (ippStsNoErr != sts) {break;}

        sts = ippsGFPXGetElement(pQnrGFXE, pModulus, gfxElemLen, 
                                 pGFPExtCtx);
        if (ippStsNoErr != sts) {break;}
                
        sts = ippsGFPXQGetSize(pGFPExtCtx, &stateSize);
        if (ippStsNoErr != sts) {break;}
        if (buf) {
            // Make sure the user has provided sufficient memory for the context
            if (stateSize < bufSize) {
                break;
            }
            pState = (IppsGFPXQState *) buf;
        } else {
            // use dynamic memory allocation for the structure
            pState = (IppsGFPXQState *) SAFE_ALLOC(stateSize);
        }   
        
        sts = ippsGFPXQInit(pState, 
                            pGFPExtCtx, 
                            pModulus);
        if (ippStsNoErr != sts) {break;}
    } while (0);
    
    if (ippStsNoErr != sts) {
        if (!buf) {
            SAFE_FREE(pState);
        }
        pState = NULL;
    }

    SAFE_FREE(pQnrGFXE);
    SAFE_FREE(pModulus);
    
    return pState;
}

IppsGFPXQElement *createQuadFieldExtElement(const Ipp32u* pA, Ipp32u lenA,
                                            IppsGFPXQState* pGFPQExtCtx,
                                            void * buf, unsigned int bufSize)
{
    IppStatus sts = ippStsNoErr;
    IppsGFPXQElement * pElement = NULL;
    Ipp32u elementSize = 0;
    
    do {
        Ipp32u zero[] = {0};
        Ipp32u zLen = sizeof(zero)/sizeof(Ipp32u);
        sts = ippsGFPXQElementGetSize(pGFPQExtCtx, &elementSize);
        if (ippStsNoErr != sts) {break;}
        if (buf) {
            // Make sure the user has provided sufficient memory for the context
            if (elementSize < bufSize) {
                break;
            }
            pElement = (IppsGFPXQElement *) buf;
        } else {
            // use dynamic memory allocation for the structure
            pElement = (IppsGFPXQElement *) SAFE_ALLOC(elementSize);
            if (!pElement) {sts = ippStsErr; break;}
        }   
        //if null initializer just provide dummy data
        if (NULL == pA) {
            pA = zero;
            lenA = zLen;
        }
        sts = ippsGFPXQElementInit(pElement, pA, lenA, pGFPQExtCtx); 
        if (ippStsNoErr != sts) {break;}
    } while (0);

    if (ippStsNoErr != sts) {
        if (!buf) {
            SAFE_FREE(pElement);
        }
        pElement = NULL;
    }

    return pElement;
}

IppsTatePairingDE3State *createTatePairing(const IppsGFPECState* pG1,
                                           const IppsGFPXECState* pG2,
                                           const IppsGFPXQState* pGT,
                                           void * buf, unsigned int bufSize)
{
    IppStatus sts = ippStsNoErr;
    IppsTatePairingDE3State *pState = NULL;
    Ipp32u stateSize = 0;
    
    do {
        sts = ippsTatePairingDE3GetSize(pG1, pG2, &stateSize);
        if (ippStsNoErr != sts) {break;}
        if (buf) {
            // Make sure the user has provided sufficient memory for the context
            if (stateSize < bufSize) {
                break;
            }
            pState = (IppsTatePairingDE3State *) buf;
        } else {
            // use dynamic memory allocation for the structure
            pState = (IppsTatePairingDE3State *) SAFE_ALLOC(stateSize);
            if (!pState) {sts = ippStsErr; break;}
        }   
        
        sts = ippsTatePairingDE3Init(pState, pG1, pG2, pGT); 
        if (ippStsNoErr != sts) {break;}
    } while (0);
    if (ippStsNoErr != sts) {
        // we had a problem during init, free any allocated memory
        if (!buf) {
            SAFE_FREE(pState);
        }
        pState = NULL;
    }

    return pState;
}

