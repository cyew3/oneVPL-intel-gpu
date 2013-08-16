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
\brief This file defines constants that are used in the EPID SIK
*/

#ifndef __EPID_CONSTANTS_H__
#define __EPID_CONSTANTS_H__

#ifdef __cplusplus
extern "C" {
#endif


/** 
@defgroup EPIDConstants EPID Constants
This module defines the constant values used for EPID data items.
@{
*/

#define EPID_GROUP_ID_SIZE_BITS  ( 32 )                                       ///< Number of bits in the EPID Group ID
#define EPID_SVER_SIZE_BITS      ( 16 )                                       ///< Number of bits in the EPID Version number
#define EPID_BLOBID_SIZE_BITS    ( 16 )                                       ///< Number of bits in the EPID Blob ID number
#define EPID_RLVER_SIZE_BITS     ( 32 )                                       ///< Number of bits in the EPID Revocation List version
#define EPID_RLVER_LEN_BITS      ( 32 )                                       ///< Number of bits in the EPID Revocation list length

#define EPID_NUMBER_SIZE_BITS    ( 256 )                                      ///< Number of bits in the EPID baseline big integer

#define EPID_G1ELM_SIZE_BITS    ( 512 )                                       ///< Number of bits an element of G1 
#define EPID_G2ELM_SIZE_BITS    ( 1536 )                                      ///< Number of bits an element of G2
#define EPID_G3ELM_SIZE_BITS    ( 512 )                                       ///< Number of bits an element of G3
#define EPID_GTELM_SIZE_BITS    ( 1536 )                                      ///< Number of bits an element of GT

#define EPID_SLEN                (80)                                         ///< EPID Security parameter

#define EPID_SMALL_INT_SIZE_BITS ( 32 )                                       ///< Number of bits in the EPID small integer
#define EPID_ND_SIZE_BITS        ( 80 )                                       ///< Number of bits in the EPID ND value in a signature
#define EPID_SF_SIZE_BITS        ( 600 )                                      ///< Number of bits in the EPID SF value in a signature
#define EPID_SF_MAX_SIZE_BITS    ( 593 )                                      ///< The "sf" value must never be larger than 2**593
#define EPID_ORDERG2_SIZE_BITS   ( 768 )                                      ///< Number of bits in the EPID OrderG2 value in Parameters
#define EPID_HASH_SIZE_BITS      ( 256 )                                      ///< Number of bits in the Message Digest 
#define EPID_RF_SIZE_BITS        ( EPID_NUMBER_SIZE_BITS + EPID_HASH_SIZE_BITS + EPID_SLEN ) ///< Size of the EPID RF value in bits

#define EPID_GROUP_ID_SIZE       ( EPID_GROUP_ID_SIZE_BITS / 8 )               ///< Size of the EPID Group ID
#define EPID_SVER_SIZE           ( EPID_SVER_SIZE_BITS / 8 )                   ///< Size of the EPID Version Number
#define EPID_BLOBID_SIZE         ( EPID_BLOBID_SIZE_BITS / 8 )                 ///< Size of the EPID Blob ID number
#define EPID_RLVER_SIZE          ( EPID_RLVER_SIZE_BITS / 8 )                  ///< Size of the EPID Revocation List version
#define EPID_RLVER_LEN_SIZE      ( EPID_RLVER_LEN_BITS / 8 )                   ///< Size of the EPID Revocation List length

#define EPID_NUMBER_SIZE         ( EPID_NUMBER_SIZE_BITS / 8 )                 ///< Size of the EPID baseline big integer
#define EPID_SMALL_INT_SIZE      ( EPID_SMALL_INT_SIZE_BITS / 8 )              ///< Size of the EPID small integer

#define EPID_G1ELM_SIZE          ( EPID_G1ELM_SIZE_BITS / 8 )                  ///< Size of EPID G1 element in bytes  
#define EPID_G2ELM_SIZE          ( EPID_G2ELM_SIZE_BITS / 8 )                  ///< Size of EPID G2 element in bytes  
#define EPID_G3ELM_SIZE          ( EPID_G3ELM_SIZE_BITS / 8 )                  ///< Size of EPID G3 element in bytes  
#define EPID_GTELM_SIZE          ( EPID_GTELM_SIZE_BITS / 8 )                  ///< Size of EPID GT element in bytes  


#define EPID_ND_SIZE             ( EPID_ND_SIZE_BITS / 8 )                     ///< Size of the EPID ND value in a signature
#define EPID_HASH_SIZE           ( EPID_HASH_SIZE_BITS / 8 )                   ///< Size of Message Digest in bytes

#define EPID_ORDERG2_SIZE        ( EPID_ORDERG2_SIZE_BITS / 8 )                ///< Size of the EPID OrderG2 value in Parameters

#define EPID_SF_SIZE             ( EPID_SF_SIZE_BITS / 8 )                     ///< Size of the EPID SF value in a signature
#define EPID_RF_SIZE             ( EPID_RF_SIZE_BITS / 8 )                     ///< Size of the EPID RF value

#define EPID_SVER                ( 1 )                                        ///< Version number of the EPID specification that is implemented in this version of the SIK

#define EPID_ND_SIZE_DWORD       (BITSIZE_WORD(EPID_ND_SIZE_BITS))             ///< Size of the EPID ND value to use in IPP structures (i.e. DWORD aligned)
#define EPID_SF_SIZE_DWORD       (BITSIZE_WORD(EPID_SF_SIZE_BITS))             ///< Size of the EPID sf value to use in IPP structures (i.e. DWORD aligned)


#define EPID_SEED_SIZE_BITS      ( 256 )                                      ///< Number of bits in the EPID seed key for compressed key implementations
#define EPID_SEED_SIZE           ( EPID_SEED_SIZE_BITS / 8 )                   ///< Size of the EPID seed for compressed key implementation

#define EPID_SIGNATURE_BLOB_SIZE_BITS (4520 + 16 + 16)                        ///< Number of bits in the signature blob
#define EPID_SIGNATURE_BLOB_SIZE      (EPID_SIGNATURE_BLOB_SIZE_BITS / 8)      ///< Size of the signature blob.  Used to verify compiler has packed the C structures correctly


#define RL_HEADER_SIZE               (16)                                    ///< size (in bytes) of the header of a key based revocation list
#define RL_SVER_OFFSET               (0)                                     ///< offset (in bytes) into a key based revocation list header where the SVER is contained
#define RL_BLOBID_OFFSET             (2)                                     ///< offset (in bytes) into a key based revocation list header where the BLOBID is contained
#define RL_GID_OFFSET                (4)                                     ///< offset (in bytes) into a key based revocation list header where the GID is contained
#define RL_VER_OFFSET                (8)                                     ///< offset (in bytes) into a key based revocation list header where the RL version is contained
#define RL_N1_OFFSET                 (12)                                    ///< offset (in bytes) into a key based revocation list header where the count of revoked keys is contained



/*@}*/


/** 
@defgroup ecDSAConstants ECDSA Constants
This module defines the ECDSA constant values required by the EPID protocol spec
@{
*/

#define ECC_PRIME_NUMBER_SIZE_BITS     ( 256 )                               ///< Number of bits in the ECC prime number
#define ECC_COEFFICIENT_SIZE_BITS      ( 256 )                               ///< Number of bits in the ECC coefficient
#define ECC_BASEPOINT_SIZE_BITS        ( 512 )                               ///< Number of bits in the ECC basepoint
#define ECC_BASEPOINT_ORDER_SIZE_BITS  ( 256 )                               ///< Number of bits in the ECC basepoint order
#define ECC_PUBLIC_KEY_SIZE_BITS       ( 512 )                               ///< Number of bits in the ECC public key
#define ECC_PRIVATE_KEY_SIZE_BITS      ( 256 )                               ///< Number of bits in the ECC private key

#define ECC_PRIME_NUMBER_SIZE          ( ECC_PRIME_NUMBER_SIZE_BITS / 8 )    ///< Size of the ECC prime number
#define ECC_COEFFICIENT_SIZE           ( ECC_COEFFICIENT_SIZE_BITS / 8 )     ///< Size of the ECC coefficient
#define ECC_BASEPOINT_SIZE             ( ECC_BASEPOINT_SIZE_BITS / 8 )       ///< Size of the ECC basepoint
#define ECC_BASEPOINT_ORDER_SIZE       ( ECC_BASEPOINT_ORDER_SIZE_BITS / 8 ) ///< Size of the ECC basepoint order
#define ECC_PUBLIC_KEY_SIZE            ( ECC_PUBLIC_KEY_SIZE_BITS / 8 )      ///< Size of the ECC public key
#define ECC_PRIVATE_KEY_SIZE           ( ECC_PRIVATE_KEY_SIZE_BITS / 8 )     ///< Size of the ECC private key

#define ECDSA_PUBLIC_KEY_SIZE_BITS     ( 512 )                               ///< Number of bits in a public key
#define ECDSA_PRIVATE_KEY_SIZE_BITS    ( 256 )                               ///< Number of bits in a private key
#define ECDSA_SIGNATURE_SIZE_BITS      ( 512 )                               ///< Number of bits in a digital signature

#define ECDSA_PUBLIC_KEY_SIZE          ( ECDSA_PUBLIC_KEY_SIZE_BITS / 8 )    ///< Size of a public key
#define ECDSA_PRIVATE_KEY_SIZE         ( ECDSA_PRIVATE_KEY_SIZE_BITS / 8 )   ///< Size of a private key
#define ECDSA_SIGNATURE_SIZE           ( ECDSA_SIGNATURE_SIZE_BITS / 8 )     ///< Size of a digital signature

/*@}*/



/** 
\brief Enumeration of all the data types to be serialized to/from persisent storage
*/
typedef enum {
    epidParametersCertificate = 0,        ///< EPID parameters as defined in EPID protocol spec
    epidReservedFutureUse_01        = 1,        ///< Reserving value 1 for future use
    epidGroupPrivateKey             = 2,        ///< Group private key as defined in EPID protocol spec
    epidMemberPrivateKey            = 3,        ///< Member Private Key as defined in EPID protocol spec
    epidCompressedPrivateKey        = 4,        ///< Compressed Member Private Key as defined in EPID protocol spec
    epidSignature             = 5,        ///< EPID signature created by member and sent to verifier
    epidPreComputedSignature  = 6,        ///< EPID pre-computed signature used in creating signature
    epidMemberPreComputationBlob    = 7,        ///< Member pre-computation blob as defined in EPID protocol spec
    epidVerifierPreComputationBlob  = 8,        ///< Verifier pre-computation blob as defined in EPID protocol spec
    epidRootVerificationKey         = 9,        ///< ECDSA public key used to verify signatures
    epidRootSigningKey              = 10,       ///< ECDSA private key used to create signatures
    epidReservedFutureUse_11        = 11,       ///< Reserving value 11 for future use
    epidGroupCertificate            = 12,       ///< Group certificate as defined in EPID protocol spec
    epidKeyBasedRevocationList      = 13,       ///< EPID key based revocation list
    epidSigBasedRevocationList      = 14,       ///< EPID signature based revocation list (not currently defined or implemented)

    // Following items are not defined as serialized items in the spec so are not assigned specific enum values
    epidPrngStateData,                          ///< State information for PRNG to be saved in a file so PRNG changes from run to run
    epidCurrentlyUsedGIDValues,                 ///< Array of big endian 4 byte integers
    epidAllowedBaseNames                        ///< Array of C strings
} EPIDSerializedObjectType;

#ifdef __cplusplus
}
#endif

#endif
