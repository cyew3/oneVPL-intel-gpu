/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#include "pavpsdk_defs.h"

#include <tchar.h>
#include <windows.h>
#include <Wincrypt.h>
#include <Bcrypt.h>
#include <vector>
#include <d3d9.h>
#include <intsafe.h>

#include "mscrypto.h"
#include "pavp_sample_common.h"

// definitions from opmapi.h
#define _OPM_OMAC_SIZE 16

//  Generate OMAC1 signature using AES128
#define AES_BLOCKLEN    (16)
#define AES_KEYSIZE_128 (16)

inline void LShift(const BYTE *lpbOpd, BYTE *lpbRes)
{
    for( DWORD i = 0; i < AES_BLOCKLEN; i++ )
    {
        lpbRes[i] = lpbOpd[i] << 1;
        if( i < AES_BLOCKLEN - 1 )
        {
            lpbRes[i] |= ( (unsigned char)lpbOpd[i+1] ) >> 7;
        }
    }
}


inline void XOR( 
    BYTE *lpbLHS, 
    const BYTE *lpbRHS, 
    DWORD cbSize = AES_BLOCKLEN 
    )
{
    for( DWORD i = 0; i < cbSize; i++ )
    {
        lpbLHS[i] ^= lpbRHS[i];
    }
}


