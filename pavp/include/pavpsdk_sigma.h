/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef __PAVPSDK_SIGMA_H__
#define __PAVPSDK_SIGMA_H__

#include "pavpsdk_pavpsession.h"
#include "intel_pavp_epid_api.h"

/**@ingroup PAVPSession 
@{
*/


#define ECDSA_PRIVKEY_LEN              32
//#define ECDSA_PUBKEY_LEN               64
/*#define ECDSA_SIGNATURE_LEN            64
#define EPID_PARAM_LEN                 876        // EPID cryptosystem context length
#define EPID_CERT_LEN                  392        // EPID certificate length
#define EPID_PUBKEY_LEN                328        // EPID cert length - EC-DSA sig length
#define EPID_SIG_LEN                   569        // EPID signature length
*/#define SIGMA_PUBLIC_KEY_LEN           64
#define SIGMA_SESSION_KEY_LEN          16
#define SIGMA_MAC_KEY_LEN              16

#define SIGMA_MAC_LEN                  32
//#define PAVP_EPID_STREAM_KEY_LEN       16


#define SIGMA_API_VERSION(major, minor) ((major << 16) | (minor))


/** SIGMA session management.
Can do SIGMA session keys exchange. It then provides encrypt, decrypt and sign 
operations with keys exchanged.
@ingroup PAVPSession
*/
class CSIGMASession: public CPAVPSession
{
typedef uint8 EcDsaPrivKey[ECDSA_PRIVKEY_LEN];
/*typedef unsigned char EcDsaPubKey[ECDSA_PUBKEY_LEN];
typedef unsigned char EcDsaSig[ECDSA_SIGNATURE_LEN];
typedef unsigned char EpidSig[EPID_SIG_LEN];
*/typedef unsigned char SigmaPubKey[SIGMA_PUBLIC_KEY_LEN];
typedef uint8 SigmaSessionKey[SIGMA_SESSION_KEY_LEN];
typedef uint8 SigmaMacKey[SIGMA_MAC_KEY_LEN];
/*typedef unsigned char HMAC[SIGMA_MAC_LEN];
typedef unsigned char StreamKey[PAVP_EPID_STREAM_KEY_LEN];
*/
typedef uint32 PAVPStreamId;
typedef uint32 PAVPSessId;

public:
    /// Creates a SIGMA session object.
    CSIGMASession();

    virtual ~CSIGMASession();

    /** SIGMA session key exchange.
    Perform SIGMA session key exchange based on Cert3p certificate and 
    private key.
    @param[in] cpDevice Interface to execute CP commands.
    @param[in] certAPIVersion Certificate3p version.
    @param[in] cert Buffer pointer contains certificate.
    @param[in] certSize Certificate size in bytes.
    @param[in] revocationLists Pointer to array of Private-key based 
        Revocation Lists.
    @param[in] revocationListsCount Number of revocation lists pointer.
    @param[in] privKey Certificate private key.
    @param[in] seed Seed for random number generator.
    @param[in] seedBits Number of bits in seed buffer.
    
    @return PAVP_STATUS_NOT_SUPPORTED Session already opened. Try closing the 
        session and retry.
    */
    virtual PavpEpidStatus Open(
        CCPDevice *cpDevice,
        uint32 certAPIVersion,
        const uint8 *cert, 
        uint32 certSize, 
        const void **revocationLists, 
        uint32 revocationListsCount,
        const EcDsaPrivKey privKey,
        const uint32 *seed,
        uint32 seedBits);

    /// Get version of the certificate3p used to open a sigma session.
    uint32 GetCertAPIVersion() { return m_certAPIVersion; };
    
    virtual CCPDevice *GetCPDevice() { return m_cpDevice; };
    virtual uint32 GetSessionId() { return m_SigmaSessionId; };
    virtual uint32 GetActualAPIVersion() { return m_actualAPIVersion; };


    /// Encrypt with SIGMA session key.
    virtual PavpEpidStatus Encrypt(const uint8* src, uint8* dst, uint32 size);
    
    /// Decrypt with SIGMA session key.
    virtual PavpEpidStatus Decrypt(const uint8* src, uint8* dst, uint32 size);

    /// Sign with SIGMA signing key.
    virtual PavpEpidStatus Sign(const uint8* msg, uint32 msgSize, uint8* signature);

    /// Close SIGMA session
    virtual PavpEpidStatus Close();

    /** Set pre-computation blob
    @param[in] blob Pre-computation blob.
    @param[in] pchCert PCH certificate blob was created for.
    */
    PavpEpidStatus SetPreComputationBlob(
        const void *blob, 
        const EpidCert *pchCert);

    /** Get pre-computation blob
    @param[out] blob Buffer to put pre-computation blob into.
    @param[in,out] blobSize Size in bytes of the blob buffer on input and number
        of butes in pre-computation blob on output.
    @param[out] pchCert PCH certificate blob was created for.
    */
    PavpEpidStatus GetPreComputationBlob(uint8 *blob, uint32 *blobSize, EpidCert *pchCert);

private:
    CCPDevice *m_cpDevice;
	bool m_isOpen;
    uint32 m_certAPIVersion;
    uint32 m_actualAPIVersion;
    PAVPSessId m_SigmaSessionId;
    SigmaSessionKey m_SessionKey;
    SigmaMacKey m_SigningKey;

    uint8 m_verifierPreComputationBlobBuffer[4672/8]; // EPID 1.0 EPIDVerifierPreComputationBlob 
    void *m_verifierPreComputationBlob;
    EpidCert m_pchCert; // PCH certificate m_verifierPreComputationBlob was computed for

    PAVPSDK_DISALLOW_COPY_AND_ASSIGN(CSIGMASession);
};

/** @} */ //@ingroup PAVPSession 

#endif//__PAVPSDK_SIGMA_H__