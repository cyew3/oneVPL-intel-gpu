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
 * @brief This file defines interfaces for allocating and releasing
 * memory buffers for underlying data types
 */

#ifndef _MEMUTILS_H
#define _MEMUTILS_H


#ifdef __cplusplus
extern "C"
{
#endif

#include "ippcp.h"

/** 
 * @brief Create an finite field using raw input data.  Allocates
 * memory and assumes caller will free it when no longer needed
 */
IppsGFPXState* createFiniteField(IppsGFPState* pGFpCtx, 
                                 const Ipp32u degree,
                                 const Ipp32u* pCoeffs,
                                 const void * buf, 
                                 const unsigned int bufSize);

/** 
 * @brief Create a prime field using raw input data.  Allocates memory
 * and assumes caller will free it when no longer needed
 */
IppsGFPState* createPrimeField(const Ipp32u * pPrime, 
                               const Ipp32u bitSize, 
                               const void * buf, 
                               const unsigned int bufSize);

/** 
 * @brief Create an ECC prime field using raw input data.  Allocates
 * memory and assumes caller will free it when no longer needed
 */
IppsGFPECState *createECPrimeField(const Ipp32u bitSize,
                                   const Ipp32u* pA, const Ipp32u* pB,
                                   const Ipp32u* pX, const Ipp32u* pY,
                                   const Ipp32u* pQ, const Ipp32u* pOrder,
                                   const Ipp32u cofactor,
                                   IppsGFPState** ppGFpCtx,
                                   IppsGFPECPoint **pPoint,
                                   void * ecbuf, const unsigned int ecbufSize,
                                   void * buf, const unsigned int bufSize);

/** 
 * @brief Create an ECC over GFP point using raw input data.
 * Allocates memory and assumes caller will free it when no longer
 * needed
 */
IppsGFPECPoint *createGFPECPoint(IppsGFPECState *pEC,
                                 void * buf, const unsigned int bufSize);

/** 
 * @brief Create a GFP element using raw input data.  Allocates memory
 * and assumes caller will free it when no longer needed
 */
IppsGFPElement* createGFPElement(const Ipp32u *pData, const Ipp32u dataLen,
                                 IppsGFPState* pGF, 
                                 void * buf, unsigned int bufSize);

/** 
 * @brief Create an ECC finite field using raw input data.  Allocates
 * memory and assumes caller will free it when no longer needed
 */
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
                                     void * buf, const unsigned int bufSize);

/** 
 * @brief Create a finite field element using raw input data.
 * Allocates memory and assumes caller will free it when no longer
 * needed
 */
IppsGFPXElement* createFiniteFieldElement(IppsGFPXState* pGFPExtCtx, 
                                          const Ipp32u* pA, 
                                          Ipp32u lenA,
                                          const void * buf, 
                                          const unsigned int bufSize);

/** 
 * @brief Create a Quadratic Field Extension ECC point using raw input
 * data.  Allocates memory and assumes caller will free it when no
 * longer needed
 */
IppsGFPXECPoint *createGFPXECPoint(IppsGFPXECState *pEC,
                                   void * buf, const unsigned int bufSize);

/** 
 * @brief Create a Quadratic Field Extension using raw input data.
 * Allocates memory and assumes caller will free it when no longer
 * needed
 */
IppsGFPXQState *createQuadFieldExt(IppsGFPXState* pGFPExtCtx,
                                   const Ipp32u* pQnr, const unsigned int qnrLen,
                                   const void * buf, const unsigned int bufSize);

/** 
 * @brief Create a Quadratic Field Extension element using raw input
 * data.  Allocates memory and assumes caller will free it when no
 * longer needed
 */
IppsGFPXQElement *createQuadFieldExtElement(const Ipp32u* pA, Ipp32u lenA,
                                            IppsGFPXQState* pGFPQExtCtx,
                                            void * buf, unsigned int bufSize);

/** 
 * @brief Create a Tate Pairing based on structured input Allocates
 * memory and assumes caller will free it when no longer needed
 */
IppsTatePairingDE3State *createTatePairing(const IppsGFPECState* pG1,
                                           const IppsGFPXECState* pG2,
                                           const IppsGFPXQState* pGT,
                                           void * buf, unsigned int bufSize);

/// An enumerator used by createBigNum to determine if the data needs
/// to be byteswapped
typedef enum
{
    Little,
    Big
} DataEndianness;

/** 
 * @brief Creates an IPP BigNum structure and initializes it with data
 * if requested
 *
 * If data is NULL, then the structure will be initialized but will
 * not contain any data
 *
 * If buf is NULL, then dynamic memory allocation will be used to
 * allocate the context info it is the responsibility of the caller to
 * free the memory when it is no longer needed
 *
 * If buf is non-NULL, then it will be used to store the context.  If
 * the memory was statically allocated, then there is no need to free
 * it when it is no longer needed

 * @param[in] data        Pointer to the data to be stored in the bug num structure.  
 *                        Can be NULL
 * @param[in] dataSize    sizeof data (in bytes, not words)
 * @param[in] direction   Indicates if incoming data is little or big endian
 * @param[in] buf         Pointer to a buffer which will be used to store the
 *                        context.  If null, will use dynamic memory allocation.
 * @param[in] bufSize     sizeof buf.  If too small for context, function
 *                        will return NULL
 */
IppsBigNumState * createBigNum(void * data, int dataSize, 
                               DataEndianness direction, 
                               void * buf, int bufSize);

#ifdef __cplusplus
}
#endif

#endif /*__memutils_h__*/
/* End of file safeid_memory.h */
