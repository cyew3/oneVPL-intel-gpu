/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#include "pavpsdk_defs.h"

#include "pavpsdk_sigma.h"
#include "ippcp.h"
#include <memory.h>
#include "sigma_crypto_utils.h"
#include "intel_pavp_epid_api.h"

#include "epid_errors.h"
#include "epid_types.h"
#include "epid_macros.h"
#include "epid_verifier.h"

#include <windows.h> //SecureZeroMemory

uint8 EpidParams[] = { 
0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x89, 0x57, 0x3f, 0x17, 0x47, 0x30, 0x8c, 0x43, 0xd5, 0xee, 
0x41, 0x97, 0x96, 0x19, 0x72, 0xbb, 0x86, 0x88, 0xed, 0x4b, 0xef, 0x04, 0xab, 0xae, 0xc3, 0x8e, 
0xec, 0x51, 0xc3, 0xd3, 0x09, 0xf9, 0x24, 0xe5, 0xd9, 0xbc, 0x67, 0x7f, 0x81, 0x0d, 0xf0, 0x25, 
0x58, 0xf7, 0x53, 0x13, 0xa9, 0x8a, 0xa6, 0x10, 0x47, 0x65, 0x5d, 0x73, 0x9e, 0xf1, 0x94, 0xeb, 
0x05, 0xb1, 0xa7, 0x11, 0x00, 0x00, 0x12, 0x97, 0x05, 0x53, 0xd7, 0xc8, 0x81, 0xf7, 0x78, 0xc2, 
0x2c, 0x37, 0xb6, 0xc0, 0x16, 0x3e, 0x68, 0x24, 0x3a, 0x84, 0x78, 0x1c, 0x0a, 0xdf, 0x9b, 0xb3, 
0xed, 0x21, 0xc4, 0x46, 0xe5, 0xa7, 0xa3, 0x92, 0x00, 0x3a, 0x2e, 0x39, 0x0e, 0x10, 0xd8, 0xac, 
0x47, 0xcb, 0x29, 0xc8, 0xf1, 0x2c, 0x7f, 0x11, 0x99, 0x2a, 0x18, 0xb7, 0xef, 0x73, 0x48, 0xa6, 
0xbe, 0x70, 0xa6, 0x8b, 0x97, 0x34, 0x8a, 0xb1, 0x02, 0x16, 0x7a, 0x61, 0x53, 0xdd, 0xf6, 0xe2, 
0x89, 0x15, 0xa0, 0x94, 0xf1, 0xb5, 0xdc, 0x65, 0x21, 0x15, 0x62, 0xe1, 0x7d, 0xc5, 0x43, 0x89, 
0xee, 0xb4, 0xef, 0xc8, 0xa0, 0x8e, 0x34, 0x0f, 0x04, 0x82, 0x27, 0xe1, 0xeb, 0x98, 0x64, 0xc2, 
0x8d, 0x8f, 0xdd, 0x0e, 0x82, 0x40, 0xae, 0xd4, 0x31, 0x63, 0xd6, 0x46, 0x32, 0x16, 0x85, 0x7a, 
0xb7, 0x18, 0x68, 0xb8, 0x17, 0x02, 0x81, 0xa6, 0x06, 0x20, 0x76, 0xe8, 0x54, 0x54, 0x53, 0xb4, 
0xa9, 0xd8, 0x44, 0x4b, 0xaa, 0xfb, 0x1c, 0xfd, 0xae, 0x15, 0xca, 0x29, 0x79, 0xa6, 0x24, 0xa4, 
0x0a, 0xf6, 0x1e, 0xac, 0xed, 0xfb, 0x10, 0x41, 0x08, 0x66, 0xa7, 0x67, 0x36, 0x6e, 0x62, 0x71, 
0xb7, 0xa6, 0x52, 0x94, 0x8f, 0xfb, 0x25, 0x9e, 0xe6, 0x4f, 0x25, 0xe5, 0x26, 0x9a, 0x2b, 0x6e, 
0x7e, 0xf8, 0xa6, 0x39, 0xae, 0x46, 0xaa, 0x24, 0x00, 0x03, 0xdf, 0xfc, 0xbe, 0x2f, 0x5c, 0x2e, 
0x45, 0x49, 0x7a, 0x2a, 0x91, 0xba, 0xd1, 0x3e, 0x01, 0xec, 0x5f, 0xc2, 0x15, 0x14, 0x10, 0xb3, 
0x28, 0x5e, 0x56, 0xcc, 0x26, 0x51, 0x24, 0x93, 0x0e, 0x6c, 0x99, 0x96, 0x38, 0xe0, 0x7d, 0x68, 
0x8c, 0xb7, 0x97, 0x23, 0xf4, 0xac, 0x4d, 0xbc, 0x5e, 0x01, 0x15, 0xff, 0x45, 0x60, 0x08, 0x13, 
0xcd, 0x59, 0xd7, 0x73, 0xb0, 0x0c, 0x20, 0x5e, 0xab, 0xaa, 0x24, 0x31, 0xe2, 0x2a, 0xa2, 0x53, 
0x8a, 0xf7, 0x86, 0xd5, 0x19, 0x78, 0xc5, 0x55, 0x9c, 0x08, 0xb7, 0xe2, 0xf4, 0xd0, 0x37, 0x74, 
0x93, 0x56, 0x62, 0x7b, 0x95, 0xcc, 0x2c, 0xb0, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbc, 0xe6, 0xfa, 0xad, 0xa7, 0x17, 0x9e, 0x84, 
0xf3, 0xb9, 0xca, 0xc2, 0xfc, 0x63, 0x25, 0x51, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 
0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x5a, 0xc6, 0x35, 0xd8, 
0xaa, 0x3a, 0x93, 0xe7, 0xb3, 0xeb, 0xbd, 0x55, 0x76, 0x98, 0x86, 0xbc, 0x65, 0x1d, 0x06, 0xb0, 
0xcc, 0x53, 0xb0, 0xf6, 0x3b, 0xce, 0x3c, 0x3e, 0x27, 0xd2, 0x60, 0x4b, 0x07, 0x78, 0x3b, 0x0d, 
0xfe, 0x4a, 0xa3, 0x19, 0x49, 0xb0, 0xce, 0xaf, 0x3f, 0x74, 0x0f, 0x32, 0x16, 0x0c, 0x8b, 0x46, 
0x94, 0x5b, 0xa5, 0xb0, 0xe4, 0x8a, 0xda, 0xd8, 0x88, 0x32, 0x90, 0x53, 0x08, 0xf7, 0xa2, 0xaa, 
0xba, 0x62, 0xb3, 0xfe, 0x29, 0x80, 0xc9, 0x5b, 0x63, 0x53, 0xc8, 0x24, 0x3c, 0x7c, 0x1f, 0x4c, 
0xda, 0xcd, 0xe5, 0x5f, 0xa2, 0x36, 0x93, 0x04, 0x3c, 0x3a, 0xbc, 0x2e, 0x02, 0x10, 0x9a, 0xf4, 
0x06, 0x32, 0x30, 0x89, 0xcb, 0x95, 0xe9, 0x55, 0x0e, 0x9d, 0xaf, 0x0e, 0x98, 0xcd, 0xca, 0xdc, 
0xb1, 0xff, 0xfc, 0xd1, 0x45, 0x66, 0xbb, 0x86, 0x46, 0x1e, 0x8c, 0x30, 0x04, 0x78, 0x53, 0xe1, 
0x3f, 0x96, 0xc5, 0xe4, 0x15, 0x23, 0x7b, 0x1f, 0x3f, 0x2c, 0xd3, 0x95, 0x40, 0xbc, 0x7a, 0x31, 
0x1f, 0x14, 0x38, 0x9e, 0x1a, 0xa5, 0xd6, 0x63, 0x10, 0x91, 0xe4, 0xd3, 0x00, 0xb4, 0x02, 0xbc, 
0x47, 0xfa, 0xa6, 0x29, 0x82, 0x0b, 0xb1, 0xd5, 0xff, 0xf2, 0xe6, 0xb0, 0xc6, 0xae, 0xe8, 0x7b, 
0x91, 0xd9, 0xee, 0x66, 0x07, 0x1f, 0xfd, 0xa2, 0xe7, 0x02, 0x66, 0xdd, 0x05, 0x2e, 0xf8, 0xc6, 
0xc1, 0x6a, 0xef, 0x3c, 0xc1, 0x95, 0xf6, 0x26, 0xce, 0x5e, 0x55, 0xd1, 0x64, 0x13, 0x28, 0xb1, 
0x18, 0x57, 0xd8, 0x1b, 0x84, 0xfa, 0xec, 0x7e, 0x5d, 0x99, 0x06, 0x49, 0x05, 0x73, 0x35, 0xa9, 
0xa7, 0xf2, 0xa1, 0x92, 0x5f, 0x3e, 0x7c, 0xdf, 0xac, 0xfe, 0x0f, 0xf5, 0x08, 0xd0, 0x3c, 0xae, 
0xcd, 0x58, 0x00, 0x5f, 0xd0, 0x84, 0x7e, 0xea, 0x63, 0x57, 0xfe, 0xc6, 0x01, 0x56, 0xda, 0xf3, 
0x72, 0x61, 0xda, 0xc6, 0x93, 0xb0, 0xac, 0xef, 0xaa, 0xd4, 0x51, 0x6d, 0xca, 0x71, 0x1e, 0x06, 
0x73, 0xea, 0x83, 0xb2, 0xb1, 0x99, 0x4a, 0x4d, 0x4a, 0x0d, 0x35, 0x07, 0x6b, 0x17, 0xd1, 0xf2, 
0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc, 0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2, 0x77, 0x03, 0x7d, 0x81, 
0x2d, 0xeb, 0x33, 0xa0, 0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96, 0x4f, 0xe3, 0x42, 0xe2, 
0xfe, 0x1a, 0x7f, 0x9b, 0x8e, 0xe7, 0xeb, 0x4a, 0x7c, 0x0f, 0x9e, 0x16, 0x2b, 0xce, 0x33, 0x57, 
0x6b, 0x31, 0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5, 0x93, 0xca, 0x4a, 0x6a, 
0x3e, 0xe3, 0x16, 0xef, 0x63, 0x8b, 0xfb, 0x84, 0xf7, 0xff, 0xae, 0x27, 0xbe, 0xaf, 0x88, 0xcd, 
0xdf, 0x63, 0x0a, 0xe4, 0x29, 0x96, 0x12, 0x32, 0xe7, 0xf4, 0xc7, 0x36, 0xdd, 0xf8, 0x71, 0x21, 
0xf5, 0x73, 0x9e, 0x3d, 0x9f, 0xbf, 0xa1, 0x14, 0x90, 0xa7, 0x67, 0x1b, 0x24, 0xdf, 0xde, 0x96, 
0x83, 0x05, 0x83, 0x6d, 0x6e, 0x34, 0xf4, 0xa1, 0xd0, 0xb8, 0x36, 0x5d
};

