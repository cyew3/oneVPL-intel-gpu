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
 * @brief This file defines interfaces to print routines for math
 * objects
 */

#include "ippcp.h"

///@brief print a byte string
void PrintOctString(const Ipp8u* pStr, const int strLen, const int dir);

///@brief print a prime field
void PrintPrimeField(const IppsGFPState* pGF);

///@brief print a prime field element
void PrintGFPElement(const IppsGFPElement* pA, const IppsGFPState* pGF);

///@brief print an ECC prime field
void PrintECPrimeField(const IppsGFPECState *pEC);

///@brief print an ECC point
void PrintECPoint(const IppsGFPECPoint* pPoint, IppsGFPECState *pEC); 

///@brief print a finite field
void PrintFiniteField(const IppsGFPXState* pGFPExtCtx);

///@brief print a quadratic field extension element
void PrintGFPXElement(const IppsGFPXElement* pA, const IppsGFPXState* pGF);

///@brief print an ECC finite field
void PrintECFiniteField(const IppsGFPXECState *pEC); 

///@brief print a quadratic field extension ECC point
void PrintXECPoint(const IppsGFPXECPoint* pPoint, IppsGFPXECState *pEC);

///@brief print a quadratic field 
void PrintQuadField(const IppsGFPXQState* pGF);

///@brief print a second degree quadratic field extension element
void PrintGFPXQElement(const IppsGFPXQElement* pA, const IppsGFPXQState* pGF);
