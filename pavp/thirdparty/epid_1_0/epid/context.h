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
 * @brief This file defines an interface of functions for initializing
 * internal context structures
 */

#ifndef __context_h__
#define __context_h__

#ifdef __cplusplus
extern "C" {
#endif

#include "epid_constants.h"
#include "epid_types.h"
#include "epid_errors.h"
#include "epid_macros.h"
#include "octstring.h"
#include "ippcp.h"


typedef struct EPIDParameters
{
    IppsGFPECState*          G1; // from a12, b12, g1.x, g1.y, p12, h1, Fq12
    
    IppsGFPXECState*         G2; // from ...
   
    IppsGFPECState*          G3; // from a3, b3, g1.x, g1.y, p3, h3, Fq3

    IppsGFPXQState*          GT; // from Fq coeff qnr
    
    
    IppsTatePairingDE3State* tp;
    
    IppsGFPECPoint*          g1;
    IppsGFPXECPoint*         g2;
    IppsGFPECPoint*          g3;

    Ipp32u p12[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u q12[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u h1;
    Ipp32u a12[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u b12[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    // extra room for high order coefficent for d = 3 included
    Ipp32u coeffs[4*BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];

    // extra room for high order coefficent included
    Ipp32u qnr[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];

    Ipp32u orderG2[3*BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];

    Ipp32u p3[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u q3[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u h3;
    Ipp32u a3[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u b3[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];

    Ipp32u g1x[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u g1y[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u g2x[3*BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u g2y[3*BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];

    Ipp32u g3x[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u g3y[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
 
    IppsGFPState* Fq12; //PF used to generate G1,G2
    IppsGFPXState* Fqd; //FF used to generate G2
    IppsGFPState* Fq3; //PF used to generate G3

    IppsGFPState* Fp12; //PF based on p12
    IppsGFPState* Fp3;  //PF based on p3
    
} EPIDParameters;

EPID_RESULT 
epidParametersContext_init(const EPIDParameterCertificateBlob *paramsCert,
                           EPIDParameters *ctx);

void epidParametersContext_destroy(EPIDParameters *ctx);


void epidParametersContext_display(EPIDParameters *ctx);


typedef struct EPIDGroup
{
    unsigned char            gid[EPID_GROUP_ID_SIZE];

    IppsGFPECPoint*          h1;
    IppsGFPECPoint*          h2;
    IppsGFPXECPoint*         w;
 
    IppsGFPElement*          h1x;
    IppsGFPElement*          h1y;

    IppsGFPElement*          h2x;
    IppsGFPElement*          h2y;

    IppsGFPXElement*         wx;
    IppsGFPXElement*         wy;

    Ipp32u                   h1xData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u                   h1yData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u                   h2xData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u                   h2yData[BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];

    Ipp32u                   wxData[3*BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
    Ipp32u                   wyData[3*BITSIZE_WORD(EPID_NUMBER_SIZE_BITS)];
} EPIDGroup;


EPID_RESULT 
epidGroupContext_init(const EPIDGroupCertificateBlob * groupCert,
                      EPIDParameters *params,
                      EPIDGroup *ctx);

void epidGroupContext_destroy(EPIDGroup *ctx);
    

#ifdef __cplusplus
}
#endif


#endif /*__context_h__*/
/* End of file context.h */
