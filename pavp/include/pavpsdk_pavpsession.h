/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef _PAVPSDK_PAVPSESSION_H
#define _PAVPSDK_PAVPSESSION_H

//#include "intel_pavp_epid_api.h"
#include "pavpsdk_defs.h"

/**
@defgroup PAVPSession PAVP session management.
*/

/**@ingroup PAVPSession 
@{
*/

/** Interface for CP device accepts commands for execution.
Provdes methods for device both key exchange and general purpose functions
execution.
@ingroup PAVPSession
*/
class CCPDevice
{
public:
    virtual ~CCPDevice() {};
    /** Execute device function.
    @param funcID Defines function to execute.
    @param dataIn Pointer to the buffer to execute.
    @param sizeIn dataIn buffer size in bytes.
    @param dataOut Pointer to the buffer recieving execute result.
    @param sizeOut dataOut buffer size in bytes.
    */
    virtual PavpEpidStatus ExecuteFunc(
        uint32 funcID,
        const void *dataIn,
        uint32 sizeIn,
        void *dataOut,
        uint32 sizeOut) = 0;
    
    /** Execute Key Exchange command.
    Input and output buffers must begin from header
    typedef struct _PAVPCmdHdr 
    {
        uint32    ApiVersion;
        uint32    CommandId;
        uint32    Status;
        uint32    BufferLength;
    } PAVPCmdHdr;
    followed by command buffer of BufferLength bytes.
    
    @param dataIn Pointer to the buffer with command to execute.
    @param sizeIn dataIn buffer size in bytes.
    @param dataOut Pointer to the buffer recieving command execute result.
    @param sizeOut dataOut buffer size in bytes.
    */
    virtual PavpEpidStatus ExecuteKeyExchange(
        const void *dataIn,
        uint32 sizeIn,
        void *dataOut,
        uint32 sizeOut) = 0;
};

/** Base class for PAVP session management.
Used for managing PAVP session estabilished with the driver or HW. 
Session open and close are implementation specific. The main operations 
PAVP session provides are encrypt, decrypt and sign with session keys.
All custom operations for PAVP session shoudl be executed with CCPDevice 
pointer returned by GetCPDevice() function. All PAVP sessions have a 
properties can be requested usign GetSessionId() and GetActualAPIVersion() 
functions.
@ingroup PAVPSession
*/
class CPAVPSession
{
public:
    /// Creates CP session with driver.
    CPAVPSession() {};
    virtual ~CPAVPSession() {};

    /// Get CP commands execution interface in use. 
    virtual CCPDevice *GetCPDevice() = 0;

    /// Get CP session ID
    virtual uint32 GetSessionId() = 0;

    /// Get actual PAVP API version supported by HW.
    virtual uint32 GetActualAPIVersion() = 0;

    /** Encrypt with session key.
    @param[in] src Pointer to the buffer to encrypt.
    @param[out] dst Pointer to the buffer recieving encrypted data.
    @param[in] size Size in bytes of src and dst buffers. Must be 16 bytes aligned.

    @return PAVP_STATUS_SUCCESS No error.
    @return PAVP_STATUS_BAD_POINTER Null pointer.
    @return PAVP_STATUS_LENGTH_ERROR Buffer size is wrong.
    */
    virtual PavpEpidStatus Encrypt(const uint8* src, uint8* dst, uint32 size) = 0;
    
    /** Decrypt with session key.
    @param[in] src Pointer to the buffer to decrypt.
    @param[out] dst Pointer to the buffer recieving decrypted data.
    @param[in] size Size in bytes of src and dst buffers. Must be 16 bytes aligned.

    @return PAVP_STATUS_SUCCESS No error.
    @return PAVP_STATUS_BAD_POINTER Null pointer.
    @return PAVP_STATUS_LENGTH_ERROR Buffer size is wrong.
    */
    virtual PavpEpidStatus Decrypt(const uint8* src, uint8* dst, uint32 size) = 0;

    /** Sign with session signing key.
    @param[in] msg Pointer to the buffer to decrypt.
    @param[out] msgSize Pointer to the buffer recieving decrypted data.
    @param[in] signature Size in bytes of src and dst buffers. Must be 16 bytes aligned.

    @return PAVP_STATUS_SUCCESS No error.
    @return PAVP_STATUS_BAD_POINTER Null pointer.
    @return PAVP_STATUS_LENGTH_ERROR Buffer size is wrong.
    */
    virtual PavpEpidStatus Sign(const uint8* msg, uint32 msgSize, uint8* signature) = 0;

    /// Close the session.
    virtual PavpEpidStatus Close() = 0; 
};

/** @} */ //@ingroup PAVPSession 

#endif//_PAVPSDK_PAVPSESSION_H