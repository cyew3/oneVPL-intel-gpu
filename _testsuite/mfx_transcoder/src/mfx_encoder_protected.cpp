/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_encoder_protected.h"

#ifdef PAVP_BUILD
mfxStatus MFXProtectedEncodeWRAPPER::InitBitstream(mfxBitstream *bs)
{
    MFX_CHECK_STS(MFXEncodeWRAPPER::InitBitstream(bs));

    if (0 != m_refParams.m_params.Protected)
    {
        mfxU64 nBufferSize = bs->MaxLength;

        // align unprotected buffer. We will use it for store decrypted data.
        nBufferSize = (nBufferSize+15)&~(0xf); 
        // encrypted buffer must be aligned to cipher block size. 
        mfxU64 encrypedDataMaxLength = (nBufferSize+15)&~(0xf); 
        // allocate for unprotected and two protected buffers in case we have 
        mfxU64 allocationSize = nBufferSize + 2 * (sizeof(mfxEncryptedData) + encrypedDataMaxLength);

        mfxU8 *p = NULL;
        MFX_CHECK_WITH_ERR(p = new mfxU8[allocationSize], MFX_ERR_MEMORY_ALLOC);

        memcpy(p, bs->Data, bs->MaxLength);
        bs->Data = p;
        p += nBufferSize;
        bs->MaxLength = nBufferSize;

        bs->EncryptedData = (mfxEncryptedData*)p;
        p += sizeof(mfxEncryptedData);
        memset(bs->EncryptedData, 0, sizeof(mfxEncryptedData));

        bs->EncryptedData->Data = p;
        p += encrypedDataMaxLength;
        bs->EncryptedData->MaxLength = encrypedDataMaxLength;

        bs->EncryptedData->Next = (mfxEncryptedData*)p;
        p += sizeof(mfxEncryptedData);
        memset(bs->EncryptedData->Next, 0, sizeof(mfxEncryptedData));

        bs->EncryptedData->Next->Data = (mfxU8*)p;
        p += encrypedDataMaxLength;
        bs->EncryptedData->Next->MaxLength = encrypedDataMaxLength;
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXProtectedEncodeWRAPPER::PutBsData(mfxBitstream* pData)
{
    if (0 != m_refParams.m_params.Protected && NULL != pData)
    {
        // for protected encoding data returned though encrypted data buffers
        if (NULL == pData->EncryptedData
            || 0 != pData->DataLength)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        mfxEncryptedData *cur = pData->EncryptedData;
        // offset from pData->Data pointer to write decrypted data
        mfxU32 offset = 0;
        while(NULL != cur && 0 != cur->DataLength)
        {
            // cur->DataLength is size after decryption but buffer required to do decryption must be aligned 16
            mfxU32 alignedSize = (cur->DataLength + 15)&(~0xf);
            // if assumption on buffer size required was wrong
            if (cur->MaxLength - offset < alignedSize)
                return MFX_ERR_UNKNOWN;
            // decrypt from EncryptedData buffers into regular bitstream buffer
            m_pavpVideo->Decrypt(
                cur->Data, 
                pData->Data + offset, 
                alignedSize,
                &cur->CipherCounter,
                sizeof(cur->CipherCounter));
            offset += cur->DataLength;
            cur->DataLength = 0;
            cur = cur->Next;
        }
        pData->DataLength = offset;
    }

    return MFXEncodeWRAPPER::PutBsData(pData);
}
#endif//PAVP_BUILD