CSIGMASession::CSIGMASession()
{
    m_cpDevice = NULL;
	m_isOpen = false;
    m_certAPIVersion = 0;
    m_actualAPIVersion = 0;
    m_SigmaSessionId = 0;
    memset(m_SessionKey, 0, sizeof(m_SessionKey));
    memset(m_SigningKey, 0, sizeof(m_SigningKey));
    m_verifierPreComputationBlob = NULL;
}

CSIGMASession::~CSIGMASession()
{
    if (m_isOpen) 
        Close();
}

PavpEpidStatus CSIGMASession::Open(
    CCPDevice *cpDevice,
    uint32 certAPIVersion,
    const uint8 *cert, 
    uint32 certSize, 
    const void **revocationLists, 
    uint32 revocationListsCount, 
    const EcDsaPrivKey privKey,
    const uint32 *seed,
    uint32 seedBits)
{
    if (NULL == cpDevice ||
        NULL == cert ||
        (0 != revocationListsCount && NULL == revocationLists) ||
        NULL == privKey)
        return PAVP_STATUS_BAD_POINTER;
    // If redo-ing the SIGMA key exchange, make sure to clean up the old session
    if (NULL != m_cpDevice)
        return PAVP_STATUS_NOT_SUPPORTED;

    m_cpDevice = cpDevice;

    PavpEpidStatus          PavpStatus = PAVP_STATUS_SUCCESS;

    SigmaPubKey             HwPublicEccKey = {0};            // g^a
    SigmaPubKey             AppPublicEccKey = {0};           // g^b
    uint8                   GaGb[2 * SIGMA_PUBLIC_KEY_LEN];  // g^a || g^b
    bool                    bGaReceived = false;

    // For PAVP SDK Crypto Utility:
    CdgStatus               Status = CdgStsOk;
    CdgResult               VerifRes = CdgInvalid;

    // For PCH GetCaps Call:
    GetCapsInBuff           sGetCapsIn = {{0}};
    GetCapsOutBuff          sGetCapsOut = {{0}};
    
    // For Step 1:
    GetHwEccPubKeyInBuff    sGetHwEccPubKeyIn = {{0}};
    GetHwEccPubKeyOutBuff   sGetHwEccPubKeyOut = {{0}};
        
    // For Step 2:
    HwAppExchgCertsInBuff   sExchgCertsIn = {{0}};
    HwAppExchgCertsOutBuff  sExchgCertsOut = {{0}};
//    unsigned char           PrivKey3p[ECDSA_PRIVKEY_LEN];
//    Cert3p                  Certificate3p = {0};            // This would be the application's certificate, signed by Intel

    EPIDVerifier* epidVerifierCtx = NULL;

    SigmaCryptoUtils sigmaCrypto;

    for(;;)
    {
        Status = sigmaCrypto.Init(seed, seedBits);
        if(Status != CdgStsOk)
        {
            PavpStatus = PAVP_STATUS_CRYPTO_DATA_GEN_ERROR;
            break;
        }

        m_certAPIVersion = certAPIVersion;

        // Get actual PAVP API version
        sGetCapsIn.Header.ApiVersion    = SIGMA_API_VERSION(2, 0);
        sGetCapsIn.Header.BufferLength  = 0;
        sGetCapsIn.Header.CommandId     = CMD_GET_PCH_CAPABILITIES;
        sGetCapsIn.Header.PavpCmdStatus = PAVP_STATUS_SUCCESS;

        PavpStatus = cpDevice->ExecuteKeyExchange(
              &sGetCapsIn,  sizeof(sGetCapsIn), 
              &sGetCapsOut, sizeof(sGetCapsOut));
        if( PAVP_EPID_FAILURE(PavpStatus) || PAVP_EPID_FAILURE(sGetCapsOut.Header.PavpCmdStatus) )
        {
            if( PAVP_EPID_FAILURE(sGetCapsOut.Header.PavpCmdStatus) )
                // CMD_GET_PCH_CAPABILITIES not yet supported for PAVP API 1.5.
                m_actualAPIVersion = SIGMA_API_VERSION(1, 5);
            else
            {
                PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("SIGMA key exchange fails for CMD_GET_PCH_CAPABILITIES, api 0x%08x (cmdStatus=0x%08x).\n"), 
                    sGetCapsIn.Header.ApiVersion, sGetCapsOut.Header.PavpCmdStatus));
                PavpStatus = PAVP_STATUS_GET_CAPS_FAILED;
                break;
            }
        }
        else
            m_actualAPIVersion = sGetCapsOut.Caps.PAVPVersion;

        if (m_actualAPIVersion > SIGMA_API_VERSION(3, 0))
        {
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("Forcing API version change from %d "), 
                m_actualAPIVersion));
            m_actualAPIVersion = SIGMA_API_VERSION(3, 0);
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("to %d.\n"), 
                m_actualAPIVersion));
        }


        sGetHwEccPubKeyIn.Header.ApiVersion     = certAPIVersion;
        sGetHwEccPubKeyIn.Header.BufferLength   = 0;
        sGetHwEccPubKeyIn.Header.CommandId      = CMD_GET_HW_ECC_PUBKEY;
        sGetHwEccPubKeyIn.Header.PavpCmdStatus         = PAVP_STATUS_SUCCESS;

        PavpStatus = cpDevice->ExecuteKeyExchange(
              &sGetHwEccPubKeyIn, sizeof(sGetHwEccPubKeyIn), 
              &sGetHwEccPubKeyOut, sizeof(sGetHwEccPubKeyOut));

        if( PAVP_EPID_FAILURE(PavpStatus) || PAVP_EPID_FAILURE(sGetHwEccPubKeyOut.Header.PavpCmdStatus) )
        {
            if( PAVP_EPID_FAILURE(sGetHwEccPubKeyOut.Header.PavpCmdStatus) )
                PavpStatus = (PavpEpidStatus)sGetHwEccPubKeyOut.Header.PavpCmdStatus;
            else
                PavpStatus = PAVP_STATUS_GET_PUBKEY_FAILED;
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("SIGMA key exchange fails for CMD_GET_HW_ECC_PUBKEY, api 0x%08x (cmdStatus=0x%08x).\n"), 
                sGetHwEccPubKeyIn.Header.ApiVersion, sGetHwEccPubKeyOut.Header.PavpCmdStatus));
            break;
        }

        bGaReceived = true;

        // Copy out g^a
        memcpy(HwPublicEccKey, sGetHwEccPubKeyOut.Ga, SIGMA_PUBLIC_KEY_LEN);

        // Copy out the SIGMA session ID
        m_SigmaSessionId = sGetHwEccPubKeyOut.SigmaSessionId;

        // Derive the session key, Sk and message signing key, Mk using the PAVP EPID SDK code:
        // This will also give g^b back to send to the hardware
        Status = sigmaCrypto.DeriveSkMk(   (unsigned char*)HwPublicEccKey, SIGMA_PUBLIC_KEY_LEN,
                                    (unsigned char*)AppPublicEccKey, SIGMA_PUBLIC_KEY_LEN,
                                    (unsigned char*)m_SessionKey, SIGMA_SESSION_KEY_LEN,
                                    (unsigned char*)m_SigningKey, SIGMA_MAC_KEY_LEN);
        if(Status != CdgStsOk)
        {
            PavpStatus = PAVP_STATUS_DERIVE_SIGMA_KEYS_FAILED;
            break;
        }

        // Create an HMAC using Mk this application's certificate.
        PavpStatus = Sign(cert, certSize, (unsigned char*)&(sExchgCertsIn.Certificate3pHmac));
        if(PAVP_EPID_FAILURE(PavpStatus)) 
            break;

        // Copy over g^a || g^b to the struct to give to the hardware
        memcpy(GaGb, HwPublicEccKey, SIGMA_PUBLIC_KEY_LEN);
        memcpy(GaGb + SIGMA_PUBLIC_KEY_LEN, AppPublicEccKey, SIGMA_PUBLIC_KEY_LEN);

        // Sign the concatenation of g^a || g^b
        Status = sigmaCrypto.MsgSign3p(privKey, ECDSA_PRIVKEY_LEN,
                           GaGb, (2 * SIGMA_PUBLIC_KEY_LEN),
                           (uint8*)&(sExchgCertsIn.EcDsaSigGaGb), ECDSA_SIGNATURE_LEN);
        if(Status != CdgStsOk) 
        {
            PavpStatus = PAVP_STATUS_CRYPTO_DATA_GEN_ERROR;
            break;
        }

        sExchgCertsIn.Header.ApiVersion     = certAPIVersion;
        sExchgCertsIn.Header.BufferLength   = sizeof(HwAppExchgCertsInBuff) - sizeof(PAVPCmdHdr);
        sExchgCertsIn.Header.CommandId      = CMD_EXCHG_HW_APP_CERT;
        sExchgCertsIn.Header.PavpCmdStatus  = PAVP_STATUS_SUCCESS;
        sExchgCertsIn.SigmaSessionId        = m_SigmaSessionId;

        memcpy(sExchgCertsIn.Gb, AppPublicEccKey, SIGMA_PUBLIC_KEY_LEN);
        memcpy(&sExchgCertsIn.Certificate3p, cert, certSize);

        PavpStatus = cpDevice->ExecuteKeyExchange(
              &sExchgCertsIn, sizeof(sExchgCertsIn), 
              &sExchgCertsOut, sizeof(sExchgCertsOut) );

        if( PAVP_EPID_FAILURE(PavpStatus) || PAVP_EPID_FAILURE(sExchgCertsOut.Header.PavpCmdStatus) )
        {
            if( PAVP_EPID_FAILURE(sExchgCertsOut.Header.PavpCmdStatus) )
                PavpStatus = (PavpEpidStatus)sExchgCertsOut.Header.PavpCmdStatus;
            else
                PavpStatus = PAVP_STATUS_CERTIFICATE_EXCHANGE_FAILED;
            PAVPSDK_DEBUG_MSG(1, (pavpsdk_T("SIGMA key exchange fails for CMD_EXCHG_HW_APP_CERT, api 0x%08x (cmdStatus=0x%08x).\n"), 
                sExchgCertsIn.Header.ApiVersion, sExchgCertsOut.Header.PavpCmdStatus));
            break;
        }

        // Verify the HMAC at sExchgCertsOut.CertificatePchHmac
        uint8 signature[SIGMA_MAC_LEN];
        PavpStatus = Sign((unsigned char*)&(sExchgCertsOut.CertificatePch), sizeof(EpidCert), signature);
        if(PAVP_EPID_FAILURE(PavpStatus)) 
        {
            PavpStatus = PAVP_STATUS_CRYPTO_DATA_GEN_ERROR;
            break;
        }
        if(0 != memcmp(signature, (unsigned char*)sExchgCertsOut.CertificatePchHmac, SIGMA_MAC_LEN)) 
        {
            PavpStatus = PAVP_STATUS_PCH_HMAC_INVALID;
            break;
        }

        // Verify Intel root signature of certificate PCH (sExchgCertsOut.CertificatePchHmac)
        VerifRes = CdgInvalid;
        Status = sigmaCrypto.VerifyCertPchSigIntel((unsigned char*)&(sExchgCertsOut.CertificatePch), EPID_CERT_LEN, &VerifRes);
        if(Status != CdgStsOk) 
        {
            PavpStatus = PAVP_STATUS_CRYPTO_DATA_GEN_ERROR;
            break;
        }
        if(VerifRes != CdgValid)
        {
            PavpStatus = PAVP_STATUS_PCH_CERTIFICATE_INVALID;
            break;
        }

        // Use EPID to verify g^a || g^b using the hardware's EPID public key from the certificate.
        EPID_RESULT EpidRes = EPID_SUCCESS;

        uint8 revocationBuffer[GET_RL_REQUIRED_LENGTH(0)];
        void *revocation = NULL;
        for (uint32 i = 0; i < revocationListsCount; i++)
            if (HSTRING_TO_U32(((EPIDGroupCertificateBlob*)&(sExchgCertsOut.CertificatePch))->gid) == GET_RL_GID(revocationLists[i]) &&
                (NULL == revocation || GET_RL_VER(revocation) < GET_RL_VER(revocationLists[i])))
            {
                revocation = (void*)revocationLists[i]; // Warning: Remove const qualifier as EPID shouldn't modify it
                break;
            }

        if (NULL == revocation)
        {
            revocation = revocationBuffer;
            memset(revocation, 0, sizeof(revocationBuffer));
            SET_RL_SVER(revocation, EPID_SVER);
            SET_RL_BLOBID(revocation, epidKeyBasedRevocationList);
            SET_RL_GID(revocation, HSTRING_TO_U32(((EPIDGroupCertificateBlob*)&(sExchgCertsOut.CertificatePch))->gid));
            SET_RL_VER(revocation, 0);
            SET_RL_N1(revocation, 0);
        }

        bool createPreCompBlob = NULL == m_verifierPreComputationBlob || 
            0 != memcmp(&m_pchCert, &(sExchgCertsOut.CertificatePch), sizeof(EpidCert));
        m_verifierPreComputationBlob = m_verifierPreComputationBlobBuffer;
        if (createPreCompBlob)
            memcpy(&m_pchCert, &(sExchgCertsOut.CertificatePch), sizeof(EpidCert));

        EpidRes = epidVerifier_create(
            (EPIDParameterCertificateBlob*)EpidParams, 
            (EPIDGroupCertificateBlob*)&(sExchgCertsOut.CertificatePch),
            revocation, 
            createPreCompBlob, 
            (EPIDVerifierPreComputationBlob *)m_verifierPreComputationBlob, 
            &epidVerifierCtx);
        if (EpidRes != EPID_SUCCESS) 
        {
            PavpStatus = PAVP_STATUS_CRYPTO_DATA_GEN_ERROR;
            break;
        }

        EpidRes = epidVerifier_verifyMemberSignature(
            epidVerifierCtx, 
            (unsigned char*)GaGb, (2 * SIGMA_PUBLIC_KEY_LEN),
            (unsigned char *)NULL, 0,
            (EPIDSignatureBlob*)sExchgCertsOut.EpidSigGaGb);
        if (EpidRes != EPID_SUCCESS) 
        {
            PavpStatus = PAVP_STATUS_PCH_EPID_SIGNATURE_INVALID;
            break;
        }    
        
        m_cpDevice = cpDevice;
        break;    
    }

    if (NULL != epidVerifierCtx)
    {
        epidVerifier_delete(&epidVerifierCtx);
        epidVerifierCtx = NULL;
    }

    if( bGaReceived && PAVP_EPID_FAILURE(PavpStatus) )
    {
        CloseSigmaSessionInBuff     sCloseSessionIn = {{0}};
        CloseSigmaSessionOutBuff    sCloseSessionOut = {{0}};

        sCloseSessionIn.Header.ApiVersion   = m_certAPIVersion;
        sCloseSessionIn.Header.BufferLength = sizeof(CloseSigmaSessionInBuff) - sizeof(PAVPCmdHdr);
        sCloseSessionIn.Header.CommandId    = CMD_CLOSE_SIGMA_SESSION;
        sCloseSessionIn.Header.PavpCmdStatus = PAVP_STATUS_SUCCESS;
        sCloseSessionIn.SigmaSessionId      = m_SigmaSessionId;

        cpDevice->ExecuteKeyExchange(
           &sCloseSessionIn, sizeof(sCloseSessionIn), 
           &sCloseSessionOut, sizeof(sCloseSessionOut) );
    }
	else
		m_isOpen = true;

    return PavpStatus;
}

