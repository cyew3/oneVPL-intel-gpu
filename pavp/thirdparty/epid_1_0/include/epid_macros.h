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


/**\file
\brief This file contains various macros used in the EPID SIK
*/


#ifndef __EPID_MACROS_H__
#define __EPID_MACROS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <string.h>

/** 
@defgroup miscMacros Miscellaneous Macros
These macros are used in the SIK for various purposes
@{
*/

#ifndef FALSE
#define FALSE               0
#endif

#ifndef TRUE
#define TRUE                1
#endif

#ifndef NULL
#define NULL                0
#endif


// Useful defines goes here

#ifndef BITSIZE_WORD
#define BITSIZE_WORD(n) ((((n)+31)>>5))
#endif

#ifndef COUNT_OF
#define COUNT_OF(a) (sizeof(a)/sizeof((a)[0]))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#define SAFE_ALLOC(size) calloc(1,(size))
#define SAFE_FREE(ptr) {if (NULL != (ptr)) {free(ptr); (ptr)=NULL;}}


/*@}*/



/** 
@defgroup endiannessConversionMacros Endianness conversion Macros
These macros are using for ensuring 2 and 4 byte data items are processed
with the correct endianness.  They are useful on platforms with a Little Endian
microarchitecture since data items in EPID are serialized in Big Endian format
@{
*/

/* hexString <-> unsigned 32 bit integer conversion */
#define HSTRING_TO_U32(ptrByte)  \
    (((ptrByte)[0]) <<24)   \
    +(((ptrByte)[1]) <<16)   \
    +(((ptrByte)[2]) <<8)    \
    +((ptrByte)[3])
#define HSTRING_TO_U16(ptrByte)  \
    (((ptrByte)[0]) <<8)   \
    +((ptrByte)[1])
#define U32_TO_HSTRING(ptrByte, x)  \
    (ptrByte)[0] = (unsigned char)((x)>>24); \
    (ptrByte)[1] = (unsigned char)((x)>>16); \
    (ptrByte)[2] = (unsigned char)((x)>>8);  \
    (ptrByte)[3] = (unsigned char)(x)

#define U16_TO_HSTRING(ptrByte, x)  \
    (ptrByte)[0] = (unsigned char)((x)>>8); \
    (ptrByte)[1] = (unsigned char)(x)

#define U32_TO_HSTRING_I(x) (unsigned char)((x)>>24), (unsigned char)((x)>>16),\
    (unsigned char)((x)>>8), (unsigned char)(x)
#define U16_TO_HSTRING_I(x) (unsigned char)((x)>>8), (unsigned char)(x)


/*@}*/


/** 
@defgroup revocationListMacros Revocation List Macros
This module defines a set of C macros that are used to parse revocation lists.
Macros are used for this parsing instead of a C struct because unlike other
EPID items, revocation lists are not of a fixed length
@{
*/

//Macros useful for parsing key based revocation lists.  They are not of fixed
//length so we can't use  standard C structures as we do for other EPID items

// Pass zero to get length of an empty RL
#define GET_RL_REQUIRED_LENGTH(n)   ((RL_HEADER_SIZE) + (EPID_NUMBER_SIZE) * (n) + (ECDSA_SIGNATURE_SIZE))

// ptr for each of these macros MUST point to the beginning of the buffer containing the revocation list

// Following macros assume gid, RLver, n1 are little endian outside of the context of the revocation list buffer
// i.e. in the buffer itself, these items are big endian but in all places where they are being used in the SIK
//      to do some useful thing, they are little endian

#define SET_RL_SVER(ptr, sver)      U16_TO_HSTRING(((unsigned char *) (ptr) + RL_SVER_OFFSET), (sver)) 
#define GET_RL_SVER(ptr)            HSTRING_TO_U16(((unsigned char *) (ptr) + RL_SVER_OFFSET)) 

#define SET_RL_BLOBID(ptr, blobid)  U16_TO_HSTRING(((unsigned char *) (ptr) + RL_BLOBID_OFFSET), (blobid)) 
#define GET_RL_BLOBID(ptr)          HSTRING_TO_U16(((unsigned char *) (ptr) + RL_BLOBID_OFFSET)) 

#define SET_RL_GID(ptr, gid)        U32_TO_HSTRING(((unsigned char *) (ptr) + RL_GID_OFFSET), (gid)) 
#define GET_RL_GID(ptr)             HSTRING_TO_U32(((unsigned char *) (ptr) + RL_GID_OFFSET)) 

#define SET_RL_VER(ptr, ver)        U32_TO_HSTRING(((unsigned char *) (ptr) + RL_VER_OFFSET), (ver)) 
#define GET_RL_VER(ptr)             HSTRING_TO_U32(((unsigned char *) (ptr) + RL_VER_OFFSET)) 

#define SET_RL_N1(ptr, n1)          U32_TO_HSTRING(((unsigned char *) (ptr) + RL_N1_OFFSET),  (n1)) 
#define GET_RL_N1(ptr)              HSTRING_TO_U32(((unsigned char *) (ptr) + RL_N1_OFFSET)) 

#define GET_RL_SIGN_LENGTH(ptr)     ((RL_HEADER_SIZE) + ((EPID_NUMBER_SIZE) * (GET_RL_N1(ptr))))

#define GET_RL_LENGTH(ptr)          ((GET_RL_SIGN_LENGTH(ptr)) + (ECDSA_SIGNATURE_SIZE))

#define GET_RL_SIG_PTR(ptr)         (((unsigned char *) (ptr)) + (GET_RL_LENGTH(ptr)) - (ECDSA_SIGNATURE_SIZE))

#define GET_RL_F_PTR(ptr, n)        (((unsigned char *) (ptr)) + (RL_HEADER_SIZE) + (EPID_NUMBER_SIZE) * (n))


/*@}*/

#ifdef __cplusplus
}
#endif

#endif /*__EPID_MACROS_H__*/
/* End of file safeid_macros.h */