HRESULT ComputeOMAC(
    BYTE AesKey[ 16 ],              // Session key
    PUCHAR pb,                      // Data
    DWORD cb,                       // Size of the data
    BYTE *pTag                      // Receives the OMAC
    )
{
    HRESULT hr = S_OK;
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_KEY_HANDLE hKey = NULL;
    DWORD cbKeyObject = 0;
    DWORD cbData = 0;
    PBYTE pbKeyObject = NULL;

    PUCHAR Key = (PUCHAR)AesKey;

    struct 
    {
        BCRYPT_KEY_DATA_BLOB_HEADER Header;
        UCHAR Key[AES_KEYSIZE_128];
    } KeyBlob;

    KeyBlob.Header.dwMagic = BCRYPT_KEY_DATA_BLOB_MAGIC;
    KeyBlob.Header.dwVersion = BCRYPT_KEY_DATA_BLOB_VERSION1;
    KeyBlob.Header.cbKeyData = AES_KEYSIZE_128;
    CopyMemory(KeyBlob.Key, Key, sizeof(KeyBlob.Key));

    BYTE rgbLU[_OPM_OMAC_SIZE];
    BYTE rgbLU_1[_OPM_OMAC_SIZE];
    BYTE rBuffer[_OPM_OMAC_SIZE];

    hr = BCryptOpenAlgorithmProvider(
        &hAlg, 
        BCRYPT_AES_ALGORITHM, 
        MS_PRIMITIVE_PROVIDER, 
        0
        );

    //  Get the size needed for the key data
    if (S_OK == hr) 
    {
        hr = BCryptGetProperty(
            hAlg, 
            BCRYPT_OBJECT_LENGTH, 
            (PBYTE)&cbKeyObject, 
            sizeof(DWORD), 
            &cbData, 
            0
            );
    }

    //  Allocate the key data object
    if (S_OK == hr) 
    {
        pbKeyObject = new (std::nothrow) BYTE[cbKeyObject];
        if (NULL == pbKeyObject) 
        {
            hr = E_OUTOFMEMORY;
        }
    }

    //  Set to CBC chain mode
    if (S_OK == hr) 
    {
        hr = BCryptSetProperty(
            hAlg, 
            BCRYPT_CHAINING_MODE, 
            (PBYTE)BCRYPT_CHAIN_MODE_CBC, 
            sizeof(BCRYPT_CHAIN_MODE_CBC), 
            0
            );
    }

    //  Set the key
    if (S_OK == hr) 
    {
        hr = BCryptImportKey(hAlg, NULL, BCRYPT_KEY_DATA_BLOB, &hKey, 
            pbKeyObject, cbKeyObject, (PUCHAR)&KeyBlob, sizeof(KeyBlob), 0);
    }

    //  Encrypt 0s
    if (S_OK == hr) 
    {
        DWORD cbBuffer = sizeof(rBuffer);
        ZeroMemory(rBuffer, sizeof(rBuffer));

        hr = BCryptEncrypt(hKey, rBuffer, cbBuffer, NULL, NULL, 0, 
            rBuffer, sizeof(rBuffer), &cbBuffer, 0);
    }

    //  Compute OMAC1 parameters
    if (S_OK == hr)
    {
        const BYTE bLU_ComputationConstant = 0x87;
        LPBYTE pbL = rBuffer;

        LShift( pbL, rgbLU );
        if( pbL[0] & 0x80 )
        {
            rgbLU[_OPM_OMAC_SIZE - 1] ^= bLU_ComputationConstant;
        }
        LShift( rgbLU, rgbLU_1 );
        if( rgbLU[0] & 0x80 )
        {
            rgbLU_1[_OPM_OMAC_SIZE - 1] ^= bLU_ComputationConstant;
        }
    }

    //  Generate the hash. 
    if (S_OK == hr) 
    {
        // Redo the key to restart the CBC.

        BCryptDestroyKey(hKey);
        hKey = NULL;

        hr = BCryptImportKey(hAlg, NULL, BCRYPT_KEY_DATA_BLOB, &hKey,
            pbKeyObject, cbKeyObject, (PUCHAR)&KeyBlob, sizeof(KeyBlob), 0);
    }

    if (S_OK == hr) 
    {
        PUCHAR pbDataInCur = pb;
        cbData = cb;
        do
        {
            DWORD cbBuffer = 0;

            if (cbData > _OPM_OMAC_SIZE) 
            {
                CopyMemory( rBuffer, pbDataInCur, _OPM_OMAC_SIZE );

                hr = BCryptEncrypt(hKey, rBuffer, sizeof(rBuffer), NULL, 
                    NULL, 0, rBuffer, sizeof(rBuffer), &cbBuffer, 0);

                pbDataInCur += _OPM_OMAC_SIZE;
                cbData -= _OPM_OMAC_SIZE;
            }
            else 
            {   
                if (cbData == _OPM_OMAC_SIZE)
                {
                    CopyMemory(rBuffer, pbDataInCur, _OPM_OMAC_SIZE);
                    XOR(rBuffer, rgbLU);
                }
                else 
                {
                    ZeroMemory( rBuffer, _OPM_OMAC_SIZE );
                    CopyMemory( rBuffer, pbDataInCur, cbData );
                    rBuffer[ cbData ] = 0x80;

                    XOR(rBuffer, rgbLU_1);
                }

                hr = BCryptEncrypt(hKey, rBuffer, sizeof(rBuffer), NULL, NULL, 
                    0, (PUCHAR)pTag, 16, &cbBuffer, 0);

                cbData = 0;
            }
                
        } while( S_OK == hr && cbData > 0 );
    }

    //  Clean up
    if (hKey)
    {
        BCryptDestroyKey(hKey);
    }
    if (hAlg)
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }
    delete [] pbKeyObject;
    return hr;
}


void ReverseMemCopy(BYTE *pbDest, BYTE const *pbSource, DWORD cb)
{
    for (DWORD i = 0; i < cb; i++) 
    {
        pbDest[cb - 1 - i] = pbSource[i];
    }
}


void CopyAndAdvancePtr(BYTE*& pDest, const BYTE* pSrc, DWORD cb)
{
    memcpy(pDest, pSrc, cb);
    pDest += cb;
}


typedef struct
{ 
    RSAPUBKEY rsapubkey; 
    BYTE modulus[1]; // rsapubkey.bitlen / 8
} PUBLIC_KEY_VALUES;


//------------------------------------------------------------------------
//
// ImportRsaPublicKey
//
// Converts an RSA public key from an RSAPUBKEY blob into an 
// BCRYPT_RSAKEY_BLOB and sets the public key on the CNG provider.
//
//------------------------------------------------------------------------