PavpEpidStatus CSIGMASession::Encrypt(const uint8* src, uint8* dst, uint32 size)
{
    IppStatus sts = ippStsNoErr;

    if (NULL == src || NULL == dst)
        return PAVP_STATUS_BAD_POINTER;
    
    int rijSize = 0;
    ippsRijndael128GetSize(&rijSize);

    IppsRijndael128Spec* rij = (IppsRijndael128Spec*)(new uint8[rijSize]);
    if(NULL == rij)
        return PAVP_STATUS_MEMORY_ALLOCATION_ERROR;

    sts = ippsRijndael128Init(m_SessionKey, IppsRijndaelKey128, rij);
    if (ippStsNoErr == sts)
        sts = ippsRijndael128EncryptECB(src, dst, size, rij, IppsCPPaddingNONE);

    SecureZeroMemory(rij, rijSize);
    PAVPSDK_SAFE_DELETE_ARRAY(rij);

    if(ippStsNoErr != sts)
        return PAVP_STATUS_CRYPTO_DATA_GEN_ERROR;
    return PAVP_STATUS_SUCCESS;
}

PavpEpidStatus CSIGMASession::Decrypt(const uint8* src, uint8* dst, uint32 size)
{
    IppStatus sts = ippStsNoErr;

    if (NULL == src || NULL == dst)
        return PAVP_STATUS_BAD_POINTER;
    
    int rijSize = 0;
    ippsRijndael128GetSize(&rijSize);

    IppsRijndael128Spec* rij = (IppsRijndael128Spec*)(new uint8[rijSize]);
    if(NULL == rij)
        return PAVP_STATUS_MEMORY_ALLOCATION_ERROR;

    sts = ippsRijndael128Init(m_SessionKey, IppsRijndaelKey128, rij);
    if (ippStsNoErr == sts)
        sts = ippsRijndael128DecryptECB(src, dst, size, rij, IppsCPPaddingNONE);

    SecureZeroMemory(rij, rijSize);
    PAVPSDK_SAFE_DELETE_ARRAY(rij);

    if(ippStsNoErr != sts)
        return PAVP_STATUS_CRYPTO_DATA_GEN_ERROR;
    return PAVP_STATUS_SUCCESS;
}

