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
* @brief This file implements functions that perform the "multiexp"
* capability required in the EPID spec
*/


#include "multiexp.h"

#include "epid_errors.h"
#include "epid_constants.h"
#include "epid_macros.h"

#include "memutils.h"
#include "octstring.h"

#include "ippcp.h"


/// @brief get the index into the multiexp array
static int getTotal(Ipp32u *b1,   int len1, 
                    Ipp32u *b2,   int len2,
                    Ipp32u *b3,   int len3,
                    Ipp32u *b4,   int len4,
                    int     index)
{
    int total;

    int bit1, bit2, bit3, bit4;

    bit1  = getBit(b1, index, len1);
    bit2  = getBit(b2, index, len2);
    total = bit1 + 2*bit2;

    if (b3) {
        bit3   = getBit(b3, index, len3);
        total += 4*bit3;
    } 
    if (b4) {
        bit4   = getBit(b4, index, len4);
        total += 8*bit4;
    }

    return (total);
}

EPID_RESULT ecpfMultiExp(IppsGFPECState*groupState,
                         IppsGFPECPoint *P1, Ipp32u *b1, Ipp32u b1Len,
                         IppsGFPECPoint *P2, Ipp32u *b2, Ipp32u b2Len,
                         IppsGFPECPoint *P3, Ipp32u *b3, Ipp32u b3Len,
                         IppsGFPECPoint *P4, Ipp32u *b4, Ipp32u b4Len,
                         IppsGFPECPoint *result)
{
    EPID_RESULT retValue = EPID_FAILURE;
    IppStatus  sts;

    int actualLen1, actualLen2, actualLen3, actualLen4;
    int maxLen, total;
    int index;

    // Default to assuming only 2 items
    unsigned int usedInArray = 4;

    IppsGFPECPoint* t[16];

    memset((unsigned char *) t, 0, sizeof(t));

    do
    {
        if (P4) {
            usedInArray = 16;
        } else if (P3) {
            usedInArray = 8;
        }

        for (index = 0; index < usedInArray; index++) {
            t[index] = createGFPECPoint(groupState, NULL, 0);
            if (NULL == t[index]) {break;}
        }
        if (index != usedInArray) {break;}

        actualLen1   = getBitSize(b1, b1Len);
        actualLen2   = getBitSize(b2, b2Len);
        maxLen = (actualLen1 > actualLen2)   ? actualLen1 : actualLen2;

        if (b3) {
            actualLen3   = getBitSize(b3, b3Len);
            maxLen = (maxLen > actualLen3) ? maxLen : actualLen3;
        } 
        if (b4) {
            actualLen4   = getBitSize(b4, b4Len);
            maxLen = (maxLen > actualLen4) ? maxLen : actualLen4;
        }

        sts = ippsGFPECCpyPoint(P1, t[1], groupState);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPECCpyPoint(P2, t[2], groupState);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPECAddPoint(P1, P2, t[3], groupState);
        if (ippStsNoErr != sts) {break;}

        if (P3) {
            sts = ippsGFPECCpyPoint(P3, t[4], groupState);
            if (ippStsNoErr != sts) {break;}
            sts = ippsGFPECAddPoint(P1, P3, t[5], groupState);
            if (ippStsNoErr != sts) {break;}
            sts = ippsGFPECAddPoint(P2, P3, t[6], groupState);
            if (ippStsNoErr != sts) {break;}
            sts = ippsGFPECAddPoint(t[3], P3, t[7], groupState);
            if (ippStsNoErr != sts) {break;}
        }

        if (P4) {
            sts = ippsGFPECCpyPoint(P4, t[8], groupState);
            if (ippStsNoErr != sts) {break;}
            for (index = 9; index < 16; index++) {
                sts = ippsGFPECAddPoint(t[8], t[index-8], t[index], 
                    groupState);
                if (ippStsNoErr != sts) {break;}
            }
            if (ippStsNoErr != sts) {break;}
        }

        total = getTotal(b1, actualLen1, b2, actualLen2, 
            b3, actualLen3, b4, actualLen4, maxLen - 1);

        sts = ippsGFPECCpyPoint(t[total], result, groupState);
        if (ippStsNoErr != sts) {break;}

        for (index = 0; index <= maxLen - 2; index++) {
            sts = ippsGFPECAddPoint(result, result, result, groupState);
            if (ippStsNoErr != sts) {break;}

            total = getTotal(b1, actualLen1, b2, actualLen2, 
                b3, actualLen3, b4, actualLen4, (maxLen - 2) - index);

            if (total) {
                sts = ippsGFPECAddPoint(result, t[total], result, groupState);
                if (ippStsNoErr != sts) {break;}
            }
        }
        if (ippStsNoErr != sts) {break;}

        retValue = EPID_SUCCESS;
    } while (0);

    for (index = 0; index < usedInArray; index++) {
        SAFE_FREE(t[index]);
    }

    return (retValue);
}


