//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010-2013 Intel Corporation. All Rights Reserved.
//
//
//*/

#ifndef __PCP_H__
#define __PCP_H__

#if defined(_WIN32) || defined(_WIN64)
#include <d3d9.h>
#include <dxva.h>
#pragma warning(disable : 4201)
#include <dxva2api.h>
#pragma warning(default : 4201)
#endif

#include "mfxvideo.h"
#include "mfxpcp.h"

#define IS_PROTECTION_PAVP_ANY(val) (MFX_PROTECTION_PAVP == (val) || MFX_PROTECTION_GPUCP_PAVP == (val))
#define IS_PROTECTION_ANY(val) (IS_PROTECTION_PAVP_ANY(val) || MFX_PROTECTION_GPUCP_AES128_CTR == (val))
#define IS_PROTECTION_GPUCP_ANY(val) (MFX_PROTECTION_GPUCP_PAVP == (val) || MFX_PROTECTION_GPUCP_AES128_CTR == (val))

/**
\defgroup pcp Bit stream preparation
*/
//@{
/** Structure with encryption interface.
    It allows hide what implementation of encryption we use.
*/
typedef struct {
    mfxHDL      pthis; ///< Encryptor context handle

    /** Encrypt input buffer and return cipherCounter was used.
    If destination buffer is NULL then return only
    mfxAES128CipherCounter to be used not incrementing counter inside.

    @param[in] pthis Encryptor context handle.
    @param[in] src Pointer to source buffer.
    @param[in] dst Pointer to destination buffer.
    @param[in] nbytes Size of source and destination buffers.
    @param[out] cipherCounter Pointer to recieve value for
        mfxEncryptedData::ChipherCounter.

    @return MFX_ERR_NULL_PTR One or more input pointers have unexpected NULL value.
    @return MFX_WRN_COUNTER_OUT_OF_RANGE Counter rollover to 0 happened or will
        happen next time dst not NULL.
    @return MFX_ERR_UNKNOWN Bad value of one or more arguments.
    */
    mfxStatus   (*Encrypt)(mfxHDL pthis, mfxU8 *src, mfxU8 *dst, mfxU32 nbytes,
        mfxAES128CipherCounter *cipherCounter);

    mfxStatus   (*Decrypt)(mfxHDL pthis, mfxU8 *src, mfxU8 *dst, mfxU32 nbytes,
        const mfxAES128CipherCounter& cipherCounter);
} pcpEncryptor;

//! Protected session structure
typedef struct _pcpSession *pcpSession;

/** Initialize PCP video decoder.
@param[out] sessionPCP Pointer to recieve PCP SDK session handle.
@param[in] codecId Coding standard to be used.
@param[in] encryptor context with encryption function
@param[in] all slices should be encrypted or not

@return MFX_ERR_NULL_PTR One or more input pointers have unexpected NULL value.
@return MFX_ERR_MEMORY_ALLOC Error allocating memory.
*/
mfxStatus PCPVideoDecode_Init(pcpSession *sessionPCP, mfxU32 codecId,
                              pcpEncryptor *encryptor, bool isFullEncrypt);

mfxStatus PCPVideoDecode_SetEncryptor(pcpSession sessionPCP, const pcpEncryptor *encryptor);

/** Destroys PCP video decoder initialized.
@param[in] sessionPCP PCP SDK session handle

@return MFX_ERR_NULL_PTR One or more input pointers have unexpected NULL value.
*/
mfxStatus PCPVideoDecode_Close(pcpSession sessionPCP);

/** Prapares video stream data MediaSDK processing in protected mode.
Parse input video stream and prepares output bitstream. Encrypt data using
pcpEncryptor set on PCPVideoDecode_Init call.

out->EncryptedData pointer is initialized with internal PCP session
buffers and must not be used after next call to this function.

@param[in] sessionPCP PCP SDK session handle to create
@param[in] in Input video stream
@param[out] out Pointer recieve internal bitstream pointer for prepared video
    stream. Caller is responsible to utilize all bitstream data between the
    calls as internal bitstream will be reused for next call to this function.
@param[in] isSparse Should we pack slices without gaps before encoding or should
    we put them the same way as in input stream.

@return MFX_ERR_NULL_PTR One or more input pointers have unexpected NULL value.
@return MFX_ERR_MORE_DATA Expect more data at input.
@return MFX_ERR_NOT_ENOUGH_BUFFER Output buffer is not enough.
@return MFX_ERR_UNSUPPORTED Output bitsream is not empty.
@return MFX_ERR_ABORTED Operation aborted due to insufficient AES counter range.
*/
mfxStatus PCPVideoDecode_PrepareNextFrame(pcpSession sessionPCP,
    mfxBitstream *in,
    mfxBitstream **out,
    bool isSparse);
//@}
#endif //__PCP_H__