PavpEpidStatus CSIGMASession::Sign(const uint8* msg, uint32 msgSize, uint8* signature)
{
    if (NULL == msg || NULL == signature)
        return PAVP_STATUS_BAD_POINTER;

    IppStatus sts = ippStsNoErr;

    if (NULL == msg || NULL == signature)
        return PAVP_STATUS_BAD_POINTER;
   
    int ctxSize = 0;
    ippsHMACSHA256GetSize(&ctxSize);

    IppsHMACSHA256State* ctx = (IppsHMACSHA256State*)(new uint8[ctxSize]);
    if(NULL == ctx)
        return PAVP_STATUS_MEMORY_ALLOCATION_ERROR;

    sts = ippsHMACSHA256Init(m_SigningKey, SIGMA_MAC_KEY_LEN, ctx);
    if (ippStsNoErr == sts)
    {
        ippsHMACSHA256Update(msg, msgSize, ctx);
        ippsHMACSHA256Final(signature, SIGMA_MAC_LEN, ctx);
    }

    SecureZeroMemory(ctx, ctxSize);
    PAVPSDK_SAFE_DELETE_ARRAY(ctx);

    if(ippStsNoErr != sts)
        return PAVP_STATUS_CRYPTO_DATA_GEN_ERROR;
    return PAVP_STATUS_SUCCESS;
}