EPID_RESULT qfeMultiExp(IppsGFPXQState  *groupState,
                        IppsGFPXQElement *P1, Ipp32u* b1, Ipp32u b1Len,
                        IppsGFPXQElement *P2, Ipp32u* b2, Ipp32u b2Len,
                        IppsGFPXQElement *P3, Ipp32u* b3, Ipp32u b3Len,
                        IppsGFPXQElement *P4, Ipp32u* b4, Ipp32u b4Len,
                        IppsGFPXQElement *result)
{
    EPID_RESULT retValue = EPID_FAILURE;
    IppStatus  sts;

    int actualLen1, actualLen2, actualLen3, actualLen4;
    int maxLen, total;
    int index;

    // Default to assuming only 2 items
    unsigned int usedInArray = 4;

    IppsGFPXQElement* t[16];

    memset((unsigned char *) t, 0, sizeof(t));

    do
    {
        if (P4) {
            usedInArray = 16;
        } else if (P3) {
            usedInArray = 8;
        }

        for (index = 0; index < usedInArray; index++) {
            t[index] = createQuadFieldExtElement(NULL, 0, groupState, 
                NULL, 0);
            if (NULL == t[index]) {break;}
        }
        if (index != usedInArray) {break;}

        actualLen1   = getBitSize(b1, b1Len);
        actualLen2   = getBitSize(b2, b2Len);
        maxLen = (actualLen1 > actualLen2)   ? actualLen1 : actualLen2;

        if (b3) {
            actualLen3   = getBitSize(b3, b3Len);
            maxLen = (maxLen > actualLen3) ? maxLen : actualLen3;
        } 
        if (b4) {
            actualLen4   = getBitSize(b4, b4Len);
            maxLen = (maxLen > actualLen4) ? maxLen : actualLen4;
        }

        sts = ippsGFPXQCpyElement(P1, t[1], groupState);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPXQCpyElement(P2, t[2], groupState);
        if (ippStsNoErr != sts) {break;}
        sts = ippsGFPXQMul(P1, P2, t[3], groupState);
        if (ippStsNoErr != sts) {break;}

        if (P3) {
            sts = ippsGFPXQCpyElement(P3, t[4], groupState);
            if (ippStsNoErr != sts) {break;}
            sts = ippsGFPXQMul(P1, P3, t[5], groupState);
            if (ippStsNoErr != sts) {break;}
            sts = ippsGFPXQMul(P2, P3, t[6], groupState);
            if (ippStsNoErr != sts) {break;}
            sts = ippsGFPXQMul(t[3], P3, t[7], groupState);
            if (ippStsNoErr != sts) {break;}
        }

        if (P4) {
            sts = ippsGFPXQCpyElement(P4, t[8], groupState);
            if (ippStsNoErr != sts) {break;}
            for (index = 9; index < 16; index++) {
                sts = ippsGFPXQMul(t[8], t[index-8], t[index], groupState);
                if (ippStsNoErr != sts) {break;}
            }
            if (ippStsNoErr != sts) {break;}
        }

        total = getTotal(b1, actualLen1, b2, actualLen2, 
            b3, actualLen3, b4, actualLen4, maxLen - 1);

        sts = ippsGFPXQCpyElement(t[total], result, groupState);
        if (ippStsNoErr != sts) {break;}

        for (index = 0; index <= maxLen - 2; index++) {
            sts = ippsGFPXQMul(result, result, result, groupState);
            if (ippStsNoErr != sts) {break;}

            total = getTotal(b1, actualLen1, b2, actualLen2, 
                b3, actualLen3, b4, actualLen4, (maxLen - 2) - index);

            if (total) {
                sts = ippsGFPXQMul(result, t[total], result, groupState);
                if (ippStsNoErr != sts) {break;}
            }
        }
        if (ippStsNoErr != sts) {break;}

        retValue = EPID_SUCCESS;
    } while (0);

    for (index = 0; index < usedInArray; index++) {
        SAFE_FREE(t[index]);
    }

    return (retValue);
}

