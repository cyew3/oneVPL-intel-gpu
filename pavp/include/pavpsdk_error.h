/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/

#ifndef _PAVPSDK_ERROR_H
#define _PAVPSDK_ERROR_H

#include "intel_pavp_error.h"

    // PAVP Device Status Codes
    // Internal errors:
#define PAVP_STATUS_BAD_POINTER                        0x5000 
#define PAVP_STATUS_NOT_SUPPORTED                    0x5001 // PAVP is not supported
#define PAVP_STATUS_CRYPTO_DATA_GEN_ERROR            0x5002 // An error happened in the Crypto Data Gen lib
#define PAVP_STATUS_MEMORY_ALLOCATION_ERROR            0x5003 
#define PAVP_STATUS_REFRESH_REQUIRED_ERROR            0x5004 
#define PAVP_STATUS_LENGTH_ERROR                    0x5005 
#define PAVP_STATUS_UNDEFINED_BEHAVIOR              0x5006
    // Device state errors:
#define PAVP_STATUS_PAVP_DEVICE_NOT_INITIALIZED        0x6000 // Cannot perform this function without first initializing the device
#define PAVP_STATUS_PAVP_SERVICE_NOT_CREATED        0x6001 // Cannot perform this function without first creating PAVP service
#define PAVP_STATUS_PAVP_KEY_NOT_EXCHANGED            0x6002 // Cannot perform this function without first doing a key exchange
#define PAVP_STATUS_PAVP_INVALID_SESSION            0x6003 //Invalid session
#define PAVP_STATUS_INVALID_KEY_EX_DAA              0x6004 //DAA selected but not supported
#define PAVP_STATUS_MEI_INIT                        0x6005 //PAVP DLL was already initialized DLL -> MEI
#define PAVP_STATUS_MEI_LOAD_FAIL                   0x6006 //Failed to load the PAVP DLL
#define PAVP_STATUS_MEI_FUNCTION_LOAD_FAIL          0x6007 //Failed to get PAVP DLL functions
#define PAVP_STATUS_MEI_INIT_FAIL                   0x6008 //PAVP DLL function PavpInit failed
#define PAVP_STATUS_INVALID_GUID                    0x6009 //Invalid acceleration GUID
#define PAVP_STATUS_TERMINATE_SESSION_FAIL          0x600a //Terminating session failed
#define PAVP_STATUS_LOCK_SURFACE_RW_FAIL            0x600b //Failed to lock output surface for read/write
#define PAVP_STATUS_FAILED_PROGRAM_KEY              0x600c //Failed to program keys on behalf of ME
#define PAVP_STATUS_FAILED_PLANE_ENABLE             0x600d //Failed to set plane enable
     // Key exchange protocol errors:
#define PAVP_STATUS_KEY_EXCHANGE_TYPE_NOT_SUPPORTED 0x7000 // An invalid key exchange type was used
#define PAVP_STATUS_PAVP_SERVICE_CREATE_ERROR        0x7001 // A driver error occured when creating the PAVP service
#define PAVP_STATUS_GET_PUBKEY_FAILED                0x7002 // Failed to get g^a from PCH but no detailed error was given
#define PAVP_STATUS_DERIVE_SIGMA_KEYS_FAILED        0x7003 // Sigma keys could not be derived
#define PAVP_STATUS_CERTIFICATE_EXCHANGE_FAILED        0x7004 // Could not exchange certificates with the PCH but no detailed error was given
#define PAVP_STATUS_PCH_HMAC_INVALID                0x7005 // The PCH's HMAC was invalid
#define PAVP_STATUS_PCH_CERTIFICATE_INVALID            0x7006 // The PCH's certificate was not valid
#define PAVP_STATUS_PCH_EPID_SIGNATURE_INVALID        0x7007 // The PCH's EPID signature was invalid
#define PAVP_STATUS_GET_STREAM_KEY_FAILED            0x7008 // Failed to get a stream key from the PCH but no detailed error was given
#define PAVP_STATUS_GET_CONNSTATE_FAILED            0x7009 // Failed to get a connection state value from the hardware
#define PAVP_STATUS_GET_CAPS_FAILED                    0x7010 // Failed to get PAVP capabilities from the driver
#define PAVP_STATUS_GET_FRESHNESS_FAILED            0x7011 // Failed to get a key freshness value from the hardware
#define PAVP_STATUS_SET_FRESHNESS_FAILED            0x7012 // Failed to set the key freshness value
#define PAVP_STATUS_SET_STREAM_KEY_FAILED            0x7013 // Failed to update the stream key
#define PAVP_STATUS_SET_PROTECTED_MEM_FAILED        0x7014 // Failed to set protected memory on/off
#define PAVP_STATUS_SET_PLANE_ENABLE_FAILED            0x7015 // Failed to set plane enables
#define PAVP_STATUS_SET_WINDOW_POSITION_FAILED        0x7016 // Failed to set the window position
#define PAVP_STATUS_AES_DECRYPT_FAILED                0x7017 // Failed to decrypt  
#define PAVP_STATUS_AES_ENCRYPT_FAILED                0x7018 // Failed to encrypt  
#define PAVP_STATUS_LEGACY_KEY_EXCHANGE_FAILED        0x7019 // Legacy key exchange call failed
#define PAVP_STATUS_INVALID_NONCE_RETURNED            0x701A // Decrypted nonce value was incorrect
#define PAVP_STATUS_INVALID_MEMORY_STATUS            0x701B // Decrypted memory status was invalid
#define PAVP_STATUS_API_UNSUPPORTED                    0x701C // The call is not supported with the API in use
#define PAVP_STATUS_WRONG_SESSION_TYPE                0x701D // The call is not supported for the session type in use
#define PAVP_STATUS_GET_HANDLE_FAILED                0x701E // Get PAVP handle failed
#define PAVP_STATUS_GET_PCH_CAPS_FAILED                0x701F // Get PAVP PCH capabilities failed
#define PAVP_STATUS_INVALID_CHANNEL                 0x7020 //Unsuccess to create Authenticated Channel
#define PAVP_STATUS_CAP_NOT_SUPPORTED               0x7021 //Requested capability is not Supported

// General errors
#define PAVP_STATUS_ZERO_GUID                       0x8000 //Zero GUID negotiation failed
#define PAVP_STATUS_INVALID_SESSION_TYPE            0x8001 //Invalid session type - decode, transcode
#define PAVP_STATUS_PAVP_DEV_NOT_AVAIL              0x8002 //Checks if the PAVP device is available in the current platform

#endif//_PAVPSDK_ERROR_H