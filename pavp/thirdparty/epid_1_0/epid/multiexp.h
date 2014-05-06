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
 * @brief This file defines functions that perform the "multiexp"
 * capability required in the EPID spec
 */

#ifndef _MULTIEXP_H
#define _MULTIEXP_H


#ifdef __cplusplus
extern "C"
{
#endif

#include "epid_errors.h"

#include "ippcp.h"


/** 
 * @brief A helper function used during signature computation and verification.
 *
 * It implements the multi-exponent algorithm for ECC or Prime Field
 * as defined in section 7.7 of the spec.
 *
 * @note This function isn't maximally efficient for just two items
 *       but the readability and maintainability of using one function
 *       for all 3 options seems worth it
 */
EPID_RESULT ecpfMultiExp(IppsGFPECState*   groupState,
                         IppsGFPECPoint*   P1, Ipp32u* b1, Ipp32u b1Len,
                         IppsGFPECPoint*   P2, Ipp32u* b2, Ipp32u b2Len,
                         IppsGFPECPoint*   P3, Ipp32u* b3, Ipp32u b3Len,
                         IppsGFPECPoint*   P4, Ipp32u* b4, Ipp32u b4Len,
                         IppsGFPECPoint*   result);


/** 
 * @brief A helper function used during signature computation and verification.
 *
 * It implements the multi-exponent algorithm for Quadratic Field Extensions 
 * as defined in section 7.7 of the spec.
 *
 * @note This function isn't maximally efficient for just two items
 *       but the readability and maintainability of using one function
 *       for all 3 options seems worth it
 */
EPID_RESULT qfeMultiExp(IppsGFPXQState*     groupState,
                        IppsGFPXQElement*   P1, Ipp32u* b1, Ipp32u b1Len,
                        IppsGFPXQElement*   P2, Ipp32u* b2, Ipp32u b2Len,
                        IppsGFPXQElement*   P3, Ipp32u* b3, Ipp32u b3Len,
                        IppsGFPXQElement*   P4, Ipp32u* b4, Ipp32u b4Len,
                        IppsGFPXQElement*   result);

#ifdef __cplusplus
}
#endif

#endif