HRESULT ImportRsaPublicKey(
    BCRYPT_ALG_HANDLE hAlg,     // CNG provider
    PUBLIC_KEY_VALUES *pKey,    // Pointer to the RSAPUBKEY blob.
    BCRYPT_KEY_HANDLE *phKey    // Receives a handle the imported public key.
    )
{
    HRESULT hr = S_OK;

    BYTE *pbPublicKey = NULL;
    DWORD cbKey = 0;

    // Layout of the RSA public key blob:

    //  +----------------------------------------------------------------+
    //  |     BCRYPT_RSAKEY_BLOB    | BE( dwExp ) |   BE( Modulus )      |
    //  +----------------------------------------------------------------+
    //
    //  sizeof(BCRYPT_RSAKEY_BLOB)       cbExp           cbModulus 
    //  <--------------------------><------------><---------------------->
    //
    //   BE = Big Endian Format                                                     
 
    DWORD cbModulus = (pKey->rsapubkey.bitlen + 7) / 8;
    DWORD dwExp = pKey->rsapubkey.pubexp;
    DWORD cbExp = (dwExp & 0xFF000000) ? 4 :
                  (dwExp & 0x00FF0000) ? 3 :
                  (dwExp & 0x0000FF00) ? 2 : 1;

    BCRYPT_RSAKEY_BLOB *pRsaBlob;
    PBYTE pbCurrent;

    hr = DWordAdd(cbModulus, sizeof(BCRYPT_RSAKEY_BLOB), &cbKey);

    if (FAILED(hr))
    {
        goto done;
    }

    cbKey += cbExp;

    pbPublicKey = (BYTE*)CoTaskMemAlloc(cbKey);
    if (NULL == pbPublicKey) 
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }    
    
    ZeroMemory(pbPublicKey, cbKey);
    pRsaBlob = (BCRYPT_RSAKEY_BLOB *)(pbPublicKey);
    
    // Make the Public Key Blob Header
    pRsaBlob->Magic = BCRYPT_RSAPUBLIC_MAGIC;
    pRsaBlob->BitLength = pKey->rsapubkey.bitlen;
    pRsaBlob->cbPublicExp = cbExp;
    pRsaBlob->cbModulus = cbModulus;
    pRsaBlob->cbPrime1 = 0;
    pRsaBlob->cbPrime2 = 0;

    pbCurrent = (PBYTE)(pRsaBlob + 1);
    
    // Copy pubExp Big Endian 
    ReverseMemCopy(pbCurrent, (PBYTE)&dwExp, cbExp);
    pbCurrent += cbExp;

    // Copy Modulus Big Endian 
    ReverseMemCopy(pbCurrent, pKey->modulus, cbModulus);

    // Set the key.
    hr = BCryptImportKeyPair(
        hAlg, 
        NULL, 
        BCRYPT_RSAPUBLIC_BLOB, 
        phKey,
        (PUCHAR)pbPublicKey,
        cbKey,
        0
        );

done:
    CoTaskMemFree(pbPublicKey);
    return hr;
}