PavpEpidStatus CSIGMASession::Close()
{
    PavpEpidStatus sts = PAVP_STATUS_SUCCESS;

    if (!m_isOpen || NULL == m_cpDevice)
        return PAVP_STATUS_NOT_SUPPORTED;

    CCPDevice *cpDevice = m_cpDevice;
    m_cpDevice = NULL;

    SecureZeroMemory(&m_SessionKey,   sizeof(SigmaSessionKey));
    SecureZeroMemory(&m_SigningKey,   sizeof(SigmaMacKey));

    CloseSigmaSessionInBuff     sCloseSessionIn = {{0}};
    CloseSigmaSessionOutBuff    sCloseSessionOut = {{0}};

    sCloseSessionIn.Header.ApiVersion   = m_certAPIVersion;
    sCloseSessionIn.Header.BufferLength = sizeof(CloseSigmaSessionInBuff) - sizeof(PAVPCmdHdr);
    sCloseSessionIn.Header.CommandId    = CMD_CLOSE_SIGMA_SESSION;
    sCloseSessionIn.Header.PavpCmdStatus = PAVP_STATUS_SUCCESS;
    sCloseSessionIn.SigmaSessionId      = m_SigmaSessionId;

    sts = cpDevice->ExecuteKeyExchange(
           &sCloseSessionIn, sizeof(sCloseSessionIn), 
           &sCloseSessionOut, sizeof(sCloseSessionOut) );
    if (PAVP_EPID_FAILURE(sCloseSessionOut.Header.PavpCmdStatus))
        return PAVP_STATUS_INTERNAL_ERROR;

	m_isOpen = false;

    return sts;
}

