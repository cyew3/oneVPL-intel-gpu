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
 * @brief This file defines common utlity functions for signing
 */

#include "ippcp.h"
#include "context.h"

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
                          );

EPID_RESULT epidGFPECHash(const Ipp8u* msg, const Ipp32u msgSize, 
                          IppsGFPECPoint* pR, IppsGFPECState* pEC);