HRESULT RSAES_OAEP_Encrypt(
    BYTE *Cert,
    UINT CertLen,
    const BYTE *pbDataIn,
    DWORD cbDataIn,
    BYTE **pbDataOut,
    DWORD *cbDataOut)
{
    HRESULT hr = S_OK;
    PCCERT_CONTEXT              pCert = NULL;
    PCCERT_CONTEXT              pCertTmp = NULL;
    BYTE*                       pPublicKeyBlob = NULL;
    DWORD                       dwCertSize = 0;
//    D3DAUTHENTICATEDCHANNEL_CONFIGURE_OUTPUT  output;

    // Prepare Signature
    CRYPT_DATA_BLOB   message_BLOB;
    message_BLOB.pbData = &Cert[0];
    message_BLOB.cbData = CertLen;

    HCERTSTORE  hCertStore;
    hCertStore = CertOpenStore(
        CERT_STORE_PROV_PKCS7,
        PKCS_7_ASN_ENCODING,
        NULL,
        0,
        &message_BLOB);
    if (hCertStore == NULL)
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Error: Can't open cert store CERT_STORE_PROV_PKCS7, PKCS_7_ASN_ENCODING\n")));
        hr = E_FAIL;
        goto done;
    }

    // Find last certificate in the chain
    while ((pCertTmp = CertFindCertificateInStore(
        hCertStore,
        PKCS_7_ASN_ENCODING,
        1,
        CERT_FIND_ANY,
        NULL,
        pCert)) != NULL )
    {
        pCert = pCertTmp;
/* certificate verification is not implemented in the sample
        hr = verifyCertificate(pCert);
        if (hr != S_OK)
            break;
*/
    }
    if (hr != S_OK)
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Error: Certificate is invalid.\n")));
        goto done;
    }
    if (pCert == NULL)
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Error: Can't find certificate.\n")));
        hr = E_FAIL;
        goto done;
    }
    if (pCert->pCertInfo == NULL)
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Error: Can't find certificate info.\n")));
        hr = E_FAIL;
        goto done;
    }

    pPublicKeyBlob = NULL;

    if (!CryptDecodeObject(
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        RSA_CSP_PUBLICKEYBLOB,
        pCert->pCertInfo->SubjectPublicKeyInfo.PublicKey.pbData,
        pCert->pCertInfo->SubjectPublicKeyInfo.PublicKey.cbData,
        0,
        NULL,
        &dwCertSize))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Error: Can not get size for PublicKey encryption.\n")));
        hr = E_FAIL;
        goto done;
    }

    pPublicKeyBlob = new BYTE[dwCertSize];
    if(!pPublicKeyBlob)
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }

    if (!CryptDecodeObject(
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        RSA_CSP_PUBLICKEYBLOB,
        pCert->pCertInfo->SubjectPublicKeyInfo.PublicKey.pbData,
        pCert->pCertInfo->SubjectPublicKeyInfo.PublicKey.cbData,
        0,
        pPublicKeyBlob,
        &dwCertSize))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Error: Can not encrypt PublicKey for authenrication channel.\n")));
        hr = E_FAIL;
        goto done;
    }

    PUBLIC_KEY_VALUES *pKey = (PUBLIC_KEY_VALUES *)(pPublicKeyBlob + sizeof(BLOBHEADER));

    // Load and initialize a CNG provider (Cryptography API: Next Generation)
    BCRYPT_ALG_HANDLE hAlg = 0;

    hr = BCryptOpenAlgorithmProvider(
        &hAlg, 
        BCRYPT_RSA_ALGORITHM, 
        MS_PRIMITIVE_PROVIDER, 
        0
        );

    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed calling BCryptOpenAlgorithmProvider (hr=0x%08x)\n"), hr));
        goto done;
    }

    // Import the public key into the CNG provider.

    BCRYPT_KEY_HANDLE hPublicKey = 0;

    // Import the RSA public key.
    hr = ImportRsaPublicKey(hAlg, pKey, &hPublicKey);
    if (FAILED(hr))
        goto done;

    *cbDataOut = 0;
    DWORD cbOutput = 0;
    
    BCRYPT_OAEP_PADDING_INFO paddingInfo;
    ZeroMemory(&paddingInfo, sizeof(paddingInfo));

    paddingInfo.pszAlgId = BCRYPT_SHA512_ALGORITHM;

    //Encrypt the signature.
    hr = BCryptEncrypt(
        hPublicKey,
        (PUCHAR)pbDataIn,
        cbDataIn,
        &paddingInfo,
        NULL,
        0,
        NULL,
        0,
        &cbOutput,
        BCRYPT_PAD_OAEP
        );

    if (FAILED(hr))
    {
        goto done;
    }


    *pbDataOut = new BYTE[cbOutput];
    if (NULL == *pbDataOut) 
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    
    hr = BCryptEncrypt(
        hPublicKey,
        (PUCHAR)pbDataIn,
        cbDataIn,
        &paddingInfo,
        NULL,
        0,
        *pbDataOut,
        cbOutput,
        cbDataOut,
        BCRYPT_PAD_OAEP
        );

    if (FAILED(hr))
    {
        PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Failed calling BCryptEncrypt (hr=0x%08x)\n"), hr));
        goto done;
    }
done:
    if (FAILED(hr))
        PAVPSDK_SAFE_DELETE_ARRAY(*pbDataOut);
    PAVPSDK_SAFE_DELETE_ARRAY(pPublicKeyBlob);
    return hr;
}