PavpEpidStatus CSIGMASession::SetPreComputationBlob(
        const void *blob, 
        const EpidCert *pchCert)
{
    if (NULL == blob)
        m_verifierPreComputationBlob = NULL;
    else
    {
        if (NULL == pchCert)
            return PAVP_STATUS_BAD_POINTER;
        m_verifierPreComputationBlob = m_verifierPreComputationBlobBuffer;
        memcpy(m_verifierPreComputationBlobBuffer, blob, sizeof(m_verifierPreComputationBlobBuffer));
        memcpy(&m_pchCert, pchCert, sizeof(m_pchCert));
    }
    return PAVP_STATUS_SUCCESS;
}
    
PavpEpidStatus CSIGMASession::GetPreComputationBlob(uint8 *blob, uint32 *blobSize, EpidCert *pchCert)
{
    if (NULL == blob)
    {
        if (NULL == blobSize)
            return PAVP_STATUS_BAD_POINTER;
        *blobSize = sizeof(m_verifierPreComputationBlobBuffer);
    }
    else
    {
        if (NULL == blob || NULL == pchCert)
            return PAVP_STATUS_BAD_POINTER;
        if (*blobSize != sizeof(m_verifierPreComputationBlobBuffer))
            return PAVP_STATUS_LENGTH_ERROR;
        if (NULL == m_verifierPreComputationBlob)
            return PAVP_STATUS_NOT_SUPPORTED;
        memcpy(blob, m_verifierPreComputationBlob, *blobSize);
        memcpy(pchCert, &m_pchCert, sizeof(m_pchCert));
    }
    return PAVP_STATUS_SUCCESS;
}


