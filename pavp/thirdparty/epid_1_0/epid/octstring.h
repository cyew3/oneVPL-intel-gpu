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


#ifndef __OCTSTRING_H__
#define __OCTSTRING_H__

#ifdef __cplusplus
extern "C" {
#endif

/**\file
@brief This file defines interfaces for octet string conversion routines
*/

#include "ippcp.h"

///@brief convert from uint8* to uint32* while also doing a byte swap
IppStatus octStr2BNU (const Ipp8u* pStr, int strLen, Ipp32u* pBNU);

///@brief convert from uint32* to uint8* while also doing a byte swap
IppStatus BNU2OctStr (const Ipp32u* pBNU, Ipp8u* pStr, int strLen);

/** 
 * @brief determine the actual bitsize of a little endian big number.  
 * will return the bit index to the most significant non-zero bit
 */
int getBitSize(Ipp32u* num, Ipp32u numLen);

/** 
 * @brief Will return the bit value at the current bit index of a
 * little endian big number 
 */
int getBit(Ipp32u* num, Ipp32u bitIndex, Ipp32u bitLen);

///@brief Get the raw data of a GFP element (return is big endian)
IppStatus GetOctString_GFPElem(const IppsGFPElement* pGFPElement,
                          Ipp8u* pStr, int strLen,
                          const IppsGFPState* pGFpCtx);

///@brief Get the raw data of a GFP element (return is big endian)
IppStatus SetOctString_GFPElem(const Ipp8u* pStr, const int strLen,
                          IppsGFPElement* pGFPElement, 
                          IppsGFPState* pGFpCtx);

/** 
 * @brief Get the raw data of a X and Y GFP element of an ECC Point
 * (return is big endian)
 */
IppStatus GetOctString_GFPECPoint(const IppsGFPECPoint* pPoint, 
                             Ipp8u* pStr, int strLen,
                             IppsGFPECState* pEC);

/** 
 * @brief Set the X and Y GFP element of an ECC Point (input is big
 * endian)
 */
IppStatus SetOctString_GFPECPoint(const Ipp8u* pStr, const int strLen, 
                             IppsGFPECPoint* pPoint, 
                             IppsGFPECState *pEC);

/** 
 * @brief Get the raw data of a Quadratic Field Extension element
(return is big endian) */
IppStatus GetOctString_GFPXElem(const IppsGFPXElement* pGFPXElement,
                           Ipp8u* pStr, int strLen, 
                           const IppsGFPXState* pGFPExtCtx);

/** 
 * @brief Get the raw data of a Quadratic Field Extension element
 * (over a ECC Point) (return is big endian)
 */
IppStatus GetOctString_GFPXECPoint(const IppsGFPXECPoint* pPoint, 
                              Ipp8u* pStr, int strLen, 
                              IppsGFPXECState* pEC);

/** 
 * @brief Get the raw data of a second degree Quadratic Field
 * Extension element (return is big endian) 
 */
IppStatus GetOctString_GFPXQElem(const IppsGFPXQElement* pGFPXQElement, 
                            Ipp8u* pStr, int strLen,
                            const IppsGFPXQState* pGFPQExtCtx);

/** 
 * @brief Set the second degree Quadratic Field Extension element from
 * raw data (input is big endian) 
 */
IppStatus SetOctString_GFPXQElem(const Ipp8u* pStr, const int strLen,
                            IppsGFPXQElement* pGFPXQElement, 
                            IppsGFPXQState* pGFPQExtCtx);


#ifdef __cplusplus
}
#endif

#endif






